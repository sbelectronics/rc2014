#ifndef PICONET_BUS_H
#define PICONET_BUS_H

#include <stdbool.h>
#include <stdint.h>

// Core-1 entry point. Initialises PIO0 with the four bus state
// machines (oe, read, write, log) plus the read-response DMA chain,
// then enters a tight polling loop dispatching post-read events and
// Z80 writes to the SIO/2 emulation, updating the /INT pin, and
// watching for bus reset.
void bus_core1_main(void);

// Diagnostic counters — written by core 1 (the bus poller), read by
// core 0 (the heartbeat). All fields are simple incrementing counters
// or last-seen-value snapshots; tearing on read is acceptable for
// diagnostics.
typedef struct {
    volatile uint32_t reads_per_reg [16];
    volatile uint32_t writes_per_reg[16];
    volatile uint8_t  last_read_reg;
    volatile uint8_t  last_write_reg;
    volatile uint8_t  last_write_data;
    volatile uint32_t reset_count;
} bus_stats_t;

extern bus_stats_t bus_stats;

// Live snapshot of the bus-facing input pins, taken on demand by core 0.
typedef struct {
    uint8_t a;          // A0..A3 (4 bits)
    bool    cs;         // GP12 (true = HIGH = idle)
    bool    rd;         // GP13
    bool    wr;         // GP14
    bool    m1;         // GP15
    bool    reset_sense;// GP18 (true = HIGH = bus /RESET asserted)
} bus_snapshot_t;

void bus_snapshot(bus_snapshot_t *out);

#endif
