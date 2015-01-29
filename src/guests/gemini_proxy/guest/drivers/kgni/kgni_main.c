#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/net.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/major.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
 
#include "kgni.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1

struct list_head  guestvm_list; 
static dev_t dev;
static struct cdev c_dev;
static int cookie = 1, ptag = 3, instance = 10;
uint32_t kgni_store_version = 0x004400b1;  /* hardcoded in GNI header gni_priv.h */

struct guestvm_struct *add_guest_vm(pid_t process_id)
{
        struct guestvm_struct *guestvm;

        guestvm = kmalloc(sizeof(struct guestvm_struct), GFP_KERNEL);
        if (!guestvm) {
                printk(KERN_INFO "Error: could not allocate memory\n");
                return NULL;
        }
        guestvm->guest_pid = process_id;
        list_add(&guestvm->list, &guestvm_list);
        return guestvm; 
}

void remove_guest_vm(struct guestvm_struct *gvm)
{
        struct guestvm_struct *guestvm = NULL, *tmp;

        list_for_each_entry_safe(guestvm, tmp, &guestvm_list, list) {
                if (guestvm == gvm ) {
                        list_del(&guestvm->list);
                        kfree(guestvm);
                }
        }
}

struct guestvm_struct *get_guest_vm(pid_t process_id)
{
        struct guestvm_struct *gvm = NULL;

        list_for_each_entry(gvm, &guestvm_list, list) {

                if (gvm->guest_pid == process_id) {
                        return gvm;
                }
        }

        return NULL;
}



static __inline__ unsigned long hcall2(unsigned long result,  unsigned long hcall_id, unsigned long param1, void * param2)
{
                __asm__ volatile ("movq %1, %%rax;"
                "movq %2, %%rcx;"
                "movq %3, %%rbx;"
                ".byte 0x0f, 0x01, 0xd9;"
                "movq %%rax, %0;"
                : "=A" (result)
                : "g" (hcall_id), "g" (param1), "g" (param2)
                : "%rax","%rbx","%rcx");
        return result;
}

static int kgni_open(struct inode *i, struct file *f)
{
        guest_file_t     *file_inst;
        int             err;
	int *gpid;
	unsigned long hcall=0xc0c1;
        int trial =0; 


        file_inst = kzalloc(sizeof(guest_file_t), GFP_KERNEL);
        if (file_inst == NULL) {
                return (-ENOMEM);
        }
	*gpid = current->pid;
	unsigned long  result =  hcall2(trial, hcall, &gpid, &file_inst);
        if (err) {
                goto err_hcall_open;
        }

         printk(KERN_INFO "called actual open from  guest stub driver to kgni_opne on host\n");
        f->private_data = file_inst;
        return 0;
        err_hcall_open:
                kfree(file_inst);
        return err;

    return 0;
}
static int kgni_close(struct inode *i, struct file *f)
{
    return 0;
}
/* for sysfs */
static ssize_t version_show(struct class *class, struct class_attribute *attr, char *buf)
{
        ssize_t size = scnprintf(buf, PAGE_SIZE, "0x%08x\n", kgni_store_version);

        return size;
}

static ssize_t version_store(struct class *class, struct class_attribute *attr, const char * buf, size_t len)
{
        sscanf(buf, "%u", &kgni_store_version);

        return len;
}


static int kgni_nic_type = 0;

static ssize_t show_nic_type(struct class *class, struct class_attribute *attr, char *buf)
{
        ssize_t size = scnprintf(buf, PAGE_SIZE, "%d\n", kgni_nic_type);

        return size;
}

static struct class_attribute gni_class_attrs[] = {
__ATTR(version, S_IRUGO | S_IWUSR , version_show, version_store), 
__ATTR(nic_type, S_IRUGO , show_nic_type, NULL)
};

static struct class gni_class =
{
	.name = "gni",
	.owner = THIS_MODULE,
	.class_attrs = gni_class_attrs
};

static __inline__ unsigned long hcall3(unsigned long result,  unsigned long hcall_id, unsigned long param1, void * param2, unsigned long param3, void * param4)
{
                __asm__ volatile ("movq %1, %%rax;"
                "movq %2, %%rcx;"
                "movq %3, %%rbx;"
                "movq %4, %%rdx;"
                "movq %5, %%rsi;"
                ".byte 0x0f, 0x01, 0xd9;"
                "movq %%rax, %0;"
                : "=A" (result)
                : "g" (hcall_id), "g" (param1), "g" (param2), "g" (param3), "g" (param4)
                : "%rax","%rbx","%rcx","%rsi");
        return result;
}

static long kgni_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
        kgni_arg_t q, *q_dest;
	long retval = 0;
        unsigned long hcall=0xc0c2;   /* handle cdm_attach case os_debug.c*/
        unsigned long    trial=0, result;
        unsigned long buf_is_va=1;
	pid_t	host_pid, mypid;
	struct guestvm_struct *gvm;

 
    switch (cmd)
    {
        case KGNI_GET_VARIABLES:
            q.cookie = cookie;
            q.ptag = ptag;
            q.instance = instance;
            if (copy_to_user((kgni_arg_t *)arg, &q, sizeof(kgni_arg_t)))
            {
                return -EACCES;
            }
            break;
        case KGNI_CLR_VARIABLES:
            cookie = 0;
            ptag = 0;
            instance = 0;
            break;
        case KGNI_SET_VARIABLES:
            if (copy_from_user(&q, (kgni_arg_t *)arg, sizeof(kgni_arg_t)))
            {
                return -EACCES;
            }
            cookie = q.cookie;
            ptag = q.ptag;
            instance = q.instance;
	    host_pid = q.pid;
	    mypid = current->pid;
	    gvm = get_guest_vm(mypid);
	    gvm->guest_pid = mypid;
	    gvm->host_pid = host_pid;
	    gvm->h_tag =ptag; 
	    gvm->h_cookie = cookie;
	    gvm->h_instance = instance;
            break;
	case KGNI_SEND_BUF:
		printk(KERN_INFO "Shyamali simple arg pass ioctl from guest %p\n", &q_dest);
/*
                void *kbuf_src = kmalloc(len, GFP_KERNEL);
                void *kbuf_dest = kmalloc(len, GFP_KERNEL);
*/
                if (copy_from_user(&q, (kgni_arg_t  *)arg, sizeof(kgni_arg_t))) {
                        retval = -1;
                        break;
                }
		q_dest = kmalloc(sizeof(kgni_arg_t), GFP_KERNEL);
		result =  hcall3(trial, hcall, sizeof(kgni_arg_t), &q, buf_is_va, (void *)q_dest);
                printk(KERN_INFO "called hypercall from guest stub driver\n");
                break;
	case  GNI_IOC_NIC_SETATTR: 
		printk(KERN_INFO "guest stub calling GNI_NIC_SET_ATTR command %d\n", cmd);
		gni_nic_setattr_args_t   nic_attr, resp;
		if (copy_from_user(&nic_attr, (void *)arg, sizeof(nic_attr))) {
			retval = -1;
		}
		result =  hcall3(trial, hcall, sizeof(nic_attr), &nic_attr, cmd, &resp);
                printk(KERN_INFO "called actual nic set attr from  guest stub driver\n");

        default:
            return -EINVAL;
    }
 
    return 0;
}

static struct file_operations kgni_fops =
{
    .owner = THIS_MODULE,
    .open = kgni_open,
    .release = kgni_close,
    .unlocked_ioctl = kgni_ioctl
};
 
static int __init kgni_init(void)
{
    int ret, err;
    struct device *dev_ret;
 
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "kgni")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &kgni_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
    class_register(&gni_class);
    if (IS_ERR(dev_ret = device_create(&gni_class, NULL, dev, NULL, "kgni0")))
    {
        class_unregister(&gni_class);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
    INIT_LIST_HEAD(&guestvm_list);

 
    return 0;
}

 
static void __exit kgni_exit(void)
{
   /* device_destroy(cl, dev);
    class_remove_file(cl, &class_attr_nic_type);
    class_remove_file(cl, &class_attr_version);
    class_destroy(cl); */
     device_destroy(&gni_class, dev);
    class_unregister(&gni_class);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}
 
module_init(kgni_init);
module_exit(kgni_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shyamali Mukherjee <email_at_smukher-sandia_dot_gov>");
MODULE_DESCRIPTION("kgni ioctl() Char Driver");
