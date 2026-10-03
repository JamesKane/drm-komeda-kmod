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
 * Device properties (ACPI _DSD, through FreeBSD's device_get_property()),
 * and firmware nodes: a device's child nodes as the glue found them in
 * ACPI (sky1_dp_freebsd.c).
 */
#ifndef _KOMEDA_FREEBSD_LINUX_PROPERTY_H_
#define	_KOMEDA_FREEBSD_LINUX_PROPERTY_H_

#include <linux/types.h>
#include <linux/err.h>
#include <linux/fwnode.h>

struct device;
struct device_node;

bool	device_property_present(const struct device *dev, const char *name);
int	device_property_read_u32(const struct device *dev, const char *name,
	    u32 *val);
int	device_property_read_u8(const struct device *dev, const char *name,
	    u8 *val);
int	device_property_read_string(const struct device *dev,
	    const char *name, const char **val);
#define	device_property_read_bool(dev, name)	device_property_present(dev, name)

struct fwnode_handle *dev_fwnode(const struct device *dev);
const char	*fwnode_get_name(const struct fwnode_handle *fwnode);
struct fwnode_handle *device_get_next_child_node(const struct device *dev,
		    struct fwnode_handle *child);

#define	device_for_each_child_node(dev, child)				\
	for ((child) = device_get_next_child_node((dev), NULL);	\
	    (child) != NULL;						\
	    (child) = device_get_next_child_node((dev), (child)))

static inline void
fwnode_handle_put(struct fwnode_handle *fwnode __unused)
{
}

/* No devicetree nodes behind firmware nodes here. */
static inline struct device_node *
to_of_node(const struct fwnode_handle *fwnode __unused)
{
	return (NULL);
}

/* References are looked up by the glue, not through firmware nodes. */
static inline struct fwnode_handle *
fwnode_find_reference(const struct fwnode_handle *fwnode __unused,
    const char *name __unused, unsigned int index __unused)
{
	return (ERR_PTR(-ENOENT));
}

#endif /* !_KOMEDA_FREEBSD_LINUX_PROPERTY_H_ */
