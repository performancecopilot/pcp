/*
 * Copyright (c) 2015 Red Hat.
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

/*
 * Container engine implementation for LXC
 */

extern void lxc_setup(container_engine_t *);
extern int lxc_indom_changed(container_engine_t *);
extern void lxc_insts_refresh(container_engine_t *, pmInDom);
extern int lxc_value_refresh(container_engine_t *, const char *,
		container_t *);
extern int lxc_name_matching(container_engine_t *, const char *,
		const char *, const char *);
