#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sched.h>

#define SEND_RECV_ARGS 10
int main (int argc, char *argv[])
{


    int rc;
    int retval;
    int kgni_fd;
    unsigned long len = 8000 * 3000;
     char *cmd_arg = (char *)malloc(len);

    printf("open /dev/kgni0\n");

    kgni_fd = open("/dev/kgni0", O_RDONLY);


    if (kgni_fd == -1) {
	perror("Could not open input file");
        printf("Error opening kgni device: \n");
        return -1;
    }

    int err = ioctl(kgni_fd, SEND_RECV_ARGS, &cmd_arg);

    if (err < 0) {
        printf("Error write iocrl to kgni\n");
        return -1;
    }

        printf("succes sending ioctl from guest app to guest  kernel\n");
    close(kgni_fd);

}
