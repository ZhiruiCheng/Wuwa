"""Check uemodel_to_geometry against the proven pskx path on a mesh exported in both formats.

Compares every triangle as its three corner (position, uv, color) tuples, ignoring vertex order/duplication,
and checks winding through the face normal. Usage: python -I compare_geometry.py <pskx geometry.json> <uemodel geometry.json>
"""
import json
import sys


def corners(g):
    out = []
    for tri in g['triangles']:
        corner = []
        for w in tri:
            p = g['positions'][g['wedgePointIndices'][w]]
            uv = g['uvChannels'][0][w]
            c = g['colorsRGBA8'][w]
            corner.append((tuple(round(x, 2) for x in p), tuple(round(x, 3) for x in uv), tuple(c)))
        out.append(corner)
    return out


def normal(tri):
    (a, _, _), (b, _, _), (c, _, _) = tri
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    return (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])


def key(tri):
    # rotation-invariant key that keeps winding
    i = min(range(3), key=lambda k: tri[k])
    return tuple(tri[i:] + tri[:i])


a = json.load(open(sys.argv[1], encoding='utf-8'))
b = json.load(open(sys.argv[2], encoding='utf-8'))
ka = {key(t) for t in corners(a)}
kb = {key(t) for t in corners(b)}
reversed_b = {key([t[0], t[2], t[1]]) for t in corners(b)}
print('triangles', len(ka), len(kb), 'same winding matches', len(ka & kb), 'reversed matches', len(ka & reversed_b))
print('bounds', a['validation']['boundsMinCm'], a['validation']['boundsMaxCm'], '|', b['validation']['boundsMinCm'], b['validation']['boundsMaxCm'])
