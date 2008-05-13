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
 *  Http put/get mini lib
 */

// Timeout for connect

#define HTTPLIB_TIMEOUT 3

 /* declarations */


extern char *http_server;

extern int http_port;

extern char *http_proxy_server;

extern int http_proxy_port;


/* return type */
typedef enum {

  /* Client side errors */
  ERRHOST=-1, /* No such host */
  ERRSOCK=-2, /* Can't create socket */
  ERRCONN=-3, /* Can't connect to host */
  ERRWRHD=-4, /* Write error on socket while writing header */
  ERRWRDT=-5, /* Write error on socket while writing data */
  ERRRDHD=-6, /* Read error on socket while reading result */
  ERRPAHD=-7, /* Invalid answer from data server */
  ERRNULL=-8, /* Null data pointer */
  ERRNOLG=-9, /* No/Bad length in header */
  ERRMEM=-10, /* Can't allocate memory */
  ERRRDDT=-11,/* Read error while reading data */
  ERRURLH=-12,/* Invalid url - must start with 'http://' */
  ERRURLP=-13,/* Invalid port in url */
	ERRTIME=-14,/* Timeout */  


  /* Return code by the server */
  ERR400=400, /* Invalid query */
  ERR403=403, /* Forbidden */
  ERR408=408, /* Request timeout */
  ERR500=500, /* Server error */
  ERR501=501, /* Not implemented */
  ERR503=503, /* Service overloaded */

  /* Succesful results */
  OK0 = 0,   /* successfull parse */
  OK201=201, /* Ressource succesfully created */
  OK200=200  /* Ressource succesfully read */

} http_retcode;


/* prototypes */

#ifndef OSK
http_retcode http_put(char *filename, char *data, int length, 
	     int overwrite, char *type) ;
http_retcode http_get(char *filename, char **pdata,int *plength, char *typebuf);

http_retcode http_parse_url(char *url, char **pfilename);

http_retcode http_delete(char *filename) ;

http_retcode http_head(char *filename, int *plength, char *typebuf);

#endif
