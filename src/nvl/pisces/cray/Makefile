CC = gcc
#GNI static and Dynamic libs and libalps path

GNI_HEADER=/scratch1/smukher/HEADERS/gni.headers/gni-headers-2.1-1.0400.4156.6.1.gem-include
# Base directory where xpmem, palacios etc resides
NVL_BASE=/scratch1/smukher/libhoobes-dev-may26

XPMEM_PATH      = $(NVL_BASE)/xpmem
WHITEDB_PATH    = $(NVL_BASE)/hobbes/whitedb-0.7.3
HOBBES_PATH     = $(NVL_BASE)/hobbes/libhobbes
GNI_HEADER_PATH =./include
UGNI_LIB_PATH   = /opt/cray/ugni/default/lib64
STATIC_MATH = /usr/lib64

SOURCES = xpmem_chnl_client.c \
          xpmem_chnl_serv.c 

INCDIRS = -I$(GNI_HEADER_PATH) -I$(HOBBES_PATH) -I$(WHITEDB_PATH)/Db -I$(XPMEM_PATH)/include
LIBDIRS = -L$(UGNI_LIB_PATH) -L$(STATIC_MATH) -L$(HOBBES_PATH) -L$(WHITEDB_PATH)/Db/.libs -L/opt/cray/xt-tools/lgdb/1.4/lib/alps -L/opt/cray/ugni/default/lib64
LIBS    =  -lhobbes -lalpslli -lDb -lugni -lm

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
