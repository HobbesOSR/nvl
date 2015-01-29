#ifndef QUERY_IOCTL_H
#define QUERY_IOCTL_H
#include <linux/ioctl.h>
#include "gni_priv.h"
#include <linux/sched.h>
#include <linux/pid.h>

 
typedef struct
{
    int cookie, ptag, instance, pid;
} kgni_arg_t;

typedef struct guest_file {
 	struct task_struct *host_task;
	struct file *host_file;
	int cookie;
	int pid;
	int instance;
	int hpid;
	int serverfd;
} guest_file_t;

struct guestvm_struct {
       pid_t guest_pid;
       pid_t  host_pid;
       struct file  *file;
       int h_cookie;
       int h_tag;
       int h_instance; 
       struct list_head list;
};

#define GUESTVM_PID current->pid
#define GUESTVM_TGID current->tgid

struct list_head  guest_list;
struct guestvm_struct *add_guest_vm(pid_t guest_vm_id);
struct guestvm_struct *get_guest_vm(pid_t guest_vm_id);
void remove_guest(struct guestvm_struct *_guest);
 
#define KGNI_GET_VARIABLES _IOR('k', 1, kgni_arg_t *)
#define KGNI_CLR_VARIABLES _IO('k', 2)
#define KGNI_SET_VARIABLES _IOW('k', 3, kgni_arg_t *)
#define KGNI_SEND_BUF _IOWR('k', 4, kgni_arg_t  *)
 
#endif
