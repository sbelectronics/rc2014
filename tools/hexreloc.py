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
