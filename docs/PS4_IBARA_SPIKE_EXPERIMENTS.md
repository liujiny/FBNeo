# Ibara slow-frame experiments (2026-10-04)

The user requested further optimization of the slowest frames after the
`3dfde3aef` explosion renderer change. This round establishes no further
performance improvement. All experimental production code was reverted;
`3dfde3aef3c5cf536aa754c742a83c16a3ddd752` remains the retained code baseline.
No new PS4 build or package was made.

## Findings

The two previous state6 candidate runs place their slowest frames within
roughly frames 1..16. Existing phase timing attributes this interval to
CPU/device work, with almost no blitter wait. Host-only JIT counters show
approximately 78k–102k lookups per early frame and 57k–79k native block calls.
First-frame compilation is substantial; frames 1..16 still compile roughly
15–91 blocks each. These counters do not isolate pure CPU time from all
in-handler device work, and do not demonstrate PS4 JIT activation.

A CPU sample location in the block lookup motivated cache experiments, but
sample location alone did not predict a useful change. No extra GPU work,
thread creation or system configuration changes were introduced.

## Rejected changes

Sequential baseline/candidate/candidate/baseline runs use the original
`ibara.state6`, 180 frames, no input, fixed time, 16-bit output and three
renderer lanes. Frame zero is reported separately. Mean and P95 below
average the two runs; positive changes mean **slower**. Small differences
may be noise; none establish a useful gain.

| Experiment | Mean time change | P95 change |
| --- | ---: | ---: |
| 1024-entry direct hotspot index | +0.66% | +2.50% |
| Compact four-way lookup tags | +5.62% | +3.16% |
| Only preserve native registers/maps used by each block | +0.94% | +1.29% |
| Retire blocks after eight consecutive exits of fewer than four instructions | +5.28% | +4.52% |

Every measured candidate matches the baseline video/audio hashes and final
state within its cold-replay fixture. The hotspot-index candidate additionally
passed the full CPU ASan/UBSan differential suite, added cache collisions and
arena-recycling cases, and five lifecycle replays. That correctness result
does not make it a performance improvement. The other rejected candidates
were not promoted to full CPU validation.

The reduced-frame experiment considered CMPSTR/SHAD/SHLD's use of host RBX,
not just guest memory operations. Its patch contains an ABI-sentinel test
proposal that was not executed; do not claim that test passed. All patches
remain outside the repository for future review, not as enabled features.

## Controls and limits

After reverting, rebuilding the native core produces exactly the original
SHA256 `0d587a85b6fa9a33a1a9915f85dc782a4380e8ad1d72c481df0aca2afd6223eb`.
Thus the retained source and scratch build are restored, including removal
of temporary profiling macros/functions. No Linux objects belong in a PS4
package.

An additional identical-binary control warms 180 frames, restores the same
state and measures again. Its P95 remains approximately 2.64–2.76 ms, so a
slow tail remains after prewarming. Do not present prewarming as a new game
optimization or compare its complete hash directly to the cold protocol:
video frames 1..179 match, but audio hashes and two final-state bytes differ
between cold and repeated-load protocols even for the baseline. This was
recorded, not silently attributed to the candidate or treated as an audio fix.
The reduced-frame warm experiment likewise shows no reliable gain and is
supporting evidence only (brief overlap with a read-only analysis process).

Evidence: project `testbuild/ibara-spike-logs/validation-report.json`, the
four `*-comparison.json` files, `control-rebuild.json`, `counters/counters.csv`
and the archived experiment patches. The supplied states remain unmodified
in the fixed out directory. No state, ROM or binary is committed.

The next CPU investigation should measure actual instruction/branch paths in
the early slow frames and consider longer continuous native execution or
loop translation, with precise cycle/interrupt, code-write and delay-slot
handling. Repeating these four lookup/entry heuristics is not justified by
these measurements. Previous rendering improvements remain independently
validated; no PS4 FPS improvement is claimed for this round.
