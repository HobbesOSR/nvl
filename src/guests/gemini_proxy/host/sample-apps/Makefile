#
# Copyright 2011 Cray Inc.
#
CC=gcc
CFLAGS= -g -c -Wall -DCRAY_CONFIG_GHAL_GEMINI  -I. -I../include -I../../include  -I/scratch1/smukher/HEADERS/gni.headers/gni-headers-2.1-1.0400.4156.6.1.gem-include 
LDFLAGS= -g -static -L. -lugni -lalpslli    -lalpslli -lugni
SOURCES=gni_trial.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=gni_trial1

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ -lalpslli -lugni -lpthread

.c.o:
	$(CC) $(CFLAGS) $< -o $@
