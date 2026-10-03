/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 James Kane
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The display controllers of CIX Sky1 (CIXH5010): Arm China Linlon-D60,
 * of Arm's Mali-D71 family, which komeda drives.
 *
 * Their resources: memory 0, the registers; interrupt 0.  Their clocks
 * are SCMI clocks, named in the device's CLKT table: "aclk", and one
 * pixel clock per pipeline ("pipeline@0", "pipeline@1").
 *
 * For now komeda gets only the controllers the firmware has lit (by
 * default DPU4, the one driving DP-4 on the Orange Pi 6 Plus;
 * hw.komeda.dpu_mask chooses), and only the pipelines it drives, whose
 * timing is read here before komeda resets the controller: the DP
 * transmitter has no driver yet, and keeps the stream the firmware set up,
 * so that timing is the only mode komeda may use.
 *
 * This file uses bus resources, so it is built without the Linux headers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <arm64/cix/sky1_scmi.h>

#include "komeda_freebsd_bus.h"

/* D71 registers: 512-byte blocks, the GCU first. */
#define	D71_BLOCK_SIZE		0x200
#define	GLB_CORE_INFO		0x008
#define	 CORE_INFO_NUM_BLOCKS(x)	((x) & 0xff)
#define	BLK_BLOCK_INFO		0x000
#define	 BLOCK_INFO_BLK_ID(x)	(((x) & 0x00f0) >> 4)
#define	 BLOCK_INFO_BLK_TYPE(x)	(((x) & 0xff00) >> 8)
#define	 D71_BLK_TYPE_DOU_BS	0x31
#define	BLK_CONTROL		0x0d0
#define	 BS_CTRL_EN		(1u << 0)
#define	BS_ACTIVESIZE		0x0e0
#define	BS_HINTERVALS		0x0e4
#define	BS_VINTERVALS		0x0e8
#define	BS_SYNC			0x0ec
#define	 BS_SYNC_HSP		(1u << 12)
#define	 BS_SYNC_VSP		(1u << 28)

static SYSCTL_NODE(_hw, OID_AUTO, komeda, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "komeda display controllers");

static u_int komeda_fbsd_dpu_mask = 1u << 4;
SYSCTL_UINT(_hw_komeda, OID_AUTO, dpu_mask, CTLFLAG_RDTUN,
    &komeda_fbsd_dpu_mask, 0,
    "Display controllers (ACPI _UID) komeda may attach to");

static struct komeda_fbsd_softc {
	device_t		dev;
	struct resource		*regs;
	bool			linux_attached;
} *komeda_fbsd_sc;

static char *komeda_fbsd_acpi_ids[] = { "CIXH5010", NULL };

/*
 * The SCMI id of a clock the CLKT table names: packages of the id, the
 * name, and the device.
 */
static int
komeda_fbsd_clk_id(device_t dev, const char *name, uint32_t *id)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg, *e;
	int error;
	u_int i;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	if (ACPI_FAILURE(AcpiEvaluateObject(acpi_get_handle(dev), "CLKT",
	    NULL, &buf)))
		return (ENOENT);
	pkg = buf.Pointer;
	error = ENOENT;
	for (i = 0; pkg->Type == ACPI_TYPE_PACKAGE && i < pkg->Package.Count;
	    i++) {
		e = &pkg->Package.Elements[i];
		if (e->Type != ACPI_TYPE_PACKAGE || e->Package.Count < 2 ||
		    e->Package.Elements[0].Type != ACPI_TYPE_INTEGER ||
		    e->Package.Elements[1].Type != ACPI_TYPE_STRING ||
		    strcmp(e->Package.Elements[1].String.Pointer, name) != 0)
			continue;
		*id = e->Package.Elements[0].Integer.Value;
		error = 0;
		break;
	}
	AcpiOsFree(buf.Pointer);
	return (error);
}

/*
 * The pipelines the firmware drives, and their timing: each pipeline's
 * timing controller (the display output unit's BS block) is enabled.
 */
static void
komeda_fbsd_read_timing(struct komeda_fbsd_softc *sc,
    struct komeda_fbsd_info *info)
{
	struct komeda_fbsd_timing *t;
	uint32_t blk, nblocks, off, pipe, v;

	nblocks = CORE_INFO_NUM_BLOCKS(bus_read_4(sc->regs, GLB_CORE_INFO));
	for (blk = 1; blk < nblocks; blk++) {
		off = blk * D71_BLOCK_SIZE;
		if (off + D71_BLOCK_SIZE > rman_get_size(sc->regs))
			break;
		v = bus_read_4(sc->regs, off + BLK_BLOCK_INFO);
		if (BLOCK_INFO_BLK_TYPE(v) != D71_BLK_TYPE_DOU_BS)
			continue;
		pipe = BLOCK_INFO_BLK_ID(v);
		if (pipe >= KOMEDA_FBSD_NPIPES ||
		    (bus_read_4(sc->regs, off + BLK_CONTROL) & BS_CTRL_EN) == 0)
			continue;
		t = &info->timing[pipe];
		v = bus_read_4(sc->regs, off + BS_ACTIVESIZE);
		t->hactive = v & 0x1fff;
		t->vactive = (v >> 16) & 0x1fff;
		v = bus_read_4(sc->regs, off + BS_HINTERVALS);
		t->hfp = v & 0xfff;
		t->hbp = (v >> 16) & 0x3ff;
		v = bus_read_4(sc->regs, off + BS_VINTERVALS);
		t->vfp = v & 0x3fff;
		t->vbp = (v >> 16) & 0xff;
		v = bus_read_4(sc->regs, off + BS_SYNC);
		t->hsw = v & 0x3ff;
		t->vsw = (v >> 16) & 0xff;
		t->hsync_high = (v & BS_SYNC_HSP) != 0;
		t->vsync_high = (v & BS_SYNC_VSP) != 0;
		if (t->hactive == 0 || t->vactive == 0 ||
		    sky1_scmi_clk_get_rate(info->pxclk_id[pipe],
		    &t->pixclk_hz) != 0 || t->pixclk_hz == 0)
			continue;
		info->output[pipe] = true;
		device_printf(sc->dev, "pipeline %u: %ux%u, %ju Hz pixel clock "
		    "(firmware's)\n", pipe, t->hactive, t->vactive,
		    (uintmax_t)t->pixclk_hz);
	}
}

static void
komeda_fbsd_teardown(struct komeda_fbsd_softc *sc)
{
	if (sc->regs != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->regs), sc->regs);
	sc->regs = NULL;
}

static int
komeda_fbsd_probe(device_t dev)
{
	UINT32 uid;
	int rv;

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, komeda_fbsd_acpi_ids,
	    NULL);
	if (rv > 0)
		return (rv);
	if (ACPI_FAILURE(acpi_GetInteger(acpi_get_handle(dev), "_UID", &uid)) ||
	    uid >= 32 || (komeda_fbsd_dpu_mask & (1u << uid)) == 0)
		return (ENXIO);
	device_set_desc(dev, "Arm China Linlon display controller (CIX Sky1)");
	return (rv);
}

static int
komeda_fbsd_attach(device_t dev)
{
	struct komeda_fbsd_softc *sc = device_get_softc(dev);
	struct komeda_fbsd_info info;
	const ACPI_OBJECT *obj;
	rman_res_t start, count;
	int error, rid;
	u_int i;

	/* komeda is attached to one controller only, for now. */
	if (komeda_fbsd_sc != NULL)
		return (ENXIO);
	sc->dev = dev;
	memset(&info, 0, sizeof(info));
	strlcpy(info.name, acpi_name(acpi_get_handle(dev)), sizeof(info.name));

	rid = 0;
	sc->regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->regs == NULL ||
	    bus_get_resource(dev, SYS_RES_IRQ, 0, &start, &count) != 0) {
		device_printf(dev, "no registers or interrupt\n");
		error = ENXIO;
		goto fail;
	}
	info.pa = rman_get_start(sc->regs);
	info.size = rman_get_size(sc->regs);
	info.irq = start;

	if ((error = komeda_fbsd_clk_id(dev, "aclk", &info.aclk_id)) != 0 ||
	    (error = komeda_fbsd_clk_id(dev, "pipeline@0",
	    &info.pxclk_id[0])) != 0 ||
	    (error = komeda_fbsd_clk_id(dev, "pipeline@1",
	    &info.pxclk_id[1])) != 0) {
		device_printf(dev, "no clocks in CLKT\n");
		goto fail;
	}
	if (ACPI_SUCCESS(acpi_GetProperty(dev, "aclk_freq_fixed", &obj)) &&
	    obj->Type == ACPI_TYPE_INTEGER)
		info.aclk_fixed_hz = obj->Integer.Value;

	komeda_fbsd_read_timing(sc, &info);
	for (i = 0; i < KOMEDA_FBSD_NPIPES && !info.output[i]; i++)
		;
	if (i == KOMEDA_FBSD_NPIPES) {
		device_printf(dev, "not lit by the firmware: left alone\n");
		error = ENXIO;
		goto fail;
	}

	/* The registers are komeda's from now on. */
	komeda_fbsd_teardown(sc);
	komeda_fbsd_sc = sc;
	error = -komeda_fbsd_linux_attach(dev, &info);
	if (error != 0) {
		komeda_fbsd_linux_detach();
		komeda_fbsd_sc = NULL;
		return (error);
	}
	sc->linux_attached = true;
	return (0);
fail:
	komeda_fbsd_teardown(sc);
	return (error);
}

static int
komeda_fbsd_detach(device_t dev)
{
	struct komeda_fbsd_softc *sc = device_get_softc(dev);

	if (sc->linux_attached) {
		/* Closing files later would call into the unloaded module. */
		if (komeda_fbsd_linux_busy())
			return (EBUSY);
		komeda_fbsd_linux_detach();
		komeda_fbsd_sc = NULL;
	}
	komeda_fbsd_teardown(sc);
	return (0);
}

static device_method_t komeda_fbsd_methods[] = {
	DEVMETHOD(device_probe,		komeda_fbsd_probe),
	DEVMETHOD(device_attach,	komeda_fbsd_attach),
	DEVMETHOD(device_detach,	komeda_fbsd_detach),
	DEVMETHOD_END
};

static driver_t komeda_fbsd_driver = {
	"komeda",
	komeda_fbsd_methods,
	sizeof(struct komeda_fbsd_softc),
};

DRIVER_MODULE(komeda, acpi, komeda_fbsd_driver, 0, 0);
ACPI_PNP_INFO(komeda_fbsd_acpi_ids);
MODULE_DEPEND(komeda, acpi, 1, 1, 1);
MODULE_DEPEND(komeda, drmn, 2, 2, 2);
MODULE_DEPEND(komeda, dmabuf, 1, 1, 1);
MODULE_DEPEND(komeda, linuxkpi, 1, 1, 1);
MODULE_DEPEND(komeda, linuxkpi_video, 1, 1, 1);
MODULE_DEPEND(komeda, lindebugfs, 1, 1, 1);
