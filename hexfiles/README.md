This directory contains all of the hex files in one spot, 
in case you would like to be able to program an EPROM,
but don't necessarily have the tools installs to assemble
the source code. Using these hex files assumes you have 
configured your RC2014 identically to how I configured mine. 
If you haven't then you'll need to assemble from source.

If you instead want to build these hex images from source,
then it'll take some work to install the proper tools, and
some wandering around this github repo. The /patches directory
contains patches against Grant Searle's original source, and
the /basic_work/sccpm and /basic_work/scbasic contain working 
directories where I assemble the patched files. See the README.md
file in each directory. 

Note that these are my working hex files, and reflect 
specific configurations of my RC2014. For example, 
I have selected port numbers and addresses that work
for my installed system, which may differ from 
yours.

### brom_sio.hex ###

General-purpose Boot rom. There are four boot programs installed in this rom: 0) dbas_sio, 1) bank_switch, 2) monitor, 3) gocpm. These are at 0000, 4000, 8000, and C000 respectively. People wanting to follow along with the CP/M video will want to jumper the rom board to boot the CP/M monitor. Alternatively, burn monitor.hex instead.

### brom_nixieclock_sio.hex ###

Scott's NixieClock boot rom. There are two boot programs installed in this rom: 0) Nixie Tube Clock, 1) bank switcher for supervisor board. These are at 0000 and 4000 respectively. 

### cbios64.hex ###

Grant Searle's CP/M bios, modified to use SIO board at 80h and compactflash at E0h. 

### cpm22.hex ###

Grant Searle's CP/M

### dbas_sio.hex ###

Grant Searle's basic modified to support SIO/2, with some extensions for compactflash. SIO is at 80h and compactflash is at E0h.

### down8100.hex ###

Grant Searle's download program, relocated to address 8100.

### form64.hex ###

Grant Searle's format tool for compactflash, modified to use compatctflash at E0h. 

### gocpm.hex ###

It's like the monitor, but omits the space bar prompt and boot prompt, and goes directly into an installed CP/M. Makes the boot-up sequence a little less keyboard intensive. Useful once you've already installed your CP/M.  

### monitor.hex ###

Grant Searle's CP/M monitor, modified to use SIO/2 at 80h, compactflash at E0h, and to relocate down8100.hex from address 8100. You can use this directly when following along with my CP/M blog post, or use brom_sio.hex and jumper the rom board as appropriate to select the right boot program.

### nixieclock_sio.hex ###

Scott's Nixie Tube Clock. 

### putsys.hex ###

Grant Searle's CP/M putsys tool, modified to use compactflash at E0h. Located at 8100h.