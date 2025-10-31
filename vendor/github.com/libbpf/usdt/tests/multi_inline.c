// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include "common.h"
#include "../usdt.h"

USDT_DEFINE_SEMA(sema);

static __always_inline void usdt_inner(int x)
{
	USDT(test, no_sema, x);
	USDT_WITH_SEMA(test, imp_sema, x);
	USDT_WITH_EXPLICIT_SEMA(sema, test, exp_sema, x);
}

static __always_inline void usdt(int x)
{
	usdt_inner(x + 1);
	usdt_inner(x + 2);
	usdt_inner(x + 3);
}

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	usdt(0);

	return 0;
}

const char *USDT_SPECS =
"test:no_sema base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:imp_sema base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:exp_sema base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
"test:no_sema base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:imp_sema base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:exp_sema base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
"test:no_sema base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:imp_sema base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:exp_sema base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
;

const char *BPFTRACE_SCRIPT =
"test:no_sema { x=%d -> arg0 }\n"
"test:imp_sema { x=%d -> arg0 }\n"
"test:exp_sema { x=%d -> arg0 }\n"
;

const char *BPFTRACE_OUTPUT =
"test:no_sema: x=1\n"
"test:imp_sema: x=1\n"
"test:exp_sema: x=1\n"
"test:no_sema: x=2\n"
"test:imp_sema: x=2\n"
"test:exp_sema: x=2\n"
"test:no_sema: x=3\n"
"test:imp_sema: x=3\n"
"test:exp_sema: x=3\n"
;
