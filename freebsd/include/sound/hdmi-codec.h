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
 * The HDMI codec interface, as Linux (v6.14 on) has it, for the DP transmitter's
 * audio: no codec binds to it here, so the transmitter's device registers
 * and stays silent.
 */
#ifndef _KOMEDA_FREEBSD_SOUND_HDMI_CODEC_H_
#define	_KOMEDA_FREEBSD_SOUND_HDMI_CODEC_H_

#include <linux/types.h>
#include <linux/hdmi.h>

struct device;
struct device_node;
struct snd_soc_component;

typedef int snd_pcm_format_t;

struct snd_aes_iec958 {
	unsigned char	status[24];
	unsigned char	subcode[147];
	unsigned char	pad;
	unsigned char	dig_subframe[4];
};

struct hdmi_codec_daifmt {
	enum {
		HDMI_I2S,
		HDMI_RIGHT_J,
		HDMI_LEFT_J,
		HDMI_DSP_A,
		HDMI_DSP_B,
		HDMI_AC97,
		HDMI_SPDIF,
	} fmt;
	unsigned int	bit_clk_inv:1;
	unsigned int	frame_clk_inv:1;
	unsigned int	bit_clk_provider:1;
	unsigned int	frame_clk_provider:1;
	snd_pcm_format_t bit_fmt;
};

struct hdmi_codec_params {
	struct hdmi_audio_infoframe	cea;
	struct snd_aes_iec958		iec;
	int				sample_rate;
	int				sample_width;
	int				channels;
};

typedef void (*hdmi_codec_plugged_cb)(struct device *dev, bool plugged);

struct hdmi_codec_ops {
	int	(*audio_startup)(struct device *dev, void *data);
	int	(*hw_params)(struct device *dev, void *data,
		    struct hdmi_codec_daifmt *fmt,
		    struct hdmi_codec_params *hparms);
	int	(*prepare)(struct device *dev, void *data,
		    struct hdmi_codec_daifmt *fmt,
		    struct hdmi_codec_params *hparms);
	void	(*audio_shutdown)(struct device *dev, void *data);
	int	(*mute_stream)(struct device *dev, void *data, bool enable,
		    int direction);
	int	(*get_eld)(struct device *dev, void *data, uint8_t *buf,
		    size_t len);
	int	(*get_dai_id)(struct snd_soc_component *comment,
		    struct device_node *endpoint, void *data);
	int	(*hook_plugged_cb)(struct device *dev, void *data,
		    hdmi_codec_plugged_cb fn, struct device *codec_dev);
	unsigned int	no_capture_mute:1;
};

struct hdmi_codec_pdata {
	const struct hdmi_codec_ops	*ops;
	unsigned int	i2s:1;
	unsigned int	no_i2s_playback:1;
	unsigned int	no_i2s_capture:1;
	unsigned int	spdif:1;
	unsigned int	no_spdif_playback:1;
	unsigned int	no_spdif_capture:1;
	int		max_i2s_channels;
	void		*data;
};

#define	HDMI_CODEC_DRV_NAME	"hdmi-audio-codec"

#endif /* !_KOMEDA_FREEBSD_SOUND_HDMI_CODEC_H_ */
