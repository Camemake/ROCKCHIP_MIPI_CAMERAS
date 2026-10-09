#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
AWB = json.loads((HERE / "cif-awb.json").read_text())
CAM_W, LIVE_H = 1600, 1000
STRIDE = int(AWB["unpack"]["stride"])
BLACK = int(AWB["unpack"]["black_level_8bit"])
B_GAIN = float(AWB["awb_bgr"]["B"])
R_GAIN = float(AWB["awb_bgr"]["R"])
EXPOSURE = float(AWB["awb_bgr"]["exposure"])


def unpack_le40(raw: bytes, height: int = LIVE_H, width: int = CAM_W, stride: int = STRIDE) -> np.ndarray:
    rows = np.frombuffer(raw, dtype=np.uint8, count=stride * height).reshape(height, stride)
    grp = rows[:, : (width * 10) // 8].reshape(height, width // 4, 5)
    ev, od = grp[0::2], grp[1::2]
    e0, e1, e2, e3, e4 = [ev[:, :, i].astype(np.uint16) for i in range(5)]
    o0, o1, o2, o3, o4 = [od[:, :, i].astype(np.uint16) for i in range(5)]
    g1 = e0 | ((e1 & 3) << 8)
    rr = (e1 >> 2) | ((e2 & 15) << 6)
    bb = o0 | ((o1 & 3) << 8)
    g2 = (o1 >> 2) | ((o2 & 15) << 6)
    g1b = (e2 >> 4) | ((e3 & 63) << 4)
    rrb = (e3 >> 6) | (e4 << 2)
    bbb = (o2 >> 4) | ((o3 & 63) << 4)
    g2b = (o3 >> 6) | (o4 << 2)
    view_w, view_h = width // 2, height // 2
    bgr = np.empty((view_h, view_w, 3), dtype=np.uint8)

    def u8(p):
        return np.clip((p >> 2).astype(np.int16) - BLACK, 0, 255).astype(np.uint8)

    bgr[:, 0::2, 2] = u8(rr)
    bgr[:, 0::2, 1] = u8((g1 + g2) >> 1)
    bgr[:, 0::2, 0] = u8(bb)
    bgr[:, 1::2, 2] = u8(rrb)
    bgr[:, 1::2, 1] = u8((g1b + g2b) >> 1)
    bgr[:, 1::2, 0] = u8(bbb)
    return bgr


def apply_awb(bgr: np.ndarray) -> np.ndarray:
    f = bgr.astype(np.float32)
    f[:, :, 0] *= B_GAIN
    f[:, :, 2] *= R_GAIN
    return np.clip(f * EXPOSURE, 0, 255).astype(np.uint8)


def raw10_to_bgr(raw: bytes, height: int = LIVE_H, width: int = CAM_W) -> np.ndarray:
    return apply_awb(unpack_le40(raw, height, width))
