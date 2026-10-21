"""Read master Material parameter defaults from cooked CachedExpressionData (names in RuntimeEntries order).

Usage: python -I master_defaults.py [name filter]   -> writes Evidence/MasterDefaults.json, prints filtered entries.
RuntimeEntries / [1] / [2] are scalar / vector / texture parameters; ParameterInfos[i] names ScalarValues[i] etc.
"""
import json
import sys
from pathlib import Path

ROOT = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki/Render/Shaders/Effect/EffectMaterial')
HERE = Path(__file__).resolve().parent


def defaults(path: Path) -> dict:
    obj = json.loads(path.read_text(encoding='utf-8-sig'))[0]
    params = obj['Properties'].get('CachedExpressionData', {}).get('Parameters', {})
    out = {'scalars': {}, 'vectors': {}, 'textures': {}}
    for key, kind, values in (('RuntimeEntries', 'scalars', params.get('ScalarValues', [])),
                              ('RuntimeEntries[1]', 'vectors', params.get('VectorValues', [])),
                              ('RuntimeEntries[2]', 'textures', params.get('TextureValues', []))):
        infos = params.get(key, {}).get('ParameterInfos', [])
        for info, value in zip(infos, values):
            if kind == 'vectors':
                value = [value[c] for c in ('R', 'G', 'B', 'A')]
            elif kind == 'textures':
                value = (value or {}).get('AssetPathName') if isinstance(value, dict) else value
            out[kind][info['Name']] = value
    return out


def main() -> int:
    result = {}
    for path in sorted(ROOT.rglob('M_*.json')):
        try:
            result['/Game/Aki/Render/Shaders/Effect/EffectMaterial/' + path.relative_to(ROOT).with_suffix('').as_posix()] = defaults(path)
        except (KeyError, IndexError, json.JSONDecodeError):
            continue
    (HERE / 'Evidence').mkdir(exist_ok=True)
    (HERE / 'Evidence' / 'MasterDefaults.json').write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding='utf-8')
    needle = sys.argv[1] if len(sys.argv) > 1 else ''
    for master, d in result.items():
        if not master.endswith(('M_SimpleEffect_MP', 'M_AdvancedEffect_MP', 'M_SimpleEffect_S', 'M_AdvancedEffect_S')):
            continue
        print('==', master.rsplit('/', 1)[1])
        for kind in ('vectors', 'scalars'):
            for name, value in d[kind].items():
                if needle.lower() in name.lower():
                    print(f'  {kind[:-1]} {name} = {value}')
    print(f'{len(result)} master materials')
    return 0


if __name__ == '__main__':
    sys.exit(main())
