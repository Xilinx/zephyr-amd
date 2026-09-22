/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Entropy driver for the AMD/Xilinx TRNGPSX True Random Number Generator
 * operating in HRNG (Hybrid Random Number Generator) mode.
 *
 * In HRNG mode the ring-oscillator true-entropy source (TRSS) reseeds an
 * internal DRBG; conditioned 256-bit blocks are read out of the core output
 * register. The hardware is reseeded automatically once the configured seed
 * life is reached. The register sequence mirrors the bare-metal xtrngpsx HRNG
 * flow (instantiate -> reseed -> generate).
 */

#define DT_DRV_COMPAT xlnx_pmcl_trng_11_0

#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/sys_io.h>

#include <string.h>

LOG_MODULE_REGISTER(amd_trngpsx, CONFIG_ENTROPY_LOG_LEVEL);

/* Register offsets */
#define TRNG_STATUS      0x0004U
#define TRNG_CTRL        0x0008U
#define TRNG_CTRL_3      0x0010U
#define TRNG_CORE_OUTPUT 0x00C0U
#define TRNG_RESET       0x00D0U
#define TRNG_OSC_EN      0x00D4U

/* TRNG_STATUS bits */
#define TRNG_STATUS_QCNT_SHIFT  9U
#define TRNG_STATUS_QCNT_MASK   0x00000E00U
#define TRNG_STATUS_CERTF_MASK  0x00000008U
#define TRNG_STATUS_DTF_MASK    0x00000002U
#define TRNG_STATUS_DONE_MASK   0x00000001U

/* TRNG_CTRL bits */
#define TRNG_CTRL_PERSODISABLE_MASK  0x00000400U
#define TRNG_CTRL_SINGLEGENMODE_MASK 0x00000200U
#define TRNG_CTRL_PRNGMODE_MASK      0x00000080U
#define TRNG_CTRL_PRNGSTART_MASK     0x00000020U
#define TRNG_CTRL_PRNGXS_MASK        0x00000008U
#define TRNG_CTRL_TRSSEN_MASK        0x00000004U
#define TRNG_CTRL_PRNGSRST_MASK      0x00000001U

/* TRNG_CTRL_3 bits (DF length) */
#define TRNG_CTRL_3_DLEN_SHIFT 0U
#define TRNG_CTRL_3_DLEN_MASK  0x000000FFU

/* TRNG_RESET bits */
#define TRNG_RESET_VAL_MASK  0x00000001U

/* TRNG_OSC_EN bits */
#define TRNG_OSC_EN_VAL_MASK 0x00000001U

/* Operation parameters */
#define TRNGPSX_BLOCK_BYTES         32U /* 256-bit security strength */
#define TRNGPSX_BURSTS              2U  /* 128-bit bursts per block */
#define TRNGPSX_WORDS_PER_BURST     4U
#define TRNGPSX_QCNT_READY          4U
#define TRNGPSX_RESET_DELAY_US      10U
#define TRNGPSX_RESEED_TIMEOUT_US   150000U
#define TRNGPSX_GENERATE_TIMEOUT_US 150000U

/*
 * ISR path budget: a single burst must arrive within this window or the ISR
 * gives up and returns whatever it has already produced. Kept small so the
 * spinlock (and thus interrupt latency) is bounded to well under a millisecond.
 */
#define TRNGPSX_ISR_BURST_TIMEOUT_US 200U

/*
 * Fixed HRNG parameters for this IP:
 *  - DF length: derivation-function input length in TRNG blocks.
 */
#define TRNGPSX_DF_LENGTH 7U

struct trngpsx_config {
	mm_reg_t base;
};

struct trngpsx_data {
	struct k_mutex mutex;
	struct k_spinlock lock;
	bool thread_active;
};

static inline uint32_t trngpsx_read(const struct device *dev, uint32_t off)
{
	const struct trngpsx_config *cfg = dev->config;

	return sys_read32(cfg->base + off);
}

static inline void trngpsx_write(const struct device *dev, uint32_t off, uint32_t val)
{
	const struct trngpsx_config *cfg = dev->config;

	sys_write32(val, cfg->base + off);
}

static inline void trngpsx_rmw(const struct device *dev, uint32_t off, uint32_t mask,
			       uint32_t val)
{
	uint32_t reg = trngpsx_read(dev, off);

	reg = (reg & ~mask) | (val & mask);
	trngpsx_write(dev, off, reg);
}

static int trngpsx_wait_for_event(const struct device *dev, uint32_t off, uint32_t mask,
				  uint32_t expected, uint32_t timeout_us, bool may_sleep)
{
	bool ready;

	/*
	 * Thread callers yield so other threads can run and so this poll does
	 * not pin the CPU. ISR / PRE_KERNEL paths must busy-wait.
	 */
	if (may_sleep) {
		ready = WAIT_FOR((trngpsx_read(dev, off) & mask) == expected, timeout_us,
				 k_yield());
	} else {
		ready = WAIT_FOR((trngpsx_read(dev, off) & mask) == expected, timeout_us,
				 k_busy_wait(1));
	}

	return ready ? 0 : -ETIMEDOUT;
}

static void trngpsx_set_thread_active(struct trngpsx_data *data, bool active)
{
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	data->thread_active = active;
	k_spin_unlock(&data->lock, key);
}

/* Bring the TRNG core and the PRNG unit out of reset. */
static void trngpsx_core_reset(const struct device *dev)
{
	trngpsx_rmw(dev, TRNG_RESET, TRNG_RESET_VAL_MASK, TRNG_RESET_VAL_MASK);
	k_busy_wait(TRNGPSX_RESET_DELAY_US);
	trngpsx_rmw(dev, TRNG_RESET, TRNG_RESET_VAL_MASK, 0U);

	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PRNGSRST_MASK, TRNG_CTRL_PRNGSRST_MASK);
	k_busy_wait(TRNGPSX_RESET_DELAY_US);
	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PRNGSRST_MASK, 0U);
}

/* Read one 256-bit block (two 128-bit bursts) from the output queue. */
static int trngpsx_read_block(const struct device *dev, uint8_t *block, uint32_t timeout_us,
			      bool may_sleep)
{
	int ret;

	for (uint32_t burst = 0U; burst < TRNGPSX_BURSTS; burst++) {
		ret = trngpsx_wait_for_event(dev, TRNG_STATUS, TRNG_STATUS_QCNT_MASK,
					     TRNGPSX_QCNT_READY << TRNG_STATUS_QCNT_SHIFT,
					     timeout_us, may_sleep);
		if (ret < 0) {
			return ret;
		}

		/* Deterministic test failure -> catastrophic, data is unusable. */
		if ((trngpsx_read(dev, TRNG_STATUS) & TRNG_STATUS_DTF_MASK) != 0U) {
			LOG_ERR("deterministic test failure (DTF)");
			return -EIO;
		}

		for (uint32_t idx = 0U; idx < TRNGPSX_WORDS_PER_BURST; idx++) {
			uint32_t word = trngpsx_read(dev, TRNG_CORE_OUTPUT);
			uint32_t off = (burst * TRNGPSX_WORDS_PER_BURST + idx) * sizeof(uint32_t);

			sys_put_be32(word, &block[off]);
		}
	}

	return 0;
}

static void trngpsx_scrub_block(uint8_t *block)
{
	for (size_t i = 0U; i < TRNGPSX_BLOCK_BYTES; i++) {
		((volatile uint8_t *)block)[i] = 0U;
	}
}

/*
 * HRNG runtime generate path: arm once, drain the requested number of bytes
 * from the hardware queue, then disarm. Mirrors the Linux driver session
 * pattern (PRNGSTART once per request, clear PRNGMODE/PRNGSTART at end).
 */
static int trngpsx_collect_random_data(const struct device *dev, uint8_t *buffer, uint16_t length,
				       uint32_t timeout_us, bool may_sleep)
{
	uint8_t block[TRNGPSX_BLOCK_BYTES];
	int ret = 0;

	/* PRNG generate mode and kick off output (once per read session) */
	trngpsx_rmw(dev, TRNG_CTRL,
		TRNG_CTRL_PRNGMODE_MASK | TRNG_CTRL_SINGLEGENMODE_MASK |
			TRNG_CTRL_PRNGSTART_MASK,
		TRNG_CTRL_PRNGMODE_MASK);
	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PRNGSTART_MASK, TRNG_CTRL_PRNGSTART_MASK);

	while (length > 0U) {
		uint16_t chunk = MIN(length, (uint16_t)sizeof(block));

		ret = trngpsx_read_block(dev, block, timeout_us, may_sleep);
		if (ret < 0) {
			break;
		}

		memcpy(buffer, block, chunk);
		trngpsx_scrub_block(block);
		buffer += chunk;
		length -= chunk;
	}

	/* Stop PRNG generate mode after a read session completes. */
	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PRNGMODE_MASK | TRNG_CTRL_PRNGSTART_MASK, 0U);

	return ret;
}

/* Reseed the DRBG from the true-entropy source (HRNG, blocking). */
static int trngpsx_reseed(const struct device *dev, bool may_sleep)
{
	int ret;

	/* Program derivation-function length. */
	trngpsx_rmw(dev, TRNG_CTRL_3, TRNG_CTRL_3_DLEN_MASK,
		    (uint32_t)TRNGPSX_DF_LENGTH << TRNG_CTRL_3_DLEN_SHIFT);

	/* No personalization string in HRNG mode: disable it, clear PRNG start. */
	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PERSODISABLE_MASK | TRNG_CTRL_PRNGSTART_MASK,
		    TRNG_CTRL_PERSODISABLE_MASK);

	/* Enable ring oscillators as the random seed source. */
	trngpsx_rmw(dev, TRNG_OSC_EN, TRNG_OSC_EN_VAL_MASK, TRNG_OSC_EN_VAL_MASK);

	/* Reseed mode: PRNGMODE=0, TRSSEN=1, external seed disabled. */
	trngpsx_rmw(dev, TRNG_CTRL,
		    TRNG_CTRL_PRNGMODE_MASK | TRNG_CTRL_TRSSEN_MASK | TRNG_CTRL_PRNGXS_MASK,
		    TRNG_CTRL_TRSSEN_MASK);

	/* Kick off the reseed. */
	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PRNGSTART_MASK, TRNG_CTRL_PRNGSTART_MASK);

	ret = trngpsx_wait_for_event(dev, TRNG_STATUS, TRNG_STATUS_DONE_MASK, TRNG_STATUS_DONE_MASK,
				     TRNGPSX_RESEED_TIMEOUT_US, may_sleep);
	if (ret < 0) {
		LOG_ERR("timeout waiting for reseed done");
		return ret;
	}

	/* Catastrophic entropy-source test failure. */
	if ((trngpsx_read(dev, TRNG_STATUS) & TRNG_STATUS_CERTF_MASK) != 0U) {
		LOG_ERR("catastrophic entropy test failure (CERTF)");
		return -EIO;
	}

	/* Clear PRNG start and TRSSEN for the next operation. */
	trngpsx_rmw(dev, TRNG_CTRL, TRNG_CTRL_PRNGSTART_MASK | TRNG_CTRL_TRSSEN_MASK, 0U);

	return 0;
}

static int trngpsx_get_entropy_internal(const struct device *dev, uint8_t *buffer, uint16_t length,
					bool may_sleep)
{
	int ret_collect;
	int ret_reseed;

	ret_collect = trngpsx_collect_random_data(dev, buffer, length, TRNGPSX_GENERATE_TIMEOUT_US,
						  may_sleep);
	if (ret_collect < 0) {
		LOG_ERR("failed to collect random data (%d)", ret_collect);
		for (uint16_t i = 0U; i < length; i++) {
			((volatile uint8_t *)buffer)[i] = 0U;
		}
	}

	ret_reseed = trngpsx_reseed(dev, may_sleep);
	if (ret_reseed < 0) {
		LOG_ERR("post-generate reseed failed (%d)", ret_reseed);
	}

	return (ret_collect < 0) ? ret_collect : ret_reseed;
}

static int trngpsx_get_entropy(const struct device *dev, uint8_t *buffer, uint16_t length)
{
	struct trngpsx_data *data = dev->data;
	int ret;

	/*
	 * Mutex serializes full generate+reseed between threads without
	 * masking interrupts. thread_active (under a short spinlock) makes
	 * the ISR path return -ENODATA instead of touching the core mid-session.
	 */
	k_mutex_lock(&data->mutex, K_FOREVER);
	trngpsx_set_thread_active(data, true);
	ret = trngpsx_get_entropy_internal(dev, buffer, length, true);
	trngpsx_set_thread_active(data, false);
	k_mutex_unlock(&data->mutex);

	return ret;
}

static int trngpsx_get_entropy_isr(const struct device *dev, uint8_t *buffer, uint16_t length,
				   uint32_t flags)
{
	struct trngpsx_data *data = dev->data;
	uint8_t block[TRNGPSX_BLOCK_BYTES];
	k_spinlock_key_t key;
	uint16_t produced = 0U;
	uint32_t timeout_us;
	int ret;

	/*
	 * ISR path must be bounded: never arm/disarm, reseed, or start the
	 * hardware here — only drain bytes already queued. With ENTROPY_BUSYWAIT
	 * we allow a small per-burst wait; otherwise we poll with essentially
	 * no wait. If a thread transaction is in progress, do not interleave.
	 */
	timeout_us = (flags & ENTROPY_BUSYWAIT) ? TRNGPSX_ISR_BURST_TIMEOUT_US : 0U;

	key = k_spin_lock(&data->lock);
	if (data->thread_active) {
		k_spin_unlock(&data->lock, key);
		return -ENODATA;
	}

	while (produced < length) {
		uint16_t chunk = MIN((uint16_t)(length - produced), (uint16_t)sizeof(block));

		ret = trngpsx_read_block(dev, block, timeout_us, false);
		if (ret < 0) {
			break;
		}

		memcpy(&buffer[produced], block, chunk);
		trngpsx_scrub_block(block);
		produced += chunk;
	}

	k_spin_unlock(&data->lock, key);

	return (produced > 0U) ? (int)produced : -ENODATA;
}

static int trngpsx_init(const struct device *dev)
{
	const struct trngpsx_config *cfg = dev->config;
	struct trngpsx_data *data = dev->data;
	int ret;

	k_mutex_init(&data->mutex);

	/* Bring TRNG core and PRNG unit out of reset. */
	trngpsx_core_reset(dev);

	/*
	 * Initial reseed from the true-entropy source to make the DRBG ready.
	 * PRE_KERNEL init cannot sleep, so busy-wait here.
	 */
	trngpsx_set_thread_active(data, true);
	ret = trngpsx_reseed(dev, false);
	trngpsx_set_thread_active(data, false);
	if (ret < 0) {
		LOG_ERR("initial reseed failed (%d)", ret);
		return ret;
	}

	LOG_INF("AMD TRNGPSX (HRNG) initialized at 0x%lx", (unsigned long)cfg->base);

	return 0;
}

static DEVICE_API(entropy, trngpsx_api) = {
	.get_entropy = trngpsx_get_entropy,
	.get_entropy_isr = trngpsx_get_entropy_isr,
};

#define TRNGPSX_INIT(n)                                                                           \
	static const struct trngpsx_config trngpsx_config_##n = {                                 \
		.base = DT_INST_REG_ADDR(n),                                                      \
	};                                                                                        \
	static struct trngpsx_data trngpsx_data_##n;                                              \
	DEVICE_DT_INST_DEFINE(n, trngpsx_init, NULL, &trngpsx_data_##n, &trngpsx_config_##n,      \
			      PRE_KERNEL_2, CONFIG_ENTROPY_INIT_PRIORITY, &trngpsx_api);

DT_INST_FOREACH_STATUS_OKAY(TRNGPSX_INIT)
