/*
 * Generate sequences of values with wrapping for testing how
 * tools deal with this.  Note: wrapping of *signed* integers
 * is undefined (according to gcc folks) and they take that as
 * meaning "anything can happen".  For some compilers, that's
 * taken literally at higher optimisation levels, resulting in
 * crashes, infinite loops, early exit; so *anything goes* for
 * this test program.  Do not rely on its output when signed
 * integers are being used (the default, without -u).
 *
 * http://thiemonagel.de/2010/01/signed-integer-overflow/
 * https://patchwork.kernel.org/patch/34925/
 */
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_SAMPLES 20

#ifndef LONGLONG_MAX
#define LONGLONG_MAX        9223372036854775807LL /* max "long long int" */
#endif

#ifndef ULONGLONG_MAX
#define ULONGLONG_MAX       18446744073709551615LLU /* max "unsigned long long" */
#endif

int
main(int argc, char *argv[])
{
    int use_longlong = 0;
    int num_samples = NUM_SAMPLES;
    int 	notsigned = 0;
    int		c;
    extern char	*optarg;

    while ((c = getopt(argc, argv, "ln:u")) != EOF) {
	switch (c) {
	    case 'l':
		/* use long long */
		use_longlong = 1;	
		break;

	    case 'n':
		num_samples = atoi(optarg);
		break;

	    case 'u':
		notsigned = 1;
		break;
        }
    }

    if (! notsigned ) {
        if (! use_longlong) {
	    int x = 0;
	    int prev = 0;
	    int i;
	    double a;

	    for (i=0;i<=num_samples;i++) {
	      x += INT_MAX/2 - 1;
	      a = (double)x - (double)prev; 
	      prev = x;
	      if (i > 0)
		  printf("%.4g\n", a);
	    }
	}
	else {
	    long long x = 0;
	    long long prev = 0;
	    int i;
	    double a;

	    for (i=0;i<=num_samples;i++) {
	      x += LONGLONG_MAX/2 - 1;
	      a = (double)x - (double)prev; 
	      prev = x;
	      if (i > 0)
		  printf("%.4g\n", a);
	    }

	}
    }
    else {
        if (! use_longlong) {
	    unsigned int x = 0;
	    unsigned int prev = 0;
	    int i;
	    double a;

	    for (i=0;i<=num_samples;i++) {
	      x += UINT_MAX/2 - 1;
	      a = (double)x - (double)prev; 
	      prev = x;
	      if (i > 0)
		  printf("%.4g\n", a);
	    }
	}
	else {
	    unsigned long long x = 0;
	    unsigned long long prev = 0;
	    int i;
	    double a;

	    for (i=0;i<=num_samples;i++) {
	      x += ULONGLONG_MAX/2 - 1;
	      a = (double)x - (double)prev; 
	      prev = x;
	      if (i > 0)
		  printf("%.4g\n", a);
	    }

	}
    }

    exit(0);

}
