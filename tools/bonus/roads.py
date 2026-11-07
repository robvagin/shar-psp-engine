"""Дорожная сеть трассы мини-игры (№1116, сверка g03-aicars): перекрёстки, дороги и полосы из art/b0N.p3d -> мини-блок TRF1.

Как у оригинала: соперники бонус-гонки ведутся по дорогам трассы (перекрёстки, дороги и их участки из того же
файла трассы, что и у улиц уровней) от точки WPR к следующей; раньше порт вёл их по своему плотному пути (nav.py, Дейкстра по
опоре). Блок читает тот же viewer/traffic.c (trafficInitMem), что и улицы уровня, и тот же планировщик msncar.c; поэтому формат —
TRF1 без машин, без пешеходов и без рисунка: заголовок 256 Б, перекрёстки, дороги, точки полос, выезды, одна пустая модель-заглушка
(trafficInitMem требует хотя бы одну модель и один кусок). Разбор дорог — tools/traffic_export.road_network (читает любой p3d с
этими кусками; подменяем чтение на файл трассы). BONUS_ROADS_OLD=1 в экспорте — блок не пишется (прежний путь ИИ по опоре)."""
import os, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import track as T   # noqa: E402

ROADS_OLD = os.environ.get('BONUS_ROADS_OLD') == '1'
HDR = 256


def network(n):
    """-> (junc, roads) трассы n (b0N.p3d); как tools/traffic_export.road_network для улиц"""
    import traffic_export as TE
    keep = TE.read
    try:
        TE.read = lambda path: keep(T.ART / f'b0{n}.p3d')
        return TE.road_network(n)
    finally:
        TE.read = keep


def block(n):
    """мини-TRF1 трассы n; пустой bytes, если дорог нет или ROADS_OLD"""
    if ROADS_OLD: return b''
    junc, roads = network(n)
    if not junc or not roads: return b''
    body = bytearray()
    def sec(data, a=16):
        nonlocal body
        while (HDR + len(body)) % a: body += b'\0'
        off = HDR + len(body); body += data; return off
    jrec = bytearray(); outl = []
    for j in junc:
        jrec += struct.pack('<4fIHHHHI', *j['pos'], j['r'], j['beh'], len(outl), len(j['out']), j['nin'], 0, 0); outl += j['out']
    rrec = bytearray(); prec = bytearray(); npt = 0
    for r in roads:
        flags = (1 if r['shortcut'] else 0) | (2 if r['maxCars'] == 0 else 0)
        k = len(r['pts'][0])
        rrec += struct.pack('<4H4BIHHfff', r['s'], r['e'], r['rev'], r['lanes'], flags, min(r['maxCars'], 255), r['speed'], r.get('diff', 0),
                            npt, k, 0, float(r['pts'][0][-1, 3]), r['width'], 0.)
        for lane in range(r['lanes']):
            prec += r['pts'][lane].astype('<f4').tobytes(); npt += k
    # модель-заглушка (128 Б) и один пустой кусок (80 Б): trafficInitMem без них файл не принимает
    mrec = struct.pack('<16s4H10f12ff3I', b'none', 0, 1, 0, 0, 0., .3, 1., 2., 0., 0., 0., 0., 0., 0., *([0.] * 12), 0., 0, 0, 0)
    prc = struct.pack('<6f5i4f5I', *([0.] * 6), -1, 16, 0, 0, 0, 0., 0., 0., 1., 0, 0, 0, 0, 0)
    assert len(mrec) == 128 and len(prc) == 80
    oJ = sec(bytes(jrec)); oR = sec(bytes(rrec)); oP = sec(bytes(prec))
    oO = sec(struct.pack(f'<{len(outl)}H', *outl) if outl else b''); oM = sec(mrec); oPc = sec(prc)
    end = HDR + len(body)
    F = dict(nJunc=len(junc), offJunc=oJ, nRoad=len(roads), offRoad=oR, nPt=npt, offPt=oP, nOut=len(outl), offOut=oO,
             nModel=1, offModel=oM, nPiece=1, offPiece=oPc, vertOff=end, vertBytes=0, nTex=0, offTex=end, texBytes=0,
             nSlot=0, offSlot=end, nPath=0, offPath=end, nPathPt=0, offPathPt=end, nZone=0, offZone=end, nGroup=0, offGroup=end,
             nPed=0, offPed=end, pedOff=end, pedBytes=0, huskModel=0, nPark=0, offPark=end, nLoader=0, offLoader=0, offLight=0, offSeat=0)
    head = struct.pack('<4sII', b'TRF1', 1, end) + struct.pack(f'<{len(F)}I', *F.values())
    head += b'\0' * (HDR - len(head))
    return bytes(head) + bytes(body)


if __name__ == '__main__':
    for n in T.TRACKS:
        b = block(n)
        print(f'b0{n}: {len(b)} Б')
