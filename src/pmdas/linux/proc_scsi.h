/*
 * Linux /proc/scsi/scsi metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

typedef struct {
    int			id;	     /* internal instance id */
    char		*namebuf;    /* external name, i.e. host:channel:id:lun */
    int			dev_host;
    int			dev_channel;
    int			dev_id;
    int			dev_lun;
    char		*dev_type;
    char		*dev_name;
} scsi_entry_t;

typedef struct {
    int           	nscsi;
    scsi_entry_t 	*scsi;
    pmdaIndom   	*scsi_indom;
} proc_scsi_t;

extern int refresh_proc_scsi(proc_scsi_t *);
