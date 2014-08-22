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
#include <linux/kernel.h>


#define BUF_SIZE 20

#define DEBUG_PORT1 0xc0c0
#define HEARTBEAT_PORT 0x99

#define GNI_ALPS_FILE "/scratch1/smukher/alps-info"

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

static int handle_cdmcreate(struct guest_info * info, uint_t hcall_id, void * priv_data)
{
	uint8_t ptag = info->gni_alps.ptag;
     uint32_t  instance = info->gni_alps.instance;
    uint32_t   cookie = info->gni_alps.cookie;
    gni_cdm_handle_t    cdm_hndl;
    gni_nic_handle_t    nic_hndl;
    gni_cq_handle_t    cq_hndl;
    uint32_t            devid_val = 0, local_addr = 0;
    uint32_t  device_id;
   gni_return_t        gni_error= 0; 
   
    int msg_len = info->vm_regs.rcx;
    addr_t src_va = info->vm_regs.rbx;
    addr_t host_src_va = 0;
    int buf_is_va = info->vm_regs.rdx;

    PrintDebug(info->vm_info, info, "buf_is va value %d msg_len %d\n", buf_is_va, msg_len);

    if (buf_is_va == 1) {
        if (v3_gva_to_hva(info, src_va, &host_src_va) != 0) {
            PrintDebug(info->vm_info, info, "Invalid GVA(%p)->HVA lookup\n", (void *)src_va);
        }

     }
 
    
    PrintDebug(info->vm_info, info, "OS debug hypercall cdmcreate called here : \n");
    PrintDebug(info->vm_info, info, "CDM create cookie %u ptag %d instance %u\n", info->gni_alps.cookie, info->gni_alps.ptag, info->gni_alps.instance);



    addr_t host_dest_va = 0;
        if (v3_gva_to_hva(info, src_va, &host_src_va) != 0) {
            PrintDebug(info->vm_info, info, "Invalid GVA(%p)->HVA lookup\n", (void *)src_va);
        }

        PrintDebug(info->vm_info, info, "BEFORE  CDM create\n");
        gni_error = gni_cdm_create(
                              instance,
                              ptag,
                            cookie,
                           GNI_CDM_MODE_ERR_NO_KILL,
                	  &cdm_hndl
                );
          if (gni_error != GNI_RC_SUCCESS){
        	return -1;
    		}
            PrintDebug(info->vm_info, info, "After  CDM create\n");

 	/*Attach to Gemini*/
    gni_error = gni_cdm_attach(&cdm_hndl, devid_val,
                    &local_addr, &nic_hndl);
    if (gni_error != GNI_RC_SUCCESS){
            PrintDebug(info->vm_info, info, " CDM attach failed  status %d\n", gni_error);
	return -1;
    }

    /*Completion Queue setup*/
    gni_error = gni_cq_create(&nic_hndl, 64, 0,
                    NULL, 0, &cq_hndl);
    if (gni_error != GNI_RC_SUCCESS){
            PrintDebug(info->vm_info, info, " CQ create  failed error  value %d\n",  gni_error);
	return -1;
    }
	
        return 0;
}

/* Use this IOCTL to show copy-from-user and copy to user */

static int handle_cdmattach(struct guest_info * info, uint_t hcall_id, void * priv_data)
{

  int count= info->vm_regs.rcx;
    addr_t src_va = info->vm_regs.rbx;
    addr_t dest_va = info->vm_regs.rsi;
    addr_t *host_src_va;
    addr_t *host_dest_va;
    int buf_is_va = info->vm_regs.rdx;


     host_src_va = (addr_t *)V3_Malloc(count);
    if (buf_is_va == 1) {
        if (v3_read_gva_memory(info, src_va, count, (uchar_t *)host_src_va) != 0) {
            PrintDebug(info->vm_info, info, "failed copy from user  GVA(%p)\n", (void *)src_va);
        }
        if (v3_write_gva_memory(info, dest_va, count, (uchar_t *)host_src_va) != 0) {
            PrintDebug(info->vm_info, info, "failed copy to user back  GVA(%p)\n", (void *)dest_va);
        }

     }

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

    v3_register_hypercall(vm, OS_DEBUG_HCALL, handle_hcall, state);
    v3_register_hypercall(vm, GNI_CDMCREATE, handle_cdmcreate, state);
    v3_register_hypercall(vm, GNI_CDMATTACH, handle_cdmattach, state);
    v3_register_hypercall(vm, GNI_MEMREGISTER, handle_memregister, state);
    state->debug_offset = 0;
    memset(state->debug_buf, 0, BUF_SIZE);
  
    return 0;
}


device_register("OS_DEBUG", debug_init)
