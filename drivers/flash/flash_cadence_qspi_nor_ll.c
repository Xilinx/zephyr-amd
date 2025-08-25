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

int cad_qspi_idle(struct cad_qspi_params *cad_params)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	return (sys_read32(cad_params->reg_base + CAD_QSPI_CFG) & CAD_QSPI_CFG_IDLE) >> 31;
}

int cad_qspi_set_baudrate_div(struct cad_qspi_params *cad_params, uint32_t div)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (div > 0xf) {
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
	sys_write32((sys_read32(cad_params->reg_base + CAD_QSPI_CFG) & CAD_QSPI_CFG_CS_MSK) |
			    CAD_QSPI_CFG_CS(cs),
		    cad_params->reg_base + CAD_QSPI_CFG);

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

int cad_qspi_indirect_read_start_bank(struct cad_qspi_params *cad_params, uint32_t flash_addr,
				      uint32_t num_bytes)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(flash_addr, cad_params->reg_base + CAD_QSPI_INDRDSTADDR);
	sys_write32(num_bytes, cad_params->reg_base + CAD_QSPI_INDRDCNT);
	sys_write32(CAD_QSPI_INDRD_START | CAD_QSPI_INDRD_IND_OPS_DONE,
		    cad_params->reg_base + CAD_QSPI_INDRD);

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
	sys_write32(CAD_QSPI_INDWR_START | CAD_QSPI_INDWR_INDDONE,
		    cad_params->reg_base + CAD_QSPI_INDWR);

	return 0;
}

int cad_qspi_enable(struct cad_qspi_params *cad_params)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_set_bits(cad_params->reg_base + CAD_QSPI_CFG, CAD_QSPI_CFG_ENABLE);

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

	if (cad_params == NULL) {
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

