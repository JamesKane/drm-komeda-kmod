# The komeda DRM driver (Arm Mali-D71 family and Arm China Linlon display
# controllers), built against drm-kmod.

DRMKMOD?=	${.CURDIR:H:H}/drm-kmod
.include "${DRMKMOD}/kconfig.mk"

DISPDIR=	${.CURDIR:H}/drivers/gpu/drm/arm/display

DPTXDIR=	${.CURDIR:H}/drivers/gpu/drm/cix/dptx

.PATH:	${DISPDIR}/komeda ${DISPDIR}/komeda/d71 ${.CURDIR:H}/drivers/gpu/drm \
	${DPTXDIR} ${DPTXDIR}/hdcp ${.CURDIR:H}/drivers/phy/cix \
	${.CURDIR:H}/drivers/reset \
	${.CURDIR:H}/freebsd

KMOD=	komeda

# The PHY driver first: it registers (LinuxKPI's module_init(), in object
# order) and probes its PHYs before komeda's driver binds the transmitter.
SRCS=	phy-cix-usbdp.c

SRCS+=	komeda_color_mgmt.c \
	komeda_crtc.c \
	komeda_dev.c \
	komeda_drv.c \
	komeda_event.c \
	komeda_format_caps.c \
	komeda_framebuffer.c \
	komeda_kms.c \
	komeda_pipeline.c \
	komeda_pipeline_state.c \
	komeda_plane.c \
	komeda_private_obj.c \
	komeda_wb_connector.c \
	d71_component.c \
	d71_dev.c

# What drm-kmod (Linux v6.13) does not build.
SRCS+=	drm_fbdev_dma.c \
	drm_fb_dma_helper.c \
	drm_gem_dma_helper.c \
	drm_simple_kms_helper.c

# CIX's Trilinear DP transmitter and USB-C/DP PHY (without the component
# binding to CIX's own display driver, trilin_dptx_cix.c, nor the eDP panel
# driver it registers, cix_edp_panel.c).
SRCS+=	dptx_infoframe.c \
	trilin_drm.c \
	trilin_drm_mst.c \
	trilin_dptx.c \
	trilin_dptx_audio.c \
	trilin_edp_phy.c \
	trilin_host_tmr.c \
	trilin_usbdp_phy.c \
	cix_hdcp.c \
	cix_hdcp_ioctl.c \
	hdcp2_tx_tmr.c \
	reset-sky1.c

SRCS+=	komeda_freebsd.c \
	komeda_freebsd_bus.c \
	sky1_dp_freebsd.c \
	sky1_dp_freebsd_bus.c

SRCS+=	opt_acpi.h \
	opt_drm.h \
	opt_platform.h \
	opt_vm.h \
	acpi_if.h \
	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h

.include <bsd.kmod.mk>

.include "${DRMKMOD}/linuxkpi_version.mk"
.include "${DRMKMOD}/compiler_flags.mk"

CFLAGS+= -I${.CURDIR:H}/freebsd/include
CFLAGS+= -I${.CURDIR:H}/include
CFLAGS+= -I${DISPDIR}/include
CFLAGS+= -I${DISPDIR}/komeda
CFLAGS+= -I${DISPDIR}/komeda/d71
CFLAGS+= -I${DPTXDIR} -I${DPTXDIR}/hdcp
CFLAGS+= -I${.CURDIR:H}/freebsd
CFLAGS+= -I${DRMKMOD}/linuxkpi/gplv2/include
CFLAGS+= -I${DRMKMOD}/linuxkpi/bsd/include
CFLAGS+= -I${SYSDIR}/compat/linuxkpi/common/include
CFLAGS+= -I${SYSDIR}/compat/linuxkpi/dummy/include
CFLAGS+= -I${DRMKMOD}/include
CFLAGS+= -I${DRMKMOD}/include/drm
CFLAGS+= -I${DRMKMOD}/include/uapi
CFLAGS+= -I${DRMKMOD}/include/uapi/drm
CFLAGS+= -I${DRMKMOD}/drivers/gpu

CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'
CFLAGS+= '-DLINUXKPI_PARAM_PREFIX=komeda_' -DDRM_SYSCTL_PARAM_PREFIX=_${KMOD}
CFLAGS+= ${KCONFIG:C/(.*)/-DCONFIG_\1/}
CFLAGS+= -include ${.CURDIR:H}/freebsd/include/komeda_freebsd_rename.h
CFLAGS+= -include ${.CURDIR:H}/freebsd/include/komeda_freebsd_compat.h
CFLAGS+= -DCONFIG_DRM_KOMEDA -DCONFIG_MMU

CFLAGS.komeda_freebsd_bus.c+= -DKOMEDA_FBSD_BUS
CFLAGS.sky1_dp_freebsd_bus.c+= -DKOMEDA_FBSD_BUS
CWARNFLAGS+=	-Wno-format -Wno-gnu-pointer-arith -Wno-cast-qual
CWARNFLAGS.reset-sky1.c+=	-Wno-unused-const-variable	# the FCH table
# Written for a newer DRM core: connector mode_valid() takes a const mode.
CWARNFLAGS.trilin_drm.c+=	-Wno-incompatible-function-pointer-types
CWARNFLAGS.trilin_drm_mst.c+=	-Wno-incompatible-function-pointer-types
CWARNFLAGS.phy-cix-usbdp.c+=	-Wno-unused-but-set-variable
