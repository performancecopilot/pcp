// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include "common.h"
#include "../usdt.h"
#include "ns_shared.h"

/* Alright, USDT and USDT semaphores have global visibility and naming, but
 * C++ has its own quirks, so we'll test various cases with USDTs in global
 * and namespaced context. As opposed to ns_simple test, this time we have
 * multiple files in main executable and the library.
 */

namespace main_ns
{
	USDT_DEFINE_SEMA(main_sema); /* definition */

	/* We don't have to put these declarations inside the namespace (they
	 * would work in global namespace just as fine, as demonstrated by
	 * ns_simple test), but we are doing this here anyways just to test
	 * that putting declarations inside namespaces also work
	 */
	USDT_DECLARE_SEMA(sub_sema); /* declaration */
#ifndef SHARED
	USDT_DECLARE_SEMA(lib_sema); /* declaration */
#endif

	__weak __optimize void main_func(int x)
	{
		USDT_WITH_EXPLICIT_SEMA(main_sema, test, main_main, x);
		USDT_WITH_EXPLICIT_SEMA(sub_sema, test, main_sub, x);
#ifndef SHARED
		USDT_WITH_EXPLICIT_SEMA(lib_sema, test, main_lib, x);
#endif

		printf("main: main_sema is %s.\n", USDT_SEMA_IS_ACTIVE(main_sema) ? "ACTIVE" : "INACTIVE");
		printf("main: sub_sema is %s.\n", USDT_SEMA_IS_ACTIVE(sub_sema) ? "ACTIVE" : "INACTIVE");
#ifndef SHARED
		printf("main: lib_sema is %s.\n", USDT_SEMA_IS_ACTIVE(lib_sema) ? "ACTIVE" : "INACTIVE");
#endif
	}
}

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	main_ns::main_func(1);
	sub_ns::sub_func(2);
	lib_ns::lib_func(3);

	return 0;
}

const char *USDT_SPECS =
"test:main_main base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:main_sub base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
#ifndef SHARED
"test:main_lib base=BASE1 sema=SEMA3 argn=1 args=-4@*.\n"
#endif
"test:sub_main base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:sub_sub base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
#ifndef SHARED
"test:sub_lib base=BASE1 sema=SEMA3 argn=1 args=-4@*.\n"
#endif
#ifdef SHARED
"test:lib_lib base=BASE2 sema=SEMA3 argn=1 args=-4@*.\n"
#else /* !SHARED */
"test:lib_main base=BASE1 sema=SEMA1 argn=1 args=-4@*.\n"
"test:lib_sub base=BASE1 sema=SEMA2 argn=1 args=-4@*.\n"
"test:lib_lib base=BASE1 sema=SEMA3 argn=1 args=-4@*.\n"
#endif /* SHARED */
;

const char *UNTRACED_OUTPUT =
"main: main_sema is INACTIVE.\n"
"main: sub_sema is INACTIVE.\n"
#ifndef SHARED
"main: lib_sema is INACTIVE.\n"
#endif
"sub: main_sema is INACTIVE.\n"
"sub: sub_sema is INACTIVE.\n"
#ifndef SHARED
"sub: lib_sema is INACTIVE.\n"
"lib: main_sema is INACTIVE.\n"
"lib: sub_sema is INACTIVE.\n"
#endif
"lib: lib_sema is INACTIVE.\n"
;

const char *BPFTRACE_SCRIPT =
"test:main_main { x=%d -> arg0 }\n"
"test:main_sub { x=%d -> arg0 }\n"
#ifndef SHARED
"test:main_lib { x=%d -> arg0 }\n"
#endif /* SHARED */
"test:sub_main { x=%d -> arg0 }\n"
"test:sub_sub { x=%d -> arg0 }\n"
#ifndef SHARED
"test:sub_lib { x=%d -> arg0 }\n"
#endif /* SHARED */
#ifdef SHARED
"lib:test:lib_lib { x=%d -> arg0 }\n"
#else /* !SHARED */
"test:lib_main { x=%d -> arg0 }\n"
"test:lib_sub { x=%d -> arg0 }\n"
"test:lib_lib { x=%d -> arg0 }\n"
#endif
;

const char *BPFTRACE_OUTPUT =
"test:main_main: x=1\n"
"test:main_sub: x=1\n"
#ifndef SHARED
"test:main_lib: x=1\n"
#endif /* SHARED */
"test:sub_main: x=2\n"
"test:sub_sub: x=2\n"
#ifndef SHARED
"test:sub_lib: x=2\n"
#endif /* SHARED */
#ifdef SHARED
"lib:test:lib_lib: x=3\n"
#else /* !SHARED */
"test:lib_main: x=3\n"
"test:lib_sub: x=3\n"
"test:lib_lib: x=3\n"
#endif
;

const char *TRACED_OUTPUT = ""
"main: main_sema is ACTIVE.\n"
"main: sub_sema is ACTIVE.\n"
#ifndef SHARED
"main: lib_sema is ACTIVE.\n"
#endif
"sub: main_sema is ACTIVE.\n"
"sub: sub_sema is ACTIVE.\n"
#ifndef SHARED
"sub: lib_sema is ACTIVE.\n"
"lib: main_sema is ACTIVE.\n"
"lib: sub_sema is ACTIVE.\n"
#endif
"lib: lib_sema is ACTIVE.\n"
;
