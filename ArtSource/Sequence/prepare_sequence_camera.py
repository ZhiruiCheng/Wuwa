"""原作技能特写镜头（LevelSequence，SequenceCamera/Fight/<角色>/SC_*）→ 项目 LevelSequence 的重建配方。
  python -I prepare_sequence_camera.py SC_Changli_Burst01 [--role Changli]

原作播放方式（Camera/SequenceCameraPlayerComponent.js PlayCameraSequence）：
  - 用 LevelSequenceActor 播整段序列；Tag 为 SequenceCamera 的绑定换成游戏里的 CineCamera，Tag 为 Role 的绑定换成放技能的角色；
  - 播放原点 = 挂在角色 Mesh 的 CameraAttachSocket（大招是 Root）上的一个 Actor；
  - 播到 min(序列结束, SpecificSequenceTime) 结束，按 BlendOutTime 混回战斗镜头；IsHideHud 时隐藏战斗 UI HideHudTime 秒。
这一版重建镜头本身（第一阶段）：
  - 相机：CineCamera 生成物（Tag SequenceCamera），挂到 Role 绑定的 CharacterMesh0 / Root 上（原作 3DAttach 区段），
    逐帧的位置 / 旋转关键帧原样（时间单位都是 30 帧 / 秒的帧号）；
  - 镜头参数：原作动画的是 FieldOfView（Kuro 引擎改过的 CineCamera 认 FOV），UE 的 CineCamera 每帧用焦距算 FOV，
    所以按原作相机的胶片尺寸（默认 23.76 mm 宽，原作模板 FOV 43.85° ↔ 焦距 29.52 mm 互相印证）把 FOV 换成焦距：
    焦距 = 11.88 / tan(FOV / 2)；光圈、手动对焦距离原样。
  - Role：可占有绑定（Tag Role），运行时换成放技能的角色。
没有重建、列在 notes 里的：序列里的 Niagara 生成物（要先用特效管线建出对应系统）、Kuro 后处理体积（径向模糊等 Kuro 专有参数）、
点光源、角色材质参数轨道、BP_Cinematics_Tick / BP_LevelSequenceHalo（Kuro 蓝图）、预览用的角色骨骼网格。

输出 <序列名>.sequence.json（原作数据，不提交）→ -run=WuwaSequenceCameraSetup -Recipe=... [-Apply]
"""
from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
SENSOR_WIDTH = 23.76   # UE 默认胶片宽度（mm），与原作相机模板的 FOV / 焦距一致
INTERP = {0: 'Linear', 1: 'Constant', 2: 'Cubic', 3: 'None'}


def ensure_json(package: str) -> Path:
    path = AKI / f'{package}.json'
    if not path.exists():
        subprocess.run([str(PAKEXPORT), 'json', f'Client/Content/Aki/{package}.uasset'], capture_output=True, text=True)
    if not path.exists():
        raise SystemExit(f'PakExport could not export {package}')
    return path


def frame(bound) -> int:
    return int(bound['Value']['Value'])


def channel(raw: dict, convert=None) -> dict:
    """FMovieSceneFloat/DoubleChannel → {times, keys, default, pre, post}；convert 把值换算（切线改成自动，由 UE 重算）。"""
    keys = []
    for value in raw.get('Values', []):
        tangent = value.get('Tangent', {})
        key = {'value': float(value['Value']), 'interp': INTERP.get(int(value.get('InterpMode', 2)), 'Cubic'),
               'tangentMode': int(value.get('TangentMode', 0)),
               'arrive': float(tangent.get('ArriveTangent', 0.0)), 'leave': float(tangent.get('LeaveTangent', 0.0)),
               'arriveWeight': float(tangent.get('ArriveTangentWeight', 0.0)), 'leaveWeight': float(tangent.get('LeaveTangentWeight', 0.0)),
               'weightMode': int(tangent.get('TangentWeightMode', 0))}
        if convert:
            key.update({'value': convert(key['value']), 'tangentMode': 0, 'arrive': 0.0, 'leave': 0.0})   # 0 = Auto
        keys.append(key)
    default = raw.get('DefaultValue')
    return {'times': [int(t['Value']) for t in raw.get('Times', [])], 'keys': keys,
            'default': (convert(default) if default else None) if convert else default,
            'pre': raw.get('PreInfinityExtrap', 'RCCE_Constant'), 'post': raw.get('PostInfinityExtrap', 'RCCE_Constant')}


def fov_to_focal(fov: float) -> float:
    return (SENSOR_WIDTH / 2.0) / math.tan(math.radians(fov) / 2.0)


def handle_niagara_track(entry: dict, kind: str, props: dict, get_path, names: dict, camera_guid: str, role_guids: set, notes: list) -> bool:
    """序列里 Niagara 生成物（及其 NiagaraComponent0 子绑定）的轨道 → 配方条目。返回 True 表示已处理。"""
    sections = [get_path(r)['Properties'] for r in props.get('Sections', props.get('ConstraintSections', []))]
    if kind == 'MovieSceneSpawnTrack' and sections:
        curve = sections[0].get('BoolCurve', {})
        entry['spawn'] = {'default': bool(curve.get('DefaultValue', True)) if curve.get('DefaultValue') is not None else True,
                          'times': [int(t['Value']) for t in curve.get('Times') or []], 'values': [bool(v) for v in curve.get('Values') or []]}
        return True
    if kind == 'MovieScene3DTransformTrack' and sections:
        section = sections[0]
        entry['transform'] = [channel(section[k]) for k in
                              ('Translation', 'Translation[1]', 'Translation[2]', 'Rotation', 'Rotation[1]', 'Rotation[2]', 'Scale', 'Scale[1]', 'Scale[2]')]
        entry['transformSource'] = 'transform track'
        return True
    if kind == 'MovieScene3DAttachTrack' and sections:
        section = sections[0]
        target = section['ConstraintBindingID']['Guid']
        kind_of = 'camera' if target == camera_guid else 'role' if target in role_guids else None
        if kind_of is None:
            notes.append(f"{entry['name']}: attach target {names.get(target, target)} not rebuilt")
            return True
        entry['attach'] = {'target': kind_of, 'socket': section.get('AttachSocketName') or '', 'component': section.get('AttachComponentName') or '',
                           'start': frame(section['SectionRange']['Value']['LowerBound']), 'end': frame(section['SectionRange']['Value']['UpperBound'])}
        return True
    if kind == 'MovieSceneNiagaraSystemTrack' and sections:
        section = sections[0]
        end_behavior = str(section.get('SectionEndBehavior', 'ENiagaraSystemSpawnSectionEndBehavior::SetSystemInactive')).rsplit('::', 1)[-1]
        entry['activation'] = {'start': frame(section['SectionRange']['Value']['LowerBound']), 'end': frame(section['SectionRange']['Value']['UpperBound']),
                               'endBehavior': end_behavior}
        return True
    if kind == 'MovieSceneNiagaraFloatParameterTrack':
        notes.append(f"{entry['name']}: Niagara float parameter {props.get('Parameter', {}).get('Name')} not rebuilt")
        return True
    return False


# Kuro 后处理体积（SceneColorTint / BurstDissolveThreshold / Kuro 径向模糊 / 特效暗角）→ 项目电影后处理的 MPC 参数
# （材质见 WuwaSequenceCameraSetupCommandlet：环境压黑走 BeforeDOF + 角色 CustomDepth 遮罩，径向模糊 + 暗角走色调映射后）
POST_PROCESS_PARAMETERS = {
    'Settings.SceneColorTint': [('SceneTint', 'RedCurve')],   # 原作 RGB 三条曲线相同（白 → 黑 → 白）
    'GISetting.MiscSetting.BurstDissolveThreshold': [('BurstDissolve', 'FloatCurve')],
    'Settings.KuroRadialBlurIntensity': [('RadialBlurIntensity', 'FloatCurve')],
    'Settings.KuroRadialBlurRadius': [('RadialBlurRadius', 'FloatCurve')],
    'Settings.KuroRadialBlurHardness': [('RadialBlurHardness', 'FloatCurve')],
    'Settings.KuroRadialBlurCenter': [('RadialBlurCenterX', 'Curves'), ('RadialBlurCenterY', 'Curves[1]')],
    'Settings.EffectVignetteIntensity': [('EffectVignette', 'FloatCurve')],
}
POST_PROCESS_COLLECTION = '/Game/Characters/SequenceCamera/Common/MPC_WuwaCinematic'

# 序列里的原作静态网格 → 项目资产（build_sequence_meshes.py 从原件建：网格 FBX、贴图、重建的材质）
STATIC_MESHES = {
    'SM_VortexCloud': {
        'mesh': '/Game/Characters/SequenceCamera/2X/2_7/Jiabeilina/VortexCloud/SM_VortexCloud',
        'materials': {'MI_VortexCloud_001_MP': '/Game/Characters/SequenceCamera/2X/2_7/Jiabeilina/VortexCloud/MI_VortexCloud_001'},
    },
}


def handle_post_process_track(post: dict, label: str, kind: str, props: dict, get_path, notes: list) -> bool:
    """环境 / 屏幕后处理那个 Kuro 后处理体积的轨道 → post['parameters']。返回 True 表示已处理。"""
    path = props.get('PropertyBinding', {}).get('PropertyPath', '')
    if kind == 'MovieSceneSpawnTrack' or (kind == 'MovieScene3DTransformTrack'):
        return True   # 无界体积：生成 / 变换无意义
    mapping = POST_PROCESS_PARAMETERS.get(path)
    if not mapping or not props.get('Sections'):
        return False
    section = get_path(props['Sections'][0])['Properties']
    for name, key in mapping:
        if key in section:
            post['parameters'][name] = channel(section[key])
    post['sources'].append(f'{label}: {path}')
    return True


# Kuro 第二个后处理体积的角色 GI 主光颜色 → 项目卡通材质的 MPC_WuwaToon.CharacterLightTint（M_Toon_Body 是无光照卡通，乘在主光颜色上）
TOON_COLLECTION = '/Game/Characters/Common/Toon/MPC_WuwaToon'


def eval_linear(curve: dict, t: float) -> float:
    """Kuro 浮点曲线按关键帧分段线性取值（前后常量外推；没关键帧取默认值）。"""
    times = [int(x['Value']) for x in curve.get('Times', [])]
    values = [float(v['Value']) for v in curve.get('Values', [])]
    if not times:
        return float(curve.get('DefaultValue') or 0.0)
    if t <= times[0]:
        return values[0]
    for i in range(1, len(times)):
        if t <= times[i]:
            f = (t - times[i - 1]) / max(times[i] - times[i - 1], 1)
            return values[i - 1] + (values[i] - values[i - 1]) * f
    return values[-1]


def bake_toon_main_light(color_section: dict, weight_curve: dict | None, end: int) -> list:
    """CharacterMainLightColor 按体积 BlendWeight 混回中性白（1,1,1），在两者的关键帧上取值，分段线性（重建选择）。"""
    curves = [color_section[k] for k in ('RedCurve', 'GreenCurve', 'BlueCurve')]
    times = sorted({int(x['Value']) for c in curves + ([weight_curve] if weight_curve else []) for x in c.get('Times', [])} | {0, end})
    channels = []
    for c in curves:
        keys = []
        for t in times:
            w = eval_linear(weight_curve, t) if weight_curve else 1.0
            v = 1.0 + (eval_linear(c, t) - 1.0) * w
            keys.append({'value': v, 'interp': 'Linear', 'tangentMode': 0, 'arrive': 0.0, 'leave': 0.0,
                         'arriveWeight': 0.0, 'leaveWeight': 0.0, 'weightMode': 0})
        channels.append({'times': times, 'keys': keys, 'default': 1.0, 'pre': 'RCCE_Constant', 'post': 'RCCE_Constant'})
    channels.append({'times': [], 'keys': [], 'default': 1.0, 'pre': 'RCCE_Constant', 'post': 'RCCE_Constant'})
    return channels


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('sequence', help='原作序列名，例如 SC_Changli_Burst01')
    parser.add_argument('--role', default='Changli', help='SequenceCamera/Fight 下的角色目录')
    parser.add_argument('--effects', default=None, help='特效管线的角色名（/Game/Effects/<它>/Reference，默认同 --role，嘉贝是 Jiabei）')
    args = parser.parse_args()
    package = f'Character/SequenceCamera/Fight/{args.role}/{args.sequence}'
    effects = args.effects or args.role
    objects = json.loads(ensure_json(package).read_text(encoding='utf-8'))
    by_name = {o['Name']: o for o in objects if 'Name' in o}

    def get(ref: dict) -> dict:
        return by_name[ref['ObjectName'].split(':')[-1].rstrip("'").split('.')[-1]]

    def get_path(ref: dict) -> dict:
        """同名对象（MovieScene3DTransformSection_0 等）按完整路径区分：取 Outer 链匹配的那个。"""
        path = ref['ObjectName'].split(':')[-1].rstrip("'")
        parts = path.split('.')
        for o in objects:
            if o.get('Name') != parts[-1]:
                continue
            outer = o.get('Outer', {}).get('ObjectName', '')
            if len(parts) < 2 or outer.split(':')[-1].rstrip("'").endswith(parts[-2]):
                return o
        raise KeyError(path)

    scene = next(o for o in objects if o['Type'] == 'MovieScene')['Properties']
    groups = {g['Key']: [i['Guid'] for i in g['Value']['IDs']] for g in scene.get('BindingGroups', [])}
    names = {s['Guid']: s['Name'] for s in scene.get('Spawnables', []) + scene.get('Possessables', [])}
    parents = {p['Guid']: p.get('ParentGuid') for p in scene.get('Possessables', [])}
    # 绑定组里可能留着已删绑定的 Guid（嘉贝大招 SequenceCamera 组第一个就是）：取第一个还存在的
    camera_guid = next(g for g in groups['SequenceCamera'] if g in names)
    role_guids = set(groups.get('Role', []))
    bindings = {b['ObjectGuid']: b for b in scene.get('ObjectBindings', [])}

    camera = {}
    notes = []
    niagara = {}   # 生成物 Guid -> 配方条目
    spawnable_templates = {s['Guid']: s.get('ObjectTemplate', {}).get('ObjectName', '') for s in scene.get('Spawnables', [])}
    for guid, template_ref in spawnable_templates.items():
        if 'NiagaraActor' not in template_ref:
            continue
        actor_name = template_ref.split('.')[-1].rstrip("'")
        component = next((o for o in objects if o.get('Type') == 'NiagaraComponent'
                          and o.get('Outer', {}).get('ObjectName', '').rstrip("'").endswith('.' + actor_name)), None)
        props = (component or {}).get('Properties', {})
        system = props.get('Asset', {}).get('ObjectName', '').split("'")[1] if props.get('Asset') else None
        rel = lambda key, axes, default: [float(props.get(key, {}).get(a, default)) for a in axes]
        loc, rot, scl = rel('RelativeLocation', 'XYZ', 0.0), rel('RelativeRotation', ('Roll', 'Pitch', 'Yaw'), 0.0), rel('RelativeScale3D', 'XYZ', 1.0)
        constant = lambda v: {'times': [], 'keys': [], 'default': v, 'pre': 'RCCE_Constant', 'post': 'RCCE_Constant'}
        niagara[guid] = {'name': names.get(guid, guid), 'sourceSystem': system,
                         # 特效管线（ArtSource/JiabeiAttack，阶段 Burst01_Sequence）按原名建的重建系统
                         'system': f'/Game/Effects/{effects}/Reference/Niagara/NS_Reference_{effects}_'
                                   + system.removeprefix(f'NS_Fx_{effects}_').removeprefix('NS_Fx_') if system else None,
                         'transform': [constant(v) for v in loc + rot + scl], 'transformSource': 'component template'}
    # 环境 / 屏幕后处理的 Kuro 后处理体积（带 SceneColorTint 等轨道的那个；另一个只调角色 GI 颜色，暂不重建）
    # 直接摆在序列里的静态网格（嘉贝大招的漩涡云）：网格 / 材质按 STATIC_MESHES 换成项目资产（由 build_sequence_meshes.py 建）；
    # 生成 / 变换 / 挂接轨道和 Niagara 生成物同一套处理
    static_meshes = {}
    for guid, template_ref in spawnable_templates.items():
        if "StaticMeshActor'" not in template_ref:
            continue
        actor_name = template_ref.split('.')[-1].rstrip("'")
        component = next((o for o in objects if o.get('Type') == 'StaticMeshComponent'
                          and o.get('Outer', {}).get('ObjectName', '').rstrip("'").endswith('.' + actor_name)), None)
        props = (component or {}).get('Properties', {})
        mesh = props.get('StaticMesh', {}).get('ObjectName', '').split("'")[1] if props.get('StaticMesh') else None
        materials = [m.get('ObjectName', '').split("'")[1] for m in props.get('OverrideMaterials', []) if m]
        if mesh not in STATIC_MESHES:
            notes.append(f"{names.get(guid, guid)}: static mesh {mesh} has no project mapping (STATIC_MESHES)")
            continue
        rel = lambda key, axes, default: [float(props.get(key, {}).get(a, default)) for a in axes]
        loc, rot, scl = rel('RelativeLocation', 'XYZ', 0.0), rel('RelativeRotation', ('Roll', 'Pitch', 'Yaw'), 0.0), rel('RelativeScale3D', 'XYZ', 1.0)
        constant = lambda v: {'times': [], 'keys': [], 'default': v, 'pre': 'RCCE_Constant', 'post': 'RCCE_Constant'}
        static_meshes[guid] = {'name': names.get(guid, guid), 'sourceMesh': mesh, 'sourceMaterials': materials,
                               'mesh': STATIC_MESHES[mesh]['mesh'],
                               'materials': [STATIC_MESHES[mesh]['materials'].get(m) for m in materials],
                               'transform': [constant(v) for v in loc + rot + scl], 'transformSource': 'component template'}
    post_volumes = {guid for guid, ref in spawnable_templates.items() if 'KuroPostProcessVolume' in ref}
    post = {'collection': POST_PROCESS_COLLECTION, 'parameters': {}, 'sources': [], 'roleCustomDepth': True}
    # 点光源生成物（强度轨道在 LightComponent0 子绑定上）。衰减方式按原作组件：导出的 JSON 只有非默认值，
    # 没写 bUseInverseSquaredFalloff 就是引擎默认的平方反比（LightFalloffExponent 只在旧式衰减下才用）。
    # 以前一律按旧式指数衰减建，强度 3 的 Point Light_Floor 在地面打出一大片橙红光斑（原作平方反比 + 无单位 3 几乎看不见）。
    lights = {}
    for guid, ref in spawnable_templates.items():
        if "PointLight'" not in ref:
            continue
        actor_name = ref.split('.')[-1].rstrip("'")
        component = next((o for o in objects if o.get('Type') == 'PointLightComponent'
                          and o.get('Outer', {}).get('ObjectName', '').rstrip("'").endswith('.' + actor_name)), {})
        cp = component.get('Properties', {})
        lights[guid] = {'name': names.get(guid, guid), 'attenuationRadius': float(cp.get('AttenuationRadius', 1000.0)),
                        'falloffExponent': float(cp.get('LightFalloffExponent', 8.0)),
                        'inverseSquared': bool(cp.get('bUseInverseSquaredFalloff', True)),
                        'intensity': {'times': [], 'keys': [], 'default': float(cp.get('Intensity', 0.0)), 'pre': 'RCCE_Constant', 'post': 'RCCE_Constant'},
                        'color': [1.0, 1.0, 1.0]}
        if cp.get('ToonLightType'):
            notes.append(f"{names.get(guid, guid)}: Kuro toon light parameters ({cp.get('ToonLightType')}, ToonIntensity {cp.get('ToonIntensity')}) not rebuilt (project toon is unlit)")
    toon = {'collection': TOON_COLLECTION, 'vectors': {}}
    toon_sources = {}
    for guid, binding in bindings.items():
        owner = guid if guid in names and guid not in parents else parents.get(guid)
        is_camera = guid == camera_guid or parents.get(guid) == camera_guid
        label = names.get(guid, guid)
        for ref in binding.get('Tracks', []):
            track = get_path(ref)
            kind = track['Type']
            props = track.get('Properties', {})
            if not is_camera:
                entry = niagara.get(owner) or static_meshes.get(owner)
                if entry is not None and handle_niagara_track(entry, kind, props, get_path, names, camera_guid, role_guids, notes):
                    continue
                if owner in post_volumes and handle_post_process_track(post, label, kind, props, get_path, notes):
                    continue
                path = props.get('PropertyBinding', {}).get('PropertyPath', '')
                if owner in post_volumes and path in ('GISetting.CharacterSetting.CharacterMainLightColor', 'BlendWeight'):
                    toon_sources[path] = get_path(props['Sections'][0])['Properties']
                    continue
                if owner in lights and kind == 'MovieScene3DTransformTrack':
                    section = get_path(props['Sections'][0])['Properties']
                    lights[owner]['transform'] = [channel(section[k]) for k in
                                                  ('Translation', 'Translation[1]', 'Translation[2]', 'Rotation', 'Rotation[1]', 'Rotation[2]', 'Scale', 'Scale[1]', 'Scale[2]')]
                    continue
                if owner in lights and path == 'Intensity':
                    lights[owner]['intensity'] = channel(get_path(props['Sections'][0])['Properties']['FloatCurve'])
                    continue
                if owner in lights and path == 'LightColor':
                    section = get_path(props['Sections'][0])['Properties']
                    lights[owner]['color'] = [float(section[k].get('DefaultValue') or 0.0) for k in ('RedCurve', 'GreenCurve', 'BlueCurve')]
                    continue
                if owner in lights and kind == 'MovieSceneSpawnTrack':
                    continue
                if props.get('bIsEvalDisabled'):
                    notes.append(f'{label}: {kind} {path} disabled in the original (skipped)')
                    continue
                if kind != 'MovieSceneSpawnTrack':
                    notes.append(f'{label}: {kind} {props.get("PropertyBinding", {}).get("PropertyPath", "")} not rebuilt')
                continue
            if kind == 'MovieScene3DTransformTrack':
                section = get_path(props['Sections'][0])['Properties']
                camera['transform'] = [channel(section[k]) for k in
                                       ('Translation', 'Translation[1]', 'Translation[2]', 'Rotation', 'Rotation[1]', 'Rotation[2]', 'Scale', 'Scale[1]', 'Scale[2]')]
            elif kind == 'MovieScene3DAttachTrack':
                section = get_path(props['ConstraintSections'][0])['Properties']
                target = section['ConstraintBindingID']['Guid']
                if target not in role_guids:
                    notes.append(f'camera attach target {names.get(target, target)} is not the Role binding; not rebuilt')
                    continue
                socket = section.get('AttachSocketName', '')
                if socket == 'Root':
                    # 项目的 2.0 骨架 Root 骨骼带 FBX 包装骨的 ×100 缩放（Root 插槽世界缩放 100），挂上去相机的相对位移会被放大 100 倍。
                    # Root 与 Mesh 组件位置 / 朝向一致（根运动已提取），直接挂组件等价（项目差异）。
                    notes.append('camera attach socket Root -> mesh component (project skeleton Root has x100 scale)')
                    socket = ''
                camera['attach'] = {'socket': socket, 'component': section.get('AttachComponentName', ''),
                                    'start': frame(section['SectionRange']['Value']['LowerBound']),
                                    'end': frame(section['SectionRange']['Value']['UpperBound'])}
            elif kind == 'MovieSceneFloatTrack':
                path = props['PropertyBinding']['PropertyPath']
                curve = get_path(props['Sections'][0])['Properties'].get('FloatCurve', {})
                if path == 'FieldOfView':
                    camera['focalLength'] = channel(curve, fov_to_focal)
                    camera['fieldOfViewSource'] = channel(curve)
                elif path == 'CurrentAperture':
                    camera['aperture'] = channel(curve)
                elif path == 'FocusSettings.ManualFocusDistance':
                    camera['focusDistance'] = channel(curve)
                else:
                    notes.append(f'camera float track {path} not rebuilt')
            elif kind == 'MovieSceneSpawnTrack':
                section = get_path(props['Sections'][0])['Properties']
                camera['spawnDefault'] = bool(section.get('BoolCurve', {}).get('DefaultValue', True))
            else:
                notes.append(f'camera {kind} not rebuilt')
    playback = scene['PlaybackRange']['Value']
    if 'GISetting.CharacterSetting.CharacterMainLightColor' in toon_sources:
        weight = toon_sources.get('BlendWeight', {}).get('FloatCurve')
        toon['vectors']['CharacterLightTint'] = bake_toon_main_light(toon_sources['GISetting.CharacterSetting.CharacterMainLightColor'], weight, frame(playback['UpperBound']))
        notes.append('Kuro CharacterAmbientColor / CharacterRimColor not rebuilt (project toon has no ambient / rim inputs); CharacterMainLightColor baked x BlendWeight onto MPC_WuwaToon.CharacterLightTint (multiplies the sun-driven MainLightColor)')
    recipe = {
        'source': f'Client/Content/Aki/{package}',
        'asset': f'/Game/Characters/SequenceCamera/{args.role}/{args.sequence}',
        'tickResolution': int(scene['TickResolution']['Numerator']), 'displayRate': int(scene['DisplayRate']['Numerator']),
        'playbackStart': frame(playback['LowerBound']), 'playbackEnd': frame(playback['UpperBound']),
        'sensorWidth': SENSOR_WIDTH,
        'camera': camera,
        'niagara': [e for e in niagara.values() if e.get('system')],
        'staticMeshes': list(static_meshes.values()),
        'postProcess': post if post['parameters'] else None,
        'toonLight': toon if toon['vectors'] else None,
        'lights': [l for l in lights.values() if 'transform' in l],
        'notes': sorted(set(notes)),
    }
    out = HERE / f'{args.sequence}.sequence.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    fov = camera.get('fieldOfViewSource', {}).get('keys', [])
    print(f'{out.name}: {recipe["playbackEnd"] - recipe["playbackStart"]} frames @ {recipe["displayRate"]} fps, '
          f'camera keys {len(camera.get("transform", [{}])[0].get("times", []))}, attach {camera.get("attach")}, '
          f'FOV {fov[0]["value"] if fov else "-"}..{fov[-1]["value"] if fov else "-"} → focal '
          f'{camera["focalLength"]["keys"][0]["value"]:.1f}..{camera["focalLength"]["keys"][-1]["value"]:.1f} mm')
    for note in recipe['notes']:
        print('  note:', note)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
