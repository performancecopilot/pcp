TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/perfevent
ROOTFILES =     config/syntax_error.txt \
		config/test_event_programming.txt\
		config/test_lots_of_counters.txt\
		config/test_rapl.txt \
		config/test_config.txt \
		config/test_init.txt \
		config/empty.txt \
		config/test_node_rr.txt \
		perfevent.conf \
		fakefs.tar.gz

TESTHARNESS = perfevent_test perfevent_coverage
LDIRT += $(TESTHARNESS) fakefs $(OUTDIR)
LSRCFILES = $(ROOTFILES) GNUmakefile.install

ifeq "$(PMDA_PERFEVENT)" "true"
default setup default_pcp: $(TESTHARNESS)

install install_pcp: $(ROOTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -d $(TESTDIR)/config
	$(INSTALL) -m 644 $(ROOTFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 755 $(TESTHARNESS) $(TESTDIR)
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)


OUTDIR = ./target
SRCDIR = $(TOPDIR)/src/pmdas/perfevent
SRCS = perf_event_test.c \
       architecture.c \
       mock_pfm.c \
       rapl-interface.c \
       configparser.yytest.c \
       $(SRCDIR)/perfinterface.c \

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

CFLAGS = -Wall -O0 -ggdb
CPPFLAGS = -I$(SRCDIR) -D_GNU_SOURCE -DFILESYSTEM_ROOT='"./fakefs/"'
LDFLAGS = -Wl,--wrap,syscall -Wl,--wrap,ioctl -Wl,--wrap,read -Wl,--wrap,close -Wl,--wrap,malloc -Wl,--wrap,sysconf
LDLIBS = -lm

THREADLDFLAGS = 
THREADLDLIBS = -lpthread -lrt

$(OUTDIR)/rapl-test: $(RAPL_OBJS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OUTDIR)/threadtest: $(THREAD_OBJS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(THREADLDFLAGS) -o $@ $^ $(THREADLDLIBS)

%.yytest.c: $(SRCDIR)/%.l
	flex -P yyest -o $@ $<

perfevent_coverage: $(COV_OBJS)
	$(CC) $(CFLAGS) $(COV_FLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

perfevent_test: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OUTDIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^ 

$(COV_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(COV_FLAGS) $(CPPFLAGS) -c -o $@ $^

