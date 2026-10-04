# SH3 PC-relative load optimization and native-loop experiments

## Retained change

Based on code `3dfde3aef3c5cf536aa754c742a83c16a3ddd752` (documentation HEAD
`e4604b851`). Worktree `testbuild/fbneo-sh3-jit`, branch
`ps4-sh3-native-chain-20261004`. Only PC-relative MOV.W/MOV.L address generation
is retained. Their effective address, read-map index, alignment and page offset
are known when compiling. Emit constants instead of recalculating and testing
them on every execution. Special addresses remain interpreted.

The read-map pointer and the actual value are still loaded on every execution.
There is no embedded RAM value or host data pointer; page remapping, DMA, cheats
and state loads therefore do not require a new invalidation scheme. Preserve
handler fallback, sign extension, endian conversion, EA, register state, cycle
accounting and instruction validation. No new thread/GPU path or guest clock
change. Existing AB JIT, Y row scheduler and 3dfde3 renderer remain in place.

## Native performance

Two sequential baseline/candidate/candidate/baseline trials per exact user
save state, no controls, fixed RTC, 16-bit video and three drawing threads.
state6: 180 frames; state/state1: 600 frames. Frame 0 is separate. Mean includes
all subsequent frames; P95 is computed from that same interval. No concurrent
compilation or other replay during timing. Negative values mean less elapsed
time; these are Linux host times, **not PS4 FPS**.

| Fixture | Mean, trial 1 / trial 2 | P95, trial 1 / trial 2 |
| --- | --- | --- |
| ibara.state6 | -0.69% / -0.58% | -0.29% / +0.51% |
| ibara.state | -1.84% / -1.79% | +1.25% / +2.28% |
| ibara.state1 | -2.05% / -0.50% | -10.90% / -7.84% |

This is a small, scene-dependent improvement. state6's immediate explosion
has no demonstrated P95 improvement; its sub-1% mean change is close to noise.
state's P95 regresses slightly. state1's P95 improves in both trials. Do not
claim that death-bomb stalls are solved or that first-frame JIT cost improved.

All 24 timed replays match their baseline video/audio hashes and final state.
Full CPU oracle comparisons pass GCC ASan/UBSan and Clang 18 -O3 (44,480 accepted
nonbranch encodings x3, 36,864 conditional cases, 22,528 delay-slot cases, 77
store/alias/device cases, 1,408 arithmetic edges, 3,200 random and 146 directed
cases). Additional 65 live PC-relative data/mapping/page/alias/device cases pass
GCC ASan/UBSan separately and are included in the full Clang run. Ibara 16/32,
DDPSDOJ, DDPDFK and Mushisam each pass 2,400-frame lifecycle replays against Y,
including reset, load state and rendering-thread changes.

Native candidate SHA256:
`c3c5fe1b747f3d006e109078e05f05fc9558a7c4501e86751f9662d40e404671`.
Evidence: `testbuild/ibara-native-loops-logs/validation-report.json`,
`literal-comparison.json`, `literal-repeat-comparison.json`, CPU result logs
and `literal-lifecycle-results.json`. Native scratch headers and library match
the retained source/candidate; no JIT profiling counters in that library.

## Rejected experiments

- Checked successor chaining: state6 mean +0.39%, P95 +2.57%; no reliable gain.
- Conditional taken-edge traces, up to 64 retired instructions: state6 mean
  +12.34%, P95 +7.78%; other fixtures also slower. Fewer dispatches did not
  translate into less elapsed time. The 512-entry trace pool filled in the
  first 17 frames, and generated code reached about 3.45 MB. Cache footprint
  and validation/early-exit costs remain plausible contributors, not proven
  individual causes.
- One complete loop per native trace: state6 mean +7.98%, P95 +11.00%.
- Same closed loops with one entry validation: mean +7.50%, P95 +12.21%.

All are reverted, with patches and binaries retained only in the external
experiment directory. Broad traces passed full CPU and five lifecycle tests;
closed/single-check variants passed focused trace sanitizer tests and state6
replays, not the full suite. A new trace regression caught PPC incorrectly
set to the target when a non-delayed taken branch ended a trace; the corrected
prototype passed, but **no trace implementation is enabled in retained code**.
Do not repeatedly reintroduce these variants based on dispatch counts alone.
The old-good renderer/JIT code remains the comparison baseline.

## Delivery boundary

No PS4 SELF/PKG built this round, no GitHub push and no klog connection or PS4
system change. Native objects are Linux-only and must not enter a PS4 package.
When separately requested, clean-build this worktree with explicit JIT=1,
retain Z frontend `f26f487ecd`, R4 metadata and independent Title ID. Final PKGs
remain in `dist/retroarch-ps4-upstream/out`; run pkg_validate and extracted
payload/SELF/SFO checks. Existing AB package, raw states and old branches remain.
