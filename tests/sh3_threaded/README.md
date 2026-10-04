# SH3 direct-threaded execution validation

The candidate reuses production instruction handlers and compares both complete
execution loops, including scanned CPU/device state, mapped RAM, returned cycles
and timer callback counts. Run with:

```sh
python3 tests/sh3_threaded/run.py --sanitize --output /tmp/sh3-threaded-test
```

Coverage: 3,200 randomized bounded instruction streams in normal and slice-timer
modes, plus 44 aligned memory operations and two live opcode-edit cases. Includes
zero cycle budget (legacy do/while semantics), delayed branches crossing run
boundaries, pending IRQ entry, timer callbacks, CPU-off behavior and fallback
operations. The fixture asserts that IRQs and timer callbacks actually occurred.

Random ALU streams exclude register-indirect memory operations: they can create
misaligned guest operands and trigger pre-existing host-cast alignment UB in the
old SH3 implementation. Aligned memory operations are checked separately. This
suite does not claim to fix or validate arbitrary malformed guest programs.

Full native libretro replays separately compare video, audio and save state for
Ibara, DDPSDOJ, DDPDFK and Mushisama. This is not a substitute for PS4 testing.
Set `FBNEO_SH3_THREADED_DISPATCH=0` at build time to select the original function
pointer run loop. No code-cache invalidation or new save-state fields are needed:
opcodes are still fetched from the live guest map on every instruction.

Validated on 2026-10-04: GCC ASan/UBSan, Clang 14.0.3 from the PS4 image
with Linux native target at -O3, and host Clang 18 at -O3. Production PS4
translation-unit compilation also passed; PS4 execution is still pending.
