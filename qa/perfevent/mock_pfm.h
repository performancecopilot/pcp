#ifndef MOCK_PFM_H_
#define MOCK_PFM_H_

#define RETURN_VALUES_COUNT 1024

void init_mock();

extern int pfm_initialise_retval;
extern int pfm_get_os_event_encoding_retvals[RETURN_VALUES_COUNT];
extern int perf_event_open_retvals[RETURN_VALUES_COUNT];
extern int wrap_ioctl_retval;
extern int wrap_malloc_fail;
extern int wrap_sysconf_override;
extern int wrap_sysconf_retcode;

#endif /* MOCK_PFM_H_ */
