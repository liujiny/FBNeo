#!/usr/bin/env python3
"""Execute native SH3 blocks against the interpreter; keep artifacts outside source."""
import argparse
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
# Fail closed if an upstream change gives a deferred handler side effects.
header = (root / 'src/cpu/sh4/sh3_threaded.h').read_text()
source = (root / 'src/cpu/sh4/sh4.cpp').read_text()
section = header.split('#define SH3_ALU_OPS(OP)', 1)[1].split('#define SH3_OTHER_OPS', 1)[0]
for name in re.findall(r'OP\((\w+)\)', section):
    match = re.search(r'static (?:inline )?void ' + name + r'\(const UINT16 opcode\)\s*\{', source)
    assert match, name
    start = end = match.end()
    level = 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    body = source[start:end-1]
    assert set(re.findall(r'\bm_\w+', body)) <= {'m_r', 'm_sr'}, name
    assert not re.search(r'\b(EAT|RB|RW|RL|WB|WW|WL|sh3_total_cycles|Sh3BurnCycles)\b', body), name
with tempfile.TemporaryDirectory(prefix='sh3-x64-') as tmp:
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
    command += [str(root / 'tests/sh3_x64/test.cpp'), '-o', str(binary)]
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
