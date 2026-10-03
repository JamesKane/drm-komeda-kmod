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
 * drm-kmod's <drm/drm_of.h> is empty: komeda's output bridge comes from
 * komeda_freebsd.c.
 */
#ifndef _KOMEDA_FREEBSD_DRM_OF_H_
#define	_KOMEDA_FREEBSD_DRM_OF_H_

#include <linux/types.h>
#include <drm/drm_bridge.h>

struct device;
struct device_node;

/*
 * <drm/drm_bridge.h> has an inline devm_drm_of_get_bridge() returning
 * -ENODEV without CONFIG_OF: komeda calls the glue's instead.
 */
struct drm_bridge *komeda_fbsd_of_get_bridge(struct device *dev,
		    struct device_node *np, u32 port, u32 endpoint);
#define	devm_drm_of_get_bridge(dev, np, port, endpoint)			\
	komeda_fbsd_of_get_bridge((dev), (np), (port), (endpoint))

#endif /* !_KOMEDA_FREEBSD_DRM_OF_H_ */
