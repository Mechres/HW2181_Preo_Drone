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
- **`trace_updatemotors.py [seconds] [max_hits]`** — breakpoints the real
  per-tick `UpdateMotors` call (`0x178a`) and dumps `r0-r3`
  (yaw/throttle/roll/pitch) on every hit. This is what found the
  Session 3 result (see `../SESSION3_FULLCHAIN_TEST.md`): those args read
  `0,0,0,0` regardless of writes to the "verified" RAM cells, in the
  never-paired state.
- **`continuous_stick_write.py <throttle_raw> <duration_s>`** — writes the
  four stick-value RAM cells in a tight loop (not one-off pokes), to test
  whether *sustained* writes (mimicking a real packet stream) get past
  the firmware's internal signal-acquisition hysteresis where a single
  write didn't. **Can actually spin a motor if it works — no propellers,
  someone watching, every time.** Run alongside `trace_updatemotors.py`
  in a second terminal to see live whether the real call's args ever stop
  reading zero while this is running.

None of these implement RF register read/write — those aren't memory-
mapped from the debug port's perspective (see the "things that bit us"
section of the debugging guide). Also see that guide's gotchas on
`mdw`/`mww` (word-width) silently failing/corrupting on these
not-word-aligned RAM cells, and on hardware watchpoints appearing
non-functional on this target — both bit an earlier version of these
tools.
