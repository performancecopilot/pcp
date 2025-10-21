.. SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)

==================
bpftool-struct_ops
==================
-------------------------------------------------------------------------------
tool to register/unregister/introspect BPF struct_ops
-------------------------------------------------------------------------------

:Manual section: 8

.. include:: substitutions.rst

SYNOPSIS
========

**bpftool** [*OPTIONS*] **struct_ops** *COMMAND*

*OPTIONS* := { |COMMON_OPTIONS| }

*COMMANDS* :=
{ **show** | **list** | **dump** | **register** | **unregister** | **help** }

STRUCT_OPS COMMANDS
===================

| **bpftool** **struct_ops { show | list }** [*STRUCT_OPS_MAP*]
| **bpftool** **struct_ops dump** [*STRUCT_OPS_MAP*]
| **bpftool** **struct_ops register** *OBJ* [*LINK_DIR*]
| **bpftool** **struct_ops unregister** *STRUCT_OPS_MAP*
| **bpftool** **struct_ops help**
|
| *STRUCT_OPS_MAP* := { **id** *STRUCT_OPS_MAP_ID* | **name** *STRUCT_OPS_MAP_NAME* }
| *OBJ* := /a/file/of/bpf_struct_ops.o


DESCRIPTION
===========
bpftool struct_ops { show | list } [*STRUCT_OPS_MAP*]
    Show brief information about the struct_ops in the system. If
    *STRUCT_OPS_MAP* is specified, it shows information only for the given
    struct_ops.  Otherwise, it lists all struct_ops currently existing in the
    system.

    Output will start with struct_ops map ID, followed by its map name and its
    struct_ops's kernel type.

bpftool struct_ops dump [*STRUCT_OPS_MAP*]
    Dump details information about the struct_ops in the system. If
    *STRUCT_OPS_MAP* is specified, it dumps information only for the given
    struct_ops.  Otherwise, it dumps all struct_ops currently existing in the
    system.

bpftool struct_ops register *OBJ* [*LINK_DIR*]
    Register bpf struct_ops from *OBJ*.  All struct_ops under the ELF section
    ".struct_ops" and ".struct_ops.link" will be registered to its kernel
    subsystem.  For each struct_ops in the ".struct_ops.link" section, a link
    will be created.  You can give *LINK_DIR* to provide a directory path where
    these links will be pinned with the same name as their corresponding map
    name.

bpftool struct_ops unregister  *STRUCT_OPS_MAP*
    Unregister the *STRUCT_OPS_MAP* from the kernel subsystem.

bpftool struct_ops help
    Print short help message.

OPTIONS
=======
.. include:: common_options.rst

EXAMPLES
========
**# bpftool struct_ops show**

::

    100: dctcp           tcp_congestion_ops
    105: cubic           tcp_congestion_ops

**# bpftool struct_ops unregister id 105**

::

   Unregistered tcp_congestion_ops cubic id 105

**# bpftool struct_ops register bpf_cubic.o**

::

   Registered tcp_congestion_ops cubic id 110
