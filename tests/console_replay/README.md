# Console renderer regression harness

Adapted from the local Salvia PGM replay harness. It reads a user-supplied ROM and writes only to the supplied test directory. Do not point that directory at existing user saves.

Build `headless.cpp` with `g++ -std=c++17 -O2 -ldl`. Arguments:

```
core.so rom.zip output-dir frames depth avmask state-or-- play hash reset stress lifecycle threads switch-threads
```

For a fresh replay: `2000 16 3 - 1 1 0 0 0 2 0` after the first three paths. Compare `hashes.txt` and `end.state` against the unchanged upstream-integration core built in a separate worktree. Set lifecycle to 1 to exercise reset at frame 300, save at 1200, restore at 1600 and audio-only frames 1800–1899. Set switch-threads to 1 to change at frames 700/1400/2100. Run long enough to reach those points. `stress` requires a separate exported test function and is normally 0.

Native FBNeo: `make platform=unix fpic=-fPIC -j4`. The optimized host build uses `CXX="g++ -DFBNEO_RENDER_THREADS_TEST"` to exercise the same POSIX worker and bounded CPU waits as PS4. Host replay timing is not a PS4 FPS result. The built-in input sequence can exercise attract mode if the game ignores early coin/start; inspect the actual game before claiming gameplay coverage.

`render_worker.cpp` exercises the actual pool with all heights 0..513, thread changes, disabled parallel rendering, exit/reinitialization and repeated exit. Build with `g++ -std=c++17 -O1 -g -pthread -fsanitize=address,undefined`.
