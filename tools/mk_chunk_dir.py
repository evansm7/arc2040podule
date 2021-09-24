#!/usr/bin/env python3
#
# Create a simple podule header for the RP2040 Arc Pipe Podule,
# with a chunk directory containing:
#
# - Info strings
# - Loader entry
#
# (Hmm, or make generic, without header, and create just a chunk dir/load modules.)
#
# ME 22 Sept 2021
#

import os
import sys
import struct
import getopt

# Defaults:
PRODUCT = 0xabcd
MANUFACTURER = 0x1337

################################################################################

class CDEntry:
    OS_RISCOS = 0x8
    DATA_RISCOS_LOADER = 0
    DATA_RISCOS_MODULE = 1
    # BBC ROM, Sprites etc. not supported
    OS_DD = 0xf
    DATA_DD_SERIAL = 1
    DATA_DD_MANU_DATE = 2
    DATA_DD_MOD_STATUS = 3
    DATA_DD_MANU_PLACE = 4
    DATA_DD_DESCR = 5
    DATA_DD_PART_NO = 6
    DATA_DD_EMPTY = 15

    def __init__(self, o=0, d=0, a=0, dbytes=bytearray()):
        self.OS = o
        self.D = d
        self.addr = a
        self.data = dbytes

    def setOS(self, o):
        self.OS = o

    def setD(self, d):
        self.D = d

    def setData(self, d):
        self.data = d

    def setAddr(self, a):
        self.addr = a

    def getAddr(self):
        return self.addr

    def getSize(self):
        return len(self.data)

    def getOSIB(self):
        return (self.D & 0xf) | ((self.OS & 0xf) << 4)

    def getData(self):
        return self.data

    def describe(self):
        if self.OS == self.OS_RISCOS:
            ostype = "RISC OS"
            if self.D == self.DATA_RISCOS_LOADER:
                content = "Loader"
            elif self.D == self.DATA_RISCOS_MODULE:
                content = "Relocatable Module"
            else:
                content = "UNK"
        elif self.OS == self.OS_DD:
            ostype = "Device data"
            if self.D == self.DATA_DD_SERIAL:
                content = "serial number"
            elif self.D == self.DATA_DD_MANU_DATE:
                content = "date of manufacture"
            elif self.D == self.DATA_DD_MOD_STATUS:
                content = "modification status"
            elif self.D == self.DATA_DD_MANU_PLACE:
                content = "place of manufacture"
            elif self.D == self.DATA_DD_DESCR:
                content = "description"
            elif self.D == self.DATA_DD_PART_NO:
                content = "part number"
            elif self.D == self.DATA_DD_EMPTY:
                content = "empty chunk"
            else:
                content = "UNK"
        else:
            ostype = "UNK"
            content = "UNK"
        return ostype + ", " + content + ", size 0x%x, address 0x%x" % \
            (len(self.data), self.addr)

class ChunkDirectory:
    # Internal classes:

    def __init__(self, offset):
        self.entries = []       # A list of CDEntry()s
        self.offset = offset

    def _addEntry(self, ostype, datatype, data):
        self.entries.append(CDEntry(o=ostype, d=datatype, dbytes=bytearray(data)))

    # Zero-terminated text form:
    def _addEntryZ(self, ostype, datatype, data):
        data = bytearray(data, 'ascii') + bytearray([0])
        self.entries.append(CDEntry(o=ostype, d=datatype, dbytes=data))

    # Friendlier versions:
    def addLoader(self, data):
        self._addEntry(CDEntry.OS_RISCOS, CDEntry.DATA_RISCOS_LOADER, data)

    def addRModule(self, data):
        self._addEntry(CDEntry.OS_RISCOS, CDEntry.DATA_RISCOS_MODULE, data)

    def addTextSerial(self, data):
        self._addEntryZ(CDEntry.OS_DD, CDEntry.DATA_DD_SERIAL, data)

    def addTextManufactureDate(self, data):
        self._addEntryZ(CDEntry.OS_DD, CDEntry.DATA_DD_MANU_DATE, data)

    def addTextManufacturePlace(self, data):
        self._addEntryZ(CDEntry.OS_DD, CDEntry.DATA_DD_MANU_PLACE, data)

    def addTextModificationStatus(self, data):
        self._addEntryZ(CDEntry.OS_DD, CDEntry.DATA_DD_MOD_STATUS, data)

    def addTextDescription(self, data):
        self._addEntryZ(CDEntry.OS_DD, CDEntry.DATA_DD_DESCR, data)

    def addTextPartNumber(self, data):
        self._addEntryZ(CDEntry.OS_DD, CDEntry.DATA_DD_PART_NO, data)

    def calculate(self):
        # Pass through entries, calculating running offsets/addresses.
        # Note, align new entries to 4 bytes for readability (optional!)
        #
        # The first payload address starts after the header, which is
        # nentries+8 long (including an end marker):
        addr = self.offset + (len(self.entries)+1)*8

        for e in self.entries:
            e.setAddr(addr)
            l = e.getSize()
            l = (l + 3) & ~3    # Round up to 4
            addr += l

    def describe(self):
        self.calculate()
        msg = "Chunk Directory, offset=%d, %d entries:\n" % \
            (self.offset, len(self.entries))
        i = 0
        for e in self.entries:
            msg += " Entry %d: " % (i) + e.describe() + "\n"
            i += 1
        return msg

    def render(self):
        self.calculate()
        chdir = bytearray((len(self.entries)+1)*8)
        payloads = bytearray()
        i = 0
        for e in self.entries:
            s = e.getSize()
            chdir[i:i+8] = struct.pack('<BBBBI', \
                                       e.getOSIB(), \
                                       s & 0xff, (s >> 8) & 0xff, (s >> 16) & 0xff, \
                                       e.getAddr())
            # Pad with 0-3 bytes, as length is rounded up to 4:
            padding = bytearray((4-e.getSize()) & 3)
            payloads += e.getData() + padding
            i += 8
        chdir[i:i+8] = bytearray(8)     # Null entry on end

        return chdir + payloads


################################################################################

def fatal(msg):
    print("ERROR: " + msg)
    exit(1)

def help():
    print("Syntax:  this.py <OPTIONS> [MODULE]...")
    print("\t -H                Add podule header")
    print("\t -P <u16>          Specify header product ID")
    print("\t -M <u16>          Specify header manufacturer ID")
    print("\t -d <text>         Add description")
    print("\t -D <text>         Add date string")
    print("\t -s <text>         Add serial")
    print("\t -S <text>         Add mod status")
    print("\t -p <text>         Add place string")
    print("\t -n <text>         Add part number")
    print("\t -l <filename>     Add Loader")
    print("\t -r <size>         Round/pad output size up")
    print("\t -o <filename>     Output file (required)")
    print()

# Parse command-line args
#

outfile = None
add_header = False
text_descr = None
text_date = None
text_serial = None
text_mod = None
text_place = None
text_part = None
loader = None
round_size = 0
modules = []

try:
    opts, args = getopt.getopt(sys.argv[1:], "hHP:M:d:D:s:S:p:n:l:o:r:")
except getopt.GetoptError as err:
    help()
    fatal("Invocation error: " + str(err))

for o, a in opts:
    if o == "-h":
        help()
        sys.exit()
    if o == "-H":
        add_header = True
    elif o == "-P":
        PRODUCT = int(a, 0)
    elif o == "-M":
        MANUFACTURER = int(a, 0)
    elif o == "-d":
        text_descr = a
    elif o == "-D":
        text_date = a
    elif o == "-s":
        text_serial = a
    elif o == "-S":
        text_mod = a
    elif o == "-p":
        text_place = a
    elif o == "-P":
        text_part = a
    elif o == "-l":
        loader = a
    elif o == "-o":
        outfile = a
    elif o == "-r":
        round_size = int(a, 0)
    else:
        help()
        fatal("Unknown option?")

if len(args) > 0:
    # Been supplied some modules
    module_names = args[0:]
else:
    module_names = None

if not outfile:
    help()
    fatal("Output file required (-o foo)");


# Start work!

if add_header:
    print("+ Adding header")
    header = bytearray(16)      # Header + IRQ Status Pointers
    header[0] = 0               # Holds FIQ/IRQ flags
    header[1] = 0x03            # CD=1, chunk directory present; IS=1 ISPs present
    header[3] = PRODUCT & 0xff
    header[4] = PRODUCT >> 8
    header[5] = MANUFACTURER & 0xff
    header[6] = MANUFACTURER >> 8
    header[7] = 0               # UK
    header[8] = 0               # FIQ mask = 0 (no FIQ)
    header[9] = 0               # FIQ addr = 0
    header[10] = 0
    header[11] = 0
    header[12] = 1              # IRQ mask = 1 (IRQ flag in header byte 0)
    header[13] = 0              # IRQ addr = 0
    header[14] = 0
    header[15] = 0
else:
    header = bytearray(0)

# Offset exits because a chunk directory might live right at the beginning
# of an address space (such as the 2nd CD that the loader fetches), or might
# live after the header (such as the 1st CD that the OS fetches).

cd = ChunkDirectory(len(header))

if text_descr:
    cd.addTextDescription(text_descr)
if text_date:
    cd.addTextManufactureDate(text_date)
if text_serial:
    cd.addTextSerial(text_serial)
if text_mod:
    cd.addTextModificationStatus(text_mod)
if text_place:
    cd.addTextManufacturePlace(text_place)
if text_part:
    cd.addTextPartNumber(text_part)

if loader:
    print("+ Adding loader from '" + loader + "'")
    with open(loader, 'rb') as d:
        ldata = d.read()
        cd.addLoader(ldata)
        d.close()

if module_names and len(module_names) > 0:
    for x in module_names:
        print("+ Adding module '" + x + "'")
        with open(x, 'rb') as d:
            mdata = d.read()
            cd.addRModule(mdata)
            d.close()

print(cd.describe())

output = header + cd.render()

if round_size > 0:
    l = len(output)
    pad_with = (round_size - (l % round_size)) % round_size
    print("Rounding output size %d to %d\n" % (l, l+pad_with))
    output = output + bytearray(pad_with)

print("Writing '" + outfile + "'")
with open(outfile, 'wb') as d:
    d.write(output)
    d.close()

print("Finished.\n")
