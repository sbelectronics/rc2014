#include "hayes.h"
#include "net.h"
#include "config.h"

#include "pico/stdlib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void push_string(sio_channel_t ch, const char *s) {
    sio_channel_state_t *c = sio_channel(ch);
    while (*s) {
        if (!ringbuf_push(&c->rx, (uint8_t)*s)) break;
        s++;
    }
}

static void push_response(sio_channel_t ch, const char *resp) {
    push_string(ch, "\r\n");
    push_string(ch, resp);
    push_string(ch, "\r\n");
}

// Default host/port for a NET channel — from config.h.
static const char *default_host(sio_channel_t ch) {
    return (ch == SIO_CH_NET0) ? PICONET_NET0_DEFAULT_HOST
                               : PICONET_NET1_DEFAULT_HOST;
}
static uint16_t default_port(sio_channel_t ch) {
    return (ch == SIO_CH_NET0) ? PICONET_NET0_DEFAULT_PORT
                               : PICONET_NET1_DEFAULT_PORT;
}

void hayes_init(hayes_t *h, sio_channel_t ch) {
    memset(h, 0, sizeof(*h));
    h->ch    = ch;
    h->state = HAYES_COMMAND;
    h->echo  = true;        // ATE1 default — match traditional Hayes
}

static void set_last_error(hayes_t *h, const char *reason) {
    if (!reason) reason = "unknown";
    strncpy(h->last_error, reason, sizeof(h->last_error) - 1);
    h->last_error[sizeof(h->last_error) - 1] = 0;
}

// Echo one TX byte back into the channel's RX ringbuf so it appears
// on the user's terminal. Best-effort: drops if RX is full (which is
// fine — echo is for human visibility, not for protocol correctness).
static void echo_byte(sio_channel_t ch, uint8_t b) {
    sio_channel_state_t *c = sio_channel(ch);
    ringbuf_push(&c->rx, b);
}

bool hayes_in_data_mode(const hayes_t *h) {
    return h->state == HAYES_DATA;
}

// ----- AT command execution ------------------------------------------

// Parse "host:port" or just "host" (uses default port). Returns true if
// the parse succeeded and writes into `host_out`/`port_out`. host_out
// must be at least 64 bytes.
static bool parse_host_port(const char *s, char *host_out, size_t host_cap,
                            uint16_t *port_out, uint16_t default_p) {
    while (*s == ' ') s++;
    if (!*s) return false;

    const char *colon = strchr(s, ':');
    size_t host_len = colon ? (size_t)(colon - s) : strlen(s);
    if (host_len == 0 || host_len >= host_cap) return false;
    memcpy(host_out, s, host_len);
    host_out[host_len] = 0;

    if (colon) {
        int p = atoi(colon + 1);
        if (p <= 0 || p > 65535) return false;
        *port_out = (uint16_t)p;
    } else {
        *port_out = default_p;
    }
    return true;
}

static void exec_command(hayes_t *h, const char *line) {
    // Skip leading "AT"
    if (line[0] != 'A' && line[0] != 'a') { push_response(h->ch, "ERROR"); return; }
    if (line[1] != 'T' && line[1] != 't') { push_response(h->ch, "ERROR"); return; }
    const char *p = line + 2;

    // Bare "AT"
    if (*p == 0) { push_response(h->ch, "OK"); return; }

    char c = (char)toupper((unsigned char)*p++);
    switch (c) {
        case 'D': {
            // Dial command. Optional one-letter modifier selects the
            // outbound protocol:
            //   ATDT <host:port>  → telnet (IAC negotiation on connect)
            //   ATDR <host:port>  → raw passthrough (no telnet)
            //   ATD  <host:port>  → alias for ATDT (telnet is default)
            // Bare ATD/ATDT/ATDR with no argument uses the build-time
            // default host:port for this channel.
            bool use_telnet = true;
            if (*p == 'T' || *p == 't') {
                use_telnet = true;
                p++;
            } else if (*p == 'R' || *p == 'r') {
                use_telnet = false;
                p++;
            }

            char host[64];
            uint16_t port;
            bool ok;
            if (*p == 0) {
                strncpy(host, default_host(h->ch), sizeof(host));
                host[sizeof(host) - 1] = 0;
                port = default_port(h->ch);
                ok = true;
            } else {
                ok = parse_host_port(p, host, sizeof(host), &port,
                                     default_port(h->ch));
            }
            if (!ok) { push_response(h->ch, "ERROR"); return; }
            if (!net_dial(h->ch, host, port, use_telnet)) {
                push_response(h->ch, "ERROR");
                return;
            }
            h->state = HAYES_DIALING;
            // No response yet — hayes_on_connect / hayes_on_dial_failed
            // will emit CONNECT or NO CARRIER.
            return;
        }
        case 'H':
            net_hangup(h->ch);
            push_response(h->ch, "OK");
            return;
        case 'O':
            if (!net_is_connected(h->ch)) {
                push_response(h->ch, "NO CARRIER");
                return;
            }
            push_response(h->ch, "CONNECT");
            h->state = HAYES_DATA;
            return;
        case 'I': {
            char buf[96];
            snprintf(buf, sizeof(buf), "PICONET %s", PICONET_HOSTNAME);
            push_response(h->ch, buf);
            push_response(h->ch, "OK");
            return;
        }
        case 'E':
            // ATE0 = echo off, ATE1 = echo on, bare ATE = off (matches
            // the Smartmodem 1200 default behaviour).
            if (*p == 0 || *p == '0') {
                h->echo = false;
            } else if (*p == '1') {
                h->echo = true;
            } else {
                push_response(h->ch, "ERROR");
                return;
            }
            push_response(h->ch, "OK");
            return;
        case 'L':
            // ATL — print last dial-failure reason then OK. Reason is
            // populated by the network layer via hayes_on_dial_failed
            // and cleared on a successful CONNECT.
            if (*p != 0) { push_response(h->ch, "ERROR"); return; }
            push_response(h->ch, h->last_error[0] ? h->last_error
                                                  : "no error");
            push_response(h->ch, "OK");
            return;
        case 'Z':
            net_hangup(h->ch);
            h->state = HAYES_COMMAND;
            h->plus_count = 0;
            h->last_error[0] = 0;
            push_response(h->ch, "OK");
            return;
        default:
            push_response(h->ch, "ERROR");
            return;
    }
}

// ----- TX byte dispatch ----------------------------------------------

bool hayes_on_tx_byte(hayes_t *h, uint8_t b) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    switch (h->state) {
        case HAYES_DIALING:
            // Drop bytes silently while dialling.
            return true;

        case HAYES_DATA: {
            // +++ escape detection. Need ≥ guard-ms of TX silence before
            // the first '+', tight inter-plus spacing, then ≥ guard-ms
            // of silence after the third (enforced in hayes_tick).
            if (b == '+') {
                if (h->plus_count == 0) {
                    uint32_t since_data = now - h->last_tx_time_ms;
                    if (since_data >= PICONET_HAYES_GUARD_MS) {
                        h->plus_count = 1;
                        h->plus_last_time_ms = now;
                        return true;            // hold this '+'
                    }
                    // Insufficient pre-silence → pass through.
                } else {
                    uint32_t since_plus = now - h->plus_last_time_ms;
                    if (since_plus < PICONET_HAYES_GUARD_MS / 2 &&
                        h->plus_count < 3) {
                        h->plus_count++;
                        h->plus_last_time_ms = now;
                        return true;            // hold
                    }
                    // Inter-plus spacing too wide — flush held pluses
                    // into pending_tx so pump_tx emits them BEFORE the
                    // current byte, without violating the TX ringbuf's
                    // SPSC invariant.
                    for (int i = 0; i < h->plus_count; i++) {
                        if (h->pending_tx_n < sizeof(h->pending_tx)) {
                            h->pending_tx[h->pending_tx_n++] = '+';
                        }
                    }
                    h->plus_count = 0;
                    // Fall through: this current '+' will be passed on too.
                }
            } else if (h->plus_count > 0) {
                // Non-plus byte breaks the sequence — flush held pluses.
                for (int i = 0; i < h->plus_count; i++) {
                    if (h->pending_tx_n < sizeof(h->pending_tx)) {
                        h->pending_tx[h->pending_tx_n++] = '+';
                    }
                }
                h->plus_count = 0;
            }

            h->last_tx_time_ms = now;
            return false;                       // pass through to TCP
        }

        case HAYES_COMMAND: {
            h->last_tx_time_ms = now;
            // Collect up to CR; ignore LFs. Treat BS/DEL as line edit.
            // Echo each handled byte back if ATE1 is in effect (default).
            if (b == '\r') {
                if (h->echo) {
                    // CR → CR LF so the firmware response that follows
                    // lands on its own line.
                    echo_byte(h->ch, '\r');
                    echo_byte(h->ch, '\n');
                }
                h->cmdline[h->cmdlen] = 0;
                if (h->cmdlen > 0) {
                    exec_command(h, h->cmdline);
                }
                h->cmdlen = 0;
                return true;
            }
            if (b == '\n') return true;     // never echo bare LF
            if (b == 0x08 || b == 0x7F) {
                if (h->cmdlen > 0) {
                    h->cmdlen--;
                    if (h->echo) {
                        // BS SP BS — visually erase the previous char.
                        echo_byte(h->ch, 0x08);
                        echo_byte(h->ch, ' ');
                        echo_byte(h->ch, 0x08);
                    }
                }
                return true;
            }
            if (h->cmdlen < sizeof(h->cmdline) - 1) {
                h->cmdline[h->cmdlen++] = (char)b;
                if (h->echo) echo_byte(h->ch, b);
            }
            return true;
        }
    }
    return false;
}

// ----- periodic tick -------------------------------------------------

void hayes_tick(hayes_t *h, uint32_t now_ms) {
    if (h->state != HAYES_DATA || h->plus_count == 0) return;

    uint32_t since_plus = now_ms - h->plus_last_time_ms;

    if (h->plus_count == 3) {
        // Three pluses held; commit once post-guard has elapsed.
        if (since_plus >= PICONET_HAYES_GUARD_MS) {
            h->state = HAYES_COMMAND;
            h->plus_count = 0;
            h->cmdlen = 0;
            push_response(h->ch, "OK");
        }
    } else {
        // Fewer than three pluses and the sequence timed out — flush
        // to pending_tx (same reason as the in-place flush: can't touch
        // the bus-side producer's ringbuf from here).
        if (since_plus >= PICONET_HAYES_GUARD_MS) {
            for (int i = 0; i < h->plus_count; i++) {
                if (h->pending_tx_n < sizeof(h->pending_tx)) {
                    h->pending_tx[h->pending_tx_n++] = '+';
                }
            }
            h->plus_count = 0;
        }
    }
}

// ----- dialing callbacks ---------------------------------------------

void hayes_on_connect(hayes_t *h) {
    if (h->state != HAYES_DIALING) return;
    push_response(h->ch, "CONNECT");
    h->state = HAYES_DATA;
    h->last_tx_time_ms = to_ms_since_boot(get_absolute_time());
    h->plus_count = 0;
    h->last_error[0] = 0;       // success — clear previous failure
}

void hayes_on_dial_failed(hayes_t *h, const char *reason) {
    if (h->state != HAYES_DIALING) return;
    set_last_error(h, reason);
    push_response(h->ch, "NO CARRIER");
    h->state = HAYES_COMMAND;
    h->cmdlen = 0;
}

void hayes_on_remote_close(hayes_t *h, const char *reason) {
    if (h->state != HAYES_DATA) return;
    set_last_error(h, reason ? reason : "remote closed");
    push_response(h->ch, "NO CARRIER");
    h->state = HAYES_COMMAND;
    h->cmdlen = 0;
    h->plus_count = 0;
}
