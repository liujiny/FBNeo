# SH3 opcode selection regression

Run `python3 tests/sh3_opcode_dispatch/test.py` from the FBNeo repository.
Requires Git history containing S (`efb057ecb65ef9080ce135c37380ba0f911d3141`) and a C++ compiler supporting ASan/UBSan.

The test extracts the actual old switch decoder and current table-building decoder. Instruction handlers are replaced with named recording stubs; unchanged group 0/4 decoder bodies are shared. All 65,536 opcodes must select identical handlers and pass identical operands. Initialization is also exercised under a 30-second timeout, catching wrapping loop counters.

This checks decoding, not complete execution semantics. Console replay tests additionally compare per-frame video/audio hashes and final serialized state for DDPSDOJ/DDPDFK, including reset/save/load/audio-only and thread-count changes. Native elapsed time is not PS4 FPS.
