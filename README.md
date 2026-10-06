# Camemake MIPI camera drivers for Rockchip

These are open Linux drivers for Camemake MIPI CSI-2 camera modules on Rockchip. Each folder is the sensor driver and the device-tree overlay that produced a real picture on a Luckfox Aura (RV1126B), with the module on CSI1, the second camera socket. The modules are the same ones sold for the Raspberry Pi 5. The Raspberry Pi drivers are in [RPI5_MIPI_DRIVERS](https://github.com/Camemake/RPI5_MIPI_DRIVERS).

Camemake designs the modules. Engineering is in Berlare, Belgium. The Hong Kong headquarters is in Kwun Tong, and the factory is in Xinfeng, China. Offices and distributors are on the [about page](https://www.camemake.eu/about-us). The shop for these modules is [Raspberry Pi camera modules](https://www.camemake.eu/raspberry-pi-camera-modules-rpi). The project list is on [camemake.github.io](https://camemake.github.io/).

The picture from the included viewer is a live preview in a web browser. It shows that the sensor is streaming. It is not a finished color pipeline.

Other Rockchip boards can reuse the sensor settings in these folders. The overlay is wired for the Aura's CSI1 connector. On another board, that overlay has to be pointed at that board's own camera socket. See [Other Rockchip boards](#other-rockchip-boards).

## The cameras

Every module below was connected to CSI1 of a Luckfox Aura at the time of the test. Only one module can own that socket. Power the board off before you change cameras.

| Camera | What streamed | Lanes | I2C | Link | Product |
| --- | --- | --- | --- | --- | --- |
| [GC4023](#gc4023) | 2560×1440 RAW10 RGGB | 2 | `0x29` | 351 MHz | [GC4023 for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-gc4023-rpi-gc4023-2-5mp-ff-for-raspberry-pi-1095) |
| [HM2170](#hm2170) | 1928×1088 RAW10 GRBG | 2 | `0x25` | 540 MHz | [HM2170 for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-hm2170-rpi-hm2170-2mp-ff-for-raspberry-pi-1082) |
| [OX05B1S](#ox05b1s) | 2592×1944 RAW10 GRBG | 4 | `0x36` | 450 MHz | [OX05B1S for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-ox05b1s-rpi-ox05b1s-5mp-ff-for-raspberry-pi-1088) |
| [IMX908](#imx908) | 3856×2176 RAW10 RGGB | 4 | `0x1a` | 720 MHz | Same module family. No separate shop page yet. [All MIPI modules](https://www.camemake.eu/shop/category/mipi-18) |
| [SC450AI](#sc450ai) | 2688×1520 RAW10 BGGR | 4 | `0x30` | 180 MHz | [SC450AI for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-sc450ai-rpi-sc450ai-4mp-ff-for-raspberry-pi-1096) |
| [IMX675](#imx675) | 2608×1960 RAW10 GRBG | 2 | `0x1a` | 720 MHz | [IMX675 for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-imx675-rpi-imx675-5mp-ff-for-raspberry-pi-1105) |
| [IMX586](#imx586) | 4000×3000 RAW10 RGGB | 4 | `0x1a` | 450 MHz | [IMX586 for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-imx586-rpi-imx586-48mp-ff-for-raspberry-pi-1104) |
| [SC233HGS](#sc233hgs) | 1920×1200 RAW10 BGGR | 4 | `0x30` | 270 MHz | [SC233HGS for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-sc233hgs-rpi-sc233hgs-2mp-ff-for-raspberry-pi-1092) |

"Link" is the CSI-2 clock. The receiver prints twice that number as megabits per lane. A 450 MHz link is logged as 900 Mbps.

The size in the table is the mode that streamed on the Aura. A sensor maker's page often lists the full pixel array, which can be a few lines larger, or, for the IMX586, the full 8000×6000 array. Use the streamed size with these drivers.

## Sensor makers

Camemake builds the module. The companies below make the image sensor inside it. GalaxyCore, Himax, and the SmartSens SC233HGS entry are catalog pages that list the part. Sony and OmniVision, and the SmartSens SC450AI note, are pages about that sensor.

| Sensor | Maker | Sensor page |
| --- | --- | --- |
| GC4023 | GalaxyCore | [GC4023 on the GalaxyCore product series](https://en.gcoreinc.com/products/index?subcid=17) |
| HM2170 | Himax | [Himax image sensors](https://www.himax.com.tw/products/cmos-image-sensor/image-sensors/) |
| OX05B1S | OmniVision | [OX05B1S](https://www.ovt.com/products/ox05b/) |
| IMX908 | Sony Semiconductor Solutions | [IMX908](https://www.sony-semicon.com/en/products/is/security/security/IMX908.html) |
| SC450AI | SmartSens | [SC450AI launch note](https://www.smartsenstech.com/en/mpage?id=142) |
| IMX675 | Sony Semiconductor Solutions | [IMX675 announcement](https://www.sony-semicon.com/en/news/2022/2022072001.html) |
| IMX586 | Sony | [IMX586 announcement](https://www.sony.com/en/SonyInfo/News/Press/201807/18-060E/) |
| SC233HGS | SmartSens | [SmartSens global-shutter series](https://www.smartsenstech.com/en/gs_products) |

## What you need

- A Luckfox Aura, with the camera on **CSI1** (not CSI0).
- One Camemake MIPI module from the list. The flex is the same 22-pin style as a Raspberry Pi 5 camera.
- A way to copy files onto the board and open a root shell. On the board we used, that was `adb` over Wi-Fi.
- The kernel sources that match the running kernel, so the driver can be compiled. On our board those sources are `/opt/aura-build/linux-6.1.141`, kernel `6.1.141`. A module built for a different kernel will not load.

The module has its own crystal. Leave that pin alone. On the Aura, CSI1 IO0 is that pin (gpio 129). Driving the SoC clock onto it clamps the crystal and the sensor stops answering.

## Use one camera

1. Power the board off.
2. Plug in one module.
3. Power the board on.
4. Copy that camera's folder to the board, for example `/opt/aura-build/gc4023/`.
5. Build the PHY clock helper once, from the `common` folder, and copy `phy1clk.ko` into the camera folder:

```sh
make KDIR=/opt/aura-build/linux-6.1.141
cp phy1clk.ko /opt/aura-build/gc4023/
```

Use your camera's folder in place of `gc4023`.

6. Build the camera, then start it. From that camera's folder, on the board:

```sh
sh build.sh
sh install.sh
```

7. On a computer on the same network, open `http://<board-ip>:8090/`.

`install.sh` turns the connector on, loads the driver, and starts the preview. The preview listens on port 8090. Only one preview can use that port.

To try a different camera, power the board off, swap the module, power on, and run `install.sh` from the new folder only. The overlay does not survive a reboot, which is what you want: after a reboot the connector is free for the next camera.

Do not unload the running camera receiver (`rkcif`) to swap sensors. On this kernel that hangs the board, including the network. A power cycle is the way to change cameras.

## If the preview stays black

The driver can load and still deliver no picture. Check these in order.

- The kernel log should contain the sensor's id line, for example `SC450AI id 0xbd2f`. If the id line is missing, the sensor did not answer on I2C. The usual cause is the crystal pin being driven, or the reset pin being held down.
- The kernel log should contain `data_rate_mbps` at the rate in the table below. A different rate means the link clock does not match this module.
- A capture file of size 0 means the receiver saw no frame. A file of the size listed below means a full frame arrived.
- `dmesg` may say `get vblank fail`. That line is normal on this receiver. Streaming continues.

## What each folder contains

| File | What it is |
| --- | --- |
| `*.c` | The sensor driver |
| `*-overlay.dts` | Tells the Aura which connector, which I2C address, and how many lanes |
| `regs.inc` | The register list that starts the picture |
| `build.sh` | Compiles the driver on the Aura build tree |
| `install.sh` | Loads the overlay and the driver, then starts the preview |
| `live.py` | The browser preview |
| `livecap.c` | Reads one raw frame and turns it into a small color image |
| `common/phy1clk.c` | Turns on the Aura CSI1 PHY clock. The receiver ignores the lanes until this clock is running |

`build.sh` expects the Aura tree at `/opt/aura-build`, with the kernel at `/opt/aura-build/linux-6.1.141` and a compiler on `PATH`. If your tree is somewhere else, edit the `ROOT=` line in `build.sh` before you run it.

## GC4023

Folder `gc4023/`. Product: [GC4023 2.5MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-gc4023-rpi-gc4023-2-5mp-ff-for-raspberry-pi-1095). Sensor: [GalaxyCore GC4023](https://en.gcoreinc.com/products/index?subcid=17).

GalaxyCore GC4023, I2C `0x29`. The chip id at `0x03f0` is `0x4023`. The shop text that says address `0x31` does not match this module.

2560×1440, RAW10, RGGB. The Aura packs this as `RG10`. Two lanes. The register `0x0114` is `0x01`. Link 351 MHz, logged as 702 Mbps. Crystal 27 MHz.

The receiver path is `csi2_dphy4` → `mipi2_csi2` → `rkcif_mipi_lvds2`. Each line is padded to 3200 bytes. A full frame is 4608000 bytes.

Stream on is `0x0100=0x09`. Standby is `0x0100=0x00`. The preview is the half-resolution viewer on port 8090.

## HM2170

Folder `hm2170/`. Product: [HM2170 2MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-hm2170-rpi-hm2170-2mp-ff-for-raspberry-pi-1082). Sensor: [Himax HM2170](https://www.himax.com.tw/products/cmos-image-sensor/image-sensors/).

Himax HM2170, I2C `0x25`. Address `0x24` does not answer. The chip id is three bytes at `0x0000`: `0x21 0x70` plus a revision. This module is revision `0x05` (rev D). A revision below 4 needs a different register list.

1928×1088, RAW10, GRBG. The Aura packs this as `BA10`. Two lanes. Link 540 MHz, logged as 1080 Mbps. The Intel tables this list comes from were written for a 19.2 MHz clock. This module's crystal is 27 MHz, and the link used here is `384 MHz × 27 / 19.2`.

The receiver path is `csi2_dphy4`. Each line is padded to 2560 bytes. A full frame is 2785280 bytes.

A register value of `0xffff` in the table is a delay in milliseconds, not a sensor register. Stream on is `0x0100=0x01`.

The preview on this board was about 23 frames per second.

## OX05B1S

Folder `ox05b1s/`. Product: [OX05B1S 5MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-ox05b1s-rpi-ox05b1s-5mp-ff-for-raspberry-pi-1088). Sensor: [OmniVision OX05B1S](https://www.ovt.com/products/ox05b/).

OmniVision OX05B1S, I2C `0x36`. Chip id at `0x300a` is `0x580542`.

2592×1944, RAW10, GRBG. The Aura packs this as `BA10`. Four lanes. The register `0x3010` is `0x41`. Link 450 MHz, logged as 900 Mbps. The table was captured at 12 MHz and was not rewritten. This module's crystal is 27 MHz.

The receiver path is `csi2_dphy3`. Use `csi2_dphy3` for four lanes. `csi2_dphy4` is only lane 0 and lane 1. Each line is padded to 3328 bytes. A full frame is 6469632 bytes.

Stream on is `0x0100=0x01`. Standby is `0x0100=0x00`.

This sensor is RGB-IR. The simple preview looks magenta. That is the viewer, not a broken camera.

The preview on this board was about 9 frames per second.

## IMX908

Folder `imx908/`. Sensor: [Sony IMX908](https://www.sony-semicon.com/en/products/is/security/security/IMX908.html). This is the same style of module as the Raspberry Pi cameras. The shop does not have an IMX908 page yet. The rest of the range is in the [MIPI category](https://www.camemake.eu/shop/category/mipi-18).

Sony IMX908, I2C `0x1a`. The chip id is 16-bit at `0x4c0c`: `0x8c 0x03`, which is `0x038c`. That register does not answer until `0x3000` is written `0` and about 24 ms have passed.

3856×2176, RAW10, RGGB. The Aura packs this as `RG10`. Four lanes. `0x3040=0x03` is 4-lane. Link 720 MHz (`0x3015=0x03`), logged as 1440 Mbps. `0x3014=0x03` selects the 27 MHz crystal.

The receiver path is `csi2_dphy3`. Each line is padded to 4864 bytes. A full frame is 10584064 bytes.

Stream on is standby cancel `0x3000=0x00`, wait 24 ms, then master start `0x3002=0x00`. Standby is `0x3000=0x01` and master stop is `0x3002=0x01`.

The module we tested had no lens, so the frame was white. The preview was about 6 frames per second.

`IMX908_MIPI_4L.ini` is the same sequence in the Xunfei station format. SlaveID `0x34` is the 8-bit form of I2C `0x1a`.

## SC450AI

Folder `sc450ai/`. Product: [SC450AI 4MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-sc450ai-rpi-sc450ai-4mp-ff-for-raspberry-pi-1096). Sensor: [SmartSens SC450AI](https://www.smartsenstech.com/en/mpage?id=142).

SmartSens SC450AI, I2C `0x30`. Chip id at `0x3107` is `0xbd2f`.

2688×1520, RAW10, BGGR. The Aura packs this as `BG10`. Four lanes. Link 180 MHz, logged as 360 Mbps. The table is the 4-lane list (`0x301f=0x02`) written for a 27 MHz crystal, which is this module's clock. A 2-lane table exists. Do not use it on this module.

The receiver path is `csi2_dphy3`. Each line is padded to 3584 bytes. A full frame is 5447680 bytes.

The table writes `0x0103=0x01` (soft reset). The driver waits, writes the rest, then stream on is `0x0100=0x01`. Standby is `0x0100=0x00`.

The preview on this board was about 10 frames per second.

## IMX675

Folder `imx675/`. Product: [IMX675 5MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-imx675-rpi-imx675-5mp-ff-for-raspberry-pi-1105). Sensor: [Sony IMX675](https://www.sony-semicon.com/en/news/2022/2022072001.html).

Sony IMX675, I2C `0x1a`. Module id at `0x3a00` is `0x96`.

2608×1960, RAW10, GRBG. The Aura packs this as `BA10`. Two lanes. The table writes `0x3040=0x01`. Four lanes on this family would be `0x3040=0x03`. `0x3014=0x03` selects the 27 MHz crystal. `0x3023=0x00` is RAW10.

The receiver path is `csi2_dphy4`. Each line is padded to 3328 bytes. A full frame is 6522880 bytes.

Stream on is `0x3000=0`, wait 2 ms, then `0x3002=0`. Standby is `0x3000=1` and master stop is `0x3002=1`.

On the Aura this table locks at link **720 MHz**, logged as 1440 Mbps. The same table on a Raspberry Pi 5 was run at 800 MHz. 800 MHz on this Aura PHY loses sync (`sot sync` on lanes 0 and 1) and the frame counter stays at 0. Leave the Aura driver at 720 MHz.

The preview on this board was about 9 frames per second.

## IMX586

Folder `imx586/`. Product: [IMX586 48MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-imx586-rpi-imx586-48mp-ff-for-raspberry-pi-1104). Sensor: [Sony IMX586](https://www.sony.com/en/SonyInfo/News/Press/201807/18-060E/).

Sony IMX586, I2C `0x1a`. Chip id at `0x0016` is `0x0586`.

The Aura kernel already contains an `imx586` driver. It binds any device whose name ends in `imx586`, and it expects a 37.125 MHz clock. This module's crystal is 27 MHz. The overlay in this folder is therefore named `camemake,imx586cm`, and the driver name is `imx586cm`. The camera shows up as `imx586cm 3-001a`.

4000×3000, RAW10, RGGB. The Aura packs this as `RG10`. Four lanes (`0x0114=0x03`). Link 450 MHz, logged as 900 Mbps. The table's input clock bytes are `0x0136=0x1b`, `0x0137=0x00` for the 27 MHz crystal. `0x0901=0x22` is the 2×2 bin of the 8000×6000 array, which is why the streamed picture is 4000×3000 rather than 48 megapixels.

The receiver path is `csi2_dphy3`. Each line is padded to 5120 bytes. A full frame is 15360000 bytes.

After the table the driver raises analog gain (`0x0204=0x03`, `0x0205=0xe0`, and the same pair at `0x0216` / `0x0217`), clears the test pattern (`0x0601=0`), waits 10 ms, and sets stream on `0x0100=0x01`. Standby is `0x0100=0x00`.

There is an older public repository, [IMX586_RK35xx_MIPI_DRIVER](https://github.com/Camemake/IMX586_RK35xx_MIPI_DRIVER), aimed at RK356x and RK3588 with a different clock. The driver in this folder is the one that streamed on the Aura. Use this one for the Aura module.

The preview on this board was about 4 frames per second. At this gain a bright scene looks bright. That gain is the one that produced a picture. At 1× gain the frame is a few codes above black.

## SC233HGS

Folder `sc233hgs/`. Product: [SC233HGS 2MP for Raspberry Pi](https://www.camemake.eu/shop/cm-mipi-sc233hgs-rpi-sc233hgs-2mp-ff-for-raspberry-pi-1092). Sensor: [SmartSens SC233HGS](https://www.smartsenstech.com/en/gs_products).

SmartSens SC233HGS, I2C `0x30`. Chip id at `0x3107` is `0xcb61`.

1920×1200, RAW10, BGGR. The Aura packs this as `BG10`. Four lanes. Link 270 MHz, logged as 540 Mbps. Crystal 27 MHz. The mode select in the table is `0x301f=0x48`.

The receiver path is `csi2_dphy3`. Each line is padded to 2560 bytes. A full frame is 3072000 bytes.

This connector has no external trigger wire. With trigger mode left off, the sensor emits one frame each time `0x2100` goes 0 and then 1. The low level has to last about a millisecond. The driver repeats that edge every 50 ms. Do not set the continuous-trigger bit `0x3222[0]`. That bit waits for a pulse this board does not send. `0x0100` is not the stream bit on this part.

The table's exposure (`0x3e01=0x40`) is 64 lines and the picture is almost black. After the table the driver writes `0x3e01=0xc0` and gain `0x3e09=0x80`.

The preview on this board was about 16 frames per second.

## Other Rockchip boards

The sensor driver and `regs.inc` are the part that is the same everywhere: I2C address, lanes, link frequency, and the register list above.

The overlay is the part that is the Aura. It enables `i2c-3`, the CSI1 receiver, and a dummy 27 MHz clock named `xvclk`. Reset is gpio4 pin 9, left high. Two-lane cameras use `csi2_dphy4`. Four-lane cameras use `csi2_dphy3`. Both of those logical PHYs program the CSI1 analog PHY at `0x21c50000`. `phy1clk.ko` enables that PHY's bus clock.

On another Rockchip board you keep the sensor settings and you rewrite the overlay so it attaches to that board's camera I2C bus and CSI receiver. The link frequencies above are the ones that locked on the Aura. The IMX675 note is the important one: 800 MHz locked on a Raspberry Pi 5 and did not lock on this Aura.

These packages were not run on RK3588, RK3576, or RK356x. A board whose kernel already has a driver of the same name will bind first, the way the Aura's built-in `imx586` driver does. Rename the compatible string the way `imx586cm` does, or the built-in driver takes the device.

The preview formats (`BG10`, `RG10`, `BA10`) are the Aura CIF's packed RAW10 codes. Another CIF may use a different fourcc for the same Bayer order. Read the format list from the capture device before changing it.

## Where Camemake is

Camemake publishes this driver from its own module designs. The places below are the public offices.

| Place | Address |
| --- | --- |
| European headquarters | Mosseveldstraat 57a, 9290 Berlare, Belgium |
| Hong Kong headquarters | #1608 Apec Plaza, 49 Hoi Yuen Road, Kwun Tong, Kowloon, Hong Kong |
| Factory | Intelligent Robot Industrial Park, Shenzhen Avenue, Xinfeng, China |

Sales and engineering offices in Shenzhen, and the distributor list, are on [About Camemake](https://www.camemake.eu/about-us). Phone and email are on the [contact page](https://www.camemake.eu/contactus).

## Common questions

**Which Rockchip board was tested?**

A Luckfox Aura, SoC RV1126B, kernel 6.1.141. The module was on CSI1. CSI0 was not used. Only one of these modules can own CSI1.

**Will the same folder work on RK3588, RK3576, or RK356x?**

The I2C address, lane count, link frequency, and register list are the sensor settings, and those stay. The overlay in each folder enables the Aura's `i2c-3` and CSI1 receiver. RK3588, RK3576, and RK356x were not tested with these packages. On those boards the overlay has to be rewritten for that board's camera socket. An older IMX586 tree for RK356x and RK3588 is [IMX586_RK35xx_MIPI_DRIVER](https://github.com/Camemake/IMX586_RK35xx_MIPI_DRIVER). It uses a different clock. It is not this Aura driver.

**Are these the Raspberry Pi camera modules?**

Yes. The shop pages are the Raspberry Pi product pages, and the Pi 5 drivers are [RPI5_MIPI_DRIVERS](https://github.com/Camemake/RPI5_MIPI_DRIVERS). This repository is the Rockchip path for the same modules.

**Who makes the sensor, and who makes the module?**

GalaxyCore makes the GC4023. Himax makes the HM2170. OmniVision makes the OX05B1S. Sony Semiconductor Solutions makes the IMX908, IMX675, and IMX586. SmartSens makes the SC450AI and the SC233HGS. Camemake builds the camera module and publishes the driver. Links to the sensor pages are in [Sensor makers](#sensor-makers).

**Where do I buy the module?**

Each camera section links to its page on [camemake.eu](https://www.camemake.eu/raspberry-pi-camera-modules-rpi). The IMX908 does not have a shop page yet.

## License

The kernel drivers are GPL-2.0, the same license as the Linux kernel they plug into.
