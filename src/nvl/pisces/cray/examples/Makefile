#
# Copyright 2011 Cray Inc.
#
CC=gcc -v
CFLAGS= -g -fPIC  -c -Wall -DCRAY_CONFIG_GHAL_GEMINI  -I. -I/usr/include -I../../include  -I/scratch1/smukher/HEADERS/gni.headers/gni-headers-2.1-1.0400.4156.6.1.gem-include 
LDFLAGS= -g -fPIC  -L. -L/opt/cray/ugni/default/lib64/  -L/opt/cray/xt-tools/lgdb/default/lib/alps/  -lugni -lalpslli    -lalpslli -lugni
SOURCES=mem_seg_reg_validate.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mem_reg

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ -lalpslli -lugni -lpthread

.c.o:
	$(CC) $(CFLAGS) $< -o $@
