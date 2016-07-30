import smbus
import string
import sys
import time
from ioexpand import *
from optparse import OptionParser

"""
   ixAddr = A15..A0
   ixData = D0..D7, M1, CLK, INT, MREQ, WR, RD IORQ, BUSACK
   ixControl = BUSREQ, RST, CLKEN, CLKOUT
"""

# ixData masks
M1=    0x01
CLK=   0x02
INT=   0x04
MREQ=  0x08
WR=    0x10
RD=    0x20
IORQ=  0x40
BUSACK=0x80

# ixControl masks
BUSREQ=0x01
RESET= 0x02
CLKEN= 0x04
CLKOUT=0x08

def bitswap(x):
    y=0
    if (x&128)!=0:
       y=y|1
    if (x&64)!=0:
       y=y|2
    if (x&32)!=0:
       y=y|4
    if (x&16)!=0:
       y=y|8
    if (x&8)!=0:
       y=y|16
    if (x&4)!=0:
       y=y|32
    if (x&2)!=0:
       y=y|64
    if (x&1)!=0:
       y=y|128
    return y

def hex_escape(s):
    printable = string.ascii_letters + string.digits + string.punctuation + ' '
    return ''.join(c if c in printable else r'\x{0:02x}'.format(ord(c)) for c in s)

class Supervisor:
    def __init__(self, bus, addr, verbose):
        self.ixData = MCP23017(bus, addr+1)
        self.ixControl = PCF8574(bus, addr+2)
        self.ixAddress = MCP23017(bus, addr+3)
        self.verbose = False

    def log(self, msg):
        if self.verbose:
            print >> sys.stderr, msg

    def delay(self):
        time.sleep(0.001)

    def reset(self):
        self.ixControl.not_gpio(0, RESET)
        self.delay()
        self.ixControl.or_gpio(0, RESET)

    def take_bus(self):
        self.ixControl.not_gpio(0,BUSREQ)
        self.log("wait for busack")
        while True:
            bits=self.ixData.get_gpio(1)
            if (bits & BUSACK) == 0:
                break
        self.log("busack is received")
        self.ixAddress.set_iodir(0,0x00)
        self.ixAddress.set_iodir(1,0x00)
        self.ixData.set_iodir(1, M1 | CLK | INT | BUSACK)
        self.ixData.set_gpio(1, MREQ | WR | RD | IORQ)

    def release_bus(self, reset=False):
        self.ixAddress.set_iodir(0,0xFF)
        self.ixAddress.set_iodir(1,0xFF)
        self.ixData.set_iodir(1, 0xFF)

        if reset:
            self.ixControl.not_gpio(0,RESET)
            self.delay()
            self.ixControl.or_gpio(0, RESET)

        self.ixControl.or_gpio(0,BUSREQ)
        self.log("wait for not busack")
        while True:
            bits=self.ixData.get_gpio(1)
            if (bits & BUSACK) != 0:
                break
        self.log("not busack is received")

    def slow_clock(self, rate):
        period = (1.0/float(rate))/2.0
        self.ixControl.not_gpio(0, CLKEN)
        while True:
            self.ixControl.not_gpio(0, CLKOUT)
            time.sleep(period)
            self.ixControl.or_gpio(0, CLKOUT)
            time.sleep(period)

    def normal_clock(self):
        self.ixControl.or_gpio(0, CLKEN)

    def set_address(self, addr):
        self.ixAddress.set_gpio(0, bitswap(addr>>8))
        self.ixAddress.set_gpio(1, bitswap(addr&0xFF))

    def mem_read(self, addr):
        self.set_address(addr)
        self.ixData.not_gpio(1, RD)
        self.ixData.not_gpio(1, MREQ)
        result = self.ixData.get_gpio(0)
        self.ixData.or_gpio(1, RD)
        self.ixData.or_gpio(1, MREQ)
        return result

    def mem_write(self, addr, val):
        self.set_address(addr)
        self.ixData.not_gpio(1, WR)
        self.ixData.not_gpio(1, MREQ)
        self.ixData.set_iodir(0, 0x00)
        self.ixData.set_gpio(0, val)
        self.ixData.set_iodir(0, 0xFF)
        self.ixData.or_gpio(1, WR)
        self.ixData.or_gpio(1, MREQ)

    def io_read(self, addr):
        self.set_address(addr)
        self.ixData.not_gpio(1, RD)
        self.ixData.not_gpio(1, IORQ)
        result = self.ixData.get_gpio(0)
        self.ixData.or_gpio(1, RD)
        self.ixData.or_gpio(1, IORQ)
        return result

    def io_write(self, addr, val):
        self.set_address(addr)
        self.ixData.not_gpio(1, WR)
        self.ixData.not_gpio(1, IORQ)
        self.ixData.set_iodir(0, 0x00)
        self.ixData.set_gpio(0, val)
        self.ixData.set_iodir(0, 0xFF)
        self.ixData.or_gpio(1, WR)
        self.ixData.or_gpio(1, IORQ)

    def singlestep_on(self):
        self.ixData.set_intcon(1, M1)
        self.ixData.set_intdef(1, M1)
        self.ixData.set_interrupt(1, M1)

    def singlestep_off(self):
        self.ixData.set_interrupt(1, 0)

    def autostep(self, rate):
        period = (1.0/float(rate))

        try:
           self.singlestep_on()

           while True:
               tnext = time.time() + period

               # wait for M1 to trigger MCP23017 interrupt
               bits = self.ixData.get_intf(1)
               while (bits & M1) == 0:
                   bits = self.ixData.get_intf(1)

               # clear the interrupt and reset wait
               bits=self.ixData.get_gpio(1)
               while (bits & M1)==0:
                  bits = self.ixData.get_gpio(1)

               while (time.time()<tnext):
                  time.sleep(0.0000001)
        finally:
           self.singlestep_off()

class HexFile:
    def __init__(self, addr):
        self.addr = addr
        self.bytes = []
        self.bytes_per_line = 24

    def write(self, val):
        self.bytes.append(val)

    def make_line(self, offset, buffer):
        line=":%02X%04X00" % (len(buffer), offset)
        cksum = len(buffer) + offset/256 + offset%256
        for val in buffer:
           line = line + "%02X" % val
           cksum = cksum + val
        cksum = (-(cksum % 256)) & 0xFF
        line = line + "%02X" % cksum
        return line

    def to_str(self):
        lines=[]
        bytes = self.bytes[:]
        offset = self.addr
        while bytes:
            buffer = bytes[:self.bytes_per_line]
            bytes = bytes[self.bytes_per_line:]
            lines.append(self.make_line(offset, buffer))
            offset = offset + len(buffer)
        lines.append(":00000001FF")
        return "\n".join(lines)+"\n"

    def read_line(self, line):
        orig_line = line
        if line[0]!=':' or len(line)<10:
            print >> sys.stderr, "malformed line:", orig_line
            return {"error": True, "is_eof": False}

        line = line[1:]

        rec_len = int(line[:2], 16)
        line = line[2:]
        addr = int(line[:4], 16)
        line = line[4:]
        rec = int(line[:2], 16)
        line = line[2:]
        if rec==1:
            # EOF
            return {"error": False, "is_eof": True}
        if len(line)!=rec_len*2+2:
            print >> sys.stderr, "malformed line:", orign_line
            return {"error": True, "is_eof": False}

        while len(self.bytes)<(rec_len + addr):
            self.bytes.append(0)

        cksum = rec_len + addr/256 + addr%256
        for i in range(0,rec_len):
            val = int(line[:2],16)
            self.bytes[addr + i] = val
            line = line[2:]
            cksum = cksum + val
        cksum = (-(cksum % 256)) & 0xFF

        line_cksum = int(line[:2],16)
        if line_cksum != cksum:
            print >> sys.stderr, "checksum mismatch in line:", orig_line
            return {"error": True, "is_eof": False}

        if (self.addr==None) or (addr<self.addr):
            self.addr = addr

        return {"error": False, "is_eof": False}

    def from_str(self, s):
        self.bytes=[]
        self.addr = None
        for line in s.split("\n"):
            line=line.strip()
            status = self.read_line(line)
            if status.get("is_eof"):
                self.bytes = self.bytes[self.addr:]
                return
        print >> sys.stderr, "no eof received"

def main():
    parser = OptionParser(usage="supervisor [options] command",
            description="Commands: ...")

    parser.add_option("-A", "--addr", dest="addr",
         help="address", metavar="ADDR", type="int", default=0)
    parser.add_option("-C", "--count", dest="count",
         help="count", metavar="ADDR", type="int", default=65536)
    parser.add_option("-V", "--value", dest="value",
         help="value", metavar="VAL", type="int", default=0)
    parser.add_option("-P", "--ascii", dest="ascii",
         help="print ascii value", action="store_true", default=False)
    parser.add_option("-R", "--rate", dest="rate",
         help="rate for slow clock", metavar="HERTZ", type="int", default=10)
    parser.add_option("-v", "--verbose", dest="verbose",
         help="verbose", action="store_true", default=False)
    parser.add_option("-f", "--filename", dest="filename",
         help="filename", default=None)
    parser.add_option("-r", "--reset", dest="reset_on_release",
         help="reset on release of bus", default=False)
    parser.add_option("-n", "--norelease", dest="norelease",
         help="do not release bus", default=False)

    #parser.disable_interspersed_args()

    (options, args) = parser.parse_args(sys.argv[1:])

    cmd = args[0]
    args=args[1:]

    bus = smbus.SMBus(1)
    super = Supervisor(bus, 0x20, options.verbose)

    if (cmd=="reset"):
        super.reset()

    elif (cmd=="memdump"):
        try:
            super.take_bus()
            for i in range(options.addr,options.addr+options.count):
                val = super.mem_read(i)
                if options.ascii:
                    print "%04X %02X %s" % (i, val, hex_escape(chr(val)))
                else:
                    print "%04X %02X" % (i, val)
        finally:
            if not options.norelease:
                super.release_bus()

    elif (cmd=="savehex"):
        hexdumper = HexFile(options.addr)
        try:
            super.take_bus()
            for i in range(options.addr,options.addr+options.count):
                val = super.mem_read(i)
                hexdumper.write(val)
        finally:
            if not options.norelease:
                super.release_bus()

        if options.filename:
            file(options.filename,"w").write(hexdumper.to_str())
        else:
            sys.stdout.write(hexdumper.to_str())

        # a little self-test ...
        h2 = HexFile(None)
        h2.from_str(hexdumper.to_str())
        print "okay?", h2.to_str() == hexdumper.to_str()


    elif (cmd=="loadhex"):
        hexloader = HexFile(None)
        if options.filename:
            hexloader.from_str(file(options.filename,"r").read())
        else:
            hexloader.from_str(sys.stdin.read())

        try:
            super.take_bus()
            offset = hexloader.addr
            for val in hexloader.bytes:
                super.mem_write(offset, val)
                offset = offset + 1
        finally:
            if not options.norelease:
                super.release_bus(reset=options.reset_on_release)


    elif (cmd=="peek"):
        try:
            super.take_bus()
            print "%02X" % super.mem_read(options.addr)
        finally:
            if not options.norelease:
                super.release_bus()

    elif (cmd=="poke"):
        try:
            super.take_bus()
            super.mem_write(options.addr, options.value)
        finally:
            if not options.norelease:
                super.release_bus()

    elif (cmd=="ioread"):
        try:
            super.take_bus()
            print "%02X" % super.io_read(options.addr)
        finally:
            if not options.norelease:
                super.release_bus()

    elif (cmd=="iowatch"):
        last=None
        try:
            super.take_bus()
            while True:
                x=super.io_read(options.addr)
                if (x!=last):
                    print "%02X" % x
                    last=x
        finally:
            if not options.norelease:
                super.release_bus()

    elif (cmd=="iowrite"):
        try:
            super.take_bus()
            super.io_write(options.addr, options.value)
        finally:
            if not options.norelease:
                super.release_bus()

    elif (cmd=="slowclock"):
        try:
            super.slow_clock(rate=options.rate)
        finally:
            super.normal_clock()

    elif (cmd=="singlestep"):
        try:
            from getch import getch
            super.singlestep_on()
            while True:
                print "press `s` to step, 'q' to quit",
                ch = getch()
                if ch=="s":
                    # Reading gpio will clear interrupt, but the problem is that
                    # M1 may still be low and will immediately re-interrupt.
                    bits=super.ixData.get_gpio(1)
                    while (bits & M1)==0:
                       bits = super.ixData.get_gpio(1)

                    print ""
                if ch=="q":
                    break
        finally:
            super.singlestep_off()

    elif (cmd=="singlestep"):
        try:
            from getch import getch
            super.singlestep_on()
            while True:
                print "press `s` to step, 'q' to quit",
                ch = getch()
                if ch=="s":
                    # Reading gpio will clear interrupt, but the problem is that
                    # M1 may still be low and will immediately re-interrupt. So
                    # keep reading it until we see that M1 has gone high.
                    bits=super.ixData.get_gpio(1)
                    while (bits & M1)==0:
                       bits = super.ixData.get_gpio(1)

                    print ""
                if ch=="q":
                    break
        finally:
            super.singlestep_off()

    elif (cmd=="autostep"):
        try:
            super.autostep(rate=options.rate)
        finally:
            super.normal_clock()

    elif (cmd=="showint"):
        last=None
        while True:
            v = ((super.ixData.get_gpio(1)&INT) !=0)
            if v!=last:
                print v
                last=v


if __name__=="__main__":
    main()
