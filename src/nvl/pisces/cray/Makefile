#
# Copyright 2011 Cray Inc.
#

CC=gcc
#GNI_HEADER=/scratch1/smukher/HEADERS/gni.headers/gni-headers-2.1-1.0400.4156.6.1.gem-include
GNI_HEADER=./include
WHITEDB=/scratch1/smukher/libhoobes-dev-may26/hobbes/whitedb-0.7.3
HOBBS=/scratch1/smukher/libhoobes-dev-may26/hobbes/libhobbes
XPMEM=/scratch1/smukher/libhoobes-dev-may26/xpmem
STATIC_UGNI=/scratch1/smukher/FAKE_GNI_REPO/host_code/apps

SOURCES=xpmem_chnl_serv.c 


CFLAGS= -g -c -Wall -static -DCRAY_CONFIG_GHAL_GEMINI 
INCDIR= -I$(GNI_HEADER) -I$(HOBBS) -I$(WHITEDB)/Db  -I$(XPMEM)/include
LDFLAGS= -g  -static 
LIBDIR= -L$(STATIC_UGNI) -L/opt/cray/xt-tools/lgdb/1.4/lib/alps -L/opt/cray/ugni/default/lib64 -L$(HOBBS) -L$(WHITEDB)/Db/.libs -lugni -lalpslli -lDb -lm -lhobbes

libs :=		$(HOBBS)/libhobbes.a \
		$(STATIC_UGNI)/libugni.a \
		$(WHITEDB)/Db/.libs/libDb.a \
		/usr/lib64/libm.a 

execs:=		xpmem_chnl_serv xpmem_chnl_client.so
PGMS    = $(SOURCES:.c=)
OBJS=$(SOURCES:.c=.o)

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
	$(CC) $(CFLAGS) $(INCDIR) -c $<  -o $@

% : %.o 
	$(call build,CC,$(CC)   $(LDFLAGS) $(LIBDIR) $<  $(libs) -lm -lhobbes -lalpslli -lDb -lugni -o $@)

xpmem_chnl_client.so :  xpmem_chnl_client.c
	$(call build,CC,$(CC) -fPIC -shared -DCRAY_CONFIG_GHAL_GEMINI $(INCDIR) $(LIBDIR) -ldl  -lm -lhobbes -lalpslli -lDb -lugni -o $@ $<)

clean:
	rm -f core $(wildcard  $(execs)) *.o *.so
