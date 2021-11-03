#ifndef	__GPUCOM__
#define	__GPUCOM__

struct gpupidstat {
	long		pid;
	struct gpu	gpu;
};

void	setup_gpuphotoproc(void);

unsigned long	gpuphotoproc(struct gpupidstat **, unsigned int *);

void	gpumergeproc(struct tstat      *, int,
                     struct tstat      *, int,
                     struct gpupidstat *, int);
#endif
