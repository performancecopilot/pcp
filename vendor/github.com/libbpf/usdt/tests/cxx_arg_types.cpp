// SPDX-License-Identifier: BSD-2-Clause
#include <cstdio>
#include <cstddef>
#include "common.h"
#include "../usdt.h"

/* we use __optimize below to ensure we pass registers to USDTs */
__weak __optimize void fn_chars(char a, unsigned char b, signed char c)
{
	USDT(test, chars, a, b, c);
}

__weak __optimize void fn_shorts(unsigned short a, short b)
{
	USDT(test, shorts, a, b);
}

__weak __optimize void fn_ints(unsigned int a, int b)
{
	USDT(test, ints, a, b);
}

__weak __optimize void fn_longs(unsigned long a, long b)
{
	USDT(test, longs, a, b);
}

__weak __optimize void fn_longlongs(unsigned long long a, long long b)
{
	USDT(test, longlongs, a, b);
}

__weak __optimize void fn_ptrs(const void *a, const char *b, int *c)
{
	USDT(test, ptrs, a, b, c);
}

__weak __optimize void fn_arrs(void)
{
	static volatile char str[] = "STRING";
	static volatile int ints[] = { -100, -200, -300 };

	USDT(test, arrs, str, ints);
}

struct st_byte { unsigned char x; };
struct st_reg { int x; };
struct st_regpair { long x, y; };
struct st_large { long long x, y, z; };

__weak __optimize void fn_struct_by_val_reg(struct st_byte a, struct st_reg b)
{
	USDT(test, struct_by_val_reg, a, b);
}

__weak __optimize void fn_struct_by_val_reg_pair(struct st_regpair a)
{
	/* this generates "warning: unsupported size for integer register",
	 * but compiles just fine and records valid USDT args spec
	 */
	USDT(test, struct_by_val_reg_pair, a);
}

__weak __optimize void fn_struct_by_val_stack(struct st_large a)
{
	USDT(test, struct_by_val_stack, a);
}

__weak __optimize void fn_structs_by_ref(struct st_byte *a, struct st_reg *b,
					 struct st_regpair *c, struct st_large *d)
{
	USDT(test, structs_by_ref, a, b, c, d);
}

int main(int argc, char **argv)
{
	int num = 42;
	struct st_byte s1 = { 1 };
	struct st_reg s2 = { 2 };
	struct st_regpair s3 = { 3, 4 };
	struct st_large s4 = { 4, 5, 6 };

	if (handle_args(argc, argv))
		return 0;

	fn_chars(1, 2, -3);
	fn_shorts(4, -5);
	fn_ints(6, -7);
	fn_longs(8, -9);
	fn_longlongs(10, -11);
	fn_ptrs((const void *)&main, "some literal", &num);
	fn_arrs();
	fn_struct_by_val_reg(s1, s2);
	fn_struct_by_val_reg_pair(s3);
	fn_struct_by_val_stack(s4);
	fn_structs_by_ref(&s1, &s2, &s3, &s4);

	return 0;
}

const char *USDT_SPECS =
"test:chars base=BASE1 sema=0 argn=3 args=*@* 1@* -1@*.\n"
"test:shorts base=BASE1 sema=0 argn=2 args=2@* -2@*.\n"
"test:ints base=BASE1 sema=0 argn=2 args=4@* -4@*.\n"
"test:longs base=BASE1 sema=0 argn=2 args=*@* -*@*.\n"
"test:longlongs base=BASE1 sema=0 argn=2 args=8@* -8@*.\n"
"test:ptrs base=BASE1 sema=0 argn=3 args=8@* 8@* 8@*.\n"
"test:arrs base=BASE1 sema=0 argn=2 args=8@* 8@*.\n"
"test:struct_by_val_reg base=BASE1 sema=0 argn=2 args=1@* 4@*.\n"
"test:struct_by_val_reg_pair base=BASE1 sema=0 argn=1 args=16@*.\n"
"test:struct_by_val_stack base=BASE1 sema=0 argn=1 args=24@*.\n"
"test:structs_by_ref base=BASE1 sema=0 argn=4 args=*@* *@* *@* *@*.\n"
;

const char *BPFTRACE_SCRIPT =
"test:chars { arg0=%hhu arg1=%hhu arg2=%hhd -> arg0, arg1, arg2 }\n"
"test:shorts { arg0=%hu arg1=%hd -> arg0, arg1 }\n"
"test:ints { arg0=%u arg1=%d -> arg0, arg1 }\n"
"test:longlongs { arg0=%llu arg1=%lld -> arg0, arg1 }\n"
"test:ptrs { arg0=%p arg1='%s' arg2=&%d -> arg0, str(arg1), *(int32 *)arg2 }\n"
"test:arrs { arg0='%s' arg1=(%d,%d,%d) ->					\
	str(arg0),								\
	*(int32 *)(arg1 + 0), *(int32 *)(arg1 + 4), *(int32 *)(arg1 + 8) }\n"
"test:struct_by_val_reg { arg0=%hhu arg1=%u -> arg0, arg1 }\n"
#if ALLOW_STRUCT_BY_VALUE_TEST
"test:struct_by_val_reg_pair { s.x=%llx -> arg0 }\n" /* captures first half of a struct */
#endif
/* bpftrace can't handle 24-byte struct-by-value case in struct_by_val_stack */
"test:structs_by_ref { a=(%hhu) b=(%d) c=(%lld,%lld) d=(%lld,%lld,%lld) ->	\
	*(uint8 *)arg0, *(int32 *)arg1,						\
	*(int64 *)arg2, *(int64 *)(arg2 + 8),					\
	*(int64 *)arg3, *(int64 *)(arg3 + 8), *(int64 *)(arg3 + 16) }\n"
;

const char *BPFTRACE_OUTPUT =
"test:chars: arg0=1 arg1=2 arg2=-3\n"
"test:shorts: arg0=4 arg1=-5\n"
"test:ints: arg0=6 arg1=-7\n"
"test:longlongs: arg0=10 arg1=-11\n"
"test:ptrs: arg0=0x* arg1='some literal' arg2=&42\n"
"test:arrs: arg0='STRING' arg1=(-100,-200,-300)\n"
"test:struct_by_val_reg: arg0=1 arg1=2\n"
#if ALLOW_STRUCT_BY_VALUE_TEST
"test:struct_by_val_reg_pair: s.x=3\n"
#endif
"test:structs_by_ref: a=(1) b=(2) c=(3,4) d=(4,5,6)\n"
;
