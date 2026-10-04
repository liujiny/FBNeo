# SH3 JIT guarded exit layout

Parent: 8baba6a523c7e2040ceb70770143be9dbdbd98b6 (guarded DT).

Generated RAM load/store and DT guards previously jumped over an inline failure return even when all guards succeeded. Queue each failure return and place it after the normal region return, so successful accesses fall through without that unconditional jump. Each queued exit captures the completed guest instruction count and dirty guest/host register mapping at the exact original exit location. All alignment, handler, internal-address, code-alias and DT busy-loop checks remain. Every native entry still validates all opcodes. No block chaining or guest timing changes.

The queue has at most MAX_OPS entries. Generated size is checked after emitting all failure stubs; overflow falls back to the interpreter. Metadata lives only in the cold compiler frame and does not enter save states. Native dispatcher disassembly is identical to the parent (948 bytes). Default JIT remains disabled.

Validation (local, not PS4 hardware):
- Complete Clang18 and GCC ASan/UBSan CPU differential suites, including728 added dirty-register spill/map/callback/code-alias cases and existing44,544 opcode encodings, branches/delays/DT/IRQ/timers.
-20 sequential actual-PS4-CPU-object host replays, all video/audio/end-state hashes match the parent. Native renderer, Linux VM/libc and PC CPU remain; this is not Jaguar emulation or PS4 FPS.
- Initial ABBA: DDPSDOJ600 mean-0.39%, P95-0.24%; Ibara180 mean-3.97%, P95+0.72%.
- Independent reversed ABBA: DDPSDOJ mean-1.95%, P95-2.63%; Ibara mean-0.93%, P95-2.26%. Ibara variance is material; no stable tail-latency improvement is claimed yet.
- Both games2400-frame JIT toggle/reset/checkpoint/restore/render-option lifecycle hashes match the recorded Y oracle.

Evidence: testbuild/sh3-cold-exit-logs/{comparison.json,repeat.json,summary.json,cpu-clang,cpu-gcc,toggle-lifecycle-results.json,hot-audit.json}. No SELF/PKG generated or GitHub push. Hardware validation remains pending.
