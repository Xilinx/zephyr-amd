# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""
West extension: run Lopper against a system device tree (SDT) and refresh
Zephyr board DTS / Kconfig under a Zephyr tree.
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Dict, List, Optional, Union, Any
import yaml

import lopper
from west.commands import WestCommand

class ColoredFormatter(logging.Formatter):
    """
    Custom logging formatter with color coding for different log levels.

    Features:
    - Color-coded log levels (DEBUG=cyan, INFO=green, WARNING=yellow, ERROR=red, CRITICAL=magenta)
    - Automatic color detection based on terminal capabilities
    - Environment variable control:
        * NO_COLOR=1 or LOPPER_NO_COLOR=1 - disable colors
        * FORCE_COLOR=1 or LOPPER_FORCE_COLOR=1 - force colors
    """

    # ANSI color codes
    COLORS = {
        'DEBUG': '\033[36m',     # Cyan
        'INFO': '\033[32m',      # Green
        'WARNING': '\033[33m',   # Yellow
        'ERROR': '\033[31m',     # Red
        'CRITICAL': '\033[35m',  # Magenta
        'RESET': '\033[0m'       # Reset color
    }

    def __init__(self, use_colors=None):
        super().__init__()
        # Auto-detect if colors should be used
        if use_colors is None:
            # Check environment variables first
            no_color = os.getenv('NO_COLOR') or os.getenv('LOPPER_NO_COLOR')
            force_color = os.getenv('FORCE_COLOR') or os.getenv('LOPPER_FORCE_COLOR')

            if no_color:
                self.use_colors = False
            elif force_color:
                self.use_colors = True
            else:
                # Auto-detect: use colors if stdout is a terminal
                self.use_colors = hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()
        else:
            self.use_colors = use_colors

    def format(self, record):
        if self.use_colors:
            # Get the color for this log level
            color = self.COLORS.get(record.levelname, self.COLORS['RESET'])
            reset = self.COLORS['RESET']

            # Create the formatted message with color
            log_prefix = f"{color}{record.levelname}{reset}"
        else:
            # No colors, just use the level name
            log_prefix = record.levelname

        # Format the record
        formatter = logging.Formatter(f'{log_prefix}: %(message)s')
        return formatter.format(record)


def setup_colored_logging():
    """Set up colored logging for the application."""
    # Create logger
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)

    # Remove any existing handlers
    for handler in logger.handlers[:]:
        logger.removeHandler(handler)

    # Create console handler with colored formatter
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.INFO)

    # Set the colored formatter
    colored_formatter = ColoredFormatter()
    console_handler.setFormatter(colored_formatter)

    # Add handler to logger
    logger.addHandler(console_handler)

    return logger


# Configure colored logging
logger = setup_colored_logging()

def fetch_yaml_data(config_file: Union[str, Path], dir_type: str) -> Dict[str, Any]:
    """
    Reads data from a YAML configuration file.

    Args:
        config_file: Path to the YAML configuration file
        dir_type: Description used in error messages for context

    Returns:
        Dict containing the parsed YAML data

    Raises:
        FileNotFoundError: If the configuration file doesn't exist
        yaml.YAMLError: If the YAML file is malformed
    """
    config_path = Path(config_file)
    if not config_path.is_file():
        raise FileNotFoundError(
            f"Could not find valid {dir_type} at {config_path.parent}"
        )

    logger.info(f"Loading configuration file: {config_path}")

    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
        return data
    except yaml.YAMLError as e:
        logger.error(f"Failed to parse YAML file {config_path}: {e}")
        raise

def runcmd(cmd: str, cwd: Optional[Union[str, Path]] = None,
           logfile: Optional[Union[str, Path]] = None) -> bool:
    """
    Execute shell commands with proper error handling.

    Args:
        cmd: Shell command to execute
        cwd: Working directory for the command
        logfile: File path to save command output (optional)

    Returns:
        bool: True if command succeeded, False otherwise

    Raises:
        SystemExit: If command fails and no logfile is specified
    """
    logger.info(f"Executing command: {cmd}")
    if cwd:
        logger.info(f"Working directory: {cwd}")

    try:
        if logfile is None:
            subprocess.check_call(cmd, cwd=cwd, shell=True)
        else:
            with open(logfile, 'w', encoding='utf-8') as log:
                subprocess.check_call(
                    cmd, cwd=cwd, shell=True, stdout=log, stderr=log
                )
        logger.info("Command executed successfully")
        return True
    except subprocess.CalledProcessError as e:
        logger.error(f"Command failed with return code {e.returncode}: {cmd}")
        if logfile is None:
            sys.exit(1)
        return False

class LopperCommand(WestCommand):
    """West command for running Lopper to generate device tree files and configurations."""

    def __init__(self):
        super().__init__(
            'lopper-command',
            # Keep in sync with scripts/west-commands.yml (help string).
            'generate Zephyr DTS from a system device tree (Lopper)',
            textwrap.dedent('''
            Runs Lopper on a Xilinx/AMD system device tree and writes generated
            DTS (and related files) into the Zephyr repository given by -w/--ws_dir.

            Workspace layout:
              -ws_dir must be the Zephyr manifest project directory (the folder
              that contains west.yml). Generated files are placed under that tree
              (for example boards/ and soc/). The west workspace is expected to
              have the ``lopper`` project next to zephyr (sibling directory).

            Current working directory:
              You may run this command from any directory. If the lopper revision
              in west.yml differs from the checked-out lopper project, this command
              runs ``west update lopper`` and ``west lopper-install`` with -w as the
              working directory for those nested commands (so you do not need to
              ``cd`` into zephyr first).

            Default for -w is ./zephyr/ relative to the shell's current directory;
            from the workspace root that is typically the zephyr repo. If your
            shell is already inside the zephyr repo, pass -w . (or an absolute path).
            ''').strip())

    def do_add_parser(self, parser_adder):
        """Add command-line argument parser."""
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description=self.description,
            epilog=textwrap.dedent('''
            examples:
              From workspace root (zephyr and lopper are subdirectories):

                west lopper-command -p <processor> -s /path/to/system-top.dts

              From any directory, with an explicit Zephyr path:

                west lopper-command -w /path/to/zephyr -p <processor> -s /path/to/system-top.dts

              Already cd'd into the zephyr repository:

                west lopper-command -w . -p <processor> -s /path/to/system-top.dts
            ''').strip(),
        )

        required_args = parser.add_argument_group("Required arguments")
        required_args.add_argument(
            "-p", "--proc",
            required=True,
            help="processor/domain name as used in the SDT (must match a key in the cpulist for this SDT)",
        )
        required_args.add_argument(
            "-s", "--sdt",
            required=True,
            help="path to system-top.dts (SDT root file passed to Lopper)",
        )

        parser.add_argument(
            "-b", "--board_dts",
            help="optional path to a board .dts file used as the Zephyr board source for overlay generation",
        )
        parser.add_argument(
            "-w", "--ws_dir",
            default="./zephyr/",
            help="Zephyr repository root: directory that contains west.yml (default: ./zephyr/ relative to cwd)",
        )

        return parser

    def _setup_workspace(self, ws_dir: str) -> Path:
        """
        Set up the workspace directory for lopper operations.

        Args:
            ws_dir: Base workspace directory

        Returns:
            Path to the lopper metadata directory
        """
        workspace = Path(ws_dir).resolve() / "lopper_metadata"
        workspace.mkdir(parents=True, exist_ok=True)
        logger.info(f"Workspace directory: {workspace}")
        return workspace

    def _validate_processor(self, proc: str, workspace: Path, sdt: Path) -> str:
        """
        Validate the processor name against available processors in the SDT.

        Args:
            proc: Processor name to validate
            workspace: Workspace directory
            sdt: System device tree path

        Returns:
            Processor IP name

        Raises:
            SystemExit: If processor is not valid
        """
        # Generate CPU list
        runcmd(f"lopper --werror -f -O {workspace} -i lop-cpulist.dts {sdt}", cwd=workspace)

        cpu_list_file = workspace / "cpulist.yaml"
        avail_cpu_data = fetch_yaml_data(cpu_list_file, "cpulist")

        if proc not in avail_cpu_data:
            available_procs = list(avail_cpu_data.keys())
            logger.error(
                f"Invalid processor name '{proc}'. "
                f"Valid processors for the given SDT: {available_procs}"
            )
            sys.exit(1)

        return avail_cpu_data[proc]

    def _process_microblaze_riscv(self, workspace: Path, ws_dir: str, sdt: Path,
                                  proc: str, lops_dir: Path,
                                  zephyr_board_dts: Optional[Path]) -> None:
        """Process MicroBlaze RISC-V processor."""
        logger.info("Processing MicroBlaze RISC-V processor")

        # Define file paths
        paths = {
            'generated_dts': workspace / "mbv32.dts",
            'workspace_dts': Path(ws_dir) / "boards" / "amd" / "mbv32" / "mbv32.dts",
            'workspace_kconfig_defconfig': Path(ws_dir) / "soc" / "xlnx" / "mbv32" / "Kconfig.defconfig",
            'generated_kconfig_defconfig': workspace / "Kconfig.defconfig",
            'workspace_kconfig_soc': Path(ws_dir) / "soc" / "xlnx" / "mbv32" / "Kconfig",
            'generated_kconfig_soc': workspace / "Kconfig",
            'generated_board_dt': workspace / "board.overlay",
            'workspace_board_overlay': Path(ws_dir) / "boards" / "amd" / "mbv32" / "mbv32.overlay",
            'workspace_board_defconfig': Path(ws_dir) / "boards" / "amd" / "mbv32" / "Kconfig.defconfig",
            'generated_board_defconfig': workspace / "board_Kconfig.defconfig"
        }

        lops_file = lops_dir / "lop-microblaze-riscv.dts"
        lops_file_intc = lops_dir / "lop-mbv-zephyr-intc.dts"

        # Run lopper commands
        commands = [
            f"lopper -f --enhanced -O {workspace} -i {lops_file} {sdt} {workspace}/system-domain.dts -- gen_domain_dts {proc}",
            f"lopper -f --enhanced -O {workspace} -i {lops_file} {workspace}/system-domain.dts {workspace}/system-zephyr.dts -- gen_domain_dts {proc} zephyr_dt {zephyr_board_dts}",
            f"lopper -f --enhanced -O {workspace} -i {lops_file_intc} {workspace}/system-zephyr.dts {workspace}/mbv32.dts"
        ]

        for cmd in commands:
            runcmd(cmd, cwd=workspace)

        # Copy generated files to workspace
        self._copy_files([
            (paths['generated_dts'], paths['workspace_dts']),
            (paths['generated_kconfig_defconfig'], paths['workspace_kconfig_defconfig']),
            (paths['generated_kconfig_soc'], paths['workspace_kconfig_soc']),
            (paths['generated_board_defconfig'], paths['workspace_board_defconfig'])
        ])

        if zephyr_board_dts:
            self._copy_files([(paths['generated_board_dt'], paths['workspace_board_overlay'])])

    def _process_cortexr52(self, workspace: Path, ws_dir: str, sdt: Path,
                          proc: str, lops_dir: Path,
                          zephyr_board_dts: Optional[Path]) -> None:
        """Process Cortex-R52 processor."""
        logger.info("Processing Cortex-R52 processor")

        lops_file = lops_dir / "lop-r52-imux.dts"

        if "psx_cortexr52" in proc:
            workspace_dts = Path(ws_dir) / "boards" / "amd" / "versalnet_rpu" / "versalnet_rpu.dts"
            workspace_overlay = Path(ws_dir) / "boards" / "amd" / "versalnet_rpu" / "versalnet_rpu.overlay"
        else:
            workspace_dts = Path(ws_dir) / "boards" / "amd" / "versal2_rpu" / "versal2_rpu.dts"
            workspace_overlay = Path(ws_dir) / "boards" / "amd" / "versal2_rpu" / "versal2_rpu.overlay"

        # Run lopper commands
        commands = [
            f"lopper -f --enhanced -O {workspace} {sdt} {workspace}/system-domain.dts -- gen_domain_dts {proc}",
            f"lopper -f --enhanced -O {workspace} -i {lops_file} {workspace}/system-domain.dts {workspace}/system-imux.dts",
            f"lopper -f --enhanced -O {workspace} {workspace}/system-imux.dts {workspace_dts} -- gen_domain_dts {proc} zephyr_dt {zephyr_board_dts}"
        ]

        for cmd in commands:
            runcmd(cmd, cwd=workspace)

        if zephyr_board_dts:
            self._copy_files([((os.path.join(workspace, "board.overlay")), workspace_overlay)])

    def _process_cortexa72(self, workspace: Path, ws_dir: str, sdt: Path,
                          proc: str, lops_dir: Path,
                          zephyr_board_dts: Optional[Path]) -> None:
        """Process Cortex-A72 processor."""
        logger.info("Processing Cortex-A72 processor")

        lops_file = lops_dir / "lop-a72-imux.dts"
        workspace_dts = Path(ws_dir) / "boards" / "amd" / "versal_apu" / "versal_apu.dts"
        workspace_overlay = Path(ws_dir) / "boards" / "amd" / "versal_apu" / "versal_apu.overlay"

        # Run lopper commands
        commands = [
            f"lopper -f --enhanced -O {workspace} {sdt} {workspace}/system-domain.dts -- gen_domain_dts {proc}",
            f"lopper -f --enhanced -O {workspace} -i {lops_file} {workspace}/system-domain.dts {workspace}/system-imux.dts",
            f"lopper -f --enhanced -O {workspace} {workspace}/system-imux.dts {workspace_dts} -- gen_domain_dts {proc} zephyr_dt {zephyr_board_dts}"
        ]

        for cmd in commands:
            runcmd(cmd, cwd=workspace)

        if zephyr_board_dts:
            self._copy_files([((os.path.join(workspace, "board.overlay")), workspace_overlay)])

    def _process_cortexr5(self, workspace: Path, ws_dir: str, sdt: Path,
                          proc: str, lops_dir: Path,
                          zephyr_board_dts: Optional[Path]) -> None:
        """Process Cortex-R5 processor."""
        logger.info("Processing Cortex-R5 processor")

        lops_file = lops_dir / "lop-r5-imux.dts"
        workspace_dts = Path(ws_dir) / "boards" / "amd" / "versal_rpu" / "versal_rpu.dts"
        workspace_overlay = Path(ws_dir) / "boards" / "amd" / "versal_rpu" / "versal_rpu.overlay"

        # Run lopper commands
        commands = [
            f"lopper -f --enhanced -O {workspace} {sdt} {workspace}/system-domain.dts -- gen_domain_dts {proc}",
            f"lopper -f --enhanced -O {workspace} -i {lops_file} {workspace}/system-domain.dts {workspace}/system-imux.dts",
            f"lopper -f --enhanced -O {workspace} {workspace}/system-imux.dts {workspace_dts} -- gen_domain_dts {proc} zephyr_dt {zephyr_board_dts}"
        ]

        for cmd in commands:
            runcmd(cmd, cwd=workspace)

        if zephyr_board_dts:
            self._copy_files([((os.path.join(workspace, "board.overlay")), workspace_overlay)])

    def _process_cortexa78(self, workspace: Path, ws_dir: str, sdt: Path,
                          proc: str, lops_dir: Path,
                          zephyr_board_dts: Optional[Path]) -> None:
        """Process Cortex-A78 processor."""
        logger.info("Processing Cortex-A78 processor")

        lops_file = lops_dir / "lop-a78-imux.dts"

        if "psx_cortexa78" in proc:
            workspace_dts = Path(ws_dir) / "boards" / "amd" / "versalnet_apu" / "versalnet_apu.dts"
            workspace_overlay = Path(ws_dir) / "boards" / "amd" / "versalnet_apu" / "versalnet_apu.overlay"
        else:
            workspace_dts = Path(ws_dir) / "boards" / "amd" / "versal2_apu" / "versal2_apu.dts"
            workspace_overlay = Path(ws_dir) / "boards" / "amd" / "versal2_apu" / "versal2_apu.overlay"

        # Run lopper commands
        commands = [
            f"lopper -f --enhanced -O {workspace} {sdt} {workspace}/system-domain.dts -- gen_domain_dts {proc}",
            f"lopper -f --enhanced -O {workspace} -i {lops_file} {workspace}/system-domain.dts {workspace}/system-imux.dts",
            f"lopper -f --enhanced -O {workspace} {workspace}/system-imux.dts {workspace_dts} -- gen_domain_dts {proc} zephyr_dt {zephyr_board_dts}"
        ]

        for cmd in commands:
            runcmd(cmd, cwd=workspace)

        if zephyr_board_dts:
            self._copy_files([((os.path.join(workspace, "board.overlay")), workspace_overlay)])

    def _copy_files(self, file_pairs: List[tuple]) -> None:
        """
        Copy files from source to destination.

        Args:
            file_pairs: List of (source, destination) tuples
        """
        for src, dst in file_pairs:
            try:
                # Ensure destination directory exists
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
                logger.info(f"Copied {src} -> {dst}")
            except Exception as e:
                logger.error(f"Failed to copy {src} to {dst}: {e}")
                raise

    def _get_lopper_expected_rev(self, zephyr_dir: Path) -> Optional[str]:
        """Read the lopper revision from west.yml."""
        west_yml = zephyr_dir / "west.yml"
        if not west_yml.is_file():
            logger.warning(f"west.yml not found at {west_yml}, skipping srcrev check")
            return None
        try:
            with open(west_yml, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f)
            projects = data.get('manifest', {}).get('projects', [])
            for proj in projects:
                if proj.get('name') == 'lopper':
                    return proj.get('revision')
        except Exception as e:
            logger.warning(f"Failed to parse west.yml: {e}")
        return None

    def _get_lopper_actual_rev(self, lopper_dir: Path) -> Optional[str]:
        """Get the current git HEAD commit of the lopper directory."""
        if not lopper_dir.is_dir():
            return None
        try:
            result = subprocess.run(
                ['git', 'rev-parse', 'HEAD'],
                cwd=lopper_dir,
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except Exception as e:
            logger.warning(f"Failed to get lopper git HEAD: {e}")
        return None

    def _ensure_lopper_up_to_date(self, zephyr_dir: Path) -> None:
        """Run west update and lopper-install only if lopper srcrev has changed."""
        if not zephyr_dir.is_dir():
            logger.error(f"Zephyr workspace path is not a directory: {zephyr_dir}")
            sys.exit(1)
        if not (zephyr_dir / "west.yml").is_file():
            logger.error(
                f"west.yml not found under {zephyr_dir}. "
                "Use -w/--ws_dir with the Zephyr repository root (manifest project)."
            )
            sys.exit(1)

        lopper_dir = (zephyr_dir / ".." / "lopper").resolve()

        expected_rev = self._get_lopper_expected_rev(zephyr_dir)
        actual_rev = self._get_lopper_actual_rev(lopper_dir)

        if expected_rev and actual_rev and expected_rev == actual_rev:
            logger.info(f"Lopper is already at required revision {expected_rev[:12]}, skipping west update")
            return

        if expected_rev and actual_rev:
            logger.info(f"Lopper revision mismatch: expected {expected_rev[:12]}, got {actual_rev[:12]}")
        else:
            logger.info("Could not determine lopper revision, running west update to be safe")

        logger.info("Running west update")
        runcmd("west update lopper", cwd=zephyr_dir)
        logger.info("Running west lopper-install")
        runcmd("west lopper-install", cwd=zephyr_dir)

    def do_run(self, args, unknown_args) -> None:
        """Main execution method for the lopper command."""
        try:
            logger.info("Starting lopper command execution")

            # Ensure lopper is at the revision specified in west.yml
            zephyr_dir = Path(args.ws_dir).resolve()
            self._ensure_lopper_up_to_date(zephyr_dir)

            # Validate and prepare paths
            sdt = Path(args.sdt).resolve()
            if not sdt.exists():
                logger.error(f"System device tree file not found: {sdt}")
                sys.exit(1)

            proc = args.proc
            ws_dir = Path(args.ws_dir).resolve()

            # Set up workspace
            workspace = self._setup_workspace(str(ws_dir))

            # Find board overlay if board is specified
            zephyr_board_dts = None
            if args.board_dts:
                zephyr_board_dts = Path(args.board_dts).resolve()
                if not zephyr_board_dts.exists():
                    logger.error(f" board dts file not found: {zephyr_board_dts}")
                    sys.exit(1)
            else:
                logger.warning("board dts not specified, proceeding without board overlay")

            # Get lopper operations directory
            lops_dir = Path(lopper.__file__).parent / "lops"
            if not lops_dir.exists():
                logger.error(f"Lopper operations directory not found: {lops_dir}")
                sys.exit(1)

            # Validate processor and get IP name
            logger.info(f"Validating processor: {proc}")
            proc_ip_name = self._validate_processor(proc, workspace, sdt)
            logger.info(f"Processor IP name identified: {proc_ip_name}")

            # Process based on processor type
            if "microblaze_riscv" in proc_ip_name:
                self._process_microblaze_riscv(workspace, str(ws_dir), sdt, proc, lops_dir, zephyr_board_dts)
                # Clean up workspace after successful processing
                shutil.rmtree(workspace)
                logger.info("MicroBlaze RISC-V processing completed successfully")
            elif "cortexr52" in proc_ip_name:
                self._process_cortexr52(workspace, str(ws_dir), sdt, proc, lops_dir, zephyr_board_dts)
                # Clean up workspace after successful processing
                shutil.rmtree(workspace)
                logger.info("Cortex-R52 processing completed successfully")
            elif "cortexr5" in proc_ip_name:
                self._process_cortexr5(workspace, str(ws_dir), sdt, proc, lops_dir, zephyr_board_dts)
                # Clean up workspace after successful processing
                shutil.rmtree(workspace)
                logger.info("Cortex-R5 processing completed successfully")
            elif "cortexa78" in proc_ip_name:
                self._process_cortexa78(workspace, str(ws_dir), sdt, proc, lops_dir, zephyr_board_dts)
                # Clean up workspace after successful processing
                shutil.rmtree(workspace)
                logger.info("Cortex-A78 processing completed successfully")
            elif "cortexa72" in proc_ip_name:
                self._process_cortexa72(workspace, str(ws_dir), sdt, proc, lops_dir, zephyr_board_dts)
                # Clean up workspace after successful processing
                shutil.rmtree(workspace)
                logger.info("Cortex-A72 processing completed successfully")
            else:
                logger.error(f"Unsupported processor type: {proc_ip_name}")
                sys.exit(1)

            logger.info("Lopper command execution completed successfully")

        except Exception as e:
            logger.error(f"Command execution failed: {e}")
            sys.exit(1)

