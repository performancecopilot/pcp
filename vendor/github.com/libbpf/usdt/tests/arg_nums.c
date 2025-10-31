// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include "common.h"
#include "../usdt.h"

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	USDT(test, name0);
	USDT(test, name1, 1);
	USDT(test, name2, 1, 2);
	USDT(test, name3, 1, 2, 3);
	USDT(test, name4, 1, 2, 3, 4);
	USDT(test, name5, 1, 2, 3, 4, 5);
	USDT(test, name6, 1, 2, 3, 4, 5, 6);
	USDT(test, name7, 1, 2, 3, 4, 5, 6, 7);
	USDT(test, name8, 1, 2, 3, 4, 5, 6, 7, 8);
	USDT(test, name9, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	USDT(test, name10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
	USDT(test, name11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
	USDT(test, name12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);

	return 0;
}

#ifdef __aarch64__
const char *USDT_SPECS =
"test:name0 base=BASE1 sema=0 argn=0 args=.\n"
"test:name1 base=BASE1 sema=0 argn=1 args=-4@1.\n"
"test:name2 base=BASE1 sema=0 argn=2 args=-4@1 -4@2.\n"
"test:name3 base=BASE1 sema=0 argn=3 args=-4@1 -4@2 -4@3.\n"
"test:name4 base=BASE1 sema=0 argn=4 args=-4@1 -4@2 -4@3 -4@4.\n"
"test:name5 base=BASE1 sema=0 argn=5 args=-4@1 -4@2 -4@3 -4@4 -4@5.\n"
"test:name6 base=BASE1 sema=0 argn=6 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6.\n"
"test:name7 base=BASE1 sema=0 argn=7 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6 -4@7.\n"
"test:name8 base=BASE1 sema=0 argn=8 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6 -4@7 -4@8.\n"
"test:name9 base=BASE1 sema=0 argn=9 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6 -4@7 -4@8 -4@9.\n"
"test:name10 base=BASE1 sema=0 argn=10 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6 -4@7 -4@8 -4@9 -4@10.\n"
"test:name11 base=BASE1 sema=0 argn=11 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6 -4@7 -4@8 -4@9 -4@10 -4@11.\n"
"test:name12 base=BASE1 sema=0 argn=12 args=-4@1 -4@2 -4@3 -4@4 -4@5 -4@6 -4@7 -4@8 -4@9 -4@10 -4@11 -4@12.\n"
;
#else
const char *USDT_SPECS =
"test:name0 base=BASE1 sema=0 argn=0 args=.\n"
"test:name1 base=BASE1 sema=0 argn=1 args=-4@$1.\n"
"test:name2 base=BASE1 sema=0 argn=2 args=-4@$1 -4@$2.\n"
"test:name3 base=BASE1 sema=0 argn=3 args=-4@$1 -4@$2 -4@$3.\n"
"test:name4 base=BASE1 sema=0 argn=4 args=-4@$1 -4@$2 -4@$3 -4@$4.\n"
"test:name5 base=BASE1 sema=0 argn=5 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5.\n"
"test:name6 base=BASE1 sema=0 argn=6 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6.\n"
"test:name7 base=BASE1 sema=0 argn=7 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6 -4@$7.\n"
"test:name8 base=BASE1 sema=0 argn=8 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6 -4@$7 -4@$8.\n"
"test:name9 base=BASE1 sema=0 argn=9 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6 -4@$7 -4@$8 -4@$9.\n"
"test:name10 base=BASE1 sema=0 argn=10 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6 -4@$7 -4@$8 -4@$9 -4@$10.\n"
"test:name11 base=BASE1 sema=0 argn=11 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6 -4@$7 -4@$8 -4@$9 -4@$10 -4@$11.\n"
"test:name12 base=BASE1 sema=0 argn=12 args=-4@$1 -4@$2 -4@$3 -4@$4 -4@$5 -4@$6 -4@$7 -4@$8 -4@$9 -4@$10 -4@$11 -4@$12.\n"
;
#endif

const char *BPFTRACE_SCRIPT =
"test:name0 { triggered }\n"
"test:name1 { arg0=%d -> arg0 }\n"
"test:name2 { arg0=%d arg1=%d -> arg0, arg1 }\n"
"test:name3 { arg0=%d arg1=%d arg2=%d -> arg0, arg1, arg2 }\n"
"test:name4 { arg0=%d arg1=%d arg2=%d arg3=%d -> arg0, arg1, arg2, arg3 }\n"
"test:name5 { arg0=%d arg1=%d arg2=%d arg3=%d arg4=%d -> arg0, arg1, arg2, arg3, arg4 }\n"
"test:name6 { arg0=%d arg1=%d arg2=%d arg3=%d arg4=%d arg5=%d -> arg0, arg1, arg2, arg3, arg4, arg5 }\n"
;

/* bpftrace can't get more than 6 USDT arguments as of the time of writing this test */
const char *BPFTRACE_OUTPUT =
"test:name0: triggered\n"
"test:name1: arg0=1\n"
"test:name2: arg0=1 arg1=2\n"
"test:name3: arg0=1 arg1=2 arg2=3\n"
"test:name4: arg0=1 arg1=2 arg2=3 arg3=4\n"
"test:name5: arg0=1 arg1=2 arg2=3 arg3=4 arg4=5\n"
"test:name6: arg0=1 arg1=2 arg2=3 arg3=4 arg4=5 arg5=6\n"
;
