#!/usr/bin/env pmpython
#
# Copyright (c) 2026 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
"""Check that the db2local agent can use the Db2 instance it is configured for.

The agent can only become the Db2 instance user while it still runs as root,
so a configuration mistake is best reported before pmcd is involved.  Install
runs this script for that reason.  It also works standalone, which is useful
when a configuration file is edited by hand:

    check-config.py [config]

The configuration file defaults to $PCP_SYSCONF_DIR/db2local/db2local.conf.
The exit status is 0 when the agent would be able to start, 1 when it would
not, with the reason printed on stderr.
"""

import configparser
import os
import pwd
import sys

DEFAULT_CONFIG = '/db2local/db2local.conf'


def accessible(account, path, mode):
    """Check path access with the configured account's credentials.

    Mode bits alone reject POSIX ACL grants, so resolve the path with
    access(2) in a child that has dropped to the target user.  access(2)
    also checks search permission on every parent directory.
    """
    if os.geteuid() != 0:
        return os.access(path, mode)
    try:
        pid = os.fork()
    except OSError:
        return False
    if pid == 0:
        try:
            os.initgroups(account.pw_name, account.pw_gid)
            os.setgid(account.pw_gid)
            os.setuid(account.pw_uid)
            # _exit and not sys.exit: the child must not unwind the parent.
            os._exit(0 if os.access(path, mode) else 1)  # pylint: disable=protected-access
        except OSError:
            os._exit(2)  # pylint: disable=protected-access
    return os.waitpid(pid, 0)[1] == 0


def expand(account, path):
    """Expand a leading ~/ in a configured path."""
    if path.startswith('~') and not path.startswith('~/'):
        return None
    return account.pw_dir + path[1:] if path.startswith('~/') else path


def resolve(account, path):
    """Resolve a configured path the way the agent does, or return None.

    Only a leading ~/ and absolute paths are accepted, and the result is
    resolved through any symbolic links, exactly as pmdadb2local does.
    """
    path = expand(account, path)
    if path is None or not os.path.isabs(path):
        return None
    return os.path.realpath(path)


def check(config):
    """Validate one configuration file, raising SystemExit on any problem."""
    if not os.path.isfile(config):
        raise SystemExit('Error: missing config file %s' % config)
    parser = configparser.ConfigParser()
    try:
        with open(config, encoding='utf-8') as stream:
            parser.read_file(stream)
    except OSError as error:
        raise SystemExit('Error: cannot read %s: %s'
                         % (config, error.strerror))
    except configparser.Error as error:
        raise SystemExit('Error: cannot parse %s: %s' % (config, error))
    if not parser.has_section('db2'):
        raise SystemExit('Error: missing [db2] section in %s' % config)
    section = parser['db2']

    username = section.get('user', 'db2inst1')
    try:
        account = pwd.getpwnam(username)
    except KeyError:
        raise SystemExit('Error: unknown db2local user ' + username)

    # bin_dir is optional: the instance's own sqllib/bin and sqllib/adm
    # directories are searched by default, and a configured path that an
    # upgrade removed is skipped in favour of them.
    configured = section.get('bin_dir', '').strip()
    bindir = resolve(account, configured) if configured else ''
    if bindir and not os.path.isdir(bindir):
        # The configured path can name a Db2 version that an upgrade
        # removed; use the instance directories the agent falls back to.
        sys.stderr.write('Warning: db2local bin_dir %s is unavailable\n'
                         % bindir)
        bindir = ''
    elif configured and not bindir:
        # Only a leading ~/ and absolute paths are usable, as in the agent,
        # which ignores anything else and uses the instance directories.
        sys.stderr.write('Warning: db2local bin_dir %s is unusable\n'
                         % configured)
    profile = resolve(account, section.get('profile', '~/sqllib/db2profile'))
    if profile is None or not os.path.isfile(profile):
        raise SystemExit('Error: invalid db2local profile')

    if not bindir:
        bindir = os.path.realpath(os.path.join(account.pw_dir, 'sqllib', 'bin'))
    if not os.path.isdir(bindir):
        raise SystemExit('Error: invalid db2local bin_dir')

    if not accessible(account, profile, os.R_OK):
        raise SystemExit('Error: db2local profile is not readable by '
                         + account.pw_name)

    admdir = os.path.join(os.path.dirname(profile), 'adm')
    for tool in ('db2', 'db2level', 'db2diag', 'db2pd'):
        for directory in (bindir, admdir):
            path = os.path.normpath(os.path.join(directory, tool))
            if not path.startswith(directory + os.sep):
                continue
            if not os.path.isfile(path):
                continue
            if accessible(account, path, os.X_OK):
                break
        else:
            raise SystemExit('Error: missing db2local tool %s for %s'
                             % (tool, account.pw_name))


def main(argv):
    """Validate the requested configuration file and return an exit status."""
    if len(argv) > 2:
        sys.stderr.write('Usage: %s [config]\n' % os.path.basename(argv[0]))
        return 1
    if len(argv) == 2 and argv[1] in ('-h', '--help'):
        sys.stdout.write(__doc__)
        return 0
    if len(argv) == 2:
        config = argv[1]
    else:
        config = os.environ.get('PCP_SYSCONF_DIR', '/etc/pcp') + DEFAULT_CONFIG
    check(config)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
