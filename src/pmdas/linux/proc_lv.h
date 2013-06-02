/*
 * Linux /proc/lv/lv metrics cluster
 *
 * Copyright (c) 2013 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

typedef struct {
    int			id;	     /* internal instance id */
    char		*dev_name;
    char		*lv_name;
} lv_entry_t;

typedef struct {
    int           	nlv;
    lv_entry_t 		*lv;
    pmdaIndom   	*lv_indom;
} proc_lv_t;

extern int refresh_proc_lv(proc_lv_t *);
