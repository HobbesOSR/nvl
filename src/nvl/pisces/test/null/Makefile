#
#

SHELL   = /bin/sh
CC=gcc

SOURCES=null.c

OBJS=$(SOURCES:.c=.o)


all: $(SOURCES) $(OBJS) $(execs)

%.o : %.c
	$(CC) $(CFLAGS) $(INCDIR) $(PMI_CFLAGS) $(UGNI_CFLAGS) -c $<  -o $@

% : %.o
	$(call build,CC,$(CC)   $(LDFLAGS) -o $@)

clean:
	rm -f core $(wildcard  $(execs)) *.o *.so
