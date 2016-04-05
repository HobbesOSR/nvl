#
#

CC=gcc

OBJS=null.o
execs=null


%.o : %.c
	$(CC) $(CFLAGS)  -c $<  -o $@

null : $(OBJS)
	gcc $(LDFLAGS) -o $@ $^

clean:
	rm -f core $(wildcard  $(execs)) *.o *.so
