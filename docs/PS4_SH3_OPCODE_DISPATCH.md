# SH3 opcode dispatch experiment (PS4 T)

S baseline: `efb057ecb65ef9080ce135c37380ba0f911d3141`.

`execute_one` previously selected an instruction with an outer high-nibble switch and several nested switches. CPU initialization now builds 65,536 function pointers using the same decoding decisions; execution selects the existing handler through the fetched 16-bit opcode. Groups 0 and 4 retain their existing low-byte decoders. This is a partial flattening, not a rewritten SH3 CPU or a decoded-RAM cache.

The table costs 512 KiB on PS4/x86-64 and one load plus an indirect call per instruction. It is initialized by Sh3Init and is independent of CPU state. Reset and save/load do not rebuild or serialize it. Instruction bodies, fetches, PC progression, delay slots, IRQ handling, cycle accounting, timer optimization and emulated CPU clock remain unchanged. Self-modifying RAM is still fetched on every instruction.

A first experimental implementation supplied during model handoff had duplicate table definitions and a UINT16 initialization loop that wrapped forever. Those issues were fixed before execution. The retained implementation is rebuilt from the S decoder, avoiding a duplicated default-fill loop and reset-time initialization. The raw experiment is archived locally, not used in the package.

## Validation completed on Linux

- ASan/UBSan exhaustive mapping and operand checks for all 65,536 opcodes; unchanged group 0/4 bodies.
- Serial ABBA: S/T/T/S, 3,000 frames per run and game, RGB565, one render core, fixed test clock and identical pulsed gameplay inputs.
- DDPSDOJ mean core time: S 5.10610 ms, T 4.00855 ms (21.49% reduction).
- DDPDFK mean core time: S 3.67960 ms, T 2.89000 ms (21.46% reduction).
- Every benchmark run has identical per-frame video/audio hashes and final state within each game.
- Both games: 3,000-frame 2/3-thread candidates match S; 2,400-frame lifecycle replays match S (reset300, save1200, load1600, audio-only1800-1899, thread changes700/1400/2100).

These results establish host correctness for the covered scenes and host speed improvement. PS4 frame-rate gain and full-speed gameplay require hardware validation. No automatic frame skipping or CPU downclocking was added. The newer-ROM sound issue remains deferred at the user's request.

## Bounded multicore observations

T adds `cpu_mask` to existing `[PS4 CVPERF]` windows. Bit N means the main emulation thread was observed on CPU N at frame start during that window; this is not an affinity/availability mask or a utilization measurement.

`[PS4 CVBLIT] jobs=N worker_us=X cpu_mask=M` records wall duration and observed CPU placement for the ordered blitter worker. Each report averages 300 jobs, not frames. Duration includes scheduling interruptions and excludes the main thread's completion wait. Scheduling and job size can vary. Main diagnostics stop after 7,200 frames; worker diagnostics stop after 7,200 jobs, with at most 24 reports each. After the cap, no timing or CPU-placement syscalls run. Worker counters are reset only after joining the previous worker.

These observations support deciding whether CPU placement, device waits or independent drawing work are the next bottleneck. The CV1000 list remains one ordered worker because commands can read VRAM written by earlier commands. No eight-way command split or unverified affinity pinning was added.

Source-local diagnostic checks: `tests/console_replay/cv1k_profile.cpp`, `epic12_profile.cpp`, and existing `epic12_worker.cpp`. Raw replay/benchmark/build evidence remains in project `testbuild/t-dispatch-logs`.
