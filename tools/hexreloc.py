# Python library for changing the start address of an Intel Hex Files
# by Scott M Baker
# http://www.smbaker.com/

# Note: This doesn't alter the machine code at all (for example, if there
# are absolute jumps). All it does is change the hex file so that the 
# machine code will be loaded at a different location.

import string
import sys
import time
from optparse import OptionParser

from hexfile import HexFile

def main():
 hf1 = HexFile(None)
 hf1.from_str(open(sys.argv[1]).read())

 if "x" in sys.argv[2]:
      newStart = int(sys.argv[2], 16)
 else:
     newStart = int(sys.argv[2])

 hf1.addr = newStart
 sys.stdout.write(hf1.to_str())

if __name__=="__main__":
    main()
