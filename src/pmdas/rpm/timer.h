/*
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
#ifndef TIMER_H
#define TIMER_H

extern void start_timing(void);
extern void stop_timing(void);

extern double get_user_timer(void);
extern double get_kernel_timer(void);
extern double get_elapsed_timer(void);

#endif /* TIMER_H */
