#ifndef QUERY_IOCTL_H
#define QUERY_IOCTL_H
#include <linux/ioctl.h>
 
typedef struct
{
    int cookie, ptag, instance, pid;
} kgni_arg_t;
 
#define KGNI_GET_VARIABLES _IOR('k', 1, kgni_arg_t *)
#define KGNI_CLR_VARIABLES _IO('k', 2)
#define KGNI_SET_VARIABLES _IOW('k', 3, kgni_arg_t *)
#define KGNI_SEND_BUF _IOWR('k', 4, kgni_arg_t  *)
 
#endif
