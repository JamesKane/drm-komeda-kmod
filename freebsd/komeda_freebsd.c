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
 * The Linux side of the glue: the platform device komeda attaches to, and
 * what komeda would get from devicetree and Linux's clock, DMA and bridge
 * frameworks.
 */

#include <sys/param.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_modes.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>

#include <arm64/cix/sky1_scmi.h>

#include "sky1_dp_freebsd_bus.h"

#include "komeda_freebsd_bus.h"

/* komeda's only identification function, for every compatible. */
struct komeda_chip_info;
struct komeda_dev_funcs;
const struct komeda_dev_funcs *d71_identify(u32 __iomem *reg,
    struct komeda_chip_info *chip);

static struct komeda_fbsd_info komeda_fbsd_info;
static struct platform_device *komeda_fbsd_pdev;

/* Devicetree */

static struct device_node komeda_fbsd_dev_node;
static struct device_node komeda_fbsd_pipe_node[KOMEDA_FBSD_NPIPES];
static struct device_node komeda_fbsd_port_node[KOMEDA_FBSD_NPIPES];
static struct device_node komeda_fbsd_output_node[KOMEDA_FBSD_NPIPES];
static char komeda_fbsd_pipe_name[KOMEDA_FBSD_NPIPES][16];

/*
 * The controller's node, with a "pipeline" child for each pipeline;
 * those the firmware drives have an output port, linked to an output.
 */
static void
komeda_fbsd_make_nodes(void)
{
	struct device_node *np;
	int i;

	komeda_fbsd_dev_node.name = "display-controller";
	komeda_fbsd_dev_node.full_name = komeda_fbsd_info.name;
	komeda_fbsd_dev_node.pipe = -1;
	for (i = KOMEDA_FBSD_NPIPES - 1; i >= 0; i--) {
		np = &komeda_fbsd_pipe_node[i];
		snprintf(komeda_fbsd_pipe_name[i],
		    sizeof(komeda_fbsd_pipe_name[i]), "pipeline@%d", i);
		np->name = "pipeline";
		np->full_name = komeda_fbsd_pipe_name[i];
		np->has_reg = true;
		np->reg = i;
		np->pipe = i;
		np->sibling = komeda_fbsd_dev_node.child;
		komeda_fbsd_dev_node.child = np;

		komeda_fbsd_port_node[i].name = "port";
		komeda_fbsd_port_node[i].full_name = "port@0";
		komeda_fbsd_port_node[i].pipe = i;
		komeda_fbsd_output_node[i].name = "output";
		komeda_fbsd_output_node[i].full_name =
		    "output (the firmware's mode)";
		komeda_fbsd_output_node[i].pipe = i;
	}
}

static bool
komeda_fbsd_has_output(const struct device_node *np, u32 port)
{
	return (np != NULL && np->pipe >= 0 && np->pipe < KOMEDA_FBSD_NPIPES &&
	    np->has_reg && port == 0 && komeda_fbsd_info.output[np->pipe]);
}

struct device_node *
of_graph_get_remote_node(const struct device_node *np, u32 port,
    u32 endpoint)
{
	if (!komeda_fbsd_has_output(np, port) || endpoint != 0)
		return (NULL);
	return (&komeda_fbsd_output_node[np->pipe]);
}

struct device_node *
of_graph_get_port_by_id(struct device_node *np, u32 id)
{
	if (!komeda_fbsd_has_output(np, id))
		return (NULL);
	return (&komeda_fbsd_port_node[np->pipe]);
}

const void *
of_device_get_match_data(const struct device *dev __unused)
{
	return ((const void *)d71_identify);
}

/*
 * Clocks: SCMI clocks.  komeda's are always on for now: it turns them off
 * when idle, but the clocks of a controller the firmware lit stay on until
 * there is more than the firmware's mode to go back to.  The engine clock
 * may be fixed (the _DSD's aclk_freq_fixed); komeda's choice of rate is then
 * ignored.  Other devices' clocks (the DP transmitters' and PHYs') are those
 * their ACPI CLKT names, also always on.
 */

struct clk {
	uint32_t	id;
	uint64_t	fixed_hz;
	bool		on;
};

static struct clk komeda_fbsd_aclk;
static struct clk komeda_fbsd_pxclk[KOMEDA_FBSD_NPIPES];

struct clk *
devm_clk_get(struct device *dev, const char *id)
{
	struct clk *clk;
	uint32_t clkt_id;

	if (id == NULL)
		return (ERR_PTR(-ENOENT));
	if (komeda_fbsd_pdev != NULL && dev == &komeda_fbsd_pdev->dev)
		return (strcmp(id, "aclk") == 0 ? &komeda_fbsd_aclk :
		    ERR_PTR(-ENOENT));
	if (dev->bsddev == NULL ||
	    sky1_fbsd_clkt_id(dev->bsddev, id, &clkt_id) != 0)
		return (ERR_PTR(-ENOENT));
	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (clk == NULL)
		return (ERR_PTR(-ENOMEM));
	clk->id = clkt_id;
	return (clk);
}

void
devm_clk_put(struct device *dev __unused, struct clk *clk __unused)
{
}

struct clk *
of_clk_get_by_name(struct device_node *np, const char *name)
{
	if (np == NULL || np->pipe < 0 || np->pipe >= KOMEDA_FBSD_NPIPES ||
	    strcmp(name, "pxclk") != 0)
		return (ERR_PTR(-ENOENT));
	return (&komeda_fbsd_pxclk[np->pipe]);
}

void
clk_put(struct clk *clk __unused)
{
}

int
clk_prepare_enable(struct clk *clk)
{
	int error;

	if (IS_ERR_OR_NULL(clk) || clk->on)
		return (0);
	error = sky1_scmi_clk_enable(clk->id, true);
	if (error == 0)
		clk->on = true;
	return (-error);
}

void
clk_disable_unprepare(struct clk *clk __unused)
{
}

bool
__clk_is_enabled(struct clk *clk)
{
	return (!IS_ERR_OR_NULL(clk) && clk->on);
}

unsigned long
clk_get_rate(struct clk *clk)
{
	uint64_t hz;

	if (IS_ERR_OR_NULL(clk))
		return (0);
	if (clk->fixed_hz != 0)
		return (clk->fixed_hz);
	if (sky1_scmi_clk_get_rate(clk->id, &hz) != 0)
		return (0);
	return (hz);
}

long
clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (!IS_ERR_OR_NULL(clk) && clk->fixed_hz != 0)
		return (clk->fixed_hz);
	return (rate);
}

int
clk_set_rate(struct clk *clk, unsigned long rate)
{
	if (IS_ERR_OR_NULL(clk) || clk->fixed_hz != 0 ||
	    clk_get_rate(clk) == rate)
		return (0);
	return (-sky1_scmi_clk_set_rate(clk->id, rate));
}

/*
 * The output: a bridge with a DisplayPort connector whose only mode is the
 * one the firmware left running, as the DP transmitter has no driver yet.
 */

struct komeda_fbsd_bridge {
	struct drm_bridge	bridge;
	struct drm_connector	connector;
	struct drm_display_mode	mode;
};

static struct komeda_fbsd_bridge *
komeda_fbsd_to_bridge(struct drm_bridge *bridge)
{
	return (container_of(bridge, struct komeda_fbsd_bridge, bridge));
}

static enum drm_connector_status
komeda_fbsd_connector_detect(struct drm_connector *connector __unused,
    bool force __unused)
{
	return (connector_status_connected);
}

static const struct drm_connector_funcs komeda_fbsd_connector_funcs = {
	.detect = komeda_fbsd_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int
komeda_fbsd_connector_get_modes(struct drm_connector *connector)
{
	struct komeda_fbsd_bridge *kb;
	struct drm_display_mode *mode;

	kb = container_of(connector, struct komeda_fbsd_bridge, connector);
	mode = drm_mode_duplicate(connector->dev, &kb->mode);
	if (mode == NULL)
		return (0);
	drm_mode_probed_add(connector, mode);
	return (1);
}

static enum drm_mode_status
komeda_fbsd_connector_mode_valid(struct drm_connector *connector,
    struct drm_display_mode *mode)
{
	struct komeda_fbsd_bridge *kb;

	kb = container_of(connector, struct komeda_fbsd_bridge, connector);
	return (drm_mode_match(mode, &kb->mode, DRM_MODE_MATCH_TIMINGS |
	    DRM_MODE_MATCH_CLOCK | DRM_MODE_MATCH_FLAGS) ? MODE_OK : MODE_BAD);
}

static const struct drm_connector_helper_funcs komeda_fbsd_connector_helper = {
	.get_modes = komeda_fbsd_connector_get_modes,
	.mode_valid = komeda_fbsd_connector_mode_valid,
};

static int
komeda_fbsd_bridge_attach(struct drm_bridge *bridge,
    enum drm_bridge_attach_flags flags __unused)
{
	struct komeda_fbsd_bridge *kb = komeda_fbsd_to_bridge(bridge);
	int error;

	error = drm_connector_init(bridge->dev, &kb->connector,
	    &komeda_fbsd_connector_funcs, DRM_MODE_CONNECTOR_DisplayPort);
	if (error != 0)
		return (error);
	drm_connector_helper_add(&kb->connector, &komeda_fbsd_connector_helper);
	return (drm_connector_attach_encoder(&kb->connector, bridge->encoder));
}

static const struct drm_bridge_funcs komeda_fbsd_bridge_funcs = {
	.attach = komeda_fbsd_bridge_attach,
};

struct drm_bridge *
komeda_fbsd_of_get_bridge(struct device *dev, struct device_node *np, u32 port,
    u32 endpoint)
{
	const struct komeda_fbsd_timing *t;
	struct komeda_fbsd_bridge *kb;
	struct drm_display_mode *m;

	if (!komeda_fbsd_has_output(np, port) || endpoint != 0)
		return (ERR_PTR(-ENODEV));
	kb = devm_kzalloc(dev, sizeof(*kb), GFP_KERNEL);
	if (kb == NULL)
		return (ERR_PTR(-ENOMEM));
	t = &komeda_fbsd_info.timing[np->pipe];
	m = &kb->mode;
	m->clock = t->pixclk_hz / 1000;
	m->hdisplay = t->hactive;
	m->hsync_start = m->hdisplay + t->hfp;
	m->hsync_end = m->hsync_start + t->hsw;
	m->htotal = m->hsync_end + t->hbp;
	m->vdisplay = t->vactive;
	m->vsync_start = m->vdisplay + t->vfp;
	m->vsync_end = m->vsync_start + t->vsw;
	m->vtotal = m->vsync_end + t->vbp;
	m->flags = (t->hsync_high ? DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC) |
	    (t->vsync_high ? DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC);
	m->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(m);
	kb->bridge.funcs = &komeda_fbsd_bridge_funcs;
	dev_info(dev, "%s: " DRM_MODE_FMT "\n", np->full_name, DRM_MODE_ARG(m));
	return (&kb->bridge);
}

/*
 * DMA: contiguous memory, mapped write-combined, as the controller does not
 * snoop CPU caches.  Its DMA address is the device's (through an IOMMU, not
 * the physical address): the pages are found from the kernel mapping.
 */

/*
 * Every mapping of the pages: the pages' own attribute, which user mappings
 * (LinuxKPI's, through the scatter/gather pager) and the direct map take,
 * and the kernel mapping dma_alloc_coherent() made.
 */
static int
komeda_fbsd_dma_set_memattr(void *va, size_t size, vm_memattr_t ma)
{
	vm_offset_t off;

	for (off = 0; off < round_page(size); off += PAGE_SIZE)
		pmap_page_set_memattr(PHYS_TO_VM_PAGE(
		    pmap_kextract((vm_offset_t)va + off)), ma);
	return (pmap_change_attr(va, round_page(size), ma));
}

void *
komeda_fbsd_dma_alloc_wc(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t gfp)
{
	void *va;

	va = dma_alloc_coherent(dev, size, dma_handle, gfp);
	if (va != NULL && komeda_fbsd_dma_set_memattr(va, size,
	    VM_MEMATTR_WRITE_COMBINING) != 0) {
		(void)komeda_fbsd_dma_set_memattr(va, size,
		    VM_MEMATTR_DEFAULT);
		dma_free_coherent(dev, size, va, *dma_handle);
		va = NULL;
	}
	return (va);
}

void
komeda_fbsd_dma_free_wc(struct device *dev, size_t size, void *va,
    dma_addr_t dma_handle)
{
	(void)komeda_fbsd_dma_set_memattr(va, size,
	    VM_MEMATTR_DEFAULT);
	dma_free_coherent(dev, size, va, dma_handle);
}

int
komeda_fbsd_dma_get_sgtable(struct device *dev __unused, struct sg_table *sgt,
    void *va, dma_addr_t dma_handle __unused, size_t size)
{
	int error;

	error = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (error != 0)
		return (error);
	/* Contiguous: one entry, from the first page. */
	sg_set_page(sgt->sgl, PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)va)),
	    round_page(size), 0);
	return (0);
}

/* Platform device */

static void
komeda_fbsd_pdev_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	kfree(pdev->resource);
	kfree(pdev);
}

int
komeda_fbsd_linux_attach(device_t dev, const struct komeda_fbsd_info *info)
{
	struct platform_device *pdev;
	struct resource *res;
	int error, i;

	linux_set_current(curthread);
	komeda_fbsd_info = *info;
	komeda_fbsd_make_nodes();
	komeda_fbsd_aclk.id = info->aclk_id;
	komeda_fbsd_aclk.fixed_hz = info->aclk_fixed_hz;
	for (i = 0; i < KOMEDA_FBSD_NPIPES; i++)
		komeda_fbsd_pxclk[i].id = info->pxclk_id[i];

	pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
	res = kcalloc(2, sizeof(*res), GFP_KERNEL);
	res[0].start = info->pa;
	res[0].end = info->pa + info->size - 1;
	res[0].flags = IORESOURCE_MEM;
	/* LinuxKPI's request_irq() takes FreeBSD interrupt numbers. */
	res[1].start = res[1].end = info->irq;
	res[1].flags = IORESOURCE_IRQ;
	pdev->name = "komeda";
	pdev->id = PLATFORM_DEVID_NONE;
	pdev->resource = res;
	pdev->num_resources = 2;
	/* LinuxKPI names it and sets up DMA through the ACPI device. */
	pdev->dev.bsddev = dev;
	pdev->dev.of_node = &komeda_fbsd_dev_node;
	pdev->dev.release = komeda_fbsd_pdev_release;
	error = platform_device_register(pdev);
	if (error != 0) {
		platform_device_put(pdev);
		return (error);
	}
	komeda_fbsd_pdev = pdev;
	/*
	 * komeda probes it when its driver registers, after this attach
	 * when the module is loaded (LinuxKPI's module_init() runs at
	 * SI_SUB_OFED_MODINIT).
	 */
	return (0);
}

void
komeda_fbsd_linux_detach(void)
{
	linux_set_current(curthread);
	if (komeda_fbsd_pdev != NULL)
		platform_device_unregister(komeda_fbsd_pdev);
	komeda_fbsd_pdev = NULL;
}

/*
 * Whether komeda may not be detached: once it has probed (its driver data
 * set).  The DRM device is komeda's, behind that driver data, so until it
 * can be asked whether anything has it open, komeda stays: unloading under
 * an open file, or while the controller scans a buffer out, would call
 * into a freed module.
 */
bool
komeda_fbsd_linux_busy(void)
{
	return (komeda_fbsd_pdev != NULL &&
	    platform_get_drvdata(komeda_fbsd_pdev) != NULL);
}
