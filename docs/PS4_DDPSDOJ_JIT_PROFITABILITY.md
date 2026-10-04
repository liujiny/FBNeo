# DDPSDOJ JIT profitability and PS4 cold compilation (2026-10-04)

Base: ca48f8c4e54cfef04deab8f687c61e2ae105af06. User asked why JIT is
slower for the uploaded ddpsdoj.state and whether it can be optimized.
This is DDPSDOJ (SaiDaiOuJou), not a new DDPDOJ/PGM ROM experiment.

## Measured cause

Host counters on the actual original state, fixed-time/no-input replay:

| Counter | DDPSDOJ / 600 frames | Ibara.state6 / 180 frames |
|---|---:|---:|
| JIT lookups | 29,246,649 | 7,139,778 |
| Existing rejected/short-block hits | 20,593,556 (70.41%) | 2,281,150 (31.95%) |
| Successful native entries | 8,263,287 (28.25%) | 4,834,715 (67.72%) |
| Native guest instructions | 108,101,756 | 75,779,596 |
| Entries returning before any opcode | 358,223 (1.22% of lookups) | 7,973 (0.11%) |
| New blocks compiled | 6,376 | 1,925 |

DDPSDOJ pays roughly 48,744 lookups per frame, but enters native code only
13,772 times, averaging 13.08 instructions per successful entry. Its negative
hits span lengths 0..7 (see profile.json). A negative hit runs the interpreter
without recompiling: these are not 20 million failed compilation attempts.
Partial native completion also includes normal taken conditional branches,
not just memory guards. These counters support repeated entry/fallback cost
as an important factor; they do not attribute every PS4 lost frame.

Both diagnostic runs preserve per-frame video/audio and final-state hashes.
No profiler or new logging is retained in production.

## Experiments and rejection decisions

Sequential fixed-time replays, 16-bit output, 3 drawing threads, no input,
DDPSDOJ 600 frames / Ibara.state6 180. First frame reported separately and
excluded from mean/P95. Forward/reverse ordering. No concurrent builds.

| Experiment | DDPSDOJ mean / P95 change | Ibara mean / P95 change |
|---|---:|---:|
| Minimum native length 4 | +3.51% / +0.65% | -1.94% / +1.09% |
| Minimum native length 12 | -1.53% / -3.07% | +4.16% / +4.06% |
| Minimum native length 16 | -1.51% / -5.10% | +7.01% / +9.17% |
| 4 KiB fast negative-PC cache | -5.61% / -7.66% | -0.95% / +3.40% |

A second negative-cache experiment (plus compile outlining) gives DDPSDOJ
-3.46% mean / -3.92% P95, but Ibara +3.08% / +5.37%. One Ibara run is notably
noisier; neither run establishes a stable Ibara improvement. DDPSDOJ remains
slower than JIT-off even with this candidate. Do not adopt these thresholds
or the extra cache globally. They are preserved only as external experimental
headers/binaries. All 32 experiment/confirmation replays match each fixture's
video/audio and final-state hashes; these candidates did not get the complete
CPU oracle suite and are NOT production-approved.

## Retained production change

Only outline `Sh3X64::compile` with noinline. PS4 Clang had expanded the large
emitter/compiler frame into sh3_x64_run, including every negative cache hit.
The PS4 target hot function shrinks from 1,635 to 948 bytes. Its entry changes
from six pushes plus a 0x2078 (8,312-byte) stack reservation to three pushes
(two saved registers and one alignment slot), with no large reservation.
Stack reservation itself just adjusts the pointer; do not equate it with
copying or clearing 8 KiB. Compilation now has a separate frame on misses.

Every opcode validation, mapping guard, cycle boundary and fallback decision
is unchanged. The SSE2 checker, runtime JIT switch/default OFF, original
minimum length 8 and existing renderer are preserved. No extra cache, logging,
per-game timing hacks, guest-clock changes or system changes are retained.

Host GCC already outlined compile. Consequently the final native .so is
byte-identical to ca48f8c4e's tested baseline, SHA256:
1cfc0f8b302d3cb42c473f0104ecb36cf0c6b69087a7706f54b29f46d9c9b69a
Do not claim the discarded cache's 3%-6% host gain for the retained change.
PS4 speed benefit is still unmeasured. This is a target code improvement,
not proof that JIT now beats the interpreter for DDPSDOJ.

Validation: complete Clang 18 O3 CPU oracle suite passes, including 73,984
bounded comparisons and 164 public on/off budgets, full encodings, branches,
delay slots, devices, state and error fallback. PS4 object compiles with the
pinned AC toolchain; SSE2 validation remains and hot libc comparisons do not
return. Final formal-source object matches the separately audited object.
Host GCC final rebuild equals the already extensively validated baseline.

Evidence: testbuild/ddpsdoj-jit-profit-logs: profile.json, comparison.json,
confirmation.json, summary.json, CPU result, PS4 assembly and final-audit.json.
Formal source is testbuild/fbneo-sh3-jit on ps4-sh3-cold-compile-20261004;
native scratch is restored to this source. A future package needs a clean
PS4 build of this worktree with JIT=1, Z frontend, existing metadata and fixed
out directory followed by pkg_validate/payload audit. No SELF, PKG, GitHub
push, klog connection or PS4 system write in this round. Keep runtime JIT OFF
by default until same-package hardware tests establish per-game preferences.
