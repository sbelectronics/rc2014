#include "telnet.h"
#include "config.h"

#include <stdio.h>

// Telnet (RFC 854) command bytes
#define IAC  0xFF   // Interpret As Command
#define DONT 0xFE
#define DO   0xFD
#define WONT 0xFC
#define WILL 0xFB
#define SB   0xFA   // begin sub-negotiation
#define SE   0xF0   // end sub-negotiation
// Other 2-byte commands (NOP, AYT, AO, IP, BRK, DM, EC, EL, GA) — we
// just consume and ignore.

// Telnet options we care about (RFC 855+)
#define OPT_BINARY        0   // RFC 856 — suppress NVT line-ending translation
#define OPT_ECHO          1   // RFC 857
#define OPT_SUPPRESS_GA   3   // RFC 858 — character-at-a-time mode
#define OPT_LINEMODE     34   // RFC 1184 — refuse this so client uses char mode

// ----- USB-side debug logging ----------------------------------------
// Compiled out unless PICONET_TELNET_DEBUG is non-zero in config.h.

#if PICONET_TELNET_DEBUG

static const char *opt_name(uint8_t opt) {
    switch (opt) {
        case 0:  return "BINARY";
        case 1:  return "ECHO";
        case 3:  return "SGA";
        case 5:  return "STATUS";
        case 24: return "TERMINAL-TYPE";
        case 31: return "NAWS";
        case 32: return "TSPEED";
        case 33: return "LFLOW";
        case 34: return "LINEMODE";
        case 35: return "XDISPLAY";
        case 36: return "ENVIRON";
        case 37: return "AUTHENTICATION";
        case 38: return "ENCRYPT";
        case 39: return "NEW-ENVIRON";
        default: return "?";
    }
}

static const char *verb_name(uint8_t v) {
    switch (v) {
        case WILL: return "WILL";
        case WONT: return "WONT";
        case DO:   return "DO";
        case DONT: return "DONT";
        default:   return "?";
    }
}

static void log_in (uint8_t v, uint8_t o) {
    printf("[telnet] <- %s %s (%u)\n", verb_name(v), opt_name(o), o);
}
static void log_out(uint8_t v, uint8_t o) {
    printf("[telnet] -> %s %s (%u)\n", verb_name(v), opt_name(o), o);
}
static void log_msg(const char *m) { printf("[telnet] %s\n", m); }

#else

static inline void log_in (uint8_t v, uint8_t o) { (void)v; (void)o; }
static inline void log_out(uint8_t v, uint8_t o) { (void)v; (void)o; }
static inline void log_msg(const char *m) { (void)m; }

#endif

static void queue_tx(telnet_state_t *t, uint8_t b) {
    if (t->pending_tx_n < sizeof(t->pending_tx)) {
        t->pending_tx[t->pending_tx_n++] = b;
    }
}

static void queue_iac3(telnet_state_t *t, uint8_t verb, uint8_t opt) {
    queue_tx(t, IAC);
    queue_tx(t, verb);
    queue_tx(t, opt);
    log_out(verb, opt);
}

// On first detection of incoming IAC, send our initial negotiation:
// WILL ECHO            — "we'll echo your input" → client turns off local echo
// WILL SUPPRESS-GA     — no go-ahead from us
// DO SUPPRESS-GA       — and we don't want it from client
// DONT LINEMODE        — refuse line mode → forces character-at-a-time
//
// Standard telnet clients respond by switching the local terminal to
// raw / no-local-echo mode. Combined with our IAC stripping in both
// directions, the user gets a clean transparent pipe to the Z80.
static void send_initial_negotiation(telnet_state_t *t) {
    log_msg("sending initial negotiation");
    queue_iac3(t, WILL, OPT_ECHO);
    queue_iac3(t, WILL, OPT_SUPPRESS_GA);
    queue_iac3(t, DO,   OPT_SUPPRESS_GA);
    queue_iac3(t, DONT, OPT_LINEMODE);
    // BINARY (RFC 856): both sides go binary → no NVT line-ending
    // translation, bytes pass verbatim. This is how we keep telnet
    // byte-pure rather than silently filtering CR-LF on receive.
    queue_iac3(t, WILL, OPT_BINARY);
    queue_iac3(t, DO,   OPT_BINARY);
}

void telnet_init(telnet_state_t *t) {
    t->active        = false;
    t->state         = TS_NORMAL;
    t->pending_tx_n  = 0;
}

void telnet_start_active(telnet_state_t *t) {
    if (t->active) return;
    t->active = true;
    send_initial_negotiation(t);
}

bool telnet_recv_byte(telnet_state_t *t, uint8_t b, uint8_t *out) {
    switch (t->state) {

    case TS_NORMAL:
        if (b == IAC) {
            // First-time detection: this is a telnet client. Switch on
            // telnet processing and kick off our initial negotiation.
            if (!t->active) {
                t->active = true;
                send_initial_negotiation(t);
            }
            t->state = TS_SAW_IAC;
            return false;
        }
        // Plain data byte — pass through unchanged. Byte-pure
        // transparency: we never alter, drop, or translate data
        // bytes. Line-ending conventions (CR / LF / CR-LF) are
        // negotiated via the BINARY option, not silently fixed up.
        *out = b;
        return true;

    case TS_SAW_IAC:
        switch (b) {
        case IAC:
            // Escaped literal 0xFF — deliver as data.
            t->state = TS_NORMAL;
            *out = IAC;
            return true;
        case DO:   t->state = TS_OPT_DO;   return false;
        case DONT: t->state = TS_OPT_DONT; return false;
        case WILL: t->state = TS_OPT_WILL; return false;
        case WONT: t->state = TS_OPT_WONT; return false;
        case SB:   t->state = TS_SB;       return false;
        default:
            // 2-byte command (NOP, AYT, etc.) — consume.
            t->state = TS_NORMAL;
            return false;
        }

    case TS_OPT_DO:
        log_in(DO, b);
        // Client wants US to enable option `b`.
        // We agree only on options we actually support.
        if (b == OPT_ECHO || b == OPT_SUPPRESS_GA || b == OPT_BINARY) {
            queue_iac3(t, WILL, b);
        } else {
            queue_iac3(t, WONT, b);
        }
        t->state = TS_NORMAL;
        return false;

    case TS_OPT_DONT:
        log_in(DONT, b);
        // Client tells us NOT to enable option `b`. Politely confirm.
        queue_iac3(t, WONT, b);
        t->state = TS_NORMAL;
        return false;

    case TS_OPT_WILL:
        log_in(WILL, b);
        // Client offers to enable option `b` themselves. We accept
        // SUPPRESS-GA (forces character mode) and BINARY (suppresses
        // NVT line-ending translation, keeps bytes verbatim). Refuse
        // everything else (no terminal-type, no NAWS, no env vars —
        // keep it minimal).
        if (b == OPT_SUPPRESS_GA || b == OPT_BINARY) {
            queue_iac3(t, DO, b);
        } else {
            queue_iac3(t, DONT, b);
        }
        t->state = TS_NORMAL;
        return false;

    case TS_OPT_WONT:
        log_in(WONT, b);
        // Client refuses option `b`. Politely confirm.
        queue_iac3(t, DONT, b);
        t->state = TS_NORMAL;
        return false;

    case TS_SB:
        // Sub-negotiation body — discard until IAC SE.
        if (b == IAC) t->state = TS_SB_SAW_IAC;
        return false;

    case TS_SB_SAW_IAC:
        if (b == SE) {
            t->state = TS_NORMAL;     // end of sub-negotiation
        } else if (b == IAC) {
            t->state = TS_SB;         // escaped IAC inside SB — stay in SB
        } else {
            t->state = TS_SB;         // odd, but keep consuming
        }
        return false;
    }

    // Unreachable.
    t->state = TS_NORMAL;
    return false;
}

size_t telnet_send_byte(const telnet_state_t *t, uint8_t b, uint8_t *out) {
    if (t->active && b == IAC) {
        // Escape literal 0xFF as IAC IAC.
        out[0] = IAC;
        out[1] = IAC;
        return 2;
    }
    out[0] = b;
    return 1;
}
