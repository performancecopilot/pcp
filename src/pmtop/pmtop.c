/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 1999 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"

/*
 * Outputs psinfo.psargs and top value.
 *
 * Want to periodically write to stdout
 * with the top processes for the following categories:
 *
 * cpuburn - rate converted proc.psusage.utime + stime
 * syscalls - rate converted proc.psusage.pu_sysc
 * reads - rate converted proc.psusage.bread + gbread 
 * writes - rate converted proc.psusage.bwrit + gbwrit
 * ctxswitch - rate converted proc.psusage.pu_vctx + proc.psusage.pu_ictx
 * residentsize - proc.psusage.rss
 * 
 */

#define MAX_PMID 15 /* gives enough space for all possible metrics in fetch */
char    *namelist[MAX_PMID];
pmID	pmidlist[MAX_PMID];
int	type_tab[MAX_PMID]; /* the types of the metrics */
int	valfmt_tab[MAX_PMID]; /* the types of the metrics */
int	num_pmid;
int	num_proc_pmid;

/* indexes into [MAX_PMID] arrays */
int	kernel_utime;
int	kernel_stime;
int	kernel_sysc;
int	kernel_ctx;
int	pu_utime;
int	pu_stime;
int	pu_sysc;
int	pu_bread;
int	pu_gbread;
int	pu_bwrit;
int	pu_gbwrit;
int	pu_rss;
int	pu_vctx;
int	pu_ictx;

char	*hostname;
int	top = 5;
long	num_samples = -1;
int	num_inst;
int	*instances;
char	**instance_names;
char	*runtime;
char	pid_label_fmt[8];
char	pid_fmt[8];
char	*line_fmt = "%.79s";
int	pid_len;
pmInDom proc_indom;

/* values of interest */
#define CPU_VAL 	0
#define SYSCALLS_VAL	1
#define CTX_VAL		2
#define WRITES_VAL	3
#define READS_VAL	4
#define RSS_VAL		5
#define USR_VAL		6
#define SYS_VAL		7
#define NUM_VALUES 	8

/* which values to calculate and print */
#define SHOW_CPU	(1<<CPU_VAL)
#define SHOW_SYSCALLS	(1<<SYSCALLS_VAL)
#define SHOW_CTX	(1<<CTX_VAL)
#define SHOW_WRITES	(1<<WRITES_VAL)
#define SHOW_READS	(1<<READS_VAL)
#define SHOW_RSS	(1<<RSS_VAL)
#define SHOW_USR	(1<<USR_VAL)
#define SHOW_SYS	(1<<SYS_VAL)

int	show_spec = SHOW_CPU|SHOW_SYSCALLS|SHOW_CTX|SHOW_WRITES|SHOW_READS|SHOW_RSS; /* default to show all */

typedef struct {
    char        *name;
    int         bit;
} show_map_t;

static show_map_t show_map[] = {
    { "CPU",   SHOW_CPU },
    { "SYSC",  SHOW_SYSCALLS },
    { "CTX",   SHOW_CTX },
    { "WRITE", SHOW_WRITES },
    { "READ",  SHOW_READS },
    { "RSS",   SHOW_RSS },
    { "USR",   SHOW_USR },
    { "SYS",   SHOW_SYS },
};
static int num_show = sizeof(show_map) / sizeof(show_map[0]);

typedef struct {
   int		inst; /* index into instances and names */
   double	values[NUM_VALUES];
} rate_entry_t;

static rate_entry_t *rate_tab;

double global_rates[NUM_VALUES];
double sum_rates[NUM_VALUES];

typedef struct {
   char		*val_fmt;
   char		*label_fmt;
   char		*label;
   char		*units;
   char		*fullname;
} format_entry_t;

/*
 * Warning - must match order of *_VAL macros
 */
format_entry_t format_tab[] =
{ 
    {"%7.2f", "%7s", "CPU%", "", "CPU Utilization"},
    {"%9.0f", "%9s", "SYSCALLS", "sys/sec", "System Calls"},
    {"%9.0f", "%9s", "CTX", "ctx/sec", "Context Switches"},
    {"%8.0f", "%8s", "WRITES", "Kb/sec", "Writes"},
    {"%8.0f", "%8s", "READS", "Kb/sec", "Reads"},
    {"%9.0f", "%9s", "RSS", "Kb", "Resident Size"},
    {"%7.2f", "%7s", "USR%", "", "User CPU Utilization"},
    {"%7.2f", "%7s", "SYS%", "", "System CPU Utilization"},
};

/* value table */
typedef struct {
    int		num;
    pmValue	values[MAX_PMID][2];
} val_entry_t;

val_entry_t *val_tab;

/* index will use high end for globals */
/* parallels index with other MAX_PMID arrays */
pmValue global_val[MAX_PMID][2]; 

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif

static void get_indom(void);
static int overrides(int, pmOptions *);

pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General Options"),
    PMOPT_HOST,
    PMOPT_SAMPLES,
    PMOPT_INTERVAL,
    PMAPI_OPTIONS_HEADER("Reporting Options"),
    { "", 1, 'm', "N", "report the top N values only" },
    { "", 1, 'p', "SPEC", "report certain values e.g. cpu,sysc,rss" },
    { "wide", 0, 'w', 0, "wide output for command name" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

pmOptions opts = {
    .short_options = "D:h:m:p:s:t:wzZ:?",
    .long_options = longopts,
    .override = overrides,
};

static int
parse_show_spec(const char *spec)
{
    int		val = 0;
    int		tmp;
    const char	*p;
    char	*pend;
    int		i;

    for (p = spec; *p; ) {
	tmp = (int)strtol(p, &pend, 10);
	if (tmp == -1)
	    /* special case ... -1 really means set all the bits! */
	    tmp = INT_MAX;
	if (*pend == '\0') {
	    val |= tmp;
	    break;
	}
	else if (*pend == ',') {
	    val |= tmp;
	    p = pend + 1;
	}
	else {
	    pend = strchr(p, ',');
	    if (pend != NULL)
		*pend = '\0';

	    for (i = 0; i < num_show; i++) {
		if (strcasecmp(p, show_map[i].name) == 0) {
		    val |= show_map[i].bit;
		    if (pend != NULL) {
			*pend = ',';
			p = pend + 1;
		    }
		    else
			p = "";		/* force termination of outer loop */
		    break;
		}
	    }

	    if (i == num_show) {
		if (pend != NULL)
		    *pend = ',';
		return PM_ERR_CONV;
	    }
	}
    }

    return val;
}

static char *
get_time(void)
{
    static char str[80];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(str, sizeof(str), "%c %Z", tm);
    return str;
}

static __uint64_t
get_value(int metric, pmValue *value)
{
    int sts;
    pmAtomValue val;

    if ((sts = pmExtractValue(valfmt_tab[metric], value,
		  type_tab[metric], &val, PM_TYPE_U64)) < 0) {
	fprintf(stderr, "%s: Failed to extract metric value: %s\n", 
		pmProgname, pmErrStr(sts));
	exit(1);

    }
    return val.ull;
}

static int
cpuburn_cmp(const void *x, const void *y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[CPU_VAL] < c2->values[CPU_VAL])
	return -1;
    if (c2->values[CPU_VAL] > c1->values[CPU_VAL])
	return 1;
    return 0;
}

static int
sys_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[SYS_VAL] < c2->values[SYS_VAL])
	return -1;
    if (c2->values[SYS_VAL] > c1->values[SYS_VAL])
	return 1;
    return 0;
}

static int 
usr_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[USR_VAL] < c2->values[USR_VAL])
	return -1;
    if (c2->values[USR_VAL] > c1->values[USR_VAL])
	return 1;
    return 0;
}

static int
syscall_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[SYSCALLS_VAL] < c2->values[SYSCALLS_VAL])
	return -1;
    if (c2->values[SYSCALLS_VAL] > c1->values[SYSCALLS_VAL])
	return 1;
    return 0;
}

static int
ctx_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[CTX_VAL] < c2->values[CTX_VAL])
	return -1;
    if (c2->values[CTX_VAL] > c1->values[CTX_VAL])
	return 1;
    return 0;
}

static int 
write_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[WRITES_VAL] < c2->values[WRITES_VAL])
	return -1;
    if (c2->values[WRITES_VAL] > c1->values[WRITES_VAL])
	return 1;
    return 0;
}

static int 
read_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[READS_VAL] < c2->values[READS_VAL])
	return -1;
    if (c2->values[READS_VAL] > c1->values[READS_VAL])
	return 1;
    return 0;
}

int 
rss_cmp(const void* x, const void* y)
{
    rate_entry_t *c1 = (rate_entry_t *)x;
    rate_entry_t *c2 = (rate_entry_t *)y;

    if (c1->values[RSS_VAL] < c2->values[RSS_VAL])
	return -1;
    if (c2->values[RSS_VAL] > c1->values[RSS_VAL])
	return 1;
    return 0;
}

/*
 * sum the rate values for the top entries
 */
void
calc_sum_rates(int num_entries, int val_type)
{
    int i;
    double sum = 0;

    for (i = 0; i < min(top, num_entries); i++) {
	rate_entry_t *entry = &rate_tab[i];
	sum += entry->values[val_type];
    }

    sum_rates[val_type] = sum;
}

void
print_top(rate_entry_t *rate_tab, int num_entries, int val_type)
{
    static char line[2048];
    char *ptr;
    int i,j;
    double x;

    /* --- section heading --- */
    if (global_rates[val_type] <= 0) {
	printf("%s\n", format_tab[val_type].label);
    }
    else {
	x = 100*sum_rates[val_type]/global_rates[val_type];
	printf("%s - top %d processes account for %.0f%% of %s\n",
	       format_tab[val_type].label,
	       top, x>100?100:x,
	       format_tab[val_type].fullname);
    }

    /* --- fields heading --- */
    printf(pid_label_fmt, "PID");
    for(j=0; j < NUM_VALUES; j++) {
	if (show_spec & (1<<j)) {
	    format_entry_t *fmt = &format_tab[j];
	    printf(fmt->label_fmt, fmt->label);
	}
    } 
    printf(" CMD\n");

    /* units */
    printf(pid_label_fmt, " ");
    for(j=0; j < NUM_VALUES; j++) {
	if (show_spec & (1<<j)) {
	    format_entry_t *fmt = &format_tab[j];
	    printf(fmt->label_fmt, fmt->units);
	}
    } 
    printf("\n");


    /* --- values --- */
    for(i=0; i < min(top, num_entries); i++) {
	rate_entry_t *entry = &rate_tab[i];

	*line = '\0';
	ptr = line;

	/* pid */
	sprintf(ptr, pid_fmt, instances[entry->inst]);
	ptr += strlen(ptr);

	/* other values */
	for(j=0; j < NUM_VALUES; j++) {
	    if (show_spec & (1<<j)) {
		format_entry_t *fmt = &format_tab[j];
		sprintf(ptr, fmt->val_fmt, entry->values[j]);
		ptr += strlen(ptr);
	    }
	}

	/* cmd */
        /* skip over leading pid-name in instance name */
	sprintf(ptr, " %s", &instance_names[entry->inst][pid_len+1]);

	/* ensure line fits in certain width */
	printf(line_fmt, line);
	printf("\n");
    }

    printf("\n");
}

void 
doit(void)
{
    int i, r, m, v, sts;
    static double now = 0, then = 0;
    double delta_d;
    static unsigned long num_times = 0;
    static pmResult *result[2];
    int num_entries;

    num_times++;

    if (num_times > 2) {
	pmFreeResult(result[0]);	

        /* if forever, ensure no wrapping */
        if (num_samples < 0)
	    num_times = 3;
    }

    if (num_times > 1) {
	result[0] = result[1];
    }

    get_indom();
    if ((sts = pmFetch(num_pmid, pmidlist, &result[1])) < 0) {
	for (i = 0; i < num_pmid; i++)
	    fprintf(stderr, "%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
	exit(1);
    }


    if (num_times > 1) {
        then = result[0]->timestamp.tv_sec + result[0]->timestamp.tv_usec/1000000;
        now = result[1]->timestamp.tv_sec + result[1]->timestamp.tv_usec/1000000;
	delta_d = now - then;

        if (result[1]->numpmid != num_pmid) {
	    fprintf(stderr, "%s: Failed to fetch all metrics (%d out of %d)\n", 
		    pmProgname, result[1]->numpmid, num_pmid);
	    exit(1);
	}
	
	/* --- build value table --- */

	for(i=0; i<num_inst; i++) {
	    val_tab[i].num = 0;
	}

	/* go thru each result */
	for(r=0; r<2; r++) {
	    /* go thru each metric */
	    for(m=0; m<num_pmid; m++) {
		pmValueSet *vset = result[r]->vset[m];
		if (vset->numval < 0) {
		    fprintf(stderr, "%s: Error when fetching metric %s: %s\n", 
			pmProgname, namelist[m], pmErrStr(vset->numval));
		    continue;
		}
		if (vset->numval == 0)
		    continue;
		valfmt_tab[m] = vset->valfmt;

		/* handle global metrics */
		if (((show_spec & (SHOW_CPU | SHOW_SYS)) && m == kernel_stime) || 
		    ((show_spec & (SHOW_CPU | SHOW_USR)) && m == kernel_utime) || 
		    ((show_spec & SHOW_SYSCALLS) && m == kernel_sysc) || 
		    ((show_spec & SHOW_CTX) && m == kernel_ctx)) {
		    if (vset->numval != 1) {
			fprintf(stderr, "%s: Wrong num of values for kernel metric: %d\n", 
			    pmProgname, vset->numval);
			exit(1);
		    }
		    global_val[m][r] = vset->vlist[0];	
		    continue;
		}

		/* handle proc-instance metrics */

		/* Find result inst
		 * Would be faster to start from where left off
		 * last time. Need to remember this for a metric/result.
		 */
		for(v=0; v<vset->numval; v++) {
		    pmValue val = vset->vlist[v];
		    /* go thru each possible inst */
		    for(i=0; i<num_inst; i++) {
			if (instances[i] == val.inst) {
			    val_tab[i].num++; /* needs to be zeroed at start */
			    val_tab[i].values[m][r] = val;
			    break;
			}
		    }
		}
	    }
	}
	
	/* --- calculate rates --- */

	/* handle global metrics */
	if (show_spec & SHOW_CPU) {
	    __uint64_t utime[2], stime[2];
	    double cpuburn_val;

	    utime[0] = get_value(kernel_utime, &global_val[kernel_utime][0]);
	    utime[1] = get_value(kernel_utime, &global_val[kernel_utime][1]);
	    stime[0] = get_value(kernel_stime, &global_val[kernel_stime][0]);
	    stime[1] = get_value(kernel_stime, &global_val[kernel_stime][1]);
	    
	    cpuburn_val = (utime[1] - utime[0] + stime[1] - stime[0]) / delta_d;
	    cpuburn_val /= 10; 
	    cpuburn_val = cpuburn_val > 100 ? 100 : cpuburn_val;
	    global_rates[CPU_VAL] = cpuburn_val;
	}


	if (show_spec & SHOW_SYSCALLS) {
	    __uint64_t sysc[2];

	    sysc[0] = get_value(kernel_sysc, &global_val[kernel_sysc][0]);
	    sysc[1] = get_value(kernel_sysc, &global_val[kernel_sysc][1]);
	    
	    global_rates[SYSCALLS_VAL] = (sysc[1] - sysc[0]) / delta_d;
	}

	if (show_spec & SHOW_CTX) {
	    __uint64_t ctx[2];

	    ctx[0] = get_value(kernel_ctx, &global_val[kernel_ctx][0]);
	    ctx[1] = get_value(kernel_ctx, &global_val[kernel_ctx][1]);
	    
	    global_rates[CTX_VAL] = (ctx[1] - ctx[0]) / delta_d;
	}

	if (show_spec & SHOW_SYS) {
	    __uint64_t stime[2];

	    stime[0] = get_value(kernel_stime, &global_val[kernel_stime][0]);
	    stime[1] = get_value(kernel_stime, &global_val[kernel_stime][1]);
	    
	    global_rates[SYS_VAL] = ((stime[1] - stime[0]) / delta_d) / 10;
	}

	if (show_spec & SHOW_USR) {
	    __uint64_t utime[2];

	    utime[0] = get_value(kernel_utime, &global_val[kernel_utime][0]);
	    utime[1] = get_value(kernel_utime, &global_val[kernel_utime][1]);
	    
	    global_rates[USR_VAL] = ((utime[1] - utime[0]) / delta_d) / 10;
	}


	/* handle proc-instance metrics */
	num_entries = 0;
	for(i=0; i<num_inst; i++) {
	    val_entry_t *vt = &val_tab[i];
	    rate_entry_t *rt;
	    if (vt->num < (num_proc_pmid*2)) {
		/* couldn't get instances for all proc metrics for both(*2) results */
		continue;
	    }
	    
	    rt = &rate_tab[num_entries];
	    rt->inst = i;

	    /* create cpuburn values */
	    if (show_spec & SHOW_CPU) {
		__uint64_t utime[2], stime[2];
		double cpuburn_val;

		utime[0] = get_value(pu_utime, &vt->values[pu_utime][0]);
		utime[1] = get_value(pu_utime, &vt->values[pu_utime][1]);
		stime[0] = get_value(pu_stime, &vt->values[pu_stime][0]);
		stime[1] = get_value(pu_stime, &vt->values[pu_stime][1]);
		
		cpuburn_val = (utime[1] - utime[0] + stime[1] - stime[0]) / delta_d;
		cpuburn_val /= 10; 
		cpuburn_val = cpuburn_val > 100 ? 100 : cpuburn_val;
		rt->values[CPU_VAL] = cpuburn_val;
	    }

	    /* create syscalls values */
	    if (show_spec & SHOW_SYSCALLS) {
		__uint64_t sysc[2];

		sysc[0] = get_value(pu_sysc, &vt->values[pu_sysc][0]);
		sysc[1] = get_value(pu_sysc, &vt->values[pu_sysc][1]);
		
		rt->values[SYSCALLS_VAL] = (sysc[1] - sysc[0]) / delta_d;
	    }

	    /* create ctx values */
	    if (show_spec & SHOW_CTX) {
		double ictx, vctx;

		vctx = get_value(pu_vctx, &vt->values[pu_vctx][1]) - 
		       get_value(pu_vctx, &vt->values[pu_vctx][0]);
		ictx = get_value(pu_ictx, &vt->values[pu_ictx][1]) - 
		       get_value(pu_ictx, &vt->values[pu_ictx][0]);
		
		rt->values[CTX_VAL] = (vctx + ictx) / delta_d;
	    }

	    /* create reads values */
	    if (show_spec & SHOW_READS) {
		double br, gbr;

		br = get_value(pu_bread, &vt->values[pu_bread][1]) - 
		     get_value(pu_bread, &vt->values[pu_bread][0]);
		gbr = get_value(pu_gbread, &vt->values[pu_gbread][1]) -
		      get_value(pu_gbread, &vt->values[pu_gbread][0]);
		
		rt->values[READS_VAL] = (gbr * 1024 * 1024 + br / 1024) / delta_d;
	    }

	    /* create writes values */
	    if (show_spec & SHOW_WRITES) {
		double bw, gbw;

		bw = get_value(pu_bwrit, &vt->values[pu_bwrit][1]) -
		     get_value(pu_bwrit, &vt->values[pu_bwrit][0]);
		gbw = get_value(pu_gbwrit, &vt->values[pu_gbwrit][1]) -
		      get_value(pu_gbwrit, &vt->values[pu_gbwrit][0]);
		
		rt->values[WRITES_VAL] = (gbw * 1024 * 1024 + bw / 1024) / delta_d;
	    }

	    /* create rss values */
	    if (show_spec & SHOW_RSS) {
		rt->values[RSS_VAL] = get_value(pu_rss, &vt->values[pu_rss][1]);
	    }

	    /* create sys cpu time values */
	    if (show_spec & SHOW_SYS) {
		__uint64_t stime[2];

		stime[0] = get_value(pu_stime, &vt->values[pu_stime][0]);
		stime[1] = get_value(pu_stime, &vt->values[pu_stime][1]);
		
		rt->values[SYS_VAL] = ((stime[1] - stime[0]) / delta_d) / 10;
		rt->values[SYS_VAL] = rt->values[SYS_VAL] > 100 ? 100 : rt->values[SYS_VAL];
	    }

	    /* create user cpu time values */
	    if (show_spec & SHOW_USR) {
		__uint64_t utime[2];

		utime[0] = get_value(pu_utime, &vt->values[pu_utime][0]);
		utime[1] = get_value(pu_utime, &vt->values[pu_utime][1]);
		
		rt->values[USR_VAL] = ((utime[1] - utime[0]) / delta_d) / 10;
		rt->values[USR_VAL] = rt->values[USR_VAL] > 100 ? 100 : rt->values[USR_VAL];
	    }


	    num_entries++;
	}

	/* --- report values --- */
	printf("HOST: %s\n", hostname);
	printf("DATE: %s\n\n", get_time());

	if (show_spec & SHOW_CPU) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), cpuburn_cmp);
	    calc_sum_rates(num_entries, CPU_VAL);
	    print_top(rate_tab, num_entries, CPU_VAL);
	}

	if (show_spec & SHOW_SYSCALLS) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), syscall_cmp);
	    calc_sum_rates(num_entries, SYSCALLS_VAL);
	    print_top(rate_tab, num_entries, SYSCALLS_VAL);
	}

	if (show_spec & SHOW_CTX) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), ctx_cmp);
	    calc_sum_rates(num_entries, CTX_VAL);
	    print_top(rate_tab, num_entries, CTX_VAL);
	}

	if (show_spec & SHOW_WRITES) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), write_cmp);
	    print_top(rate_tab, num_entries, WRITES_VAL);
	}

	if (show_spec & SHOW_READS) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), read_cmp);
	    print_top(rate_tab, num_entries, READS_VAL);
	}

	if (show_spec & SHOW_RSS) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), rss_cmp);
	    print_top(rate_tab, num_entries, RSS_VAL);
	}

	if (show_spec & SHOW_SYS) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), sys_cmp);
	    calc_sum_rates(num_entries, SYS_VAL);
	    print_top(rate_tab, num_entries, SYS_VAL);
	}

	if (show_spec & SHOW_USR) {
	    qsort(rate_tab, num_entries, sizeof(rate_entry_t), usr_cmp);
	    calc_sum_rates(num_entries, USR_VAL);
	    print_top(rate_tab, num_entries, USR_VAL);
	}
    }

    if (num_times-1 == num_samples) {
	exit(0);
    }
}

/*
 * go thru show_spec and build up the namelist and
 * index variables into the array
 */
void
create_namelist(void)
{
    int i = 0; /* for each pmid */
    int j = 0; /* for global pmids */

    if (show_spec & SHOW_CPU || show_spec & SHOW_USR) {
	namelist[i] = "proc.psusage.utime";
	pu_utime = i;
	i++;
	namelist[i] = "kernel.all.cpu.user";
	kernel_utime = i;
	i++; j++;
    }
    if (show_spec & SHOW_CPU || show_spec & SHOW_SYS) {
	namelist[i] = "proc.psusage.stime";
	pu_stime = i;
	i++;
	namelist[i] = "kernel.all.cpu.sys";
	kernel_stime = i;
	i++; j++;
    }
    if (show_spec & SHOW_SYSCALLS) {
	namelist[i] = "proc.psusage.sysc";
	pu_sysc = i;
	i++;
	namelist[i] = "kernel.all.syscall";
	kernel_sysc = i;
	i++; j++;
    }
    if (show_spec & SHOW_CTX) {
	namelist[i] = "proc.psusage.ictx";
	pu_ictx = i;
	i++;
	namelist[i] = "proc.psusage.vctx";
	pu_vctx = i;
	i++;
	namelist[i] = "kernel.all.pswitch";
	kernel_ctx = i;
	i++; j++;
    }
    if (show_spec & SHOW_READS) {
	namelist[i] = "proc.psusage.bread";
	pu_bread = i;
	i++;
	namelist[i] = "proc.psusage.gbread";
	pu_gbread = i;
	i++;
    }
    if (show_spec & SHOW_WRITES) {
	namelist[i] = "proc.psusage.bwrit";
	pu_bwrit = i;
	i++;
	namelist[i] = "proc.psusage.gbwrit";
	pu_gbwrit = i;
	i++;
    }
    if (show_spec & SHOW_RSS) {
	namelist[i] = "proc.psusage.rss";
	pu_rss = i;
	i++;
    }

    num_pmid = i;
    num_proc_pmid = i - j;
}

static void
get_indom(void)
{
    static int onetrip = 1;

    if (instances != NULL) 
	free(instances);
    if (instance_names != NULL)
	free(instance_names);

    if ((num_inst = pmGetInDom(proc_indom, &instances, &instance_names)) <= 0) {
	fprintf(stderr, "%s: Failed to find any proc instances : %s\n",
		pmProgname, pmErrStr(num_inst));
	exit(1);
    }

    
    /* process the instance names
     * - find how long a pid is 
     * - create pid label fmt based on pid length
     */
    if (onetrip) {
	char *str = instance_names[0];

	onetrip=0;

	/* get pid str up to first space */
	pid_len = 0;
	while (*str != '\0' && *str != ' ') {
	    pid_len++;
	    str++;
	}
	if (*str == '\0') {
	    /* shouldn't happen */
	    fprintf(stderr, "%s: Bad proc instance : %s\n",
		    pmProgname, instance_names[0]);
	    exit(1);
	}

	sprintf(pid_label_fmt, "%%%ds", pid_len);
	sprintf(pid_fmt, "%%%dd", pid_len);
    }


    rate_tab = realloc(rate_tab, sizeof(rate_entry_t)*num_inst);
    if (rate_tab == NULL) {
	fprintf(stderr, "%s: Failed to allocate rate table\n",
		pmProgname);
	exit(1);
    }

    val_tab = realloc(val_tab, sizeof(val_entry_t)*num_inst);
    if (val_tab == NULL) {
	fprintf(stderr, "%s: Failed to allocate value table\n",
		pmProgname);
	exit(1);
    }

    pmDelProfile(proc_indom, 0, NULL);
    pmAddProfile(proc_indom, num_inst, instances);
}

static int
overrides(int opt, pmOptions *opts)
{
    if (opt == 'p')
	return 1;

    if (opt == 's')
	num_samples = atoi(opts->optarg); /* continue processing 's' */
    return 0;
}

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    char	*endnum;
    pmDesc	desc;
    int		one_trip = 1;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'w':	/* wide flag */
	    line_fmt = "%.1024s";
	    break;

	case 'p':	/* show flag */
	    if (one_trip) {
		show_spec = 0;
		one_trip = 0;
	    }
	    if ((sts = parse_show_spec(opts.optarg)) < 0) {
		pmprintf("%s: unrecognized print flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    } else {
		show_spec |= sts;
	    }
	    break;

	case 'm':	/* top N */
	    top = (int)strtol(opts.optarg, &endnum, 10);
	    if (top <= 0) {
		pmprintf("%s: -m requires a positive integer\n", pmProgname);
		opts.errors++;	
	    }
	    break;
	}
    }

    if (opts.optind < argc)
        opts.errors++;

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    create_namelist();

    if (opts.interval.tv_sec == 0)
	opts.interval.tv_sec = 2;

    if (opts.nhosts > 0)
	hostname = opts.hosts[0];
    else
	hostname = "local:";

    if ((sts = c = pmNewContext(PM_CONTEXT_HOST, hostname)) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, hostname, pmErrStr(sts));
	exit(1);
    }
    hostname = (char *)pmGetContextHostName(c);

    if (pmGetContextOptions(c, &opts)) {
	pmflush();
	exit(1);
    }

    if ((sts = pmLookupName(num_pmid, namelist, pmidlist)) < 0) {
	fprintf(stderr, "%s: Failed to lookup metrics : %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    for (c = 0; c < num_pmid; c++) {
	if ((sts = pmLookupDesc(pmidlist[c], &desc)) < 0) {
	    fprintf(stderr, "%s: Failed to lookup descriptor for metric \"%s\": %s\n",
		    pmProgname, namelist[c], pmErrStr(sts));
	    exit(1);
	}
	type_tab[c] = desc.type;	
	/* ASSUMES that the first metric will always be a proc metric */
	if (c == 0) {
	    proc_indom = desc.indom;
	}
    }

    for (;;) {
	doit();
	__pmtimevalSleep(opts.interval);
    }

    return 0;
}
