/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef COMMON_H_
#define COMMON_H_

#define __weak __attribute__((weak))
#define __optimize __attribute__((optimize(2)))
#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif

/* Struct-by-value USDT argument currently only works on x86_64 with gcc
 * See: https://github.com/bpftrace/bpftrace/issues/3798
 */
#if defined(__clang__) || defined(__aarch64__)
  #define ALLOW_STRUCT_BY_VALUE_TEST 0
#else
  #define ALLOW_STRUCT_BY_VALUE_TEST 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int handle_args(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H_ */
