// SPDX-License-Identifier: BSD-2-Clause
#include "shared_usdts.h"

/* if we are in shared library mode, we need to define our own common_sema*,
 * because we can't just reference the ones defined in the main executable
 */
#ifdef SHARED
USDT_DEFINE_SEMA(common_sema1);
USDT_DEFINE_SEMA(common_sema2);
USDT_DEFINE_SEMA(common_sema3);
#endif
USDT_DEFINE_SEMA(lib_sema);

void __optimize lib_func(int x)
{
	common_usdts(x);
	lib_usdts(x);
}
