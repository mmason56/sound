/*
 *  cnl_rt274.c - ASOC Machine driver for CNL
 *
 *  Copyright (C) 2016-17 Intel Corp
 *  Author: Guneshwor Singh <guneshwor.o.singh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/module.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../../codecs/rt274.h"

#define CNL_FREQ_OUT		19200000
#define CNL_BE_FIXUP_RATE	48000
#define RT274_CODEC_DAI		"rt274-aif1"

static int cnl_rt274_clock_control(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_dai *codec_dai =
		snd_soc_card_get_codec_dai(card, RT274_CODEC_DAI);
	int ret, ratio = 100;

	if (!codec_dai)
		return -EINVAL;

	/* Codec needs clock for Jack detection and button press */
	ret = snd_soc_dai_set_sysclk(codec_dai, RT274_SCLK_S_PLL2,
				     CNL_FREQ_OUT, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "set codec sysclk failed: %d\n", ret);
		return ret;
	}

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		ret = snd_soc_dai_set_bclk_ratio(codec_dai, ratio);
		if (ret) {
			dev_err(codec_dai->dev,
				"set bclk ratio failed: %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_pll(codec_dai, 0, RT274_PLL2_S_BCLK,
					  CNL_BE_FIXUP_RATE * ratio,
					  CNL_FREQ_OUT);
		if (ret) {
			dev_err(codec_dai->dev,
				"enable PLL2 failed: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static struct snd_soc_jack cnl_headset;

/* Headset jack detection DAPM pins */
static struct snd_soc_jack_pin cnl_headset_pins[] = {
	{
		.pin = "Mic Jack",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

static const struct snd_kcontrol_new cnl_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
};

static const struct snd_soc_dapm_widget cnl_rt274_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			    cnl_rt274_clock_control,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static int cnl_dmic_fixup(struct snd_soc_pcm_runtime *rtd,
			  struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	if (params_channels(params) == 2)
		channels->min = channels->max = 2;
	else
		channels->min = channels->max = 4;

	return 0;
}

static const struct snd_soc_dapm_route cnl_map[] = {
	{"Headphone Jack", NULL, "HPO Pin"},
	{"MIC", NULL, "Mic Jack"},
	{"DMic", NULL, "SoC DMIC"},
	{"DMIC01 Rx", NULL, "Capture"},
	{"dmic01_hifi", NULL, "DMIC01 Rx"},

	{"AIF1 Playback", NULL, "ssp0 Tx"},
	{"ssp0 Tx", NULL, "codec1_out"},
	{"ssp0 Tx", NULL, "codec0_out"},

	{"ssp0 Rx", NULL, "AIF1 Capture"},
	{"codec0_in", NULL, "ssp0 Rx"},

	{"Headphone Jack", NULL, "Platform Clock"},
	{"Mic Jack", NULL, "Platform Clock"},
};

static int cnl_rt274_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_card *card = runtime->card;
	struct snd_soc_dai *codec_dai = runtime->codec_dai;
	int ret;

	ret = snd_soc_card_jack_new(runtime->card, "Headset",
		SND_JACK_HEADSET, &cnl_headset,
		cnl_headset_pins, ARRAY_SIZE(cnl_headset_pins));
	if (ret)
		return ret;

	ret = snd_soc_codec_set_jack(codec, &cnl_headset, NULL);
	if (ret)
		return ret;

	/* TDM 4 slots 24 bit, set Rx & Tx bitmask to 4 active slots */
	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0xf, 0xf, 4, 24);
	if (ret < 0) {
		dev_err(runtime->dev, "can't set codec pcm format %d\n", ret);
		return ret;
	}

	card->dapm.idle_bias_off = true;

	return 0;
}

static int cnl_be_fixup(struct snd_soc_pcm_runtime *rtd,
			    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = CNL_BE_FIXUP_RATE;
	channels->min = channels->max = 2;
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
		     SNDRV_PCM_FORMAT_S24_LE);

	return 0;
}

static struct snd_soc_dai_link cnl_rt274_dailink[] = {
	{
		.name = "SSP0-Codec",
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "i2c-INT34C2:00",
		.codec_dai_name = "rt274-aif1",
		.platform_name = "0000:00:1f.3",
		.be_hw_params_fixup = cnl_be_fixup,
		.no_pcm = 1,
		.ignore_pmdown_time = 1,
		.dai_fmt = SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = cnl_rt274_init,
	},
	{
		.name = "dmic01",
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = "0000:00:1f.3",
		.be_hw_params_fixup = cnl_dmic_fixup,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.dpcm_capture = 1,
	},
};

static int
cnl_add_dai_link(struct snd_soc_card *card, struct snd_soc_dai_link *link)
{
	link->platform_name = "0000:00:1f.3";
	link->nonatomic = 1;

	return 0;
}

/* SoC card */
static struct snd_soc_card snd_soc_card_cnl = {
	.name = "cnl-audio",
	.dai_link = cnl_rt274_dailink,
	.num_links = ARRAY_SIZE(cnl_rt274_dailink),
	.dapm_widgets = cnl_rt274_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cnl_rt274_widgets),
	.dapm_routes = cnl_map,
	.num_dapm_routes = ARRAY_SIZE(cnl_map),
	.controls = cnl_controls,
	.num_controls = ARRAY_SIZE(cnl_controls),
	.add_dai_link = cnl_add_dai_link,
	.fully_routed = true,
};

static int snd_cnl_rt274_probe(struct platform_device *pdev)
{
	snd_soc_card_cnl.dev = &pdev->dev;

	return devm_snd_soc_register_card(&pdev->dev, &snd_soc_card_cnl);
}

static struct platform_driver snd_cnl_rt274_driver = {
	.driver = {
		.name = "cnl_rt274",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_cnl_rt274_probe,
};

module_platform_driver(snd_cnl_rt274_driver);

MODULE_AUTHOR("Guneshwor Singh <guneshwor.o.singh@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cnl_rt274");
