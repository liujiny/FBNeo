# PS4 SH3 JIT validation cost and runtime fallback (2026-10-04)

Parent: `3ee52a0efdda3a7e48d9955af8dac03a9d34f693` (AC). Working branch:
`ps4-jit-validation-fix-20261004`, in `testbuild/fbneo-sh3-jit`.
The user reports AC slower than the remembered pre-JIT version; the exact
hardware comparison package/settings are unknown. Do not label this an
established AC-versus-AB 10 FPS regression.

## Change

AC's PS4 ELF calls bcmp/memcmp at each positive native entry. The linked
OpenOrbis implementation compares one byte per iteration, unlike host glibc.
Replace only this local equality operation with bounded, unaligned SSE2 loads
of eight opcodes at a time plus a word tail. Compare every checked opcode on
every entry, including the rejected terminator. Never read past the checked
range. Keep code aliases, DMA/cheats, self-modification and state-load guards.
No SDK/libc or console system changes.

Add `fbneo-sh3-jit` / `SH3 JIT (experimental)` with `disabled` (default) and
`enabled`. Apply through check_variables between frames. This host preference
is not serialized and does not alter the emulated clock. A disabled JIT or
failed executable allocation selects a separately compiled interpreter loop,
with no JIT checks/lookups per guest opcode. Reset/state restoration keep the
selected preference. Re-enabling still validates all native entries.
A failed allocator/protection operation falls back until core teardown.

The JIT build now has three dispatch tables (ordinary interpreter, sliced
interpreter, sliced JIT), one additional 512 KiB table on x64. JIT metadata and
executable mappings remain allocated lazily, only if enabled. Non-JIT builds
still have two tables and no core option. This feature still requires
`FBNEO_SH3_X64_JIT=1` at build time. Runtime default is now OFF.

## Validation

Evidence: `testbuild/jit-validation-fix-logs` outside the source worktree.

- GCC ASan/UBSan and Clang 18 O3 CPU oracle suites pass: 44,480 accepted opcode
  encodings x 3, 36,864 branch-boundary and 22,528 delay cases, existing memory,
  device, IRQ/timer and state comparisons, plus 73,984 exact-length/mismatch/
  guard-page checks and 164 public-dispatch budgets with runtime switching,
  code editing/state restoration and allocation/protection failure fallback.
- The non-JIT threaded suite also passes (3,200 random/146 directed cases).
- Both changed PS4 translation units compile with the AC pinned Docker image
  and original target flags. Their objects are isolated in the evidence
  directory; existing PS4 build objects were not replaced.
- Actual PS4 sh3_x64_run disassembly has movdqu/pcmpeqb/pmovmskb, a bounded
  16-bit tail, no bcmp/memcmp call. Disabled dispatch contains no JIT references.
- Native core SHA256: `1cfc0f8b302d3cb42c473f0104ecb36cf0c6b69087a7706f54b29f46d9c9b69a`.
  Final macro-guard rebuild is byte-identical to all tested binaries.
- 20 original-state replays: DDPSDOJ 600 frames, Ibara.state6 180, no input,
  16-bit, 3 rendering threads, fixed time; on/off/compiled-out/PS4-comparator
  control, default and every-60-frame toggling. Every frame video/audio and
  end-state hashes agree within each fixture. Timing starts after the first
  frame; first-frame cost is reported separately. Runs are sequential with no
  concurrent compilation. Detailed protocol and results are in compare.py,
  comparison.json and summary.json.

- Ibara and DDPSDOJ each pass a 2,400-frame lifecycle run, toggling JIT every
  60 frames while exercising reset, checkpoint/restore, rendering-thread
  changes and video suppression. Per-frame video/audio and final state agree
  with the stored Y interpreter reference (toggle-lifecycle-results.json).

## Host results, not PS4 FPS

Paired forward/reverse runs, averages of means and P95s (milliseconds):

| Fixture | AC with actual PS4 byte comparator | Fixed JIT on | Fixed JIT off |
|---|---:|---:|---:|
| DDPSDOJ mean / P95 | 2.654 / 4.151 | 2.166 / 3.368 | 2.012 / 2.919 |
| Ibara.state6 mean / P95 | 2.898 / 6.570 | 1.668 / 3.201 | 2.215 / 3.798 |

On vs PS4-byte comparator: mean -18.38% / -42.43%, P95 -18.87% / -51.28%.
DDPSDOJ JIT on still costs +7.67% mean / +15.40% P95 against off. Ibara on
saves 24.68% mean / 15.73% P95 against off. Runtime-off is near compiled-out
control (DDPSDOJ mean +0.69%, Ibara -2.13%; small changes/layout/noise).

Do not turn these host numbers into PS4 FPS or automatically enable JIT for
all games. Keep disabled as default until same-package, same-state hardware
A/B validation. The GPU/renderer, frontend/audio, timers and guest clocks are
unchanged. No new SELF/PKG, hardware test or GitHub push in this source round.
For a future package use a clean PS4 build of this worktree, JIT=1 to expose
the switch, Z frontend f26f487ec, existing R4 metadata and fixed out directory;
run pkg_validate and payload audits. Never package the Linux scratch objects.
