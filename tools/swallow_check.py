#!/usr/bin/env python3
"""Проверка «код съеден комментарием»: строка C, где после // идёт оператор (вызов, присваивание, if/else/return с ';' или '}'). Шестой случай в проекте (Fable, АУДИТЫ_2026-11-16/5_ЖЁСТКИЙ_РАЗБОР.md п.22), один из них убил таймер миссии.
  python3 tools/swallow_check.py viewer                 всё дерево (.c, .h); выход 1, если есть находки
  python3 tools/swallow_check.py --added БАЗА ВЕТКА     только строки, ДОБАВЛЕННЫЕ веткой против базы (git diff БАЗА...ВЕТКА): обязательный шаг ПЕРЕД каждым вливанием в int108b; выход 1, если есть находки
Скрипт помощника Fable (swallow3.py), режим --added добавлен Будкой."""
import subprocess
import sys,re,os
def split(line):
    i=0;n=len(line);ins=None
    while i<n:
        c=line[i]
        if ins:
            if c=='\\': i+=2; continue
            if c==ins: ins=None
        else:
            if c in '"\'': ins=c
            elif c=='/' and i+1<n and line[i+1]=='/': return line[:i], line[i:]
            elif c=='/' and i+1<n and line[i+1]=='*':
                j=line.find('*/',i+2)
                if j<0: return line[:i], line[i:]
                i=j+2; continue
        i+=1
    return line,''
# comment whose tail (after last cyrillic char) is a C statement ending with ';' or '}' 
tail=re.compile(r'[а-яА-ЯёЁ][^а-яА-ЯёЁ]*?\s((?:if \(|else |for \(|while \(|return |[A-Za-z_][\w.\[\]>-]*\s*(?:=[^=]|\+=|-=|\+\+|--|\())[^а-яА-ЯёЁ]*[;}])\s*(?://.*)?$')
# комментарий, начинающийся с номера правки и сразу кода: «// 1337 else tgtT1338 = 0.f;» (первая форма требует кириллицу перед кодом и такое пропускала)
tail2=re.compile(r'^\s*(?:№?\d+[а-яА-Я]?\s+)((?:if \(|else |for \(|while \(|return |[A-Za-z_][\w.\[\]>-]*\s*(?:=[^=]|\+=|-=|\+\+|--|\())[^а-яА-ЯёЁ]*[;}])\s*(?://.*)?$')
def check(line):
    code,com=split(line.rstrip('\n'))
    if not com or not code.strip(): return None
    seg=com[2:]; k=seg.find('   //'); first=seg if k<0 else seg[:k]
    m=tail.search(first) or tail2.search(first)
    return m.group(1)[:130] if m and len(m.group(1))>=14 else None
n=0
if sys.argv[1]=='--added':
    base,br=sys.argv[2],sys.argv[3]
    out=subprocess.run(['git','diff','-U0',f'{base}...{br}','--','viewer','*.c','*.h'],capture_output=True,text=True,errors='replace').stdout
    fn=None; ln=0
    for l in out.split('\n'):
        if l.startswith('+++ b/'): fn=l[6:]; continue
        if l.startswith('@@'):
            mm=re.search(r'\+(\d+)',l); ln=int(mm.group(1)) if mm else 0; continue
        if l.startswith('+') and not l.startswith('+++') and fn and fn.endswith(('.c','.h')):
            r=check(l[1:])
            if r: n+=1; print(f'{fn}:{ln}: {r}')
            ln+=1
else:
    root=sys.argv[1]
    for fn in sorted(os.listdir(root)):
        if not fn.endswith(('.c','.h')): continue
        if re.match(r'main_v\d',fn): continue
        for i,l in enumerate(open(os.path.join(root,fn),encoding='utf-8',errors='replace'),1):
            r=check(l)
            if r: n+=1; print(f'{fn}:{i}: {r}')
print('total',n); sys.exit(1 if n else 0)
