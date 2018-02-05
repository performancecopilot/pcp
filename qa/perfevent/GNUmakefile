TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/perfevent
MYFILES = $(shell echo config/*.txt) perfevent.conf fakefs.tar.gz

TESTHARNESS = perfevent_test perfevent_coverage
LDIRT += $(TESTHARNESS) fakefs $(OUTDIR)

ifeq "$(PMDA_PERFEVENT)" "true"
default setup default_pcp: $(TESTHARNESS)

install install_pcp: default $(MYFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -d $(TESTDIR)/config
	$(INSTALL) -m 644 $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 755 $(TESTHARNESS) $(TESTDIR)
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)


OUTDIR = ./target
INCDIR = $(TOPDIR)/src/include/pcp
SRCDIR = $(TOPDIR)/src/pmdas/perfevent
SRCS = perf_event_test.c \
       architecture.c \
       mock_pfm.c \
       rapl-interface.c \
       configparser.yytest.c \
       $(SRCDIR)/perfinterface.c \
	$(SRCDIR)/parse_events.c

THREAD_SRCS = threadtest.c \
	      $(SRCDIR)/perfmanager.c \
	      mockperfinterface.c

VPATH=$(SRCDIR)

OBJS = $(patsubst %.c,$(OUTDIR)/%.o,$(notdir $(SRCS)))
THREAD_OBJS = $(patsubst %.c,$(OUTDIR)/%.o,$(notdir $(THREAD_SRCS)))

RAPL_OBJS=$(OUTDIR)/rapl_test.o $(OUTDIR)/rapl-interface.o $(OUTDIR)/mock_pfm.o

COV_FLAGS= --coverage
COV_DIR=$(OUTDIR)/gcov
COV_OBJS= $(patsubst %.c,$(COV_DIR)/%.o,$(notdir $(SRCS)))

CFLAGS_OPT = -O0 -ggdb
LCFLAGS = -I$(SRCDIR) -DFILESYSTEM_ROOT='"./fakefs/"'
LLDFLAGS = -Wl,--wrap,syscall -Wl,--wrap,ioctl -Wl,--wrap,read -Wl,--wrap,close -Wl,--wrap,malloc -Wl,--wrap,sysconf $(PCP_LIBS)
LLDLIBS = $(PCPLIB) $(PCPLIB_EXTRAS)

THREADLDFLAGS = 
THREADLDLIBS = $(LIB_FOR_PTHREADS) $(LIB_FOR_RT)

$(OUTDIR)/rapl-test: $(RAPL_OBJS)
	mkdir -p $(@D)
	$(CCF) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OUTDIR)/threadtest: $(THREAD_OBJS)
	mkdir -p $(@D)
	$(CCF) $(THREADLDFLAGS) -o $@ $^ $(THREADLDLIBS)

%.yytest.c: $(SRCDIR)/%.l
	$(LEX) -P yyest -o $@ $<

perfevent_coverage: $(COV_OBJS)
	$(CCF) $(COV_FLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

perfevent_test: $(OBJS)
	$(CCF) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OUTDIR)/%.o: %.c
	mkdir -p $(@D)
	$(CCF) -c -o $@ $^ 

$(COV_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CCF) $(COV_FLAGS) -c -o $@ $^

