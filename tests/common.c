// SPDX-License-Identifier: BSD-2-Clause
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int handle_args(int argc, char **argv);

extern const char *USDT_SPECS __weak;
extern const char *UNTRACED_OUTPUT __weak;
extern const char *BPFTRACE_SCRIPT __weak;
extern const char *BPFTRACE_OUTPUT __weak;
extern const char *TRACED_OUTPUT __weak;

static void usage(const char *name)
{
	printf("Usage: %s [-U] [-b] [-B] [-t] [-T]\n", name);
}

int handle_args(int argc, char **argv)
{
	int opt;
	bool handled = false;

	while ((opt = getopt(argc, argv, "UbBtT")) != -1) {
		handled = true;
		switch (opt) {
		case 'U':
			printf("%s", &USDT_SPECS ? USDT_SPECS : "");
			continue;
		case 't':
			printf("%s", &UNTRACED_OUTPUT ? UNTRACED_OUTPUT : "");
			continue;
		case 'T':
			printf("%s", &TRACED_OUTPUT ? TRACED_OUTPUT : "");
			continue;
		case 'b':
			printf("%s", &BPFTRACE_SCRIPT ? BPFTRACE_SCRIPT : "");
			continue;
		case 'B':
			printf("%s", &BPFTRACE_OUTPUT ? BPFTRACE_OUTPUT : "");
			continue;
		default:
			usage(argv[0]);
			exit(1);
		}
	}

	if (handled)
		return 1;

	if (optind < argc) {
		usage(argv[0]);
		exit(1);
	}

	return 0;
}

#ifdef __cplusplus
}
#endif
