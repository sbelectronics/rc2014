#ifndef PICONET_CDCMENU_H
#define PICONET_CDCMENU_H

#include <stdbool.h>

// USB-CDC configuration menu. Always-responsive: any byte received on
// USB CDC drops the user into the menu, suspending background output
// (heartbeat, NET_LOG, TELNET_LOG) until they EXIT.

void cdcmenu_init(void);

// Poll once. Reads any waiting USB-CDC bytes, transitions into the
// menu on the first one, and dispatches commands when CR/LF arrive.
// Cheap when no input is pending. Call from net_core0_main loop.
void cdcmenu_poll(void);

// True while the user is interacting with the menu. Background-output
// emitters (diag_tick, NET_LOG, TELNET_LOG) check this to avoid
// trashing the menu prompt mid-line.
bool cdcmenu_active(void);

// Heartbeat enable flag. Initialised from PICONET_HEARTBEAT_DEFAULT
// at boot; toggled at runtime via the HEARTBEAT menu command. Not
// persisted across reboots.
bool cdcmenu_heartbeat_enabled(void);

#endif
