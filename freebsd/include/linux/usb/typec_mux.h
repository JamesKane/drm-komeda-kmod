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
 * USB Type-C orientation switches and mode muxes: not registered (the PHY
 * registers them only when its firmware node asks, which Sky1's ACPI does
 * not; the PHY then stays in its default mode).
 */
#ifndef _KOMEDA_FREEBSD_LINUX_USB_TYPEC_MUX_H_
#define	_KOMEDA_FREEBSD_LINUX_USB_TYPEC_MUX_H_

#include <linux/err.h>

struct fwnode_handle;
struct typec_altmode;

enum typec_orientation {
	TYPEC_ORIENTATION_NONE,
	TYPEC_ORIENTATION_NORMAL,
	TYPEC_ORIENTATION_REVERSE,
};

enum {
	TYPEC_STATE_SAFE,
	TYPEC_STATE_USB,
	TYPEC_STATE_MODAL,
};

struct typec_switch_dev;
struct typec_mux_dev;

struct typec_switch_desc {
	struct fwnode_handle	*fwnode;
	int			(*set)(struct typec_switch_dev *sw,
				    enum typec_orientation orientation);
	const char		*name;
	void			*drvdata;
};

struct typec_mux_state {
	struct typec_altmode	*alt;
	unsigned long		 mode;
	void			*data;
};

struct typec_mux_desc {
	struct fwnode_handle	*fwnode;
	int			(*set)(struct typec_mux_dev *mux,
				    struct typec_mux_state *state);
	const char		*name;
	void			*drvdata;
};

static inline struct typec_switch_dev *
typec_switch_register(struct device *dev __unused,
    const struct typec_switch_desc *desc __unused)
{
	return (ERR_PTR(-ENODEV));
}

static inline void
typec_switch_unregister(struct typec_switch_dev *sw __unused)
{
}

static inline void *
typec_switch_get_drvdata(struct typec_switch_dev *sw __unused)
{
	return (NULL);
}

static inline struct typec_mux_dev *
typec_mux_register(struct device *dev __unused,
    const struct typec_mux_desc *desc __unused)
{
	return (ERR_PTR(-ENODEV));
}

static inline void
typec_mux_unregister(struct typec_mux_dev *mux __unused)
{
}

static inline void *
typec_mux_get_drvdata(struct typec_mux_dev *mux __unused)
{
	return (NULL);
}

#endif /* !_KOMEDA_FREEBSD_LINUX_USB_TYPEC_MUX_H_ */
