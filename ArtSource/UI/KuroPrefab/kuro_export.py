"""原作 UI 预制体（按状态表展开后的布局）→ 项目 LGUI3 预制体的构建数据，由 -run=WuwaKuroHudBuild 读取。

  python -I kuro_export.py hud_main.export.json

输出（原作资源，不提交）：
  ArtSource/UI/HUD/Kuro/<名字>.layout.json   节点列表（父子顺序：父节点总在子节点前）
  ArtSource/UI/HUD/Kuro/Textures/*.png       用到的精灵（按原名从图集裁出）和贴图
每个节点：name / parent（下标，-1 是根）/ type（Item、Texture、Text）/ anchor（LGUI3 的 FUIAnchorData：pivot、min、max、pos、size，
已换算到项目画布）/ scale / color / texture（PNG 相对仓库根的路径）/ slice（九宫格边距，像素）/ fill（填充型精灵）/ text / button / registry。

锚点换算（原作旧版 LGUI 的 UIWidget → LGUI3，和 Unity RectTransform 同一套）：
  对齐 Left/Center/Right（Bottom/Middle/Top）→ anchorMin = anchorMax = 0/0.5/1；Stretch → 0~1。
  非拉伸：pos = 轴心位置 − 父矩形里的锚点位置，size = 宽高；拉伸：size = 宽 − 父宽，pos = 左边距 + size × 轴心。
布局组件排出来的位置已经算好（运行时状态固定），导出后不再需要布局组件。
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I 不会把脚本目录加进 sys.path
import kuro_layout as kl  # noqa: E402
import kuro_motion as km  # noqa: E402

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
OUT = REPO / 'ArtSource/UI/HUD/Kuro'
ALIGN = {'Left': (0.0, 0.0), 'Center': (0.5, 0.5), 'Right': (1.0, 1.0), 'Stretch': (0.0, 1.0),
         'Bottom': (0.0, 0.0), 'Middle': (0.5, 0.5), 'Top': (1.0, 1.0)}


def anchor_data(e: dict, k: float) -> dict:
    l, b, w, h = e['local']
    pw, ph = e['parent_size']
    px, py = e['pivot']
    data = {'pivot': [px, py], 'min': [0.0, 0.0], 'max': [0.0, 0.0], 'pos': [0.0, 0.0], 'size': [0.0, 0.0]}
    for axis, (start, length, parent, pivot) in enumerate(((l, w, pw, px), (b, h, ph, py))):
        lo, hi = ALIGN[e['align'][axis]]
        data['min'][axis], data['max'][axis] = lo, hi
        if lo != hi:
            size = length - parent
            data['size'][axis] = size * k
            data['pos'][axis] = (start + size * pivot) * k
        else:
            data['size'][axis] = length * k
            data['pos'][axis] = (start + length * pivot - parent * lo) * k
    return data


class Textures:
    """精灵 / 贴图 → PNG（按名字去重）。"""

    def __init__(self):
        (OUT / 'Textures').mkdir(parents=True, exist_ok=True)
        self.done: dict[str, dict] = {}
        self.atlases = {}

    def sprite(self, package: str) -> dict:
        name = package.rsplit('/', 1)[1]
        if name not in self.done:
            from PIL import Image
            png, info = kl.sprite_info(package)
            if png not in self.atlases:
                self.atlases[png] = Image.open(png).convert('RGBA')
            atlas = self.atlases[png]
            W, H = atlas.size
            x0, y0 = round(info['uv0X'] * W), round(info['uv3Y'] * H)
            path = OUT / 'Textures' / f'{name}.png'
            atlas.crop((x0, y0, x0 + info['width'], y0 + info['height'])).save(path)
            border = [info['borderLeft'], info['borderRight'], info['borderTop'], info['borderBottom']]
            self.done[name] = {'file': path.relative_to(REPO).as_posix(), 'border': border if any(border) else None}
        return self.done[name]

    def tiled(self, file: str, count: float) -> str:
        """把一格贴图横向排 count 格（最后一格按比例裁），返回新 PNG 的仓库相对路径。"""
        from PIL import Image
        src = Image.open(REPO / file).convert('RGBA')
        width = max(1, round(src.width * count))
        out = Image.new('RGBA', (width, src.height))
        for x in range(0, width, src.width):
            out.paste(src, (x, 0))
        path = (REPO / file).with_name(f'{Path(file).stem}_x{round(count * 100)}.png')
        out.save(path)
        return path.relative_to(REPO).as_posix()

    def texture(self, ref: str) -> dict:
        """Client/... 贴图包路径（用 PakExport 导出）或仓库里已有的 PNG。"""
        if ref.endswith('.png'):
            return {'file': ref, 'border': None}
        name = ref.rsplit('/', 1)[1]
        if name not in self.done:
            from PIL import Image
            path = OUT / 'Textures' / f'{name}.png'
            Image.open(kl.texture_png(ref)).save(path)
            self.done[name] = {'file': path.relative_to(REPO).as_posix(), 'border': None}
        return self.done[name]


def first(patterns: dict, path: str):
    return next((v for k, v in patterns.items() if re.search(k, path)), None)


def convert(prefab: str, state_file: str, spec: dict, k: float, textures: Textures, skip, nodes: list, index: dict,
            parent: int, prefix: str, curves: km.Curves | None = None):
    """展开一个原作预制体（按状态表），把显示中的节点追加到 nodes；parent >= 0 时根节点挂到 nodes[parent] 下。"""
    kl.STATE.clear()
    kl.STATE.update({'active': {}, 'texts': {}})
    kl.STATE.update(json.loads((HERE / state_file).read_text(encoding='utf-8')))
    kl.STATE['canvas'] = kl.DESIGN
    inst = kl.Instance(prefab)
    entries: list[dict] = []
    for root in inst.roots():
        kl.layout(root, (0.0, 0.0, kl.DESIGN[0], kl.DESIGN[1]), entries)
    local: dict[str, int] = {}
    # 规格里 include 的节点即使默认隐藏也导出（运行时才显示的文字、只挂补间的动画节点），导出成隐藏
    include = [re.compile(x) for x in spec.get('include', [])]
    motions: list[tuple[int, list]] = []
    file_to_path: dict[int, str] = {}
    for e in entries:
        forced = not e['active'] and any(x.search(e['path']) for x in include)
        if (not e['active'] and not forced) or (e['parent'] and e['parent'] not in local):
            continue
        if any(p.search(e['path']) or p.search(e['class']) for p in skip):
            continue
        path = prefix + e['path']
        node = {'name': e['path'].rsplit('/', 1)[1].replace('#', '_'), 'path': path, 'hidden': forced,
                'parent': local.get(e['parent'], parent), 'type': 'Item', 'anchor': anchor_data(e, k),
                'scale': e['local_scale'], 'rotation': e.get('rotation', 0), 'color': [e['widget']['color'][c] for c in 'RGBA']}
        # 状态表里的颜色覆盖（原作运行时由动画 / 配置改的颜色、透明度）
        node['color'] = first(kl.STATE.get('colors', {}), e['path']) or node['color']
        override = first(spec.get('textures', {}), path)
        props = e['props']
        try:
            if e['class'] == 'UISprite' and (override or isinstance(props.get('sprite'), dict)):
                tex = textures.texture(override) if override else textures.sprite(props['sprite']['asset'])
                sliced = props.get('type', 'UISpriteType::Normal') in ('UISpriteType::Sliced', 'UISpriteType::SlicedFrame', 'UISpriteType::FilledSliced')
                node.update(type='Texture', texture=tex['file'], slice=tex['border'] if sliced else None)
                # 填充型精灵（协奏圈 BarState 这类，运行时改 fillAmount）：构建时做成 UISprite
                # 平铺型精灵（能量格的刻度这类）：原作一格宽 = 设计宽度 / tileX。LGUI3 的 Tiled 按像素大小平铺、没有格数，
                # 在 0.75 倍的画布上刻度会大 1.33 倍；所以导出时按原作的格宽把贴图预先排成一条，当普通贴图拉伸（格数、间距和原作一致）
                if props.get('type') == 'UISpriteType::Tiled' and props.get('tileX') and props.get('width'):
                    tile_w = props['width'] / props['tileX']
                    node['texture'] = textures.tiled(node['texture'], e['local'][2] / tile_w)
                if props.get('type') == 'UISpriteType::FilledSliced':
                    # Kuro 的九宫格填充（LGUI3 没有）：构建时做成裁剪容器 + 满宽九宫格贴图，运行时改容器宽度（左端固定）
                    # 容器横向改成左端锚定、绝对宽度（原作的拉伸宽度按布局算好），轴心 X = 0，这样改宽度时左端不动
                    width = e['local'][2] * k
                    a = node['anchor']
                    a['min'][0] = a['max'][0] = 0.0
                    a['pivot'][0] = 0.0
                    a['pos'][0] = e['local'][0] * k
                    a['size'][0] = width
                    node['fill'] = {'method': 'Horizontal', 'origin': 0, 'flip': False, 'amount': float(props.get('fillAmount', 1.0)),
                                    'sliced': True, 'width': width}
                    node['slice'] = tex['border']
                if props.get('type') == 'UISpriteType::Filled':
                    node['fill'] = {'method': props.get('fillMethod', 'UISpriteFillMethod::Horizontal').split('::')[1],
                                    'origin': int(props.get('fillOrigin', 0)), 'flip': bool(props.get('fillDirectionFlip', False)),
                                    'amount': float(props.get('fillAmount', 1.0))}
            elif e['class'] == 'UITexture' and (override or isinstance(props.get('texture'), dict)):
                tex = textures.texture(override or props['texture']['asset'])
                node.update(type='Texture', texture=tex['file'], slice=None)
            elif e['class'] == 'UIText':
                text = first(kl.STATE.get('texts', {}), e['path']) or ''
                # 富文本里的 <size=N> 也按画布比例换算
                text = re.sub(r'<size=(\d+(?:\.\d+)?)>', lambda m: f'<size={round(float(m.group(1)) * k)}>', text)
                # 状态表里的文字颜色（原作运行时用富文本上的色）
                node['color'] = first(kl.STATE.get('textColors', {}), e['path']) or node['color']
                oc = props.get('outlineColor', {'R': 0, 'G': 0, 'B': 0, 'A': 128})
                node.update(type='Text', text={
                    'text': text, 'size': props.get('size', 18.0) * k,
                    'hAlign': props.get('hAlign', 'UITextParagraphHorizontalAlign::Center').split('::')[1],
                    'vAlign': props.get('vAlign', 'UITextParagraphVerticalAlign::Middle').split('::')[1],
                    'outline': props.get('outlineSize', 0), 'outlineColor': [oc[c] for c in 'RGBA']})
        except Exception as ex:
            print('skip resource', path, ex)
        # 裁剪画布（LGUICanvas clipType Rect）：受击残影这类按矩形裁掉子节点
        if any(c['class'] == 'LGUICanvas' and str(c['props'].get('clipType', '')).endswith('Rect') for c in e.get('components', [])):
            node['clip'] = True
        if first({p: True for p in spec.get('buttons', [])}, path):
            node['button'] = True
        local[e['path']] = len(nodes)
        index[path] = len(nodes)
        file_to_path[e['file']] = path
        if curves is not None:
            comps = [(c['class'], c['props']) for c in e.get('components', [])]
            anims = km.text_animations(comps, curves, k)
            if anims:
                node['textAnimations'] = anims
            outline = km.outline_effect(comps)
            if outline and node['type'] == 'Text':
                node['outlineEffect'] = outline
            motions.append((len(nodes), comps))
        nodes.append(node)
    # 补间的目标可能是后面的节点：全部节点导出后再解析
    for at, comps in motions:
        tw = km.tweens(comps, file_to_path, curves, k)
        if tw:
            nodes[at]['tweens'] = tw


def main(spec_path: str) -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    spec = json.loads((HERE / spec_path).read_text(encoding='utf-8'))
    k = spec['targetHeight'] / spec['designHeight']
    textures = Textures()
    skip = [re.compile(p) for p in spec.get('skip', [])]
    nodes: list[dict] = []
    index: dict[str, int] = {}
    curves = km.Curves() if spec.get('motion') else None
    convert(spec['prefab'], spec['state'], spec, k, textures, skip, nodes, index, parent=-1, prefix='', curves=curves)
    # 根节点的位置（单独的子预制体挂在别的界面下时，原作父节点的位置）
    if spec.get('rootAnchor'):
        nodes[0]['anchor'] = spec['rootAnchor']
    # 嵌套：把另一个原作预制体（带自己的状态表）挂到某个节点下（例如长离能量条 = 变身外框 + BarSlot 下的四段格子条）
    for nest in spec.get('nest', []):
        hits = [i for i, n in enumerate(nodes) if re.search(nest['under'], n['path'])]
        if len(hits) != 1:
            print(f"ERROR nest under {nest['under']}: {len(hits)} matches")
            return 1
        convert(nest['prefab'], nest['state'], spec, k, textures, skip, nodes, index, parent=hits[0], prefix=nodes[hits[0]]['path'])
    # 额外的文字（队伍槽的数字键：原作是印着数字的键帽贴图，项目里由 HUD 代码写数字）
    for add in spec.get('addTexts', []):
        for i, n in enumerate(list(nodes)):
            if re.search(add['under'], n['path']):
                path = n['path'] + '/' + add['name']
                index[path] = len(nodes)
                nodes.append({'name': add['name'], 'path': path, 'parent': i, 'type': 'Text',
                              'anchor': {'pivot': [0.5, 0.5], 'min': [0, 0], 'max': [1, 1], 'pos': [0, 0], 'size': [0, 0]},
                              'scale': [1, 1], 'color': add['color'],
                              'text': {'text': add['text'], 'size': add['size'] * k, 'hAlign': 'Center', 'vAlign': 'Middle',
                                       'outline': 0, 'outlineColor': [0, 0, 0, 0]}})
    # 额外节点（网络延迟：原作在别的界面里）
    for extra in spec.get('extra', []):
        def add_node(d, parent):
            size = d.get('size2', d['size'])
            n = {'name': d['name'], 'path': f"{nodes[parent]['path']}/{d['name']}", 'parent': parent, 'type': d.get('type', 'Item'),
                 'anchor': {'pivot': d['pivot'], 'min': d['anchor'], 'max': d['anchor'],
                            'pos': [v * k for v in d['pos']], 'size': [v * k for v in size]},
                 'scale': [1, 1], 'color': d.get('color', [255, 255, 255, 255])}
            if n['type'] == 'Texture':
                n.update(texture=textures.texture(d['texture'])['file'], slice=None)
            if n['type'] == 'Niagara':
                # 界面特效（UWuwaUINiagara）：系统资产 + 原作 UINiagara 的 NiagaraScale（设计分辨率单位，换算到项目画布）
                n['niagara'] = {'system': d['system'], 'scale': d.get('niagaraScale', 1.0) * k}
            if n['type'] == 'Text':
                n['text'] = {'text': d['text'], 'size': d['size'] * k, 'hAlign': d.get('align', 'Center'), 'vAlign': 'Middle',
                             'outline': 0, 'outlineColor': [0, 0, 0, 0]}
            index[n['path']] = len(nodes)
            nodes.append(n)
            me = len(nodes) - 1
            for c in d.get('children', []):
                add_node(c, me)
        add_node(extra, index[extra['parent']])
    # 注册表：owner 节点挂 WuwaUiComponentsRegistry，依次登记 entries（下标就是代码里的编号，只能往后加）
    for reg in spec.get('registries', []):
        def one(pattern):
            hits = [j for j, n in enumerate(nodes) if re.search(pattern, n['path'])]
            if len(hits) != 1:
                raise SystemExit(f'ERROR registry pattern {pattern}: {len(hits)} matches')
            return hits[0]
        nodes[one(reg['owner'])]['registry'] = [one(p) for p in reg['entries']]
    name = Path(spec_path).stem.split('.')[0]
    OUT.mkdir(parents=True, exist_ok=True)
    out = OUT / f'{name}.layout.json'
    # 补间目标存节点下标（构建时换成 Actor）
    for n in nodes:
        for tw in n.get('tweens', []):
            for ev in tw['onUpdate'] + tw['onStart'] + tw['onComplete']:
                ev['target'] = index[ev['target']]
    layout = {'prefab': spec['prefab'], 'nodes': nodes}
    if curves is not None:
        layout['curves'] = list(curves.items.values())
    out.write_text(json.dumps(layout, ensure_ascii=False, indent=1), encoding='utf-8')
    if km.DROPPED:
        print(f'tween events dropped (target not exported): {sorted(set(km.DROPPED))}')
    counts = {t: sum(1 for n in nodes if n['type'] == t) for t in ('Item', 'Texture', 'Text')}
    print(f'{out}: {len(nodes)} nodes {counts}, {len(textures.done)} textures, registries {sum(1 for n in nodes if "registry" in n)}, buttons {sum(1 for n in nodes if n.get("button"))}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'hud_main.export.json'))
