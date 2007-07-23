/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: pmc_format.c++,v 1.1 2005/05/26 06:38:49 kenmcd Exp $"

//
// Test PMC_Metric::formatValue routines
//

#include <iostream.h>
#include "PMC.h"
#include "Metric.h"

#pragma instantiate PMC_List<PMC_Source*>
#pragma instantiate PMC_List<PMC_Metric*>
#pragma instantiate PMC_List<PMC_Context*>

extern char *pmProgname;

int
main(int argc, char *argv[])
{
    double	d;
    char	*endptr;

    pmProgname = basename(argv[0]);
    if (argc != 2) {
	cerr << "Usage: " << pmProgname << " double" << endl;
	exit(1);
	/*NOTREACHED*/
    }

    d = strtod(argv[1], &endptr);
    if (endptr != NULL && endptr[0] != '\0') {
	cerr << pmProgname << ": argument \"" << argv[1] 
	     << "\" must be a double (\"" << endptr << "\")" << endl;
	exit(1);
	/*NOTREACHED*/
    }

    cout << PMC_Metric::formatNumber(d) << endl;

    return 0;
}
