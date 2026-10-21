"""Probe: parse a .uemodel with the UEFormat addon inside Blender (blender -b --python uemodel_probe.py -- <file>)."""
import sys
from pathlib import Path

from bl_ext.user_default.io_scene_ueformat.importer.archive.reader import FArchiveReader
from bl_ext.user_default.io_scene_ueformat.importer import import_context as ctx

path = Path(sys.argv[sys.argv.index('--') + 1])
with FArchiveReader(path.read_bytes()) as ar:
    header, body = ctx.parse_header(ar)
    dto = ctx.parse_body(header, body)
print('PROBE', header.identifier, header.file_version, header.object_name)
lod = dto.lods[0]
print('PROBE fields', [f for f in dir(lod) if not f.startswith('_')])
print('PROBE LODs', len(dto.lods), 'verts', len(lod.vertices), 'indices', len(lod.indices), 'uv sets', len(lod.uvs),
      'colors', len(getattr(lod, 'colors', []) or []), 'normals', len(getattr(lod, 'normals', []) or []))
print('PROBE first', lod.vertices[0], lod.uvs[0][0] if lod.uvs else None)
