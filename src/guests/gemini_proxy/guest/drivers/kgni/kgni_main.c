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
#include "gni_pub.h"


char kgni_driver_name[] = "kgni";
static int      kgni_driver_numdev = 1;
static dev_t    kgni_dev_id;
static int      kgni_major = 0;
struct cdev *kgni_cdev;

#define SEND_RECV_ARGS  10

int kgni_open(struct inode *inode, struct file *filp)
{
        kgni_file_t     *file_inst;

	printk(KERN_DEBUG "fake kgni open called");

        file_inst = kmalloc(sizeof(kgni_file_t), GFP_KERNEL);
        if (file_inst == NULL) {
                return (-ENOMEM);
        }

        filp->private_data = file_inst;
        return 0;

}

/**
 * kgni_close - File close
 **/
int kgni_close(struct inode *inode, struct file *filp)
{
        kgni_file_t     *file_inst;
        kgni_device_t   *kgni_dev;

        kgni_dev = container_of(inode->i_cdev, kgni_device_t, cdev);

        file_inst = filp->private_data;
	filp->private_data = NULL;

        kfree(file_inst);

        return 0;
}



/**
 * kgni_exit - driver cleanup routine
 *
 * Called before the driver is removed from the memory
 **/
static void __exit kgni_exit(void)
{
	cdev_del(kgni_cdev); /*removing the structure that we added previously*/
    printk(KERN_INFO " kgni : removed the gni from kernel\n");

    unregister_chrdev_region(kgni_dev_id,1);
    printk(KERN_INFO "kgni: unregistered the device numbers\n");

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


/**
 * kgni_ioctl - The ioctl() implementation
 **/
static long kgni_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	unsigned long hcall=0xc0c2;
        unsigned long    trial=0;
        unsigned long len= 8000 * 3000;
        unsigned long buf_is_va=1;
	kgni_file_t                     *file_inst = filp->private_data;
        kgni_device_t                   *kgni_dev;
        struct inode                    *inode = filp->f_dentry->d_inode;
	/* have send_hcalls */

	switch (cmd) {
	case SEND_RECV_ARGS:

		printk(KERN_INFO, "Shyamali simple arg pass ioctl from guest\n");
		void *kbuf_src = kmalloc(len, GFP_KERNEL); 
		void *kbuf_dest = kmalloc(len, GFP_KERNEL); 
		if (copy_from_user(kbuf_src, (void *)arg, len)) {
                        retval = -1;
                        break;
                }
		unsigned long result =  hcall3(trial, hcall, len, &kbuf_src, buf_is_va, &kbuf_dest);
		break;
/*
	case GNI_IOC_NIC_SETATTR:
                if (_IOC_SIZE(cmd) != sizeof(nic_attr)) {
                        GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
                        retval = -1;
                        break;
                }
                GPRINTK_RL(0, KERN_INFO, "Shyamali aprun command from host to node 47");
                if (copy_from_user(&nic_attr, (void *)arg, sizeof(nic_attr))) {
                        retval = -1;
                        break;
                }

                retval = send_hcall(&nic_attr, sizeof(nic_attr));
                if (retval) {
                        break;
                }
*/
	}
	return 0;

}


struct file_operations kgni_fops = {
        .owner          = THIS_MODULE,
        .unlocked_ioctl = kgni_ioctl,
        .open           = kgni_open,
        .release        = kgni_close,
};

/*
**
 * kgni_init - driver registration routine
 *
 * Returns 0 if registered successfully.
 **/
static int __init kgni_init(void)
{
	int err;
	/* Register char driver */
	printk(KERN_DEBUG "fake kgni init called");
	err = alloc_chrdev_region(&kgni_dev_id, 0, kgni_driver_numdev, kgni_driver_name);
        if (err) {
		printk(KERN_ALERT " guest kgni: failed to allocate major number\n");
                return err;
        }
	kgni_major = MAJOR(kgni_dev_id);
	printk(KERN_INFO "guest kgni : major number of our device is %d\n",kgni_major);
    	printk(KERN_INFO "guest kgni : to use mknod /dev/%s c %d 0\n",kgni_driver_name, kgni_major);
	kgni_cdev= cdev_alloc(); // Get an allocated cdev structure 
	kgni_cdev->owner=THIS_MODULE; 
	kgni_cdev->ops= &kgni_fops;
	err = cdev_add(kgni_cdev,kgni_dev_id,1);
    if(err < 0) {
        printk(KERN_ALERT "kgni: device adding to the kerknel failed\n");
        return err;
    }
    else
	printk(KERN_DEBUG " kgni init succesful %d\n", kgni_major);
	return 0;
}

module_init(kgni_init);
module_exit(kgni_exit);
