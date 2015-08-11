/*
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

/*
 * Get help text from files built using newhelp
 */
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <sys/stat.h>

typedef struct {	/* beware: this data structure mirrored in chkhelp */
    pmID	pmid;
    __uint32_t	off_oneline;
    __uint32_t	off_text;
} help_idx_t;

typedef struct {	/* beware: this data structure mirrored in chkhelp */
    int		dir_fd;
    int		pag_fd;
    int		numidx;
    help_idx_t	*index;
    char	*text;
    int		textlen;
} help_t;

static help_t	*tab = NULL;
static int	numhelp = 0;

/*
 * open the help text files and return a handle on success
 */
int
pmdaOpenHelp(char *fname)
{
    char	pathname[MAXPATHLEN];
    int		sts, size;
    help_idx_t	hdr;
    help_t	*hp;
    struct stat	sbuf;

    for (sts = 0; sts < numhelp; sts++) {
	if (tab[sts].dir_fd == -1)
	    break;
    }
    if (sts == numhelp) {
	sts = numhelp++;
	tab = (help_t *)realloc(tab, numhelp * sizeof(tab[0]));
	if (tab == NULL) {
	    __pmNoMem("pmdaOpenHelp", numhelp * sizeof(tab[0]), PM_RECOV_ERR);
	    numhelp = 0;
	    return -oserror();
	}
    }
    hp = &tab[sts];
    memset(hp, 0, sizeof(*hp));
    hp->dir_fd = -1;
    hp->pag_fd = -1;

    snprintf(pathname, sizeof(pathname), "%s.dir", fname);
    hp->dir_fd = open(pathname, O_RDONLY);
    if (hp->dir_fd < 0) {
	sts = -oserror();
	goto failed;
    }

    if (read(hp->dir_fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	sts = -EINVAL;
	goto failed;
    }

    if (hdr.pmid != 0x50635068 ||
	(hdr.off_oneline & 0xffff0000) != 0x31320000) {
	sts = -EINVAL;
	goto failed;
    }

    hp->numidx = hdr.off_text;
    size = (hp->numidx + 1) * sizeof(help_idx_t);

    hp->index = (help_idx_t *)__pmMemoryMap(hp->dir_fd, size, 0);
    if (hp->index == NULL) {
	sts = -oserror();
	goto failed;
    }

    snprintf(pathname, sizeof(pathname), "%s.pag", fname);
    hp->pag_fd = open(pathname, O_RDONLY);
    if (hp->pag_fd < 0) {
	sts = -oserror();
	goto failed;
    }
    if (fstat(hp->pag_fd, &sbuf) < 0) {
	sts = -oserror();
	goto failed;
    }
    hp->textlen = (int)sbuf.st_size;
    hp->text = (char *)__pmMemoryMap(hp->pag_fd, hp->textlen, 0);
    if (hp->text == NULL) {
	sts = -oserror();
	goto failed;
    }
    return numhelp - 1;

failed:
    pmdaCloseHelp(numhelp-1);
    return sts;
}

/*
 * retrieve pmID help text, ...
 *
 */
char *
pmdaGetHelp(int handle, pmID pmid, int type)
{
    int		i;
    help_t	*hp;

    if (handle < 0 || handle >= numhelp)
	return NULL;
    hp = &tab[handle];

    /* search forwards -- could use binary chop */
    for (i = 1; i <= hp->numidx; i++) {
	if (hp->index[i].pmid == pmid) {
	    if (type & PM_TEXT_ONELINE)
		return &hp->text[hp->index[i].off_oneline];
	    else
		return &hp->text[hp->index[i].off_text];
	}
    }

    return NULL;
}

/*
 * retrieve pmInDom help text, ...
 *
 */
char *
pmdaGetInDomHelp(int handle, pmInDom indom, int type)
{
    int			i;
    help_t		*hp;
    pmID		pmid;
    __pmID_int		*pip = (__pmID_int *)&pmid;

    if (handle < 0 || handle >= numhelp)
	return NULL;
    hp = &tab[handle];

    *pip = *((__pmID_int *)&indom);
    /*
     * set a bit here to disambiguate pmInDom from pmID
     * -- this "hack" is shared between here and newhelp/newhelp.c
     */
    pip->flag = 1;

    /* search backwards ... pmInDom entries are at the end */
    for (i = hp->numidx; i >= 1; i--) {
	if (hp->index[i].pmid == pmid) {
	    if (type & PM_TEXT_ONELINE)
		return &hp->text[hp->index[i].off_oneline];
	    else
		return &hp->text[hp->index[i].off_text];
	}
    }

    return NULL;
}

/*
 * this is only here for chkhelp(1) ... export the control data strcuture
 */
PMDA_CALL void *
__pmdaHelpTab(void)
{
    return (void *)tab;
}

void
pmdaCloseHelp(int handle)
{
    help_t		*hp;

    if (handle < 0 || handle >= numhelp)
	return;
    hp = &tab[handle];

    if (hp->dir_fd != -1)
	close(hp->dir_fd);
    if (hp->pag_fd != -1)
	close(hp->pag_fd);
    if (hp->index != NULL)
	__pmMemoryUnmap((void *)hp->index, (hp->numidx+1) * sizeof(help_idx_t));
    if (hp->text != NULL)
	__pmMemoryUnmap(hp->text, hp->textlen);

    hp->textlen = 0;
    hp->dir_fd = -1;
    hp->pag_fd = -1;
    hp->numidx = 0;
    hp->index = NULL;
    hp->text = NULL;
}
