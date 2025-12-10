#!/usr/bin/env python3
import argparse
import subprocess
import yaml
import sys
import os
import tempfile
import platform as platform_module
from datetime import datetime


def get_host_os():
    return platform_module.system()


def get_host_architecture():
    return platform_module.machine()


def is_macos():
    return get_host_os() == "Darwin"


def is_arm64():
    return get_host_architecture() in ("arm64", "aarch64")


def _parse_platform_string(value, separator):
    """Parse a string into a list of platform names, handling both delimiters and empty values."""
    if not value:
        return None
    # Use separator as primary delimiter, then split on whitespace
    platforms = [p.strip() for p in value.replace(separator, ' ').split() if p.strip()]
    return platforms if platforms else None


def resolve_platforms_to_run(cli_platforms=None, env_platforms=None, config_file=None):
    """
    Resolve platforms to run with priority hierarchy.

    Priority:
    1. CLI arguments (comma or space-separated)
    2. Environment variable PCP_CI_QUICK_PLATFORMS
    3. Config file .pcp-ci-quick (line-separated, comments ignored)
    4. None - requires user to specify platforms

    Args:
        cli_platforms: String of platforms from command line
        env_platforms: String of platforms from environment variable
        config_file: Path to config file (default: .pcp-ci-quick)

    Returns:
        List of platform names, or None if no platforms found
    """
    if config_file is None:
        config_file = ".pcp-ci-quick"

    # Priority 1: CLI arguments
    platforms = _parse_platform_string(cli_platforms, ',')
    if platforms:
        return platforms

    # Priority 2: Environment variable
    platforms = _parse_platform_string(env_platforms, ',')
    if platforms:
        return platforms

    # Priority 3: Config file
    if os.path.isfile(config_file):
        try:
            with open(config_file, encoding="utf-8") as f:
                platforms = [line.strip() for line in f if line.strip() and not line.strip().startswith('#')]
                if platforms:
                    return platforms
        except (IOError, OSError) as e:
            print(f"Warning: Could not read config file {config_file}: {e}", file=sys.stderr)

    return None


def print_quick_mode_help():
    """Print helpful error message for quick mode."""
    print("\nError: No platforms specified for --quick mode.", file=sys.stderr)
    print("\nPlease specify platforms using one of these methods:\n", file=sys.stderr)
    print("  1. Command line (comma or space-separated):", file=sys.stderr)
    print("     python3 build/ci/ci-run.py --quick ubuntu2404-container,fedora43-container reproduce\n", file=sys.stderr)
    print("  2. Environment variable:", file=sys.stderr)
    print("     export PCP_CI_QUICK_PLATFORMS='ubuntu2404-container fedora43-container'", file=sys.stderr)
    print("     python3 build/ci/ci-run.py --quick reproduce\n", file=sys.stderr)
    print("  3. Config file (.pcp-ci-quick in repo root):", file=sys.stderr)
    print("     ubuntu2404-container", file=sys.stderr)
    print("     fedora43-container", file=sys.stderr)
    print("     centos-stream10-container", file=sys.stderr)
    print("     python3 build/ci/ci-run.py --quick reproduce\n", file=sys.stderr)


def _execute_command(runner, args, platform_name=None):
    """
    Execute the appropriate command on the runner.

    Args:
        runner: The runner instance (ContainerRunner, etc.)
        args: Parsed command-line arguments
        platform_name: Optional platform name for logging (used in quick mode)
    """
    try:
        if args.main_command == "setup":
            runner.setup(args.pcp_path)
            runner.task("setup")
        elif args.main_command == "destroy":
            runner.destroy()
        elif args.main_command == "task":
            runner.task(args.task_name)
        elif args.main_command == "artifacts":
            runner.task(f"copy_{args.artifact}_artifacts")
            runner.get_artifacts(args.artifact, args.path)
        elif args.main_command == "exec":
            runner.exec(" ".join(args.command), check=False)
        elif args.main_command == "shell":
            runner.shell()
        elif args.main_command == "reproduce":
            all_tasks = list(runner.platform["tasks"].keys())
            if args.until not in all_tasks:
                print(f"Error: Unknown task '{args.until}'. Available tasks: {', '.join(all_tasks)}", file=sys.stderr)
                sys.exit(1)
            run_tasks = all_tasks[: all_tasks.index(args.until) + 1]

            if platform_name:
                # In quick mode, shorten the message
                print(f"[{platform_name}] Running tasks: {', '.join(run_tasks)}")
            else:
                print("Preparing a new virtual environment with PCP preinstalled, this will take about 20 minutes...")

            started = datetime.now()
            runner.setup(args.pcp_path)
            for task in run_tasks:
                print(f"\n[{platform_name if platform_name else 'CI'}] Running task {task}...")
                runner.task(task)
            duration_min = (datetime.now() - started).total_seconds() / 60
            print(f"\n[{platform_name if platform_name else 'CI'}] Tasks completed, took {duration_min:.0f}m.")

            if not platform_name:  # Only show in non-quick mode
                if "install" in all_tasks and all_tasks.index(args.until) >= all_tasks.index("install"):
                    print("\nPlease run:\n")
                    print("    sudo -u pcpqa -i ./check XXX\n")
                    print("to run a QA test. PCP is already installed, from sources located in './pcp'.")
                print("Starting a shell in the new virtual environment...\n")
                runner.shell()
        else:
            print(f"Error: Unknown command {args.main_command}", file=sys.stderr)
            sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"Error on {platform_name or 'command'}: {e}", file=sys.stderr)
        # In quick mode, continue to next platform instead of exiting
        if platform_name:
            return
        sys.exit(1)


class DirectRunner:
    def __init__(self, platform_name: str, platform):
        self.platform_name = platform_name
        self.platform = platform
        # example: /tmp/pcp-ci-ubuntu2004
        self.build_dir = os.path.join(tempfile.gettempdir(), f"pcp-ci-{self.platform_name}")
        self.command_preamble = "set -eux\nexport runner=direct\n"

    def setup(self, pcp_path):
        os.mkdir(self.build_dir)
        # copy sources to a temp folder to not mess up the source dir
        subprocess.run(["rsync", "-a", f"{pcp_path}/", f"{self.build_dir}/pcp/"], check=True)
        self.exec("mkdir -p ../artifacts/build ../artifacts/test")

    def destroy(self):
        pass

    def exec(self, command, check=True):
        command = self.command_preamble + command
        subprocess.run(["bash", "-"], cwd=f"{self.build_dir}/pcp", input=command.encode(), check=check)

    def shell(self):
        pass

    def task(self, task_name):
        self.exec(self.platform["tasks"][task_name])

    def get_artifacts(self, artifact, path):
        subprocess.run(
            [
                "rsync",
                "-a",
                f"{self.build_dir}/artifacts/{artifact}/",
                f"{path}/",
            ],
            check=True,
        )


class VirtualMachineRunner:
    """
    this class is deprecated and might get removed soon
    """

    def __init__(self, platform_name: str, platform):
        self.platform_name = platform_name
        self.platform = platform
        self.vm_name = f"pcp-ci-{self.platform_name}"
        self.ssh_config_file = f".ssh-config-{self.platform_name}"
        self.vagrant_env = {
            "PATH": "/usr/bin",
            "VAGRANT_CWD": os.path.dirname(__file__),
            "VAGRANT_NAME": self.vm_name,
            "VAGRANT_BOX": self.platform["vm"]["box"],
        }
        self.command_preamble = "set -eux\nexport runner=vm\n"

    def setup(self, pcp_path):
        post_start = self.platform["container"].get("post_start")

        subprocess.run(["vagrant", "up"], env=self.vagrant_env, check=True)
        subprocess.run(f"vagrant ssh-config > {self.ssh_config_file}", env=self.vagrant_env, shell=True, check=True)
        subprocess.run(
            ["rsync", "-a", "-e", f"ssh -F {self.ssh_config_file}", f"{pcp_path}/", f"{self.vm_name}:pcp/"], check=True
        )
        self.exec("mkdir -p artifacts/build artifacts/test")

        if post_start:
            self.exec(post_start)

    def exec(self, command, check=True):
        command = self.command_preamble + command
        subprocess.run(
            ["ssh", "-F", self.ssh_config_file, self.vm_name, "bash", "-"], input=command.encode(), check=check
        )

    def shell(self):
        subprocess.run(["ssh", "-F", self.ssh_config_file, self.vm_name], check=False)

    def task(self, task_name):
        self.exec(self.platform["tasks"][task_name])

    def get_artifacts(self, artifact, path):
        subprocess.run(
            [
                "rsync",
                "-a",
                "-e",
                f"ssh -F {self.ssh_config_file}",
                f"{self.vm_name}:artifacts/{artifact}/",
                f"{path}/",
            ],
            check=True,
        )


class ContainerRunner:
    def _setup_macos_config(self):
        """Configure podman for macOS (no sudo, platform flags for ARM64)."""
        self.sudo = []
        self.security_opts = []
        if is_arm64():
            # Specify container architecture explicitly on ARM64 macOS
            arch = "linux/arm64" if self.use_native_arch else "linux/amd64"
            self.platform_flags = ["--platform", arch]

    def _setup_linux_config(self):
        """Configure podman for Linux (sudo for Ubuntu, system labels)."""
        try:
            with open("/etc/os-release", encoding="utf-8") as f:
                for line in f:
                    k, v = line.rstrip().split("=")
                    if k == "NAME" and v == '"Ubuntu"':
                        self.sudo = ["sudo", "-E", "XDG_RUNTIME_DIR="]
                        self.security_opts = ["--security-opt", "label=disable"]
                        break
        except FileNotFoundError:
            pass

    def __init__(self, platform_name: str, platform, use_native_arch: bool = False):
        self.platform_name = platform_name
        self.platform = platform
        self.container_name = f"pcp-ci-{self.platform_name}"
        self.image_name = f"{self.container_name}-image"
        self.command_preamble = "set -eux\nexport runner=container\n"
        self.platform_flags = []
        self.use_native_arch = use_native_arch
        self.sudo = []
        self.security_opts = []

        if is_macos():
            self._setup_macos_config()
        else:
            self._setup_linux_config()

    def setup(self, pcp_path):
        containerfile = self.platform["container"]["containerfile"]

        # platform_flags specifies container architecture (e.g., --platform linux/arm64)
        # on ARM64 macOS, allowing explicit control over native vs emulated builds
        subprocess.run(
            [*self.sudo, "podman", "build", *self.platform_flags, "--squash", "-t", self.image_name, "-f", "-"],
            input=containerfile.encode(),
            check=True,
        )

        subprocess.run([*self.sudo, "podman", "rm", "-f", self.container_name], stderr=subprocess.DEVNULL, check=False)
        subprocess.run(
            [
                *self.sudo,
                "podman",
                "run",
                *self.platform_flags,
                "-dt",
                "--name",
                self.container_name,
                "--privileged",
                *self.security_opts,
                self.image_name,
            ],
            check=True,
        )

        # Copy PCP sources
        subprocess.run(
            [*self.sudo, "podman", "cp", f"{pcp_path}/", f"{self.container_name}:/home/pcpbuild/pcp"], check=True
        )

        self.exec("sudo chown -R pcpbuild:pcpbuild .")
        # Ensure .git is a valid git repository (Makepkgs requires it)
        # On macOS with worktrees, .git is a file pointing to the actual repo, which won't work in a container
        # On Linux, .git might be a real directory with submodule references that don't work
        # Solution: Reinitialize as a git repo if not already a valid one
        self.exec("if ! git rev-parse --git-dir >/dev/null 2>&1; then rm -rf .git && git init; fi")
        self.exec("mkdir -p ../artifacts/build ../artifacts/test")

    def destroy(self):
        subprocess.run([*self.sudo, "podman", "rm", "-f", self.container_name], check=True)
        subprocess.run([*self.sudo, "podman", "rmi", self.image_name], check=True)

    def exec(self, command, check=True):
        command = self.command_preamble + command
        subprocess.run(
            [
                *self.sudo,
                "podman",
                "exec",
                "-i",
                "-u",
                "pcpbuild",
                "-w",
                "/home/pcpbuild/pcp",
                "-e",
                "PCP_QA_ARGS",
                self.container_name,
                "bash",
                "-",
            ],
            input=command.encode(),
            check=check,
        )

    def shell(self):
        subprocess.run(
            [
                *self.sudo,
                "podman",
                "exec",
                "-it",
                "-u",
                "pcpbuild",
                "-w",
                "/home/pcpbuild/pcp",
                self.container_name,
                "bash",
            ],
            check=False,
        )

    def task(self, task_name):
        self.exec(self.platform["tasks"][task_name])

    def get_artifacts(self, artifact, path):
        subprocess.run(
            [*self.sudo, "podman", "cp", f"{self.container_name}:/home/pcpbuild/artifacts/{artifact}/.", path],
            check=True,
        )


def _determine_use_native_arch(args):
    """Determine whether to use native architecture based on platform and flags."""
    use_native_arch = False
    if is_macos():
        # On macOS: default to native arch unless --emulate is specified
        use_native_arch = not args.emulate
    # On Linux or with --native-arch flag: respect explicit --native-arch
    if args.native_arch:
        use_native_arch = True
    return use_native_arch


def _create_runner(platform_name, platform, use_native_arch):
    """Create the appropriate runner for the given platform type."""
    platform_type = platform.get("type")
    if platform_type == "direct":
        return DirectRunner(platform_name, platform)
    elif platform_type == "vm":
        return VirtualMachineRunner(platform_name, platform)
    elif platform_type == "container":
        return ContainerRunner(platform_name, platform, use_native_arch=use_native_arch)
    else:
        print(f"Error: Unknown platform type: {platform_type}", file=sys.stderr)
        sys.exit(1)


def _load_platform_definition(platform_name):
    """Load platform YAML definition file."""
    platform_def_path = os.path.join(os.path.dirname(__file__), f"platforms/{platform_name}.yml")
    try:
        with open(platform_def_path, encoding="utf-8") as f:
            return yaml.safe_load(f)
    except FileNotFoundError:
        print(f"Error: Platform definition not found: {platform_def_path}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pcp_path", default=".")
    parser.add_argument(
        "--native-arch",
        action="store_true",
        help="Use native host architecture instead of amd64 (useful on macOS for faster builds)"
    )
    parser.add_argument(
        "--emulate",
        action="store_true",
        help="Force amd64 emulation even on native ARM64 systems (default on non-macOS)"
    )
    parser.add_argument(
        "--quick",
        nargs="?",
        const=True,
        metavar="PLATFORMS",
        help="Quick mode: run multiple platforms. Platforms can be comma or space-separated, "
             "or loaded from PCP_CI_QUICK_PLATFORMS env var or .pcp-ci-quick config file"
    )
    parser.add_argument("platform", nargs="?")
    subparsers = parser.add_subparsers(dest="main_command")

    subparsers.add_parser("setup")
    subparsers.add_parser("destroy")

    parser_task = subparsers.add_parser("task")
    parser_task.add_argument("task_name")

    parser_artifacts = subparsers.add_parser("artifacts")
    parser_artifacts.add_argument("artifact", choices=["build", "test"])
    parser_artifacts.add_argument("--path", default="./artifacts")

    parser_exec = subparsers.add_parser("exec")
    parser_exec.add_argument("command", nargs=argparse.REMAINDER)

    subparsers.add_parser("shell")

    parser_reproduce = subparsers.add_parser("reproduce")
    parser_reproduce.add_argument("--until", default="init_qa")

    args = parser.parse_args()

    # Handle quick mode
    if args.quick is not None:
        quick_platforms = resolve_platforms_to_run(
            cli_platforms=args.platform if args.quick is True else args.quick,
            env_platforms=os.environ.get("PCP_CI_QUICK_PLATFORMS")
        )

        if not quick_platforms:
            print_quick_mode_help()
            sys.exit(1)

        if not args.main_command:
            print("Error: Quick mode requires a subcommand (setup, task, reproduce, etc.)", file=sys.stderr)
            sys.exit(1)

        use_native_arch = _determine_use_native_arch(args)

        for platform_name in quick_platforms:
            print(f"\n{'='*60}")
            print(f"Running on platform: {platform_name}")
            print(f"{'='*60}\n")

            platform = _load_platform_definition(platform_name)
            runner = _create_runner(platform_name, platform, use_native_arch)
            _execute_command(runner, args, platform_name)

        sys.exit(0)

    # Normal (non-quick) mode
    if not args.platform:
        print("Error: Platform argument required (unless using --quick mode)", file=sys.stderr)
        sys.exit(1)

    platform = _load_platform_definition(args.platform)
    use_native_arch = _determine_use_native_arch(args)
    runner = _create_runner(args.platform, platform, use_native_arch)

    _execute_command(runner, args)


if __name__ == "__main__":
    main()
