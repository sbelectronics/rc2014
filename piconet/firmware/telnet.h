#ifndef PICONET_TELNET_H
#define PICONET_TELNET_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Per-channel telnet protocol state. Passive — the channel starts in
// "raw passthrough" mode and only switches to telnet processing when
// it sees an incoming IAC (0xFF) byte. This makes the channel work
// transparently for both:
//
//   * Raw TCP clients (nc, ncat without -t): never send IAC, so we
//     never engage telnet machinery, never inject IAC bytes back.
//     Bytes pass through unchanged.
//
//   * Telnet clients (telnet, putty, ncat -t): send IAC during
//     option negotiation at connect, which triggers our telnet path.
//     We then negotiate "WILL ECHO" + "WILL SUPPRESS-GO-AHEAD" + "DO
//     SUPPRESS-GO-AHEAD" so the client switches to character-at-a-
//     time mode with no local echo (bytes go immediately, server
//     handles echo). Subsequent IAC sequences in either direction
//     are stripped/escaped properly.

typedef enum {
    TS_NORMAL = 0,    // not in middle of an IAC sequence
    TS_SAW_IAC,       // last byte was 0xFF
    TS_OPT_DO,        // last bytes were IAC DO  — next is option num
    TS_OPT_DONT,      // last bytes were IAC DONT
    TS_OPT_WILL,      // last bytes were IAC WILL
    TS_OPT_WONT,      // last bytes were IAC WONT
    TS_SB,            // sub-negotiation: discard until IAC SE
    TS_SB_SAW_IAC,    // in SB, last byte was IAC: next byte is SE or escaped IAC
} telnet_parse_state_t;

typedef struct {
    bool                 active;        // true once we've seen IAC inbound
    telnet_parse_state_t state;

    // Pending IAC bytes that need to go OUT to the peer. Drained by
    // pump_tx before any data bytes. Sized for ~10 short responses.
    uint8_t              pending_tx[64];
    uint8_t              pending_tx_n;
} telnet_state_t;

// Reset telnet state to "raw / detecting" — call on accept_cb,
// connected_cb, on connection close, and at sio_channel_reset.
void telnet_init(telnet_state_t *t);

// Force telnet into active mode and queue our initial negotiation
// (WILL ECHO, WILL SGA, DO SGA, DONT LINEMODE). Use this when the
// channel is dedicated to telnet clients — e.g. on accept of a new
// connection — instead of waiting for the client to send IAC first.
// Many telnet clients (incl. Ubuntu's inetutils-telnet) wait for the
// server to drive negotiation, so passive-only mode deadlocks.
//
// Trade-off: if a raw client (nc) connects, it will receive ~12 bytes
// of IAC negotiation that the Z80 sees as garbage data. Acceptable
// for channels intended for telnet use.
void telnet_start_active(telnet_state_t *t);

// Process one inbound byte (TCP → us). Returns true if `out` should
// be pushed to the channel's RX ringbuf (data byte to deliver to Z80).
// Returns false if the byte was consumed by the IAC parser. May
// append response bytes to t->pending_tx.
bool telnet_recv_byte(telnet_state_t *t, uint8_t b, uint8_t *out);

// Process one outbound byte (Z80 → TCP). Writes to `out` the byte(s)
// that should actually be sent on the wire. Returns the number of
// bytes written (1 normally; 2 if telnet is active and the byte is
// 0xFF, which must be IAC-escaped as 0xFF 0xFF). `out` must have
// space for at least 2 bytes.
size_t telnet_send_byte(const telnet_state_t *t, uint8_t b, uint8_t *out);

#endif
