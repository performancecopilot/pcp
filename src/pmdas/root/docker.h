/*
 * Copyright (c) 2014-2015 Red Hat.
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
 * Container engine implementation for Docker
 */

extern void docker_setup(container_engine_t *);
extern int docker_indom_changed(container_engine_t *);
extern void docker_insts_refresh(container_engine_t *, pmInDom);
extern int docker_value_refresh(container_engine_t *, const char *,
				container_t *);
extern int docker_name_matching(container_engine_t *, const char *,
		const char *, const char *);
