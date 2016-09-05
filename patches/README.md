This directory contains patches that I have made to assembly source
code on the Internet that I don't necessarily have permission 
to redistribute the original.

## dualser.patch: ##
   Original: http://searle.hostei.com/grant/z80/sbc_NascomBasic32k.zip

   Adds support for two 68B50 serial interfaces. 
   (see int32k.asm)

   Extends basic
   with ISER, OSER, RSER, and BAUD keywords. Assumes a CTC chip
   is available to act as a baud rate generator for the
   second serial port. 
   (see bas32k.asm)

   Adds support for SIO/2.
   (see int_sio.asm)

   Adds support for compactflash disk (DINIT, DREAD, DWRITE)
   (see bas_disk.asm)

   Original was 32K basic by Grant Searle
   http://searle.hostei.com/grant/z80/sbc_NascomBasic32k.zip

## cpm.patch ##
   Original: http://searle.hostei.com/grant/cpm/z80sbcFiles.zip

## xmodem.patch ##
   Original: https://drive.google.com/folderview?id=0B-XdfCubTNJJR0duMlFUMWk3OUU&usp=sharing

   Removes RXTIMR that was causing issues
   Relocates stack to avoid overrun
 