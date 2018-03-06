extern void do_index(__pmFILE *);
extern void do_meta(__pmFILE *);
extern void do_data(__pmFILE *, char *);

extern int	dflag;		/* detail off by default */
extern int	rflag;		/* replication off by default */
extern int	vflag;		/* verbose off by default */
extern int	thres;		/* cut-off percentage from -x for -d */

/* from libpcp's internal.h */
#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohpmID(a)           (a)
#define __ntohpmInDom(a)        (a)
#else
#define __ntohpmID(a)           ntohl(a)
#define __ntohpmInDom(a)        ntohl(a)
#endif
