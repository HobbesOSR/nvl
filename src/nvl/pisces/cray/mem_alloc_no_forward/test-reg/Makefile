#
# Copyright 2011 Cray Inc.
#
CC=gcc -v
CFLAGS= -g -fPIC  -c -Wall -DCRAY_CONFIG_GHAL_GEMINI  -I. -I/usr/include -I../../include  -I/opt/cray/alps/default/include -I/opt/cray/gni-headers/default/include 
LDFLAGS= -g -fPIC  -L/opt/cray/alps/default/lib64 -L/opt/cray/ugni/default/lib64/  -L/opt/cray/alps/default/lib64  -lugni -lalpslli    -lalpslli -lugni
SOURCES=mem_seg_reg_validate.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mem_reg

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ -lalpslli -lugni -lpthread

.c.o:
	$(CC) $(CFLAGS) $< -o $@
