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
 * Devicetree, as komeda sees it: komeda_freebsd.c builds these nodes from
 * the display controller's ACPI description (LinuxKPI's <linux/of.h> is
 * empty).
 */
#ifndef _KOMEDA_FREEBSD_LINUX_OF_H_
#define	_KOMEDA_FREEBSD_LINUX_OF_H_

#include_next <linux/of.h>
#include <linux/types.h>

struct device;

struct device_node {
	const char		*name;		/* "pipeline" */
	const char		*full_name;	/* "pipeline@0" */
	struct device_node	*child;
	struct device_node	*sibling;
	bool			 has_reg;
	u32			 reg;
	int			 pipe;		/* its pipeline, or -1 */
};

struct of_device_id {
	char		name[32];
	char		type[32];
	char		compatible[128];
	const void	*data;
};

static inline struct device_node *
of_node_get(struct device_node *np)
{
	return (np);
}

static inline void
of_node_put(struct device_node *np __unused)
{
}

static inline const char *
of_node_full_name(const struct device_node *np)
{
	return (np != NULL ? np->full_name : "<no-node>");
}

static inline bool
of_node_name_eq(const struct device_node *np, const char *name)
{
	return (np != NULL && strcmp(np->name, name) == 0);
}

static inline int
of_property_read_u32(const struct device_node *np, const char *propname,
    u32 *value)
{
	if (np == NULL || !np->has_reg || strcmp(propname, "reg") != 0)
		return (-EINVAL);
	*value = np->reg;
	return (0);
}

#define	for_each_available_child_of_node(parent, child)			\
	for ((child) = (parent) != NULL ? (parent)->child : NULL;	\
	    (child) != NULL; (child) = (child)->sibling)

const void	*of_device_get_match_data(const struct device *dev);

#endif /* !_KOMEDA_FREEBSD_LINUX_OF_H_ */
