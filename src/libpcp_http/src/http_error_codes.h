/* http_error_codes.h - Error code definitions

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

#ifndef HTTP_ERROR_CODES_H
#define HTTP_ERROR_CODES_H

/* Error sources */
#define FETCHER_ERROR	0
#define ERRNO			1
#define H_ERRNO			2

/* HTTP Fetcher error codes */
#define HF_SUCCESS		0
#define HF_METAERROR	1
#define HF_NULLURL		2
#define HF_HEADTIMEOUT	3
#define HF_DATATIMEOUT	4
#define HF_FRETURNCODE	5
#define HF_CRETURNCODE	6
#define HF_STATUSCODE	7
#define HF_CONTENTLEN	8
#define HF_HERROR		9
#define HF_CANTREDIRECT 10
#define HF_MAXREDIRECTS 11

#endif
