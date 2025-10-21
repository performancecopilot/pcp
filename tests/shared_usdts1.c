// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include "shared_usdts.h"

USDT_DEFINE_SEMA(common_sema1);
USDT_DEFINE_SEMA(common_sema2);
USDT_DEFINE_SEMA(common_sema3);

USDT_DEFINE_SEMA(exec_sema);

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	common_usdts(1);
	exec_usdts(1);

	other_file_func(2);
	lib_func(3);

	return 0;
}

const char *USDT_SPECS =
/* main file USDTs */
"test:common_no_sema1 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema2 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema3 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_imp_sema1 base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:common_imp_sema2 base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
"test:common_imp_sema3 base=BASE1 sema=SEMA3 argn=1 args=-4@*.\n"
"test:common_exp_sema1 base=BASE1 sema=SEMA4 argn=1 args=-4@*.\n"
"test:common_exp_sema2 base=BASE1 sema=SEMA5 argn=1 args=-4@*.\n"
"test:common_exp_sema3 base=BASE1 sema=SEMA6 argn=1 args=-4@*.\n"
"test:exec_no_sema base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:exec_imp_sema base=BASE1 sema=SEMA7 argn=1 args=-4@*.\n"
"test:exec_exp_sema base=BASE1 sema=SEMA8 argn=1 args=-4@*.\n"
/* second file USDTs, same as main file USDTs */
"test:common_no_sema1 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema2 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema3 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_imp_sema1 base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:common_imp_sema2 base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
"test:common_imp_sema3 base=BASE1 sema=SEMA3 argn=1 args=-4@*.\n"
"test:common_exp_sema1 base=BASE1 sema=SEMA4 argn=1 args=-4@*.\n"
"test:common_exp_sema2 base=BASE1 sema=SEMA5 argn=1 args=-4@*.\n"
"test:common_exp_sema3 base=BASE1 sema=SEMA6 argn=1 args=-4@*.\n"
"test:exec_no_sema base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:exec_imp_sema base=BASE1 sema=SEMA7 argn=1 args=-4@*.\n"
"test:exec_exp_sema base=BASE1 sema=SEMA8 argn=1 args=-4@*.\n"
#ifdef SHARED
/* static lib USDTs */
"test:common_no_sema1 base=BASE2 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema2 base=BASE2 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema3 base=BASE2 sema=0 argn=1 args=-4@*.\n"
"test:common_imp_sema1 base=BASE2 sema=SEMA9 argn=1 args=-4@*.\n"
"test:common_imp_sema2 base=BASE2 sema=SEMA10 argn=1 args=-4@*.\n"
"test:common_imp_sema3 base=BASE2 sema=SEMA11 argn=1 args=-4@*.\n"
"test:common_exp_sema1 base=BASE2 sema=SEMA12 argn=1 args=-4@*.\n"
"test:common_exp_sema2 base=BASE2 sema=SEMA13 argn=1 args=-4@*.\n"
"test:common_exp_sema3 base=BASE2 sema=SEMA14 argn=1 args=-4@*.\n"
"test:lib_no_sema base=BASE2 sema=0 argn=1 args=-4@*.\n"
"test:lib_imp_sema base=BASE2 sema=SEMA15 argn=1 args=-4@*.\n"
"test:lib_exp_sema base=BASE2 sema=SEMA16 argn=1 args=-4@*.\n"
#else /* !SHARED */
/* static lib USDTs */
"test:common_no_sema1 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema2 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_no_sema3 base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:common_imp_sema1 base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:common_imp_sema2 base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
"test:common_imp_sema3 base=BASE1 sema=SEMA3 argn=1 args=-4@*.\n"
"test:common_exp_sema1 base=BASE1 sema=SEMA4 argn=1 args=-4@*.\n"
"test:common_exp_sema2 base=BASE1 sema=SEMA5 argn=1 args=-4@*.\n"
"test:common_exp_sema3 base=BASE1 sema=SEMA6 argn=1 args=-4@*.\n"
"test:lib_no_sema base=BASE1 sema=0 argn=1 args=-4@*.\n"
"test:lib_imp_sema base=BASE1 sema=SEMA9 argn=1 args=-4@*.\n"
"test:lib_exp_sema base=BASE1 sema=SEMA10 argn=1 args=-4@*.\n"
#endif /* SHARED */
;

const char *UNTRACED_OUTPUT =
/* main file output */
"common_imp_sema1 is INACTIVE.\n"
"common_imp_sema2 is INACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is INACTIVE.\n"
"common_exp_sema2 is INACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"exec_imp_sema is INACTIVE.\n"
"exec_exp_sema is INACTIVE.\n"
/* second file output */
"common_imp_sema1 is INACTIVE.\n"
"common_imp_sema2 is INACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is INACTIVE.\n"
"common_exp_sema2 is INACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"exec_imp_sema is INACTIVE.\n"
"exec_exp_sema is INACTIVE.\n"
/* library file output */
"common_imp_sema1 is INACTIVE.\n"
"common_imp_sema2 is INACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is INACTIVE.\n"
"common_exp_sema2 is INACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"lib_imp_sema is INACTIVE.\n"
"lib_exp_sema is INACTIVE.\n"
;

/* In shared library mode, USDT semaphores cannot cross boundaries of their
 * containing executable or shared library. So there can't be cross library or
 * executable-library references for explicit USDT semaphores. For implicit
 * semaphores, it's possible to reuse USDT_WITH_SEMA() with the same
 * group:name identifier, but they will be indepedent between libraries and
 * executable. I.e., their semaphores are not shared, so attaching to USDTs in
 * executable won't "activate" USDTs associated with the same group:name in in
 * shared library.
 *
 * For static library mode all this is moot, because static library is part of
 * executable, so it's as if there was no library involved.
 */

const char *BPFTRACE_SCRIPT =
#ifdef SHARED
/* executable USDTS */
"test:common_no_sema1 { x=%d -> arg0 }\n"
"test:common_imp_sema1 { x=%d -> arg0 }\n"
"test:common_exp_sema1 { x=%d -> arg0 }\n"
"test:exec_no_sema { x=%d -> arg0 }\n"
"test:exec_imp_sema { x=%d -> arg0 }\n"
"test:exec_exp_sema { x=%d -> arg0 }\n"
/* shared lib USDTs */
"lib:test:common_no_sema2 { x=%d -> arg0 }\n"		/* not shared with executable */
"lib:test:common_imp_sema2 { x=%d -> arg0 }\n"		/* not shared with executable */
"lib:test:common_exp_sema2 { x=%d -> arg0 }\n"		/* not shared with executable */
"lib:test:lib_no_sema { x=%d -> arg0 }\n"
"lib:test:lib_imp_sema { x=%d -> arg0 }\n"
"lib:test:lib_exp_sema { x=%d -> arg0 }\n"
#else
"test:common_no_sema1 { x=%d -> arg0 }\n"
"test:common_imp_sema1 { x=%d -> arg0 }\n"
"test:common_exp_sema1 { x=%d -> arg0 }\n"
"test:exec_no_sema { x=%d -> arg0 }\n"
"test:exec_imp_sema { x=%d -> arg0 }\n"
"test:exec_exp_sema { x=%d -> arg0 }\n"
"test:common_no_sema2 { x=%d -> arg0 }\n"		/* shared between exec and static lib */
"test:common_imp_sema2 { x=%d -> arg0 }\n"		/* shared between exec and static lib */
"test:common_exp_sema2 { x=%d -> arg0 }\n"		/* shared between exec and static lib */
"test:lib_no_sema { x=%d -> arg0 }\n"
"test:lib_imp_sema { x=%d -> arg0 }\n"
"test:lib_exp_sema { x=%d -> arg0 }\n"
#endif /* SHARED */
;

const char *BPFTRACE_OUTPUT =
#ifdef SHARED
/* main()'s output */
"test:common_no_sema1: x=1\n"
"test:common_imp_sema1: x=1\n"
"test:common_exp_sema1: x=1\n"
/* note, no test:common_*_sema2, but lib has test:common_*_sema2 */
"test:exec_no_sema: x=1\n"
"test:exec_imp_sema: x=1\n"
"test:exec_exp_sema: x=1\n"
/* other_file_func()'s output */
"test:common_no_sema1: x=2\n"
"test:common_imp_sema1: x=2\n"
"test:common_exp_sema1: x=2\n"
/* note, no test:common_*_sema2, but lib has test:common_*_sema2 */
"test:exec_no_sema: x=2\n"
"test:exec_imp_sema: x=2\n"
"test:exec_exp_sema: x=2\n"
/* lib_func()'s output */
/* note, no test:common_*_sema1, but executable has test:common_*_sema1 */
"lib:test:common_no_sema2: x=3\n"		/* independent from executable */
"lib:test:common_imp_sema2: x=3\n"		/* independent from executable */
"lib:test:common_exp_sema2: x=3\n"		/* independent from executable */
"lib:test:lib_no_sema: x=3\n"
"lib:test:lib_imp_sema: x=3\n"
"lib:test:lib_exp_sema: x=3\n"
#else /* !SHARED */
/* main()'s output */
"test:common_no_sema1: x=1\n"
"test:common_no_sema2: x=1\n"
"test:common_imp_sema1: x=1\n"
"test:common_imp_sema2: x=1\n"
"test:common_exp_sema1: x=1\n"
"test:common_exp_sema2: x=1\n"
"test:exec_no_sema: x=1\n"
"test:exec_imp_sema: x=1\n"
"test:exec_exp_sema: x=1\n"
/* other_file_func()'s output */
"test:common_no_sema1: x=2\n"
"test:common_no_sema2: x=2\n"
"test:common_imp_sema1: x=2\n"
"test:common_imp_sema2: x=2\n"
"test:common_exp_sema1: x=2\n"
"test:common_exp_sema2: x=2\n"
"test:exec_no_sema: x=2\n"
"test:exec_imp_sema: x=2\n"
"test:exec_exp_sema: x=2\n"
/* lib_func()'s output */
"test:common_no_sema1: x=3\n"
"test:common_no_sema2: x=3\n"
"test:common_imp_sema1: x=3\n"
"test:common_imp_sema2: x=3\n"
"test:common_exp_sema1: x=3\n"
"test:common_exp_sema2: x=3\n"
"test:lib_no_sema: x=3\n"
"test:lib_imp_sema: x=3\n"
"test:lib_exp_sema: x=3\n"
#endif /* SHARED */
;

const char *TRACED_OUTPUT =
#ifdef SHARED
/* main file output */
"common_imp_sema1 is ACTIVE.\n"
"common_imp_sema2 is INACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is ACTIVE.\n"
"common_exp_sema2 is INACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"exec_imp_sema is ACTIVE.\n"
"exec_exp_sema is ACTIVE.\n"
/* second file output */
"common_imp_sema1 is ACTIVE.\n"
"common_imp_sema2 is INACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is ACTIVE.\n"
"common_exp_sema2 is INACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"exec_imp_sema is ACTIVE.\n"
"exec_exp_sema is ACTIVE.\n"
/* library file output */
"common_imp_sema1 is INACTIVE.\n"
"common_imp_sema2 is ACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is INACTIVE.\n"
"common_exp_sema2 is ACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"lib_imp_sema is ACTIVE.\n"
"lib_exp_sema is ACTIVE.\n"
#else /* !SHARED */
/* main file output */
"common_imp_sema1 is ACTIVE.\n"
"common_imp_sema2 is ACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is ACTIVE.\n"
"common_exp_sema2 is ACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"exec_imp_sema is ACTIVE.\n"
"exec_exp_sema is ACTIVE.\n"
/* second file output */
"common_imp_sema1 is ACTIVE.\n"
"common_imp_sema2 is ACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is ACTIVE.\n"
"common_exp_sema2 is ACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"exec_imp_sema is ACTIVE.\n"
"exec_exp_sema is ACTIVE.\n"
/* library file output */
"common_imp_sema1 is ACTIVE.\n"
"common_imp_sema2 is ACTIVE.\n"
"common_imp_sema3 is INACTIVE.\n"
"common_exp_sema1 is ACTIVE.\n"
"common_exp_sema2 is ACTIVE.\n"
"common_exp_sema3 is INACTIVE.\n"
"lib_imp_sema is ACTIVE.\n"
"lib_exp_sema is ACTIVE.\n"
#endif
;
