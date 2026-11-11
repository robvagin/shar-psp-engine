#!/usr/bin/env python3
"""Гейт: гонка на ставку (gr1) - взнос списывается при X на карточке, этап взноса закрывается, идёт этап гонки.
Использование: wager_check.py smoke.txt [--fee N] [--coins N]   (по умолчанию взнос 20 для L1, монет 300)
Проверки (по журналу эмулятора): MSN wager-accept; ровно одно MSN wager-fee coins=COINS-FEE (кошелёк после взноса); после него MSN stage gr1.1.1 (гонка).
Красный контроль: старый бинарник или flag wageracceptold 1 - этап остаётся gr1.1.0, списания нет."""
import re, sys
path = sys.argv[1]; fee = 20; coins = 300
if '--fee' in sys.argv: fee = int(sys.argv[sys.argv.index('--fee') + 1])
if '--coins' in sys.argv: coins = int(sys.argv[sys.argv.index('--coins') + 1])
L = open(path, errors='replace').read().splitlines(); f = []
acc = next((i for i, l in enumerate(L) if l.startswith('MSN wager-accept')), None)
pay = [i for i, l in enumerate(L) if re.match(r'MSN wager-fee coins=%d stage=' % (coins - fee), l)]
race = next((i for i, l in enumerate(L) if l.startswith('MSN stage gr1.1.1')), None)
if acc is None: f.append('нет MSN wager-accept (X на карточке не дошёл до взноса)')
if len(pay) != 1: f.append('списаний взноса %d (ждали 1: кошелёк %d после взноса %d)' % (len(pay), coins - fee, fee))
if race is None: f.append('этап гонки gr1.1.1 не начался')
elif pay and race < pay[0]: f.append('гонка началась раньше списания')
print('CHECK wager accept=%s pay=%d race=%s' % (acc is not None, len(pay), race is not None))
for x in f: print('CHECK FAIL:', x)
print('RESULT wager %s' % ('FAIL' if f else 'OK')); sys.exit(1 if f else 0)
