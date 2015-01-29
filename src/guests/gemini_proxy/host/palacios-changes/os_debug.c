/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest_mem.h>
#include "gni_pub.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* #include <linux/pid.h> */


#define BUF_SIZE 20

#define DEBUG_PORT1 0xc0c0
#define HEARTBEAT_PORT 0x99


struct debug_state {
    char debug_buf[BUF_SIZE];
    uint_t debug_offset;

};


static int handle_gen_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct debug_state * state = priv_data;

    state->debug_buf[state->debug_offset++] = *(char*)src;

    if ((*(char*)src == 0xa) ||  (state->debug_offset == (BUF_SIZE - 1))) {
	PrintDebug(core->vm_info, core, "VM_CONSOLE>%s", state->debug_buf);
	memset(state->debug_buf, 0, BUF_SIZE);
	state->debug_offset = 0;
    }

    return length;
}

static int handle_hb_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    uint32_t val = 0;

    if (length == 1) {
	val = *(uint8_t *)src;
    } else if (length == 2) {
	val = *(uint16_t *)src;
    } else {
	val = *(uint32_t *)src;
    }

    V3_Print(core->vm_info, core, "HEARTBEAT> %x (%d)\n", val, val);

    return length;
}

static int handle_hcall(struct guest_info * info, uint_t hcall_id, void * priv_data) {
    struct debug_state * state = (struct debug_state *)priv_data;

    int msg_len = info->vm_regs.rcx;
    addr_t src_va = info->vm_regs.rbx;
    addr_t dest_va = info->vm_regs.rsi;
    addr_t host_src_va = 0;
    addr_t host_dest_va = 0;
    int buf_is_va = info->vm_regs.rdx;

    PrintDebug(info->vm_info, info, "buf_is va value %d\n", buf_is_va);

    if (buf_is_va == 1) {
	if (v3_gva_to_hva(info, src_va, &host_src_va) != 0) {
            PrintDebug(info->vm_info, info, "Invalid GVA(%p)->HVA lookup\n", (void *)src_va);
        }
	if (v3_gva_to_hva(info, dest_va, &host_dest_va) != 0) {
            PrintDebug(info->vm_info, info, "Invalid GVA(%p)->HVA lookup\n", (void *)dest_va);
        }
	    
     }
   
    memcpy((char *)state->debug_buf, (void*)host_src_va, 20);
     state->debug_offset = 500;

     memcpy((void *)host_dest_va, (void*)state, sizeof(struct debug_state));

    PrintDebug(info->vm_info, info, "VM_CONSOLE OS debug hypercall called here : Shyamali : msg_len %d> buf%s\n", msg_len, state->debug_buf);

    return 0;
}
/*
static inline struct task_struct *kgni_task_from_pid(pid_t pid)
{
        struct task_struct *task;
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        return task;
}
*/
static int handle_kgniopen(struct guest_info * info, uint_t hcall_id, void * priv_data)
{
    int gpid = info->vm_regs.rcx;
    addr_t dest_va = info->vm_regs.rbx;
    addr_t host_dest_va = 0;
    int buf_is_va = info->vm_regs.rdx;
    struct file *file=NULL;
    int fd;


        if (v3_gva_to_hva(info, dest_va, &host_dest_va) != 0) {
            PrintDebug(info->vm_info, info, "Invalid GVA(%p)->HVA lookup\n", (void *)dest_va);
        }

        PrintDebug(info->vm_info, info, "Before calling kgni open\n");
	fd = sys_open_from_kernel("/dev/kgni0", O_RDWR, 0, &file);
        PrintDebug(info->vm_info, info, " after open /dev/kgni0 fd we got %d\n", fd);
        return 0;
}
/* Use this IOCTL to show copy-from-user and copy to user */
#define   IOCTL_CMD_PALACIOS  -1000000000
 
static int handle_kgniioctl(struct guest_info * info, uint_t hcall_id, void * priv_data)
{

    int count= info->vm_regs.rcx;
    addr_t src_va = info->vm_regs.rbx;
    addr_t dest_va = info->vm_regs.rsi;
    addr_t *host_src_va;
    addr_t *host_dest_va;
    int cmd = info->vm_regs.rdx;
    int fd;
    struct file *file = NULL;


     PrintDebug(info->vm_info, info, " handle_cdmattach  hypercall %d src %p dest %p cmd %d\n", count, (void *)src_va, (void *)dest_va, cmd);
     host_src_va = (addr_t *)V3_Malloc(count);
        if (v3_read_gva_memory(info, src_va, count, (uchar_t *)host_src_va) == 0) {
            PrintDebug(info->vm_info, info, "failed copy from user  GVA(%p)\n", (void *)src_va);
        }
	fd = sys_open_from_kernel("/dev/kgni0", O_RDWR, 0, &file);
        PrintDebug(info->vm_info, info, " after open /dev/kgni0 fd we got %d\n", fd);
	cmd = cmd + IOCTL_CMD_PALACIOS; /* add a constant value to ioctl cmd, deduct isnide kgni driver */
	kgni_ioctl(file, cmd, (unsigned long)host_src_va);
        PrintDebug(info->vm_info, info, " after calling kgni ioctl \n");
/*
        if (v3_write_gva_memory(info, dest_va, count, (uchar_t *)host_src_va) == 0) {
            PrintDebug(info->vm_info, info, "failed copy to user back  GVA(%p)\n", (void *)dest_va);
        }
*/
	sys_close(fd);
    PrintDebug(info->vm_info, info, "VM_CONSOLE OS debug test copy to and from user \n");
  return 0;
}


static int handle_memregister(struct guest_info * info, uint_t hcall_id, void * priv_data)
{
	return 0;
}

static int debug_free(struct debug_state * state) {

    // unregister hypercall

    V3_Free(state);
    return 0;
};

#ifdef V3_CONFIG_CHECKPOINT
static int debug_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct debug_state * dbg = (struct debug_state *)private_data;
    
    V3_CHKPT_SAVE_AUTOTAG(ctx, dbg->debug_buf, savefailout);
    V3_CHKPT_SAVE_AUTOTAG(ctx, dbg->debug_offset, savefailout);
    
    return 0;

 savefailout:
    PrintError(VM_NONE,VCORE_NONE, "Failed to save debug\n");
    return -1;

}


static int debug_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct debug_state * dbg = (struct debug_state *)private_data;
    
    V3_CHKPT_LOAD_AUTOTAG(ctx, dbg->debug_buf,loadfailout);
    V3_CHKPT_LOAD_AUTOTAG(ctx, dbg->debug_offset,loadfailout);
    
    return 0;

 loadfailout:
    PrintError(VM_NONE, VCORE_NONE, "Failed to load debug\n");
    return -1;
}

#endif



static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))debug_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = debug_save,
    .load = debug_load
#endif 
};




static int debug_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct debug_state * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");

    state = (struct debug_state *)V3_Malloc(sizeof(struct debug_state));

    if (!state) {
	PrintError(vm, VCORE_NONE, "Cannot allocate in init\n");
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "Creating OS Debug Device\n");

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    if (v3_dev_hook_io(dev, DEBUG_PORT1,  NULL, &handle_gen_write) == -1) {
	PrintError(vm, VCORE_NONE, "Error hooking OS debug IO port\n");
	v3_remove_device(dev);
	return -1;
    }


    if (v3_dev_hook_io(dev, HEARTBEAT_PORT, NULL, &handle_hb_write) == -1) {
	PrintError(vm, VCORE_NONE, "error hooking OS heartbeat port\n");
	v3_remove_device(dev);
	return -1;
    }

/* include palacios/vmm_hypercall.h  
	typedef enum {
    SYMMOD_SYMS_HCALL =    0x0600,
.....
    GUEST_INFO_HCALL =     0x3000,         // no args
    OS_DEBUG_HCALL =       0xc0c0,          // RBX: msg_gpa, RCX: msg_len, RDX: buf_is_va (flag)
    GNI_CDMCREATE =       0xc0c1,          // RBX:instance, RCX: ptag, RDX: cookie, RDI: modes, RSI: return hdl 
    GNI_CDMATTACH =       0xc0c2,          // RBX: cdm_hdl, RCX: device, RDX: *local_addr, RDI: *nic_hdl
    GNI_MEMREGISTER =       0xc0c3,        // RBX: nic_hdl, RCX: address, RDX: length, RDI: dst_cq_hdl, RSI: flags 
} hcall_id_t;
       */
    v3_register_hypercall(vm, OS_DEBUG_HCALL, handle_hcall, state);
    v3_register_hypercall(vm, GUEST_KGNI_OPEN, handle_kgniopen, state);
    v3_register_hypercall(vm, GUEST_KGNI_IOCTL, handle_kgniioctl, state);
    v3_register_hypercall(vm, GNI_MEMREGISTER, handle_memregister, state);
    state->debug_offset = 0;
    memset(state->debug_buf, 0, BUF_SIZE);
  
    return 0;
}


device_register("OS_DEBUG", debug_init)
