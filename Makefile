PHONY := 

CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = 
LEX = flex
YACC = bison -y

TARGET = c99

SRCS = c99.c
OBJS = $(SRCS:.c=.o)

PHONY += all
all: $(TARGET)

$(TARGET): libnu.a nu.h $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) libnu.a $(LIBS)

libnu.a: libnu
	(cd libnu && ./compile && cp include/nu.h ../ && cp build/libnu.a ../)

nu.h: libnu.a

PHONY += clean
clean:
	rm -f $(TARGET) $(OBJS) libnu.a nu.h

.PHONY: $(PHONY)
