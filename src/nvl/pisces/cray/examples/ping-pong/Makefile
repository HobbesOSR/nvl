#
# Copyright 2011 Cray Inc.
#
GNI_HEADER=/lscratch2/smukher/new-prgenv/kgni-header-driver/cray-gni-headers-4.0/include
CC=gcc -v
CFLAGS= -g -fPIC  -c -Wall -DCRAY_CONFIG_GHAL_GEMINI  -I. -I/usr/include -I../../include  -I/opt/cray/alps/default/include -I/opt/cray/gni-headers/default/include -I/opt/cray/pmi/default/include -I$(GNI_HEADER) -I$(GNI_HEADER)/gemini
LDFLAGS= -g -fPIC  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64/  -L/opt/cray/alps/default/lib64  -lugni -lalpslli    -lalpslli -lugni
SOURCES=ping_pong.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=ping_pong

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ -lalpslli -lugni -lpthread

.c.o:
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f *.o  ping_pong
