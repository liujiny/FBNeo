# Guarded SH3 DT native translation (2026-10-05)

This change extends the existing experimental SH3 JIT with DT (decrement and test), retaining the default-disabled core option. The DDPSDOJ stored-scene diagnostic counted 5,793,798 negative-cache stops at DT, versus 54,871 in Ibara; these are rejection records, not a complete instruction profile.

## Semantics

The legacy DT always reads the post-fetch PPC through RW when BUSY_LOOP_HACKS is enabled, even when no busy-loop iterations will run. Native DT reloads the live READ mapping and checks the following word. A handler mapping or 0x8bfd (BF -2) exits before DT; the interpreter then performs the decrement, read callback, and original busy-loop cycle accounting. Ordinary direct memory uses native decrement and SR.T update. No ROM-specific address or cached map pointer is introduced.

DT is implemented in Compiler::op, not Compiler::alu: it must not be fused as a pure ALU delay slot. The existing dispatcher continues to validate every compiled instruction and enforce pending interrupt, cycle-budget, opcode-edit, alias and page guards. Current WaitState is disabled in sh4.cpp; this assumption must be revisited if that implementation changes.

## Validation

GCC ASan/UBSan and Clang18 complete production-loop differential suites passed: 3,072 new DT boundary cases, all 44,544 accepted native encodings, existing randomized CPU/device state, RAM, cycles, interrupt, timer, branch/delay, live-code and memory tests. The new tests exercise native execution, zero/wraparound counters, busy-loop patterns, separate READ/FETCH backing, runtime handler remapping and callbacks, cross-page peeks, aliases, and taken delay slots.

28 sequential recorded-scene comparisons used DDPSDOJ600 frames and Ibara.state6 180 frames. Video/audio and ending state hashes match baseline in every run. Both games also passed 2,400-frame JIT-toggle/reset/checkpoint/restore/render-option lifecycles against the earlier Y references.

The actual pinned PS4 CPU object was hosted with an audited mmap/mprotect ABI adapter in the Linux replay harness. This preserves PS4 compiler output, but runs on the PC CPU and native rest-of-core; it is NOT PS4/Jaguar emulation or hardware FPS.

| Target object comparison | DDPSDOJ mean / P95 | Ibara mean / P95 |
| --- | --- | --- |
| Initial forward/reverse | -2.49% / -4.30% | -0.34% / -0.61% |
| Separate ABBA repeat | -5.47% / -6.05% | -0.54% / -8.02% |

Ibara mean is effectively unchanged and its P95 is noisy. In the initial comparison DDPSDOJ still costs 4.04% more with this JIT than without JIT; keep the option default disabled and continue investigating short-block overhead. Native GCC showed larger DDPSDOJ gains (-8.25%) but is not substituted for target-object evidence.

Evidence: project testbuild/sh3-dt-logs contains candidate and baseline sources, both CPU suites, compiler/assembly files, comparison.json, confirmation.json, summaries, and toggle-lifecycle-results.json. The first directed fixture incorrectly left a handler map installed across reset; fixing reset setup resolved that test-harness failure without altering production code. See test-fixture-note.txt.

No full PS4 SELF/PKG, GitHub push, klog connection or PS4 system modification was performed. Diagnostic host-adapted objects must never be packaged. A future PS4 package requires a clean target build with the established frontend and packaging rules.
