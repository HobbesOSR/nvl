/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/* User level test procedures */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <dlfcn.h>		/* For dlsym */




typedef int (*orig_open_f_type) (const char *pathname, int flags);

int
open (const char *pathname, int flags, ...)
{
  /* Some evil injected code goes here. */
  printf ("The victim used open(...) to access '%s'!!!\n", pathname);
  orig_open_f_type orig_open;
  orig_open = (orig_open_f_type) dlsym (RTLD_NEXT, "open");
  return orig_open (pathname, flags);
}

int
kgni_open (const char *pathname, int flags)
{
}

int
main (int argc, char *argv[])
{
  return 0;

}
