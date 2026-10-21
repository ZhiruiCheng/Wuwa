"""Per-layer material audit: blend mode, master, alpha / emissive switches, texture alpha usage.

Usage: python -I audit_layers.py <Stage> [system filter]
Helps find layers whose reconstruction shows full quads (texture without real alpha, additive originals, ...).
"""
import json
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
MASTERS = json.loads((HERE / 'Evidence' / 'MasterDefaults.json').read_text(encoding='utf-8'))


def alpha_stats(texture_path: str) -> str:
    png = EXPORTS / (texture_path.replace('/Game/', 'Client/Content/') + '.png')
    if not png.exists():
        return 'no png'
    img = Image.open(png)
    if 'A' not in img.getbands():
        return f'{img.mode} no alpha'
    a = img.getchannel('A')
    lo, hi = a.getextrema()
    rgb = img.convert('RGB').convert('L')
    return f'{img.mode} alpha {lo}-{hi} mean={sum(a.getdata()) / (a.width * a.height):.0f} lum-mean={sum(rgb.getdata()) / (a.width * a.height):.0f}'


def main(stage: str, needle: str = '') -> int:
    params = json.loads((HERE / 'Evidence' / f'LayerParameters_{stage}.json').read_text(encoding='utf-8'))
    materials = {m['leaf']: m for m in params['materials']}
    for system in params['systems']:
        if needle and needle not in system['asset']:
            continue
        print('==', system['asset'].rsplit('/', 1)[1])
        for layer in system['layers']:
            if not layer['enabled']:
                continue
            for r in layer.get('renderers') or []:
                if not r:
                    continue
                rp = r['properties']
                sprite = r['type'] == 'NiagaraSpriteRendererProperties'
                ref = rp.get('Material') if sprite else (rp.get('OverrideMaterials') or [{}])[0].get('ExplicitMat')
                if not ref:
                    continue
                leaf = ref['ObjectPath'].rsplit('.', 1)[0].replace('Client/Content/', '/Game/')
                m = materials.get(leaf)
                if not m:
                    continue
                chain = m['chainLeafToRoot']
                master = next((c['asset'] for c in chain if c.get('type') == 'Material' or not c.get('exists')), chain[-1]['asset'])
                blend = next((c['basePropertyOverrides'].get('BlendMode') for c in chain
                              if c.get('basePropertyOverrides', {}).get('bOverride_BlendMode')), 'master default')
                k = m['knownEffectiveOverrides']
                md = MASTERS.get(master, {'vectors': {}, 'scalars': {}})

                def vec(name):
                    return k['vectors'].get(name, {}).get('value', md['vectors'].get(name))

                def sca(name):
                    return k['scalars'].get(name, {}).get('value', md['scalars'].get(name))

                base = k['textures'].get('Base', {}).get('value')
                mask = k['textures'].get('Mask', {}).get('value')
                print(f"  {layer['handle']:<22} {'S' if sprite else 'M'} {leaf.rsplit('/', 1)[1]:<34} master={master.rsplit('/', 1)[1]:<22} blend={blend}")
                print(f"      Base_AlphaSwitch={vec('Base_AlphaSwitch')} AlphaMul={sca('Base_Alpha_Multiply')} Mask_AlphaSwitch={vec('Mask_AlphaSwitch')} EmissiveSwitch={vec('Base_EmissiveSwitch')} Color={vec('Base_Color')}")
                print(f"      base={base and base.rsplit('/', 1)[1]} [{alpha_stats(base) if base else '-'}]  mask={mask and mask.rsplit('/', 1)[1]} [{alpha_stats(mask) if mask else '-'}]")
    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
