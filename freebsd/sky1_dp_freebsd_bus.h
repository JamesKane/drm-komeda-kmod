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
 * Between the FreeBSD side of the DP glue (sky1_dp_freebsd_bus.c: ACPI
 * drivers for the DP transmitters and USB-C/DP PHYs, and helpers) and its
 * Linux side (sky1_dp_freebsd.c).
 */
#ifndef _SKY1_DP_FREEBSD_BUS_H_
#define	_SKY1_DP_FREEBSD_BUS_H_

#define	SKY1_DP_FBSD_NMEM	4
#define	SKY1_DP_FBSD_NCHILD	4

struct sky1_dp_fbsd_info {
	const char	*driver;		/* the Linux driver's name */
	device_t	 dev;			/* its ACPI device */
	int		 nmem;
	uint64_t	 mem_pa[SKY1_DP_FBSD_NMEM], mem_size[SKY1_DP_FBSD_NMEM];
	int		 irq;			/* or -1 */
	/* Child nodes (a PHY's: USBP, UDPP). */
	int		 nchild;
	char		 child[SKY1_DP_FBSD_NCHILD][8];
	/* A transmitter's PHY (_DSD dp_phy): its device and child node. */
	device_t	 dp_phy_dev;
	char		 dp_phy_child[8];
};

/* Linux side */
int	sky1_dp_fbsd_linux_attach(const struct sky1_dp_fbsd_info *info);
void	sky1_dp_fbsd_linux_detach(device_t dev);

/* FreeBSD side */
int	sky1_fbsd_clkt_id(device_t dev, const char *name, uint32_t *id);
int	sky1_fbsd_rstl_index(device_t dev, const char *name,
	    unsigned long *index);
int	sky1_fbsd_rst0_update(unsigned int offset, unsigned int bit, bool set);
int	sky1_fbsd_string_prop(device_t dev, const char *name,
	    const char **val);

#endif /* !_SKY1_DP_FREEBSD_BUS_H_ */
