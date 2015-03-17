#define STS_FATAL	-2
#define STS_WARNING	-1
#define STS_OK		0

#define STATE_OK	1
#define STATE_MISSING	2
#define STATE_BAD	3

extern int		index_state;
extern int		meta_state;
extern int		log_state;
extern __pmLogLabel	log_label;

extern char	sep;
extern int	vflag;
extern char	*archbasename;	/* after basename() */
extern char	*archdirname;	/* after dirname() */

extern int pass0(char *);
extern void pass1(__pmContext *, char *);

