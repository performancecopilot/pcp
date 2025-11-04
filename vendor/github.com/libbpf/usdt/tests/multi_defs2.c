// SPDX-License-Identifier: BSD-2-Clause
#include "common.h"
#include "../usdt.h"

void __optimize other_file_func(int x)
{
	USDT(test, no_sema, x, x, x, x, x);
	USDT_WITH_SEMA(test, sema, x, x, x, x, x);
}
