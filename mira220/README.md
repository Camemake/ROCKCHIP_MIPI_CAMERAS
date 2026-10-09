# MIRA220 on Rockchip RV1126B

This folder is a test driver for the ams OSRAM Mira220. It is not one of the Luckfox Aura pictures listed in the main table of this repository. Those eight cameras streamed on Aura CSI1. This package was written from the CameVision Ego, which is also an RV1126B board, with two color Mira220 sensors.

Sensor: [ams OSRAM Mira220](https://ams-osram.com/products/sensor-solutions/cmos-image-sensors/ams-mira220).

The Camemake product that uses this sensor is the [CameVision Ego](https://www.camemake.eu/shop/cv-ego01-os-camevision-ego-dual-global-shutter-ai-stereo-camera-2414), SKU CV-EGO01-OS-AM. That is a stereo camera, not one of the Raspberry Pi flex modules.

Camemake's European headquarters is in Berlare, Belgium. Offices are on the [about page](https://www.camemake.eu/about-us).

## Sensor

| | |
| --- | --- |
| Maker | ams OSRAM |
| Compatible | `ams,mira220` |
| I2C | `0x54`, 16-bit address, 8-bit value |
| Array | 1600×1400, global shutter |
| CSI | RAW10, 2 lanes, link 750 MHz (1500 Mbps per lane) |
| Clock | Dummy `xvclk` at 27 MHz. The driver also accepts 38.4 MHz |

750 MHz is the link in the AMS driver. The ams OSRAM page lists the same interface speed, 1500 Mbps. The register list is `mira220_init.h`. `MIRA220_MIPI_2L.ini` is that list in the Xunfei station format.

The Mira220 has no analog gain. Register `0x107d` is VSTART, the top of the window. This driver writes `0` there so the full 1600×1400 frame starts at the origin. Do not write a gain code to `0x107d`.

The driver tells the receiver the pixel order is BGGR (`SBGGR10_1X10`, Aura fourcc `BG10`). The module notes from the Ego say the OTP pattern is GRBG (`SGRBG`). On this CIF, GRBG is `BA10`, not `BG10`. Those two files do not agree. Read the format the capture node lists, and check the picture, before you treat the colors as settled.

On a module that already has a crystal, leave that pin alone. Driving the SoC clock onto it clamps the crystal and the sensor stops answering. The `xvclk` in the overlay only satisfies the driver's clock request.

## Which socket this overlay uses

`mira220-overlay.dts` enables `i2c3` and `csi2_dphy3`, then `mipi2_csi2` and `rkcif_mipi_lvds2`, with data lanes 1 and 2.

That combination is not the socket used by the other cameras in this repository.

| Board and socket | I2C | 2-lane PHY |
| --- | --- | --- |
| Luckfox Aura CSI1 | `i2c3` | `csi2_dphy4`. `csi2_dphy3` is the 4-lane PHY |
| CameVision Ego CAM0 | `i2c3` | `csi2_dphy0` into `mipi_csi2` and `rkcif_mipi_lvds` |
| CameVision Ego CAM1 | `i2c6` | `csi2_dphy3` into `mipi2_csi2` and `rkcif_mipi_lvds2` |

On the Aura, `install.sh` for the other cameras also loads `phy1clk.ko` before the sensor, and the overlay holds reset gpio4 pin 9 high. This MIRA220 install does neither. Copy `phy1clk.ko` from `common/` only after the overlay is pointed at `csi2_dphy4`.

Do not unload `rkcif` or `rkisp` to try another wiring. On this kernel that hangs the board. Power the board off, then change the overlay.

## Build on the Aura tree

From this folder on the board, with the kernel tree at `/opt/aura-build/linux-6.1.141`:

```sh
sh build.sh
sh install.sh
```

`install.sh` stops if another camera module from this repository is still loaded. Power-cycle first. It then loads the overlay and `mira220.ko`.

The color preview is not the port 8090 viewer used by the other cameras. After the sensor is on the graph:

```sh
python3 isp/raw10_cif.py
```

That script unpacks CIF RAW10 and applies the measured white balance in `isp/cif-awb.json`. Hardware 3A is not connected. `isp/mira220_camevision-mira220_default.json` is the IQ file for a later RKAIQ path. The notes in `isp/README.md` say why that path is not live, and which gain and color settings broke the picture.

## Files

| File | What it is |
| --- | --- |
| `mira220.c` | The sensor driver |
| `mira220_init.h` | The AMS register list |
| `mira220-overlay.dts` | The test overlay described above |
| `MIRA220_MIPI_2L.ini` | The same register list for a Xunfei station |
| `build.sh` / `install.sh` | Compile and load on the Aura build tree |
| `isp/raw10_cif.py` | CIF RAW10 preview with the measured white balance |
| `isp/README.md` | Color notes from the Ego |
