#ifndef PICONET_HAYES_H
#define PICONET_HAYES_H

#include <stdbool.h>
#include <stdint.h>
#include "sio.h"

// Per-channel Hayes parser state (one per NET channel).
typedef enum {
    HAYES_DATA = 0,        // bytes pass through to/from TCP
    HAYES_DIALING,         // ATD in flight; TX dropped, RX silent
    HAYES_COMMAND,         // bytes feed AT parser, responses to RX queue
} hayes_state_t;

typedef struct {
    sio_channel_t  ch;
    hayes_state_t  state;

    // ATE0 / ATE1 — local echo of typed characters in COMMAND mode.
    // Default ATE1 (true). Has no effect in DATA or DIALING.
    bool           echo;

    // +++ escape detector (only watched in DATA state).
    uint32_t       last_tx_time_ms;    // time of last non-plus TX byte
    uint32_t       plus_last_time_ms;  // time of most recent '+'
    uint8_t        plus_count;         // 0..3 held pluses

    // Bytes hayes wants pump_tx to send to TCP (e.g. flushed pluses
    // from an aborted escape sequence, or from a guard-time timeout).
    // Written only by hayes from core 0; consumed only by pump_tx on
    // core 0 — so single-threaded, no queue invariants to violate.
    uint8_t        pending_tx[4];
    uint8_t        pending_tx_n;

    // AT command line buffer.
    char           cmdline[128];
    uint8_t        cmdlen;

    // Last dial failure reason, populated by hayes_on_dial_failed /
    // hayes_on_remote_close, cleared by hayes_on_connect. Queried via
    // ATL. Empty string until the first failure.
    char           last_error[64];
} hayes_t;

void hayes_init(hayes_t *h, sio_channel_t ch);

// Called by core 0's main loop with each byte the Z80 wrote to this
// channel's data register. Returns true if the byte was consumed by the
// parser (do not forward to TCP); false if it should pass through.
bool hayes_on_tx_byte(hayes_t *h, uint8_t b);

// Periodic tick for guard-time enforcement on the +++ detector. Call
// from the main loop (cheap when nothing's happening).
void hayes_tick(hayes_t *h, uint32_t now_ms);

// Dial / connect / disconnect events from the network layer. These
// produce CONNECT / NO CARRIER / etc. responses into the channel's RX
// queue and update `state`. `reason` (for failed/closed) is a short
// human-readable string stored in `last_error` for ATL retrieval; pass
// NULL for "unknown".
void hayes_on_connect(hayes_t *h);
void hayes_on_dial_failed(hayes_t *h, const char *reason);
void hayes_on_remote_close(hayes_t *h, const char *reason);

// Returns true if the channel is currently in DATA mode (and therefore
// TCP-bound bytes should flow normally). Used by the network layer to
// decide whether to actually drain the TX ring buffer to the socket.
bool hayes_in_data_mode(const hayes_t *h);

#endif
