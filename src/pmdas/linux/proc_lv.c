/*
 * Linux LVM Devices Cluster
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
 */

#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "proc_lv.h"


#define	MAPDIR		"/dev/mapper"


int
refresh_proc_lv (proc_lv_t *lvs)
{
  DIR *dirp;
  struct dirent *dentry;
  struct stat statbuf;
  char path[64];

  dirp = opendir (MAPDIR);
  if (dirp == NULL)
    return 1;
  
  lvs->nlv = 0;
  lvs->lv = NULL;
  while ((dentry = readdir (dirp)))
    {
      char linkname [256];
      int linkname_len;
      snprintf (path, sizeof path, "%s/%s", MAPDIR, dentry->d_name);

      if (stat (path, &statbuf) == -1)
	continue;

      if (!S_ISBLK (statbuf.st_mode))
	continue;

      linkname_len = readlink(path, linkname, sizeof(linkname));

      int i = lvs->nlv;
      lvs->nlv += 1;
      
      lvs->lv = (lv_entry_t *)realloc(lvs->lv, lvs->nlv * sizeof(lv_entry_t));
      lvs->lv[i].id = i+1;

      lvs->lv[i].dev_name = malloc(strlen(dentry->d_name)+1);
      strcpy (lvs->lv[i].dev_name, dentry->d_name);

      lvs->lv[i].lv_name = malloc(linkname_len+1);
      strcpy (lvs->lv[i].lv_name, linkname);
    }
  
  closedir (dirp);

  if (lvs->lv_indom->it_numinst != lvs->nlv)
    {
      lvs->lv_indom->it_numinst = lvs->nlv;
      lvs->lv_indom->it_set = (pmdaInstid *)realloc(lvs->lv_indom->it_set,
						       lvs->nlv * sizeof(pmdaInstid));
      memset(lvs->lv_indom->it_set, 0, lvs->nlv * sizeof(pmdaInstid));
    }
  int dix;
  for (dix = 0; dix < lvs->nlv; dix++)
    {
      int skip_prefix = 0;
      lvs->lv_indom->it_set[dix].i_inst = lvs->lv[dix].id;
      if (strncmp (lvs->lv[dix].lv_name, "../", 3) == 0)
	skip_prefix = 3;
      lvs->lv_indom->it_set[dix].i_name = lvs->lv[dix].lv_name + skip_prefix;
    }
  return 0;
}
