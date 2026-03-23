/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc. (AMD)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/net/net_if.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/ethernet/eth_xilinx_axienet.h>

#define AXIENET_BD_COUNT_TX_MAX 512
#define AXIENET_BD_COUNT_RX_MAX 512

static void print_help(const struct shell *shell)
{
	shell_print(shell,
		"Usage:\n"
		"  net axienet_bd_config help\n"
		"      Show this help message.\n"
		"\n"
		"  net axienet_bd_config <iface_idx>\n"
		"      Get current TX/RX BD counts\n"
		"\n"
		"  net axienet_bd_config <iface_idx> <num_tx> <num_rx>\n"
		"      Set new TX/RX BD counts\n"
		"\n"
		"Workflow:\n"
		"  1. net iface down <iface_idx>\n"
		"  2. net axienet_bd_config <iface_idx> <num_tx> <num_rx>\n"
		"  3. net iface up <iface_idx>\n"
		"\n"
		"Examples:\n"
		"  net axienet_bd_config 1          -- get current TX/RX BD counts\n"
		"  net axienet_bd_config 1 32 64    -- set 32 TX + 64 RX BDs\n"
		"\n"
		"NOTE: TX BDs must be in [1..512], RX BDs must be in [1..512].\n"
		"      Ensure CONFIG_HEAP_MEM_POOL_SIZE is large enough for the\n"
		"      chosen TX/RX BD counts.");
}

static int cmd_axienet_bd_config(const struct shell *shell, size_t argc, char **argv)
{
	/* No args or "help" : print help */
	if (argc < 2 || (argc == 2 && strcmp(argv[1], "help") == 0)) {
		print_help(shell);
		return 0;
	}

	int iface_idx = atoi(argv[1]);
	struct net_if *iface = net_if_get_by_index(iface_idx);

	if (!iface) {
		shell_error(shell, "Invalid interface index %d", iface_idx);
		return -EINVAL;
	}

	const struct device *dev = net_if_get_device(iface);

	/* Query mode: one positional arg (iface_idx only) */
	if (argc == 2) {
		size_t num_tx, num_rx;

		xilinx_axienet_get_bd_counts(dev, &num_tx, &num_rx);
		shell_print(shell,
			"Interface %d (%s): TX BDs = %zu, RX BDs = %zu  [%s]",
			iface_idx, dev->name, num_tx, num_rx,
			net_if_is_up(iface) ? "UP" : "DOWN");
		return 0;
	}

	/* Set mode: requires iface_idx + num_tx + num_rx */
	if (argc < 4) {
		shell_error(shell,
			"Usage: net axienet_bd_config <iface_idx> <num_tx> <num_rx>");
		shell_error(shell, "Run 'net axienet_bd_config help' for details.");
		return -EINVAL;
	}

	if (net_if_is_up(iface)) {
		shell_warn(shell,
			"Interface %d is UP. Bring it down:",
			iface_idx);
		shell_warn(shell, "net iface down %d", iface_idx);
		shell_warn(shell,
			"Then: net axienet_bd_config %d <num_tx> <num_rx>",
			iface_idx);
		return -EBUSY;
	}

	size_t num_tx = (size_t)atoi(argv[2]);
	size_t num_rx = (size_t)atoi(argv[3]);

	if (num_tx > AXIENET_BD_COUNT_TX_MAX || num_rx > AXIENET_BD_COUNT_RX_MAX) {
		shell_warn(shell,
			"TX BDs must be <= %d and RX BDs must be <= %d (got tx=%zu rx=%zu)",
			AXIENET_BD_COUNT_TX_MAX, AXIENET_BD_COUNT_RX_MAX, num_tx, num_rx);
		return -EINVAL;
	}

	int ret = xilinx_axienet_set_bd_counts(dev, num_tx, num_rx);

	if (ret) {
		shell_error(shell, "Failed to set BD counts: %d", ret);
		return ret;
	}

	shell_print(shell,
		"BD counts set: tx=%zu rx=%zu. Run 'net iface up %d' to apply.",
		num_tx, num_rx, iface_idx);
	return 0;
}

SHELL_SUBCMD_ADD((net), axienet_bd_config, NULL,
	"AXI Ethernet TX/RX BD config",
	cmd_axienet_bd_config, 0, 3);
