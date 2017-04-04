/*
 * Copyright (c) 2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Uses the same regular expression logic as pmdaweblog, but extracted
 * here so new patterns and access logs can be tested
 *
 * Usage: check_match configfile pat_name [input]
 *	configfile	regex spec file as used by pmdaweblog
 *	pat_name	use only this names regex from configfile
 *	input		test input to try and match, defaults to stdin
 */

#include <ctype.h>
#include <pmapi.h>
#if defined(HAVE_REGEX_H)
#include <regex.h>
#endif
#include <sys/types.h>

int
main(int argc, char *argv[])
{
    FILE	*fc;
    char	buf[1024];
    char	*p;
    char	*q;
#ifdef HAVE_REGEX
    char	*comp = NULL;
    char	sub0[1024];
    char	sub1[1024];
    char	sub2[1024];
    char	sub3[1024];
#endif
    int		lno = 0;
    int		regex_posix = 0;
    int		cern_format = 0;
    int		common_extended_format = 0;
    int		squid_format = 0;
    int		methodpos = 1, c_statuspos = 2, sizepos = 2, s_statuspos = 2;
    long	client_cache_hits, proxy_cache_hits, remote_fetches;
    double	proxy_bytes, remote_bytes;
#if (defined HAVE_REGEXEC) && (defined HAVE_REGCOMP)
    regex_t	re = {0};
    regmatch_t	pmatch[5];
    size_t	nmatch = 5;
#endif    


    if (argc < 3 || argc > 4) {
	fprintf(stderr, "Usage: check_match configfile pat_name [input]\n");
	exit(1);
    }

    if ((fc = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "check_match: cannot open configfile \"%s\": %s\n", argv[1], osstrerror());
	exit(1);
    }

    if (argc == 4) {
	if (freopen(argv[3], "r", stdin) == NULL) {
	    fprintf(stderr, "check_match: cannot open input \"%s\": %s\n", argv[3], osstrerror());
	    exit(1);
	}
    }

    while (fgets(buf, sizeof(buf), fc) != NULL) {
	lno++;

	if (strncmp(buf, "regex", 5) != 0) continue;
	if (strncmp(buf, "regex_posix", 11) == 0) {
	    regex_posix = 1;
	    p = &buf[11];
	}
	else {
	    regex_posix = 0;	    
	    p = &buf[5];
	}

	while (*p && isspace((int)*p)) p++;
	if (*p == '\0') continue;
	q = p++;
	while (*p && !isspace((int)*p)) p++;
	if (*p == '\0') continue;
	*p = '\0';

	cern_format = squid_format = common_extended_format = 0;

	if (strcmp(q, argv[2]) == 0) {
	    if(regex_posix) {

		q = ++p;
		while (*p && !isspace((int)*p)) p++;
		if (*p == '\0') continue;
		*p = '\0';
		fprintf(stderr, "args are (%s)\n", q);
		if(strncmp(q, "method,size", 11) == 0) {
		    cern_format = 1;
		    methodpos = 1;
		    sizepos = 2;
		}
		else if(strncmp(q, "size,method", 11) == 0) {
		    methodpos = 2;
		    sizepos = 1;
		}
		else {
		    char *str;
		    int pos;

		    pos = 1;
		    str=q;
		    do {
			switch(str[0]) {
			case '1':
			    methodpos = pos++;
			    break;
			case '2':
			    sizepos = pos++;
			    break;
			case '3':
			    c_statuspos = pos++;
			    break;
			case '4':
			    s_statuspos = pos++;
			    break;
			case '-':
			    methodpos = 1;
			    sizepos = 2;
			    str[0] = '\0';
			    break;
			case ',':
			case '\0':
			    break;
			default:
			    fprintf(stderr,
				"could figure out arg order params (%s)\n",
				str);
			    exit(1);
			}
		    } while ( *str++ );

                    if(c_statuspos > 0 && s_statuspos > 0) {
			if(strcmp(argv[2], "SQUID") == 0)
                            squid_format = 1;
			else
		            common_extended_format = 1;
		    } else
			cern_format = 1;
		}
	
		fprintf(stderr, "cern: %d, cef: %d, squid: %d, MP: %d, SP: %d, CSP: %d, SSP: %d\n",
			 cern_format, common_extended_format, squid_format,
			 methodpos, sizepos, c_statuspos, s_statuspos);
	    }

	    q = ++p;
	    while (*p && *p != '\n') p++;
	    while (p >= q && isspace((int)*p)) p--;
	    p[1] = '\0';
	    if(regex_posix) {
#ifdef HAVE_REGCOMP
		fprintf(stderr, "%s[%d]: regex_posix: %s\n", argv[1], lno, q);
		fclose(fc);
		if(regcomp(&re, q, REG_EXTENDED) != 0 ) {
		    fprintf(stderr, "Error: bad regular expression\n");
		    exit(1);
		}
#else
		fprintf(stderr, "%s[%d]: no support for POSIX regexp\n", 
			argv[1], lno);
#endif
	    }
	    else {
#ifdef HAVE_REGCMP
		if(strcmp(argv[2], "CERN") == 0)
		    cern_format = 1;
		else if (strcmp(argv[2], "NS_PROXY") == 0)
		    common_extended_format = 1;
		else if (strcmp(argv[2], "SQUID") == 0)
		    squid_format = 1;

		fprintf(stderr, "%s[%d]: regex: %s\n", argv[1], lno, q);
		fclose(fc);
		comp = regcmp(q, NULL);
		if (comp == NULL) {
		    fprintf(stderr, "Error: bad regular expression\n");
		    exit(1);
		}
#else
		fprintf(stderr, "%s[%d]: regcmp is not available\n", 
			argv[1], lno);
#endif
	    }
	    break;
	}
    }

    lno = 0;
    remote_fetches = proxy_cache_hits = client_cache_hits = 0;
    remote_bytes = proxy_bytes = 0.0;
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
	lno++;
	if(regex_posix) {
#ifdef HAVE_REGEXEC
	    if(regexec(&re, buf, nmatch, pmatch, 0) == 0) {
		buf[pmatch[methodpos].rm_eo] = '\0';
		buf[pmatch[sizepos].rm_eo] = '\0';
                if(common_extended_format || squid_format) {
		    buf[pmatch[c_statuspos].rm_eo] = '\0';
		    buf[pmatch[s_statuspos].rm_eo] = '\0';
                }
                
    	        if(common_extended_format) {
                    fprintf(stderr,"[%d] M: %s, S: %s, CS: %s, SS: %s\n",
			      lno,
                              &buf[pmatch[methodpos].rm_so], 
                              &buf[pmatch[sizepos].rm_so], 
                              &buf[pmatch[c_statuspos].rm_so], 
                              &buf[pmatch[s_statuspos].rm_so]); 
                    if(strcmp(&buf[pmatch[c_statuspos].rm_so], "200") == 0 &&
                       strcmp(&buf[pmatch[s_statuspos].rm_so], "200") == 0) {
                      fprintf(stderr,"\tREMOTE fetch of %.0f bytes\n",
                              atof(&buf[pmatch[sizepos].rm_so]));
                      remote_fetches++;
                      remote_bytes += atof(&buf[pmatch[sizepos].rm_so]);
                    }
                    if(strcmp(&buf[pmatch[c_statuspos].rm_so], "200") == 0 &&
                       (strcmp(&buf[pmatch[s_statuspos].rm_so], "304") == 0 ||
                        strcmp(&buf[pmatch[s_statuspos].rm_so], "-") == 0)) {
                      fprintf(stderr,"\tCACHE return of %.0f bytes\n",
                              atof(&buf[pmatch[sizepos].rm_so]));
                      proxy_cache_hits++;
                      proxy_bytes += atof(&buf[pmatch[sizepos].rm_so]);

                    }
                    if(strcmp(&buf[pmatch[c_statuspos].rm_so], "304") == 0 &&
                       (strcmp(&buf[pmatch[s_statuspos].rm_so], "304") == 0 ||
                        strcmp(&buf[pmatch[s_statuspos].rm_so], "-") == 0)) {
                      fprintf(stderr,"\tCLIENT hit of %.0f bytes\n",
                              atof(&buf[pmatch[sizepos].rm_so]));
                      client_cache_hits++;
                    }
    	        } else if(squid_format) {
                    fprintf(stderr,"[%d] M: %s, S: %s, CS: %s, SS: %s\n",
			      lno,
                              &buf[pmatch[methodpos].rm_so], 
                              &buf[pmatch[sizepos].rm_so], 
                              &buf[pmatch[c_statuspos].rm_so], 
                              &buf[pmatch[s_statuspos].rm_so]); 
                    if(strcmp(&buf[pmatch[c_statuspos].rm_so], "200") == 0 &&
                       (strstr(&buf[pmatch[s_statuspos].rm_so],
                                                "_MISS")!=NULL ||
                        strstr(&buf[pmatch[s_statuspos].rm_so],
                                                "_CLIENT_REFRESH")!=NULL ||
                        strstr(&buf[pmatch[s_statuspos].rm_so],
                                                "_SWAPFAIL")!=NULL)){
                      fprintf(stderr,"\tREMOTE fetch of %.0f bytes (code: %s, Squid result code: %s)\n",
                              atof(&buf[pmatch[sizepos].rm_so]),
                              &buf[pmatch[c_statuspos].rm_so], &buf[pmatch[s_statuspos].rm_so]);
                      remote_fetches++;
                      remote_bytes += atof(&buf[pmatch[sizepos].rm_so]);
                    }
                    if(strcmp(&buf[pmatch[c_statuspos].rm_so], "200") == 0 &&
                       strstr(&buf[pmatch[s_statuspos].rm_so], "_HIT") != NULL) {
                      fprintf(stderr,"\tCACHE return of %.0f bytes (code: %s, Squid result code: %s)\n",
                              atof(&buf[pmatch[sizepos].rm_so]),
                              &buf[pmatch[c_statuspos].rm_so], &buf[pmatch[s_statuspos].rm_so]);
                      proxy_cache_hits++;
                      proxy_bytes += atof(&buf[pmatch[sizepos].rm_so]);

                    }
                    if(strcmp(&buf[pmatch[c_statuspos].rm_so], "304") == 0 &&
                       strstr(&buf[pmatch[s_statuspos].rm_so], "_HIT") != NULL) {
                      fprintf(stderr,"\tCLIENT hit of %.0f bytes (code: %s, Squid result code: %s)\n",
                              atof(&buf[pmatch[sizepos].rm_so]), 
                              &buf[pmatch[c_statuspos].rm_so], &buf[pmatch[s_statuspos].rm_so]);
                      client_cache_hits++;
                    }
    	        } else {
	              fprintf(stderr, "[%d] match: method=\"%s\" size=\"%s\"\n", lno, 
                              &buf[pmatch[methodpos].rm_so], &buf[pmatch[sizepos].rm_so]);
                }
            }
	    else
		fprintf(stderr, "[%d] no match: %s\n", lno, buf);
#else
	    fprintf(stderr, "[%d] - no regexec()\n", lno);
#endif
	}
	else {
#ifdef HAVE_REGEX
	    if (regex(comp, buf, sub0, sub1, sub2, sub3) != NULL) {
              if(common_extended_format) {

		fprintf(stderr,"[%d] M: %s, S: %s, CS: %s, SS: %s\n",
		      lno, sub0, sub1, sub2, sub3);

                if(strcmp(sub2, "200") == 0 &&
                   strcmp(sub3, "200") == 0 ) {
                  fprintf(stderr,"\tREMOTE fetch of %s bytes\n", sub1);
                  remote_fetches++;
                  remote_bytes += atof(sub1);
                }
                if(strcmp(sub2, "200") == 0 &&
                   (strcmp(sub3, "304") == 0 || strcmp(sub3, "-") == 0)) {
                  fprintf(stderr,"\tCACHE return of %s bytes\n", sub1);
                  proxy_cache_hits++;
                  proxy_bytes += atof(sub1);
                }
                if(strcmp(sub2, "304") == 0 &&
                   (strcmp(sub3, "304") == 0 || strcmp(sub3, "-") == 0)) {
                  fprintf(stderr,"\tCLIENT hit   of %s bytes\n", sub1);
                  client_cache_hits++;
                } 
              } else if(squid_format) {

                    fprintf(stderr,"[%d] M: %s, S: %s, CS: %s, SS: %s\n",
                              lno, sub0, sub1, sub2, sub3);

                    if(strcmp(sub2, "200") == 0 &&
                       (strstr(sub3, "_MISS") != NULL ||
                        strstr(sub3, "_CLIENT_REFRESH")!= NULL ||
                        strstr(sub3, "_SWAPFAIL") != NULL)){

                      fprintf(stderr,"\tREMOTE fetch of %.0f bytes (code: %s, Squid result code: %s)\n",
                              atof(sub1), 
                              sub2, sub3);

                      remote_fetches++;
                      remote_bytes += atof(sub1);
                    }
                    if(strcmp(sub2, "200") == 0 &&
                       strstr(sub3, "_HIT") != NULL) {

                      fprintf(stderr,"\tCACHE return of %.0f bytes (code: %s, Squid result code: %s)\n",
                              atof(sub1), 
                              sub2, sub3);

                      proxy_cache_hits++;
                      proxy_bytes += atof(sub1);

                    }
                    if(strcmp(sub2, "304") == 0 &&
                       strstr(sub3, "_HIT") != NULL) {

                      fprintf(stderr,"\tCLIENT hit of %.0f bytes (code: %s, Squid result code: %s)\n",
                              atof(sub3), 
                              sub2, sub3);

                      client_cache_hits++;
                    }
	      } else {
	              fprintf(stderr, "[%d] match: method=\"%s\" size=\"%s\"\n", lno, 
                              sub0, sub1);
              }
	    }
	    else
		fprintf(stderr, "[%d] no match: %s\n", lno, buf);
#else
	    fprintf(stderr, "[%d] - no regex()\n", lno);
#endif
	}
    }

    if(common_extended_format || squid_format) {
	fprintf(stderr,"Proxy Cache Summary Report\n\n");

	fprintf(stderr,
	    "# requests %ld\n# client cache hits %ld\n# cache hits %ld\n# remote fetches %ld\n",
	    (client_cache_hits + proxy_cache_hits + remote_fetches),
	    client_cache_hits, proxy_cache_hits, remote_fetches);
	fprintf(stderr,
	    "\nTotal Mbytes      %f bytes\nFrom proxy cache  %f Mbytes\nFrom remote sites %f Mbytes\n\n",
	    (proxy_bytes + remote_bytes)/1000000.0,
	    proxy_bytes/1000000.0, remote_bytes/1000000.0);

	fprintf(stderr,
	    "Client Cache %% hit rate: %.2f\n", 
	    100.0*(float)client_cache_hits/(float)(client_cache_hits + proxy_cache_hits + remote_fetches));
	fprintf(stderr,
	    "Proxy  Cache %% hit rate: %.2f\n", 
	    100.0*(float)proxy_cache_hits/(float)(client_cache_hits + proxy_cache_hits + remote_fetches));
	fprintf(stderr,
	    "Local  Cache %% hit rate: %.2f\n", 
	    100.0*(float)(client_cache_hits + proxy_cache_hits)/
		(float)(client_cache_hits + proxy_cache_hits + remote_fetches));

	fprintf(stderr,
	    "\nAverage fetch size: Proxy  -> Client: %.2f  Kb\n",
	    proxy_bytes/proxy_cache_hits/1000.0);
	fprintf(stderr,
	    "Average fetch size: Remote -> Client : %.2f  Kb\n",
	    remote_bytes/remote_fetches/1000.0);

	fprintf(stderr,"\nClient Cache bandwidth reduction effectiveness: UNKNOWN\n");
	fprintf(stderr,
	    "Proxy  Cache bandwidth reduction effectiveness: %f%%\n", 
	    100.0*proxy_bytes/(proxy_bytes +  remote_bytes));

    }

    exit(0);
}
