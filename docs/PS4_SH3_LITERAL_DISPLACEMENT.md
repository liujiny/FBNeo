# SH3 native literal displacement addressing

Parent eeba0514d507ddaab9a2b68b387e5e5947caa522. Only MOVLI/MOVWI native generation changes.

## Behavior

The guest literal address is known when emitting the block. Use its physical page index as a disp32 in the live MemMapR load and its within-page offset as a disp32 in the value load, replacing two immediate register materializations and indexed addresses. The live read map and literal value are still loaded on every execution. No host page pointer or literal value is embedded. Handler guard snapshots, EA, guest cycles, data rotation, register allocation and code validation are unchanged.

## Validation

Full Clang18 and GCC ASan/UBSan CPU suites pass, including all opcode encodings and65 PC-relative map/page/alias/device cases. PS4 Clang14 target compile passes. Both no-JIT normal/slice loops and known tables have identical bytes and symbolic relocations to parent.48 sequential game replays preserve complete video/audio and end-state hashes for DDPSDOJ, DDPDFK and Ibara. Four2400-frame lifecycle runs (Ibara,DDPSDOJ,DDPDFK,Mushisama) preserve golden video/audio/endstate through JIT toggles, reset, save/restore, AV masks and worker-count changes. Formal-source target object is compared byte-for-byte before commit.

## Host measurements

Two-order JIT wall mean changes: DDPSDOJ -0.40/-1.24%, DDPDFK -0.39/-0.35%, Ibara +0.41/-0.12%. Reverse main-thread CPU means -0.52/-0.43/-0.04%. DDPS same120 slow frames CPU -0.48%; DDP +0.16% (no slow-tail improvement). Reverse Ibara CPU P95 +0.85%; no-JIT Ibara mean +0.99% despite unchanged hot-loop code. Small/noisy effects, not PS4 FPS proof. Retain a small codegen simplification with repeated target mean benefit, not a claim of fixed frame drops.

Artifacts: project testbuild/sh3-literal-displacement-logs; summary.json, slow-window.json, repeat.json, dual.json, no-jit-audit.json and retained.json.
