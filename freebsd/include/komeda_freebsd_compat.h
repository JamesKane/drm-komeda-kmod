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
 * Linux interfaces komeda uses that LinuxKPI and drm-kmod lack.  Included
 * before everything else in every source file (see komeda/Makefile).
 */
#ifndef _KOMEDA_FREEBSD_COMPAT_H_
#define	_KOMEDA_FREEBSD_COMPAT_H_

#ifndef KOMEDA_FBSD_BUS	/* komeda_freebsd_bus.c uses FreeBSD's headers */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/iosys-map.h>
#include <linux/mm.h>

struct device;
struct sg_table;
struct vm_area_struct;

#ifndef IOSYS_MAP_INIT_VADDR
#define	IOSYS_MAP_INIT_VADDR(v)						\
	((struct iosys_map){ .vaddr = (v), .is_iomem = false })
#endif

static inline void
vm_flags_mod(struct vm_area_struct *vma, unsigned long set,
    unsigned long clear)
{
	vma->vm_flags = (vma->vm_flags | set) & ~clear;
}

/*
 * komeda's output bridge comes from the glue, also in the files that do not
 * include <drm/drm_of.h>: <drm/drm_bridge.h>'s inline one fails.
 */
#include <drm/drm_of.h>

#ifndef SET_RUNTIME_PM_OPS
#define	SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn)		\
	.runtime_suspend = (suspend_fn), .runtime_resume = (resume_fn),	\
	.runtime_idle = (idle_fn),
#endif

/* CIX Sky1's reset lines (drivers/reset/reset-sky1.c). */
int		sky1_fbsd_reset_signal(unsigned long id, unsigned int *offset,
		    unsigned int *bit);
unsigned int	sky1_fbsd_reset_delay_us(void);

/* Mapped through devres, which unmaps as the device goes. */
#define	devm_iounmap(dev, addr)		((void)(dev), (void)(addr))

/* drm-kmod's (Linux v6.13) dma-buf names. */
#define	dma_buf_vmap_unlocked(buf, map)		dma_buf_vmap((buf), (map))
#define	dma_buf_vunmap_unlocked(buf, map)	dma_buf_vunmap((buf), (map))

/*
 * Contiguous memory the CPU maps write-combined: the display controller
 * does not snoop CPU caches (_CCA 0), and LinuxKPI's dma_alloc_attrs()
 * ignores DMA_ATTR_WRITE_COMBINE (komeda_freebsd.c).
 */
void	*komeda_fbsd_dma_alloc_wc(struct device *dev, size_t size,
	    dma_addr_t *dma_handle, gfp_t gfp);
void	 komeda_fbsd_dma_free_wc(struct device *dev, size_t size, void *va,
	    dma_addr_t dma_handle);
int	 komeda_fbsd_dma_get_sgtable(struct device *dev, struct sg_table *sgt,
	    void *va, dma_addr_t dma_handle, size_t size);
#define	dma_alloc_wc(dev, size, h, gfp)					\
	komeda_fbsd_dma_alloc_wc((dev), (size), (h), (gfp))
#define	dma_free_wc(dev, size, va, h)					\
	komeda_fbsd_dma_free_wc((dev), (size), (va), (h))
#define	dma_get_sgtable(dev, sgt, va, h, size)				\
	komeda_fbsd_dma_get_sgtable((dev), (sgt), (va), (h), (size))

#endif /* !KOMEDA_FBSD_BUS */
#endif /* !_KOMEDA_FREEBSD_COMPAT_H_ */
