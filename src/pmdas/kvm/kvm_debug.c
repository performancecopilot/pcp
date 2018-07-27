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
#include <dirent.h>
#include "kvm_debug.h"
#include "kvmstat.h"

int
refresh_kvm(kvmstat_t *kvm)
{
    int sts = 0;
    DIR *kvm_dir;
    struct dirent* de;
    FILE    *fp;
    char buffer[BUFSIZ];
    char path[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "/sys/kernel/debug/kvm");
    path[sizeof(path)-1] = '\0';
    if ((kvm_dir = opendir(path)) == NULL)
	return -oserror();

    while ((de = readdir(kvm_dir)) != NULL) {   
	if (!strncmp(de->d_name, ".", 1))
	    continue;

	pmsprintf(path, sizeof(path), "/sys/kernel/debug/kvm/%s", de->d_name);
	path[sizeof(path)-1] = '\0';
	if ((fp = fopen(path, "r")) == NULL) {
	    sts = -oserror();
	    break;
	}


        while (fgets(buffer, BUFSIZ, fp) != NULL)
        {
            if (!strncmp(de->d_name, "efer_reload", 11)) {
               kvm->debug[0] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "exits", 5)) {
               kvm->debug[1] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "fpu_reload", 10)) {
               kvm->debug[2] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "halt_attempted_poll", 19)) {
               kvm->debug[3] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "halt_exits", 10)) {
               kvm->debug[4] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "halt_successful_poll", 20)) {
               kvm->debug[5] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "halt_wakeup", 11)) {
               kvm->debug[6] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "host_state_reload", 17)) {
               kvm->debug[7] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "hypercalls", 10)) {
               kvm->debug[8] = atoll(buffer);
               break;
            }
            else if (!strncmp(de->d_name, "insn_emulation", 14)) {
               kvm->debug[9] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "insn_emulation_fail", 19)) {
               kvm->debug[10] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "invlpg", 6)) {
               kvm->debug[11] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "io_exits", 8)) {
               kvm->debug[12] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "irq_exits", 9)) {
               kvm->debug[13] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "irq_injections", 14)) {
               kvm->debug[14] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "irq_window", 10)) {
               kvm->debug[15] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "largepages", 10)) {
               kvm->debug[16] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmio_exits", 10)) {
               kvm->debug[17] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_cache_miss", 14)) {
               kvm->debug[18] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_flooded", 11)) {
               kvm->debug[19] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_pde_zapped", 14)) {
               kvm->debug[20] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_pte_updated", 15)) {
               kvm->debug[21] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_pte_write", 13)) {
               kvm->debug[22] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_recycled", 12)) {
               kvm->debug[23] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_shadow_zapped", 17)) {
               kvm->debug[24] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "mmu_unsync", 10)) {
               kvm->debug[25] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "nmi_injections", 14)) {
               kvm->debug[26] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "nmi_window", 10)) {
               kvm->debug[27] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "pf_fixed", 8)) {
               kvm->debug[28] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "pf_guest", 8)) {
               kvm->debug[29] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "remote_tlb_flush", 16)) {
               kvm->debug[30] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "request_irq", 11)) {
               kvm->debug[31] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "signal_exits", 12)) {
               kvm->debug[32] = atoll(buffer);
               break;
            }

            else if (!strncmp(de->d_name, "tlb_flush", 9)) {
               kvm->debug[33] = atoll(buffer);
               break;
            }
        }
        fclose(fp);
    } 
    closedir(kvm_dir);
    return sts;
}
