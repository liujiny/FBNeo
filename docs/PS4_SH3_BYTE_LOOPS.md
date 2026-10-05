# PS4 SH3 byte-loop batching in the JIT fallback

Parent: a4ab81a3ca5320349f3bb71006a30ce6dd47a092 (exact DIV1 Q/T and interpreter-only RAM bookkeeping retained).

## Trigger and implementation

Slow-frame profiling found `MOV.B @src+,temp; MOV.B temp,@dst; DT count; BF/S; ADD #1,dst` and the analogous fill loop still interpreted with JIT enabled. Their four/five-op bodies are below the normal native-block threshold. Batch only these exact live patterns at a taken BF/S; this reduces repeated interpreter dispatch without altering guest clocks, rendering or skipping frames.

The existing branch/delay pair costs three cycles. Each subsequent complete copy/fill iteration costs six/five, minus one for the final not-taken branch. Preserve sign extension, EA, PC/PPC, delay state, SR and unsigned counter wrap. Limit batches by available cycles, counter and both RAM page boundaries. Do not cache guest data or assume a game-specific address.

Required guards: distinct operand registers, direct ordinary READ/WRITE mappings, live complete FETCH pattern in one page, no pending IRQ, slice timers, and unchanged optional WaitState behavior. DT's READ probe is distinct from FETCH and must not be a handler or the legacy 8bfd busy-loop sentinel. Reject writes overlapping either code/probe or the source host interval before any mutation. Word-swapped memory bytes retain exact parity behavior.

Only the UseJit specialization attempts batching. Runtime JIT default remains disabled. Actual PS4 object audit verifies both no-JIT/normal functions and dispatch tables: identical raw bytes and symbolic relocations to parent. Other architectures/backends unchanged.

## Evidence and limits

Full Clang CPU tests and GCC ASan/UBSan pass, including 7544 new byte-loop/guard cases, all44896 native encodings, original365888 DIV1 oracle, callbacks, pending interrupts, timers, self-modifying code and state comparisons. Tests clear their negative JIT cache before later fresh-cache assertions; production caching is unchanged.

48 sequential game replays (legacy and reversed dual-clock harness) preserve established video/audio/end-state hashes. Host uses Ryzen5600 under WSL; CPU object is compiled with the pinned PS4 toolchain and bridged only for these tests. The mixed library is not a package input.

| JIT enabled | First mean | Reverse mean | Reverse main-thread CPU | P95 caveat |
|---|---:|---:|---:|---|
| DDPSDOJ | -5.45% | -4.18% | -4.30% | -2.38% first, +1.87% reverse; unstable |
| DDPDFK | -3.41% | -2.48% | -2.37% | -4.37% first, -3.78% reverse |
| Ibara | -0.05% | +1.36% | +1.60% | +2.33% first, -1.72% reverse; near-flat/noisy |

The same120 slow-frame window improved CPU time about0.85% for DDPSDOJ and3.41% for DDPDFK. No-JIT measured variation is noise; its execution functions are unchanged. Do not extrapolate host percentages to PS4 FPS or claim DDPSDOJ tail drops resolved.

Four2400-frame lifecycle checks match the established hashes: JIT toggle every60 frames, reset300, checkpoint1200/restore1600, AV-mask1800–1900 and render-core changes700/1400/2100. Formal object SHA256: `cffe98158d069af9b6c174d24ae262ce0e233f75ad41cc0839733c3d55590f98`; its renamed host object is byte-identical to the one used for replay. No package, push, klog or system change in this round. Earlier both-mode variants were not retained because no-JIT had no dependable gain.

## Reproduction records

Project `testbuild/sh3-byte-jitonly-logs`: build.py, test.cpp, cpu-clang/cpu-gcc, audit.py/no-jit-audit.json, compare.py/repeat.json, dual.py/dual.json, slow-window.py, lifecycle.py, and REPORT.md. Earlier candidates: sh3-byte-loops-logs and sh3-byte-outline-logs; loop evidence: sh3-short-loop-profile.
