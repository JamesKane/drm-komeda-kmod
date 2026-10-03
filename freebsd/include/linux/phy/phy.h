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
 * The generic PHY API, as far as CIX Sky1's DP transmitter and USB-C/DP
 * PHY use it (glue): a PHY, its operations, and lookup by name.
 */
#ifndef _KOMEDA_FREEBSD_LINUX_PHY_PHY_H_
#define	_KOMEDA_FREEBSD_LINUX_PHY_PHY_H_

#include <linux/types.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/phy/phy-dp.h>

struct device;
struct device_node;
struct of_phandle_args;

enum phy_mode {
	PHY_MODE_INVALID,
	PHY_MODE_USB_HOST,
	PHY_MODE_USB_HOST_LS,
	PHY_MODE_USB_HOST_FS,
	PHY_MODE_USB_HOST_HS,
	PHY_MODE_USB_HOST_SS,
	PHY_MODE_USB_DEVICE,
	PHY_MODE_USB_DEVICE_LS,
	PHY_MODE_USB_DEVICE_FS,
	PHY_MODE_USB_DEVICE_HS,
	PHY_MODE_USB_DEVICE_SS,
	PHY_MODE_USB_OTG,
	PHY_MODE_UFS_HS_A,
	PHY_MODE_UFS_HS_B,
	PHY_MODE_PCIE,
	PHY_MODE_ETHERNET,
	PHY_MODE_MIPI_DPHY,
	PHY_MODE_SATA,
	PHY_MODE_LVDS,
	PHY_MODE_DP,
};

union phy_configure_opts {
	struct phy_configure_opts_dp	dp;
};

struct phy;

struct phy_ops {
	int	(*init)(struct phy *phy);
	int	(*exit)(struct phy *phy);
	int	(*power_on)(struct phy *phy);
	int	(*power_off)(struct phy *phy);
	int	(*set_mode)(struct phy *phy, enum phy_mode mode, int submode);
	int	(*configure)(struct phy *phy, union phy_configure_opts *opts);
	int	(*validate)(struct phy *phy, enum phy_mode mode, int submode,
		    union phy_configure_opts *opts);
	int	(*reset)(struct phy *phy);
	int	(*calibrate)(struct phy *phy);
	struct module	*owner;
};

struct phy_attrs {
	u32		bus_width;
	u32		max_link_rate;
	enum phy_mode	mode;
};

struct phy {
	struct device		*dev;	/* the provider's */
	const struct phy_ops	*ops;
	struct mutex		 mutex;
	int			 init_count;
	int			 power_count;
	struct phy_attrs	 attrs;
	void			*drvdata;
};

struct phy_provider;

struct phy	*devm_phy_create(struct device *dev, struct device_node *node,
		    const struct phy_ops *ops);
int		 phy_create_lookup(struct phy *phy, const char *con_id,
		    const char *dev_id);
struct phy	*devm_phy_optional_get(struct device *dev, const char *string);
int		 phy_init(struct phy *phy);
int		 phy_exit(struct phy *phy);
int		 phy_power_on(struct phy *phy);
int		 phy_power_off(struct phy *phy);
int		 phy_set_mode_ext(struct phy *phy, enum phy_mode mode,
		    int submode);
int		 phy_configure(struct phy *phy, union phy_configure_opts *opts);
int		 phy_validate(struct phy *phy, enum phy_mode mode, int submode,
		    union phy_configure_opts *opts);

#define	phy_set_mode(phy, mode)		phy_set_mode_ext((phy), (mode), 0)

static inline enum phy_mode
phy_get_mode(struct phy *phy)
{
	return (phy->attrs.mode);
}

static inline int
phy_get_bus_width(struct phy *phy)
{
	return (phy->attrs.bus_width);
}

static inline void
phy_set_bus_width(struct phy *phy, int bus_width)
{
	phy->attrs.bus_width = bus_width;
}

static inline void *
phy_get_drvdata(struct phy *phy)
{
	return (phy->drvdata);
}

static inline void
phy_set_drvdata(struct phy *phy, void *data)
{
	phy->drvdata = data;
}

/* Providers: lookup is by name only (phy_create_lookup()). */
struct phy	*of_phy_simple_xlate(struct device *dev,
		    const struct of_phandle_args *args);

static inline struct phy_provider *
devm_of_phy_provider_register(struct device *dev __unused,
    struct phy *(*xlate)(struct device *, const struct of_phandle_args *)
    __unused)
{
	return ((struct phy_provider *)dev);
}

#endif /* !_KOMEDA_FREEBSD_LINUX_PHY_PHY_H_ */
