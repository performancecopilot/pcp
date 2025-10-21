/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __SHARED_USDTS_H__
#define __SHARED_USDTS_H__

#include <stdio.h>
#include "../usdt.h"
#include "common.h"

USDT_DECLARE_SEMA(common_sema1);
USDT_DECLARE_SEMA(common_sema2);
USDT_DECLARE_SEMA(common_sema3);

USDT_DECLARE_SEMA(exec_sema);
USDT_DECLARE_SEMA(lib_sema);

static __always_inline void common_usdts(int x)
{
	USDT(test, common_no_sema1, x);
	USDT(test, common_no_sema2, x);
	USDT(test, common_no_sema3, x);

	USDT_WITH_SEMA(test, common_imp_sema1, x);
	USDT_WITH_SEMA(test, common_imp_sema2, x);
	USDT_WITH_SEMA(test, common_imp_sema3, x);
	printf("common_imp_sema1 is %s.\n", USDT_IS_ACTIVE(test, common_imp_sema1) ? "ACTIVE" : "INACTIVE");
	printf("common_imp_sema2 is %s.\n", USDT_IS_ACTIVE(test, common_imp_sema2) ? "ACTIVE" : "INACTIVE");
	printf("common_imp_sema3 is %s.\n", USDT_IS_ACTIVE(test, common_imp_sema3) ? "ACTIVE" : "INACTIVE");

	USDT_WITH_EXPLICIT_SEMA(common_sema1, test, common_exp_sema1, x);
	USDT_WITH_EXPLICIT_SEMA(common_sema2, test, common_exp_sema2, x);
	USDT_WITH_EXPLICIT_SEMA(common_sema3, test, common_exp_sema3, x);
	printf("common_exp_sema1 is %s.\n", USDT_SEMA_IS_ACTIVE(common_sema1) ? "ACTIVE" : "INACTIVE");
	printf("common_exp_sema2 is %s.\n", USDT_SEMA_IS_ACTIVE(common_sema2) ? "ACTIVE" : "INACTIVE");
	printf("common_exp_sema3 is %s.\n", USDT_SEMA_IS_ACTIVE(common_sema3) ? "ACTIVE" : "INACTIVE");
}

static __always_inline void exec_usdts(int x)
{
	USDT(test, exec_no_sema, x);

	USDT_WITH_SEMA(test, exec_imp_sema, x);
	printf("exec_imp_sema is %s.\n", USDT_IS_ACTIVE(test, exec_imp_sema) ? "ACTIVE" : "INACTIVE");

	USDT_WITH_EXPLICIT_SEMA(exec_sema, test, exec_exp_sema, x);
	printf("exec_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(exec_sema) ? "ACTIVE" : "INACTIVE");
}

static __always_inline void lib_usdts(int x)
{
	USDT(test, lib_no_sema, x);

	USDT_WITH_SEMA(test, lib_imp_sema, x);
	printf("lib_imp_sema is %s.\n", USDT_IS_ACTIVE(test, lib_imp_sema) ? "ACTIVE" : "INACTIVE");

	USDT_WITH_EXPLICIT_SEMA(lib_sema, test, lib_exp_sema, x);
	printf("lib_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(lib_sema) ? "ACTIVE" : "INACTIVE");
}

extern void other_file_func(int x);
extern void lib_func(int x);

#endif /* __SHARED_USDTS_H_ */
