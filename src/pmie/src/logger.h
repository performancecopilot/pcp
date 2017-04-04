/*
 * Provided by Alan Hoyt <ahoyt@moser-inc.com> as part of the Solaris port,
 * this code came from
 * http://www.mit.edu/afs/sipb/service/logging/arch/sun4x_55/build/zephyr/clients/syslogd/syslogd.c-test1
 * 
 * Copyright (c) 1983, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define NOPRI 0x10 /* the "no priority" priority */
#define LOG_MAKEPRI(f,p) (((f) << 3) + (p))
#define LOG_MARK LOG_MAKEPRI(LOG_NFACILITIES, 0) /* mark "facility" */

#ifndef LOG_PRIMASK
#define LOG_PRIMASK 0x07
#endif
#ifndef LOG_FACMASK
#define LOG_FACMASK 0x03f8
#endif

typedef struct code {
  char	*c_name;
  int	c_val;
} CODE;

struct code prioritynames[] = {
  { "panic", LOG_EMERG },
  { "alert", LOG_ALERT },
  { "crit", LOG_CRIT },
  { "error", LOG_ERR },
  { "warning", LOG_WARNING },
  { "notice", LOG_NOTICE },
  { "info", LOG_INFO },
  { "debug", LOG_DEBUG },
  { "none", NOPRI },
  { "emerg", LOG_EMERG },
  { "err", LOG_ERR },
  { "warn", LOG_WARNING },
  { NULL, -1 }
};

struct code facilitynames[] = {
  { "daemon", LOG_DAEMON },
#ifndef IS_MINGW
  { "kern", LOG_KERN },
  { "user", LOG_USER },
  { "mail", LOG_MAIL },
  { "auth", LOG_AUTH },
  { "syslog", LOG_SYSLOG },
  { "lpr", LOG_LPR },
  { "news", LOG_NEWS },
  { "uucp", LOG_UUCP },
  { "cron", LOG_CRON },
  { "reserved", -1 },
  { "reserved", -1 },
  { "reserved", -1 },
  { "cron", LOG_CRON },
  { "local0", LOG_LOCAL0 },
  { "local1", LOG_LOCAL1 },
  { "local2", LOG_LOCAL2 },
  { "local3", LOG_LOCAL3 },
  { "local4", LOG_LOCAL4 },
  { "local5", LOG_LOCAL5 },
  { "local6", LOG_LOCAL6 },
  { "local7", LOG_LOCAL7 },
  { "security", LOG_AUTH },
  { "mark", LOG_MARK },
#endif
  { NULL, -1 }
};

