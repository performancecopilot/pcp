/*
 * Linux acct metrics cluster
 *
 * Copyright (c) 2020 Fujitsu.
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

#include "acct.h"

void acct_init(proc_acct_t *proc_acct) {
}

void refresh_acct(proc_acct_t *proc_acct) {
}

int acct_fetchCallBack(int i_inst, int item, proc_acct_t* proc_acct, pmAtomValue *atom) {
	return 0;
}
