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
 * regmap, for the CIX Sky1 USB-C/DP PHY: registers reached through the
 * driver's own reg_read/reg_write (LinuxKPI's <linux/regmap.h> is empty).
 */
#ifndef _KOMEDA_FREEBSD_LINUX_REGMAP_H_
#define	_KOMEDA_FREEBSD_LINUX_REGMAP_H_

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/err.h>

struct device;

struct regmap_config {
	int		reg_bits;
	int		reg_stride;
	int		val_bits;
	bool		fast_io;
	unsigned int	max_register;
	int		(*reg_write)(void *context, unsigned int reg,
			    unsigned int val);
	int		(*reg_read)(void *context, unsigned int reg,
			    unsigned int *val);
};

struct reg_default {
	unsigned int	reg;
	unsigned int	def;
};

struct reg_sequence {
	unsigned int	reg;
	unsigned int	def;
	unsigned int	delay_us;
};

struct regmap {
	void			*context;
	struct regmap_config	 config;
};

struct regmap	*devm_regmap_init(struct device *dev, const void *bus,
		    void *context, const struct regmap_config *config);

static inline int
regmap_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	return (map->config.reg_write(map->context, reg, val));
}

static inline int
regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
	return (map->config.reg_read(map->context, reg, val));
}

static inline int
regmap_update_bits(struct regmap *map, unsigned int reg, unsigned int mask,
    unsigned int val)
{
	unsigned int old;
	int error;

	if ((error = regmap_read(map, reg, &old)) != 0)
		return (error);
	return (regmap_write(map, reg, (old & ~mask) | (val & mask)));
}

static inline int
regmap_multi_reg_write(struct regmap *map, const struct reg_sequence *regs,
    int num)
{
	int error, i;

	for (i = 0; i < num; i++) {
		if ((error = regmap_write(map, regs[i].reg, regs[i].def)) != 0)
			return (error);
		if (regs[i].delay_us != 0)
			udelay(regs[i].delay_us);
	}
	return (0);
}

#endif /* !_KOMEDA_FREEBSD_LINUX_REGMAP_H_ */
