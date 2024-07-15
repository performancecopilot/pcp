//
// Test whether Qt GUI Apps can expect to function on this host.
//

#include <unistd.h>
#include <signal.h>
#include <QApplication>

void
on_sigabrt(int signum)
{
    signal(signum, SIG_DFL);
    _exit(1);
}

int
main(int argc, char* argv[])
{
    signal(SIGABRT, on_sigabrt);
    QApplication a(argc, argv);
    _exit(0);
}
