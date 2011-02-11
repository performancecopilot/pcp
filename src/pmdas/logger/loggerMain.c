#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logger.h"

void
loggerMain(pmdaInterface *dispatch)
{
    int pmcdfd = __pmdaInFd(dispatch);
    int maxfd;
    fd_set fds;
    fd_set readyfds;
    int nready;
    int monitorfd;

    /* Try to open logfile to monitor */
    monitorfd = open(monitor_path, O_RDONLY);
    if (monitorfd < 0) {
	__pmNotifyErr(LOG_ERR, "open failure on %s", monitor_path);
	exit(1);
    }

    FD_ZERO(&fds);
    FD_SET(monitorfd, &fds);
    FD_SET(pmcdfd, &fds);
    maxfd = (monitorfd > pmcdfd) ? monitorfd : pmcdfd;

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);

	if (nready == 0)
	    continue;
	else if (nready < 0) {
	    if (errno != EINTR) {
		__pmNotifyErr(LOG_ERR, "select failure");
		exit(1);
	    }
	    continue;
	}

	if (FD_ISSET(pmcdfd, &readyfds)) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		__pmNotifyErr(LOG_DEBUG, "processing pmcd request [fd=%d]", pmcdfd);
#endif
	    if (__pmdaMainPDU(dispatch) < 0) {
		exit(1);	/* fatal if we lose pmcd */
	    }
	}
	if (FD_ISSET(monitorfd, &readyfds)) {
	    /* do something with logfile input */
	}
    }
}
