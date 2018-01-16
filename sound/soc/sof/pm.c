// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <sound/core.h>
#include <sound/soc.h>
#include "sof-priv.h"

int snd_sof_runtime_suspend(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_runtime_suspend);

int snd_sof_runtime_resume(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_runtime_resume);

int snd_sof_resume(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_resume);

int snd_sof_suspend(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_suspend);

int snd_sof_suspend_late(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_suspend_late);

#if 0

int sof_hsw_dsp_load(struct sof_hsw *hsw)
{
	struct sof_dsp *dsp = hsw->dsp;
	struct sof_fw *sof_fw, *t;
	int ret;

	dev_dbg(hsw->dev, "loading audio DSP....");

	ret = sof_dsp_wake(dsp);
	if (ret < 0) {
		dev_err(hsw->dev, "error: failed to wake audio DSP\n");
		return -ENODEV;
	}

	ret = sof_dsp_dma_get_channel(dsp, 0);
	if (ret < 0) {
		dev_err(hsw->dev, "error: cant allocate dma channel %d\n", ret);
		return ret;
	}

	list_for_each_entry_safe_reverse(sof_fw, t, &dsp->fw_list, list) {
		ret = sof_fw_reload(sof_fw);
		if (ret < 0) {
			dev_err(hsw->dev, "error: SST FW reload failed\n");
			sof_dsp_dma_put_channel(dsp);
			return -ENOMEM;
		}
	}
	ret = sof_block_alloc_scratch(hsw->dsp);
	if (ret < 0)
		return -EINVAL;

	sof_dsp_dma_put_channel(dsp);
	return 0;
}

static int sof_hsw_dsp_restore(struct sof_hsw *hsw)
{
	struct sof_dsp *dsp = hsw->dsp;
	int ret = 0;

	dev_dbg(hsw->dev, "restoring audio DSP....");

	ret = sof_dsp_dma_get_channel(dsp, 0);
	if (ret < 0) {
		dev_err(hsw->dev, "error: cant allocate dma channel %d\n", ret);
		return ret;
	}

	ret = sof_hsw_dx_state_restore(hsw);
	if (ret < 0) {
		dev_err(hsw->dev, "error: SST FW context restore failed\n");
		sof_dsp_dma_put_channel(dsp);
		return -ENOMEM;
	}
	sof_dsp_dma_put_channel(dsp);

	/* wait for DSP boot completion */
	sof_dsp_boot(dsp);

	return ret;
}

int sof_hsw_dsp_runtime_suspend(struct sof_hsw *hsw)
{
	int ret;

	dev_dbg(hsw->dev, "audio dsp runtime suspend\n");

	ret = sof_hsw_dx_set_state(hsw, SST_HSW_DX_STATE_D3, &hsw->dx);
	if (ret < 0)
		return ret;

	sof_dsp_stall(hsw->dsp);

	ret = sof_hsw_dx_state_dump(hsw);
	if (ret < 0)
		return ret;

	sof_ipc_drop_all(&hsw->ipc);

	return 0;
}

int sof_hsw_dsp_runtime_sleep(struct sof_hsw *hsw)
{
	struct sof_fw *sof_fw, *t;
	struct sof_dsp *dsp = hsw->dsp;

	list_for_each_entry_safe(sof_fw, t, &dsp->fw_list, list) {
		sof_fw_unload(sof_fw);
	}
	sof_block_free_scratch(dsp);

	hsw->boot_complete = false;

	sof_dsp_sleep(dsp);

	return 0;
}

int sof_hsw_dsp_runtime_resume(struct sof_hsw *hsw)
{
	struct device *dev = hsw->dev;
	int ret;

	dev_dbg(dev, "audio dsp runtime resume\n");

	if (hsw->boot_complete)
		return 1; /* tell caller no action is required */

	ret = sof_hsw_dsp_restore(hsw);
	if (ret < 0)
		dev_err(dev, "error: audio DSP boot failure\n");

	sof_hsw_init_module_state(hsw);

	ret = wait_event_timeout(hsw->boot_wait, hsw->boot_complete,
		msecs_to_jiffies(IPC_BOOT_MSECS));
	if (ret == 0) {
		dev_err(hsw->dev, "error: audio DSP boot timeout IPCD 0x%x IPCX 0x%x\n",
			sof_dsp_shim_read_unlocked(hsw->dsp, SST_IPCD),
			sof_dsp_shim_read_unlocked(hsw->dsp, SST_IPCX));
		return -EIO;
	}

	/* Set ADSP SSP port settings - sadly the FW does not store SSP port
	   settings as part of the PM context. */
	ret = sof_hsw_device_set_config(hsw, hsw->dx_dev, hsw->dx_mclk,
					hsw->dx_mode, hsw->dx_clock_divider);
	if (ret < 0)
		dev_err(dev, "error: SSP re-initialization failed\n");

	return ret;
}
#endif
