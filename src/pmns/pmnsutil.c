/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "pmapi.h"
#include "impl.h"
#include "pmnsutil.h"

static FILE	*outf;

/*
 * breadth-first traversal
 */
void
pmns_traverse(__pmnsNode *p, int depth, char *path, void(*func)(__pmnsNode *, int, char *))
{
    char	*newpath;
    __pmnsNode	*q;

    if (p != NULL) {
	/* breadth */
	for (q = p; q != NULL; q = q->next) 
	    (*func)(q, depth, path);
	if (depth > 0)
	    (*func)(NULL, -1, NULL);	/* end of level */
	/* descend */
	for (q = p; q != NULL; q = q->next) {
	    if (q->first != NULL) {
		newpath = (char *)malloc(strlen(path)+strlen(q->name)+2);
		if (depth == 0)
		    *newpath = '\0';
		else if (depth == 1)
		    strcpy(newpath, q->name);
		else {
		    strcpy(newpath, path);
		    strcat(newpath, ".");
		    strcat(newpath, q->name);
		}
		pmns_traverse(q->first, depth+1, newpath, func);
		free(newpath);
	    }
	}
    }
}

/*
 * generate an ASCII PMNS from the internal format produced by
 * pmLoadNameSpace and friends
 */
static void
output(__pmnsNode *p, int depth, char *path)
{
    static int lastdepth = -1;

    if (depth == 0) {
	fprintf(outf, "root {\n");
	lastdepth = 1;
	return;
    }
    else if (depth < 0) {
	if (lastdepth > 0)
	    fprintf(outf, "}\n");
	lastdepth = -1;
	return;
    }
    else if (depth != lastdepth)
	fprintf(outf, "\n%s {\n", path);
    lastdepth = depth;
    if (p->first != NULL)
	fprintf(outf, "\t%s\n", p->name);
    else {
	if (IS_DYNAMIC_ROOT(p->pmid))
	    fprintf(outf, "\t%s\t%d:*:*\n", p->name, pmid_cluster(p->pmid));
	else
	    fprintf(outf, "\t%s\t%d:%d:%d\n", p->name, pmid_domain(p->pmid), pmid_cluster(p->pmid), pmid_item(p->pmid));
    }
}

void
pmns_output(__pmnsNode *root, FILE *f)
{
    outf = f;
    pmns_traverse(root, 0, "", output);
    output(NULL, -2, NULL);		/* special hack for null PMNS */
}
