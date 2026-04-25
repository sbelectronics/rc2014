#include "sio.h"
#include "config.h"

#include "pico.h"

#include <string.h>

// Per-channel ring-buffer storage. Lives in BSS so it ends up in SRAM
// — needed because both cores access these.
static uint8_t rx_storage[SIO_NUM_CHANNELS][PICONET_RX_RING_SIZE];
static uint8_t tx_storage[SIO_NUM_CHANNELS][PICONET_TX_RING_SIZE];

sio_channel_state_t channels[SIO_NUM_CHANNELS];

// ----- helpers --------------------------------------------------------

// Map register index (0..7) to (channel, is_ctrl) per PICONET.md §2.1:
//   A0 = channel select, A1 = command/data, A2 = SIO chip select.
static inline uint8_t reg_channel(uint8_t reg) {
    return (reg & 1) | ((reg >> 1) & 2);
}
static inline bool reg_is_ctrl(uint8_t reg) {
    return (reg >> 1) & 1;
}

static bool __not_in_flash_func(channel_int_pending)(const sio_channel_state_t *s) {
    uint8_t wr1 = s->wr[1];

    // TX interrupt
    if ((wr1 & 0x02) && s->tx_int_pending) return true;

    // RX interrupt — modes 00/01/10/11
    uint8_t rx_mode = (wr1 >> 3) & 0x03;
    bool rx_avail = !ringbuf_empty(&s->rx);
    switch (rx_mode) {
        case 0: break;
        case 1: if (rx_avail && s->rx_int_armed) return true; break;
        case 2:
        case 3: if (rx_avail) return true; break;
    }

    // Ext/status (DCD change etc.) not synthesised in this skeleton.
    return false;
}

// ----- public API -----------------------------------------------------

void sio_init(void) {
    memset(channels, 0, sizeof(channels));
    for (int i = 0; i < SIO_NUM_CHANNELS; i++) {
        ringbuf_init(&channels[i].rx, rx_storage[i], PICONET_RX_RING_SIZE);
        ringbuf_init(&channels[i].tx, tx_storage[i], PICONET_TX_RING_SIZE);
    }
}

void sio_channel_reset(sio_channel_t ch) {
    sio_channel_state_t *s = &channels[ch];
    memset(s->wr, 0, sizeof(s->wr));
    s->wr_pointer       = 0;
    s->tx_int_pending   = false;
    s->rx_int_armed     = false;
    ringbuf_init(&s->rx, rx_storage[ch], PICONET_RX_RING_SIZE);
    ringbuf_init(&s->tx, tx_storage[ch], PICONET_TX_RING_SIZE);
}

sio_channel_state_t *sio_channel(sio_channel_t ch) {
    return &channels[ch];
}

void __not_in_flash_func(sio_bus_write)(uint8_t reg, uint8_t data) {
    if (reg > 7) return;
    sio_channel_t ch = (sio_channel_t)reg_channel(reg);
    sio_channel_state_t *s = &channels[ch];

    if (!reg_is_ctrl(reg)) {
        // Data write — push to TX ring; drop if full (skeleton).
        ringbuf_push(&s->tx, data);
        return;
    }

    // Control write. WR0 is targeted when wr_pointer == 0 and carries
    // a command code in b5..b3 plus a new pointer in b2..b0.
    if (s->wr_pointer == 0) {
        uint8_t cmd = (data >> 3) & 0x07;
        switch (cmd) {
            case 0: case 1:                    // null
                break;
            case 2:                            // reset ext/status ints
                break;
            case 3:                            // channel reset
                sio_channel_reset(ch);
                return;
            case 4:                            // EI on next RX char
                s->rx_int_armed = true;
                break;
            case 5:                            // reset TX INT pending
                s->tx_int_pending = false;
                break;
            case 6:                            // error reset
                break;
            case 7:                            // return from int — no chain
                break;
        }
        s->wr_pointer = data & 0x07;
        s->wr[0] = data;
    } else {
        s->wr[s->wr_pointer] = data;
        s->wr_pointer = 0;
    }
}

bool __not_in_flash_func(sio_int_pending)(void) {
    for (int i = 0; i < SIO_NUM_CHANNELS; i++) {
        if (channel_int_pending(&channels[i])) return true;
    }
    return false;
}
