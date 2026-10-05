# Exact x86-64 DIV1 status calculation

Based on bed52d2ea. Preserve the original add/sub decision and replace the nested Q/T status branches with boolean arithmetic. Let sign be the old Rn sign and carry be unsigned carry (addition) or borrow (subtraction): new Q = sign XOR carry XOR old M; new T = NOT(sign XOR carry). Rn is shifted and written before Rm is read, preserving n==m behavior. Other architectures retain the original implementation. No emulated cycle changes, native opcode expansion, timing shortcuts or new diagnostics.

The PS4 compiler emits conditional moves for the source-level add/sub choice: 144 bytes with no jumps, versus the original 264 bytes with 13 jumps. The prior 64-bit-add implementation was rejected despite being smaller (131 bytes) because independent-input throughput regressed.

Validation: Clang and GCC ASan/UBSan full CPU suites pass, including 365888 cases against an independent copy of the original DIV1 handler, all register aliases, all Q/M/T combinations, edge values and random full-SR values. Existing RAM unaligned oracle limitation remains documented in the parent RAM report; no new sanitizer suppression. 48 game replay video/audio/endstate hashes match golden fixtures across legacy and dual-clock harnesses. Four 2400-frame lifecycle runs (Ibara, DDPSDOJ, DDPDFK, Mushisam), including JIT toggle, reset, checkpoint/restore and rendering-option changes, match the existing reference hashes.

Actual PS4 function code hosted on this PC: independent-input microbenchmark mean 7.98 -> 7.59 ns (~4.8% lower); dependent 32-step chains 8.28 -> 5.25 ns (~36.6% lower). All checksums match. These are narrow host measurements, not PS4 hardware timings or whole-game gains.

Whole-game target-object replays: DDPDFK JIT-on mean -1.32% first pass, -0.56% reverse dual-clock; P95 -2.92%/-0.97%. DDPSDOJ JIT-on +0.39%/-0.28%; Ibara +0.51%/-0.60%, effectively flat. JIT-off results and tail timings vary; full raw/summary results are retained. No claim of stable PS4 FPS improvement. JIT remains disabled by default.
