import string
import sys
import time
from optparse import OptionParser

from hexfile import HexFile

def main():
 hf1 = HexFile(None)
 hf1.from_str(open(sys.argv[1]).read())

 csum=0
 sys.stdout.write("U0\n")
 sys.stdout.write(":")
 for b in hf1.bytes:
     sys.stdout.write("%02X" % b)
     csum = csum + b
 sys.stdout.write(">")
 sys.stdout.write("%02X" % (len(hf1.bytes) & 0xFF))
 sys.stdout.write("%02X" % (csum & 0xFF))
 sys.stdout.write("\n")

if __name__=="__main__":
    main()
