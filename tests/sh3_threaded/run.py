#!/usr/bin/env python3
"""Compare both real SH3 dispatch loops; writes build artifacts outside source."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='sh3-threaded-') as tmp:
    out = args.output.resolve() if args.output else Path(tmp)
    out.mkdir(parents=True, exist_ok=True)
    binary = out / 'test'
    includes = ['burn', 'cpu', 'burn/snd', 'burn/devices', 'burner', 'cpu/sh4',
                'burner/libretro', 'burner/libretro/libretro-common/include']
    command = shlex.split(os.environ.get('CXX', 'g++')) + [
        '-std=gnu++98', '-O2' if args.sanitize else '-O3', '-g', '-D__LIBRETRO__', '-DLSB_FIRST',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-Wno-write-strings']
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    command += ['-I' + str(root / 'src' / p) for p in includes]
    command += [str(root / 'tests/sh3_threaded/test.cpp'), '-o', str(binary)]
    with (out / 'compile.log').open('w') as log:
        compiled = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    if compiled.returncode:
        print((out / 'compile.log').read_text())
        raise SystemExit(compiled.returncode)
    with (out / 'result.log').open('w') as log:
        result = subprocess.run([str(binary)], stdout=log, stderr=subprocess.STDOUT,
                                timeout=180, env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1'))
    print((out / 'result.log').read_text())
    raise SystemExit(result.returncode)
