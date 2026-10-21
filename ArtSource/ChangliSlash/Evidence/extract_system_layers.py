"""Read cooked exports of arbitrary NS systems into an evidence manifest (same layout as LayerParameters.json).

Generalised from extract_layer_parameters.py (first attack only): systems and their DataAssets are passed on the
command line, leaf materials are collected from the systems' active renderers. Never modifies source exports.
Usage: python extract_system_layers.py <output.json> <NS name>:<DA name> [...]
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path('C:/GamePakExtractor/Output/Exports')
OUT = Path(__file__).parent
NIAGARA = 'Client/Content/Aki/Effect/Niagara/NI_R2T1ChangliMd10011/'
DATA_ASSETS = 'Client/Content/Aki/Effect/DataAsset/Niagara/R2T1ChangliMd10011/'
CURVE_TYPES = {'NiagaraDataInterfaceCurve': 1, 'NiagaraDataInterfaceVectorCurve': 3, 'NiagaraDataInterfaceColorCurve': 4}


def package(ref):
    return re.sub(r'\.\d+$', '', ref)


def game_path(pkg):
    return pkg.replace('Client/Content/', '/Game/').replace('Engine/Content/', '/Engine/')


def load(pkg):
    path = ROOT / (package(pkg) + '.json')
    if not path.exists():
        return path, [], ''
    text = path.read_text(encoding='utf-8-sig')
    objs = json.loads(text)
    return path, objs if isinstance(objs, list) else [objs], text


def line(text, needle):
    offset = text.find(needle)
    return text.count('\n', 0, offset) + 1 if offset >= 0 else None


def values(props, field, source, text):
    result = {}
    for p in props.get(field, []):
        name = p.get('ParameterInfo', {}).get('Name')
        if not name:
            continue
        value = p.get('ParameterValue', p.get('Value'))
        if isinstance(value, dict) and 'R' in value:
            value = [value[k] for k in ('R', 'G', 'B', 'A')]
        elif isinstance(value, dict) and 'ObjectPath' in value:
            value = game_path(package(value['ObjectPath']))
        result[name] = {'value': value, 'source': source, 'line': line(text, '"Name": "' + name + '"')}
        if 'bOverride' in p:
            result[name]['bOverride'] = p['bOverride']
    return result


def material_record(leaf_pkg):
    pkg, chain, visited = leaf_pkg, [], set()
    while pkg and pkg not in visited:
        visited.add(pkg)
        path, objs, txt = load(pkg)
        obj = next((o for o in objs if o.get('Type') in ('MaterialInstanceConstant', 'Material')), None)
        if obj is None:
            chain.append({'asset': game_path(pkg), 'source': str(path), 'exists': False})
            break
        p = obj.get('Properties', {})
        chain.append({
            'asset': game_path(pkg), 'source': str(path), 'exists': True, 'type': obj['Type'],
            'parent': game_path(package(p['Parent']['ObjectPath'])) if p.get('Parent') else None,
            'scalars': values(p, 'ScalarParameterValues', str(path), txt),
            'vectors': values(p, 'VectorParameterValues', str(path), txt),
            'textures': values(p, 'TextureParameterValues', str(path), txt),
            'staticSwitches': values(p.get('StaticParameters', {}), 'StaticSwitchParameters', str(path), txt),
            'basePropertyOverrides': p.get('BasePropertyOverrides', {}),
        })
        pkg = package(p.get('Parent', {}).get('ObjectPath', ''))
    merged = {k: {} for k in ('scalars', 'vectors', 'textures', 'staticSwitches')}
    for entry in reversed(chain):
        for k in merged:
            merged[k].update({name: value for name, value in entry.get(k, {}).items() if value.get('bOverride', True)})
    return {
        'leaf': game_path(leaf_pkg), 'chainLeafToRoot': chain, 'knownEffectiveOverrides': merged,
        'formulaStatus': 'Unknown: no master Material expression graph exported. Parameter names and numeric values are evidence, not proof of channel/UV/formula semantics.',
    }


def system_record(ns_name, da_name, leaves):
    pkg = NIAGARA + ns_name
    path, objs, txt = load(pkg)
    system = next(o for o in objs if o['Type'] == 'NiagaraSystem')

    def resolve(ref):
        return objs[int(ref['ObjectPath'].rsplit('.', 1)[1])] if ref else None

    layers = []
    for handle in system['Properties']['EmitterHandles']:
        emitter = resolve(handle.get('Instance'))
        record = {'handle': handle['Name'], 'enabled': handle.get('bIsEnabled', True), 'instance': emitter['Name'] if emitter else None}
        if emitter:
            ep = emitter.get('Properties', {})
            record['explicitEmitterProperties'] = {k: v for k, v in ep.items() if k not in ('UpdateScriptProps', 'SpawnScriptProps', 'RendererProperties', 'GPUComputeScript')}
            renderers = []
            for renderer_ref in ep.get('RendererProperties', []):
                renderer = resolve(renderer_ref)
                if not renderer:
                    renderers.append(None)
                    continue
                rp = renderer.get('Properties', {})
                renderers.append({'type': renderer['Type'], 'properties': rp})
                material = rp.get('Material') or (rp.get('OverrideMaterials') or [{}])[0].get('ExplicitMat')
                if record['enabled'] and material:
                    leaves.add(package(material['ObjectPath']))
            record['renderers'] = renderers
            curves, seen = [], set()
            for script in objs:
                if script.get('Type') != 'NiagaraScript' or not script.get('Outer', {}).get('ObjectName', '').endswith(':' + emitter['Name'] + "'"):
                    continue
                for binding in script.get('Properties', {}).get('CachedDefaultDataInterfaces', []):
                    di = resolve(binding.get('DataInterface'))
                    if di is None or di.get('Type') not in CURVE_TYPES:
                        continue
                    key = (binding.get('Name'), binding['DataInterface']['ObjectPath'])
                    if key in seen:
                        continue
                    seen.add(key)
                    cp = di.get('Properties', {})
                    width = CURVE_TYPES[di['Type']]
                    lut = cp.get('ShaderLUT', [])
                    samples = [lut[i:i + width] for i in range(0, len(lut), width)]
                    curves.append({
                        'semanticBinding': binding['Name'], 'type': di['Type'],
                        'dataInterfaceOuter': di.get('Outer', {}).get('ObjectName'),
                        'exportedDomain': {k: cp[k] for k in ('LUTMinTime', 'LUTMaxTime', 'LUTInvTimeRange', 'LUTNumSamplesMinusOne') if k in cp},
                        'sampleCount': len(samples), 'components': width, 'samples': samples,
                        'samplingInputKnown': False,
                    })
            record['curves'] = curves
        layers.append(record)
    if not da_name:
        # 镜头序列里直接放的 Niagara（没有 EffectModel DA）
        return {'asset': game_path(pkg), 'source': str(path), 'dataAsset': None, 'dataAssetSource': None,
                'dataAssetExplicitProperties': {}, 'layers': layers}
    da_path, da_objs, _ = load(DATA_ASSETS + da_name)
    return {'asset': game_path(pkg), 'source': str(path), 'dataAsset': da_name, 'dataAssetSource': str(da_path),
            'dataAssetExplicitProperties': da_objs[0].get('Properties', {}) if da_objs else {}, 'layers': layers}


def main(output, pairs):
    leaves, systems = set(), []
    for pair in pairs:
        ns_name, da_name = pair.split(':', 1)
        systems.append(system_record(ns_name, da_name, leaves))
    manifest = {
        'schemaVersion': 1, 'sourceExportRoot': str(ROOT), 'generator': Path(__file__).name,
        'materials': [material_record(leaf) for leaf in sorted(leaves)], 'systems': systems,
        'reconstructionChoicesNotOriginalFacts': [
            'Material graph absent: ordering and channel math of reconstructed shaders is authored.',
            'Uniform LUT samples are exact exported data; curve sampling inputs are not recovered.',
            'Cooked scripts carry no rapid-iteration constants: lifetime, spawn count and burst timing are not in the export.',
        ],
    }
    target = OUT / output
    target.write_text(json.dumps(manifest, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    print(json.dumps({'output': str(target), 'materials': len(manifest['materials']), 'systems': len(systems),
                      'curves': sum(len(l.get('curves', [])) for s in systems for l in s['layers'])}, ensure_ascii=False))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2:])
