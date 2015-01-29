#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sched.h>
 
#include "kgni.h"
#include "gni_priv.h"
#include "gni_pub.h"
 
void get_vars(int fd)
{
    kgni_arg_t q;
 
    if (ioctl(fd, KGNI_GET_VARIABLES, &q) == -1)
    {
        perror("kgni ioctl get");
    }
    else
    {
        printf("cookie : %d\n", q.cookie);
        printf("ptag: %d\n", q.ptag);
        printf("instance    : %d\n", q.instance);
    }
}
void clr_vars(int fd)
{
    if (ioctl(fd, KGNI_CLR_VARIABLES) == -1)
    {
        perror("kgni ioctl clr cookies");
    }
}
void send_ioctl(fd)
{
	int rc;
	gni_nic_setattr_args_t nic_attrs;
	nic_attrs.rank = 10;
        nic_attrs.ptag = 1000;
        nic_attrs.cookie = 2000;
        nic_attrs.modes = 0;
	rc = ioctl(fd, GNI_IOC_NIC_SETATTR, &nic_attrs);
}

void send_buf(fd)
{
	kgni_arg_t q;
	
 	q.cookie = 3000;
	q.ptag = 5000;
	q.instance = 20000;
	
	if (ioctl(fd, KGNI_SEND_BUF, &q) == -1)
	{
		perror("kgni ioctl send buf");
	}
	else {
		printf("sucessfully sent buffer \n");
	}
}	
void set_vars(int fd)
{
    int v;
    kgni_arg_t q;
 
    printf("Enter cookie: ");
    scanf("%d", &v);
    getchar();
    q.cookie = v;
    printf("Enter ptag: ");
    scanf("%d", &v);
    getchar();
    q.ptag = v;
    printf("Enter instance: ");
    scanf("%d", &v);
    getchar();
    q.instance = v;
    printf("Enter host pid: ");
    scanf("%d", &v);
    getchar();
    q.instance = v;
 
    if (ioctl(fd, KGNI_SET_VARIABLES, &q) == -1)
    {
        perror("kgni ioctl set cookies");
    }
}
 
int main(int argc, char *argv[])
{
    char *file_name = "/dev/kgni0";
    int fd;
    enum
    {
        e_get,
        e_clr,
        e_set,
        b_set
    } option;
 
    if (argc == 1)
    {
        option = e_get;
    }
    else if (argc == 2)
    {
        if (strcmp(argv[1], "-g") == 0)
        {
            option = e_get;
        }
        else if (strcmp(argv[1], "-c") == 0)
        {
            option = e_clr;
        }
        else if (strcmp(argv[1], "-s") == 0)
        {
            option = e_set;
        }
        else if (strcmp(argv[1], "-b") == 0)
        {
            option = b_set;
        }
        else
        {
            fprintf(stderr, "Usage: %s [-g | -c | -s | -b]\n", argv[0]);
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s [-g | -c | -s | -b]\n", argv[0]);
        return 1;
    }
    fd = open(file_name, O_RDWR);
    if (fd == -1)
    {
        perror("/dev/kgni0 open");
        return 2;
    }
 
    switch (option)
    {
        case e_get:
            get_vars(fd);
            break;
        case e_clr:
            clr_vars(fd);
            break;
        case e_set:
            set_vars(fd);
            break;
        case b_set:
            send_buf(fd);
	    send_ioctl(fd);
            break;
        default:
            break;
    }
 
    close (fd);
 
    return 0;
}
