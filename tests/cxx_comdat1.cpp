// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include "common.h"
#include "cxx_comdat.h"

/*
 * See cxx_comdat.h: both this file and cxx_comdat2.cpp instantiate
 * cxx_comdat_probe()'s out-of-line COMDAT copy; the linker discards one of
 * them and its .note.stapsdt entry must go with it (and not dangle or fail
 * the link). One or two comdat notes may legitimately survive depending on
 * the linker, so USDT_SPECS pins down just the first one; run_test.sh
 * rejects dangling leftovers.
 */

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	cxx_comdat_probe(1);
	cxx_comdat_other_func(2);

	return 0;
}

const char *USDT_SPECS =
"test:comdat base=* sema=0 argn=1 args=*.\n"
"...\n"
;
