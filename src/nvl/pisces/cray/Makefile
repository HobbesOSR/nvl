CC = gcc
XPMEM_PATH      ?= ../xpmem
WHITEDB_PATH    ?= ../hobbes/whitedb-0.7.3
HOBBES_PATH     ?= ../hobbes/libhobbes
GNI_HEADER_PATH ?= ./include
UGNI_LIB_PATH   ?= ./lib

SOURCES = xpmem_chnl_client.c \
          xpmem_chnl_serv.c 

INCDIRS = -I$(GNI_HEADER_PATH) -I$(HOBBES_PATH) -I$(WHITEDB_PATH)/Db -I$(XPMEM_PATH)/include
LIBDIRS = -L$(UGNI_LIB_PATH) -L$(HOBBES_PATH) -L$(WHITEDB_PATH)/Db/.libs -L/opt/cray/xt-tools/lgdb/1.4/lib/alps -L/opt/cray/ugni/default/lib64
LIBS    = -lm -lhobbes -lalpslli -lDb -lugni

CFLAGS  = -g -c -Wall -static -DCRAY_CONFIG_GHAL_GEMINI $(INCDIRS)
LDFLAGS = -g -static $(LIBDIRS) $(LIBS)

execs   = xpmem_chnl_client xpmem_chnl_serv
PGMS    = $(SOURCES:.c=)
OBJS    = $(SOURCES:.c=.o)


build = \
	@if [ -z "$V" ]; then \
		echo '   [$1]     $@'; \
		$2; \
	else \
		echo '$2'; \
		$2; \
	fi


all: $(SOURCES) $(OBJS) $(execs) 


%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@


% : %.o 
	$(call build,CC,$(CC) $(LDFLAGS) $< $(LIBS) -o $@)


clean:
	rm -f core $(wildcard  $(execs)) *.o
