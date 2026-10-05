# SH3 RAM bookkeeping optimization

Based on AD 27476d177. This change only changes no-JIT slice-timer loop; normal-timer and JIT-on target ELF hot-loop bytes AND symbolic relocation records match AD (loop-audit.json, relocation-audit.json). Linked addresses/layout can still differ.

48 game replays match video/audio/final states. Four 2400-frame lifecycle tests (Ibara, DDPSDOJ, DDPDFK, Mushisam), JIT toggle/reset/checkpoint/restore/render options, match prior reference hashes. Full Clang and GCC sanitizer CPU tests and additional 600 random RAM tests pass; pre-existing AD unaligned-RL UB handling documented in DESIGN.md and parent sh3-ram-runs-logs.

Performance first/second no-JIT mean deltas: DDPS -6.87/-3.43%; DDP -6.26/-0.18%; Ibara -9.80/+0.61%. JIT-on DDPS +5.88/-0.79%, DDP -1.14/-0.84%, Ibara +3.73/-0.86%. No stable JIT-on gain claimed. These are host runs of actual target CPU object, not PS4 FPS.

Second run reversed JIT-on order only; no-JIT remained base/ram/ram/base. off-reverse.py explicitly ran ram/base/base/ram for no-JIT. All 12 additional replays completed and matched. All measured replays sequential with no build/test overlap.

Final reverse no-JIT results: {"ddpsdoj-disabled": {"mean": -6.021766395632522, "p95": -7.292486930473041}, "ddpdfk-disabled": {"mean": -10.812595837734984, "p95": -8.525855468068855}, "ibara-disabled": {"mean": -5.472181055262537, "p95": -8.594831002843062}}
60 replay hashes match, four lifecycle games pass. Retained in new local branch; PS4 hardware benefit remains unverified.
