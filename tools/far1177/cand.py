"""№1178: кандидаты в вывески уровня для просмотра глазами: python3 -I tools/far1177/cand.py LEVEL.shr OUT.png [IDS.txt проверка полноты]
Признак: плотность границ рисунка (AUC 0,89 на списке L5), мало кусков и треугольников у текстуры, не земля, ширина >= 32. Лист подписан номерами."""
import sys, collections; from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import tex_gate as T, budget as B
import numpy as np
from PIL import Image, ImageDraw
def cands(p, n=110):
    b = open(p, 'rb').read(); recs, *_ = T.walk(b); L = B.load(p); use = collections.defaultdict(lambda: [0, 0])
    for r in L['recs']:
        if r['owner'] == 0 and r['tex'] >= 0: u = use[r['tex']]; u[0] += 1; u[1] += r['ni'] // 3
    out = []
    for i, r in enumerate(recs):
        if i not in use or r[1] < 32 or b[r[0] + 9] == 1 or use[i][1] > 700: continue
        a = T.level0(b, r).astype(np.float32); g = a[..., :3] @ np.array([.299, .587, .114], np.float32)
        e = ((np.abs(np.diff(g, axis=1)) > 40).mean() + (np.abs(np.diff(g, axis=0)) > 40).mean()) / 2
        if e >= .07: out.append((e, i))
    out.sort(reverse=True); return b, recs, [i for _, i in out[:n]]
if __name__ == '__main__':
    b, recs, ids = cands(sys.argv[1])
    if len(sys.argv) > 3:
        pos = {int(x) for x in Path(sys.argv[3]).read_text().split()}; print('полнота', len(pos & set(ids)), 'из', len(pos), 'кандидатов', len(ids))
    cols = 12; tile = 80; rows = (len(ids) + cols - 1) // cols; sh = Image.new('RGB', (cols * tile, rows * (tile + 10)), (30, 30, 30)); d = ImageDraw.Draw(sh)
    for k, i in enumerate(ids):
        a = T.level0(b, recs[i]); im = Image.fromarray(a[:, :, :4].astype(np.uint8), 'RGBA'); bg = Image.new('RGBA', im.size, (255, 0, 255, 255)); bg.alpha_composite(im)
        x = (k % cols) * tile; y = (k // cols) * (tile + 10); sh.paste(bg.convert('RGB').resize((tile - 2, tile - 2)), (x, y)); d.text((x + 1, y + tile - 1), str(i), fill=(255, 255, 0))
    sh.save(sys.argv[2]); print(len(ids), '->', sys.argv[2])
