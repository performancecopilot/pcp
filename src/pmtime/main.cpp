/*
 * Copyright (c) 2014,2016, Red Hat.
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
#include <QApplication>
#include <pcp/pmapi.h>
#include "timelord.h"
#include "pmtime.h"

static int Dflag;

static pmOptions opts;
static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_GUIPORT,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static void setupEnvironment(void)
{
    char *value;
    QString confirm = pmGetConfig("PCP_BIN_DIR");
    confirm.prepend("PCP_XCONFIRM_PROG=");
    confirm.append(QChar(pmPathSeparator()));
    confirm.append("pmquery");
    if ((value = strdup((const char *)confirm.toLatin1())) != NULL)
	putenv(value);
    if (getenv("PCP_STDERR") == NULL &&	// do not overwrite, for QA
	((value = strdup("PCP_STDERR=DISPLAY")) != NULL))
	putenv(value);

    QCoreApplication::setOrganizationName("PCP");
    QCoreApplication::setApplicationName("pmtime");
}

static int
override(int opt, pmOptions *opts)
{
    (void)opts;
    if (opt == 'D')
	Dflag = 1;
    return 0;
}

int main(int argc, char **argv)
{
    int		sts, autoport = 0;

    QApplication a(argc, argv);
    setupEnvironment();

    /* -a/-h ignored, back-compat for time control from libpcp_gui */
    opts.short_options = "ahD:p:V?";
    opts.long_options = longopts;
    opts.override = override;
    (void)pmGetOptions(argc, argv, &opts);
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT) || opts.optind != argc) {
	if ((opts.flags & PM_OPTFLAG_EXIT)) {
	    unsetenv("PCP_STDERR");
	    sts = 0;
	} else {
	    sts = 1;
	}
	pmUsageMessage(&opts);
	exit(sts);
    }

    if (!opts.guiport) {
	char	*endnum, *envstr;

	autoport = 1;
	if ((envstr = getenv("PMTIME_PORT")) == NULL) {
	    opts.guiport = PmTime::BasePort;
	} else {
	    opts.guiport = strtol(envstr, &endnum, 0);
	    if (*endnum != '\0' || opts.guiport < 0) {
		pmprintf(
		    "%s: PMTIME_PORT must be a numeric port number (not %s)\n",
			pmGetProgname(), envstr);
		pmflush();
		exit(1);
	    }
	}
    }

    console = new Console;
    TimeLord tl(&a);
    do {
	if (tl.listen(QHostAddress::LocalHost, opts.guiport))
	    break;
	opts.guiport++;
    } while (autoport && (opts.guiport >= 0));

    if (!opts.guiport || tl.isListening() == false) {
	if (!autoport)
	    pmprintf("%s: cannot find an available port\n", pmGetProgname());
	else
	    pmprintf("%s: cannot connect to requested port (%d)\n",
		    pmGetProgname(), opts.guiport);
	pmflush();
	exit(1);
    } else if (autoport) {	/* write to stdout for client */
	char	name[32];

	sts = pmsprintf(name, sizeof(name), "port=%u\n", opts.guiport);
	if (write(fileno(stdout), name, sts + 1) < 0) {
	    if (errno != EPIPE) {
		pmprintf("%s: cannot write port for client: %s\n",
		    pmGetProgname(), strerror(errno));
		pmflush();
	    }
	    exit(1);
	}
    }

    PmTimeLive hc;
    PmTimeArch ac;
    tl.setContext(&hc, &ac);

    hc.init();
    if (!Dflag) hc.disableConsole();
    else hc.popup(1);

    ac.init();
    if (!Dflag) ac.disableConsole();
    else ac.popup(1);

    a.exec();
    return 0;
}
