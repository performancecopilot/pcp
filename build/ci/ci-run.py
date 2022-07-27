#!/usr/bin/env python3
import argparse
import subprocess
import yaml
import sys
import os
import tempfile
from datetime import datetime


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
    def __init__(self, platform_name: str, platform):
        self.platform_name = platform_name
        self.platform = platform
        self.container_name = f"pcp-ci-{self.platform_name}"
        self.image_name = f"{self.container_name}-image"
        self.command_preamble = "set -eux\nexport runner=container\n"

        # on Ubuntu, systemd inside the container only works with sudo
        # also don't run as root in general on Github actions,
        # otherwise the direct runner would run everything as root
        self.sudo = []
        self.security_opts = []
        with open("/etc/os-release", encoding="utf-8") as f:
            for line in f:
                k, v = line.rstrip().split("=")
                if k == "NAME":
                    if v == '"Ubuntu"':
                        self.sudo = ["sudo", "-E", "XDG_RUNTIME_DIR="]
                        self.security_opts = ["--security-opt", "label=disable"]
                    break

    def setup(self, pcp_path):
        containerfile = self.platform["container"]["containerfile"]

        # build a new image
        subprocess.run(
            [*self.sudo, "podman", "build", "--squash", "-t", self.image_name, "-f", "-"],
            input=containerfile.encode(),
            check=True,
        )

        # start a new container
        subprocess.run([*self.sudo, "podman", "rm", "-f", self.container_name], stderr=subprocess.DEVNULL, check=False)
        subprocess.run(
            [
                *self.sudo,
                "podman",
                "run",
                "-dt",
                "--name",
                self.container_name,
                "--privileged",
                *self.security_opts,
                self.image_name,
            ],
            check=True,
        )

        subprocess.run(
            [*self.sudo, "podman", "cp", f"{pcp_path}/", f"{self.container_name}:/home/pcpbuild/pcp"], check=True
        )
        self.exec("sudo chown -R pcpbuild:pcpbuild .")
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pcp_path", default=".")
    parser.add_argument("platform")
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
    platform_def_path = os.path.join(os.path.dirname(__file__), f"platforms/{args.platform}.yml")
    with open(platform_def_path, encoding="utf-8") as f:
        platform = yaml.safe_load(f)
    platform_type = platform.get("type")
    if platform_type == "direct":
        runner = DirectRunner(args.platform, platform)
    elif platform_type == "vm":
        runner = VirtualMachineRunner(args.platform, platform)
    elif platform_type == "container":
        runner = ContainerRunner(args.platform, platform)

    if args.main_command == "setup":
        try:
            runner.setup(args.pcp_path)
            runner.task("setup")
        except subprocess.CalledProcessError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.main_command == "destroy":
        try:
            runner.destroy()
        except subprocess.CalledProcessError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.main_command == "task":
        try:
            runner.task(args.task_name)
        except subprocess.CalledProcessError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.main_command == "artifacts":
        runner.task(f"copy_{args.artifact}_artifacts")
        runner.get_artifacts(args.artifact, args.path)
    elif args.main_command == "exec":
        runner.exec(" ".join(args.command), check=False)
    elif args.main_command == "shell":
        runner.shell()
    elif args.main_command == "reproduce":
        all_tasks = ["setup", "build", "install", "init_qa", "qa"]
        run_tasks = all_tasks[: all_tasks.index(args.until) + 1]

        print("Preparing a new virtual environment with PCP preinstalled, this will take about 20 minutes...")
        started = datetime.now()
        runner.setup(args.pcp_path)
        for task in run_tasks:
            print(f"\nRunning task {task}...")
            runner.task(task)
        duration_min = (datetime.now() - started).total_seconds() / 60
        print(f"\nVirtual environment setup done, took {duration_min:.0f}m.")

        if all_tasks.index(args.until) >= all_tasks.index("install"):
            print("\nPlease run:\n")
            print("    sudo -u pcpqa -i ./check XXX\n")
            print("to run a QA test. PCP is already installed, from sources located in './pcp'.")
        print("Starting a shell in the new virtual environment...\n")
        runner.shell()
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
