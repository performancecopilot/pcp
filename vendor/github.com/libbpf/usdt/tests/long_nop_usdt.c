// SPDX-License-Identifier: BSD-2-Clause
#if __x86_64__
#define USDT_NOP .byte 0x0f, 0x1f, 0x44, 0x00, 0x00 /* nop5 */
#endif
#include "arg_types.c"
