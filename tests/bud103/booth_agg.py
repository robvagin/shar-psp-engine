import sys,re
for tag,path in [a.split('=',1) for a in sys.argv[1:]]:
    L=open(path,errors='replace').read().split('\n')
    lv=[re.search(r'n=(\d)',l).group(1) for l in L if l.startswith('LEVEL ev=load')][:1]
    ph=[l for l in L if l.startswith('PHONEIN')]
    n=re.search(r'n=(-?\d+)',ph[0]).group(1) if ph else '?'
    ok=sum(1 for l in ph if 'ok=1' in l)
    dbg=[l for l in L if l.startswith('BOOTHDBG')]
    # dedupe by t (duplicate lines: two calls per press)
    seen={};
    for l in dbg:
        t=re.search(r't=([\d.]+)',l).group(1); seen[t]=l
    d=list(seen.values())
    near=sum(1 for l in d if not re.search(r'near=-1',l)); off=sum(1 for l in d if 'boothsOff=1' in l)
    op=sum(1 for l in L if l.startswith('PHONE view open')); busy=sum(1 for l in L if l.startswith('PHONE busy')); fail=sum(1 for l in L if 'PHONE open-fail' in l)
    print('%s: уровень %s, будок %s, поставлен в центр %d, нажатий △ %d, будка рядом %d, выключены(boothsOff) %d, открыто %d, гудок занято %d, отказ памяти %d'%(tag,lv[0] if lv else '?',n,ok,len(d),near,off,op,busy,fail))
