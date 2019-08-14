$(info Running PCP-STATSD makefile)

IAM = statsd
DOMAIN = STATSD
PMDA_DIR = $(PCP_PMDAS_DIR)/$(IAM)
CURRENT_WORKING_DIRECTORY = $(pwd)
ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif

include $(PCP_INC_DIR)/builddefs

TARGET_EXEC ?= pmda$(IAM)
TEST_BASIC_EXEC ?= basic-parser
TEST_RAGEL_EXEC ?= ragel

ROOT_BUILD_DIR ?= ./build
MAIN_BUILD_DIR ?= $(ROOT_BUILD_DIR)/$(TARGET_EXEC)
TEST_BASIC_BUILD_DIR ?= $(ROOT_BUILD_DIR)/$(TEST_BASIC_EXEC)
TEST_RAGEL_BUILD_DIR ?= $(ROOT_BUILD_DIR)/$(TEST_RAGEL_EXEC)

SRC_DIRS ?= ./src

RAGEL_TARGET := ./src/parser-ragel.c
RAGEL_SRCS := $(shell find $(SRC_DIRS) -name *.rl)
SRCS := $(RAGEL_TARGET) $(shell find $(SRC_DIRS) \( -name *.cpp -or -name *.c -or -name *.s \) ! -path $(RAGEL_TARGET))
OBJS := $(SRCS:%=$(MAIN_BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

TEST_BASIC_OBJS := $(SRCS:%=$(TEST_BASIC_BUILD_DIR)/%.o)
TEST_BASIC_DEPS := $(OBJS:.o=.d)

TEST_RAGEL_OBJS := $(SRCS:%=$(TEST_RAGEL_BUILD_DIR)/%.o)
TEST_RAGEL_DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS += $(CURRENT_WORKING_DIRECTORY)

ifneq "$(PCP_INC_DIR)" "/usr/include/pcp"
INC_DIRS += $(PCP_INC_DIR)/..
endif

ifneq "$(PCP_LIB_DIR)" "/user/lib"
ifeq "$(PCP_PLATFORM)" "darwin"
PCP_LIBS += -L$(PCP_LIB_DIR) -Wl,-rpath $(PCP_LIB_DIR)
else
PCP_LIBS += -L$(PCP_LIB_DIR) -Wl,-rpath=$(PCP_LIB_DIR)
endif
endif

INC_FLAGS := $(addprefix -I,$(INC_DIRS))

LDLIBS := -lhdr_histogram_static -lchan -lm -lpthread -lpcp_web -lpcp -lpcp_pmda

CFLAGS = -Wextra $(INC_FLAGS) -MMD -MP -g

DFILES	= README.md

all default default_pcp: $(MAIN_BUILD_DIR)/$(TARGET_EXEC)
	cp $(MAIN_BUILD_DIR)/$(TARGET_EXEC) $(TARGET_EXEC)

$(RAGEL_TARGET): $(RAGEL_SRCS)
	ragel -C $<

$(MAIN_BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -g -o $@ $(LDLIBS)

$(TEST_BASIC_BUILD_DIR)/$(TEST_BASIC_EXEC): CFLAGS += -D_TEST_TARGET=1
$(TEST_BASIC_BUILD_DIR)/$(TEST_BASIC_EXEC): $(TEST_BASIC_OBJS)
	$(CC) $(TEST_BASIC_OBJS) -g -o $@ $(LDLIBS)

$(TEST_RAGEL_BUILD_DIR)/$(TEST_RAGEL_EXEC): CFLAGS += -D_TEST_TARGET=2
$(TEST_RAGEL_BUILD_DIR)/$(TEST_RAGEL_EXEC): $(TEST_RAGEL_OBJS)
	$(CC) $(TEST_RAGEL_OBJS) -g -o $@ $(LDLIBS)

# c source
$(MAIN_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -O0 -c $< -o $@ 

# Tests dont work with optimization ON
$(TEST_BASIC_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -O0 -c $< -o $@ 

$(TEST_RAGEL_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -O0 -c $< -o $@ 

.PHONY: test

clean:
	$(RM) -r $(ROOT_BUILD_DIR)
	$(RM) -r $(RAGEL_TARGET)
	$(RM) -r $(TARGET_EXEC)
	$(RM) -r statsd.log help.dir help.pag debug

run:
	./$(TARGET_EXEC)

test-basic: $(TEST_BASIC_BUILD_DIR)/$(TEST_BASIC_EXEC)
	$^

test-ragel: $(TEST_RAGEL_BUILD_DIR)/$(TEST_RAGEL_EXEC) 
	$^		

install: all

clobber:

activate: 
	./Install

deactivate:
	./Remove

debug:
	dbpmda -n root

MKDIR_P ?= mkdir -p
