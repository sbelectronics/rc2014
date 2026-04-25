#ifndef PICONET_SIO_H
#define PICONET_SIO_H

#include <stdbool.h>
#include <stdint.h>
#include "ringbuf.h"
#include "telnet.h"

// Skeleton Z80 SIO/2 emulation per PICONET.md §2.2. Two SIO/2 chips,
// each with two channels (A and B) — four channels total.
//
// Channel index → logical port (in PICONET.md §2):
//     0 = SIO #1 Ch A = UART0 (inbound listener, port from config.h)
//     1 = SIO #1 Ch B = UART1 (inbound listener)
//     2 = SIO #2 Ch A = NET0  (outbound, Hayes-controlled)
//     3 = SIO #2 Ch B = NET1  (outbound, Hayes-controlled)

#define SIO_NUM_CHANNELS 4

typedef enum {
    SIO_CH_UART0 = 0,
    SIO_CH_UART1 = 1,
    SIO_CH_NET0  = 2,
    SIO_CH_NET1  = 3,
} sio_channel_t;

// Per-channel state. RX = network → bus (Z80 reads). TX = bus → network
// (Z80 writes). Owned jointly by core 0 (network producer/consumer) and
// core 1 (bus consumer/producer); only the SPSC ring buffers cross the
// core boundary, and the rest of the struct is touched by core 1 only
// except for the volatile flags below.
typedef struct {
    ringbuf_t rx;          // network → bus
    ringbuf_t tx;          // bus → network

    // Set by core 0 to tell core 1 the connection state changed; core 1
    // recomputes RR0 b3 (DCD) from this.
    volatile bool connected;

    // SIO write-register file. Index 0..7 = WR0..WR7.
    uint8_t wr[8];
    uint8_t wr_pointer;    // 0..7, set by lower bits of WR0; auto-clears

    // Latched IRQ-source flags so the SIO_INT_ACK behaviour matches the
    // chip well enough for IM1 polling drivers.
    bool tx_int_pending;   // TX buffer transitioned empty after WR1 enabled
    bool rx_int_armed;     // first-char-only IRQ armed

    // Telnet protocol state. Starts in passive (raw) mode; flips to
    // active on first incoming IAC byte. UART channels (inbound TCP
    // listeners) use this; NET channels leave it untouched (raw).
    telnet_state_t telnet;
} sio_channel_state_t;

void sio_init(void);

extern sio_channel_state_t channels[SIO_NUM_CHANNELS];

// Bus-side write — called from core 1 bus front-end on every write.
void sio_bus_write(uint8_t reg, uint8_t data);

// Reset a single channel (channel reset command, or hardware reset).
void sio_channel_reset(sio_channel_t ch);

// Aggregate /INT signal. True if any of the four channels has a
// pending, enabled interrupt source per its WR1 mask.
bool sio_int_pending(void);

// Direct access for the network layer. Both halves run on core 0.
sio_channel_state_t *sio_channel(sio_channel_t ch);

#endif
