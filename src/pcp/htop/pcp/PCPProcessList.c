/*
htop - PCPProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "PCPProcessList.h"

#include <math.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "PCPProcess.h"
#include "Process.h"
#include "Settings.h"
#include "XUtils.h"


static int PCPProcessList_computeCPUcount(void) {
   int cpus;
   pmAtomValue value;
   if (Metric_values(PCP_HINV_NCPU, &value, 1, PM_TYPE_32) != NULL)
      cpus = value.l;
   else
      cpus = Metric_instanceCount(PCP_PERCPU_SYSTEM);
   return cpus > 1 ? cpus : 1;
}

static void PCPProcessList_updateCPUcount(PCPProcessList* this) {
   ProcessList* pl = &(this->super);
   int cpus = PCPProcessList_computeCPUcount();
   if (cpus == pl->cpuCount)
      return;

   pl->cpuCount = cpus;
   free(this->percpu);
   free(this->values);

   this->percpu = xCalloc(cpus, sizeof(pmAtomValue *));
   for (int i = 0; i < cpus; i++)
      this->percpu[i] = xCalloc(CPU_METRIC_COUNT, sizeof(pmAtomValue));
   this->values = xCalloc(cpus, sizeof(pmAtomValue));
}

static char* setUser(UsersTable* this, unsigned int uid, int pid, int offset) {
   char* name = Hashtable_get(this->users, uid);
   if (name)
      return name;

   pmAtomValue value;
   if (Metric_instance(PCP_PROC_ID_USER, pid, offset, &value, PM_TYPE_STRING)) {
      Hashtable_put(this->users, uid, value.cp);
      name = value.cp;
   }
   return name;
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   PCPProcessList* this = xCalloc(1, sizeof(PCPProcessList));
   ProcessList* super = &(this->super);

   ProcessList_init(super, Class(PCPProcess), usersTable, pidMatchList, userId);

   pmAtomValue value;
   if (Metric_values(PCP_BOOTTIME, &value, 1, PM_TYPE_64) != NULL)
      this->btime = value.ll;

   this->cpu = xCalloc(CPU_METRIC_COUNT, sizeof(pmAtomValue));
   PCPProcessList_updateCPUcount(this);

   return super;
}

void ProcessList_delete(ProcessList* pl) {
   PCPProcessList* this = (PCPProcessList*) pl;
   ProcessList_done(pl);
   free(this->values);
   for (int i = 0; i < pl->cpuCount; i++)
      free(this->percpu[i]);
   free(this->percpu);
   free(this->cpu);
   free(this);
}

static void PCPProcessList_updateProcInfo(Process* process, int pid, int offset, char* command, int* commLen, long long boottime) {
   PCPProcess* pp = (PCPProcess*) process;
   pmAtomValue value;
   char *string;

   if (!Metric_instance(PCP_PROC_CMD, pid, offset, &value, PM_TYPE_STRING))
      value.cp = xStrdup("<unknown>");
   string = value.cp;
   const int commLenIn = *commLen - 1;
   int commsize = MINIMUM((int)strlen(value.cp) + 1, commLenIn);
   strncpy(command, value.cp, commLenIn);
   command[commsize] = '\0';
   *commLen = commsize;
   free(string);

   if (Metric_instance(PCP_PROC_TGID, pid, offset, &value, PM_TYPE_U32))
      process->tgid = value.ul;
   else
      process->tgid = 1;

   if (Metric_instance(PCP_PROC_PPID, pid, offset, &value, PM_TYPE_U32))
      process->ppid = value.ul;
   else
      process->ppid = 1;

   if (Metric_instance(PCP_PROC_STATE, pid, offset, &value, PM_TYPE_STRING)) {
      process->state = value.cp[0];
      free(value.cp);
   } else {
      process->state = 'X';
   }

   if (Metric_instance(PCP_PROC_PGRP, pid, offset, &value, PM_TYPE_U32))
      process->pgrp = value.ul;
   else
      process->pgrp = 0;

   if (Metric_instance(PCP_PROC_SESSION, pid, offset, &value, PM_TYPE_U32))
      process->session = value.ul;
   else
      process->session = 0;

   if (Metric_instance(PCP_PROC_TTY, pid, offset, &value, PM_TYPE_U32))
      process->tty_nr = value.ul;
   else
      process->tty_nr = 0;

   if (Metric_instance(PCP_PROC_TTYPGRP, pid, offset, &value, PM_TYPE_U32))
      process->tpgid = value.ul;
   else
      process->tpgid = 0;

   if (Metric_instance(PCP_PROC_FLAGS, pid, offset, &value, PM_TYPE_U32))
      process->flags = value.ul;
   else
      process->flags = 0;

   if (Metric_instance(PCP_PROC_MINFLT, pid, offset, &value, PM_TYPE_U32))
      process->minflt = value.ul;
   else
      process->minflt = 0;

   if (Metric_instance(PCP_PROC_CMINFLT, pid, offset, &value, PM_TYPE_U32))
      pp->cminflt = value.ul;
   else
      pp->cminflt = 0;

   if (Metric_instance(PCP_PROC_MAJFLT, pid, offset, &value, PM_TYPE_U32))
      process->majflt = value.ul;
   else
      process->majflt = 0;

   if (Metric_instance(PCP_PROC_CMAJFLT, pid, offset, &value, PM_TYPE_U32))
      pp->cmajflt = value.ul;
   else
      pp->cmajflt = 0;

   if (Metric_instance(PCP_PROC_UTIME, pid, offset, &value, PM_TYPE_U32))
      pp->utime = value.ul;
   else
      pp->utime = 0;

   if (Metric_instance(PCP_PROC_STIME, pid, offset, &value, PM_TYPE_U32))
      pp->stime = value.ul;
   else
      pp->stime = 0;

   if (Metric_instance(PCP_PROC_CUTIME, pid, offset, &value, PM_TYPE_U32))
      pp->cutime = value.ul;
   else
      pp->cutime = 0;

   if (Metric_instance(PCP_PROC_CSTIME, pid, offset, &value, PM_TYPE_U32))
      pp->cstime = value.ul;
   else
      pp->cstime = 0;

   if (Metric_instance(PCP_PROC_PRIORITY, pid, offset, &value, PM_TYPE_U32))
      process->priority = value.ul;
   else
      process->priority = 0;

   if (Metric_instance(PCP_PROC_NICE, pid, offset, &value, PM_TYPE_U32))
      process->nice = value.ul;
   else
      process->nice = 0;

   if (Metric_instance(PCP_PROC_THREADS, pid, offset, &value, PM_TYPE_U32))
      process->nlwp = value.ul;
   else
      process->nlwp = 0;

   if (Metric_instance(PCP_PROC_STARTTIME, pid, offset, &value, PM_TYPE_U64))
      process->starttime_ctime = value.ull;
   else
      process->starttime_ctime = 0;

   if (Metric_instance(PCP_PROC_EXITSIGNAL, pid, offset, &value, PM_TYPE_U32))
      process->exit_signal = value.ul;
   else
      process->exit_signal = 0;

   if (Metric_instance(PCP_PROC_PROCESSOR, pid, offset, &value, PM_TYPE_U32))
      process->processor = value.ul;
   else
      process->processor = 0;

   process->starttime_ctime += boottime;
   process->time = pp->utime + pp->stime;
}

static void PCPProcessList_updateIO(PCPProcess* process, int pid, int offset, unsigned long long now) {
   pmAtomValue value;

   if (Metric_instance(PCP_PROC_IO_RCHAR, pid, offset, &value, PM_TYPE_U64))
      process->io_rchar = value.ull;
   else
      process->io_rchar = 0;

   if (Metric_instance(PCP_PROC_IO_WCHAR, pid, offset, &value, PM_TYPE_U64))
      process->io_wchar = value.ull;
   else
      process->io_wchar = 0;

   if (Metric_instance(PCP_PROC_IO_SYSCR, pid, offset, &value, PM_TYPE_U64))
      process->io_syscr = value.ull;
   else
      process->io_syscr = 0;

   if (Metric_instance(PCP_PROC_IO_SYSCW, pid, offset, &value, PM_TYPE_U64))
      process->io_syscw = value.ull;
   else
      process->io_syscw = 0;

   if (Metric_instance(PCP_PROC_IO_CANCELLED, pid, offset, &value, PM_TYPE_U64))
      process->io_cancelled_write_bytes = value.ull;
   else
      process->io_cancelled_write_bytes = 0;

   if (Metric_instance(PCP_PROC_IO_READB, pid, offset, &value, PM_TYPE_U64)) {
      unsigned long long last_read = process->io_read_bytes;
      process->io_read_bytes = value.ull;
      process->io_rate_read_bps =
               ((double)(value.ull - last_read)) /
               (((double)(now - process->io_rate_read_time)) / 1000);
      process->io_rate_read_time = now;
   } else {
      process->io_read_bytes = -1LL;
      process->io_rate_read_bps = NAN;
      process->io_rate_read_time = -1LL;
   }

   if (Metric_instance(PCP_PROC_IO_WRITEB, pid, offset, &value, PM_TYPE_U64)) {
      unsigned long long last_write = process->io_write_bytes;
      process->io_write_bytes = value.ull;
      process->io_rate_write_bps =
               ((double)(value.ull - last_write)) /
               (((double)(now - process->io_rate_write_time)) / 1000);
      process->io_rate_write_time = now;
   } else {
      process->io_write_bytes = -1LL;
      process->io_rate_write_bps = NAN;
      process->io_rate_write_time = -1LL;
   }
}

static void PCPProcessList_updateMemory(PCPProcess* process, int pid, int offset) {
   pmAtomValue value;

   if (Metric_instance(PCP_PROC_MEM_SIZE, pid, offset, &value, PM_TYPE_U32))
      process->super.m_virt = value.ul;
   else
      process->super.m_virt = 0;

   if (Metric_instance(PCP_PROC_MEM_RSS, pid, offset, &value, PM_TYPE_U32))
      process->super.m_resident = value.ul;
   else
      process->super.m_resident = 0;

   if (Metric_instance(PCP_PROC_MEM_SHARE, pid, offset, &value, PM_TYPE_U32))
      process->m_share = value.ul;
   else
      process->m_share = 0;

   if (Metric_instance(PCP_PROC_MEM_TEXTRS, pid, offset, &value, PM_TYPE_U32))
      process->m_trs = value.ul;
   else
      process->m_trs = 0;

   if (Metric_instance(PCP_PROC_MEM_LIBRS, pid, offset, &value, PM_TYPE_U32))
      process->m_lrs = value.ul;
   else
      process->m_lrs = 0;

   if (Metric_instance(PCP_PROC_MEM_DATRS, pid, offset, &value, PM_TYPE_U32))
      process->m_drs = value.ul;
   else
      process->m_drs = 0;

   if (Metric_instance(PCP_PROC_MEM_DIRTY, pid, offset, &value, PM_TYPE_U32))
      process->m_dt = value.ul;
   else
      process->m_dt = 0;
}

static void PCPProcessList_updateSmaps(PCPProcess* process, pid_t pid, int offset) {
   pmAtomValue value;

   if (Metric_instance(PCP_PROC_SMAPS_PSS, pid, offset, &value, PM_TYPE_U64))
      process->m_pss = value.ull;
   else
      process->m_pss = 0LL;

   if (Metric_instance(PCP_PROC_SMAPS_SWAP, pid, offset, &value, PM_TYPE_U64))
      process->m_swap = value.ull;
   else
      process->m_swap = 0LL;

   if (Metric_instance(PCP_PROC_SMAPS_SWAPPSS, pid, offset, &value, PM_TYPE_U64))
      process->m_psswp = value.ull;
   else
      process->m_psswp = 0LL;
}

static void PCPProcessList_readOomData(PCPProcess* process, int pid, int offset) {
   pmAtomValue value;
   if (Metric_instance(PCP_PROC_OOMSCORE, pid, offset, &value, PM_TYPE_U32))
      process->oom = value.ul;
   else
      process->oom = 0;
}

static void PCPProcessList_readCtxtData(PCPProcess* process, int pid, int offset) {
   pmAtomValue value;
   unsigned long ctxt = 0;

   if (Metric_instance(PCP_PROC_VCTXSW, pid, offset, &value, PM_TYPE_U32))
      ctxt += value.ul;
   if (Metric_instance(PCP_PROC_NVCTXSW, pid, offset, &value, PM_TYPE_U32))
      ctxt += value.ul;
   if (ctxt > process->ctxt_total)
      process->ctxt_diff = ctxt - process->ctxt_total;
   else
      process->ctxt_diff = 0;
   process->ctxt_total = ctxt;
}

static char* setString(Metric metric, int pid, int offset, char* string) {
   if (string)
      free(string);
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_STRING))
      string = value.cp;
   else
      string = NULL;
   return string;
}

static void PCPProcessList_updateTTY(PCPProcess* process, int pid, int offset) {
   process->ttyDevice = setString(PCP_PROC_TTYNAME, pid, offset, process->ttyDevice);
}

static void PCPProcessList_readCGroups(PCPProcess* process, int pid, int offset) {
   process->cgroup = setString(PCP_PROC_CGROUPS, pid, offset, process->cgroup);
}

static void PCPProcessList_readSecattrData(PCPProcess* process, int pid, int offset) {
   process->secattr = setString(PCP_PROC_LABELS, pid, offset, process->secattr);
}

static void PCPProcessList_updateUsername(Process* process, int pid, int offset, UsersTable* users) {
   unsigned int uid = 0;
   pmAtomValue value;
   if (Metric_instance(PCP_PROC_ID_UID, pid, offset, &value, PM_TYPE_U32))
      uid = value.ul;
   process->st_uid = uid;
   process->user = setUser(users, uid, pid, offset);
}

static void setComm(Process* process, const char* command, int len) {
   if (process->comm && process->commLen >= len) {
      strncpy(process->comm, command, process->commLen);
      process->comm[process->commLen-1] = '\0';
   } else {
      if (process->comm)
         free(process->comm);
      process->comm = xStrdup(command);
   }
   process->commLen = len;
}

static void PCPProcessList_setComm(Process* process, const char* command, int len) {
   process->basenameOffset = -1;
   setComm(process, command, len);
}

static void PCPProcessList_updateCmdline(Process* process, int pid, int offset) {
   pmAtomValue value;
   if (Metric_instance(PCP_PROC_PSARGS, pid, offset, &value, PM_TYPE_STRING)) {
      bool kthread = (value.cp[0] == '(');
      Process_setKernelThread(process, kthread);
      process->basenameOffset = kthread ? 1 : 0;
      for (int i = 0; value.cp[i] != '\0'; i++) {
         if (!isspace(value.cp[i]))
            break;
         process->basenameOffset++;
      }
      setComm(process, value.cp, strlen(value.cp) + 1);
      free(value.cp);
   } else {
      process->comm = NULL;
      process->commLen = 0;
   }
}

static bool PCPProcessList_updateProcesses(PCPProcessList* this, double period, struct timeval tv) {
   ProcessList* pl = (ProcessList*) this;
   const Settings* settings = pl->settings;

   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   unsigned long long now = tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
   int pid = -1, offset = -1;

   /* for every process ... */
   while (Metric_iterate(PCP_PROC_PID, &pid, &offset)) {

      bool existed;
      Process* proc = ProcessList_getProcess(pl, pid, &existed, PCPProcess_new);
      PCPProcess* pp = (PCPProcess*) proc;

      if (settings->flags & PROCESS_FLAG_IO)
         PCPProcessList_updateIO(pp, pid, offset, now);

      PCPProcessList_updateMemory(pp, pid, offset);

      if ((settings->flags & PROCESS_FLAG_LINUX_SMAPS) &&
          (Process_isKernelThread(proc) == false)) {
         if (Metric_enabled(PCP_PROC_SMAPS_PSS))
            PCPProcessList_updateSmaps(pp, pid, offset);
      }

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) ||
                      (hideUserlandThreads && Process_isUserlandThread(proc)));

      char command[MAX_NAME + 1];
      int commLen = sizeof(command);
      unsigned int tty_nr = proc->tty_nr;

      PCPProcessList_updateProcInfo(proc, pid, offset, command, &commLen, this->btime);

      if (tty_nr != proc->tty_nr)
         PCPProcessList_updateTTY(pp, pid, offset);

      unsigned long long int lasttimes = pp->utime + pp->stime;
      float percent_cpu = (pp->utime + pp->stime - lasttimes) / period * 100.0;
      proc->percent_cpu = isnan(percent_cpu) ?
                          0.0 : CLAMP(percent_cpu, 0.0, pl->cpuCount * 100.0);
      proc->percent_mem = proc->m_resident / (double)pl->totalMem * 100.0;

      if (!existed) {
         PCPProcessList_updateUsername(proc, pid, offset, pl->usersTable);
         PCPProcessList_updateCmdline(proc, pid, offset);
         Process_fillStarttimeBuffer(proc);
         ProcessList_add(pl, proc);
      } else if (settings->updateProcessNames && proc->state != 'Z') {
         PCPProcessList_updateCmdline(proc, pid, offset);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_CGROUP)
         PCPProcessList_readCGroups(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_LINUX_OOM)
         PCPProcessList_readOomData(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_LINUX_CTXT)
         PCPProcessList_readCtxtData(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_LINUX_SECATTR)
         PCPProcessList_readSecattrData(pp, pid, offset);

      if (proc->state == 'Z' && proc->basenameOffset == 0) {
         PCPProcessList_setComm(proc, command, commLen);
      } else if (Process_isThread(proc)) {
         if (Process_isKernelThread(proc)) {
            pl->kernelThreads++;
            PCPProcessList_setComm(proc, command, commLen);
         } else {
            pl->userlandThreads++;
            if (settings->showThreadNames)
               PCPProcessList_updateCmdline(proc, pid, offset);
            else
               PCPProcessList_setComm(proc, command, commLen);
         }
      }

      pl->totalTasks++;
      if (proc->state == 'R')
         pl->runningTasks++;
      proc->updated = true;
   }
   return true;
}

static void PCPProcessList_updateMemoryInfo(ProcessList* super) {
   unsigned long long int shmem = 0;
   unsigned long long int sreclaimable = 0;
   super->totalMem = super->usedMem = super->cachedMem = 0;
   super->usedSwap = super->totalSwap = 0;

   pmAtomValue value;
   if (Metric_values(PCP_MEM_TOTAL, &value, 1, PM_TYPE_U64) != NULL)
      super->totalMem = value.ull;
   if (Metric_values(PCP_MEM_USED, &value, 1, PM_TYPE_U64) != NULL)
      super->usedMem = value.ull;
   if (Metric_values(PCP_MEM_BUFFERS, &value, 1, PM_TYPE_U64) != NULL)
      super->buffersMem = value.ull;
   if (Metric_values(PCP_MEM_SRECLAIM, &value, 1, PM_TYPE_U64) != NULL)
      sreclaimable = value.ull;
   if (Metric_values(PCP_MEM_SHARED, &value, 1, PM_TYPE_U64) != NULL)
      shmem = value.ull;
   if (Metric_values(PCP_MEM_CACHED, &value, 1, PM_TYPE_U64) != NULL) {
      super->cachedMem = value.ull;
      super->cachedMem += sreclaimable;
      super->cachedMem -= shmem;
   }
   if (Metric_values(PCP_SWAP_USED, &value, 1, PM_TYPE_U64) != NULL)
      super->usedSwap = value.ull / 1024;
   if (Metric_values(PCP_SWAP_TOTAL, &value, 1, PM_TYPE_U64) != NULL)
      super->totalSwap = value.ull / 1024;
}

/* make copies of previously sampled values to avoid overwrite */
static inline void PCPProcessList_backupCPUTime(pmAtomValue* values) {
   /* the PERIOD fields (must) mirror the TIME fields */
   for (unsigned int metric = 0; metric < CPU_TOTAL_PERIOD; metric++) {
      values[metric + CPU_TOTAL_TIME] = values[metric];
   }
}

static inline void PCPProcessList_saveCPUTimePeriod(pmAtomValue* values, CPUMetric previous, pmAtomValue* latest) {
   pmAtomValue *value;

   /* new value for period */
   value = &values[previous];
   if (latest->ull > value->ull)
      value->ull = latest->ull - value->ull;
   else
      value->ull = 0;

   /* new value for time */
   value = &values[previous - CPU_TOTAL_PERIOD];
   value->ull = latest->ull;
}

/* using copied sampled values and new values, calculate derivations */
static void PCPProcessList_deriveCPUTime(pmAtomValue* values) {

   pmAtomValue* usertime = &values[CPU_USER_TIME];
   pmAtomValue* guesttime = &values[CPU_GUEST_TIME];
   usertime->ull -= guesttime->ull;

   pmAtomValue* nicetime = &values[CPU_NICE_TIME];
   pmAtomValue* guestnicetime = &values[CPU_GUESTNICE_TIME];
   nicetime->ull -= guestnicetime->ull;

   pmAtomValue* idletime = &values[CPU_IDLE_TIME];
   pmAtomValue* iowaittime = &values[CPU_IOWAIT_TIME];
   pmAtomValue* idlealltime = &values[CPU_IDLE_ALL_TIME];
   idlealltime->ull = idletime->ull + iowaittime->ull;

   pmAtomValue* systemtime = &values[CPU_SYSTEM_TIME];
   pmAtomValue* irqtime = &values[CPU_IRQ_TIME];
   pmAtomValue* softirqtime = &values[CPU_SOFTIRQ_TIME];
   pmAtomValue* systalltime = &values[CPU_SYSTEM_ALL_TIME];
   systalltime->ull = systemtime->ull + irqtime->ull + softirqtime->ull;

   pmAtomValue* virtalltime = &values[CPU_GUEST_TIME];
   virtalltime->ull = guesttime->ull + guestnicetime->ull;

   pmAtomValue* stealtime = &values[CPU_STEAL_TIME];
   pmAtomValue* totaltime = &values[CPU_TOTAL_TIME];
   totaltime->ull = usertime->ull + nicetime->ull + systalltime->ull +
                    idlealltime->ull + stealtime->ull + virtalltime->ull;

#if 0
   PCPProcessList_saveCPUTimePeriod(values, CPU_USER_PERIOD, usertime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_NICE_PERIOD, nicetime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_SYSTEM_PERIOD, systemtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_SYSTEM_ALL_PERIOD, systalltime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IDLE_ALL_PERIOD, idlealltime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IDLE_PERIOD, idletime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IOWAIT_PERIOD, iowaittime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IRQ_PERIOD, irqtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_SOFTIRQ_PERIOD, softirqtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_STEAL_PERIOD, stealtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_GUEST_PERIOD, virtalltime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_TOTAL_PERIOD, totaltime);
#endif
}

static void PCPProcessList_updateAllCPUTime(PCPProcessList* this, Metric metric, CPUMetric cpumetric)
{
   pmAtomValue* value = &this->cpu[cpumetric];
   if (Metric_values(metric, value, 1, PM_TYPE_U64) == NULL)
      memset(&value, 0, sizeof(pmAtomValue));
}

static void PCPProcessList_updatePerCPUTime(PCPProcessList* this, Metric metric, CPUMetric cpumetric)
{
   int cpus = this->super.cpuCount;
   if (Metric_values(metric, this->values, cpus, PM_TYPE_U64) == NULL)
      memset(this->values, 0, cpus * sizeof(pmAtomValue));
   for (int i = 0; i < cpus; i++)
      this->percpu[i][cpumetric].ull = this->values[i].ull;
}

static void PCPProcessList_updatePerCPUReal(PCPProcessList* this, Metric metric, CPUMetric cpumetric)
{
   int cpus = this->super.cpuCount;
   if (Metric_values(metric, this->values, cpus, PM_TYPE_DOUBLE) == NULL)
      memset(this->values, 0, cpus * sizeof(pmAtomValue));
   for (int i = 0; i < cpus; i++)
      this->percpu[i][cpumetric].d = this->values[i].d;
}

static void PCPProcessList_updateHeader(ProcessList* super, const Settings* settings) {
   PCPProcessList_updateMemoryInfo(super);

   PCPProcessList* this = (PCPProcessList*) super;
   PCPProcessList_updateCPUcount(this);

   PCPProcessList_backupCPUTime(this->cpu);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_USER, CPU_USER_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_NICE, CPU_NICE_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_SYSTEM, CPU_SYSTEM_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_IDLE, CPU_IDLE_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_IOWAIT, CPU_IOWAIT_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_IRQ, CPU_IRQ_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_SOFTIRQ, CPU_SOFTIRQ_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_STEAL, CPU_STEAL_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_GUEST, CPU_GUEST_TIME);
   PCPProcessList_deriveCPUTime(this->cpu);

   for (int i = 0; i < super->cpuCount; i++)
      PCPProcessList_backupCPUTime(this->percpu[i]);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_USER, CPU_USER_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_NICE, CPU_NICE_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_SYSTEM, CPU_SYSTEM_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_IDLE, CPU_IDLE_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_IOWAIT, CPU_IOWAIT_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_IRQ, CPU_IRQ_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_SOFTIRQ, CPU_SOFTIRQ_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_STEAL, CPU_STEAL_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_GUEST, CPU_GUEST_TIME);
   for (int i = 0; i < super->cpuCount; i++)
      PCPProcessList_deriveCPUTime(this->percpu[i]);

   if (settings->showCPUFrequency)
      PCPProcessList_updatePerCPUReal(this, PCP_HINV_CPUCLOCK, CPU_FREQUENCY);
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   PCPProcessList* this = (PCPProcessList*) super;
   const Settings* settings = super->settings;
   bool enabled = !pauseProcessUpdate;

   bool flagged = settings->showCPUFrequency;
   Metric_enable(PCP_HINV_CPUCLOCK, flagged);

   /* In pause mode do not sample per-process metric values at all */
   for (int metric = PCP_PROC_PID; metric < PCP_METRIC_COUNT; metric++)
      Metric_enable(metric, enabled);

   flagged = settings->flags & PROCESS_FLAG_LINUX_CGROUP;
   Metric_enable(PCP_PROC_CGROUPS, flagged && enabled);
   flagged = settings->flags & PROCESS_FLAG_LINUX_OOM;
   Metric_enable(PCP_PROC_OOMSCORE, flagged && enabled);
   flagged = settings->flags & PROCESS_FLAG_LINUX_CTXT;
   Metric_enable(PCP_PROC_VCTXSW, flagged && enabled);
   Metric_enable(PCP_PROC_NVCTXSW, flagged && enabled);
   flagged = settings->flags & PROCESS_FLAG_LINUX_SECATTR;
   Metric_enable(PCP_PROC_LABELS, flagged && enabled);

   /* Sample smaps metrics on every second pass to improve performance */
   static int smaps_flag;
   smaps_flag = !smaps_flag;
   Metric_enable(PCP_PROC_SMAPS_PSS, smaps_flag && enabled);
   Metric_enable(PCP_PROC_SMAPS_SWAP, smaps_flag && enabled);
   Metric_enable(PCP_PROC_SMAPS_SWAPPSS, smaps_flag && enabled);

   struct timeval timestamp;
   Metric_fetch(&timestamp);

   double sample = this->timestamp;
   this->timestamp = pmtimevalToReal(&timestamp);

   PCPProcessList_updateHeader(super, settings);

   /* In pause mode only update global data for meters (CPU, memory, etc) */
   if (pauseProcessUpdate)
      return;

   double period = sample - this->timestamp;
   PCPProcessList_updateProcesses(this, period, timestamp);
}
