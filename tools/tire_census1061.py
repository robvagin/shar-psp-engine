#!/usr/bin/env python3
"""№1061 перепись покрышек: по всем машинам диска (art/cars/*.p3d) для каждого колеса w0..w3 — материалы (шейдер, картинка), треугольники
протектора (нормаль поперёк оси колеса) и боковины (вдоль оси), средний цвет картинки в точках протектора и боковины, общая ли картинка с кузовом.
  .venv/bin/python tools/tire_census1061.py OUT_DIR   -> OUT_DIR/tire_census1061.tsv
"""
import json, os, re, sys
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent))
import psp_export as pe   # noqa: E402
from convert import Converter, GAME, OUT   # noqa: E402

W = np.array([(a, b, 1 - a - b) for a in np.linspace(0, 1, 7) for b in np.linspace(0, 1, 7) if a + b <= 1])


def tex_img(url):
    img = Image.open(pe.ASSETS / url).convert('RGBA')
    if url in pe.PNG_SRC and os.environ.get('SHR_PNG_FLIP', '1') != '0': img = img.transpose(Image.FLIP_TOP_BOTTOM)
    return np.asarray(img, np.float32) / 255.


def sample(im, U):
    H, Wd = im.shape[:2]; q = np.einsum('wk,tkc->twc', W, U).reshape(-1, 2)
    x = (np.mod(q[:, 0], 1) * Wd).astype(int).clip(0, Wd - 1); y = (np.mod(q[:, 1], 1) * H).astype(int).clip(0, H - 1)
    px = im[y, x]; k = px[:, 3] > .5
    return px[k, :3].mean(0) if k.any() else None


def rows_for(name, meta, raw):
    body_urls = set(); out = []
    for obj in meta['objects']:
        if re.fullmatch(r'w[0-3]', obj['name']): continue
        for prim in meta['meshes'].get(obj['mesh'], []): body_urls.add(meta['materials'].get(prim['shader'], {}).get('textureUrl', ''))
    for obj in meta['objects']:
        if not re.fullmatch(r'w[0-3]', obj['name']): continue
        for prim in meta['meshes'].get(obj['mesh'], []):
            at = prim['attributes']; g = lambda k, w: np.frombuffer(raw, '<f4', at[k][1], at[k][0]).reshape(-1, w)
            pos, uv = g('position', 3), g('uv', 2); idx = np.frombuffer(raw, '<u4', at['indices'][1], at['indices'][0])
            T = pos[idx].reshape(-1, 3, 3); U = uv[idx].reshape(-1, 3, 2)
            n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); l2 = (n * n).sum(1) + 1e-12
            side = n[:, 0] ** 2 >= .49 * l2
            mat = meta['materials'].get(prim['shader'], {}); url = mat.get('textureUrl', '')
            im = tex_img(url) if url else None
            tread = sample(im, U[~side]) if im is not None and (~side).any() else None
            sw = sample(im, U[side]) if im is not None and side.any() else None
            f = lambda c: '-' if c is None else '%02x%02x%02x' % tuple((c * 255).astype(int))
            out.append((name, obj['name'], prim['shader'], url, 'x'.join(map(str, im.shape[1::-1])) if im is not None else '-', int((~side).sum()), int(side.sum()),
                        f(tread), f(sw), 'общая с кузовом' if url in body_urls else 'своя', int(bool(mat.get('alpha'))), int(bool(mat.get('translucent') or mat.get('blend')))))
    return out


def main():
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True); conv = Converter(); rows = []
    for p in sorted((GAME / 'art' / 'cars').glob('*.p3d')):
        tmp = f'tc1061_{os.getpid()}'
        try:
            conv.export(p, tmp, vehicle=True)
            meta = json.loads((OUT / f'{tmp}.json').read_text()); raw = (OUT / f'{tmp}.bin').read_bytes()
            rows += rows_for(p.stem, meta, raw)
        except Exception as e:
            rows.append((p.stem, '-', '-', '-', '-', 0, 0, '-', '-', f'ошибка {type(e).__name__}', 0, 0))
        finally:
            for ext in ('json', 'bin'):
                if (OUT / f'{tmp}.{ext}').exists(): (OUT / f'{tmp}.{ext}').unlink()
    hdr = 'машина\tколесо\tшейдер\tкартинка\tразмер\tтреуг_протектор\tтреуг_боковина\tцвет_протектор\tцвет_боковина\tкартинка_кузова\talpha\tblend'
    (out / 'tire_census1061.tsv').write_text(hdr + '\n' + '\n'.join('\t'.join(map(str, r)) for r in rows) + '\n')
    print(f'машин {len({r[0] for r in rows})}, строк {len(rows)}')


if __name__ == '__main__':
    main()
