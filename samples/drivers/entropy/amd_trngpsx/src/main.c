/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/sys/printk.h>

#define ENTROPY_SAMPLE_BYTES 32U

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
	uint8_t *buf;
	int ret;

	buf = k_malloc(ENTROPY_SAMPLE_BYTES);
	if (buf == NULL) {
		printk("failed to allocate entropy buffer\n");
		return 0;
	}

	if (!device_is_ready(dev)) {
		printk("entropy device not ready\n");
		k_free(buf);
		return 0;
	}

	printk("entropy device ready: %s\n", dev->name);

	ret = entropy_get_entropy(dev, buf, ENTROPY_SAMPLE_BYTES);
	if (ret < 0) {
		printk("entropy_get_entropy failed (%d)\n", ret);
		k_free(buf);
		return 0;
	}

	printk("random bytes:");
	for (size_t i = 0; i < ENTROPY_SAMPLE_BYTES; i++) {
		printk(" %02x", buf[i]);
	}
	printk("\ndone\n");

	k_free(buf);

	return 0;
}
