# Bounded scalar chunks in SH3 native opcode validation

Parent1155178d2893e110664d706d08c7c23ba25291aa. Keep original per-entry source/map/opcode validation and the8-word SSE2 prefix loop. Consume the remaining0..7words as4/2/1-word chunks, using builtin memcpy for alignment/alias safety. Never read beyond checked words, including guest page ends. This is separate from the previously rejected overlapping-vector-tail experiment.

Existing full Clang and GCC ASan/UBSan CPU suites pass, including73984 comparator/guard-page/mismatch cases, live opcode edits/maps, delayed branches, precise cycles, IRQ and device callbacks. No production diagnostics. Normal and no-JIT target hot-loop raw bytes, known tables and relocations are identical to the parent. Existing baseline sanitizer exception for original unaligned RL input is unchanged.

48 game replays (JIT on/off) match established video/audio/end-state hashes. JIT-on mean first/reverse: DDPSDOJ -1.84%/-2.03%, DDPDFK -0.65%/-0.77%, Ibara -0.51%/+1.00%. Reverse main-thread CPU mean: -1.78%/-0.60%/-0.74%. DDPS P95 improves1.70%/1.18%; DDP P95 +0.90%/~0%, so no demonstrated DDP tail improvement. Same120 slow-frame CPU windows: DDPS -1.53%, DDP -0.09%. No-JIT differences remain small/noisy despite byte-identical hot paths; DDP no-JIT reverse P95 +2.31% is reported rather than omitted.

Four2400-frame Ibara/DDPSDOJ/DDPDFK/Mushisama toggle/reset/state/AV-mask/render-core lifecycle runs match established video/audio/end-state hashes. Formal target object is checked byte-for-byte against the measured candidate before commit. All performance is host target-CPU/native-rest bridging on Ryzen5600/WSL; not PS4 FPS. Source and artifacts: testbuild/sh3-validation-chunks-logs. No PKG, push, klog or PS4 system changes.
