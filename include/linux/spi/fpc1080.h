#ifndef LINUX_SPI_FPC1080_H
#define LINUX_SPI_FPC1080_H

struct fpc1080_adc_setup {
	u8 gain;
	u8 offset;
	u8 pxl_setup;
};

struct fpc1080_platform_data {
	int irq_gpio;
	int reset_gpio;
	struct fpc1080_adc_setup adc_setup;
};

#endif
