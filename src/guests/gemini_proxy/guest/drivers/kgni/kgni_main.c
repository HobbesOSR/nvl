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
 
#include "kgni.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static int cookie = 1, ptag = 3, instance = 10;
 
static int kgni_open(struct inode *i, struct file *f)
{
    return 0;
}
static int kgni_close(struct inode *i, struct file *f)
{
    return 0;
}

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
    kgni_arg_t q, q_dest;
	long retval = 0;
        unsigned long hcall=0xc0c2;   /* handle cdm_attach case os_debug.c*/
        unsigned long    trial=0;
        unsigned long buf_is_va=1;

 
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
            break;
	case KGNI_SEND_BUF:
		printk(KERN_INFO, "Shyamali simple arg pass ioctl from guest\n");
/*
                void *kbuf_src = kmalloc(len, GFP_KERNEL);
                void *kbuf_dest = kmalloc(len, GFP_KERNEL);
*/
                if (copy_from_user(&q, (kgni_arg_t  *)arg, sizeof(kgni_arg_t))) {
                        retval = -1;
                        break;
                }
		unsigned long result =  hcall3(trial, hcall, sizeof(kgni_arg_t), &q, buf_is_va, &q_dest);
                printk(KERN_INFO, "called hypercall from guest stub driver\n");
                break;
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
    int ret;
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
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "kgni0")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
 
    return 0;
}
 
static void __exit kgni_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}
 
module_init(kgni_init);
module_exit(kgni_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shyamali Mukherjee <email_at_smukher-sandia_dot_gov>");
MODULE_DESCRIPTION("kgni ioctl() Char Driver");
