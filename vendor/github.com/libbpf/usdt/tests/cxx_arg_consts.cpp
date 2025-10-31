// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include <cstddef>
#include "common.h"
#include "../usdt.h"

namespace ns
{

__weak __optimize void fn_in_ns()
{
	USDT(test, chars, (unsigned char)1, (signed char)-1);
	USDT(test, shorts, (unsigned short)2, (short)-2);
	USDT(test, ints, (unsigned int)3, (int)-3);
	USDT(test, longs, (unsigned long)4, (long)-4);
	USDT(test, longlongs, (unsigned long long)5, (long long)-5);
	USDT(test, size_ts, (size_t)6, (ssize_t)-6);
	USDT(test, ptrs, (void *)NULL, (void *)7);
}

}

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	ns::fn_in_ns();

	return 0;
}

#ifdef __aarch64__
const char *USDT_SPECS =
"test:chars base=BASE1 sema=0 argn=2 args=1@1 -1@-1.\n"
"test:shorts base=BASE1 sema=0 argn=2 args=2@2 -2@-2.\n"
"test:ints base=BASE1 sema=0 argn=2 args=4@3 -4@-3.\n"
"test:longs base=BASE1 sema=0 argn=2 args=*@4 -*@-4.\n"
"test:longlongs base=BASE1 sema=0 argn=2 args=8@5 -8@-5.\n"
"test:size_ts base=BASE1 sema=0 argn=2 args=*@6 -*@-6.\n"
"test:ptrs base=BASE1 sema=0 argn=2 args=*@0 *@7.\n"
;
#else
const char *USDT_SPECS =
"test:chars base=BASE1 sema=0 argn=2 args=1@$1 -1@$-1.\n"
"test:shorts base=BASE1 sema=0 argn=2 args=2@$2 -2@$-2.\n"
"test:ints base=BASE1 sema=0 argn=2 args=4@$3 -4@$-3.\n"
"test:longs base=BASE1 sema=0 argn=2 args=*@$4 -*@$-4.\n"
"test:longlongs base=BASE1 sema=0 argn=2 args=8@$5 -8@$-5.\n"
"test:size_ts base=BASE1 sema=0 argn=2 args=*@$6 -*@$-6.\n"
"test:ptrs base=BASE1 sema=0 argn=2 args=*@$0 *@$7.\n"
;
#endif

const char *BPFTRACE_SCRIPT =
"test:chars { arg0=%hhu arg1=%hhd -> arg0, arg1 }\n"
"test:shorts { arg0=%hu arg1=%hd -> arg0, arg1 }\n"
"test:ints { arg0=%u arg1=%d -> arg0, arg1 }\n"
/* don't want to deal with long sizing across BPF and 32-bit arches */
"test:longlongs { arg0=%llu arg1=%lld -> arg0, arg1 }\n"
/* don't want to deal with size_t/ssize_t sizing across BPF and 32-bit arches */
"test:ptrs { arg0=%p arg1=%p -> arg0, arg1 }\n"
;

const char *BPFTRACE_OUTPUT =
"test:chars: arg0=1 arg1=-1\n"
"test:shorts: arg0=2 arg1=-2\n"
"test:ints: arg0=3 arg1=-3\n"
/* we don't trace test:longs, see above */
"test:longlongs: arg0=5 arg1=-5\n"
/* we don't trace test:size_ts, see above */
"test:ptrs: arg0=(nil) arg1=0x7\n"
;
