PHONY :=

CC = gcc
COPTS =
CFLAGS = $(COPTS) -Wall -Wextra -Iinclude -Iobj -Wno-unused 
LIBS = obj/libnu.a
LEX = flex
YACC = bison -y -Wno-other -Wno-yacc -Wno-conflicts-sr

ARCH ?= x86
TARGET = c99$(ARCH)
CPPTARG = cppc

OBJDIR = obj
$(shell mkdir -p $(OBJDIR))

SRCS = src/main.c src/walker.c src/code.c
SRCS += targets/$(ARCH)/codegen.c

OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))
OBJS += $(OBJDIR)/y.tab.o $(OBJDIR)/lex.yy.o

DEPS = $(OBJS:.o=.d)

all: $(TARGET) $(CPPTARG)

$(OBJS): $(OBJDIR)/libnu.a include/nu.h

%.d: %.c
	@set -e; \
	$(CC) -M $(CFLAGS) $< | sed 's|\($*\)\.o[ :]*|$(OBJDIR)/\1.o $(OBJDIR)/\1.d : |g' > $@; \
	[ -s $@ ] || rm -f $@

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/%.o: targets/$(ARCH)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/y.tab.c $(OBJDIR)/y.tab.h: src/c99.y include/nu.h
	$(YACC) -d -o $(OBJDIR)/y.tab.c src/c99.y

$(OBJDIR)/lex.yy.c: src/c99.l $(OBJDIR)/y.tab.h include/nu.h
	$(LEX) -o $(OBJDIR)/lex.yy.c src/c99.l

$(OBJDIR)/y.tab.o: $(OBJDIR)/y.tab.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/lex.yy.o: $(OBJDIR)/lex.yy.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJS) FORCE
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(CPPTARG): src/pp/*.c FORCE
	$(CC) $(CFLAGS) -o $@ src/pp/*.c $(LIBS)

CLEANF += $(OBJDIR)/libnu.a
CLEAND += libnu/build
CLEANF += libnu/configure
$(OBJDIR)/libnu.a:
	@mkdir -p $(OBJDIR)
	(cd libnu && ./compile && cp include/nu.h ../include && cp build/libnu.a ../$(OBJDIR))

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

# Include dependency rules
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
