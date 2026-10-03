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
 * CIX Sky1's DP transmitters (CIXH502F, Trilinear) and USB-C/DP combo
 * PHYs (CIXH2033), for CIX's Linux drivers through the Linux side of the
 * glue (sky1_dp_freebsd.c); and the ACPI and register helpers it uses.
 *
 * Only DP04 and UCP3 by default, the transmitter driving DP-4 on the
 * Orange Pi 6 Plus and its PHY (hw.komeda.dptx_mask, hw.komeda.udphy_mask):
 * the PHY driver resets its PHY, and the other PHYs carry USB that is in
 * use (UCP3's USB side feeds an unused port).
 *
 * This file uses bus resources, so it is built without the Linux headers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include "sky1_dp_freebsd_bus.h"

SYSCTL_DECL(_hw_komeda);

static u_int sky1_dp_fbsd_dptx_mask = 1u << 4;
SYSCTL_UINT(_hw_komeda, OID_AUTO, dptx_mask, CTLFLAG_RDTUN,
    &sky1_dp_fbsd_dptx_mask, 0,
    "DP transmitters (ACPI _UID) to attach to");
static u_int sky1_dp_fbsd_udphy_mask = 1u << 3;
SYSCTL_UINT(_hw_komeda, OID_AUTO, udphy_mask, CTLFLAG_RDTUN,
    &sky1_dp_fbsd_udphy_mask, 0,
    "USB-C/DP PHYs (ACPI _UID) to attach to; their driver resets them");

/* The RST0 reset block (ACPI RST0). */
#define	SKY1_RST0_PA		0x16000000UL
#define	SKY1_RST0_SIZE		PAGE_SIZE

static struct mtx sky1_fbsd_rst0_mtx;
MTX_SYSINIT(sky1_fbsd_rst0, &sky1_fbsd_rst0_mtx, "sky1 RST0", MTX_DEF);
static volatile uint32_t *sky1_fbsd_rst0;

/* Helpers */

/* The SCMI id of a clock CLKT names: packages of id, name, device. */
int
sky1_fbsd_clkt_id(device_t dev, const char *name, uint32_t *id)
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
 * The RST0 index of a reset line RSTL names: packages of the reset
 * controller, the index, the device, and the name.
 */
int
sky1_fbsd_rstl_index(device_t dev, const char *name, unsigned long *index)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg, *e;
	int error;
	u_int i;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	if (ACPI_FAILURE(AcpiEvaluateObject(acpi_get_handle(dev), "RSTL",
	    NULL, &buf)))
		return (ENOENT);
	pkg = buf.Pointer;
	error = ENOENT;
	for (i = 0; pkg->Type == ACPI_TYPE_PACKAGE && i < pkg->Package.Count;
	    i++) {
		e = &pkg->Package.Elements[i];
		if (e->Type != ACPI_TYPE_PACKAGE || e->Package.Count < 4 ||
		    e->Package.Elements[1].Type != ACPI_TYPE_INTEGER ||
		    e->Package.Elements[3].Type != ACPI_TYPE_STRING ||
		    strcmp(e->Package.Elements[3].String.Pointer, name) != 0)
			continue;
		*index = e->Package.Elements[1].Integer.Value;
		error = 0;
		break;
	}
	AcpiOsFree(buf.Pointer);
	return (error);
}

/* Set or clear a bit of an RST0 register. */
int
sky1_fbsd_rst0_update(unsigned int offset, unsigned int bit, bool set)
{
	uint32_t v;

	if (offset + sizeof(uint32_t) > SKY1_RST0_SIZE)
		return (EINVAL);
	mtx_lock(&sky1_fbsd_rst0_mtx);
	if (sky1_fbsd_rst0 == NULL)
		sky1_fbsd_rst0 = pmap_mapdev(SKY1_RST0_PA, SKY1_RST0_SIZE);
	v = sky1_fbsd_rst0[offset / 4];
	sky1_fbsd_rst0[offset / 4] = set ? v | bit : v & ~bit;
	mtx_unlock(&sky1_fbsd_rst0_mtx);
	return (0);
}

/* A string _DSD property, which lives as long as the device's _DSD. */
int
sky1_fbsd_string_prop(device_t dev, const char *name, const char **val)
{
	const ACPI_OBJECT *obj;

	if (ACPI_FAILURE(acpi_GetProperty(dev, name, &obj)) ||
	    obj->Type != ACPI_TYPE_STRING)
		return (ENOENT);
	*val = obj->String.Pointer;
	return (0);
}

static void
sky1_dp_fbsd_name(ACPI_HANDLE h, char *buf, size_t len)
{
	char name[5];
	ACPI_BUFFER b;

	b.Pointer = name;
	b.Length = sizeof(name);
	if (ACPI_SUCCESS(AcpiGetName(h, ACPI_SINGLE_NAME, &b)))
		strlcpy(buf, name, len);
	else
		buf[0] = '\0';
}

/* The device's memory resources and interrupt. */
static int
sky1_dp_fbsd_resources(device_t dev, struct sky1_dp_fbsd_info *info)
{
	rman_res_t start, count;
	int i;

	for (i = 0; i < SKY1_DP_FBSD_NMEM &&
	    bus_get_resource(dev, SYS_RES_MEMORY, i, &start, &count) == 0;
	    i++) {
		info->mem_pa[i] = start;
		info->mem_size[i] = count;
	}
	info->nmem = i;
	info->irq = bus_get_resource(dev, SYS_RES_IRQ, 0, &start, &count) == 0 ?
	    (int)start : -1;
	return (info->nmem > 0 ? 0 : ENXIO);
}

static bool
sky1_dp_fbsd_probe_uid(device_t dev, char **ids, u_int mask, const char *desc)
{
	UINT32 uid;

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, ids, NULL) > 0 ||
	    ACPI_FAILURE(acpi_GetInteger(acpi_get_handle(dev), "_UID", &uid)) ||
	    uid >= 32 || (mask & (1u << uid)) == 0)
		return (false);
	device_set_desc(dev, desc);
	return (true);
}

/* DP transmitters */

static char *sky1_dptx_ids[] = { "CIXH502F", NULL };

static int
sky1_dptx_probe(device_t dev)
{
	return (sky1_dp_fbsd_probe_uid(dev, sky1_dptx_ids,
	    sky1_dp_fbsd_dptx_mask, "Trilinear DisplayPort transmitter (CIX Sky1)") ?
	    BUS_PROBE_DEFAULT : ENXIO);
}

static int
sky1_dptx_attach(device_t dev)
{
	struct sky1_dp_fbsd_info info;
	const ACPI_OBJECT *obj;
	ACPI_HANDLE parent;
	int error;

	memset(&info, 0, sizeof(info));
	info.driver = "trilin-dptx";
	info.dev = dev;
	if ((error = sky1_dp_fbsd_resources(dev, &info)) != 0)
		return (error);
	/* dp_phy: a reference to the PHY's child node (UCP3.UDPP). */
	if (ACPI_SUCCESS(acpi_GetProperty(dev, "dp_phy", &obj)) &&
	    obj->Type == ACPI_TYPE_LOCAL_REFERENCE &&
	    ACPI_SUCCESS(AcpiGetParent(obj->Reference.Handle, &parent))) {
		info.dp_phy_dev = acpi_get_device(parent);
		sky1_dp_fbsd_name(obj->Reference.Handle, info.dp_phy_child,
		    sizeof(info.dp_phy_child));
	}
	if (info.dp_phy_dev == NULL)
		device_printf(dev, "no dp_phy\n");
	return (-sky1_dp_fbsd_linux_attach(&info));
}

/* USB-C/DP PHYs */

static char *sky1_udphy_ids[] = { "CIXH2033", NULL };

static int
sky1_udphy_probe(device_t dev)
{
	return (sky1_dp_fbsd_probe_uid(dev, sky1_udphy_ids,
	    sky1_dp_fbsd_udphy_mask, "USB-C/DisplayPort combo PHY (CIX Sky1)") ?
	    BUS_PROBE_DEFAULT : ENXIO);
}

static int
sky1_udphy_attach(device_t dev)
{
	struct sky1_dp_fbsd_info info;
	ACPI_HANDLE child;
	int error;

	memset(&info, 0, sizeof(info));
	info.driver = "cix-usbdp-phy";
	info.dev = dev;
	if ((error = sky1_dp_fbsd_resources(dev, &info)) != 0)
		return (error);
	for (child = NULL; info.nchild < SKY1_DP_FBSD_NCHILD &&
	    ACPI_SUCCESS(AcpiGetNextObject(ACPI_TYPE_DEVICE,
	    acpi_get_handle(dev), child, &child));)
		sky1_dp_fbsd_name(child, info.child[info.nchild++],
		    sizeof(info.child[0]));
	return (-sky1_dp_fbsd_linux_attach(&info));
}

static int
sky1_dp_fbsd_detach(device_t dev)
{
	sky1_dp_fbsd_linux_detach(dev);
	return (0);
}

static device_method_t sky1_dptx_methods[] = {
	DEVMETHOD(device_probe,		sky1_dptx_probe),
	DEVMETHOD(device_attach,	sky1_dptx_attach),
	DEVMETHOD(device_detach,	sky1_dp_fbsd_detach),
	DEVMETHOD_END
};

static driver_t sky1_dptx_driver = {
	"sky1_dptx",
	sky1_dptx_methods,
	0,
};

static device_method_t sky1_udphy_methods[] = {
	DEVMETHOD(device_probe,		sky1_udphy_probe),
	DEVMETHOD(device_attach,	sky1_udphy_attach),
	DEVMETHOD(device_detach,	sky1_dp_fbsd_detach),
	DEVMETHOD_END
};

static driver_t sky1_udphy_driver = {
	"sky1_udphy",
	sky1_udphy_methods,
	0,
};

/*
 * Both attach as the module loads, before the Linux drivers probe (at
 * LinuxKPI's module_init()), the PHY's before komeda's (komeda/Makefile):
 * a transmitter finds its PHY then.
 */
DRIVER_MODULE(sky1_udphy, acpi, sky1_udphy_driver, 0, 0);
DRIVER_MODULE(sky1_dptx, acpi, sky1_dptx_driver, 0, 0);
