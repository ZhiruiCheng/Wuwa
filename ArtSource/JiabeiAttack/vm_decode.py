"""Decode a cooked Niagara script's VectorVM bytecode into per-attribute expressions.

The cooked export keeps no RapidIterationParameters: the module inputs (lifetime, mesh scale, orientation,
sprite size, velocity, colour...) were baked into the bytecode as literals (CachedScriptVM.ScriptLiterals).
This walks the classic VectorVM bytecode (encoding as read by UE 5.7 VectorVMOptimizer.cpp), keeps a symbolic
expression per temp register, and prints what each particle attribute is set to.

  python -I vm_decode.py <NS export .json> <emitter name> [spawn|update] [--asm]

Constant operands are byte offsets into the constant buffer table
(global, system, owner, emitter parameter blocks, then the script's external parameters, then ScriptLiterals;
see FNiagaraEmitterInstanceImpl::BuildConstantBufferTable). The literal block is located from the operand offsets.
"""
from __future__ import annotations

import json
import struct
import sys

# opcode -> (name, inputs) for the plain vector ops (1 const-flag byte, then uint16 per input and the output)
VEC = {
    1: ('add', 2), 2: ('sub', 2), 3: ('mul', 2), 4: ('div', 2), 5: ('mad', 3), 6: ('lerp', 3), 7: ('rcp', 1),
    8: ('rsq', 1), 9: ('sqrt', 1), 10: ('neg', 1), 11: ('abs', 1), 12: ('exp', 1), 13: ('exp2', 1), 14: ('log', 1),
    15: ('log2', 1), 16: ('sin', 1), 17: ('cos', 1), 18: ('tan', 1), 19: ('asin', 1), 20: ('acos', 1), 21: ('atan', 1),
    22: ('atan2', 2), 23: ('ceil', 1), 24: ('floor', 1), 25: ('fmod', 2), 26: ('frac', 1), 27: ('trunc', 1),
    28: ('clamp', 3), 29: ('min', 2), 30: ('max', 2), 31: ('pow', 2), 32: ('round', 1), 33: ('sign', 1),
    34: ('step', 2), 35: ('random', 1), 37: ('cmplt', 2), 38: ('cmple', 2), 39: ('cmpgt', 2), 40: ('cmpge', 2),
    41: ('cmpeq', 2), 42: ('cmpneq', 2), 43: ('select', 3), 44: ('addi', 2), 45: ('subi', 2), 46: ('muli', 2),
    47: ('divi', 2), 48: ('clampi', 3), 49: ('mini', 2), 50: ('maxi', 2), 51: ('absi', 1), 52: ('negi', 1),
    53: ('signi', 1), 54: ('randomi', 1), 55: ('cmplti', 2), 56: ('cmplei', 2), 57: ('cmpgti', 2), 58: ('cmpgei', 2),
    59: ('cmpeqi', 2), 60: ('cmpneqi', 2), 61: ('bit_and', 2), 62: ('bit_or', 2), 63: ('bit_xor', 2),
    64: ('bit_not', 1), 65: ('bit_lshift', 2), 66: ('bit_rshift', 2), 67: ('logic_and', 2), 68: ('logic_or', 2),
    69: ('logic_xor', 2), 70: ('logic_not', 1), 71: ('f2i', 1), 72: ('i2f', 1), 73: ('f2b', 1), 74: ('b2f', 1),
    75: ('i2b', 1), 76: ('b2i', 1), 95: ('half_to_float', 1), 96: ('fasi', 1), 97: ('iasf', 1),
}
INPUT = {77: 'inputdata_float', 78: 'inputdata_int32', 79: 'inputdata_half', 80: 'inputdata_noadvance_float',
         81: 'inputdata_noadvance_int32', 82: 'inputdata_noadvance_half'}
OUTPUT = {83: 'outputdata_float', 84: 'outputdata_int32', 85: 'outputdata_half'}
INT_TYPES = ('NiagaraInt32', 'NiagaraBool', 'Enum', 'NiagaraID')


def components(attributes):
    """Float and int component names of the particle data set, in attribute order."""
    floats, ints = [], []
    sizes = {'NiagaraFloat': 1, 'Vector2D': 2, 'Vector2f': 2, 'Vector': 3, 'Vector3f': 3, 'Vector4': 4,
             'Vector4f': 4, 'LinearColor': 4, 'Quat': 4, 'Quat4f': 4, 'NiagaraPosition': 3}
    axes = 'xyzw'
    for attribute in attributes:
        type_name = attribute['TypeDef']['ClassStructOrEnum']['ObjectName'].split("'")[1] if attribute['TypeDef'].get('ClassStructOrEnum') else 'Enum'
        name = attribute['Name']
        if type_name == 'NiagaraID':
            ints += [name + '.Index', name + '.AcquireTag']
        elif type_name == 'NiagaraRandInfo':  # FNiagaraRandInfo: Seed1, Seed2, Seed3（都是 int32）
            ints += [name + '.Seed1', name + '.Seed2', name + '.Seed3']
        elif type_name == 'NiagaraSpawnInfo':  # Count, InterpStartDt, IntervalDt, SpawnGroup
            ints += [name + '.Count', name + '.SpawnGroup']
            floats += [name + '.InterpStartDt', name + '.IntervalDt']
        elif type_name in INT_TYPES or attribute['TypeDef'].get('UnderlyingType') == 3:
            ints.append(name)
        else:
            n = sizes.get(type_name)
            if n is None:
                raise ValueError(f'unknown attribute type {type_name} for {name}')
            floats += [name if n == 1 else f'{name}.{axes[i]}' for i in range(n)]
    return floats, ints


def fmt(value: float) -> str:
    return f'{value:.6g}'


class Decoder:
    def __init__(self, script: dict, literal_base: int | None = None):
        vm = script['Properties']['CachedScriptVM']
        self.code = bytes(vm['ByteCode'])
        self.literals = bytes(vm['ScriptLiterals'])
        self.ext = vm.get('CalledVMExternalFunctions', [])
        self.floats, self.ints = components(vm['Attributes'])
        self.literal_base = literal_base
        self.lines: list[str] = []

    def walk(self):
        """Yield (offset, opname, payload) for every instruction."""
        code, pc = self.code, 0
        while pc < len(code):
            op = code[pc]
            start = pc
            pc += 1
            if op == 0:
                yield start, 'done', None
                break
            if op in VEC:
                name, n = VEC[op]
                flags = code[pc]
                idx = struct.unpack_from('<%dH' % (n + 1), code, pc + 1)
                pc += 1 + 2 * (n + 1)
                yield start, name, (flags, idx[:n], idx[n])
            elif op in INPUT:
                yield start, INPUT[op], struct.unpack_from('<3H', code, pc)
                pc += 6
            elif op in OUTPUT:
                flags = code[pc]
                yield start, OUTPUT[op], (flags, struct.unpack_from('<4H', code, pc + 1))
                pc += 9
            elif op == 86:  # acquireindex
                flags = code[pc]
                yield start, 'acquireindex', (flags, struct.unpack_from('<3H', code, pc + 1))
                pc += 7
            elif op == 87:  # external_func_call; this engine branch encodes the function index as one byte (UE 5.7: two)
                fn = code[pc]
                info = self.ext[fn]
                n_in = len(info.get('InputParamLocations', []))
                n_out = info.get('NumOutputs', 0)
                regs = struct.unpack_from('<%dH' % (n_in + n_out), code, pc + 1)
                pc += 1 + 2 * (n_in + n_out)
                yield start, 'external_func_call', (fn, regs[:n_in], regs[n_in:])
            elif op == 88:  # exec_index
                yield start, 'exec_index', struct.unpack_from('<H', code, pc)
                pc += 2
            elif op == 91:  # enter_stat_scope
                pc += 2
            elif op == 92:  # exit_stat_scope
                pass
            elif op in (93, 94):  # update_id / acquire_id
                yield start, 'update_id' if op == 93 else 'acquire_id', struct.unpack_from('<3H', code, pc)
                pc += 6
            else:
                raise ValueError(f'opcode {op} at {start} not handled')

    def const_offsets(self):
        out = []
        for _, name, payload in self.walk():
            if payload and name not in ('external_func_call', 'exec_index', 'update_id', 'acquire_id') and not name.startswith('input'):
                if name.startswith('output'):
                    flags, (ds, dst_idx, src, dst) = payload
                    if flags & 1:
                        out.append(src)
                elif name == 'acquireindex':
                    flags, (ds, inp, outp) = payload
                    if flags & 1:
                        out.append(inp)
                else:
                    flags, inputs, _ = payload
                    out += [r for i, r in enumerate(inputs) if flags & (1 << i)]
            elif name == 'external_func_call':
                _, inputs, _ = payload
                out += [r for r in inputs if r != 0xFFFF and not r & 0x8000]
        return out

    # ---- constants -------------------------------------------------------------------------------------------
    # Engine parameter blocks (global, system, owner, emitter) are fixed structs: 608 bytes in this engine branch
    # (P[608] is the first external parameter, e.g. "<Emitter>.AlphaScale"). External parameters follow; spawn
    # scripts with interpolated spawning repeat both (PREV_ values); ScriptLiterals come last.
    ENGINE_BLOCKS = 608

    def setup_constants(self, script: dict, interpolated: bool, system: bool = False):
        store = script['Properties']['ScriptExecutionParamStore']
        names = {}
        external = [e for e in store['SortedParameterOffsets'] if 'DataInterface' not in json.dumps(e['TypeDef'])]
        size = store.get('ParameterSize', 0)
        for e in external:
            names[e['Offset']] = e['Name']
        self.external_names = names
        self.external_size = size
        # 系统脚本前面只有 32 字节的全局参数块（由 RNG 的 0xFFFFFF 掩码 / 移位 8 两个字面量定位）
        self.engine_blocks = 32 if system else self.ENGINE_BLOCKS
        self.literal_base = (self.engine_blocks + size) * (2 if interpolated else 1)
        self.external_data = bytes(store.get('ParameterData', []))

    def const(self, offset: int):
        """Number for literals and known external values, otherwise a symbolic name."""
        base = self.literal_base
        if base <= offset < base + len(self.literals):
            raw = self.literals[offset - base: offset - base + 4]
            f = struct.unpack('<f', raw)[0]
            if f != f or (f != 0.0 and not (1e-30 < abs(f) < 1e12)):
                return f'int({struct.unpack("<i", raw)[0]})'
            return f
        rel = offset - getattr(self, 'engine_blocks', self.ENGINE_BLOCKS)
        if 0 <= rel < self.external_size * 2:
            name = self.external_names.get(rel % self.external_size if rel >= self.external_size else rel, f'ext+{rel}')
            return ('PREV_' if rel >= self.external_size else '') + name
        if offset == 0:
            return 'Engine.DeltaTime'
        return f'Engine[{offset}]'

    # ---- symbolic run ------------------------------------------------------------------------------------------
    FOLD = {'add': lambda a, b: a + b, 'sub': lambda a, b: a - b, 'mul': lambda a, b: a * b,
            'div': lambda a, b: a / b if b else float('nan'), 'mad': lambda a, b, c: a * b + c,
            'neg': lambda a: -a, 'min': min, 'max': max}

    def run(self, show_asm=False):
        reg: dict[int, object] = {}
        lines: list[tuple[str, str, list[str]]] = []  # (ssa name, expr, deps)
        writes: dict[str, object] = {}
        counter = [0]

        def show(v):
            return fmt(v) if isinstance(v, float) else str(v)

        def emit(expr: str, deps: list[str]):
            counter[0] += 1
            name = f'v{counter[0]}'
            lines.append((name, expr, deps))
            return name

        for at, name, payload in self.walk():
            if show_asm:
                print(at, name, payload)
            if name == 'done':
                break
            if name.startswith('input'):
                ds, comp, dst = payload
                table = self.ints if 'int32' in name else self.floats
                reg[dst] = f'Particles.{table[comp]}' if ds == 0 and comp < len(table) else f'In{ds}[{comp}]'
            elif name.startswith('output'):
                flags, (ds, _, src, dst) = payload
                value = self.const(src) if flags & 1 else reg.get(src, f'r{src}')
                table = self.ints if 'int32' in name else self.floats
                writes[f'{table[dst]}' if ds == 0 and dst < len(table) else f'Out{ds}[{dst}]'] = value
            elif name == 'external_func_call':
                fn, inputs, outputs = payload
                info = self.ext[fn]
                args = [('-' if r == 0xFFFF else (reg.get(r & 0x7FFF, f'r{r & 0x7FFF}') if r & 0x8000 else self.const(r))) for r in inputs]
                owner = info['OwnerName'].replace('Emitter.', '')
                ssa = emit(f'{owner}::{info["Name"]}({", ".join(show(a) for a in args)})', [a for a in args if isinstance(a, str)])
                for k, r in enumerate(outputs):
                    # 数据接口函数可能有超过 4 个输出（例如骨骼网格采样），超出 rgba 的用序号
                    reg[r] = ssa + (('.' + 'rgba'[k] if k < 4 else f'.out{k}') if len(outputs) > 1 else '')
            elif name == 'exec_index':
                reg[payload[0]] = 'ExecIndex'
            elif name in ('acquireindex', 'update_id', 'acquire_id'):
                pass
            else:
                flags, inputs, out = payload
                args = [self.const(r) if flags & (1 << i) else reg.get(r, f'r{r}') for i, r in enumerate(inputs)]
                if name in self.FOLD and all(isinstance(a, float) for a in args):
                    reg[out] = float(self.FOLD[name](*args))
                else:
                    reg[out] = emit(f'{name}({", ".join(show(a) for a in args)})', [a for a in args if isinstance(a, str)])
        self.lines, self.writes = lines, writes
        return writes

    def report(self, full=False):
        """Print the SSA lines the writes depend on, then the writes."""
        needed, by_name = set(), {n: (e, d) for n, e, d in self.lines}
        stack = [v for v in self.writes.values() if isinstance(v, str)]
        while stack:
            v = stack.pop().split('.')[0]
            if v in by_name and v not in needed:
                needed.add(v)
                stack += by_name[v][1]
        for n, e, _ in self.lines:
            if full or n in needed:
                print(f'  {n} = {e}')
        for key in sorted(self.writes):
            v = self.writes[key]
            print(f'{key} = {fmt(v) if isinstance(v, float) else v}')


def find_script(data, emitter: str, usage: str):
    wanted = {'spawn': 'SpawnScript', 'update': 'UpdateScript', 'system_spawn': 'SystemSpawnScript', 'system_update': 'SystemUpdateScript'}[usage]
    if usage.startswith('system'):
        return next((o for o in data if o.get('Type') == 'NiagaraScript' and o.get('Name') == wanted), None)
    for o in data:
        if o.get('Type') == 'NiagaraScript' and (o.get('Name') == wanted or o.get('Name', '').startswith(wanted + '_')):
            # 没有参数的脚本（导出里没有 SortedParameterOffsets）认不出属于哪个发射器，跳过
            names = [e['Name'] for e in o['Properties'].get('ScriptExecutionParamStore', {}).get('SortedParameterOffsets', [])]
            if any(n.startswith(emitter + '.') or f'.{emitter}.' in n for n in names):
                return o
    return None


def decode(data, emitter: str, usage: str = 'spawn') -> Decoder | None:
    script = find_script(data, emitter, usage)
    if script is None:
        return None
    decoder = Decoder(script)
    names = [e['Name'] for e in script['Properties']['ScriptExecutionParamStore']['SortedParameterOffsets']]
    decoder.setup_constants(script, interpolated=any('InterpSpawnStartDt' in n for n in names))
    decoder.user_defaults = user_parameter_defaults(data)
    decoder.run()
    return decoder


def user_parameter_defaults(data) -> dict[str, float]:
    """系统 ExposedParameters 里 User.* 浮点参数的默认值（脚本把它们当外部常量读；组件没覆盖时就是这些值）。"""
    system = next((o for o in data if o.get('Type') == 'NiagaraSystem'), None)
    store = ((system or {}).get('Properties') or {}).get('ExposedParameters') or {}
    raw = bytes(store.get('ParameterData') or [])
    out = {}
    for e in store.get('SortedParameterOffsets') or []:
        if "'NiagaraFloat'" in json.dumps(e.get('TypeDef')) and e['Offset'] + 4 <= len(raw):
            value = struct.unpack_from('<f', raw, e['Offset'])[0]
            out[e['Name']] = value
            out['PREV_' + e['Name']] = value
    # 调用方（prepare_attack）把特效 DA 的 FloatParameters 作为一个附加对象挂在导出列表里：运行时库洛按名字写 User.<Key>
    for o in data:
        if o.get('Type') == 'WuwaUserParameterOverrides':
            for name, value in o['Values'].items():
                out['User.' + name] = out['PREV_User.' + name] = float(value)
    return out


def main(argv):
    path, emitter, *rest = argv
    usage = 'update' if 'update' in rest else 'spawn'
    data = json.load(open(path, encoding='utf-8'))
    decoder = decode(data, emitter, usage)
    if decoder is None:
        raise SystemExit(f'no {usage} script for {emitter}')
    if '--asm' in rest:
        decoder.run(show_asm=True)
    print(f'# {usage} script of {emitter}: {len(decoder.code)} bytes, literal base {decoder.literal_base}')
    decoder.report(full='--full' in rest)


if __name__ == '__main__':
    main(sys.argv[1:])

# ---- numeric evaluation ----------------------------------------------------------------------------------------
import math as _m

_OPS = {
    'add': lambda a, b: a + b, 'sub': lambda a, b: a - b, 'mul': lambda a, b: a * b,
    'div': lambda a, b: a / b if b else 0.0, 'mad': lambda a, b, c: a * b + c, 'lerp': lambda a, b, t: a + (b - a) * t,
    'rcp': lambda a: 1 / a if a else 0.0, 'rsq': lambda a: 1 / _m.sqrt(a) if a > 0 else 0.0, 'sqrt': lambda a: _m.sqrt(max(a, 0)),
    'neg': lambda a: -a, 'abs': abs, 'exp': _m.exp, 'exp2': lambda a: 2 ** a, 'log': lambda a: _m.log(a) if a > 0 else -1e30,
    'log2': lambda a: _m.log2(a) if a > 0 else -1e30, 'sin': _m.sin, 'cos': _m.cos, 'tan': _m.tan,
    'asin': lambda a: _m.asin(max(-1, min(1, a))), 'acos': lambda a: _m.acos(max(-1, min(1, a))), 'atan': _m.atan,
    'atan2': _m.atan2, 'ceil': _m.ceil, 'floor': _m.floor, 'fmod': lambda a, b: _m.fmod(a, b) if b else 0.0,
    'frac': lambda a: a - _m.floor(a), 'trunc': _m.trunc, 'clamp': lambda a, lo, hi: max(lo, min(hi, a)), 'min': min,
    'max': max, 'pow': lambda a, b: a ** b if a >= 0 else 0.0, 'round': round, 'sign': lambda a: (a > 0) - (a < 0),
    'step': lambda a, b: 1.0 if b >= a else 0.0, 'cmplt': lambda a, b: a < b, 'cmple': lambda a, b: a <= b,
    'cmpgt': lambda a, b: a > b, 'cmpge': lambda a, b: a >= b, 'cmpeq': lambda a, b: a == b, 'cmpneq': lambda a, b: a != b,
    'select': lambda c, a, b: a if c else b, 'addi': lambda a, b: a + b, 'subi': lambda a, b: a - b, 'muli': lambda a, b: a * b,
    'divi': lambda a, b: int(a / b) if b else 0, 'clampi': lambda a, lo, hi: max(lo, min(hi, a)), 'mini': min, 'maxi': max,
    'absi': abs, 'negi': lambda a: -a, 'signi': lambda a: (a > 0) - (a < 0), 'cmplti': lambda a, b: a < b,
    'cmplei': lambda a, b: a <= b, 'cmpgti': lambda a, b: a > b, 'cmpgei': lambda a, b: a >= b,
    'cmpeqi': lambda a, b: a == b, 'cmpneqi': lambda a, b: a != b, 'logic_and': lambda a, b: bool(a) and bool(b),
    'logic_or': lambda a, b: bool(a) or bool(b), 'logic_xor': lambda a, b: bool(a) != bool(b), 'logic_not': lambda a: not a,
    'bit_and': lambda a, b: int(a) & int(b), 'bit_or': lambda a, b: int(a) | int(b), 'bit_xor': lambda a, b: int(a) ^ int(b),
    'bit_not': lambda a: ~int(a), 'bit_lshift': lambda a, b: int(a) << int(b), 'bit_rshift': lambda a, b: int(a) >> int(b),
    'f2i': int, 'i2f': float, 'f2b': bool, 'b2f': float, 'i2b': bool, 'b2i': int, 'fasi': lambda a: a, 'iasf': lambda a: a,
}


def evaluate(decoder: Decoder, inputs=None, constants=None, curves=None, random=0.5, exec_index=0, default_curve=0.0, other_inputs=0.0) -> dict:
    """Run the script once for one particle. inputs: Particles.* values; constants: names from const() (User.* fall back
    to the system's ExposedParameters defaults);
    curves: {'Rotation Rate.FloatCurve': f(x) or number, ...} (missing curves read 0); random(x) = random * x."""
    inputs, constants, curves = inputs or {}, constants or {}, curves or {}
    reg: dict[int, object] = {}
    out: dict[str, object] = {}

    def cval(offset):
        v = decoder.const(offset)
        if isinstance(v, float):
            return v
        if v.startswith('int('):
            return int(v[4:-1])
        return constants.get(v, getattr(decoder, 'user_defaults', {}).get(v, 0.0))

    for _, name, payload in decoder.walk():
        if name == 'done':
            break
        if name.startswith('input'):
            ds, comp, dst = payload
            table = decoder.ints if 'int32' in name else decoder.floats
            key = table[comp] if ds == 0 and comp < len(table) else f'In{ds}[{comp}]'
            reg[dst] = inputs.get(key, other_inputs if key.startswith('In') else 0.0)
        elif name.startswith('output'):
            flags, (ds, _, src, dst) = payload
            table = decoder.ints if 'int32' in name else decoder.floats
            out[table[dst] if ds == 0 and dst < len(table) else f'Out{ds}[{dst}]'] = cval(src) if flags & 1 else reg.get(src, 0.0)
        elif name == 'external_func_call':
            fn, ins, outs = payload
            info = decoder.ext[fn]
            owner = info['OwnerName'].replace('Emitter.', '')
            args = [0.0 if r == 0xFFFF else (reg.get(r & 0x7FFF, 0.0) if r & 0x8000 else cval(r)) for r in ins]
            curve = curves.get(owner, default_curve)
            value = curve(*args) if callable(curve) else curve
            values = list(value) if isinstance(value, (list, tuple)) else [value] * len(outs)
            for k, r in enumerate(outs):
                reg[r] = values[k] if k < len(values) else 0.0
        elif name == 'exec_index':
            reg[payload[0]] = exec_index
        elif name in ('acquireindex', 'update_id', 'acquire_id'):
            pass
        else:
            flags, ins, o = payload
            args = [cval(r) if flags & (1 << i) else reg.get(r, 0.0) for i, r in enumerate(ins)]
            if name == 'random':
                reg[o] = random * args[0]
            elif name == 'randomi':
                reg[o] = int(random * args[0])
            else:
                reg[o] = _OPS[name](*args)
    return out


def burst_start_times(data, horizon: float = 2.5, dt: float = 1 / 60) -> dict:
    """各发射器第一次瞬时爆发（SpawnBurst.Count > 0）的发射器时刻：逐帧跑 SystemUpdateScript（状态带到下一帧）。
    原作用发射器状态模块的循环延迟 / 爆发时刻把一些层推后（如长离大招序列 Burst03 的画卷 Fire / Fire001 在 1.07 秒才出）。
    引擎输入按 1（与 burst_counts 相同的取法）。"""
    script = find_script(data, '', 'system_update')
    if script is None:
        return {}
    decoder = Decoder(script)
    decoder.setup_constants(script, interpolated=False, system=True)
    state, first = {}, {}
    for frame in range(int(horizon / dt)):
        out = evaluate(decoder, inputs=dict(state), constants={'Engine.DeltaTime': dt}, random=0.5, default_curve=1.0, other_inputs=1.0)
        state.update({k: v for k, v in out.items() if not k.startswith('Out')})
        for key, value in out.items():
            if key.endswith('.SpawnBurst.Count'):
                try:
                    count = float(value)
                except (TypeError, ValueError):
                    continue
                if count > 0:
                    first.setdefault(key.split('.')[0], frame * dt)
    return first


def burst_counts(data, random_values=(0.0, 0.5, 0.999)) -> dict:
    """Instantaneous spawn burst count per emitter, from the first SystemUpdateScript tick (min / mid / max over random)."""
    script = find_script(data, '', 'system_update')
    if script is None:
        return {}
    decoder = Decoder(script)
    decoder.setup_constants(script, interpolated=False, system=True)
    counts: dict[str, list] = {}
    for r in random_values:
        out = evaluate(decoder, inputs={}, constants={'Engine.DeltaTime': 1 / 30}, random=r, default_curve=1.0, other_inputs=1.0)
        for key, value in out.items():
            if key.endswith('.SpawnBurst.Count'):
                counts.setdefault(key.split('.')[0], []).append(int(value))
    return {k: (min(v), max(v)) for k, v in counts.items()}