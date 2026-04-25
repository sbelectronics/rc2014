// Local configuration — credentials and anything else that shouldn't
// be checked in. Copy this file to config_local.h and edit. The
// config_local.h copy is git-ignored.
//
// These values are used as build-time fallback defaults for the WiFi
// credentials. The real source of truth at runtime is the persistent
// config in flash, which is set via the USB-CDC menu (any keystroke
// on the CDC interface drops you into it). Boot order:
//
//   1. If flash has a valid stored config → use it.
//   2. Else if PICONET_WIFI_SSID below is non-empty → use these values
//      as the fallback (handy if you flash a UF2 onto a fresh board
//      and want WiFi to come up immediately without USB provisioning).
//   3. Else the card stays unconfigured — bus side still works, WiFi
//      sits idle, USB CDC menu waits for the user to provision.
//
// Leave SSID empty ("") if you want to force USB-CDC provisioning on
// every fresh board.

#ifndef PICONET_CONFIG_LOCAL_H
#define PICONET_CONFIG_LOCAL_H

#define PICONET_WIFI_SSID       ""
#define PICONET_WIFI_PASSWORD   ""
#define PICONET_WIFI_AUTH       CYW43_AUTH_WPA2_AES_PSK

#endif
