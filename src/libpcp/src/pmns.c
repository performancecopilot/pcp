/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: pmns.c,v 1.14 2006/06/30 05:47:11 makc Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include "pmapi.h"
#include "impl.h"

/*
 *  %s fields in CPP_FMT
 *
 *  cpp-cmd from cpp_path[]
 *  cpp_simple_args (assumed to apply for all, eg. -U... -U... -P -undef -...)
 *  /var/pcp or similar
 *  /usr/pcp or similar
 *  input pmns file name
 */
#define CPP_FMT "%s %s -I. -I%s/pmns -I%s/pmns %s"

static char	*cpp_path[] = {
    CPP_SIMPLE,
    "/lib/cpp",
    "/usr/cpu/sysgen/root/lib/cpp",
    "/usr/lib/cpp",
    "/usr/cpu/sysgen/root/usr/lib/cpp",
    "/usr/bin/cpp",
    NULL
};


/* token types */
#define NAME	1
#define PATH	2
#define PMID	3
#define LBRACE	4
#define RBRACE	5
#define BOGUS	10

#define UNKNOWN_MARK_STATE -1           /* tree not all marked the same way */
#define PMID_MASK	0x3fffffff	/* 30 bits of PMID */
#define MARK_BIT	0x40000000	/* mark bit */


static int	lineno = 0;
static char	linebuf[256];
static char	*linep;
static char	fname[256];
static char	tokbuf[256];
static pmID	tokpmid;
static int	numpmid;

static __pmnsNode *seen; /* list of pass-1 subtree nodes */

/* Last modification time for loading main_pmns file. */
#if defined(HAVE_STAT_TIMESTRUC)
static timestruc_t	last_mtim;
#elif defined(HAVE_STAT_TIMESPEC)
static struct timespec	last_mtim;
#elif defined(HAVE_STAT_TIMESPEC_T)
static timespec_t	last_mtim;
#elif defined(HAVE_STAT_TIME_T)
static time_t	last_mtim;
#else
!bozo!
#endif

/* The curr_pmns points to PMNS to use for API ops.
 * Curr_pmns will point to either the main_pmns or
 * a pmns from a version 2 archive context.
 */
static __pmnsTree *curr_pmns = NULL; 

/* The main_pmns points to the loaded PMNS (not from archive). */
static __pmnsTree *main_pmns = NULL; 


/* == 1 if PMNS loaded and __pmExportPMNS has been called */
static int	export;

extern int	errno;

static int havePmLoadCall = 0;
static int useExtPMNS = 0; /* set by __pmUsePMNS() */

static int load(const char *filename, int binok, int dupok);
static __pmnsNode *locate(const char *name, __pmnsNode *root);

/*
 * Allow IRIX Translation to be selectively turned off
 */
#ifndef IRIX_XLATION
#define IRIX_XLATION 1
#endif

#if IRIX_XLATION
/*
 * Change the PMNS name by stripping/adding some of starting nodes
 * Translation rules are held in static tables.
 * This provides compatibility between the old and new namespace naming conventions.
 * The rules for building a list of translations is:
 * The simple case is purely when a node string matches strip, it is replaced with
 * the add string.
 * A default case amongs siblings can be designated by making strip == "".
 * A strip string does not always need to have an accompanying add string. If no
 * add string is designated, then the translation may depend on the next node in the 
 * specified name.
 *
 * These rules allow:
 * Grouping of nodes after translation.
 * Un-grouping of nodes after translation.
 */

typedef struct _nsxl_node {
    char                *strip; /* what to strip */
    char                *add;   /* what to add */
    struct _nsxl_node   *first; /* first of the child nodes */
    struct _nsxl_node   *next;  /* next sibling node */
    int                 status;
} nsxl_node;

#define L_FILLER_CHAR ' '

#define NSXL_IRIX_NODE "irix"
#define NSXL_IRIX_LEN	4

static int IsIrixName(const char* name)
{
    static size_t irix_len = NSXL_IRIX_LEN;
    if (strncmp(name, NSXL_IRIX_NODE, irix_len) == 0) {
	if (name[irix_len] == '\0' || name[irix_len] == '.')
	    return 1;
    }
    return 0;
}

static nsxl_node *
GetStripIrixNodes(void)
{
    static nsxl_node dres_node  = { "",             "resource",     NULL,   NULL, 1 };
    static nsxl_node efs_node   = { "efs",          "efs",          NULL,   &dres_node, 1 };
    static nsxl_node vn_node    = { "vnodes",       "vnodes",       NULL,   &efs_node, 1 };
    static nsxl_node bc_node    = { "buffer_cache", "buffer_cache", NULL,   &vn_node, 1 };
    static nsxl_node nc_node    = { "name_cache",   "name_cache",   NULL,   &bc_node, 1 };

    static nsxl_node onuma_node = { "", "numa", NULL, NULL, 1 };
    static nsxl_node onode_node = { "", "node", NULL, NULL, 1 };

    static nsxl_node xlv_node   = { "xlv",      "xlv",      NULL,       NULL, 1 };
    static nsxl_node xbow_node  = { "xbow",     "xbow",     NULL,       &xlv_node, 1 };
    static nsxl_node engr_node  = { "engr",     "engr",     NULL,       &xbow_node, 1 };
    static nsxl_node pmda_node  = { "pmda",     "pmda",     NULL,       &engr_node, 1 };
    static nsxl_node xpc_node   = { "xpc",      "xpc",      NULL,       &pmda_node, 1 };
    static nsxl_node kaio_node  = { "kaio",     "kaio",     NULL,       &xpc_node, 1 };
    static nsxl_node xfs_node   = { "xfs",      "xfs",      NULL,       &kaio_node, 1 };
    static nsxl_node ipc_node   = { "ipc",      "ipc",      NULL,       &xfs_node, 1 };
    static nsxl_node filesys_node  = { "filesys", "filesys",NULL,       &ipc_node, 1 };
    static nsxl_node nfs3_node  = { "nfs3",     "nfs3",     NULL,       &filesys_node, 1 };
    static nsxl_node nfs_node   = { "nfs",      "nfs",      NULL,       &nfs3_node, 1 };
    static nsxl_node rpc_node   = { "rpc",      "rpc",      NULL,       &nfs_node, 1 };
    static nsxl_node gfx_node   = { "gfx",      "gfx",      NULL,       &rpc_node, 1 };
    static nsxl_node network_node  = { "network", "network",NULL,       &gfx_node, 1 };
    static nsxl_node swapdev_node  = { "swapdev", "swapdev",NULL,       &network_node, 1 };
    static nsxl_node swap_node  = { "swap",     "swap",     NULL,       &swapdev_node, 1 };
    static nsxl_node mem_node   = { "mem",      "mem",      NULL,       &swap_node, 1 };
    static nsxl_node disk_node  = { "disk",     "disk",     NULL,       &mem_node, 1 };
    static nsxl_node kernel_node= { "kernel",   "kernel",   NULL,       &disk_node, 1 };
    static nsxl_node res_node   = { "resource", "",         &nc_node,   &kernel_node, 1 };
    static nsxl_node node_node  = { "node", "origin",       &onode_node,&res_node, 1 };
    static nsxl_node numa_node  = { "numa", "origin",       &onuma_node,&node_node, 1 };
    
    static nsxl_node irix_node	= { "irix", "",	            &numa_node, NULL, 1 };
   
    static nsxl_node root_node	= { "",	    "",             &irix_node, NULL, 1 };

    return &root_node;
}

static nsxl_node *
GetAddIrixNodes(void)
{
    static nsxl_node r11_node       = { "procovf", "procovf",   NULL,           NULL };
    static nsxl_node r10_node       = { "fileovf", "fileovf",   NULL,           &r11_node, 0 };
    static nsxl_node r9_node        = { "nstream_head", "nstream_head",  NULL,  &r10_node, 0 };
    static nsxl_node r8_node        = { "nstream_queue", "nstream_queue",NULL,  &r9_node, 0 };
    static nsxl_node r7_node        = { "dquot", "dquot",       NULL,           &r8_node, 0 };
    static nsxl_node r6_node        = { "maxdmasz", "maxdmasz", NULL,           &r7_node, 0 };
    static nsxl_node r5_node        = { "maxpmem", "maxpmem",   NULL,           &r6_node, 0 };
    static nsxl_node r4_node        = { "syssegsz", "syssegsz",   NULL,           &r5_node, 0 };
    static nsxl_node r3_node        = { "hbuf", "hbuf",         NULL,           &r4_node, 0 };
    static nsxl_node r2_node        = { "nbuf", "nbuf",         NULL,           &r3_node, 0 };
    static nsxl_node r1_node        = { "nproc", "nproc",       NULL,           &r2_node, 0 };

    static nsxl_node node_node      = { "node", "node",         NULL,           NULL, 1 };
    static nsxl_node numa_node      = { "numa", "numa",         NULL,           &node_node, 1 };
    
    static nsxl_node irix_node      = { "",     "irix",         NULL,           NULL, 1 };
    static nsxl_node res_node       = { "resource", "irix.resource", &r1_node,  &irix_node, 1 };
    static nsxl_node efs_node       = { "efs",    "irix.resource.efs", NULL, &res_node, 1 };
    static nsxl_node vn_node        = { "vnodes", "irix.resource.vnodes", NULL, &efs_node, 1 };
    static nsxl_node bc_node        = { "buffer_cache", "irix.resource.buffer_cache",NULL,&vn_node, 1 };
    static nsxl_node nc_node        = { "name_cache", "irix.resource.name_cache", NULL, &bc_node, 1 };
    static nsxl_node nonode_node    = { "node", "node",         NULL,           &nc_node, 1 };
    static nsxl_node nonuma_node    = { "numa", "numa",         NULL,           &nonode_node, 1 };
    static nsxl_node origin_node    = { "origin","irix",        &numa_node,     &nonuma_node, 1 };
   
    static nsxl_node root_node      = { "",     "",             &origin_node,   NULL, 1 };

    return &root_node;
}

static void
PrependNsxlNode(char *node, char *name)
{
    size_t node_strlen = strlen(node);
    char *start = strrchr(name, (int)L_FILLER_CHAR);	
    if (start != NULL && node_strlen > 0) {
        if (*(start+1) != 0)
            *start = '.';
        else
            *start = 0;
        start = start - node_strlen;
        strncpy(start, node, node_strlen);
    }
}

static char *
HandleNodeEnd(size_t mem_size, int size_aggr, const char *name)
{
    char *new_name;
    if ((new_name= (char *)malloc(mem_size)) == NULL)
        return NULL;
    memset(new_name, L_FILLER_CHAR, mem_size);
    new_name[mem_size - 1] = 0;
    strncpy(&new_name[size_aggr], name, strlen(name));
        
    return new_name;
}

static int 
CompareXlName(const char *name, const char *node_str)
{
    if (strncmp(name, node_str, strlen(node_str)) == 0)
        if (name[strlen(node_str)] == 0 || name[strlen(node_str)] == '.')
            return 0;
    return 1;
}

/* 
 * Find the lowest node matching the strip name without traversing a default node
 */
static const nsxl_node *
FindNode(const char *name, const nsxl_node *node)
{
    const char  *tail;
    nsxl_node  *np = NULL;

    /* Traverse until '.' or '\0' */
    for (tail = name; *tail && *tail != '.'; tail++)
        ;

    /* Compare name with all the child nodes */
    for (np = node->first; np != NULL; np = np->next) {
        if (CompareXlName(name, np->strip) == 0)
             break;
    }

    if (strlen(name) == 0) {
        return node;
    }
    else if (np == NULL) {
        return NULL;
    }
    else {
        if (*tail == '.')
            tail++;
        return FindNode(tail, np);
    }
}

static char *
StripNode(const char *name, int size_aggr, const nsxl_node *node, const nsxl_node **node_end)
{
    const char  *tail;
    nsxl_node  *np = NULL;

    /* Traverse until '.' or '\0' */
    for (tail = name; *tail && *tail != '.'; tail++)
        ;

    /* Compare name with all the child nodes */
    for (np = node->first; np != NULL; np = np->next) {
        if (strlen(np->strip) == 0 || CompareXlName(name, np->strip) == 0)
            break;
    }

    if (np == NULL) {
        *node_end = node;
        return HandleNodeEnd( size_aggr + strlen(name) + 1, size_aggr, name );
    }
    else {
        char *stripped_name;
        int  add_strlen = (int)strlen(np->add);
        if (add_strlen > 0)
            size_aggr += strlen(np->add) + 1; /* for the '.' */
        if (strlen(np->strip) > 0) {
            size_t skip = (strlen(np->strip) + 1 > strlen(name) ? strlen(name) : strlen(np->strip) + 1);
            stripped_name = StripNode(&name[skip], size_aggr, np, node_end);
        }
        else
            stripped_name = StripNode(name, size_aggr, np, node_end);
        if (stripped_name != NULL) {
            PrependNsxlNode(np->add, stripped_name);
        }
        return stripped_name;
    }
}

static int
GetXlChildren(const char *name, const nsxl_node *node, const int pmns_location,
                char ***enfants, int **statuslist, __pmnsNode **pm_np, char **newname)
{
    char        **l_enfants = NULL;
    int         *l_statuslist = NULL;
    int         num_enfants = 0;
    const nsxl_node   *np_unique;
    nsxl_node   *np = NULL;
    char *l_newname = StripNode(name, 0, node, &np_unique);

    if (newname != NULL)
        *newname = NULL;

    np_unique = FindNode(name, node);

    /* Found nothing */
    if (np_unique == NULL && pmns_location != PMNS_LOCAL) {
        *newname = l_newname;
        return 0;
    }

    if (l_newname == NULL)
        return -errno;

    if (np_unique != NULL)
        np = np_unique->first;

    /* if its a leaf, just do a locate to get the PMNS node */
    if (np == NULL) {
        if (pmns_location == PMNS_LOCAL) {
            *pm_np = locate(l_newname, curr_pmns->root);
            if (*pm_np == NULL) {
                free(l_newname);
                return PM_ERR_NAME;
            }
        }
        else {
            *newname = l_newname;
        }
    }
    else {
        int enf_ix = 0;
        int need = 0;
        char *p;

        while(np != NULL) {
            if (strlen(np->strip) != 0) {
                need += strlen(np->strip) + 1;
                num_enfants++;
            }
            np = np->next;
        }

        if (num_enfants > 0) {
            if ((l_enfants = (char**)malloc(num_enfants*sizeof(*l_enfants) + need)) == NULL) {
                free(l_newname);
                return -errno;
            }
            if ((l_statuslist = (int*)malloc(num_enfants*sizeof(*l_statuslist))) == NULL) {
                free(l_newname);
                free(l_enfants);
                return -errno;
            }
        }

        p = (char *)&l_enfants[num_enfants];
        np = np_unique->first;
        while(np != NULL) {
            if (strlen(np->strip) == 0) {
                if (pmns_location == PMNS_LOCAL) {
                    *pm_np = locate(l_newname, curr_pmns->root);
                    if (*pm_np == NULL) 
                        return PM_ERR_NAME;
                }
                else {
                    *newname = l_newname;
                }
            }
            else {
                strcpy(p, np->strip);
                l_enfants[enf_ix] = p;
                l_statuslist[enf_ix] = np->status;
                p += strlen(np->strip) + 1;
                enf_ix++;
            }
        np = np->next;
        }
        *enfants = l_enfants;
        *statuslist = l_statuslist;
    }
    if (newname == NULL)
        free(l_newname);

    return num_enfants;
}

/*
 * Count how many unique Add names can be derived from the specified nodes.
 * Note that a node with an add string of "" will cause traversal to the 
 * next level.
 */
static int
CountNsxlAddNodes(const nsxl_node *node)
{
    int             counter = 0;
    const nsxl_node *np = node->first;
    
    while(np != NULL) {
        if (strlen(np->add) == 0) {
            counter += CountNsxlAddNodes(np);
        }
        counter++;
        np = np->next;
    }
    return counter;
}

/*
 * Re-usable namelist. 
 * The memory allocated for the list member is re-used. The size of the list is
 * increased if necessary. Currently only used to traverse the Xlator name space nodes.
 * Use the iface functions to access this variable.
 * ClearNsxlNamelist should be called before using it.
 */
typedef struct {
        int     numalloc;
        int     numused;
        char    **namelist;
} nsxl_namelist;

static nsxl_namelist xl_namelist = { 0, 0, NULL };

static int
AddNsxlName(char *name)
{
    if (xl_namelist.numalloc == xl_namelist.numused) {
        if (xl_namelist.numalloc == 0) {
            xl_namelist.namelist = (char**)malloc(sizeof(*xl_namelist.namelist));    
            if (xl_namelist.namelist == NULL)
                return -errno;
            xl_namelist.numalloc++;
        }
        else {
            char **tmp_namelist = (char**)realloc(xl_namelist.namelist,
                                  (xl_namelist.numalloc + 1)*sizeof(*xl_namelist.namelist));
            if (tmp_namelist == NULL)
                return -errno;
            xl_namelist.namelist = tmp_namelist;
            xl_namelist.numalloc++;
        }
    }

    xl_namelist.namelist[xl_namelist.numused] = name;
    xl_namelist.numused++;
    
    return 0;
}

static int
ClearNsxlNamelist(void)
{
    int ix;
    for(ix = 0; ix < xl_namelist.numused; ix++)
        if (xl_namelist.namelist[ix] != NULL)
            free(xl_namelist.namelist[ix]);

    xl_namelist.numused = 0;
    return 0;
}

static int
TraverseNsxlNodes(const nsxl_node *np, int accum_len)
{
    /*
     * This could return a list of options since when a high node is specified alone, it
     * may translate to more than one lower node.
     * So, create a name for all child nodes which do not contain a strip string.
     * eg. origin becomes irix.numa and irix.node
     */
    const nsxl_node   *l_np;
    int         xlnl_bookmk = 0;

    for(l_np = np->first; l_np != NULL; l_np = l_np->next) {
        int new_len = (int)(strlen(l_np->add) > 0 ? accum_len + strlen(l_np->add) + 1 : accum_len);
        if (l_np->first == NULL) {
            char *new_name = HandleNodeEnd(new_len, accum_len, l_np->add);
            if (new_name != NULL)
                AddNsxlName(new_name); 
        }
        else {
            int name_ix;
            TraverseNsxlNodes(l_np, new_len);
            for(name_ix = xlnl_bookmk; name_ix<xl_namelist.numused; name_ix++) {
                PrependNsxlNode(l_np->add, xl_namelist.namelist[name_ix]);                    
            }
            xlnl_bookmk = xl_namelist.numused - 1;
        }
    }

    return 0;
}

static char *
StripIrix(const char *name)
{
    const nsxl_node *node_end = NULL;
    char *l_name = StripNode( name, 0, GetStripIrixNodes(), &node_end);	
    if ( l_name != NULL && strcmp(l_name, name) == 0 ) {
        free(l_name);
        return NULL;
    }
    return l_name;
}

static int
BuildXlList(const char *prefix, char ***namelist, int *listlen, const nsxl_node *np)
{
    int status;
    char **l_namelist;

    if ((status = TraverseNsxlNodes(np, 0)) < 0)
        return status;

    if (xl_namelist.numused > 0) {
        int name_ix;
        l_namelist = (char**)malloc(xl_namelist.numused*sizeof(*l_namelist));
        if (l_namelist == NULL) {
            ClearNsxlNamelist();
            return -errno;
        }
        if (strlen(prefix) == 0) {
            for(name_ix = 0; name_ix < xl_namelist.numused; name_ix++) {
                l_namelist[name_ix] = xl_namelist.namelist[name_ix];
                xl_namelist.namelist[name_ix] = NULL;
            }
        }
        else {
            for(name_ix = 0; name_ix < xl_namelist.numused; name_ix++) {
                l_namelist[name_ix] = (char*)malloc(strlen(xl_namelist.namelist[name_ix]) +
                                                strlen(prefix) + 2); 
                if (l_namelist[name_ix] == NULL) {
                    int ix;
                    for(ix = 0; ix < name_ix; ix++) {
                        free(l_namelist[name_ix]);
                    }
                ClearNsxlNamelist();
                free(l_namelist);
                return -errno;
                }

                strcat( strcpy(l_namelist[name_ix], prefix), ".");
                strcat(l_namelist[name_ix], xl_namelist.namelist[name_ix]);
                free(xl_namelist.namelist[name_ix]);
                xl_namelist.namelist[name_ix] = NULL;
            }
        }
        *listlen = xl_namelist.numused; 
        xl_namelist.numused = 0;
    }
    else {
        l_namelist = (char**)malloc(sizeof(*l_namelist));
        if (l_namelist == NULL)
            return -errno;
        l_namelist[0] = strdup(prefix);
        if (l_namelist[0] == NULL) {
            free(l_namelist);
            return -errno;
        }
        *listlen = 1;
    }
    *namelist = l_namelist;
    return 0;
}

static char *
AddIrix(const char *name)
{
    const nsxl_node *node_end = NULL;
    char *l_name = StripNode( name, 0, GetAddIrixNodes(), &node_end);	
    if ( l_name != NULL && strcmp(l_name, name) == 0 ) {
        free(l_name);
        return NULL;
    }
    return l_name;
}

static int
AddIrixList(const char *name, char ***namelist, int *listlen)
{
    const nsxl_node *node_end = NULL;
    char *l_name = StripNode( name, 0, GetAddIrixNodes(), &node_end);	
    if (l_name == NULL)
        return -errno;

    return BuildXlList(l_name, namelist, listlen, node_end); 
}

static char **
BuildFailNameList(char *namelist[], const pmID pmidlist[], 
                const int numpid, int *num_failed)
{
    int list_ix;
    int fail_ix = 0;
    char **l_namelist;

    *num_failed = 0;
    for(list_ix = 0; list_ix < numpid; list_ix++) {
        if (pmidlist[list_ix] == PM_ID_NULL)
            (*num_failed)++;
    }

    if ((l_namelist = (char**)malloc((*num_failed)*sizeof(*l_namelist))) == NULL) {
        *num_failed = 0;
        return NULL;
        }

    for(list_ix = 0; list_ix < numpid; list_ix++) {
        if (pmidlist[list_ix] == PM_ID_NULL) {
            l_namelist[fail_ix] = AddIrix(namelist[list_ix]);

            if (l_namelist[fail_ix] != NULL) {
                fail_ix++;
            }
        }
    }
    *num_failed = fail_ix;
    return l_namelist;
}

static void
MergeLists(pmID pmidlist[], int numpmid, pmID fail_pmid[], int num_failed)
{
    int pmid_ix;
    int fail_ix = 0;

    for(pmid_ix = 0; pmid_ix < numpmid && fail_ix < num_failed; pmid_ix++) {
        if (pmidlist[pmid_ix] == PM_ID_NULL) {
            pmidlist[pmid_ix] = fail_pmid[fail_ix];
            fail_ix++;
        }
    }
}

/* Forward reference */
static int
GetChildrenStatusRemote(__pmContext *ctxp, const char *, char ***, int **);

/*
 * Retry with IRIX added to name 
 */
static int
GetChildrenStatusRetry(__pmContext *ctxp, const char *name, char ***offspring, int **statuslist)
{
    int     num_xlch = 0;
    char    **xlch = NULL;
    int     *xlstatus = NULL;
    char    *newname;
    int     i;
    int     j;
    int     n;

    num_xlch = GetXlChildren(name, GetAddIrixNodes(), PMNS_REMOTE, &xlch, &xlstatus, NULL, &newname);
    
    if (num_xlch < 0) {
        if (newname != NULL)
            free(newname);
        return num_xlch;
    }

    if (newname != NULL) {
        n = GetChildrenStatusRemote(ctxp, newname, offspring, statuslist);
        free(newname);
        if (n > 0) {
            /* May need to merge the two lists */
            if (num_xlch > 0) {
                int need = 0;
                int tot_chld = 0;
                char **l_childlst;
                char *cp;

                for(i = 0; i < num_xlch; i++) {
                    need += strlen(xlch[i]) + 1; 
                    tot_chld++;
                }
                for(i = 0; i < n; i++) {
                    need += strlen((*offspring)[i]) + 1; 
                    tot_chld++;
                }

                l_childlst = (char**)malloc(need + tot_chld*sizeof(*l_childlst));
		if (statuslist != NULL)
		    *statuslist = (int*)realloc(*statuslist, tot_chld*sizeof(**statuslist));
                if (l_childlst == NULL || *statuslist == NULL) {
                    free(*offspring);
		    if (statuslist != NULL)
			free(*statuslist);
                    free(xlch);
                    free(xlstatus);
                    return -errno;
                }
               
                cp = (char*)&l_childlst[tot_chld];
                for(i = 0; i < n; i++) {
                    strcpy(cp, (*offspring)[i]);
                    l_childlst[i] = cp;
                    cp += strlen((*offspring)[i]) + 1;
                }
                for(j = 0; j < num_xlch; i++, j++) {
                    strcpy(cp, xlch[j]);
                    l_childlst[i] = cp;
                    cp += strlen(xlch[j]) + 1;
		    if (statuslist != NULL)
			(*statuslist)[i] = xlstatus[j];
                }
                free(*offspring);
                free(xlch);
                free(xlstatus);
                *offspring = l_childlst;
                n += num_xlch;
            }
        }
        else if (n != 0 && n != PM_ERR_NAME) {
            if (num_xlch > 0)
                free(xlch);
            return n;
        }
    }
    else if (num_xlch > 0) {
        *offspring = xlch;
	if (statuslist != NULL)
	    *statuslist = xlstatus;
        n = num_xlch;
    }
    else
        return PM_ERR_NAME;

    return n;
}

/* 
 * Retrieve all Add names starting at the xlator node 'node'
 */
static void
GetAllAddNames(char **newlist, int *newstatus, int *usednames, const nsxl_node *node)
{
    const nsxl_node *np = node->first;

    while(np != NULL) {
        if (strlen(np->add) == 0) {
            GetAllAddNames(newlist, newstatus, usednames, np);
        }
        else {
            newlist[*usednames] = np->add;
            newstatus[*usednames] = np->status;
            (*usednames)++;
        }
        np = np->next;
    }
    
}

/*
 * Check for any children which may be 'strippable'
 */
static int 
CheckForXlChild(int num_offs, char ***offspring)
{
    const nsxl_node *node = GetStripIrixNodes();
    int i;

    for(i = 0; i < num_offs; i++) {
        const nsxl_node *np = node->first;
    
        while(np != NULL) {
            if (strcmp(np->strip, (*offspring)[i]) == 0 && strlen(np->strip) > 0)
                return 1;
            np = np->next;
        }
    }
    
    return 0;
}

/*
 * Change a list of children to allow removal of the irix node.
 */
static int 
RehashChildren(const char *name, int num_offs, char ***offspring, int **statuslist, char **newname)
{
    const nsxl_node *np_unique;
    char            *l_newname;
    const nsxl_node *node;
    const nsxl_node *np;
    int             maxnames = 0;
    int             usednames = 0;
    int             uniqnames = 0;
    char            **newlist;
    int             *newstatus;
    char            **l_offspring;
    int             *l_statuslist;
    int             off_ix;
    int             cmp_ix;
    int             need = 0;
    char            *cp;

    /* Get the default node name */
    l_newname = StripNode(name, 0, GetAddIrixNodes(), &np_unique);
    if (l_newname == NULL)
        return -errno;

    /* Find the node with the default name as its strip entry */
    node = FindNode(l_newname, GetStripIrixNodes());
    free(l_newname);
    if (node == NULL)
        return 0;

    /* Setup a store for the wanted names list */
    maxnames = CountNsxlAddNodes(node);
    maxnames += num_offs;
    if ((newlist = (char**)malloc(maxnames*sizeof(*newlist))) == NULL)
        return -errno;
    if ((newstatus = (int*)malloc(maxnames*sizeof(*newstatus))) == NULL) {
        free(newlist);
        return -errno;
    }

    /* Build a list of ptrs to the string to use as offspring */
    for(off_ix = 0; off_ix < num_offs; off_ix++) {
        np = node->first;
        while(np != NULL) {
            if (strcmp((*offspring)[off_ix], np->strip) == 0) {
                if (strlen(np->add) > 0) {
                    newlist[usednames] = np->add;
                    newstatus[usednames] = np->status;
                    usednames++;
                }
                else {
                    GetAllAddNames(newlist, newstatus, &usednames, np);
                }
                break;
            }
            np = np->next;
        }
        if (np == NULL) {
            newlist[usednames] = (*offspring)[off_ix];
            newstatus[usednames] = (*statuslist)[off_ix];
            usednames++;
        }
    }

    /* May be duplicates in the list, NULL them out */
    uniqnames = usednames;
    for(off_ix = 0; off_ix < usednames; off_ix++) {
        for(cmp_ix = off_ix + 1; cmp_ix < usednames && newlist[off_ix] != NULL; cmp_ix++) {
            if (newlist[cmp_ix] != NULL && strcmp(newlist[off_ix], newlist[cmp_ix]) == 0) {
                newlist[cmp_ix] = NULL;
                uniqnames--;
            }
        }
        if (newlist[off_ix] != NULL)
            need += strlen(newlist[off_ix]) + 1;
    }

    /* Build a new offspring and status list */
    if ((l_offspring = (char**)malloc(uniqnames*sizeof(*l_offspring) + need)) == NULL){
        free(newlist);
        free(newstatus);
        return -errno;
    }
    if ((l_statuslist = (int*)malloc(uniqnames*sizeof(*l_statuslist))) == NULL) {
        free(newlist);
        free(newstatus);
        free(l_offspring);
        return -errno;
    }
    cp = (char *)&l_offspring[uniqnames];
    cmp_ix = 0;
    for(off_ix = 0; off_ix < usednames; off_ix++) {
        if (newlist[off_ix] != NULL) {
            l_statuslist[cmp_ix] = newstatus[off_ix];
            strcpy(cp, newlist[off_ix]);
            l_offspring[cmp_ix] = cp;
            cp += strlen(newlist[off_ix]) + 1;
            cmp_ix++;
        }
    }
    free(*offspring);
    free(*statuslist);
    *offspring = l_offspring;
    *statuslist = l_statuslist;
    *newname = node->strip;

    free(newlist);
    free(newstatus);

    return uniqnames;
}

/*
 * Merge two lists of children. THis may involve removing duplicate entries.
 */
static int 
MergeChildren(const char *name, int num_offs, char ***offspring, int **statuslist, 
              int num_mrg, char ***mrg_off, int **mrg_status)
{
    char    **l_offspring;
    int     *l_statuslist;
    int     i;
    int     n;
    int     need = 0;
    char    *cp;

    for(i = 0; i < num_offs; i++) {
        if (strcmp((*offspring)[i], name) != 0)
            need += strlen((*offspring)[i]) + 1;
        }
    for(i = 0; i < num_mrg; i++)
        need += strlen((*mrg_off)[i]) + 1;

    if ((l_offspring = (char**)malloc((num_offs + num_mrg) * sizeof(*l_offspring) + need)) == NULL)
        return -errno;
    if ((l_statuslist = (int*)malloc((num_offs + num_mrg) * sizeof(*l_statuslist))) == NULL) {
        free(l_offspring);
        return -errno;
        }

    n = 0;
    cp = (char*)&l_offspring[num_offs + num_mrg];

    for(i = 0; i < num_offs; i++) {
        if (strcmp((*offspring)[i], name) != 0) {
	    if (statuslist != NULL)
		l_statuslist[n] = (*statuslist)[i];
            strcpy(cp, (*offspring)[i]);
            l_offspring[n] = cp;
            cp += strlen(cp) + 1;
            n++;
        }
    }
    for(i = 0; i < num_mrg; i++, n++) {
        l_statuslist[n] = (*mrg_status)[i];
        strcpy(cp, (*mrg_off)[i]);
        l_offspring[n] = cp;
        cp += strlen(cp) + 1;
    }

    free(*offspring);
    free(*mrg_off);
    free(*mrg_status);

    *offspring = l_offspring;

    if (statuslist != NULL) {
	free(*statuslist);
	*statuslist = l_statuslist;
    }

    return n;
}
#endif /* IRIX_XLATION */


/*
 * Set current pmns to an externally supplied PMNS.
 * Useful for testing the API routines during debugging.
 */
void
__pmUsePMNS(__pmnsTree *t)
{
    useExtPMNS = 1;
    curr_pmns = t;
}


static const char *
pmPMNSLocationStr(int location)
{
    if (location < 0)
	return pmErrStr(location);

    switch(location) {
      case PMNS_LOCAL: return "Local";
      case PMNS_REMOTE: return "Remote";
      case PMNS_ARCHIVE: return "Archive";
      default: return "Internal Error";
    }
}


static int
LoadDefault(char *reason_msg)
{
  if (main_pmns == NULL) {
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "pmGetPMNSLocation: Loading local PMNS for %s PMAPI context\n",
                reason_msg);
    }
#endif
    if (load(PM_NS_DEFAULT, 1, 0) < 0)
      return PM_ERR_NOPMNS;
    else
      return PMNS_LOCAL;
  }
  else
    return PMNS_LOCAL;
}

/*
 * Return the pmns_location.
 * Possibly load the default PMNS.
 */
int 
pmGetPMNSLocation(void)
{
  int pmns_location = PM_ERR_NOPMNS;
  int n;
  int sts;
  __pmIPC *ipc;
  __pmContext  *ctxp;
  int version = 0;

  if (useExtPMNS) {
      return PMNS_LOCAL;
  }

  /* 
   * Determine if we are to use PDUs or local PMNS file.
   * Load PMNS if necessary.
   */
  if (!havePmLoadCall) {
    if ((n = pmWhichContext()) >= 0) {
      ctxp = __pmHandleToPtr(n);
      switch(ctxp->c_type) {
        case PM_CONTEXT_HOST:
	    if (ctxp->c_pmcd->pc_fd == -1)
		return PM_ERR_IPC;
	    if ((sts = __pmFdLookupIPC(ctxp->c_pmcd->pc_fd, &ipc)) < 0) {
	      __pmNotifyErr(LOG_ERR, 
			"pmGetPMNSLocation: version lookup failed (context=%d, fd=%d): %s", 
			n, ctxp->c_pmcd->pc_fd, pmErrStr(sts));
	      pmns_location = PM_ERR_NOPMNS;
	    }
            else if (ipc->version == PDU_VERSION1) {
	      pmns_location = LoadDefault("PMCD (version 1)");
	    }
	    else if (ipc->version == PDU_VERSION2) {
	      pmns_location = PMNS_REMOTE;
	    }
	    else {
            	__pmNotifyErr(LOG_ERR, 
			"pmGetPMNSLocation: bad host PDU version (context=%d, fd=%d, ver=%d)",
			n, ctxp->c_pmcd->pc_fd, ipc->version);
	      	pmns_location = PM_ERR_NOPMNS;
	    }
	    break;
        case PM_CONTEXT_LOCAL:
	    pmns_location = LoadDefault("local");
	    break;
        case PM_CONTEXT_ARCHIVE:
            version = ctxp->c_archctl->ac_log->l_label.ill_magic & 0xff;
	    if (version == PM_LOG_VERS01) {
	    	pmns_location = LoadDefault("archive (version 1)");
            }
	    else if (version == PM_LOG_VERS02) {
		pmns_location = PMNS_ARCHIVE;
                curr_pmns = ctxp->c_archctl->ac_log->l_pmns; 
            }
	    else {
	        __pmNotifyErr(LOG_ERR, "pmGetPMNSLocation: bad archive version (context=%d, fd=%d, ver=%d)",
			n, ctxp->c_pmcd->pc_fd, version); 
	        pmns_location = PM_ERR_NOPMNS;
	    }

	    break;
        default: 
	    __pmNotifyErr(LOG_ERR, "pmGetPMNSLocation: bogus context type: %d", ctxp->c_type); 
            pmns_location = PM_ERR_NOPMNS;
      }/*switch*/
    }
    else {
      pmns_location = PM_ERR_NOPMNS; /* no context for client */
    }
  }
  else { /* have explicit external load call */
    if (main_pmns == NULL)
      pmns_location = PM_ERR_NOPMNS;
    else
      pmns_location = PMNS_LOCAL;
  }

#ifdef PCP_DEBUG
  if (pmDebug & DBG_TRACE_PMNS) {
    static int last_pmns_location = -1;
    if (pmns_location != last_pmns_location) {
	fprintf(stderr, "pmGetPMNSLocation() -> %s\n", 
            pmPMNSLocationStr(pmns_location));
	last_pmns_location = pmns_location;
    }
  }
#endif

  /* fix up curr_pmns for API ops */
  if (pmns_location == PMNS_LOCAL)
    curr_pmns = main_pmns;
  else if (pmns_location != PMNS_ARCHIVE)
    curr_pmns = NULL;

  return pmns_location;
}/*pmGetPMNSLocation*/


/*
 * Our own PMNS locator.
 * Don't distinguish between ARCHIVE or LOCAL.
 */
static
int GetLocation(void)
{
  int loc = pmGetPMNSLocation();
  if (loc == PMNS_ARCHIVE) return PMNS_LOCAL;
  return loc;
}

#if !defined(LIBIRIXPMDA)
/*
 * support for restricted pmLoadNameSpace services, based upon license
 * capabilities
 *
 * NB! On linux /var/pcp is automagically replaced by the value of
 *     PCP_VAR_DIR from the pcp.conf.
 */

static struct {
    int		cap;		/* & with result from  __pmGetLicenseCap() */
    char	*root;		/* what to use for PM_NS_DEFAULT */
    int		ascii;		/* allow ascii format? */
    int		secure;		/* only secure binary format? */
} ctltab[] = {
    { PM_LIC_PCP,	"/var/pcp/pmns/root",		1,	0 },
    { PM_LIC_WEB,	"/var/pcp/pmns/root_web",	0,	1 },
#ifdef PCP_DEBUG
    { PM_LIC_DEV,	"/tmp/root_dev",		0,	1 },
#endif
    { 0,		"/var/pcp/pmns/root",		1,	0 },
};
static int	numctl = sizeof(ctltab)/sizeof(ctltab[0]);

/*
 * for debugging, and visible via __pmDumpNameSpace()
 *
 * verbosity is 0 (name), 1 (names and pmids) or 2 (names, pmids and linked-list structures)
 */
static void
dumptree(FILE *f, int level, __pmnsNode *rp, int verbosity)
{
    int		i;
    __pmID_int	*pp;

    if (rp != NULL) {
	if (verbosity > 1)
	    fprintf(f, "" PRINTF_P_PFX "%p", rp);
	for (i = 0; i < level; i++) {
	    fprintf(f, "    ");
	}
	fprintf(f, " %-16.16s", rp->name);
	pp = (__pmID_int *)&rp->pmid;
	if (verbosity > 0 && rp->first == NULL)
	    fprintf(f, " %d %d.%d.%d 0x%08x", rp->pmid,
	        pp->domain, pp->cluster, pp->item,
	        rp->pmid);
	if (verbosity > 1) {
	    fprintf(f, "\t[first: ");
	    if (rp->first) fprintf(f, "" PRINTF_P_PFX "%p", rp->first);
	    else fprintf(f, "<null>");
	    fprintf(f, " next: ");
	    if (rp->next) fprintf(f, "" PRINTF_P_PFX "%p", rp->next);
	    else fprintf(f, "<null>");
	    fprintf(f, " parent: ");
	    if (rp->parent) fprintf(f, "" PRINTF_P_PFX "%p", rp->parent);
	    else fprintf(f, "<null>");
	    fprintf(f, " hash: ");
	    if (rp->hash) fprintf(f, "" PRINTF_P_PFX "%p", rp->hash);
	    else fprintf(f, "<null>");
	}
	fputc('\n', f);
	dumptree(f, level+1, rp->first, verbosity);
	dumptree(f, level, rp->next, verbosity);
    }
}
#endif

static void
err(char *s)
{
    if (lineno > 0)
	pmprintf("[%s:%d] ", fname, lineno);
    pmprintf("Error Parsing ASCII PMNS: %s\n", s);
    if (lineno > 0) {
	char	*p;
	pmprintf("    %s", linebuf);
	for (p = linebuf; *p; p++)
	    ;
	if (p[-1] != '\n')
	    pmprintf("\n");
	if (linep) {
	    p = linebuf;
	    for (p = linebuf; p < linep; p++) {
		if (!isspace((int)*p))
		    *p = ' ';
	    }
	    *p++ = '^';
	    *p++ = '\n';
	    *p = '\0';
	    pmprintf("    %s", linebuf);
	}
    }
    pmflush();
}

/*
 * lexical analyser for loading the ASCII pmns
 */
static int
lex(int reset)
{
    static int	first = 1;
    static FILE	*fin;
    static char	*lp;
    char	*tp;
    int		colon;
    int		type;
    int		d, c, e;
    __pmID_int	pmid_int;

    if (reset) {
	/* reset! */
	linep = NULL;
	first = 1;
	return 0;
    }

    if (first) {
	int		i;
	char	*var_dir   = pmGetConfig("PCP_VAR_DIR");
	char	*share_dir = pmGetConfig("PCP_SHARE_DIR");

	first = 0;
	for (i = 0; cpp_path[i] != NULL; i++) {
	    if (access(cpp_path[i], X_OK) != 0)
		continue;
	    if ((lp = (char *)malloc(1 + strlen(CPP_FMT)
		+ strlen(cpp_path[i]) + strlen(CPP_SIMPLE_ARGS)
		+ strlen(var_dir) + strlen(share_dir) 
		+ strlen(fname))) == NULL) {
		return -errno;
	    }

/* safe */  sprintf(lp, CPP_FMT, cpp_path[i], CPP_SIMPLE_ARGS, var_dir, 
		    share_dir, fname);

	    fin = popen(lp, "r");
	    free(lp);
	    if (fin == NULL)
		return -errno;
	    break;
	}
	if (cpp_path[i] == NULL) {
	    pmprintf("pmLoadNameSpace: Unable to find an executable cpp at any of ...\n");
	    for (i = 0; cpp_path[i] != NULL; i++)
		pmprintf("    %s\n", cpp_path[i]);
	    pmprintf("Sorry, but this is fatal\n");
	    pmflush();
	    exit(1);
	}

	lp = linebuf;
	*lp = '\0';
    }

    while (*lp && isspace((int)*lp)) lp++;

    while (*lp == '\0') {
	for ( ; ; ) {
	    char	*p;
	    char	*q;
	    int		inspace = 0;

	    if (fgets(linebuf, sizeof(linebuf), fin) == NULL) {
		if ( pclose(fin) != 0 ) {
		    lineno = -1; /* We're outside of line counting range now */
                    err("cpp returned non-zero exit status");
                    return PM_ERR_PMNS;
		} else {
		    return 0;
		}
	    }
	    for (q = p = linebuf; *p; p++) {
		if (isspace((int)*p)) {
		    if (!inspace) {
			if (q > linebuf && q[-1] != ':')
			    *q++ = *p;
			inspace = 1;
		    }
		}
		else if (*p == ':') {
		    if (inspace) {
			q--;
			inspace = 0;
		    }
		    *q++ = *p;
		}
		else {
		    *q++ = *p;
		    inspace = 0;
		}
	    }
	    if (p[-1] != '\n') {
		err("Absurdly long line, cannot recover");
		pclose(fin);	/* wait for cpp to finish */
		exit(1);
	    }
	    *q = '\0';
	    if (linebuf[0] == '#') {
#if defined(IS_DARWIN)
		if (sscanf(linebuf, "#pragma GCC set_debug_pwd \"%s", fname) == 1)
			goto skipline;
#endif
		/* cpp control line */
		if ( sscanf(linebuf, "# %d \"%s", &lineno, fname) != 2 ) {
		    err ("Illegal cpp construction");
                    return PM_ERR_PMNS;
		}
#if defined(IS_DARWIN)
skipline:
#endif
		--lineno;
		for (p = fname; *p; p++)
		    ;
		*--p = '\0';
		continue;
	    }
	    else
		lineno++;
	    lp = linebuf;
	    while (*lp && isspace((int)*lp)) lp++;
	    break;
	}
    }

    linep = lp;
    tp = tokbuf;
    while (!isspace((int)*lp))
	*tp++ = *lp++;
    *tp = '\0';

    if (tokbuf[0] == '{' && tokbuf[1] == '\0') return LBRACE;
    else if (tokbuf[0] == '}' && tokbuf[1] == '\0') return RBRACE;
    else if (isalpha((int)tokbuf[0])) {
	type = NAME;
	for (tp = &tokbuf[1]; *tp; tp++) {
	    if (*tp == '.')
		type = PATH;
	    else if (!isalpha((int)*tp) && !isdigit((int)*tp) && *tp != '_')
		break;
	}
	if (*tp == '\0') return type;
    }
    colon = 0;
    for (tp = tokbuf; *tp; tp++) {
	if (*tp == ':') {
	    if (++colon > 3) return BOGUS;
	}
	else if (!isdigit((int)*tp)) return BOGUS;
    }

    /*
     * Internal PMID format
     * domain 8 bits
     * cluster 12 bits
     * enumerator 10 bits
     */
    if (sscanf(tokbuf, "%d:%d:%d", &d, &c, &e) != 3 || d > 255 || c > 4095 || e > 1023) {
	err("Illegal PMID");
	return BOGUS;
    }
    pmid_int.pad = 0;
    pmid_int.domain = d;
    pmid_int.cluster = c;
    pmid_int.item = e;
    tokpmid = *(pmID *)&pmid_int;

    return PMID;
}

/*
 * Remove the named node from the seen list and return it.
 * The seen-list is a list of subtrees from pass 1.
 */

static __pmnsNode *
findseen(char *name)
{
    __pmnsNode	*np;
    __pmnsNode	*lnp; /* last np */

    for (np = seen, lnp = NULL; np != NULL; lnp = np, np = np->next) {
	if (strcmp(np->name, name) == 0) {
	    if (np == seen)
		seen = np->next;
	    else
		lnp->next = np->next;
	    np->next = NULL;
	    return np;
	}
    }
    return NULL;
}

/*
 * Attach the subtrees from pass-1 to form a whole 
 * connected tree.
 */
static int
attach(char *base, __pmnsNode *rp)
{
    int		i;
    __pmnsNode	*np;
    __pmnsNode	*xp;
    char	*path;

    if (rp != NULL) {
	for (np = rp->first; np != NULL; np = np->next) {
	    if (np->pmid == PM_ID_NULL) {
		/* non-terminal node ... */
		if (*base == '\0') {
		    if ((path = (char *)malloc(strlen(np->name)+1)) == NULL)
			return -errno;
		    strcpy(path, np->name);
		}
		else {
		    if ((path = (char *)malloc(strlen(base)+strlen(np->name)+2)) == NULL)
			return -errno;
		    strcpy(path, base);
		    strcat(path, ".");
		    strcat(path, np->name);
		}
		if ((xp = findseen(path)) == NULL) {
		    snprintf(linebuf, sizeof(linebuf), "Cannot find definition for non-terminal node \"%s\" in name space",
		        path);
		    err(linebuf);
		    return PM_ERR_PMNS;
		}
		np->first = xp->first;
		free(xp);
		numpmid--;
		i = attach(path, np);
		free(path);
		if (i != 0)
		    return i;
	    }
	}
    }
    return 0;
}

/*
 * Create a fullpath name by walking from the current
 * tree node up to the root.
 */
static int
backname(__pmnsNode *np, char **name)
{
    __pmnsNode	*xp;
    char	*p;
    int		nch;

    nch = 0;
    xp = np;
    while (xp->parent != NULL) {
	nch += (int)strlen(xp->name)+1;
	xp = xp->parent;
    }

    if ((p = (char *)malloc(nch)) == NULL)
	return -errno;

    p[--nch] = '\0';
    xp = np;
    while (xp->parent != NULL) {
	int	xl;

	xl = (int)strlen(xp->name);
	nch -= xl;
	strncpy(&p[nch], xp->name, xl);
	xp = xp->parent;
	if (xp->parent == NULL)
	    break;
	else
	    p[--nch] = '.';
    }
    *name = p;

    return 0;
}

/*
 * Fixup the parent pointers of the tree.
 * Fill in the hash table with nodes from the tree.
 * Hashing is done on pmid.
 */
static int
backlink(__pmnsTree *tree, __pmnsNode *root, int dupok)
{
    __pmnsNode	*np;
    int		status;

    for (np = root->first; np != NULL; np = np->next) {
	np->parent = root;
	if (np->pmid != PM_ID_NULL) {
	    int		i;
	    __pmnsNode	*xp;
	    i = np->pmid % tree->htabsize;
	    for (xp = tree->htab[i]; xp != NULL; xp = xp->hash) {
		if (xp->pmid == np->pmid && !dupok) {
		    char *nn, *xn;
		    backname(np, &nn);
		    backname(xp, &xn);
		    snprintf(linebuf, sizeof(linebuf), "Duplicate metric id (%s) in name space for metrics \"%s\" and \"%s\"\n",
		        pmIDStr(np->pmid), nn, xn);
		    err(linebuf);
		    free(nn);
		    free(xn);
		    return PM_ERR_PMNS;
		}
	    }
	    np->hash = tree->htab[i];
	    tree->htab[i] = np;
	}
	if ((status = backlink(tree, np, dupok)))
	    return status;
    }
    return 0;
}

/*
 * Build up the whole tree by attaching the subtrees
 * from the seen list.
 * Create the hash table keyed on pmid.
 *
 */
static int
pass2(int dupok)
{
    __pmnsNode	*np;
    int		status;

    lineno = -1;

    main_pmns = (__pmnsTree*)malloc(sizeof(*main_pmns));
    if (main_pmns == NULL) {
        return -errno;
    }

    /* Get the root subtree out of the seen list */
    if ((main_pmns->root = findseen("root")) == NULL) {
	err("No name space entry for \"root\"");
	return PM_ERR_PMNS;
    }

    if (findseen("root") != NULL) {
	err("Multiple name space entries for \"root\"");
	return PM_ERR_PMNS;
    }

    /* Build up main tree from subtrees in seen-list */
    if ( (status = attach("", main_pmns->root)) )
	return status;

    /* Make sure all subtrees have been used in the main tree */
    for (np = seen; np != NULL; np = np->next) {
	snprintf(linebuf, sizeof(linebuf), "Disconnected subtree (\"%s\") in name space", np->name);
	err(linebuf);
	status = PM_ERR_PMNS;
    }
    if (status)
	return status;

    main_pmns->symbol = NULL;
    main_pmns->contiguous = 0;
    main_pmns->mark_state = UNKNOWN_MARK_STATE;

    return __pmFixPMNSHashTab(main_pmns, numpmid, dupok);

}







/*
 * clear/set the "mark" bit used by pmTrimNameSpace, for all pmids
 */
static void
mark_all(__pmnsTree *pmns, int bit)
{
    int		i;
    __pmnsNode	*np;
    __pmnsNode	*pp;

    if (pmns->mark_state == bit)
        return;

    pmns->mark_state = bit;
    for (i = 0; i < pmns->htabsize; i++) {
	for (np = pmns->htab[i]; np != NULL; np = np->hash) {
	    for (pp = np ; pp != NULL; pp = pp->parent) {
		if (bit)
		    pp->pmid |= MARK_BIT;
		else
		    pp->pmid &= ~MARK_BIT;
	    }
	}
    }
}

/*
 * clear/set the "mark" bit used by pmTrimNameSpace, for one pmid, and
 * for all parent nodes on the path to the root of the PMNS
 */
static void
mark_one(__pmnsTree *pmns, pmID pmid, int bit)
{
    __pmnsNode	*np;

    if (pmns->mark_state == bit)
        return;

    pmns->mark_state = UNKNOWN_MARK_STATE;
    for (np = pmns->htab[pmid % pmns->htabsize]; np != NULL; np = np->hash) {
	if ((np->pmid & PMID_MASK) == (pmid & PMID_MASK)) {
	    for ( ; np != NULL; np = np->parent) {
		if (bit)
		    np->pmid |= MARK_BIT;
		else
		    np->pmid &= ~MARK_BIT;
	    }
	    return;
	}
    }
}



/*
 * Create a new empty PMNS for Adding nodes to.
 * Use with __pmAddPMNSNode() and __pmFixPMNSHashTab()
 */
int
__pmNewPMNS(__pmnsTree **pmns)
{
    __pmnsTree *t = NULL;
    __pmnsNode *np = NULL;

    t = (__pmnsTree*)malloc(sizeof(*main_pmns));
    if (t == NULL) {
        return -errno;
    }

    /* Insert the "root" node first */
    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
	return -errno;
    np->pmid = PM_ID_NULL;
    np->parent = np->first = np->hash = np->next = NULL;
    np->name = strdup("root");
    if (np->name == NULL) {
	free(np);
	return -errno;
    }

    t->root = np;
    t->htab = NULL;
    t->htabsize = 0;
    t->symbol = NULL;
    t->contiguous = 0;
    t->mark_state = UNKNOWN_MARK_STATE;

    *pmns = t;
    return 0;
}

/*
 * Go through the tree and build a hash table.
 * Fix up parent links while we're there.
 * Unmark all nodes.
 */
int
__pmFixPMNSHashTab(__pmnsTree *tree, int numpmid, int dupok)
{
    int sts;
    int htabsize = numpmid/5;

    /*
     * make the average hash list no longer than 5, and the number
     * of hash table entries not a multiple of 2, 3 or 5
     */
    if (htabsize % 2 == 0) htabsize++;
    if (htabsize % 3 == 0) htabsize += 2;
    if (htabsize % 5 == 0) htabsize += 2;
    tree->htabsize = htabsize;
    tree->htab = (__pmnsNode **)calloc(htabsize, sizeof(__pmnsNode *));
    if (tree->htab == NULL)
	return -errno;

    if ((sts = backlink(tree, tree->root, dupok)) < 0)
        return sts;
    mark_all(tree, 0);
    return 0;
}


/*
 * Add a new node for fullpath, name, with pmid.
 * Does NOT update the hash table;
 * need to call __pmFixPMNSHashTab() for that.
 * Recursive routine.
 */

static int
AddPMNSNode(__pmnsNode *root, int pmid, const char *name)
{
    __pmnsNode *np = NULL;
    const char *tail;
    int nch;

    /* Traverse until '.' or '\0' */
    for (tail = name; *tail && *tail != '.'; tail++)
	;

    nch = (int)(tail - name);

    /* Compare name with all the child nodes */
    for (np = root->first; np != NULL; np = np->next) {
	if (strncmp(name, np->name, (int)nch) == 0 && np->name[(int)nch] == '\0')
	    break;
    }

    if (np == NULL) { /* no match with child */
	__pmnsNode *parent_np = root;
        const char *name_p = name;
        int is_first = 1;
 
        /* create nodes until reach leaf */

        for ( ; ; ) { 
	    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
		return -errno;


            /* fixup name */
	    if ((np->name = (char *)malloc(nch+1)) == NULL)
		return -errno;
	    strncpy(np->name, name_p, nch);
	    np->name[nch] = '\0';


            /* fixup some links */
	    np->first = np->hash = np->next = NULL;
            np->parent = parent_np;
            if (is_first) {
                is_first = 0;
                if (root->first != NULL) {
                    /* chuck new node at front of list */
                    np->next = root->first;
		}
            }
            parent_np->first = np;

            /* at this stage, assume np is a non-leaf */
	    np->pmid = PM_ID_NULL;

            parent_np = np;
            if (*tail == '\0')
               break;
            name_p += nch+1; /* skip over node + dot */ 
            for (tail = name_p; *tail && *tail != '.'; tail++)
	        ;
            nch = (int)(tail - name_p);

        }/*loop*/

	np->pmid = pmid; /* set pmid of leaf node */
	return 0;
    }
    else if (*tail == '\0') { /* matched with whole path */
        if (np->pmid != pmid)
	    return PM_ERR_PMID;
        else 
	    return 0;
    }
    else {
	return AddPMNSNode(np, pmid, tail+1); /* try matching with rest of pathname */
    }

}


/*
 * Add a new node for fullpath, name, with pmid.
 * NOTE: Need to call __pmFixPMNSHashTab() to update hash table
 *       when have finished adding nodes.
 */
int
__pmAddPMNSNode(__pmnsTree *tree, int pmid, const char *name)
{
    if (tree->contiguous) {
       pmprintf("Cannot add node to contiguously allocated tree!\n"); 
       pmflush();
       exit(1);
    }

    return AddPMNSNode(tree->root, pmid, name);
}


#if !defined(LIBIRIXPMDA)

/*
 * 32-bit and 64-bit dependencies ... there are TWO external format,
 * both created by pmnscomp ... choose the correct one based upon
 * how big pointer is ...
 *
 * Magic cookies in the binary format file
 *	PmNs	- old 32-bit (Version 0)
 *	PmN1	- new 32-bit and 64-bit (Version 1)
 *	PmN2	- new 32-bit and 64-bit (Version 1 + checksum)
 *
 * if "secure", only Version 2 is allowed
 *
 * File format:
 *
 *   Version 0
 *     htab
 *     tree-nodes
 *     symbols
 *
 *
 *
 *   Version 1/2
 *     symbols
 *     list of binary-format PMNS (see below)
 *
 *   Binary-format PMNS
 *     htab size, htab entry size
 *     tree-node-tab size, tree-node-tab entry size
 *     htab
 *     tree-nodes
 *     
 */
static int
loadbinary(int secure)
{
    FILE	*fbin;
    char	magic[4];
    int		nodecnt;
    int		symbsize;
    int		htabsize;
    int		i;
    int		try;
    int		version;
    __int32_t	sum;
    __int32_t	chksum;
    long	endsum;
    __psint_t	ord;
    __pmnsNode	*root;
    __pmnsNode	**htab;
    char	*symbol;

    for (try = 0; try < 2; try++) {
	if (try == 0) {
	    strcpy(linebuf, fname);
	    strcat(linebuf, ".bin");
	}
	else
	    strcpy(linebuf, fname);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "loadbinary(secure=%d file=%s)\n", secure, linebuf);
#endif
	if ((fbin = fopen(linebuf, "r")) == NULL)
	    continue;

	if (fread(magic, sizeof(magic), 1, fbin) != 1) {
	    fclose(fbin);
	    continue;
	}
	version = -1;
	if (strncmp(magic, "PmNs", 4) == 0) {
#if !defined(HAVE_32BIT_PTR)
	    __pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: old 32-bit format binary file \"%s\"", linebuf);
	    fclose(fbin);
	    continue;
#else
	    version = 0;
#endif
	}
	else if (strncmp(magic, "PmN1", 4) == 0)
	    version= 1;
	else if (strncmp(magic, "PmN2", 4) == 0) {
	    version= 2;
	    if (fread(&sum, sizeof(sum), 1, fbin) != 1) {
		fclose(fbin);
		continue;
	    }
	    sum = ntohl(sum);
	    endsum = ftell(fbin);
	    chksum = __pmCheckSum(fbin);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS)
		fprintf(stderr, "Version 2 Binary PMNS Checksums: got=%x expected=%x\n", chksum, sum);
#endif
	    if (chksum != sum) {
		__pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: checksum failure for binary file \"%s\"", linebuf);
		fclose(fbin);
		continue;
	    }
	    fseek(fbin, endsum, SEEK_SET);
	}
	if (version == -1 || (secure && version != 2)) {
	    fclose(fbin);
	    continue;
	}

	if (version == 0) {
	    /*
	     * Expunge support for Version 0 binary PMNS format.
	     * It can never work on anything but 32-bit int and 32-bit ptrs.
	     */
	    goto bad;
	}
	else if (version == 1 || version == 2) {
	    int		sz_htab_ent;
	    int		sz_nodetab_ent;

	    if (fread(&symbsize, sizeof(symbsize), 1, fbin) != 1) goto bad;
	    symbsize = ntohl(symbsize);
	    symbol = (char *)malloc(symbsize);
	    if (symbol == NULL) {
		__pmNoMem("loadbinary-symbol", symbsize, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    if (fread(symbol, sizeof(symbol[0]), 
	        symbsize, fbin) != symbsize) goto bad;


	    /* once for each style ... or until EOF */
	    for ( ; ; ) {
		long	skip;

		if (fread(&htabsize, sizeof(htabsize), 1, fbin) != 1) goto bad;
		htabsize = ntohl(htabsize);
		if (fread(&sz_htab_ent, sizeof(sz_htab_ent), 1, fbin) != 1) goto bad;
		sz_htab_ent = ntohl(sz_htab_ent);
		if (fread(&nodecnt, sizeof(nodecnt), 1, fbin) != 1) goto bad;
		nodecnt = ntohl(nodecnt);
		if (fread(&sz_nodetab_ent, sizeof(sz_nodetab_ent), 1, fbin) != 1) goto bad;
		sz_nodetab_ent = ntohl(sz_nodetab_ent);
		if (sz_htab_ent == sizeof(htab[0]) && sz_nodetab_ent == sizeof(*root))
		   break; /* found correct one */

		/* skip over hash-table and node-table */
		skip = htabsize * sz_htab_ent + nodecnt * sz_nodetab_ent;
		fseek(fbin, skip, SEEK_CUR);
            }

	    /* the structure elements are all the right size */
	    main_pmns = (__pmnsTree*)malloc(sizeof(*main_pmns));
	    htab = (__pmnsNode **)malloc(htabsize * sizeof(htab[0]));
	    root = (__pmnsNode *)malloc(nodecnt * sizeof(*root));

	    if (main_pmns == NULL || htab == NULL || root == NULL) {
		__pmNoMem("loadbinary-1",
			 sizeof(*main_pmns) +
			 htabsize * sizeof(htab[0]) + 
			 nodecnt * sizeof(*root),
			 PM_FATAL_ERR);
		/*NOTREACHED*/
	    }

	    if (fread(htab, sizeof(htab[0]), htabsize, fbin) != htabsize) goto bad;
	    if (fread(root, sizeof(*root), nodecnt, fbin) != nodecnt) goto bad;

#if defined(HAVE_32BIT_PTR)
	    /* swab htab : pointers are 32 bits */
	    for (i=0; i < htabsize; i++) {
		htab[i] = (__pmnsNode *)ntohl((__uint32_t)htab[i]);
	    }

	    /* swab all nodes : pointers are 32 bits */
	    for (i=0; i < nodecnt; i++) {
		__pmnsNode *p = &root[i];
		p->pmid = __ntohpmID(p->pmid);
		p->parent = (__pmnsNode *)ntohl((__uint32_t)p->parent);
		p->next = (__pmnsNode *)ntohl((__uint32_t)p->next);
		p->first = (__pmnsNode *)ntohl((__uint32_t)p->first);
		p->hash = (__pmnsNode *)ntohl((__uint32_t)p->hash);
		p->name = (char *)ntohl((__uint32_t)p->name);
	    }
#elif defined(HAVE_64BIT_PTR)
	    /* swab htab : pointers are 64 bits */
	    for (i=0; i < htabsize; i++) {
		__ntohll((char *)&htab[i]);
	    }

	    /* swab all nodes : pointers are 64 bits */
	    for (i=0; i < nodecnt; i++) {
		__pmnsNode *p = &root[i];
		p->pmid = __ntohpmID(p->pmid);
		__ntohll((char *)&p->parent);
		__ntohll((char *)&p->next);
		__ntohll((char *)&p->first);
		__ntohll((char *)&p->hash);
		__ntohll((char *)&p->name);
	    }
#else
!bozo!
#endif

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMNS)
		fprintf(stderr, "Loaded Version 1 or 2 Binary PMNS, nodetab ent = %d bytes\n", sz_nodetab_ent);
#endif
	}

	fclose(fbin);

	/* relocate */
	for (i = 0; i < htabsize; i++) {
	    ord = (ptrdiff_t)htab[i];
	    if (ord == (__psint_t)-1)
		htab[i] = NULL;
	    else
		htab[i] = &root[ord];
	}

	for (i = 0; i < nodecnt; i++) {
	    ord = (__psint_t)root[i].parent;
	    if (ord == (__psint_t)-1)
		root[i].parent = NULL;
	    else
		root[i].parent = &root[ord];
	    ord = (__psint_t)root[i].next;
	    if (ord == (__psint_t)-1)
		root[i].next = NULL;
	    else
		root[i].next = &root[ord];
	    ord = (__psint_t)root[i].first;
	    if (ord == (__psint_t)-1)
		root[i].first = NULL;
	    else
		root[i].first = &root[ord];
	    ord = (__psint_t)root[i].hash;
	    if (ord == (__psint_t)-1)
		root[i].hash = NULL;
	    else
		root[i].hash = &root[ord];
	    ord = (__psint_t)root[i].name;
	    root[i].name = &symbol[ord];
	}

        /* set the pmns tree fields */
	main_pmns->root = root;
	main_pmns->htab = htab;
        main_pmns->htabsize = htabsize;
	main_pmns->symbol = symbol;
	main_pmns->contiguous = 1;
	main_pmns->mark_state = UNKNOWN_MARK_STATE;
	return 1;
	
bad:
	__pmNotifyErr(LOG_WARNING, "pmLoadNameSpace: bad binary file, \"%s\"", linebuf);
	fclose(fbin);
	return 0;
    }

    /* failed to open and/or find magic cookie */
    return 0;
}
#endif


/*
 * fsa for parser
 *
 *	old	token	new
 *	0	NAME	1
 *	0	PATH	1
 *	1	LBRACE	2
 *      2	NAME	3
 *	2	RBRACE	0
 *	3	NAME	3
 *	3	PMID	2
 *	3	RBRACE	0
 */
static int
loadascii(int dupok)
{
    int		state = 0;
    int		type;
    __pmnsNode	*np;


    /* do some resets */
    lex(1);      /* reset analyzer */
    seen = NULL; /* make seen-list empty */
    numpmid = 0;


    if (access(fname, R_OK) == -1) {
	snprintf(linebuf, sizeof(linebuf), "Cannot open \"%s\"", fname);
	err(linebuf);
	return -errno;
    }
    lineno = 1;

    while ((type = lex(0)) > 0) {
	switch (state) {

	case 0:
	    if (type != NAME && type != PATH) {
		err("Expected NAME or PATH");
		return PM_ERR_PMNS;
	    }
	    state = 1;
	    break;

	case 1:
	    if (type != LBRACE) {
		err("{ expected");
		return PM_ERR_PMNS;
	    }
	    state = 2;
	    break;

	case 2:
	    if (type == NAME) {
		state = 3;
	    }
	    else if (type == RBRACE) {
		state = 0;
	    }
	    else {
		err("Expected NAME or }");
		return PM_ERR_PMNS;
	    }
	    break;

	case 3:
	    if (type == NAME) {
		state = 3;
	    }
	    else if (type == PMID) {
		np->pmid = tokpmid;
		state = 2;
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_PMNS) {
				fprintf(stderr, "pmLoadNameSpace: %s -> 0x%0x\n",
						np->name, (int)np->pmid);
			}
#endif
	    }
	    else if (type == RBRACE) {
		state = 0;
	    }
	    else {
		err("Expected NAME, PMID or }");
		return PM_ERR_PMNS;
	    }
	    break;

	default:
	    err("Internal botch");
	    abort();

	}

	if (state == 1 || state == 3) {
	    if ((np = (__pmnsNode *)malloc(sizeof(*np))) == NULL)
		return -errno;
	    numpmid++;
	    if ((np->name = (char *)malloc(strlen(tokbuf)+1)) == NULL)
		return -errno;
	    strcpy(np->name, tokbuf);
	    np->first = np->hash = np->next = np->parent = NULL;
	    np->pmid = PM_ID_NULL;
	    if (state == 1) {
		np->next = seen;
		seen = np;
	    }
	    else {
		if (seen->hash)
		    seen->hash->next = np;
		else
		    seen->first = np;
		seen->hash = np;
	    }
	}
	else if (state == 0) {
	    if (seen) {
		__pmnsNode	*xp;

		for (np = seen->first; np != NULL; np = np->next) {
		    for (xp = np->next; xp != NULL; xp = xp->next) {
			if (strcmp(xp->name, np->name) == 0) {
			    snprintf(linebuf, sizeof(linebuf), "Duplicate name \"%s\" in subtree for \"%s\"\n",
			        np->name, seen->name);
			    err(linebuf);
			    return PM_ERR_PMNS;
			}
		    }
		}
	    }
	}
    }

    if (type == 0)
	type = pass2(dupok);


    if (type == 0) {
#ifdef MALLOC_AUDIT
	_malloc_reset_();
	atexit(_malloc_audit_);
#endif
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS)
	    fprintf(stderr, "Loaded ASCII PMNS\n");
#endif
    }

    return type;
}

static int
whichcap(void)
{
    int cap;
    int i;

    cap = __pmGetLicenseCap();
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS)
	fprintf(stderr, "__pmGetLicenseCap -> cap=%x\n", cap);
#endif
    for (i = 0; i < numctl-1; i++) {
	if ((cap & ctltab[i].cap) == ctltab[i].cap)
	    return i;
    }
    /*
     * last entry in the table is a "catch all" default
     */
    return numctl-1;
}

static const char * 
getfname(const char *filename, int cap_id)
{
    /*
     * 0xffffffff is there to maintain backwards compatibility with
     * PCP 1.0
     */
    if (filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff) {
	char	*def_pmns;
	def_pmns = getenv("PMNS_DEFAULT");
	if (def_pmns != NULL) {
	    /* get default PMNS name from environment */
	    return def_pmns;
	}
	else {
	    /* otherwise use the hard-coded pathname but check for prefix */
	    if ( ! strncmp (ctltab[cap_id].root, "/var/pcp", 8) ) {
		static char repname[MAXPATHLEN];
		snprintf (repname, sizeof(repname), "%s%s", pmGetConfig("PCP_VAR_DIR"),ctltab[cap_id].root+8);
		return (repname);
	    } else { /* Doesn't start with /var/pcp - return as is */
		return ctltab[cap_id].root;
	    }
	}
    }
    else
	return filename;

}

int
__pmHasPMNSFileChanged(const char *filename)
{
  static const char *f = NULL;

  int cap_id = whichcap();

  if (cap_id < 0)
     return 1; /* error encountered -> must have changed :) */

  f = getfname(filename, cap_id);

  if (f == NULL)
     return 1; /* error encountered -> must have changed :) */
 
  /* if still using same filename ... */
  if (strcmp(f, fname) == 0) {
     struct stat statbuf;

     if (stat(f, &statbuf) == 0) {
         /* If the modification times have changed */
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "__pmHasPMNSFileChanged(%s) -> %s last=%d now=%d\n",
		filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff ? "PM_NS_DEFAULT" : filename,
		f, (int)last_mtim, (int)statbuf.st_mtime);
	}
#endif
         return ((statbuf.st_mtime == last_mtim) ? 0 : 1);
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "__pmHasPMNSFileChanged(%s) -> %s last=%d.%09ld now=%d.%09ld\n",
		filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff ? "PM_NS_DEFAULT" : filename,
		f, (int)last_mtim.tv_sec, last_mtim.tv_nsec,
		(int)statbuf.st_mtimespec.tv_sec, statbuf.st_mtimespec.tv_nsec);
	}
#endif
	return ((statbuf.st_mtimespec.tv_sec == last_mtim.tv_sec &&
	    statbuf.st_mtimespec.tv_nsec == last_mtim.tv_nsec) ? 0 : 1);
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "__pmHasPMNSFileChanged(%s) -> %s last=%d.%09ld now=%d.%09ld\n",
		filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff ? "PM_NS_DEFAULT" : filename,
		f, (int)last_mtim.tv_sec, last_mtim.tv_nsec,
		(int)statbuf.st_mtim.tv_sec, statbuf.st_mtim.tv_nsec);
	}
#endif
	return ((statbuf.st_mtim.tv_sec == last_mtim.tv_sec &&
	    statbuf.st_mtim.tv_nsec == last_mtim.tv_nsec) ? 0 : 1);
#else
!bozo!
#endif
     }
     else {
         return 1; /* error encountered -> must have changed :) */
     }
  }
  return 1; /* different filenames atleast */
}

static int
load(const char *filename, int binok, int dupok)
{
    int 	i = 0;

    if (main_pmns != NULL) {
	if (export) {
	    export = 0;

	    /*
	     * drop the loaded PMNS ... huge memory leak, but it is
	     * assumed the caller has saved the previous PMNS after calling
	     * __pmExportPMNS()
	     */
	    main_pmns = NULL;
	}
	else {
	    return PM_ERR_DUPPMNS;
        }
    }


#if !defined(LIBIRIXPMDA)

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "pmLoadNameSpace: control table\n");
	for (i = 0; i < numctl; i++) {
	    fprintf(stderr, "[%d] cap=0x%x ascii=%d secure=%d root=%s\n", i,
		ctltab[i].cap, ctltab[i].ascii, ctltab[i].secure, ctltab[i].root);
	}
	fprintf(stderr, "PMNS_DEFAULT=%s\n", getenv("PMNS_DEFAULT"));
    }
#endif

    i = whichcap();
    if (i < 0)
       return PM_ERR_LICENSE;

    strcpy(fname, getfname(filename, i));
 
#else
    if (filename == PM_NS_DEFAULT || (__psint_t)filename == 0xffffffff) {
	/* in the libirixpmda build context, we _must_ give a file name */
	fprintf(stderr, "pmLoadNameSpace: require filename for "
	                "libirixpmda build use!\n");
	exit(1);
    }
    strcpy(fname, filename);
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS)
	fprintf(stderr, "load(name=%s, binok=%d, dupok=%d) lic case=%d fname=%s\n",
		filename, binok, dupok, i, fname);
#endif

    /* Note modification time of pmns file */
    {
        struct stat statbuf;
        if (stat(fname, &statbuf) == 0) {
#if defined(HAVE_ST_MTIME_WITH_E)
            last_mtim = statbuf.st_mtime; /* possible struct assignment */
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
            last_mtim = statbuf.st_mtimespec; /* possible struct assignment */
#else
            last_mtim = statbuf.st_mtim; /* possible struct assignment */
#endif
        }
    }

#if !defined(LIBIRIXPMDA)
    /* try the easy way, c/o pmnscomp */
    if (binok && loadbinary(ctltab[i].secure)) {
	mark_all(main_pmns, 0);
	return 0;
    }

    if (!ctltab[i].ascii)
	return PM_ERR_LICENSE;
#endif

    /*
     * the hard way, compiling as we go ...
     */
    
    return loadascii(dupok);
}

/*
 * just for pmnscomp to use
 */
__pmnsTree*
__pmExportPMNS(void)
{
    export = 1;
    return main_pmns;
}

/*
 * Find and return the named node in the tree, root.
 */
static __pmnsNode *
locate(const char *name, __pmnsNode *root)
{
    const char	*tail;
    ptrdiff_t	nch;
    __pmnsNode	*np;

    /* Traverse until '.' or '\0' */
    for (tail = name; *tail && *tail != '.'; tail++)
	;

    nch = tail - name;

    /* Compare name with all the child nodes */
    for (np = root->first; np != NULL; np = np->next) {
	if (strncmp(name, np->name, (int)nch) == 0 && np->name[(int)nch] == '\0' &&
	    (np->pmid & MARK_BIT) == 0)
	    break;
    }

    if (np == NULL) /* no match with child */
	return NULL;
    else if (*tail == '\0') /* matched with whole path */
	return np;
    else
	return locate(tail+1, np); /* try matching with rest of pathname */
}

/*
 * PMAPI routines from here down
 */

int
pmLoadNameSpace(const char *filename)
{
    havePmLoadCall = 1;
    return load(filename, 1, 0);
}

int
pmLoadASCIINameSpace(const char *filename, int dupok)
{
    havePmLoadCall = 1;
    return load(filename, 0, dupok);
}

/*
 * Assume that each node has been malloc'ed separately.
 * This is the case for an ASCII loaded PMNS.
 * Traverse entire tree and free each node.
 */
static void
FreeTraversePMNS(__pmnsNode *parent)
{
  __pmnsNode *np, *next;

  if (!parent) return;

  /* Free child sub-trees */
  for (np = parent->first; np != NULL; np = next) {
    next = np->next;
    FreeTraversePMNS(np);
  }

  free(parent->name);
  free(parent);

}

void
__pmFreePMNS(__pmnsTree *pmns)
{
    if (pmns != NULL) {
      if (pmns->contiguous) {
        free(pmns->root);
        free(pmns->htab);
        free(pmns->symbol);
      }
      else { 
        free(pmns->htab);
        FreeTraversePMNS(pmns->root); 
      }

      free(pmns);
    }
}

void
pmUnloadNameSpace(void)
{
    havePmLoadCall = 0;
    __pmFreePMNS(main_pmns);
    main_pmns = NULL;
}

static int
request_names (__pmContext *ctxp, int numpmid, char *namelist[])
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendNameList(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
			 numpmid, namelist, NULL);
    if (n < 0) {
	n = __pmMapErrno(n);
    }

    return (n);
}

int
pmRequestNames (int ctxid, int numpmid, char *namelist[])
{
    int n;
    __pmContext *ctxp;

    if ((n =__pmGetHostContextByID(ctxid, &ctxp)) >= 0) {
	if ((n = request_names(ctxp, numpmid, namelist)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_NAMES;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (n);
}

static int
receive_names (__pmContext *ctxp, int numpmid, pmID pmidlist[])
{
    int n;
    __pmPDU      *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_PMNS_IDS) {
	/* Note:
	 * pmLookupName may return an error even though
	 * it has a valid list of ids.
	 * This is why we need op_status.
	 */
	int op_status; 
	n = __pmDecodeIDList(pb, PDU_BINARY, 
			       numpmid, pmidlist, &op_status);
	if (n >= 0)
	    n = op_status;
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
    }
    else if (n != PM_ERR_TIMEOUT) {
	n = PM_ERR_IPC;
    }

    return (n);
}

int
pmReceiveNames (int ctxid, int numpmid, pmID pmidlist[])
{
    int n;
    __pmContext *ctxp;

    if ((n =__pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_NAMES)) >= 0) {
	n = receive_names(ctxp, numpmid, pmidlist);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;


    }

    return (n);
}

int
pmLookupName(int numpmid, char *namelist[], pmID pmidlist[])
{
    int pmns_location;
    int	sts = 0;

    if (numpmid < 1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmLookupName(%d, ...) bad numpmid!\n", numpmid);
	}
#endif
	return PM_ERR_TOOSMALL;
    }

    pmns_location = GetLocation();
    
    if (pmns_location < 0) {
        sts = pmns_location;
    }
    else if (pmns_location == PMNS_LOCAL) {
        int		i;
        __pmnsNode	*np;

	for (i = 0; i < numpmid; i++) {
            /* if we locate it and its a leaf */
	    if ((np = locate(namelist[i], curr_pmns->root)) != NULL ) {
               if (np->first == NULL)
		  pmidlist[i] = np->pmid;
               else {
		  sts = PM_ERR_NONLEAF;
		  pmidlist[i] = PM_ID_NULL;
               }
            }
#if IRIX_XLATION
	    else {
		char *l_name;
		if (IsIrixName(namelist[i]))
		    l_name = StripIrix(namelist[i]);
		else
		    l_name = AddIrix(namelist[i]);
                if (l_name == NULL) {
                    pmidlist[i] = PM_ID_NULL;
                    sts = PM_ERR_NAME;
                }
                else {
                    if ((np = locate(l_name, curr_pmns->root)) != NULL ) {
                        if (np->first == NULL)
                            pmidlist[i] = np->pmid;
                        else {
                            sts = PM_ERR_NONLEAF;
                            pmidlist[i] = PM_ID_NULL;
                        }
                    }
                    else {
		        sts = PM_ERR_NAME;
		        pmidlist[i] = PM_ID_NULL;
                    }
                    free(l_name);
                }
	    }
#elif
	    else {
		sts = PM_ERR_NAME;
		pmidlist[i] = PM_ID_NULL;
	    }
#endif /* IRIX_XLATION */
	}

    	sts = (sts == 0 ? i : sts);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    int	i;
	    fprintf(stderr, "pmLookupName(%d, ...) using local PMNS returns %d and ...\n",
		numpmid, sts);
	    for (i = 0; i < numpmid; i++) {
		fprintf(stderr, "  name[%d]: \"%s\"", i, namelist[i]);
		if (sts >= 0)
		    fprintf(stderr, " PMID: 0x%x %s",
			pmidlist[i], pmIDStr(pmidlist[i]));
		fputc('\n', stderr);
	    }
	}
#endif

    }
    else {
        /* assume PMNS_REMOTE */
	int         n;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);
	if ((sts = request_names (ctxp, numpmid, namelist)) >= 0) {
	    sts = receive_names(ctxp, numpmid, pmidlist);
#if IRIX_XLATION
	    if (sts == PM_ERR_NAME) {
		/* re-send a list of names which were not found the
		 * first time with the 'irix' node at the start.  if
		 * there is a failure at any step, then just return
		 * the results of the first request.
		 */
		int num_failed;
		__pmPDU      *pb;
		char **fail_nl = BuildFailNameList(namelist, pmidlist, numpmid, &num_failed);
		if (fail_nl != NULL) {
		    n = __pmSendNameList(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
					 num_failed, fail_nl, NULL);
		    if (n > 0) {
			n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
			if (n == PDU_PMNS_IDS) {
			    pmID *fail_pmid = (pmID*)malloc(num_failed*sizeof(*fail_pmid));
			    if (fail_pmid != NULL) {
				int op_status2;
				int sts2;
				sts2 = __pmDecodeIDList(pb, PDU_BINARY, 
							num_failed, fail_pmid, &op_status2);
				if (sts2 >= 0) {
				    MergeLists(pmidlist, numpmid, fail_pmid, num_failed);
				    if (op_status2 < 0)
					sts = op_status2;
				    else
					sts = numpmid;
				}
				free(fail_pmid);
			    }
			}
		    }
		    while( num_failed > 0) {
			free(fail_nl[num_failed - 1]);
			num_failed--;
		    }
		    free(fail_nl);
		}
	    }
#endif /* IRIX_XLATION */
	}
    }

    return sts;
}

#if !defined(LIBIRIXPMDA)

static int
request_names_of_children(__pmContext *ctxp, const char *name, int wantstatus)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendChildReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, name, wantstatus);
    if (n < 0) {
        n =  __pmMapErrno(n);
    }

    return (0);
}

int
pmRequestNamesOfChildern (int ctxid, const char *name, int wantstatus)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID (ctxid, &ctxp)) >= 0) {
	if ((n = request_names_of_children(ctxp, name, wantstatus)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_CHILD;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (n);
}

static int
receive_names_of_children (__pmContext *ctxp, char ***offspring,
			   int **statuslist)
{
    int n;
    __pmPDU      *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_PMNS_NAMES) {
	int numnames;

	n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, 
			       offspring, statuslist);
	if (n >= 0) {
	    n = numnames;
	}
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
	    }
    else if (n != PM_ERR_TIMEOUT)
	n =  PM_ERR_IPC;

    return n;
}

int
pmReceiveNamesOfChildren (int ctxid, char ***offsprings, int **status)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID (ctxid, &ctxp, PDU_PMNS_CHILD)) >= 0) {
	n = receive_names_of_children (ctxp, offsprings, status);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return (n);
}

static int
GetChildrenStatusRemote(__pmContext *ctxp, const char *name,
			char ***offspring, int **statuslist)
{
    int n;

    if ((n = request_names_of_children(ctxp, name,
				       (statuslist==NULL) ? 0 : 1)) >= 0) {
	n = receive_names_of_children (ctxp, offspring, statuslist);
    }
    return (n);
}

/*
 * It is allowable to pass in a statuslist arg of NULL. It is therefore important
 * to check that this is not NULL before accessing it.
 */
int
pmGetChildrenStatus(const char *name, char ***offspring, int **statuslist)
{
    int *status = NULL;
    int pmns_location = GetLocation();

    if (pmns_location < 0 )
	return pmns_location;

    if (name == NULL) 
	return PM_ERR_NAME;

    if (pmns_location == PMNS_LOCAL) {

	__pmnsNode	*np;
	__pmnsNode	*tnp;
	int		i;
	int		j;
	int		need;
	int		num;
	char		**result;
	char		*p;

        int     num_xlch = 0;
        char    **xlch = NULL;
        int     *xlstatus = NULL;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmGetChildren(name=\"%s\") [local]\n", name);
	}
#endif

	/* avoids ambiguity, for errors and leaf nodes */
	*offspring = NULL;
	if (statuslist)
	  *statuslist = NULL;

	if (*name == '\0')
	    np = curr_pmns->root; /* use "" to name the root of the PMNS */

	else if ((np = locate(name, curr_pmns->root)) == NULL) {
#if IRIX_XLATION
	    const nsxl_node *xl_node;
	    if (IsIrixName(name))
		xl_node = GetStripIrixNodes();
	    else
		xl_node = GetAddIrixNodes();
            num_xlch = GetXlChildren(name, xl_node, pmns_location, 
                                     &xlch, &xlstatus, &np, NULL);
            
            if (num_xlch < 0)
	        return num_xlch;
#else
	   return PM_ERR_NAME;
#endif
	}

        if (np != NULL && num_xlch == 0)
	    if (np->first == NULL)
	       /* this is a leaf node */
	       return 0;

	need = 0;
	num = 0;

        if (np != NULL) {
	    for (i = 0, tnp = np->first; tnp != NULL; tnp = tnp->next, i++) {
	        if ((tnp->pmid & MARK_BIT) == 0) {
		    num++;
		    need += sizeof(**offspring) + strlen(tnp->name) + 1;
	        }
	    }
	}
        for(i = 0; i < num_xlch; i++) {
            num++;
            need += sizeof(**offspring) + strlen(xlch[i]) + 1;
        }

	if ((result = (char **)malloc(need)) == NULL)
	    return -errno;

        if (statuslist != NULL) {
          if ((status = (int *)malloc(num*sizeof(int))) == NULL)
            return -errno;
        }

	p = (char *)&result[num];

        if (np != NULL) {
	    for (i = 0, tnp = np->first; tnp != NULL; tnp = tnp->next) {
	        if ((tnp->pmid & MARK_BIT) == 0) {
		    result[i] = p;
		    if (statuslist != NULL) 
		      status[i] = (tnp->first != NULL); /* has children */
		    strcpy(result[i], tnp->name);
		    p += strlen(tnp->name) + 1;
		    i++;
	        }
	    }
	}
        else
            i = 0;

        for(j = 0; j < num_xlch; i++, j++) {
            result[i] = p;
            if (statuslist != NULL)
                status[i] = xlstatus[j];
            strcpy(result[i], xlch[j]);
            p += strlen(xlch[j]) + 1;
        }
        if (num_xlch > 0) {
            free(xlch);
            free(xlstatus);
        }

	*offspring = result;
	if (statuslist != NULL)
	  *statuslist = status;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMNS) {
	    fprintf(stderr, "pmGetChildren -> ");
	    __pmDumpNameList(stderr, num, result);
	}
#endif

	return num;
    }

    else {
	/* assume PMNS_REMOTE */
	int         n;
	__pmContext *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);
        n = GetChildrenStatusRemote(ctxp, name, offspring, statuslist);

#if IRIX_XLATION
        if (n >= 0 && strlen(name) == 0) {
            /*
             * If none of the children were 'irix', then assume a pmcd with a non-irix
             * namespace is being spoken to, so there is no need to check for any more
	     * children.
             */
            if (CheckForXlChild(n, offspring)) {
                char **l_offs;
                int  *l_statl;
                int  l_n;
                l_n = GetChildrenStatusRetry(ctxp, name, &l_offs, &l_statl);
                if (l_n > 0) {
                    char *newname;
                    /*
                     * Now we have a list of children derived from the first default 
                     * GetIrix translation.
                     * This now needs to be adjusted using the StripIrix translation. 
                     * That will group/expand the list of children in line with the
                     * translation rules.
                     */
                    if ((l_n = RehashChildren(name, l_n, &l_offs, &l_statl, &newname)) > 0)
                        n = MergeChildren(newname, n, offspring, statuslist, l_n, &l_offs, &l_statl);
                }
            }
        }

        else if (n == PM_ERR_NAME) {
            /* Retry with IRIX added to name */
            n = GetChildrenStatusRetry(ctxp, name, offspring, statuslist);
        }
#endif /* IRIX_XLATION */
        return n;
    }

}

int
pmGetChildren(const char *name, char ***offspring)
{
  return pmGetChildrenStatus(name, offspring, NULL);
}

static int
request_namebypmid (__pmContext *ctxp, pmID pmid)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendIDList(ctxp->c_pmcd->pc_fd, PDU_BINARY, 1, &pmid, 0);
    if (n < 0)
	n = __pmMapErrno(n);

    return (n);
}

int
pmRequestNameID (int ctxid, pmID pmid)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctxid, &ctxp)) >= 0) {
	if ((n = request_namebypmid (ctxp, pmid)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_IDS;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (n);
}

static int
receive_namesbyid (__pmContext *ctxp, char ***namelist)
{
    int         n;
    __pmPDU      *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
                   ctxp->c_pmcd->pc_tout_sec, &pb);
    
    if (n == PDU_PMNS_NAMES) {
	int numnames;

	n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, namelist, NULL);
	if (n >= 0) {
	    n = numnames;
    	}
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
    }
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return n;
}

static int 
receive_a_name (__pmContext *ctxp, char **name)
{
    int n;
    char **namelist;

    if ((n = receive_namesbyid(ctxp, &namelist)) >= 0) {
	char *newname = strdup(namelist[0]);
	free(namelist);
	if (newname == NULL) {
	    n =  -((errno) ? errno : ENOMEM);
	} else {
	    *name = newname;
	    n = 0;
	}
    }

    return (n);
}

int
pmReceiveNameID (int ctxid, char **name)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_IDS)) >= 0) {
	n = receive_a_name (ctxp, name);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return (n);
}

int
pmReceiveNamesAll (int ctxid, char ***namelist)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_IDS)) >= 0) {
	n = receive_namesbyid (ctxp, namelist);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return (n);
}

int
pmNameID(pmID pmid, char **name)
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    else if (pmns_location == PMNS_LOCAL) {
    	__pmnsNode	*np;
	for (np = curr_pmns->htab[pmid % curr_pmns->htabsize]; np != NULL; np = np->hash) {
	    if (np->pmid == pmid)
		return backname(np, name);
	}
    	return PM_ERR_PMID;
    }

    else {
	/* assume PMNS_REMOTE */
	int         n;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);

	if ((n = request_namebypmid (ctxp, pmid)) >= 0) {
	    n = receive_a_name(ctxp, name);
	}
	return n;
    }

}

int
pmNameAll(pmID pmid, char ***namelist)
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    else if (pmns_location == PMNS_LOCAL) {
    	__pmnsNode	*np;
	int		sts;
	int		n = 0;
	int		len = 0;
	int		i;
	char	*sp;
	char	**tmp = NULL;

	for (np = curr_pmns->htab[pmid % curr_pmns->htabsize]; np != NULL; np = np->hash) {
	    if (np->pmid == pmid) {
		n++;
		if ((tmp = (char **)realloc(tmp, n * sizeof(tmp[0]))) == NULL)
		    return -errno;
		if ((sts = backname(np, &tmp[n-1])) < 0) {
		    /* error, ... free any partial allocations */
		    for (i = n-2; i >= 0; i--)
			free(tmp[i]);
		    free(tmp);
		    return sts;
		}
		len += strlen(tmp[n-1])+1;
	    }
	}

	if (n == 0)
	    return PM_ERR_PMID;

	len += n * sizeof(tmp[0]);
	if ((tmp = (char **)realloc(tmp, len)) == NULL)
	    return -errno;

	sp = (char *)&tmp[n];
	for (i = 0; i < n; i++) {
	    strcpy(sp, tmp[i]);
	    free(tmp[i]);
	    tmp[i] = sp;
	    sp += strlen(sp)+1;
	}

	*namelist = tmp;
	return n;
    }

    else {
	/* assume PMNS_REMOTE */
	int         n;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);

	if ((n = request_namebypmid (ctxp, pmid)) >= 0) {
	    n = receive_namesbyid (ctxp, namelist);
	}
	return n;
    }
}


/*
 * generic depth-first recursive descent of the PMNS
 */
static int
TraversePMNS_local(const char *name, void(*func)(const char *name))
{
    int		sts;
    char	**enfants;

    if ((sts = pmGetChildren(name, &enfants)) < 0) {
	return sts;
    }
    else if (sts == 0) {
	/* leaf node, name is full name of a metric */
	(*func)(name);
    }
    else if (sts > 0) {
	int	j;
	char	*newname;
	int	n;

	for (j = 0; j < sts; j++) {
	    newname = (char *)malloc(strlen(name) + 1 + strlen(enfants[j]) + 1);
	    if (newname == NULL) {
		printf("pmTraversePMNS: malloc: %s\n", strerror(errno));
		exit(1);
	    }
	    if (*name == '\0')
		strcpy(newname, enfants[j]);
	    else {
		strcpy(newname, name);
		strcat(newname, ".");
		strcat(newname, enfants[j]);
	    }
	    n = TraversePMNS_local(newname, func);
	    free(newname);
	    if (sts == 0)
		sts = n;
	}
	free(enfants);
    }

    return sts;
}

#if IRIX_XLATION
static int 
RetryTraversePMNSRemote(char *name, void(*func)(const char *name), int ctx_fd)
{
    __pmPDU      *pb;
   
    int n = __pmSendTraversePMNSReq(ctx_fd, PDU_BINARY, name);

    if (n >= 0) {
        n = __pmGetPDU(ctx_fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
        if (n == PDU_PMNS_NAMES) {
            int numnames;
            int i;
            char **namelist;

            n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, &namelist, NULL);
            if (n >= 0) {
                for (i=0; i<numnames; i++) {
                    char *stripname = StripIrix(namelist[i]);
                    if (stripname != NULL) {
                        func(stripname);
                        free(stripname);
                    }
                }
                free(namelist);
                return numnames;
            }
        }
        else if (n == PDU_ERROR) {
		    __pmDecodeError(pb, PDU_BINARY, &n);
            return n;
            }
        else if (n != PM_ERR_TIMEOUT)
            return PM_ERR_IPC;
    }
    
    return n;
}
#endif /* IRIX_XLATION */

static int
request_traverse_pmns (__pmContext *ctxp, const char *name)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendTraversePMNSReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, name);
    if (n < 0) {
	n = __pmMapErrno(n);
    }
    return (n);
}

int
pmRequestTraversePMNS (int ctx, const char *name)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if ((n = request_traverse_pmns(ctxp, name)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_PMNS_TRAVERSE;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }
    return (n);
}

int
pmReceiveTraversePMNS (int ctxid, void(*func)(const char *name))
{
    int n;
    __pmContext *ctxp;
    __pmPDU *pb;

    if ((n = __pmGetBusyHostContextByID(ctxid, &ctxp, PDU_PMNS_TRAVERSE)) < 0) {
	return (n);
    }

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_PMNS_NAMES) {
	int numnames;
	int i;
	char **namelist;

	n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, &namelist, NULL);
	if (n >= 0) {
	    for (i=0; i<numnames; i++) {
		func(namelist[i]);
	    }
	
	    free(namelist);
	}
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
    }
    else if (n != PM_ERR_TIMEOUT) {
	n = PM_ERR_IPC;
    }

    ctxp->c_pmcd->pc_curpdu = 0;
    ctxp->c_pmcd->pc_tout_sec = 0;

    return n;
}

int
pmTraversePMNS(const char *name, void(*func)(const char *name))
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    if (name == NULL) 
	return PM_ERR_NAME;

    if (pmns_location == PMNS_LOCAL)
	return TraversePMNS_local(name, func);

    else { 
	/* assume PMNS_REMOTE */
	int         n;
	__pmPDU      *pb;
	__pmContext  *ctxp;

        /* As we have PMNS_REMOTE there must be
         * a current host context.
         */
	n = pmWhichContext();
	assert(n >= 0);
	ctxp = __pmHandleToPtr(n);
	if ((n = request_traverse_pmns (ctxp, name)) < 0) {
	    return (n);
	} else {
	    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
                          TIMEOUT_DEFAULT, &pb);
	    if (n == PDU_PMNS_NAMES) {
		int numnames;
		int i;
                char **namelist;

		n = __pmDecodeNameList(pb, PDU_BINARY, &numnames, 
		                      &namelist, NULL);
		if (n < 0)
		  return n;

#if IRIX_XLATION
                if (strlen(name) == 0) {
                    for (i=0; i<numnames; i++) {
                        char *stripname = StripIrix(namelist[i]);
                        if (stripname != NULL) {
                            func(stripname);
                            free(stripname);
                        }
                        else
                            func(namelist[i]);
                    }
                }
                else {
		    for (i=0; i<numnames; i++) {
                        func(namelist[i]);
                    }
                }
#else
		for (i=0; i<numnames; i++) {
                    func(namelist[i]);
                }
#endif
		free(namelist);
                return n;
	    }
	    else if (n == PDU_ERROR) {
		__pmDecodeError(pb, PDU_BINARY, &n);

#if IRIX_XLATION
                /* if the error is PM_ERR_NAME, AddIrix and try again */
                if (n == PM_ERR_NAME) {
                    /* 
                     * Retry with 'irix' added to the name. 
                     * this may generate a list of names to use.
                     */
                    char **namelist;
                    int  listlen = 0;
                    int  list_ix;
                    if ((n = AddIrixList(name, &namelist, &listlen)) < 0)
                        return n;
                    n = 0;
                    for(list_ix=0; list_ix<listlen; list_ix++) {
                        int rt_sts = RetryTraversePMNSRemote(namelist[list_ix], func, 
                                                             ctxp->c_pmcd->pc_fd);
                        if (rt_sts < 0) {
                            n = rt_sts;
                            break;
                        }
                        n += rt_sts;
                    }
                    for(list_ix=0; list_ix<listlen; list_ix++)
                        free(namelist[list_ix]);
                    free(namelist);
		    return n;
                }
                else
#endif
		    return n;
            }
	    else if (n != PM_ERR_TIMEOUT)
		return PM_ERR_IPC;
            return n;
	}
    }
}/*pmTraversePMNS*/


int
pmTrimNameSpace(void)
{
    int		i;
    __pmContext	*ctxp;
    __pmHashCtl	*hcp;
    __pmHashNode	*hp;
    int 	version;
    int pmns_location = GetLocation();

    if (pmns_location < 0)
	return pmns_location;

    else if (pmns_location == PMNS_REMOTE)
        return 0;


    /* for PMNS_LOCAL ... */

    if ((ctxp = __pmHandleToPtr(pmWhichContext())) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type != PM_CONTEXT_ARCHIVE) {
	/* unset all of the marks */
	mark_all(curr_pmns, 0);
	return 0;
    }

    version = ctxp->c_archctl->ac_log->l_label.ill_magic & 0xff;

    /* Don't do any trimming for the new archives -
     * they have their own built-in PMNS.
     * Exception: if an explicit load PMNS call was made.
     */
    if (version == PM_LOG_VERS01 || havePmLoadCall) {
	/*
	 * (1) set all of the marks, and
	 * (2) clear the marks for those metrics defined in the archive
	 */
	mark_all(curr_pmns, 1);
	hcp = &ctxp->c_archctl->ac_log->l_hashpmid;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i]; hp != NULL; hp = hp->next) {
		mark_one(curr_pmns, (pmID)hp->key, 0);
	    }
	}
    }

    return 0;
}

void
__pmDumpNameSpace(FILE *f, int verbosity)
{
    int pmns_location = GetLocation();

    if (pmns_location < 0)
        fprintf(f, "__pmDumpNameSpace: Unable to determine PMNS location\n");

    else if (pmns_location == PMNS_REMOTE)
        fprintf(f, "__pmDumpNameSpace: Name Space is remote !\n");

    dumptree(f, 0, curr_pmns->root, verbosity);
}

#endif
