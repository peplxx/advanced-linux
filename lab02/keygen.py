#!/usr/bin/env python3
"""
Keygen for hack_app.

Algorithm (reverse-engineered from hack_app via objdump):
  1. CPUID leaf 1 → take EAX (processor info) and EDX (feature flags)
  2. Byte-swap both 32-bit values to big-endian
  3. PSN = "%08X%08X" % (bswap(EAX), bswap(EDX))   (16-char hex string)
  4. digest = MD5(PSN)                               (16 raw bytes)
  5. license = hex(digest[15], digest[14], ..., digest[0])
     i.e. the MD5 digest with byte order reversed, then hex-encoded → 32-char key

The license is stored by the app as xattr "user.license" on the binary itself.
"""
import ctypes
import ctypes.util
import hashlib
import mmap
import struct
import subprocess
import sys
import os


def cpuid_via_asm(leaf: int):
    """Execute CPUID using a tiny machine-code thunk via mmap+ctypes."""
    code = bytes([
        0x53,                       # push  rbx
        0x89, 0xf0,                 # mov   eax, esi      (leaf)
        0x31, 0xc9,                 # xor   ecx, ecx
        0x0f, 0xa2,                 # cpuid
        0x89, 0x07,                 # mov   [rdi],    eax
        0x89, 0x5f, 0x04,           # mov   [rdi+4],  ebx
        0x89, 0x4f, 0x08,           # mov   [rdi+8],  ecx
        0x89, 0x57, 0x0c,           # mov   [rdi+12], edx
        0x5b,                       # pop   rbx
        0xc3,                       # ret
    ])
    buf = mmap.mmap(-1, 4096, prot=mmap.PROT_READ | mmap.PROT_WRITE | mmap.PROT_EXEC)
    buf.write(code)

    FUNC = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32)
    addr = ctypes.addressof(ctypes.c_char.from_buffer(buf))
    func = FUNC(addr)

    result = (ctypes.c_uint32 * 4)()
    func(result, leaf)
    buf.close()
    return result[0], result[1], result[2], result[3]   # eax, ebx, ecx, edx


def bswap32(v: int) -> int:
    return struct.unpack(">I", struct.pack("<I", v & 0xFFFFFFFF))[0]


def generate_license():
    eax, ebx, ecx, edx = cpuid_via_asm(1)

    part1 = bswap32(eax)
    part2 = bswap32(edx)

    PSN = "%08X%08X" % (part1, part2)

    digest = hashlib.md5(PSN.encode("ascii")).digest()

    license_chars = []
    for i in range(16):
        license_chars.append("%02x" % digest[15 - i])
    license_key = "".join(license_chars)

    return PSN, license_key


def main():
    hwid, key = generate_license()
    print(f"HWID:    {hwid}")
    print(f"License: {key}")

    if len(sys.argv) > 1 and sys.argv[1] == "--apply":
        binary = sys.argv[2] if len(sys.argv) > 2 else "hack_app"
        path = os.path.realpath(binary)
        os.setxattr(path, b"user.license", key.encode("ascii"))
        print(f"License written to xattr 'user.license' on {path}")


if __name__ == "__main__":
    main()
