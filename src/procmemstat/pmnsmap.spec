metrics {
#ifdef linux
    proc.memory.rss	RSS_TOTAL
    proc.memory.textrss	RSS_TEXT
    proc.memory.librss	RSS_LIB
    proc.memory.datrss	RSS_DATA
#endif
#ifdef sgi
    /* IRIX */
    proc.memory.virtual.txt	V_TXT
    proc.memory.virtual.dat	V_DAT
    proc.memory.virtual.bss	V_BSS
    proc.memory.virtual.stack	V_STK
    proc.memory.virtual.shm	V_SHM
    proc.memory.physical.txt	P_TXT
    proc.memory.physical.dat	P_DAT
    proc.memory.physical.bss	P_BSS
    proc.memory.physical.stack	P_STK
    proc.memory.physical.shm	P_SHM
#endif
}
