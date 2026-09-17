// SPDX-License-Identifier: BSD-2-Clause
#include "common.h"
#include "cxx_comdat.h"

void __optimize cxx_comdat_other_func(int x)
{
	cxx_comdat_probe(x);
}
