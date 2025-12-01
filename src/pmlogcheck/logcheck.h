#define STS_FATAL	-2
#define STS_WARNING	-1
#define STS_OK		0

#define STATE_OK	1
#define STATE_MISSING	2
#define STATE_BAD	3

extern char		sep;
extern int		vflag;
extern int		nowrap;
extern int		index_state;
extern int		meta_state;
extern int		log_state;
extern int		mark_count;
extern int		result_count;
extern int		repair;

extern char *		goldenfname;
extern int		goldenmagic;
extern __pmTimestamp	goldenstart;

extern int pass0(char *);
extern int pass1(__pmContext *, char *);
extern int pass2(__pmContext *, char *);
extern int pass3(__pmContext *, char *, pmOptions *);
extern int prompt(char *, char *);
extern int try_truncate(char *, off_t, off_t);
