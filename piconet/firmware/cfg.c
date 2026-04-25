#include "cfg.h"
#include "config.h"

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "pico/cyw43_arch.h"   // for CYW43_AUTH_* macros

#include <string.h>
#include <strings.h>
#include <stdio.h>

// ----------------------------------------------------------------------
// On-flash layout — last 4 KB sector of flash. Single-buffered: a save
// erases then writes; a power loss mid-save reverts to defaults on the
// next boot (acceptable for v1; reprovision via USB CDC).
//
//   offset 0:                   uint32_t magic
//   offset 4:                   uint32_t version
//   offset 8:                   cfg_t payload
//   offset PAYLOAD_END..page-4: padding (0xFF after erase)
//   offset PAGE-4:              uint32_t crc32 of bytes [0..page-4)
//
// One FLASH_PAGE_SIZE (256-byte) page is enough for everything we
// store today; we still reserve the full sector since erase granularity
// is 4 KB.
// ----------------------------------------------------------------------

#define CFG_MAGIC        0x50494E45u    // "PINE" — PIcoNEt
#define CFG_VERSION      1u

#define CFG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CFG_FLASH_ADDR   ((const uint8_t *)(XIP_BASE + CFG_FLASH_OFFSET))

#define CFG_PAGE_BYTES   FLASH_PAGE_SIZE   // 256
#define CFG_CRC_OFFSET   (CFG_PAGE_BYTES - 4)

typedef struct {
    uint32_t magic;
    uint32_t version;
    cfg_t    payload;
} cfg_header_t;

_Static_assert(sizeof(cfg_header_t) <= CFG_CRC_OFFSET,
               "cfg payload + header must fit before CRC slot");

static cfg_t s_cfg;
static bool  s_loaded_from_flash;

// CRC32 (IEEE 802.3 polynomial, reflected). Standard zlib variant.
static uint32_t crc32(const uint8_t *data, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++) {
            c = (c >> 1) ^ (0xEDB88320u & -(c & 1u));
        }
    }
    return ~c;
}

static bool flash_payload_valid(const uint8_t *page) {
    const cfg_header_t *h = (const cfg_header_t *)page;
    if (h->magic   != CFG_MAGIC)   return false;
    if (h->version != CFG_VERSION) return false;
    uint32_t expect;
    memcpy(&expect, page + CFG_CRC_OFFSET, sizeof(expect));
    if (crc32(page, CFG_CRC_OFFSET) != expect) return false;
    return true;
}

void cfg_load_defaults(void) {
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.wifi.auth = CFG_AUTH_WPA2;

#if defined(PICONET_WIFI_SSID)
    strncpy(s_cfg.wifi.ssid, PICONET_WIFI_SSID, sizeof(s_cfg.wifi.ssid) - 1);
#endif
#if defined(PICONET_WIFI_PASSWORD)
    strncpy(s_cfg.wifi.psk,  PICONET_WIFI_PASSWORD, sizeof(s_cfg.wifi.psk) - 1);
#endif
    // PICONET_WIFI_AUTH from config_local.h is a CYW43_AUTH_* macro.
    // Fold it back into our enum where we recognise it.
#if defined(PICONET_WIFI_AUTH)
    switch (PICONET_WIFI_AUTH) {
        case CYW43_AUTH_OPEN:           s_cfg.wifi.auth = CFG_AUTH_OPEN;  break;
        case CYW43_AUTH_WPA2_AES_PSK:   s_cfg.wifi.auth = CFG_AUTH_WPA2;  break;
        case CYW43_AUTH_WPA2_MIXED_PSK: s_cfg.wifi.auth = CFG_AUTH_MIXED; break;
#ifdef CYW43_AUTH_WPA3_SAE_AES_PSK
        case CYW43_AUTH_WPA3_SAE_AES_PSK: s_cfg.wifi.auth = CFG_AUTH_WPA3; break;
#endif
        default: break;
    }
#endif
}

void cfg_init(void) {
    s_loaded_from_flash = false;
    if (flash_payload_valid(CFG_FLASH_ADDR)) {
        const cfg_header_t *h = (const cfg_header_t *)CFG_FLASH_ADDR;
        s_cfg = h->payload;
        s_loaded_from_flash = true;
        printf("cfg: loaded from flash (ssid='%s', auth=%s)\n",
               s_cfg.wifi.ssid, cfg_auth_name(s_cfg.wifi.auth));
        return;
    }
    cfg_load_defaults();
    if (s_cfg.wifi.ssid[0]) {
        printf("cfg: flash empty — using config_local.h defaults "
               "(ssid='%s')\n", s_cfg.wifi.ssid);
    } else {
        printf("cfg: unconfigured — connect USB CDC and provision "
               "(any keystroke enters menu)\n");
    }
}

cfg_t *cfg_get(void) { return &s_cfg; }

bool cfg_is_usable(void) {
    return s_cfg.wifi.ssid[0] != 0;
}

bool cfg_loaded_from_flash(void) { return s_loaded_from_flash; }

uint32_t cfg_auth_to_cyw43(cfg_auth_t a) {
    switch (a) {
        case CFG_AUTH_OPEN:  return CYW43_AUTH_OPEN;
        case CFG_AUTH_WPA2:  return CYW43_AUTH_WPA2_AES_PSK;
        case CFG_AUTH_MIXED: return CYW43_AUTH_WPA2_MIXED_PSK;
#ifdef CYW43_AUTH_WPA3_SAE_AES_PSK
        case CFG_AUTH_WPA3:  return CYW43_AUTH_WPA3_SAE_AES_PSK;
#else
        case CFG_AUTH_WPA3:  return CYW43_AUTH_WPA2_AES_PSK;  // fallback
#endif
    }
    return CYW43_AUTH_WPA2_AES_PSK;
}

const char *cfg_auth_name(cfg_auth_t a) {
    switch (a) {
        case CFG_AUTH_OPEN:  return "OPEN";
        case CFG_AUTH_WPA2:  return "WPA2";
        case CFG_AUTH_WPA3:  return "WPA3";
        case CFG_AUTH_MIXED: return "MIXED";
    }
    return "?";
}

bool cfg_auth_parse(const char *s, cfg_auth_t *out) {
    if      (!strcasecmp(s, "OPEN"))  *out = CFG_AUTH_OPEN;
    else if (!strcasecmp(s, "WPA2"))  *out = CFG_AUTH_WPA2;
    else if (!strcasecmp(s, "WPA3"))  *out = CFG_AUTH_WPA3;
    else if (!strcasecmp(s, "MIXED")) *out = CFG_AUTH_MIXED;
    else return false;
    return true;
}

// ----------------------------------------------------------------------
// Save path. flash_safe_execute pauses core 1 (and lwIP timers) for
// the duration of the erase+program — required by the SDK because
// XIP can't read from flash while it's being written.
// ----------------------------------------------------------------------

typedef struct {
    uint8_t page[CFG_PAGE_BYTES];
    bool    ok;
} cfg_save_ctx_t;

static void cfg_save_worker(void *arg) {
    cfg_save_ctx_t *ctx = (cfg_save_ctx_t *)arg;
    flash_range_erase  (CFG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CFG_FLASH_OFFSET, ctx->page, CFG_PAGE_BYTES);
    ctx->ok = true;
}

bool cfg_save(void) {
    cfg_save_ctx_t ctx;
    memset(ctx.page, 0xFF, sizeof(ctx.page));
    ctx.ok = false;

    cfg_header_t *h = (cfg_header_t *)ctx.page;
    h->magic   = CFG_MAGIC;
    h->version = CFG_VERSION;
    h->payload = s_cfg;

    uint32_t crc = crc32(ctx.page, CFG_CRC_OFFSET);
    memcpy(ctx.page + CFG_CRC_OFFSET, &crc, sizeof(crc));

    int rc = flash_safe_execute(cfg_save_worker, &ctx, 1000);
    if (rc != PICO_OK) {
        printf("cfg: save failed (flash_safe_execute rc=%d)\n", rc);
        return false;
    }
    s_loaded_from_flash = true;
    return ctx.ok;
}

// ----- wipe -----------------------------------------------------------

static void cfg_wipe_worker(void *arg) {
    (void)arg;
    flash_range_erase(CFG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
}

bool cfg_wipe(void) {
    int rc = flash_safe_execute(cfg_wipe_worker, NULL, 1000);
    if (rc != PICO_OK) {
        printf("cfg: wipe failed (flash_safe_execute rc=%d)\n", rc);
        return false;
    }
    s_loaded_from_flash = false;
    cfg_load_defaults();
    return true;
}
