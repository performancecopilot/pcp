// SPDX-License-Identifier: BSD-2-Clause
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include "../usdt.h"

/* This is more of a demo of intended use of explicitly defined (and shared!)
 * USDT semaphore. We implement a request start/end/progress set of USDTs
 * which are all activated by the same shared explicitly-defined USDT
 * semaphore.
 * This is important because regardless if one of those request tracing USDTs
 * is activated, some of them, or all of them, we need to capture some
 * per-request information (in this demon, request ID and start/end timestamps
 * to calculate latency), so shared semaphore helps with that.
 */

static inline uint64_t get_time_ns(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);

	return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

USDT_DEFINE_SEMA(req_sema);

__weak __optimize void handle_request(int x)
{
	char req_id[128];
	uint64_t start_ts, cur_ts;

	/* avoid expensive extra work if none of relevant USDTs are traced */
	if (USDT_SEMA_IS_ACTIVE(req_sema)) {
		start_ts = get_time_ns();
		snprintf(req_id, sizeof(req_id), "req_#%llu", (unsigned long long)start_ts);
		USDT_WITH_EXPLICIT_SEMA(req_sema, req, req_start, req_id, start_ts, x);
		printf("(tracing) req started: x=%d id=%s\n", x, req_id);
	} else {
		printf("(no tracing) req started: x=%d\n", x);
	}

	x *= 3; /* useful request work */

	if (USDT_SEMA_IS_ACTIVE(req_sema)) {
		cur_ts = get_time_ns();
		USDT_WITH_EXPLICIT_SEMA(req_sema, req, req_progress,
					req_id, cur_ts - start_ts /* elapsed time */, x);
		printf("(tracing) req in progress: x=%d id=%s\n", x, req_id);
	} else {
		printf("(no tracing) req in progress: x=%d\n", x);
	}

	x += 42; /* the rest of useful request work */

	if (USDT_SEMA_IS_ACTIVE(req_sema)) {
		cur_ts = get_time_ns();
		USDT_WITH_EXPLICIT_SEMA(req_sema, req, req_end, req_id,
					cur_ts - start_ts /* duration */, x);
		printf("(tracing) req finished: x=%d id=%s\n", x, req_id);
	} else {
		printf("(no tracing) req finished: x=%d\n", x);
	}
}

int main(int argc, char **argv)
{
	if (handle_args(argc, argv))
		return 0;

	handle_request(1);
	handle_request(2);

	return 0;
}

const char *USDT_SPECS =
"req:req_start base=BASE1 sema=SEMA1 argn=3 args=*.\n"
"req:req_progress base=BASE1 sema=SEMA1 argn=3 args=*.\n"
"req:req_end base=BASE1 sema=SEMA1 argn=3 args=*.\n"
;

const char *UNTRACED_OUTPUT =
"(no tracing) req started: x=1\n"
"(no tracing) req in progress: x=3\n"
"(no tracing) req finished: x=45\n"

"(no tracing) req started: x=2\n"
"(no tracing) req in progress: x=6\n"
"(no tracing) req finished: x=48\n"
;

/* We attach only to req:req_end USDT, but request ID and latency is still
 * collected and provided.
 */
const char *BPFTRACE_SCRIPT =
"req:req_end { id=%s x=%d duration=%llu. -> str(arg0), arg2, arg1 }\n"
;

const char *BPFTRACE_OUTPUT =
"req:req_end: id=req_#* x=45 duration=*.\n"
"req:req_end: id=req_#* x=48 duration=*.\n"
;

const char *TRACED_OUTPUT =
"(tracing) req started: x=1 id=req_#*\n"
"(tracing) req in progress: x=3 id=req_#*\n"
"(tracing) req finished: x=45 id=req_#*\n"

"(tracing) req started: x=2 id=req_#*\n"
"(tracing) req in progress: x=6 id=req_#*\n"
"(tracing) req finished: x=48 id=req_#*\n"
;
