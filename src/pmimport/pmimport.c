/*
 * pmimport - front end for PCP archive conversion tool
 *
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
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <stdarg.h>
#include <dlfcn.h>
#include "pmimport.h"

#define MAX_FLUSHSIZE 100000

/* Prototypes */
static int  openArchiveLog ( char *host, char *timezone, char *archBase ) ;
static int  putResult ( pmResult *result ) ;
static int  makeNameList ( IndomEntry *indom, char  ***nl, int **il ) ;
static int  putMark ( __pmTimeval timestamp ) ;
static void parseCommandLine ( int argc, char *argv[] ) ;
static void listPlugins ( char *path ) ;
static void openPlugin ( char *plugin ) ;
static void usage ( void ) ;
static void runDone ( int status, char *msg ) ;
static void *__e_malloc ( size_t size, char *where ) ;
static void __debugMsg ( int dbg, char *format, ... ) ;
static void __errorMsg ( int status, char *format, ... ) ;

/* Global variables */
static char           *plugin; /* filename of plugin */
static char	      pluginPath[MAXPATHLEN];
static char           *archBase = NULL;/* base names for archive */
static char           *infilename = NULL; /* input filename */
static char           *host = NULL; /* hostname */
static char           *tz = NULL; /* timezone */
static const int      archive_version = PM_LOG_VERS02; /* Type of archive */
static __pmLogCtl     logctl;
static struct timeval last_stamp;

/* Externs */
extern int pmDebug;
extern int optind;
extern int errno;

/* API function pointers */
typedef int ( * primeImportFile_t ) ( const char *file,
				      int *fd,
				      char **host,
				      char **timezone );

typedef ResultStatus ( * getPmResult_t ) ( const int infile,
					   pmResult **result );

typedef int ( * getPmDesc_t ) ( const pmID id,
				pmDesc **desc,
				char **metricName );

typedef int ( * getIndom_t ) ( const pmInDom id,
					IndomEntry **entryTable );

primeImportFile_t  primeImportFile_p;
getPmResult_t getPmResult_p;
getPmDesc_t getPmDesc_p;
getIndom_t getIndom_p;

int
main(int argc, char *argv[])
{
    int      infile = 0;
    pmResult *result = NULL;
    int      getAnotherResult = 0;
    int      rval = 0;
    int      sts = 0;

    parseCommandLine ( argc, argv ) ;

    openPlugin ( plugin ) ;

    sts = ( * primeImportFile_p ) ( infilename, &infile, &host, &tz ) ;
    if ( sts < 0 || infile < 0 ) {
	__errorMsg ( 1, "Failed to initialize input file" ) ;
    }

    if ( sts != PMIMPORT_API_VERSION ) {
	__errorMsg ( 1,
		     "Incompatible plugin version.  Got %d, expected %d",
		     sts,
		     PMIMPORT_API_VERSION ) ;
    }

    if ( openArchiveLog ( host, tz, archBase ) == 0 ) {
	__errorMsg ( 1, "Failed to open archive log" ) ;
    }

    getAnotherResult = 1;
    do
    {
	switch ( ( * getPmResult_p ) ( infile, &result ) ) {
	case RS_Ok:		/* got an ok pmResult */
	    rval = putResult ( result ) ;
	    if ( rval <= 0 ) {
		__errorMsg ( 0, "Failed to write pmResult to archive" ) ;
		getAnotherResult = 0;
	    }
	    break;

	case RS_Reset:		/* got a reset marker */
	    {
		__pmTimeval tmp;
		if ( result->timestamp.tv_sec > 0 ||
		     result->timestamp.tv_usec > 0 ) {
		    tmp.tv_sec = ( __int32_t ) result->timestamp.tv_sec;
		    tmp.tv_usec = ( __int32_t ) result->timestamp.tv_usec;
		}
		else {
		    /* write out last timestamp + 1msec (as per pmlogger) */
		    tmp.tv_sec = ( __int32_t ) last_stamp.tv_sec;
		    tmp.tv_usec = ( __int32_t ) last_stamp.tv_usec + 1000;
		}

		__debugMsg( DBG_TRACE_APPL2,
			    "got RS_Reset: %s",
			    asctime ( localtime ( ( clock_t * ) &tmp.tv_sec ) ) ) ;

		rval = putMark ( tmp ) ;
		if ( rval < 0 ) {
		    __errorMsg ( 1,
				 "Unable to put mark record: %s",
				 pmErrStr ( rval ) ) ;
		}
	    }
	    break;

	case RS_Error:
	default:		/* failed - probably eof */
	    getAnotherResult = 0;
	    break;
	}
    } while ( getAnotherResult ) ;

    runDone ( 0, NULL ) ;
    return 0;
}

static int
openArchiveLog ( char *host, char *timezone, char *archBase ) 
{
    int status = 0;

    status = __pmLogCreate ( host, archBase, archive_version, &logctl ) ;
    if ( status < 0 ) {
	__errorMsg ( 0, "__pmLogCreate: %s", pmErrStr ( status ) ) ;
	return 0;
    }

    strcpy ( logctl.l_label.ill_tz, timezone ) ;
    pmNewZone ( logctl.l_label.ill_tz ) ;

    return 1;
}

static int
putResult ( pmResult *result ) 
{
    __pmPDU     *pb = NULL;
    long        old_offset = 0;
    long        old_meta_offset = 0;
    long        new_offset = 0;
    long        new_meta_offset = 0;
    int         needti = 0;
    int         i = 0;
    int         sts = 0;
    static long flushsize = MAX_FLUSHSIZE;
    char        **namelist = NULL;
    int         *instlist = NULL;
    __pmTimeval tmptime;

#ifdef PCP_DEBUG
    if ( pmDebug & DBG_TRACE_APPL2 ) {
	__pmDumpResult ( stdout, result ) ;
    }
#endif

    /* write result to archive */
    sts = __pmEncodeResult ( fileno ( logctl.l_mfp ) , result, &pb ) ;
    if ( sts < 0 ) {
	__errorMsg ( 0, "__pmEncodeResult: %s", pmErrStr ( sts ) ) ;
	runDone ( 1, "Unable to continue" ) ;
    }

    /* force use of log version */
    __pmOverrideLastFd ( fileno ( logctl.l_mfp ) ) ;
    old_offset = ftell ( logctl.l_mfp ) ;

    sts = __pmLogPutResult ( &logctl, pb ) ;
    if ( sts < 0 ) {
	__errorMsg ( 0, "putResult: %s", pmErrStr ( sts ) ) ;
	runDone ( 1, "Unable to continue" ) ;
    }

    /* check that we know about indoms and desc's */
    /* if not, get indoms and descs and write to meta file */
    old_meta_offset = ftell ( logctl.l_mdfp ) ;
    for ( i = 0; i < result->numpmid; i++ ) {
	pmValueSet *vsp = result->vset[i];
	pmDesc     desc;
	pmDesc     *descptr = NULL;
	IndomEntry *indom = NULL;
	char       *names = NULL;
	int        numnames = 0;

	sts = __pmLogLookupDesc ( &logctl, vsp->pmid, &desc ) ;
	if ( sts < 0 ) {
	    /* pmid not found */
	    numnames = ( * getPmDesc_p ) ( vsp->pmid, &descptr, &names ) ;
	    if ( numnames < 0 ) {
		__errorMsg ( 0, "getPmDesc: %s", pmErrStr ( numnames ) ) ;
		runDone ( 1, "Unable to continue" ) ;
	    }
	    sts = __pmLogPutDesc ( &logctl, descptr, numnames, &names ) ;
	    if ( sts < 0 ) {
		__errorMsg ( 0, "__pmLogPutDesc: %s", pmErrStr ( sts ) ) ;
		runDone ( 1, "Unable to continue" ) ;
	    }
	    desc = *descptr;
	}
	/* if an instance domain with values */
	if ( desc.indom != PM_INDOM_NULL && vsp->numval > 0 ) {
	    sts = ( * getIndom_p ) ( desc.indom, &indom ) ;
	    if ( sts < 0 ) {
		__errorMsg ( 0, "getIndom: %s", pmErrStr ( sts ) ) ;
		runDone ( 1, "Unable to continue" ) ;
	    }
	    if ( indom->state == IS_Set ) {
		/* shouldn't assume lists are compacted as we want */
		numnames = makeNameList ( indom, &namelist, &instlist ) ;
		tmptime.tv_sec = ( __int32_t ) result->timestamp.tv_sec;
		tmptime.tv_usec = ( __int32_t ) result->timestamp.tv_usec;
		sts = __pmLogPutInDom ( &logctl,
					indom->id,
					&tmptime,
					numnames,
					instlist,
					namelist ) ;
		/* We're not going to free this memory at all, so's we don't
		   screw up some deep down data structures */
		if ( sts < 0 ) {
		    __errorMsg ( 0, "__pmLogPutInDom: %s", pmErrStr ( sts ) ) ;
		    runDone ( 1, "Unable to continue" ) ;
		}
		indom->state = IS_Acknowledged;
		needti = 1;
		__debugMsg( DBG_TRACE_APPL2, "pmLogPutIndex: indom changed" ) ;
	    }
	}
    }

    /* check if we need to update time log */
    if ( ftell ( logctl.l_mfp ) > flushsize ) {
	__debugMsg( DBG_TRACE_APPL2, "pmLogPutIndex: ftell > flushsize (%ld)", flushsize ) ;
	needti = 1;
    }

    if ( old_offset == 0 || 
	 old_offset == sizeof ( __pmLogLabel ) + ( 2 * sizeof ( int ) ) ) {
	/* first result in this volume */
	__debugMsg( DBG_TRACE_APPL2, "pmLogPutIndex: first result" ) ;
	needti = 1;
    }

    if ( needti ) {
	fflush ( logctl.l_mdfp ) ;
	/*
	 * need to unwind seek pointer to start of most recent
	 * result ( but if this is the first one, skip the label
	 * record, what a crock ) , ... ditto for the meta data
	 */
	if ( old_offset == 0 ) {
	    old_offset = sizeof ( __pmLogLabel ) + ( 2 * sizeof ( int ) ) ;
	}
	new_offset = ftell ( logctl.l_mfp ) ;
	new_meta_offset = ftell ( logctl.l_mdfp ) ;
	fseek ( logctl.l_mfp, old_offset, SEEK_SET ) ;
	fseek ( logctl.l_mdfp, old_meta_offset, SEEK_SET ) ;
	tmptime.tv_sec = ( __int32_t ) result->timestamp.tv_sec;
	tmptime.tv_usec = ( __int32_t ) result->timestamp.tv_usec;

	__debugMsg( DBG_TRACE_APPL2,
		    "pmLogPutIndex: %s",
		    asctime ( localtime ( ( clock_t * ) &tmptime.tv_sec ) ) ) ;
    
	__pmLogPutIndex ( &logctl, &tmptime );
	/*
	 * ... and put them back
	 */
	fseek ( logctl.l_mfp, new_offset, SEEK_SET ) ;
	fseek ( logctl.l_mdfp, new_meta_offset, SEEK_SET ) ;
	flushsize = ftell ( logctl.l_mfp ) + MAX_FLUSHSIZE;
    }

    last_stamp.tv_sec = result->timestamp.tv_sec;
    last_stamp.tv_usec = result->timestamp.tv_usec;

    return 1;
}

static int
makeNameList ( IndomEntry *indom, char  ***nl, int **il ) 
{
    char **namelist = NULL;
    int  *instlist = NULL;
    char *cp = NULL;
    int  presentcnt = 0;
    int  need = 0;
    int  i = 0;

    /* create tmp namelist/instlist to pass to __pmLogPutInDom */

    /* alloc more than enough memory */
    namelist = __e_malloc ( indom->numinst * sizeof ( char * ) ,
			    "makeNameList" ) ;
    instlist = __e_malloc ( indom->numinst * sizeof ( int ) ,
			    "makeNameList" ) ;
    for ( i = 0; i < indom->numinst; i++ ) {
	need += strlen ( indom->namelist[i] ) + 1;
    }
    namelist[0] = __e_malloc ( need,
			       "makeNameList" ) ;

    cp = namelist[0];
    presentcnt = 0;
    for ( i = 0; i < indom->numinst; i++ ) {
	if ( indom->instlist[i] != -1 ) {
	    /* only present instances are included */
	    instlist[presentcnt] = indom->instlist[i];
	    namelist[presentcnt] = cp;
	    strcpy ( cp, indom->namelist[i] ) ;
	    cp += strlen ( indom->namelist[i] ) + 1;
	    presentcnt++;
	}
    }

    *nl = namelist;
    *il = instlist;

    return presentcnt;
}

static int
putMark ( __pmTimeval timestamp ) 
{
    struct {
	__pmPDU		hdr;
	__pmTimeval	timestamp;	/* when returned */
	int		numpmid;	/* zero PMIDs to follow */
	__pmPDU		tail;
    } mark;

    if ( last_stamp.tv_sec == 0 && last_stamp.tv_usec == 0 ) {
	/* no earlier pmResult, no point adding a mark record */
	return 0;
    }

    __debugMsg( DBG_TRACE_APPL2,
		"putMark: %s",
		asctime ( localtime ( ( clock_t * ) &timestamp.tv_sec ) ) ) ;
    
    mark.hdr = htonl ( ( int ) sizeof ( mark ) ) ;
    mark.tail = mark.hdr;
    mark.timestamp.tv_sec = ( __int32_t ) timestamp.tv_sec;
    mark.timestamp.tv_usec = ( __int32_t ) timestamp.tv_usec;
    mark.timestamp.tv_sec = htonl ( mark.timestamp.tv_sec ) ;
    mark.timestamp.tv_usec = htonl ( mark.timestamp.tv_usec ) ;
    mark.numpmid = htonl ( 0 ) ;

    if ( fwrite ( &mark, 1, sizeof ( mark ), logctl.l_mfp ) !=
	 sizeof ( mark ) ) {
	return - ( oserror() ) ;
    }

    return 0;
}

static void
parseCommandLine ( int argc, char *argv[] ) 
{
    int		sep = __pmPathSeparator();
    int 	status = 0;
    int 	getoptCh = 0;

    __pmSetProgname ( argv[0] );
    snprintf ( pluginPath, sizeof(pluginPath), "%s%c" "config" "%c" "pmimport",
		pmGetConfig("PCP_VAR_DIR"), sep, sep );

    while ( ( getoptCh = getopt ( argc, argv, "D:lh:Z:?" ) ) != EOF ) {
        switch ( getoptCh ) {
	case 'D':		/* debug flag */
	    status = __pmParseDebug ( optarg ) ;
	    if ( status < 0 ) {
		__errorMsg ( 0, 
			     "Unrecognized debug flag specification (%s)",
			     optarg ) ;
		usage() ;
		exit ( 1 );
	    }
	    else {
		pmDebug |= status;
	    }
	    break;
	case 'l':		/* list default plugins */
	    listPlugins ( pluginPath ) ;
	    exit(0);
	    break;
	case 'h':		/* host name */
	    host = optarg;
	    break;
	case 'Z':		/* set timezone to argument */
	    tz = optarg;
	    break;
        case '?':
	    usage() ;
	    exit ( 0 ) ;
        default:
	    usage() ;
	    exit ( 1 ) ;
	}
    }

    /* if timezone is not set, set it to $TZ */
    if ( tz == NULL ) {
#if defined(IRIX6_5)
	if ( _MIPS_SYMBOL_PRESENT ( __pmTimezone ) ) {
		tz = __pmTimezone();
	}
	else {
	    if ( ( tz = getenv ( "TZ" ) ) == NULL ) {
		if ( ( tz = strdup ( "UTC" ) ) == NULL ) {
		    __errorMsg ( 1,
				 "%s: strdup(%d) failed: %s",
				 "parseCommandLine",
				 strlen ( "UTC" ) ,
				 strerror ( oserror() ) ) ;
		}
	    }
	}
#else
	tz = __pmTimezone();
#endif
    }

    if ( optind != argc - 3 ) {
	usage() ;
	exit ( 1 ) ;
    }
    
    plugin = argv[optind];
    infilename = argv[optind + 1];
    archBase = argv[optind + 2];

    __debugMsg ( DBG_TRACE_APPL0,
		 "input file: %s, pcp archive: %s",
		 infilename,
		 archBase ) ;
}

static void
listPlugins ( char *path )
{
    DIR			*dirp = NULL;
    struct dirent	*direntp = NULL;
    struct stat		s;
    int			sts = 0;
    int			count = 0;
    int			sep = __pmPathSeparator();
    char		buf[MAXNAMLEN] = "";

    dirp = opendir ( path ) ;
    if ( dirp == NULL ) {
	sts = oserror();
	__errorMsg ( 1,
		     "cannot read directory: %s: %s\n",
		     path,
		     strerror ( sts ) ) ;
    }

    pmprintf ( "Default plugin path: %s\n\n", path ) ;

    while ( ( direntp = readdir ( dirp ) ) != NULL ) {
	if ( direntp->d_name[0] == '.' ) {
	    continue;
	}

	sprintf ( buf, "%s%c%s", path, sep, direntp->d_name );
	if ( stat ( buf,  &s ) != 0 ) {
	    continue;
	}
	if ( ! S_ISREG(s.st_mode) && ! S_ISLNK(s.st_mode)) {
	    continue;
	}

	count++;
	pmprintf ( "  %s\n", direntp->d_name );
    }

    if ( count == 0 ) {
	pmprintf ( "  no plugins found\n" );
    }
    pmflush();
}

static void
openPlugin ( char *plugin )
{
    int   sep = __pmPathSeparator();
    void  *handle;
    char  *p = NULL;

    if ( strchr ( plugin, '/' ) == NULL ) {
	p = __e_malloc ( MAXNAMLEN, "openPlugin" );
	sprintf ( p, "%s%c%s", pluginPath, sep, plugin );
	plugin = p;
    }

#if !defined(HAVE_DLOPEN)
    __errorMsg ( 1,
	    "unable to open plugin %s: no shared library support",
	     plugin );
#else
    handle = dlopen ( plugin, RTLD_NOW );
    if ( handle == NULL ) {
	__errorMsg ( 1,
		    "unable to open plugin %s: %s",
		     plugin,
		     dlerror() ) ;
    }

    primeImportFile_p = ( primeImportFile_t ) dlsym ( handle,
						      "primeImportFile" );
    if ( primeImportFile_p == NULL ) {
	__errorMsg ( 1,
		    "cannot find function primeImportFile in %s: %s",
		     plugin,
		     dlerror() ) ;
    }

    getPmResult_p = ( getPmResult_t ) dlsym ( handle, "getPmResult" );
    if ( getPmResult_p == NULL ) {
	__errorMsg ( 1,
		    "cannot find function getPmResult in %s: %s",
		     plugin,
		     dlerror() ) ;
    }

    getPmDesc_p = ( getPmDesc_t ) dlsym ( handle, "getPmDesc" );
    if ( getPmDesc_p == NULL ) {
	__errorMsg ( 1,
		    "cannot find function getPmDesc in %s: %s",
		     plugin,
		     dlerror() ) ;
    }

    getIndom_p = ( getIndom_t ) dlsym ( handle, "getIndom" );
    if ( getIndom_p == NULL ) {
	__errorMsg ( 1,
		     "cannot find function getIndom in %s: %s",
		     plugin,
		     dlerror() ) ;
    }
#endif
}

static void
usage ( void ) 
{
    pmprintf ( "\
Usage: %s [options] plugin input-file output-archive\n\
where:\n\
  plugin         converter to use\n\
  input-file     input filename\n\
  output-archive name of pcp archive to be created\n\
\n\
options:\n\
  -h host        hostname for metrics source\n\
  -l             list available converters\n\
  -Z timezone    set reporting timezone\n", pmProgname ) ;
    pmflush() ;
}

static void
runDone ( int status, char *msg ) 
{
#ifdef PCP_DEBUG
    if ( pmDebug & DBG_TRACE_APPL0 ) {
        if ( msg != NULL && msg[0] != '\0' ) {
    	    pmprintf ( "%s: %s, exiting\n", pmProgname, msg ) ;
	    pmflush() ;
        }
        else {
    	    pmprintf ( "%s: End of run time, exiting\n", pmProgname ) ;
	    pmflush() ;
        }
    }
#endif

    /* write the last time stamp */
    if ( last_stamp.tv_sec != 0 ) {
	__pmTimeval	tmp;
	tmp.tv_sec = ( __int32_t ) last_stamp.tv_sec;
	tmp.tv_usec = ( __int32_t ) last_stamp.tv_usec;
	__pmLogPutIndex ( &logctl, &tmp ) ;
    }

    __pmLogClose ( &logctl ) ;

    exit ( status ) ;
}

/*
 * __e_malloc:  generic malloc routine that can handle error status.
 */
static void *
__e_malloc ( size_t size, char *where ) 
{
    void *ptr = NULL;

    ptr = calloc ( 1, size ) ;
    if ( !ptr ) {
	__errorMsg ( 1,
		     "%s: malloc(%d) failed: %s",
		     where,
		     size,
		     strerror ( oserror() ) ) ;
    }
    return ( ptr ) ;
}

static void
__debugMsg ( int dbg, char *format, ... ) 
{
#ifdef PCP_DEBUG

    va_list     arglist;
    static char buffer[2048];

    if ( ! ( pmDebug & dbg ) ) {
	return;
    }

    buffer[0] = '\0';

    va_start ( arglist, format ) ;
    vsprintf ( buffer, format, arglist ) ;
    fprintf ( stderr, "%s: %s\n", pmProgname, buffer ) ;
    va_end ( arglist ) ;

#else
    return;
#endif /* PCP_DEBUG */
}

static void
__errorMsg ( int status, char *format, ... )
{
    va_list     arglist;
    static char buffer[2048];

    buffer[0] = '\0';

    va_start ( arglist, format ) ;
    vsprintf ( buffer, format, arglist ) ;
    fprintf ( stderr, "%s: %s\n", pmProgname, buffer ) ;
    va_end ( arglist ) ;

    if ( status > 0 ) {
	exit ( status ) ;
    }
}
