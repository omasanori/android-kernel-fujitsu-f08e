/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/
// File Name:
//      mmcdl_common.h
//
// History:
//      2011.06.23  Created.
//
/*----------------------------------------------------------------------------*/
#ifndef _MMCDL_COMMON_H
#define _MMCDL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <linux/ioctl.h>

/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/
typedef struct mmcdl_data {
  unsigned int	iSector;		//
  uint8_t* 		iBuff;			//
  loff_t		iSize;			//
} mmcdl_data_t;

#define IOC_MAGIC_MMCDL 'w'

#define IOCTL_MMCDLWRITE	_IOW(IOC_MAGIC_MMCDL, 1, mmcdl_data_t)
#define IOCTL_MMCDLREAD		_IOR(IOC_MAGIC_MMCDL, 2, mmcdl_data_t)

/* FUJITSU:2011/12/13 SDDL ADD-S */
#define IOCTL_MMCDLGETSIZE	_IOW(IOC_MAGIC_MMCDL, 3, unsigned long *)
/* FUJITSU:2011/12/13 SDDL ADD-E */
/* TESTMODE20120116 ADD-S */
#define IOCTL_MMCDLGETINFO	_IOW(IOC_MAGIC_MMCDL, 4, uint8_t *)
/* TESTMODE20120116 ADD-E */
/* FUJITSU:2012-06-16 MC kari add */
#define IOCTL_RAMGETINFO	_IOW(IOC_MAGIC_MMCDL, 5, uint8_t *)
/* FUJITSU:2012-06-16 MC kari add */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* _MMCDL_COMMON_H */
