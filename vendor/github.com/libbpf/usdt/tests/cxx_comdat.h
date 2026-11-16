/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __CXX_COMDAT_H__
#define __CXX_COMDAT_H__

#include "../usdt.h"
#include "common.h"

/*
 * A USDT inside a C++ inline function defined in a header: every TU that
 * uses it emits its own out-of-line copy in a COMDAT group, the linker keeps
 * one copy and discards the rest. The .note.stapsdt entry emitted next to
 * each copy is not part of that COMDAT group, so the notes of the discarded
 * copies are left referencing a discarded section:
 *   - BFD ld fails the link outright ("`.text._Z...' referenced in section
 *     `.note.stapsdt' ... defined in discarded section");
 *   - LLD silently resolves the note's location to 0 + addend, producing a
 *     dangling note (caught by run_test.sh's location validity check).
 *
 * noinline forces the out-of-line COMDAT copy to exist at any optimization
 * level; a fully inlined copy would produce a valid note at every inline
 * site instead.
 */
inline __attribute__((noinline)) void cxx_comdat_probe(int x)
{
	USDT(test, comdat, x);
}

extern void cxx_comdat_other_func(int x);

#endif /* __CXX_COMDAT_H__ */
