#ifndef _PMI_UTIL_H_
#define _PMI_UTIL_H_

#include <linux/ioctl.h>
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __KERNEL__
#include <sys/ioctl.h>
#else
#include <linux/ioctl.h>
#endif
#define MAXIMUM_CQ_RETRY_COUNT 60

  typedef struct pmi_allgather_args
  {
    uint32_t comm_size;		/* IN size of world_comm */
    void *in_data;		/* IN data to post for gather */
    uint16_t in_data_len;	/* IN size of the data to post */
    void *out_data;		/* OUT  after gather from all ranks */
  } pmi_allgather_args_t;

  typedef struct pmi_getsize_args
  {
    uint32_t comm_size;		/* IN size of world_comm */
  } pmi_getsize_args_t;

  typedef struct pmi_getrank_args
  {
    uint32_t myrank;		/* IN size of world_comm */
  } pmi_getrank_args_t;

/* IOCTL commands */
#define PMI_IOC_MAGIC   'P'
#define PMI_IOC_ALLGATHER     _IOWR(PMI_IOC_MAGIC, 1, pmi_allgather_args_t)
#define PMI_IOC_GETSIZE        _IOWR(PMI_IOC_MAGIC, 2, pmi_getsize_args_t)
#define PMI_IOC_GETRANK        _IOWR(PMI_IOC_MAGIC, 3, pmi_getrank_args_t)
#define PMI_IOC_FINALIZE       _IOWR(PMI_IOC_MAGIC, 4,NULL)
#define PMI_IOC_BARRIER        _IOWR(PMI_IOC_MAGIC, 5, NULL)
#define PMI_IOC_BARRIER        _IOWR(PMI_IOC_MAGIC, 6, NULL)

#ifdef __cplusplus
}				/* extern "C" */
#endif

#endif /*_PMI_UTIL_H_*/
