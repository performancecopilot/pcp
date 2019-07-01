$(info Running PCP-STATSD makefile)

TARGET_EXEC ?= pcp-statsd
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
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LDLIBS := -lhdr_histogram_static -lchan -lm -lpthread -lpcp_web -lpcp

CFLAGS ?=-Wall -Wextra $(INC_FLAGS) -MMD -MP

all: $(MAIN_BUILD_DIR)/$(TARGET_EXEC)

$(RAGEL_TARGET): $(RAGEL_SRCS)
	ragel -C $<

$(MAIN_BUILD_DIR)/$(TARGET_EXEC): CFLAGS += -D_TEST_TARGET=0
$(MAIN_BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(TEST_BASIC_BUILD_DIR)/$(TEST_BASIC_EXEC): CFLAGS += -D_TEST_TARGET=1
$(TEST_BASIC_BUILD_DIR)/$(TEST_BASIC_EXEC): $(TEST_BASIC_OBJS)
	$(CC) $(TEST_BASIC_OBJS) -o $@ $(LDLIBS)

$(TEST_RAGEL_BUILD_DIR)/$(TEST_RAGEL_EXEC): CFLAGS += -D_TEST_TARGET=2
$(TEST_RAGEL_BUILD_DIR)/$(TEST_RAGEL_EXEC): $(TEST_RAGEL_OBJS)
	$(CC) $(TEST_RAGEL_OBJS) -o $@ $(LDLIBS)

# c source
$(MAIN_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -g -c $< -o $@ 

$(TEST_BASIC_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -g -c $< -o $@ 

$(TEST_RAGEL_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -g -c $< -o $@ 

.PHONY: test

clean:
	$(RM) -r $(ROOT_BUILD_DIR)
	$(RM) -r $(RAGEL_TARGET)

run: 
	$(MAIN_BUILD_DIR)/$(TARGET_EXEC)

test-basic: $(TEST_BASIC_BUILD_DIR)/$(TEST_BASIC_EXEC)
	$^

test-ragel: $(TEST_RAGEL_BUILD_DIR)/$(TEST_RAGEL_EXEC) 
	$^

MKDIR_P ?= mkdir -p
