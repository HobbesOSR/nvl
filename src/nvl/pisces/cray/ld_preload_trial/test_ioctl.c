  // test_ioctl.c simple test for ioctl             
  // cc -O2 -o test_ioctl test_ioctl.c              
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "gni_priv.h"
#include "gni_pub.h"

#define KGNI_DEVICE "/dev/kgni0"

int
main ()
{
  int fd;
  int ret;
  int i;
  char arg_data[200];

  fd = open (KGNI_DEVICE, O_RDWR);
  if (fd < 0)
    {
      printf (" failed to open %s \n", KGNI_DEVICE);
      exit (1);
    }
  ret = ioctl (fd, GNI_IOC_NIC_SETATTR, &arg_data);
  if (ret < 0)
    {
      printf (" failed to GETD on %s\n", KGNI_DEVICE);
      exit (1);
    }
  printf ("initial data ...\n");
  for (i = 0; i < 8; i++)
    {
      printf ("data[%d] %x     data[%d] %x\n",
	      i, arg_data[i], i + 8, arg_data[i + 8]);
    }
  close (fd);
  return 0;
}
