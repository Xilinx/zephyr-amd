/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Xilinx IOModule UART - Child Driver
 *
 * This driver implements the UART peripheral within the IOModule MFD.
 * The UART is UartLite-compatible for basic TX/RX operations but differs
 * in interrupt management:
 *
 * Register offsets (from IOModule base):
 *   0x00: RX Data FIFO (read)
 *   0x04: TX Data FIFO (write)
 *   0x08: Status Register (read-only)
 *
 * Interrupt control uses IOModule IER register (offset 0x38):
 *   Bit 0: UART Error interrupt
 *   Bit 1: UART Receive interrupt
 *   Bit 2: UART Transmit interrupt
 *
 * Unlike standalone UartLite which has its own CTRL_REG for interrupt
 * enable, the IOModule UART shares the IOModule INTC IER/IAR registers.
 */

#define DT_DRV_COMPAT xlnx_iomodule_uart

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/mfd/xlnx_iomodule.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(xlnx_iomodule_uart, CONFIG_UART_LOG_LEVEL);

/* UART Register Offsets (from IOModule base) */
#define UART_RX_OFFSET     IOMODULE_REG_UART_RX    /* 0x00 */
#define UART_TX_OFFSET     IOMODULE_REG_UART_TX    /* 0x04 */
#define UART_STAT_OFFSET   IOMODULE_REG_UART_STAT  /* 0x08 */

/* Status Register bit definitions (same as UartLite) */
#define STAT_RX_FIFO_VALID_DATA BIT(0)
#define STAT_TX_FIFO_FULL       BIT(3)
#define STAT_OVERRUN_ERROR      BIT(5)
#define STAT_FRAME_ERROR        BIT(6)
#define STAT_PARITY_ERROR       BIT(7)
#define STAT_ERROR_MASK         GENMASK(7, 5)

/* IOModule UART interrupt bits in IER/IAR */
#define UART_IRQ_ERROR BIT(IOMODULE_IRQ_BIT_UART_ERROR)  /* Bit 0 */
#define UART_IRQ_TX    BIT(IOMODULE_IRQ_BIT_UART_TX)     /* Bit 1 */
#define UART_IRQ_RX    BIT(IOMODULE_IRQ_BIT_UART_RX)     /* Bit 2 */
#define UART_IRQ_ALL   (UART_IRQ_ERROR | UART_IRQ_RX | UART_IRQ_TX)

struct iomodule_uart_config {
	const struct device *parent;
};

struct iomodule_uart_data {
	mm_reg_t base;
	uint32_t errors;

	struct k_spinlock rx_lock;
	struct k_spinlock tx_lock;

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	const struct device *dev;
	struct k_timer timer;
	uart_irq_callback_user_data_t callback;
	void *callback_data;

	volatile uint8_t tx_irq_enabled : 1;
	volatile uint8_t rx_irq_enabled : 1;
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

static inline uint32_t iomodule_uart_read_reg(struct iomodule_uart_data *data,
					      uint32_t offset)
{
	return sys_read32(data->base + offset);
}

static inline void iomodule_uart_write_reg(struct iomodule_uart_data *data,
					   uint32_t offset, uint32_t value)
{
	sys_write32(value, data->base + offset);
}

static inline uint32_t iomodule_uart_read_status(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status;

	/* Cache errors as they may be cleared by reading STAT_REG */
	status = iomodule_uart_read_reg(data, UART_STAT_OFFSET);
	data->errors |= (status & STAT_ERROR_MASK);

	return status | data->errors;
}

static inline void iomodule_uart_clear_status(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;

	data->errors = 0;
}

static int iomodule_uart_poll_in(const struct device *dev, unsigned char *c)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status;
	k_spinlock_key_t key;
	int ret = -1;

	key = k_spin_lock(&data->rx_lock);
	status = iomodule_uart_read_status(dev);
	if ((status & STAT_RX_FIFO_VALID_DATA) != 0) {
		*c = (unsigned char)(iomodule_uart_read_reg(data, UART_RX_OFFSET) & 0xFF);
		ret = 0;
	}
	k_spin_unlock(&data->rx_lock, key);

	return ret;
}

static void iomodule_uart_poll_out(const struct device *dev, unsigned char c)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status;
	k_spinlock_key_t key;
	bool done = false;

	while (!done) {
		key = k_spin_lock(&data->tx_lock);
		status = iomodule_uart_read_status(dev);
		if ((status & STAT_TX_FIFO_FULL) == 0) {
			iomodule_uart_write_reg(data, UART_TX_OFFSET, (uint32_t)c);
			done = true;
		}
		k_spin_unlock(&data->tx_lock, key);
	}
}

static int iomodule_uart_err_check(const struct device *dev)
{
	uint32_t status = iomodule_uart_read_status(dev);
	int err = 0;

	if (status & STAT_OVERRUN_ERROR) {
		err |= UART_ERROR_OVERRUN;
	}
	if (status & STAT_PARITY_ERROR) {
		err |= UART_ERROR_PARITY;
	}
	if (status & STAT_FRAME_ERROR) {
		err |= UART_ERROR_FRAMING;
	}

	iomodule_uart_clear_status(dev);

	return err;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

/**
 * @brief Enable UART interrupts via IOModule IER
 *
 * Uses the MFD parent's shadow register API since IER is write-only.
 */
static void iomodule_uart_irq_enable_bits(const struct device *dev, uint32_t bits)
{
	const struct iomodule_uart_config *config = dev->config;

	xlnx_iomodule_irq_enable(config->parent, bits);
}

static void iomodule_uart_irq_disable_bits(const struct device *dev, uint32_t bits)
{
	const struct iomodule_uart_config *config = dev->config;

	xlnx_iomodule_irq_disable(config->parent, bits);
}

static int iomodule_uart_fifo_fill(const struct device *dev,
				   const uint8_t *tx_data, int len)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status;
	k_spinlock_key_t key;
	int count = 0;

	while (len - count > 0) {
		key = k_spin_lock(&data->tx_lock);
		status = iomodule_uart_read_status(dev);
		if ((status & STAT_TX_FIFO_FULL) == 0) {
			iomodule_uart_write_reg(data, UART_TX_OFFSET, tx_data[count++]);
		} else {
			k_spin_unlock(&data->tx_lock, key);
			break;
		}
		k_spin_unlock(&data->tx_lock, key);
	}

	return count;
}

static int iomodule_uart_fifo_read(const struct device *dev,
				   uint8_t *rx_data, const int len)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status;
	k_spinlock_key_t key;
	int count = 0;

	while ((len - count) > 0) {
		key = k_spin_lock(&data->rx_lock);
		status = iomodule_uart_read_status(dev);
		if ((status & STAT_RX_FIFO_VALID_DATA) != 0) {
			rx_data[count++] = (uint8_t)(iomodule_uart_read_reg(
				data, UART_RX_OFFSET) & 0xFF);
		} else {
			k_spin_unlock(&data->rx_lock, key);
			break;
		}
		k_spin_unlock(&data->rx_lock, key);
	}

	return count;
}

static void iomodule_uart_tx_soft_isr(struct k_timer *timer)
{
	struct iomodule_uart_data *data =
		CONTAINER_OF(timer, struct iomodule_uart_data, timer);

	if (data->callback) {
		data->callback(data->dev, data->callback_data);
	}
}

static void iomodule_uart_irq_tx_enable(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status;

	data->tx_irq_enabled = true;
	iomodule_uart_irq_enable_bits(dev, UART_IRQ_TX);

	status = iomodule_uart_read_status(dev);
	if ((status & STAT_TX_FIFO_FULL) == 0 && data->callback) {
		/*
		 * TX_FIFO_EMPTY event might have already passed.
		 * Generate a soft interrupt to trigger the callback.
		 */
		k_timer_start(&data->timer, K_NO_WAIT, K_NO_WAIT);
	}
}

static void iomodule_uart_irq_tx_disable(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;

	data->tx_irq_enabled = false;
	if (!data->rx_irq_enabled) {
		iomodule_uart_irq_disable_bits(dev, UART_IRQ_ALL);
	} else {
		iomodule_uart_irq_disable_bits(dev, UART_IRQ_TX);
	}
}

static int iomodule_uart_irq_tx_ready(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status = iomodule_uart_read_status(dev);

	return (((status & STAT_TX_FIFO_FULL) == 0) && data->tx_irq_enabled);
}

static int iomodule_uart_irq_tx_complete(const struct device *dev)
{
	uint32_t status = iomodule_uart_read_status(dev);

	return (status & STAT_TX_FIFO_FULL) == 0;
}

static void iomodule_uart_irq_rx_enable(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;

	data->rx_irq_enabled = true;
	iomodule_uart_irq_enable_bits(dev, UART_IRQ_RX);
}

static void iomodule_uart_irq_rx_disable(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;

	data->rx_irq_enabled = false;
	if (!data->tx_irq_enabled) {
		iomodule_uart_irq_disable_bits(dev, UART_IRQ_ALL);
	} else {
		iomodule_uart_irq_disable_bits(dev, UART_IRQ_RX);
	}
}

static int iomodule_uart_irq_rx_ready(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;
	uint32_t status = iomodule_uart_read_status(dev);

	return ((status & STAT_RX_FIFO_VALID_DATA) && data->rx_irq_enabled);
}

static int iomodule_uart_irq_is_pending(const struct device *dev)
{
	return (iomodule_uart_irq_tx_ready(dev) ||
		iomodule_uart_irq_rx_ready(dev));
}

static int iomodule_uart_irq_update(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 1;
}

static void iomodule_uart_irq_callback_set(const struct device *dev,
					   uart_irq_callback_user_data_t cb,
					   void *user_data)
{
	struct iomodule_uart_data *data = dev->data;

	data->callback = cb;
	data->callback_data = user_data;
}

/**
 * @brief UART ISR called by MFD parent when UART interrupt bits are pending
 */
static void iomodule_uart_isr(const struct device *dev)
{
	struct iomodule_uart_data *data = dev->data;

	/* Acknowledge UART interrupts */
	uint32_t pending = sys_read32(data->base + IOMODULE_REG_IPR) & UART_IRQ_ALL;

	if (pending) {
		sys_write32(pending, data->base + IOMODULE_REG_IAR);
	}

	if (data->callback) {
		data->callback(dev, data->callback_data);
	}
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int iomodule_uart_init(const struct device *dev)
{
	const struct iomodule_uart_config *config = dev->config;
	struct iomodule_uart_data *data = dev->data;

	if (!device_is_ready(config->parent)) {
		LOG_ERR("IOModule MFD parent not ready");
		return -ENODEV;
	}

	/* Get shared base address from MFD parent */
	data->base = xlnx_iomodule_get_base_addr(config->parent);

	LOG_DBG("IOModule UART at base 0x%lx", (unsigned long)data->base);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	data->dev = dev;
	k_timer_init(&data->timer, &iomodule_uart_tx_soft_isr, NULL);

	/* Disable all UART interrupts initially */
	iomodule_uart_irq_disable_bits(dev, UART_IRQ_ALL);

	/* Register ISR with MFD parent */
	xlnx_iomodule_set_irq_handler(config->parent, dev,
				      XLNX_IOMODULE_PERIPH_UART,
				      iomodule_uart_isr);
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

	return 0;
}

static DEVICE_API(uart, iomodule_uart_driver_api) = {
	.poll_in = iomodule_uart_poll_in,
	.poll_out = iomodule_uart_poll_out,
	.err_check = iomodule_uart_err_check,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = iomodule_uart_fifo_fill,
	.fifo_read = iomodule_uart_fifo_read,
	.irq_tx_enable = iomodule_uart_irq_tx_enable,
	.irq_tx_disable = iomodule_uart_irq_tx_disable,
	.irq_tx_ready = iomodule_uart_irq_tx_ready,
	.irq_tx_complete = iomodule_uart_irq_tx_complete,
	.irq_rx_enable = iomodule_uart_irq_rx_enable,
	.irq_rx_disable = iomodule_uart_irq_rx_disable,
	.irq_rx_ready = iomodule_uart_irq_rx_ready,
	.irq_is_pending = iomodule_uart_irq_is_pending,
	.irq_update = iomodule_uart_irq_update,
	.irq_callback_set = iomodule_uart_irq_callback_set,
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

#define IOMODULE_UART_INIT(n)                                                   \
	static struct iomodule_uart_data iomodule_uart_data_##n;                \
									        \
	static const struct iomodule_uart_config iomodule_uart_config_##n = {   \
		.parent = DEVICE_DT_GET(DT_INST_PARENT(n)),                    \
	};                                                                      \
									        \
	DEVICE_DT_INST_DEFINE(n,                                                \
			      &iomodule_uart_init,                              \
			      NULL,                                             \
			      &iomodule_uart_data_##n,                          \
			      &iomodule_uart_config_##n,                        \
			      PRE_KERNEL_1,                                     \
			      CONFIG_SERIAL_INIT_PRIORITY,                      \
			      &iomodule_uart_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IOMODULE_UART_INIT)
