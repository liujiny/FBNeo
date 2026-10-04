# SH3 native multiplication and MAC register transfers

Parent0d94b538979c8121375bbc5ce8db99ac7fcc1d13 (cold guard exits).

Translate MULL into a low32-bit host multiply and immediate architectural MACL update; also translate STSMACL/STSMACH/LDSMACL/LDSMACH through live MAC globals. These operations stay outside pure GPR/SR delay-slot fusion. MULL retains its extra guest cycle: nonbranch_cycles tracks exact prefix extras separately from conservative branch admission margin. Normal returns, queued partial guard exits and taken branches include their exact prefix extras; admission includes the whole region's conservative cost. No changes to game clocks, IRQs, timers, device callbacks or save-state data. Runtime native dispatcher disassembly stays identical to the parent. Default JIT remains disabled.

Validation:
- Complete Clang18 and GCC ASan/UBSan differential CPU suites,759 added multiply/MAC/budget/guard/branch/delay/DT/callback cases;44,896 accepted opcode encodings. Random IRQ/timer streams now include MAC operations.
-28 sequential actual-PS4-CPU-object host replays match all video/audio/end-state hashes.
- DDPSDOJ600 initial mean/P95 -2.47/-3.06%; reversed repeat -1.41/-4.99% against cold-exit parent.
- Ibara180 initial +2.52/+8.78%; reversed repeat -2.39/-5.84%; additional ABBA -1.61/-5.23%. Preserve the initial regression in the evidence; Ibara results are variable, not a guaranteed tail-latency improvement.
- Same candidate enabled vs disabled: DDPS mean+1.96%,P95+8.02%; Ibara -32.12/-21.05%. DDPS JIT has not decisively beaten the interpreter.
- Both2400-frame toggle/reset/checkpoint/restore/render-option lifecycles match recorded Y references.

Evidence: testbuild/sh3-multiply-logs/{comparison.json,repeat.json,ibara-check.json,summary.json,cpu-clang,cpu-gcc,toggle-lifecycle-results.json,hot-audit.json}.

All timing is local PC execution of the PS4-compiled CPU object, with native renderer and Linux VM/libc, not PS4 hardware/Jaguar emulation. No SELF/PKG/push/klog. Hardware validation remains pending.
