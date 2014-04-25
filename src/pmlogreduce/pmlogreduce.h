#include "pmapi.h"
#include "impl.h"

#define NUM_SEC_PER_DAY		86400

/*
 * Value control for a metric-instance and the last observed input value.
 * Used for rate conversion and supression of repeating values for
 * instantaneous and discrete metrics
 */
typedef struct value {
    struct value	*next;		/* next for this metric */
    int			inst;		/* instance id */
    pmAtomValue		value;		/* last output value */
    struct timeval	timestamp;	/* time of last output value */
    /*
     * last interval value interpretation ... set in doscan() and used in
     * rewrite()
     */
    int			control;
    int			nobs;		/* number of observations */
    int			nwrap;		/* number of counter wraps */
    pmAtomValue		pvalue;		/* used for counter wrap detection */
} value_t;

/*
 * control values ... bit fields
 */
#define V_INIT	1
#define V_SEEN	2

/*
 * instance domain control
 */
typedef struct {
    int		indom;
    int		numinst;
    int		*inst;
    char	**name;
} indom_t;

/*
 * Metric control record in metric hash list
 */
typedef struct {
    pmDesc	idesc;		/* input archive descriptor */
    pmDesc	odesc;		/* output archive descriptor */
    value_t	*first;		/* list of values, one per instance */
    indom_t	*idp;		/* instance domain control, if any */
    int		mode;		/* have to skip or rewrite the value format */
} metric_t;
#define MODE_NORMAL	0
#define MODE_REWRITE	1
#define MODE_SKIP	2

extern __pmTimeval	current;	/* most recent timestamp overall */
extern char		*iname;		/* name of input archive */
extern pmLogLabel	ilabel;		/* input archive label */
extern int		numpmid;	/* all metrics from the input archive */
extern pmID		*pmidlist;	/* ditto */
extern char		**namelist;	/* ditto */
extern metric_t		*metriclist;	/* ditto */
extern __pmLogCtl	logctl;		/* output archive control */
extern double		targ;		/* -t arg - interval b/n output samples */
extern int		sarg;		/* -s arg - finish after X samples */
extern char		*Sarg;		/* -S arg - window start */
extern char		*Targ;		/* -T arg - window end */
extern char		*Aarg;		/* -A arg - output time alignment */
extern int		varg;		/* -v arg - switch log vol every X */
extern int		zarg;		/* -z arg - use archive timezone */
extern char		*tz;		/* -Z arg - use timezone from user */


extern int	_pmLogGet(__pmLogCtl *, int, __pmPDU **);
extern int	_pmLogPut(FILE *, __pmPDU *);
extern void	newlabel(void);
extern void	writelabel(void);
extern void	newvolume(char *, __pmTimeval *);

extern pmResult *rewrite(pmResult *);
extern void	rewrite_free(void);

extern void	dometric(const char *);
extern void	doindom(pmResult *);
extern void	doscan(struct timeval *);
