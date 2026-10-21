"""Read a cooked UE shader library (ShaderArchive-*.ushaderbytecode, FSerializedShaderArchive).

  python -I shader_archive.py <archive> [--find-in <file>...]   list stats / find which shader maps a package references
  python -I shader_archive.py <archive> --dump <shadermap hash hex> <out dir>   write that shader map's shaders

Layout (UE 4.26+): uint32 version, then TArray<FSHAHash> ShaderMapHashes, TArray<FSHAHash> ShaderHashes,
TArray<FShaderMapEntry{u32 ShaderIndicesOffset, u32 NumShaders, u32 FirstPreloadIndex, u32 NumPreloadEntries}>,
TArray<FShaderCodeEntry{u64 Offset, u32 Size, u32 UncompressedSize, u8 Frequency}>,
TArray<FFileCachePreloadEntry{i64 Offset, i64 Size}>, TArray<u32> ShaderIndices, then the code blob.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path


class Archive:
    def __init__(self, path: Path):
        self.path = path
        with open(path, 'rb') as f:
            head = f.read(64 * 1024 * 1024)
        self.version, = struct.unpack_from('<I', head, 0)
        pos = 4

        def array(pos, item):
            n, = struct.unpack_from('<i', head, pos)
            return n, pos + 4

        n, pos = array(pos, 20)
        self.shader_map_hashes = [head[pos + 20 * i: pos + 20 * (i + 1)] for i in range(n)]
        pos += 20 * n
        n, pos = array(pos, 20)
        self.shader_hashes = [head[pos + 20 * i: pos + 20 * (i + 1)] for i in range(n)]
        pos += 20 * n
        n, pos = array(pos, 16)
        self.map_entries = [struct.unpack_from('<4I', head, pos + 16 * i) for i in range(n)]
        pos += 16 * n
        n, pos = array(pos, 17)
        self.code_entries = []
        for i in range(n):
            offset, size, usize, freq = struct.unpack_from('<QIIB', head, pos)
            self.code_entries.append((offset, size, usize, freq))
            pos += 17
        n, pos = array(pos, 16)
        pos += 16 * n
        n, pos = array(pos, 4)
        self.indices = list(struct.unpack_from('<%dI' % n, head, pos))
        pos += 4 * n
        self.code_start = pos
        self.map_index = {h: i for i, h in enumerate(self.shader_map_hashes)}

    def shaders_of(self, map_index: int):
        offset, count, _, _ = self.map_entries[map_index]
        return [self.indices[offset + k] for k in range(count)]

    def read_code(self, shader_index: int) -> tuple[bytes, int, int, int]:
        offset, size, usize, freq = self.code_entries[shader_index]
        with open(self.path, 'rb') as f:
            f.seek(self.code_start + offset)
            data = f.read(size)
        return data, size, usize, freq


def lz4_block(src: bytes, size: int) -> bytes:
    """Decompress one raw LZ4 block (no frame header)."""
    out = bytearray()
    i, n = 0, len(src)
    while i < n:
        token = src[i]; i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1; lit += b
                if b != 255: break
        out += src[i:i + lit]; i += lit
        if i >= n: break
        off = src[i] | (src[i + 1] << 8); i += 2
        if off == 0 or off > len(out):
            raise ValueError(f'bad offset {off} at {i} (out {len(out)})')
        ml = token & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255: break
        ml += 4
        start = len(out) - off
        for k in range(ml):
            out.append(out[start + k])
    if len(out) != size:
        raise ValueError(f'size {len(out)} != {size}')
    return bytes(out)


def disassemble(dxbc: bytes) -> str:
    """D3DDisassemble from the system d3dcompiler_47.dll."""
    import ctypes
    from ctypes import wintypes
    lib = ctypes.WinDLL('d3dcompiler_47.dll')
    blob = ctypes.c_void_p()
    hr = lib.D3DDisassemble(ctypes.c_char_p(dxbc), ctypes.c_size_t(len(dxbc)), ctypes.c_uint(0), None, ctypes.byref(blob))
    if hr != 0:
        raise OSError(f'D3DDisassemble failed 0x{hr & 0xffffffff:08x}')
    vtbl = ctypes.cast(ctypes.cast(blob, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    get_ptr = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vtbl[3])
    get_size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vtbl[4])
    text = ctypes.string_at(get_ptr(blob), get_size(blob)).decode('latin-1')
    ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vtbl[2])(blob)
    return text


def dump_map(archive: 'Archive', map_hash: str, out_dir: Path) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    index = archive.map_index[bytes.fromhex(map_hash)]
    written = []
    for k, shader in enumerate(archive.shaders_of(index)):
        data, size, usize, freq = archive.read_code(shader)
        code = lz4_block(data, usize) if size != usize else data
        at = code.find(b'DXBC')
        if at < 0:
            continue
        total, = struct.unpack_from('<I', code, at + 24)
        dxbc = code[at:at + total]
        path = out_dir / f'{k}_freq{freq}.asm'
        path.write_text(disassemble(dxbc), encoding='utf-8')
        (out_dir / f'{k}_freq{freq}.ueheader.bin').write_bytes(code[:at])
        written.append(path)
    return written


def main(argv):
    archive = Archive(Path(argv[0]))
    print(f'version {archive.version}: {len(archive.shader_map_hashes)} shader maps, {len(archive.shader_hashes)} shaders, '
          f'code at {archive.code_start}')
    compressed = sum(1 for e in archive.code_entries if e[1] != e[2])
    print(f'compressed entries: {compressed}/{len(archive.code_entries)}; frequencies: '
          f'{sorted({e[3] for e in archive.code_entries})}')
    if '--dump' in argv:
        k = argv.index('--dump')
        for p in dump_map(archive, argv[k + 1], Path(argv[k + 2])):
            print('wrote', p, p.stat().st_size)
    if '--find-in' in argv:
        for name in argv[argv.index('--find-in') + 1:]:
            data = Path(name).read_bytes()
            hits = [i for i, h in enumerate(archive.shader_map_hashes) if h in data]
            print(name, [(archive.shader_map_hashes[i].hex(), len(archive.shaders_of(i))) for i in hits])


if __name__ == '__main__':
    main(sys.argv[1:])
