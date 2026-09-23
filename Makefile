# General
CC	:= gcc
CFLAGS	:= -std=c11 -pedantic -Wall -Wextra -MMD -MP -O2
CDFLAGS	:= -g -O0
LDFLAGS	:=
LDLIBS	:=

OBJDIR	:= build
SRCDIR	:= src

# Target
TARGET	:= ifj26c
SRCS	:= $(wildcard $(SRCDIR)/*)
OBJS	:= $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS	:= $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.d)

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

# Target rules
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR):
	mkdir -p $@

-include $(DEPS)
