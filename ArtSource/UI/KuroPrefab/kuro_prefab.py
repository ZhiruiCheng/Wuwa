"""原作 UI 预制体（Kuro 的 PrefabAsset，CUE4Parse 导出成 JSON）解码：层级、组件、带标签属性。

PrefabAsset 的属性数据是 UE 的带标签属性序列化，FName 换成了 ReferenceFNames 里的 uint16 下标：
  标签 = Name(u16)  ['None' 结束]  Type(u16)  Size(i32)  ArrayIndex(i32)
         StructProperty: StructName(u16) + Guid(16)   BoolProperty: 值(u8)   Byte/EnumProperty: EnumName(u16)
         Array/SetProperty: InnerType(u16)   MapProperty: KeyType(u16) ValueType(u16)
         HasPropertyGuid(u8) [+ Guid(16)]  然后是 Size 字节的值
嵌套预制体：NestedPrefabInstancesData 给出实例（PrefabInstanceFileID → 源预制体），VirtualObjectsSerializeData 把实例里的对象
（FileID）映射回源预制体里的对象（SourcePrefabObjectFileID，经 PrefabInstanceChainFileID 找到是哪个实例），
PropertyModificationsSerializeData 是对这些对象的属性覆盖（只含列出的属性）。

用法（调试）：python -I kuro_prefab.py <预制体包路径，如 UI/UIResources/UiFight/Prefabs/UiView_Fight_Prefab>
"""
from __future__ import annotations

import json
import struct
import subprocess
import sys
from pathlib import Path

EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
GUID = 16

# 二进制（非带标签）序列化的引擎结构体
NATIVE_STRUCTS = {
    'Vector2D': ('<2f', ('X', 'Y')),
    'Vector': ('<3f', ('X', 'Y', 'Z')),
    'Vector4': ('<4f', ('X', 'Y', 'Z', 'W')),
    'Rotator': ('<3f', ('Pitch', 'Yaw', 'Roll')),
    'Quat': ('<4f', ('X', 'Y', 'Z', 'W')),
    'LinearColor': ('<4f', ('R', 'G', 'B', 'A')),
    'IntPoint': ('<2i', ('X', 'Y')),
    'Guid': ('<4I', ('A', 'B', 'C', 'D')),
}


def package_json(package: str) -> Path:
    """Client/Content/Aki/<package>.json；没有就用 PakExport 导出。"""
    rel = package if package.startswith('Client/') else f'Client/Content/Aki/{package}'
    rel = rel.split('.')[0]
    path = EXPORTS / f'{rel}.json'
    if not path.exists():
        subprocess.run([str(PAKEXPORT), 'json', f'{rel}.uasset'], check=True, capture_output=True)
    return path


def game_to_package(game_path: str) -> str:
    """/Game/Aki/X/Y.Y → Client/Content/Aki/X/Y"""
    return 'Client/Content/' + game_path.split('.')[0][len('/Game/'):]


class Reader:
    def __init__(self, data: list[int] | bytes, names: list[str]):
        self.b = bytes(data)
        self.p = 0
        self.names = names

    def take(self, n: int) -> bytes:
        v = self.b[self.p:self.p + n]
        self.p += n
        return v

    def u8(self): return self.take(1)[0]
    def u16(self): return struct.unpack('<H', self.take(2))[0]
    def i32(self): return struct.unpack('<i', self.take(4))[0]
    def f32(self): return struct.unpack('<f', self.take(4))[0]
    def name(self): return self.names[self.u16()]

    def fstring(self) -> str:
        n = self.i32()
        if n == 0:
            return ''
        if n < 0:
            return self.take(-n * 2).decode('utf-16-le').rstrip('\x00')
        return self.take(n).decode('latin-1').rstrip('\x00')


def read_value(r: Reader, ptype: str, size: int, tag: dict):
    end = r.p + size
    if ptype == 'BoolProperty':
        return tag['bool']
    if ptype == 'IntProperty':
        return r.i32()
    if ptype in ('UInt32Property',):
        return struct.unpack('<I', r.take(4))[0]
    if ptype == 'Int64Property':
        return struct.unpack('<q', r.take(8))[0]
    if ptype == 'FloatProperty':
        return r.f32()
    if ptype == 'DoubleProperty':
        return struct.unpack('<d', r.take(8))[0]
    if ptype in ('NameProperty',):
        return r.name()
    if ptype == 'EnumProperty':
        return r.name()
    if ptype == 'ByteProperty':
        if tag.get('enum') and tag['enum'] != 'None':
            return r.name() if size == 2 else r.take(size)
        return r.u8()
    if ptype in ('StrProperty',):
        return r.fstring()
    if ptype == 'TextProperty':
        raw = r.take(size)
        return {'_text': raw.hex()}
    if ptype in ('ObjectProperty', 'WeakObjectProperty', 'ClassProperty', 'InterfaceProperty'):
        return read_object(r)
    if ptype in ('SoftObjectProperty', 'SoftClassProperty'):
        v = r.take(size)
        return {'_soft': v.hex()}
    if ptype == 'StructProperty':
        return read_struct(r, tag['struct'], end)
    if ptype in ('ArrayProperty', 'SetProperty'):
        if ptype == 'SetProperty':
            r.i32()  # 删除项数
        count = r.i32()
        inner = tag['inner']
        out = []
        if inner == 'StructProperty':
            # 数组里的结构体：先有一个内部标签
            itag = read_tag(r)
            for _ in range(count):
                out.append(read_struct(r, itag['struct'], None))
        else:
            for _ in range(count):
                if inner in ('ObjectProperty', 'WeakObjectProperty', 'ClassProperty'):
                    out.append(read_object(r))
                elif inner == 'IntProperty':
                    out.append(r.i32())
                elif inner == 'FloatProperty':
                    out.append(r.f32())
                elif inner in ('NameProperty', 'EnumProperty'):
                    out.append(r.name())
                elif inner == 'StrProperty':
                    out.append(r.fstring())
                elif inner == 'BoolProperty':
                    out.append(bool(r.u8()))
                elif inner == 'ByteProperty':
                    out.append(r.u8())
                else:
                    out.append({'_raw': inner})
                    break
        r.p = end
        return out
    # 其他（Map、Delegate ……）：原样留着
    return {'_raw': ptype, '_hex': r.take(size).hex()[:200]}


NATIVE_LOWER = {k.lower(): v for k, v in NATIVE_STRUCTS.items()}


def read_struct(r: Reader, struct_name: str, end: int | None):
    # 名字表按不区分大小写去重（Color 可能存成 'color'），按小写比较
    key = struct_name.lower()
    if key in NATIVE_LOWER:
        fmt, keys = NATIVE_LOWER[key]
        vals = struct.unpack(fmt, r.take(struct.calcsize(fmt)))
        return dict(zip(keys, vals))
    if key == 'color':
        b, g, rr, a = r.take(4)
        return {'R': rr, 'G': g, 'B': b, 'A': a}
    if key in ('softobjectpath', 'softclasspath'):
        return {'_softpath': r.name()}
    return read_props(r, end)


def read_object(r: Reader):
    """对象引用：首字节是类型。0 空；2 资产（ReferenceAssets 下标 u16）；3 类（ReferenceClasses 下标 u16）；
    4 本预制体里的对象（FileID u32）。其他类型原样返回。"""
    kind = r.u8()
    if kind == 0:
        return None
    if kind == 2:
        return {'asset': r.u16()}
    if kind == 3:
        return {'class': r.u16()}
    if kind == 4:
        return {'file': struct.unpack('<I', r.take(4))[0]}
    if kind == 6:
        # 内联的子对象（Instanced，如 LGUIPlayTweenComponent.playTween）：自己的 FileID、外层对象、类、对象名、标志、
        # 属性数据长度，然后是它的带标签属性
        file = struct.unpack('<I', r.take(4))[0]
        outer = read_object(r)
        cls = read_object(r)
        name = r.name()
        flags, size = struct.unpack('<II', r.take(8))
        end = r.p + size
        props = read_props(r, end)
        r.p = end
        return {'subobject': file, 'outer': outer, 'class': cls, 'name': name, 'flags': flags, 'props': props}
    return {'kind': kind}


def read_tag(r: Reader) -> dict | None:
    name = r.name()
    if name == 'None':
        return None
    tag = {'name': name, 'type': r.name(), 'size': r.i32(), 'index': r.i32()}
    t = tag['type']
    if t == 'StructProperty':
        tag['struct'] = r.name()
        r.take(GUID)
    elif t == 'BoolProperty':
        tag['bool'] = bool(r.u8())
    elif t in ('ByteProperty', 'EnumProperty'):
        tag['enum'] = r.name()
    elif t in ('ArrayProperty', 'SetProperty'):
        tag['inner'] = r.name()
    elif t == 'MapProperty':
        tag['key'] = r.name()
        tag['value'] = r.name()
    if r.u8():
        r.take(GUID)
    return tag


# 名字表按不区分大小写去重，同一个属性在不同预制体里大小写可能不同（color / Color）：统一成第一次见到的写法，
# 否则合并属性覆盖时会变成两个键
CANONICAL: dict[str, str] = {}


class Props(dict):
    """属性字典，键不区分大小写（按 CANONICAL 统一写法存取）：代码里写 props.get('type') 也能取到存成 'Type' 的值。"""

    @staticmethod
    def _k(key):
        return CANONICAL.get(key.lower(), key) if isinstance(key, str) else key

    def __getitem__(self, key): return super().__getitem__(self._k(key))
    def __setitem__(self, key, value): super().__setitem__(self._k(key), value)
    def __contains__(self, key): return super().__contains__(self._k(key))
    def get(self, key, default=None): return super().get(self._k(key), default)
    def setdefault(self, key, default=None): return super().setdefault(self._k(key), default)
    def pop(self, key, *args): return super().pop(self._k(key), *args)


def read_props(r: Reader, end: int | None = None) -> dict:
    out = Props()
    while end is None or r.p < end:
        tag = read_tag(r)
        if tag is None:
            break
        start = r.p
        try:
            value = read_value(r, tag['type'], tag['size'], tag)
        except Exception as e:  # 解不了的值跳过，保证后面的属性还能读
            value = {'_error': str(e), '_type': tag['type']}
        r.p = start + tag['size']
        name = CANONICAL.setdefault(tag['name'].lower(), tag['name'])
        key = name if tag['index'] == 0 else f"{name}[{tag['index']}]"
        out[key] = value
    return out


def decode(data: list[int], names: list[str]) -> dict:
    if not data:
        return {}
    return read_props(Reader(data, names))


def load_prefab(package: str) -> dict:
    d = json.loads(package_json(package).read_text(encoding='utf-8'))
    return next(o for o in d if o['Type'] == 'PrefabAsset')['Properties']


def main(package: str):
    sys.stdout.reconfigure(encoding='utf-8')
    p = load_prefab(package)
    names = p['ReferenceFNames']
    for a in p['ActorsSerializeData'][:6]:
        print('ACTOR parent', a['ParentActorFileID'], 'root', a['RootComponentFileID'], p['ReferenceClasses'][a['RootClassIndex']]['ObjectName'])
        print('  actor:', json.dumps(decode(a['ActorPropertyData'], names), ensure_ascii=False)[:600])
        print('  root :', json.dumps(decode(a['RootComponentPropertyData'], names), ensure_ascii=False)[:1200])
    for c in p['ComponentsSerializeData'][:8]:
        print('COMP', c['Name'], p['ReferenceClasses'][c['ClassIndex']]['ObjectName'], c['OuterFileID'], c['ParentComponentFileID'])
        print('  ', json.dumps(decode(c['PropertyData'], names), ensure_ascii=False)[:800])


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'UI/UIResources/UiFight/Prefabs/UiView_Fight_Prefab')
