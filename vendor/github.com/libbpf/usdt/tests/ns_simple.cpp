// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include "common.h"
#include "../usdt.h"

/* Alright, USDT and USDT semaphores have global visibility and naming, but
 * C++ has its own quirks, so we'll test various cases with USDTs in global
 * and namespaced context.
 */

USDT_DEFINE_SEMA(global_exp_sema); /* definition */

USDT_DECLARE_SEMA(ns_exp_sema); /* declaration */
USDT_DECLARE_SEMA(nested_ns_exp_sema); /* declaration */

__weak __optimize void global_func(int x)
{
	USDT(test, global_no_sema);
	USDT_WITH_SEMA(test, global_imp_sema, x);

	printf("global: global_imp_sema is %s.\n", USDT_IS_ACTIVE(test, global_imp_sema) ? "ACTIVE" : "INACTIVE");

	USDT_WITH_EXPLICIT_SEMA(global_exp_sema, test, global_exp_sema_main, x);
	USDT_WITH_EXPLICIT_SEMA(ns_exp_sema, test, ns_exp_sema_sub_global, x);
	USDT_WITH_EXPLICIT_SEMA(nested_ns_exp_sema, test, nested_ns_exp_sema_sub_global, x);

	printf("global: global_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(global_exp_sema) ? "ACTIVE" : "INACTIVE");
	printf("global: ns_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(ns_exp_sema) ? "ACTIVE" : "INACTIVE");
	printf("global: nested_ns_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(nested_ns_exp_sema) ? "ACTIVE" : "INACTIVE");
}

namespace ns
{
	USDT_DEFINE_SEMA(ns_exp_sema);

	__weak __optimize void ns_func(int x)
	{
		USDT(test, ns_no_sema);
		USDT_WITH_SEMA(test, ns_imp_sema, x);

		printf("ns: ns_imp_sema is %s.\n", USDT_IS_ACTIVE(test, ns_imp_sema) ? "ACTIVE" : "INACTIVE");

		USDT_WITH_EXPLICIT_SEMA(global_exp_sema, test, global_exp_sema_sub_ns, x);
		USDT_WITH_EXPLICIT_SEMA(ns_exp_sema, test, ns_exp_sema_main, x);
		USDT_WITH_EXPLICIT_SEMA(nested_ns_exp_sema, test, nested_ns_exp_sema_sub_ns, x);

		printf("ns: global_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(global_exp_sema) ? "ACTIVE" : "INACTIVE");
		printf("ns: ns_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(ns_exp_sema) ? "ACTIVE" : "INACTIVE");
		printf("ns: nested_ns_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(nested_ns_exp_sema) ? "ACTIVE" : "INACTIVE");
	}
}

namespace ns2 { namespace ns2_nested
{
	USDT_DEFINE_SEMA(nested_ns_exp_sema);

	__weak __optimize void nested_ns_func(int x)
	{
		USDT(test, nested_ns_no_sema);
		USDT_WITH_SEMA(test, nested_ns_imp_sema, x);

		printf("nested_ns: nested_ns_imp_sema is %s.\n", USDT_IS_ACTIVE(test, nested_ns_imp_sema) ? "ACTIVE" : "INACTIVE");

		USDT_WITH_EXPLICIT_SEMA(global_exp_sema, test, global_exp_sema_sub_nested_ns, x);
		USDT_WITH_EXPLICIT_SEMA(ns_exp_sema, test, ns_exp_sema_sub_nested_ns, x);
		USDT_WITH_EXPLICIT_SEMA(nested_ns_exp_sema, test, nested_ns_exp_sema_main, x);

		printf("nested_ns: global_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(global_exp_sema) ? "ACTIVE" : "INACTIVE");
		printf("nested_ns: ns_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(ns_exp_sema) ? "ACTIVE" : "INACTIVE");
		printf("nested_ns: nested_ns_exp_sema is %s.\n", USDT_SEMA_IS_ACTIVE(nested_ns_exp_sema) ? "ACTIVE" : "INACTIVE");
	}
}}

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	global_func(1);
	ns::ns_func(2);
	ns2::ns2_nested::nested_ns_func(3);

	return 0;
}

const char *USDT_SPECS =
"test:global_no_sema base=BASE1 sema=0 argn=0 args=.\n"
"test:global_imp_sema base=BASE1 sema=SEMA1 argn=1 args=*.\n"
"test:global_exp_sema_main base=BASE1 sema=SEMA2 argn=1 args=*.\n"
"test:ns_exp_sema_sub_global base=BASE1 sema=SEMA3 argn=1 args=*.\n"
"test:nested_ns_exp_sema_sub_global base=BASE1 sema=SEMA4 argn=1 args=*.\n"

"test:ns_no_sema base=BASE1 sema=0 argn=0 args=.\n"
"test:ns_imp_sema base=BASE1 sema=SEMA5 argn=1 args=*.\n"
"test:global_exp_sema_sub_ns base=BASE1 sema=SEMA2 argn=1 args=*.\n"
"test:ns_exp_sema_main base=BASE1 sema=SEMA3 argn=1 args=*.\n"
"test:nested_ns_exp_sema_sub_ns base=BASE1 sema=SEMA4 argn=1 args=*.\n"

"test:nested_ns_no_sema base=BASE1 sema=0 argn=0 args=.\n"
"test:nested_ns_imp_sema base=BASE1 sema=SEMA6 argn=1 args=*.\n"
"test:global_exp_sema_sub_nested_ns base=BASE1 sema=SEMA2 argn=1 args=*.\n"
"test:ns_exp_sema_sub_nested_ns base=BASE1 sema=SEMA3 argn=1 args=*.\n"
"test:nested_ns_exp_sema_main base=BASE1 sema=SEMA4 argn=1 args=*.\n"
;

const char *UNTRACED_OUTPUT =
"global: global_imp_sema is INACTIVE.\n"
"global: global_exp_sema is INACTIVE.\n"
"global: ns_exp_sema is INACTIVE.\n"
"global: nested_ns_exp_sema is INACTIVE.\n"
"ns: ns_imp_sema is INACTIVE.\n"
"ns: global_exp_sema is INACTIVE.\n"
"ns: ns_exp_sema is INACTIVE.\n"
"ns: nested_ns_exp_sema is INACTIVE.\n"
"nested_ns: nested_ns_imp_sema is INACTIVE.\n"
"nested_ns: global_exp_sema is INACTIVE.\n"
"nested_ns: ns_exp_sema is INACTIVE.\n"
"nested_ns: nested_ns_exp_sema is INACTIVE.\n"
;

const char *BPFTRACE_SCRIPT =
"test:global_no_sema { triggered }\n"
"test:ns_no_sema { triggered }\n"
"test:nested_ns_no_sema { triggered }\n"

"test:global_imp_sema { x=%d -> arg0 }\n"
"test:ns_imp_sema { x=%d -> arg0 }\n"
"test:nested_ns_imp_sema { x=%d -> arg0 }\n"

/* attach to non-main USDTs with shared explicit semas; they should still activate all semas */
"test:global_exp_sema_sub_ns { x=%d -> arg0 }\n"
"test:ns_exp_sema_sub_nested_ns { x=%d -> arg0 }\n"
"test:nested_ns_exp_sema_sub_global { x=%d -> arg0 }\n"
;

const char *BPFTRACE_OUTPUT =
"test:global_no_sema: triggered\n"
"test:global_imp_sema: x=1\n"
"test:nested_ns_exp_sema_sub_global: x=1\n"

"test:ns_no_sema: triggered\n"
"test:ns_imp_sema: x=2\n"
"test:global_exp_sema_sub_ns: x=2\n"

"test:nested_ns_no_sema: triggered\n"
"test:nested_ns_imp_sema: x=3\n"
"test:ns_exp_sema_sub_nested_ns: x=3\n"
;

/* All semas are active, even if not all USDTs are attached, due to shared explicit semaphores. */
const char *TRACED_OUTPUT = ""
"global: global_imp_sema is ACTIVE.\n"
"global: global_exp_sema is ACTIVE.\n"
"global: ns_exp_sema is ACTIVE.\n"
"global: nested_ns_exp_sema is ACTIVE.\n"
"ns: ns_imp_sema is ACTIVE.\n"
"ns: global_exp_sema is ACTIVE.\n"
"ns: ns_exp_sema is ACTIVE.\n"
"ns: nested_ns_exp_sema is ACTIVE.\n"
"nested_ns: nested_ns_imp_sema is ACTIVE.\n"
"nested_ns: global_exp_sema is ACTIVE.\n"
"nested_ns: ns_exp_sema is ACTIVE.\n"
"nested_ns: nested_ns_exp_sema is ACTIVE.\n"
;
