_sample {
	kernel.all.load		LOADAV
	kernel.all.cpu.user	CPU_USER
	kernel.all.cpu.sys		CPU_KERNEL
	kernel.all.cpu.sxbrk	CPU_SXBRK
	kernel.all.cpu.intr	CPU_INTR
	kernel.all.cpu.idle	CPU_IDLE
	kernel.all.cpu.wait.total	CPU_WAIT
	mem.freemem		FREEMEM
	kernel.all.runque		RUNQ
	kernel.all.runocc		RUNQ_OCC
	kernel.all.swap.swpque	SWAPQ
	kernel.all.swap.swpocc	SWAPQ_OCC
	swap.pagesout		SWAPOUT
	kernel.all.syscall		SYSCALL
	kernel.all.pswitch		CONTEXTSW
	kernel.all.intr.non_vme	INTR
	disk.all.read		DKREAD
	disk.all.write		DKWRITE
}

_extra {
	network.interface.in.packets	PACK_IN
	network.interface.out.packets	PACK_OUT
}
