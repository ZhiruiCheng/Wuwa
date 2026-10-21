"""Prepare an attack recipe (AttackNN.json) from exported evidence, without touching UE assets.

Usage: python prepare_attack.py Attack03|Attack04|Attack05

Reuses the shader reconstruction of prepare_reference.py (first attack). Differences, all explicit:
- curves are resampled over their exported LUT domain (LUTMinTime / LUTInvTimeRange) on normalized age;
- mesh WPO combines Rotation Rate (integrated), Z Rotation Vector (degrees) and Scale Factor (XY) curves;
- a Scale RGBA colour curve stands in for the colour curve when a layer has no Color curve;
- original groups with several children are expanded into one placement per supported child;
- layers this pipeline cannot build (ribbon renderers, distortion materials, meshes not exported yet) are listed in skippedLayers.
Lifetimes, intensities, counts and sprite emission are not in the cooked export and stay authored reconstruction.
"""
from __future__ import annotations

import json
import math
import shutil
import sys
from pathlib import Path

import prepare_reference as ref

HERE = Path(__file__).resolve().parent
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
SAMPLES = ref.SAMPLES

# 第一段的重建系统；后面几段原作直接复用 R1a01 的刀光组。
ATTACK01_GROUPS = {
    'DA_Fx_Group_R1a01_01_N_Dg': 'NS_Reference_Attack01_N_Dg',
    'DA_Fx_Group_R1a01_01_N_Dg1': 'NS_Reference_Attack01_N_Dg1',
    'DA_Fx_Group_R1a01_01_N1_Dg': 'NS_Reference_Attack01_N1_Dg',
}
# 每段：要生成的原作 NS → 重建系统名；原作组 → 子效果（重建系统名，组内延迟）。组里没导出 / 不支持的子效果（灯光、后处理、贴花、音频、Mesh 模型）不列。
CONFIG = {
    'Attack03': {
        'systems': {
            'NS_Fx_Changli_R1a03_01_Daoguang': 'NS_Reference_Attack03_Daoguang01',
            'NS_Fx_Changli_R1a03_02_Daoguang': 'NS_Reference_Attack03_Daoguang02',
            'NS_Fx_Changli_R1a03_N_Dg1': 'NS_Reference_Attack03_N_Dg1',
            'NS_Fx_Changli_R1a02_01_N_Dg': 'NS_Reference_Attack03_N_Dg',
        },
        'groups': {
            'DA_Fx_Group_R1a03_01': [('NS_Reference_Attack03_Daoguang01', 0.0)],
            'DA_Fx_Group_R1a03_02': [('NS_Reference_Attack03_Daoguang02', 0.0)],
            'DA_Fx_Group_R1a03_01_N_Dg1': [('NS_Reference_Attack03_N_Dg1', 0.0)],
            'DA_Fx_Group_R1a02_01_N_Dg': [('NS_Reference_Attack03_N_Dg', 0.0)],
        },
        'description': '第三段攻击（AM_Attack03）：原作 8 个 AnimNotifyEffect，全部 Attached=false。网格 / 贴图为导出原件，材质与 Niagara 为重建。由 prepare_attack.py 生成。',
    },
    'Attack04': {
        'systems': {
            'NS_Fx_Changli_R1a04_Flying_Local': 'NS_Reference_Attack04_Flying',
            'NS_Fx_Changli_R1a04_Xuli': 'NS_Reference_Attack04_Xuli',
            'NS_Fx_Changli_San_End': 'NS_Reference_Attack04_SanEnd',
        },
        'groups': {
            **{group: [(name, 0.0)] for group, name in ATTACK01_GROUPS.items()},
            'DA_Fx_Group_R1a04_Xuli': [('NS_Reference_Attack04_Xuli', 0.0)],
            'DA_Fx_Group_R1a04_Flying_Local': [('NS_Reference_Attack04_Flying', 0.0)],
            'DA_Fx_Group_San_end': [('NS_Reference_Attack04_SanEnd', 0.0)],
            # DA_Fx_Group_R1a04_San 只有一个 Mesh 模型（DA_Fx_Mesh_San，未导出、Mesh 模型类型也还没实现）。
        },
        'description': '第四段攻击（AM_Attack04）：原作 4 个 AnimNotifyEffect + 2 个 AnimNotifyStateEffect（收伞）。刀光复用第一段 R1a01；网格 / 贴图为导出原件，材质与 Niagara 为重建。由 prepare_attack.py 生成。',
    },
    'Attack05': {
        'systems': {
            'NS_Fx_Changli_R1a04_01_DgW': 'NS_Reference_Attack05_DgW',
            'NS_Fx_Changli_R1a04_01_DgW_Dilie': 'NS_Reference_Attack05_DgWDilie',
            'NS_Fx_Changli_R1a04_01_Dilie': 'NS_Reference_Attack05_Dilie',
            'NS_Fx_Changli_R1a4_Blink': 'NS_Reference_Attack05_Blink',
            'NS_Fx_Changli_R1a4_Blink2': 'NS_Reference_Attack05_Blink2',
        },
        'groups': {
            **{group: [(name, 0.0)] for group, name in ATTACK01_GROUPS.items()},
            # 原组还有 DA_Fx_Light_R1a04_2、DA_Fx_Post_R1a04_1（未导出）。
            'DA_Fx_Group_R1a04_Blink': [('NS_Reference_Attack05_Blink', 0.0)],
            'DA_Fx_Group_R1a04_Blink2': [('NS_Reference_Attack05_Blink2', 0.0)],
            # 原组还有 DA_Fx_Decal_R2a04_1_DgW（延迟 0.02）、DA_Au_Role_Changli_atk04_hit（未导出）。
            'DA_Fx_Group_R1a04_01_DgW': [('NS_Reference_Attack05_DgW', 0.0), ('NS_Reference_Attack05_DgWDilie', 0.0), ('NS_Reference_Attack05_Dilie', 0.0)],
            # DA_Fx_Group_R1a04_1_Trail 只有 Ribbon 拖尾，生成器还不支持 Ribbon。
        },
        'description': '第五段攻击（项目 Attack05 = 原作 AM_Attack04_1）：刀光复用第一段 R1a01；网格 / 贴图为导出原件，材质与 Niagara 为重建。由 prepare_attack.py 生成。',
    },
}
# 动态材质参数曲线（溶解 / UV 偏移）接进重建 shader；第三段也用，第一、二段（旧生成器）不受影响。
DYNAMIC_PARAMETERS_IN_ATTACK03 = True
CLAMP_OFFSET_UV = False
# 第一段同类层的人工寿命 / 亮度（cooked 导出里没有）；新层按名字类比，未知类型用 default。
LIFETIME = {'Liang': .45, 'BG': .42, 'Fether_B': .54, 'Fether_R': .58, 'Wenli_WG': .54, 'default': .5}
INTENSITY = {'Liang': 1.25, 'BG': .85, 'Fether_B': 1.4, 'Fether_R': 1.25, 'Wenli_WG': 1.4, 'default': 1.2}
# 精灵层沿用第一段的重建材质（同一 MI）；其他精灵材质照常生成。
REUSED_SPRITE_MATERIALS = {
    '/Game/Aki/Effect/Materials/MI_Sprite/MI_Trans_Wenli_140031_S': 'Reference_Attack01_N_Dg1_yumao001',
    '/Game/Aki/Effect/Materials/MI_Sprite/MI_Trans_Wenli_140031_S1': 'Reference_Attack01_N_Dg1_yumao002',
}


def kind(handle):
    for key in ('Wenli_WG', 'Fether_B', 'Fether_R', 'BG'):
        if key in handle:
            return key
    if 'Liang' in handle or 'liang' in handle or 'Glow' in handle:
        return 'Liang'
    return 'default'


def domain_rows(curve):
    """24 rows over normalized age 0..1, sampling the LUT through its exported domain (clamped outside it)."""
    samples = curve['samples']
    domain = curve.get('exportedDomain', {})
    lo = domain.get('LUTMinTime', 0.0)
    inv = float(domain.get('LUTInvTimeRange', 1.0))  # 导出里单点曲线写成 'Infinity'
    hi = domain['LUTMaxTime'] if 'LUTMaxTime' in domain else lo + (1.0 / inv if math.isfinite(inv) and inv > 0 else 0.0)
    rows = []
    for i in range(SAMPLES):
        age = i / (SAMPLES - 1)
        t = 0.0 if hi <= lo else min(max((age - lo) / (hi - lo), 0.0), 1.0)
        p = t * (len(samples) - 1)
        low = math.floor(p)
        high = min(low + 1, len(samples) - 1)
        rows.append([a + (b - a) * (p - low) for a, b in zip(samples[low], samples[high])])
    return rows


def find(layer, pattern):
    return next((c for c in layer['curves'] if pattern in c['semanticBinding'] and c['samples']), None)


def resampled_layer(layer):
    """Copy of the layer whose colour / alpha curves are already resampled over their domain, for ref.shader()."""
    curves = []
    color = find(layer, '.Color.ColorCurve') or find(layer, '.Scale RGBA.')
    if color:
        curves.append({'semanticBinding': layer['handle'] + '.Color.ColorCurve', 'samples': domain_rows(color)})
    alpha = find(layer, '.Scale Alpha.FloatCurve')
    if alpha:
        curves.append({'semanticBinding': layer['handle'] + '.Scale Alpha.FloatCurve', 'samples': domain_rows(alpha)})
    return {**layer, 'curves': curves}


def mesh_wpo(layer, lifetime):
    rate = find(layer, '.Rotation Rate.')
    angle = find(layer, '.Z.Rotation Vector.')
    scale = find(layer, '.Scale Factor.VectorCurve')
    if not (rate or angle or scale):
        return '', None
    code = '// Reconstructed mesh motion from original curves (domain-resampled over normalizedAge).\n'
    code += '// Assumptions: rate units=turns/second about local +Z; Rotation Vector Z in degrees; Scale Factor scales local XY; pivot=mesh origin.\n'
    code += f'float a=saturate(Age);\nfloat cp=a*{ref.f(SAMPLES - 1)};\nint ci=min((int)floor(cp),{SAMPLES - 2});\nfloat cf=cp-(float)ci;\n'
    evidence = {}
    theta = '0.0'
    if rate:
        rows = [r[0] for r in domain_rows(rate)]
        cumulative = [[0.0]]
        for left, right in zip(rows, rows[1:]):
            cumulative.append([cumulative[-1][0] + (left + right) * .5 / (SAMPLES - 1)])
        code += ref.curve_hlsl('integratedRate', cumulative)
        theta = f'integratedRate*{ref.f(lifetime * math.tau)}'
        evidence['rotationRate'] = {'binding': rate['semanticBinding'], 'reconstructedEndDegrees': cumulative[-1][0] * lifetime * 360}
    if angle:
        code += ref.curve_hlsl('angleDegrees', domain_rows(angle))
        theta = f'({theta})+radians(angleDegrees)'
        evidence['rotationVectorZ'] = {'binding': angle['semanticBinding'], 'units': 'degrees reconstruction'}
    code += f'float theta={theta};\n'
    if scale:
        code += ref.curve_hlsl('scaleFactor', domain_rows(scale))
        code += 'float3 scaled=float3(P.x*scaleFactor.x,P.y*scaleFactor.y,P.z*scaleFactor.z);\n'
        evidence['scaleFactor'] = {'binding': scale['semanticBinding']}
    else:
        code += 'float3 scaled=P;\n'
    code += 'float sn=sin(theta);\nfloat cs=cos(theta);\n'
    code += 'float3 turned=float3(scaled.x*cs-scaled.y*sn,scaled.x*sn+scaled.y*cs,scaled.z);\nreturn turned-P;\n'
    return code, evidence


def texture_entry(name, package):
    png = EXPORTS / (package + '.png')
    dest = HERE / 'Textures' / (name + '.png')
    if name == ref.WHITE:
        if not dest.exists():
            ref.make_white(dest)
        return {'name': name, 'file': 'Textures/' + dest.name, 'srgb': True, 'substitute': True}
    if not dest.exists() or ref.sha(dest) != ref.sha(png):
        shutil.copy2(png, dest)
    srgb = None
    metadata = EXPORTS / (package + '.json')
    if metadata.exists():
        data = ref.read(metadata)
        objects = data if isinstance(data, list) else [data]
        srgb = next((o for o in objects if o.get('Type') == 'Texture2D'), {}).get('Properties', {}).get('SRGB')
    if srgb is None:
        srgb = '/Color/' in package
    return {'name': name, 'file': 'Textures/' + dest.name, 'srgb': bool(srgb), 'source': str(png), 'sha256': ref.sha(dest)}




def curve_range(layer, pattern, component=0):
    curve = find(layer, pattern)
    if not curve:
        return None
    values = [row[component] for row in curve['samples']]
    return [min(values), max(values)]


def sprite_emission(layer):
    """Reconstruction defaults for a sprite layer; ranges come from its exported curves when present."""
    lifetime = curve_range(layer, '.Lifetime.FloatCurve')
    # 有些 Lifetime 曲线是 0~1 的归一化系数而不是秒数（最小值 0 会让粒子寿命为 0），这种不用。
    if not lifetime or lifetime[0] <= 0.02:
        lifetime = None
    width = curve_range(layer, '.X.Sprite Size.')
    height = curve_range(layer, '.Y.Sprite Size.')
    radius = curve_range(layer, 'Large Radius.FloatCurve')
    sources = ['lifetime' if lifetime else None,
               'sprite size' if width and height else None, 'radius' if radius else None]
    return {
        'count': 12, 'lifetime': [round(lifetime[0], 4), round(lifetime[1], 4)] if lifetime else [0.3, 0.8],
        'sizeMin': [round(width[0], 3), round(height[0], 3)] if width and height else [20, 20],
        'sizeMax': [round(width[1], 3), round(height[1], 3)] if width and height else [45, 45],
        # 原作半径是系统单位；重建网格要乘 modelScale 3 才对上，所以这里除以 3 换算到局部单位。
        'radius': [round(radius[0] / 3, 3), round(radius[1] / 3, 3)] if radius else [0, 15],
        'speed': [20, 60], 'drag': 0, 'sizeFollowsScale': True, 'radialVelocity': True, 'arc': [0, 360], 'height': 8,
        'source': 'curve ranges for ' + (', '.join(s for s in sources if s) or 'nothing') + '; everything else generic burst defaults',
    }


def is_distortion(material):
    return any('Distortion' in (entry.get('asset') or '') for entry in material['chainLeafToRoot'])


def dynamic_shader(material, layer, sprite):
    """ref.shader() plus the layer's original Dynamic Material Parameter curves, baked as LUTs over normalizedAge.

    Which parameter feeds which input is a reconstruction guess (the master material graph is not exported),
    chosen from the curve shapes and the MI switches Base_bClamp / Mask_bClamp:
    Index 1 Param 1 = remaining amount (curves run 1 -> 0, dissolve progress = 1 - value); Index 0 Param 2 / Param 3 = base UV U / V offset;
    Index 0 Param 1 = mask UV V offset. Offsets scroll (wrap); cutting clamped textures outside 0..1 (CLAMP_OFFSET_UV) hid most layers, so it is off.
    Layers without any of these curves keep the age-based fade and dissolve of ref.shader().
    """
    code = ref.shader(material, resampled_layer(layer), sprite)
    curves = {(i, n): find(layer, f'.Index {i} Param {n}.') for i in (0, 1) for n in (1, 2, 3)}
    if not any(curves.values()):
        return code, []
    luts, used = '', []
    for (i, n), curve in curves.items():
        name = f'dyn{i}p{n}'
        if curve:
            luts += ref.curve_hlsl(name, domain_rows(curve))
            used.append(curve['semanticBinding'])
        else:
            luts += f'float {name}=0.0;\n'

    def replace(anchor, text):
        nonlocal code
        assert code.count(anchor) == 1, anchor
        code = code.replace(anchor, text)

    replace('float2 baseUv', '// Original Dynamic Material Parameter curves (mapping is reconstruction, see prepare_attack.py).\n' + luts + 'float2 baseUv')
    noise = 'float2 noise=(Texture2DSample(TexNoise,TexNoiseSampler,noiseUv).rg-0.5);\n'
    replace(noise, 'baseUv += float2(dyn0p2, dyn0p3);\nmaskUv.y += dyn0p1;\n' + noise)
    # 按 bClamp 在 0..1 外裁掉会让偏移 ±1 时整张贴图移出去、大部分时间看不见（实测第三、五段的网格层几乎全消失），所以默认不裁，只做滚动。
    if CLAMP_OFFSET_UV and ref.known(material, 'scalars', 'Base_bClamp', 0):
        anchor = 'float4 base=Texture2DSample(TexBase,TexBaseSampler,baseUv);\n'
        replace(anchor, anchor + 'base *= (all(baseUv >= 0.0) && all(baseUv <= 1.0)) ? 1.0 : 0.0;\n')
    if CLAMP_OFFSET_UV and ref.known(material, 'scalars', 'Mask_bClamp', 0):
        anchor = 'float4 mask=Texture2DSample(TexMask,TexMaskSampler,maskUv);\n'
        replace(anchor, anchor + 'mask *= (all(maskUv >= 0.0) && all(maskUv <= 1.0)) ? 1.0 : 0.0;\n')
    if curves[(1, 1)]:
        # 溶解进度由原作曲线驱动；只在寿命最后一小段兜底淡出，避免曲线没走完时突然消失。
        replace('float fadeIn=smoothstep(0.0,0.04,a);\nfloat fadeOut=1.0-smoothstep(0.55,1.0,a);\n',
                'float fadeIn=smoothstep(0.0,0.04,a);\nfloat fadeOut=1.0-smoothstep(0.85,1.0,a);\n')
        replace('float dissolveCut=smoothstep(0.60,1.0,a);\nfloat breakup=1.0-smoothstep(dissolveValue+0.08,dissolveValue+0.33,dissolveCut);\n',
                'float dissolveCut=saturate(1.0-dyn1p1);\nfloat breakup=1.0-smoothstep(dissolveValue-0.1,dissolveValue+0.1,dissolveCut*1.2);\n')
    return code, used


def build_systems(attack, parameters, textures, meshes, skipped):
    materials = {m['leaf']: m for m in parameters['materials']}
    systems = []
    for system in parameters['systems']:
        source = system['asset'].rsplit('/', 1)[1]
        name = CONFIG[attack]['systems'][source]
        layers = []
        for layer in system['layers']:
            if not layer['enabled']:
                continue
            for renderer in layer.get('renderers', []):
                if renderer is None:
                    continue
                rp = renderer['properties']
                if renderer['type'] not in ('NiagaraSpriteRendererProperties', 'NiagaraMeshRendererProperties'):
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': f'{renderer["type"]} not supported by the builder'})
                    continue
                sprite = renderer['type'] == 'NiagaraSpriteRendererProperties'
                matref = rp['Material'] if sprite else rp['OverrideMaterials'][0]['ExplicitMat']
                matpath = matref['ObjectPath'].rsplit('.', 1)[0].replace('Client/Content/', '/Game/')
                material = materials[matpath]
                if is_distortion(material):
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': 'distortion (refraction) material not reconstructed'})
                    continue
                definition = {'name': layer['handle'], 'sprite': sprite, 'sortOrder': rp.get('SortOrderHint', 0),
                              'pivot': [rp.get('PivotOffset', {}).get(k, 0) for k in ('X', 'Y', 'Z')],
                              'sourceMaterial': matpath, 'sourceSystem': system['asset'], 'sourceEmitter': layer['instance']}
                if sprite and matpath in REUSED_SPRITE_MATERIALS:
                    definition['id'] = REUSED_SPRITE_MATERIALS[matpath]
                    definition['reconstruction'] = 'Same MI as the first attack: reuses its reconstructed material.'
                    if attack != 'Attack03':
                        definition['emission'] = sprite_emission(layer)
                    layers.append(definition)
                    continue
                if not sprite:
                    mesh = rp['ParticleMesh']['ObjectPath'].rsplit('.', 1)[0].split('/')[-1]
                    if not (HERE / 'Geometry' / (mesh + '.json')).exists():
                        skipped.append({'system': name, 'layer': layer['handle'], 'reason': f'geometry {mesh} not exported yet'})
                        continue
                definition['id'] = f'Reference_{attack}_{name.replace("NS_Reference_" + attack + "_", "")}_{layer["handle"]}'
                bindings = {}
                for key in ('Base', 'Second', 'Noise', 'Mask', 'Dissolve'):
                    texture = ref.known(material, 'textures', key, '/Game/Aki/Render/Common/' + ref.WHITE)
                    texture_name = texture.rsplit('/', 1)[1]
                    bindings['Tex' + key] = texture_name
                    if texture_name not in textures:
                        textures[texture_name] = texture_entry(texture_name, texture.replace('/Game/', 'Client/Content/'))
                definition['textures'] = bindings
                if attack == 'Attack03' and not DYNAMIC_PARAMETERS_IN_ATTACK03:
                    definition['shader'] = ref.shader(material, resampled_layer(layer), sprite)
                else:
                    definition['shader'], dynamic = dynamic_shader(material, layer, sprite)
                    if dynamic:
                        definition['dynamicParameterCurves'] = dynamic
                definition['reconstruction'] = 'Shader wiring, lifetime, intensity, fade/dissolve are authored; colour/alpha LUTs resampled over their exported domain on normalizedAge.'
                if sprite:
                    definition['intensity'] = .8
                    definition['wpo'] = ''
                    if attack != 'Attack03':
                        definition['emission'] = sprite_emission(layer)
                else:
                    meshes.add(mesh)
                    definition['mesh'] = mesh
                    definition['lifetime'] = LIFETIME[kind(layer['handle'])]
                    definition['intensity'] = INTENSITY[kind(layer['handle'])]
                    definition['wpo'], motion = mesh_wpo(layer, definition['lifetime'])
                    if motion:
                        definition['motionReconstruction'] = motion
                layers.append(definition)
        entry = {'name': name, 'source': system['asset'], 'generate': True, 'layers': layers}
        location = system['dataAssetExplicitProperties'].get('Location', {}).get('Constant')
        if location:
            entry['modelLocation'] = [location.get(k, 0.0) for k in ('X', 'Y', 'Z')]
        systems.append(entry)
    return systems


def yaw_keys(rotation, scale):
    keys = rotation['Curve[2]']['EditorCurveData']['Keys']
    return [{'time': k['Time'] * scale, 'value': k['Value'],
             'arriveTangent': k['ArriveTangent'] / scale, 'leaveTangent': k['LeaveTangent'] / scale} for k in keys]


def main(attack):
    config = CONFIG[attack]
    parameters = ref.read(HERE / f'Evidence/LayerParameters_{attack}.json')
    evidence = next(a for a in ref.read(HERE / 'Evidence/AttackFxReference.json')['attacks'] if a['projectAttack'] == attack)
    textures, meshes, skipped = {}, set(), []
    systems = build_systems(attack, parameters, textures, meshes, skipped)
    buildable = {s['name'] for s in systems if s['layers']}
    das = {config['systems'][s['asset'].rsplit('/', 1)[1]]: s['dataAssetExplicitProperties'] for s in parameters['systems']}
    attack01 = {p['system']: p for p in ref.read(HERE / 'Attack01.json')['placements']}

    events = [dict(e, state=False) for e in evidence['sourceEvents'] if e['included']]
    events += [dict(e, authoredSeconds=e['authoredBeginSeconds'], state=True) for e in evidence.get('sourceSequenceEffects', []) if e['included']]
    first = min(e['authoredSeconds'] for e in events)
    scale = evidence['timelineScale']
    placements, unplaced = [], []
    for event in sorted(events, key=lambda e: e['authoredSeconds']):
        group = event['effectAsset'].rsplit('/', 1)[1].split('.', 1)[0]
        # 原作蒙太奇里的引用和组文件名大小写不一定一致（..._Dgw / ..._DgW）。
        declared = next((v for k, v in config['groups'].items() if k.lower() == group.lower()), [])
        children = [(n, d) for n, d in declared if n in buildable or n.startswith('NS_Reference_Attack01_')]
        if not children:
            unplaced.append({'sourceNotify': event['notifyObject'], 'sourceEffect': group, 'reason': 'no buildable child effect'})
            continue
        for name, child_delay in children:
            placement = {
                'system': name,
                'location': [event['location'][k] for k in ('X', 'Y', 'Z')] if event['location'] else [0, 0, 0],
                'rotation': [event['rotation'][k] for k in ('Pitch', 'Yaw', 'Roll')] if event['rotation'] else [0, 0, 0],
                'scale': [event['scale'][k] for k in ('X', 'Y', 'Z')] if event['scale'] else [1, 1, 1],
                'delay': (event['authoredSeconds'] - first) * scale + child_delay,
                # 原作 AnimNotifyEffect.Attached 默认 true；显式写了 false 的不跟随角色。
                'attached': event.get('attachedExplicit') is not False,
                'socket': event.get('socketName') or 'Root',
                'sourceNotify': event['notifyObject'], 'sourceSeconds': event['authoredSeconds'], 'sourceEffect': group,
            }
            if event['state']:
                placement['sourceStateDurationSeconds'] = event['durationSeconds']
            if name in attack01 and 'localYawKeys' in attack01[name]:
                placement['localYawKeys'] = attack01[name]['localYawKeys']
                placement['localYawSource'] = attack01[name]['localYawSource']
            rotation = das.get(name, {}).get('Rotation')
            if rotation and rotation.get('bUseCurve'):
                placement['localYawKeys'] = yaw_keys(rotation, scale)
                placement['localYawSource'] = 'Original DA Rotation.Curve[2], timeline-scaled; axis mapping to local yaw is reconstruction.'
            placements.append(placement)

    recipe = {
        'schemaVersion': 1, 'description': config['description'],
        'notifyAuthoredSeconds': first * scale,
        'textures': sorted(textures.values(), key=lambda t: t['name']), 'meshes': sorted(meshes),
        'systems': systems, 'placements': placements, 'skippedLayers': skipped, 'unplacedEvents': unplaced,
        'reconstructionChoices': [
            'Colour / alpha / motion LUTs resampled over their exported LUT domain on normalizedAge (first attack ignored domains).',
            'Mesh WPO: Rotation Rate integrated as turns/second about local +Z, Z Rotation Vector in degrees, Scale Factor on local axes; all reconstruction.',
            'Scale RGBA colour curve is used as the colour curve when a layer has no Color curve.',
            'Lifetime / intensity per layer by analogy with the first attack; not in the cooked export.',
            'Sprite emission: exported Lifetime / Sprite Size / Large Radius curve ranges where present, otherwise generic burst defaults; AttackNNTuning.json overrides.',
            'Original groups with several children become one placement per supported child; state effects (AnimNotifyStateEffect) are placed at their begin time.',
            'Dynamic Material Parameter curves are baked into the shader: Index 1 Param 1 = remaining amount (dissolve = 1 - value), Index 0 Param 2/3 = base UV U/V offset, Index 0 Param 1 = mask UV V offset; offsets scroll without clipping. Mapping chosen from curve shapes and MI switches, not recovered from the master material.',
        ],
    }
    if attack == 'Attack03':
        # 第三段的生成结果在加入组展开 / 插槽 / 状态特效之前就定下了，保持它的字段不变。
        recipe.pop('unplacedEvents')
        recipe['reconstructionChoices'] = recipe['reconstructionChoices'][:4] + [
            'Sprite layers with the first attack\'s MIs reuse its reconstructed materials; their emission comes from Attack03Tuning.json.']
        for placement in placements:
            placement.pop('socket')
    (HERE / f'{attack}.json').write_text(json.dumps(recipe, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'recipe': f'{attack}.json', 'textures': len(textures), 'meshes': sorted(meshes),
                      'layers': {s['name']: [l['name'] for l in s['layers']] for s in systems},
                      'placements': [(p['system'].replace('NS_Reference_', ''), round(p['delay'], 3), p['attached'], p.get('socket')) for p in placements],
                      'skipped': skipped, 'unplaced': unplaced}, ensure_ascii=False, indent=1))


if __name__ == '__main__':
    main(sys.argv[1])
