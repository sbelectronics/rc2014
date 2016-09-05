#! /bin/bash

HEXMERGE="python ../tools/hexmerge.py"
HEXRELOC="python ../tools/hexreloc.py"

# SIO boot rom, for use with SIO/2 board
# Installs the following:
#   1) Basic with dualser and disk extensions
#   2) Bank-1 switcher for use with supervisor board
#   3) CP/M monitor
#   4) Boot directly to CP/M

cp ../basic_work/scbasic/dbas_sio.hex dbas_sio.hex
$HEXRELOC ../basic_work/other/gobank1.hex 0x4000 > gobank1_4000.hex
$HEXRELOC ../basic_work/sccpm/monitor.hex 0x8000 > monitor_8000.hex
$HEXRELOC ../basic_work/sccpm/gocpm.hex 0xC000 > gocpm_C000.hex
$HEXMERGE dbas_sio.hex gobank1_4000.hex monitor_8000.hex gocpm_C000.hex > brom_sio.hex
