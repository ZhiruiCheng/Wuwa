"""Convert exported StaticMesh .uemodel files (UEFormat v10) into ArtSource Geometry JSON.

Same layout as ChangliSlash/Evidence/pskx_to_geometry.py, so WuwaReferenceAssetImport can import either.
Format version >= PreserveOriginalTransforms stores original UE transforms; the Blender importer mirrors
afterwards (create/space.py), so the data read here, before that step, is already UE local centimeters with
UE UVs and UE winding. A .uemodel has no point/wedge split: every render vertex is both a point and a wedge.

Run inside Blender (the UEFormat addon provides the reader):
  blender -b --factory-startup --addons bl_ext.user_default.io_scene_ueformat --python uemodel_to_geometry.py -- <out dir> <package path> [...]
package path is relative to the export root, e.g. Client/Content/Aki/Effect/Mesh/Calbrena/Mod_Fazhen_80007
"""
import hashlib
import json
import sys
from pathlib import Path

from bl_ext.user_default.io_scene_ueformat.importer import import_context as ctx
from bl_ext.user_default.io_scene_ueformat.importer.archive.reader import FArchiveReader
from bl_ext.user_default.io_scene_ueformat.importer.version import EUEFormatVersion

EXPORTS = Path('C:/GamePakExtractor/Output/Exports')


def convert(package: str) -> dict:
    source = EXPORTS / (package + '.uemodel')
    data = source.read_bytes()
    with FArchiveReader(data) as ar:
        header, body = ctx.parse_header(ar)
        dto = ctx.parse_body(header, body)
    if header.file_version < EUEFormatVersion.PreserveOriginalTransforms:
        raise ValueError(f'{source}: format {int(header.file_version)} predates original transforms; coordinates would need mirroring')
    lod = dto.lods[0]
    positions = [[float(c) for c in v] for v in lod.vertices]
    count = len(positions)
    uvs = [[[float(c) for c in uv] for uv in channel] for channel in lod.uvs]
    normals = [[float(c) for c in n] for n in lod.normals] if len(lod.normals) else []
    has_colors = bool(lod.colors)
    if has_colors:
        colors = [[int(round(float(c) * 255)) for c in rgba] for rgba in lod.colors[0].data]
    else:
        colors = [[255, 255, 255, 255] for _ in range(count)]
    triangles = [[int(i) for i in tri] for tri in lod.indices]
    tri_materials = [0] * len(triangles)
    for index, material in enumerate(lod.materials):
        for face in range(material.first_index // 3, material.first_index // 3 + material.num_faces):
            if face < len(tri_materials):
                tri_materials[face] = index
    metadata = EXPORTS / (package + '.json')
    return {
        'schemaVersion': 1, 'name': package.rsplit('/', 1)[1],
        'sourceUemodel': str(source).replace('\\', '/'), 'sourceUemodelSha256': hashlib.sha256(data).hexdigest(),
        'sourceUemodelFormatVersion': int(header.file_version),
        'sourceMetadataJson': str(metadata).replace('\\', '/') if metadata.exists() else None,
        'sourcePackagePath': package,
        'coordinateSystem': 'UE local centimeters (UEFormat original transforms, before the Blender-space mirror); do not flip',
        'attributeMapping': 'positions indexed by point; UVs/normals/colors indexed by wedge; triangles index wedges (point == wedge for .uemodel)',
        'vertexColorEncoding': 'RGBA unsigned bytes, linear UNORM shader values (divide each channel by 255; no sRGB transform)',
        'hasVertexColors': has_colors,
        'positions': positions, 'wedgePointIndices': list(range(count)),
        'uvChannels': uvs, 'normals': normals, 'colorsRGBA8': colors,
        'triangles': triangles, 'triangleMaterialIndices': tri_materials,
        'triangleSmoothingGroups': [1] * len(triangles),
        'materials': [m.material_name for m in lod.materials],
        'validation': {
            'pointCount': count, 'wedgeCount': count, 'triangleCount': len(triangles),
            'uvChannelCount': len(uvs), 'normalCount': len(normals), 'sourceColorCount': count if has_colors else 0,
            'boundsMinCm': [min(p[a] for p in positions) for a in range(3)],
            'boundsMaxCm': [max(p[a] for p in positions) for a in range(3)],
        },
    }


def main() -> int:
    args = sys.argv[sys.argv.index('--') + 1:]
    out = Path(args[0])
    out.mkdir(parents=True, exist_ok=True)
    for package in args[1:]:
        geometry = convert(package)
        target = out / (geometry['name'] + '.json')
        target.write_text(json.dumps(geometry, ensure_ascii=False, indent=1), encoding='utf-8')
        v = geometry['validation']
        print(f"GEOMETRY {geometry['name']} points={v['pointCount']} tris={v['triangleCount']} uv={v['uvChannelCount']} colors={geometry['hasVertexColors']}")
    return 0


main()
