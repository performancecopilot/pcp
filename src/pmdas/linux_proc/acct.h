/*
 * Linux acct metrics cluster
 *
 * Copyright (c) 2020 Fujitsu.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

enum {
	ACCT_TTY      = 0,
	ACCT_EXITCODE = 1,
	ACCT_UID      = 2,
	ACCT_GID      = 3,
	ACCT_PID      = 4,
	ACCT_PPID     = 5,
	ACCT_BTIME    = 6,
	ACCT_ETIME    = 7,
	ACCT_UTIME    = 8,
	ACCT_STIME    = 9,
	ACCT_MEM      = 10,
	ACCT_IO       = 11,
	ACCT_RW       = 12,
	ACCT_MINFLT   = 13,
	ACCT_MAJFLT   = 14,
	ACCT_SWAPS    = 15,
};
