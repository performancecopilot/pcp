/*
 * Unconditionally kill your parent ... BEWARE!
 */
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

int
main()
{
    kill(getppid(), SIGKILL);
    return(0);
}
