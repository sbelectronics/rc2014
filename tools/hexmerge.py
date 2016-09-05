# Python library for merging two Intel Hex Files
# by Scott M Baker
# http://www.smbaker.com/

import string
import sys
import time
from optparse import OptionParser

from hexfile import HexFile

def main():
  hf1 = HexFile(None)
  hf1.from_str(open(sys.argv[1]).read())

  for arg in sys.argv[2:]:
      hfm = HexFile(None)
      hfm.from_str(open(arg).read())
      hf1.merge(hfm)

  sys.stdout.write(hf1.to_str())

if __name__=="__main__":
    main()
