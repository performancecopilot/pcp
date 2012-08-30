/*
 * Lightweight telnet clone for shping and espping PMDAs.
 *
 * Usage: telnet-probe [-v] host port
 *
 * Once telnet connection is established:
 *	read stdin until EOF, writing to telnet connection
 *	read telnet until EOF, discarding data
 * -c (connect only) flag skips the send-receive processing
 *
 * Exit status is 1 in the case of any errors, else 0.
 */

#include "pmapi.h"
#include "impl.h"

int
main(int argc, char *argv[])
{
    struct hostent	*servInfo;
    char		*endnum;
    int			port = 0;
    int			s;
    int			nodelay = 1;
    struct linger	nolinger = {1, 0};
    struct sockaddr_in	myAddr;
    FILE		*fp;
    int			errflag = 0;
    int			cflag = 0;
    int			vflag = 0;
    int			sts = 1;
    int			c;

    while ((c = getopt(argc, argv, "cv?")) != EOF) {
        switch (c) {
	case 'c':
	    cflag = 1;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (optind+2 != argc) {
	fprintf(stderr, "%s: requires two arguments\n", argv[0]);
	errflag++;
    }
    else {
	port = (int)strtol(argv[optind+1], &endnum, 10);
	if (*endnum != '\0' || port < 0) {
	    fprintf(stderr, "%s: port must be a positive number\n", argv[0]);
	    errflag++;
	}
    }
    if (errflag) {
	fprintf(stderr, "Usage: %s [-c] [-v] host port\n", argv[0]);
	goto done;
    }

    if ((servInfo = gethostbyname(argv[optind])) == NULL) {
	if (vflag)
	    fprintf(stderr, "gethostbyname: %s\n", hoststrerror());
	goto done;
    }
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	if (vflag)
	    fprintf(stderr, "socket: %s\n", netstrerror());
	goto done;
    }
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &nodelay, (__pmSockLen)sizeof(nodelay));
    setsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &nolinger, (__pmSockLen)sizeof(nolinger));
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    myAddr.sin_port = htons(port);
    if (connect(s, (struct sockaddr*) &myAddr, sizeof(myAddr)) < 0) {
	if (vflag)
	    fprintf(stderr, "connect: %s\n", netstrerror());
	goto done;
    }

    if (cflag) {
	/* skip send-recv exercise */
	sts = 0;
	goto done;
    }

    fp = fdopen(s, "r+");
    if (vflag)
	fprintf(stderr, "send ...\n");
    while ((c = getc(stdin)) != EOF) {
	if (vflag) {
	    fputc(c, stderr);
	    fflush(stderr);
	}
	fputc(c, fp);
	if (ferror(fp)) {
	    if (vflag)
		fprintf(stderr, "telnet write: %s\n", osstrerror());
	    goto done;
	}
	fflush(fp);
    }

    if (vflag)
	fprintf(stderr, "recv ...\n");
    while ((c = getc(fp)) != EOF) {
	if (vflag) {
	    fputc(c, stderr);
	    fflush(stderr);
	}
    }
    if (ferror(fp)) {
	if (vflag)
	    fprintf(stderr, "telnet read: %s\n", osstrerror());
	goto done;
    }

    sts = 0;

done:
    if (vflag)
	fprintf(stderr, "exit: %d\n", sts);
    exit(sts);
}
