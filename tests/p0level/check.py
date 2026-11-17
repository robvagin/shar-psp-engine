import sys,re
def parse(f):
    r=dict(reopen_ok=0,ioerr=0,ioerr_after=0,pwr=0,ld_loads_late=0,ld_dead=0,ld_open=0,ld_close=0,cr_open=0,cr_close=0,fd_end=-9)
    sc=None; loads=[]
    for ln in open(f,errors='replace'):
        if ln.startswith('SCENE '): sc=ln.split()[1]
        if ln.startswith('COLRING reopen ok'):
            r['reopen_ok']+=1; m=re.search(r'open=(\d+) close=(\d+)',ln)
            if m: r['cr_open']=int(m.group(1)); r['cr_close']=int(m.group(2))
        if 'WATCHDOG colring ioerr' in ln:
            r['ioerr']+=1
            if sc in ('AFTER','AFTER_END','S40_END'): r['ioerr_after']+=1
        if ln.startswith('PWR suspend'): r['pwr']+=1
        if ln.startswith('LOADERFD'):
            d=dict(re.findall(r'(\w+)=(-?\d+)',ln)); loads.append(int(d['loads'])); r['ld_dead']=int(d['dead']); r['ld_open']=int(d['open']); r['ld_close']=int(d['close']); r['fd_end']=int(d['fd'])
    if len(loads)>=4: r['ld_loads_late']=loads[-1]-loads[-4]   # загрузок за последние ~6 с: загрузчик жив?
    return r
mode=sys.argv[3] if len(sys.argv)>3 else 'one'
N=parse(sys.argv[1]); O=parse(sys.argv[2]); print('новое:',N); print('старое (p0levelold p0colringold):',O)
ok=True
if mode=='one':
    if N['pwr']!=1: print('FAIL: сна не было (PWR suspend %d)'%N['pwr']); ok=False
    if N['reopen_ok']<1: print('FAIL новое: colring не переоткрылся'); ok=False
    if N['ioerr_after']>5: print('FAIL новое: ioerr после сна не прекращается (%d)'%N['ioerr_after']); ok=False
    if N['fd_end']<0: print('FAIL новое: у загрузчика после сна fd = %d'%N['fd_end']); ok=False
    if not (O['ioerr_after']>=N['ioerr_after']+10): print('КРАСНЫЙ НЕ ПОЙМАН (colring): без лечения ошибок чтения нет'); ok=False
    if not (O['ld_close']<N['ld_close']): print('КРАСНЫЙ НЕ ПОЙМАН (уровень): прежний путь тоже закрыл мёртвый файл после сна (close %d против %d)'%(O['ld_close'],N['ld_close'])); ok=False   # прежний путь: kill-хук файл не закрывает, номер жив, но мёртв — лечения нет, закрытий нет
else:
    nl=N['ld_open']-N['ld_close']; nc=N['cr_open']-N['cr_close']; ol=O['ld_open']-O['ld_close']
    print('40 снов: открытий минус закрытий — загрузчик %d (живой файл = 1), colring %d; старое: загрузчик %d'%(nl,nc,ol))
    if nl>1 or nc>1: print('FAIL новое: утечка дескрипторов'); ok=False
    if not ol>=6: print('КРАСНЫЙ НЕ ПОЙМАН: прежний путь не копит открытия (%d)'%ol); ok=False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
