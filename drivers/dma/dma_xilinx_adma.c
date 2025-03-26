/*
 * Copyright (c) 2025 Advanced Micro Devices.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/drivers/dma.h>
#include <soc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/cache.h>
#include <zephyr/drivers/clock_control.h>

#include "dma_xilinx_adma.h"

#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_zynqmp_dma_1_0)
#define DT_DRV_COMPAT xlnx_zynqmp_dma_1_0
#elif DT_HAS_COMPAT_STATUS_OKAY(amd_versal2_dma_1_0)
#define DT_DRV_COMPAT amd_versal2_dma_1_0
#endif

LOG_MODULE_REGISTER(dma_xilinx_adma, CONFIG_DMA_LOG_LEVEL);

/* global configuration per DMA device */
struct dma_xilinx_adma_config {
	struct dma_xilinx_adma_registers *reg;
	bool cachecoherent;
	const struct device *main_clock;
	const struct device *apb_clock;
	uint8_t channel_id;
	void (*irq_configure)();
};

struct dma_xilinx_adma_data {
	struct dma_context ctx;
	struct k_spinlock lock;
	dma_callback_t dma_callback;
	void *callback_user_data;
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t block;
	bool device_has_been_reset;
	struct k_event irq_event;
};

static inline void dma_xilinx_adma_write_reg(uint32_t val,
					     volatile uint32_t *reg)
{
	sys_write32(val, (mem_addr_t)(uintptr_t)reg);
}

static inline uint32_t dma_xilinx_adma_read_reg(volatile uint32_t *reg)
{
	return sys_read32((mem_addr_t)(uintptr_t)reg);
}

static void dma_xilinx_adma_isr(const struct device *dev)
{
	const struct dma_xilinx_adma_config *cfg = dev->config;
	struct dma_xilinx_adma_data *data = dev->data;
	uint32_t status = dma_xilinx_adma_read_reg(&cfg->reg->chan_isr);

	if (!data->device_has_been_reset) {
		LOG_ERR("DMA not ready, ignoring the interrupt\r\n");
		return;
	}

	/* Invalidate the data */
	sys_cache_data_invd_range((void *)data->dst_addr, data->block);
	if (status & XILINX_ADMA_DONE) {
		if (data->dma_callback) {
			data->dma_callback(dev, data->callback_user_data,
					cfg->channel_id, status);
		}
		k_event_post(&data->irq_event, XILINX_ADMA_INT_DONE);
	}
	dma_xilinx_adma_write_reg(XILINX_ADMA_IDS_DEFAULT_MASK, &cfg->reg->chan_ids);
}

/* Configure DMA channel */
static int dma_xilinx_adma_configure(const struct device *dev,
				     uint32_t channel, struct dma_config *dma_cfg)
{
	const struct dma_xilinx_adma_config *cfg = dev->config;
	struct dma_xilinx_adma_data *data = dev->data;
	struct dma_block_config *current_block = dma_cfg->head_block;

	k_spinlock_key_t key = k_spin_lock(&data->lock);

	if (!dma_cfg->head_block) {
		return -EINVAL;
	}

	data->src_addr = current_block->source_address;
	data->dst_addr = current_block->dest_address;
	data->block = current_block->block_size;

	if (!data->device_has_been_reset) {
		LOG_INF("Soft-resetting the DMA core!");
		uint32_t val;

		/* Disables all interrupts */
		dma_xilinx_adma_write_reg(XILINX_ADMA_IDS_DEFAULT_MASK, &cfg->reg->chan_ids);
		dma_xilinx_adma_write_reg(XILINX_ADMA_IDS_DEFAULT_MASK, &cfg->reg->chan_isr);

		/* Configurations reset */
		dma_xilinx_adma_write_reg(XILINX_ADMA_RESET_VAL, &cfg->reg->chan_cntrl0);
		dma_xilinx_adma_write_reg(XILINX_ADMA_RESET_VAL1, &cfg->reg->chan_cntrl1);
		dma_xilinx_adma_write_reg(XILINX_ADMA_RESET_VAL2, &cfg->reg->chan_cntrl2);
		dma_xilinx_adma_write_reg(XILINX_ADMA_DATA_ATTR_RST_VAL,
					  &cfg->reg->chan_data_attr);

		if (cfg->cachecoherent) {
			val = XILINX_ADMA_AXCOHRNT;
			val = (val & ~XILINX_ADMA_AXCACHE) |
				(XILINX_ADMA_AXCACHE_VAL << XILINX_ADMA_AXCACHE_OFST);
			dma_xilinx_adma_write_reg(val, &cfg->reg->chan_dscr_attr);
		}

		val = dma_xilinx_adma_read_reg(&cfg->reg->chan_data_attr);
		if (cfg->cachecoherent) {
			val = (val & ~XILINX_ADMA_ARCACHE) |
				(XILINX_ADMA_AXCACHE_VAL << XILINX_ADMA_ARCACHE_OFST);
			val = (val & ~XILINX_ADMA_AWCACHE) |
				(XILINX_ADMA_AXCACHE_VAL << XILINX_ADMA_AWCACHE_OFST);
		}
		dma_xilinx_adma_write_reg(val, &cfg->reg->chan_data_attr);

		/* Clearing the interrupt account rgisters */
		val = dma_xilinx_adma_read_reg(&cfg->reg->chan_irq_src_acct);
		val = dma_xilinx_adma_read_reg(&cfg->reg->chan_irq_dst_acct);

		data->device_has_been_reset = true;
	}

	dma_xilinx_adma_write_reg(XILINX_ADMA_AXI_RD_DST_DSCR, &cfg->reg->chan_cntrl0);
	/* store callback and user data */
	data->dma_callback = dma_cfg->dma_callback;
	data->callback_user_data = dma_cfg->user_data;

	k_spin_unlock(&data->lock, key);
	return 0;
}

static int dma_xilinx_adma_start(const struct device *dev, uint32_t channel)
{
	const struct dma_xilinx_adma_config *cfg = dev->config;
	struct dma_xilinx_adma_data *data = dev->data;
	k_timeout_t timeout;
	uint64_t addr;

	k_spinlock_key_t key = k_spin_lock(&data->lock);

	dma_xilinx_adma_write_reg(((data->src_addr) & XILINX_ADMA_WORD0_LSB_MASK),
				  &cfg->reg->chan_srcdscr_wrd0);
	addr = data->src_addr;
	dma_xilinx_adma_write_reg(((addr >> XILINX_ADMA_WORD1_MSB_SHIFT)
				  & XILINX_ADMA_WORD1_MSB_MASK), &cfg->reg->chan_srcdscr_wrd1);

	dma_xilinx_adma_write_reg(((data->block) & XILINX_ADMA_WORD2_SIZE_MASK),
				  &cfg->reg->chan_srcdscr_wrd2);

	dma_xilinx_adma_write_reg(((data->dst_addr) & XILINX_ADMA_WORD0_LSB_MASK),
				  &cfg->reg->chan_dstdscr_wrd0);
	addr = data->dst_addr;
	dma_xilinx_adma_write_reg(((addr >> XILINX_ADMA_WORD1_MSB_SHIFT)
				  & XILINX_ADMA_WORD1_MSB_MASK), &cfg->reg->chan_dstdscr_wrd1);

	dma_xilinx_adma_write_reg(((data->block) & XILINX_ADMA_WORD2_SIZE_MASK),
				  &cfg->reg->chan_dstdscr_wrd2);

	dma_xilinx_adma_write_reg(XILINX_ADMA_DESC_CTRL_COHRNT, &cfg->reg->chan_srcdscr_wrd3);
	dma_xilinx_adma_write_reg(XILINX_ADMA_DESC_CTRL_COHRNT, &cfg->reg->chan_dstdscr_wrd3);

	/* Flush the source buffer */
	sys_cache_data_flush_range((void *)data->src_addr, data->block);
	/* Enable Interrupts	*/
	dma_xilinx_adma_write_reg(XILINX_ADMA_INT_EN_DEFAULT_MASK, &cfg->reg->chan_ien);
	/* Start DMA */
	dma_xilinx_adma_write_reg(XILINX_ADMA_START, &cfg->reg->chan_cntrl2);

	k_spin_unlock(&data->lock, key);
	timeout = K_MSEC(POLL_TIMEOUT_COUNTER);
	if (!(k_event_wait(&data->irq_event, XILINX_ADMA_INT_DONE, false, timeout))) {
		LOG_ERR("Transfer Failed, Timeout error");
		return -EINVAL;
	}

	return 0;
}

/* DMA API callbacks */
static const struct dma_driver_api dma_xilinx_adma_driver_api = {
	.config = dma_xilinx_adma_configure,
	.start = dma_xilinx_adma_start,
};

static int dma_xilinx_adma_init(const struct device *dev)
{
	const struct dma_xilinx_adma_config *cfg = dev->config;
	struct dma_xilinx_adma_data *data = dev->data;
	int ret;

	ret = clock_control_on(cfg->apb_clock, NULL);
	if (ret < 0) {
		LOG_ERR("Failed to enable APB clock");
		return ret;
	}

	ret = clock_control_on(cfg->main_clock, NULL);
	if (ret < 0) {
		LOG_ERR("Failed to enable main clock");
		return ret;
	}

	k_event_init(&data->irq_event);
	/* Configure IRQs */
	cfg->irq_configure();
	return 0;
}

#define XILINX_ADMA_INIT(n)									\
	static void dma_xilinx_adma##n##_irq_configure(void)					\
	{											\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),				\
			dma_xilinx_adma_isr, DEVICE_DT_INST_GET(n), DT_INST_IRQ(n, flags));	\
		irq_enable(DT_INST_IRQN(n));							\
	}											\
	static const struct dma_xilinx_adma_config dma_xilinx_adma##n##_config = {		\
		.reg = (void *)(uintptr_t)DT_INST_REG_ADDR(n),					\
		.cachecoherent = DT_INST_PROP_OR(n, cache_coherent, 0),				\
		.main_clock = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR_BY_IDX(n, 0)),			\
		.apb_clock = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR_BY_IDX(n, 1)),			\
		.channel_id = n, /*Assign channel ID based on instance number*/			\
		.irq_configure = dma_xilinx_adma##n##_irq_configure,				\
	};											\
	static struct dma_xilinx_adma_data dma_xilinx_adma##n##_data = {			\
		.ctx = {.magic = DMA_MAGIC, .atomic = NULL},					\
	};											\
	DEVICE_DT_INST_DEFINE(n, &dma_xilinx_adma_init, NULL,					\
			&dma_xilinx_adma##n##_data,						\
			&dma_xilinx_adma##n##_config, POST_KERNEL,				\
			CONFIG_DMA_INIT_PRIORITY, &dma_xilinx_adma_driver_api);

DT_INST_FOREACH_STATUS_OKAY(XILINX_ADMA_INIT)
