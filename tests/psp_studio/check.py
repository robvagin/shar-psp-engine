#!/usr/bin/env python3
"""Headless Chrome: real canvas fidelity, crop/reset/upload/export/state/width matrix."""
import argparse,json,re,subprocess,time,tempfile
from pathlib import Path
from urllib.parse import quote
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('html',type=Path);p.add_argument('--old',action='store_true');a=p.parse_args()
out=ROOT/'build/psp-studio/qa';out.mkdir(parents=True,exist_ok=True)
html=a.html.read_text();assert not re.search(r'(?:src|href)=[\"\']https?://',html),'network resource'
qa=r'''
<script>
(async()=>{
 const assert=(yes,n)=>{if(!yes)throw Error(n)};
 const wait=async(fn)=>{for(let i=0;i<100;i++){if(fn())return;await new Promise(r=>setTimeout(r,10))}throw Error('wait')};
 try{
 await wait(()=>window.__READY);
 const S=window.STUDIO,c=S.canvas,$=id=>document.getElementById(id),ctx=c.getContext('2d');
 await S.select(0);const original=c.toDataURL();const ref=new Image();ref.src=S.frames[0].url;await ref.decode();
 const rc=document.createElement('canvas');rc.width=480;rc.height=272;rc.getContext('2d').drawImage(ref,0,0);
 assert(original===rc.toDataURL(),'original pixels differ');
 const source=rc.getContext('2d').getImageData(0,0,480,272).data;
 $('guide').click();assert($('guides').classList.contains('on'),'guide');assert(c.toDataURL()===original,'guide changed export');$('guide').click();
 $('x').value=-1;assert(!S.crop(),'invalid crop accepted');assert(c.toDataURL()===original,'invalid crop changed pixels');
 $('x').value=0;$('w').value=240;$('h').value=136;assert(S.crop(),'valid crop');
 const cropped=ctx.getImageData(0,0,480,272).data;
 for(let i=0;i<20;i++){const x=(i*11)%240,y=(i*7)%136;for(let k=0;k<4;k++)assert(cropped[(2*y*480+2*x)*4+k]===source[(y*480+x)*4+k],'nearest crop pixels')}
 S.reset();assert(c.toDataURL()===original,'reset');
 $('next').click();await wait(()=>$('name').textContent===S.frames[1].name);$('prev').click();await wait(()=>$('name').textContent===S.frames[0].name);
 let saved;HTMLAnchorElement.prototype.click=function(){saved={name:this.download,url:this.href}};
 $('save').click();assert(saved.name==='PSP-превью.png'&&saved.url===original,'PNG export');
 const upload=async(file)=>{const dt=new DataTransfer();dt.items.add(file);$('file').files=dt.files;$('file').dispatchEvent(new Event('change'))};
 const bytes=Uint8Array.from(atob(S.frames[0].url.split(',')[1]),x=>x.charCodeAt(0));
 await upload(new File([bytes],'Кириллическая картинка.png',{type:'image/png'}));await wait(()=>$('name').textContent==='Кириллическая картинка.png');
 const count=S.frames.length;
 await upload(new File(['bad'],'сломанная.png',{type:'image/png'}));await wait(()=>$('status').classList.contains('error'));assert(S.frames.length===count,'broken file kept the old frame');
 await upload(new File(['text'],'текст.txt',{type:'text/plain'}));assert($('status').classList.contains('error'),'wrong MIME');
 assert(document.documentElement.scrollWidth<=innerWidth,'page overflow');
 document.body.dataset.qa='PASS';document.title='QA PASS';
 }catch(e){document.body.dataset.qa='FAIL '+e.message;document.title='QA FAIL';}
})();</script>
'''
if a.old:html=html.replace('ctx.imageSmoothingEnabled=false','ctx.imageSmoothingEnabled=true')
page=out/('old.html' if a.old else 'test.html');page.write_text(html.replace('</html>',qa+'</html>'))
chrome='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
widths=(1440,1280,768,390) if not a.old else (1440,)
for width in widths:
 profile=Path(tempfile.mkdtemp(prefix='cdp-',dir=out))
 cmd=[chrome,'--headless','--disable-gpu','--no-first-run','--disable-background-networking','--disable-sync','--disable-extensions',f'--user-data-dir={profile}','--remote-debugging-port=0','about:blank']
 with (out/f'{width}-{int(a.old)}.log').open('w') as log:
  browser=subprocess.Popen(cmd,stdout=log,stderr=log)
  try:
   active=profile/'DevToolsActivePort'
   for _ in range(300):
    if active.exists():break
    if browser.poll() is not None:raise RuntimeError('Chrome exited '+str(browser.returncode))
    time.sleep(.1)
   port,ws=active.read_text().splitlines()[:2]
   r=subprocess.run(['node','tests/psp_studio/cdp.mjs','ws://127.0.0.1:'+port+ws,'file://'+quote(str(page.resolve())),str(width),str(out/f'{width}-{int(a.old)}.png')],capture_output=True,text=True,timeout=60)
   assert r.returncode==0,r.stdout+r.stderr
   result=json.loads(r.stdout);answer=result['qa']
   assert result['width']==width,('viewport mismatch',result,width)
   assert result['scroll']<=width,('page overflow',result)
   print(width,answer,'actual viewport',result['width'],flush=True)
   if a.old:assert answer.startswith('FAIL nearest crop'),answer
   else:assert answer=='PASS',answer
  finally:
   browser.terminate()
   try:browser.wait(timeout=10)
   except subprocess.TimeoutExpired:browser.kill();browser.wait()
print('PASS: OLD blurred crop caught' if a.old else 'PASS: real browser contracts at four exact widths')
