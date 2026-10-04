"""Read-only decoder check. No C++ compiler, build or game execution.

Accept only the small switch/call/return grammar used by these decoders.
Unsupported syntax fails closed instead of being silently ignored.
"""
import re


def function(text, name):
    match = re.search(r'static (?:inline )?(?:void|Sh3OpcodeHandler) ' + name + r'\([^;]*?\)\s*\{', text)
    assert match, name
    start = match.start()
    cursor = text.index('{', start) + 1
    depth = 1
    while depth:
        if text[cursor] == '{':
            depth += 1
        elif text[cursor] == '}':
            depth -= 1
        cursor += 1
    return text[start:cursor]


class DecoderParser:
    def __init__(self, body):
        self.text = re.sub(r'//[^\n]*|/\*.*?\*/', '', body, flags=re.S).strip()

    def take(self, pattern):
        match = re.match(pattern, self.text)
        assert match, 'Unsupported decoder syntax: ' + self.text[:100]
        self.text = self.text[match.end():].lstrip()
        return match

    def sequence(self):
        nodes = []
        while self.text and not self.text.startswith(('case ', '}')):
            if self.text.startswith('switch'):
                expr = re.sub(r'\s+', '', self.take(r'switch\s*\((.*?)\)\s*\{').group(1))
                assert expr in ('opcode&0xf000', 'opcode&0xff', 'opcode&0x0f', '(opcode>>8)&0x0f'), expr
                cases = {}
                while self.text.startswith('case '):
                    key = int(self.take(r'case\s+(0x[0-9a-f]+)\s*:').group(1), 16)
                    assert key not in cases, 'Duplicate case'
                    cases[key] = self.sequence()
                    assert cases[key] and cases[key][-1][0] in ('break', 'return'), 'Unexpected fallthrough'
                self.take(r'\}')
                nodes.append(('switch', expr, cases))
            elif self.text.startswith('break'):
                self.take(r'break\s*;')
                nodes.append(('break',))
            else:
                match = self.take(r'(return\s+)?([A-Za-z_][A-Za-z_0-9]*)(\(opcode\))?\s*;')
                assert match.group(1) or match.group(3), 'Expected call or return'
                nodes.append(('return' if match.group(1) else 'call', match.group(2)))
        return nodes


def parse_function(text, name):
    code = function(text, name)
    parser = DecoderParser(code[code.index('{') + 1:-1])
    nodes = parser.sequence()
    assert not parser.text, parser.text[:100]
    return nodes


def select(nodes, opcode, functions):
    for node in nodes:
        if node[0] == 'break':
            return None
        if node[0] in ('return', 'call'):
            name = node[1]
            if name in functions:
                return select(functions[name], opcode, functions)
            return name, opcode
        expr, cases = node[1:]
        if expr == '(opcode>>8)&0x0f':
            key = (opcode >> 8) & 15
        else:
            key = opcode & int(expr.split('&')[1], 16)
        result = select(cases.get(key, []), opcode, functions)
        if result is not None:
            return result
    return None


def verify(baseline, source):
    old = {name: parse_function(baseline, name) for name in
           ('execute_one', 'execute_one_0000', 'execute_one_4000')}
    new = {name: parse_function(source, name) for name in
           ('decode_opcode', 'decode_opcode_0000', 'decode_opcode_4000')}
    # Execution must pass the unmodified opcode to the selected handler;
    # the initialization loop must remain wide enough to terminate.
    compact = lambda text: re.sub(r'\s+', '', text[text.index('{'):])
    assert compact(function(source, 'execute_one')) == '{opcode_dispatch[opcode](opcode);}'
    assert compact(function(source, 'init_opcode_dispatch')) == (
        '{for(UINT32opcode=0;opcode<0x10000;opcode++){'
        'opcode_dispatch[opcode]=decode_opcode((UINT16)opcode);}}')
    handlers = set()
    for opcode in range(0x10000):
        expected = select(old['execute_one'], opcode, old)
        actual = select(new['decode_opcode'], opcode, new)
        assert expected is not None and actual == expected, (hex(opcode), expected, actual)
        handlers.add(actual[0])
    return {'opcodes': 65536, 'expanded_group_0_4_opcodes': 8192,
            'selected_handlers': len(handlers), 'compiled': False}
