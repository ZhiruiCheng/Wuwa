"""原作蒙太奇上的位移吸附 / 转向通知 → 项目蒙太奇上对应通知的安装配方。
  python -I prepare_motion_notifies.py --character changli

数据来源：原作蒙太奇（PakExport 导出的 JSON）上的
  - TsAnimNotifyStatePositionBranchTarget（位移吸附到目标位置）→ UWuwaAnimNotifyState_MoveToTarget（Config 里的字段）
  - TsAnimNotifyStateRotate（旋转到黑板目标或技能目标或输入方向）→ UWuwaAnimNotifyState_SkillRotate
  - TsAnimNotifyStateRoleRotate → UWuwaAnimNotifyState_RoleRotate
  - TsAnimNotifyStateKeepAwayFromGround（和地面保持距离）→ UWuwaAnimNotifyState_KeepAwayFromGround
导出的 JSON 只含非默认值；项目通知的默认值与原作一致，所以只写出现的字段。
项目还没有的配置（黑板目标、目标偏移、定向旋转、屏蔽标签、自定义曲线等）出现非默认值就报错，免得装上去行为不一样。
原作蒙太奇 → 项目蒙太奇的对应沿用子弹通知配方（ArtSource/Bullet/prepare_bullet_notifies.py）。

输出 <character>.motion_notifies.json，由 -run=WuwaMotionNotifySetup -Character=x [-Apply] 装到项目蒙太奇上。原作数据，不提交。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'Bullet'))
import prepare_bullet_notifies as bullets  # noqa: E402

# 原作类 → (项目通知类型, {原作字段: 项目字段}, 不支持的字段)
TYPES = {
    'TsAnimNotifyStatePositionBranchTarget_C': ('MoveToTarget', {
        'Distance': 'Distance', 'MaxSpeed': 'MaxSpeed', '是否永远面向目标': 'bLookAtTarget', '忽略Z轴方向': 'bIgnoreZ',
        '忽略水平方向': 'bIgnoreHorizontal', '永远修正Z轴': 'bAlwaysMoveZ', '忽略双方半径': 'bIgnoreRadius',
        '允许反向移动': 'bAllowReverse', '允许正向移动': 'bAllowForward', '最小吸附距离': 'MinDistance', '最大吸附距离': 'MaxDistance',
        # 世界坐标加到目标点上（OffsetByOrientation / OffsetByAbsolute 都没开时）
        'TargetOffset': 'TargetOffset',
    }, ['MoveCurve', 'IsShareTarget', 'TargetRotation', 'OffsetByOrientation', 'OffsetByAbsolute',
        'MinHeightFromTargetFloor', '黑板类型', '黑板值', '黑板目标Socket']),
    'TsAnimNotifyStateRotate_C': ('SkillRotate', {
        '旋转速度': 'RotateSpeed', '是否自动朝向目标': 'bAutoFaceTarget', '是否平滑旋转': 'bSmooth', '是否应用旋转偏移': 'bApplyOffset',
        '旋转偏移': 'Offset', '停止旋转阈值': 'PauseThreshold', '继续旋转阈值': 'ResumeThreshold',
    }, ['Curve', '设置为朝向黑板目标', '黑板类型', '朝向黑板目标名', '只在横板模式中生效', '定向旋转功能', '定向旋转阈值最小值',
        '定向旋转阈值最大值', '定向旋转方式', '调试定向旋转范围', '屏蔽标签列表']),
    'TsAnimNotifyStateRoleRotate_C': ('RoleRotate', {
        '旋转速度': 'RotateSpeed', '是否自动朝向目标': 'bAutoFaceTarget',
    }, ['TagContainer', '只在横板模式中生效']),
    # 角色和地面保持一定距离（心眼·跃从地面离地、空中普攻 4 保持高度）
    'TsAnimNotifyStateKeepAwayFromGround_C': ('KeepAwayFromGround', {
        '距离水平面最小高度': 'MinHeight', '距离水平面最大高度': 'MaxHeight', 'MaxSpeed': 'MaxSpeed', '不使用默认插值曲线': 'bIgnoreInterpolation',
    }, ['MoveCurve']),
    # 大招：动画和子弹冻结（时停）、角色放大招（不能切人 / 无敌；HitPriority 推人优先级项目没有，不装）
    'TsAnimNotifyStateAbsoluteTimeStop_C': ('AbsoluteTimeStop', {'是否冻结移动效果': 'bStopMove'},
                                            ['副本计时停止']),
    'TsAnimNotifyStateBurst_C': ('BurstState', {'不能切人': 'bForbidSwitch', '是否无敌': 'bInvincible'}, []),
    # 技能特写镜头（瞬时通知）：配置折算到 Config 文本，镜头序列用 WuwaSequenceCameraSetup 重建的同名资产
    'TsAnimNotifySwitchSequenceCamera_C': ('SequenceCamera', {'特写镜头配置': 'Config', 'CameraAttachSocket': 'CameraAttachSocket'},
                                           ['AdditiveRotation', 'ExtraSphereLocation',
                                            '启用特定功能下的镜头配置', '特定功能下的镜头配置', 'ShotBeforePlaying']),
    # 技能镜头修改：配置折算到 FWuwaCameraModify 文本（camera_modify_config）；持续时间 0 = 蒙太奇结束时退回
    'TsAnimNotifyCameraModify_C': ('CameraModify', {'相机修改配置': 'Modify', '持续时间': 'Duration', '淡入时间': 'BlendInTime', '淡出时间': 'BlendOutTime'},
                                   ['淡入曲线', '淡出曲线']),
    'TsAnimNotifyStateCameraModify_C': ('CameraModifyWindow', {'相机修改配置': 'Modify', '淡入时间': 'BlendInTime', '淡出时间': 'BlendOutTime'},
                                        ['淡入曲线', '淡出曲线', '打断后继续']),
}
# 原作有、项目暂不关心的字段（不影响行为：角色 / 怪物战斗机制停止是联机副本里的开关；HitPriority 推人优先级）
IGNORED_FIELDS = {'角色战斗机制停止', '怪物战斗机制停止', 'HitPriority',
                  # 特写镜头：检测镜头碰撞的骨骼、重置锁定镜头、只隐藏部分战斗 UI（项目整块隐藏主 HUD）、禁用输入 / 动态模糊（默认值）
                  'CameraDetectSocket', 'bResetLockOnCamera', 'IsShowExtraSphere', 'IsIgnoreCharacterCollision',
                  'DisableMovementInput', 'DisableLookAtInput', 'DisableMotionBlur', '生效客户端',
                  # 特写镜头播放前的镜头碰撞检测（被挡住就不播）：项目不做检测、总是播 = 原作 强制播放Sequence 为真；额外检测球只给这个检测用
                  '强制播放Sequence', 'ExtraDetectSphereRadius',
                  # 镜头修改：打断后的淡出时间（项目被打断也按 淡出时间 退回）、生效客户端类型（项目只有本地玩家，镜头修改只给本地玩家看的角色）、
                  # 条件（按镜头距离 / 锁定目标高差等决定改不改；项目一律生效）、NeedSimulate、Tag（同名修改互相顶替，项目按句柄管理）
                  '打断淡出时间', '生效客户端类型', '条件', 'NeedSimulate', 'Tag'}

# 原作 SCameraModifySettings → FWuwaCameraModify（只取项目支持的项；其余非默认值打印出来）
CAMERA_MODIFY_SUPPORTED = {'Priority', 'IsModifiedArmLength', 'ArmLength', 'ArmLengthAdditional', 'IsModifiedArmRotation',
                           'IsModifiedArmRotationPitch', 'IsModifiedArmRotationYaw', 'IsModifiedArmRotationRoll', 'ArmRotation',
                           'ArmRotationAdditional', 'IsModifiedCameraOffset', 'IsModifiedCameraOffsetX', 'IsModifiedCameraOffsetY',
                           'IsModifiedCameraOffsetZ', 'CameraOffset', 'CameraOffsetAdditional', 'IsModifiedCameraFov', 'CameraFov', 'IsLockInput',
                           # 覆盖玩家输入 / 强制播放 / 插值臂位置：项目锁输入时本来就不吃玩家输入、总是播、镜头位置总是插值
                           'OverrideCameraInput', 'IsForcePlayModify', 'IsLerpArmLocation', 'StopModifyOnMontageEnd',
                           # 原作结束时把臂长 / 角度"定格"在修改后的值（ResetFinalArm*）：项目一律混回修改前的镜头
                           'ResetFinalArmRotation'}
CAMERA_MODIFY_NOTES: list[str] = []


def camera_modify_config(value: dict, where: str) -> str:
    v = {re.sub(r'_\d+_[0-9A-F]{32}$', '', k): x for k, x in value.items()}
    for key, x in v.items():
        if key not in CAMERA_MODIFY_SUPPORTED and not bullets.is_empty(x):
            CAMERA_MODIFY_NOTES.append(f'{where}: camera modify {key} = {str(x)[:80]} not supported')
    rot = lambda r: f"(Pitch={float((r or {}).get('Pitch', 0.0))},Yaw={float((r or {}).get('Yaw', 0.0))},Roll={float((r or {}).get('Roll', 0.0))})"
    vec = lambda r: f"(X={float((r or {}).get('X', 0.0))},Y={float((r or {}).get('Y', 0.0))},Z={float((r or {}).get('Z', 0.0))})"
    b = lambda k: 'True' if v.get(k) else 'False'
    # 原作结构体默认值（导出只写和默认不同的字段；按全部角色出现过的值反推）：IsModifiedArmRotation、StopModifyOnMontageEnd 默认开，其余开关默认关
    rotation = bool(v.get('IsModifiedArmRotation', True))
    offset = bool(v.get('IsModifiedCameraOffset'))
    fields = [f"Priority={int(v.get('Priority', 0))}",
              f"bModifyArmLength={b('IsModifiedArmLength')}", f"ArmLength={float(v.get('ArmLength', 0.0))}",
              f"ArmLengthAdditional={float(v.get('ArmLengthAdditional', 0.0))}",
              f"bModifyPitch={'True' if rotation and v.get('IsModifiedArmRotationPitch') else 'False'}",
              f"bModifyYaw={'True' if rotation and v.get('IsModifiedArmRotationYaw') else 'False'}",
              f"ArmRotation={rot(v.get('ArmRotation'))}", f"ArmRotationAdditional={rot(v.get('ArmRotationAdditional'))}",
              f"bModifyOffsetX={'True' if offset and v.get('IsModifiedCameraOffsetX') else 'False'}",
              f"bModifyOffsetY={'True' if offset and v.get('IsModifiedCameraOffsetY') else 'False'}",
              f"bModifyOffsetZ={'True' if offset and v.get('IsModifiedCameraOffsetZ') else 'False'}",
              f"CameraOffset={vec(v.get('CameraOffset'))}", f"CameraOffsetAdditional={vec(v.get('CameraOffsetAdditional'))}",
              f"bModifyFov={b('IsModifiedCameraFov')}", f"Fov={float(v.get('CameraFov', 90.0))}", f"bLockInput={b('IsLockInput')}",
              f"bStopOnMontageEnd={'True' if v.get('StopModifyOnMontageEnd', True) else 'False'}"]
    return '(' + ','.join(fields) + ')'


def sequence_camera_config(value: dict) -> str:
    """原作 特写镜头配置 → FWuwaSequenceCameraConfig 文本（序列换成 /Game/Characters/SequenceCamera/<角色>/<同名>）。"""
    # 导出里字段名带 GUID 后缀（UserDefinedStruct SSequenceCamera_Settings）；只写非默认值，没写的 = 结构体默认（EnableSpecificSequenceTime 默认假：
    # 全部 TsAnimNotifySwitchSequenceCamera 里它只以 true 出现过）。没开 EnableSpecificSequenceTime 时 SpecificSequenceTime 不生效（原作 PlayCameraSequence）
    value = {re.sub(r'_\d+_[0-9A-F]{32}$', '', k): v for k, v in value.items()}
    if not value.get('EnableSpecificSequenceTime'):
        value['SpecificSequenceTime'] = 0.0
    sequence = value['CameraSequence']['ObjectPath'].split('/SequenceCamera/Fight/')[1].rsplit('.', 1)[0]
    asset = f"/Game/Characters/SequenceCamera/{sequence}"
    name = asset.rsplit('/', 1)[1]
    fields = [f'CameraSequence="{asset}.{name}"', f"BlendInTime={float(value.get('BlendInTime', 0.0))}",
              f"BlendOutTime={float(value.get('BlendOutTime', 0.0))}", f"bHideHud={'True' if value.get('IsHideHud') else 'False'}",
              f"HideHudTime={float(value.get('HideHudTime', 0.0))}", f"SpecificSequenceTime={float(value.get('SpecificSequenceTime', 0.0))}"]
    return '(' + ','.join(fields) + ')'
# 原作导出里和行为无关的字段；在横板模式中禁用 = 项目没有横板模式，等于一直生效
IGNORED = {'exportIndex', 'CurrentTimeLength', '在横板模式中禁用'}

# 项目补的通知（原作没有，写明原因）：
# 长离 E 起手（AM_Skill01_Start）原作只在前 0.147 秒吸附到目标前 100 厘米，之后起手动画还有约 125 厘米的前冲根运动，
# 项目里会冲到怪物头顶、落在怪物碰撞体上（原作有穿人 / 推人优先级 GoThrough、HitPriority，项目没有）。
# 补一段只"抵消"的窗口：吸附结束到起手结束，离目标 400 厘米以内时不再往前冲（不往前拉、不往后退），落点停在怪物前面。
EXTRA = {
    'changli': [{'type': 'MoveToTarget', 'sourceAsset': 'AM_Skill01_Start', 'time': 0.14676514, 'end': 1.1666666,
                 'properties': {'Distance': 100.0, 'bAllowForward': False, 'MaxDistance': 400.0},
                 'note': 'project: hold in front of the target after the original pull window'}],
}


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(bullets.CHARACTERS))
    args = parser.parse_args()
    config = bullets.CHARACTERS[args.character]

    notifies, errors = [], []
    for source, project in config['montages'].items():
        objects = json.loads(bullets.ensure_json(f"{config['anim']}/{source}").read_text(encoding='utf-8'))
        by_name = {o.get('Name'): o for o in objects}
        montage = next(o for o in objects if o.get('Type') == 'AnimMontage')
        length = float(montage['Properties']['SequenceLength'])
        for n in montage['Properties'].get('Notifies', []):
            ref = ((n.get('NotifyStateClass') or n.get('Notify')) or {}).get('ObjectName', '')
            obj = by_name.get(ref.split(':', 1)[-1].rstrip("'").rsplit('.', 1)[-1])
            if not obj or obj.get('Type') not in TYPES:
                continue
            kind, fields, unsupported = TYPES[obj['Type']]
            time = float(n.get('LinkValue', 0.0))
            duration = float(n.get('Duration', 0.0))
            properties = {}
            for key, value in obj['Properties'].items():
                if key in IGNORED or key in IGNORED_FIELDS:
                    continue
                if key in fields:
                    if key == '特写镜头配置':
                        value = sequence_camera_config(value)
                    elif key == '相机修改配置':
                        value = camera_modify_config(value, f'{source} @ {time:.3f}s')
                    elif key == 'TargetOffset':
                        value = f"(X={value.get('X', 0.0)},Y={value.get('Y', 0.0)},Z={value.get('Z', 0.0)})"
                    properties[fields[key]] = value
                elif key in unsupported:
                    if not bullets.is_empty(value):
                        errors.append(f'{source} @ {time:.3f}s {kind}: {key} = {value!r} is not supported yet')
                else:
                    errors.append(f'{source} @ {time:.3f}s {kind}: unknown field {key} = {value!r}')
            notifies.append({'type': kind, 'sourceAsset': source, 'sourceLength': length, 'asset': project,
                             'time': time, 'duration': duration, 'properties': properties})

    for extra in EXTRA.get(args.character, []):
        base = next(n for n in notifies if n['sourceAsset'] == extra['sourceAsset'])
        notifies.append({'type': extra['type'], 'sourceAsset': extra['sourceAsset'], 'sourceLength': base['sourceLength'],
                         'asset': base['asset'], 'time': extra['time'], 'duration': extra['end'] - extra['time'],
                         'properties': extra['properties'], 'note': extra['note']})

    folder = bullets.AKI / config['anim']
    markers = [t[:-2] for t in TYPES]
    not_installed = sorted(p.stem for p in folder.glob('AM_*.json')
                           if p.stem not in config['montages'] and any(m in p.read_text(encoding='utf-8') for m in markers))

    out = HERE / f'{args.character}.motion_notifies.json'
    out.write_text(json.dumps({'character': args.character, 'notifies': notifies, 'notInstalled': not_installed},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(notifies)} notifies on {len(config["montages"])} montages; {len(not_installed)} original montages not mapped')
    for n in notifies:
        print(f"  {n['sourceAsset']} @ {n['time']:.3f}s +{n['duration']:.3f}s {n['type']} {n['properties']}")
    for note in sorted(set(CAMERA_MODIFY_NOTES)):
        print(f'  note: {note}')
    for e in errors:
        print(f'  ERROR {e}')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
