# Notes on cpm apps #

## Xmodem ##

Downloaded from: https://drive.google.com/folderview?id=0B-XdfCubTNJJR0duMlFUMWk3OUU&usp=sharing

Ran into three issues:

1. The stack, which was set to the area before program start, overflowed something important. So I moved it.
2. The receive routines call RXTIMR for some reason before starting the transfer. Since RCTIMR clobbers the stack on return to return from the parent, this causes untold mayhem by returning at the wrong time.
3. The default receive buffer of cbios64 was 50 bytes, which was too small to hold the 128 byte xmodem packet. I increased it to 150. Flow control would mitigate this problem.

These issues are patched by xmodem.patch in the patches directory