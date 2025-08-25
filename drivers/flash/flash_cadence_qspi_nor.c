/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT cdns_qspi_nor

#include "flash_cadence_qspi_nor_ll.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(flash_cadence, CONFIG_FLASH_LOG_LEVEL);

struct flash_cad_priv {
	DEVICE_MMIO_NAMED_RAM(qspi_reg);
	DEVICE_MMIO_NAMED_RAM(qspi_data);
	struct cad_qspi_params params;
};

struct flash_cad_config {
	DEVICE_MMIO_NAMED_ROM(qspi_reg);
	DEVICE_MMIO_NAMED_ROM(qspi_data);
};

static const struct flash_parameters flash_cad_parameters = {
	.write_block_size = QSPI_BYTES_PER_DEV,
	.erase_value = 0xff,
};

#define DEV_DATA(dev)	((struct flash_cad_priv *)((dev)->data))
#define DEV_CFG(dev)	((struct flash_cad_config *)((dev)->config))

int flash_cad_qspi_stig_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy)
{

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy bytes\n");
		return -1;
	}

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
					CAD_QSPI_FLASHCMD_OPCODE(opcode) |
					CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES(dummy),
					NULL, NULL, 0, 0);
}

int flash_cad_qspi_stig_read_cmd(struct cad_qspi_params *cad_params, uint32_t opcode,
				 uint32_t dummy, uint32_t num_bytes, uint32_t *output)
{
	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy byes\n");
		return -1;
	}

	if (num_bytes > 8 || num_bytes == 0)
		return -1;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_ENRDDATA(1) |
		       CAD_QSPI_FLASHCMD_NUMRDDATABYTES(num_bytes - 1) |
		       CAD_QSPI_FLASHCMD_ENCMDADDR(0) | CAD_QSPI_FLASHCMD_ENMODEBIT(0) |
		       CAD_QSPI_FLASHCMD_NUMADDRBYTES(0) | CAD_QSPI_FLASHCMD_ENWRDATA(0) |
		       CAD_QSPI_FLASHCMD_NUMWRDATABYTES(0) | CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy);

	if (cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
				     cmd, NULL, output, num_bytes, 0)) {
		LOG_ERR("failed to send stig cmd\n");
		return -1;
	}

	return 0;
}

int flash_cad_qspi_stig_wr_cmd(struct cad_qspi_params *cad_params, uint32_t opcode,
			       uint32_t dummy, uint32_t num_bytes, uint32_t *input)
{
	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy byes\n");
		return -1;
	}

	if (num_bytes > 8 || num_bytes == 0)
		return -1;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_ENRDDATA(0) |
		       CAD_QSPI_FLASHCMD_NUMRDDATABYTES(0) | CAD_QSPI_FLASHCMD_ENCMDADDR(0) |
		       CAD_QSPI_FLASHCMD_ENMODEBIT(0) | CAD_QSPI_FLASHCMD_NUMADDRBYTES(0) |
		       CAD_QSPI_FLASHCMD_ENWRDATA(1) |
		       CAD_QSPI_FLASHCMD_NUMWRDATABYTES(num_bytes - 1) |
		       CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy);

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
					cmd, NULL, input, num_bytes, 1);
}

int flash_cad_qspi_stig_addr_cmd(struct cad_qspi_params *cad_params, uint32_t opcode,
				 uint32_t dummy, uint32_t addr)
{
	uint32_t cmd;

	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1))
		return -1;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy) |
	      CAD_QSPI_FLASHCMD_ENCMDADDR(1) | CAD_QSPI_FLASHCMD_NUMADDRBYTES(2);

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
					cmd, &addr, NULL, 0, 0);
}

int flash_cad_qspi_device_bank_select(struct cad_qspi_params *cad_params, uint32_t bank)
{
	int status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);

	if (status != 0) {
		return status;
	}

	status = flash_cad_qspi_stig_wr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN_EXT_REG,
					    0, 1, &bank);

	if (status != 0) {
		return status;
	}

	return flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WRDIS, 0);
}

int flash_cad_qspi_device_status(struct cad_qspi_params *cad_params, uint32_t *status)
{
	return flash_cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDSR, 0, 1, status);
}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
int flash_cad_qspi_n25q_enable(struct cad_qspi_params *cad_params)
{
	cad_qspi_set_read_config(cad_params, QSPI_FAST_READ, CAD_QSPI_INST_SINGLE,
				 CAD_QSPI_ADDR_FASTREAD, CAT_QSPI_ADDR_SINGLE_IO, 1, 0);
	cad_qspi_set_write_config(cad_params, QSPI_WRITE, 0, 0, 0);

	return 0;
}

int flash_cad_qspi_n25q_wait_for_program_and_erase(struct cad_qspi_params *cad_params,
						   int program_only)
{
	uint32_t status, flag_sr;
	int count = 0;

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		status = flash_cad_qspi_device_status(cad_params, &status);
		if (status != 0) {
			LOG_ERR("Error getting device status\n");
			return -1;
		}
		if (!CAD_QSPI_STIG_SR_BUSY(status))
			break;
		count++;
	}

	if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
		LOG_ERR("Timed out waiting for idle\n");
		return -1;
	}

	count = 0;

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		status = flash_cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDFLGSR,
						      0, 1, &flag_sr);
		if (status != 0) {
			LOG_ERR("Error waiting program and erase.\n");
			return status;
		}

		if ((program_only && CAD_QSPI_STIG_FLAGSR_PROGRAMREADY(flag_sr)) ||
		    (!program_only && CAD_QSPI_STIG_FLAGSR_ERASEREADY(flag_sr)))
			break;
	}

	if (count >= CAD_QSPI_COMMAND_TIMEOUT)
		LOG_ERR("Timed out waiting for program and erase\n");

	if ((program_only && CAD_QSPI_STIG_FLAGSR_PROGRAMERROR(flag_sr)) ||
	    (!program_only && CAD_QSPI_STIG_FLAGSR_ERASEERROR(flag_sr))) {
		LOG_ERR("Error programming/erasing flash\n");
		flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_CLFSR, 0);
		return -1;
	}

	return 0;
}
#endif

int flash_cad_qspi_enable_subsector_bank(struct cad_qspi_params *cad_params, uint32_t addr)
{
	int status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);

	if (status != 0) {
		return status;
	}

	status = flash_cad_qspi_stig_addr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_SUBSEC_ERASE,
					      0, addr);
	if (status != 0) {
		return status;
	}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	status = flash_cad_qspi_n25q_wait_for_program_and_erase(cad_params, 0);
#endif
	return status;
}

int flash_cad_qspi_erase_subsector(struct cad_qspi_params *cad_params, uint32_t addr)
{
	int status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = flash_cad_qspi_device_bank_select(cad_params, addr >> 24);
	if (status != 0) {
		return status;
	}

	return flash_cad_qspi_enable_subsector_bank(cad_params, addr);
}

int flash_cad_qspi_erase_sector(struct cad_qspi_params *cad_params, uint32_t addr)
{
	int status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = flash_cad_qspi_device_bank_select(cad_params, addr >> 24);

	if (status != 0) {
		return status;
	}

	status = flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);

	if (status != 0) {
		return status;
	}

	status = flash_cad_qspi_stig_addr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_SEC_ERASE, 0, addr);
	if (status != 0) {
		return status;
	}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	status = flash_cad_qspi_n25q_wait_for_program_and_erase(cad_params, 0);
#endif
	return status;
}

void flash_cad_qspi_calibration(struct cad_qspi_params *cad_params, uint32_t dev_clk,
				uint32_t qspi_clk_mhz)
{
	int status;
	uint32_t dev_sclk_mhz = 27; /*min value to get biggest 0xF div factor*/
	uint32_t data_cap_delay;
	uint32_t sample_rdid;
	uint32_t rdid;
	uint32_t div_actual;
	uint32_t div_bits;
	int first_pass, last_pass;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
	}

	/*1.  Set divider to bigger value (slowest SCLK)
	 *2.  RDID and save the value
	 */
	div_actual = (qspi_clk_mhz + (dev_sclk_mhz - 1)) / dev_sclk_mhz;
	div_bits = (((div_actual + 1) / 2) - 1);
	status = cad_qspi_set_baudrate_div(cad_params, 0xf);

	status = flash_cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID,
					      0, 3, &sample_rdid);
	if (status != 0) {
		return;
	}

	/*3. Set divider to the intended frequency
	 *4.  Set the read delay = 0
	 *5.  RDID and check whether the value is same as item 2
	 *6.  Increase read delay and compared the value against item 2
	 *7.  Find the range of read delay that have same as
	 *    item 2 and divide it to 2
	 */
	div_actual = (qspi_clk_mhz + (dev_clk - 1)) / dev_clk;
	div_bits = (((div_actual + 1) / 2) - 1);
	status = cad_qspi_set_baudrate_div(cad_params, div_bits);

	if (status != 0) {
		return;
	}

	data_cap_delay = 0;
	first_pass = -1;
	last_pass = -1;

	do {
		if (status != 0) {
			break;
		}

		status = flash_cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID,
						      0, 3, &rdid);
		if (status != 0) {
			break;
		}

		if (rdid == sample_rdid) {
			if (first_pass == -1)
				first_pass = data_cap_delay;
			else
				last_pass = data_cap_delay;
		}

		data_cap_delay++;

		sys_write32(CAD_QSPI_RDDATACAP_BYP(1) | CAD_QSPI_RDDATACAP_DELAY(data_cap_delay),
			    cad_params->reg_base + CAD_QSPI_RDDATACAP);

	} while (data_cap_delay < 0x10);

	if (first_pass > 0) {
		int diff = first_pass - last_pass;

		data_cap_delay = first_pass + diff / 2;
	}

	sys_write32(CAD_QSPI_RDDATACAP_BYP(1) | CAD_QSPI_RDDATACAP_DELAY(data_cap_delay),
		    cad_params->reg_base + CAD_QSPI_RDDATACAP);
	status = flash_cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &rdid);

	if (status != 0) {
		return;
	}
}

int flash_cad_qspi_init(struct cad_qspi_params *cad_params, uint32_t clk_phase, uint32_t clk_pol,
			uint32_t csda, uint32_t csdads, uint32_t cseot, uint32_t cssot,
			uint32_t rddatacap)
{
	int status = 0;
	uint32_t qspi_desired_clk_freq;
	uint32_t rdid = 0;
	uint32_t cap_code;

	LOG_INF("Initializing Qspi");
	if (!cad_params) {
		LOG_ERR("Wrong parameter");
		return -EINVAL;
	}

	if (cad_qspi_idle(cad_params) == 0) {
		LOG_ERR("device not idle");
		return -EBUSY;
	}
	status = cad_qspi_timing_config(cad_params, clk_phase, clk_pol, csda, csdads, cseot, cssot,
					rddatacap);

	if (status != 0) {
		LOG_ERR("config set timing failure\n");
		return status;
	}

	sys_write32(CAD_QSPI_REMAPADDR_VALUE_SET(0), cad_params->reg_base + CAD_QSPI_REMAPADDR);

	status = cad_qspi_int_disable(cad_params, CAD_QSPI_INT_STATUS_ALL);
	if (status != 0) {
		LOG_ERR("failed disable\n");
		return status;
	}

	cad_qspi_set_baudrate_div(cad_params, 0xf);
	status = cad_qspi_enable(cad_params);
	if (status != 0) {
		LOG_ERR("failed enable\n");
		return status;
	}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	status = flash_cad_qspi_n25q_enable(cad_params);
	if (status != 0) {
		LOG_ERR("failed qspi flash enable\n");
		return status;
	}
#endif

	qspi_desired_clk_freq = 100;
	flash_cad_qspi_calibration(cad_params, qspi_desired_clk_freq, cad_params->clk_rate);

	status = flash_cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &rdid);

	if (status != 0) {
		LOG_ERR("Error reading RDID\n");
		return status;
	}

	/*
	 * NOTE: The Size code seems to be a form of BCD (binary coded decimal).
	 * The first nibble is the 10's digit and the second nibble is the 1's
	 * digit in the number of bytes.
	 *
	 * Capacity ID samples:
	 * 0x15 :   16 Mb =>   2 MiB => 1 << 21 ; BCD=15
	 * 0x16 :   32 Mb =>   4 MiB => 1 << 22 ; BCD=16
	 * 0x17 :   64 Mb =>   8 MiB => 1 << 23 ; BCD=17
	 * 0x18 :  128 Mb =>  16 MiB => 1 << 24 ; BCD=18
	 * 0x19 :  256 Mb =>  32 MiB => 1 << 25 ; BCD=19
	 * 0x1a
	 * 0x1b
	 * 0x1c
	 * 0x1d
	 * 0x1e
	 * 0x1f
	 * 0x20 :  512 Mb =>  64 MiB => 1 << 26 ; BCD=20
	 * 0x21 : 1024 Mb => 128 MiB => 1 << 27 ; BCD=21
	 */

	cap_code = CAD_QSPI_STIG_RDID_CAPACITYID(rdid);

	if (!(((cap_code >> 4) > 0x9) || ((cap_code & 0xf) > 0x9))) {
		uint32_t decoded_cap = ((cap_code >> 4) * 10) + (cap_code & 0xf);

		cad_params->qspi_device_size = 1 << (decoded_cap + 6);
		LOG_INF("QSPI Capacity: %x", cad_params->qspi_device_size);

	} else {
		LOG_ERR("Invalid CapacityID encountered: 0x%02x", cap_code);
		return -1;
	}

	cad_qspi_configure_dev_size(cad_params, QSPI_ADDR_BYTES, QSPI_BYTES_PER_DEV,
				    QSPI_BYTES_PER_BLOCK);

	LOG_INF("Flash size: %d Bytes", cad_params->qspi_device_size);

	return status;
}

int flash_cad_qspi_read(struct cad_qspi_params *cad_params, void *buffer,
			uint32_t offset, uint32_t size)
{
	uint32_t bank_count, bank_addr, bank_offset, copy_len;
	uint8_t *read_data;
	int i, status;

	status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (offset >= cad_params->qspi_device_size ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || size == 0) {
		LOG_ERR("Invalid read parameter\n");
		return -EINVAL;
	}

	if (CAD_QSPI_INDRD_RD_STAT(sys_read32(cad_params->reg_base + CAD_QSPI_INDRD))) {
		LOG_ERR("Read in progress\n");
		return -ENOTBLK;
	}

	/*
	 * bank_count : Number of bank(s) affected, including partial banks.
	 * bank_addr  : Aligned address of the first bank,
	 *		including partial bank.
	 * bank_ofst  : The offset of the bank to read.
	 *		Only used when reading the first bank.
	 */
	bank_count = CAD_QSPI_BANK_ADDR(offset + size - 1) - CAD_QSPI_BANK_ADDR(offset) + 1;
	bank_addr = offset & CAD_QSPI_BANK_ADDR_MSK;
	bank_offset = offset & (CAD_QSPI_BANK_SIZE - 1);

	read_data = (uint8_t *)buffer;

	copy_len = MIN(size, CAD_QSPI_BANK_SIZE - bank_offset);

	for (i = 0; i < bank_count; ++i) {
		status = flash_cad_qspi_device_bank_select(cad_params,
							   CAD_QSPI_BANK_ADDR(bank_addr));
		if (status != 0) {
			break;
		}

		status = cad_qspi_read_bank(cad_params, read_data, bank_offset, copy_len);

		if (status != 0) {
			break;
		}

		bank_addr += CAD_QSPI_BANK_SIZE;
		read_data += copy_len;
		size -= copy_len;
		bank_offset = 0;
		copy_len = MIN(size, CAD_QSPI_BANK_SIZE);
	}

	return status;
}

int flash_cad_qspi_erase(struct cad_qspi_params *cad_params, uint32_t offset, uint32_t size)
{
	int status = 0;
	uint32_t subsector_offset = offset & (CAD_QSPI_SUBSECTOR_SIZE - 1);
	uint32_t erase_size = MIN(size, CAD_QSPI_SUBSECTOR_SIZE - subsector_offset);

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	while (size) {
		status = flash_cad_qspi_erase_subsector(cad_params, offset);

		if (status != 0) {
			break;
		}

		offset += erase_size;
		size -= erase_size;
		erase_size = MIN(size, CAD_QSPI_SUBSECTOR_SIZE);
	}
	return status;
}

int flash_cad_qspi_write_bank(struct cad_qspi_params *cad_params, uint32_t offset, uint8_t *buffer,
			      uint32_t size)
{
	int status = 0;
	uint32_t page_offset = offset & (CAD_QSPI_PAGE_SIZE - 1);
	uint32_t write_size = MIN(size, CAD_QSPI_PAGE_SIZE - page_offset);

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	while (size) {
		status = cad_qspi_indirect_page_bound_write(cad_params, offset, buffer, write_size);
		if (status != 0) {
			break;
		}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
		status = flash_cad_qspi_n25q_wait_for_program_and_erase(cad_params, 1);
		if (status != 0) {
			break;
		}
#endif

		offset += write_size;
		buffer += write_size;
		size -= write_size;
		write_size = MIN(size, CAD_QSPI_PAGE_SIZE);
	}
	return status;
}

int flash_cad_qspi_write(struct cad_qspi_params *cad_params, void *buffer, uint32_t offset,
			 uint32_t size)
{
	int status, i;
	uint32_t bank_count, bank_addr, bank_offset, copy_len;
	uint8_t *write_data;

	status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (offset >= cad_params->qspi_device_size ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || size == 0) {
		return -EINVAL;
	}

	if (CAD_QSPI_INDWR_RDSTAT(sys_read32(cad_params->reg_base + CAD_QSPI_INDWR))) {
		LOG_ERR("QSPI Error: Write in progress\n");
		return -ENOTBLK;
	}

	bank_count = CAD_QSPI_BANK_ADDR(offset + size - 1) - CAD_QSPI_BANK_ADDR(offset) + 1;
	bank_addr = offset & CAD_QSPI_BANK_ADDR_MSK;
	bank_offset = offset & (CAD_QSPI_BANK_SIZE - 1);

	write_data = buffer;

	copy_len = MIN(size, CAD_QSPI_BANK_SIZE - bank_offset);

	for (i = 0; i < bank_count; ++i) {
		status = flash_cad_qspi_device_bank_select(cad_params,
							   CAD_QSPI_BANK_ADDR(bank_addr));

		if (status != 0) {
			break;
		}

		status = flash_cad_qspi_write_bank(cad_params, bank_offset, write_data, copy_len);
		if (status != 0) {
			break;
		}

		bank_addr += CAD_QSPI_BANK_SIZE;
		write_data += copy_len;
		size -= copy_len;
		bank_offset = 0;

		copy_len = MIN(size, CAD_QSPI_BANK_SIZE);
	}
	return status;
}

int flash_cad_qspi_update(struct cad_qspi_params *cad_params, void *buffer, uint32_t offset,
			  uint32_t size)
{
	int status = 0;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = flash_cad_qspi_erase(cad_params, offset, size);

	if (status != 0) {
		return status;
	}

	return flash_cad_qspi_write(cad_params, buffer, offset, size);
}

void flash_cad_qspi_reset(struct cad_qspi_params *cad_params)
{
	flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RESET_EN, 0);
	flash_cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RESET_MEM, 0);
}

static int flash_cad_read(const struct device *dev, off_t offset,
				void *data, size_t len)
{
	struct flash_cad_priv *priv = dev->data;
	struct cad_qspi_params *cad_params = &priv->params;
	int rc;

	if ((data == NULL) || (len == 0)) {
		LOG_ERR("Invalid input parameter for QSPI Read!");
		return -EINVAL;
	}

	rc = flash_cad_qspi_read(cad_params, data, (uint32_t)offset, len);

	if (rc < 0) {
		LOG_ERR("Cadence QSPI Flash Read Failed");
		return rc;
	}

	return 0;
}

static int flash_cad_erase(const struct device *dev, off_t offset,
				size_t len)
{
	struct flash_cad_priv *priv = dev->data;
	struct cad_qspi_params *cad_params = &priv->params;
	int rc;

	if (len == 0) {
		LOG_ERR("Invalid input parameter for QSPI Erase!");
		return -EINVAL;
	}

	rc = flash_cad_qspi_erase(cad_params, (uint32_t)offset, len);

	if (rc < 0) {
		LOG_ERR("Cadence QSPI Flash Erase Failed!");
		return rc;
	}

	return 0;
}

static int flash_cad_write(const struct device *dev, off_t offset,
				const void *data, size_t len)
{
	struct flash_cad_priv *priv = dev->data;
	struct cad_qspi_params *cad_params = &priv->params;
	int rc;

	if ((data == NULL) || (len == 0)) {
		LOG_ERR("Invalid input parameter for QSPI Write!");
		return -EINVAL;
	}

	rc = flash_cad_qspi_write(cad_params, (void *)data, (uint32_t)offset, len);

	if (rc < 0) {
		LOG_ERR("Cadence QSPI Flash Write Failed!");
		return rc;
	}

	return 0;
}

static const struct flash_parameters *
flash_cad_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &flash_cad_parameters;
}

static const struct flash_driver_api flash_cad_api = {
	.erase = flash_cad_erase,
	.write = flash_cad_write,
	.read = flash_cad_read,
	.get_parameters = flash_cad_get_parameters,
};

static int flash_cad_init(const struct device *dev)
{
	struct flash_cad_priv *priv = dev->data;
	struct cad_qspi_params *cad_params = &priv->params;
	int rc;

	DEVICE_MMIO_NAMED_MAP(dev, qspi_reg, K_MEM_CACHE_NONE);
	DEVICE_MMIO_NAMED_MAP(dev, qspi_data, K_MEM_CACHE_NONE);

	cad_params->reg_base = DEVICE_MMIO_NAMED_GET(dev, qspi_reg);
	cad_params->data_base = DEVICE_MMIO_NAMED_GET(dev, qspi_data);

	rc = flash_cad_qspi_init(cad_params, QSPI_CONFIG_CPHA,
				 QSPI_CONFIG_CPOL, QSPI_CONFIG_CSDA,
				 QSPI_CONFIG_CSDADS, QSPI_CONFIG_CSEOT,
				 QSPI_CONFIG_CSSOT, 0);

	if (rc < 0) {
		LOG_ERR("Cadence QSPI Flash Init Failed");
		return rc;
	}

	return 0;
}

#define CREATE_FLASH_CADENCE_QSPI_DEVICE(inst)				\
	static struct flash_cad_priv flash_cad_priv_##inst = {		\
		.params = {						\
			.clk_rate = DT_INST_PROP(inst, clock_frequency),\
			.data_size = DT_INST_REG_SIZE_BY_IDX(inst, 1),  \
		},							\
	};								\
									\
	static struct flash_cad_config flash_cad_config_##inst = {	\
		DEVICE_MMIO_NAMED_ROM_INIT_BY_NAME(			\
				qspi_reg, DT_DRV_INST(inst)),		\
		DEVICE_MMIO_NAMED_ROM_INIT_BY_NAME(			\
				qspi_data, DT_DRV_INST(inst)),		\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			flash_cad_init,					\
			NULL,						\
			&flash_cad_priv_##inst,				\
			&flash_cad_config_##inst,			\
			POST_KERNEL,					\
			CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		\
			&flash_cad_api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_FLASH_CADENCE_QSPI_DEVICE)
