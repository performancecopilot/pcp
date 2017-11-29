/*
 * pmlogconv - convert PCP archive logs from V1 to V2 format
 */

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "pcp/pmapi.h"
#include "pcp/libpcp.h"

#define LOG 0
#define META 1

typedef struct {		/* input archive control */
    int		ctx;
    char	*name;
    pmLogLabel	label;
    __pmPDU	*pb[2];
    int		pick[2];
    int		eof[2];
} inarch_t;

static __pmLogCtl	logctl;		/* output archive control */

static inarch_t		inarch;
static pmTimeval	current;	/* most recently output timestamp */

static __pmHashCtl	pmid_done;

static int		exit_status = 0;

extern int _pmLogGet(__pmLogCtl *, int, __pmPDU **);
extern int _pmLogPut(FILE *, __pmPDU *);
extern void _pmUnpackDesc(__pmPDU *, pmDesc *);
extern void rewrite_pdu(__pmPDU *);

/*
 * 
 */
void
writelabel_metati(int do_rewind)
{
    if (do_rewind) __pmRewind(logctl.l_tifp);
    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);


    if (do_rewind) __pmRewind(logctl.l_mdfp);
    logctl.l_label.ill_vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label);
}

void
writelabel_data(void)
{
    logctl.l_label.ill_vol = 0;
    __pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
}

static void
_report(FILE *fp)
{
    off_t	here;
    struct stat	sbuf;

    here = __pmLseek(p, 0L, SEEK_CUR);
    fprintf(stderr, "Error occurred at byte offset %ld into a file of", here);
    if (__pmFstat(fp, &sbuf) < 0)
	fprintf(stderr, ": stat: %s\n", strerror(errno));
    else
	fprintf(stderr, " %ld bytes.\n", sbuf.st_size);
    fprintf(stderr, "The last record, and the remainder of this file will not be merged.\n");
    exit_status = 1;
}

/*
 * pick next archive record
 *	- if next metadata is pmDesc, output this
 *	- else choose youngest data or pmInDom metadata
 *	- if data and pmIndom have same timestamp, choose pmInDom
 */
static int
nextrec(void)
{
    int		sts;
    int		pick = PM_ERR_EOL;
    pmTimeval	*this;
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;

    if (!inarch.eof[META]) {
	if (inarch.pb[META] == (__pmPDU *)0) {
	    /* refill metadata buffer */
	    ctxp = __pmHandleToPtr(inarch.ctx);
	    lcp = ctxp->c_archctl->ac_log;
	    if ((sts = _pmLogGet(lcp, PM_LOG_VOL_META, &inarch.pb[META])) < 0) {
		inarch.eof[META] = 1;
		if (sts != PM_ERR_EOL) {
		    fprintf(stderr, "%s: Error: __pmLogRead[meta %s]: %s\n",
			pmGetProgname(), inarch.name, pmErrStr(sts));
		    _report(lcp->l_mdfp);
		    exit(1);
		}
	    }
	}
    }

    if (!inarch.eof[LOG]) {
	if (inarch.pb[LOG] == (__pmPDU *)0) {
	    ctxp = __pmHandleToPtr(inarch.ctx);
	    lcp = ctxp->c_archctl->ac_log;
	    if ((sts = _pmLogGet(lcp, 0, &inarch.pb[LOG])) < 0) {
		inarch.eof[LOG] = 1;
		if (sts != PM_ERR_EOL) {
		    fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
			pmGetProgname(), inarch.name, pmErrStr(sts));
		    _report(lcp->l_mfp);
		    exit(1);
		}
	    }
	}
    }

    if (!inarch.eof[META] && ntohl(inarch.pb[META][1]) == TYPE_DESC) {
	/* pmDesc entry, output this immediately */
	pmDesc	desc;
	_pmUnpackDesc(inarch.pb[META], &desc);
	if (__pmHashSearch((int)desc.pmid, &pmid_done) != (__pmHashNode *)0) {
	    /* already been processed from another log, skip this one */
	    fprintf(stderr, "Botch: pmDesc for pmid %s seen twice in input archive\n", pmIDStr(desc.pmid));
	    exit(1);
	}
	__pmHashAdd((int)desc.pmid, (void *)0, &pmid_done);
	inarch.pick[META] = TYPE_DESC;
	return 0;
    }

    if (!inarch.eof[LOG]) {
	this = (pmTimeval *)&inarch.pb[LOG][1];
	    inarch.pick[LOG] = 1;
	    pick = 0;
	    current.tv_sec = ntohl(this->tv_sec);
	    current.tv_usec = ntohl(this->tv_usec);
    }

    if (!inarch.eof[META]) {
	this = (pmTimeval *)&inarch.pb[META][2];
	if (ntohl(this->tv_sec) < current.tv_sec ||
	    (ntohl(this->tv_sec) == current.tv_sec && ntohl(this->tv_usec) <= current.tv_usec)) {
		inarch.pick[LOG] = 0;
		inarch.pick[META] = TYPE_INDOM;
		pick = 0;
		current.tv_sec = ntohl(this->tv_sec);
		current.tv_usec = ntohl(this->tv_usec);
	}
    }

    return pick;
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*p;
    int		errflag = 0;
    char	*output;
    char	*pmnsfile = NULL;
    int		needti = 0;
    off_t	old_log_offset;
    off_t	new_log_offset;
    off_t	old_meta_offset;
    off_t	new_meta_offset;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:n:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'n':
	    pmnsfile = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag != 0 || pmnsfile == NULL || optind != argc-2) {
	fprintf(stderr,
"Usage: %s [options] input-archive output-archive\n\
\n\
Options\n\
  -D debug	   standard PCP debug flag\n\
  -n pmnsfile	   PMNS to use (not optional)\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: Error loading namespace: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    inarch.name = argv[optind];
    if ((inarch.ctx = pmNewContext(PM_CONTEXT_ARCHIVE, inarch.name)) < 0) {
	fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		pmGetProgname(), inarch.name, pmErrStr(inarch.ctx));
	exit(1);
    }
    if ((sts = pmGetArchiveLabel(&inarch.label)) < 0) {
	fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n",
	    pmGetProgname(), inarch.name, pmErrStr(sts));
	exit(1);
    }

    inarch.pb[LOG] = inarch.pb[META] = (__pmPDU *)0;
    inarch.eof[LOG] = inarch.eof[META] = 0;
    inarch.pick[LOG] = inarch.pick[META] = 0;

    output = argv[argc-1];
    if ((sts = __pmLogCreate("", output, PM_LOG_VERS02, &logctl)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    logctl.l_label.ill_magic = PM_LOG_MAGIC | PM_LOG_VERS02;
    logctl.l_label.ill_pid = inarch.label.ll_pid;
    logctl.l_label.ill_start.tv_sec = inarch.label.ll_start.tv_sec;
    logctl.l_label.ill_start.tv_usec = inarch.label.ll_start.tv_usec;
    strcpy(logctl.l_label.ill_hostname, inarch.label.ll_hostname);
    strcpy(logctl.l_label.ill_tz, inarch.label.ll_tz);
    writelabel_metati(0);

    /*
     * do meta and log records at the same time ... in the case of an
     * equal match, the log record is processed before the corresponding
     * meta data
     */
    for ( ; ; ) {
	if (nextrec() < 0)
	    break;

	if (inarch.pick[LOG]) {
	    old_log_offset = __pmFtell(logctl.l_mfp);
	    old_meta_offset = __pmFtell(logctl.l_mdfp);
	    if (old_log_offset == 0) {
		/* write label record for data file */
		logctl.l_label.ill_start.tv_sec = current.tv_sec;
		logctl.l_label.ill_start.tv_usec = current.tv_usec;
		writelabel_data();
		old_log_offset = __pmFtell(logctl.l_mfp);
		needti = 1;
	    }

	    /*
	     * ignore 2^31 check for vol switch on output archive ...
	     * if input archive is one volume, would have been unreadable
	     * if too large
	     * if input archive is multivolume, we create a single
	     * volume output, so there is a potential problem here ...
	     * TODO - revisit this issue only if a need arises
	     */

	    /* translate data record from V1 to V2 */
	    rewrite_pdu(inarch.pb[LOG]);

	    /* write data record out */
	    if ((sts = _pmLogPut(logctl.l_mfp, inarch.pb[LOG])) < 0) {
		fprintf(stderr, "%s: Error: _pmLogPut: log data: %s\n",
			pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    /* free data record buffer */
	    free(inarch.pb[LOG]);
	    inarch.pb[LOG] = (__pmPDU *)0;
	    inarch.pick[LOG] = 0;

	    if (needti) {
		__pmFflush(logctl.l_mfp);
		__pmFflush(logctl.l_mdfp);
		new_log_offset = __pmFtell(logctl.l_mfp);
		new_meta_offset = __pmFtell(logctl.l_mdfp);
		__pmFseek(logctl.l_mfp, old_log_offset, SEEK_SET);
		__pmFseek(logctl.l_mdfp, old_meta_offset, SEEK_SET);
		__pmLogPutIndex(&logctl, &current);
		__pmFseek(logctl.l_mfp, new_log_offset, SEEK_SET);
		__pmFseek(logctl.l_mdfp, new_log_offset, SEEK_SET);
		needti = 0;
	    }
	}

	if (inarch.pick[META]) {
	    /* write metadata out and force temporal index update if indom */
	    if (inarch.pick[META] == TYPE_DESC) {
		pmDesc	desc;
		char	**names = NULL;
		int	numnames;
		char	*myname;

		_pmUnpackDesc(inarch.pb[META], &desc);
		if ((numnames = pmNameAll(desc.pmid, &names)) < 0) {
		    myname = strdup(pmIDStr(desc.pmid));
		}
		else {
		    myname = strdup(names[0]);
		    free(names);
		}
		__pmLogPutDesc(&logctl, &desc, 1, &myname);
		free(myname);
	    }
	    else {
		if ((sts = _pmLogPut(logctl.l_mdfp, inarch.pb[META])) < 0) {
		    fprintf(stderr, "%s: Error: _pmLogPut: meta data: %s\n",
			    pmGetProgname(), pmErrStr(sts));
		    exit(1);
		}
		needti = 1;
	    }
	    /* free metadata buffer */
	    free(inarch.pb[META]);
	    inarch.pb[META] = (__pmPDU *)0;
	    inarch.pick[META] = 0;
	}
    }

    writelabel_metati(1);

    exit(exit_status);
}
