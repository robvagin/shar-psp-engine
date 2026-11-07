#!/usr/bin/env python3
"""Сверка прогонов гейта сохранений в эмуляторе (tests/savegame/emu.sh) по строкам smoke.txt (viewer/savemenu.c, main.c saveLog):
  save    — запись прошла и подтверждена чтением (SVIO op=save res=00000000 verify=O), «Save Successful», вопрос «выйти?» (quit_confirm; второго вопроса
            о записи нет: в этой паузе уже сохраняли, P13-Д4) и выход в меню (вторая строка MENU), игра не упала (END)
  load    — при запуске слот 1 целый (SVSCAN O), загрузка слота 1 (LOADBACK: назад в главное меню), RESUME GAME, SVPROG src=load = прогресс перед записью (SVPROG src=mark
            прогона 1) по полям msn/cards/wasps/gags/coins/wcoins/vol/radar
  resume  — RESUME GAME: SVLATEST при старте меню, SVPROG src=load тот же прогресс
  corrupt — слот 1 испорчен (SVSCAN D), в LOAD GAME он серый: сообщение «нет данных» (MSG_LOAD_CARD_EMPTY_(PS2)), запись НЕ удаляется (P14-5, как PS2),
            игра дошла до NEW GAME без записи (SVPROG src=new: прогресс пустой) и END — не упала
  cut     — (№1170) обрыв питания посреди записи слота файлом (flag svcut 1): слот цел после «перезагрузки» (flag svrescan); прежний порядок записи оставляет его пустым
    emu_check.py SAVE_DIR LOAD_DIR [RESUME_DIR CORRUPT_DIR [CUT_DIR]]   -> первая строка итог, код 0 = OK
"""
import sys
from pathlib import Path

KEYS = ('msn', 'cards', 'wasps', 'gags', 'coins', 'wcoins', 'vol', 'radar')


def lines(d):
    p = Path(d) / 'smoke.txt'
    return p.read_text(errors='replace').splitlines() if p.exists() else []


def kv(line):
    return dict(p.split('=', 1) for p in line.split()[1:] if '=' in p)


def prog(line):
    d = kv(line)
    return {k: d.get(k) for k in KEYS}


def first(log, head, **want):
    for l in log:
        if l.startswith(head + ' ') and all(kv(l).get(k) == v for k, v in want.items()):
            return l
    return None


def main():
    dirs = sys.argv[1:]
    fails, notes = [], []
    save = lines(dirs[0])
    if not any(l.startswith('END') for l in save): fails.append('прогон 1: нет END (упал / завис)')
    mark = first(save, 'SVPROG', src='mark')
    if not mark: fails.append('прогон 1: нет отметки прогресса (SVPROG src=mark)')
    io = first(save, 'SVIO', op='save')
    if not io or kv(io).get('res') != '00000000' or kv(io).get('verify') != 'O':
        fails.append('прогон 1: запись не подтверждена (%s)' % (io or 'нет SVIO op=save'))
    if not first(save, 'SVMSG', key='MSG_SAVE_SUCCESSFUL'): fails.append('прогон 1: нет «Save Successful»')
    # g09b-menu 02.11 (P13-1, P13-Д4): сначала «Are you sure you want to quit?» (quit_confirm); второй вопрос «сохранить?» (MSG_CONFIRM_SAVE_BEFORE_QUIT) — только если в этой паузе
    # не сохраняли; прогон 1 сохраняет, значит второго вопроса быть не должно (SVM_QUITASK_OLD / SVM_SAVEDFLAG_OLD вернули бы прежний порядок)
    if not first(save, 'SVMSG', key='quit_confirm'): fails.append('прогон 1: QUIT GAME без вопроса «выйти?»')
    if first(save, 'SVMSG', key='MSG_CONFIRM_SAVE_BEFORE_QUIT'): fails.append('прогон 1: после записи выход спросил «сохранить?» второй раз')
    if sum(l.startswith('MENU ') for l in save) < 2: fails.append('прогон 1: после QUIT GAME нет выхода в главное меню')
    want = prog(mark) if mark else None
    load = lines(dirs[1])
    if not any(l.startswith('END') for l in load): fails.append('прогон 2: нет END (упал / завис)')
    sc = first(load, 'SVSCAN')
    if not sc or not kv(sc).get('slots', '').startswith('O'): fails.append('прогон 2: при запуске слот 1 не целый (%s)' % sc)
    if not first(load, 'LOADBACK'): fails.append('прогон 2: после загрузки слота главное меню не вернулось (нет LOADBACK, правка 448: LOAD GAME не стартует игру, стартует RESUME GAME)')
    got = first(load, 'SVPROG', src='load')
    if not got: fails.append('прогон 2: загрузки не было (нет SVPROG src=load) — прогресс слетел')
    elif want and prog(got) != want:
        diff = ', '.join('%s %s->%s' % (k, want[k], prog(got)[k]) for k in KEYS if want[k] != prog(got)[k])
        fails.append('прогон 2: прогресс после загрузки другой: ' + diff)
    else: notes.append('прогресс после перезапуска тот же: ' + ' '.join('%s=%s' % (k, want[k]) for k in KEYS))
    if len(dirs) > 2:
        res = lines(dirs[2])
        if not first(res, 'SVLATEST'): fails.append('прогон 3: нет самого свежего сохранения для RESUME GAME')
        g = first(res, 'SVPROG', src='load')
        if not g or (want and prog(g) != want): fails.append('прогон 3: RESUME GAME не вернул прогресс (%s)' % g)
        else: notes.append('RESUME GAME: тот же прогресс')
    if len(dirs) > 3:
        cor = lines(dirs[3])
        if not any(l.startswith('END') for l in cor): fails.append('прогон 4: с испорченной записью нет END (упал / завис)')
        sc = first(cor, 'SVSCAN')
        if not sc or not kv(sc).get('slots', '').startswith('D'): fails.append('прогон 4: испорченный слот не опознан (%s)' % sc)
        # g09b-menu P14-5 (SVM_LOADDAMAGED): в LOAD GAME повреждённый слот серый и не выбирается, удаления из загрузки нет (как PS2); если годных слотов нет — «No … save data present»
        if not first(cor, 'SVMSG', key='MSG_LOAD_CARD_EMPTY_(PS2)'): fails.append('прогон 4: испорченный слот не дал «нет данных» (MSG_LOAD_CARD_EMPTY_(PS2))')
        if first(cor, 'SVMSG', key='ERROR_DATA_CORRUPT_LOADING_(PS2)'): fails.append('прогон 4: испорченный слот предложен к загрузке (должен быть серым)')
        if first(cor, 'SVIO', op='delete'): fails.append('прогон 4: загрузка удалила испорченную запись (в PS2 удаления из загрузки нет)')
        g = first(cor, 'SVPROG', src='new')
        if not g or kv(g).get('msn') != '00000000' or kv(g).get('coins') != '0':
            fails.append('прогон 4: новая игра не с нуля (%s)' % g)
        elif not first(cor, 'GAME'): fails.append('прогон 4: игра не началась')
        else: notes.append('испорченная запись: опознана, серая («нет данных»), не удалена, новая игра с нуля')
    if len(dirs) > 4:   # прогон 5 (№1170): обрыв питания посреди записи слота файлом — слот цел
        cut = lines(dirs[4])
        if not any(l.startswith('END') for l in cut): fails.append('прогон 5: нет END (упал / завис / не успел дойти до LOAD GAME)')
        sc0 = first(cut, 'SVSCAN')
        if not sc0 or not kv(sc0).get('slots', '').startswith('O'): fails.append('прогон 5: готовая запись слота не принята при запуске (%s)' % sc0)
        sv = [l for l in cut if l.startswith('SVFILE ') and kv(l).get('op') == 'save']
        if not any(kv(l).get('res') == '-9' for l in sv): fails.append('прогон 5: обрыв не сработал (нет SVFILE op=save res=-9: EBOOT без хука svcut?)')
        scans = [l for l in cut if l.startswith('SVSCAN ')]
        if len(scans) < 2: fails.append('прогон 5: после обрыва нет повторного чтения слотов (flag svrescan: нет второй строки SVSCAN)')
        elif not kv(scans[-1]).get('slots', '').startswith('O'): fails.append('прогон 5: после обрыва записи слот пропал (повторный SVSCAN slots=%s)' % kv(scans[-1]).get('slots'))
        else: notes.append('обрыв питания посреди записи: слот цел после перезагрузки (SVSCAN slots=%s)' % kv(scans[-1]).get('slots'))
    # 28.09 (проверяющий): подписи слотов попадали на кнопки вопроса/сообщения. Что нарисовано на кнопках — строки SVROWS
    rows = [kv(l) for d in dirs for l in lines(d) if l.startswith('SVROWS')]
    up = lambda x: (x or '').upper()
    badr = [r for r in rows if (r.get('scr') == 'notice' and up(r.get('r0')) != 'CONTINUE') or
            (r.get('scr') == 'prompt' and (up(r.get('r0')), up(r.get('r1'))) != ('YES', 'NO'))]
    if not any(r.get('scr') == 'notice' for r in rows): fails.append('нет ни одного экрана сообщения (SVROWS scr=notice) — кнопки не проверены')
    elif badr: fails.append('на кнопках не их подписи: ' + '; '.join('%s r0=%s r1=%s' % (r.get('scr'), r.get('r0'), r.get('r1')) for r in badr[:3]))
    else: notes.append('кнопки вопросов и сообщений — YES/NO и CONTINUE (%d экранов)' % len(rows))
    # прогресс через 5 с игры после загрузки (src=now) — подгрузка клеток не вернула собранное
    now = first(load, 'SVPROG', src='now')
    if want and (not now or prog(now) != want): fails.append('прогон 2: через 5 с игры прогресс другой (%s)' % now)
    if fails:
        print('FAIL: ' + '; '.join(fails)); return 1
    print('OK: ' + '; '.join(notes)); return 0


if __name__ == '__main__':
    sys.exit(main())
