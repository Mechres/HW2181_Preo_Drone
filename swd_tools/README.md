# SWD Tools

All of these talk to a persistent `openocd -f ../openocd_hw2181.cfg`
session via its Tcl RPC port (6666) — start that first. See
`../SWD_DEBUGGING_GUIDE.md` for the full setup (wiring, config, gotchas).

- **`iap_program_page.py <page_num> <source.bin>`** — erase + reprogram
  one 1KB flash page from a source image, with byte-for-byte verify.
  This is the tool that applied `PATCH_CRC_BYPASS.md`.
- **`mem_check.py`** — one-shot read of the two locations most useful for
  checking "did the drone receive anything real" (see docstring in the
  file for which addresses and why).
- **`bp_watch.py <addr> [seconds]`** — set a hardware breakpoint, resume
  the target, and report if/when it fires. Read the caveat in its
  docstring before trusting a hit as a real signal — several candidate
  addresses on this firmware fire unconditionally every main-loop tick.

None of these implement RF register read/write — those aren't memory-
mapped from the debug port's perspective (see the "things that bit us"
section of the debugging guide).
