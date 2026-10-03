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

/* Clocks: SCMI clocks on Sky1 (komeda_freebsd.c). */
#ifndef _KOMEDA_FREEBSD_LINUX_CLK_H_
#define	_KOMEDA_FREEBSD_LINUX_CLK_H_

struct device;
struct device_node;
struct clk;

struct clk	*devm_clk_get(struct device *dev, const char *id);
void		 devm_clk_put(struct device *dev, struct clk *clk);
struct clk	*of_clk_get_by_name(struct device_node *np, const char *name);
void		 clk_put(struct clk *clk);
int		 clk_prepare_enable(struct clk *clk);
void		 clk_disable_unprepare(struct clk *clk);
unsigned long	 clk_get_rate(struct clk *clk);
int		 clk_set_rate(struct clk *clk, unsigned long rate);
long		 clk_round_rate(struct clk *clk, unsigned long rate);

#endif /* !_KOMEDA_FREEBSD_LINUX_CLK_H_ */
