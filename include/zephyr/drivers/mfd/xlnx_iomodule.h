/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Xilinx IOModule MFD Driver API Header
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_XLNX_IOMODULE_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_XLNX_IOMODULE_H_

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IOModule peripheral types for IRQ handler registration
 */
enum xlnx_iomodule_periph {
	XLNX_IOMODULE_PERIPH_UART = 0,  /**< UART peripheral (IRQ bits 0-2) */
	XLNX_IOMODULE_PERIPH_PIT,       /**< Programmable Interval Timer (IRQ bits 3-6) */
	XLNX_IOMODULE_PERIPH_FIT,       /**< Fixed Interval Timer (IRQ bits 7-10) */
	XLNX_IOMODULE_PERIPH_GPI,       /**< General Purpose Input (IRQ bits 11-14) */
	XLNX_IOMODULE_PERIPH_INTC,      /**< Interrupt Controller for external IRQs (bits 16-31) */
	XLNX_IOMODULE_PERIPH_COUNT
};

/**
 * @brief IOModule child ISR handler type
 */
typedef void (*xlnx_iomodule_irq_handler_t)(const struct device *dev);

/**
 * @brief Register an IRQ handler for an IOModule peripheral
 *
 * Child drivers call this function to register their ISR with the
 * parent MFD driver. When an interrupt occurs, the parent driver
 * reads the interrupt status register and dispatches to the
 * appropriate child handler.
 *
 * @param dev       IOModule MFD parent device
 * @param child_dev Child device to pass to handler
 * @param periph    Peripheral type (UART, PIT, FIT, GPI, or INTC)
 * @param handler   ISR function to call when peripheral has interrupt
 */
void xlnx_iomodule_set_irq_handler(const struct device *dev,
				   const struct device *child_dev,
				   enum xlnx_iomodule_periph periph,
				   xlnx_iomodule_irq_handler_t handler);

/**
 * @brief Register an IRQ handler for a specific PIT instance
 *
 * Unlike other peripherals that share a single handler slot per type,
 * each of the 4 PITs has its own handler slot. The MFD parent checks
 * individual PIT interrupt bits and dispatches to the correct handler.
 *
 * @param dev       IOModule MFD parent device
 * @param child_dev PIT child device to pass to handler
 * @param pit_id    PIT instance number (0-3)
 * @param handler   ISR function to call when this PIT fires
 */
void xlnx_iomodule_set_pit_irq_handler(const struct device *dev,
					const struct device *child_dev,
					uint32_t pit_id,
					xlnx_iomodule_irq_handler_t handler);

/**
 * @brief Get the base address of the IOModule
 *
 * Child drivers use this to access their specific register offsets
 * within the IOModule address space.
 *
 * @param dev IOModule MFD parent device
 * @return Base address of the IOModule
 */
mm_reg_t xlnx_iomodule_get_base_addr(const struct device *dev);

/**
 * @brief Enable interrupt bits in the IOModule IER
 *
 * The IOModule IER register is write-only (reads return 0), so the MFD
 * parent maintains a software shadow. Children must use this API instead
 * of directly writing to IER.
 *
 * @param dev  IOModule MFD parent device
 * @param mask Bitmask of interrupt bits to enable
 */
void xlnx_iomodule_irq_enable(const struct device *dev, uint32_t mask);

/**
 * @brief Disable interrupt bits in the IOModule IER
 *
 * @param dev  IOModule MFD parent device
 * @param mask Bitmask of interrupt bits to disable
 */
void xlnx_iomodule_irq_disable(const struct device *dev, uint32_t mask);

/**
 * @brief Get the current IER shadow value
 *
 * @param dev IOModule MFD parent device
 * @return Current software shadow of the IER register
 */
uint32_t xlnx_iomodule_irq_get_enabled(const struct device *dev);

/*
 * IOModule Register Offsets - shared by all child drivers
 *
 * Interrupt Controller Registers:
 *   ISR (0x30): Interrupt Status Register - shows active interrupts
 *   IPR (0x34): Interrupt Pending Register - shows enabled & active
 *   IER (0x38): Interrupt Enable Register - enable/disable interrupts
 *   IAR (0x3C): Interrupt Acknowledge Register - write to clear
 *
 * UART Registers:
 *   UART_RX (0x00): Receive Data FIFO
 *   UART_TX (0x04): Transmit Data FIFO
 *   UART_STATUS (0x08): Status Register
 *   UART_BAUD (0x4C): Baud Rate Divider (when implemented)
 *
 * PIT Registers (n = 0..3):
 *   PIT_PRELOAD(n) (0x40 + n*0x10): Preload value
 *   PIT_COUNTER(n) (0x44 + n*0x10): Current counter value
 *   PIT_CONTROL(n) (0x48 + n*0x10): Control register
 *
 * GPI/GPO Registers (n = 1..4):
 *   GPI(n) (0x10 + (n-1)*4): General Purpose Input n
 *   GPO(n) (0x10 + (n-1)*4): General Purpose Output n
 */
#define IOMODULE_REG_ISR        0x30
#define IOMODULE_REG_IPR        0x34
#define IOMODULE_REG_IER        0x38
#define IOMODULE_REG_IAR        0x3C

#define IOMODULE_REG_UART_RX    0x00
#define IOMODULE_REG_UART_TX    0x04
#define IOMODULE_REG_UART_STAT  0x08
#define IOMODULE_REG_UART_BAUD  0x4C

#define IOMODULE_REG_GPI(n)     (0x10 + ((n) - 1) * 4)  /* n = 1..4 */
#define IOMODULE_REG_GPO(n)     (0x10 + ((n) - 1) * 4)  /* n = 1..4 */

#define IOMODULE_REG_PIT_PRELOAD(n)  (0x40 + (n) * 0x10)  /* n = 0..3 */
#define IOMODULE_REG_PIT_COUNTER(n)  (0x44 + (n) * 0x10)  /* n = 0..3 */
#define IOMODULE_REG_PIT_CONTROL(n)  (0x48 + (n) * 0x10)  /* n = 0..3 */

/* Interrupt bit positions */
#define IOMODULE_IRQ_BIT_UART_ERROR   0
#define IOMODULE_IRQ_BIT_UART_TX      1
#define IOMODULE_IRQ_BIT_UART_RX      2
#define IOMODULE_IRQ_BIT_PIT(n)       (3 + (n))   /* n = 0..3, bits 3-6 */
#define IOMODULE_IRQ_BIT_FIT(n)       (7 + (n))   /* n = 0..3, bits 7-10 */
#define IOMODULE_IRQ_BIT_GPI(n)       (11 + (n))  /* n = 0..3, bits 11-14 */
#define IOMODULE_IRQ_BIT_EXTERNAL(n)  (16 + (n))  /* n = 0..15, bits 16-31 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_XLNX_IOMODULE_H_ */
