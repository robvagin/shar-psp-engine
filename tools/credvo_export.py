#!/usr/bin/env python3
"""№1104, код Play Credits Dialog: реплики Кэнга и Кодос в титрах (13 диалогов, 31 клип). Клипы лежат на диске игры (conversations/c_<имя>_<n>_noboxconv_<kan|kod>.rsd,
PS-ADPCM моно, 24000 Гц) и кладутся в тот же пакет, что реплики меню (tools/fe_audio_export.py → fe_audio.sap): движок держит один дополнительный пакет потоков (audio.c
audioExtraStreams), поэтому второго файла нет; клипы идут потоком с карты, в память не грузятся. Имена дорожек: KK_<диалог>_<n>.
Порядок диалогов и их номера строк титров — в viewer/main_save.h (таблица, строки оригинала + 16 для PAL). Запуск: python tools/credvo_export.py   — печатает список клипов"""
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
DIALOGS = ('pubcredits', 'foxcredits', 'radproducer', 'radlead', 'raddesign', 'radworld', 'radmodel', 'radfx', 'radfmvart', 'radfeart', 'radprog', 'radtest', 'radsound')
CONV = ROOT / 'hit-and-run-web' / 'source' / 'game' / 'conversations'

def clips():
    """[(имя дорожки, путь)] по порядку диалогов и номеров реплик (1, 2, 3 …); номера подряд без пропусков"""
    out = []
    for d in DIALOGS:
        found = {}
        for p in CONV.iterdir():
            m = re.fullmatch(r'c_%s_(\d+)_noboxconv_(kan|kod)\.rsd' % d, p.name.lower())
            if m: found[int(m.group(1))] = p
        if not found or sorted(found) != list(range(1, len(found) + 1)): raise ValueError('credvo: реплики диалога %s не подряд: %s' % (d, sorted(found)))
        for n in sorted(found): out.append(('KK_%s_%d' % (d, n), found[n]))
    return out

if __name__ == '__main__':
    c = clips()
    print('credvo: %d клипов' % len(c))
    for name, p in c: print(' ', name, p.name)
