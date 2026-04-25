#include "bus.h"
#include "config.h"
#include "sio.h"

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#include "bus_pio.pio.h"

// ------------------------------------------------------------------------
// ARCHITECTURE — DMA pipeline, ARM out of read response path
//
// READ RESPONSE (hardware-driven, ~150 ns deterministic):
//   1. Z80 asserts /CS + /RD.
//   2. SM_read (PIO) detects /CS LOW, captures A0..A3, computes
//      &shadow32[addr] using preloaded Y register, pushes the 32-bit
//      pointer to its RX FIFO.
//   3. DMA "trigger" channel (DREQ-paced on PIO RX): copies the
//      pointer from RX FIFO to "fetch" channel's al3_read_addr_trig.
//      That single write both sets fetch's READ_ADDR and triggers it.
//   4. DMA "fetch" channel (no DREQ — just DREQ_FORCE so it fires
//      immediately on trigger): reads the 32-bit shadow word and
//      writes it to SM_read's TX FIFO. count=1, completes after one
//      transfer, raises IRQ.
//   5. SM_read pulls the word, drives D0..D7 through U2 for the
//      duration of /RD low, then releases.
//   6. ARM IRQ handler rearms fetch's count=1. Runs ~200 ns after the
//      cycle, with ~1.4 µs of slack before the next Z80 IO cycle.
//
// POST-READ (deferred, via SM_log):
//   SM_log fires AFTER /RD HIGH. ARM bus_poll pops ringbuf and
//   refreshes shadow with next byte. Has up to 1.6 µs of slack.
//
// SHADOW (single writer, core 1 only):
//   uint32_t sio_shadow32[16] aligned to 64 bytes. Byte stored in low
//   8 bits of each word. ARM updates via refresh_shadow() which is
//   called from SM_log handler (post-read), sio_bus_write handler,
//   and channel reset.
//
// TIMING BUDGET (Z80 @ 7.37 MHz, 1 wait state):
//   Z80 effective sample at T2 + 345 ns.
//   /CS LOW at T2 + 10 ns (U4 prop).
//   PIO sees /CS via sync at T2 + 23 ns.
//   PIO push pointer at T2 + ~70 ns (after compute: mov + in + in + push).
//   DMA trigger transfers in ~4 cycles = 27 ns: T2 + 97 ns.
//   DMA fetch transfers in ~4 cycles = 27 ns: T2 + 124 ns.
//   PIO pull + drive enable: T2 + ~150 ns.
//   Margin: 345 - 150 = 195 ns. VERY HEALTHY.
//
//   No ARM iteration jitter. No worst case to worry about. The latency
//   is determined entirely by the PIO + DMA hardware pipeline.
// ------------------------------------------------------------------------

static PIO bus_pio = pio0;
static uint sm_oe, sm_read, sm_write, sm_log;
static int  dma_chan_trigger, dma_chan_fetch;

bus_stats_t bus_stats;

// 32-bit shadow table, 64-byte aligned. PIO computes &shadow32[addr]
// using Y register preload; alignment lets us OR-with-shifted-addr
// instead of doing an ADD (PIO can't ADD).
__attribute__((aligned(64)))
static uint32_t sio_shadow32[16];

// ------------------------------------------------------------------------
// Shadow refresh — recompute the data + ctrl shadow entries for one
// channel from current ringbuf / connection / WR-register state.
// Called only from core 1.
// ------------------------------------------------------------------------

static inline int reg_data_of(int ch) { return (ch & 1) | ((ch & 2) << 1); }
static inline int reg_ctrl_of(int ch) { return reg_data_of(ch) | 2; }

static void __not_in_flash_func(refresh_shadow)(int ch) {
    sio_channel_state_t *s = &channels[ch];
    int data_reg = reg_data_of(ch);
    int ctrl_reg = reg_ctrl_of(ch);

    uint32_t rh = s->rx.head;
    uint32_t rt = s->rx.tail;

    // Data shadow: front of RX, or 0xFF if empty.
    sio_shadow32[data_reg] = (rh == rt)
                             ? 0xFFu
                             : s->rx.buf[rt & s->rx.mask];

    // Control shadow: RR<wr_pointer>.
    uint8_t v;
    uint8_t wp = s->wr_pointer;
    if (wp == 0) {
        v = 0x20;                                    // CTS always asserted
        if (rh != rt)                     v |= 0x01; // RX char available
        if (s->tx.head - s->tx.tail !=
            s->tx.mask + 1)               v |= 0x04; // TX buffer empty
        if (s->connected)                 v |= 0x08; // DCD

        // RR0 b1 = INT pending for this channel (per WR1 mask).
        uint8_t wr1 = s->wr[1];
        bool ip = false;
        if ((wr1 & 0x02) && s->tx_int_pending) {
            ip = true;
        } else {
            uint8_t rx_mode = (wr1 >> 3) & 0x03;
            bool    rxa     = (rh != rt);
            if      (rx_mode == 1 && rxa && s->rx_int_armed) ip = true;
            else if ((rx_mode == 2 || rx_mode == 3) && rxa)  ip = true;
        }
        if (ip) v |= 0x02;
    } else if (wp == 2) {
        v = s->wr[2];                                // RR2 = vector readback
    } else {
        v = 0;
    }
    sio_shadow32[ctrl_reg] = v;
}

static void init_shadow(void) {
    for (int i = 0; i < 16; i++) sio_shadow32[i] = 0xFF;
    for (int i = 0; i < SIO_NUM_CHANNELS; i++) refresh_shadow(i);
}

// ------------------------------------------------------------------------
// DMA fetch-completion IRQ handler. Runs on core 1 (where the IRQ is
// enabled). Just rearms fetch's transfer_count back to 1 — the next
// trigger from DMA "trigger" channel will set READ_ADDR and fire fetch
// again. Critically, we use the NON-trigger transfer_count register so
// the rearm doesn't accidentally fire fetch with the stale READ_ADDR.
// ------------------------------------------------------------------------
static void __not_in_flash_func(dma_fetch_complete_irq)(void) {
    // Clear the IRQ line for fetch channel (DMA_IRQ_0).
    dma_hw->ints0 = 1u << dma_chan_fetch;
    // Rearm: count=1, no trigger. Fetch waits for next trigger from
    // the trigger channel (which fires when PIO pushes a new pointer).
    dma_hw->ch[dma_chan_fetch].transfer_count = 1;
}

// ------------------------------------------------------------------------
// Hot polling loop — core 1's main loop.
//
// With the DMA pipeline handling reads, ARM only services:
//   - SM_log RX FIFO (post-read events: ringbuf pop, shadow refresh)
//   - SM_write RX FIFO (Z80 writes to our SIO/2 registers)
//   - Maintenance every ~1024 iterations (/INT update, reset detect,
//     keep DMA "trigger" channel's transfer_count topped up so it
//     never runs out).
// ------------------------------------------------------------------------
static void __not_in_flash_func(bus_poll)(void) {
    uint32_t rxempty = (bus_pio->fstat >> PIO_FSTAT_RXEMPTY_LSB) & 0xF;

    // Common case: nothing to do.
    if ((rxempty & ((1u << sm_log) | (1u << sm_write))) ==
        ((1u << sm_log) | (1u << sm_write))) return;

    // (A) Post-read events. SM_log fires AFTER /RD HIGH, so we can
    //     mutate shadow without affecting the cycle in flight.
    if (!(rxempty & (1u << sm_log))) {
        uint32_t a   = bus_pio->rxf[sm_log];
        uint8_t  reg = (uint8_t)(a & 0x0F);

        bus_stats.reads_per_reg[reg]++;
        bus_stats.last_read_reg = reg;

        if (reg <= 7) {
            int ch = (reg & 1) | ((reg >> 1) & 2);
            sio_channel_state_t *s = &channels[ch];
            bool is_ctrl = (reg >> 1) & 1;

            if (!is_ctrl) {
                // Data read: pop the byte we just delivered.
                if (s->rx.head != s->rx.tail) {
                    __sync_synchronize();
                    s->rx.tail = s->rx.tail + 1;
                }
                s->rx_int_armed = false;
            } else {
                s->wr_pointer = 0;
            }
            refresh_shadow(ch);
        }
    }

    // (B) Z80 writes to our SIO/2 registers.
    if (!(rxempty & (1u << sm_write))) {
        uint32_t w    = bus_pio->rxf[sm_write];
        uint8_t  data = (uint8_t)(w & 0xFF);
        uint8_t  reg  = (uint8_t)((w >> 8) & 0x0F);

        sio_bus_write(reg, data);

        bus_stats.writes_per_reg[reg]++;
        bus_stats.last_write_reg  = reg;
        bus_stats.last_write_data = data;

        if (reg <= 7) {
            int ch = (reg & 1) | ((reg >> 1) & 2);
            refresh_shadow(ch);
        }
    }
}

static inline void __not_in_flash_func(update_int)(void) {
    gpio_put(PICONET_PIN_INT_DRV, sio_int_pending());
}

void bus_snapshot(bus_snapshot_t *out) {
    uint32_t all = gpio_get_all();
    out->a   = (uint8_t)((all >> PICONET_PIN_A0) & 0x0F);
    out->cs  = (all >> PICONET_PIN_CS) & 1;
    out->rd  = (all >> PICONET_PIN_RD) & 1;
    out->wr  = (all >> PICONET_PIN_WR) & 1;
    out->m1  = (all >> PICONET_PIN_M1) & 1;
    out->reset_sense = (all >> PICONET_PIN_RESET_SENSE) & 1;
}

// ------------------------------------------------------------------------
// DMA chain setup.
// ------------------------------------------------------------------------
static void setup_dma_chain(void) {
    dma_chan_trigger = dma_claim_unused_channel(true);
    dma_chan_fetch   = dma_claim_unused_channel(true);

    // --- Fetch channel: shadow word → PIO TX FIFO ---
    // count=1 per arming. Completes after each transfer and raises an
    // IRQ that ARM uses to rearm count=1. Uses DREQ_FORCE (TREQ_SEL=63
    // = "always ready") so it fires immediately when triggered, no
    // pacing — that's what we want, since the trigger only happens
    // when the trigger channel writes a new pointer to our al3.
    dma_channel_config fc = dma_channel_get_default_config(dma_chan_fetch);
    channel_config_set_transfer_data_size(&fc, DMA_SIZE_32);
    channel_config_set_read_increment (&fc, false);
    channel_config_set_write_increment(&fc, false);
    // No DREQ pacing — fire on trigger only.
    channel_config_set_dreq(&fc, DREQ_FORCE);
    dma_channel_configure(dma_chan_fetch, &fc,
        &bus_pio->txf[sm_read],   // dest: PIO TX FIFO
        NULL,                      // src: set by trigger channel via al3
        1,                         // 1 word per arming
        false);                    // do not start; first trigger comes from `trigger`

    // Enable IRQ on fetch completion. Wire to DMA_IRQ_0 on this core.
    dma_channel_set_irq0_enabled(dma_chan_fetch, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_fetch_complete_irq);
    irq_set_priority(DMA_IRQ_0, 0);                  // highest priority
    irq_set_enabled(DMA_IRQ_0, true);

    // --- Trigger channel: PIO RX FIFO → fetch.al3_read_addr_trig ---
    // DREQ-paced on PIO RX (fires once per PIO push). count = 4 billion
    // (effectively infinite); ARM's maintenance loop tops it up
    // periodically just in case.
    dma_channel_config tc = dma_channel_get_default_config(dma_chan_trigger);
    channel_config_set_transfer_data_size(&tc, DMA_SIZE_32);
    channel_config_set_read_increment (&tc, false);
    channel_config_set_write_increment(&tc, false);
    channel_config_set_dreq(&tc, pio_get_dreq(bus_pio, sm_read, false));  // PIO RX DREQ
    dma_channel_configure(dma_chan_trigger, &tc,
        &dma_hw->ch[dma_chan_fetch].al3_read_addr_trig,  // dest: fetch's READ_ADDR + trigger
        &bus_pio->rxf[sm_read],                          // src: PIO RX FIFO
        0xFFFFFFFFu,                                     // ~4B transfers; rearmed by ARM maint
        true);                                           // start; will wait on DREQ
}

// Called periodically from maintenance. Tops up trigger channel's
// transfer count so it can't run out (which would take ~4 billion
// reads — ~11 hours at sustained max IO rate — but rearm is cheap
// insurance).
static void __not_in_flash_func(rearm_trigger)(void) {
    dma_hw->ch[dma_chan_trigger].transfer_count = 0xFFFFFFFFu;
}

// ------------------------------------------------------------------------
// Core-1 entry point.
// ------------------------------------------------------------------------
void __not_in_flash_func(bus_core1_main)(void) {
    // Register this core with the SDK's flash-safe machinery so core 0
    // can call cfg_save() (flash_range_erase/program) without deadlock.
    // Without this, flash_safe_execute on core 0 would hang waiting for
    // an acknowledgement from us.
    flash_safe_execute_core_init();

    sm_oe    = pio_claim_unused_sm(bus_pio, true);
    sm_read  = pio_claim_unused_sm(bus_pio, true);
    sm_write = pio_claim_unused_sm(bus_pio, true);
    sm_log   = pio_claim_unused_sm(bus_pio, true);

    uint off_oe    = pio_add_program(bus_pio, &piconet_oe_program);
    uint off_read  = pio_add_program(bus_pio, &piconet_read_program);
    uint off_write = pio_add_program(bus_pio, &piconet_write_program);
    uint off_log   = pio_add_program(bus_pio, &piconet_log_program);

    // GP18 (RESET_SENSE) — input with pull-up.
    gpio_init(PICONET_PIN_RESET_SENSE);
    gpio_set_dir(PICONET_PIN_RESET_SENSE, GPIO_IN);
    gpio_pull_up(PICONET_PIN_RESET_SENSE);

    // GP19 (INT_DRV) — output, default LOW.
    gpio_init(PICONET_PIN_INT_DRV);
    gpio_set_dir(PICONET_PIN_INT_DRV, GPIO_OUT);
    gpio_put(PICONET_PIN_INT_DRV, 0);

    // RP2350 pads default to ISO=1 — bind every bus-facing pin to PIO0.
    for (uint p = PICONET_PIN_D0; p <= PICONET_PIN_M1; p++) {
        pio_gpio_init(bus_pio, p);
    }
    pio_gpio_init(bus_pio, PICONET_PIN_DIR_DATA);
    pio_gpio_init(bus_pio, PICONET_PIN_OE_DATA);

    // Initialise shadow BEFORE PIO read SM enables (it'll start
    // pushing pointers as soon as /CS triggers).
    init_shadow();

    piconet_oe_program_init   (bus_pio, sm_oe,    off_oe,
                               PICONET_PIN_CS, PICONET_PIN_OE_DATA);
    piconet_read_program_init (bus_pio, sm_read,  off_read,
                               PICONET_PIN_D0, PICONET_PIN_A0,
                               PICONET_PIN_CS, PICONET_PIN_RD,
                               PICONET_PIN_DIR_DATA,
                               (uint32_t)(uintptr_t)sio_shadow32);
    piconet_write_program_init(bus_pio, sm_write, off_write,
                               PICONET_PIN_D0, PICONET_PIN_RD);
    piconet_log_program_init  (bus_pio, sm_log,   off_log,
                               PICONET_PIN_A0, PICONET_PIN_RD);

    // DMA pipeline — read response runs entirely in hardware after this.
    setup_dma_chain();

    // Maintenance loop. bus_poll() handles SM_log + SM_write (post-
    // read effects + Z80 writes). Slow cadence handles /INT, reset
    // detection, and topping up the DMA trigger channel's count.
    bool prev_reset = false;
    uint32_t maint_ctr = 0;
    for (;;) {
        bus_poll();

        if (++maint_ctr >= 1024) {
            maint_ctr = 0;
            update_int();
            rearm_trigger();
            bool now_reset = gpio_get(PICONET_PIN_RESET_SENSE);
            if (now_reset && !prev_reset) {
                for (int i = 0; i < SIO_NUM_CHANNELS; i++) {
                    sio_channel_reset((sio_channel_t)i);
                    refresh_shadow(i);
                }
                bus_stats.reset_count++;
            }
            prev_reset = now_reset;
        }
    }
}
