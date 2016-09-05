import string
import sys
import time
from optparse import OptionParser

from hexfile import HexFile

def main():
 hf1 = HexFile(None)
 hf1.from_str(open(sys.argv[1]).read())

 hf2 = HexFile(None)
 hf2.from_str(open(sys.argv[2]).read())

 if (hf1.addr!=hf2.addr):
     print >> sys.stderr, "addr mismatch", hf1.addr, hf2.addr

 if len(hf1.bytes) != len(hf2.bytes):
     print >> sys.stderr, "len mismatch", len(hf1.bytes), len(hf2.bytes)

 minlen=min(len(hf1.bytes), len(hf2.bytes))

 for i in range(0, hf1.addr):
     hf1.bytes = [0] + hf1.bytes

 for i in range(0, hf2.addr):
     hf2.bytes = [0] + hf2.bytes

 start = max(hf1.addr, hf2.addr)
 for i in range( start, start + minlen ):
     if hf1.bytes[i] != hf2.bytes[i]:
         print >> sys.stderr, "mismatch %04X %02X %02X" % (i, hf1.bytes[i], hf2.bytes[i])



if __name__=="__main__":
    main()
