// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include "common.h"
#include "../usdt.h"
#include "ns_shared.h"

namespace lib_ns
{
	USDT_DEFINE_SEMA(lib_sema); /* definition */

#ifndef SHARED
	/* We don't have to put these declarations inside the namespace (they
	 * would work in global namespace just as fine, as demonstrated by
	 * ns_simple test), but we are doing this here anyways just to test
	 * that putting declarations inside namespaces also work
	 */
	USDT_DECLARE_SEMA(main_sema); /* declaration */
	USDT_DECLARE_SEMA(sub_sema); /* declaration */
#endif

	__weak __optimize void lib_func(int x)
	{
#ifndef SHARED
		USDT_WITH_EXPLICIT_SEMA(main_sema, test, lib_main, x);
		USDT_WITH_EXPLICIT_SEMA(sub_sema, test, lib_sub, x);
#endif
		USDT_WITH_EXPLICIT_SEMA(lib_sema, test, lib_lib, x);

#ifndef SHARED
		printf("lib: main_sema is %s.\n", USDT_SEMA_IS_ACTIVE(main_sema) ? "ACTIVE" : "INACTIVE");
		printf("lib: sub_sema is %s.\n", USDT_SEMA_IS_ACTIVE(sub_sema) ? "ACTIVE" : "INACTIVE");
#endif
		printf("lib: lib_sema is %s.\n", USDT_SEMA_IS_ACTIVE(lib_sema) ? "ACTIVE" : "INACTIVE");
	}
}

