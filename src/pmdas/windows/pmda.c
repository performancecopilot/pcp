/*
 * Windows PMDA
 *
 * Only ever installed as a daemon PMDA ... needs shim.exe to call the
 * required Win32 APIs.  Communication is via pipes with shim.exe and
 * a mmap()'d shared memory region.
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 * 
 */

#ident "$Id: pmda.c,v 1.6 2007/02/20 00:08:32 kimbrr Exp $"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "./domain.h"
#include "./shm.h"

/*
 * Windows PMDA
 *
 */

extern pmdaIndom	indomtab[];
extern int		indomtab_sz;
extern pmdaMetric	*metrictab;
extern int		metrictab_sz;

shm_hdr_t		*shm = NULL;

extern void init_data(int);

static int		shm_fd;
static shm_hdr_t	*new_hdr;
static int		hdr_size;
static int		shm_oldsize = 0;

static FILE		*send_f;
static FILE		*recv_f;

static char		response[100];	// response on pipe back from shim.exe
static char		shimprog[MAXPATHLEN];

static int		numatoms = 0;

static void
shm_dump_hdr(FILE *f, char *msg, shm_hdr_t *smp)
{
    int		i;
    int		*p;

    fprintf(f, "[PMDA] Dump shared memory header: %s\n", msg);
    fprintf(f, "magic    0x%8x\n", smp->magic);
    fprintf(f, "size       %8d\n", smp->size);
    fprintf(f, "nseg       %8d\n", smp->nseg);
    fprintf(f, "segment     base     nelt elt_size\n");
    for (i = 0; i < smp->nseg; i++) {
	fprintf(f, "[%5d] %8d %8d %8d\n", i, smp->segment[i].base,
		smp->segment[i].nelt, smp->segment[i].elt_size);
    }
    p = (int *)&((char *)shm)[shm->segment[smp->nseg-1].base];
    fprintf(f, "end      0x%8x\n", *p);
    fflush(f);
}

/*
 * Only called after initialization, so shm is valid.
 * This is the PMDA version ... there is also a shim.exe version that
 * is semantically equivalent.
 */
static void
shm_remap(int newsize)
{
    int		*p;
#ifdef PCP_DEBUG
    static int	first = 1;
#endif

    munmap(shm, (off_t)shm_oldsize);
    shm = (shm_hdr_t *)mmap(NULL, (off_t)newsize, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == NULL) {
	fprintf(stderr, "shm_remap: mmap() failed: %s\n", strerror(errno));
	exit(1);
    }
    shm_oldsize = newsize;

#ifdef PCP_DEBUG
    if (first && (pmDebug & DBG_TRACE_APPL1)) {
	shm_dump_hdr(stderr, "intial shm_remap", shm);
	first = 0;
    }
#endif

    /*
     * Integrity checks
     */
    
    if (shm->magic != SHM_MAGIC) {
	shm_dump_hdr(stderr, "shm_remap: Error: bad magic!", shm);
	exit(1);
    }

    p = (int *)&((char *)shm)[shm->segment[shm->nseg-1].base];
    if (*p != SHM_MAGIC) {
	fprintf(stderr, "shm_remap: Error: bad end segment: 0x%x not 0x%x\n", *p, SHM_MAGIC);
	shm_dump_hdr(stderr, "shm_remap", shm);
	exit(1);
    }

#ifdef PCP_DEBUG
    if (first && (pmDebug & DBG_TRACE_APPL1)) {
	shm_dump_hdr(stderr, "intial shm_remap", shm);
	first = 0;
    }
#endif
}

/*
 * Only called after initialization, so shm is valid
 * This is the PMDA version ... there is also a shim.exe version that
 * is semantically equivalent.
 *
 * new contains just the header with the desired shape elt_size and
 * nelt entries for each segment.
 *
 * Note, each segment in the shm region is only allowed to exand ...
 * contraction would make the re-shaping horribly complicated in the
 * presence of concurrent expansion.  So if the total size is unchanged,
 * none of the segments have changed.  When the total size increases, one
 * or more of the segments have increased.
 */
static void
shm_reshape(shm_hdr_t *new)
{
    int		i;
    int		sts;
    int		base;
    char	*src;
    char	*dst;

    if (new->size == shm->size)
	/* do nothing */
	return;

    /*
     * compute new base offsets and check for any shrinking segments
     */
    base = SHM_ROUND(hdr_size);
    for (i = 0; i < new->nseg; i++) {
	if (new->segment[i].elt_size * new->segment[i].nelt <
	    shm->segment[i].elt_size * shm->segment[i].nelt) {
	    fprintf(stderr, "shm_reshape: Botch: segment[%d] shrank!\n", i);
	    shm_dump_hdr(stderr, "Old", shm);
	    shm_dump_hdr(stderr, "New", new);
	    exit(1);
	}
	new->segment[i].base = base;
	base = SHM_ROUND(base + new->segment[i].elt_size * new->segment[i].nelt);
    }
    new->size = base;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	shm_dump_hdr(stderr, "shm_reshape - Old", shm);
    }
#endif

    lseek(shm_fd, (off_t)(new->size-1), SEEK_SET);
    if ((sts = write(shm_fd, "\377", 1)) != 1) {
	fprintf(stderr, "shm_reshape: write() to expand file to %d bytes failed: %s\n",
	    new->size, strerror(errno));
	exit(1);
    }

    shm->size = new->size;
    shm_remap(new->size);

    /*
     * shift segments from last to first to avoid clobbering good data
     */
    for (i = new->nseg-1; i >=0; i--) {
	if (new->segment[i].base > shm->segment[i].base) {
	    src = (char *)&((char *)shm)[shm->segment[i].base];
	    dst = (char *)&((char *)shm)[new->segment[i].base];
	    /* note, may overlap so memmove() not memcpy() */
	    memmove(dst, src, new->segment[i].nelt * new->segment[i].elt_size);
	}
	else {
	    /* this and earlier ones are not moving */
	    break;
	}
    }

    /* update header */
    for (i = 0; i < shm->nseg; i++) {
	shm->segment[i].base = new->segment[i].base;
	shm->segment[i].elt_size = new->segment[i].elt_size;
	shm->segment[i].nelt = new->segment[i].nelt;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	shm_dump_hdr(stderr, "shm_reshape - New", shm);
    }
#endif
}

static void
init_shm(void)
{
    int		base;
    int		size;
    int		sts;
    int		i;
    int		j;
    int		nseg;
    shm_inst_t	instctl = { 0, "" };

    if ((shm_fd = open(SHM_FILENAME, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
	fprintf(stderr, "init: shm open(%s) failed: %s\n", SHM_FILENAME, strerror(errno));
	exit(1);
    }

    /*
     * we are going to use one segment for each instance domain, so
     * we end up with ...
     *		0	shm_metrictab[]
     *		1	scratch (input and out data)
     *		2	1st indom
     *		3	2nd indom
     *		...
     *		?	end
     */
    nseg = indomtab_sz + 3;

    hdr_size = sizeof(shm_hdr_t) + (nseg-1)*sizeof(shm_seg_t);
    if ((new_hdr = (shm_hdr_t *)malloc(hdr_size)) == NULL) {
	fprintf(stderr, "init: shm malloc hdr: %s\n", strerror(errno));
	exit(1);
    }
    if ((new_hdr = (shm_hdr_t *)malloc(hdr_size)) == NULL) {
	fprintf(stderr, "init: shm malloc hdr: %s\n", strerror(errno));
	exit(1);
    }
    new_hdr->magic = SHM_MAGIC;
    new_hdr->nseg = nseg;
    base = SHM_ROUND(hdr_size);

    /* shm_metrictab[] */
    new_hdr->segment[SEG_METRICS].base = base;
    new_hdr->segment[SEG_METRICS].elt_size = sizeof(shm_metric_t);
    new_hdr->segment[SEG_METRICS].nelt = metrictab_sz;
    size = metrictab_sz * sizeof(shm_metric_t);
    /*
     * don't write this one ... we'll fill it in once the shm region
     * is mapped
     */
    base = SHM_ROUND(base + size);
    lseek(shm_fd, (off_t)base, SEEK_SET);

    /*
     * scratch, always count size in units of one byte ... starts off empty
     */
    new_hdr->segment[SEG_SCRATCH].base = base;
    new_hdr->segment[SEG_SCRATCH].elt_size = 1;
    new_hdr->segment[SEG_SCRATCH].nelt = 0;

    /*
     * indoms, always count in units of a shm_inst_t ... start off empty
     */
    size = sizeof(shm_inst_t);
    for (i = 0; i < indomtab_sz; i++) {
	new_hdr->segment[SEG_INDOM+i].base = base;
	new_hdr->segment[SEG_INDOM+i].elt_size = sizeof(shm_inst_t);
	new_hdr->segment[SEG_INDOM+i].nelt = 1;
	if ((sts = write(shm_fd, &instctl, size)) != size) {
	    fprintf(stderr, "init: shm write() %d bytes failed for indom[%d] -> %d: %s\n",
		sts, i, size, strerror(errno));
	    exit(1);
	}
	base = SHM_ROUND(base + size);
    }

    /* end segment */
    new_hdr->segment[nseg-1].base = base;
    new_hdr->segment[nseg-1].elt_size = sizeof(int);
    new_hdr->segment[nseg-1].nelt = 1;
    base = SHM_ROUND(base + sizeof(int));
    /*
     * messy, need to pad to the SHM_ROUND() alignment of the
     * total size so the shm size and the file size are the same
     */
    j = (base - new_hdr->segment[nseg-1].base) / sizeof(int);
    i = SHM_MAGIC;
    while (j > 0) {
	if ((sts = write(shm_fd, &i, sizeof(int))) != sizeof(int)) {
	    fprintf(stderr, "init: shm write() %d bytes failed for end segment -> %d: %s\n",
		sizeof(int), sts, strerror(errno));
	    exit(1);
	}
	j--;
    }

    new_hdr->size = base;

    /* go back and do the header */
    lseek(shm_fd, (off_t)0, SEEK_SET);
    if ((sts = write(shm_fd, new_hdr, hdr_size)) != hdr_size) {
	fprintf(stderr, "init: shm write() %d bytes failed for hdr -> %d: %s\n",
	    hdr_size, sts, strerror(errno));
	exit(1);
    }

    /* mmap */
    shm = (shm_hdr_t *)mmap(NULL, (off_t)new_hdr->size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == NULL) {
	fprintf(stderr, "init: mmap() failed: %s\n", strerror(errno));
	exit(1);
    }
}

static void
init_shim(int argc, char **argv)
{
    int		send[2];	/* from the master's perspective */
    int		recv[2];
    int		sts;

    /* pipe and launch shim.exe */
    if ((sts = pipe(send)) < 0) {
	fprintf(stderr, "init: send pipe() failed: %s\n", strerror(errno));
	exit(1);
    }
    if ((sts = pipe(recv)) < 0) {
	fprintf(stderr, "init: recv pipe() failed: %s\n", strerror(errno));
	exit(1);
    }
    sts = fork();
    if (sts == 0) {
	/*
	 * child ... re-assign stdin and stdout and exec shim.exe
	 */
	int	i;
	int	newfd;
	char	**newargv;

	close(0);
	newfd = dup(send[0]);
	if (newfd != 0) {
	    fprintf(stderr, "init: fd=%d expecting 0 for stdin\n", newfd);
	    exit(1);
	}
	close(send[0]);
	close(send[1]);
	close(1);
	newfd = dup(recv[1]);
	if (newfd != 1) {
	    fprintf(stderr, "init: fd=%d expecting 1 for stdout\n", newfd);
	    exit(1);
	}
	close(recv[0]);
	close(recv[1]);

	if ((newargv = (char **)malloc((argc+1)*sizeof(char *))) == NULL) {
	    fprintf(stderr, "init: malloc argv[%d]: %s\n",
		(argc+1)*sizeof(char *), strerror(errno));
	    exit(1);
	}
	for (i = 0; i < argc; i++) {
	    newargv[i] = argv[i];
	}
	newargv[i] = NULL;

	execv(shimprog, newargv);
	fprintf(stderr, "init: execv(%s): %s\n", shimprog, strerror(errno));
	exit(1);
    }
    else if (sts < 0) {
	fprintf(stderr, "init: fork() failed: %s\n", strerror(errno));
	exit(1);
    }

    /*
     * parent
     */
    if ((send_f = fdopen(send[1], "w")) == NULL) {
	fprintf(stderr, "init: send fdopen(%d) failed: %s\n", send[1], strerror(errno));
	exit(1);
    }
    setlinebuf(send_f);
    close(send[0]);
    if ((recv_f = fdopen(recv[0], "r")) == NULL) {
	fprintf(stderr, "init: recv fdopen(%d) failed: %s\n", recv[0], strerror(errno));
	exit(1);
    }
    setlinebuf(recv_f);
    close(recv[1]);

    /*
     * initialize shim.exe
     */
    fprintf(send_f, "init\n");
    fflush(send_f);
    if (fgets(response, sizeof(response), recv_f) == NULL) {
	fprintf(stderr, "init: recv EOF: %s\n", strerror(errno));
	exit(1);
    }
    if (strncmp(response, "ok", 2) == 0) {
	if (shm->size != shm_oldsize)
	    shm_remap(shm->size);
    }
    else {
	fprintf(stderr, "init: recv unexpected: %s\n", response);
	exit(1);
    }

}

/*
 * Rebuild the PMDA's indomtab[idx] using the instance data in the
 * corresponding shm seegment.
 */
static void
redo_indom(int idx)
{
    int		seg = SEG_INDOM + idx;
    shm_inst_t	*ip;
    int		numinst;
    int		j;

    if (shm->segment[seg].nelt == 0)
	return;

    ip = (shm_inst_t *)&((char *)shm)[shm->segment[seg].base];
    if (ip->i_name[0] != 'c') {
	return;
    }

    /*
     * indom has changed
     */
    numinst = ip->i_inst;	// really numinst hiding here

    /*
     * number of instances changed, or inst ids changed or inst
     * names changed ... simplest is to rebuild the indomtab[] entry
     */
    for (j = 0; j < indomtab[idx].it_numinst; j++) {
	free(indomtab[idx].it_set[j].i_name);
    }
    indomtab[idx].it_numinst = numinst;
    if ((indomtab[idx].it_set = (pmdaInstid *)realloc(indomtab[idx].it_set,
    numinst * sizeof(pmdaInstid))) == NULL) {
	fprintf(stderr, "redo_indom: realloc indomtab[%d][%d]: %s\n",
	    idx, numinst * sizeof(pmdaInstid), strerror(errno));
	exit(1);
    }
    for (j = 0; j < numinst; j++) {
	indomtab[idx].it_set[j].i_inst = ip[j+1].i_inst;
	if ((indomtab[idx].it_set[j].i_name = strdup(ip[j+1].i_name)) == NULL) {
	    fprintf(stderr, "redo_indom: malloc name[%d][%d] %s [%d]: %s\n",
		idx, j, ip[j+1].i_name, strlen(ip[j+1].i_name), strerror(errno));
	    exit(1);
	}
    }
    ip[0].i_name[0] = ' ';	// clear the changed flag

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "redo indomtab[%d]\n", idx);
	for (j = 0; j < indomtab[idx].it_numinst; j++) {
	    fprintf(stderr, "  [%d] %d \"%s\"\n", j,
		indomtab[idx].it_set[j].i_inst, indomtab[idx].it_set[j].i_name);
	}
	fflush(stderr);
    }
#endif

}

static void
prefetch(int numpmid, pmID pmidlist[])
{
    int		delta, numextra = 0;
    int		sts, i;
    pmID	*dst;
    __pmID_int	extra[2];
    __pmID_int	*pmidp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "prefetch(numpmid=%d, ...)\n", numpmid);
    }
#endif

    /* we have derived filesys metrics, so may need to fetch more... ugh */
    for (i = 0; i < numpmid; i++) {
	pmidp = (__pmID_int *)&pmidlist[i];
	if ((pmidp->cluster == 0) &&
	    (pmidp->item >= 117 && pmidp->item <= 119)) {
	    extra[0] = extra[1] = *pmidp;
	    extra[0].item = 120;
	    extra[1].item = 121;
	    numextra = 2;
	    break;
	}
    }

    delta = (numpmid + numextra) * sizeof(pmID) -
	    shm->segment[SEG_SCRATCH].elt_size * shm->segment[SEG_SCRATCH].nelt;
    if (delta > 0) {
	memcpy(new_hdr, shm, hdr_size);
	new_hdr->segment[SEG_SCRATCH].nelt = (numpmid+numextra) * sizeof(pmID);
	new_hdr->size += delta;
	shm_reshape(new_hdr);
    }
    dst = (pmID *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];
    if (numextra)
	memcpy(dst + numpmid, extra, numextra * sizeof(pmID));
    memcpy(dst, pmidlist, numpmid * sizeof(pmID));
    numatoms = 0;
    fprintf(send_f, "prefetch %d\n", numpmid + numextra);
    fflush(send_f);
    if (fgets(response, sizeof(response), recv_f) == NULL) {
	fprintf(stderr, "prefetch: recv EOF: %s\n", strerror(errno));
	numatoms = PM_ERR_IPC;
	return;
    }
    if (strncmp(response, "err", 3) == 0) {
	sscanf(&response[4], "%d", &sts);
	numatoms = sts;
	return;
    }
    if (strncmp(response, "ok", 2) != 0) {
	numatoms = PM_ERR_IPC;
	return;
    }
    sscanf(&response[3], "%d", &numatoms);
    if (shm->size != shm_oldsize)
	shm_remap(shm->size);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	if (numatoms == 0)
	    fprintf(stderr, "prefetch: no values\n");
	else if (numatoms < 0)
	    fprintf(stderr, "prefetch: Error: %s\n", pmErrStr(numatoms));
	else {
	    int			i;
	    shm_result_t	*rtab;
	    rtab = (shm_result_t *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];
	    for (i = 0; i < numatoms; i++) {
		fprintf(stderr, "prefetch[%d] pmid=%s inst=%d ul=%u ull=%llu\n",
			i, pmIDStr(rtab[i].r_pmid), rtab[i].r_inst,
			rtab[i].r_atom.ul, rtab[i].r_atom.ull);
	    }
	}
	fflush(stderr);
    }
#endif

}

/*
 * wrapper for pmdaFetch which primes the shm region ready for
 * the next fetch
 * ... real callback is fetch_callback()
 */
static int
fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		s;

    prefetch(numpmid, pmidlist);

    /*
     * Update the indom tables, so libpcp_pmda will see the correct
     * list of instances to drive the callback.
     * Really only need to update the indoms that match metrics in
     * the fetch, but this involves
     * 	- searching metrictab[] to find the descriptor
     * 	- seaching indomtab[] to find the matching indom
     *
     * I decided it was not worth the effort ... redo_indom() is
     * really quick if there is nothing to be done!
     */
    for (s = 0; s < indomtab_sz; s++) {
	redo_indom(s);
    }

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
fetch_callback(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int			i, count;
    shm_result_t	*rtab;
    pmAtomValue		myatom;
    __pmID_int		*pmidp;

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "fetch_callback(%s, %d, ...)\n", pmIDStr(mdesc->m_desc.pmid), inst);
	fflush(stderr);
    }
#endif

    /*
     * perhaps it is one of the lucky ones that are derived or 
     * do not use the PDH services
     */
    pmidp = (__pmID_int *)&mdesc->m_desc.pmid;
    if (pmidp->cluster == 0) {
	switch (pmidp->item) {
	    case 106:	/* hinv.physmem */
		myatom.ul = shm->physmem;
		*atom = myatom;
		return 1;
		/*NOTREACHED*/

	    case 107:	/* hinv.ncpu */
		myatom.ul = indomtab[CPU_INDOM].it_numinst;
		*atom = myatom;
		return 1;
		/*NOTREACHED*/

	    case 108:	/* hinv.ndisk */
		myatom.ul = indomtab[DISK_INDOM].it_numinst;
		*atom = myatom;
		return 1;
		/*NOTREACHED*/

	    case 109:	/* kernel.uname.distro */
		myatom.cp = shm->uname;
		*atom = myatom;
		return 1;
		/*NOTREACHED*/

	    case 110:	/* kernel.uname.release */
		myatom.cp = shm->build;
		*atom = myatom;
		return 1;
		/*NOTREACHED*/

	}
    }

    if (numatoms <= 0)
	return numatoms;

    rtab = (shm_result_t *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];

    /*
     * special case the filesystem metrics at this point -
     * mapping the PDH services semantics for these to the
     * saner metrics from other platforms is not pretty...
     */
    if ((pmidp->cluster == 0) &&
	(pmidp->item == 67 || (pmidp->item >= 117 && pmidp->item <= 119))) {
	float used_space, free_space, free_percent;
	unsigned long long used, avail, capacity;
	int item;

	for (count = 0, i = 0; i < numatoms; i++) {
	    if (rtab[i].r_inst != inst)
		continue;
	    if (pmidp->item == 67) {	/* filesys.full, rtab holds %Free */
		atom->f = (1.0 - rtab[i].r_atom.f) * 100.0;
		return 1;
	    }
	    item = ((__pmID_int*)&rtab[i].r_pmid)->item;
	    if (item == 120) {		/* dummy metric, rtab holds FreeMB */
		free_space = ((float)rtab[i].r_atom.ul);
		count++;
	    } else if (item == 121) {	/* dummy metric, rtab holds %Free */
		free_percent = rtab[i].r_atom.f;
		count++;
	    }
	}
	if (count != 2)	/* we need both "dummy" metric values below */
	    return 0;

	used_space = free_space * (1.0 - free_percent);
	used = 1024 * (unsigned long long)used_space;	/* MB to KB */
	avail = 1024 * (unsigned long long)free_space;	/* MB to KB */
	capacity = used + avail;

	if (pmidp->item == 117)		/* filesys.capacity */
	    atom->ull = capacity;
	else if (pmidp->item == 118)	/* filesys.used */
	    atom->ull = used;
	else if (pmidp->item == 119)	/* filesys.free */
	    atom->ull = avail;
	return 1;
    }

    /*
     * search in shm for pmAtomValues previously deposited by prefetch
     */
    for (i = 0; i < numatoms; i++) {
	if (rtab[i].r_pmid == mdesc->m_desc.pmid && rtab[i].r_inst == inst) {
	    *atom = rtab[i].r_atom;
#ifdef PCP_DEBUG
	    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		/* desperate */
		fprintf(stderr, "fetch_callback: success @ prefetch[%d]\n", i);
		fflush(stderr);
	    }
#endif
	    return 1;
	}
    }

    return 0;

}

static int
help(int ident, int type, char **buf, pmdaExt *pmda)
{
    char	*src;
    int		sts;

    fprintf(send_f, "help %d %d\n", ident, type);
    fflush(send_f);
    if (fgets(response, sizeof(response), recv_f) == NULL) {
	fprintf(stderr, "help: recv EOF: %s\n", strerror(errno));
	return PM_ERR_IPC;
    }
    if (strncmp(response, "err", 3) == 0) {
	sscanf(&response[4], "%d", &sts);
	return sts;
    }
    if (strncmp(response, "ok", 2) != 0)
	return PM_ERR_IPC;

    if (shm->size != shm_oldsize)
	shm_remap(shm->size);
    src = (char *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];
    if ((*buf = strdup(src)) == NULL) {
	fprintf(stderr, "help: malloc[%d] failed: %s\n", strlen(src), strerror(errno));
	return -errno;
    }

    return 0;
}

int
instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    int			s;
    int			sts;

    for (s = 0; s < indomtab_sz; s++) {
	if (indomtab[s].it_indom == indom) {
	    /* TODO refresh -> shim <- ok? */
	    redo_indom(s);
	    break;
	}
    }
    if (s == indomtab_sz) {
	fprintf(stderr, "instance: Warning: indom %s not in indomtab[]\n",
		pmInDomStr(indom));
    }

    sts = pmdaInstance(indom, inst, name, result, pmda);

    return sts;
}

/*
 * Initialise the agent.
 */
static void 
pmda_init(pmdaInterface *dp)
{

    pmdaSetFetchCallBack(dp, fetch_callback);
    pmdaInit(dp, indomtab, indomtab_sz, metrictab, metrictab_sz);
    dp->version.two.fetch = fetch;
    dp->version.two.instance = instance;
    dp->version.two.text = help;
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -e exec      pathname to shim executable [shim.exe]\n"
	  "  -l logfile   write log into logfile rather than using default log name\n",
	      stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			err = 0;
    pmdaInterface	desc;
    char		*p;
    int			c;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    pmdaDaemon(&desc, PMDA_INTERFACE_3, pmProgname, WINDOWS,
		"windows.log", NULL);

    snprintf(shimprog, sizeof(shimprog),
    		"%s/windows/shim.exe", pmGetConfig("PCP_PMDAS_DIR"));

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:?" "e:", &desc, &err)) != EOF) {
	switch (c) {
	    case 'e':	/* alternate shim executable */
		strcpy(shimprog, optarg);
		break;

	    case '?':
	    default:
		err++;
		break;
	}
    }
   
    if (err) {
    	usage();
	/*NOTREACHED*/
    }

    pmdaOpenLog(&desc);

    /*
     * initialize the shared memory region, establish the pipe
     * for communication with shim.exe and launch shim.exe
     */
    init_shm();
    init_data(desc.domain);
    init_shim(argc, argv);

    pmda_init(&desc);

    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
    /*NOTREACHED*/
}
