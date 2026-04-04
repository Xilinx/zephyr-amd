/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Xilinx IOModule Programmable Interval Timer (PIT) - Child Driver
 *
 * This driver implements the system timer using PIT peripherals within
 * the IOModule MFD. The IOModule has up to 4 PITs, each with:
 *   - Preload Register: Initial/reload value for the counter
 *   - Counter Register: Current counter value (down-counter)
 *   - Control Register: Enable, interrupt enable, auto-reload
 *
 * PIT Register offsets (from IOModule base, n = 0..3):
 *   PIT_PRELOAD(n): 0x40 + n*0x10
 *   PIT_COUNTER(n): 0x44 + n*0x10
 *   PIT_CONTROL(n): 0x48 + n*0x10
 *
 * PIT interrupt bits in IOModule IER:
 *   Bit 3: PIT 0  |  Bit 4: PIT 1  |  Bit 5: PIT 2  |  Bit 6: PIT 3
 *
 * PITs can be cascaded: PIT n can use PIT (n-1) output as its prescaler,
 * enabling longer intervals than a single counter can provide.
 */

#include <zephyr/drivers/timer/system_timer.h>
#include <zephyr/drivers/mfd/xlnx_iomodule.h>

LOG_MODULE_REGISTER(xlnx_iomodule_pit, CONFIG_XLNX_IOMODULE_PIT_LOG_LEVEL);

#define DT_DRV_COMPAT xlnx_iomodule_pit

/* PIT register offsets relative to IOModule base */
#define PIT_PRELOAD_OFFSET(n)  IOMODULE_REG_PIT_PRELOAD(n)  /* 0x40 + n*0x10 */
#define PIT_COUNTER_OFFSET(n)  IOMODULE_REG_PIT_COUNTER(n)  /* 0x44 + n*0x10 */
#define PIT_CONTROL_OFFSET(n)  IOMODULE_REG_PIT_CONTROL(n)  /* 0x48 + n*0x10 */

/* PIT Control Register bits */
#define PIT_CTRL_ENABLE        BIT(0)    /* Enable the PIT counter */
#define PIT_CTRL_AUTO_RELOAD   BIT(1)    /* Auto-reload preload value on expiry */

/* PIT IRQ bit in IOModule IER: Bit (3 + timer_id) */
#define PIT_IRQ_BIT(timer_id)  BIT(IOMODULE_IRQ_BIT_PIT(timer_id))

struct iomodule_pit_config {
	const struct device *parent;
	uint32_t timer_id;
	uint32_t clock_freq;
};

struct iomodule_pit_data {
	mm_reg_t base;
	uint32_t cycles_per_tick;
	uint32_t last_cycles;
};

/* The system timer instance */
static const struct device *sys_timer_dev;

static inline uint32_t pit_read_reg(struct iomodule_pit_data *data, uint32_t offset)
{
	return sys_read32(data->base + offset);
}

static inline void pit_write_reg(struct iomodule_pit_data *data, uint32_t offset,
				 uint32_t value)
{
	sys_write32(value, data->base + offset);
}

static inline uint32_t pit_read_counter(const struct device *dev)
{
	const struct iomodule_pit_config *config = dev->config;
	struct iomodule_pit_data *data = dev->data;

	return pit_read_reg(data, PIT_COUNTER_OFFSET(config->timer_id));
}

static inline void pit_set_preload(const struct device *dev, uint32_t value)
{
	const struct iomodule_pit_config *config = dev->config;
	struct iomodule_pit_data *data = dev->data;

	pit_write_reg(data, PIT_PRELOAD_OFFSET(config->timer_id), value);
}

static inline void pit_set_control(const struct device *dev, uint32_t flags)
{
	const struct iomodule_pit_config *config = dev->config;
	struct iomodule_pit_data *data = dev->data;

	pit_write_reg(data, PIT_CONTROL_OFFSET(config->timer_id), flags);
}

/**
 * @brief PIT interrupt handler called by MFD parent
 *
 * The MFD parent's ISR dispatches here when PIT interrupt bits
 * (bits 3-6) are pending in the IOModule IPR.
 */
static void iomodule_pit_isr(const struct device *dev)
{
	const struct iomodule_pit_config *config = dev->config;
	struct iomodule_pit_data *data = dev->data;
	uint32_t irq_bit = PIT_IRQ_BIT(config->timer_id);

	if (dev == sys_timer_dev) {
		/*
		 * Update last_cycles BEFORE writing IAR to clear the ISR bit.
		 * sys_clock_cycle_get_32() uses the ISR bit to detect "timer
		 * fired but ISR hasn't run yet." If we cleared the ISR bit first,
		 * there would be a window where the bit reads as 0 but last_cycles
		 * is still stale — causing sys_clock_cycle_get_32() to return a
		 * value 1 tick too small. With this ordering, ISR bit = 0 always
		 * implies last_cycles has already been updated.
		 */
		data->last_cycles += data->cycles_per_tick;
	}

	/* Acknowledge the PIT interrupt (clears ISR bit) */
	sys_write32(irq_bit, data->base + IOMODULE_REG_IAR);

	if (dev == sys_timer_dev) {
		sys_clock_announce(1);
	}
}

uint32_t sys_clock_elapsed(void)
{
	return 0;
}

uint32_t sys_clock_cycle_get_32(void)
{
	if (sys_timer_dev == NULL) {
		return 0;
	}

	const struct iomodule_pit_config *config = sys_timer_dev->config;
	struct iomodule_pit_data *data = sys_timer_dev->data;

	/*
	 * PIT is a down-counter. To produce a globally monotonically increasing
	 * cycle count, we combine the accumulated full-tick cycles (last_cycles,
	 * updated in the ISR each tick) with the elapsed cycles in the current
	 * incomplete tick (cycles_per_tick - current_counter).
	 *
	 * IRQ lock prevents the tick ISR from running while we sample both
	 * registers. However, the hardware can still fire the interrupt and
	 * reload the counter while we hold the lock. If the PIT interrupt bit
	 * is pending in ISR, the counter has already reloaded but last_cycles
	 * has not been updated yet — add cycles_per_tick to compensate.
	 */
	unsigned int key = irq_lock();
	uint32_t counter = pit_read_reg(data, PIT_COUNTER_OFFSET(config->timer_id));
	uint32_t isr_val = sys_read32(data->base + IOMODULE_REG_ISR);
	uint32_t cycles = data->last_cycles + (data->cycles_per_tick - counter);

	if (isr_val & PIT_IRQ_BIT(config->timer_id)) {
		cycles += data->cycles_per_tick;
	}

	irq_unlock(key);

	return cycles;
}

static int iomodule_pit_init(const struct device *dev)
{
	const struct iomodule_pit_config *config = dev->config;
	struct iomodule_pit_data *data = dev->data;

	if (!device_is_ready(config->parent)) {
		LOG_ERR("IOModule MFD parent not ready");
		return -ENODEV;
	}

	/* Get shared base address from MFD parent */
	data->base = xlnx_iomodule_get_base_addr(config->parent);
	data->cycles_per_tick = config->clock_freq / CONFIG_SYS_CLOCK_TICKS_PER_SEC;

	LOG_DBG("IOModule PIT%d at base 0x%lx, clock %u Hz, cycles/tick %u",
		config->timer_id, (unsigned long)data->base,
		config->clock_freq, data->cycles_per_tick);

	/* Disable the PIT */
	pit_set_control(dev, 0);

	/* Set preload value for desired tick rate */
	pit_set_preload(dev, data->cycles_per_tick);

	/* Register per-PIT ISR with MFD parent */
	xlnx_iomodule_set_pit_irq_handler(config->parent, dev,
					  config->timer_id,
					  iomodule_pit_isr);

	/*
	 * Enable PIT interrupt in IOModule IER via MFD parent API.
	 * IER is write-only, so we must use the shadow register.
	 */
	xlnx_iomodule_irq_enable(config->parent, PIT_IRQ_BIT(config->timer_id));

	/* Enable the PIT: enable + auto-reload */
	pit_set_control(dev, PIT_CTRL_ENABLE | PIT_CTRL_AUTO_RELOAD);

	/* Register as system timer if this is the first PIT instance */
	if (sys_timer_dev == NULL) {
		sys_timer_dev = dev;
		data->last_cycles = 0;
		LOG_INF("IOModule PIT%d registered as system timer", config->timer_id);
	}

	return 0;
}

#define IOMODULE_PIT_INIT(n)                                                     \
	static struct iomodule_pit_data iomodule_pit_data_##n;                   \
									         \
	static const struct iomodule_pit_config iomodule_pit_config_##n = {      \
		.parent = DEVICE_DT_GET(DT_INST_PARENT(n)),                     \
		.timer_id = DT_INST_PROP(n, xlnx_pit_timer_id),                \
		.clock_freq = DT_INST_PROP(n, xlnx_clock_freq),                \
	};                                                                       \
									         \
	DEVICE_DT_INST_DEFINE(n,                                                 \
			      &iomodule_pit_init,                                \
			      NULL,                                              \
			      &iomodule_pit_data_##n,                            \
			      &iomodule_pit_config_##n,                          \
			      PRE_KERNEL_2,                                      \
			      CONFIG_SYSTEM_CLOCK_INIT_PRIORITY,                 \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(IOMODULE_PIT_INIT)
