# drm-komeda-kmod

The Linux komeda DRM driver (Arm Mali-D71 family display controllers, and
Arm China's Linlon-D6/D60) for FreeBSD, built against
[drm-kmod](https://github.com/freebsd/drm-kmod)'s DRM core and LinuxKPI.
Brought up on the CIX Sky1 (Orange Pi 6 Plus), whose five Linlon-D60
controllers firmware describes in ACPI.

- `drivers/gpu/drm/arm/display/`: komeda from Linux v6.13, the release
  drm-kmod's DRM core follows, with Linux v7.1's Linlon-D6 support
  (8fd7576fd6d8, Cunyuan Liu).  GPL-2.0.
- `drivers/gpu/drm/`, `include/drm/`: Linux v6.13's GEM DMA, fb-DMA and
  simple-KMS helpers, which drm-kmod does not build.  GPL-2.0, MIT.
- `komeda/`: the module build.
- `freebsd/`: FreeBSD glue (BSD-2-Clause):
  - `komeda_freebsd_bus.c`: the ACPI driver (`CIXH5010`).  Attaches only
    to controllers the firmware has lit (`hw.komeda.dpu_mask`, DPU4 by
    default), and reads the timing of the pipelines the firmware drives.
  - `komeda_freebsd.c`: the platform device komeda attaches to;
    devicetree nodes and graph made from the ACPI description; SCMI clocks
    by the names of the `CLKT` table; write-combined contiguous DMA memory;
    and a bridge with a DisplayPort connector whose only mode is the
    firmware's.
  - `include/`: Linux headers LinuxKPI and drm-kmod lack or leave empty,
    and `komeda_freebsd_compat.h`, included into every file.

Changes to the Linux files are marked `#ifdef __linux__` / `__FreeBSD__`.

## Building

Against a FreeBSD source tree with the `sky1_scmi` driver (branch
`orangepi-6-plus` of [JamesKane/freebsd-src](https://github.com/JamesKane/freebsd-src))
and a drm-kmod checkout:

    make DRMKMOD=/path/to/drm-kmod SYSDIR=/path/to/src/sys

## Status

komeda drives the controller the firmware lit, at the firmware's mode: the
DP transmitter (Trilinear, `CIXH502F`) has no driver yet, so komeda leaves
the firmware's stream running and reprograms it with the same timing (a
reset drops the DP link for good).  sway runs on it, rendering with
panthor into buffers komeda scans out directly.

Not yet: the DP transmitter and its USB-C/DP PHY (other modes, hotplug,
the other outputs), the cursor plane, the IOMMU, runtime suspend (clocks
are kept on), unloading (komeda stays once attached), and releasing
mapped buffers (a mapping keeps its buffer).
