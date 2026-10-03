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

/* GPIOs: none yet (the eDP panel's); every lookup finds none. */
#ifndef _KOMEDA_FREEBSD_LINUX_GPIO_CONSUMER_H_
#define	_KOMEDA_FREEBSD_LINUX_GPIO_CONSUMER_H_

struct device;
struct gpio_desc;

enum gpiod_flags {
	GPIOD_ASIS,
	GPIOD_IN,
	GPIOD_OUT_LOW,
	GPIOD_OUT_HIGH,
};

static inline struct gpio_desc *
devm_gpiod_get_optional(struct device *dev __unused, const char *con_id __unused,
    enum gpiod_flags flags __unused)
{
	return (NULL);
}

static inline int
gpiod_get_value_cansleep(const struct gpio_desc *desc __unused)
{
	return (0);
}

static inline void
gpiod_set_value_cansleep(struct gpio_desc *desc __unused, int value __unused)
{
}

#endif /* !_KOMEDA_FREEBSD_LINUX_GPIO_CONSUMER_H_ */
