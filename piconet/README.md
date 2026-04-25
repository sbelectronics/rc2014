# PICONET

A four-channel WiFi network card for the [RC2014](https://rc2014.co.uk/)
Z80/Z180 retrocomputer bus, built around a Raspberry Pi Pico 2 W. The
card emulates two Zilog Z80 SIO/2 dual-channel UARTs at the bus level,
giving CP/M, FUZIX, BASIC and bare-metal Z80 software four "serial
ports" — two of which listen for inbound TCP connections, and two of
which dial out using a Hayes-style AT command set.

## What it does

- **Two inbound listeners** (UART0, UART1) — each accepts a TCP
  connection on a configurable port. Bytes from the remote peer
  appear on the SIO/2 RX register; bytes the Z80 writes go out
  through the socket. Telnet IAC negotiation is handled passively
  (raw `nc` clients work transparently).
- **Two outbound dialers** (NET0, NET1) — each speaks a Hayes
  command set (`ATDT host:port` to dial, `+++` to escape, `ATH` to
  hang up). Picks per-dial between full client-side telnet
  negotiation (`ATDT`, default) and raw passthrough (`ATDR`).
- **Four-channel polled and interrupt-driven receive** — verified
  on real Z80 and Z180 hardware in IM1 interrupt mode.
- **WiFi over CYW43439** with TCP keepalive so idle telnet sessions
  don't hiccup on the first keystroke after a long pause.
- **Sub-200 ns deterministic bus response** via a PIO + DMA pipeline
  that takes the ARM CPU completely out of the read-response path.

## How it works

```
   ┌──────────────┐   /CS, /RD, /WR     ┌─────────────────────┐
   │              │ ------------------> │                     │
   │  RC2014 bus  │   D0..D7, A0..A3    │  Pi Pico 2 W (one)  │
   │  (Z80/Z180)  │ <-----------------> │  PIO0 + DMA + ARM   │
   │              │   /INT (open coll)  │  + cyw43 WiFi       │
   │              │ <------------------ │                     │
   └──────────────┘                     └─────────────────────┘
                                                  │  WiFi
                                                  ▼
                                            (TCP, lwIP)
```

The Pico has two physical roles split across its two cores:

- **Core 1** owns PIO0 and the bus-facing GPIOs. Four PIO state
  machines (`oe`, `read`, `write`, `log`) decode every Z80 IO cycle
  addressed at the card. A small "shadow" memory holds the latest
  byte the Z80 should see on a read; a DMA chain copies that byte
  from shadow to the PIO TX FIFO whenever a read fires, with no ARM
  involvement. Total /CS-LOW-to-bus-drive time is ~150 ns, leaving
  ~195 ns of margin against the Z80 sample point at 7.37 MHz.

- **Core 0** runs the cyw43 WiFi driver, lwIP, the four TCP
  channels (two listeners, two dialers), the Hayes AT command
  parser for the outbound channels, and the telnet IAC parser for
  any channel that needs it. It hands data to/from core 1 through
  per-channel single-producer-single-consumer ring buffers.

The two SIO/2 chips and four channels are skeleton emulations —
enough WR0..WR7 register state, IM1 interrupt sourcing, RR0 status
bits, and DCD handling that real polling and interrupt-driven SIO
drivers Just Work.

## Hardware

One Pi Pico 2 W, two 74LVC245 level shifters (data + control), one
74HCT138 address decoder, one 74HCT05 open-collector inverter for
/INT, and a few passives. Power comes from the bus 5 V rail through
a Schottky diode into Pico VSYS. USB stays usable for flashing and
diagnostic CDC output.

## Firmware

C with the official Pico SDK, CMake, Ninja. The bus interface is
~30 lines of PIO assembly. Build instructions in
[firmware/BUILD.md](firmware/BUILD.md).

```
cd firmware
cmake -G Ninja -B build .
cmake --build build           # produces build/piconet.uf2
```

WiFi credentials live in **flash**, set via the USB-CDC configuration
menu (see below). `firmware/config_local.h` is a git-ignored
build-time fallback used only on a board whose flash hasn't been
provisioned yet — leave its SSID/PSK empty (the default) to force
USB-CDC provisioning on first boot, or fill them in to bake
credentials so a fresh board comes up on WiFi automatically.

## Configuration menu

Connect a host to the Pico's USB CDC interface (`picocom`, `screen`,
PuTTY) and press any key. The card drops into a configuration menu:

```
piconet> HELP
Commands (case-insensitive):
  SHOW                     show current configuration
  SET SSID <value>         WiFi SSID (max 32 chars)
  SET PSK <value>          WiFi pre-shared key (max 63 chars)
  SET AUTH OPEN|WPA2|WPA3|MIXED
                           authentication mode
  DEFAULTS                 revert in-memory cfg to config_local.h
  WIPE                     erase flash config (test unconfigured boot)
  SAVE                     write config to flash
  REBOOT                   reset the Pico (apply saved config)
  HEARTBEAT ON|OFF         toggle periodic USB heartbeat output
                           (suppressed while in menu; resumes on EXIT)
  HB ...                   alias for HEARTBEAT
  HELP, ?                  this list
  EXIT                     leave menu, resume normal operation
```

`SHOW` reports both the configuration and the runtime WiFi state:

```
piconet> SHOW
  source : flash
  ssid   : MyNetwork
  psk    : hunter2hunter2
  auth   : WPA2
  mac    : 28:cd:c1:01:23:45
  ip     : 192.168.1.42
  hb     : OFF (boot default OFF)
```

(SSID is case-sensitive per 802.11; type it verbatim. PSK is also
case-sensitive. Menu verbs and `OPEN`/`WPA2`/`WPA3`/`MIXED` are
case-insensitive.)

Typical first-time provisioning:

```
piconet> SET SSID MyNetwork
piconet> SET PSK hunter2hunter2
piconet> SET AUTH WPA2
piconet> SAVE
saved. Type EXIT or REBOOT to apply.
piconet> EXIT
```

`SAVE` is context-aware. Before WiFi has associated either `EXIT` or
`REBOOT` will pick up the new config (the main loop sees the new
credentials on the next iteration). Once the card has already
associated to a network, switching SSIDs requires `REBOOT` because
the firmware doesn't currently tear down an existing association
just to apply new credentials.

The menu is always available — drop in any time, change anything,
`EXIT` to resume. Background output (heartbeat, network events,
telnet logs) is suspended while you're in the menu so the prompt
stays clean. To prove there's still a way back in, a `[press any key
on USB CDC for the configuration menu]` reminder prints once every
heartbeat interval, even when heartbeat output itself is turned off.

`WIPE` erases the persistent flash config and reverts the in-memory
copy to the `config_local.h` defaults. Useful for testing the
unconfigured-boot path (after `WIPE`, `REBOOT` and watch what a
factory-fresh board does).

## Usage

From any Z80 / Z180 software that can drive a Z80 SIO/2 (BASIC,
CP/M, FUZIX, an ASM test harness):

**Inbound** — connect from a workstation. UART0 / UART1 are listening
on the ports configured in `config.h`. From your terminal:

```
nc piconet 2300            # raw passthrough
telnet piconet 2300        # full negotiation; firmware handles IAC
```

Bytes you type appear on the Z80's SIO/2 RX register; bytes the Z80
writes to its TX register come back to you.

**Outbound (telnet BBS)** — from BASIC on the Z80:

```basic
PRINT #1, "ATDT bbs.m68k.club:23"
' wait for "CONNECT", then start interactive session
```

**Outbound (raw socket)** — same but with `ATDR`:

```basic
PRINT #1, "ATDR myhost.local:7777"
```

**Escape from data mode back to command mode**: send `+++` with
≥1 s of TX silence on either side. See the AT command set below for
the full reference.

## AT command set

The two outbound channels (NET0, NET1) speak a Hayes-style command
set. Commands are case-insensitive ASCII terminated by CR (`\r`).
LF on input is ignored. Responses are wrapped in `\r\n` on both
sides. Local echo of typed characters is on by default (`ATE1`).

### Channel states

Each outbound channel is always in one of three states:

- **COMMAND** — bytes from the Z80 are interpreted as AT commands.
  This is the state at boot, after a hangup, after a `+++` escape,
  and after the remote closes the socket.
- **DIALING** — a dial is in flight. Bytes from the Z80 are dropped
  silently; the channel waits for the TCP handshake (plus telnet
  negotiation if `ATDT`) to complete.
- **DATA** — the channel is connected; bytes flow through the TCP
  socket transparently. The only thing the firmware watches for in
  this state is the `+++` escape sequence.

### Commands

| Command            | Effect                                                                                      |
|--------------------|---------------------------------------------------------------------------------------------|
| `AT`               | No-op. Responds `OK`. Useful as a liveness check.                                           |
| `ATDT host:port`   | Dial telnet — TCP connect plus client-side IAC negotiation (`DO ECHO`, `WILL`/`DO SGA`, `WILL`/`DO BINARY`) on completion. |
| `ATDR host:port`   | Dial raw — TCP connect with no telnet processing; bytes pass verbatim in both directions.   |
| `ATD host:port`    | Alias for `ATDT` (telnet is the default).                                                   |
| `ATDT` / `ATDR`    | Same as above but use the build-time default host:port for this channel.                    |
| `ATH`              | Hang up the TCP connection (if any). Stays in COMMAND.                                      |
| `ATO`              | Return to DATA mode without changing the TCP state. Errors with `NO CARRIER` if not connected. |
| `ATI`              | Print firmware identification line and the assigned WiFi hostname.                          |
| `ATE0` / `ATE1`    | Local echo off / on. Default `ATE1`. Bare `ATE` is equivalent to `ATE0`.                    |
| `ATL`              | Print the most recent dial-failure reason, then `OK`. Cleared on successful CONNECT and on `ATZ`. Returns `no error` if nothing has gone wrong since boot. |
| `ATZ`              | Reset the channel: drop TCP, clear buffers, clear last error, return to COMMAND.            |

### Responses

| String        | Meaning                                                                                       |
|---------------|-----------------------------------------------------------------------------------------------|
| `OK`          | Command accepted.                                                                             |
| `CONNECT`     | TCP connect succeeded; channel is now in DATA mode.                                           |
| `NO CARRIER`  | Connection failed, refused, timed out, or remote closed during DATA. Query `ATL` for the reason. |
| `ERROR`       | Malformed command, unknown command letter, or syntactically invalid host:port.                |

### The `+++` escape

To get from DATA back to COMMAND while keeping the TCP connection
open, send three `+` characters with the following timing — chosen
so that binary data which happens to contain `+++` doesn't trigger
the escape by accident:

1. ≥1 s of TX silence before the first `+`
2. The three pluses arrive within <500 ms of each other
3. ≥1 s of TX silence after the third `+`

If any of those constraints fails, the held pluses are flushed
forward to the TCP socket so the remote sees them as ordinary data.
After a successful escape the firmware emits `OK` and the channel
sits in COMMAND mode with the TCP connection still alive — `ATO`
returns to DATA without touching the socket; `ATH` hangs up.

### Typical autodial sequence

A terminal program that wants to switch destinations from an unknown
prior state can do this:

```
wait ≥1.1 s   (no TX)            ; satisfy +++ pre-guard
send "+++"                       ; rapid back-to-back
wait ≥1.1 s   (no TX)            ; satisfy +++ post-guard
send "\r"                        ; flush any +++ that was buffered
                                 ; in COMMAND-mode cmdline (ignore
                                 ; any resulting ERROR response)
send "ATH\r"                     ; idempotent hang-up
send "ATDT host:port\r"          ; dial; wait for CONNECT or NO CARRIER
```

ATH is idempotent (always returns `OK`, even if there was nothing to
hang up), so the sequence is safe to run from any starting state.

## Status

- Hardware fabricated, populated, and verified.
- Firmware works on **both Z80 and Z180**, both directions, in
  polled and IM1 interrupt-driven modes.
- Inbound: telnet and raw clients both work; idle TCP keepalive
  prevents WiFi-power-save first-keystroke lag.
- Outbound: ATDT (telnet) and ATDR (raw) both verified against live
  endpoints; ATL retrieves the last dial-failure reason.
- Runtime configuration via USB CDC menu (SSID, PSK, auth, heartbeat
  toggle, factory wipe) — credentials live in flash, no rebuild
  required to change them.
- Pending: interrupt-driven transmit, Z80-side ASM test harness for
  full SIO register coverage, real CP/M or FUZIX driver.

## Documentation

- [firmware/BUILD.md](firmware/BUILD.md) — build and flash
  instructions.
