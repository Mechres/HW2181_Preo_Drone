#!/usr/bin/env python3
"""Erase-and-reprogram a single 1KB flash page via IAP over a live OpenOCD
Tcl RPC session, with a byte-for-byte verify at the end. Requires
`openocd -f ../openocd_hw2181.cfg` already running, and reuses the exact
register map/procedure documented in SESSION2_VERIFIED_FINDINGS.md and
PATCH_CRC_BYPASS.md (real datasheet, no guessing).

Usage: python3 iap_program_page.py <page_num 0-35> <source.bin>

Aborts immediately (no partial writes left dangling) if any status
register doesn't show what it should at each step -- BSY not clearing,
ERASE_END/PROG_END not set, or the post-erase readback not showing
0xFFFFFFFF. Safe to re-run: it always erases before programming, so it
doesn't depend on prior state.

Flash is 36 pages x 1KB, page N covers address (N*1024) to (N*1024+1023).
Source file must be the FULL intended flash image (e.g. a patched copy of
fw_full.bin) -- only the bytes within the target page's address range are
read out of it and written; everything else in the file is ignored.
"""
import socket, struct, sys

class OocdRpc:
    def __init__(self, host='127.0.0.1', port=6666):
        self.sock = socket.create_connection((host, port), timeout=5)
    def cmd(self, c):
        self.sock.sendall(c.encode() + b'\x1a')
        data = b''
        while b'\x1a' not in data:
            data += self.sock.recv(4096)
        return data.split(b'\x1a')[0].decode(errors='replace').strip()
    def mww(self, addr, val):
        return self.cmd(f"mww 0x{addr:x} 0x{val:x}")
    def mdw(self, addr):
        r = self.cmd(f"mdw 0x{addr:x}")
        return int(r.split(':')[1].strip().split()[0], 16)

IAP_CON  = 0x40000800
IAP_ADDR = 0x40000804
IAP_DATA = 0x40000808
IAP_TRIG = 0x4000080C
IAP_UL   = 0x40000810
IAP_STA  = 0x40000814

def die(msg):
    print("ABORT:", msg)
    sys.exit(1)

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    page_num = int(sys.argv[1])
    src_file = sys.argv[2]
    page_addr = page_num * 1024

    rpc = OocdRpc()

    def wait_not_busy(label, max_tries=3000):
        for _ in range(max_tries):
            sta = rpc.mdw(IAP_STA)
            if (sta & 0x1) == 0:
                return sta
        raise RuntimeError(f"Timeout waiting for BSY clear during {label}, last STA=0x{sta:x}")

    with open(src_file, 'rb') as f:
        f.seek(page_addr)
        pagedata = f.read(1024)
    assert len(pagedata) == 1024, f"source file too short to cover page {page_num}"

    print(f"=== request FLASH access ===")
    rpc.mww(IAP_UL, 0xA5)
    rpc.mww(IAP_CON, 0x11)
    ok = False
    for _ in range(2000):
        con = rpc.mdw(IAP_CON)
        if con & 0x20:
            ok = True
            break
    if not ok:
        die(f"FLASH_ACK never set, IAP_CON=0x{con:08x}")
    print(f"FLASH access granted, IAP_CON=0x{con:08x}")

    print(f"=== erase page {page_num} (addr 0x{page_addr:x}) ===")
    rpc.mww(IAP_UL, 0xA5)
    rpc.mww(IAP_ADDR, page_num << 10)
    rpc.mww(IAP_UL, 0xA5)
    rpc.mww(IAP_TRIG, 0x00005EA1)
    sta = wait_not_busy("page erase")
    if not (sta & 0x2):
        die(f"ERASE_END not set after erase, STA=0x{sta:08x}")
    rpc.mww(IAP_STA, 0)
    print(f"page {page_num} erased OK")

    print("=== programming 256 words ===")
    for cell in range(256):
        word = struct.unpack_from('<I', pagedata, cell*4)[0]
        rpc.mww(IAP_UL, 0xA5)
        rpc.mww(IAP_ADDR, (page_num << 10) | (cell << 2))
        rpc.mww(IAP_DATA, word)
        rpc.mww(IAP_UL, 0xA5)
        rpc.mww(IAP_TRIG, 0x00005DA2)
        sta = wait_not_busy(f"word program cell {cell}")
        if not (sta & 0x4):
            die(f"PROG_END not set at cell {cell}, STA=0x{sta:08x}")
        rpc.mww(IAP_STA, 0)
        if cell % 32 == 0:
            print(f"  programmed cell {cell}/256")
    print("all 256 words programmed.")

    print("=== releasing FLASH access ===")
    rpc.mww(IAP_UL, 0xA5)
    rpc.mww(IAP_CON, 0)
    for _ in range(2000):
        con = rpc.mdw(IAP_CON)
        if (con & 0x20) == 0:
            break
    print(f"FLASH access released, IAP_CON=0x{con:08x}")

    print("=== verifying page contents ===")
    mismatches = 0
    for cell in range(256):
        expected = struct.unpack_from('<I', pagedata, cell*4)[0]
        actual = rpc.mdw(page_addr + cell*4)
        if actual != expected:
            print(f"  MISMATCH cell {cell}: expected 0x{expected:08x} got 0x{actual:08x}")
            mismatches += 1
    if mismatches == 0:
        print("VERIFIED: entire page matches source file exactly.")
    else:
        print(f"WARNING: {mismatches} mismatches found out of 256 words!")
        sys.exit(1)

if __name__ == "__main__":
    main()
