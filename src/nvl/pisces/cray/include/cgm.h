/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright 2007 Cray Inc. All Rights Reserved.
 */

#ifndef _CRAY_CGM_H
#define _CRAY_CGM_H

#include <linux/types.h>
#include <linux/version.h>

typedef struct _cgm_reservation_struct *cgm_reservation_t;

extern cgm_reservation_t cgm_global;

extern int cgm_reserve_gatt_pages_named(char *name, uint32_t maxpages,
					cgm_reservation_t *handle);
extern int cgm_release_gatt_pages(cgm_reservation_t handle);
extern int cgm_alloc_gatt_pages(cgm_reservation_t handle, uint32_t npages);
extern int cgm_free_gatt_pages(cgm_reservation_t handle, int index,
			       uint32_t npages);
extern int cgm_get_gatt_pages_count(cgm_reservation_t hdl);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <asm/gart.h>
#else
#define GPTE_VALID    1
#define GPTE_COHERENT 2
#endif

/* from arch/x86_64/kernel/pci-gart.c:
 * Copyright 2002 Andi Kleen, SuSE Labs.
 */
#define GPTE_ENCODE(x) \
	(((x) & 0xfffff000) | (((x) >> 32) << 4) | GPTE_VALID | GPTE_COHERENT)
#define GPTE_DECODE(x) (((x) & 0xfffff000) | (((u64)(x) & 0xff0) << 28))
/* end arch/x86_64/kernel/pci-gart.c */

/* Globals for use only by the following static inline functions */
extern uint32_t *cgm_gatt;
extern uint64_t cgm_gart;
extern uint32_t cgm_unmapped_entry;

/* Set GATT page table entry */
static inline int cgm_set_gatt_pageaddr(cgm_reservation_t hdl, int index,
					uint64_t phys_mem)
{
	if (!cgm_global)
		return -ENODEV;

	cgm_gatt[index] = GPTE_ENCODE(phys_mem);
	return 0;
}

static inline int cgm_clear_gatt_pageaddr(cgm_reservation_t hdl, int index)
{
	if (!cgm_global)
		return -ENODEV;

	cgm_gatt[index] = cgm_unmapped_entry;
	return 0;
}

/* Get GATT page table entry */
static inline int cgm_get_gatt_pageaddr(cgm_reservation_t hdl, int index,
					uint64_t *phys_mem)
{
	if (!cgm_global)
		return -ENODEV;

	*phys_mem = GPTE_DECODE(cgm_gatt[index]);
	return 0;
}


/* Get address in aperture corresponding to index */
static inline int cgm_get_gart_addr(cgm_reservation_t hdl, int index,
				    uint64_t *phys_mem)
{
	if (!cgm_global)
		return -ENODEV;

	*phys_mem = cgm_gart + (index << PAGE_SHIFT);
	return 0;
}


#endif /* _CRAY_CGM_H */
