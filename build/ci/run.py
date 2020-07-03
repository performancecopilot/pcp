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
        self.build_dir = os.path.join(tempfile.gettempdir(), f"pcp-ci-{self.platform_name}")

    def setup(self, pcp_path):
        os.mkdir(self.build_dir)
        subprocess.run(['rsync', '-a', f"{pcp_path}/", f"{self.build_dir}/pcp/"], check=True)
        self.exec('mkdir -p artifacts/build artifacts/test')

    def exec(self, command, check=True):
        command = 'set -eux\n' + command
        subprocess.run(['bash', '-'], cwd=self.build_dir, input=command.encode(), check=check)

    def shell(self):
        pass

    def task(self, task_name):
        self.exec(self.platform['tasks'][task_name])

    def get_artifacts(self, artifacts_path):
        subprocess.run(['rsync', '-a', f"{self.build_dir}/artifacts/", f"{artifacts_path}/", ], check=True)


class VirtualMachineRunner:
    def __init__(self, platform_name: str, platform):
        self.platform_name = platform_name
        self.platform = platform
        self.vm_name = f"pcp-ci-{self.platform_name}"
        self.ssh_config_file = f".ssh-config-{self.platform_name}"
        self.vagrant_env = {
            'PATH': '/usr/bin',
            'VAGRANT_CWD': os.path.dirname(__file__),
            'VAGRANT_NAME': self.vm_name,
            'VAGRANT_BOX': self.platform['vm']['box']
        }

    def setup(self, pcp_path):
        post_start = self.platform['container'].get('post_start')

        subprocess.run(['vagrant', 'up'], env=self.vagrant_env, check=True)
        subprocess.run(f"vagrant ssh-config > {self.ssh_config_file}", env=self.vagrant_env, shell=True, check=True)
        subprocess.run(['rsync', '-a', '-e', f"ssh -F {self.ssh_config_file}",
                        f"{pcp_path}/", f"{self.vm_name}:pcp/"], check=True)
        self.exec('mkdir -p artifacts/build artifacts/test')

        if post_start:
            self.exec(post_start)

    def exec(self, command, check=True):
        command = 'set -eux\n' + command
        subprocess.run(['ssh', '-F', self.ssh_config_file, self.vm_name, 'bash', '-'],
                       input=command.encode(), check=check)

    def shell(self):
        subprocess.run(['ssh', '-F', self.ssh_config_file, self.vm_name])

    def task(self, task_name):
        self.exec(self.platform['tasks'][task_name])

    def get_artifacts(self, artifacts_path):
        subprocess.run(['rsync', '-a', '-e', f"ssh -F {self.ssh_config_file}",
                        f"{self.vm_name}:artifacts/", f"{artifacts_path}/", ], check=True)


class ContainerRunner:
    def __init__(self, platform_name: str, platform):
        self.platform_name = platform_name
        self.platform = platform
        self.container_name = f"pcp-ci-{self.platform_name}"

        # on Ubuntu, systemd inside the container only works with sudo
        self.sudo = []
        with open('/etc/os-release') as f:
            for line in f:
                k, v = line.rstrip().split('=')
                if k == 'NAME':
                    if v == '"Ubuntu"':
                        self.sudo = ['sudo']
                    break

    def setup(self, pcp_path):
        image_name = f"{self.container_name}-image"
        containerfile = self.platform['container']['containerfile']
        init = self.platform['container'].get('init', '/sbin/init')

        # build a new image
        subprocess.run([*self.sudo, 'podman', 'build', '--squash', '-t', image_name, '-f', '-'],
                       input=containerfile.encode(), check=True)

        # start a new container
        subprocess.run([*self.sudo, 'podman', 'rm', '-f', self.container_name], stderr=subprocess.DEVNULL)
        subprocess.run([*self.sudo, 'podman', 'run', '-d', '--name', self.container_name,
                        '--privileged', image_name, init], check=True)

        self.exec('mkdir -p artifacts/build artifacts/test')
        subprocess.run([*self.sudo, 'podman', 'cp', pcp_path, f"{self.container_name}:/home/pcpbuild/pcp"], check=True)

    def exec(self, command, check=True):
        command = 'set -eux\n' + command
        subprocess.run([*self.sudo, 'podman', 'exec', '-i',
                        '-u', 'pcpbuild', '-w', '/home/pcpbuild',
                        '-e', 'is_container=true',
                        self.container_name, 'bash', '-'],
                       input=command.encode(), check=check)

    def shell(self):
        subprocess.run([*self.sudo, 'podman', 'exec', '-it',
                        '-u', 'pcpbuild', '-w', '/home/pcpbuild',
                        '-e', 'is_container=true',
                        self.container_name, 'bash'])

    def task(self, task_name):
        self.exec(self.platform['tasks'][task_name])

    def get_artifacts(self, artifacts_path):
        subprocess.run([*self.sudo, 'podman', 'cp',
                        f"{self.container_name}:/home/pcpbuild/artifacts/.", artifacts_path], check=True)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='main_command')
    parser.add_argument('--platform', required=True)
    parser.add_argument('--runner', required=True, choices=['direct', 'vm', 'container'])
    parser.add_argument('--pcp_path', default='.')

    subparsers.add_parser('setup')

    parser_task = subparsers.add_parser('task')
    parser_task.add_argument('task_name')

    parser_artifacts = subparsers.add_parser('artifacts')
    parser_artifacts.add_argument('--artifacts_path', default='./artifacts')

    parser_exec = subparsers.add_parser('exec')
    parser_exec.add_argument('command', nargs=argparse.REMAINDER)

    subparsers.add_parser('shell')

    subparsers.add_parser('reproduce')

    args = parser.parse_args()
    platform_def_path = os.path.join(os.path.dirname(__file__), f"platforms/{args.platform}.yml")
    with open(platform_def_path) as f:
        platform = yaml.safe_load(f)
    if args.runner == 'direct':
        runner = DirectRunner(args.platform, platform)
    elif args.runner == 'vm':
        runner = VirtualMachineRunner(args.platform, platform)
    elif args.runner == 'container':
        runner = ContainerRunner(args.platform, platform)

    if args.main_command == 'setup':
        runner.setup(args.pcp_path)
    elif args.main_command == 'task':
        runner.task(args.task_name)
    elif args.main_command == 'artifacts':
        runner.get_artifacts(args.artifacts_path)
    elif args.main_command == 'exec':
        runner.exec(' '.join(args.command), check=False)
    elif args.main_command == 'shell':
        runner.shell()
    elif args.main_command == 'reproduce':
        print("Preparing a new virtual environment with PCP preinstalled, this will take about 20 minutes...\n")

        started = datetime.now()
        runner.setup(args.pcp_path)
        for task in ['update', 'install_build_dependencies', 'build', 'install', 'init_qa']:
            runner.task(task)
        duration_min = (datetime.now() - started).total_seconds() / 60

        print(f"\nVirtual environment setup done, took {duration_min:.0f}m.\n")
        print("Please run:\n")
        print("    sudo -u pcpqa -i ./check XXX\n")
        print("to run a QA test. PCP is already installed, from sources located in './pcp'.")
        print("Starting a shell in the new virtual environment...\n")
        runner.shell()
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
