/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Xilinx IOModule Interrupt Controller - Child Driver
 *
 * This driver implements the same xlnx_intc_irq_*() API as the AXI INTC
 * driver (intc_xlnx.c), but operates through the IOModule MFD parent.
 *
 * Key differences from AXI INTC:
 *   - Register offsets: ISR=0x30, IPR=0x34, IER=0x38, IAR=0x3C
 *     (vs AXI INTC: ISR=0x00, IPR=0x04, IER=0x08, IAR=0x0C)
 *   - No MER (Master Enable Register) - interrupts are always enabled
 *   - No SIE/CIE/IVR/IMR/IVAR optional registers
 *   - Shares base address with UART, PIT, FIT, GPI/GPO peripherals
 *
 * Interrupt bit assignments:
 *   Bits 0-2:   Internal UART (Error, RX, TX)
 *   Bits 3-6:   Internal PIT 1-4
 *   Bits 7-10:  Internal FIT 1-4
 *   Bits 11-14: Internal GPI 1-4
 *   Bits 16-31: External Interrupt Inputs
 *
 * The driver registers with the MFD parent to receive external interrupt
 * notifications and dispatches them through _sw_isr_table.
 */

#define DT_DRV_COMPAT xlnx_iomodule_intc

#include <zephyr/drivers/mfd/xlnx_iomodule.h>
#include <zephyr/init.h>
#include <zephyr/sw_isr_table.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(xlnx_iomodule_intc, CONFIG_INTC_LOG_LEVEL);

/*
 * IOModule INTC Register Offsets
 * Note: These are offsets from the IOModule base address (shared with
 * other peripherals), NOT from a dedicated INTC base address.
 */
#define XIN_ISR_OFFSET  IOMODULE_REG_ISR   /* 0x30 - Interrupt Status */
#define XIN_IPR_OFFSET  IOMODULE_REG_IPR   /* 0x34 - Interrupt Pending */
#define XIN_IER_OFFSET  IOMODULE_REG_IER   /* 0x38 - Interrupt Enable */
#define XIN_IAR_OFFSET  IOMODULE_REG_IAR   /* 0x3C - Interrupt Acknowledge */

/* Static state - single instance like AXI INTC */
static mm_reg_t intc_base_addr;
static const struct device *intc_mfd_parent;
static bool intc_is_ready;

static inline uint32_t iomodule_intc_read(uint32_t offset)
{
	return sys_read32(intc_base_addr + offset);
}

static inline void iomodule_intc_write(uint32_t data, uint32_t offset)
{
	sys_write32(data, intc_base_addr + offset);
}

/*
 * Public API - same interface as intc_xlnx.h
 *
 * These functions are called by the SoC-level arch_irq_enable/disable/etc.
 * and must work for ALL 32 interrupt bits (internal + external).
 */

uint32_t xlnx_intc_irq_get_enabled(void)
{
	/*
	 * IER is write-only, use MFD parent's software shadow.
	 */
	if (!intc_is_ready) {
		return 0;
	}
	return xlnx_iomodule_irq_get_enabled(intc_mfd_parent);
}

uint32_t xlnx_intc_get_status_register(void)
{
	return iomodule_intc_read(XIN_ISR_OFFSET);
}

uint32_t xlnx_intc_irq_pending(void)
{
	return iomodule_intc_read(XIN_IPR_OFFSET);
}

uint32_t xlnx_intc_irq_pending_vector(void)
{
	return find_lsb_set(xlnx_intc_irq_pending()) - 1;
}

void xlnx_intc_irq_enable(uint32_t irq)
{
	__ASSERT_NO_MSG(irq < 32);

	if (!intc_is_ready) {
		LOG_ERR("IOModule INTC not ready");
		return;
	}

	/*
	 * IER is write-only. Use MFD parent's shadow register API
	 * which maintains a software copy and writes the full mask.
	 */
	xlnx_iomodule_irq_enable(intc_mfd_parent, BIT(irq));
}

void xlnx_intc_irq_disable(uint32_t irq)
{
	__ASSERT_NO_MSG(irq < 32);

	if (!intc_is_ready) {
		return;
	}

	xlnx_iomodule_irq_disable(intc_mfd_parent, BIT(irq));
}

void xlnx_intc_irq_acknowledge(uint32_t mask)
{
	iomodule_intc_write(mask, XIN_IAR_OFFSET);
}

/**
 * @brief IOModule INTC handler for external interrupts
 *
 * This handler is called by the MFD parent when external interrupt bits
 * (16-31) are pending. It dispatches to the appropriate handler in
 * _sw_isr_table.
 *
 * Note: Internal peripheral interrupts (UART, PIT, FIT, GPI) are handled
 * directly by their respective child drivers through the MFD parent.
 */
static void iomodule_intc_ext_irq_handler(const struct device *dev)
{
	uint32_t irq, irq_mask;

	ARG_UNUSED(dev);

	/* Get pending external interrupts */
	irq_mask = xlnx_intc_irq_pending();

	/* Only handle external interrupts (bits 16-31) */
	irq_mask &= GENMASK(31, 16);

	while (irq_mask != 0) {
		irq = find_lsb_set(irq_mask) - 1;

		uint32_t table_idx = irq + CONFIG_2ND_LVL_ISR_TBL_OFFSET;
		const struct _isr_table_entry *ite = &_sw_isr_table[table_idx];

		ite->isr(ite->arg);

		xlnx_intc_irq_acknowledge(BIT(irq));

		irq_mask &= ~BIT(irq);
	}
}

static int iomodule_intc_init(const struct device *dev)
{
	const struct device *parent = DEVICE_DT_GET(DT_INST_PARENT(0));

	if (!device_is_ready(parent)) {
		LOG_ERR("IOModule MFD parent not ready");
		return -ENODEV;
	}

	/* Store parent reference for IER shadow API */
	intc_mfd_parent = parent;

	/* Get the shared base address from the MFD parent */
	intc_base_addr = xlnx_iomodule_get_base_addr(parent);

	LOG_DBG("IOModule INTC using base address 0x%lx",
		(unsigned long)intc_base_addr);

	/*
	 * IER and IAR are already cleared by the MFD parent init.
	 * No need to write them again here.
	 */

	intc_is_ready = true;

	/* Register with MFD parent for external interrupt dispatch */
	xlnx_iomodule_set_irq_handler(parent, dev,
				      XLNX_IOMODULE_PERIPH_INTC,
				      iomodule_intc_ext_irq_handler);

	LOG_INF("IOModule INTC initialized");

	return 0;
}

#define IOMODULE_INTC_INIT(inst)                                                \
	DEVICE_DT_INST_DEFINE(inst, &iomodule_intc_init, NULL, NULL, NULL,      \
			      PRE_KERNEL_1,                                     \
			      CONFIG_MFD_XLNX_IOMODULE_CHILD_INIT_PRIORITY,    \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(IOMODULE_INTC_INIT)
