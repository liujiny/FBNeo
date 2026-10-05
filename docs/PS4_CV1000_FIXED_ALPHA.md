# CV1000 exact fixed-alpha reciprocal multiplication

Parent d6b299ad00ec75081f624f90ce9da0ff0b58d1b5. Keep128-page alpha cache and existing row scheduling/worker count.

## Change

Fixed five-bit alpha contributions previously used `mullo(value, alpha)` followed by `mulhi(product, 2115)`. Precompute `alpha * 2115` once per draw and use one unsigned-high multiply for partial alpha1..30. The coefficient fits unsigned16 (maximum63450). Alpha0/31 retain the original branches. Device alpha bytes are shifted right3 in epic12.cpp, so the input domain is0..31.

Reassociation is exact: both expressions compute floor(value * alpha *2115 /65536); neither former intermediate product nor new coefficient overflows16bits within its domain. All1024 channel/alpha combinations match integer division by31. Tint's preceding divide/clamp remains separate; no reassociation across rounding stages. Colour-dependent contributions retain their old multiplication path.

## Evidence

Existing full renderer ASan/UBSan and scalar differential oracles pass: pixels, guest blit-delay accounting, upload decoding, RAW/WAR barriers, cache invalidation and worker lifecycle. PS4 renderer object compiles. Native CPU object is byte-identical to the parent benchmark baseline.

48 game replays match established video/audio/end-state hashes, with JIT on/off. Tests use native GCC renderer/core on Ryzen5600/WSL, not a PS4 pthread object linked into Linux. These are host performance results, not PS4 frame-rate measurements.

| JIT enabled | First mean | Reverse mean | First P95 | Reverse P95 |
|---|---:|---:|---:|---:|
| DDPSDOJ | -8.23% | -0.34% | -10.78% | -3.03% |
| DDPDFK | -4.22% | -2.92% | -6.36% | -4.47% |
| Ibara | -9.29% | -1.51% | -4.57% | -0.31% |

The magnitude varies substantially with run order; preserve both matrices, not only the favorable first run. No-JIT means also slightly improve in these comparisons; DDPS first no-JIT P95 regresses2.94%, then improves5.00% in reverse.

Mode diagnostics indicate applicability to59%/39%/32% of clipped rectangle area for DDPS/DDP/Ibara. These counts include internal transparent holes and are not instruction counts or promised speedups. Larger512-page cache was separately rejected despite fewer misses.

Four 2400-frame lifecycle runs (Ibara, DDPSDOJ, DDPDFK, Mushisama) match established video/audio/end-state hashes. Formal-source PS4 target object is checked byte-for-byte against the tested candidate before committing. Reproduction data: project testbuild/cv1k-blend-recip-logs, source-mode data in cv1k-alpha512-profile. No new production logging, PS4 system change, PKG or push.
