"""Trace the export dependencies of Jiabei's attack effects and list what is still missing.

Usage: python -I trace_dependencies.py   (writes Dependencies.json and prints a summary)

Starting points are the effect groups referenced by the attack sequences' AnimNotifyEffect notifies
(see Docs: Attack01..04 + Attack_charge / AM_Charge). For every referenced asset the script checks the
file the reconstruction pipeline needs: JSON for data assets / Niagara / material instances, PNG for
textures, PSKX for static meshes. Nothing is copied or modified.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
ROLE = ROOT / 'Character/Role/FemaleXL/Jiabeilina/CommonAnim'
HERE = Path(__file__).resolve().parent

# 段 -> 起点（动画序列 / 蒙太奇 JSON，里面的 AnimNotifyEffect 引用特效组）
STAGES = {
    'Attack01': ['Attack01'],
    'Attack02': ['Attack02'],
    'Attack03': ['Attack03'],
    'Attack04': ['Attack04'],
    'Charge': ['Attack_charge', 'AM_Charge'],
}

REF = re.compile(r'(?:/Game/Aki/|Client/Content/Aki/)([A-Za-z0-9_/\-]+?)(?:\.[A-Za-z0-9_]+)?["\']')


def refs_in(path: Path) -> list[str]:
    text = path.read_text(encoding='utf-8', errors='replace')
    out = []
    for m in REF.finditer(text):
        rel = m.group(1)
        if rel not in out:
            out.append(rel)
    return out


def classify(rel: str) -> str:
    name = rel.rsplit('/', 1)[-1]
    if rel.startswith('Effect/Texture') or rel.startswith('Render/Common') or name.startswith('T_'):
        return 'texture'
    if rel.startswith('Effect/Mesh') or name.startswith(('Mod_', 'SM_')):
        return 'mesh'
    if name.startswith(('MI_', 'M_', 'MF_')):
        return 'material'
    if name.startswith('NS_'):
        return 'niagara'
    if name.startswith('NE_') or '/Emitter' in rel:
        return 'emitter'
    if name.startswith('DA_Au') or '/Audio/' in rel or name.startswith(('Play_', 'AkEvent')):
        return 'audio'
    if name.startswith('DA_'):
        return 'dataasset'
    if name.startswith(('Curve', 'CF_', 'CV_', 'CC_')) or '/Curves/' in rel:
        return 'curve'
    if name.startswith(('PC_', 'HPC_')) or 'PointCache' in rel or 'PointCloud' in rel:
        return 'pointcache'
    return 'other'


NEEDS = {'texture': '.png', 'mesh': '.pskx'}


def status(rel: str, kind: str) -> dict:
    base = ROOT / rel
    have_json = base.with_suffix('.json').exists()
    entry = {'path': rel, 'kind': kind, 'json': have_json}
    need = NEEDS.get(kind)
    if need:
        candidates = [base.with_suffix(need)] + ([base.with_suffix('.psk')] if need == '.pskx' else [])
        entry['file'] = any(c.exists() for c in candidates)
    return entry


def walk(start_files: list[Path]) -> dict[str, dict]:
    seen: dict[str, dict] = {}
    queue: list[tuple[str, int, str]] = []
    for f in start_files:
        for rel in refs_in(f):
            if rel.startswith(('Effect/', 'Render/')):
                queue.append((rel, 0, f.stem))
    while queue:
        rel, depth, parent = queue.pop(0)
        if rel in seen:
            seen[rel]['usedBy'].add(parent)
            continue
        kind = classify(rel)
        entry = status(rel, kind)
        entry['usedBy'] = {parent}
        entry['depth'] = depth
        seen[rel] = entry
        # Niagara 数据资产 JSON 还没导出时，按名字找同名 NS（DA_Fx_X -> NS_Fx_X）继续往下追，标记为推测
        if not entry['json'] and kind == 'dataasset' and '/DataAsset/Niagara/' in rel:
            guess = 'NS_' + rel.rsplit('/', 1)[-1][3:]
            hits = list((ROOT / 'Effect/Niagara').rglob(guess + '.json'))
            if hits:
                ns = hits[0].relative_to(ROOT).with_suffix('').as_posix()
                entry['guessedNiagara'] = ns
                queue.append((ns, depth + 1, rel.rsplit('/', 1)[-1] + '(guess)'))
            else:
                entry['guessedNiagara'] = None
        # 往下追：数据资产 / Niagara / 材质实例（不进基础 Material 的整张图）
        if not entry['json'] or kind in ('texture', 'mesh', 'audio', 'curve', 'pointcache'):
            continue
        if kind == 'material' and rel.rsplit('/', 1)[-1].startswith('M_'):
            continue
        for child in refs_in((ROOT / rel).with_suffix('.json')):
            if child != rel and child.startswith(('Effect/', 'Render/')):
                queue.append((child, depth + 1, rel.rsplit('/', 1)[-1]))
    return seen


def main() -> int:
    report = {}
    for stage, starts in STAGES.items():
        files = [ROLE / f'{s}.json' for s in starts if (ROLE / f'{s}.json').exists()]
        assets = walk(files)
        for a in assets.values():
            a['usedBy'] = sorted(a['usedBy'])
        report[stage] = sorted(assets.values(), key=lambda a: (a['depth'], a['kind'], a['path']))

    (HERE / 'Dependencies.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    missing_all: dict[str, set[str]] = {}
    for stage, assets in report.items():
        kinds: dict[str, int] = {}
        for a in assets:
            kinds[a['kind']] = kinds.get(a['kind'], 0) + 1
            lacking = []
            if not a['json'] and a['kind'] not in ('texture', 'mesh'):
                lacking.append('json')
            if a.get('file') is False:
                lacking.append(NEEDS[a['kind']])
            if lacking and a['kind'] not in ('audio',):
                missing_all.setdefault(a['path'], set()).update(f'{stage}:{"+".join(lacking)}' for _ in [0])
        print(f'== {stage}: ' + ', '.join(f'{k}={v}' for k, v in sorted(kinds.items())))
    print(f'\n== missing ({len(missing_all)})')
    for path, stages in sorted(missing_all.items()):
        print(f'{path}  [{", ".join(sorted(stages))}]')
    return 0


if __name__ == '__main__':
    sys.exit(main())
