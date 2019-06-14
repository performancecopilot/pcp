$(info Running PCP-STATSD makefile)

TARGET_EXEC ?= pcp-statsd

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LDLIBS := -lhdr_histogram_static -lchan -lm -lpthread -lpcp_web

CFLAGS ?=-Wall -Wextra $(INC_FLAGS) -MMD -MP

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -g -c $< -o $@ 

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

run: 
	./build/pcp-statsd

-include $(DEPS)

MKDIR_P ?= mkdir -p