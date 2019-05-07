TARGET_EXEC ?= a.out

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_LIBS_DIRS = -I./vendor/HdrHistogram_c/src -I./vendor/chan/src -I./vendor/chan/.libs
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LDFLAGS := -L./vendor/HdrHistogram_c/src -L./vendor/chan/.libs -L./vendor/chan/.libs
LDLIBS := -lhdr_histogram_static -lchan -lm -lpthread

CFLAGS ?=-Wall -Wextra $(INC_LIBS_DIRS) $(INC_FLAGS) -MMD -MP -g

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ 

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

dependencies: chan histogram

chan:
	cd vendor/chan && ./autogen.sh && ./configure && $(MAKE)

histogram:
	cd vendor/HdrHistogram_c && cmake . && $(MAKE)

run: 
	./build/a.out

-include $(DEPS)

MKDIR_P ?= mkdir -p
