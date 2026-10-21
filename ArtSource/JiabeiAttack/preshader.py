"""Read the uniform-expression preshaders of a cooked material shader map (UE 4.26 layout as used by Kuro).

  python -I preshader.py <MI .uexp> [--map N]

A cooked FUniformExpressionSet stores, in order:
  vector preshader headers (uint32 OpcodeOffset, uint32 OpcodeSize) — one per float4 slot of the material uniform buffer,
  scalar preshader headers (same) — packed four per float4 slot after the vectors,
  scalar parameter infos (28 bytes: stripped name, Index -1, Association, pad, float default),
  vector parameter infos (40 bytes: same header + float4 default),
  texture parameter infos, then the preshader opcode bytes.
Opcodes seen: 02 Constant (float4 follows for vector programs), 03 ScalarParameter u16, 04 VectorParameter u16,
0x23 ComponentSwizzle (u8 count, 4 x u8 index). Parameter names are stripped in cooked data; only defaults remain.
"""
from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

ENTRY = b'\xff\xff\xff\xff\x02\x00\x00\x00\x00\x00\x00\x00'


def parameter_runs(data: bytes) -> list[list[int]]:
    """Offsets of parameter-info entries (the ENTRY marker), grouped per shader map."""
    offs = [m.start() for m in re.finditer(re.escape(ENTRY), data)]
    runs, run = [], [offs[0]]
    for o in offs[1:]:
        if o - run[-1] <= 60:
            run.append(o)
        else:
            runs.append(run)
            run = [o]
    runs.append(run)
    return [r for r in runs if len(r) > 3]


def parse_parameters(data: bytes, run: list[int]):
    """Scalar entries are 28 bytes, vector entries 40, texture entries 32 (only the last group).
    A 56-byte gap holds two scalars where the second entry's marker differs (non-global association)."""
    scalars, vectors = [], []
    for i, o in enumerate(run):
        gap = run[i + 1] - o if i + 1 < len(run) else 32
        if gap == 28:
            scalars.append(struct.unpack_from('<f', data, o + 12)[0])
        elif gap == 56:
            scalars.append(struct.unpack_from('<f', data, o + 12)[0])
            scalars.append(struct.unpack_from('<f', data, o + 40)[0])
        elif gap == 40 or (gap == 32 and i > 0 and run[i] - run[i - 1] == 40):
            vectors.append(struct.unpack_from('<4f', data, o + 12))
    return scalars, vectors


def find_headers(data: bytes, first_entry: int):
    """Walk back from the first parameter entry over (offset, size) pairs. Scalar headers end just before the
    parameter table; vector headers precede them. Each array's offsets are increasing and contiguous."""
    pos = first_entry - 12  # entry = 8 bytes name + marker; first entry name starts 8 bytes earlier, plus a 4-byte field
    pairs = []
    while pos >= 8:
        off, size = struct.unpack_from('<II', data, pos - 8)
        if size > 4096 or off > 1 << 20:
            break
        pairs.append((off, size))
        pos -= 8
    pairs.reverse()
    return pairs, pos


def decode(code: bytes, scalars, vectors) -> str:
    out, i = [], 0
    while i < len(code):
        op = code[i]
        if op == 0x02:
            out.append('const(%s)' % ','.join('%.4g' % x for x in struct.unpack_from('<4f', code, i + 1)))
            i += 17
        elif op == 0x03:
            k, = struct.unpack_from('<H', code, i + 1)
            out.append(f's{k}={scalars[k]:.4g}' if k < len(scalars) else f's{k}')
            i += 3
        elif op == 0x04:
            k, = struct.unpack_from('<H', code, i + 1)
            out.append(f'v{k}={tuple(round(x, 4) for x in vectors[k])}' if k < len(vectors) else f'v{k}')
            i += 3
        elif op == 0x23:
            n = code[i + 1]
            out.append('.' + ''.join('xyzw'[c] for c in code[i + 2:i + 2 + n]))
            i += 6
        else:
            out.append(f'op{op:02x}')
            i += 1
    return ' '.join(out)


def main(argv):
    data = Path(argv[0]).read_bytes()
    runs = parameter_runs(data)
    which = int(argv[argv.index('--map') + 1]) if '--map' in argv else 0
    run = runs[which]
    print(f'{len(runs)} parameter tables; using #{which} at {run[0]} ({len(run)} entries)')
    scalars, vectors = parse_parameters(data, run)
    print(f'scalars {len(scalars)}, vectors {len(vectors)}')
    return data, run, scalars, vectors


if __name__ == '__main__':
    main(sys.argv[1:])
