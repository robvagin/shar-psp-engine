#!/usr/bin/env python3
"""SHAR for PSP: страница-сборщик в браузере. Местный сервер только на 127.0.0.1: отдаёт app/ui, принимает образ диска,
ведёт сборку (app/core/build.sh) и копирует готовое на PSP. Только стандартная библиотека Python 3.8+.
  python3 app/server.py [--port 8642] [--no-open]
API: GET /api/state · GET /api/events (SSE) · POST /api/disc (тело — образ, X-File-Name) · POST /api/stop · POST /api/put · POST /api/show"""
import argparse, hashlib, http.server, json, os, queue, re, shutil, signal, subprocess, sys, threading, time, urllib.parse, webbrowser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / 'app/ui'
sys.path.insert(0, str(ROOT / 'app/core'))
from iso_id import identify   # noqa: E402

LOG = ROOT / 'build/app/build.log'
TYPES = {'.html': 'text/html; charset=utf-8', '.svg': 'image/svg+xml', '.woff2': 'font/woff2', '.js': 'text/javascript', '.css': 'text/css', '.txt': 'text/plain'}


class Core:
    """Одно состояние на машину: что показывает экран PSP. Меняется только под замком, каждое изменение уходит всем страницам."""
    def __init__(self):
        self.lock = threading.Lock(); self.subs = []; self.proc = None
        self.s = {'state': 'idle'}

    def set(self, **kw):
        with self.lock:
            self.s = dict(kw); msg = json.dumps(self.s)
            for q in list(self.subs): q.put(msg)

    def patch(self, **kw):
        with self.lock:
            self.s.update(kw); msg = json.dumps(self.s)
            for q in list(self.subs): q.put(msg)

    # --- сборка
    def start(self, iso):
        if self.proc and self.proc.poll() is None: return
        LOG.parent.mkdir(parents=True, exist_ok=True)
        self.proc = subprocess.Popen(['bash', str(ROOT / 'app/core/build.sh'), str(iso)], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, encoding='utf-8', errors='replace', bufsize=1, start_new_session=True)   # вывод установки не всегда UTF-8 (полосы curl)
        threading.Thread(target=self._follow, args=(self.proc,), daemon=True).start()

    def _follow(self, p):
        t0 = time.time(); p0 = None; err = ''
        self.set(state='build', step=1, total=5, title='Setting up the tools', pct=0, eta='')
        with open(LOG, 'a', encoding='utf-8') as log:
            log.write(f'\n--- {time.strftime("%Y-%m-%d %H:%M:%S")} build\n')
            for line in p.stdout:   # поток не должен падать от любой строки: иначе сборка идёт, а экран стоит
                log.write(line); log.flush()
                if not line.startswith('@@ '): continue
                try: ev = json.loads(line[3:])
                except ValueError: continue
                if not isinstance(ev, dict): continue
                if 'error' in ev: err = str(ev['error']); continue
                if not all(k in ev for k in ('step', 'total', 'title')): continue
                pct = float(ev.get('pct', 0))
                if p0 is None and pct > 0: p0, t0 = pct, time.time()   # отсчёт с первой точки этого запуска (продолжение идёт быстрее)
                self.patch(state='build', step=ev['step'], total=ev['total'], title=ev['title'], pct=pct, eta=self._eta(t0, p0, pct))
        rc = p.wait()
        if rc == 0 and self.s.get('pct', 0) >= 100: self.set(state='done', size=_size_gb(ROOT / 'build'))
        elif rc in (-signal.SIGTERM, 143) or self.s.get('state') == 'idle': self.set(state='idle')
        else: self.set(state='error', title='The build stopped', note=err or 'See build/app/build.log for details')

    @staticmethod
    def _eta(t0, p0, pct):
        if p0 is None or pct - p0 < 1: return 'working it out'
        left = (time.time() - t0) * (100 - pct) / (pct - p0)
        if left < 90: return 'about a minute left'
        if left < 3600: return f'about {round(left / 60)} min left'
        return f'about {left / 3600:.1f} h left'

    def stop(self):
        p = self.proc
        if p and p.poll() is None:
            self.set(state='idle')
            try: os.killpg(p.pid, signal.SIGTERM)
            except ProcessLookupError: pass

    # --- на PSP: только изменившиеся файлы набора (build/ship_manifest.txt), сверка SHA-256 после копии
    def put(self):
        threading.Thread(target=self._put, daemon=True).start()

    def _put(self):
        man = ROOT / 'build/ship_manifest.txt'
        if not man.exists(): self.set(state='error', title='Nothing to copy yet', note='Build the game first'); return
        files = [l.split(None, 1) for l in man.read_text().splitlines()[2:] if re.match(r'^[0-9a-f]{64}\s', l)]
        self.set(state='copy', pct=0, title='Connect your PSP by USB', note='Settings › USB Connection on the PSP')
        root = None
        for _ in range(600):   # 10 минут ждём карту
            root = psp_root()
            if root: break
            time.sleep(1)
        if not root: self.set(state='done', size=_size_gb(ROOT / 'build')); return
        dst = Path(root) / 'PSP/GAME/SHARVIEW'; total = sum((ROOT / 'build' / n.strip()).stat().st_size for _, n in files) or 1; done = 0
        self.patch(title='Copying to PSP', note='Only changed files. Do not unplug the PSP until this finishes')
        try:
            for h, n in files:
                n = n.strip(); src = ROOT / 'build' / n; d = dst / n; size = src.stat().st_size
                if not (d.exists() and d.stat().st_size == size and _sha(d) == h):
                    d.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(src, d)
                    if _sha(d) != h: raise OSError(f'{n} did not match after copying')
                done += size; self.patch(pct=round(done * 100 / total, 1))
            for junk in dst.rglob('._*'): junk.unlink(missing_ok=True)
        except OSError as e:
            self.set(state='error', title='Copying stopped', note=str(e)); return
        self.set(state='onpsp')

    def show(self):
        target = ROOT / 'build'
        if sys.platform == 'darwin': subprocess.Popen(['open', str(target)])
        elif 'microsoft' in os.uname().release.lower(): subprocess.Popen(['explorer.exe', subprocess.run(['wslpath', '-w', str(target)], capture_output=True, text=True).stdout.strip()])
        else: subprocess.Popen(['xdg-open', str(target)])


def _sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()


def _size_gb(d):
    man = d / 'ship_manifest.txt'
    if not man.exists(): return ''
    total = sum((d / l.split(None, 1)[1].strip()).stat().st_size for l in man.read_text().splitlines()[2:] if re.match(r'^[0-9a-f]{64}\s', l))
    return f'{total / 1e9:.1f} GB'


def psp_root():
    """Корень карты PSP: macOS /Volumes/PSP, Linux /media/$USER/PSP и др., WSL — буквы дисков Windows; иначе любой том с PSP/GAME"""
    if os.environ.get('SHIP_PSP'): return os.environ['SHIP_PSP']   # свой путь к карте (как tools/portable.sh; проверка без настоящей PSP)
    user = os.environ.get('USER', '')
    for d in ['/Volumes/PSP', f'/media/{user}/PSP', f'/run/media/{user}/PSP', '/media/PSP', '/mnt/PSP']:
        if Path(d, 'PSP').is_dir(): return d
    for base in ['/Volumes', f'/media/{user}', f'/run/media/{user}', '/mnt']:
        try:
            for d in Path(base).iterdir():
                if d.name in ('c', 'wsl', 'wslg') : continue   # WSL: системный диск Windows не карта
                if (d / 'PSP/GAME').is_dir(): return str(d)
        except OSError:
            pass
    return None


CORE = Core()


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *a): pass

    def _json(self, obj, code=200):
        b = json.dumps(obj).encode(); self.send_response(code); self.send_header('Content-Type', 'application/json'); self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)

    def _local(self):   # только эта машина и только наша страница (чужой сайт не отправит образ или команду)
        origin = self.headers.get('Origin')
        return self.client_address[0] in ('127.0.0.1', '::1') and (origin is None or re.match(r'^http://(127\.0\.0\.1|localhost):\d+$', origin))

    def do_GET(self):
        path = urllib.parse.urlparse(self.path).path
        if path == '/api/state': return self._json(CORE.s)
        if path == '/api/events': return self._events()
        f = (UI / (path.lstrip('/') or 'index.html')).resolve()
        if UI.resolve() not in f.parents or not f.is_file(): self.send_error(404); return
        b = f.read_bytes(); self.send_response(200); self.send_header('Content-Type', TYPES.get(f.suffix, 'application/octet-stream'))
        self.send_header('Content-Length', str(len(b))); self.send_header('Cache-Control', 'no-store'); self.end_headers(); self.wfile.write(b)

    def _events(self):
        q = queue.Queue(); q.put(json.dumps(CORE.s))
        with CORE.lock: CORE.subs.append(q)
        self.send_response(200); self.send_header('Content-Type', 'text/event-stream'); self.send_header('Cache-Control', 'no-store'); self.end_headers()
        try:
            while True:
                try: msg = q.get(timeout=15)
                except queue.Empty: msg = None
                self.wfile.write((f'data: {msg}\n\n' if msg else ': ping\n\n').encode()); self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with CORE.lock: CORE.subs.remove(q)

    def do_POST(self):
        if not self._local(): self.send_error(403); return
        path = urllib.parse.urlparse(self.path).path
        if path == '/api/disc': return self._disc()
        if path == '/api/stop': CORE.stop(); return self._json({'ok': True})
        if path == '/api/put': CORE.put(); return self._json({'ok': True})
        if path == '/api/show': CORE.show(); return self._json({'ok': True})
        self.send_error(404)

    def _disc(self):
        if CORE.proc and CORE.proc.poll() is None: return self._json({'ok': False, 'why': 'a build is already running'}, 409)
        n = int(self.headers.get('Content-Length', 0)); name = urllib.parse.unquote(self.headers.get('X-File-Name', 'disc.iso'))
        name = re.sub(r'[^A-Za-z0-9._ -]', '_', Path(name).name) or 'disc.iso'
        game = ROOT / 'game'; game.mkdir(exist_ok=True); dst = game / name; tmp = dst.with_suffix(dst.suffix + '.part')
        CORE.set(state='check', title='Reading the disc', note='Copying the image next to the builder', pct=0)
        got = 0
        with open(tmp, 'wb') as f:
            while got < n:
                b = self.rfile.read(min(1 << 20, n - got))
                if not b: break
                f.write(b); got += len(b)
                if got % (64 << 20) < (1 << 20): CORE.patch(pct=round(got * 100 / n))
        if got != n: tmp.unlink(missing_ok=True); CORE.set(state='idle'); return self._json({'ok': False, 'why': 'the upload stopped'}, 400)
        tmp.replace(dst)
        ident = identify(dst)
        if not ident.get('ok'):
            dst.unlink(missing_ok=True)
            CORE.set(state='error', title='This is a different game' if ident.get('exe') else 'This is not a PS2 disc image',
                     note='Expected The Simpsons: Hit & Run, SLES 518.97 (PAL) or SLUS 206.24 (NTSC)')
            return self._json(ident, 400)
        CORE.set(state='check', title=f'{ident["region"]} disc found', note=ident['exe'].replace('_', ' '), pct=100)
        time.sleep(1.2); CORE.start(dst)
        return self._json(ident)


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--port', type=int, default=8642); ap.add_argument('--no-open', action='store_true'); a = ap.parse_args()
    srv = None
    for port in range(a.port, a.port + 20):
        try: srv = http.server.ThreadingHTTPServer(('127.0.0.1', port), Handler); break
        except OSError: continue
    if not srv: sys.exit('no free port')
    url = f'http://127.0.0.1:{srv.server_address[1]}/'
    print(f'Hit & Run PSP Builder: {url}  (keep this window open; Ctrl+C stops)', flush=True)
    def _open():   # WSL: браузер Windows (WSL2 отдаёт 127.0.0.1 в Windows); иначе системный
        if 'microsoft' in os.uname().release.lower(): subprocess.Popen(['cmd.exe', '/c', 'start', url], stderr=subprocess.DEVNULL)
        else: webbrowser.open(url)
    if not a.no_open: threading.Timer(.6, _open).start()
    try: srv.serve_forever()
    except KeyboardInterrupt: CORE.stop()


if __name__ == '__main__':
    main()
