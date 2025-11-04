// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include "common.h"
#include "../usdt.h"

__weak __optimize void some_func(int x)
{
	USDT(test, no_sema, x);
}

__weak __optimize void some_other_func(int x)
{
	USDT_WITH_SEMA(test, sema, x);
}

extern void other_file_func(int x);

/*
 * Test USDTs (semaphoreless and implicit semaphore versions) with the same
 * name used across multiple files. Just for the fun of it, use incompatible
 * arguments, which is not illegale (even if non-sensical).
 */
int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	USDT(test, no_sema, 1, 2, 3);
	some_func(4);

	USDT_WITH_SEMA(test, sema, 5, 6, 7);
	some_other_func(8);

	other_file_func(9);

	return 0;
}

/* all sema-based USDTs should share the same sema address */
const char *USDT_SPECS =
"test:no_sema base=BASE1 sema=0 argn=1 args=*.\n"
"test:sema base=BASE1 sema=SEMA1 argn=1 args=*.\n"
"test:no_sema base=BASE1 sema=0 argn=3 args=*.\n"
"test:sema base=BASE1 sema=SEMA1 argn=3 args=*.\n"
"test:no_sema base=BASE1 sema=0 argn=5 args=*.\n"
"test:sema base=BASE1 sema=SEMA1 argn=5 args=*.\n"
;

const char *BPFTRACE_SCRIPT =
"test:no_sema { triggered }\n"
"test:sema { triggered }\n"
;

const char *BPFTRACE_OUTPUT =
"test:no_sema: triggered\n"
"test:no_sema: triggered\n"
"test:sema: triggered\n"
"test:sema: triggered\n"
"test:no_sema: triggered\n"
"test:sema: triggered\n"
;
