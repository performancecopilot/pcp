/*
 * Copyright (c) 2018 Ken McDonell, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logsize.h"

void
do_index(__pmFILE *f, int version)
{
    long	oheadbytes = __pmFtell(f);
    size_t	indexbytes = 0;
    int		nrec = 0;
    int		sts;
    struct stat	sbuf;
    size_t	record_size;
    void	*buffer;

    __pmFstat(f, &sbuf);

    if (version >= PM_LOG_VERS03)
	record_size = 8*sizeof(__pmPDU);
    else
	record_size = 5*sizeof(__pmPDU);
    if ((buffer = (void *)malloc(record_size)) == NULL) {
	pmNoMem("do_index: buffer", record_size, PM_FATAL_ERR);
	/* NOTREACHED */
    }

    while ((sts = __pmFread(buffer, 1, record_size, f)) == record_size) { 
	nrec++;
	indexbytes += record_size;
    }

    printf("  index: %zd bytes [%.0f%%, %d entries]\n", indexbytes, 100*(float)indexbytes/sbuf.st_size, nrec);
    printf("  overhead: %ld bytes [%.0f%%]\n", oheadbytes, 100*(float)oheadbytes/sbuf.st_size);
    sbuf.st_size -= (indexbytes + oheadbytes);
    if (sbuf.st_size != 0)
	printf("  unaccounted for: %lld bytes\n", (long long)sbuf.st_size);

    free(buffer);
    return;
}

