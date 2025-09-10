/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT xlnx_versal_ospi_1_0

#include "flash_cadence_qspi_nor_ll.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(flash_cadence, CONFIG_FLASH_LOG_LEVEL);

/*
 * Macros defined for addresses which are outside the
 * controller space.
 */
#define XPMC_IOU_SLCR_BASEADDR			0xF1060000U
#define XPMC_IOU_SLCR_OSPI_MUX_SEL		0x00000504U
#define XPMC_IOU_SLCR_OSPI_MUX_SEL_DAC_MASK	0x00000002U

struct flash_cad_priv {
	DEVICE_MMIO_NAMED_RAM(qspi_reg);
	DEVICE_MMIO_NAMED_RAM(qspi_data);
	struct cad_qspi_params params;
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	struct flash_pages_layout layout;
#endif
};

struct flash_cad_config {
	DEVICE_MMIO_NAMED_ROM(qspi_reg);
	DEVICE_MMIO_NAMED_ROM(qspi_data);
};

static const struct flash_parameters flash_cad_parameters = {
	.write_block_size = 1,
	.erase_value = 0xff,
};

#define DEV_DATA(dev)	((struct flash_cad_priv *)((dev)->data))
#define DEV_CFG(dev)	((struct flash_cad_config *)((dev)->config))

int cad_qspi_linear_mux_configure(struct cad_qspi_params *cad_params,
				  uint32_t iou_slcr, bool enable)
{
	uint32_t reg, status;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_disable(cad_params);
	if (status)
		return status;

	reg = sys_read32(iou_slcr + XPMC_IOU_SLCR_OSPI_MUX_SEL);

	if (enable)
		reg |= XPMC_IOU_SLCR_OSPI_MUX_SEL_DAC_MASK;
	else
		reg &= ~XPMC_IOU_SLCR_OSPI_MUX_SEL_DAC_MASK;

	sys_write32(reg, iou_slcr + XPMC_IOU_SLCR_OSPI_MUX_SEL);

	status = cad_qspi_enable(cad_params);
	if (status)
		return status;

	return 0;
}

int flash_stig_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy)
{
	int status;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy bytes\n");
		return -1;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* configure address size */
	cad_qspi_configure_addr_size(cad_params);

	cad_params->proto = CAD_QSPI_WRITE_1_1_1;
	cad_params->dummy = 0;

	/* Setup Write Instr. Reg. */
	cad_qspi_setup_write_instr(cad_params, opcode);

	status =
	cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
				 CAD_QSPI_FLASHCMD_OPCODE(opcode) |
				 CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES(dummy) |
				 CAD_QSPI_FLASHCMD_NUMADDRBYTES(cad_params->addr_size - 1),
				 NULL, NULL, 0, 0);

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* chip de-select */
	cad_qspi_cs_deassert(cad_params);

	return status;
}

int flash_stig_read_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy,
			uint32_t num_bytes, uint32_t *output)
{
	int status;
	uint32_t cmd;

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

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_ENRDDATA(1) |
	      CAD_QSPI_FLASHCMD_NUMRDDATABYTES(num_bytes - 1) |
	      CAD_QSPI_FLASHCMD_ENCMDADDR(0) | CAD_QSPI_FLASHCMD_ENMODEBIT(0) |
	      CAD_QSPI_FLASHCMD_NUMADDRBYTES(3) | CAD_QSPI_FLASHCMD_ENWRDATA(0) |
	      CAD_QSPI_FLASHCMD_NUMWRDATABYTES(0) | CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy);

	/* configure address size */
	cad_qspi_configure_addr_size(cad_params);

	if (cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
				     cmd, NULL, output, num_bytes, 0)) {
		LOG_ERR("failed to send stig cmd\n");
		return -1;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* chip de-select */
	cad_qspi_cs_deassert(cad_params);

	return 0;
}

int flash_stig_wr_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy,
		      uint32_t num_bytes, uint32_t *input)
{
	uint32_t cmd;

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

	cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_ENRDDATA(0) |
	      CAD_QSPI_FLASHCMD_NUMRDDATABYTES(0) | CAD_QSPI_FLASHCMD_ENCMDADDR(0) |
	      CAD_QSPI_FLASHCMD_ENMODEBIT(0) | CAD_QSPI_FLASHCMD_NUMADDRBYTES(0) |
	      CAD_QSPI_FLASHCMD_ENWRDATA(1) |
	      CAD_QSPI_FLASHCMD_NUMWRDATABYTES(num_bytes - 1) |
	      CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy);

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
					cmd, NULL, input, num_bytes, 1);
}

int flash_stig_addr_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy,
			uint32_t addr)
{
	uint32_t cmd;
	int status;

	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1))
		return -1;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* configure address size */
	cad_qspi_configure_addr_size(cad_params);

	cad_params->proto = CAD_QSPI_WRITE_1_1_1;
	/* Setup Instr Register */
	cad_qspi_setup_write_instr(cad_params, opcode);

	cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) |
		CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy) |
		CAD_QSPI_FLASHCMD_ENCMDADDR(1) |
		CAD_QSPI_FLASHCMD_NUMADDRBYTES(cad_params->addr_size - 1);

	status = cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
					  cmd, &addr, NULL, 0, 0);

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* chip de-select */
	cad_qspi_cs_deassert(cad_params);

	return status;
}

int flash_device_status(struct cad_qspi_params *cad_params, uint32_t *status)
{
	cad_params->addr_size = 0;
	return flash_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDSR, 0, 1, status);
}

#if CAD_QSPI_MICRON_MT35X_SUPPORT
int flash_mt35x_enable(struct cad_qspi_params *cad_params)
{
	cad_qspi_set_read_config(cad_params, CAD_QSPI_STIG_OPCODE_RDFLGSR,
				 CAD_QSPI_INST_SINGLE, CAD_QSPI_ADDR_FASTREAD,
				 CAT_QSPI_ADDR_SINGLE_IO, 0, 0);
	cad_qspi_set_write_config(cad_params, CAD_QSPI_STIG_OPCODE_WRITE_4B,
				  0, 0, 0);

	return 0;
}

int flash_mt35x_wait_for_program_and_erase(struct cad_qspi_params *cad_params, int program_only)
{
	uint32_t status, flag_sr;
	int count = 0;

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		status = flash_device_status(cad_params, &status);
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
		status = flash_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDFLGSR, 0, 1,
					     &flag_sr);
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
		flash_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_CLFSR, 0);
		return -1;
	}

	return 0;
}
#endif

int flash_ospi_read(struct cad_qspi_params *cad_params, void *buffer,
		    uint32_t offset, uint32_t size)
{
	uint8_t *read_data = (uint8_t *)buffer;
	int status, i;
	uint32_t read_count = 0, read_words, mod_bytes;
	uint8_t *buf_end = (uint8_t *)buffer + size;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	if (offset >= cad_params->qspi_device_size ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || size == 0) {
		LOG_ERR("Invalid read parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_get_indirect_read_status(cad_params);
	if (status) {
		LOG_ERR("Read in progress\n");
		return -ENOTBLK;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	/* chip select */
	cad_qspi_cs_assert(cad_params);

	cad_params->addr_size = 4;
	/* configure address size */
	cad_qspi_configure_addr_size(cad_params);

	/* configure mux Linear */
	cad_qspi_linear_mux_configure(cad_params, XPMC_IOU_SLCR_BASEADDR, 1);
#if CAD_QSPI_MICRON_MT35X_SUPPORT
	status = flash_mt35x_enable(cad_params);
	if (status != 0)
		goto err;
#endif

	cad_params->proto = CAD_QSPI_READ_1_8_8;
	cad_params->dummy = 16;
	cad_params->mode_bit_en = 0;
	cad_params->ddr = 0;
	cad_qspi_setup_read_instr(cad_params, CAD_QSPI_STIG_OPCODE_READ_OCTAL_IO_4B);

	status = cad_qspi_indirect_read_config(cad_params, offset, size);
	if (status != 0)
		goto err;

	sys_write32(CAD_QSPI_IRQMASK, cad_params->reg_base + CAD_QSPI_IRQSTATUS);

	status = cad_qspi_int_disable(cad_params, CAD_QSPI_IRQMSK);
	if (status != 0)
		goto err;

	status = cad_qspi_indirect_read_start(cad_params);
	if (status != 0)
		goto err;

	status = cad_qspi_irqstatus_poll(cad_params, CAD_QSPI_INDWMARK_IRQMSK);
	if (status != 0)
		goto err;

	read_words = size / 4;
	mod_bytes = size % 4;

	while (read_count < size) {
		uint32_t tmp;

		for (i = 0; i < read_words; i++) {
			tmp = sys_read32(cad_params->data_base);
			memcpy((uint8_t *)(read_data + read_count), &tmp, 4);
			read_count += 4;
		}
		if (mod_bytes) {
			tmp = sys_read32(cad_params->data_base + read_count);
			uint32_t rem_bytes = (buf_end - (uint8_t *)(read_data + read_count));

			memcpy((uint8_t *)(read_data + read_count), &tmp, rem_bytes);
			read_count += rem_bytes;
		}
	}

	status = cad_qspi_int_disable(cad_params, 0x00);
	if (status != 0)
		goto err;

	status = cad_qspi_indirect_read_status_poll(cad_params);
	if (status != 0)
		goto err;

	/* Confugure mux Linear*/
	cad_qspi_linear_mux_configure(cad_params, XPMC_IOU_SLCR_BASEADDR, 0);
#if CAD_QSPI_MICRON_MT35X_SUPPORT
	status = flash_mt35x_enable(cad_params);
	if (status != 0)
		goto err;
#endif

err:
	/* check for the controller idle */
	cad_qspi_check_idle(cad_params);

	/* chip select */
	cad_qspi_cs_assert(cad_params);

	return 0;
}

int flash_ospi_erase(struct cad_qspi_params *cad_params, uint32_t offset, uint32_t size)
{
	uint32_t status, numsect, sect;
	uint32_t addr = offset;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	numsect = (size / CAD_QSPI_SECTOR_SIZE);
	if (size % CAD_QSPI_SECTOR_SIZE)
		numsect++;

	for (sect = 0; sect < numsect; sect++) {
		cad_params->addr_size = 4;
		status = flash_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);
		if (status != 0)
			return status;

		cad_params->addr_size = 4;
		status = flash_stig_addr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_SEC_ERASE_4B,
					     0, addr);
		if (status != 0)
			return status;
#if CAD_QSPI_MICRON_MT35X_SUPPORT
		status = flash_mt35x_wait_for_program_and_erase(cad_params, 0);
#else
		int count = 0;

		while (count < CAD_QSPI_COMMAND_TIMEOUT) {
			status = flash_device_status(cad_params, &status);
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
#endif
		addr += CAD_QSPI_SECTOR_SIZE;
	}

	return 0;
}

int flash_ospi_write(struct cad_qspi_params *cad_params, void *buffer,
		     uint32_t offset, uint32_t size)
{
	uint32_t page_offset = offset & (CAD_QSPI_PAGE_SIZE - 1);
	uint32_t write_size = MIN(size, CAD_QSPI_PAGE_SIZE - page_offset);
	uint8_t *write_data = (uint8_t *)buffer;
	uint32_t ofs = offset;
	int status;

	if (!cad_params) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* check for the controller idle */
	status = cad_qspi_check_idle(cad_params);
	if (status != 0)
		return status;

	if (offset >= cad_params->qspi_device_size ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || size == 0) {
		return -EINVAL;
	}

	if (cad_qspi_get_indirect_write_status(cad_params)) {
		LOG_ERR("QSPI Error: Write in progress\n");
		return -ENOTBLK;
	}

	while (size) {
		cad_params->addr_size = 0;
		status = flash_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);
		if (status != 0)
			goto err;

		/* check for the controller idle */
		status = cad_qspi_check_idle(cad_params);
		if (status != 0)
			goto err;

		/* chip select */
		cad_qspi_cs_assert(cad_params);

		cad_params->addr_size = 4;
		/* configure address size */
		cad_qspi_configure_addr_size(cad_params);

		cad_params->proto = CAD_QSPI_WRITE_1_1_8;
		/* Setup Instr Register */
		cad_qspi_setup_write_instr(cad_params, CAD_QSPI_STIG_OPCODE_READ_OCTAL_IO_4B);

		/* Confugure mux Linear*/
		cad_qspi_linear_mux_configure(cad_params, XPMC_IOU_SLCR_BASEADDR, 1);
#if CAD_QSPI_MICRON_MT35X_SUPPORT
		status = flash_mt35x_enable(cad_params);
		if (status != 0)
			goto err;
#endif
		if (size < write_size)
			write_size = size;

		status = cad_qspi_indirect_write_config(cad_params, ofs, write_size);
		if (status != 0)
			goto err;

		status = cad_qspi_indirect_page_write(cad_params, ofs, write_data, write_size);
		if (status != 0)
			goto err;

		/* Confugure mux Linear*/
		cad_qspi_linear_mux_configure(cad_params, XPMC_IOU_SLCR_BASEADDR, 0);
#if CAD_QSPI_MICRON_MT35X_SUPPORT
		status = flash_mt35x_enable(cad_params);
		if (status != 0)
			goto err;
#endif
		/* check for the controller idle */
		status = cad_qspi_check_idle(cad_params);
		if (status != 0)
			goto err;

		/* chip select */
		cad_qspi_cs_assert(cad_params);
#if CAD_QSPI_MICRON_MT35X_SUPPORT
		status = flash_mt35x_wait_for_program_and_erase(cad_params, 0);
#else
		int count = 0;

		while (count < CAD_QSPI_COMMAND_TIMEOUT) {
			status = flash_device_status(cad_params, &status);
			if (status != 0) {
				LOG_ERR("Error getting device status\n");
				goto err;
			}
			if (!CAD_QSPI_STIG_SR_BUSY(status))
				break;
			count++;
		}

		if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
			LOG_ERR("Timed out waiting for idle\n");
			goto err;
		}
#endif

		ofs += write_size;
		write_data += write_size;
		size -= write_size;
		write_size = MIN(size, CAD_QSPI_PAGE_SIZE);
	}
err:

	/* check for the controller idle */
	cad_qspi_check_idle(cad_params);

	/* chip select */
	cad_qspi_cs_assert(cad_params);

	return status;
}

static int flash_cad_read(const struct device *dev, off_t offset,
			  void *data, size_t len)
{
	struct flash_cad_priv *priv = dev->data;
	struct cad_qspi_params *cad_params = &priv->params;
	int rc;

	if (data && len == 0) {
		return 0;
	} else if (!data || len == 0) {
		LOG_ERR("Invalid input parameter for QSPI Read!");
		return -EINVAL;
	}
	rc = flash_ospi_read(cad_params, data, (uint32_t)offset, len);
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

	rc = flash_ospi_erase(cad_params, (uint32_t)offset, len);
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

	if (!data || len == 0) {
		LOG_ERR("Invalid input parameter for QSPI Write!");
		return -EINVAL;
	}

	rc = flash_ospi_write(cad_params, (void *)data, (uint32_t)offset, len);
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

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void flash_ospi_pages_layout(const struct device *dev,
				    const struct flash_pages_layout **layout,
				    size_t *layout_size)
{
	struct flash_cad_priv *priv = dev->data;

	*layout = &priv->layout;
	*layout_size = 1;
}
#endif /* CONFIG_FLASH_PAGE_LAYOUT */

static const struct flash_driver_api flash_cad_api = {
	.erase = flash_cad_erase,
	.write = flash_cad_write,
	.read = flash_cad_read,
	.get_parameters = flash_cad_get_parameters,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = flash_ospi_pages_layout,
#endif /* CONFIG_FLASH_PAGE_LAYOUT */
};

uint32_t flash_enter_exit_4BAdd_Mode(struct cad_qspi_params *param, uint32_t enable)
{
	int status = 0, count = 0;
	uint32_t cmd;

	if (!param) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (enable)
		cmd = CAD_QSPI_STIG_OPCODE_ENTER_4B_ADDR_MODE;
	else
		cmd = CAD_QSPI_STIG_OPCODE_EXIT_4B_ADDR_MODE;

	param->addr_size = 0;
	status = flash_stig_cmd(param, CAD_QSPI_STIG_OPCODE_WREN, 0);
	if (status != 0)
		return status;

	status = flash_stig_cmd(param, cmd, 0);
	if (status != 0)
		return status;

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		status = flash_device_status(param, &status);
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
	return 0;
}

static int flash_cad_init(const struct device *dev)
{
	struct flash_cad_priv *priv = dev->data;
	struct cad_qspi_params *cad_params = &priv->params;
	uint32_t rdid;
	int rc;

	DEVICE_MMIO_NAMED_MAP(dev, qspi_reg, K_MEM_CACHE_NONE);
	DEVICE_MMIO_NAMED_MAP(dev, qspi_data, K_MEM_CACHE_NONE);

	cad_params->reg_base = DEVICE_MMIO_NAMED_GET(dev, qspi_reg);
	cad_params->data_base = DEVICE_MMIO_NAMED_GET(dev, qspi_data);

	if (cad_qspi_idle(cad_params) == 0) {
		LOG_ERR("device not idle");
		return -EBUSY;
	}

	/* Disable the controller before performing any reg writes. */
	rc = cad_qspi_disable(cad_params);
	if (rc != 0) {
		LOG_ERR("ctrl disable failure\n");
		goto err;
	}

	rc = cad_qspi_ctrl_init(cad_params);
	if (rc != 0) {
		LOG_ERR("ctrl initalization failure\n");
		goto err;
	}

	rc = cad_qspi_set_mode(cad_params, CAD_QSPI_EDGE_MODE_SDR_NON_PHY);
	if (rc != 0) {
		LOG_ERR("calibration failure\n");
		goto err;
	}

	cad_qspi_device_reset_via_ospi(cad_params);

	rc = cad_qspi_set_baudrate_div(cad_params, 0x5);
	if (rc != 0) {
		LOG_ERR("failed to set baud div\n");
		return rc;
	}

	/* Setup baudrate delays */
	cad_qspi_set_delays(cad_params);

	cad_qspi_set_chip_select(cad_params, 0);

	rc = cad_qspi_enable(cad_params);
	if (rc != 0) {
		LOG_ERR("failed enable\n");
		return rc;
	}

#if CAD_QSPI_MICRON_MT35X_SUPPORT
	rc = flash_mt35x_enable(cad_params);
	if (rc != 0) {
		goto err;
	}
#endif
	rc = flash_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &rdid);
	if (rc != 0) {
		LOG_ERR("Error reading RDID\n");
		goto err;
	}

	LOG_INF("QSPI RDID: %x", rdid);

	cad_qspi_configure_dev_size(cad_params, QSPI_ADDR_BYTES, QSPI_BYTES_PER_DEV,
				    QSPI_BYTES_PER_BLOCK);
	LOG_INF("Flash size: %d Bytes", cad_params->qspi_device_size);

	if (cad_params->qspi_device_size > SIXTEENMB) {
		rc = flash_enter_exit_4BAdd_Mode(cad_params, 1);
		if (rc != 0) {
			LOG_ERR("4-Byte addr set failure\n");
			goto err;
		}
	}

err:
	if (rc < 0) {
		LOG_ERR("Cadence QSPI Flash Init Failed");
		return rc;
	}
	return 0;
}

#define QSPI_PAGE_LAYOUT(n)					\
	.layout = {						\
		.pages_count = (DT_INST_PROP(n, size) / 8)	\
			       / CONFIG_QSPI_LAYOUT_PAGE_SIZE,	\
		.pages_size = CONFIG_QSPI_LAYOUT_PAGE_SIZE,	\
	}

#define ASSERT_WITHIN_RANGE(val, min, max, str) \
	BUILD_ASSERT(val >= min && val <= max, str);
#define ASSERT_FIFO_DEPTH_VALID(val, str) \
	BUILD_ASSERT(val == 256, str);
#define ASSERT_FIFO_WIDTH_VALID(val, str) \
	BUILD_ASSERT(val == 4, str);

#define CREATE_FLASH_CADENCE_QSPI_DEVICE(inst)				\
									\
	ASSERT_WITHIN_RANGE(DT_INST_PROP(inst, cdns_tslch_ns), 0, 255,	\
			    "Invalid clock delay value for tslch")	\
	ASSERT_WITHIN_RANGE(DT_INST_PROP(inst, cdns_tchsh_ns), 0, 255,	\
			    "Invalid clock delay value for tchsh")	\
	ASSERT_WITHIN_RANGE(DT_INST_PROP(inst, cdns_tsd2d_ns), 0, 255,	\
			    "Invalid clock delay value for tsd2d")	\
	ASSERT_WITHIN_RANGE(DT_INST_PROP(inst, cdns_tshsl_ns), 0, 255,	\
			    "Invalid clock delay value for tshsl")	\
	ASSERT_FIFO_DEPTH_VALID(DT_INST_PROP(inst, cdns_fifo_depth),	\
				"Invalid fifo depth")			\
	ASSERT_FIFO_WIDTH_VALID(DT_INST_PROP(inst, cdns_fifo_width),	\
				"Invalid fido width")			\
	static struct flash_cad_priv flash_cad_priv_##inst = {		\
		.params = {						\
			.clk_rate = DT_INST_PROP(inst, clock_frequency),\
			.data_size = DT_INST_REG_SIZE_BY_IDX(inst, 1),  \
			.fifo_depth = DT_INST_PROP(inst, cdns_fifo_depth),\
			.fifo_width = DT_INST_PROP(inst, cdns_fifo_width),\
			.tslch_ns = DT_INST_PROP(inst, cdns_tslch_ns),\
			.tchsh_ns = DT_INST_PROP(inst, cdns_tchsh_ns),\
			.tsd2d_ns = DT_INST_PROP(inst, cdns_tsd2d_ns),\
			.tshsl_ns = DT_INST_PROP(inst, cdns_tshsl_ns),\
			.qspi_device_size = DT_INST_PROP(inst, size),\
		},							\
		IF_ENABLED(CONFIG_FLASH_PAGE_LAYOUT,			\
			(QSPI_PAGE_LAYOUT(inst),))			\
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
