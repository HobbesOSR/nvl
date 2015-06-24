#define _GNU_SOURCE
#include <dlfcn.h>
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

typedef int (*next_ioctl_f_type) (int fd, int request, void *data);


int
ioctl (int fd, int request, void *data)
{
  char *msg;

  fprintf (stderr, "ioctl : wrapping ioctl\n");
  fflush (stderr);
  next_ioctl_f_type next_ioctl;
  next_ioctl = dlsym (RTLD_NEXT, "ioctl");
  fprintf (stderr, "next_ioctl = %p\n", next_ioctl);
  fflush (stderr);
  if ((msg = dlerror ()) != NULL)
    {
      fprintf (stderr, "ioctl: dlopen failed : %s\n", msg);
      fflush (stderr);
      exit (1);
    }
  else
    fprintf (stderr, "ioctl: wrapping done\n");
  fflush (stderr);
  if (request == 1)
    {				/* SCSI_IOCTL_SEND_COMMAND ? */
      /* call back trace */
      fprintf (stderr, "SCSI_IOCTL_SEND_COMMAND ioctl\n");
      fflush (stderr);
      show_stackframe ();
    }
  return next_ioctl (fd, request, data);
}
