#
#

SHELL   = /bin/sh
CC=gcc
#GNI_HEADER=/opt/cray/gni-headers/default/include
GNI_HEADER=/lscratch2/smukher/new-prgenv/kgni-header-driver/cray-gni-headers-4.0/include
WHITEDB=/lscratch2/smukher/new-hobbes-LANL-cray/hobbes/whitedb-0.7.3
HOBBS=/lscratch2/smukher/new-hobbes-LANL-cray/hobbes/libhobbes
XPMEM=/lscratch2/smukher/new-hobbes-LANL-cray/xpmem
UGNI=/opt/cray/ugni/default/lib64
ALPS=/opt/cray/alps/default
PMI=/opt/cray/pmi/default

SOURCES=xpmem_chnl_serv.c 

PMI_CFLAGS = $(shell pkg-config --cflags cray-pmi)
PMI_LIBS = $(shell pkg-config --libs cray-pmi)
UGNI_CFLAGS = $(shell pkg-config --cflags cray-ugni)
UGNI_LIBS = $(shell pkg-config --libs cray-ugni)

CFLAGS= -g  -Wall -fPIC -DCRAY_CONFIG_GHAL_GEMINI -Wunused-variable -Wunused-variable
INCDIR= -I$(GNI_HEADER) -I$(ALPS)/include -I$(HOBBS) -I$(WHITEDB)/Db  -I$(XPMEM)/include -I$(PMI)/include
LDFLAGS= -g  
LIBDIR=  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64 -L$(HOBBS) -L$(WHITEDB)/Db/.libs -lugni -lalpslli -lDb -lm -lhobbes

libs :=		$(HOBBS)/libhobbes.a \
		$(UGNI)/libugni.a \
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
	$(CC) $(CFLAGS) $(INCDIR) $(PMI_CFLAGS) $(UGNI_CFLAGS) -c $<  -o $@

% : %.o 
	$(call build,CC,$(CC)   $(LDFLAGS) $(LIBDIR) $(UGNI_LIBS) $(PMI_LIBS) $<  $(libs) -lm -lhobbes -lalpslli -lDb -lugni -lpmi -o $@)

xpmem_chnl_client.so :  xpmem_chnl_client.c
	$(call build,CC,$(CC) $(CFLAGS)   -fPIC -shared -DCRAY_CONFIG_GHAL_GEMINI $(INCDIR) $(LIBDIR)  -ldl  -lm -lhobbes -lalpslli -lDb -lugni -o $@ $< $(libs))

clean:
	rm -f core $(wildcard  $(execs)) *.o *.so
