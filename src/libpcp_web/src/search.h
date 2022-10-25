/*
 * Copyright (c) 2020,2022 Red Hat.
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
 */
#ifndef SEARCH_SCHEMA_H
#define SEARCH_SCHEMA_H

#include <pmapi.h>
#include <mmv_stats.h>
#include "redis.h"
#include "private.h"
#include "schema.h"
#include "slots.h"

#define FT_TEXT_KEY	"pcp:text"
#define FT_TEXT_KEY_LEN	(sizeof(FT_TEXT_KEY)-1)

#define FT_ADD		"FT.ADD"
#define FT_ADD_LEN	(sizeof(FT_ADD)-1)
#define FT_CREATE	"FT.CREATE"
#define FT_CREATE_LEN	(sizeof(FT_CREATE)-1)
#define FT_SEARCH	"FT.SEARCH"
#define FT_SEARCH_LEN	(sizeof(FT_SEARCH)-1)
#define FT_INFIELDS	"INFIELDS"
#define FT_INFIELDS_LEN	(sizeof(FT_INFIELDS)-1)
#define FT_INFO		"FT.INFO"
#define FT_INFO_LEN	(sizeof(FT_INFO)-1)
#define FT_ASC		"ASC"
#define FT_ASC_LEN	(sizeof(FT_ASC)-1)
#define FT_FIELDS	"FIELDS"
#define FT_FIELDS_LEN	(sizeof(FT_FIELDS)-1)
#define FT_HELPTEXT	"HELPTEXT"
#define FT_HELPTEXT_LEN	(sizeof(FT_HELPTEXT)-1)
#define FT_HIGHLIGHT	"HIGHLIGHT"
#define FT_HIGHLIGHT_LEN (sizeof(FT_HIGHLIGHT)-1)
#define FT_INDOM	"INDOM"
#define FT_INDOM_LEN	(sizeof(FT_INDOM)-1)
#define FT_LIMIT	"LIMIT"
#define FT_LIMIT_LEN	(sizeof(FT_LIMIT)-1)
#define FT_NAME		"NAME"
#define FT_NAME_LEN	(sizeof(FT_NAME)-1)
#define FT_ONELINE	"ONELINE"
#define FT_ONELINE_LEN	(sizeof(FT_ONELINE)-1)
#define FT_PARTIAL	"PARTIAL"
#define FT_PARTIAL_LEN	(sizeof(FT_PARTIAL)-1)
#define FT_PAYLOAD	"PAYLOAD"
#define FT_PAYLOAD_LEN	(sizeof(FT_PAYLOAD)-1)
#define FT_RETURN	"RETURN"
#define FT_RETURN_LEN	(sizeof(FT_RETURN)-1)
#define FT_REPLACE	"REPLACE"
#define FT_REPLACE_LEN	(sizeof(FT_REPLACE)-1)
#define FT_SCHEMA	"SCHEMA"
#define FT_SCHEMA_LEN	(sizeof(FT_SCHEMA)-1)
#define FT_SCORE	"SCORE"
#define FT_SCORE_LEN	(sizeof(FT_SCORE)-1)
#define FT_SCORER	"SCORER"
#define FT_SCORER_LEN	(sizeof(FT_SCORER)-1)
#define FT_SCORER_BM25	"BM25"
#define FT_SCORER_BM25_LEN (sizeof(FT_SCORER_BM25)-1)
#define FT_SORTABLE	"SORTABLE"
#define FT_SORTABLE_LEN	(sizeof(FT_SORTABLE)-1)
#define FT_SORTBY	"SORTBY"
#define FT_SORTBY_LEN	(sizeof(FT_SORTBY)-1)
#define FT_TAG		"TAG"
#define FT_TAG_LEN	(sizeof(FT_TAG)-1)
#define FT_TEXT		"TEXT"
#define FT_TEXT_LEN	(sizeof(FT_TEXT)-1)
#define FT_TYPE		"TYPE"
#define FT_TYPE_LEN	(sizeof(FT_TYPE)-1)
#define FT_WEIGHT	"WEIGHT"
#define FT_WEIGHT_LEN	(sizeof(FT_WEIGHT)-1)
#define FT_WITHPAYLOADS	"WITHPAYLOADS"
#define FT_WITHPAYLOADS_LEN (sizeof(FT_WITHPAYLOADS)-1)
#define FT_WITHSCORES	"WITHSCORES"
#define FT_WITHSCORES_LEN  (sizeof(FT_WITHSCORES)-1)

extern void redisSearchInit(struct dict *);
extern void redisSearchClose(void);
extern void redis_load_search_schema(void *);
extern void redis_search_text_add(redisSlots *, pmSearchTextType,
		const char *, const char *, const char *, const char *, void *);

/*
 * Asynchronous search baton structures
 */
typedef struct redisSearchBaton {
    seriesBatonMagic	header;		/* MAGIC_SEARCH */

    redisSlots		*slots;		/* Redis server slots */
    pmSearchFlags	flags;
    int			error;
    void		*module;
    pmSearchCallBacks	*callbacks;
    pmLogInfoCallBack	info;
    struct timespec	started;
    void		*userdata;
    void		*arg;
} redisSearchBaton;

#endif	/* SEARCH_SCHEMA_H */
