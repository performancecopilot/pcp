/*
 * Copyright (c) 2018 Fujitsu.
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

#define DEBUG_SIZE 34
#define LENGTH 1024

typedef struct {
    long long debug[DEBUG_SIZE];
} kvmstat_t;

extern int refresh_kvm(kvmstat_t *kvm);
