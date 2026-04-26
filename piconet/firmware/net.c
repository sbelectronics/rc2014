#include "net.h"
#include "sio.h"
#include "hayes.h"
#include "bus.h"
#include "cfg.h"
#include "cdcmenu.h"
#include "config.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netif.h"

#include <stdio.h>
#include <string.h>

// USB-side debug logging for the dial path. Compiled out unless
// PICONET_NET_DEBUG is non-zero in config.h. At runtime, also
// suppressed while the user is in the configuration menu so the
// prompt stays clean.
#if PICONET_NET_DEBUG
#define NET_LOG(fmt, ...) do { \
    if (!cdcmenu_active()) printf("[net] " fmt "\n", ##__VA_ARGS__); \
} while (0)
#else
#define NET_LOG(fmt, ...) ((void)0)
#endif

// lwIP err_t → short human-readable string. Used both for NET_LOG
// output and to populate hayes->last_error for ATL retrieval.
static const char *err_str(err_t e) {
    switch (e) {
        case ERR_OK:         return "ok";
        case ERR_MEM:        return "out of memory";
        case ERR_BUF:        return "buffer error";
        case ERR_TIMEOUT:    return "timeout";
        case ERR_RTE:        return "no route to host";
        case ERR_INPROGRESS: return "in progress";
        case ERR_VAL:        return "illegal value";
        case ERR_WOULDBLOCK: return "would block";
        case ERR_USE:        return "address in use";
        case ERR_ALREADY:    return "already connecting";
        case ERR_ISCONN:     return "already connected";
        case ERR_CONN:       return "not connected";
        case ERR_IF:         return "netif error";
        case ERR_ABRT:       return "aborted";
        case ERR_RST:        return "connection reset";
        case ERR_CLSD:       return "connection closed";
        case ERR_ARG:        return "illegal argument";
        default:             return "unknown error";
    }
}

typedef struct {
    sio_channel_t ch;
    bool          is_outbound;     // NET0/NET1 = true, UART0/UART1 = false
    struct tcp_pcb *pcb;           // active connection (NULL if none)
    struct tcp_pcb *listen_pcb;    // for inbound channels only
    hayes_t       hayes;           // valid if is_outbound
    uint16_t      pending_port;    // set during DNS resolution
    bool          pending_telnet;  // set by net_dial: true for ATDT, false for ATDR
    // Whether telnet IAC processing is engaged on this channel. Always
    // true for inbound (passive parser handles raw clients fine);
    // pending_telnet flips it for each outbound dial.
    bool          telnet_engaged;
} net_chan_t;

static net_chan_t chans[SIO_NUM_CHANNELS];

// ----- forward declarations ------------------------------------------
static err_t recv_cb   (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static void  err_cb    (void *arg, err_t err);
static err_t accept_cb (void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t connected_cb(void *arg, struct tcp_pcb *pcb, err_t err);
static void  dns_cb    (const char *name, const ip_addr_t *ipaddr, void *arg);
static bool  start_connect(net_chan_t *nc, const ip_addr_t *addr, uint16_t port);

// ----- helpers --------------------------------------------------------

static inline net_chan_t *chan_of(sio_channel_t ch) { return &chans[ch]; }

// Encode one outbound byte into `out`, optionally through telnet's
// IAC IAC escaping. Returns the number of bytes written (1 or 2).
// Caller must guarantee `out` has space for at least 2 bytes.
static inline size_t encode_out_byte(net_chan_t *nc, uint8_t b, uint8_t *out) {
    if (nc->telnet_engaged) {
        return telnet_send_byte(&sio_channel(nc->ch)->telnet, b, out);
    }
    out[0] = b;
    return 1;
}

// Enable lwIP TCP keepalive on a connection. The Pico W's CYW43439
// otherwise parks the radio in PM2 between beacons, and the first
// keystroke after ~30s idle eats a wake/DTIM-buffer round trip
// (~100 ms). Trickle one tiny probe every 15 s of true idle to keep
// the radio busy. Also detects dead peers within ~60 s of silence.
static void enable_keepalive(struct tcp_pcb *pcb) {
    ip_set_option(pcb, SOF_KEEPALIVE);
    pcb->keep_idle  = 15000;
    pcb->keep_intvl = 15000;
    pcb->keep_cnt   = 4;
}

static void close_pcb(net_chan_t *nc) {
    if (!nc->pcb) return;
    tcp_arg   (nc->pcb, NULL);
    tcp_recv  (nc->pcb, NULL);
    tcp_sent  (nc->pcb, NULL);
    tcp_err   (nc->pcb, NULL);
    if (tcp_close(nc->pcb) != ERR_OK) {
        tcp_abort(nc->pcb);
    }
    nc->pcb = NULL;
    sio_channel(nc->ch)->connected = false;
    // Telnet engagement is per-connection. Inbound channels re-engage
    // on the next accept_cb; outbound channels re-engage on the next
    // ATDT (or stay disengaged for ATDR).
    nc->telnet_engaged = false;
}

// ----- LWIP callbacks -------------------------------------------------

static err_t recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    net_chan_t *nc = (net_chan_t *)arg;
    if (!nc) return ERR_OK;

    if (!p) {
        // Remote closed (lwIP signals graceful FIN with NULL pbuf).
        NET_LOG("remote closed ch=%d", (int)nc->ch);
        if (nc->is_outbound) hayes_on_remote_close(&nc->hayes,
                                                   "remote closed");
        close_pcb(nc);
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    sio_channel_state_t *s = sio_channel(nc->ch);

    // Outbound channels suppress data delivery to the Z80 while they
    // aren't in DATA mode (e.g. user escaped via +++ and is now in
    // COMMAND). Telnet IAC processing still runs so the server sees
    // our negotiation responses, but the resulting data bytes are
    // dropped — they'd otherwise interleave with AT response strings
    // and confuse the user. ATO (or a fresh dial) returns to DATA
    // and subsequent data flows normally.
    bool deliver = !nc->is_outbound || nc->hayes.state == HAYES_DATA;

    // Worst case after telnet processing: every input byte is a data
    // byte that fits in the ringbuf. IAC sequences shrink the stream,
    // so the upper bound is p->tot_len.
    if (deliver && ringbuf_free(&s->rx) < p->tot_len) {
        return ERR_MEM;
    }

    // Route through the telnet IAC parser if telnet is engaged on this
    // channel. Inbound channels are always engaged (passive parser
    // handles raw `nc` clients as a no-op until they send IAC).
    // Outbound channels are engaged for ATDT, bypassed for ATDR.
    struct pbuf *q;
    if (nc->telnet_engaged) {
        for (q = p; q; q = q->next) {
            const uint8_t *src = (const uint8_t *)q->payload;
            for (u16_t i = 0; i < q->len; i++) {
                uint8_t out;
                if (telnet_recv_byte(&s->telnet, src[i], &out) && deliver) {
                    ringbuf_push(&s->rx, out);
                }
            }
        }
    } else if (deliver) {
        for (q = p; q; q = q->next) {
            const uint8_t *src = (const uint8_t *)q->payload;
            for (u16_t i = 0; i < q->len; i++) {
                ringbuf_push(&s->rx, src[i]);
            }
        }
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void err_cb(void *arg, err_t err) {
    net_chan_t *nc = (net_chan_t *)arg;
    if (!nc) return;
    NET_LOG("err_cb ch=%d err=%d (%s)", (int)nc->ch, (int)err, err_str(err));
    // pcb is already freed by lwIP at this point.
    nc->pcb = NULL;
    sio_channel(nc->ch)->connected = false;
    nc->telnet_engaged = false;
    if (nc->is_outbound) {
        if (nc->hayes.state == HAYES_DIALING) {
            hayes_on_dial_failed(&nc->hayes, err_str(err));
        } else if (nc->hayes.state == HAYES_DATA) {
            hayes_on_remote_close(&nc->hayes, err_str(err));
        }
    }
}

static err_t accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    net_chan_t *nc = (net_chan_t *)arg;
    if (err != ERR_OK || !newpcb) return ERR_VAL;

    if (nc->pcb) {
        // One peer at a time; refuse subsequent connections.
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    nc->pcb = newpcb;
    tcp_arg (newpcb, nc);
    tcp_recv(newpcb, recv_cb);
    tcp_err (newpcb, err_cb);
    enable_keepalive(newpcb);
    sio_channel(nc->ch)->connected = true;
    // Fresh telnet state per connection. Drive negotiation actively —
    // many telnet clients (incl. Ubuntu's inetutils-telnet) wait for
    // the server to initiate. Trade-off: raw `nc` clients see ~18
    // bytes of IAC garbage at start; acceptable for channels intended
    // for telnet use.
    nc->telnet_engaged = true;
    telnet_init(&sio_channel(nc->ch)->telnet);
    telnet_start_active_server(&sio_channel(nc->ch)->telnet);
#if PICONET_TELNET_DEBUG
    if (!cdcmenu_active()) {
        printf("[telnet] accept on ch=%d, queued %u init bytes\n",
               nc->ch, sio_channel(nc->ch)->telnet.pending_tx_n);
    }
#endif
    return ERR_OK;
}

static err_t connected_cb(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)pcb;
    net_chan_t *nc = (net_chan_t *)arg;
    if (err != ERR_OK) {
        NET_LOG("connect FAILED ch=%d err=%d (%s)",
                (int)nc->ch, (int)err, err_str(err));
        if (nc->is_outbound) hayes_on_dial_failed(&nc->hayes, err_str(err));
        nc->pcb = NULL;
        sio_channel(nc->ch)->connected = false;
        return err;
    }
    NET_LOG("connect OK ch=%d", (int)nc->ch);
    sio_channel(nc->ch)->connected = true;
    // Engage client-side telnet now that the TCP handshake completed,
    // if this dial requested it (ATDT). pending_telnet was set by
    // net_dial. ATDR leaves telnet_engaged false → raw passthrough.
    if (nc->is_outbound && nc->pending_telnet) {
        nc->telnet_engaged = true;
        telnet_init(&sio_channel(nc->ch)->telnet);
        telnet_start_active_client(&sio_channel(nc->ch)->telnet);
    }
    if (nc->is_outbound) hayes_on_connect(&nc->hayes);
    return ERR_OK;
}

static void dns_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    net_chan_t *nc = (net_chan_t *)arg;
    if (!ipaddr) {
        NET_LOG("dns FAILED name=%s", name ? name : "?");
        if (nc->is_outbound) hayes_on_dial_failed(&nc->hayes,
                                                  "DNS lookup failed");
        return;
    }
    NET_LOG("dns OK name=%s addr=%s", name ? name : "?",
            ipaddr_ntoa(ipaddr));
    if (!start_connect(nc, ipaddr, nc->pending_port)) {
        // tcp_new() or tcp_connect() failed synchronously — without
        // this branch the channel would sit in DIALING forever.
        if (nc->is_outbound) hayes_on_dial_failed(&nc->hayes,
                                                  "tcp_connect failed");
    }
}

// ----- outbound helpers ----------------------------------------------

static bool start_connect(net_chan_t *nc, const ip_addr_t *addr, uint16_t port) {
    nc->pcb = tcp_new();
    if (!nc->pcb) {
        NET_LOG("tcp_new FAILED ch=%d", (int)nc->ch);
        return false;
    }
    tcp_arg(nc->pcb, nc);
    tcp_recv(nc->pcb, recv_cb);
    tcp_err (nc->pcb, err_cb);
    enable_keepalive(nc->pcb);
    NET_LOG("tcp_connect ch=%d to %s:%u", (int)nc->ch,
            ipaddr_ntoa(addr), (unsigned)port);
    err_t err = tcp_connect(nc->pcb, addr, port, connected_cb);
    if (err != ERR_OK) {
        NET_LOG("tcp_connect sync err=%d (%s)", (int)err, err_str(err));
        tcp_abort(nc->pcb);
        nc->pcb = NULL;
        return false;
    }
    return true;
}

// ----- public API ----------------------------------------------------

bool net_dial(sio_channel_t ch, const char *host, uint16_t port, bool use_telnet) {
    if (ch != SIO_CH_NET0 && ch != SIO_CH_NET1) return false;
    net_chan_t *nc = chan_of(ch);
    if (nc->pcb) return false;

    nc->pending_port   = port;
    nc->pending_telnet = use_telnet;
    // Engagement is set in connected_cb after TCP handshake. Force off
    // here so that any race or stale value from a prior dial can't
    // accidentally enable telnet processing for an ATDR dial.
    nc->telnet_engaged = false;

    NET_LOG("dial ch=%d host=%s port=%u %s",
            (int)ch, host, (unsigned)port,
            use_telnet ? "(telnet)" : "(raw)");

    ip_addr_t addr;
    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(host, &addr, dns_cb, nc);
    bool result;
    if (err == ERR_OK) {
        NET_LOG("dns cached addr=%s", ipaddr_ntoa(&addr));
        result = start_connect(nc, &addr, port);
    } else if (err == ERR_INPROGRESS) {
        NET_LOG("dns lookup in progress for %s", host);
        result = true;     // dns_cb will call start_connect later
    } else {
        NET_LOG("dns sync err=%d (%s)", (int)err, err_str(err));
        result = false;
    }
    cyw43_arch_lwip_end();
    return result;
}

void net_hangup(sio_channel_t ch) {
    if (ch != SIO_CH_NET0 && ch != SIO_CH_NET1) return;
    net_chan_t *nc = chan_of(ch);
    cyw43_arch_lwip_begin();
    close_pcb(nc);
    cyw43_arch_lwip_end();
}

bool net_is_connected(sio_channel_t ch) {
    return sio_channel(ch)->connected;
}

bool net_is_dialing(sio_channel_t ch) {
    if (ch != SIO_CH_NET0 && ch != SIO_CH_NET1) return false;
    return chan_of(ch)->hayes.state == HAYES_DIALING;
}

// ----- TX pump --------------------------------------------------------

// Drain bus → TCP for one channel. Holds lwip lock briefly. For NET
// channels in command mode we still drain (the Hayes parser consumes
// bytes), but we do not push to TCP unless in DATA mode. Note that
// `hayes_on_tx_byte` returns true if the byte was consumed (do not
// forward).
static void pump_tx(net_chan_t *nc) {
    sio_channel_state_t *s = sio_channel(nc->ch);

    // For NET channels: feed every TX byte through the Hayes parser.
    // Parser may consume (return true), pass through (return false),
    // or append bytes to `pending_tx` (flushed pluses). The lwIP lock
    // is held for the entire hayes interaction so that recv_cb (which
    // fires from the lwIP background worker's IRQ context in
    // threadsafe_background mode) cannot race with any push_response
    // call hayes makes into the RX ringbuf.
    if (nc->is_outbound) {
        uint8_t buf[128];
        size_t n = 0;

        cyw43_arch_lwip_begin();

        // 1. Telnet pending_tx (IAC negotiation responses, only
        //    populated when telnet_engaged). Goes first so protocol
        //    bytes precede data on the wire — peer applies negotiation
        //    state before interpreting subsequent bytes. Never IAC-
        //    escape these — they ARE IAC bytes.
        if (nc->telnet_engaged) {
#if PICONET_TELNET_DEBUG
            if (s->telnet.pending_tx_n > 0 && !cdcmenu_active()) {
                printf("[telnet] pump_tx (out) flushing %u IAC bytes (ch=%d)\n",
                       s->telnet.pending_tx_n, nc->ch);
            }
#endif
            for (uint8_t i = 0;
                 i < s->telnet.pending_tx_n && n < sizeof(buf);
                 i++) {
                buf[n++] = s->telnet.pending_tx[i];
            }
            s->telnet.pending_tx_n = 0;
        }

        // 2. Hayes-queued bytes (e.g. flushed pluses from a timed-out
        //    +++ sequence). These are user data — escape via telnet
        //    when engaged.
        for (uint8_t i = 0; i < nc->hayes.pending_tx_n && n + 2 <= sizeof(buf); i++) {
            n += encode_out_byte(nc, nc->hayes.pending_tx[i], &buf[n]);
        }
        nc->hayes.pending_tx_n = 0;

        // 3. Drain TX ringbuf — feed each byte through Hayes parser;
        //    forward to the wire only if not consumed and we're in
        //    DATA mode. Reserve 2 slots in buf for telnet escape worst
        //    case (0xFF → IAC IAC).
        uint8_t b;
        while (n + 2 <= sizeof(buf) && ringbuf_pop(&s->tx, &b)) {
            bool consumed = hayes_on_tx_byte(&nc->hayes, b);

            // Hayes may have just appended flushed pluses — emit them
            // BEFORE the current byte.
            for (uint8_t i = 0; i < nc->hayes.pending_tx_n && n + 2 <= sizeof(buf); i++) {
                n += encode_out_byte(nc, nc->hayes.pending_tx[i], &buf[n]);
            }
            nc->hayes.pending_tx_n = 0;

            if (!consumed && n + 2 <= sizeof(buf)) {
                if (nc->hayes.state == HAYES_DATA && nc->pcb) {
                    n += encode_out_byte(nc, b, &buf[n]);
                }
                // Otherwise the byte is silently dropped (DIALING, or
                // COMMAND mode non-AT byte — shouldn't happen in DATA).
            }
        }

        if (n && nc->pcb) {
            u16_t free_space = tcp_sndbuf(nc->pcb);
            if (free_space > 0) {
                u16_t to_write = (n < free_space) ? (u16_t)n : free_space;
                if (tcp_write(nc->pcb, buf, to_write, TCP_WRITE_FLAG_COPY) == ERR_OK) {
                    tcp_output(nc->pcb);
                }
            }
        }

        cyw43_arch_lwip_end();
    } else {
        // Inbound channel — TX bytes go through the telnet filter.
        // The whole interaction with telnet.pending_tx + tcp_write must
        // happen under the lwIP lock so recv_cb (which fires from the
        // background worker and can write telnet.pending_tx via the
        // IAC parser) cannot preempt mid-drain.
        if (!nc->pcb) return;

        cyw43_arch_lwip_begin();

        u16_t free_space = tcp_sndbuf(nc->pcb);
        if (free_space > 0) {
            uint8_t buf[128];
            u16_t n = 0;

            // 1. Drain telnet pending_tx (negotiation responses) FIRST,
            //    so option-negotiation bytes always precede data on the
            //    wire — clients react to negotiation before deciding how
            //    to display data.
#if PICONET_TELNET_DEBUG
            if (s->telnet.pending_tx_n > 0 && !cdcmenu_active()) {
                printf("[telnet] pump_tx flushing %u IAC bytes (ch=%d)\n",
                       s->telnet.pending_tx_n, nc->ch);
            }
#endif
            for (uint8_t i = 0;
                 i < s->telnet.pending_tx_n && n < sizeof(buf) && n < free_space;
                 i++) {
                buf[n++] = s->telnet.pending_tx[i];
            }
            s->telnet.pending_tx_n = 0;

            // 2. Drain TX ringbuf, escaping bytes as needed. Reserve 2
            //    slots in buf for the worst case (0xFF needs IAC IAC).
            uint8_t b;
            while (n + 2 <= sizeof(buf) && n + 2 <= free_space &&
                   ringbuf_pop(&s->tx, &b)) {
                n += telnet_send_byte(&s->telnet, b, &buf[n]);
            }

            if (n) {
                if (tcp_write(nc->pcb, buf, n, TCP_WRITE_FLAG_COPY) == ERR_OK) {
                    tcp_output(nc->pcb);
                }
            }
        }

        cyw43_arch_lwip_end();
    }
}

// ----- bring-up -------------------------------------------------------

// State of the WiFi bring-up tracked across retries so the heartbeat
// can show progress even before the host attaches picocom.
static volatile int wifi_attempts = 0;
static volatile int wifi_last_err = 0;
static volatile bool wifi_inited  = false;
static volatile bool wifi_assoc   = false;

bool net_wifi_associated(void) { return wifi_assoc; }

bool net_get_ip_str(char *out, size_t cap) {
    if (cap == 0) return false;
    out[0] = 0;
    if (!wifi_inited || !netif_default) return false;
    const ip4_addr_t *ip = netif_ip4_addr(netif_default);
    if (!ip || !ip4_addr_get_u32(ip)) return false;
    snprintf(out, cap, "%s", ip4addr_ntoa(ip));
    return true;
}

bool net_get_mac_str(char *out, size_t cap) {
    if (cap == 0) return false;
    out[0] = 0;
    if (!wifi_inited) return false;
    uint8_t mac[6];
    if (cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac) != 0) return false;
    snprintf(out, cap, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

static bool wifi_try_connect(void) {
    if (!wifi_inited) {
        if (cyw43_arch_init() != 0) {
            wifi_last_err = -1;   // init failure
            return false;
        }
        cyw43_arch_enable_sta_mode();
        wifi_inited = true;
    }

    cfg_t *cfg = cfg_get();
    wifi_attempts++;
    int err = cyw43_arch_wifi_connect_timeout_ms(cfg->wifi.ssid,
                                                 cfg->wifi.psk,
                                                 cfg_auth_to_cyw43(cfg->wifi.auth),
                                                 30000);
    if (err != 0) {
        wifi_last_err = err;
        return false;
    }

    cyw43_arch_lwip_begin();
    netif_set_hostname(netif_default, PICONET_HOSTNAME);
    cyw43_arch_lwip_end();

    // Best-effort PM nudge after association — not relied upon. The
    // cyw43 driver re-engages PM in practice regardless of this call;
    // first-keystroke wake latency is solved at the TCP layer instead
    // via SOF_KEEPALIVE on each connection (see enable_keepalive).
    cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);

    wifi_assoc = true;
    wifi_last_err = 0;
    return true;
}

static bool start_listener(net_chan_t *nc, uint16_t port) {
    cyw43_arch_lwip_begin();
    struct tcp_pcb *pcb = tcp_new();
    bool ok = false;
    if (pcb) {
        if (tcp_bind(pcb, IP_ANY_TYPE, port) == ERR_OK) {
            struct tcp_pcb *lpcb = tcp_listen_with_backlog(pcb, 1);
            if (lpcb) {
                tcp_arg(lpcb, nc);
                tcp_accept(lpcb, accept_cb);
                nc->listen_pcb = lpcb;
                ok = true;
            }
        }
        if (!ok) tcp_close(pcb);
    }
    cyw43_arch_lwip_end();
    return ok;
}

// ----- diagnostics ----------------------------------------------------

static const char *hayes_state_name(hayes_state_t s) {
    switch (s) {
        case HAYES_DATA:    return "DATA";
        case HAYES_DIALING: return "DIAL";
        case HAYES_COMMAND: return "CMD";
    }
    return "?";
}

static const char *channel_name(int i) {
    static const char *n[] = {"UART0", "UART1", "NET0 ", "NET1 "};
    return n[i];
}

static void diag_tick(void) {
    static absolute_time_t next_hb;
    static bool first = true;
    static bool ip_printed = false;
    if (first) {
        first = false;
        next_hb = make_timeout_time_ms(2000);
    }

    // No background output while the user is in the config menu —
    // the prompt would get trashed.
    if (cdcmenu_active()) return;

    // One-shot IP print as soon as DHCP returns one (only meaningful
    // once cyw43 has been initialised). Always emitted regardless of
    // the heartbeat toggle — it's a one-time provisioning event.
    if (!ip_printed && wifi_inited && netif_default) {
        const ip4_addr_t *ip = netif_ip4_addr(netif_default);
        if (ip && ip4_addr_get_u32(ip) != 0) {
            printf("DHCP: ip=%s netmask=%s gw=%s\n",
                   ip4addr_ntoa(ip),
                   ip4addr_ntoa(netif_ip4_netmask(netif_default)),
                   ip4addr_ntoa(netif_ip4_gw(netif_default)));
            ip_printed = true;
        }
    }

    if (absolute_time_diff_us(get_absolute_time(), next_hb) > 0) return;
    next_hb = make_timeout_time_ms(5000);

    // Heartbeat block — full diagnostic dump. Suppressed when the
    // user has turned heartbeat off; the menu hint below always
    // fires so they know how to get back in.
    if (cdcmenu_heartbeat_enabled()) {
        uint32_t up_s = to_ms_since_boot(get_absolute_time()) / 1000;

        if (wifi_inited) {
            int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            const ip4_addr_t *ip = netif_default ? netif_ip4_addr(netif_default) : NULL;
            printf("[hb %us] wifi=%d ip=%s attempts=%d lastErr=%d busRst=%lu\n",
                   (unsigned)up_s, link,
                   (ip && ip4_addr_get_u32(ip)) ? ip4addr_ntoa(ip) : "0.0.0.0",
                   wifi_attempts, wifi_last_err,
                   (unsigned long)bus_stats.reset_count);
        } else {
            printf("[hb %us] wifi=uninit attempts=%d lastErr=%d busRst=%lu\n",
                   (unsigned)up_s, wifi_attempts, wifi_last_err,
                   (unsigned long)bus_stats.reset_count);
        }

        // Live GPIO snapshot (read on demand, no per-edge IRQ overhead).
        bus_snapshot_t snap;
        bus_snapshot(&snap);
        printf("  live: A=0x%X /CS=%d /RD=%d /WR=%d /M1=%d RSTsense=%d\n",
               snap.a, snap.cs, snap.rd, snap.wr, snap.m1, snap.reset_sense);

        uint32_t total_r = 0, total_w = 0;
        for (int i = 0; i < 16; i++) {
            total_r += bus_stats.reads_per_reg[i];
            total_w += bus_stats.writes_per_reg[i];
        }
        printf("  pio:   R=%lu W=%lu lastR=0x%X lastW=0x%X(=0x%02X)\n",
               (unsigned long)total_r, (unsigned long)total_w,
               bus_stats.last_read_reg,
               bus_stats.last_write_reg, bus_stats.last_write_data);
        if (total_r || total_w) {
            printf("       perReg:");
            for (int i = 0; i < 16; i++) {
                uint32_t r = bus_stats.reads_per_reg[i];
                uint32_t w = bus_stats.writes_per_reg[i];
                if (r || w) {
                    printf(" [%X]R%lu/W%lu",
                           i, (unsigned long)r, (unsigned long)w);
                }
            }
            printf("\n");
        }

        for (int i = 0; i < SIO_NUM_CHANNELS; i++) {
            sio_channel_state_t *s = sio_channel(i);
            char modebuf[20] = "";
            if (i >= SIO_CH_NET0) {
                snprintf(modebuf, sizeof(modebuf), " hayes=%s",
                         hayes_state_name(chans[i].hayes.state));
            }
            printf("  %s: conn=%d rx=%lu/%u tx=%lu/%u%s\n",
                   channel_name(i), (int)s->connected,
                   (unsigned long)ringbuf_count(&s->rx),
                   (unsigned)PICONET_RX_RING_SIZE,
                   (unsigned long)ringbuf_count(&s->tx),
                   (unsigned)PICONET_TX_RING_SIZE,
                   modebuf);
        }
    }

    // Menu hint. Always fires (when not in the menu and the timer
    // elapsed) so the user always has a reminder of how to get in,
    // whether or not heartbeat output is enabled.
    printf("  [press any key on USB CDC for the configuration menu]\n");
}

// ----- main loop ------------------------------------------------------

void net_core0_main(void) {
    // Init channel descriptors.
    for (int i = 0; i < SIO_NUM_CHANNELS; i++) {
        chans[i].ch          = (sio_channel_t)i;
        chans[i].is_outbound = (i == SIO_CH_NET0 || i == SIO_CH_NET1);
        chans[i].pcb         = NULL;
        chans[i].listen_pcb  = NULL;
        if (chans[i].is_outbound) hayes_init(&chans[i].hayes, (sio_channel_t)i);
    }

    // Link LED off until WiFi associates.
    gpio_init(PICONET_PIN_LED_LINK);
    gpio_set_dir(PICONET_PIN_LED_LINK, GPIO_OUT);
    gpio_put(PICONET_PIN_LED_LINK, 0);

    bool listeners_started = false;
    absolute_time_t next_wifi_attempt = nil_time;

    for (;;) {
        // Always-responsive USB-CDC menu. Polled every iteration so
        // any keystroke from the host drops into provisioning mode.
        cdcmenu_poll();

        // Pause all bring-up work while the user is in the menu.
        // Otherwise a half-edited config (e.g. SSID set but PSK still
        // empty) would kick off a doomed connect attempt mid-prompt
        // and trash the menu output. WiFi attempts resume on EXIT.
        if (!cdcmenu_active()) {
            // Drive WiFi connection state forward — only if we have a
            // usable config. With no SSID we sit idle and wait for the
            // user to provision via the USB-CDC menu (then REBOOT).
            if (!wifi_assoc && cfg_is_usable()) {
                if (is_nil_time(next_wifi_attempt) ||
                    absolute_time_diff_us(get_absolute_time(), next_wifi_attempt) <= 0) {
                    printf("WiFi: connecting to '%s' (attempt %d)...\n",
                           cfg_get()->wifi.ssid, wifi_attempts + 1);
                    if (wifi_try_connect()) {
                        printf("WiFi: associated\n");
                        gpio_put(PICONET_PIN_LED_LINK, 1);
                    } else {
                        printf("WiFi: attempt failed (err=%d), retrying in 5s\n",
                               wifi_last_err);
                        next_wifi_attempt = make_timeout_time_ms(5000);
                    }
                }
            } else if (wifi_assoc && !listeners_started) {
                if (!start_listener(&chans[SIO_CH_UART0], PICONET_UART0_LISTEN_PORT))
                    printf("UART0 listen on %d failed\n", PICONET_UART0_LISTEN_PORT);
                if (!start_listener(&chans[SIO_CH_UART1], PICONET_UART1_LISTEN_PORT))
                    printf("UART1 listen on %d failed\n", PICONET_UART1_LISTEN_PORT);
                listeners_started = true;
                printf("PICONET ready\n");
            }
        }

        // TCP/Hayes machinery is only safe once lwIP and the listeners
        // are alive. Heartbeat runs unconditionally so diagnostics work
        // even before WiFi associates.
        if (listeners_started) {
            for (int i = 0; i < SIO_NUM_CHANNELS; i++) pump_tx(&chans[i]);

            // hayes_tick may push_response into RX ringbuf (e.g. when
            // the +++ guard-time elapses and it emits "OK"); take the
            // lwIP lock so recv_cb can't race with that push.
            uint32_t now = to_ms_since_boot(get_absolute_time());
            cyw43_arch_lwip_begin();
            hayes_tick(&chans[SIO_CH_NET0].hayes, now);
            hayes_tick(&chans[SIO_CH_NET1].hayes, now);
            cyw43_arch_lwip_end();
        }

        diag_tick();

        // Threadsafe-background mode — no explicit poll required, but a
        // small sleep keeps the core from spinning.
        sleep_us(500);
    }
}
