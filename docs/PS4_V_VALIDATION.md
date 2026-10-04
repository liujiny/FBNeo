# V cumulative optimization build (2026-10-04)

User explicitly authorized compilation and PKG output after source-only rounds.
Runtime source: `e2389eb79ee2c5f1e0385217ad5f69ae63e8bcfb` on `ps4-sh3-timer-batching-20261004`.
Combines timer batching/reciprocal division, bounded peaks, SIMD fixed-alpha
blends and wrapped output conversion/copy. Preserves the S frontend
`f8e0ea4f19e1a2bfa6aba37f1ed9c9c5b3a01226` and existing PCSX/MAME cores.

Native validation passed: 1,200,000 reciprocal quotient/remainder cases,
320,024 timer steps/callback traces, 65,536 blend rounding cases, 1,000,000
packed additions, 7,048 blend images and 2,184 output rows. All three unit
suites used ASan/UBSan, and both SIMD blend variants were exercised.

DDPSDOJ/DDPDFK U/V/V/U 3,000-frame video/audio/state hashes matched; 2/3-thread
and 2,400-frame lifecycle cases matched, as did Mushisam lifecycle. Both games
also passed 3,000 frames in explicitly enabled XRGB8888 output with 3 threads.
Combined native core-time reduction: DDPSDOJ 4.37%, DDPDFK 9.32%. These do not
attribute gains to individual changes or predict PS4 FPS. Timed ABBA ran before
PS4 compilation and the separate 32-bit replay suite.

PS4 compilation passed; 1,140 objects are ELF64 FreeBSD/x86-64. The EPIC12
object contains paddusb and packssdw instructions. Final SELF SHA256:
`71742567f9c17b0f42a1c6b12c6dc66e01a1cf688129c4b5a2909cd48761584d` (99668592 bytes).

Package: `RetroArch_PS4_V_Optimized_RAPS10019.pkg`
Title: `RAPS10019`
Content: `UP0001-RAPS10019_00-0000000000000001`
Size: 179109888 bytes (170.8125 MiB)
SHA256: `a8322d26a20b0a1f46b05bc9931848e347fcc6e8ccbc0c9ed6a12be1223bbb03`

Only output directory: project `dist/retroarch-ps4-upstream/out`.
PkgTool.Core pkg_validate passed 32 checks. Extracted 18 payloads match stage;
all preserved files match the immutable S report, including the real SELF
magic/hash of eboot.bin. R4 gde/obs/attribute SFO semantics verified.
Full report/scripts/evidence: local `testbuild/v-final-logs`; portable replay
summary: `tests/console_replay/verified-v-final-20261004.json`.

Hardware performance/audio stability and whole-console power-off cause remain
unconfirmed. The user reports large PS4 games do not power off. Current klog
receiver audit shows only socket receive, no control payload. S heap changes
are process-local parameters; no new frontend or persistent system changes
were made for V. The pre-existing authorized klog receiver was found active
and stopped locally before delivery. Do not automatically reconnect. First
hardware comparison can run without klog to isolate that variable.
