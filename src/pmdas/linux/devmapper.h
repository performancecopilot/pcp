/*
 * Linux /dev/mapper metrics cluster
 *
 * Copyright (c) 2013 Red Hat.
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

typedef struct {
    int			id;	     /* internal instance id */
    char		*dev_name;
    char		*lv_name;
} lv_entry_t;

typedef struct {
    int           	nlv;
    lv_entry_t 		*lv;
    pmdaIndom   	*lv_indom;
} dev_mapper_t;

extern int refresh_dev_mapper(dev_mapper_t *);
