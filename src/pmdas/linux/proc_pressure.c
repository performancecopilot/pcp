/*
 * Linux /proc/pressure/ metrics clusters
 *
 * Copyright (c) 2019 Red Hat.
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
#include "linux.h"
#include "proc_pressure.h"

#define SOME	"some"
#define FULL	"full"

/*
 * Read one line from a /proc/pressure file to fill a pressure_t struct;
 * format of each line is:
 *     TYPE avg10=NN.NN avg60=NN.NN avg300=NN.NN total=NN
 */
static int
read_pressure(FILE *fp, const char *type, pressure_t *pp)
{
    static char	fmt[] = "TYPE avg10=%f avg60=%f avg300=%f total=%llu\n";

    strncpy(fmt, type, 4);
    return fscanf(fp, fmt, &pp->avg[0], &pp->avg[1], &pp->avg[2],
		(unsigned long long *)&pp->total) == 4;
}

int
refresh_proc_pressure_cpu(proc_pressure_t *proc_pressure)
{
    char	buf[MAXPATHLEN];
    FILE	*fp;
    int		sts;

    memset(&proc_pressure->some_cpu, 0, sizeof(proc_pressure->some_cpu));

    if (!(fp = linux_statsfile("/proc/pressure/cpu", buf, sizeof(buf))))
	return -oserror();

    sts = read_pressure(fp, SOME, &proc_pressure->some_cpu);
    proc_pressure->some_cpu.updated = sts;

    fclose(fp);
    return 0;
}

int
refresh_proc_pressure_mem(proc_pressure_t *proc_pressure)
{
    char	buf[MAXPATHLEN];
    FILE	*fp;
    int		sts;

    memset(&proc_pressure->some_mem, 0, sizeof(proc_pressure->some_mem));
    memset(&proc_pressure->full_mem, 0, sizeof(proc_pressure->full_mem));

    if (!(fp = linux_statsfile("/proc/pressure/memory", buf, sizeof(buf))))
	return -oserror();

    sts = read_pressure(fp, SOME, &proc_pressure->some_mem);
    proc_pressure->some_mem.updated = sts;
    sts = read_pressure(fp, FULL, &proc_pressure->full_mem);
    proc_pressure->full_mem.updated = sts;

    fclose(fp);
    return 0;
}

int
refresh_proc_pressure_io(proc_pressure_t *proc_pressure)
{
    char	buf[MAXPATHLEN];
    FILE	*fp;
    int		sts;

    memset(&proc_pressure->some_io, 0, sizeof(proc_pressure->some_io));
    memset(&proc_pressure->full_io, 0, sizeof(proc_pressure->some_io));

    if (!(fp = linux_statsfile("/proc/pressure/io", buf, sizeof(buf))))
	return -oserror();

    sts = read_pressure(fp, SOME, &proc_pressure->some_io);
    proc_pressure->some_io.updated = sts;
    sts = read_pressure(fp, FULL, &proc_pressure->full_io);
    proc_pressure->full_io.updated = sts;

    fclose(fp);
    return 0;
}

int
average_proc_pressure(pressure_t *pp, unsigned int inst, pmAtomValue *atom)
{
    if (inst == 10)
    	atom->f = pp->avg[0];
    else if (inst == 60)
    	atom->f = pp->avg[1];
    else if (inst == 300)
    	atom->f = pp->avg[2];
    else
    	return PM_ERR_INST;
    return 0;
}
