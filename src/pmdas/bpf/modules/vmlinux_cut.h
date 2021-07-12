/**
 * kernel-side typedef and structs to allow compilation of BPF programs
 * 
 * These structs should match the kernel definitions and are best created by copying them
 * from existing kernel type information. You can regenerate a complete vmlinux.h file
 * in which you find these values. To do this, use a BTF enabled kernel and run:
 * $ bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
 * 
 * From this file you can extract the needed values.
 */

typedef signed char __s8;

typedef unsigned char __u8;

typedef short int __s16;

typedef short unsigned int __u16;

typedef int __s32;

typedef unsigned int __u32;

typedef long long int __s64;

typedef long long unsigned int __u64;

typedef __s8 s8;

typedef __u8 u8;

typedef __s16 s16;

typedef __u16 u16;

typedef __s32 s32;

typedef __u32 u32;

typedef __s64 s64;

typedef __u64 u64;

enum {
	false = 0,
	true = 1,
};

typedef __u16 __le16;

typedef __u16 __be16;

typedef __u32 __be32;

typedef __u64 __be64;

typedef __u32 __wsum;

typedef _Bool bool;

typedef u32 __kernel_dev_t;

typedef __kernel_dev_t dev_t;

enum bpf_map_type {
	BPF_MAP_TYPE_UNSPEC = 0,
	BPF_MAP_TYPE_HASH = 1,
	BPF_MAP_TYPE_ARRAY = 2,
	BPF_MAP_TYPE_PROG_ARRAY = 3,
	BPF_MAP_TYPE_PERF_EVENT_ARRAY = 4,
	BPF_MAP_TYPE_PERCPU_HASH = 5,
	BPF_MAP_TYPE_PERCPU_ARRAY = 6,
	BPF_MAP_TYPE_STACK_TRACE = 7,
	BPF_MAP_TYPE_CGROUP_ARRAY = 8,
	BPF_MAP_TYPE_LRU_HASH = 9,
	BPF_MAP_TYPE_LRU_PERCPU_HASH = 10,
	BPF_MAP_TYPE_LPM_TRIE = 11,
	BPF_MAP_TYPE_ARRAY_OF_MAPS = 12,
	BPF_MAP_TYPE_HASH_OF_MAPS = 13,
	BPF_MAP_TYPE_DEVMAP = 14,
	BPF_MAP_TYPE_SOCKMAP = 15,
	BPF_MAP_TYPE_CPUMAP = 16,
	BPF_MAP_TYPE_XSKMAP = 17,
	BPF_MAP_TYPE_SOCKHASH = 18,
	BPF_MAP_TYPE_CGROUP_STORAGE = 19,
	BPF_MAP_TYPE_REUSEPORT_SOCKARRAY = 20,
	BPF_MAP_TYPE_PERCPU_CGROUP_STORAGE = 21,
	BPF_MAP_TYPE_QUEUE = 22,
	BPF_MAP_TYPE_STACK = 23,
	BPF_MAP_TYPE_SK_STORAGE = 24,
	BPF_MAP_TYPE_DEVMAP_HASH = 25,
};

struct pt_regs {
	long unsigned int r15;
	long unsigned int r14;
	long unsigned int r13;
	long unsigned int r12;
	long unsigned int bp;
	long unsigned int bx;
	long unsigned int r11;
	long unsigned int r10;
	long unsigned int r9;
	long unsigned int r8;
	long unsigned int ax;
	long unsigned int cx;
	long unsigned int dx;
	long unsigned int si;
	long unsigned int di;
	long unsigned int orig_ax;
	long unsigned int ip;
	long unsigned int cs;
	long unsigned int flags;
	long unsigned int sp;
	long unsigned int ss;
};

struct rq;

typedef int __kernel_pid_t;
typedef __kernel_pid_t pid_t;
struct task_struct {
	// struct thread_info thread_info;
	volatile long int state;
	void *stack;
	// refcount_t usage;
	unsigned int flags;
	unsigned int ptrace;
	int on_cpu;
	// struct __call_single_node wake_entry;
	unsigned int cpu;
	unsigned int wakee_flips;
	long unsigned int wakee_flip_decay_ts;
	struct task_struct *last_wakee;
	int recent_used_cpu;
	int wake_cpu;
	int on_rq;
	int prio;
	int static_prio;
	int normal_prio;
	unsigned int rt_priority;
	const struct sched_class *sched_class;
	// struct sched_entity se;
	// struct sched_rt_entity rt;
	// struct task_group *sched_task_group;
	// struct sched_dl_entity dl;
	// struct uclamp_se uclamp_req[2];
	// struct uclamp_se uclamp[2];
	// struct hlist_head preempt_notifiers;
	unsigned int btrace_seq;
	unsigned int policy;
	int nr_cpus_allowed;
	// const cpumask_t *cpus_ptr;
	// cpumask_t cpus_mask;
	long unsigned int rcu_tasks_nvcsw;
	u8 rcu_tasks_holdout;
	u8 rcu_tasks_idx;
	int rcu_tasks_idle_cpu;
	// struct list_head rcu_tasks_holdout_list;
	int trc_reader_nesting;
	int trc_ipi_to_cpu;
	// union rcu_special trc_reader_special;
	bool trc_reader_checked;
	// struct list_head trc_holdout_list;
	// struct sched_info sched_info;
	// struct list_head tasks;
	// struct plist_node pushable_tasks;
	// struct rb_node pushable_dl_tasks;
	struct mm_struct *mm;
	struct mm_struct *active_mm;
	// struct vmacache vmacache;
	// struct task_rss_stat rss_stat;
	int exit_state;
	int exit_code;
	int exit_signal;
	int pdeath_signal;
	long unsigned int jobctl;
	unsigned int personality;
	unsigned int sched_reset_on_fork: 1;
	unsigned int sched_contributes_to_load: 1;
	unsigned int sched_migrated: 1;
	unsigned int sched_psi_wake_requeue: 1;
	int: 28;
	unsigned int sched_remote_wakeup: 1;
	unsigned int in_execve: 1;
	unsigned int in_iowait: 1;
	unsigned int restore_sigmask: 1;
	unsigned int in_user_fault: 1;
	unsigned int no_cgroup_migration: 1;
	unsigned int frozen: 1;
	unsigned int use_memdelay: 1;
	unsigned int in_memstall: 1;
	long unsigned int atomic_flags;
	// struct restart_block restart_block;
	pid_t pid;
	pid_t tgid;
	long unsigned int stack_canary;
	struct task_struct *real_parent;
	struct task_struct *parent;
	// struct list_head children;
	// struct list_head sibling;
	struct task_struct *group_leader;
	// struct list_head ptraced;
	// struct list_head ptrace_entry;
	struct pid *thread_pid;
	// struct hlist_node pid_links[4];
	// struct list_head thread_group;
	// struct list_head thread_node;
	struct completion *vfork_done;
	int *set_child_tid;
	int *clear_child_tid;
	u64 utime;
	u64 stime;
	u64 gtime;
	// struct prev_cputime prev_cputime;
	long unsigned int nvcsw;
	long unsigned int nivcsw;
	u64 start_time;
	u64 start_boottime;
	long unsigned int min_flt;
	long unsigned int maj_flt;
	// struct posix_cputimers posix_cputimers;
	const struct cred *ptracer_cred;
	const struct cred *real_cred;
	const struct cred *cred;
	struct key *cached_requested_key;
	char comm[16];
	struct nameidata *nameidata;
	// struct sysv_sem sysvsem;
	// struct sysv_shm sysvshm;
	long unsigned int last_switch_count;
	long unsigned int last_switch_time;
	struct fs_struct *fs;
	struct files_struct *files;
	struct io_uring_task *io_uring;
	struct nsproxy *nsproxy;
	struct signal_struct *signal;
	struct sighand_struct *sighand;
	// sigset_t blocked;
	// sigset_t real_blocked;
	// sigset_t saved_sigmask;
	// struct sigpending pending;
	long unsigned int sas_ss_sp;
	// size_t sas_ss_size;
	unsigned int sas_ss_flags;
	struct callback_head *task_works;
	struct audit_context *audit_context;
	// kuid_t loginuid;
	unsigned int sessionid;
	// struct seccomp seccomp;
	u64 parent_exec_id;
	u64 self_exec_id;
	// spinlock_t alloc_lock;
	// raw_spinlock_t pi_lock;
	// struct wake_q_node wake_q;
	// struct rb_root_cached pi_waiters;
	struct task_struct *pi_top_task;
	struct rt_mutex_waiter *pi_blocked_on;
	void *journal_info;
	struct bio_list *bio_list;
	struct blk_plug *plug;
	struct reclaim_state *reclaim_state;
	struct backing_dev_info *backing_dev_info;
	struct io_context *io_context;
	struct capture_control *capture_control;
	long unsigned int ptrace_message;
	// kernel_siginfo_t *last_siginfo;
	// struct task_io_accounting ioac;
	unsigned int psi_flags;
	u64 acct_rss_mem1;
	u64 acct_vm_mem1;
	u64 acct_timexpd;
	// nodemask_t mems_allowed;
	// seqcount_t mems_allowed_seq;
	int cpuset_mem_spread_rotor;
	int cpuset_slab_spread_rotor;
	struct css_set *cgroups;
	// struct list_head cg_list;
	u32 closid;
	u32 rmid;
	struct robust_list_head *robust_list;
	struct compat_robust_list_head *compat_robust_list;
	// struct list_head pi_state_list;
	struct futex_pi_state *pi_state_cache;
	// struct mutex futex_exit_mutex;
	unsigned int futex_state;
	struct perf_event_context *perf_event_ctxp[2];
	// struct mutex perf_event_mutex;
	// struct list_head perf_event_list;
	struct mempolicy *mempolicy;
	short int il_prev;
	short int pref_node_fork;
	int numa_scan_seq;
	unsigned int numa_scan_period;
	unsigned int numa_scan_period_max;
	int numa_preferred_nid;
	long unsigned int numa_migrate_retry;
	u64 node_stamp;
	u64 last_task_numa_placement;
	u64 last_sum_exec_runtime;
	// struct callback_head numa_work;
	struct numa_group *numa_group;
	long unsigned int *numa_faults;
	long unsigned int total_numa_faults;
	long unsigned int numa_faults_locality[3];
	long unsigned int numa_pages_migrated;
	struct rseq *rseq;
	u32 rseq_sig;
	long unsigned int rseq_event_mask;
	// struct tlbflush_unmap_batch tlb_ubc;
	// union {
	// 	refcount_t rcu_users;
	// 	struct callback_head rcu;
	// };
	struct pipe_inode_info *splice_pipe;
	// struct page_frag task_frag;
	struct task_delay_info *delays;
	int nr_dirtied;
	int nr_dirtied_pause;
	long unsigned int dirty_paused_when;
	u64 timer_slack_ns;
	u64 default_timer_slack_ns;
	int curr_ret_stack;
	int curr_ret_depth;
	struct ftrace_ret_stack *ret_stack;
	long long unsigned int ftrace_timestamp;
	// atomic_t trace_overrun;
	// atomic_t tracing_graph_pause;
	long unsigned int trace;
	long unsigned int trace_recursion;
	struct mem_cgroup *memcg_in_oom;
	// gfp_t memcg_oom_gfp_mask;
	int memcg_oom_order;
	unsigned int memcg_nr_pages_over_high;
	struct mem_cgroup *active_memcg;
	struct request_queue *throttle_queue;
	struct uprobe_task *utask;
	unsigned int sequential_io;
	unsigned int sequential_io_avg;
	int pagefault_disabled;
	struct task_struct *oom_reaper_list;
	struct vm_struct *stack_vm_area;
	// refcount_t stack_refcount;
	int patch_state;
	void *security;
	u64 mce_addr;
	__u64 mce_ripv: 1;
	__u64 mce_whole_page: 1;
	__u64 __mce_reserved: 62;
	// struct callback_head mce_kill_me;
	// struct thread_struct thread;
};

enum {
	BPF_ANY = 0,
	BPF_NOEXIST = 1,
	BPF_EXIST = 2,
	BPF_F_LOCK = 4,
};

enum {
	BPF_F_NO_PREALLOC = 1,
	BPF_F_NO_COMMON_LRU = 2,
	BPF_F_NUMA_NODE = 4,
	BPF_F_RDONLY = 8,
	BPF_F_WRONLY = 16,
	BPF_F_STACK_BUILD_ID = 32,
	BPF_F_ZERO_SEED = 64,
	BPF_F_RDONLY_PROG = 128,
	BPF_F_WRONLY_PROG = 256,
	BPF_F_CLONE = 512,
	BPF_F_MMAPABLE = 1024,
};
