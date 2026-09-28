#!/usr/bin/env python3
"""Print the SHA-1 of the executable (main.dol) inside a GameCube disc image."""
import hashlib
import sys

with open(sys.argv[1], "rb") as d:
    d.seek(0x420)
    dol = int.from_bytes(d.read(4), "big")
    d.seek(dol)
    h = d.read(0x100)
    be = lambda o: int.from_bytes(h[o:o + 4], "big")
    size = max([0x100] + [be(i * 4) + be(0x90 + i * 4) for i in range(18) if be(0x90 + i * 4)])
    d.seek(dol)
    print(hashlib.sha1(d.read(size)).hexdigest())
