import string
import sys
import time

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

    def merge(self, hf):
        for i in range(0, self.addr):
            self.bytes = [0] + self.bytes

        while (hf.addr+len(hf.bytes)>len(self.bytes)):
            self.bytes.append(0)

        for i in range(0, len(hf.bytes)):
            self.bytes[hf.addr+i] = hf.bytes[i]

        self.addr = min(self.addr, hf.addr)
        self.bytes = self.bytes[self.addr:]

