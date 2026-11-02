#!/usr/bin/env python3
"""№1111: тексты вопросов паузы и главного меню (выход, новая игра, перезапуск и отмена миссии) на выбранном языке -> prompts.txt, по строке на вопрос, коды знаков шрифтов игры.
Движок (viewer/savemenu.c) читает lang/<буква>/prompts.txt; английские остаются в коде. Ключи текстовой библиотеки диска: MSG_CONFIRM_QUIT, MSG_CONFIRM_NEW_GAME, MSG_CONFIRM_RESTART, MSG_CONFIRM_ABORT.
  LANG_BIBLE=F python3 tools/lang_prompts.py --out build/lang/F/prompts.txt"""
import os, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_export as U
import accents

KEYS = ('MSG_CONFIRM_QUIT', 'MSG_CONFIRM_NEW_GAME', 'MSG_CONFIRM_RESTART', 'MSG_CONFIRM_ABORT')

def main():
    out = sys.argv[sys.argv.index('--out') + 1]
    B = U.load_bible(U.LANG); lines = []
    for k in KEYS:
        t = B(k)
        if not t:
            sys.exit('нет ключа %s в таблице %s' % (k, U.LANG))
        lines.append(accents.font_bytes(t.replace('\\', ' ')))
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_bytes(b'\n'.join(lines) + b'\n')

if __name__ == '__main__':
    main()
