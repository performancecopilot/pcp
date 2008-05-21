/*
 * This software is redistributed under agreement with the author
 * Laurent Demailly (L@demailly.com).  The original version is
 * available under the terms and conditions of The "Artistic License"
 * from http://www.demailly.com/~dl/
 *
 * Copyright (c) 1996-2000 Laurent Demailly.  All rights reserved.
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

/*
 * Http put/get mini lib
 *
 * Description : Use http protocol, connects to server to echange data
 */

/* #define VERBOSE */

/* http_lib - Http data exchanges mini library.
 */


#include <platform_defs.h>
#include <unistd.h>
#include <ctype.h>
#include "http_lib.h"

static int http_read_line (int fd,char *buffer, int max) ;
static int http_read_buffer (int fd,char *buffer, int max) ;

#define SERVER_DEFAULT "adonis"

/* pointer to a mallocated string containing server name or NULL */
char *http_server = NULL;
/* server port number */
int  http_port = 0;
/* pointer to proxy server name or NULL */
char *http_proxy_server = NULL;
/* proxy server port number or 0 */
int http_proxy_port = 0;
/* user agent id string */
static char *http_user_agent="adlib/3 ($Date: 2004/07/22 07:00:57 $)";

/*
 * read a line from file descriptor
 * returns the number of bytes read. negative if a read error occured
 * before the end of line or the max.
 * cariage returns (CR) are ignored.
 */
static int http_read_line (fd,buffer,max) 
     int fd; /* file descriptor to read from */
     char *buffer; /* placeholder for data */
     int max; /* max number of bytes to read */
{ /* not efficient on long lines (multiple unbuffered 1 char reads) */
  int n=0;
  while (n<max) {
    if (read(fd,buffer,1)!=1) {
      n= -n;
      break;
    }
    n++;
    if (*buffer=='\015') continue; /* ignore CR */
    if (*buffer=='\012') break;    /* LF is the separator */
    buffer++;
  }
  *buffer=0;
  return n;
}


/*
 * read data from file descriptor
 * retries reading until the number of bytes requested is read.
 * returns the number of bytes read. negative if a read error (EOF) occured
 * before the requested length.
 */
static int http_read_buffer (fd,buffer,length) 
     int fd;  /* file descriptor to read from */
     char *buffer; /* placeholder for data */
     int length; /* number of bytes to read */
{
  int n,r;
  for (n=0; n<length; n+=r) {
    r=read(fd,buffer,length-n);
    if (r<=0) return -n;
    buffer+=r;
  }
  return n;
}


typedef enum 
{
  CLOSE,  /* Close the socket after the query (for put) */
  KEEP_OPEN /* Keep it open */
} querymode;

#ifndef OSK

static http_retcode http_query(char *command, char *url,
			       char *additional_header, querymode mode, 
			       char* data, int length, int *pfd);
#endif

/* beware that filename+type+rest of header must not exceed MAXBUF */
/* so we limit filename to 256 and type to 64 chars in put & get */
#define MAXBUF 512

// Alarm handler
static int timeout;
static void alrm(int i)
{
	timeout=1;
}

/*
 * Pseudo general http query
 *
 * send a command and additional headers to the http server.
 * optionally through the proxy (if http_proxy_server and http_proxy_port are
 * set).
 *
 * Limitations: the url is truncated to first 256 chars and
 * the server name to 128 in case of proxy request.
 */
static http_retcode http_query(command, url, additional_header, mode,
			      data, length, pfd) 
     char *command;	/* command to send  */
     char *url;		/* url / filename queried  */
     char *additional_header;	/* additional header */
     querymode mode; 		/* type of query */
     char *data;  /* Data to send after header. If NULL, not data is sent */
     int length;  /* size of data */
     int *pfd;    /* pointer to variable where to set file descriptor value */
{
  int     s;
  struct  hostent *hp;
  struct  sockaddr_in     server;
  char header[MAXBUF];
  int  hlg,t,res;
  mysocklen_t	len;
  http_retcode ret;
  int  proxy=(http_proxy_server!=NULL && http_proxy_port!=0);
  int  port = proxy ? http_proxy_port : http_port ;
	int connok;
  
  if (pfd) *pfd=-1;

	// Setup alarm
	timeout=0;
	signal(SIGALRM,alrm);
	alarm(HTTPLIB_TIMEOUT);

  /* get host info by name :*/
  if ((hp = gethostbyname( proxy ? http_proxy_server 
			         : ( http_server ? http_server 
				                 : SERVER_DEFAULT )
                         ))) {
    memset((char *) &server,0, sizeof(server));
    memmove((char *) &server.sin_addr, hp->h_addr, hp->h_length);
    server.sin_family = hp->h_addrtype;
    server.sin_port = (unsigned short) htons( port );
  } else {
		alarm(0); // Clear alarm
    return ERRHOST;
	}

  /* create socket */
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		alarm(0); // Clear alarm
    return ERRSOCK;
	}
  setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, 0, (mysocklen_t)0);

	// Make socket non-blocking so we can timeout on connect
	fcntl(s,F_SETFL,O_NONBLOCK);

  /* connect to server */
	t=0;
	len = sizeof(res);
	getsockopt(s,SOL_SOCKET,SO_ERROR,(void*)&res,&len); // Clear error
	connok=0;
  if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
		if (errno == EINPROGRESS) {
			fd_set wfds;
			struct timeval tv;

			// OK, we will do select()
			FD_ZERO(&wfds);
			FD_SET(s,&wfds);
			tv.tv_sec = HTTPLIB_TIMEOUT;
			tv.tv_usec = 0;
			res = select(s+1,NULL,&wfds,NULL,&tv);
			if (res < 0) {
				// Error
				ret=ERRCONN;
				fprintf(stderr,"Cannot select: %s\n",strerror(errno));
			}
			else if (res == 0) {
				// Timeout
				timeout=1;
				fprintf(stderr,"Timeout\n");
			}
			else {
				// Connected(?)
				len = sizeof(res);
				if (!FD_ISSET(s,&wfds)) {
					ret=ERRCONN; // ???
					fprintf(stderr,"FD not set\n");
				}
				else if (getsockopt(s,SOL_SOCKET,SO_ERROR,(void*)&res,&len) < 0) {
					// Error
					fprintf(stderr,"Cannot get socket options: %s\n",strerror(errno));
					ret=ERRCONN;
				}
				else {
					if ((res == 0) || (res == 1)) connok=1; // No error, OK
					else {
						fprintf(stderr,"Socket error %d\n",res);
						ret=ERRCONN;
					}
				}
			}
			
		}
		else ret=ERRCONN;
	}
	else connok=1;

	// Make socket blocking again
	fcntl(s,F_SETFL,0);
  if (connok) {
    if (pfd) *pfd=s;
    
    /* create header */
    if (proxy) {
      sprintf(header,
"%s http://%.128s:%d/%.256s HTTP/1.0\015\012User-Agent: %s\015\012%s\015\012",
	      command,
	      http_server,
	      http_port,
	      url,
	      http_user_agent,
	      additional_header
	      );
    } else {
			// NOTE: VirtualNameHost support (Host: field) added by lemming@ucw.cz 12/Oct/2000
      sprintf(header,
"%s /%.256s HTTP/1.1\015\012User-Agent: %s\015\012Host: %s\015\012%s\015\012",
	      command,
	      url,
	      http_user_agent,
				http_server,
	      additional_header
	      );
    }
    
    hlg=strlen(header);

    /* send header */
    if (write(s,header,hlg)!=hlg)
      ret= ERRWRHD;

    /* send data */
    else if (length && data && (write(s,data,length)!=length) ) 
      ret= ERRWRDT;

    else {
      /* read result & check */
      ret=http_read_line(s,header,MAXBUF-1);
#ifdef VERBOSE
      fputs(header,stderr);
      putc('\n',stderr);
#endif	
      if (ret<=0) 
	ret=ERRRDHD;
      else if (sscanf(header,"HTTP/1.%*d %03d",(int*)&ret)!=1) 
	  ret=ERRPAHD;
      else if (mode==KEEP_OPEN) {
				alarm(0); // Clear alarm
				return ret;
			}
    }
  }
  /* close socket */
  close(s);
	alarm(0); // Clear alarm
	if (timeout) ret=ERRTIME;
  return ret;
}


/*
 * Put data on the server
 *
 * This function sends data to the http data server.
 * The data will be stored under the ressource name filename.
 * returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters 
 *              and type to 64.
 */
http_retcode http_put(filename, data, length, overwrite, type) 
     char *filename;  /* name of the ressource to create */
     char *data;      /* pointer to the data to send   */
     int length;      /* length of the data to send  */
     int overwrite;   /* flag to request to overwrite the ressource if it
			 was already existing */
     char *type;      /* type of the data, if NULL default type is used */
{
  char header[MAXBUF];
  if (type) 
    sprintf(header,"Content-length: %d\015\012Content-type: %.64s\015\012%s",
	    length,
	    type  ,
	    overwrite ? "Control: overwrite=1\015\012" : ""
	    );
  else
    sprintf(header,"Content-length: %d\015\012%s",length,
	    overwrite ? "Control: overwrite=1\015\012" : ""
	    );
  return http_query("PUT",filename,header,CLOSE, data, length, NULL);
}


/*
 * Get data from the server
 *
 * This function gets data from the http data server.
 * The data is read from the ressource named filename.
 * Address of new new allocated memory block is filled in pdata
 * whose length is returned via plength.
 * 
 * returns a negative error code or a positive code from the server
 * 
 *
 * limitations: filename is truncated to first 256 characters
 */
http_retcode http_get(filename, pdata, plength, typebuf) 
     char *filename; /* name of the ressource to read */
     char **pdata; /* address of a pointer variable which will be set
		      to point toward allocated memory containing read data.*/
     int  *plength;/* address of integer variable which will be set to
		      length of the read data */
     char *typebuf; /* allocated buffer where the read data type is returned.
		    If NULL, the type is not returned */
     
{
  http_retcode ret;
  
  char header[MAXBUF];
  char *pc;
  int  fd;
  int  n,length=-1;

  if (!pdata) return ERRNULL; else *pdata=NULL;
  if (plength) *plength=0;
  if (typebuf) *typebuf='\0';

  ret=http_query("GET",filename,"",KEEP_OPEN, NULL, 0, &fd);
  if (ret==200) {
    while (1) {
      n=http_read_line(fd,header,MAXBUF-1);
#ifdef VERBOSE
      fputs(header,stderr);
      putc('\n',stderr);
#endif	
      if (n<=0) {
	close(fd);
	return ERRRDHD;
      }
      /* empty line ? (=> end of header) */
      if ( n>0 && (*header)=='\0') break;
      /* try to parse some keywords : */
      /* convert to lower case 'till a : is found or end of string */
      for (pc=header; (*pc!=':' && *pc) ; pc++) *pc=tolower(*pc);
      sscanf(header,"content-length: %d",&length);
      if (typebuf) sscanf(header,"content-type: %s",typebuf);
    }
    if (length<=0) {
			length = 0;
			/*
			 * NOTE: The code was not originally designed to handle chunked mode. I have changed it a little
			 * so it can handle one chunk. It is not completely OK, but for our purposes it seems to be so.
			 * Michal Kara, lemming@ucw.cz 12/Oct/2000
			 */
      n=http_read_line(fd,header,MAXBUF-1); // Try to get chunked line. Only first chunk is retrieved.
      if (n<=0) {
				close(fd);
				return ERRRDHD;
			}
			pc = header;
			// Convert hexadecimal chunk length to a number
			while(*pc && (*pc != '\n') && (*pc != '\r')) {
				if (isdigit((int)*pc)) {
					length = (length<<4) + (*pc-'0');
				}
				else if (isalpha((int)*pc)) {
					length = (length<<4) + (toupper((int)*pc)-'A'+10);
				}
				else {
		      close(fd);
  		    return ERRNOLG;
				}
				pc++;
			}
			if (length <= 0) {
	      close(fd);
  	    return ERRNOLG;
			}
    }
    if (plength) *plength=length;
    if (!(*pdata=malloc(length+1))) {
      close(fd);
      return ERRMEM;
    }
    n=http_read_buffer(fd,*pdata,length);
    close(fd);
    if (n!=length) ret=ERRRDDT;
    if ((n >= 0) && (n <= length)) (*pdata)[n]=0;
  } else if (ret>=0) close(fd);
  return ret;
}


/*
 * Request the header
 *
 * This function outputs the header of thehttp data server.
 * The header is from the ressource named filename.
 * The length and type of data is eventually returned (like for http_get(3))
 *
 * returns a negative error code or a positive code from the server
 * 
 * limitations: filename is truncated to first 256 characters
 */
http_retcode http_head(filename, plength, typebuf) 
     char *filename; /* name of the ressource to read */
     int  *plength;/* address of integer variable which will be set to
		      length of the data */
     char *typebuf; /* allocated buffer where the data type is returned.
		    If NULL, the type is not returned */
{
/* mostly copied from http_get : */
  http_retcode ret;
  
  char header[MAXBUF];
  char *pc;
  int  fd;
  int  n,length=-1;

  if (plength) *plength=0;
  if (typebuf) *typebuf='\0';

  ret=http_query("HEAD",filename,"",KEEP_OPEN, NULL, 0, &fd);
  if (ret==200) {
    while (1) {
      n=http_read_line(fd,header,MAXBUF-1);
#ifdef VERBOSE
      fputs(header,stderr);
      putc('\n',stderr);
#endif	
      if (n<=0) {
	close(fd);
	return ERRRDHD;
      }
      /* empty line ? (=> end of header) */
      if ( n>0 && (*header)=='\0') break;
      /* try to parse some keywords : */
      /* convert to lower case 'till a : is found or end of string */
      for (pc=header; (*pc!=':' && *pc) ; pc++) *pc=tolower(*pc);
      sscanf(header,"content-length: %d",&length);
      if (typebuf) sscanf(header,"content-type: %s",typebuf);
    }
    if (plength) *plength=length;
    close(fd);
  } else if (ret>=0) close(fd);
  return ret;
}



/*
 * Delete data on the server
 *
 * This function request a DELETE on the http data server.
 *
 * returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters 
 */

http_retcode http_delete(filename) 
     char *filename;  /* name of the ressource to create */
{
  return http_query("DELETE",filename,"",CLOSE, NULL, 0, NULL);
}



/* parses an url : setting the http_server and http_port global variables
 * and returning the filename to pass to http_get/put/...
 * returns a negative error code or 0 if sucessfully parsed.
 */
http_retcode http_parse_url(url,pfilename)
    /* writeable copy of an url */
     char *url;  
    /* address of a pointer that will be filled with allocated filename
     * the pointer must be equal to NULL before calling or it will be 
     * automatically freed (free(3))
     */
     char **pfilename; 
{
  char *pc,c;
  
  http_port=80;
  if (http_server) {
    free(http_server);
    http_server=NULL;
  }
  if (*pfilename) {
    free(*pfilename);
    *pfilename=NULL;
  }
  
  if (strncasecmp("http://",url,7)) {
#ifdef VERBOSE
    fprintf(stderr,"invalid url (must start with 'http://')\n");
#endif
    return ERRURLH;
  }
  url+=7;
  for (pc=url,c=*pc; (c && c!=':' && c!='/');) c=*pc++;
  *(pc-1)=0;
  if (c==':') {
    if (sscanf(pc,"%d",&http_port)!=1) {
#ifdef VERBOSE
      fprintf(stderr,"invalid port in url\n");
#endif
      return ERRURLP;
    }
    for (pc++; (*pc && *pc!='/') ; pc++) ;
    if (*pc) pc++;
  }

  http_server=strdup(url);
  *pfilename= strdup ( c ? pc : "") ;

#ifdef VERBOSE
  fprintf(stderr,"host=(%s), port=%d, filename=(%s)\n",
	    http_server,http_port,*pfilename);
#endif
  return OK0;
}

