#!/usr/bin/env python3
"""
Binary patch for hack_app: disables the license check entirely.

The patch targets the conditional branch at VA 0x159e inside main().
After strncmp(md5decode, xattrValue, 0x21) the code does:

    159c:  85 c0                test   %eax,%eax
    159e:  75 07                jne    15a7              # skip "licensed=1" on mismatch
    15a0:  c7 45 e4 01 00 00 00 movl   $0x1,-0x1c(%rbp) # licensed = 1

We NOP out the JNE so "licensed = 1" always executes:

    159e:  90                   nop
    159f:  90                   nop

This makes the app always take the "Your app is licensed to this PC!" path,
regardless of whether a valid xattr license is present.

Because the .text LOAD segment maps file offset 0x1000 → VA 0x1000,
the file offset equals the virtual address: 0x159e.
"""
import shutil
import sys
import os

SRC = "hack_app"
DST = "hack_app_patched"

FILE_OFFSET = 0x159e
ORIGINAL    = bytes([0x75, 0x07])   # jne +7  (skip licensed=1)
PATCHED     = bytes([0x90, 0x90])   # nop; nop (always set licensed=1)

shutil.copy2(SRC, DST)

with open(DST, "r+b") as f:
    f.seek(FILE_OFFSET)
    found = f.read(len(ORIGINAL))
    if found != ORIGINAL:
        print(f"ERROR: expected {ORIGINAL.hex()} at offset 0x{FILE_OFFSET:x}, "
              f"found {found.hex()}")
        sys.exit(1)
    f.seek(FILE_OFFSET)
    f.write(PATCHED)

os.chmod(DST, 0o755)
print(f"Patched {DST}: NOP-ed JNE at offset 0x{FILE_OFFSET:x}")
print(f"License check is now unconditionally bypassed.")
