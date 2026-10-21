"""原作 UI 预制体 → 展平的布局树（递归展开嵌套预制体、套上属性覆盖），可导出 JSON 和预览图。

  python -I kuro_layout.py UI/UIResources/UiFight/Prefabs/UiView_Fight_Prefab [--png out.png] [--json out.json]

布局按原作旧版 LGUI 的 UIWidget 计算（2560×1440 设计分辨率）：
  anchorHAlign Left/Center/Right：轴心的 X = 父矩形左 / 中 / 右 + anchorOffsetX；Stretch：左右边距 stretchLeft/Right。
  anchorVAlign Top/Middle/Bottom：轴心的 Y（向上为正）= 父矩形上 / 中 / 下 + anchorOffsetY；Stretch：stretchTop/Bottom。
  没存的字段是默认值：Center / Middle、pivot (0.5,0.5)、100×100、白色。
精灵：LGUITexturePackerSpriteData → 导入器 TPI_*（SpriteInfoMap 里的尺寸、九宫格、UV）→ UIResources 里同名图集 PNG。
"""
from __future__ import annotations

import copy
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I 不会把脚本目录加进 sys.path
import kuro_prefab as kp  # noqa: E402

UIRES = Path('C:/GamePakExtractor/WutheringWaves-UIResources-3.6')
DESIGN = (2560.0, 1440.0)


# ---------------------------------------------------------------- 解码一个预制体
def resolve_refs(value, p: dict):
    """把对象引用换成可读的名字（资产路径 / 类名），资产下标是本预制体自己的 ReferenceAssets。"""
    if isinstance(value, dict):
        if set(value) == {'asset'}:
            a = p['ReferenceAssets'][value['asset']]
            if not a:
                return {'asset': None, 'name': None, 'type': None}
            return {'asset': a['ObjectPath'].split('.')[0], 'name': a['ObjectName'].split("'")[1], 'type': a['ObjectName'].split("'")[0]}
        if set(value) == {'class'}:
            c = p['ReferenceClasses'][value['class']]
            return {'class': c['ObjectName'].split("'")[1] if c else 'Unknown'}
        return kp.Props((k, resolve_refs(v, p)) for k, v in value.items())
    if isinstance(value, list):
        return [resolve_refs(v, p) for v in value]
    return value


def decode(data, p):
    return resolve_refs(kp.decode(data, p['ReferenceFNames']), p)


def merge(dst: dict, src: dict):
    for k, v in src.items():
        if isinstance(v, dict) and isinstance(dst.get(k), dict) and not ({'asset', 'file', 'class'} & set(v)):
            merge(dst[k], v)
        else:
            dst[k] = copy.deepcopy(v)


class Node:
    def __init__(self, file_id, cls, props, active, hierarchy):
        self.file_id = file_id
        self.cls = cls                      # 根组件类：UIItem / UISprite / UIText / UITexture ……
        self.props = props                  # 根组件属性
        self.components = []                # [(类, 属性)] 其他组件
        self.active = active
        self.hierarchy = hierarchy
        self.children: list[Node] = []
        self.source = None                  # 来自哪个嵌套预制体

    @property
    def name(self):
        return self.props.get('displayName', self.cls)


class Instance:
    """一个预制体实例：节点、FileID → (节点, 属性字典) 的映射、嵌套实例。"""

    def __init__(self, package: str, depth=0):
        self.package = package
        p = kp.load_prefab(package)
        self.p = p
        self.objects: dict[int, tuple[Node, dict]] = {}
        self.nodes: dict[int, Node] = {}
        self.instances: dict[int, Instance] = {}
        # 类引用可能解析不出来（null，Kuro 自定义的类），记成 Unknown
        classes = [c['ObjectName'].split("'")[1] if c else 'Unknown' for c in p.get('ReferenceClasses', [])]
        for a in p.get('ActorsSerializeData', []):
            node = Node(a['FileID'], classes[a['RootClassIndex']], decode(a['RootComponentPropertyData'], p),
                        a.get('IsActive', True), a.get('HierarchyIndex', 0))
            node.parent_id = a['ParentActorFileID']
            self.nodes[a['FileID']] = node
            self.objects[a['FileID']] = (node, node.props)
            self.objects[a['RootComponentFileID']] = (node, node.props)
        for c in p.get('ComponentsSerializeData', []):
            owner = self.nodes.get(c['OuterFileID'])
            if owner is None:
                continue
            props = decode(c['PropertyData'], p)
            props['_name'] = c.get('Name', '')     # 组件名（补间动画按名字找目标组件）
            owner.components.append((classes[c['ClassIndex']], props))
            self.objects[c['FileID']] = (owner, props)
        # 嵌套预制体
        for n in p.get('NestedPrefabInstancesData', []):
            src = kp.game_to_package(n['SourcePrefabPath']['AssetPathName'])
            sub = Instance(src, depth + 1)
            self.instances[n['PrefabInstanceFileID']] = sub
            for root in sub.roots():
                root.parent_id = n['ParentFileID']
                root.hierarchy = n['HierarchyIndex']
                root.source = src.rsplit('/', 1)[1]
                self.nodes[root.file_id + 10_000_000 * (len(self.instances))] = root  # 只为挂接，键不冲突即可
        chains = {c['FileID']: c['FileIDChain'] for c in p.get('NestedPrefabInstanceChains', [])}
        for vo in p.get('VirtualObjectsSerializeData', []):
            target = self.resolve(chains.get(vo['PrefabInstanceChainFileID'], []), vo['SourcePrefabObjectFileID'])
            if target:
                self.objects[vo['FileID']] = target
        # 属性覆盖
        for pm in p.get('PropertyModificationsSerializeData', []):
            target = self.objects.get(pm['TargetFileID'])
            if target is None:
                continue
            node, props = target
            changes = decode(pm['PropertyData'], p)
            merge(props, changes)
            if 'bIsUIActive' in changes and props is node.props:
                node.active = changes['bIsUIActive']
        # 层级：按父 FileID 挂接（父可能是本预制体的 Actor，也可能是嵌套实例里的对象）
        for node in list(self.nodes.values()):
            pid = getattr(node, 'parent_id', 0)
            if pid and pid in self.objects:
                parent = self.objects[pid][0]
                if node not in parent.children:
                    parent.children.append(node)
        for node in self.nodes.values():
            node.children.sort(key=lambda c: c.hierarchy)

    def resolve(self, chain: list[int], source_id: int):
        if not chain:
            return self.objects.get(source_id)
        sub = self.instances.get(chain[0])
        return sub.resolve(chain[1:], source_id) if sub else None

    def roots(self) -> list[Node]:
        return [n for n in self.nodes.values() if not getattr(n, 'parent_id', 0)]


# ---------------------------------------------------------------- 布局
def widget(node: Node) -> dict:
    w = kp.Props(node.props.get('widget', {}))
    w.setdefault('anchorHAlign', 'UIAnchorHorizontalAlign::Center')
    w.setdefault('anchorVAlign', 'UIAnchorVerticalAlign::Middle')
    w.setdefault('pivot', {'X': 0.5, 'Y': 0.5})
    for k in ('anchorOffsetX', 'anchorOffsetY', 'stretchLeft', 'stretchRight', 'stretchTop', 'stretchBottom'):
        w.setdefault(k, 0.0)
    w.setdefault('width', 100.0)
    w.setdefault('height', 100.0)
    w.setdefault('color', {'R': 255, 'G': 255, 'B': 255, 'A': 255})
    return w


STATE: dict = {'active': {}, 'texts': {}}


def state_active(path: str):
    """状态表里按路径正则强制显隐（模拟运行时：键鼠模式、单人、没有活动面板……），没匹配返回 None。"""
    import re
    result = None
    for pattern, value in STATE['active'].items():
        if re.search(pattern, path):
            result = value
    return result


def comp(node: Node, cls: str):
    return next((p for c, p in node.components if c == cls), None)


def is_shown(node: Node, full: str) -> bool:
    forced = state_active(full)
    return forced if forced is not None else (node.active and node.props.get('bIsUIActive', True))


def ignores_layout(node: Node) -> bool:
    e = comp(node, 'UILayoutElement')
    return bool(e) and e.get('LayoutElementType', '').endswith('IgnoreLayout')


def child_labels(node: Node):
    """同名兄弟加序号（第二个起 名字#1、#2 ……），让状态表能指定到具体哪一个。"""
    seen: dict[str, int] = {}
    for child in node.children:
        n = seen.get(child.name, 0)
        seen[child.name] = n + 1
        yield child, (child.name if n == 0 else f'{child.name}#{n}')


def measure(node: Node, full: str) -> tuple[float, float]:
    """自然尺寸：布局组件开了 Width/HeightFitToChildren 时按子节点算，否则是 widget 的宽高。"""
    w = widget(node)
    width, height = w['width'], w['height']
    for cls, horizontal in (('UIHorizontalLayout', True), ('UIVerticalLayout', False)):
        lp = comp(node, cls)
        if lp is None:
            continue
        pad = lp.get('Padding', {})
        sizes = [measure(c, f'{full}/{lab}') for c, lab in child_labels(node)
                 if is_shown(c, f'{full}/{lab}') and not ignores_layout(c)]
        if not sizes:
            continue
        spacing = lp.get('Spacing', 0.0)
        main = sum(sz[0 if horizontal else 1] for sz in sizes) + spacing * (len(sizes) - 1)
        cross = max(sz[1 if horizontal else 0] for sz in sizes)
        if horizontal:
            if lp.get('WidthFitToChildren'):
                width = main + pad.get('Left', 0) + pad.get('Right', 0)
            if lp.get('HeightFitToChildren'):
                height = cross + pad.get('Top', 0) + pad.get('Bottom', 0)
        else:
            if lp.get('HeightFitToChildren'):
                height = main + pad.get('Top', 0) + pad.get('Bottom', 0)
            if lp.get('WidthFitToChildren'):
                width = cross + pad.get('Left', 0) + pad.get('Right', 0)
    return width, height


def arrange(node: Node, full: str, rect) -> dict:
    """横向 / 纵向布局组件：按 Padding、Spacing、Align 排可见且不忽略布局的子节点，返回 {id(子节点): (左, 下, 宽, 高)}。
    （ExpendChildren* 先不处理：HUD 里用到的地方不影响位置。）"""
    left, bottom, width, height = rect
    for cls, horizontal in (('UIHorizontalLayout', True), ('UIVerticalLayout', False)):
        lp = comp(node, cls)
        if lp is None:
            continue
        pad = lp.get('Padding', {})
        pl, pr, pt, pb = (pad.get(k, 0.0) for k in ('Left', 'Right', 'Top', 'Bottom'))
        spacing = lp.get('Spacing', 0.0)
        align = lp.get('Align', 'ELGUILayoutAlignmentType::UpperLeft').split('::')[1]
        v = 'Upper' if align.startswith('Upper') else ('Lower' if align.startswith('Lower') else 'Middle')
        h = 'Left' if align.endswith('Left') else ('Right' if align.endswith('Right') else 'Center')
        items = [(c, measure(c, f'{full}/{lab}')) for c, lab in child_labels(node)
                 if is_shown(c, f'{full}/{lab}') and not ignores_layout(c)]
        if not items:
            return {}
        inner_l, inner_r = left + pl, left + width - pr
        inner_b, inner_t = bottom + pb, bottom + height - pt
        main = sum(sz[0 if horizontal else 1] for _, sz in items) + spacing * (len(items) - 1)
        out = {}
        if horizontal:
            x = {'Left': inner_l, 'Center': (inner_l + inner_r - main) / 2, 'Right': inner_r - main}[h]
            for c, (cw, ch) in items:
                y = {'Upper': inner_t - ch, 'Middle': (inner_b + inner_t - ch) / 2, 'Lower': inner_b}[v]
                out[id(c)] = (x, y, cw, ch)
                x += cw + spacing
        else:
            y = {'Upper': inner_t, 'Middle': (inner_b + inner_t + main) / 2, 'Lower': inner_b + main}[v]
            for c, (cw, ch) in items:
                x = {'Left': inner_l, 'Center': (inner_l + inner_r - cw) / 2, 'Right': inner_r - cw}[h]
                out[id(c)] = (x, y - ch, cw, ch)
                y -= ch + spacing
        return out
    return {}


def layout(node: Node, parent_rect, out: list, path='', visible=True, label=None, forced_rect=None, xf=(0.0, 0.0, 1.0, 1.0)):
    """parent_rect = (左, 下, 宽, 高)，Y 向上。记录每个节点的绝对矩形（左、上，Y 向下，方便画图）。
    forced_rect：父节点的布局组件给定的矩形（左、下、宽、高），这时不按锚点算。
    xf：父节点局部坐标 → 屏幕坐标（ox, oy, kx, ky）。RelativeScale3D 绕自己的轴心缩放自己和子节点
    （LGUI 的界面平面里 X 是水平、Y 是竖直）。"""
    w = widget(node)
    pl, pb, pw, ph = parent_rect
    h_align = w['anchorHAlign'].split('::')[1]
    v_align = w['anchorVAlign'].split('::')[1]
    px, py = w['pivot']['X'], w['pivot']['Y']
    if h_align == 'Stretch':
        width = pw - w['stretchLeft'] - w['stretchRight']
        left = pl + w['stretchLeft']
    else:
        width = w['width']
        ax = {'Left': pl, 'Center': pl + pw / 2, 'Right': pl + pw}[h_align]
        left = ax + w['anchorOffsetX'] - width * px
    if v_align == 'Stretch':
        height = ph - w['stretchTop'] - w['stretchBottom']
        bottom = pb + w['stretchBottom']
    else:
        height = w['height']
        ay = {'Bottom': pb, 'Middle': pb + ph / 2, 'Top': pb + ph}[v_align]
        bottom = ay + w['anchorOffsetY'] - height * py
    full = f'{path}/{label or node.name}'
    shown = visible and is_shown(node, full)
    # 布局组件撑开的尺寸（宽 / 高随子节点），按轴心保持位置
    mw, mh = measure(node, full)
    if forced_rect is None and (mw, mh) != (width, height) and h_align != 'Stretch' and v_align != 'Stretch':
        left += (width - mw) * px
        bottom += (height - mh) * py
        width, height = mw, mh
    if forced_rect is not None:
        left, bottom, width, height = forced_rect
    sc = node.props.get('RelativeScale3D') or {}
    sx, sy = sc.get('X', 1.0), sc.get('Y', 1.0)
    # 界面平面里的旋转（旧版 LGUI：Yaw）。只支持 180°（镜像用的，等价于两个轴取反）；其他角度只记录
    yaw = (node.props.get('RelativeRotation') or {}).get('Yaw', 0.0)
    rotation = round(yaw / 90.0) * 90 % 360 if abs(yaw) > 1 else 0
    if rotation == 180:
        sx, sy = -sx, -sy
    elif rotation:
        print('WARN rotation', rotation, full)
    pivot_x, pivot_y = left + width * px, bottom + height * py
    ox, oy, kx, ky = xf
    own = (ox + kx * (pivot_x - sx * pivot_x), oy + ky * (pivot_y - sy * pivot_y), kx * sx, ky * sy)
    screen_left, screen_bottom = own[0] + own[2] * left, own[1] + own[3] * bottom
    screen_w, screen_h = own[2] * width, own[3] * height
    flip = [screen_w < 0, screen_h < 0]
    if screen_w < 0:
        screen_left, screen_w = screen_left + screen_w, -screen_w
    if screen_h < 0:
        screen_bottom, screen_h = screen_bottom + screen_h, -screen_h
    entry = {
        'path': full, 'scale': abs(own[2]), 'parent': path, 'flip': flip,
        # 导出到 LGUI3 用：父节点局部坐标里的矩形（左、下、宽、高，Y 向上，原点在父矩形左下角）、父矩形尺寸、对齐、轴心、自身缩放
        'local': [left - pl, bottom - pb, width, height], 'parent_size': [pw, ph],
        'align': [h_align, v_align], 'pivot': [px, py], 'local_scale': [sx, sy] if rotation != 180 else [-sx, -sy],
        'rotation': rotation, 'class': node.cls, 'source': node.source, 'active': shown, 'file': node.file_id,
        'rect': [screen_left, STATE.get('canvas', DESIGN)[1] - (screen_bottom + screen_h), screen_w, screen_h],
        'widget': w, 'props': kp.Props((k, v) for k, v in node.props.items() if k not in ('widget',)),
        'components': [{'class': c, 'props': cp} for c, cp in node.components],
    }
    out.append(entry)
    placed = arrange(node, full, (left, bottom, width, height))
    for child, lab in child_labels(node):
        layout(child, (left, bottom, width, height), out, entry['path'], shown, lab, placed.get(id(child)), own)


# ---------------------------------------------------------------- 精灵
_importers: dict[str, dict] = {}


def sprite_info(sprite_package: str):
    """精灵资产 → (图集 PNG 路径, 像素矩形 x,y,w,h, 九宫格 l,r,t,b)。"""
    sp = json.loads(kp.package_json(sprite_package).read_text(encoding='utf-8'))[0]
    importer = sp['Properties']['Importer']['ObjectPath'].split('.')[0]
    if importer not in _importers:
        _importers[importer] = json.loads(kp.package_json(importer).read_text(encoding='utf-8'))[0]['Properties']
    imp = _importers[importer]
    name = sprite_package.rsplit('/', 1)[1]
    index = next(i for i, s in enumerate(imp['Sprites']) if s.split('.')[-1] == name)
    info = dict(next(e['Value'] for e in imp['SpriteInfoMap'] if int(e['Key']) == index))
    # 精灵资产自己的九宫格边距（BorderLeft ……）优先于导入器里的
    for key in ('Left', 'Right', 'Top', 'Bottom'):
        if f'Border{key}' in sp['Properties']:
            info[f'border{key}'] = sp['Properties'][f'Border{key}']
    # 有的导入器（TPI_Common）每个精灵一张单独的贴图（_SpriteTextures/<精灵名>），UV 相对这张小图；
    # 其他的（TPI_UiFight）是几张大图集，按 SpriteAtlasTextureMap 找
    # 老的导入器没有 SoftAtlasTextures，是硬引用 AtlasTextures（ObjectPath 已经是包路径）
    if 'SoftAtlasTextures' in imp:
        atlases = [kp.game_to_package(a['AssetPathName'].split('.')[0]) for a in imp['SoftAtlasTextures']]
    else:
        atlases = [a['ObjectPath'].split('.')[0] for a in imp['AtlasTextures']]
    own = next((a for a in atlases if a.rsplit('/', 1)[1] == name), None)
    if own is None:
        atlas_index = next(e['Value'] for e in imp['SpriteAtlasTextureMap'] if int(e['Key']) == index)
        own = atlases[atlas_index]
    return texture_png(own), info


def texture_png(package: str) -> Path:
    """贴图 PNG：用 PakExport 从当前游戏包导出（网上的 UIResources 是别的版本，图集排布对不上）。"""
    png = kp.EXPORTS / f'{package}.png'
    if not png.exists():
        kp.subprocess.run([str(kp.PAKEXPORT), 'export', f'{package}.uasset'], check=True, capture_output=True)
    return png


def main(argv):
    sys.stdout.reconfigure(encoding='utf-8')
    package = argv[0]
    if '--state' in argv:
        STATE.update(json.loads(Path(argv[argv.index('--state') + 1]).read_text(encoding='utf-8')))
    inst = Instance(package)
    out = []
    # --scale：游戏里的界面缩放（锚在屏幕边上，元素整体缩小）= 设计分辨率放大 1/scale 倍
    scale = float(argv[argv.index('--scale') + 1]) if '--scale' in argv else 1.0
    canvas = (DESIGN[0] / scale, DESIGN[1] / scale)
    STATE['canvas'] = canvas
    for root in inst.roots():
        layout(root, (0.0, 0.0, canvas[0], canvas[1]), out)
    if '--json' in argv:
        Path(argv[argv.index('--json') + 1]).write_text(json.dumps(out, ensure_ascii=False, indent=1, default=str), encoding='utf-8')
    if '--png' in argv:
        render(out, argv[argv.index('--png') + 1])
    for e in out:
        if e['active']:
            x, y, w, h = e['rect']
            spr = e['props'].get('sprite', {}).get('name') if isinstance(e['props'].get('sprite'), dict) else ''
            print(f"{e['path'][-90:]:90s} {e['class']:10s} {x:7.0f} {y:7.0f} {w:6.0f} {h:6.0f} {spr or ''}")


def nine_slice(img, border, width, height):
    """九宫格：四角原样，边和中间拉伸到目标尺寸（UISprite 的 Sliced）。"""
    from PIL import Image
    l, r, t, b = border
    W, H = img.size
    tw, th = max(int(width), l + r + 1), max(int(height), t + b + 1)
    out = Image.new('RGBA', (tw, th))
    xs_src, ys_src = [0, l, W - r, W], [0, t, H - b, H]
    xs_dst, ys_dst = [0, l, tw - r, tw], [0, t, th - b, th]
    for i in range(3):
        for j in range(3):
            box = (xs_src[i], ys_src[j], xs_src[i + 1], ys_src[j + 1])
            size = (xs_dst[i + 1] - xs_dst[i], ys_dst[j + 1] - ys_dst[j])
            if box[2] > box[0] and box[3] > box[1] and size[0] > 0 and size[1] > 0:
                out.paste(img.crop(box).resize(size), (xs_dst[i], ys_dst[j]))
    return out


def render(out, png_path):
    from PIL import Image
    size = STATE.get('canvas', DESIGN)
    canvas = Image.new('RGBA', (int(size[0]), int(size[1])), (40, 44, 52, 255))
    atlases = {}
    for e in out:
        if not e['active'] or e['class'] not in ('UISprite', 'UITexture'):
            continue
        ref = e['props'].get('sprite') if e['class'] == 'UISprite' else e['props'].get('texture')
        if not isinstance(ref, dict) or 'asset' not in ref:
            continue
        try:
            if e['class'] == 'UISprite':
                png, info = sprite_info(ref['asset'])
                if png not in atlases:
                    atlases[png] = Image.open(png).convert('RGBA')
                atlas = atlases[png]
                W, H = atlas.size
                x0 = round(info['uv0X'] * W)
                y0 = round(info['uv3Y'] * H)
                img = atlas.crop((x0, y0, x0 + info['width'], y0 + info['height']))
                border = (info['borderLeft'], info['borderRight'], info['borderTop'], info['borderBottom'])
                if any(border) and e['props'].get('type', 'UISpriteType::Normal') in ('UISpriteType::Sliced', 'UISpriteType::SlicedFrame', 'UISpriteType::FilledSliced'):
                    img = nine_slice(img, border, e['rect'][2], e['rect'][3])
            else:
                img = Image.open(texture_png(ref['asset'])).convert('RGBA')
        except Exception as ex:
            print('render skip', e['path'], ex)
            continue
        x, y, w, h = e['rect']
        if w < 1 or h < 1:
            continue
        img = img.resize((max(1, int(w)), max(1, int(h))))
        if e.get('flip', [False, False])[0]:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
        if e.get('flip', [False, False])[1]:
            img = img.transpose(Image.FLIP_TOP_BOTTOM)
        c = e['widget']['color']
        if (c['R'], c['G'], c['B'], c['A']) != (255, 255, 255, 255):
            r, g, b, a = img.split()
            r = r.point(lambda v, m=c['R']: v * m // 255)
            g = g.point(lambda v, m=c['G']: v * m // 255)
            b = b.point(lambda v, m=c['B']: v * m // 255)
            a = a.point(lambda v, m=c['A']: v * m // 255)
            img = Image.merge('RGBA', (r, g, b, a))
        canvas.alpha_composite(img, (int(x), int(y)))
        pass
    from PIL import ImageDraw, ImageFont
    import re
    draw = ImageDraw.Draw(canvas)
    for e in out:
        if not e['active'] or e['class'] != 'UIText':
            continue
        text = next((v for k, v in STATE['texts'].items() if re.search(k, e['path'])), None)
        if text is None:
            continue
        size = max(1, int(e['props'].get('size', 18) * e.get('scale', 1.0)))
        font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', size)
        c = e['widget']['color']
        x, y, w, h = e['rect']
        align = e['props'].get('hAlign', 'UITextParagraphHorizontalAlign::Center').split('::')[1]
        tw = draw.textlength(text, font=font)
        tx = x if align == 'Left' else (x + w - tw if align == 'Right' else x + (w - tw) / 2)
        ty = y + (h - size) / 2 - size * 0.15
        draw.text((tx, ty), text, font=font, fill=(c['R'], c['G'], c['B'], c['A']), stroke_width=1 if e['props'].get('outlineSize') else 0, stroke_fill=(0, 0, 0, 120))
    canvas.save(png_path)
    print('PREVIEW', png_path)


if __name__ == '__main__':
    main(sys.argv[1:])
