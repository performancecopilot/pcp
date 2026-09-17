// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include "common.h"
#include "../usdt.h"

/*
 * Validate USDT note integrity when the linker garbage-collects sections.
 * This test is compiled with -ffunction-sections -fdata-sections and linked
 * with -Wl,--gc-sections (see Makefile).
 *
 * gc_collected_func() is never called, so its section is eligible for
 * collection. .note.stapsdt is a non-SHF_ALLOC section and linkers differ in
 * whether references from it keep sections alive:
 *   - BFD ld treats them as GC roots, so the unused function (and its
 *     semaphore) survive and the note stays valid;
 *   - LLD does not follow them: the function is collected and the note's
 *     relocation against the collected section silently resolves to
 *     0 + addend, leaving a dangling note whose "location" is the nop's
 *     offset inside the discarded section (tiny bogus value). The implicit
 *     semaphore in .probes and the .stapsdt.base section are collected the
 *     same way, zeroing the note's semaphore/base fields.
 * Tools iterating notes then trip on the dangling entry; e.g. libbpf fails
 * the entire probe, including its valid same-name copies.
 *
 * run_test.sh fails any test whose binary contains a note whose location
 * falls in no executable segment, which is what this test is really
 * asserting.
 * Which of gc_collected_func()'s notes survive (and with what semaphore
 * values) is linker-dependent, so USDT_SPECS only pins down the live
 * probes and accepts anything extra. The BASE* and SEMA* stubs (as opposed
 * to plain *) assert that the kept probes' .stapsdt.base and semaphore
 * references stayed intact (LLD used to garbage-collect those sections and
 * zero out the fields even in surviving notes).
 */

void gc_collected_func(int x)
{
	USDT(test, collected, x);
	USDT_WITH_SEMA(test, collected_sema, x);
}

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	USDT(test, kept, 1);
	USDT_WITH_SEMA(test, kept_sema, 2);

	return 0;
}

const char *USDT_SPECS =
"...\n"
"test:kept base=BASE* sema=0 argn=1 args=*.\n"
"test:kept_sema base=BASE* sema=SEMA* argn=1 args=*.\n"
"...\n"
;
