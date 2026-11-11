#!/usr/bin/env python3
"""Гейт Модели-96: поза нового .shr против старого, по всем клипам, в каждом кадре и между кадрами, через читатель skin.c.
  gate.py OLD.shr NEW.shr [--scale K]   -> строка PASS/FAIL, макс. ошибки: поворот сустава (град), перенос сустава и корня (мм), положение сустава в модели (мм)
Пороги: поворот < 0.2°, перенос < 2 мм, положение сустава в модели < 8 мм (≈ полпикселя Мардж на экране PSP) (цепочка); K множит пороги (красный контроль: --scale 0.01 должен падать)."""
import sys, subprocess, struct, os, math, numpy as np, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce

def build():
    exe = Path(tempfile.gettempdir()) / 'animpack96_dump'
    env = dict(os.environ); env.setdefault('SDKROOT', '/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk')
    subprocess.check_call(['/Library/Developer/CommandLineTools/usr/bin/clang', '-std=c11', '-O1', '-Wno-unused-function', '-DSKIN_HOST', '-I', str(ROOT / 'viewer'), str(ROOT / 'tests/animpack96/dump.c'),
                           str(ROOT / 'viewer/skin.c'), '-lm', '-o', str(exe)], env=env)
    return exe

def dump(exe, shr):
    out = Path(tempfile.gettempdir()) / f'animpack96_{os.getpid()}.bin'
    subprocess.check_call([str(exe), str(shr), str(out)])
    b = out.read_bytes(); out.unlink()
    nc, nj = struct.unpack_from('<II', b, 0); p = 8; clips = []
    for _ in range(nc):
        ns, = struct.unpack_from('<I', b, p); p += 4
        a = np.frombuffer(b, '<f4', ns * (nj * 7 + 3), p).reshape(ns, nj * 7 + 3); p += ns * (nj * 7 + 3) * 4
        clips.append((a[:, :nj * 7].reshape(ns, nj, 7).astype(np.float64), a[:, nj * 7:].astype(np.float64)))
    return clips

def fk(joints, q, t):
    """положения суставов в модели: q (ns,nj,4), t (ns,nj,3) -> (ns,nj,3)"""
    ns, nj = q.shape[:2]; W = np.zeros((ns, nj, 4, 4)); pos = np.zeros((ns, nj, 3))
    for j, jt in enumerate(joints):
        L = np.zeros((ns, 4, 4)); L[:, 3, 3] = 1
        for s in range(ns): L[s, :3, :3] = ce.m_from_q(q[s, j]) * jt['s']
        L[:, :3, 3] = t[:, j]
        W[:, j] = L if jt['parent'] < 0 else np.einsum('sij,sjk->sik', W[:, jt['parent']], L)
        pos[:, j] = W[:, j, :3, 3]
    return pos

def compare(old, new, k=1.0, exe=None):
    exe = exe or build(); a = dump(exe, old); b = dump(exe, new)
    m = ce.read_chr(Path(old)); joints = m['joints']
    assert len(a) == len(b), 'clip count differs'
    worst = dict(rot=0., tr=0., root=0., pos=0.); where = dict(rot='', tr='', root='', pos='')
    nsamp = 0
    for c, ((qa, ra), (qb, rb)) in enumerate(zip(a, b)):
        assert qa.shape == qb.shape, f'clip {c}: sample count'
        d = np.abs(np.sum(qa[..., :4] * qb[..., :4], axis=-1)).clip(0, 1); rot = np.degrees(2 * np.arccos(d)).max()
        tr = np.abs(qa[..., 4:] - qb[..., 4:]).max() * 1000; root = np.abs(ra - rb).max() * 1000
        pa = fk(joints, qa[..., :4], qa[..., 4:]); pb = fk(joints, qb[..., :4], qb[..., 4:]); pos = np.linalg.norm(pa - pb, axis=-1).max() * 1000
        nsamp += qa.shape[0]
        for key, v in (('rot', rot), ('tr', tr), ('root', root), ('pos', pos)):
            if v > worst[key]: worst[key] = v; where[key] = m['clips'][c]['name']
    lim = dict(rot=0.2 * k, tr=2.0 * k, root=2.0 * k, pos=8.0 * k)
    ok = all(worst[x] <= lim[x] for x in worst)
    msg = (f"{len(a)} клипов, {nsamp} поз; макс. ошибка: поворот {worst['rot']:.3f}° ({where['rot']}), перенос {worst['tr']:.3f} мм, корень {worst['root']:.3f} мм ({where['root']}), "
          f"положение сустава {worst['pos']:.3f} мм ({where['pos']}); пороги {lim['rot']:.3f}°/{lim['tr']:.3f}/{lim['root']:.3f}/{lim['pos']:.3f} мм")
    return ok, msg, worst

def main():
    old, new = sys.argv[1], sys.argv[2]; k = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1.0
    ok, msg, _ = compare(old, new, k); print(msg); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)

if __name__ == '__main__': main()
