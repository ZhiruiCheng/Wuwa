"""Convert an exported StaticMesh .pskx (CUE4Parse ActorX writer) into ArtSource Geometry JSON.

Undoes the writer's MIRROR_MESH (Y negated on points and normals, first two wedge indices of every face swapped),
so the result is in UE local centimeters like the existing Geometry/*.json. Never modifies the source export.
Usage: python pskx_to_geometry.py <package path under Exports, e.g. Client/Content/Aki/Effect/Mesh/Yangyang/Mod_Daoguang_Yangyang_003>
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
OUT = Path(__file__).resolve().parents[1] / 'Geometry'


def read_chunks(data):
    chunks, offset = {}, 0
    order = []
    while offset + 32 <= len(data):
        name = data[offset:offset + 20].split(b'\0', 1)[0].decode('ascii')
        flags, size, count = struct.unpack_from('<iii', data, offset + 20)
        chunks[name] = (offset + 32, size, count)
        order.append({'name': name, 'typeFlags': flags, 'size': size, 'count': count, 'offset': offset})
        offset += 32 + size * count
    return chunks, order


def convert(package):
    pskx = EXPORTS / (package + '.pskx')
    data = pskx.read_bytes()
    chunks, order = read_chunks(data)
    start, size, count = chunks['PNTS0000']
    positions = [list(struct.unpack_from('<fff', data, start + i * size)) for i in range(count)]
    for p in positions:
        p[1] = -p[1]
    start, size, count = chunks['VTXW0000']
    wedge_points, uvs, wedge_materials = [], [], []
    for i in range(count):
        point, u, v, mat = struct.unpack_from('<IffB', data, start + i * size)
        wedge_points.append(point)
        uvs.append([u, v])
        wedge_materials.append(mat)
    face_name = 'FACE3200' if 'FACE3200' in chunks else 'FACE0000'
    start, size, count = chunks[face_name]
    index_format = '<III' if face_name == 'FACE3200' else '<HHH'
    index_bytes = struct.calcsize(index_format)
    triangles, tri_materials, smoothing = [], [], []
    for i in range(count):
        base = start + i * size
        w1, w0, w2 = struct.unpack_from(index_format, data, base)  # stored mirrored: [w1, w0, w2]
        mat, aux = struct.unpack_from('<BB', data, base + index_bytes)
        group, = struct.unpack_from('<I', data, base + index_bytes + 2)
        triangles.append([w0, w1, w2])
        tri_materials.append(mat)
        smoothing.append(group)
    normals = []
    if 'VTXNORMS' in chunks:
        start, size, count = chunks['VTXNORMS']
        normals = [list(struct.unpack_from('<fff', data, start + i * size)) for i in range(count)]
        for n in normals:
            n[1] = -n[1]
    colors = []
    if 'VERTEXCOLOR' in chunks:
        start, size, count = chunks['VERTEXCOLOR']
        colors = [list(struct.unpack_from('<BBBB', data, start + i * size)) for i in range(count)]
    has_colors = bool(colors)
    if not has_colors:
        colors = [[255, 255, 255, 255] for _ in wedge_points]
    extra_uvs = []
    index = 0
    while f'EXTRAUVS{index}' in chunks:
        start, size, count = chunks[f'EXTRAUVS{index}']
        extra_uvs.append([list(struct.unpack_from('<ff', data, start + i * size)) for i in range(count)])
        index += 1
    materials = []
    if 'MATT0000' in chunks:
        start, size, count = chunks['MATT0000']
        materials = [data[start + i * size:start + i * size + 64].split(b'\0', 1)[0].decode('ascii', 'replace') for i in range(count)]
    name = package.rsplit('/', 1)[1]
    metadata = EXPORTS / (package + '.json')
    result = {
        'schemaVersion': 1, 'name': name,
        'sourcePskx': str(pskx).replace('\\', '/'), 'sourcePskxSha256': hashlib.sha256(data).hexdigest(),
        'sourceMetadataJson': str(metadata).replace('\\', '/') if metadata.exists() else None,
        'sourcePackagePath': package,
        'coordinateSystem': 'UE local centimeters; PSK mirror Y and swapped indices already undone; do not flip again',
        'attributeMapping': 'positions indexed by point; UVs/normals/colors indexed by wedge; triangles index wedges',
        'vertexColorEncoding': 'RGBA unsigned bytes, linear UNORM shader values (divide each channel by 255; no sRGB transform)',
        'hasVertexColors': has_colors,
        'positions': positions, 'wedgePointIndices': wedge_points,
        'uvChannels': [uvs] + extra_uvs, 'normals': normals, 'colorsRGBA8': colors,
        'triangles': triangles, 'triangleMaterialIndices': tri_materials, 'triangleSmoothingGroups': smoothing,
        'materials': materials, 'chunks': order,
        'validation': {
            'pointCount': len(positions), 'wedgeCount': len(wedge_points), 'triangleCount': len(triangles),
            'uvChannelCount': 1 + len(extra_uvs), 'normalCount': len(normals), 'sourceColorCount': len(colors) if has_colors else 0,
            'boundsMinCm': [min(p[a] for p in positions) for a in range(3)],
            'boundsMaxCm': [max(p[a] for p in positions) for a in range(3)],
        },
    }
    return result


if __name__ == '__main__':
    for package in sys.argv[1:]:
        geometry = convert(package)
        target = OUT / (geometry['name'] + '.json')
        target.write_text(json.dumps(geometry, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'output': str(target), **{k: geometry['validation'][k] for k in ('pointCount', 'wedgeCount', 'triangleCount', 'uvChannelCount')}}))
