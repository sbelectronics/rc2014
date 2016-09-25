#!/bin/bash

#ZASM="../z80-asm/zasm-4.0.15-Linux64/zasm"
ZASM="../z80-asm/zasm-4.0/Linux/zasm"     
HEXMERGE="python ../tools/hexmerge.py"

$ZASM -u -x nixieclock.asm
$ZASM -u -x intsio.asm

$HEXMERGE nixieclock.hex intsio.hex > nixieclock_sio.hex

