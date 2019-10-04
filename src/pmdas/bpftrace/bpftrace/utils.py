from typing import Optional, Tuple
import re
import subprocess


def get_bpftrace_version(bpftrace_path: str) -> Optional[Tuple]:
    version_str = subprocess.check_output([bpftrace_path, '--version'], encoding='utf8').strip()
    re_version = re.match(r'bpftrace v(\d+)\.(\d+)\.(\d+)', version_str)
    if re_version:
        # regex enforces exactly 3 integers
        return version_str, tuple(map(int, re_version.groups()))  # (major, minor, patch)
    return version_str, None


def get_tracepoints_csv(bpftrace_path: str) -> str:
    return subprocess.check_output([bpftrace_path, '-l'], encoding='utf8').strip().replace('\n', ',')
