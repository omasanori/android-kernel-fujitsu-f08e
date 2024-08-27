/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/

#ifndef _EDC_H_
#define _EDC_H_

#include <linux/spinlock.h>

#define	HSCALL_PUSH_INTERVAL	20

#define PHI35_LOOP 1000/* 1sec */
#define RESUME_PHI35_LOOP 1200/* 1.2sec */ 

enum {
	EDC_PLUG	= 0,
	EDC_SWITCH	= 1,
	EDC_NUM_OF_COUNT,
};

enum {
	EDC_NO_DEVICE	= 0,
	EDC_DEVICE		= 1,
};

enum {
	EARPHONE_PROBE_STAGE_0 = 0,
	EARPHONE_PROBE_STAGE_1,
	EARPHONE_PROBE_STAGE_2,
	EARPHONE_PROBE_STAGE_3,
};

enum {
	EARPHONE_REMOVE_STAGE_0 = 0,
	EARPHONE_REMOVE_STAGE_1,
};

enum {
	EARPHONE_RESUME_STAGE_0 = 0,
	EARPHONE_RESUME_STAGE_1,
	EARPHONE_RESUME_STAGE_2,
	EARPHONE_RESUME_STAGE_3,
};

enum {
	EARPHONE_USB_STAGE_0 = 0,
	EARPHONE_USB_STAGE_1,
};

enum {
	EARPHONE_NOT = 0,
	EARPHONE_MIC,
	EARPHONE_PIERCE,
};

enum {
	EDC_OK = 0,
	EDC_NG,
};
enum {
	PIERCE_MACHINE_TYPE_NOT = 0,
	PIERCE_MACHINE_TYPE,
};

enum{
	EDC_STATE_INIT=0,
	EDC_STATE_LOOP,
	EDC_STATE_CHATT,
	EDC_STATE_DESITION,
	EDC_STATE_ISR,
	EDC_STATE_PLUG,
	EDC_STATE_SUSPEND,
	EDC_STATE_RESUME,
	EDC_STATE_MAX,
};
#define EDC_STATE_OK  0
#define EDC_STATE_NG -1

#define EDC_MACHINE_TYPE_FJDEV001  	0x50
#define EDC_MACHINE_TYPE_FJDEV002 	0x40
#define EDC_MACHINE_TYPE_FJDEV004 	0x60
#define EDC_MACHINE_TYPE_FJDEV005 	0x10 /* add 2013/4/12 */

#define EDC_BOARD_TYPE_FJDEV001_2_3 0x5
#define EDC_BOARD_TYPE_FJDEV002_2_2 0x3

#define EDC_BOARD_TYPE_FJDEV002_RYO_2 0x6

typedef struct edc_handle {
	int io_data;				// IO status
	int Polling_Counter;		// chattering counter
	struct timer_list timer;	// struct timer
	int	gpio_det_irq;			// IO DET data
	int timer_flag;				// timer flag
} edc_handle_t;

#if 1
#define EDC_DEBUG 1
#endif

#ifdef EDC_DEBUG
#define	EDC_DBG_PRINTK(a...)	printk(a)
#else
#define	EDC_DBG_PRINTK(a...)
#endif

#if 0
#define EDC_DEBUG_LOOP 1
#endif

#ifdef EDC_DEBUG_LOOP
#define	EDC_DBG_LOOP_PRINTK(a...)	printk(a)
#else
#define	EDC_DBG_LOOP_PRINTK(a...)
#endif

#if 0
#define EDC_DEBUG_IRQ 1
#endif

#ifdef EDC_DEBUG_IRQ
#define	EDC_DBG_IRQ_PRINTK(a...)	printk(a)
#else
#define	EDC_DBG_IRQ_PRINTK(a...)
#endif

#if 0
#define EDC_DEBUG_STATE 1
#endif

#ifdef EDC_DEBUG_STATE
#define	EDC_DBG_STATE_PRINTK(a...)	printk(a)
#else
#define	EDC_DBG_STATE_PRINTK(a...)
#endif

#define	OPT_PIERCE_SWITCH	1
#define	OPT_USB_EARPHONE	0
#define	OPT_NV_READ			1
#define OPT_IOCTL_TEST		0
#define OPT_AUDIO_POLLING	0

#endif /* _EDC_H_ */
