#!/usr/bin/env python3
"""Упаковка анимаций CHR1 (Модели-96, выключатель ANIMPACK96_OLD=1 — как до упаковки).

Формат дорожки не меняется (SkinTrackRec 16 Б), новое живёт в поле pad (раньше 0, старые файлы читаются как есть):
  биты 0-1  вращение:  0 = s16 x4 с кадрами (как было) | 1 = u32 «три меньших» с кадрами u16 | 2 = u32 «три меньших», кадры 0..n-1 подряд (кадров в файле нет) | 3 = то же с кадрами u8
  биты 2-3  перенос:   0 = f32 x3 с кадрами (как было) | 1 = f16 x3 с кадрами u16             | 2 = f16 x3, кадры 0..n-1 подряд | 3 = f16 x3 с кадрами u8
  u32 «три меньших»: биты 30-31 — номер наибольшей компоненты (она восстанавливается из единичной длины, знак выбран так, что она >= 0),
  в остальных трёх по 10 бит со знаком (-512..511) значение = v * (sqrt(1/2) / 511).
Каждая дорожка пакуется, только если ошибка в пределах порога (иначе остаётся как было): вращение <= ROT_TOL_DEG, перенос <= TRANS_TOL м.
Хвост кадров (u16, добивка до 4 Б) остаётся там, где он есть."""
import math, os
import numpy as np

ROT_TOL_DEG = 0.15        # порог одной дорожки; цель задачи < 0.2°
TRANS_TOL = 0.0003        # 0.3 мм (цель < 2 мм; корень несёт движение персонажа, ему берём строже)
REST_TOL_DEG = 0.05       # дорожка из ключей, равных позе покоя, сводится к «нет дорожки» (n = 0 — поза покоя)
S3 = math.sqrt(0.5) / 511.0
OLD = os.environ.get('ANIMPACK96_OLD') == '1'

ROT_LEGACY, ROT_PACKED, ROT_DENSE, ROT_P8 = 0, 1, 2, 3
TR_LEGACY, TR_HALF, TR_HALF_DENSE, TR_H8 = 0, 1, 2, 3


def _ang(a, b):
    d = np.abs(np.sum(a * b, axis=-1)).clip(0, 1)
    return 2 * np.degrees(np.arccos(d))


def pack_quats(q):
    """q (n,4) x,y,z,w единичные -> (u32[n], декодированные q). Перебор округлений ±1 даёт минимум угловой ошибки."""
    q = np.asarray(q, np.float64); n = len(q)
    q = q / np.linalg.norm(q, axis=1, keepdims=True)
    big = np.argmax(np.abs(q), axis=1)
    sg = np.sign(q[np.arange(n), big]); sg[sg == 0] = 1
    q = q * sg[:, None]
    others = np.array([[c for c in range(4) if c != i] for i in range(4)])[big]           # (n,3)
    comp = np.take_along_axis(q, others, 1)                                                # (n,3)
    base = np.floor(comp / S3).astype(np.int64)
    best = None
    offs = np.array([(a, b, c) for a in (0, 1) for b in (0, 1) for c in (0, 1)])
    for o in offs:
        v = np.clip(base + o, -511, 511)
        c = v * S3
        w = np.sqrt(np.maximum(0, 1 - np.sum(c * c, axis=1)))
        dq = np.zeros((n, 4)); np.put_along_axis(dq, others, c, 1); dq[np.arange(n), big] = w
        e = _ang(dq, q)
        if best is None: best = (e.copy(), v.copy(), dq.copy())
        else:
            m = e < best[0]; best[0][m] = e[m]; best[1][m] = v[m]; best[2][m] = dq[m]
    _, v, dq = best
    word = (big.astype(np.uint32) << 30) | ((v[:, 0] & 1023).astype(np.uint32)) | ((v[:, 1] & 1023).astype(np.uint32) << 10) | ((v[:, 2] & 1023).astype(np.uint32) << 20)
    return word.astype('<u4'), dq


def unpack_quats(words):
    w = np.asarray(words, np.uint32); n = len(w)
    big = (w >> 30).astype(np.int64)
    v = np.stack([((w >> (10 * k)) & 1023).astype(np.int64) for k in range(3)], 1)
    v = np.where(v >= 512, v - 1024, v)
    c = v * S3
    lg = np.sqrt(np.maximum(0, 1 - np.sum(c * c, axis=1)))
    others = np.array([[x for x in range(4) if x != i] for i in range(4)])[big]
    q = np.zeros((n, 4)); np.put_along_axis(q, others, c, 1); q[np.arange(n), big] = lg
    return q


def _pad4(b): return b + bytes((-len(b)) % 4)


def pack_rot(fr, q, allow=True):
    """-> (режим, байты). Legacy при OLD или если ошибка дорожки выше порога."""
    fr = list(int(x) for x in fr); n = len(fr)
    q = np.asarray(q, np.float64)
    legacy = _pad4(np.asarray(fr, '<u2').tobytes()) + np.clip(np.round(q * 32767), -32767, 32767).astype('<i2').tobytes()
    if OLD or not allow: return ROT_LEGACY, legacy
    qn = q / np.linalg.norm(q, axis=1, keepdims=True)
    if n > 1 and np.abs(np.sum(qn[1:] * qn[:-1], axis=1)).min() < 0.02: return ROT_LEGACY, legacy   # почти пол-оборота между ключами: знак решает путь, не трогаем
    words, dq = pack_quats(q)
    if _ang(dq, q / np.linalg.norm(q, axis=1, keepdims=True)).max() > ROT_TOL_DEG: return ROT_LEGACY, legacy
    if fr == list(range(n)): return ROT_DENSE, words.tobytes()
    if max(fr) < 256: return ROT_P8, _pad4(np.asarray(fr, 'u1').tobytes()) + words.tobytes()
    return ROT_PACKED, _pad4(np.asarray(fr, '<u2').tobytes()) + words.tobytes()


def pack_trans(fr, v, tol=TRANS_TOL):
    fr = list(int(x) for x in fr); n = len(fr)
    v = np.asarray(v, np.float64)
    legacy = _pad4(np.asarray(fr, '<u2').tobytes()) + v.astype('<f4').tobytes()
    if OLD: return TR_LEGACY, legacy
    h = v.astype('<f2')
    if np.abs(h.astype(np.float64) - v).max() > tol or np.abs(v).max() >= 65000: return TR_LEGACY, legacy
    if fr == list(range(n)): return TR_HALF_DENSE, h.tobytes()
    if max(fr) < 256: return TR_H8, _pad4(np.asarray(fr, 'u1').tobytes()) + h.tobytes()
    return TR_HALF, _pad4(np.asarray(fr, '<u2').tobytes()) + h.tobytes()


def is_rest_rot(q, rest_q):
    """все ключи в пределах REST_TOL_DEG от позы покоя сустава"""
    if OLD: return False
    q = np.asarray(q, np.float64); r = np.asarray(rest_q, np.float64); r = r / np.linalg.norm(r)
    return bool(_ang(q / np.linalg.norm(q, axis=1, keepdims=True), r[None, :]).max() <= REST_TOL_DEG)


def decode_rot(mode, buf, off, n):
    """-> (кадры (n,), q (n,4)) из бинарного файла"""
    if mode == ROT_LEGACY:
        fr = np.frombuffer(buf, '<u2', n, off); q = np.frombuffer(buf, '<i2', n * 4, off + ((2 * n + 3) & ~3)).reshape(-1, 4) / 32767.
        return fr, q
    if mode == ROT_PACKED:
        fr = np.frombuffer(buf, '<u2', n, off); w = np.frombuffer(buf, '<u4', n, off + ((2 * n + 3) & ~3))
        return fr, unpack_quats(w)
    if mode == ROT_P8:
        return np.frombuffer(buf, 'u1', n, off), unpack_quats(np.frombuffer(buf, '<u4', n, off + ((n + 3) & ~3)))
    return np.arange(n), unpack_quats(np.frombuffer(buf, '<u4', n, off))


def decode_trans(mode, buf, off, n):
    if mode == TR_LEGACY:
        return np.frombuffer(buf, '<u2', n, off), np.frombuffer(buf, '<f4', n * 3, off + ((2 * n + 3) & ~3)).reshape(-1, 3).astype(np.float64)
    if mode == TR_HALF:
        return np.frombuffer(buf, '<u2', n, off), np.frombuffer(buf, '<f2', n * 3, off + ((2 * n + 3) & ~3)).reshape(-1, 3).astype(np.float64)
    if mode == TR_H8:
        return np.frombuffer(buf, 'u1', n, off), np.frombuffer(buf, '<f2', n * 3, off + ((n + 3) & ~3)).reshape(-1, 3).astype(np.float64)
    return np.arange(n), np.frombuffer(buf, '<f2', n * 3, off).reshape(-1, 3).astype(np.float64)
