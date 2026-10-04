# Avoid the DDPS code-arena rebuild burst

Parentdabc93383df780a0c271d48d0b57be5175bb4a69 (internal native dispatcher loop).

Change only CODE_BYTES from4MiB to8MiB. Cache metadata/associativity, opcode validation, W^X protection, bounded recycling and allocation-failure interpreter fallback are unchanged. Maximum virtual mapping grows by4MiB; PS4 physical commitment is not assumed to match Linux. JIT remains disabled by default until hardware validation.

Evidence for the cause: in the600-frame DDPSDOJ state replay, the4MiB arena's only recycle falls in zero-based frames240-269. This produces1940 successful native builds vs171 in the preceding30frames. Independent uninstrumented timing previously found that window36% slower withJIT than the interpreter. With8MiB, the same window builds166 blocks and has no recycle. Total successful builds6798->4443, emittedbytes8,316,590->5,434,557, finalused5,467,072bytes. Per-frame lookups/native blocks/native ops/zero exits/budget exits are identical; guest execution is unchanged. Ibara uses2,991,888bytes and all diagnostic counters are unchanged.

Validation:
- Clang18 and GCC ASan/UBSan complete CPU suites, including forced full-arena recycle, stale-entry clearing, edited code reuse, release and existing allocation/protection failures.
-24 sequential actual-PS4-CPU-object uninstrumented host replays preserve every video/audio/end-state hash.
- DDPS600 initial mean/P95 -7.03/-12.10%; reversed repeat -6.55/-13.07% vs4MiB. Slow240-269 window -33.74% and-43.01% respectively.
- Ibara180 mean -1.15/-0.42%, essentially flat; P95 -2.50/+8.70% is inconsistent. No Ibara tail improvement is claimed; retain raw variability for hardware follow-up.
- Same8MiB candidate JIT on/off repeat: DDPS mean-4.83%,P95-0.68%(near parity forP95); Ibara mean-34.48%,P95-21.64%.
- Two2400-frame toggle/reset/checkpoint/restore/render-option lifecycles match Y references.
- Separate diagnostic4/8MiB replay hashes also match. Instrumented timing is not used for benefit claims. Diagnostic code is outside production files.

Evidence: testbuild/sh3-cache8m-logs/{comparison.json,repeat.json,summary.json,cpu-clang,cpu-gcc,toggle-lifecycle-results.json,dispatcher-diff.txt}; testbuild/sh3-tail-profile-logs and sh3-cache8m-profile for counters. Target dispatcher differs only in allocation-capacity constants, not executed-region validation.

This is local PC execution of the PS4 CPU object with native rest-of-core/Linux VM, not PS4/Jaguar emulation or measured hardware FPS. Longer sessions can still fill8MiB; this is not a claim that all compilation stalls are eliminated. No SELF/PKG/push/klog performed.
