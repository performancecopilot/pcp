// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include "common.h"
#include "../usdt.h"
#include "ns_shared.h"

namespace sub_ns
{
	USDT_DEFINE_SEMA(sub_sema); /* definition */

	/* We don't have to put these declarations inside the namespace (they
	 * would work in global namespace just as fine, as demonstrated by
	 * ns_simple test), but we are doing this here anyways just to test
	 * that putting declarations inside namespaces also work
	 */
	USDT_DECLARE_SEMA(main_sema); /* declaration */
#ifndef SHARED
	USDT_DECLARE_SEMA(lib_sema); /* declaration */
#endif

	__weak __optimize void sub_func(int x)
	{
		USDT_WITH_EXPLICIT_SEMA(main_sema, test, sub_main, x);
		USDT_WITH_EXPLICIT_SEMA(sub_sema, test, sub_sub, x);
#ifndef SHARED
		/* see comment in sema_tricky.c for why this doesn't work in shared library mode */
		USDT_WITH_EXPLICIT_SEMA(lib_sema, test, sub_lib, x);
#endif

		printf("sub: main_sema is %s.\n", USDT_SEMA_IS_ACTIVE(main_sema) ? "ACTIVE" : "INACTIVE");
		printf("sub: sub_sema is %s.\n", USDT_SEMA_IS_ACTIVE(sub_sema) ? "ACTIVE" : "INACTIVE");
#ifndef SHARED
		printf("sub: lib_sema is %s.\n", USDT_SEMA_IS_ACTIVE(lib_sema) ? "ACTIVE" : "INACTIVE");
#endif
	}
}
