typedef struct {
    char	*name;
    pmID	pmid;
    pmDesc	desc;
    int		meta_done;
} pmi_metric;

typedef struct {
    pmInDom	indom;
    int		ninstance;
    char	**name;		// list of external instance names
    int		*inst;		// list of internal instance identifiers
    int		namebuflen;	// names are packed in namebuf[] as
    char	*namebuf;	// required by __pmLogPutInDom()
    int		meta_done;
} pmi_indom;

typedef struct {
    int		midx;		// index into metric[]
    int		inst;		// internal instance identifier
} pmi_handle;

typedef struct {
    int		state;
    char	*archive;
    char	*hostname;
    char	*timezone;
    __pmLogCtl	logctl;
    pmResult	*result;
    int		nmetric;
    pmi_metric	*metric;
    int		nindom;
    pmi_indom	*indom;
    int		nhandle;
    pmi_handle	*handle;
    int		last_sts;
    struct timeval	last_stamp;
} pmi_context;

#define CONTEXT_START	1
#define CONTEXT_ACTIVE	2
#define CONTEXT_END	3

extern int _pmi_stuff_value(pmi_context *, pmi_handle *, const char *);
extern int _pmi_put_result(pmi_context *, pmResult *);
extern int _pmi_end(pmi_context *);
