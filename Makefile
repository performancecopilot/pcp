$(info Running PCP-STATSD makefile)

TARGET_EXEC ?= pcp-statsd
TEST_EXEC ?= basic-parser

ROOT_BUILD_DIR ?= ./build
MAIN_BUILD_DIR ?= $(ROOT_BUILD_DIR)/$(TARGET_EXEC)
TEST_BUILD_DIR ?= $(ROOT_BUILD_DIR)/$(TEST_EXEC)

SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(MAIN_BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

TEST_OBJS := $(SRCS:%=$(TEST_BUILD_DIR)/%.o)
TEST_DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LDLIBS := -lhdr_histogram_static -lchan -lm -lpthread -lpcp_web -lpcp

CFLAGS ?=-Wall -Wextra $(INC_FLAGS) -MMD -MP

all: $(MAIN_BUILD_DIR)/$(TARGET_EXEC) $(TEST_BUILD_DIR)/$(TEST_EXEC)

$(MAIN_BUILD_DIR)/$(TARGET_EXEC): CFLAGS += -D_TEST_TARGET=0
$(MAIN_BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(TEST_BUILD_DIR)/$(TEST_EXEC): CFLAGS += -D_TEST_TARGET=1
$(TEST_BUILD_DIR)/$(TEST_EXEC): $(TEST_OBJS)
	$(CC) $(TEST_OBJS) -o $@ $(LDLIBS)

# c source
$(MAIN_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -g -c $< -o $@ 

$(TEST_BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -g -c $< -o $@ 

.PHONY: test

clean:
	$(RM) -r $(ROOT_BUILD_DIR)

run: 
	$(MAIN_BUILD_DIR)/$(TARGET_EXEC)

test:
	$(TEST_BUILD_DIR)/$(TEST_EXEC)

MKDIR_P ?= mkdir -p