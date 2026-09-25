/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Xilinx IOModule GPIO - Child Driver
 *
 * Each devicetree instance represents one GPI or GPO port (1-4) within
 * the IOModule MFD. Ports are fixed-direction:
 *   - GPI: read-only data register, optional edge interrupt (IER bits 11-14)
 *   - GPO: write-only data register, software shadow required
 *
 * Register map (from IOModule base):
 *   GPO1-4: 0x10, 0x14, 0x18, 0x1C (write-only)
 *   GPI1-4: 0x20, 0x24, 0x28, 0x2C (read-only)
 */

#define DT_DRV_COMPAT xlnx_iomodule_gpio

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/drivers/mfd/xlnx_iomodule.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gpio_xlnx_iomodule, CONFIG_GPIO_LOG_LEVEL);

/* Fixed GPI edge-interrupt capability values (xlnx,gpio-interrupt-type) */
#define IOMODULE_GPI_EDGE_NONE    0
#define IOMODULE_GPI_EDGE_BOTH    1
#define IOMODULE_GPI_EDGE_RISING  2
#define IOMODULE_GPI_EDGE_FALLING 3

/**
 * @brief Per-instance IOModule GPI/GPO port configuration.
 */
struct iomodule_gpio_config {
	struct gpio_driver_config common;
	const struct device *parent;
	uint32_t out_init;
	uint8_t port;
	bool is_input;
	uint8_t edge_type;
};

/**
 * @brief Per-instance IOModule GPI/GPO port runtime state.
 *
 * The IOModule provides one interrupt status bit for each GPI port, not
 * one status bit for each pin. For GPI ports configured for both-edge
 * detection, previous_gpi_data is compared with the current GPI value to
 * infer which pins changed and the direction of the observed transition.
 *
 * If a pin changes and returns to its previous value before the GPI
 * register is sampled, the transition cannot be attributed to a specific
 * pin because the hardware does not provide per-pin interrupt status.
 */
struct iomodule_gpio_data {
	struct gpio_driver_data common;
	mm_reg_t base;
	uint32_t out_shadow;
	uint32_t int_pins;
	uint32_t rising_int_pins;
	uint32_t falling_int_pins;
	uint32_t previous_gpi_data;
	sys_slist_t callbacks;
};

/**
 * @brief Check whether this port supports software per-pin edge handling.
 *
 * Software per-pin edge classification requires the hardware GPI port to
 * generate interrupts on both rising and falling edges. Ports configured
 * as rising-only or falling-only cannot maintain a complete software
 * shadow because opposite transitions do not generate interrupts.
 *
 * @param config Port configuration.
 * @return true for an input port configured for both-edge detection.
 */
static inline bool
iomodule_gpio_interrupt_capable(const struct iomodule_gpio_config *config)
{
	return config->is_input &&
	       config->edge_type == IOMODULE_GPI_EDGE_BOTH;
}

/**
 * @brief Get the valid pin bitmask for this port.
 *
 * @param dev Port device instance.
 * @return Bitmask of valid pins derived from ngpios.
 */
static inline uint32_t iomodule_gpio_pin_mask(const struct device *dev)
{
	const struct iomodule_gpio_config *config = dev->config;

	return config->common.port_pin_mask;
}

/**
 * @brief Get this port's data register offset.
 *
 * @param config Port configuration.
 * @return Register offset from the IOModule base address.
 */
static inline uint32_t
iomodule_gpio_reg_offset(const struct iomodule_gpio_config *config)
{
	if (config->is_input) {
		return IOMODULE_REG_GPI(config->port);
	}

	return IOMODULE_REG_GPO(config->port);
}

/**
 * @brief Sample the live GPI data register for this port.
 *
 * @param dev Port device instance.
 * @return Masked live value of all valid pins on this GPI port.
 */
static inline uint32_t iomodule_gpio_gpi_sample(const struct device *dev)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t mask = iomodule_gpio_pin_mask(dev);

	return sys_read32(data->base + iomodule_gpio_reg_offset(config)) & mask;
}

/**
 * @brief GPIO driver API: configure a single pin.
 *
 * IOModule ports are fixed-direction in hardware. This function validates
 * the requested flags against the fixed direction and, for GPO pins,
 * applies the requested initial output level.
 *
 * @param dev Port device instance.
 * @param pin Pin index within the port.
 * @param flags Requested GPIO configuration flags.
 * @return 0 on success, -EINVAL for an invalid pin, or -ENOTSUP for an
 *         unsupported flag combination.
 */
static int iomodule_gpio_pin_configure(const struct device *dev,
				       gpio_pin_t pin,
				       gpio_flags_t flags)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t mask = iomodule_gpio_pin_mask(dev);

	/*
	 * The BUILD_ASSERT validates the configured port width. This runtime
	 * check validates the pin number supplied by the API caller.
	 */
	if ((pin >= 32U) || ((mask & BIT(pin)) == 0U)) {
		return -EINVAL;
	}

	if ((flags & (GPIO_INPUT | GPIO_OUTPUT)) == GPIO_DISCONNECTED) {
		return -ENOTSUP;
	}

	if ((flags & (GPIO_INPUT | GPIO_OUTPUT)) ==
	    (GPIO_INPUT | GPIO_OUTPUT)) {
		return -ENOTSUP;
	}

	if ((flags & GPIO_SINGLE_ENDED) != 0U) {
		return -ENOTSUP;
	}

	if ((flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) != 0U) {
		return -ENOTSUP;
	}

	if (config->is_input) {
		if ((flags & GPIO_OUTPUT) != 0U) {
			return -ENOTSUP;
		}
	} else if ((flags & GPIO_INPUT) != 0U) {
		return -ENOTSUP;
	}

	if (!config->is_input &&
	    ((flags & (GPIO_OUTPUT_INIT_LOW |
		       GPIO_OUTPUT_INIT_HIGH)) != 0U)) {
		unsigned int key = irq_lock();
		uint32_t bit = BIT(pin);

		if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0U) {
			data->out_shadow |= bit;
		} else if ((flags & GPIO_OUTPUT_INIT_LOW) != 0U) {
			data->out_shadow &= ~bit;
		}

		sys_write32(data->out_shadow & mask,
			    data->base + iomodule_gpio_reg_offset(config));
		irq_unlock(key);
	}

	return 0;
}

#ifdef CONFIG_GPIO_GET_CONFIG
/**
 * @brief GPIO driver API: read back a pin's current configuration.
 *
 * @param dev Port device instance.
 * @param pin Pin index within the port.
 * @param out_flags Output GPIO flags.
 * @return 0 on success or -EINVAL for an invalid pin.
 */
static int iomodule_gpio_pin_get_config(const struct device *dev,
					gpio_pin_t pin,
					gpio_flags_t *out_flags)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t mask = iomodule_gpio_pin_mask(dev);

	if ((pin >= 32U) || ((mask & BIT(pin)) == 0U)) {
		return -EINVAL;
	}

	*out_flags = config->is_input ? GPIO_INPUT : GPIO_OUTPUT;

	if (!config->is_input &&
	    ((data->out_shadow & BIT(pin)) != 0U)) {
		*out_flags |= GPIO_OUTPUT_INIT_HIGH;
	} else if (!config->is_input) {
		*out_flags |= GPIO_OUTPUT_INIT_LOW;
	}

	return 0;
}
#endif /* CONFIG_GPIO_GET_CONFIG */

/**
 * @brief GPIO driver API: read the raw port value.
 *
 * For GPI ports, this reads the live hardware register. For GPO ports,
 * it returns the software shadow because the GPO registers are write-only.
 *
 * @param dev Port device instance.
 * @param value Output masked port value.
 * @return 0 on success.
 */
static int iomodule_gpio_port_get_raw(const struct device *dev,
				      uint32_t *value)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t mask = iomodule_gpio_pin_mask(dev);

	if (config->is_input) {
		*value = sys_read32(data->base +
				    iomodule_gpio_reg_offset(config)) & mask;
	} else {
		*value = data->out_shadow & mask;
	}

	return 0;
}

/**
 * @brief GPIO driver API: update masked bits of a GPO port.
 *
 * @param dev Port device instance.
 * @param mask Bitmask of pins to update.
 * @param value New values for the masked pins.
 * @return 0 on success or -ENOTSUP for a GPI port.
 */
static int iomodule_gpio_port_set_masked_raw(const struct device *dev,
					     uint32_t mask,
					     uint32_t value)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t pin_mask = iomodule_gpio_pin_mask(dev);
	unsigned int key;

	if (config->is_input) {
		return -ENOTSUP;
	}

	mask &= pin_mask;
	value &= mask;

	key = irq_lock();

	data->out_shadow = (data->out_shadow & ~mask) | value;

	sys_write32(data->out_shadow & pin_mask,
		    data->base + iomodule_gpio_reg_offset(config));

	irq_unlock(key);

	return 0;
}

/**
 * @brief GPIO driver API: set masked GPO bits.
 *
 * @param dev Port device instance.
 * @param mask Bitmask of pins to set.
 * @return 0 on success or -ENOTSUP for a GPI port.
 */
static int iomodule_gpio_port_set_bits_raw(const struct device *dev,
					   uint32_t mask)
{
	return iomodule_gpio_port_set_masked_raw(dev, mask, mask);
}

/**
 * @brief GPIO driver API: clear masked GPO bits.
 *
 * @param dev Port device instance.
 * @param mask Bitmask of pins to clear.
 * @return 0 on success or -ENOTSUP for a GPI port.
 */
static int iomodule_gpio_port_clear_bits_raw(const struct device *dev,
					     uint32_t mask)
{
	return iomodule_gpio_port_set_masked_raw(dev, mask, 0U);
}

/**
 * @brief GPIO driver API: toggle masked GPO bits.
 *
 * @param dev Port device instance.
 * @param pins Bitmask of pins to toggle.
 * @return 0 on success or -ENOTSUP for a GPI port.
 */
static int iomodule_gpio_port_toggle_bits(const struct device *dev,
					  uint32_t pins)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t pin_mask = iomodule_gpio_pin_mask(dev);
	uint32_t mask = pin_mask & pins;
	unsigned int key;

	if (config->is_input) {
		return -ENOTSUP;
	}

	key = irq_lock();

	data->out_shadow ^= mask;

	sys_write32(data->out_shadow & pin_mask,
		    data->base + iomodule_gpio_reg_offset(config));

	irq_unlock(key);

	return 0;
}

/**
 * @brief GPI port interrupt service routine.
 *
 * The IOModule exposes one interrupt status bit for the complete GPI port.
 * The driver compares the current GPI value with the previously sampled
 * value to infer which pins changed and their observed direction.
 *
 * Source identification is best-effort. If a pin changes and returns to
 * its previous value before the GPI register is sampled, the interrupt
 * cannot be attributed to a specific pin.
 *
 * @param dev GPI port device instance.
 */
static void iomodule_gpio_gpi_isr(const struct device *dev)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t irq_bit = BIT(IOMODULE_IRQ_BIT_GPI(config->port - 1));
	uint32_t current_data;
	uint32_t changed_pins;
	uint32_t rising_now;
	uint32_t falling_now;
	uint32_t fire_mask;

	/* Acknowledge this port's GPI interrupt bit only. */
	sys_write32(irq_bit, data->base + IOMODULE_REG_IAR);

	current_data = iomodule_gpio_gpi_sample(dev);
	changed_pins = current_data ^ data->previous_gpi_data;
	data->previous_gpi_data = current_data;

	rising_now = changed_pins & current_data;
	falling_now = changed_pins & ~current_data;

	fire_mask = (rising_now & data->rising_int_pins) |
		    (falling_now & data->falling_int_pins);

	if (fire_mask != 0U) {
		gpio_fire_callbacks(&data->callbacks, dev, fire_mask);
	}
}

/**
 * @brief GPIO driver API: configure or disable a pin interrupt.
 *
 * The IOModule interrupt configuration applies to the complete GPI port.
 * This driver supports software per-pin edge selection only when the port
 * is configured in hardware for both-edge detection.
 *
 * On the first interrupt-enabled pin, any stale GPI interrupt is cleared,
 * the previous GPI value is synchronized, and the hardware port interrupt
 * is enabled.
 *
 * @param dev GPI port device instance.
 * @param pin Pin index within the port.
 * @param mode Interrupt mode.
 * @param trig Requested edge trigger.
 * @return 0 on success, -EINVAL for an invalid pin, or -ENOTSUP for an
 *         unsupported mode, trigger, or hardware configuration.
 */
static int iomodule_gpio_pin_interrupt_configure(const struct device *dev,
						 gpio_pin_t pin,
						 enum gpio_int_mode mode,
						 enum gpio_int_trig trig)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t irq_bit;
	uint32_t pin_bit;
	uint32_t prev_int_pins;
	unsigned int key;

	if (!iomodule_gpio_interrupt_capable(config)) {
		return -ENOTSUP;
	}

	if ((pin >= 32U) || ((iomodule_gpio_pin_mask(dev) & BIT(pin)) == 0U)) {
		return -EINVAL;
	}

	pin_bit = BIT(pin);
	irq_bit = BIT(IOMODULE_IRQ_BIT_GPI(config->port - 1));

	if (mode == GPIO_INT_MODE_DISABLED) {
		key = irq_lock();

		data->int_pins &= ~pin_bit;
		data->rising_int_pins &= ~pin_bit;
		data->falling_int_pins &= ~pin_bit;

		if (data->int_pins == 0U) {
			xlnx_iomodule_irq_disable(config->parent, irq_bit);
		}

		irq_unlock(key);

		return 0;
	}

	if (mode != GPIO_INT_MODE_EDGE) {
		return -ENOTSUP;
	}

	if ((trig != GPIO_INT_TRIG_HIGH) && (trig != GPIO_INT_TRIG_LOW) &&
	    (trig != GPIO_INT_TRIG_BOTH)) {
		return -ENOTSUP;
	}

	key = irq_lock();

	prev_int_pins = data->int_pins;
	data->int_pins |= pin_bit;

	if (trig == GPIO_INT_TRIG_HIGH) {
		data->rising_int_pins |= pin_bit;
		data->falling_int_pins &= ~pin_bit;
	} else if (trig == GPIO_INT_TRIG_LOW) {
		data->falling_int_pins |= pin_bit;
		data->rising_int_pins &= ~pin_bit;
	} else {
		data->rising_int_pins |= pin_bit;
		data->falling_int_pins |= pin_bit;
	}

	if (prev_int_pins == 0U) {
		/*
		 * Clear any stale GPI port interrupt and synchronize the
		 * software sample before enabling the hardware interrupt.
		 */
		sys_write32(irq_bit, data->base + IOMODULE_REG_IAR);
		data->previous_gpi_data = iomodule_gpio_gpi_sample(dev);
		xlnx_iomodule_irq_enable(config->parent, irq_bit);
	}

	irq_unlock(key);

	return 0;
}

/**
 * @brief GPIO driver API: report observable pending GPI pin interrupts.
 *
 * The IOModule exposes a pending interrupt bit for the complete GPI port
 * but does not provide per-pin pending status. This function first checks
 * the hardware port pending bit and then infers candidate pin sources by
 * comparing the current GPI value with the previous value used by the ISR.
 *
 * The query is non-destructive. It does not acknowledge the hardware
 * interrupt and does not update previous_gpi_data, so a subsequent ISR
 * continues to use the original comparison baseline.
 *
 * If a signal changes and returns to its previous state before GPIx is
 * sampled, the originating pin cannot be identified. In that case this
 * function can return zero even though the GPI port interrupt is pending.
 *
 * @param dev Port device instance.
 * @return Bitmask of enabled pins with an observable pending transition.
 */
static uint32_t iomodule_gpio_get_pending_int(const struct device *dev)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t irq_bit;
	uint32_t current_data;
	uint32_t changed_pins;
	uint32_t rising_now;
	uint32_t falling_now;
	uint32_t fire_mask;
	unsigned int key;

	if (!iomodule_gpio_interrupt_capable(config)) {
		return 0U;
	}

	irq_bit = BIT(IOMODULE_IRQ_BIT_GPI(config->port - 1));

	/*
	 * Protect the pending-bit check and software classification from the
	 * local ISR. Otherwise, the ISR could acknowledge the interrupt or
	 * update previous_gpi_data between these operations.
	 */
	key = irq_lock();

	if ((sys_read32(data->base + IOMODULE_REG_IPR) & irq_bit) == 0U) {
		irq_unlock(key);
		return 0U;
	}

	current_data = iomodule_gpio_gpi_sample(dev);
	changed_pins = current_data ^ data->previous_gpi_data;

	rising_now = changed_pins & current_data;
	falling_now = changed_pins & ~current_data;

	fire_mask = (rising_now & data->rising_int_pins) |
		    (falling_now & data->falling_int_pins);

	irq_unlock(key);

	return fire_mask;
}

/**
 * @brief GPIO driver API: add or remove a callback.
 *
 * @param dev Port device instance.
 * @param callback Callback structure.
 * @param set true to add the callback, false to remove it.
 * @return 0 on success or a negative errno value on failure.
 */
static int iomodule_gpio_manage_callback(const struct device *dev,
					 struct gpio_callback *callback,
					 bool set)
{
	struct iomodule_gpio_data *data = dev->data;

	return gpio_manage_callback(&data->callbacks, callback, set);
}

#ifdef CONFIG_GPIO_GET_DIRECTION
/**
 * @brief GPIO driver API: report pin direction.
 *
 * IOModule ports are fixed-direction, so every valid pin in the requested
 * map has the direction assigned to the complete port.
 *
 * @param dev Port device instance.
 * @param map Bitmask of pins to query.
 * @param inputs Output input pin mask, or NULL.
 * @param outputs Output output pin mask, or NULL.
 * @return 0 on success.
 */
static int iomodule_gpio_port_get_direction(const struct device *dev,
					    gpio_port_pins_t map,
					    gpio_port_pins_t *inputs,
					    gpio_port_pins_t *outputs)
{
	const struct iomodule_gpio_config *config = dev->config;
	gpio_port_pins_t pin_mask = iomodule_gpio_pin_mask(dev) & map;

	if (config->is_input) {
		if (inputs != NULL) {
			*inputs = pin_mask;
		}

		if (outputs != NULL) {
			*outputs = 0U;
		}
	} else {
		if (inputs != NULL) {
			*inputs = 0U;
		}

		if (outputs != NULL) {
			*outputs = pin_mask;
		}
	}

	return 0;
}
#endif /* CONFIG_GPIO_GET_DIRECTION */

static DEVICE_API(gpio, iomodule_gpio_driver_api) = {
	.pin_configure = iomodule_gpio_pin_configure,
#ifdef CONFIG_GPIO_GET_CONFIG
	.pin_get_config = iomodule_gpio_pin_get_config,
#endif
	.port_get_raw = iomodule_gpio_port_get_raw,
	.port_set_masked_raw = iomodule_gpio_port_set_masked_raw,
	.port_set_bits_raw = iomodule_gpio_port_set_bits_raw,
	.port_clear_bits_raw = iomodule_gpio_port_clear_bits_raw,
	.port_toggle_bits = iomodule_gpio_port_toggle_bits,
	.pin_interrupt_configure = iomodule_gpio_pin_interrupt_configure,
	.manage_callback = iomodule_gpio_manage_callback,
	.get_pending_int = iomodule_gpio_get_pending_int,
#ifdef CONFIG_GPIO_GET_DIRECTION
	.port_get_direction = iomodule_gpio_port_get_direction,
#endif
};

/**
 * @brief Register this GPI port's ISR with the IOModule parent.
 *
 * This is a no-op for GPO ports and GPI ports that are not configured for
 * supported both-edge interrupt handling.
 *
 * @param dev Port device instance.
 * @return 0 on success.
 */
static int iomodule_gpio_register_gpi_irq(const struct device *dev)
{
	const struct iomodule_gpio_config *config = dev->config;

	if (!iomodule_gpio_interrupt_capable(config)) {
		return 0;
	}

	xlnx_iomodule_set_gpi_irq_handler(config->parent, dev, config->port - 1,
					  iomodule_gpio_gpi_isr);

	return 0;
}

/**
 * @brief Initialize one IOModule GPI/GPO port instance.
 *
 * @param dev Port device instance.
 * @return 0 on success or -ENODEV if the parent is not ready.
 */
static int iomodule_gpio_init(const struct device *dev)
{
	const struct iomodule_gpio_config *config = dev->config;
	struct iomodule_gpio_data *data = dev->data;
	uint32_t mask = iomodule_gpio_pin_mask(dev);
	int ret;

	if (!device_is_ready(config->parent)) {
		LOG_ERR("IOModule MFD parent not ready");
		return -ENODEV;
	}

	data->base = xlnx_iomodule_get_base_addr(config->parent);

	sys_slist_init(&data->callbacks);

	data->out_shadow = 0U;
	data->int_pins = 0U;
	data->rising_int_pins = 0U;
	data->falling_int_pins = 0U;
	data->previous_gpi_data = 0U;

	if (config->is_input) {
		data->previous_gpi_data = iomodule_gpio_gpi_sample(dev);

		ret = iomodule_gpio_register_gpi_irq(dev);
		if (ret != 0) {
			return ret;
		}
	} else {
		data->out_shadow = config->out_init & mask;

		sys_write32(data->out_shadow,
			    data->base + iomodule_gpio_reg_offset(config));
	}

	LOG_DBG("IOModule GPIO port %u (%s) at base 0x%lx, mask 0x%x",
		config->port, config->is_input ? "input" : "output",
		(unsigned long)data->base, mask);

	return 0;
}

#define IOMODULE_GPIO_INIT(n)                                                   \
	BUILD_ASSERT(DT_INST_PROP(n, xlnx_gpio_port) >= 1 &&                      \
		     DT_INST_PROP(n, xlnx_gpio_port) <= 4,                      \
		     "xlnx,gpio-port must be 1..4");                            \
	BUILD_ASSERT(DT_INST_PROP(n, ngpios) >= 1 &&                              \
		     DT_INST_PROP(n, ngpios) <= 32,                             \
		     "ngpios must be 1..32");                                   \
	static struct iomodule_gpio_data iomodule_gpio_data_##n;                  \
	static const struct iomodule_gpio_config iomodule_gpio_config_##n = {   \
		.common = {                                                     \
			.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_NGPIOS(        \
				DT_INST_PROP(n, ngpios)),                       \
		},                                                              \
		.parent = DEVICE_DT_GET(DT_INST_PARENT(n)),                      \
		.port = DT_INST_PROP(n, xlnx_gpio_port),                        \
		.is_input = DT_INST_ENUM_HAS_VALUE(n, xlnx_gpio_direction, input), \
		.edge_type = DT_INST_PROP_OR(n, xlnx_gpio_interrupt_type,      \
					     IOMODULE_GPI_EDGE_NONE),           \
		.out_init = DT_INST_PROP_OR(n, xlnx_gpio_init, 0),              \
	};                                                                      \
	DEVICE_DT_INST_DEFINE(n, iomodule_gpio_init, NULL,                      \
			      &iomodule_gpio_data_##n,                          \
			      &iomodule_gpio_config_##n,                        \
			      PRE_KERNEL_1,                                     \
			      CONFIG_MFD_XLNX_IOMODULE_CHILD_INIT_PRIORITY,     \
			      &iomodule_gpio_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IOMODULE_GPIO_INIT)
