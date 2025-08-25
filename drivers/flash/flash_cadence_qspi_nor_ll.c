/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "flash_cadence_qspi_nor_ll.h"

#include <string.h>

#include <zephyr/logging/log.h>

#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(flash_cadence_ll, CONFIG_FLASH_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
void cad_qspi_setup_read_instr(struct cad_qspi_params *params, uint32_t opcode)
{
	uint32_t dummy_clks = params->dummy, mode_bit_en = 0, ddr_en = 0;
	uint32_t dataxfer_type, addrxfer_type, instxfer_type;
	uint32_t reg;

	switch (params->proto) {
	case CAD_QSPI_READ_1_1_1:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0;
		instxfer_type = DQ0;
		break;
	case CAD_QSPI_READ_1_1_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0;
		instxfer_type = DQ0;
		break;
	case CAD_QSPI_READ_1_8_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0_7;
		instxfer_type = DQ0;
		break;
	case CAD_QSPI_READ_8_8_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0_7;
		instxfer_type = DQ0_7;
		break;
	case CAD_QSPI_READ_8_0_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0;
		instxfer_type = DQ0_7;
		break;
	case CAD_QSPI_READ_8_0_0:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0;
		instxfer_type = DQ0_7;
		break;
	case CAD_QSPI_READ_8_8_0:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0_7;
		instxfer_type = DQ0_7;
		break;
	default:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0;
		instxfer_type = DQ0;
	}
	reg = CAD_QSPI_DEV_DUMMY_CLK_CYCLE(dummy_clks) |
		CAD_QSPI_DEV_MODE_BIT(mode_bit_en) |
		CAD_QSPI_DEV_DDR_EN(ddr_en) |
		CAD_QSPI_DEV_DATA_TYPE(dataxfer_type) |
		CAD_QSPI_DEV_ADDR_TYPE(addrxfer_type) |
		CAD_QSPI_DEV_INST_TYPE(instxfer_type) |
		CAD_QSPI_DEV_OPCODE(opcode);
	sys_write32(reg, params->reg_base + CAD_QSPI_DEVRD);

	reg = sys_read32(params->reg_base + CAD_QSPI_RDDATACAP);
	reg &= ~CAD_OSPI_RDDATACAPTG_DQS_ENABLE_MASK;
	sys_write32(0x1, params->reg_base + CAD_QSPI_RDDATACAP);
}

void cad_qspi_setup_write_instr(struct cad_qspi_params *params, uint32_t opcode)
{
	uint32_t dummy_clks = params->dummy;
	uint32_t dataxfer_type, addrxfer_type, instxfer_type;
	uint32_t reg;

	switch (params->proto) {
	case CAD_QSPI_WRITE_1_1_1:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0;
		instxfer_type = DQ0;
		break;
	case CAD_QSPI_WRITE_1_1_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0;
		instxfer_type = DQ0;
		break;
	case CAD_QSPI_WRITE_1_8_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0_7;
		instxfer_type = DQ0;
		break;
	case CAD_QSPI_WRITE_8_8_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0_7;
		instxfer_type = DQ0_7;
		break;
	case CAD_QSPI_WRITE_8_0_8:
		dataxfer_type = DQ0_7;
		addrxfer_type = DQ0;
		instxfer_type = DQ0_7;
		break;
	case CAD_QSPI_WRITE_8_0_0:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0;
		instxfer_type = DQ0_7;
		break;
	case CAD_QSPI_WRITE_8_8_0:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0_7;
		instxfer_type = DQ0_7;
		break;
	default:
		dataxfer_type = DQ0;
		addrxfer_type = DQ0;
		instxfer_type = DQ0;
	}
	reg = CAD_QSPI_DEV_DUMMY_CLK_CYCLE(dummy_clks) |
		CAD_QSPI_DEV_DATA_TYPE(dataxfer_type) |
		CAD_QSPI_DEV_ADDR_TYPE(addrxfer_type) |
		CAD_QSPI_DEV_OPCODE(opcode);
	sys_write32(reg, params->reg_base + CAD_QSPI_DEVWR);

	reg = sys_read32(params->reg_base + CAD_QSPI_DEVRD);
	reg &= ~CAD_QSPI_DEV_RD_INSTR_FLD_MASK;
	reg |= CAD_QSPI_DEV_INST_TYPE(instxfer_type);
	sys_write32(reg, params->reg_base + CAD_QSPI_DEVRD);
}
#endif

void cad_qspi_cs_assert(struct cad_qspi_params *params)
{
	uint32_t reg, cs;

	reg = sys_read32(params->reg_base + CAD_QSPI_CFG);
	reg &= CAD_QSPI_CFG_CS_MSK;
	cs = (~((uint32_t)0x01 << params->cad_qspi_cs)) & (uint32_t)0xF;
	reg |= cs << CAD_QSPI_CFG_CS_SHIFT;

	sys_write32(reg, params->reg_base + CAD_QSPI_CFG);
}

void cad_qspi_cs_deassert(struct cad_qspi_params *params)
{
	uint32_t reg;

	reg = sys_read32(params->reg_base + CAD_QSPI_CFG);
	reg &= CAD_QSPI_CFG_CS_MSK;
	reg |= (~CAD_QSPI_CFG_CS_MSK);

	sys_write32(reg, params->reg_base + CAD_QSPI_CFG);
}

int cad_qspi_disable(struct cad_qspi_params *cad_params)
{
	int reg;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}
	reg = sys_read32(cad_params->reg_base + CAD_QSPI_CFG);
	reg &= ~CAD_QSPI_CFG_ENABLE;
	sys_write32(reg, cad_params->reg_base + CAD_QSPI_CFG);

	return 0;
}

#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
int cad_qspi_ctrl_init(struct cad_qspi_params *cad_params)
{
	int reg;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* Configure the SRAM split to 1:1 . */
	sys_write32((cad_params->fifo_depth / 2), cad_params->reg_base + CAD_QSPI_SRAMPART);

	/* Load indirect trigger address. */
	sys_write32(cad_params->data_base, cad_params->reg_base + CAD_QSPI_INDIRECTTRIGGERADDR);

	/* Program read watermark -- 1/2 of the FIFO. */
	sys_write32(cad_params->fifo_depth * cad_params->fifo_width / 2,
		    cad_params->reg_base + CAD_QSPI_INDRDWATERMARK);

	/* Program write watermark -- 1/8 of the FIFO. */
	sys_write32(cad_params->fifo_depth * cad_params->fifo_width / 8,
		    cad_params->reg_base + CAD_QSPI_INDWRWATERMARK);

	/* Disable direct access controller & Reset PHY */
	reg = sys_read32(cad_params->reg_base + CAD_QSPI_CFG);
	reg &= ~CAD_QSPI_CFG_ENB_DIR_ACC_CTRL;
	sys_write32(reg, cad_params->reg_base + CAD_QSPI_CFG);

	/* Reset the Delay lines */
	sys_write32(CAD_QSPI_PHY_CONFIG_RESET_FLD_MASK, cad_params->reg_base + CAD_QSPI_PHY_CFG);

	/* Reset PHY */
	reg = sys_read32(cad_params->reg_base + CAD_QSPI_CFG);
	reg &= ~CAD_QSPI_CFG_PHY_ENABLE_MASK;
	sys_write32(reg, cad_params->reg_base + CAD_QSPI_CFG);

	return 0;
}
#endif

int cad_qspi_idle(struct cad_qspi_params *cad_params)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	return (sys_read32(cad_params->reg_base + CAD_QSPI_CFG) &
		CAD_QSPI_CFG_IDLE) >> CAD_QSPI_CFG_IDLE_MASK;
}

int cad_qspi_check_idle(struct cad_qspi_params *params)
{
	uint32_t delaycnt = 0;

	while (cad_qspi_idle(params) == 0) {
		if (delaycnt == MAX_DELAY_CNT) {
			LOG_ERR("Timed out waiting for ctlr idle\n");
			return -EINVAL;
		}
		k_sleep(K_USEC((1)));
		delaycnt++;
	}

	return 0;
}

int cad_qspi_set_baudrate_div(struct cad_qspi_params *cad_params, uint32_t div)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (div > CAD_QSPI_CFG_BAUDDIV_MAX) {
		return CAD_INVALID;
	}

	sys_clear_bits(cad_params->reg_base + CAD_QSPI_CFG, ~CAD_QSPI_CFG_BAUDDIV_MSK);

	sys_set_bits(cad_params->reg_base + CAD_QSPI_CFG, CAD_QSPI_CFG_BAUDDIV(div));

	return 0;
}

int cad_qspi_configure_dev_size(struct cad_qspi_params *cad_params, uint32_t addr_bytes,
				uint32_t bytes_per_dev, uint32_t bytes_per_block)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_DEVSZ_ADDR_BYTES(addr_bytes) |
			    CAD_QSPI_DEVSZ_BYTES_PER_PAGE(bytes_per_dev) |
			    CAD_QSPI_DEVSZ_BYTES_PER_BLOCK(bytes_per_block),
		    cad_params->reg_base + CAD_QSPI_DEVSZ);
	return 0;
}

int cad_qspi_set_read_config(struct cad_qspi_params *cad_params, uint32_t opcode,
			     uint32_t instr_type, uint32_t addr_type, uint32_t data_type,
			     uint32_t mode_bit, uint32_t dummy_clk_cycle)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_DEV_OPCODE(opcode) | CAD_QSPI_DEV_INST_TYPE(instr_type) |
			    CAD_QSPI_DEV_ADDR_TYPE(addr_type) | CAD_QSPI_DEV_DATA_TYPE(data_type) |
			    CAD_QSPI_DEV_MODE_BIT(mode_bit) |
			    CAD_QSPI_DEV_DUMMY_CLK_CYCLE(dummy_clk_cycle),
		    cad_params->reg_base + CAD_QSPI_DEVRD);

	return 0;
}

int cad_qspi_set_write_config(struct cad_qspi_params *cad_params, uint32_t opcode,
			      uint32_t addr_type, uint32_t data_type, uint32_t dummy_clk_cycle)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_DEV_OPCODE(opcode) | CAD_QSPI_DEV_ADDR_TYPE(addr_type) |
			    CAD_QSPI_DEV_DATA_TYPE(data_type) |
			    CAD_QSPI_DEV_DUMMY_CLK_CYCLE(dummy_clk_cycle),
		    cad_params->reg_base + CAD_QSPI_DEVWR);

	return 0;
}

int cad_qspi_timing_config(struct cad_qspi_params *cad_params, uint32_t clkphase, uint32_t clkpol,
			   uint32_t csda, uint32_t csdads, uint32_t cseot, uint32_t cssot,
			   uint32_t rddatacap)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t cfg = sys_read32(cad_params->reg_base + CAD_QSPI_CFG);

	cfg &= CAD_QSPI_CFG_SELCLKPHASE_CLR_MSK & CAD_QSPI_CFG_SELCLKPOL_CLR_MSK;
	cfg |= CAD_QSPI_SELCLKPHASE(clkphase) | CAD_QSPI_SELCLKPOL(clkpol);

	sys_write32(cfg, cad_params->reg_base + CAD_QSPI_CFG);

	sys_write32(CAD_QSPI_DELAY_CSSOT(cssot) | CAD_QSPI_DELAY_CSEOT(cseot) |
			    CAD_QSPI_DELAY_CSDADS(csdads) | CAD_QSPI_DELAY_CSDA(csda),
		    cad_params->reg_base + CAD_QSPI_DELAY);

	return 0;
}

int cad_qspi_stig_cmd_helper(struct cad_qspi_params *cad_params, int cs, uint32_t cmd,
			     uint32_t *addr, uint32_t *data, uint32_t num_bytes, bool out)
{
	uint32_t count = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (addr)
		sys_write32(*addr, cad_params->reg_base + CAD_QSPI_FLASHCMD_ADDR);

	/* write data */
	if (data && out) {
		sys_write32(data[0], cad_params->reg_base + CAD_QSPI_FLASHCMD_WRDATA0);

		if (num_bytes > 4)
			sys_write32(data[1], cad_params->reg_base + CAD_QSPI_FLASHCMD_WRDATA1);
	}
	/* chip select */
	cad_qspi_cs_assert(cad_params);

	sys_write32(cmd, cad_params->reg_base + CAD_QSPI_FLASHCMD);
	sys_write32(cmd | CAD_QSPI_FLASHCMD_EXECUTE, cad_params->reg_base + CAD_QSPI_FLASHCMD);

	do {
		uint32_t reg = sys_read32(cad_params->reg_base + CAD_QSPI_FLASHCMD);

		if (!(reg & CAD_QSPI_FLASHCMD_EXECUTE_STAT))
			break;
		count++;
	} while (count < CAD_QSPI_COMMAND_TIMEOUT);

	if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
		LOG_ERR("Error sending QSPI command %x, timed out\n", cmd);
		return CAD_QSPI_ERROR;
	}

	/* read data */
	if (data && !out) {
		data[0] = sys_read32(cad_params->reg_base + CAD_QSPI_FLASHCMD_RDDATA0);

		if (num_bytes > 4) {
			data[1] = sys_read32(cad_params->reg_base + CAD_QSPI_FLASHCMD_RDDATA1);
		}
	}
	return 0;
}

int cad_qspi_indirect_read_start(struct cad_qspi_params *cad_params)
{
	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_INDRD_START | CAD_QSPI_INDRD_IND_OPS_DONE,
		    cad_params->reg_base + CAD_QSPI_INDRD);

	return 0;
}

int cad_qspi_indirect_read_start_bank(struct cad_qspi_params *cad_params, uint32_t flash_addr,
				      uint32_t num_bytes)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(flash_addr, cad_params->reg_base + CAD_QSPI_INDRDSTADDR);
	sys_write32(num_bytes, cad_params->reg_base + CAD_QSPI_INDRDCNT);

	return cad_qspi_indirect_read_start(cad_params);
}

int cad_qspi_indirect_write_start(struct cad_qspi_params *cad_params)
{
	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_INDWR_START | CAD_QSPI_INDWR_INDDONE,
		    cad_params->reg_base + CAD_QSPI_INDWR);

	return 0;
}

int cad_qspi_indirect_write_start_bank(struct cad_qspi_params *cad_params, uint32_t flash_addr,
				       uint32_t num_bytes)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(flash_addr, cad_params->reg_base + CAD_QSPI_INDWRSTADDR);
	sys_write32(num_bytes, cad_params->reg_base + CAD_QSPI_INDWRCNT);

	return cad_qspi_indirect_write_start(cad_params);
}

int cad_qspi_enable(struct cad_qspi_params *cad_params)
{
	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_set_bits(cad_params->reg_base + CAD_QSPI_CFG, CAD_QSPI_CFG_ENABLE);

	return 0;
}

uint32_t calculate_ticks_for_ns(uint32_t ref_clk_hz, uint32_t ns_val)
{
	uint32_t ticks;

	ticks = ref_clk_hz / KHZ(1);
	ticks = DIV_ROUND_UP(ticks * ns_val, MHZ(1));

	return ticks;
}

#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
void cad_qspi_set_delays(struct cad_qspi_params *param)
{
	uint32_t tshsl, tchsh, tslch, tsd2d;
	uint32_t reg;
	int tsclk;

	/* calculate the number of ref ticks for one sclk tick */
	tsclk = DIV_ROUND_UP(OSPI_REF_CLK, param->clk_rate);

	tshsl = calculate_ticks_for_ns(OSPI_REF_CLK, param->tshsl_ns);
	/* this particular value must be at least one sclk */
	if (tshsl < tsclk)
		tshsl = tsclk;

	tchsh = calculate_ticks_for_ns(OSPI_REF_CLK, param->tchsh_ns);
	tslch = calculate_ticks_for_ns(OSPI_REF_CLK, param->tslch_ns);
	tsd2d = calculate_ticks_for_ns(OSPI_REF_CLK, param->tsd2d_ns);

	reg = (tshsl & CAD_QSPI_DELAY_TSHSL_MASK) << CAD_QSPI_DELAY_TSHSL_LSB;
	reg |= (tchsh & CAD_QSPI_DELAY_TCHSH_MASK) << CAD_QSPI_DELAY_TCHSH_LSB;
	reg |= (tslch & CAD_QSPI_DELAY_TSLCH_MASK) << CAD_QSPI_DELAY_TSLCH_LSB;
	reg |= (tsd2d & CAD_QSPI_DELAY_TSD2D_MASK) << CAD_QSPI_DELAY_TSD2D_LSB;
	sys_write32(reg, param->reg_base + CAD_QSPI_DELAY);
}
#endif

void cad_qspi_device_reset_via_ospi(struct cad_qspi_params *param)
{
	uint32_t reg;

	reg = sys_read32(param->reg_base + CAD_QSPI_CFG);
	reg |= CAD_QSPI_CFG_RESET_FLD_MASK;
	sys_write32(reg, param->reg_base + CAD_QSPI_CFG);

	reg = sys_read32(param->reg_base + CAD_QSPI_CFG);
	reg &= ~CAD_QSPI_CFG_RESET_PIN_FLD_MASK;
	sys_write32(reg, param->reg_base + CAD_QSPI_CFG);

	k_sleep(K_USEC((5)));

	reg = sys_read32(param->reg_base + CAD_QSPI_CFG);
	reg |= CAD_QSPI_CFG_RESET_PIN_FLD_MASK;
	sys_write32(reg, param->reg_base + CAD_QSPI_CFG);

	k_sleep(K_USEC((150)));

	reg = sys_read32(param->reg_base + CAD_QSPI_CFG);
	reg &= ~CAD_QSPI_CFG_RESET_PIN_FLD_MASK;
	sys_write32(reg, param->reg_base + CAD_QSPI_CFG);

	k_sleep(K_USEC((1200)));
}

void cad_qspi_setdlldelay(struct cad_qspi_params *params)
{
	sys_write32(0x00, params->reg_base + CAD_QSPI_PHY_CFG);
	sys_write32(CAD_QSPI_PHY_CONFIG_RESYNC_FLD_MASK,
		    params->reg_base + CAD_QSPI_PHY_CFG);
}

int cad_qspi_set_mode(struct cad_qspi_params *params, uint32_t mode)
{
	int ret = 0;
	uint32_t reg;

	if (!params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (mode != CAD_QSPI_EDGE_MODE_SDR_NON_PHY &&
	    mode != CAD_QSPI_EDGE_MODE_SDR_PHY &&
	    mode != CAD_QSPI_EDGE_MODE_DDR_PHY) {
		LOG_ERR("Unsupported mode\n");
		return ret;
	}

	ret = cad_qspi_disable(params);
	if (ret != 0) {
		LOG_ERR("failed disable\n");
		return ret;
	}

	reg = sys_read32(params->reg_base + CAD_QSPI_CFG);
	reg &= ~(CAD_QSPI_CFI_ENDTR_PROTOCOL_MASK | CAD_QSPI_CFG_PHY_ENABLE_MASK);
	sys_write32(reg, params->reg_base + CAD_QSPI_CFG);

	reg = sys_read32(params->reg_base + CAD_QSPI_WR_COMPL_CTRL);
	reg &= ~CAD_QSPI_WR_COMPL_CTRL_POLL_COUNT(0xFF);
	reg |= (CAD_QSPI_POLL_CNT_NON_PHY << CAD_QSPI_WR_COMPL_CTRL_POLL_COUNT_SHIFT);
	sys_write32(reg, params->reg_base + CAD_QSPI_WR_COMPL_CTRL);

	reg = sys_read32(params->reg_base + CAD_QSPI_RDDATACAP);
	reg &= ~CAD_QSPI_RDDATACAP_DELAY_MASK;

	reg |= (CAD_QSPI_NON_PHY_RD_DLY << CAD_QSPI_RDDATACAPT_DELAY_SHIFT);

	sys_write32(reg, params->reg_base + CAD_QSPI_RDDATACAP);

	cad_qspi_setdlldelay(params);

	ret = cad_qspi_enable(params);
	if (ret != 0) {
		LOG_ERR("failed enable\n");
		return ret;
	}

	return 0;
}

int cad_qspi_int_disable(struct cad_qspi_params *cad_params, uint32_t mask)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (cad_qspi_idle(cad_params) == 0)
		return -1;

	if ((CAD_QSPI_INT_STATUS_ALL & mask) == 0)
		return -1;

	sys_write32(mask, cad_params->reg_base + CAD_QSPI_IRQMSK);
	return 0;
}

void cad_qspi_set_chip_select(struct cad_qspi_params *cad_params, int cs)
{

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
	}

	cad_params->cad_qspi_cs = cs;
}

int cad_qspi_indirect_page_bound_write(struct cad_qspi_params *cad_params, uint32_t offset,
				       uint8_t *buffer, uint32_t len)
{
	int status = 0, i;
	uint32_t write_count, write_capacity, *write_data, space, write_fill_level, sram_partition;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_indirect_write_start_bank(cad_params, offset, len);

	if (status != 0) {
		return status;
	}

	write_count = 0;
	sram_partition =
		CAD_QSPI_SRAMPART_ADDR(sys_read32(cad_params->reg_base + CAD_QSPI_SRAMPART));
	write_capacity = (uint32_t)CAD_QSPI_SRAM_FIFO_ENTRY_COUNT - sram_partition;

	while (write_count < len) {
		write_fill_level = CAD_QSPI_SRAMFILL_INDWRPART(
			sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));
		space = MIN(write_capacity - write_fill_level,
			    (len - write_count) / sizeof(uint32_t));
		write_data = (uint32_t *)(buffer + write_count);
		for (i = 0; i < space; ++i)
			sys_write32(*write_data++, cad_params->data_base);

		write_count += space * sizeof(uint32_t);
	}

	return 0;
}

int cad_qspi_read_bank(struct cad_qspi_params *cad_params, uint8_t *buffer, uint32_t offset,
		       uint32_t size)
{
	int status;
	uint32_t read_count = 0, *read_data;
	int level = 1, count = 0, i;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_indirect_read_start_bank(cad_params, offset, size);

	if (status != 0) {
		return status;
	}

	while (read_count < size) {
		do {
			level = CAD_QSPI_SRAMFILL_INDRDPART(
				sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));
			read_data = (uint32_t *)(buffer + read_count);
			for (i = 0; i < level; ++i)
				*read_data++ = sys_read32(cad_params->data_base);

			read_count += level * sizeof(uint32_t);
			count++;
		} while (level > 0);
	}

	return 0;
}

int cad_qspi_set_rd_data_cap_dly(struct cad_qspi_params *cad_params,
				 uint32_t val, uint32_t data_cap_delay)
{
	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_RDDATACAP_BYP(val) |
		    CAD_QSPI_RDDATACAP_DELAY(data_cap_delay),
		    cad_params->reg_base + CAD_QSPI_RDDATACAP);

	return 0;
}

#if DT_HAS_COMPAT_STATUS_OKAY(xlnx_versal_ospi_1_0)
void cad_qspi_configure_addr_size(struct cad_qspi_params *params)
{
	uint32_t reg;

	reg = sys_read32(params->reg_base + CAD_QSPI_DEVSZ);
	reg &= ~(CAD_QSPI_DEVSZ_ADDR_BYTES_FLD_MASK);

	if (params->addr_size != 0)
		reg |= (params->addr_size - 1);

	sys_write32(reg, params->reg_base + CAD_QSPI_DEVSZ);
}
#endif

int cad_qspi_get_indirect_read_status(struct cad_qspi_params *params)
{
	return (sys_read32(params->reg_base + CAD_QSPI_INDRD) &
		CAD_QSPI_INDRD_IND_OPS_DONE) >> 5;

}

int cad_qspi_indirect_read_status_poll(struct cad_qspi_params *params)
{
	uint32_t delaycnt = 0, reg;

	while (!cad_qspi_get_indirect_read_status(params)) {
		if (delaycnt == MAX_IDAC_DELAY_CNT) {
			LOG_ERR("Timed out waiting for indirect read complete\n");
			return -EINVAL;
		}
		k_sleep(K_USEC((1)));
		delaycnt++;
	}

	/* write status bit */
	reg = sys_read32(params->reg_base + CAD_QSPI_INDRD);
	reg |= CAD_QSPI_INDRD_IND_OPS_DONE;
	sys_write32(reg, params->reg_base + CAD_QSPI_INDRD);

	return 0;
}

int cad_qspi_get_indirect_write_status(struct cad_qspi_params *params)
{
	return (sys_read32(params->reg_base + CAD_QSPI_INDWR) & CAD_QSPI_INDWR_INDDONE) >> 5;
}

int cad_qspi_indirect_write_status_poll(struct cad_qspi_params *params)
{
	uint32_t delaycnt = 0;

	while (!cad_qspi_get_indirect_write_status(params)) {
		if (delaycnt == MAX_IDAC_DELAY_CNT) {
			LOG_ERR("Timed out waiting for indirect write complete\n");
			return -EINVAL;
		}
		k_sleep(K_USEC((1)));
		delaycnt++;
	}

	/* write status bit */
	sys_write32(CAD_QSPI_INDWR_INDDONE, params->reg_base + CAD_QSPI_INDWR);

	return 0;
}

int cad_qspi_indirect_page_write(struct cad_qspi_params *cad_params, uint32_t offset,
				 uint8_t *buffer, uint32_t len)
{
	int status = 0, i;
	uint32_t *write_data, write_words, mod_bytes;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	write_words = len / 4;
	mod_bytes = len % 4;

	status = cad_qspi_indirect_write_start(cad_params);
	if (status != 0)
		return status;

	write_data = (uint32_t *)buffer;
	for (i = 0; i < write_words; i++)
		sys_write32(*(write_data++), cad_params->data_base);

	if (mod_bytes) {
		uint32_t tmp = 0xFFFFFFFF;

		memcpy(&tmp, write_data, mod_bytes);
		sys_write32(tmp, cad_params->data_base);
	}
	return cad_qspi_indirect_write_status_poll(cad_params);
}

int cad_qspi_indirect_write_config(struct cad_qspi_params *cad_params,
				   uint32_t offset, uint32_t size)
{
	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* SRAM Partition configuration for write transfer */
	sys_write32(CAD_QSPI_SRAMPART_VAL, cad_params->reg_base + CAD_QSPI_SRAMPART);

	sys_write32(0xFFFFFFFF, cad_params->reg_base + CAD_QSPI_INDWRWATERMARK);

	/* Configure Address */
	sys_write32(offset, cad_params->reg_base + CAD_QSPI_INDWRSTADDR);

	/* Configure number of bytes to write and indirect address */
	sys_write32(size, cad_params->reg_base + CAD_QSPI_INDWRCNT);
	sys_write32(cad_params->data_base,
		    cad_params->reg_base + CAD_QSPI_INDIRECTTRIGGERADDR);

	/* configure trigger range */
	sys_write32(CAD_QSPI_TRIGGERRRANGE,
		    cad_params->reg_base + CAD_QSPI_INDIRECTTRIGGERADDRRANGE);

	return 0;
}

int cad_qspi_indirect_read_config(struct cad_qspi_params *cad_params,
				  uint32_t offset, uint32_t size)
{
	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* SRAM Partition configuration for write transfer */
	sys_write32(CAD_QSPI_SRAMPART_VAL, cad_params->reg_base + CAD_QSPI_SRAMPART);

	sys_write32(CAD_QSPI_INDRDWATERMARK_LEVEL,
		    cad_params->reg_base + CAD_QSPI_INDRDWATERMARK);

	/* Configure Address */
	sys_write32(offset, cad_params->reg_base + CAD_QSPI_INDRDSTADDR);

	/* Configure number of bytes to read and indirect address */
	sys_write32(size, cad_params->reg_base + CAD_QSPI_INDRDCNT);

	sys_write32(cad_params->data_base,
		    cad_params->reg_base + CAD_QSPI_INDIRECTTRIGGERADDR);

	/* configure trigger range */
	sys_write32(CAD_QSPI_TRIGGERRRANGE,
		    cad_params->reg_base + CAD_QSPI_INDIRECTTRIGGERADDRRANGE);

	return 0;
}

int cad_qspi_irqstatus_poll(struct cad_qspi_params *cad_params, uint32_t irq)
{
	uint32_t reg;
	uint32_t count = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	do {
		if (count == MAX_IRQ_WAIT_CNT) {
			LOG_ERR("Timed out waiting for irq\n");
			return -1;
		}
		count++;
		k_sleep(K_USEC((1)));
		reg = sys_read32(cad_params->reg_base + CAD_QSPI_IRQSTATUS);
	} while ((reg & irq) == 0);

	/* Clear the Bit */
	sys_write32(irq, cad_params->reg_base + CAD_QSPI_IRQSTATUS);

	return 0;
}
