# Post-T2 performance candidates — stop before compilation

Date: 2026-10-04. Branch: `ps4-sh3-full-dispatch-20261004`.
Build baseline: T2, `4758ee7df64c5ea4d5f9a3ca5c729e253778f809`.
The user requested another optimization round and asked about dense bullet
scenes, while retaining the explicit stop before compilation.

## Hardware evidence

T2 / RAPS10017 boots and executes both games in the captured PS4 klog. The user
still reports frame drops. DDPSDOJ has 21 windows / 6,300 frames and DDPDFK has
20 windows / 6,000 frames in this recording. Startup windows are included and
must not be presented as a controlled gameplay benchmark.

DDPSDOJ window 8 averages 18.065 ms in the CPU/device phase; window 16 averages
17.240 ms. Its heaviest reported blitter batch averages 17.027 ms per job.
DDPDFK window 13 averages 16.524 ms in the CPU/device phase before the other
frame work; the maximum final synchronization window is 4.006 ms.

`cpu_us` includes device work and in-run waits. Blitter averages are per 300
jobs, not frames, and include scheduling delays. Both threads were observed
across CPU IDs 0–5; that is neither utilization nor proof of six concurrent
workers. The existing ordered worker must retain VRAM command dependencies.

Local evidence: project `testbuild/post-t2-source-logs/hardware_results.json`
and its klog snapshot. No ROMs or raw logs belong in this repository.

## Source changes

1. SH3 group 0/4 selection now occurs during table initialization. All 8,192
   opcodes in those groups call their final existing handler directly. This
   reduces interpreter dispatch work, including instructions that a game's
   bullet movement/collision code may use; it does not change that game code.
   The table remains 512 KiB and no decoded RAM cache is introduced.
2. EPIC12 fixed source/destination alpha blending has an identity-source
   specialization. When source alpha is 31 and each tint component is 31 or
   32, both original integer rounding stages return the input 5-bit component.
   Taking `pen & 0x00f8f8f8` replaces three component table reads and repacking.
   Selection occurs once per sprite. All other source tint/alpha combinations
   retain the existing LUT path.

The second change is useful only for draws meeting that condition. The klog
does not contain their frequency; no claim is made that every bullet uses it.
Ordinary untinted, unblended sprites already use `REALLY_SIMPLE` direct copy.
Changing those copies to the same operation would not be a new optimization.

The blend loop retains source/destination alias ordering, transparent-pixel
handling, clipping, flips, source-wrap rejection and emulated blitter delay.
No command reordering, eight-way worker split, frameskip, bullet reduction,
CPU downclock, or audio change is included. The specialization doubles the
fixed-blend template variants from eight to sixteen; compiled code size and
instruction-cache effects remain to be measured.

## Checks completed without compilation

- Parsed all 65,536 actual SH3 decoder mappings/operands against S. Rejected
  deliberate wrong-handler, operand, missing-case and wrapping-counter edits.
- Verified source outside the SH3 decoder region is identical to T2.
- Exhausted 65,536 tint/alpha/component rounding combinations: the only
  identity pairs are `(31,31)` and `(32,31)`.
- Checked 262,144 packed RGB cases across the eight identity tint triplets,
  including unrelated pixel bits. This is algebra, not C++ execution.
- Added 1,024 explicit image differential cases (6,024 total) for identity and
  adjacent tint/alpha boundaries, flips, transparency and destination alpha.
  The C++ image test has NOT been compiled or run for these changes.

Read-only checks can be repeated with:

```sh
rtk proxy python3 -B tests/sh3_opcode_dispatch/test.py --static-only
rtk proxy python3 -B tests/epic12_blit/static_identity.py
rtk proxy git diff --check
```

## Resume only after the user authorizes compilation

Keep the two changes separable for baseline/SH3-only/combined comparisons.
First run the compiled ASan/UBSan opcode fixture and EPIC12 differential test;
then identical DDPSDOJ/DDPDFK gameplay and lifecycle replays versus T2/S. Check
image, audio and state hashes, including overlapping VRAM and thread changes.
Measure dense bullet/boss scenes separately from startup/menu frames. Reject
regressions even when average host timings improve.

Only then use the PS4 toolchain and package with the immutable S frontend
SELF, independent Title ID and required `pkg_validate` checks. Audit object
targets to avoid the previous Linux/FreeBSD object mix. The current `.elf`,
`.oelf` and `.self` in the production tree still belong to T2, not this source.
No new binary or package has been generated. Packaging rules and the fixed
output directory remain in the project handoff and console optimization skill.
