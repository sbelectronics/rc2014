#ifndef PICONET_NET_H
#define PICONET_NET_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "sio.h"

// Core-0 entry point: bring up cyw43_arch + LWIP, open the two inbound
// listening sockets (UART0/UART1), and enter the main event loop. The
// loop pumps each channel's TCP <-> ringbuf moves and the Hayes parsers
// for NET0/NET1.
void net_core0_main(void);

// Outbound API used by the Hayes parser. Returns true if the dial
// attempt was scheduled successfully (transitions channel to "dialing");
// false if the parameters were invalid or another connection is open.
//
// `use_telnet` selects between ATDT (telnet — IAC negotiation on
// connect, IAC IAC escaping in both directions) and ATDR (raw — bytes
// pass through verbatim). See PICONET.md §3.6.
bool net_dial(sio_channel_t ch, const char *host, uint16_t port, bool use_telnet);

// Drop the outbound TCP connection on `ch`, if any.
void net_hangup(sio_channel_t ch);

// Returns true if `ch` is a NET channel currently connected to a remote
// peer (TCP ESTABLISHED).
bool net_is_connected(sio_channel_t ch);

// Returns true if `ch` is a NET channel currently in the middle of a
// TCP connect() attempt.
bool net_is_dialing(sio_channel_t ch);

// True once the cyw43 driver has associated to the configured AP.
// The menu's SAVE handler uses this to choose between "EXIT to apply"
// (no association yet, bring-up will pick up the new cfg) and "REBOOT
// to apply" (already associated, must restart to switch credentials).
bool net_wifi_associated(void);

// Format the assigned IPv4 address into `out[cap]`. Returns true if
// DHCP has produced an address; false (and writes "") otherwise.
bool net_get_ip_str(char *out, size_t cap);

// Format the WiFi station MAC as "aa:bb:cc:dd:ee:ff". Returns true
// once cyw43 has been initialised; false (and writes "") otherwise.
bool net_get_mac_str(char *out, size_t cap);

#endif
