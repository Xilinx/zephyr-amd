# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import subprocess
import sys
import lopper
import os
import shutil
from west.commands import WestCommand

def get_dir_path(fpath):
    """
    This api takes file path and returns it's directory path

    Args:
        fpath: Path to get the directory path from.
    Returns:
        string: Full Directory path of the passed path
    """
    return os.path.dirname(fpath.rstrip(os.path.sep))

def runcmd(cmd, cwd=None, logfile=None) -> bool:
    """
    Run the shell commands.

    Args:
        | cmd: shell command that needs to be called
        | logfile: file to save the command output if required
    """
    ret = True
    if logfile is None:
        try:
            subprocess.check_call(cmd, cwd=cwd, shell=True)
        except subprocess.CalledProcessError as exc:
            ret = False
            sys.exit(1)
    else:
        try:
            subprocess.check_call(cmd, cwd=cwd, shell=True, stdout=logfile, stderr=logfile)
        except subprocess.CalledProcessError:
            ret = False
    return ret

class LopperCommand(WestCommand):
    def __init__(self):
        super().__init__(
            'lopper-command',  # The name of the command
            'Install lopper dependencies and run lopper commands',  # Help text for the command
            'This command runs lopper commands based on user inputs.'
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name, help=self.help)
        required_argument = parser.add_argument_group("Required arguments")
        required_argument.add_argument(
            "-p",
            "--proc",
            action="store",
            help="Specify the processor name",
            required=True,
        )
        required_argument.add_argument(
            "-s",
            "--sdt",
            action="store",
            help="Specify the System device-tree path (till system-top.dts file)",
            required=True,
        )
        parser.add_argument(
            "-w",
            "--ws_dir",
            action="store",
            help="Workspace directory (zephyr repository path) where domain will be created (Default: zephyr directory in Current Work Directory)",
            default='./zephyr/',
        )

        return parser

    def do_run(self, args, unknown_args):
        sdt = os.path.abspath(args.sdt)
        proc = args.proc
        workspace = os.path.abspath(args.ws_dir)
        workspace = os.path.join(os.path.abspath(args.ws_dir), "lopper_metadata")
        if not os.path.exists(workspace):
            os.makedirs(workspace)
        generated_dts_file = os.path.join(workspace, "mbv32.dts")
        workspace_dts_file = os.path.join(os.path.abspath(args.ws_dir), "boards", "amd", "mbv32", "mbv32.dts")
        workspace_kconfig_defconfig = os.path.join(os.path.abspath(args.ws_dir), "soc", "xlnx", "mbv32", "Kconfig.defconfig")
        generated_kconfig_defconfig = os.path.join(workspace, "Kconfig.defconfig")
        workspace_kconfig_soc = os.path.join(os.path.abspath(args.ws_dir), "soc", "xlnx", "mbv32", "Kconfig")
        generated_kconfig_soc = os.path.join(workspace, "Kconfig")
        lops_dir = os.path.join(get_dir_path(lopper.__file__), "lops")


        lops_file = os.path.join(lops_dir, "lop-microblaze-riscv.dts")
        lops_file_intc = os.path.join(lops_dir, "lop-mbv-zephyr-intc.dts")
        runcmd(f"lopper -f --enhanced -O {workspace} -i {lops_file} {sdt} {workspace}/system-domain.dts -- gen_domain_dts {proc}",
                cwd = workspace)
        runcmd(f"lopper -f --enhanced -O {workspace} -i {lops_file} {workspace}/system-domain.dts {workspace}/system-zephyr.dts -- gen_domain_dts {proc} zephyr_dt",
                cwd = workspace)
        runcmd(f"lopper -f --enhanced -O {workspace} -i {lops_file_intc}  {workspace}/system-zephyr.dts  {workspace}/mbv32.dts",
                 cwd = workspace)

        shutil.copy(generated_dts_file, workspace_dts_file)
        shutil.copy(generated_kconfig_defconfig, workspace_kconfig_defconfig)
        shutil.copy(generated_kconfig_soc, workspace_kconfig_soc)
        shutil.rmtree(workspace)

