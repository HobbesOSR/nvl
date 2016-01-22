#
# Copyright 2011 Cray Inc.
#
# Some of the gni header files are copied here locally, which should not be needed later.
# but for NOW this static link needs it.


WHITEDB=/home/smukher/hobbes/whitedb-0.7.3
HOBBS=/home/smukher/hobbes/libhobbes
XPMEM=/home/smukher/xpmem
UGNI=/opt/cray/ugni/default/lib64
# Define where static client *.o is located which intercept libc calls open, ioctl
INTERCEPT_CLIENT_OBJ=/home/smukher/nvl/src/nvl/pisces/cray/static-link

CC=gcc -v
GNI_HEADER=/home/smukher/gni-headers/cray-gni-headers-4.0/include
CC=gcc -v
CFLAGS= -g -c -fPIC   -Wall -DCRAY_CONFIG_GHAL_GEMINI  -I. -I/usr/include -I../../include  -I/opt/cray/alps/default/include  -I/opt/cray/pmi/default/include -I$(GNI_HEADER) -I$(GNI_HEADER)/gemini
LDFLAGS= -g -fPIC -static  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64/  -L/opt/cray/alps/default/lib64  -L$(HOBBS) -L$(XPMEM) -L$(WHITEDB) -lugni -lalpslli    -lalpslli -lugni -lhobbes 
SOURCES=ping_pong.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=ping_pong

libs :=         $(HOBBS)/libhobbes.a \
                $(UGNI)/libugni.a \
                $(WHITEDB)/Db/.libs/libDb.a \
                /usr/lib64/libm.a

all: $(SOURCES) $(EXECUTABLE)
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

$(EXECUTABLE): $(OBJECTS) 
	$(CC)  $(LDFLAGS) $(OBJECTS) -Wl,-wrap,open -Wl,-wrap=open -Wl,-wrap=ioctl $(INTERCEPT_CLIENT_OBJ)/xpmem_chnl_client.o  -o $@ $(libs) -lalpslli -lugni -lpthread  -lugni -lalpslli 

clean:
	rm -f *.o  ping_pong
