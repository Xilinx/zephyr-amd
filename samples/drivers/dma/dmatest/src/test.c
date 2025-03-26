/*
 * Copyright (c) 2025 Advanced Micro Devices.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/cache.h>

LOG_MODULE_REGISTER(dma_test, LOG_LEVEL_DBG);

/* DMA device label (from your device tree) */
#define DMA_DEVICE_LABEL DT_LABEL(DT_NODELABEL(dma_controller))

/* Buffer size for DMA transfer */
#define BUFFER_SIZE	128

/* Source and destination buffers */
static uint8_t src_buffer[CONFIG_DMA_TRANSFER_CHANNEL][BUFFER_SIZE] __aligned(64);
static uint8_t dst_buffer[CONFIG_DMA_TRANSFER_CHANNEL][BUFFER_SIZE] __aligned(64);

/* DMA completion callback */
static void dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	if (status >= 0) {
		LOG_INF("DMA transfer completed on channel %u", channel);
	} else {
		LOG_ERR("DMA transfer failed on channel %u (status: %d)", channel, status);
	}
}

int main(void)
{
	const struct device *dma_dev[CONFIG_DMA_TRANSFER_CHANNEL];
	struct dma_config dma_cfg[CONFIG_DMA_TRANSFER_CHANNEL] = {0};
	struct dma_block_config dma_block_cfg[CONFIG_DMA_TRANSFER_CHANNEL] = {0};
	int ret;

	for (int i = 0; i < CONFIG_DMA_TRANSFER_CHANNEL; i++) {
		for (int j = 0; j < BUFFER_SIZE; j++) {
			src_buffer[i][j] = i + j;
		}
		memset(dst_buffer, 0, BUFFER_SIZE); /* Clear destination buffer */
	}

	dma_dev[0] = DEVICE_DT_GET(DT_NODELABEL(adma0));
	dma_dev[1] = DEVICE_DT_GET(DT_NODELABEL(adma1));
	dma_dev[2] = DEVICE_DT_GET(DT_NODELABEL(adma2));
	dma_dev[3] = DEVICE_DT_GET(DT_NODELABEL(adma3));
	dma_dev[4] = DEVICE_DT_GET(DT_NODELABEL(adma4));
	dma_dev[5] = DEVICE_DT_GET(DT_NODELABEL(adma5));
	dma_dev[6] = DEVICE_DT_GET(DT_NODELABEL(adma6));
	dma_dev[7] = DEVICE_DT_GET(DT_NODELABEL(adma7));

	/* Check if DMA device is ready */
	for (int i = 0; i < CONFIG_DMA_TRANSFER_CHANNEL; i++) {
		if (!device_is_ready(dma_dev[i])) {
			LOG_ERR("DMA device is not ready");
			continue;
		}

		/* Configure DMA channel */
		dma_cfg[i].channel_direction = MEMORY_TO_MEMORY; /* Simple mode */
		dma_cfg[i].source_data_size = 1; /* 1 byte per transfer */
		dma_cfg[i].dest_data_size = 1; /* 1 byte per transfer */
		dma_cfg[i].source_burst_length = 1; /* Single transfer per burst */
		dma_cfg[i].dest_burst_length = 1; /* Single transfer per burst */
		dma_cfg[i].dma_callback = dma_callback; /* Set completion callback */
		dma_cfg[i].user_data = NULL; /* No user data */

		/* Configure DMA block */
		dma_block_cfg[i].block_size = BUFFER_SIZE; /* Transfer size */
		dma_block_cfg[i].source_address = (uint32_t)src_buffer[i]; /* Source buffer */
		dma_block_cfg[i].dest_address = (uint32_t)dst_buffer[i]; /* Destination buffer */
		dma_block_cfg[i].source_gather_en = 0;
		dma_block_cfg[i].dest_scatter_en = 0;
		dma_cfg[i].head_block = &dma_block_cfg[i];

		/* Configure the DMA channel */
		ret = dma_config(dma_dev[i], 0, &dma_cfg[i]);
		if (ret != 0) {
			LOG_ERR("Failed to configure DMA channel (error: %d)", ret);
			return -EINVAL;
		}

		/* Start the DMA transfer */
		ret = dma_start(dma_dev[i], 0);
		if (ret != 0) {
			LOG_ERR("Failed to start DMA transfer (error: %d)", ret);
			return -EINVAL;
		}
	}

	/* Verify the transfer */
	for (int i = 0; i < CONFIG_DMA_TRANSFER_CHANNEL; i++) {
		if (memcmp(src_buffer[i], dst_buffer[i], BUFFER_SIZE) == 0) {
			LOG_INF("DMA transfer verified successfully on channel %d", i);
		} else {
			LOG_ERR("DMA transfer verification failed on channel %d", i);
			return -EINVAL;
		}
	}

	LOG_INF("DMA transfer verified all on channels\n");
	return 0;
}
