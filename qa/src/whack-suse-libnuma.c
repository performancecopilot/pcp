/*
 * in the QA Farm, openSUSE Tumbleweed VMs are exposed to an
 * interaction between libnuma and the kernel that emits this
 * annoying message for heaps of QA tests:
 * get_mempolicy: Function not implemented
 *
 * The "fix" here, as per Claude, is to build a DSO and override
 * the libnuma version via LD_PRELOAD
 *
 * Recipe ...
 * $ make whack-suse-libnuma.so
 * $ sudo cp whack-suse-libnuma.so /usr/local/lib
 * $ vi to create /etc/ld.so.preload with these contents
 * /usr/local/lib/whack-suse-libnuma.so
 *
 */

#include <errno.h>
#include <string.h>

long get_mempolicy(int *mode, unsigned long *nodemask, unsigned long maxnode, void *addr, unsigned long flags)
{
    if (mode)
	*mode = 0;	/* MPOL_DEFAULT */

    if (nodemask && maxnode > 0) {
        unsigned long nwords = (maxnode + (sizeof(unsigned long) * 8 - 1))
                                / (sizeof(unsigned long) * 8);
        memset(nodemask, 0, nwords * sizeof(unsigned long));
        nodemask[0] = 1; /* node 0 present */
    }

    return 0; /* success */
}
