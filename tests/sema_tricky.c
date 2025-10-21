// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "../usdt.h"

USDT_DEFINE_SEMA(exec_sema1); /* defined in the executable */
USDT_DEFINE_SEMA(exec_sema2); /* defined in the executable */

#ifdef SHARED
bool is_exec_sema1_active(void) { return USDT_SEMA_IS_ACTIVE(exec_sema1); }
bool is_exec_sema2_active(void) { return USDT_SEMA_IS_ACTIVE(exec_sema2); }
bool is_exec_imp_sema1_active(void) { return USDT_IS_ACTIVE(test, exec_imp_sema1); }
bool is_exec_imp_sema2_active(void) { return USDT_IS_ACTIVE(test, exec_imp_sema2); }

extern bool is_lib_sema1_active(void);
extern bool is_lib_sema2_active(void);
extern bool is_lib_imp_sema1_active(void);
extern bool is_lib_imp_sema2_active(void);
#else /* !SHARED */
USDT_DECLARE_SEMA(lib_sema1); /* defined in the library */
USDT_DECLARE_SEMA(lib_sema2); /* defined in the library */

static bool is_lib_sema1_active(void) { return USDT_SEMA_IS_ACTIVE(lib_sema1); }
static bool is_lib_sema2_active(void) { return USDT_SEMA_IS_ACTIVE(lib_sema2); }
static bool is_lib_imp_sema1_active(void) { return USDT_IS_ACTIVE(test, lib_imp_sema1); }
static bool is_lib_imp_sema2_active(void) { return USDT_IS_ACTIVE(test, lib_imp_sema2); }
#endif

extern void lib_func(void);

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	USDT_WITH_SEMA(test, exec_imp_sema1);
	USDT_WITH_SEMA(test, exec_imp_sema2);
	printf("exec: exec_imp_sema1 is %s.\n", USDT_IS_ACTIVE(test, exec_imp_sema1) ? "ACTIVE" : "INACTIVE");
	printf("exec: exec_imp_sema2 is %s.\n", USDT_IS_ACTIVE(test, exec_imp_sema2) ? "ACTIVE" : "INACTIVE");

	USDT_WITH_EXPLICIT_SEMA(exec_sema1, test, exec_exp_sema1);
	USDT_WITH_EXPLICIT_SEMA(exec_sema2, test, exec_exp_sema2);
	printf("exec: exec_exp_sema1 is %s.\n", USDT_SEMA_IS_ACTIVE(exec_sema1) ? "ACTIVE" : "INACTIVE");
	printf("exec: exec_exp_sema2 is %s.\n", USDT_SEMA_IS_ACTIVE(exec_sema2) ? "ACTIVE" : "INACTIVE");

	/* Sharing USDT semaphores (both implicit and explicit) isn't
	 * supported and doesn't work between shared libraries or between
	 * executable and shared library. So the direct "library" semaphore
	 * references only work if library is statically linked library, which
	 * becomes part of the main executable (so there is, really, no
	 * library involved as far as ELF and linker are concerned). So for
	 * shared library case library provides an API returning the state of
	 * the USDT, which we then use to check the state.
	 */
#ifndef SHARED
	/* executable-side USDTs that use library-defined explicit semaphore */
	USDT_WITH_EXPLICIT_SEMA(lib_sema1, test, lib_sema_exec_side1);
	USDT_WITH_EXPLICIT_SEMA(lib_sema2, test, lib_sema_exec_side2);
#endif /* SHARED */
	printf("exec: lib_imp_sema1 is %s.\n", is_lib_imp_sema1_active() ? "ACTIVE" : "INACTIVE");
	printf("exec: lib_imp_sema2 is %s.\n", is_lib_imp_sema2_active() ? "ACTIVE" : "INACTIVE");
	printf("exec: lib_exp_sema1 is %s.\n", is_lib_sema1_active() ? "ACTIVE" : "INACTIVE");
	printf("exec: lib_exp_sema2 is %s.\n", is_lib_sema2_active() ? "ACTIVE" : "INACTIVE");

	lib_func();

	return 0;
}

const char *USDT_SPECS =
/* executable-only USDTs */
"test:exec_imp_sema1 base=BASE1 sema=SEMA1 argn=0 args=.\n"
"test:exec_imp_sema2 base=BASE1 sema=SEMA2 argn=0 args=.\n"
"test:exec_exp_sema1 base=BASE1 sema=SEMA3 argn=0 args=.\n"
"test:exec_exp_sema2 base=BASE1 sema=SEMA4 argn=0 args=.\n"
#ifdef SHARED /* shared library */
/* library-only USDTs */
"test:lib_imp_sema1 base=BASE2 sema=SEMA5 argn=0 args=.\n"
"test:lib_imp_sema2 base=BASE2 sema=SEMA6 argn=0 args=.\n"
"test:lib_exp_sema1 base=BASE2 sema=SEMA7 argn=0 args=.\n"
"test:lib_exp_sema2 base=BASE2 sema=SEMA8 argn=0 args=.\n"
#else /* !SHARED */
/* exec/library shared USDTs */
"test:lib_sema_exec_side1 base=BASE1 sema=SEMA5 argn=0 args=.\n"
"test:lib_sema_exec_side2 base=BASE1 sema=SEMA6 argn=0 args=.\n"
"test:exec_sema_lib_side1 base=BASE1 sema=SEMA3 argn=0 args=.\n"
"test:exec_sema_lib_side2 base=BASE1 sema=SEMA4 argn=0 args=.\n"
/* library-only USDTs */
"test:lib_imp_sema1 base=BASE1 sema=SEMA7 argn=0 args=.\n"
"test:lib_imp_sema2 base=BASE1 sema=SEMA8 argn=0 args=.\n"
"test:lib_exp_sema1 base=BASE1 sema=SEMA5 argn=0 args=.\n"
"test:lib_exp_sema2 base=BASE1 sema=SEMA6 argn=0 args=.\n"
#endif /* SHARED */
;

const char *UNTRACED_OUTPUT =
/* executable output */
"exec: exec_imp_sema1 is INACTIVE.\n"
"exec: exec_imp_sema2 is INACTIVE.\n"
"exec: exec_exp_sema1 is INACTIVE.\n"
"exec: exec_exp_sema2 is INACTIVE.\n"
/* executable output */
"exec: lib_imp_sema1 is INACTIVE.\n"
"exec: lib_imp_sema2 is INACTIVE.\n"
"exec: lib_exp_sema1 is INACTIVE.\n"
"exec: lib_exp_sema2 is INACTIVE.\n"
/* library output */
"lib: exec_imp_sema1 is INACTIVE.\n"
"lib: exec_imp_sema2 is INACTIVE.\n"
"lib: exec_exp_sema1 is INACTIVE.\n"
"lib: exec_exp_sema2 is INACTIVE.\n"
/* library output */
"lib: lib_imp_sema1 is INACTIVE.\n"
"lib: lib_imp_sema2 is INACTIVE.\n"
"lib: lib_exp_sema1 is INACTIVE.\n"
"lib: lib_exp_sema2 is INACTIVE.\n"
;

/* See shared_usdts test for explanation of somewhat unintuitive behavior of
 * semaphores shared between executable and library (and how the behavior is
 * influenced by static vs shared library differences).
 *
 * As opposed to shared_usdts test, this test concentrates on the use case
 * where USDTs are not shared (i.e., they are triggered either only in the
 * executable or only in the library), but semaphores are checked in both
 * places.
 *
 * Note, both for executable and library USDTs we do not attach to the second
 * set of USDTs to validate that those semaphores will remain inactive.
 */
const char *BPFTRACE_SCRIPT =
"test:exec_imp_sema1 { triggered }\n"
"test:exec_exp_sema1 { triggered }\n"
#ifdef SHARED
"lib:test:lib_imp_sema1 { triggered }\n"
"lib:test:lib_exp_sema1 { triggered }\n"
#else /* !SHARED */
"test:lib_imp_sema1 { triggered }\n"
"test:lib_exp_sema1 { triggered }\n"
#endif /* SHARED */
;

const char *BPFTRACE_OUTPUT =
/* executable output */
"test:exec_imp_sema1: triggered\n"
"test:exec_exp_sema1: triggered\n"
/* library output */
#ifdef SHARED
"lib:test:lib_imp_sema1: triggered\n"
"lib:test:lib_exp_sema1: triggered\n"
#else /* !SHARED */
"test:lib_imp_sema1: triggered\n"
"test:lib_exp_sema1: triggered\n"
#endif /* SHARED */
;

const char *TRACED_OUTPUT =
/* executable output */
"exec: exec_imp_sema1 is ACTIVE.\n"
"exec: exec_imp_sema2 is INACTIVE.\n"
"exec: exec_exp_sema1 is ACTIVE.\n"
"exec: exec_exp_sema2 is INACTIVE.\n"
/* executable output */
"exec: lib_imp_sema1 is ACTIVE.\n"
"exec: lib_imp_sema2 is INACTIVE.\n"
"exec: lib_exp_sema1 is ACTIVE.\n"
"exec: lib_exp_sema2 is INACTIVE.\n"
/* library output */
"lib: exec_imp_sema1 is ACTIVE.\n"
"lib: exec_imp_sema2 is INACTIVE.\n"
"lib: exec_exp_sema1 is ACTIVE.\n"
"lib: exec_exp_sema2 is INACTIVE.\n"
/* library output */
"lib: lib_imp_sema1 is ACTIVE.\n"
"lib: lib_imp_sema2 is INACTIVE.\n"
"lib: lib_exp_sema1 is ACTIVE.\n"
"lib: lib_exp_sema2 is INACTIVE.\n"
;
