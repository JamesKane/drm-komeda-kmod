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

/*
 * ACPI: LinuxKPI's ACPI_COMPANION() finds none, so drivers take their
 * devicetree paths, which the glue serves from ACPI; these are for the
 * ACPI paths they still compile.
 */
#include <linux/property.h>
#define	has_acpi_companion(dev)		((void)(dev), false)
#define	acpi_dev_uid_to_integer(adev, uid)	((void)(adev), (void)(uid), -ENODEV)

#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <sys/ioccom.h>
#include <sys/sysctl.h>

#include <linux/arm-smccc.h>

#ifndef IRQF_ONESHOT
#define	IRQF_ONESHOT		0x00002000	/* threaded handlers only */
#endif
#define	system_freezable_wq	system_wq	/* no suspend yet */

/* fbdev ops on DMA memory: LinuxKPI's, the same for every kind. */
#define	__FB_DEFAULT_DMAMEM_OPS_RDWR	__FB_DEFAULT_IOMEM_OPS_RDWR
#define	__FB_DEFAULT_DMAMEM_OPS_DRAW	__FB_DEFAULT_IOMEM_OPS_DRAW

/*
 * A threaded interrupt without a primary handler: Linux's default one wakes
 * the thread.  LinuxKPI calls the primary handler unconditionally (fixed in
 * freebsd-src, but not in kernels built before).
 */
static inline irqreturn_t
komeda_fbsd_irq_wake_thread(int irq __unused, void *arg __unused)
{
	return (IRQ_WAKE_THREAD);
}
#define	devm_request_threaded_irq(dev, irq, handler, thread, flags, name, \
	    arg)							\
	lkpi_request_irq(dev, irq,					\
	    (handler) != NULL ? (handler) : komeda_fbsd_irq_wake_thread,	\
	    thread, flags, name, arg)
#define	wake_up_interruptible_poll(wq, mask)	wake_up_interruptible(wq)
#define	_IOC_SIZE(cmd)		IOCPARM_LEN(cmd)
#define	_IOC_NR(cmd)		((cmd) & 0xff)
#define	is_acpi_node(fwnode)	((void)(fwnode), false)
#define	of_alias_get_id(np, stem)	((void)(np), (void)(stem), -ENODEV)

/* Module parameters (hw.komeda, komeda_freebsd_bus.c). */
SYSCTL_DECL(_hw_komeda);

/*
 * No ALSA: the DP transmitter's audio device is not made (it carries on
 * without).
 */
static inline struct platform_device *
platform_device_register_data(struct device *parent __unused,
    const char *name __unused, int id __unused, const void *data __unused,
    size_t size __unused)
{
	return (ERR_PTR(-ENODEV));
}

/* CIX Sky1's DP transmitters (sky1_dp_freebsd.c). */
struct drm_device;
bool	sky1_dp_fbsd_have_dptx(void);
int	sky1_dp_fbsd_dptx_bind(struct drm_device *drm, uint32_t possible_crtcs);

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
