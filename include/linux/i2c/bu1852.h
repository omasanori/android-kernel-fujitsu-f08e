/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/
#ifndef _LINUX_BU1852_H
#define _LINUX_BU1852_H

#include <linux/types.h>
#include <linux/i2c.h>

/* platform data for the ROHM BU1852 20-bit I/O expander driver */

struct bu1852_platform_data {
	/* number of the first GPIO */
	unsigned	gpio_base;

	/* initial pullup for input GPIOs, 0=enable PU, 1=disable */
	unsigned char	pullup1;				/* GPIO[07-00] */

	/* initial pulldown for input GPIOs, 0=enable PD, 1=disable */
	unsigned char	pulldown1;				/* GPIO[15-08] */
	unsigned char	pulldown2;				/* GPIO[19-16] */

	void		*context;	/* param to setup/teardown */

	int		(*setup)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	int		(*teardown)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	const char	*const *names;
};

#endif /* _LINUX_BU1852_H */
