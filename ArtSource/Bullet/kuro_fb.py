"""Decode Kuro ConfigDB flatbuffer rows using the compiled JS accessor definitions.

usage: kuro_fb.py <Define/Config/X.js> <db> <table> <id> [<id>...]
"""
import re
import sqlite3
import struct
import sys

sys.stdout.reconfigure(encoding='utf-8')
SUBTYPE_DIR = None


def parse_js(path):
    src = open(path, encoding='utf-8').read()
    fields = []
    for m in re.finditer(r'(\w+)\((?:\w+(?:,\w+)?)?\)\{var (\w)=this\.J7\.__offset\(this\.z7,(\d+)\)(.*?)\}(?=[a-zA-Z_]\w*\(|static|$|\})', src):
        name, off, body = m.group(1), int(m.group(3)), m.group(4)
        if name.endswith('Length') or name.startswith('Get'):
            continue
        if '__vector(' in body:
            if '__indirect' in body:
                sub = re.search(r'new (\w+)_1\.(\w+)', body)
                kind = ('vec_table', sub.group(2) if sub else '?')
            elif '__string' in body:
                kind = ('vec_str',)
            else:
                rd = re.search(r'read(\w+)\(', body)
                kind = ('vec', rd.group(1) if rd else 'Int32')
        elif '__string' in body:
            kind = ('str',)
        elif '__indirect' in body:
            sub = re.search(r'new (\w+)_1\.(\w+)', body)
            kind = ('table', sub.group(2) if sub else '?')
        else:
            rd = re.search(r'read(\w+)\(', body)
            kind = ('scalar', rd.group(1) if rd else 'Int32')
        fields.append((name, off, kind))
    seen = set()
    out = []
    for f in fields:
        if f[0] not in seen:
            seen.add(f[0])
            out.append(f)
    return out


FMT = {'Int8': 'b', 'Uint8': 'B', 'Bool': '?', 'Int16': 'h', 'Uint16': 'H', 'Int32': 'i', 'Uint32': 'I',
       'Float32': 'f', 'Float64': 'd', 'Int64': 'q', 'Uint64': 'Q'}


class FB:
    def __init__(self, b):
        self.b = b

    def u32(self, p): return struct.unpack_from('<I', self.b, p)[0]
    def i32(self, p): return struct.unpack_from('<i', self.b, p)[0]

    def offset(self, tab, voff):
        vt = tab - self.i32(tab)
        if voff < struct.unpack_from('<H', self.b, vt)[0]:
            return struct.unpack_from('<H', self.b, vt + voff)[0]
        return 0

    def read(self, kind, p):
        f = FMT[kind]
        return struct.unpack_from('<' + f, self.b, p)[0]

    def string(self, p):
        p += self.u32(p)
        n = self.u32(p)
        return self.b[p + 4:p + 4 + n].decode('utf-8', 'replace')

    def vector(self, p):
        p += self.u32(p)
        return p + 4, self.u32(p)


def decode_table(fb, tab, fields, subdefs):
    row = {}
    for name, off, kind in fields:
        o = fb.offset(tab, off)
        if not o:
            continue
        p = tab + o
        if kind[0] == 'scalar':
            row[name] = fb.read(kind[1], p)
        elif kind[0] == 'str':
            row[name] = fb.string(p)
        elif kind[0] == 'vec':
            start, n = fb.vector(p)
            size = struct.calcsize(FMT[kind[1]])
            row[name] = [fb.read(kind[1], start + i * size) for i in range(n)]
        elif kind[0] == 'vec_str':
            start, n = fb.vector(p)
            row[name] = [fb.string(start + 4 * i) for i in range(n)]
        elif kind[0] in ('vec_table', 'table'):
            sub = subdefs(kind[1])
            if kind[0] == 'table':
                row[name] = decode_table(fb, p + fb.u32(p), sub, subdefs) if sub else '<table>'
            else:
                start, n = fb.vector(p)
                row[name] = [decode_table(fb, start + 4 * i + fb.u32(start + 4 * i), sub, subdefs) if sub else '<table>' for i in range(n)]
    return row


def main():
    js, db, table, *ids = sys.argv[1:]
    import os
    subdir = os.path.join(os.path.dirname(js), 'SubType')
    cache = {}

    def subdefs(name):
        if name not in cache:
            p = os.path.join(subdir, name + '.js')
            cache[name] = parse_js(p) if os.path.exists(p) else None
        return cache[name]

    fields = parse_js(js)
    c = sqlite3.connect(db)
    key = [r[1] for r in c.execute(f'pragma table_info({table})')][0]
    for i in ids:
        r = c.execute(f'select BinData from {table} where {key}=?', (float(i) if key.lower() == 'id' and i.replace('.', '', 1).isdigit() else i,)).fetchone()
        if not r:
            print(i, 'missing')
            continue
        b = r[0]
        fb = FB(b)
        root = fb.u32(0)
        print(i, decode_table(fb, root, fields, subdefs))


if __name__ == '__main__':
    main()
