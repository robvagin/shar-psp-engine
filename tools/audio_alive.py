#!/usr/bin/env python3
"""Звук в прогоне смоука действительно ИГРАЛ, а не просто «флаги включены» (04.11, окно gates; арх-разбор К3 §2 п.2: смоук звука не слышал).
  python3 tools/audio_alive.py ПАПКА_SMOKE [--min-music-s 3] [--min-loop-s 3] [--scenes A,B]
Свидетельства берутся из строк самого движка, а не из подписи интерфейса:
  AUDV t=… cur=<дорожка> state=3 buf=<сэмплов в буфере потока> vol=… bus=… master=… user=…   музыка: дорожка выбрана (cur ≠ -), поток играет (state 3), буфер наполнен (buf > 0), громкость не нулевая
  SNDV t=… scene=… voices=… loops=… dropped=… under=…                                         звуки: голоса заняты, петли живы, нет недоборов (under) и потерь (dropped)
  STREAM s=music track=…                                                                      смена дорожек музыки
  END                                                                                         обязателен: прогон без END не доказывает ничего (код 3 — не измерено)
Проверки (каждая обязана иметь данные: пустой журнал — не «чисто», а FAIL «нет данных»):
  м  секунд с играющей музыкой (AUDV: cur ≠ -, state 3, buf > 0, vol·bus·master·user > 0) >= --min-music-s
  з  в строках SNDV (в сценах --scenes, по умолчанию все) есть секунды с voices >= 1 и loops >= 1, не меньше --min-loop-s секунд
  н  недоборов звука (under) и потерь голосов (dropped) нет ни в одной строке SNDV
Выход: 0 всё есть · 1 звук не играл или есть недоборы · 3 нет END (не измерено) · 2 нет журнала."""
import re, sys, os
from pathlib import Path


def kv(line):
    return dict(re.findall(r'(\w+)=(\S+)', line))


def fnum(x, d=0.0):
    try: return float(x)
    except (TypeError, ValueError): return d


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    opt = lambda n, d: sys.argv[sys.argv.index(n) + 1] if n in sys.argv else d
    if not a: sys.exit(__doc__)
    f = Path(a[0]) / 'smoke.txt'
    if not f.exists(): print('ЗВУК: нет журнала smoke.txt'); return 2
    min_m, min_l = float(opt('--min-music-s', 3)), float(opt('--min-loop-s', 3))
    scenes = set(x for x in opt('--scenes', '').split(',') if x)
    log = f.read_text(errors='replace').splitlines()
    if not any(l.startswith('END') for l in log): print('ЗВУК: нет строки END — не измерено'); return 3
    mus_s, tracks, loop_s, under, dropped, nsndv, nauds = 0, [], 0, 0, 0, 0, 0
    for l in log:
        if l.startswith('AUDV '):
            d = kv(l); nauds += 1
            vol = fnum(d.get('vol')) * fnum(d.get('bus')) * fnum(d.get('master')) * fnum(d.get('user'), 1.0)
            if d.get('cur', '-') != '-' and d.get('state') == '3' and fnum(d.get('buf')) > 0 and vol > 0:
                mus_s += 1; tracks.append(d['cur'].split('~')[0])
        elif l.startswith('SNDV '):
            d = kv(l)
            if scenes and d.get('scene') not in scenes: continue
            nsndv += 1
            if fnum(d.get('voices')) >= 1 and fnum(d.get('loops')) >= 1: loop_s += 1
            under += int(fnum(d.get('under'))); dropped += int(fnum(d.get('dropped')))
    res = {}
    res['м'] = (mus_s >= min_m, f'музыка играла {mus_s} с из {nauds} строк AUDV (нужно {min_m:g}); дорожки: ' + (', '.join(sorted(set(tracks))) or 'нет'))
    res['з'] = (nsndv > 0 and loop_s >= min_l, f'звуки: секунд с живыми голосами и петлями {loop_s} из {nsndv} строк SNDV (нужно {min_l:g})' + ('' if nsndv else ' — НЕТ ДАННЫХ'))
    res['н'] = (nsndv > 0 and under == 0 and dropped == 0, f'недоборов {under}, потерянных голосов {dropped}' + ('' if nsndv else ' — НЕТ ДАННЫХ'))
    ok = all(v[0] for v in res.values())
    print('ЗВУК: ' + ('PASS' if ok else 'FAIL') + ' · ' + ' · '.join(f'{k} {"ok" if v[0] else "FAIL"}' for k, v in res.items()))
    for k, v in res.items(): print(f'  {k}: {"ok" if v[0] else "FAIL"} — {v[1]}')
    return 0 if ok else 1


if __name__ == '__main__': sys.exit(main())
