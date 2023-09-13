# Project Setup
PROJECT := scallop
BUILDDIR := ./build
SRCDIR   := ./src
OBJDIR   := ./obj

# External Library Dependencies
RAYCO_DIR = ./ext/RayCO

# Gather Project Sources and Folders
SOURCES := $(notdir $(shell find ./src -follow -name '*.c'))
FOLDERS := $(sort $(dir $(shell find ./src -follow -name '*.c')))
FOLDERS += $(RAYCO_DIR)/src
OBJECTS  := $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SOURCES)))
INCLUDE  := $(patsubst %,-I%,$(FOLDERS))
VPATH    := $(FOLDERS)

# Unit Test Configuration
TEST_SRCS := $(notdir $(shell find ./test -follow -name 'test_*.c'))
TEST_DIRS := $(sort $(dir $(shell find ./test -follow -name 'test_*.c')))
TEST_OBJS := $(patsubst %.c,%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst %.c,%.mut,$(TEST_SRCS))
TEST_INCL := $(patsubst %,-I%,$(TEST_DIRS))
AUX_SRCS  := $(notdir $(shell find ./test -follow -name '*.c' -not -name 'test*'))
AUX_OBJS  := $(patsubst %.c,%.o,$(AUX_SRCS))
VPATH     += $(TEST_DIRS)

# Toolchain Configuration
AR           := ar
LD           := ld
CC           := gcc
BIN          := /usr/local/bin
LIB          := /usr/local/lib
#CFLAGS       := $(INCLUDE) -Wall -pipe -std=c99 -fPIC -D__USE_MISC -D__USE_BSD
CFLAGS       := $(INCLUDE) -Wall -pipe -fPIC

# Platform Conditional Linker Flags
DEBUG_CFLAGS := -O0 -g -D BLAMMO_ENABLE -fmax-errors=3
ifeq ($(ANDROID_ROOT),)
LDFLAGS      := -lc -pie -lpthread -lrayco 
COV_REPORT   := gcovr -r . --html-details -o coverage.html 
else
LDFLAGS      := -pie -lpthread -lrayco
COV_REPORT   :=
endif

# Append External Libraries
LDFLAGS += -L$(RAYCO_DIR)

# Debug Info
$(info $$INCLUDE is [${INCLUDE}])
$(info $$SOURCES is [${SOURCES}])
$(info $$FOLDERS is [${FOLDERS}])
$(info $$OBJECTS is [${OBJECTS}])
$(info $$CFLAGS is [${CFLAGS}])

# Make Targets
.PHONY: all
all: rayco
all: CFLAGS += -O2 -fomit-frame-pointer
all: $(PROJECT)

.PHONY: debug
debug: rayco_debug
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(BUILDDIR)/$(PROJECT)_debug $(OBJECTS) $(LDFLAGS)

# Pattern match rule for project sources
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(PROJECT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(BUILDDIR)/$(PROJECT) $(OBJECTS) $(LDFLAGS)

.PHONY: test
test: CFLAGS += $(TEST_INCL) $(DEBUG_CFLAGS) -Wno-unused-label
test: SOURCES := $(filter-out $(SRCDIR)/main.c,$(SOURCES))
test: OBJECTS := $(filter-out $(OBJDIR)/main.o,$(OBJECTS))
ifeq ($(ANDROID_ROOT),)
test: CFLAGS += -fprofile-arcs -ftest-coverage
endif
test: rayco_test
test: $(TEST_BINS)
	for testmut in test_*mut; do ./$$testmut; done
	$(COV_REPORT)

test_%.mut : test_%.o $(AUX_OBJS) $(OBJECTS) rayco_debug
	$(CC) $(CFLAGS) -o $@ $< $(AUX_OBJS) $(OBJECTS) $(LDFLAGS)

.PHONY: notabs
notabs:
	find . -type f -regex ".*\.[ch]" -exec sed -i -e "s/\t/    /g" {} +

.PHONY: getcore
getcore:
	rm -f core*
	cp `ls -1t /var/lib/systemd/coredump/core* | head -n 1` core.zst
	unzstd core.zst

.PHONY: clean
clean: rayco_clean
clean:
	rm -f core* *.gcno *.gcda coverage*html coverage.css *.log \
	$(TEST_OBJS) $(TEST_BINS) $(AUX_OBJS) $(OBJDIR)/* \
	$(OBJECTS) $(BUILDDIR)/$(PROJECT) $(BUILDDIR)/$(PROJECT)_debug

# Dependencies
.PHONY: rayco
rayco:
	$(MAKE) -C $(RAYCO_DIR)

.PHONY: rayco_debug
rayco_debug:
	$(MAKE) -C $(RAYCO_DIR) debug

.PHONY: rayco_clean
rayco_clean:
	$(MAKE) -C $(RAYCO_DIR) clean

.PHONY: rayco_test
rayco_test:
	$(MAKE) -C $(RAYCO_DIR) test
