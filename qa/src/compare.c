/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <math.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    double	d[3];
    long	l[3];
    char	*endp;
    int		haveDouble = 0;
    int		i, j;
    int		smallVal = 2;
    int		smallDVal = 2.0;
    double	tolerance = 0.1;
    int		c;
    int		err = 0;
    int		nArgs;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "s:t:")) != EOF)
	switch (c) {
	  case 's':			/* small value */
	    smallVal = (int)strtol(optarg, &endp, 0);
	    if (*endp == '.') {
		smallDVal = strtod(optarg, &endp);
		haveDouble = 1;
	    }
	    if (*endp != '\0') {
		fprintf(stderr, "small value \"%s\" was not numeric\n", optarg);
		err++;
	    }
	    break;

	  case 't':			/* tolerance (fraction of max) */
	    tolerance = strtod(optarg, &endp);
	    if (*endp != '\0') {
		fprintf(stderr, "tolerance \"%s\" was not numeric\n", optarg);
		err++;
	    }
	    break;

	  case '?':
	    err++;
	}

    nArgs = argc - optind;
    if (err || nArgs < 2 || nArgs > 3) {
	fprintf(stderr,
		"Usage: %s [-t tolerance] [-s small] number number [number]\n",
		pmProgname);
	exit(1);
    }

    for (i = 0; i < nArgs; i++) {
	if (!haveDouble) {
	    l[i] = strtol(argv[optind + i], &endp, 0);
	    if (*endp == '.') {
		for (j = 0; j < i; j++)
		    d[j] = (double)l[j];
		d[i] = strtod(argv[i], &endp);
		haveDouble = 1;
	    }
	}
	else
	    d[i] = strtod(argv[optind + i], &endp);
	if (*endp != '\0') {
	    fprintf(stderr, "%s: non-numeric argument \"%s\"\n", pmProgname, argv[optind + i]);
	    exit(1);
	}
    }

    if (nArgs == 2)
	/* check for 2 values within within 10% or one small and the delta small */
	if (haveDouble) {
	    double
		delta = fabs(d[1] - d[0]),
		max = d[0] > d[1] ? d[0] : d[1];
	    if (delta / max <= tolerance)
		exit(0);
	    else
		if ((d[0] <= smallDVal || d[1] <= smallDVal) && delta <= smallDVal)
		    exit(0);
	    exit(1);
	}
	else {
	    long
		delta = l[1] > l[0] ? l[1] - l[0] : l[0] - l[1],
		max = l[0] > l[1] ? l[0] : l[1];
	    if ((float)delta / (float)max <= tolerance)
		exit(0);
	    else
		if ((l[0] <= smallVal || l[1] <= smallVal) && delta <= smallVal)
		    exit(0);
	    exit(1);
	}
    else
	if (haveDouble)
	    if (d[0] <= d[1] && d[1] <= d[2])
		exit(0);
	    else
		exit(1);
	else
	    if (l[0] <= l[1] && l[1] <= l[2])
		exit(0);
	    else
		exit(1);
    
}
