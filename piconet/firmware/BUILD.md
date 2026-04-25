# Building and flashing PICONET firmware (Linux)

## One-time setup

```bash
# Toolchain (Debian/Ubuntu — adjust for your distro)
sudo apt install cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 libstdc++-arm-none-eabi-newlib build-essential

# pico-sdk
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk && git submodule update --init
echo 'export PICO_SDK_PATH=$HOME/pico-sdk' >> ~/.bashrc
source ~/.bashrc
```

WiFi credentials are normally provisioned at runtime via the USB-CDC
configuration menu (any keystroke on the CDC interface enters it),
and persisted in flash. `config_local.h` (git-ignored) holds optional
build-time fallback values used only when flash hasn't been
provisioned yet — leave its SSID/PSK empty to require menu
provisioning on first boot, or fill them in to bake credentials so a
fresh board joins WiFi automatically. Other knobs (inbound listen
ports, default ATD targets for NET0/NET1, etc.) live in `config.h`
and can be edited in place.

## Build

```bash
cd /path/to/piconet/firmware
cmake -G Ninja -B build .
cmake --build build
```

Output: `build/piconet.uf2` (also `.elf`, `.bin`, `.hex`).

## Flash

### Option A — `picotool load` (recommended; no buttons, no replug)

Reboots the running Pico into BOOTSEL via USB, flashes, and reboots
back into the new firmware. The picocom session survives the brief USB
re-enumerate. Requires `pico_enable_stdio_usb` in CMakeLists (already
set in this project).

**Install picotool from source.** The Debian/Ubuntu apt package is
usually too old (no RP2350 support, no `load` command). Build the
current upstream version:

```bash
# Dependencies — libusb is REQUIRED for picotool to talk to the Pico
# over USB. If it's missing, picotool builds without USB support and
# you'll get "ERROR: Unknown command: load" or warnings about USB.
sudo apt install libusb-1.0-0-dev pkg-config

# Build and install picotool
git clone https://github.com/raspberrypi/picotool.git ~/picotool
cd ~/picotool
mkdir -p build && cd build
cmake -DPICO_SDK_PATH=$PICO_SDK_PATH ..
make -j$(nproc)
sudo make install
```

Verify USB support is built in:
```bash
picotool version
```
Output should NOT contain "compiled without USB support". If it does,
re-check that `libusb-1.0-0-dev` was installed *before* the cmake step,
then `rm -rf ~/picotool/build/*` and rebuild.

udev rule so flashing doesn't require sudo every time:
```bash
echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-picotool.rules
sudo udevadm control --reload && sudo udevadm trigger
```

Flash (works against either a Pico in BOOTSEL or a running Pico with
USB stdio enabled):
```bash
picotool load build/piconet.uf2 -f -x
```
`-f` = force (don't ask which device), `-x` = execute (boot into the
new firmware after flashing).

### Option B — BOOTSEL + USB (no extra tools)

1. Hold the BOOTSEL button on the Pico 2 W, plug in USB.
2. It mounts as `/media/$USER/RPI-RP2350` (or wherever your distro
   auto-mounts removable drives).
3. `cp build/piconet.uf2 /media/$USER/RPI-RP2350/` — flashes, reboots,
   ejects.

### Option C — SWD via debugprobe (best for live debugging)

A second Pico flashed with the
[debugprobe](https://github.com/raspberrypi/debugprobe) firmware acts
as a CMSIS-DAP probe: no replug for each flash, plus a serial console
on the target's UART or USB-CDC.

1. Wire SWCLK / SWDIO / GND from the debugprobe to the target's SWD
   pads (on the back of the Pico 2 W).
2. Install OpenOCD ≥ 0.12 (needs RP2350 support):
   ```bash
   sudo apt install openocd
   # if your distro's is older, build from raspberrypi/openocd
   ```
3. udev rule so you don't need sudo:
   ```bash
   echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000c", MODE="0666"' \
     | sudo tee /etc/udev/rules.d/60-picoprobe.rules
   sudo udevadm control --reload && sudo udevadm trigger
   ```
4. Flash:
   ```bash
   openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
           -c "adapter speed 5000" \
           -c "program build/piconet.elf verify reset exit"
   ```
5. Serial console (debugprobe exposes one):
   ```bash
   picocom -b 115200 /dev/ttyACM0
   ```
   You'll see `Connecting to '<SSID>'...` and then `PICONET ready`.

## First-boot smoke test (no RC2014 required)

1. Power the Pico over USB (or via the bus once installed).
2. Open the target's USB-CDC console (`/dev/ttyACM1` if a debugprobe
   is also attached — the debugprobe is `ttyACM0`).
3. Confirm WiFi associates and `PICONET ready` prints.
4. From another machine on the same network:
   ```bash
   telnet <pico-ip> 2300
   ```
   The connection should hold; bytes you type are queued into UART0's
   RX ring, waiting for a Z80 read.
