/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Xilinx IOModule Multi-Function Device (MFD) Parent Driver
 *
 * The IOModule is a composite peripheral containing:
 * - Interrupt Controller (INTC) - handles internal + external IRQs
 * - UART (UartLite compatible)
 * - 4x Programmable Interval Timers (PIT)
 * - 4x Fixed Interval Timers (FIT)
 * - 4x General Purpose Input (GPI)
 * - 4x General Purpose Output (GPO)
 *
 * This MFD driver manages the shared base address and IRQ line,
 * dispatching interrupts to registered child drivers.
 *
 * IOModule IRQ bit assignments:
 *   Bit 0:     UART Error
 *   Bit 1:     UART Transmit
 *   Bit 2:     UART Receive
 *   Bit 3-6:   PIT 1-4 (Programmable Interval Timer)
 *   Bit 7-10:  FIT 1-4 (Fixed Interval Timer)
 *   Bit 11-14: GPI 1-4 (General Purpose Input)
 *   Bit 16-31: External Interrupts (directly from INTC ports)
 */

#define DT_DRV_COMPAT xlnx_iomodule

#include <zephyr/kernel.h>
#include <zephyr/drivers/mfd/xlnx_iomodule.h>

LOG_MODULE_REGISTER(mfd_xlnx_iomodule, CONFIG_MFD_LOG_LEVEL);

/* IOModule Register Offsets */
#define IOMODULE_ISR_OFFSET     0x30    /* Interrupt Status Register */
#define IOMODULE_IPR_OFFSET     0x34    /* Interrupt Pending Register */
#define IOMODULE_IER_OFFSET     0x38    /* Interrupt Enable Register */
#define IOMODULE_IAR_OFFSET     0x3C    /* Interrupt Acknowledge Register */

/* IRQ Bit Masks */
#define IOMODULE_IRQ_UART_ERROR    BIT(0)
#define IOMODULE_IRQ_UART_RX       BIT(1)
#define IOMODULE_IRQ_UART_TX       BIT(2)
#define IOMODULE_IRQ_UART_MASK     (IOMODULE_IRQ_UART_ERROR | \
				    IOMODULE_IRQ_UART_RX | \
				    IOMODULE_IRQ_UART_TX)
#define IOMODULE_IRQ_PIT_MASK      GENMASK(6, 3)   /* PIT 1-4 */
#define IOMODULE_IRQ_FIT_MASK      GENMASK(10, 7)  /* FIT 1-4 */
#define IOMODULE_IRQ_GPI_MASK      GENMASK(14, 11) /* GPI 1-4 */
#define IOMODULE_IRQ_EXTERNAL_MASK GENMASK(31, 16) /* External IRQs */

#define IOMODULE_MAX_PITS 4

struct xlnx_iomodule_child {
	const struct device *dev;
	xlnx_iomodule_irq_handler_t handler;
};

struct xlnx_iomodule_data {
	struct xlnx_iomodule_child children[XLNX_IOMODULE_PERIPH_COUNT];
	struct xlnx_iomodule_child pit_children[IOMODULE_MAX_PITS];
	/*
	 * Software shadow of the IER register.
	 * The IOModule IER is write-only (reads return 0), so we must
	 * track the enabled interrupt mask in software.
	 */
	uint32_t ier_shadow;
	struct k_spinlock ier_lock;
};

struct xlnx_iomodule_config {
	mm_reg_t base;
	void (*irq_config_func)(const struct device *dev);
};

static inline uint32_t iomodule_read32(const struct xlnx_iomodule_config *config,
				       uint32_t offset)
{
	return sys_read32(config->base + offset);
}

static inline void iomodule_write32(const struct xlnx_iomodule_config *config,
				    uint32_t offset, uint32_t value)
{
	sys_write32(value, config->base + offset);
}

/**
 * @brief IOModule top-level ISR
 *
 * This ISR is connected to the single IRQ line from the IOModule.
 * It reads the Interrupt Status Register to determine which
 * peripheral(s) have pending interrupts and dispatches to the
 * registered child handlers.
 */
static void xlnx_iomodule_isr(const struct device *dev)
{
	const struct xlnx_iomodule_config *config = dev->config;
	struct xlnx_iomodule_data *data = dev->data;
	uint32_t pending;
	struct xlnx_iomodule_child *child;

	/* Read pending interrupts */
	pending = iomodule_read32(config, IOMODULE_IPR_OFFSET);

	if (pending == 0) {
		return;
	}

	/* Dispatch UART interrupts */
	if ((pending & IOMODULE_IRQ_UART_MASK) != 0) {
		child = &data->children[XLNX_IOMODULE_PERIPH_UART];
		if (child->handler != NULL) {
			child->handler(child->dev);
		}
	}

	/* Dispatch PIT interrupts (per-timer) */
	if ((pending & IOMODULE_IRQ_PIT_MASK) != 0) {
		for (int i = 0; i < IOMODULE_MAX_PITS; i++) {
			if ((pending & BIT(3 + i)) != 0 &&
			    data->pit_children[i].handler != NULL) {
				data->pit_children[i].handler(
					data->pit_children[i].dev);
			}
		}
	}

	/* Dispatch FIT interrupts */
	if ((pending & IOMODULE_IRQ_FIT_MASK) != 0) {
		child = &data->children[XLNX_IOMODULE_PERIPH_FIT];
		if (child->handler != NULL) {
			child->handler(child->dev);
		}
	}

	/* Dispatch GPI interrupts */
	if ((pending & IOMODULE_IRQ_GPI_MASK) != 0) {
		child = &data->children[XLNX_IOMODULE_PERIPH_GPI];
		if (child->handler != NULL) {
			child->handler(child->dev);
		}
	}

	/* Dispatch External Interrupts to INTC child */
	if ((pending & IOMODULE_IRQ_EXTERNAL_MASK) != 0) {
		child = &data->children[XLNX_IOMODULE_PERIPH_INTC];
		if (child->handler != NULL) {
			child->handler(child->dev);
		}
	}

	/*
	 * Note: Individual child drivers are responsible for acknowledging
	 * their specific interrupts by writing to IAR.
	 */
}

void xlnx_iomodule_set_irq_handler(const struct device *dev,
				   const struct device *child_dev,
				   enum xlnx_iomodule_periph periph,
				   xlnx_iomodule_irq_handler_t handler)
{
	struct xlnx_iomodule_data *data = dev->data;
	struct xlnx_iomodule_child *child;

	__ASSERT(periph < XLNX_IOMODULE_PERIPH_COUNT,
		 "Invalid peripheral type %d", periph);

	child = &data->children[periph];
	child->dev = child_dev;
	child->handler = handler;

	LOG_DBG("Registered handler for peripheral %d, child %s",
		periph, child_dev->name);
}

void xlnx_iomodule_set_pit_irq_handler(const struct device *dev,
					const struct device *child_dev,
					uint32_t pit_id,
					xlnx_iomodule_irq_handler_t handler)
{
	struct xlnx_iomodule_data *data = dev->data;

	__ASSERT(pit_id < IOMODULE_MAX_PITS,
		 "Invalid PIT id %d", pit_id);

	data->pit_children[pit_id].dev = child_dev;
	data->pit_children[pit_id].handler = handler;

	LOG_DBG("Registered PIT%d handler, child %s", pit_id, child_dev->name);
}

mm_reg_t xlnx_iomodule_get_base_addr(const struct device *dev)
{
	const struct xlnx_iomodule_config *config = dev->config;

	return config->base;
}

void xlnx_iomodule_irq_enable(const struct device *dev, uint32_t mask)
{
	const struct xlnx_iomodule_config *config = dev->config;
	struct xlnx_iomodule_data *data = dev->data;
	k_spinlock_key_t key;

	key = k_spin_lock(&data->ier_lock);
	data->ier_shadow |= mask;
	iomodule_write32(config, IOMODULE_IER_OFFSET, data->ier_shadow);
	k_spin_unlock(&data->ier_lock, key);
}

void xlnx_iomodule_irq_disable(const struct device *dev, uint32_t mask)
{
	const struct xlnx_iomodule_config *config = dev->config;
	struct xlnx_iomodule_data *data = dev->data;
	k_spinlock_key_t key;

	key = k_spin_lock(&data->ier_lock);
	data->ier_shadow &= ~mask;
	iomodule_write32(config, IOMODULE_IER_OFFSET, data->ier_shadow);
	k_spin_unlock(&data->ier_lock, key);
}

uint32_t xlnx_iomodule_irq_get_enabled(const struct device *dev)
{
	struct xlnx_iomodule_data *data = dev->data;

	return data->ier_shadow;
}

static int xlnx_iomodule_init(const struct device *dev)
{
	const struct xlnx_iomodule_config *config = dev->config;
	struct xlnx_iomodule_data *data = dev->data;

	LOG_DBG("IOModule MFD at 0x%lx", (unsigned long)config->base);

	/* Initialize IER shadow to 0 and clear all interrupts */
	data->ier_shadow = 0;
	iomodule_write32(config, IOMODULE_IER_OFFSET, 0);
	iomodule_write32(config, IOMODULE_IAR_OFFSET, 0xFFFFFFFF);

	/* Connect and enable the IRQ */
	config->irq_config_func(dev);

	return 0;
}

#define XLNX_IOMODULE_INIT(n)                                                   \
									        \
	static void xlnx_iomodule_config_func_##n(const struct device *dev);    \
									        \
	static const struct xlnx_iomodule_config xlnx_iomodule_config_##n = {   \
		.base = DT_INST_REG_ADDR(n),                                    \
		.irq_config_func = xlnx_iomodule_config_func_##n,               \
	};                                                                      \
									        \
	static struct xlnx_iomodule_data xlnx_iomodule_data_##n;                \
									        \
	DEVICE_DT_INST_DEFINE(n,                                                \
			      &xlnx_iomodule_init,                              \
			      NULL,                                             \
			      &xlnx_iomodule_data_##n,                          \
			      &xlnx_iomodule_config_##n,                        \
			      PRE_KERNEL_1,                                     \
			      CONFIG_MFD_XLNX_IOMODULE_INIT_PRIORITY,           \
			      NULL);                                            \
									        \
	static void xlnx_iomodule_config_func_##n(const struct device *dev)     \
	{                                                                       \
		IRQ_CONNECT(DT_INST_IRQN(n),                                    \
			    0,                                                  \
			    xlnx_iomodule_isr,                                  \
			    DEVICE_DT_INST_GET(n),                              \
			    0);                                                 \
		irq_enable(DT_INST_IRQN(n));                                    \
	}

DT_INST_FOREACH_STATUS_OKAY(XLNX_IOMODULE_INIT)
