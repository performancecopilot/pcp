/* http_error_codes.c - Error code declarations

	HTTP Fetcher 
 	Copyright (C) 2001, 2003, 2004 Lyle Hanson (lhanson@users.sourceforge.net)

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	See LICENSE file for details
 */


	/* Note that '%d' cannot be escaped at this time */
const char *http_errlist[] =
	{
	"Success",										/* HF_SUCCESS		*/
	"Internal Error. What the hell?!",				/* HF_METAERROR		*/
	"Got NULL url",									/* HF_NULLURL		*/
	"Timed out, no metadata for %d seconds",		/* HF_HEADTIMEOUT 	*/
	"Timed out, no data for %d seconds",			/* HF_DATATIMEOUT	*/
	"Couldn't find return code in HTTP response",	/* HF_FRETURNCODE	*/
	"Couldn't convert return code in HTTP response",/* HF_CRETURNCODE	*/
	"Request returned a status code of %d",			/* HF_STATUSCODE	*/
	"Couldn't convert Content-Length to integer",	/* HF_CONTENTLEN	*/
	"Network error (description unavailable)",		/* HF_HERROR		*/
	"Status code of %d but no Location: field",		/* HF_CANTREDIRECT  */
	"Followed the maximum number of redirects (%d)" /* HF_MAXREDIRECTS  */
	};

	/* Used to copy in messages from http_errlist[] and replace %d's with
	 *	the value of errorInt.  Then we can pass the pointer to THIS */
char convertedError[128];
