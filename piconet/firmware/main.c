#include "config.h"
#include "sio.h"
#include "bus.h"
#include "net.h"
#include "cfg.h"
#include "cdcmenu.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"

int main(void) {
    stdio_init_all();

    // Activity LED — toggled later by bus events; set up here for
    // immediate visibility at boot.
    gpio_init(PICONET_PIN_LED_ACT);
    gpio_set_dir(PICONET_PIN_LED_ACT, GPIO_OUT);
    gpio_put(PICONET_PIN_LED_ACT, 0);

    // Load persistent configuration (or fall back to config_local.h
    // defaults / unconfigured) before anyone reads cfg_get().
    cfg_init();

    // USB-CDC config menu state. Polled from net_core0_main; reachable
    // from the host by sending any byte over the CDC interface.
    cdcmenu_init();

    // Shared SIO state must be ready before either core touches it.
    sio_init();

    // Hand the bus front-end to core 1 — it then owns PIO0 and the
    // bus-related GPIOs forever.
    multicore_launch_core1(bus_core1_main);

    // Core 0 runs WiFi + lwIP + the Hayes parsers + the menu pump.
    net_core0_main();

    // Unreachable.
    return 0;
}
