# drm-komeda-kmod

The Linux komeda DRM driver (Arm Mali-D71 family display controllers, and
Arm China's Linlon-D6/D60) for FreeBSD, built against
[drm-kmod](https://github.com/freebsd/drm-kmod)'s DRM core and LinuxKPI.
Brought up on the CIX Sky1 (Orange Pi 6 Plus), whose five Linlon-D60
controllers firmware describes in ACPI, with CIX's drivers for its DP
transmitters and USB-C/DP combo PHYs.

- `drivers/gpu/drm/arm/display/`: komeda from Linux v6.13, the release
  drm-kmod's DRM core follows, with Linux v7.1's Linlon-D6 support
  (8fd7576fd6d8, Cunyuan Liu).  GPL-2.0.
- `drivers/gpu/drm/`, `include/drm/`: Linux v6.13's GEM DMA, fb-DMA,
  fbdev-DMA and simple-KMS helpers, which drm-kmod does not build.
  GPL-2.0, MIT.
- `drivers/gpu/drm/cix/dptx/`, `drivers/phy/cix/`: CIX's Trilinear DP
  transmitter and USB-C/DP PHY drivers, from
  [Sky1-Linux/linux-sky1](https://github.com/Sky1-Linux/linux-sky1)'s
  patches (CIX's, and Entrpi's fixes); `drivers/reset/reset-sky1.c` and
  its bindings for the reset line table.  GPL-2.0.  Not built: CIX's
  binding to its own display driver (`trilin_dptx_cix.c`) and the eDP
  panel driver.
- `komeda/`: the module build.
- `freebsd/`: FreeBSD glue (BSD-2-Clause):
  - `komeda_freebsd_bus.c`: the ACPI driver (`CIXH5010`).  Attaches only
    to controllers the firmware has lit (`hw.komeda.dpu_mask`, DPU4 by
    default), and reads the timing of the pipelines the firmware drives.
  - `komeda_freebsd.c`: the platform device komeda attaches to;
    devicetree nodes and graph made from the ACPI description; SCMI clocks
    by the names of the `CLKT` table; write-combined contiguous DMA memory;
    and, without a DP transmitter, a bridge with a DisplayPort connector
    whose only mode is the firmware's.
  - `sky1_dp_freebsd_bus.c`: the ACPI drivers for the DP transmitters
    (`CIXH502F`, `hw.komeda.dptx_mask`, DP04 by default) and USB-C/DP PHYs
    (`CIXH2033`, `hw.komeda.udphy_mask`, UCP3 by default: their driver
    resets them), and `CLKT`, `RSTL` and `_DSD` lookups.
  - `sky1_dp_freebsd.c`: their platform devices, and what CIX's drivers
    get from Linux: device properties and child nodes, regmap, reset
    control (the `RST0` block), and the generic PHY API.  komeda's CRTC
    binds the transmitter, which makes the encoder and connector.
  - `include/`: Linux headers LinuxKPI and drm-kmod lack or leave empty,
    and `komeda_freebsd_compat.h`, included into every file.

Changes to the Linux files are marked `#ifdef __linux__` / `__FreeBSD__`.

## Building

Against a FreeBSD source tree with the `sky1_scmi` driver (branch
`orangepi-6-plus` of [JamesKane/freebsd-src](https://github.com/JamesKane/freebsd-src))
and a drm-kmod checkout:

    make DRMKMOD=/path/to/drm-kmod SYSDIR=/path/to/src/sys

The kernel needs LinuxKPI's support for threaded interrupts without a
primary handler (19ef96c6e7 on branch `hmp-sky1`), or the compatibility
header's stand-in for it; and drm-kmod's free `hw.dri` slot search
(22655461b5 on branch `sysfbdrm`) for a third DRM device's sysctls.

## Status

komeda drives the controller the firmware lit (DPU4) through CIX's DP
transmitter (DP04) and its USB-C/DP PHY (UCP3): the transmitter detects
the monitor, trains the link and sets modes.  On the Orange Pi 6 Plus
this is the HDMI port, behind a Parade PS185 DP-to-HDMI converter,
through which the monitor's EDID does not read (on Linux either): the
modes are the standard ones up to 1920x1080, at 8 bpc or more.  The
console (vt(4), through DRM's fbdev emulation) moves onto it when komeda
loads, and comes back when a compositor exits.  sway runs on it,
rendering with panthor into buffers komeda scans out directly.  komeda
keeps the controller as the firmware left it (a reset drops the DP link
for good).

Not yet: the other outputs (DPU0-3 and their transmitters), the monitor's
own modes (EDID through the PS185), DP audio (no ALSA), the cursor plane,
the IOMMU, runtime suspend (clocks are kept on), unloading (komeda stays
once attached), and releasing mapped buffers (a mapping keeps its
buffer).
