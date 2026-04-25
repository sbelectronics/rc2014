#include "config.h"
#include "sio.h"
#include "bus.h"
#include "net.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"

int main(void) {
    stdio_init_all();

    // Activity LED — toggled later by bus events; set up here for
    // immediate visibility at boot.
    gpio_init(PICONET_PIN_LED_ACT);
    gpio_set_dir(PICONET_PIN_LED_ACT, GPIO_OUT);
    gpio_put(PICONET_PIN_LED_ACT, 0);

    // Shared SIO state must be ready before either core touches it.
    sio_init();

    // Hand the bus front-end to core 1 — it then owns PIO0 and the
    // bus-related GPIOs forever.
    multicore_launch_core1(bus_core1_main);

    // Core 0 runs WiFi + LWIP + the Hayes parsers.
    net_core0_main();

    // Unreachable.
    return 0;
}
