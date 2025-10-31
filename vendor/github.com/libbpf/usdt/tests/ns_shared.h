/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __NS_SHARED_H__
#define __NS_SHARED_H__

#include <cstdio>
#include "../usdt.h"
#include "common.h"

namespace main_ns
{
	extern void main_func(int x);
}

namespace sub_ns
{
	extern void sub_func(int x);
}

namespace lib_ns
{
	extern void lib_func(int x);
}

#endif /* __NS_SHARED_H_ */
