#!/usr/bin/env python3
"""Родной MFK -> команды MSN1 -> общий host/trafficUpdate; настоящий старый код красный"""
import os,re,shlex,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];os.chdir(root)
out=root/'build/pedgroup';out.mkdir(exist_ok=True)
lib=shlex.split(subprocess.check_output(['bash','-c','V=viewer; . tools/bots/lib.sh; printf "%s" "$BOTS_LIB"'],text=True))
source=root/'hit-and-run-web/source/game/scripts/missions'
results={}
# №1132: этот тест держит прежнее «миссия поверх ворот» (PEDZONE1132_OLD=1); новое правило проверяет tests/pedzone
for side,flags in [('new',['-DPEDZONE1132_OLD=1']),('old',['-DPEDZONE1132_OLD=1','-DUSE_PED_GROUP_OLD=1'])]:
 binary=out/side
 cmd=['cc','-std=c11','-O2','-w','-DSKIN_HOST','-Iviewer','-Itools/bots',*flags,'tests/pedgroup/host.c',*lib,'-lm','-o',str(binary)]
 with (out/(side+'.cc')).open('w') as f:
  if subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode:raise SystemExit(2)
 rows=[]
 for level in range(1,8):
  stage=root/'build/ship_stage';pack=stage if level==1 else stage/f'l{level}'
  r=subprocess.run([str(binary),str(pack/'level.shr'),str(stage/'homer.shr'),str(pack/f'level{level:02d}.msn')],capture_output=True,text=True,env=dict(os.environ,BW_NO_ROOMS='1',BW_NO_COLLECT='1',BW_NO_AUDIO='1'))
  (out/f'L{level}-{side}.log').write_text(r.stdout+r.stderr)
  if r.returncode not in (0,1):raise SystemExit(f'fixture failed L{level}: rc={r.returncode}')
  expected=[]
  for name in re.findall(r'^FILE (.+)$',r.stdout,re.M):
   p=source/name
   if not p.is_file():p=source/f'level{level:02d}'/Path(name).name
   if not p.is_file():raise SystemExit(f'missing raw MFK {name}')
   for line,text in enumerate(p.read_text().splitlines(),1):
    text=re.sub(r'//.*','',text)
    m=re.search(r'\bUsePedGroup\s*\(\s*(\d+)\s*\)',text)
    if m:expected.append((name,str(line),m[1]))
  actual=re.findall(r'^SOURCE (.+) (\d+) (\d+)$',r.stdout,re.M)
  if sorted(actual)!=sorted(expected):raise SystemExit(f'FAIL authored command coverage L{level}: {len(actual)}/{len(expected)}')
  print(f'L{level} {side} rc={r.returncode} nativecommands={len(actual)}',flush=True)
  rows.append((r.returncode,len(actual)))
 results[side]=rows
ok=all(rc==0 for rc,n in results['new']) and any(rc==1 and n for rc,n in results['old'])
print('PASS native seven-level commands/spawn/zone/cleanup; OLD caught' if ok else 'FAIL')
raise SystemExit(not ok)
