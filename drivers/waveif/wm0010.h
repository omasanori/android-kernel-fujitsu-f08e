/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/

#ifndef _WM0010_H_
#define _WM0010_H_

#include <linux/device.h>
#include <linux/spi/spi.h>

enum auddsp_ctrl {
	AUDDSP_CTRL_INIT,
	AUDDSP_CTRL_TERM,
	AUDDSP_CTRL_ENABLE,
	AUDDSP_CTRL_DISABLE
};

enum auddsp_gpio {
	AUDDSP_GPIO_RESET,
	AUDDSP_GPIO_READY,
	AUDDSP_GPIO_REQUEST,
	AUDDSP_GPIO_XIRQ
};
enum {
	AUDDSP_GPIO_VALUE_LOW,      /* same as 0 */
	AUDDSP_GPIO_VALUE_HIGH,     /* same as 1 */
	AUDDSP_GPIO_VALUE_INACTIVE, /* logical */
	AUDDSP_GPIO_VALUE_ACTIVE    /* logical */
};

struct auddsp_irqinfo {
	struct {
		int irq;
		int pol;
	} ready, request, xirq;
};

void *auddsp_getenv(struct device *dev,struct auddsp_irqinfo *irqinfo);
void auddsp_putenv(void *env);
int  auddsp_power_ctrl(void *env, enum auddsp_ctrl ctrl);
int  auddsp_clk_ctrl(void *env, enum auddsp_ctrl ctrl);
int  auddsp_gpio_ctrl(void *env, enum auddsp_ctrl ctrl);
int  auddsp_gpio_get_value(void *env, enum auddsp_gpio gpio);
void auddsp_gpio_set_value(void *env, enum auddsp_gpio gpio,int value);
int  auddsp_spi_sync(struct spi_device *spi,struct spi_message *message);

#endif
