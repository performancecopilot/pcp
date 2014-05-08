/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#include <QtGui/QApplication>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "timelord.h"
#include "pmtime.h"

static void setupEnvironment(void)
{
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.prepend("PCP_XCONFIRM_PROG=");
    confirm.append(QChar(__pmPathSeparator()));
    confirm.append("pmquery");
    putenv(strdup((const char *)confirm.toAscii()));
    if (getenv("PCP_STDERR") == NULL)	// do not overwrite, for QA
	putenv(strdup("PCP_STDERR=DISPLAY"));

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName("pmtime");
}

int main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			errorFlag = 0;
    int			port = -1, autoport = 0;
    char		*endnum, *envstr, portname[32];
    static char		usage[] = "Usage: %s [-V] [-a | -h] [-p port]\n";

    QApplication a(argc, argv);
    __pmSetProgname(argv[0]);
    setupEnvironment();

    while ((c = getopt(argc, argv, "ahp:D:V?")) != EOF) {
	switch (c) {

	case 'a':
	case 'h':
	    break;

	case 'p':
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || port < 0) {
		pmprintf("%s: requires a numeric port (not %s)\n",
			pmProgname, optarg);
		errorFlag++;
	    }
	    break;

	case 'D':
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		errorFlag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'V':		/* version */
	    printf("%s %s\n", pmProgname, pmGetConfig("PCP_VERSION"));
	    exit(0);

	case '?':
	    errorFlag++;
	    break;
	}
    }

    if (errorFlag || optind != argc) {
	pmprintf(usage, pmProgname);
	pmflush();
	exit(1);
    }

    if (port == -1) {
	autoport = 1;
	if ((envstr = getenv("KMTIME_PORT")) == NULL) {
	    port = PmTime::BasePort;
	} else {
	    port = strtol(envstr, &endnum, 0);
	    if (*endnum != '\0' || port < 0) {
		pmprintf(
		    "%s: KMTIME_PORT must be a numeric port number (not %s)\n",
			pmProgname, envstr);
		pmflush();
		exit(1);
	    }
	}
    }

    console = new Console;
    TimeLord tl(&a);
    do {
	if (tl.listen(QHostAddress::LocalHost, port))
	    break;
	port++;
    } while (autoport && (port >= 0));

    if (!port || tl.isListening() == false) {
	if (!autoport)
	    pmprintf("%s: cannot find an available port\n", pmProgname);
	else
	    pmprintf("%s: cannot connect to requested port (%d)\n",
		    pmProgname, port);
	pmflush();
	exit(1);
    } else if (autoport) {	/* write to stdout for client */
	c = snprintf(portname, sizeof(portname), "port=%u\n", port);
	if (write(fileno(stdout), portname, c + 1) < 0) {
	    if (errno != EPIPE) {
		pmprintf("%s: cannot write port for client: %s\n",
		    pmProgname, strerror(errno));
		pmflush();
	    }
	    exit(1);
	}
    }

    PmTimeLive hc;
    PmTimeArch ac;
    tl.setContext(&hc, &ac);

    hc.init();
    if (!pmDebug) hc.disableConsole();
    else hc.popup(1);

    ac.init();
    if (!pmDebug) ac.disableConsole();
    else ac.popup(1);

    a.exec();
    return 0;
}
