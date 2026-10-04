#!/usr/bin/env python3
"""Exhaustively compare production opcode selection with the S decoder.

Handlers are instrumented, not reimplemented: the two actual decoder bodies
select the same stub ID and deliver the same 16-bit operand for every opcode.
Game replays separately verify instruction execution, cycles and device state.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import tempfile
from static_mapping import function, verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--static-only', action='store_true',
                    help='Parse and compare decoder mappings without invoking a compiler')
args = parser.parse_args()

root = Path(__file__).resolve().parents[2]
source = (root / 'src/cpu/sh4/sh4.cpp').read_text()
baseline = subprocess.check_output(
    ['git', 'show', 'efb057ecb65ef9080ce135c37380ba0f911d3141:src/cpu/sh4/sh4.cpp'],
    cwd=root, text=True)


if args.static_only:
    print(json.dumps(verify(baseline, source), sort_keys=True))
    raise SystemExit(0)

helpers = [function(baseline, name) for name in ['execute_one_0000', 'execute_one_4000']]
decoder_helpers = [function(source, name) for name in ['decode_opcode_0000', 'decode_opcode_4000']]
original = function(baseline, 'execute_one')
decoder = function(source, 'decode_opcode')
initializer = function(source, 'init_opcode_dispatch')
handlers = set(re.findall(r'\b([A-Za-z_][A-Za-z_0-9]*)\(opcode\)', original + '\n' + '\n'.join(helpers)))
handlers |= set(re.findall(r'return ([A-Za-z_][A-Za-z_0-9]*);', decoder + '\n' + '\n'.join(decoder_helpers)))
handlers -= {'execute_one_0000', 'execute_one_4000'}
fixture = '#include <stdint.h>\n#include <stdio.h>\ntypedef uint16_t UINT16;\ntypedef uint32_t UINT32;\nstatic int selected; static UINT16 operand;\n'
for number, name in enumerate(sorted(handlers), 1):
    fixture += 'static void %s(UINT16 op) { selected = %d; operand = op; }\n' % (name, number)
fixture += '\n'.join(helpers) + '\n'
fixture += original.replace('void execute_one(', 'void baseline_execute_one(') + '\n'
fixture += 'typedef void (*Sh3OpcodeHandler)(const UINT16);\nstatic Sh3OpcodeHandler opcode_dispatch[0x10000];\n'
fixture += '\n'.join(decoder_helpers) + '\n' + decoder + '\n' + initializer + '\n'
fixture += r"""
int main() {
 init_opcode_dispatch();
 for (UINT32 opcode=0; opcode<0x10000; ++opcode) {
  selected=0; operand=0;
  baseline_execute_one((UINT16)opcode);
  int expected=selected; UINT16 expected_operand=operand;
  selected=0; operand=0;
  if (!opcode_dispatch[opcode]) return 2;
  opcode_dispatch[opcode]((UINT16)opcode);
  if (selected!=expected || operand!=expected_operand) {
   printf("Mismatch opcode %04x expected %d got %d\n", opcode, expected, selected);
   return 1;
  }
 }
 puts("PASS: all 65536 opcode mappings and operands match S, including predecoded groups 0/4");
}
"""
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / 'dispatch.cpp'
    executable = Path(directory) / 'dispatch'
    cpp.write_text(fixture)
    subprocess.run(['c++', '-std=gnu++98', '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
