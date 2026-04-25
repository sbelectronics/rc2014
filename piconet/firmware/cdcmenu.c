#include "cdcmenu.h"
#include "cfg.h"
#include "config.h"
#include "net.h"

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define LINE_BUF_SIZE 160

static bool s_active;
static bool s_heartbeat_enabled;
static char s_line[LINE_BUF_SIZE];
static int  s_line_len;

bool cdcmenu_active(void)            { return s_active; }
bool cdcmenu_heartbeat_enabled(void) { return s_heartbeat_enabled; }

void cdcmenu_init(void) {
    s_active            = false;
    s_heartbeat_enabled = (PICONET_HEARTBEAT_DEFAULT != 0);
    s_line_len          = 0;
}

// ----- output helpers -------------------------------------------------

static void prompt(void) {
    fputs("piconet> ", stdout);
    fflush(stdout);
}

static void banner(void) {
    fputs("\r\n"
          "PICONET configuration menu\r\n"
          "Type HELP for command list, EXIT to resume normal operation.\r\n",
          stdout);
}

static void show_help(void) {
    fputs(
        "Commands (case-insensitive):\r\n"
        "  SHOW                     show current configuration\r\n"
        "  SET SSID <value>         WiFi SSID (max 32 chars)\r\n"
        "  SET PSK <value>          WiFi pre-shared key (max 63 chars)\r\n"
        "  SET AUTH OPEN|WPA2|WPA3|MIXED\r\n"
        "                           authentication mode\r\n"
        "  DEFAULTS                 revert in-memory cfg to config_local.h\r\n"
        "  WIPE                     erase flash config (test unconfigured boot)\r\n"
        "  SAVE                     write config to flash\r\n"
        "  REBOOT                   reset the Pico (apply saved config)\r\n"
        "  HEARTBEAT ON|OFF         toggle periodic USB heartbeat output\r\n"
        "                           (suppressed while in menu; resumes on EXIT)\r\n"
        "  HB ...                   alias for HEARTBEAT\r\n"
        "  HELP, ?                  this list\r\n"
        "  EXIT                     leave menu, resume normal operation\r\n",
        stdout);
}

static void show_config(void) {
    cfg_t *c = cfg_get();
    char mac[24], ip[20];

    printf("  source : %s\r\n", cfg_loaded_from_flash() ? "flash"
                                                        : "config_local.h / unset");
    printf("  ssid   : %s\r\n", c->wifi.ssid[0] ? c->wifi.ssid : "(unset)");
    printf("  psk    : %s\r\n", c->wifi.psk[0]  ? c->wifi.psk  : "(unset)");
    printf("  auth   : %s\r\n", cfg_auth_name(c->wifi.auth));
    printf("  mac    : %s\r\n",
           net_get_mac_str(mac, sizeof(mac)) ? mac : "(cyw43 not initialised)");
    printf("  ip     : %s\r\n",
           net_get_ip_str(ip,  sizeof(ip))  ? ip  :
           net_wifi_associated()            ? "(no DHCP lease yet)" :
                                              "(not associated)");
    printf("  hb     : %s (boot default %s)\r\n",
           s_heartbeat_enabled ? "ON" : "OFF",
           PICONET_HEARTBEAT_DEFAULT ? "ON" : "OFF");
}

// ----- parsing helpers ------------------------------------------------

// Trim leading/trailing whitespace in place, return pointer to first
// non-space char.
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = 0;
    return s;
}

// Pull off the first whitespace-delimited token, advancing *cursor
// past it. Returns NULL if no token. Modifies *cursor in place.
static char *next_token(char **cursor) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { *cursor = p; return NULL; }
    char *start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) { *p++ = 0; }
    *cursor = p;
    return start;
}

// ----- command dispatch -----------------------------------------------

// Returns false if the menu should exit (EXIT command).
static bool dispatch(char *line) {
    char *cursor = line;
    char *verb = next_token(&cursor);
    if (!verb) return true;       // empty line — re-prompt

    for (char *q = verb; *q; q++) *q = (char)toupper((unsigned char)*q);

    if (!strcmp(verb, "EXIT") || !strcmp(verb, "QUIT")) {
        fputs("[resuming normal operation]\r\n", stdout);
        return false;
    }
    if (!strcmp(verb, "HELP") || !strcmp(verb, "?")) {
        show_help();
        return true;
    }
    if (!strcmp(verb, "SHOW")) {
        show_config();
        return true;
    }
    if (!strcmp(verb, "DEFAULTS")) {
        cfg_load_defaults();
        fputs("loaded config_local.h defaults; SAVE to persist.\r\n", stdout);
        return true;
    }
    if (!strcmp(verb, "WIPE")) {
        if (cfg_wipe()) {
            fputs("flash erased. In-memory config reverted to "
                  "config_local.h defaults. REBOOT to test the "
                  "unconfigured boot path.\r\n", stdout);
        } else {
            fputs("wipe FAILED.\r\n", stdout);
        }
        return true;
    }
    if (!strcmp(verb, "SAVE")) {
        if (!cfg_save()) {
            fputs("save FAILED.\r\n", stdout);
            return true;
        }
        if (net_wifi_associated()) {
            // Already associated to the previous network; bring-up
            // is one-shot, so a fresh config doesn't take effect
            // until the next boot.
            fputs("saved. Already associated — type REBOOT to switch "
                  "networks (EXIT alone keeps current association).\r\n",
                  stdout);
        } else {
            // Bring-up hasn't completed yet; main loop will pick up
            // the new cfg as soon as we leave the menu (REBOOT also
            // works and is the safer choice on a fresh provisioning).
            fputs("saved. Type EXIT or REBOOT to apply.\r\n", stdout);
        }
        return true;
    }
    if (!strcmp(verb, "REBOOT")) {
        fputs("rebooting...\r\n", stdout);
        sleep_ms(50);             // let the line drain to the host
        watchdog_reboot(0, 0, 0);
        for (;;) { tight_loop_contents(); }
    }
    if (!strcmp(verb, "HEARTBEAT") || !strcmp(verb, "HB")) {
        char *arg = next_token(&cursor);
        if (arg) {
            if      (!strcasecmp(arg, "ON"))  s_heartbeat_enabled = true;
            else if (!strcasecmp(arg, "OFF")) s_heartbeat_enabled = false;
            else { fputs("usage: HEARTBEAT ON|OFF\r\n", stdout); return true; }
        }
        if (s_heartbeat_enabled) {
            fputs("heartbeat: ON (output resumes after EXIT)\r\n", stdout);
        } else {
            fputs("heartbeat: OFF\r\n", stdout);
        }
        return true;
    }
    if (!strcmp(verb, "SET")) {
        char *what = next_token(&cursor);
        if (!what) { fputs("usage: SET <SSID|PSK|AUTH> <value>\r\n", stdout); return true; }
        for (char *q = what; *q; q++) *q = (char)toupper((unsigned char)*q);

        // Remainder of the line is the value (verbatim, case-preserved,
        // may include spaces). next_token would split on whitespace,
        // which we don't want for SSID/PSK.
        char *value = trim(cursor);
        cfg_t *c = cfg_get();

        if (!strcmp(what, "SSID")) {
            if (strlen(value) >= sizeof(c->wifi.ssid)) {
                printf("SSID too long (max %u)\r\n",
                       (unsigned)(sizeof(c->wifi.ssid) - 1));
                return true;
            }
            strncpy(c->wifi.ssid, value, sizeof(c->wifi.ssid) - 1);
            c->wifi.ssid[sizeof(c->wifi.ssid) - 1] = 0;
            fputs("ssid set; SAVE to persist.\r\n", stdout);
            return true;
        }
        if (!strcmp(what, "PSK")) {
            if (strlen(value) >= sizeof(c->wifi.psk)) {
                printf("PSK too long (max %u)\r\n",
                       (unsigned)(sizeof(c->wifi.psk) - 1));
                return true;
            }
            strncpy(c->wifi.psk, value, sizeof(c->wifi.psk) - 1);
            c->wifi.psk[sizeof(c->wifi.psk) - 1] = 0;
            fputs("psk set; SAVE to persist.\r\n", stdout);
            return true;
        }
        if (!strcmp(what, "AUTH")) {
            cfg_auth_t a;
            if (!cfg_auth_parse(value, &a)) {
                fputs("auth must be OPEN, WPA2, WPA3, or MIXED.\r\n", stdout);
                return true;
            }
            c->wifi.auth = a;
            printf("auth set to %s; SAVE to persist.\r\n", cfg_auth_name(a));
            return true;
        }
        fputs("unknown SET target. Try SSID, PSK, or AUTH.\r\n", stdout);
        return true;
    }

    printf("unknown command '%s'. Type HELP.\r\n", verb);
    return true;
}

// ----- input pump -----------------------------------------------------

static void handle_byte(int c) {
    if (c == '\r' || c == '\n') {
        fputs("\r\n", stdout);
        s_line[s_line_len] = 0;
        bool keep_going = dispatch(s_line);
        s_line_len = 0;
        if (!keep_going) {
            s_active = false;
            return;
        }
        prompt();
        return;
    }
    if (c == 0x08 || c == 0x7F) {        // BS / DEL
        if (s_line_len > 0) {
            s_line_len--;
            fputs("\b \b", stdout);
            fflush(stdout);
        }
        return;
    }
    if (c < ' ' || c >= 0x7F) return;    // ignore other control chars

    if (s_line_len < (int)sizeof(s_line) - 1) {
        s_line[s_line_len++] = (char)c;
        fputc(c, stdout);
        fflush(stdout);
    }
}

void cdcmenu_poll(void) {
    for (;;) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) return;

        if (!s_active) {
            s_active = true;
            s_line_len = 0;
            banner();
            prompt();
            // Don't echo this byte if it was the trigger keystroke;
            // we already consumed it as the menu trigger. Exception:
            // if it's an actual command character (printable), feed
            // it in so the user doesn't lose it.
            if (c >= ' ' && c < 0x7F) {
                handle_byte(c);
            }
            continue;
        }
        handle_byte(c);
    }
}
