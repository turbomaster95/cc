PHONY :=

CC = gcc
COPTS =
CFLAGS = $(COPTS) -std=c99 -Wall -Wextra -Iinclude -Iobj -Wno-unused
CFLAGS += -D_POSIX_C_SOURCE=200809L -MMD -MP
LIBS = obj/libnu.a
LEX = flex
YACC = bison -y -Wno-other -Wno-yacc -Wno-conflicts-sr

ARCH ?= x86
TARGET = c99$(ARCH)
CPPTARG = cppc

OBJDIR = obj
$(shell mkdir -p $(OBJDIR))

SRCS = src/main.c src/walker.c src/code.c src/parser.c src/symb.c
SRCS += targets/$(ARCH)/codegen.c

OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))
OBJS += $(OBJDIR)/lex.yy.o

DEPS = $(OBJS:.o=.d)

VPATH = src targets/$(ARCH)

all: $(TARGET) $(CPPTARG)

$(OBJS): | $(OBJDIR)/libnu.a include/nu.h

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/lex.yy.c: src/c99.l include/nu.h
	$(LEX) -o $(OBJDIR)/lex.yy.c src/c99.l

$(OBJDIR)/lex.yy.o: $(OBJDIR)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) FORCE
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(CPPTARG): src/pp/*.c FORCE
	$(CC) $(CFLAGS) -MF $(OBJDIR)/cppc.d -o $@ src/pp/*.c $(LIBS)

CLEANF += $(OBJDIR)/libnu.a
CLEAND += libnu/build
CLEANF += libnu/configure
$(OBJDIR)/libnu.a:
	@mkdir -p $(OBJDIR)
	(cd libnu && ./compile && cp include/nu.h ../include && cp include/nus.h ../include && cp build/libnu.a ../$(OBJDIR))

CLEANF += include/nu.h
include/nu.h: $(OBJDIR)/libnu.a

PHONY += clean
clean:
	rm -f $(TARGET) $(CPPTARG) $(OBJS) $(DEPS) $(CLEANF)
	rm -rf $(CLEAND)
	rm -rf $(OBJDIR)

PHONY += FORCE
FORCE:

.PHONY: $(PHONY)

# Include dependency rules safely
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
