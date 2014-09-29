#include "perfmon/pfmlib.h"
#include "perfmon/perf_event.h"
#include "mock_pfm.h"

#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define BASE_FAKE_FD 65000

int pfm_initialise_retval = 0;
int pfm_get_os_event_encoding_retvals[RETURN_VALUES_COUNT];
int n_get_os_event_encoding_calls = 0;
int perf_event_open_retvals[RETURN_VALUES_COUNT];
int n_perf_event_open_calls = 0;
int wrap_ioctl_retval = 0;
int wrap_malloc_fail = 0;
int wrap_sysconf_override = 0;
int wrap_sysconf_retcode = -1;

void init_mock()
{
    pfm_initialise_retval = 0;
    memset(pfm_get_os_event_encoding_retvals, 0, sizeof pfm_get_os_event_encoding_retvals);
    n_get_os_event_encoding_calls = 0;
    memset(perf_event_open_retvals, 0, sizeof perf_event_open_retvals);
    n_perf_event_open_calls = 0;
    wrap_ioctl_retval = 0;
    wrap_malloc_fail = 0;
    wrap_sysconf_override = 0;
    wrap_sysconf_retcode = -1;
}

/* Mock implementations of pfm library functions to allow unit testing */

void pfm_terminate(void)
{
}

pfm_err_t pfm_get_os_event_encoding(const char *str, int dfl_plm, pfm_os_t os, void *args)
{
    pfm_err_t ret = pfm_get_os_event_encoding_retvals[n_get_os_event_encoding_calls];
    n_get_os_event_encoding_calls = (n_get_os_event_encoding_calls + 1) % RETURN_VALUES_COUNT;
    return ret;
}

long int __wrap_syscall(long int sysno, ...)
{
    static long int fake_fd = BASE_FAKE_FD;
    if(__NR_perf_event_open == sysno)
    {
        int ret = perf_event_open_retvals[n_perf_event_open_calls];
        n_perf_event_open_calls = (n_perf_event_open_calls + 1) % RETURN_VALUES_COUNT;

        if(ret) {
            errno = EINTR;
            return -1;
        }
        else
            return fake_fd++;
    }
    else
    {
        errno = ENOTSUP;
        return -1;
    }
}

long int __real_sysconf (int name);

long int __wrap_sysconf (int name)
{
    if(wrap_sysconf_override) {
        if(wrap_sysconf_retcode == -1) {
            errno = EINTR;
        }
        return wrap_sysconf_retcode;
    }
    return __real_sysconf(name);
}

void *__real_malloc (size_t __size);

void *__wrap_malloc(size_t size)
{
    if(wrap_malloc_fail) {
        /* could be dicy if malloc continues to fail. Therefore reset it */
        wrap_malloc_fail = 0;
        return NULL;
    }
    return __real_malloc(size);
}

ssize_t __real_read(int fd, void *buf, size_t count);

ssize_t __wrap_read(int fd, void *buf, size_t count)
{
    if(fd >= BASE_FAKE_FD)
    {
        memset(buf, 0, count);
        return count;
    }

    return __real_read(fd, buf, count);
}

int __real_close(int fd);

int __wrap_close(int fd)
{
    if(fd >= BASE_FAKE_FD)
    {
        return 0;
    }
    return __real_close(fd);
}

int __wrap_ioctl (int fd, unsigned long int request, ...)
{
    if(fd >= BASE_FAKE_FD)
    {
        if(wrap_ioctl_retval) {
            errno = EINTR;
            return -1;
        }
        else
            return 0;
    }
    else
    {
        errno = ENOTSUP;
        return -1;
    }
}

const char *pfm_strerror(int code)
{
    return "sadness";
}

pfm_err_t pfm_get_pmu_info(pfm_pmu_t pmu, pfm_pmu_info_t *output)
{
    int retval = -1;
    if(0 == pmu)
    {
        output->is_present = 1;
        output->name = "pmuname";
        output->desc = "description";
        output->pmu = 0;
        output->nevents = 1;
        output->num_cntrs = 2;
        output->num_fixed_cntrs = 3;
        output->first_event = 0;
        retval = PFM_SUCCESS;
    }
    else if ( pmu < 20 )
    {
        output->is_present = 0;
        retval = PFM_SUCCESS;
    }
    return retval;
}

pfm_err_t pfm_get_event_attr_info(int eidx, int aidx, pfm_os_t os, pfm_event_attr_info_t *output)
{
    int retval = -1;

    if(eidx == 0) 
    {
        switch(aidx) 
        {
            case 0:
                output->name = "attr0";
                output->type = PFM_ATTR_UMASK;
                retval = PFM_SUCCESS;
                break;
            case 1:
                output->name = "attr1";
                output->type = PFM_ATTR_MOD_BOOL;
                retval = PFM_SUCCESS;
                break;
            case 2:
                output->name = "attr2";
                output->type = PFM_ATTR_UMASK;
                retval = PFM_SUCCESS;
                break;
        }
    }

    return retval;
}

pfm_err_t pfm_get_event_info(int idx, pfm_os_t os, pfm_event_info_t *output)
{
    if(idx < 2 && os == PFM_OS_PERF_EVENT_EXT) {
        output->pmu = idx;
        output->name = "event";
        output->nattrs = 4;
        return 0;
    }
    else
    {
        return -1;
    }
}

int pfm_get_event_next(int idx)
{
    if(idx > 4) {
        return -1;
    } else {
        return idx+1;
    }
}

pfm_err_t pfm_initialize(void)
{
    return pfm_initialise_retval;
}
