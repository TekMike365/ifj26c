# General
CC	:= gcc
CFLAGS	:= -std=c11 -pedantic -Wall -Wextra -MMD -MP -O2
CDFLAGS	:= -g -O0 -DDEBUG
LDFLAGS	:=
LDLIBS	:=

OBJDIR	:= build
SRCDIR	:= src
TESTDIR := test

# Target
TARGET	:= ifj26c
SRCS	:= $(wildcard $(SRCDIR)/*.c)
OBJS	:= $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS	:= $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.d)

TEST_TARGET := test_ifj26c
TESTS	:= $(wildcard $(TESTDIR)/*.c)
TEST_OBJS := $(TESTS:$(TESTDIR)/%.c=$(OBJDIR)/tests/%.o)
TEST_DEPS := $(TESTS:$(TESTDIR)/%.c=$(OBJDIR)/tests/%.d)

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

.PHONY: test
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Target rules
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR):
	mkdir -p $@

# Test target rules
$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR)/tests/%.o: $(TESTDIR)/%.c | $(OBJDIR)/tests
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/tests:
	mkdir -p $@

-include $(DEPS) $(TEST_DEPS)
