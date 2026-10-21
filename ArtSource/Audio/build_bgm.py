"""背景音乐：两首循环曲 + 播放事件，按 O 在两首之间切换（wuwa.BGM.Toggle）。
  python -I build_bgm.py   （需要 WAAPI，同 build_wwise.py）

曲目取自原作音乐媒体（prepare_audio.decode 解出的 wav）：
  - 黎那汐塔：拉古那狂欢节完整混音 laguna_Carnival_FullMix（事件 play_quest_music_2_0_3_carnival）；
  - 战斗：2.x 通用战斗曲 battle_beauty_common_bpm125（事件 play_quest_music_2_1_jinkugrand）。
原作是互动音乐（分段 / 状态切换），这里只做成无限循环的普通声音。之后在 UE 里 reconcile（或在编辑器里同步事件资产）。
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_audio as pa  # noqa: E402
from build_wwise import Waapi  # noqa: E402

ROOT = 'Wuwa_BGM'
TRACKS = {
    'BGM_Rinascita': ('773650859', 'laguna_Carnival_FullMix'),
    'BGM_Battle': ('492306525', 'battle_beauty_common_bpm125'),
}


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    pa.ensure_raw([f'WwiseAudio_Generated/Media/{media}.wem' for media, _ in TRACKS.values()])
    w = Waapi()
    w.call('ak.wwise.core.getInfo')
    for path in (f'\\Containers\\Default Work Unit\\{ROOT}', f'\\Events\\Default Work Unit\\{ROOT}'):
        if w.find(path):
            w.call('ak.wwise.core.object.delete', {'object': path})
    w.call('ak.wwise.core.object.create', {'parent': '\\Containers\\Default Work Unit', 'type': 'Folder', 'name': ROOT})
    w.call('ak.wwise.core.object.create', {'parent': '\\Events\\Default Work Unit', 'type': 'Folder', 'name': ROOT})
    events = []
    for name, (media, debug) in TRACKS.items():
        wav = pa.decode(media, debug)
        r = w.call('ak.wwise.core.audio.import', {
            'importOperation': 'replaceExisting', 'default': {'importLanguage': 'SFX'},
            'imports': [{'audioFile': wav, 'objectPath': f'\\Containers\\Default Work Unit\\{ROOT}\\<Sound SFX>{name}', 'originalsSubFolder': ROOT}]},
            {'return': ['id']})
        sound = r['objects'][-1]['id']
        for prop, value in (('IsLoopingEnabled', True), ('IsLoopingInfinite', True), ('IsStreamingEnabled', True), ('Volume', -4.0)):
            w.call('ak.wwise.core.object.setProperty', {'object': sound, 'property': prop, 'value': value})
        event = f'Play_{name}'
        event_id = w.call('ak.wwise.core.object.create', {'parent': f'\\Events\\Default Work Unit\\{ROOT}', 'type': 'Event', 'name': event})['id']
        w.call('ak.wwise.core.object.create', {'parent': event_id, 'type': 'Action', 'name': '', '@Target': sound, '@ActionType': 1})
        events.append(event)
    w.call('ak.wwise.core.project.save')
    w.call('ak.wwise.core.soundbank.generate', {'soundbanks': [{'name': e} for e in events], 'writeToDisk': True, 'platforms': ['Windows']}, {})
    print('built', events)
    return 0


if __name__ == '__main__':
    sys.exit(main())
