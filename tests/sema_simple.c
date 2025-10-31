// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include "common.h"
#include "../usdt.h"

USDT_DEFINE_SEMA(explicit_sema);

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	USDT(test, no_sema);

	printf("Implicit semaphore state: %s.\n",
	       USDT_IS_ACTIVE(test, implicit_sema) ? "SET" : "UNSET");
	if (USDT_IS_ACTIVE(test, implicit_sema)) {
		USDT_WITH_SEMA(test, implicit_sema);
		printf("Implicit semaphore USDT is ACTIVE.\n");
	}

	printf("Explicit semaphore state: %s.\n",
	       USDT_SEMA_IS_ACTIVE(explicit_sema) ? "SET" : "UNSET");
	if (USDT_SEMA_IS_ACTIVE(explicit_sema)) {
		USDT_WITH_EXPLICIT_SEMA(explicit_sema, test, explicit_sema);
		printf("Explicit semaphore USDT is ACTIVE.\n");
	}

	return 0;
}

const char *USDT_SPECS =
"test:no_sema base=BASE1 sema=0 argn=0 args=.\n"
"test:implicit_sema base=BASE1 sema=SEMA1 argn=0 args=.\n"
"test:explicit_sema base=BASE1 sema=SEMA2 argn=0 args=.\n"
;

const char *UNTRACED_OUTPUT =
"Implicit semaphore state: UNSET.\n"
"Explicit semaphore state: UNSET.\n"
;

const char *BPFTRACE_SCRIPT =
"test:no_sema { triggered }\n"
"test:implicit_sema { triggered }\n"
"test:explicit_sema { triggered }\n"
;

const char *BPFTRACE_OUTPUT =
"test:no_sema: triggered\n"
"test:implicit_sema: triggered\n"
"test:explicit_sema: triggered\n"
;

const char *TRACED_OUTPUT =
"Implicit semaphore state: SET.\n"
"Implicit semaphore USDT is ACTIVE.\n"
"Explicit semaphore state: SET.\n"
"Explicit semaphore USDT is ACTIVE.\n"
;
