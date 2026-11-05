#!/usr/bin/env python3
"""Writes a tiny synthetic DS ROM with WFC strings in an uncompressed ARM9, ARM7 and one overlay.
Usage: mkrom.py out.nds [nowfc]"""
import struct, sys
out = sys.argv[1]; nowfc = len(sys.argv) > 2
def blob(n, s):
    b = bytearray((i * 7 + 3) & 0xFF for i in range(n))
    for i in range(len(b)):
        if b[i] == 0: b[i] = 1
    if not nowfc:
        for k, t in enumerate(s):
            b[0x100 + k * 80: 0x100 + k * 80 + len(t) + 1] = t + b"\0"
    return bytes(b)
a9 = blob(0x6000, [b"https://nas.nintendowifi.net/ac", b"https://", b"nintendowifi.net"])
a7 = blob(0x2000, [b"https://nas.nintendowifi.net/ac"])
ov = blob(0x1000, [b"http://conntest.nintendowifi.net/"])
a9off = 0x4000; a7off = a9off + len(a9) + 0x1FF & ~0x1FF
otab = a7off + len(a7) + 0x1FF & ~0x1FF; fat = otab + 0x40; f0 = fat + 0x200
rom = bytearray(f0 + len(ov))
rom[0:8] = b"TESTGAME"; rom[0x0C:0x10] = b"ABCD"
struct.pack_into("<IIII", rom, 0x20, a9off, 0x02000000, 0x02000000, len(a9))
struct.pack_into("<IIII", rom, 0x30, a7off, 0x02380000, 0x02380000, len(a7))
struct.pack_into("<I", rom, 0x48, fat); struct.pack_into("<II", rom, 0x50, otab, 0x20)
rom[a9off:a9off + len(a9)] = a9; rom[a7off:a7off + len(a7)] = a7
struct.pack_into("<I", rom, otab, 0)
struct.pack_into("<II", rom, fat, f0, f0 + len(ov)); rom[f0:f0 + len(ov)] = ov
open(out, "wb").write(rom)
