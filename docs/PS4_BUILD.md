# PS4 dynamic core build

Set ORBISDEV=/usr/local/orbisdev and OO_PS4_TOOLCHAIN=/opt/openorbis. Mount
the project's OpenOrbis toolchain at /opt/openorbis in the Orbis build image,
install lld, then run from src/burner/libretro:

```
make platform=orbis-dynamic -j4 orbis-self
```

Do not mix Linux-native objects with PS4 objects. Host tests belong in a
separate checkout. Keep allocator/module patches and persistent render-worker
ordering/finish-on-reset/save/unload safeguards. Temporary PS4 performance
logging is removed; errors and actual emulator functionality remain.

See PS4_V_VALIDATION.md for the last packaged baseline and native validation.
Tests under tests/sh3_opcode_dispatch, sh3_stopped_timer, epic12_blit,
epic12_screen_copy and console_replay provide the retained regression tools.
