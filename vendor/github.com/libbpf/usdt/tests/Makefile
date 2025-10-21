# SPDX-License-Identifier: BSD-2-Clause
AWK ?= awk
READELF ?= readelf
BPFTRACE ?= bpftrace
OUTPUT ?= .output
SHARED ?= 0

CFLAGS += $(EXTRA_CFLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

export AWK READELF BPFTRACE

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s\n'					\
		      "$(1)"						\
		      "$(patsubst $(OUTPUT)/%,%,$(2))"
	MAKEFLAGS += --no-print-directory
endif

ifeq ($(OUTPUT),)
$(error OUTPUT variable cannot be empty)
endif

ifeq ($(SHARED),1)
CFLAGS += -DSHARED -fPIC
CXXFLAGS += -DSHARED -fPIC
OBJDIR := $(OUTPUT)/shared
else
OBJDIR := $(OUTPUT)/static
endif


NONTESTS = tester common

TESTS := $(filter-out $(NONTESTS), 								\
		$(shell ls *.c *.cpp 2>/dev/null | grep -v '^lib' | 				\
			${AWK} '{split($$0, p, /[^A-Za-z_]+/); print p[1]}' |			\
			sort | uniq 								\
		)										\
)
LIBS := $(filter-out $(NONTESTS), 								\
		$(shell ls lib*.c lib*.cpp 2>/dev/null | 					\
			${AWK} '{split($$0, p, /[^A-Za-z_]+/); print substr(p[1], 4)}' |	\
			sort | uniq 								\
		)										\
)

BUILD_TARGETS = $(TESTS:%=build-%)
TEST_TARGETS = $(TESTS:%=test-%)

.PHONY: all
all: build

.PHONY: clean
clean:
	$(call msg,CLEAN)
	$(Q)$(RM) -rf $(OUTPUT)

.PHONY: list
list:
	$(Q)$(foreach test,$(TESTS), $(info $(test)))

.PHONY: build
build: $(BUILD_TARGETS)

.PHONY: test
test: $(TEST_TARGETS)

$(OUTPUT) $(OBJDIR):
	$(Q)mkdir -p $@

# arg_types.c/cxx_arg_types.cpp generate "warning: unsupported size for integer register"
$(OBJDIR)/arg_types.o: CFLAGS += -w
$(OBJDIR)/long_nop_usdt.o: CFLAGS += -w
$(OBJDIR)/cxx_arg_types.opp: CXXFLAGS += -w
$(OBJDIR)/cxx_long_nop_usdt.opp: CXXFLAGS += -w

$(OBJDIR)/%.o: %.c $(wildcard *.h) | $(OBJDIR)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) -g -c $< -o $@

$(OBJDIR)/%.opp: %.cpp $(wildcard *.h) | $(OBJDIR)
	$(call msg,CXX,$@)
	$(Q)$(CXX) $(CXXFLAGS) -g -c $< -o $@

ifeq ($(SHARED),1)
define BUILD_RULE_LIB
$$(OBJDIR)/$(1): $$(OBJDIR)/lib$(1).so

$$(OBJDIR)/lib$(1).so: $$(patsubst %.c,$$(OBJDIR)/%.o,$$(wildcard lib$(1)*.c)) 			\
		       $$(patsubst %.cpp,$$(OBJDIR)/%.opp,$$(wildcard lib$(1)*.cpp))		\
		       | $$(OBJDIR)
	$$(call msg,SHLIB,$$@)
	$$(Q)$$(if $$(filter %.opp, $$^),							\
		$$(CXX) $$(CXXFLAGS) $$(LDFLAGS) --shared -g $$^ -o $$@,			\
		$$(CC) $$(CFLAGS) $$(LDFLAGS) --shared -g $$^ -o $$@)
endef
else # !SHARED
define BUILD_RULE_LIB
$$(OBJDIR)/$(1): $$(OBJDIR)/lib$(1).a

$$(OBJDIR)/lib$(1).a: $$(patsubst %.c,$$(OBJDIR)/%.o,$$(wildcard lib$(1)*.c)) 			\
		      $$(patsubst %.cpp,$$(OBJDIR)/%.opp,$$(wildcard lib$(1)*.cpp))		\
		      | $$(OBJDIR)
	$$(call msg,LIB,$$@)
	$$(Q)$$(RM) -f $$@
	$$(Q)$$(AR) rcs $$@ $$^

endef
endif # SHARED

define BUILD_RULE
$$(OBJDIR)/$(1): $$(patsubst %.c,$$(OBJDIR)/%.o,$$(wildcard $(1)*.c)) 				\
		 $$(patsubst %.cpp,$$(OBJDIR)/%.opp,$$(wildcard $(1)*.cpp)) 			\
		 $$(OBJDIR)/common.o | $$(OBJDIR)
	$$(call msg,LINK,$$@)
	$$(Q)$$(if $$(filter %.opp, $$^),							\
		$$(CXX) $$(CXXFLAGS) $$(LDFLAGS) -g $$^ -o $$@,					\
		$$(CC) $$(CFLAGS) $$(LDFLAGS) -g $$^ -o $$@)
endef

$(foreach test, $(TESTS), $(eval $(call BUILD_RULE,$(test))))
$(foreach lib, $(LIBS), $(eval $(call BUILD_RULE_LIB,$(lib))))

$(BUILD_TARGETS): build-%: $(OBJDIR)/%

$(TEST_TARGETS): test-%: build-%
	$(call msg,TESTING,$(if $(filter 1,$(SHARED)),shared/,static/)$(patsubst test-%,%,$@))
	$(Q)./run_test.sh $(OBJDIR) $(patsubst test-%,%,$@)
