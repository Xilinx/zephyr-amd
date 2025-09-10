/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CAD_QSPI_NOR_LL_H
#define CAD_QSPI_NOR_LL_H

#include <zephyr/device.h>

#define CAD_QSPI_MICRON_N25Q_SUPPORT		CONFIG_CAD_QSPI_MICRON_N25Q_SUPPORT
#define CAD_QSPI_MICRON_MT35X_SUPPORT		CONFIG_CAD_QSPI_MICRON_MT35X_SUPPORT
#define CQSPI_REG_CONFIG_BAUD_MASK		0xF
#define CAD_INVALID				-1
#define CAD_QSPI_ERROR				-2

#define CAD_QSPI_EDGE_MODE_SDR_PHY		0x0U
#define CAD_QSPI_EDGE_MODE_SDR_NON_PHY		0x1U
#define CAD_QSPI_EDGE_MODE_DDR_PHY		0x2U

#define OSPI_REF_CLK				200000000
#define SIXTEENMB				0x1000000
#define MAX_DELAY_CNT				10000
#define MAX_IDAC_DELAY_CNT			10000000
#define MAX_IRQ_WAIT_CNT			1000000

#define CAD_QSPI_ADDR_FASTREAD			0
#define CAD_QSPI_ADDR_FASTREAD_DUAL_IO		1
#define CAD_QSPI_ADDR_FASTREAD_QUAD_IO		2
#define CAT_QSPI_ADDR_SINGLE_IO			0
#define CAT_QSPI_ADDR_DUAL_IO			1
#define CAT_QSPI_ADDR_QUAD_IO			2

#define CAD_QSPI_READ_1_1_1			0
#define CAD_QSPI_READ_1_0_1			1
#define CAD_QSPI_READ_1_1_8			2
#define CAD_QSPI_READ_1_8_8			3
#define CAD_QSPI_READ_8_8_8			4
#define CAD_QSPI_READ_8_0_8			5
#define CAD_QSPI_READ_8_0_0			6
#define CAD_QSPI_READ_8_8_0			7

#define CAD_QSPI_WRITE_1_1_1			0
#define CAD_QSPI_WRITE_1_0_1			1
#define CAD_QSPI_WRITE_1_1_8			2
#define CAD_QSPI_WRITE_1_8_8			3
#define CAD_QSPI_WRITE_8_8_8			4
#define CAD_QSPI_WRITE_8_0_8			5
#define CAD_QSPI_WRITE_8_0_0			6
#define CAD_QSPI_WRITE_8_8_0			7

/**
 * Bus width macros for Single/Dual/Quad/Octal.
 */
#define DQ0					0
#define DQ0_1					1
#define DQ0_3					2
#define DQ0_7					3

#define CAD_QSPI_BANK_ADDR(x)			((x) >> 24)
#define CAD_QSPI_BANK_ADDR_MSK			GENMASK(31, 24)

#define CAD_QSPI_COMMAND_TIMEOUT		0x10000000

#define CAD_QSPI_CFG				0x0
#define CAD_QSPI_CFG_PHY_ENABLE_MASK		(BIT(3))
#define CAD_QSPI_CFG_ENB_DIR_ACC_CTRL		(BIT(7))
#define CAD_QSPI_CFG_BAUDDIV_MSK		0xff87ffff
#define CAD_QSPI_CFG_BAUDDIV(x)			FIELD_PREP(0x780000, x)
#define CAD_QSPI_CFG_BAUDDIV_MAX		0xF
#define CAD_QSPI_CFG_CS_MSK			~0x3c00
#define CAD_QSPI_CFG_CS_SHIFT			10
#define CAD_QSPI_CFG_CS(x)			(((x) << 11))
#define CAD_QSPI_CFG_ENABLE			(BIT(0))
#define CAD_QSPI_CFG_ENDMA_CLR_MSK		0xffff7fff
#define CAD_QSPI_CFG_IDLE			(BIT(31))
#define CAD_QSPI_CFG_IDLE_MASK			31
#define CAD_QSPI_CFG_SELCLKPHASE_CLR_MSK	0xfffffffb
#define CAD_QSPI_CFG_SELCLKPOL_CLR_MSK		0xfffffffd
#define CAD_QSPI_CFI_ENDTR_PROTOCOL_MASK	0x01000000
#define CAD_QSPI_CFG_RESET_FLD_MASK		0x00000040
#define CAD_QSPI_CFG_RESET_PIN_FLD_MASK		0x00000020
#define CAD_QSPI_CFG_BAUD_MASK			0xF
#define CAD_QSPI_CFG_BAUD_LSB			19

#define CAD_QSPI_DELAY				0xc
#define CAD_QSPI_DELAY_TSLCH_LSB               0
#define CAD_QSPI_DELAY_TCHSH_LSB               8
#define CAD_QSPI_DELAY_TSD2D_LSB               16
#define CAD_QSPI_DELAY_TSHSL_LSB               24
#define CAD_QSPI_DELAY_TSLCH_MASK              0xFF
#define CAD_QSPI_DELAY_TCHSH_MASK              0xFF
#define CAD_QSPI_DELAY_TSD2D_MASK              0xFF
#define CAD_QSPI_DELAY_TSHSL_MASK              0xFF

#define CAD_QSPI_DELAY_CSSOT(x)			(FIELD_GET(0xff, (x)) << 0)
#define CAD_QSPI_DELAY_CSEOT(x)			(FIELD_GET(0xff, (x)) << 8)
#define CAD_QSPI_DELAY_CSDADS(x)		(FIELD_GET(0xff, (x)) << 16)
#define CAD_QSPI_DELAY_CSDA(x)			(FIELD_GET(0xff, (x)) << 24)

#define CAD_QSPI_WR_COMPL_CTRL			0x38
#define CAD_QSPI_WR_COMPL_CTRL_POLL_COUNT_SHIFT	16
#define CAD_QSPI_WR_COMPL_CTRL_POLL_COUNT(x)	(FIELD_GET(0xff, (x)) << 16)
#define CAD_QSPI_POLL_CNT_NON_PHY		0x1
#define CAD_QSPI_WR_COMPL_CTRL			0x38

#define CAD_QSPI_DEVSZ				0x14
#define CAD_QSPI_DEVSZ_ADDR_BYTES_FLD_MASK	0xF
#define CAD_QSPI_DEVSZ_ADDR_BYTES(x)		((x) << 0)
#define CAD_QSPI_DEVSZ_BYTES_PER_PAGE(x)	((x) << 4)
#define CAD_QSPI_DEVSZ_BYTES_PER_BLOCK(x)	((x) << 16)

#define CAD_QSPI_DEVWR				0x8
#define CAD_QSPI_DEVRD				0x4
#define CAD_QSPI_DEV_OPCODE(x)			(FIELD_GET(0xff, (x)) << 0)
#define CAD_QSPI_DEV_INST_TYPE(x)		(FIELD_GET(0x03, (x)) << 8)
#define CAD_QSPI_DEV_ADDR_TYPE(x)		(FIELD_GET(0x03, (x)) << 12)
#define CAD_QSPI_DEV_DATA_TYPE(x)		(FIELD_GET(0x03, (x)) << 16)
#define CAD_QSPI_DEV_MODE_BIT(x)		(FIELD_GET(0x01, (x)) << 20)
#define CAD_QSPI_DEV_DDR_EN(x)			(FIELD_GET(0x01, (x)) << 10)
#define CAD_QSPI_DEV_DUMMY_CLK_CYCLE(x)		(FIELD_GET(0x1f, (x)) << 24)
#define CAD_QSPI_DEV_RD_INSTR_FLD_MASK		0x300

#define CAD_QSPI_FLASHCMD			0x90
#define CAD_QSPI_FLASHCMD_ADDR			0x94
#define CAD_QSPI_FLASHCMD_EXECUTE		0x1
#define CAD_QSPI_FLASHCMD_EXECUTE_STAT		0x2
#define CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX	5
#define CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES(x)     (FIELD_PREP(0x000f80, (x)))
#define CAD_QSPI_FLASHCMD_OPCODE(x)             (FIELD_GET(0xff, (x)) << 24)
#define CAD_QSPI_FLASHCMD_ENRDDATA(x)           (FIELD_GET(1, (x)) << 23)
#define CAD_QSPI_FLASHCMD_NUMRDDATABYTES(x)     (FIELD_GET(0xf, (x)) << 20)
#define CAD_QSPI_FLASHCMD_ENCMDADDR(x)          (FIELD_GET(1, (x)) << 19)
#define CAD_QSPI_FLASHCMD_ENMODEBIT(x)		(FIELD_GET(1, (x)) << 18)
#define CAD_QSPI_FLASHCMD_NUMADDRBYTES(x)	(FIELD_GET(0x3, (x)) << 16)
#define CAD_QSPI_FLASHCMD_ENWRDATA(x)		(FIELD_GET(1, (x)) << 15)
#define CAD_QSPI_FLASHCMD_NUMWRDATABYTES(x)	(FIELD_GET(0x7, (x)) << 12)
#define CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(x)	(FIELD_GET(0x1f, (x)) << 7)
#define CAD_QSPI_FLASHCMD_RDDATA0		0xa0
#define CAD_QSPI_FLASHCMD_RDDATA1		0xa4
#define CAD_QSPI_FLASHCMD_WRDATA0		0xa8
#define CAD_QSPI_FLASHCMD_WRDATA1		0xac

#define CAD_QSPI_RDDATACAP			0x10
#define CAD_QSPI_RDDATACAP_BYP(x)		(FIELD_GET(1, (x)) << 0)
#define CAD_QSPI_RDDATACAP_DELAY(x)		(FIELD_GET(0xf, (x)) << 1)
#define CAD_QSPI_RDDATACAP_DELAY_MASK		0x0000001E
#define CAD_QSPI_NON_PHY_RD_DLY			0x1
#define CAD_QSPI_RDDATACAPT_DELAY_SHIFT		1
#define CAD_OSPI_RDDATACAPTG_DQS_ENABLE_MASK	0x100

#define CAD_QSPI_REMAPADDR			0x24
#define CAD_QSPI_REMAPADDR_VALUE_SET(x)		(FIELD_GET(0xffffffff, (x)) << 0)

#define CAD_QSPI_SRAMPART			0x18
#define CAD_QSPI_SRAMPART_VAL			0x80
#define CAD_QSPI_SRAMFILL			0x2c
#define CAD_QSPI_SRAMPART_ADDR(x)		(FIELD_GET(0x3ff, ((x) >> 0)))
#define CAD_QSPI_SRAM_FIFO_ENTRY_COUNT		(512 / sizeof(uint32_t))
#define CAD_QSPI_SRAMFILL_INDWRPART(x)		(FIELD_GET(0x00ffff, ((x) >> 16)))
#define CAD_QSPI_SRAMFILL_INDRDPART(x)		(FIELD_GET(0x00ffff, ((x) >> 0)))

#define CAD_QSPI_INDIRECTTRIGGERADDR		0x1c
#define CAD_QSPI_INDIRECTTRIGGERADDRRANGE	0x80
#define CAD_QSPI_TRIGGERRRANGE			0x6

#define CAD_QSPI_PHY_CFG			0xb4
#define CAD_QSPI_PHY_CONFIG_RESET_FLD_MASK	0x40000000
#define CAD_QSPI_PHY_CONFIG_RESYNC_FLD_MASK	0x80000000

#define CAD_QSPI_SELCLKPHASE(x)			(FIELD_GET(1, (x)) << 2)
#define CAD_QSPI_SELCLKPOL(x)			(FIELD_GET(1, (x)) << 1)

#define CAD_QSPI_STIG_FLAGSR_PROGRAMREADY(x)	(FIELD_GET(1, ((x) >> 7)))
#define CAD_QSPI_STIG_FLAGSR_ERASEREADY(x)	(FIELD_GET(1, ((x) >> 7)))
#define CAD_QSPI_STIG_FLAGSR_ERASEERROR(x)	(FIELD_GET(1, ((x) >> 5)))
#define CAD_QSPI_STIG_FLAGSR_PROGRAMERROR(x)	(FIELD_GET(1, ((x) >> 4)))
#define CAD_QSPI_STIG_OPCODE_CLFSR		0x50
#define CAD_QSPI_STIG_OPCODE_RDID		0x9f
#define CAD_QSPI_STIG_OPCODE_WRDIS		0x4
#define CAD_QSPI_STIG_OPCODE_WREN		0x6
#define CAD_QSPI_STIG_OPCODE_SUBSEC_ERASE	0x20
#define CAD_QSPI_STIG_OPCODE_SEC_ERASE		0xd8
#define CAD_QSPI_STIG_OPCODE_WREN_EXT_REG	0xc5
#define CAD_QSPI_STIG_OPCODE_DIE_ERASE		0xc4
#define CAD_QSPI_STIG_OPCODE_BULK_ERASE		0xc7
#define CAD_QSPI_STIG_OPCODE_RDSR		0x5
#define CAD_QSPI_STIG_OPCODE_RDFLGSR		0x70
#define CAD_QSPI_STIG_OPCODE_RESET_EN		0x66
#define CAD_QSPI_STIG_OPCODE_RESET_MEM		0x99
#define CAD_QSPI_STIG_OPCODE_ENTER_4B_ADDR_MODE	0xb7
#define CAD_QSPI_STIG_OPCODE_EXIT_4B_ADDR_MODE	0xb9
#define CAD_QSPI_STIG_RDID_CAPACITYID(x)	(FIELD_GET(0xff, ((x) >> 16)))
#define CAD_QSPI_STIG_SR_BUSY(x)		(FIELD_GET(1, ((x))))

#define CAD_QSPI_STIG_OPCODE_SEC_ERASE_4B	0xdc
#define CAD_QSPI_STIG_OPCODE_READ_4B		0x13
#define CAD_QSPI_STIG_OPCODE_READ_OCTAL_IO_4B	0xcc
#define CAD_QSPI_STIG_OPCODE_WRITE_4B		0x12

#define CAD_QSPI_INST_SINGLE			0
#define CAD_QSPI_INST_DUAL			1
#define CAD_QSPI_INST_QUAD			2

#define CAD_QSPI_IRQSTATUS			0x40
#define CAD_QSPI_IRQMASK				0x1ffff

#define CAD_QSPI_INDRDWATERMARK			0x64
#define CAD_QSPI_INDRDWATERMARK_LEVEL		0x200

#define CAD_QSPI_INDRDSTADDR			0x68
#define CAD_QSPI_INDRDCNT			0x6c
#define CAD_QSPI_INDRD				0x60
#define CAD_QSPI_INDRD_RD_STAT(x)		(FIELD_GET(1, ((x) >> 2)))
#define CAD_QSPI_INDRD_START			1
#define CAD_QSPI_INDRD_IND_OPS_DONE		0x20

#define CAD_QSPI_INDWR				0x70
#define CAD_QSPI_INDWR_RDSTAT(x)		(FIELD_GET(1, ((x) >> 2)))
#define CAD_QSPI_INDWRWATERMARK			0x74
#define CAD_QSPI_INDWRSTADDR			0x78
#define CAD_QSPI_INDWRCNT			0x7c
#define CAD_QSPI_INDWR				0x70
#define CAD_QSPI_INDWR_START			0x1
#define CAD_QSPI_INDWR_INDDONE			0x20

#define CAD_QSPI_INT_STATUS_ALL			0x0000ffff

#define CAD_QSPI_N25Q_DIE_SIZE			0x02000000
#define CAD_QSPI_BANK_SIZE			0x01000000
#define CAD_QSPI_PAGE_SIZE			0x00000100
#define CAD_QSPI_TROGGER_ADDR			0xc0000000
#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
#define CAD_QSPI_IRQMSK				0x1044
#define CAD_QSPI_INDWMARK_IRQMSK		0x40
#else
#define CAD_QSPI_IRQMSK				0x44
#endif

#define CAD_QSPI_SUBSECTOR_SIZE			CONFIG_CAD_QSPI_NOR_SUBSECTOR_SIZE
#define CAD_QSPI_SECTOR_SIZE			CONFIG_CAD_QSPI_NOR_SECTOR_SIZE
#define QSPI_ADDR_BYTES				CONFIG_QSPI_ADDR_BYTES
#define QSPI_BYTES_PER_DEV			CONFIG_QSPI_BYTES_PER_DEV
#define QSPI_BYTES_PER_BLOCK			CONFIG_QSPI_BYTES_PER_BLOCK

#define QSPI_FAST_READ				0xb

#define QSPI_WRITE				0x2

/* QSPI CONFIGURATIONS */
#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
#define QSPI_CONFIG_CPOL			0
#define QSPI_CONFIG_CPHA			0

#define QSPI_CONFIG_CSSOT			0x01
#define QSPI_CONFIG_CSEOT			0x01
#define QSPI_CONFIG_CSDADS			0x00
#define QSPI_CONFIG_CSDA			0x0A
#else
#define QSPI_CONFIG_CPOL			1
#define QSPI_CONFIG_CPHA			1

#define QSPI_CONFIG_CSSOT			0x14
#define QSPI_CONFIG_CSEOT			0x14
#define QSPI_CONFIG_CSDADS			0xff
#define QSPI_CONFIG_CSDA			0xc8
#endif

struct cad_qspi_params {
	uintptr_t	reg_base;
	uintptr_t	data_base;
	uint32_t	data_size;
	int		clk_rate;
	uint32_t	qspi_device_size;
	int		cad_qspi_cs;
#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
	uint32_t	fifo_depth;
	uint32_t	fifo_width;
	uint32_t	addr_size;
	uint32_t	tshsl_ns;
	uint32_t	tsd2d_ns;
	uint32_t	tchsh_ns;
	uint32_t	tslch_ns;
	uint32_t	proto;
	uint32_t	dummy;
	uint32_t	mode_bit_en;
	uint32_t	ddr;
#endif
};

int cad_qspi_init(struct cad_qspi_params *cad_params,
	uint32_t clk_phase, uint32_t clk_pol, uint32_t csda,
	uint32_t csdads, uint32_t cseot, uint32_t cssot,
	uint32_t rddatacap);
void cad_qspi_set_chip_select(struct cad_qspi_params *cad_params,
	int cs);
int cad_qspi_erase(struct cad_qspi_params *cad_params,
	uint32_t offset, uint32_t size);
int cad_qspi_write(struct cad_qspi_params *cad_params, void *buffer,
	uint32_t offset, uint32_t size);
int cad_qspi_read(struct cad_qspi_params *cad_params, void *buffer,
	uint32_t offset, uint32_t size);
int cad_qspi_update(struct cad_qspi_params *cad_params, void *buffer,
	uint32_t offset, uint32_t size);
int cad_qspi_stig_cmd_helper(struct cad_qspi_params *cad_params, int cs, uint32_t cmd,
			     uint32_t *addr, uint32_t *data, uint32_t num_bytes, bool out);
int cad_qspi_set_read_config(struct cad_qspi_params *cad_params, uint32_t opcode,
			     uint32_t instr_type, uint32_t addr_type, uint32_t data_type,
			     uint32_t mode_bit, uint32_t dummy_clk_cycle);
int cad_qspi_set_write_config(struct cad_qspi_params *cad_params, uint32_t opcode,
			      uint32_t addr_type, uint32_t data_type, uint32_t dummy_clk_cycle);
int cad_qspi_set_baudrate_div(struct cad_qspi_params *cad_params, uint32_t div);
int cad_qspi_idle(struct cad_qspi_params *cad_params);
int cad_qspi_timing_config(struct cad_qspi_params *cad_params, uint32_t clkphase, uint32_t clkpol,
			   uint32_t csda, uint32_t csdads, uint32_t cseot, uint32_t cssot,
			   uint32_t rddatacap);
int cad_qspi_int_disable(struct cad_qspi_params *cad_params, uint32_t mask);
int cad_qspi_enable(struct cad_qspi_params *cad_params);
int cad_qspi_configure_dev_size(struct cad_qspi_params *cad_params, uint32_t addr_bytes,
				uint32_t bytes_per_dev, uint32_t bytes_per_block);
int cad_qspi_read_bank(struct cad_qspi_params *cad_params, uint8_t *buffer, uint32_t offset,
		       uint32_t size);
int cad_qspi_indirect_page_bound_write(struct cad_qspi_params *cad_params, uint32_t offset,
				       uint8_t *buffer, uint32_t len);
int cad_qspi_set_rd_data_cap_dly(struct cad_qspi_params *cad_params,
				 uint32_t val, uint32_t data_cap_delay);
int cad_qspi_check_idle(struct cad_qspi_params *params);
void cad_qspi_configure_addr_size(struct cad_qspi_params *params);
void cad_qspi_setup_write_instr(struct cad_qspi_params *params, uint32_t opcode);
void cad_qspi_setup_read_instr(struct cad_qspi_params *params, uint32_t opcode);
void cad_qspi_cs_deassert(struct cad_qspi_params *params);
void cad_qspi_cs_assert(struct cad_qspi_params *params);
int cad_qspi_ctrl_init(struct cad_qspi_params *cad_params);
int cad_qspi_set_mode(struct cad_qspi_params *params, uint32_t mode);
void cad_qspi_device_reset_via_ospi(struct cad_qspi_params *param);
void cad_qspi_set_delays(struct cad_qspi_params *param);
int cad_qspi_get_indirect_read_status(struct cad_qspi_params *params);
int cad_qspi_linear_mux_configure(struct cad_qspi_params *cad_params,
				  uint32_t iou_slcr, bool enable);
int cad_qspi_irqstatus_poll(struct cad_qspi_params *cad_params, uint32_t irq);
int cad_qspi_indirect_read_status_poll(struct cad_qspi_params *params);
int cad_qspi_indirect_read_config(struct cad_qspi_params *cad_params,
				  uint32_t offset, uint32_t size);
int cad_qspi_indirect_read_start(struct cad_qspi_params *cad_params);
int cad_qspi_indirect_write_config(struct cad_qspi_params *cad_params,
				   uint32_t offset, uint32_t size);
int cad_qspi_indirect_page_write(struct cad_qspi_params *cad_params, uint32_t offset,
				 uint8_t *buffer, uint32_t len);
int cad_qspi_disable(struct cad_qspi_params *cad_params);
int cad_qspi_get_indirect_write_status(struct cad_qspi_params *params);

#endif
