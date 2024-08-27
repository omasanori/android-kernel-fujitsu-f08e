/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/

#ifndef _TMC112_H_
#define _TMC112_H_

#include <linux/clk.h>
#include <mach/vreg.h>
#include <linux/spinlock.h>

int tmc112_boot(void);
int tmc112_halt(void);

/* private data */
struct tmc112_pdata {
	int pin_reset;
	int pin_request;
	int gpio_request_irq;
	int pin_ready;
	int pin_xirq;
	struct vreg *vreg_core;
	struct vreg *vreg_io;
	struct clk* clk_b;

	int request_raised;
	int ready_raised;
	wait_queue_head_t	wq_request;

	/* request count */
	int suspend;				/* suspend invoked while DSP in active */
	int suspend_withrun;		/* suspend invoked while DSP active */
	int resume;					/* resume requested */

	int cancel_read;			/* IOCTL_CANCEL_READ requested */
	int running;				/* Opened */
};

#endif
