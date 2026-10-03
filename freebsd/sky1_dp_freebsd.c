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
 * The Linux side of the DP glue: the platform devices of CIX Sky1's DP
 * transmitters and USB-C/DP PHYs, and what CIX's drivers for them would get
 * from Linux: device properties and child nodes, regmap, reset control,
 * and the generic PHY API.  And the transmitter's binding to komeda's DRM
 * device, in place of CIX's component binding to its own display driver.
 */

#include <sys/param.h>
#include <sys/bus.h>

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/hdmi.h>

#include <drm/display/drm_dp.h>

#include <drm/drm_device.h>

#include "trilin_dptx.h"
#include "trilin_drm.h"

#include "sky1_dp_freebsd_bus.h"

/* Devices */

struct sky1_dp_fbsd_fwnode {
	struct fwnode_handle	 fwnode;
	const char		*name;
};

struct sky1_dp_fbsd_dev {
	struct list_head		 link;
	struct sky1_dp_fbsd_info	 info;
	struct platform_device		*pdev;
	struct sky1_dp_fbsd_fwnode	 child[SKY1_DP_FBSD_NCHILD];
	struct trilin_dpsub		*dpsub;		/* a transmitter's */
};

static LIST_HEAD(sky1_dp_fbsd_devs);
static DEFINE_MUTEX(sky1_dp_fbsd_lock);

static struct sky1_dp_fbsd_dev *
sky1_dp_fbsd_find(const struct device *dev)
{
	struct sky1_dp_fbsd_dev *d;

	list_for_each_entry(d, &sky1_dp_fbsd_devs, link)
		if (&d->pdev->dev == dev)
			return (d);
	return (NULL);
}

static void
sky1_dp_fbsd_pdev_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	kfree(pdev->resource);
	kfree(pdev);
}

int
sky1_dp_fbsd_linux_attach(const struct sky1_dp_fbsd_info *info)
{
	struct sky1_dp_fbsd_dev *d;
	struct platform_device *pdev;
	struct resource *res;
	int error, i, n;

	linux_set_current(curthread);
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
	res = kcalloc(info->nmem + 1, sizeof(*res), GFP_KERNEL);
	d->info = *info;
	for (i = 0; i < info->nchild; i++)
		d->child[i].name = d->info.child[i];
	for (n = 0; n < info->nmem; n++) {
		res[n].start = info->mem_pa[n];
		res[n].end = info->mem_pa[n] + info->mem_size[n] - 1;
		res[n].flags = IORESOURCE_MEM;
	}
	if (info->irq >= 0) {
		/* LinuxKPI's request_irq() takes FreeBSD interrupt numbers. */
		res[n].start = res[n].end = info->irq;
		res[n].flags = IORESOURCE_IRQ;
		n++;
	}
	pdev->name = info->driver;
	pdev->id = PLATFORM_DEVID_NONE;
	pdev->resource = res;
	pdev->num_resources = n;
	/* LinuxKPI names it and sets up DMA through the ACPI device. */
	pdev->dev.bsddev = info->dev;
	pdev->dev.release = sky1_dp_fbsd_pdev_release;
	d->pdev = pdev;
	mutex_lock(&sky1_dp_fbsd_lock);
	list_add_tail(&d->link, &sky1_dp_fbsd_devs);
	mutex_unlock(&sky1_dp_fbsd_lock);
	/* Its driver, if any, probes it when the driver registers. */
	error = platform_device_register(pdev);
	if (error != 0) {
		mutex_lock(&sky1_dp_fbsd_lock);
		list_del(&d->link);
		mutex_unlock(&sky1_dp_fbsd_lock);
		platform_device_put(pdev);
		kfree(d);
	}
	return (error);
}

void
sky1_dp_fbsd_linux_detach(device_t dev)
{
	struct sky1_dp_fbsd_dev *d, *t;

	linux_set_current(curthread);
	mutex_lock(&sky1_dp_fbsd_lock);
	list_for_each_entry_safe(d, t, &sky1_dp_fbsd_devs, link) {
		if (d->info.dev != dev)
			continue;
		list_del(&d->link);
		mutex_unlock(&sky1_dp_fbsd_lock);
		platform_device_unregister(d->pdev);
		kfree(d);
		return;
	}
	mutex_unlock(&sky1_dp_fbsd_lock);
}

/* Device properties: ACPI _DSD, by FreeBSD's device_get_property(). */

bool
device_property_present(const struct device *dev, const char *name)
{
	return (dev->bsddev != NULL && device_has_property(dev->bsddev, name));
}

int
device_property_read_u32(const struct device *dev, const char *name,
    u32 *val)
{
	uint32_t v;

	if (dev->bsddev == NULL || device_get_property(dev->bsddev, name, &v,
	    sizeof(v), DEVICE_PROP_UINT32) < 0)
		return (-EINVAL);
	*val = v;
	return (0);
}

int
device_property_read_u8(const struct device *dev, const char *name, u8 *val)
{
	u32 v;
	int error;

	if ((error = device_property_read_u32(dev, name, &v)) != 0)
		return (error);
	*val = v;
	return (0);
}

int
device_property_read_string(const struct device *dev, const char *name,
    const char **val)
{
	if (dev->bsddev == NULL ||
	    sky1_fbsd_string_prop(dev->bsddev, name, val) != 0)
		return (-EINVAL);
	return (0);
}

/* Firmware nodes: a device's child nodes, by name. */

struct fwnode_handle *
dev_fwnode(const struct device *dev)
{
	return (dev->fwnode);
}

const char *
fwnode_get_name(const struct fwnode_handle *fwnode)
{
	return (container_of(fwnode, struct sky1_dp_fbsd_fwnode,
	    fwnode)->name);
}

struct fwnode_handle *
device_get_next_child_node(const struct device *dev,
    struct fwnode_handle *child)
{
	struct sky1_dp_fbsd_dev *d;
	int i;

	if ((d = sky1_dp_fbsd_find(dev)) == NULL || d->info.nchild == 0)
		return (NULL);
	if (child == NULL)
		return (&d->child[0].fwnode);
	for (i = 0; i < d->info.nchild - 1; i++)
		if (child == &d->child[i].fwnode)
			return (&d->child[i + 1].fwnode);
	return (NULL);
}

/* regmap: through the driver's accessors. */

struct regmap *
devm_regmap_init(struct device *dev, const void *bus __unused,
    void *context, const struct regmap_config *config)
{
	struct regmap *map;

	if (config->reg_read == NULL || config->reg_write == NULL)
		return (ERR_PTR(-EINVAL));
	map = devm_kzalloc(dev, sizeof(*map), GFP_KERNEL);
	if (map == NULL)
		return (ERR_PTR(-ENOMEM));
	map->context = context;
	map->config = *config;
	return (map);
}

/* Reset control: RST0's lines, active low, by the ACPI RSTL table. */

struct reset_control {
	unsigned int	offset;
	unsigned int	bit;
};

struct reset_control *
devm_reset_control_get(struct device *dev, const char *id)
{
	struct reset_control *rstc;
	unsigned long index;
	unsigned int bit, offset;

	if (dev->bsddev == NULL || id == NULL ||
	    sky1_fbsd_rstl_index(dev->bsddev, id, &index) != 0 ||
	    sky1_fbsd_reset_signal(index, &offset, &bit) != 0)
		return (ERR_PTR(-ENOENT));
	rstc = devm_kzalloc(dev, sizeof(*rstc), GFP_KERNEL);
	if (rstc == NULL)
		return (ERR_PTR(-ENOMEM));
	rstc->offset = offset;
	rstc->bit = bit;
	return (rstc);
}

int
reset_control_assert(struct reset_control *rstc)
{
	if (IS_ERR_OR_NULL(rstc))
		return (0);
	return (-sky1_fbsd_rst0_update(rstc->offset, rstc->bit, false));
}

int
reset_control_deassert(struct reset_control *rstc)
{
	if (IS_ERR_OR_NULL(rstc))
		return (0);
	return (-sky1_fbsd_rst0_update(rstc->offset, rstc->bit, true));
}

int
reset_control_reset(struct reset_control *rstc)
{
	int error;

	if ((error = reset_control_assert(rstc)) != 0)
		return (error);
	usleep_range(sky1_fbsd_reset_delay_us(), 2 * sky1_fbsd_reset_delay_us());
	error = reset_control_deassert(rstc);
	usleep_range(sky1_fbsd_reset_delay_us(), 2 * sky1_fbsd_reset_delay_us());
	return (error);
}

/*
 * The generic PHY API: a PHY, its operations, counted power and init,
 * and lookup by name (phy_create_lookup()), by its provider's ACPI device.
 */

struct sky1_dp_fbsd_phy_lookup {
	struct list_head	 link;
	struct phy		*phy;
	char			 con_id[16];
};

static LIST_HEAD(sky1_dp_fbsd_phys);

struct phy *
devm_phy_create(struct device *dev, struct device_node *node __unused,
    const struct phy_ops *ops)
{
	struct phy *phy;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (phy == NULL)
		return (ERR_PTR(-ENOMEM));
	phy->dev = dev;
	phy->ops = ops;
	mutex_init(&phy->mutex);
	return (phy);
}

int
phy_create_lookup(struct phy *phy, const char *con_id,
    const char *dev_id __unused)
{
	struct sky1_dp_fbsd_phy_lookup *l;

	l = kzalloc(sizeof(*l), GFP_KERNEL);
	if (l == NULL)
		return (-ENOMEM);
	l->phy = phy;
	strlcpy(l->con_id, con_id, sizeof(l->con_id));
	mutex_lock(&sky1_dp_fbsd_lock);
	list_add_tail(&l->link, &sky1_dp_fbsd_phys);
	mutex_unlock(&sky1_dp_fbsd_lock);
	return (0);
}

static struct phy *
sky1_dp_fbsd_phy_find(device_t provider, const char *con_id)
{
	struct sky1_dp_fbsd_phy_lookup *l;
	struct phy *phy;

	phy = NULL;
	mutex_lock(&sky1_dp_fbsd_lock);
	list_for_each_entry(l, &sky1_dp_fbsd_phys, link) {
		if (l->phy->dev->bsddev == provider &&
		    strcmp(l->con_id, con_id) == 0) {
			phy = l->phy;
			break;
		}
	}
	mutex_unlock(&sky1_dp_fbsd_lock);
	return (phy);
}

/*
 * A transmitter's "dp_phy" is the PHY child node its _DSD dp_phy names;
 * other names are the device's own PHYs.
 */
struct phy *
devm_phy_optional_get(struct device *dev, const char *string)
{
	struct sky1_dp_fbsd_dev *d;

	if (strcmp(string, "dp_phy") == 0) {
		d = sky1_dp_fbsd_find(dev);
		if (d == NULL || d->info.dp_phy_dev == NULL)
			return (NULL);
		return (sky1_dp_fbsd_phy_find(d->info.dp_phy_dev,
		    d->info.dp_phy_child));
	}
	return (dev->bsddev != NULL ?
	    sky1_dp_fbsd_phy_find(dev->bsddev, string) : NULL);
}

struct phy *
of_phy_simple_xlate(struct device *dev __unused,
    const struct of_phandle_args *args __unused)
{
	return (ERR_PTR(-ENODEV));
}

int
phy_init(struct phy *phy)
{
	int error = 0;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	mutex_lock(&phy->mutex);
	if (phy->init_count++ == 0 && phy->ops->init != NULL &&
	    (error = phy->ops->init(phy)) != 0)
		phy->init_count--;
	mutex_unlock(&phy->mutex);
	return (error);
}

int
phy_exit(struct phy *phy)
{
	int error = 0;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	mutex_lock(&phy->mutex);
	if (phy->init_count > 0 && --phy->init_count == 0 &&
	    phy->ops->exit != NULL)
		error = phy->ops->exit(phy);
	mutex_unlock(&phy->mutex);
	return (error);
}

int
phy_power_on(struct phy *phy)
{
	int error = 0;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	mutex_lock(&phy->mutex);
	if (phy->power_count++ == 0 && phy->ops->power_on != NULL &&
	    (error = phy->ops->power_on(phy)) != 0)
		phy->power_count--;
	mutex_unlock(&phy->mutex);
	return (error);
}

int
phy_power_off(struct phy *phy)
{
	int error = 0;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	mutex_lock(&phy->mutex);
	if (phy->power_count > 0 && --phy->power_count == 0 &&
	    phy->ops->power_off != NULL)
		error = phy->ops->power_off(phy);
	mutex_unlock(&phy->mutex);
	return (error);
}

int
phy_set_mode_ext(struct phy *phy, enum phy_mode mode, int submode)
{
	int error = 0;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	mutex_lock(&phy->mutex);
	if (phy->ops->set_mode != NULL)
		error = phy->ops->set_mode(phy, mode, submode);
	if (error == 0)
		phy->attrs.mode = mode;
	mutex_unlock(&phy->mutex);
	return (error);
}

int
phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	int error;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	if (phy->ops->configure == NULL)
		return (-EOPNOTSUPP);
	mutex_lock(&phy->mutex);
	error = phy->ops->configure(phy, opts);
	mutex_unlock(&phy->mutex);
	return (error);
}

int
phy_validate(struct phy *phy, enum phy_mode mode, int submode,
    union phy_configure_opts *opts)
{
	int error;

	if (IS_ERR_OR_NULL(phy))
		return (0);
	if (phy->ops->validate == NULL)
		return (-EOPNOTSUPP);
	mutex_lock(&phy->mutex);
	error = phy->ops->validate(phy, mode, submode, opts);
	mutex_unlock(&phy->mutex);
	return (error);
}

/* No DP audio (no ALSA): no audio infoframe either. */
ssize_t
hdmi_audio_infoframe_pack_for_dp(const struct hdmi_audio_infoframe *frame
    __unused, struct dp_sdp *sdp __unused, u8 dp_version __unused)
{
	return (-ENODEV);
}

/*
 * The transmitter driving CRTCs (possible_crtcs) of komeda's DRM device:
 * probed, with its encoder and connector made on it, as CIX's component
 * binding does with its own display driver.  The first transmitter for now
 * (one output).
 */
int
sky1_dp_fbsd_dptx_bind(struct drm_device *drm, uint32_t possible_crtcs)
{
	struct sky1_dp_fbsd_dev *d;
	struct trilin_dpsub *dpsub;
	int error;

	mutex_lock(&sky1_dp_fbsd_lock);
	list_for_each_entry(d, &sky1_dp_fbsd_devs, link)
		if (strcmp(d->info.driver, "trilin-dptx") == 0)
			break;
	mutex_unlock(&sky1_dp_fbsd_lock);
	if (&d->link == &sky1_dp_fbsd_devs)
		return (-ENODEV);
	if (d->dpsub != NULL)
		return (-EBUSY);
	/*
	 * Its PHY first: the PHY driver may register after komeda's, and
	 * probing it again (as LinuxKPI does when a driver registers) then
	 * binds the transmitter.
	 */
	if (d->info.dp_phy_dev != NULL &&
	    sky1_dp_fbsd_phy_find(d->info.dp_phy_dev,
	    d->info.dp_phy_child) == NULL)
		return (-EPROBE_DEFER);
	dpsub = devm_kzalloc(&d->pdev->dev, sizeof(*dpsub), GFP_KERNEL);
	if (dpsub == NULL)
		return (-ENOMEM);
	dpsub->dev = &d->pdev->dev;
	if ((error = trilin_dp_probe(dpsub, drm)) != 0) {
		dev_err(dpsub->dev, "probe failed: %d\n", error);
		return (error);
	}
	if ((error = trilin_dp_drm_init(dpsub)) != 0) {
		dev_err(dpsub->dev, "DRM init failed: %d\n", error);
		trilin_dp_remove(dpsub);
		return (error);
	}
	/* It assumes the first CRTC (TRILIN_DPTX_POSSIBLE_CRTCS_SST). */
	dpsub->dp->encoder.base.possible_crtcs = possible_crtcs;
	d->dpsub = dpsub;
	return (0);
}

/* Whether a transmitter is there to bind. */
bool
sky1_dp_fbsd_have_dptx(void)
{
	struct sky1_dp_fbsd_dev *d;
	bool found = false;

	mutex_lock(&sky1_dp_fbsd_lock);
	list_for_each_entry(d, &sky1_dp_fbsd_devs, link)
		if (strcmp(d->info.driver, "trilin-dptx") == 0)
			found = true;
	mutex_unlock(&sky1_dp_fbsd_lock);
	return (found);
}
