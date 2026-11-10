.DEFAULT_GOAL := all

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

CC ?= cc
AR ?= ar
ARFLAGS := rcs
CFLAGS ?= -Wall -Wextra -Wpedantic -O3
# CFLAGS += -fsanitize=address -g -fno-omit-frame-pointer
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?= -lm

SRC_DIR := src
BIN_DIR := bin
OBJ_DIR := obj
DEPS_DIR := deps

# Every C file under src, including subdirectories.
SRCS := $(sort $(shell find $(SRC_DIR) -type f -name '*.c'))

# Mirror the source tree under the object directory:
#   src/matrix.c   -> obj/matrix.o
#   src/lib/term.c -> obj/lib/term.o
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Applications are the top-level C files: src/<name>.c
APP_SRCS := $(sort $(wildcard $(SRC_DIR)/*.c))
LIB_SRCS := $(filter-out $(APP_SRCS),$(SRCS))
LIB_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))

PROGRAMS := $(sort $(basename $(notdir $(APP_SRCS))))
BINS := $(addprefix $(BIN_DIR)/,$(PROGRAMS))

# Every library object is put in a static library. The linker pulls in only
# the members each application actually references.
LIBRARY := $(OBJ_DIR)/libtermutils.a

all: $(BINS) $(OBJS)

build: all

objs: $(OBJS)

# Build each object file from its corresponding source file.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

$(LIBRARY): $(LIB_OBJS) | $(OBJ_DIR)
	rm -f $@
	$(AR) $(ARFLAGS) $@ $^

define PROGRAM_template
$(BIN_DIR)/$(1): $(OBJ_DIR)/$(1).o $(LIBRARY) | $(BIN_DIR)
	$$(CC) $$(CFLAGS) $$(LDFLAGS) $$^ $$(LDLIBS) -o $$@

$(1): $(BIN_DIR)/$(1)
endef

$(foreach prog,$(PROGRAMS),$(eval $(call PROGRAM_template,$(prog))))

-include $(DEPS)

install: all
	@for prog in $(PROGRAMS); do \
		install -Dm755 $(BIN_DIR)/$$prog $(DESTDIR)$(BINDIR)/$$prog; \
	done

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) $(DEPS_DIR)

.DELETE_ON_ERROR:

.PHONY: all build objs install clean $(PROGRAMS)
