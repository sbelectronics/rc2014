#ifndef PICONET_CFG_H
#define PICONET_CFG_H

#include <stdbool.h>
#include <stdint.h>

// Persistent configuration stored in the last 4 KB sector of flash.
// Loaded once at boot, mutated only by the USB-CDC config menu, and
// re-read by the runtime via cfg_get(). After cfg_save() the user
// must reboot for changes to take effect — the WiFi stack reads its
// credentials only during net_core0_main bring-up.

typedef enum {
    CFG_AUTH_OPEN  = 0,
    CFG_AUTH_WPA2  = 1,
    CFG_AUTH_WPA3  = 2,
    CFG_AUTH_MIXED = 3,
} cfg_auth_t;

typedef struct {
    char       ssid[33];     // 32 + NUL
    char       psk[64];      // 63 + NUL
    cfg_auth_t auth;
} cfg_wifi_t;

typedef struct {
    cfg_wifi_t wifi;
} cfg_t;

// Initialise the in-memory copy. Reads flash; if the flash sector is
// blank or the magic/version/CRC don't validate, falls back to the
// build-time defaults from config_local.h (when those are non-empty).
// Call once at boot before anyone reads cfg_get().
void cfg_init(void);

// Live in-memory copy of the configuration. The menu mutates this
// directly; cfg_save() commits it to flash.
cfg_t *cfg_get(void);

// True if the current config has a usable SSID (non-empty). When
// false, net_core0_main skips WiFi bring-up and waits for the user
// to provision via USB CDC.
bool cfg_is_usable(void);

// True if the current in-memory config came from flash (vs the
// config_local.h fallback or empty defaults). Useful for menu SHOW.
bool cfg_loaded_from_flash(void);

// Convert the cfg_auth_t enum to the matching CYW43_AUTH_* macro.
uint32_t cfg_auth_to_cyw43(cfg_auth_t a);

// Human-readable name for the auth type ("OPEN", "WPA2", etc.).
const char *cfg_auth_name(cfg_auth_t a);

// Parse "OPEN", "WPA2", "WPA3", "MIXED" (case-insensitive). Returns
// true on success and stores the parsed value in `*out`.
bool cfg_auth_parse(const char *s, cfg_auth_t *out);

// Commit the current in-memory cfg to flash. Returns true on success.
// The save erases the reserved sector and writes a fresh struct with
// magic/version/CRC. Briefly disables interrupts and pauses core 1
// (required by the SDK's flash_safe_execute machinery), so don't
// call this from a tight loop.
bool cfg_save(void);

// Replace the in-memory cfg with the boot defaults — config_local.h
// values if present, empty otherwise. Doesn't touch flash; user must
// SAVE to persist.
void cfg_load_defaults(void);

// Erase the reserved flash sector and reset the in-memory cfg to
// boot defaults. After this, the next boot sees blank flash. Useful
// for testing the unconfigured-boot path. Returns true on success.
bool cfg_wipe(void);

#endif
