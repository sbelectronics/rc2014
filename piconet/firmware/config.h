#ifndef PICONET_CONFIG_H
#define PICONET_CONFIG_H

// Build-time fallback for WiFi credentials. config_local.h is git-
// ignored. Empty SSID/PSK strings are valid (force USB-CDC
// provisioning on first boot); fill them in to bake credentials into
// the firmware so a fresh board comes up on WiFi without manual
// provisioning. The runtime source of truth is the persistent flash
// config; config_local.h values are used only when flash hasn't been
// provisioned yet.
#include "config_local.h"

#define PICONET_HOSTNAME        "piconet"

// Inbound listen ports (UART0 = SIO/2 #1 Ch A, UART1 = SIO/2 #1 Ch B).
// A remote peer connects in to one of these; bytes flow straight through.
#define PICONET_UART0_LISTEN_PORT   2300
#define PICONET_UART1_LISTEN_PORT   2301

// Outbound default host:port for ATD-with-no-argument
// (NET0 = SIO/2 #2 Ch A, NET1 = SIO/2 #2 Ch B).
#define PICONET_NET0_DEFAULT_HOST   "example.com"
#define PICONET_NET0_DEFAULT_PORT   23
#define PICONET_NET1_DEFAULT_HOST   "example.com"
#define PICONET_NET1_DEFAULT_PORT   23

// Per-channel ring buffer sizes (bytes). Must be a power of two.
#define PICONET_RX_RING_SIZE        2048
#define PICONET_TX_RING_SIZE        2048

// Hayes guard time (ms of TX-side silence required either side of "+++").
#define PICONET_HAYES_GUARD_MS      1000

// Set to 1 to log telnet IAC negotiation traffic over USB-CDC for
// debugging. Each direction logs verb + option name. Off by default
// in normal operation.
#define PICONET_TELNET_DEBUG        0

// Set to 1 to log network/dial events (DNS resolution, TCP connect
// outcomes, lwIP errors) over USB-CDC. Useful when ATD comes back as
// NO CARRIER and you need to know why. Off by default in normal use.
#define PICONET_NET_DEBUG           1

// Default state of the periodic USB-CDC heartbeat at boot. The user
// can flip this at runtime via the configuration menu (HEARTBEAT
// ON/OFF). Runtime changes do NOT persist across reboots — boot
// always restores this default.
#define PICONET_HEARTBEAT_DEFAULT   0

// Pin map — locked by SCHEMATIC.md §4.7. Do not change without
// changing hardware.
#define PICONET_PIN_D0          0   // ..GP7 = D7
#define PICONET_PIN_A0          8   // ..GP11 = A3
#define PICONET_PIN_CS          12
#define PICONET_PIN_RD          13
#define PICONET_PIN_WR          14
#define PICONET_PIN_M1          15  // unused logically; '138 already filters
#define PICONET_PIN_DIR_DATA    16  // U2 DIR (0=Pico->bus during read)
#define PICONET_PIN_OE_DATA     17  // U2 /OE (active-low)
#define PICONET_PIN_RESET_SENSE 18  // active-HIGH (U5 inverts /RESET)
#define PICONET_PIN_INT_DRV     19  // drive HIGH to assert /INT (U5 inverts)
#define PICONET_PIN_LED_LINK    20
#define PICONET_PIN_LED_ACT     21

#endif
