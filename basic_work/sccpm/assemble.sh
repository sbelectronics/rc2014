#!/bin/bash

ZASM="../../z80-asm/zasm-4.0/Linux/zasm"
HEXMERGE="python ../../tools/hexmerge.py"
HEXRELOC="python ../../tools/hexreloc.py"

$ZASM -u -x monitor.asm
$ZASM -u -x putsys.asm
$ZASM -u -x cbios64.asm
$ZASM -u -x download.asm
$ZASM -u -x form64.asm
$ZASM -u -x cpm22.asm
$ZASM -u -x gocpm.asm

$HEXRELOC download.hex 0x8100 > down8100.hex
