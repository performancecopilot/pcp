#include "jsmn.h"
#include <string.h>
#include <stdlib.h>
/*
 * JSMN helper interfaces for efficiently extracting JSON configs
 */

int
jsmneq(const char *js, jsmntok_t *tok, const char *s)
{
    if (tok->type != JSMN_STRING)
	return -1;
    if (strlen(s) == tok->end - tok->start &&
	strncasecmp(js + tok->start, s, tok->end - tok->start) == 0)
	return 0;
    return -1;
}

int
jsmnflag(const char *js, jsmntok_t *tok, int *bits, int flag)
{
    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    if (strncmp(js + tok->start, "true", sizeof("true")-1) == 0)
	*bits |= flag;
    else
	*bits &= ~flag;
    return 0;
}

int
jsmnint(const char *js, jsmntok_t *tok, int *value)
{
    char	buffer[64];

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    strncpy(buffer, js + tok->start, tok->end - tok->start);
    buffer[tok->end - tok->start] = '\0';
    *value = (int)strtol(buffer, NULL, 0);
    return 0;
}

int
jsmnstrdup(const char *js, jsmntok_t *tok, char **name)
{
    char	*s = *name;

    if (tok->type != JSMN_STRING)
	return -1;
    if (s)
	free(s);
    s = strndup(js + tok->start, tok->end - tok->start);
    return ((*name = s) == NULL) ? -1 : 0;
}
