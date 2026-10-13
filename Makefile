# General
CC	:= gcc
CFLAGS	:= -std=c11 -pedantic -Wall -Wextra -MMD -MP -O2
CDFLAGS	:= -g -O0 -DDEBUG
LDFLAGS	:=
LDLIBS	:=

OBJDIR	:= build
OBJDIR_SRC := $(OBJDIR)/src
OBJDIR_TEST := $(OBJDIR)/test

SRCDIR	:= src
TESTDIR := test

# Target
TARGET	:= ifj26c
SRCS	:= $(wildcard $(SRCDIR)/*.c)
OBJS	:= $(SRCS:$(SRCDIR)/%.c=$(OBJDIR_SRC)/%.o)
DEPS	:= $(SRCS:$(SRCDIR)/%.c=$(OBJDIR_SRC)/%.d)

LIB_SRCS := $(filter-out $(SRCDIR)/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:$(SRCDIR)/%.c=$(OBJDIR_SRC)/%.o)

TESTER := $(TESTDIR)/tester.c
TESTER_OBJ := $(OBJDIR_TEST)/tester.o
TESTER_DEP := $(OBJDIR_TEST)/tester.d

TESTS	:= $(wildcard $(TESTDIR)/test_*.c)
TEST_OBJS := $(TESTS:$(TESTDIR)/%.c=$(OBJDIR_TEST)/%.o)
TEST_DEPS := $(TESTS:$(TESTDIR)/%.c=$(OBJDIR_TEST)/%.d)
TEST_TARGETS := $(TESTS:$(TESTDIR)/%.c=$(OBJDIR)/%)

# General rules
.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: debug
debug: CFLAGS += $(CDFLAGS)
debug: $(TARGET)

.PHONY: format
format: $(shell find . -regex ".*\.[ch]")
	clang-format -i $^

.PHONY: build-tests
build-tests: $(TEST_TARGETS)

.PHONY: test
test: build-tests
	@for test in $(TEST_TARGETS); do ./$$test; done


# Target rules
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR_SRC)/%.o: $(SRCDIR)/%.c | $(OBJDIR_SRC)
	$(CC) -c $(CFLAGS) $< -o $@	


# Test target rules

# Static pattern rule to 
# prevent make from auto-deleting test objs
$(OBJDIR)/%: $(OBJDIR_TEST)/%.o $(TESTER_OBJ) $(LIB_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)


$(OBJDIR_TEST)/%.o: $(TESTDIR)/%.c | $(OBJDIR_TEST)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR_SRC) $(OBJDIR_TEST):
	mkdir -p $@

-include $(DEPS) $(TEST_DEPS) $(TESTER_DEP)
