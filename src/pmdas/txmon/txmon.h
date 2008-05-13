/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

/*
 * the txmon shm segment
 *
 * control comes at the beginning, with index[] expanded to match the
 * number of transaction types (control->n_tx)
 *
 * then follows one stat_t per transaction type, subject to the
 * constraint that each stat_t is hardware cache aligned to avoid
 * anti-social bus traffic
 */

typedef struct {
    int		level;		/* controls stats collection levels */
    int		n_tx;		/* # of tx types */
    int		index[1];	/* will be expanded when allocated */
} control_t;

static control_t	*control;

#define MAXNAMESIZE 20

typedef struct {

    /* managed by txmon PMDA ... do not fiddle with these! */
    char		type[MAXNAMESIZE];	/* tx type name */
    unsigned int	reset_count;	/* tx count @ last reset */

    /* initialized and then read by txmon PMDA, updated by txrecord */
    unsigned int	count;		/* tx count since epoch */
    float		max_time;	/* maximum elapsed time */
    double		sum_time;	/* aggregate elapsed time */
} stat_t;

/* arbitrary shm key */
#define KEY (key_t)0xdeadbeef
