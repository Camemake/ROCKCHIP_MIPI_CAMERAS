# MIRA220 color on RV1126B

Hardware RKAIQ is **not** the live path. CIF compact RAW10 plus the measured AWB is.

## CIF unpack

Rockchip CIF `BG10` is 4 × 10-bit pixels in 5 little-endian bytes. Stride is 2048 on Ego.
Do not treat the buffer as CSI-2 RAW10.

## AWB

Measured on identity demosaic (no gains). LED tubes, ceiling, and ColorChecker whites
all sat at **B/G ≈ 0.43, R/G ≈ 0.73**. One gain set:

| | B | G | R | exposure |
| --- | --- | --- | --- | --- |
| 8-bit | 2.22 | 1.00 | 1.38 | 1.38 |

See `cif-awb.json`. `raw10_cif.py` applies it.

## RKAIQ IQ

`mira220_camevision-mira220_default.json` is the 1600×1400 IQ for `rkaiq_3A_server`.
Install under `/oem/usr/share/iqfiles/` as `mira220_camevision-mira220_default.json`.

The ISP notifier on this DT still completes with `isp_inp=0` (sditf is a single `port`,
so it never async-registers). Do not unbind `rkisp`. Do not poke `isp_inp` from a module.

## Things that broke color

- Analog gain written to `0x107d` (that register is VSTART)
- G crushed to 0.62 and B lifted to 2.6 (magenta chart, then pink lights)
- 3200 K libcamera CCM on office LEDs
- Magenta-only subtract (R and B both above G) — leftover was a blue room
