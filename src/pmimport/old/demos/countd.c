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
 */

/*
 * pmimport sample plugin
 *
 * This plugin is designed to work in concert with the script 'countproc'.
 * countproc will count the number of processes currently running which
 * end in the letter 'd' (ie. most likely daemons) and other processes.
 * The output of countproc looks like:
 *
 * Tue Dec  1 14:32:34 1998, daemon=29 other=59
 *
 * This plugin will write a PCP archive log record for each line produced
 * by procCount.  It uses two metrics, countd.total.processes and
 * countd.processes; the latter having the instances 'daemon' and 'other'.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmimport.h>
#include "domain.h"

#define PMID(x,y,z)	((x<<22)|(y<<10)|z)


#define LINEBUFSIZE	4096
typedef struct {
    char		buffer[LINEBUFSIZE];
    char		*end;
    char		*start;
    char		*line;
} LineBuffer;


static struct {
    struct timeval	timestamp;
    int			daemon;
    int			other;
} processStats;


static const int	MAX_INDOM_ENTRIES = 1;
static IndomEntry	*indomTable;

#define PM_INDOM_PROC	0
typedef struct {
    char		*metricName;
    pmDesc		desc;
} MetricInfo;
static MetricInfo	metricInfoTable[] = {

    { "countd.processes",
      { PMID ( PMIMPORT,0,0 ) , PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },

    { "countd.total.processes",
      { PMID ( PMIMPORT,0,1 ) , PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } }

};

static int		numMetrics = sizeof(metricInfoTable) /
				     sizeof(metricInfoTable[0]);


/* Prototypes for file-scope functions */
static ResultStatus getNextRecord( int infile );
static int getLine( int infile, LineBuffer *buf );
static void setupProcIndom( IndomEntry *procIndom, int id );
static int addNewName( IndomEntry *indom, char *name );
static pmValueSet *setVset ( pmDesc *desc, pmValueSet **vset );
static int freeVset ( pmValueSet **vset );
static int countNumval ( pmDesc *desc );
static void *__e_malloc ( size_t size, char *where );
static void *__e_realloc ( void *ptr, size_t size, char *where );


/*
 * Open the import file, check that everything is ok, do any once only
 * operations or initializations.
 */
int
primeImportFile ( const char *file, int *infile, char **host, char **timezone ) 
{
    /* open import file */
    if( infile == NULL ) {
	fprintf( stderr, "%s: Bad address for infile\n", pmProgname );
	exit(1);
    }

    if(( *infile = open( file, O_RDONLY )) < 0 ) {
	fprintf( stderr,
		 "%s: Error opening input file %s: %s\n",
		 pmProgname,
		 file,
		 strerror( oserror() ));
	exit(1);
    }

    /* validate hostname */
    if( *host == NULL || *host[0] == '\0' ) {	
	fprintf( stderr,
		 "%s: Hostname must be specified for this plugin\n",
		 pmProgname );
	exit(1);
    }

    if( strlen( *host ) >= PM_LOG_MAXHOSTLEN ) {
	fprintf( stderr, "%s: Hostname too long: %s\n", pmProgname, *host );
	exit(1);
    }

    /* validate timezone */
    if( timezone == NULL || *timezone == '\0' ) {
	fprintf( stderr, "%s: Timezone not specified\n", pmProgname );
	exit(1);
    }

    if( strlen( *timezone ) >= PM_TZ_MAXLEN ) {
	fprintf( stderr,
		 "%s: Timezone too long: %s\n",
		 pmProgname,
		 *timezone );
	exit(1);
    }

    /* setup table of instance domains (just one, in this case) */
    indomTable = __e_malloc( MAX_INDOM_ENTRIES * sizeof( IndomEntry ),
			     "primeImportFile" );

    setupProcIndom( &indomTable[PM_INDOM_PROC], PM_INDOM_PROC );

    /* tell front-end what API version we are using */
    return PMIMPORT_API_VERSION;
}

/*
 * Construct a pmResult for the front-end to write out to the PCP archive.
 */
ResultStatus
getPmResult ( const int infile, pmResult **result ) 
{
    int			i = 0;
    int			j = 0;
    int			sts = 0;
    size_t		need = 0;
    static pmResult	*pmr = NULL;
    static int		oldNumMetrics = 0;
    pmAtomValue		atom;

    if( result == NULL ) {
	return RS_Error;
    }

    /* allocate space for pmResult - replace with highwater mark algorithm */
    if( pmr == NULL || oldNumMetrics != numMetrics ) {
	if( pmr != NULL ) {
	    pmFreeResult( pmr );
	}
	need = sizeof( pmResult ) +
               (( numMetrics - 1 ) * sizeof( pmValueSet * ));
	pmr = __e_malloc( need, "getPmResult" );
	oldNumMetrics = numMetrics;
    }

    /* go get some data */
    switch( getNextRecord( infile )) {
    case RS_Reset:		/* set timestamp of reset record */
	pmr->timestamp = processStats.timestamp;
	return RS_Reset;
    case RS_Error:
	return RS_Error;
    }

    pmr->timestamp = processStats.timestamp;
    pmr->numpmid = numMetrics;

    /* for every metric that we handle */
    for( i = 0; i < numMetrics; i++ ) {

	if(( setVset ( &metricInfoTable[i].desc, &pmr->vset[i] )) == NULL ) {
	    return RS_Error;
	}

	/* for each instance (or just once if not an instance of a domain */
	for( j = 0; j < pmr->vset[i]->numval; j++ ) {

	    switch( metricInfoTable[i].desc.pmid ) {

	    case PMID(PMIMPORT,0,0):	/* countd.processes */
		switch(j) {
		case 0:		/* 'daemon' instance */
		    atom.ul = processStats.daemon;
		    break;
		case 1:		/* 'other' instance */
		    atom.ul = processStats.other;
		    break;
		default:
		    continue;
		}
		break;

	    case PMID(PMIMPORT,0,1):	/* countd.total.processes */
		atom.ul = processStats.daemon + processStats.other;
		break;

	    default:
		continue;
	    }

	    /* stuff the value into the pmResult */
	    sts = __pmStuffValue( &atom,
				  0, 
				  &pmr->vset[i]->vlist[j],
				  metricInfoTable[i].desc.type );
	    if( sts < 0 ) {
		__pmFreeResultValues( pmr );
		return RS_Error;
	    }

	    pmr->vset[i]->valfmt = sts;

	    if( metricInfoTable[i].desc.indom != PM_INDOM_NULL ) {
		pmr->vset[i]->vlist[j].inst =
                    indomTable[metricInfoTable[i].desc.indom].instlist[j];
	    }

	}
    }

    *result = pmr;
    return RS_Ok;
}

/*
 * Do a simple search for a metric's pmDesc and string name based on a pmID
 */
int
getPmDesc( const pmID id, pmDesc **desc, char **metricName ) 
{
    int i = 0;

    if( desc == NULL || metricName == NULL ) {
	return PM_ERR_PMID;
    }

    for( i = 0; i < numMetrics; i++ ) {
	if( metricInfoTable[i].desc.pmid == id ) {
	    *desc = &metricInfoTable[i].desc;
	    *metricName = metricInfoTable[i].metricName;
	    return 1;
	}
    }

    return PM_ERR_PMID;
}

/*
 * Do a simple search for an IndomEntry based on a pmIndom
 */
int
getIndom( const pmInDom id, IndomEntry **entryTable ) 
{
    int i = 0;

    if( entryTable == NULL ) {
	return PM_ERR_INDOM;
    }

    for( i = 0; i < MAX_INDOM_ENTRIES; i++ ) {
	if( indomTable[i].id == id ) {
	    *entryTable = &indomTable[i];
	    return indomTable[i].numinst;
	}
    }

    return PM_ERR_INDOM;
}

/*
 * Setup the Instance Domain information for the procs indom
 */
static void
setupProcIndom( IndomEntry *procIndom, int id )
{
    procIndom->id = id;
    addNewName( procIndom, "daemon" );
    addNewName( procIndom, "other" );
}

/*
 * Go get some data from the import file
 */
static ResultStatus
getNextRecord( int infile )
{
    static LineBuffer	buf;
    struct tm		timestamp;
    int			daemon = 0;
    int			other = 0;
    int			gotdaemon = 0;
    int			gotother = 0;
    char		*p = NULL;
    char		*d = NULL;
    char		*errmsg = NULL;
    int			sts = 0;

    /* read a line */
    sts = getLine( infile, &buf );
    if( sts == 0 ) {
	return RS_Error;
    }

    /* get the date from the line */
    p = strtok(buf.line, ",");
    if( p == NULL ) {
	return RS_Error;
    }

    if( __pmParseCtime(p, &timestamp, &errmsg ) < 0) {
	fprintf( stderr, "%s: Error parsing input date/time\n", pmProgname );
	fprintf( stderr, "%s", errmsg );
	free( errmsg );
	return RS_Error;
    }

    /* for each data field, get the label and data */
    p = strtok( NULL, "= " );
    while( p != NULL )
    {
	d = strtok( NULL, " " );
	if( strcmp( "daemon", p ) == 0 ) {
	    daemon = atoi( d );
	    gotdaemon++;
	}
	else if( strcmp( "other", p ) == 0 ) {
	    other = atoi( d );
	    gotother++;
	}
	else {
	    return RS_Error;
	}
	p = strtok( NULL, "= " );
    }

    processStats.timestamp.tv_sec = mktime(&timestamp);
    processStats.timestamp.tv_usec = 0;

    if( gotdaemon ) {
	processStats.daemon = daemon;
    }
    if( gotother ) {
	processStats.other = other;
    }

    return RS_Ok;
}

/*
 * Buffered read-a-line from a file
 */
static int
getLine( int infile, LineBuffer *buf )
{
    char	*p = NULL;
    int		nchr = 0;
    ssize_t	sts = 0;

    p = buf->start;

    for( ;; ) {
	while( p < buf->end ) {
	    if( *p == '\n' ) {
		*p = '\0';
		buf->line = buf->start;
		buf->start = ++p;
		return (int)(p - buf->line);
	    }
	    p++;
	}

	nchr = (int)(buf->end - buf->start);
	if( nchr == LINEBUFSIZE ) {
	    /* buffer full, and no newline! ... truncate and return */
	    buf->buffer[LINEBUFSIZE - 1] = '\n';
	    p = &buf->buffer[LINEBUFSIZE - 1];
	    continue;
	}
	if( nchr ) {
	    /* shuffle partial line to start of buffer */
	    memcpy(buf, buf->start, nchr);
	}
	buf->start = buf->buffer;
	buf->end = &buf->buffer[nchr];

	/* refill */
	sts = read( infile, buf->end, LINEBUFSIZE - nchr );
	if( sts <= 0 ) {
	    /* no more, either terminate last line, or really return status */
	    if( nchr ) {
		*buf->end = '\n';
		sts = 1;
	    }
	    else
		return (int)sts;
	}
	p = buf->end;
	buf->end = &buf->end[sts];
    }
}

/*
 * Add a new instance to an instance domain
 */
static int
addNewName( IndomEntry *indom, char *name ) 
{
    int i = 0;
    int numinst = indom->numinst;

    if( indom == NULL || name == NULL ) 
	return 0;

    if( indom->namelist != NULL ) {
	for( i = 0; i < numinst; i++ ) {
	    if( strcmp ( indom->namelist[i], name ) == 0 ) {
		return 0;
	    }
	}
    }

    indom->namelist = __e_realloc( indom->namelist,
				   sizeof( char * ) * ( numinst + 1 ) ,
				   "addNewName" ) ;

    indom->instlist = __e_realloc( indom->instlist,
				   sizeof( int * ) * ( numinst + 1 ) ,
				   "addNewName" ) ;

    indom->namelist[numinst] = strdup( name );
    if( indom->namelist[numinst] == NULL ){
	fprintf( stderr,
		 "%s: %s: strdup(%ld) failed: %s\n",
		 pmProgname,
		 "addNewName",
		 strlen( name ),
		 strerror( oserror() )) ;
	exit(1);
    }
    indom->instlist[numinst] = numinst;
    indom->numinst++;
    indom->state = IS_Set;

    return 1;
}

/*
 * Setup the vset part of a pmresult
 */
static pmValueSet *
setVset ( pmDesc *desc, pmValueSet **vset ) 
{
    size_t	need = 0;
    int		numval = 0;

    if( desc == NULL || vset == NULL ) {
	return NULL;
    }

    if( *vset != NULL ) {
	if( (*vset)->pmid == desc->pmid ) {
	    if( (*vset)->numval == countNumval( desc )) {
		return *vset;
	    }
	}
	
	/* This vset is different than last time, so chuck it away and
	   make a new one */
	freeVset( vset );
    }

    numval = countNumval( desc );
    need = sizeof( pmValueSet ) + (( numval - 1 ) * sizeof( pmValue ));
    *vset = __e_malloc( need, "setVset" );
    (*vset)->pmid = desc->pmid;
    (*vset)->numval = numval;

    return *vset;
}

/*
 * Free everything that may be in a vset
 */
static int
freeVset ( pmValueSet **vset ) 
{
    int i = 0;

    if( vset == NULL || *vset == NULL ) {
	return 0;
    }

    if( (*vset)->valfmt == PM_VAL_DPTR ) {
	for( i = 0; i < (*vset)->numval; i++ ) {
	    if( (*vset)->vlist[i].value.pval ) 
		free( (*vset)->vlist[i].value.pval );
	}
    }
    free( *vset );
    *vset = NULL;
    return 1;
}

/*
 * Find the number of values for a pmDesc
 */
static int
countNumval ( pmDesc *desc ) 
{
    int i = 0;

    if( desc->indom == PM_INDOM_NULL ) {
	return 1;
    }

    for( i = 0; i < MAX_INDOM_ENTRIES; i++ ) {
	if( indomTable[i].id == desc->indom ) {
	    return indomTable[i].numinst;
	}
    }

    return 0;
}

/*
 * __e_malloc:  generic malloc routine that can handle error status.
 */
static void *
__e_malloc ( size_t size, char *where ) 
{
    void *ptr = NULL;

    ptr = calloc( 1, size );
    if( !ptr ){
	fprintf( stderr,
		 "%s: %s: malloc(%ld) failed: %s\n",
		 pmProgname,
		 where,
		 size,
		 strerror( oserror() ));
	exit(1);
    }
    return( ptr );
}

/*
 * __e_realloc:  generic realloc routine that can handle error status.
 */
static void *
__e_realloc ( void *ptr, size_t size, char *where ) 
{
    void *n_ptr = NULL;

    n_ptr = realloc( ptr, size );
    if( !n_ptr ){
	fprintf( stderr,
		 "%s: %s: realloc(%ld) failed: %s\n",
		 pmProgname,
		 where,
		 size,
		 strerror( oserror() ));
	exit(1);
    }
    return( n_ptr );
}
