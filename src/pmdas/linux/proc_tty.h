/*
 * Copyright (c) 2017-2018 Red Hat Inc.
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

#ifndef _PROC_TTY_H
#define _PROC_TTY_H

enum {
/* Serial TTY metrics  */
    TTY_TX = 0,
    TTY_RX,
    TTY_FRAME,
    TTY_PARITY,
    TTY_BRK,
    TTY_OVERRUN,
    TTY_IRQ
};

/* /proc/tty/driver/serial metrics */
typedef struct {
    unsigned int tx;
    unsigned int rx;
    unsigned int frame;
    unsigned int parity;
    unsigned int brk;
    unsigned int overrun;
    unsigned int irq;
} ttydev_t;

extern int refresh_tty(pmInDom);

#endif
