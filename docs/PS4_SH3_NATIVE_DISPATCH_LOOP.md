# Keep the SH3 native dispatcher frame across regions

Parente21ea8cdaeb48af029d9d1194b4b216f57bb01dc (MULL/MAC support).

The caller previously looped over sh3_x64_run(), returning/re-entering its C++ frame after every successful region. Move the loop inside the dispatcher. Every iteration still recomputes entry gates, PC/fetch mapping, cache lookup, full opcode comparison and cycle admission. This does not directly link native blocks, skip validation, or hold stale guest code across stores.

Internal return contract: true only when a successful native region exhausts the budget, so the caller goes to finished; false leaves the next instruction to the interpreter. Keeping this distinction preserves the legacy first interpreted instruction when the initial budget is zero. Guarded zero-completed exits return immediately, avoiding a spin. Delay/IRQ/device/timer/save-state behavior is unchanged. No new runtime profiling or game-specific behavior. JIT remains default disabled pending hardware results.

Validation:
- Complete Clang18 and GCC ASan/UBSan CPU differential suites, including521 additional multi-region/MULL/zero-budget/guard/device boundaries and existing44,896 opcode encodings.
-24 sequential actual-PS4-CPU-object host replays: all video/audio/end-state hashes match parent.
- Initial ABBA: DDPSDOJ600 mean-4.29%,P95-0.16%; Ibara180 mean-7.09%,P95-6.21%.
- Reversed repeat: DDPS mean-5.01%,P95-2.67%; Ibara mean-4.59%,P95-3.07%. Baseline/candidate variance remains visible in raw measurements.
- Same candidate enabled vs disabled: DDPS mean-3.19%,P95+5.05%; Ibara mean-32.40%,P95-22.09%. DDPS average now edges ahead locally, but slow-frame performance still trails the interpreter.
- Both2400-frame toggle/reset/checkpoint/restore/render-option lifecycles match recorded Y references.

The target dispatcher grows948->1064 bytes and saves more registers at entry; measured gains, not code size, justify retention. The JIT-enabled threaded wrapper also grows(0xa6a0->0xb60b); non-JIT wrappers retain their sizes. These observations do not isolate the exact microarchitectural cause.

Evidence: testbuild/sh3-native-loop-logs/{comparison.json,repeat.json,summary.json,cpu-clang,cpu-gcc,toggle-lifecycle-results.json,hot-audit.json}. Native rest of core/Linux VM/libc/PC CPU remain in this bridge experiment: results are not PS4 hardware FPS/Jaguar emulation. No SELF/PKG/push/klog. Hardware confirmation pending.
