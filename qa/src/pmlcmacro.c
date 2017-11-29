/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise the state bit fiddling macros for pmlogger and friends
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

int
main()
{
    int		boo = 0x96969696;
    int		inlog;
    int		avail;
    int		mand;
    int		on;
    int		state;

    for (inlog = 0; inlog < 2; inlog++) {
	PMLC_SET_INLOG(boo, inlog);
	for (avail = 0; avail < 2; avail++) {
	    PMLC_SET_AVAIL(boo, avail);
	    for (mand = 0; mand < 2; mand++) {
		PMLC_SET_MAND(boo, mand);
		for (on = 0; on < 2; on++) {
		    PMLC_SET_ON(boo, on);
		    printf("low-order bits: %2d\n", boo & 0xf);
		    if ((boo & 0xfffffff0) != 0x96969690)
			printf("high-order bit botch: 0x%x (got) != 0x%x (expect)\n",
				boo & 0xfffffff0, 0x96969690);
		    if (PMLC_GET_ON(boo) != on)
			printf("PMLC_GET_ON botch: %d (got) != %d (expect)\n",
				PMLC_GET_ON(boo), on);
		    if (PMLC_GET_MAND(boo) != mand)
			printf("PMLC_GET_MAND botch: %d (got) != %d (expect)\n",
				PMLC_GET_MAND(boo), mand);
		    if (PMLC_GET_AVAIL(boo) != avail)
			printf("PMLC_GET_AVAIL botch: %d (got) != %d (expect)\n",
				PMLC_GET_AVAIL(boo), avail);
		    if (PMLC_GET_INLOG(boo) != inlog)
			printf("PMLC_GET_INLOG botch: %d (got) != %d (expect)\n",
				PMLC_GET_INLOG(boo), inlog);
		    state = (inlog << 3) | (avail << 2) | (mand << 1) | on;
		    if (PMLC_GET_STATE(boo) != state)
			printf("PMLC_GET_STATE botch: %d (got) != %d (expect)\n",
				PMLC_GET_STATE(boo), state);
		    PMLC_SET_STATE(boo, state);
		    if ((boo & 0xfffffff0) != 0x96969690)
			printf("high-order bit botch: 0x%x (got) != 0x%x (expect)\n",
				boo & 0xfffffff0, 0x96969690);
		}
	    }
	}
    }
    exit(0);
}
