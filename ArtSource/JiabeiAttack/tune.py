"""Edit JiabeiTuning.json from the command line and keep it readable (numeric arrays on one line).

  python -I tune.py R1a_Dg_02_1 Daoguang_An02001 tint=0.4,0.6,1 intensity=1.5 note=...
  python -I tune.py R1a_Dg_02_1 _ scale=1.8              (layer "_" = system-level field)
  python -I tune.py R1a_Kaiqiang_01 Glow001 emission.count=1 emission.sizeMin=35,35
  python -I tune.py R1a_Kaiqiang_01 Glow001 -emission.speed   (remove a field)
  python -I tune.py --format                                   (only reformat)
"""
import json
import re
import sys
from pathlib import Path

FILE = Path(__file__).resolve().parent / 'JiabeiTuning.json'


def parse(value: str):
    if value in ('true', 'false'):
        return value == 'true'
    parts = value.split(',')
    try:
        numbers = [float(p) if '.' in p or 'e' in p else int(p) for p in parts]
        return numbers if len(numbers) > 1 else numbers[0]
    except ValueError:
        return value


def dump(data) -> str:
    text = json.dumps(data, ensure_ascii=False, indent=2)
    # 数字数组压成一行
    return re.sub(r'\[\s+([-0-9.,\s]+?)\s+\]', lambda m: '[' + ', '.join(x.strip() for x in m.group(1).split(',')) + ']', text) + '\n'


def main(args) -> int:
    data = json.loads(FILE.read_text(encoding='utf-8'))
    if args and args[0] != '--format':
        system, layer, *edits = args
        sys_entry = data.setdefault('systems', {}).setdefault(system, {})
        target = sys_entry if layer == '_' else sys_entry.setdefault('layers', {}).setdefault(layer, {})
        for edit in edits:
            if edit.startswith('-'):
                path = edit[1:].split('.')
                node = target
                for p in path[:-1]:
                    node = node.get(p, {})
                node.pop(path[-1], None)
                continue
            key, value = edit.split('=', 1)
            path = key.split('.')
            node = target
            for p in path[:-1]:
                node = node.setdefault(p, {})
            node[path[-1]] = value if path[-1] == 'note' else parse(value)
    FILE.write_text(dump(data), encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
