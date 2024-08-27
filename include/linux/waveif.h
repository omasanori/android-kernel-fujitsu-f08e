#ifndef	__WAVEIF_H__
#define	__WAVEIF_H__

#include <linux/ioctl.h>

#define	WAVEIF_CANCEL_READ	_IOW('w', 0x06, int)	// no parameter
#define	WAVEIF_LOAD_FIRM	_IOW('w', 0x07, int)	// 8, 16 in kHz resolution

#endif	//	__WAVEIF_H__
