/*
 * Copyright (c) 2017,2021 Red Hat.
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
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
#include "logcheck.h"
#include "../libpcp/src/internal.h"

int		goldenmagic;
char * 		goldenfname;
__pmTimestamp	goldenstart;

#define IS_UNKNOWN	0
#define IS_INDEX	1
#define IS_META		2
#define IS_LOG		3
/*
 * Pass 0 for all files
 * - should only come here if fname exists
 * - check that file contains a number of complete records
 * - the label record and the records for the data and metadata
 *   have this format:
 *   :----------:----------------------:---------:
 *   | int len  |        stuff         | int len |
 *   | header   |        stuff         | trailer |
 *   :----------:----------------------:---------:
 *   and the len fields are in network byte order.
 *   For these records, check that the header length is equal to
 *   the trailer length
 * - for index files, following the label record there should be
 *   a number of complete records, each of which is a __pmLogTI
 *   record, with the fields converted network byte order
 *
 * TODO - repair
 * - truncate metadata and data files ... unconditional or interactive confirm?
 * - mark index as bad and needing rebuild
 * - move access check into here (if cannot open file for reading we're screwed)
 */

/*
 * Already checked len in header and trailer, so just read label
 * directly.
 * If not first file, check label consistency.
 * Checks here mimic those in __pmLogChkLabel().
 */
static int
checklabel(__pmFILE *f, char *fname, int len, int *eol)
{
    __pmLogLabel	label;
    size_t		bytes;
    long		offset = __pmFtell(f);
    __int32_t		magic;
    int			sts = STS_OK;

    /* first read the magic number for sanity and version checking */
    __pmFseek(f, sizeof(__int32_t), SEEK_SET);
    if ((bytes = __pmFread(&magic, 1, sizeof(magic), f)) != sizeof(magic)) {
	if (*eol == 0) fputc('\n', stderr);
	fprintf(stderr, "checklabel(...,%s): botch: magic read returns %zu not %zu as expected\n", fname, bytes, sizeof(magic));
	*eol = 1;
	sts = STS_FATAL;
    }

    magic = ntohl(magic);
    if ((magic & 0xffffff00) != PM_LOG_MAGIC) {
	if (*eol == 0) fputc('\n', stderr);
	fprintf(stderr, "%s: bad label magic number: 0x%x not 0x%x as expected\n",
	    fname, magic & 0xffffff00, PM_LOG_MAGIC);
	*eol = 1;
	sts = STS_FATAL;
    }
    if ((magic & 0xff) != PM_LOG_VERS02 &&
        (magic & 0xff) != PM_LOG_VERS03) {
	if (*eol == 0) fputc('\n', stderr);
	fprintf(stderr, "%s: bad label version: %d not %d or %d as expected\n",
	    fname, magic & 0xff, PM_LOG_VERS02, PM_LOG_VERS03);
	*eol = 1;
	sts = STS_FATAL;
    }

    /* now try to load the label record */
    memset((void *)&label, 0, sizeof(label));
    if ((sts = __pmLogLoadLabel(f, &label)) < 0) {
	/* don't report again if error already reported above */
	if (sts != STS_FATAL) {
	    if (*eol == 0) fputc('\n', stderr);
	    fprintf(stderr, "%s: cannot load label record: %s\n", fname, pmErrStr(sts));
	    *eol = 1;
	}
	sts = STS_FATAL;
    }
    else {
	if (vflag) {
	    if (*eol == 0) fputc('\n', stderr);
	    fprintf(stderr, "%s: label record [magic=0x%08x version=%d vol=%d pid=%d start=",
                fname, label.magic, label.magic & 0xff, label.vol, label.pid);
	    __pmPrintTimestamp(stderr, &label.start);
	    if (label.features != 0) {
		char        *bits = __pmLogFeaturesStr(label.features);
		if (bits != NULL) {
		    fprintf(stderr, " features=0x%x \"%s\"", label.features, bits);
		    free(bits);
		}
		else
		    fprintf(stderr, " features=0x%x \"???\"", label.features);
	    }
	    fprintf(stderr, " host=%s", label.hostname);
	    if (label.timezone)
		fprintf(stderr, " tz=%s", label.timezone);
	    if (label.zoneinfo)
		fprintf(stderr, " zoneinfo=%s", label.zoneinfo);
	    fprintf(stderr, "]\n");
	    *eol = 1;
	}
	if (goldenmagic == 0) {
	    if (sts == STS_OK) {
		/* first good label */
		goldenfname = strdup(fname);
		goldenmagic = magic;
		goldenstart = label.start;
	    }
	} else if ((magic & 0xff) != (goldenmagic & 0xff)) {
	    if (*eol == 0) fputc('\n', stderr);
	    fprintf(stderr, "%s: mismatched label version: %d not %d as expected from %s\n",
				fname, magic & 0xff, magic & 0xff, goldenfname);
	    *eol = 1;
	    sts = STS_FATAL;
	}
	__pmLogFreeLabel(&label);
    }
    __pmFseek(f, offset, SEEK_SET);
    return sts; 
}

int
pass0(char *fname)
{
    int		len;
    int		check;
    int		type;
    int		i;
    int		sts;
    int		nrec = 0;
    int		is = IS_UNKNOWN;
    int		eol;
    char	*p;
    __pmFILE	*f = NULL;
    int		label_ok = STS_OK;
    char	logBase[MAXPATHLEN];
    long	offset = 0;

    goldenmagic = 0;		/* force new label record each time thru' */
    if (goldenfname != NULL) {
	free(goldenfname);
	goldenfname = NULL;
    }

    if ((f = __pmFopen(fname, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open file: %s\n", fname, osstrerror());
	sts = STS_FATAL;
	goto done;
    }
    
    pmstrncpy(logBase, sizeof(logBase), fname);
    if (__pmLogBaseName(logBase) != NULL) {
	/* A valid archive suffix was found */
	p = logBase + strlen(logBase) + 1;
	if (strcmp(p, "index") == 0)
	    is = IS_INDEX;
	else if (strcmp(p, "meta") == 0)
	    is = IS_META;
	else if (isdigit((int)(*p))) {
	    is = IS_LOG;
	}
    }
    if (is == IS_UNKNOWN) {
	/*
	 * should never get here because filter() is supposed to
	 * only include PCP archive file names from scandir()
	 */
	fprintf(stderr, "%s: pass0 botch: bad file name?\n", fname);
	exit(1);
    }

    if (vflag) {
	fprintf(stderr, "%s: start pass0 ... ", fname);
	eol = 0;
    }

    type = 0;
    while ((sts = __pmFread(&len, 1, sizeof(len), f)) == sizeof(len)) {
	len = ntohl(len);
	if (len < 2 * sizeof(len)) {
	    if (vflag && !eol) {
		fputc('\n', stderr);
		eol = 1;
	    }
	    if (nrec == 0)
		fprintf(stderr, "%s: illegal header record length (%d) in label record\n", fname, len);
	    else
		fprintf(stderr, "%s[record %d]: illegal header record length (%d)\n", fname, nrec, len);
	    sts = STS_FATAL;
	    goto done;
	}
	len -= 2 * sizeof(len);
	/*
	 * gobble stuff between header and trailer without looking at it
	 * ... except for the record type in the case of metadata records
	 */
	for (i = 0; i < len; i++) {
	    check = __pmFgetc(f);
	    if (check == EOF) {
		if (vflag && !eol) {
		    fputc('\n', stderr);
		    eol = 1;
		}
		if (nrec == 0)
		    fprintf(stderr, "%s: unexpected EOF in label record body, wanted %d, got %d bytes\n", fname, len, i);
		else
		    fprintf(stderr, "%s[record %d]: unexpected EOF in record body, wanted %d, got %d bytes\n", fname, nrec, len, i);
		sts = STS_FATAL;
		goto done;
	    }
	    if (is == IS_META && i <= 3 && nrec > 0) {
		/*
		 * first word (after len) for metadata record, save type
		 */
		type = (type << 8) | check;
	    }
	}
	if ((sts = __pmFread(&check, 1, sizeof(check), f)) != sizeof(check)) {
	    if (vflag && !eol) {
		fputc('\n', stderr);
		eol = 1;
	    }
	    if (nrec == 0)
		fprintf(stderr, "%s: unexpected EOF in label record trailer, wanted %d, got %d bytes\n", fname, (int)sizeof(check), sts);
	    else
		fprintf(stderr, "%s[record %d]: unexpected EOF in record trailer, wanted %d, got %d bytes\n", fname, nrec, (int)sizeof(check), sts);
	    sts = STS_FATAL;
	    goto done;
	}
	check = ntohl(check);
	if (check < 2 * sizeof(len)) {
	    if (vflag && !eol) {
		fputc('\n', stderr);
		eol = 1;
	    }
	    if (nrec == 0)
		fprintf(stderr, "%s: illegal trailer record length (%d) in label record\n", fname, check);
	    else
		fprintf(stderr, "%s[record %d]: illegal trailer record length (%d)\n", fname, nrec, check);
	    sts = STS_FATAL;
	    goto done;
	}
	len += 2 * sizeof(len);
	if (check != len) {
	    if (vflag && !eol) {
		fputc('\n', stderr);
		eol = 1;
	    }
	    if (nrec == 0)
		fprintf(stderr, "%s: label record length mismatch: header %d != trailer %d\n", fname, len, check);
	    else
		fprintf(stderr, "%s[record %d]: length mismatch: header %d != trailer %d\n", fname, nrec, len, check);
	    sts = STS_FATAL;
	    goto done;
	}

	if (nrec == 0) {
	    int		xsts;
	    xsts = checklabel(f, fname, len - 2 * sizeof(len), &eol);
	    if (label_ok == STS_OK)
		/* just remember first not OK status */
		label_ok = xsts;
	}

	if (is == IS_INDEX) {
	    /* for index files, done label record, now eat index records */
	    size_t	record_size;
	    void	*buffer;

	    if ((goldenmagic & 0xff) >= PM_LOG_VERS03)
		record_size = 8*sizeof(__pmPDU);
	    else
		record_size = 5*sizeof(__pmPDU);
	    if ((buffer = (void *)malloc(record_size)) == NULL) {
		pmNoMem("pass0: index buffer", record_size, PM_FATAL_ERR);
		/* NOTREACHED */
	    }

	    nrec++;
	    while ((sts = __pmFread(buffer, 1, record_size, f)) == record_size) { 
		nrec++;
	    }
	    free(buffer);
	    if (sts != 0) {
		if (vflag && !eol) {
		    fputc('\n', stderr);
		    eol = 1;
		}
		fprintf(stderr, "%s[record %d]: unexpected EOF in index entry, wanted %zd, got %d bytes\n", fname, nrec, record_size, sts);
		index_state = STATE_BAD;
		sts = STS_FATAL;
		goto done;
	    }
	    goto empty_check;
	}
	else if (is == IS_META && nrec > 0) {
	    switch (type) {
		case TYPE_DESC:
		case TYPE_TEXT:
		    /* good for all versions */
		    break;

		case TYPE_INDOM:
		case TYPE_INDOM_DELTA:
		case TYPE_LABEL:
		    /* not good for V2 */
		    if ((goldenmagic & 0xff) == PM_LOG_VERS02) {
			if (vflag && !eol) {
			    fputc('\n', stderr);
			    eol = 1;
			}
			fprintf(stderr, "%s[record %d]: unexpected record type %s (%d) for V2 archive\n", fname, nrec, __pmLogMetaTypeStr(type), type);
			sts = STS_FATAL;
		    }
		    break;

		case TYPE_INDOM_V2:
		case TYPE_LABEL_V2:
		    /* not good for V3 */
		    if ((goldenmagic & 0xff) == PM_LOG_VERS03) {
			if (vflag && !eol) {
			    fputc('\n', stderr);
			    eol = 1;
			}
			fprintf(stderr, "%s[record %d]: unexpected record type %s (%d) for V3 archive\n", fname, nrec, __pmLogMetaTypeStr(type), type);
			sts = STS_FATAL;
		    }
		    break;
	    }
	}
	nrec++;
	offset = __pmFtell(f);
    }
    if (sts != 0) {
	if (vflag && !eol) {
	    fputc('\n', stderr);
	    eol = 1;
	}
	fprintf(stderr, "%s[record %d]: unexpected EOF in record header, wanted %d, got %d bytes\n", fname, nrec, (int)sizeof(len), sts);
	sts = STS_FATAL;
    }
empty_check:
    if (sts != STS_FATAL && nrec < 2) {
	if (vflag && !eol) {
	    fputc('\n', stderr);
	    eol = 1;
	}
	fprintf(stderr, "%s: contains no PCP data\n", fname);
	sts = STS_WARNING;
    }
    /*
     * sts == 0 (from __pmFread) => STS_OK
     */
done:
    if (sts == STS_FATAL && offset > 0) {
	fprintf(stderr, "%s: last valid record ends at offset %ld\n", fname, offset);
    }
    if (is == IS_INDEX) {
	if (sts == STS_OK)
	    index_state = STATE_OK;
	else
	    index_state = STATE_BAD;
    }
    else if (is == IS_META) {
	if (sts == STS_OK)
	    meta_state = STATE_OK;
	else
	    meta_state = STATE_BAD;
    }
    else {
	if (log_state == STATE_OK && sts != STS_OK)
	    log_state = STATE_BAD;
	else if (log_state == STATE_MISSING) {
	    if (sts == STS_OK)
		log_state = STATE_OK;
	    else
		log_state = STATE_BAD;
	}
    }

    if (sts == STS_OK)
	sts = label_ok;

    if (f != NULL)
	__pmFclose(f);

    if (vflag && nrec > 0 && sts != STS_FATAL)
	fprintf(stderr, "found %d records\n", nrec);

    return sts;
}

