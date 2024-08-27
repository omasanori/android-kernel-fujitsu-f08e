/*
 * Copyright(C) 2013 FUJITSU LIMITED
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/earlysuspend.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/nonvolatile_common.h>
#include <linux/wakelock.h>
#include <asm/system.h>

#include <linux/regulator/consumer.h>
#include "../../../arch/arm/mach-msm/devices.h"
#include <linux/pm_qos.h>

#include <linux/spi/fpc1080.h>
#include "fpc1080_local.h"


#define FPC1080_ACTIVATE_DEEP_SLEEP		0x2C

#define PM8921_GPIO_PM_TO_SYS(gpio)	(NR_GPIO_IRQS + (gpio) - 1)
#define PM8921_FGR_REG18_ON		20
#define FPC1080_LED_R			1
#define FPC1080_LED_G			2
#define FPC1080_LED_B			3
#define FPC1080_GPIO_RST		0
#define FPC1080_GPIO_INT		43
#define FPC1080_SPI_CS			86

#define SPI_CONTROL_OFF			0
#define SPI_CONTROL_ON			1
#define SPI_CONTROL_SHUTDOWN		2
#define SPI_CLIENT_FPS			2

#define SUPPLY_NAME			"FingerPrint_l17"
#define SUPPLY_MIN_UV			2850000
#define SUPPLY_MAX_UV			2850000
#define SUPPLY_UA_LOAD			300000

#define FPC1080_NV_NUMBER_1		0xA070	/* 41072 APNV_FINGERPRINT_LOGSW_DATA_I */
#define FPC1080_NV_NUMBER_2		0xA071	/* 41073 APNV_FINGERPRINT_LOGSW_IOCTL_I */
#define FPC1080_NV_NUMBER_3		0xB82F	/* 47151 APNV_PRINTFINGER_CONFIG_PARAM_I */
#define FPC1080_NV_SIZE			1
#define FPC1080_NV_SIZE_3		4

#define DATA_DUMP_MAX_LEN		8


#undef  spi_sync


extern void spi_env_ctrl(int runtype/* 0:off, 1:on, 2:shutdown */, int csno);
extern void fjdev_spi_bus4_softcs_mutex(int enable /* 0:unlock, 1:lock */);
extern void gpiolib_dbg_allgpio_out(int detail);

u8 fpc1080_log_flag = 0;
u8 fpc1080_debug_flag = 0;

static u8 fpc1080_spi_env;
static u8 fpc1080_spi_mode = SPI_MODE_0;
static struct pm_qos_request qos_req_list;


/*
 attribute device
 /sys/devices/platform/spi_qsd.x/spi_master/spix/spix.x
 /sys/class/spi_master/spix/spix.x
 /sys/bus/spi/drivers/fpc1080/spix.x
 /sys/bus/spi/devices/spix.x
*/


int fpc1080_spi_sync_fj(struct spi_device *spi, struct spi_message *m)
{
	int error;
	int cnt;
	struct spi_transfer *xfer;

	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
//	xfer = list_first_entry(&m->transfers , struct spi_transfer, transfer_list);
//	printk("[%s]: %s command transfer %d byte\n", __func__, xfer->len ? FPC1080_CMD(*((u8*)xfer->tx_buf)) : "None", xfer->len);
	fjdev_spi_bus4_softcs_mutex(1);
	gpio_set_value(FPC1080_SPI_CS, 0);

	error = spi_sync(spi, m);
	if (error) {
		dev_err(&spi->dev, "spi_sync failed. %d\n", error);
		goto err;
	}
	DBG_PRINT_DATA {
		printk(KERN_INFO "[FPC1080]%s: Mode:%d", __func__, spi->mode);
		list_for_each_entry(xfer, &m->transfers, transfer_list) {
			printk(" Speed:%d  ", xfer->speed_hz);
			if (xfer->tx_buf) {
				printk("Tx[%d]:", xfer->len);
				for (cnt = 0; cnt < xfer->len && cnt < DATA_DUMP_MAX_LEN; cnt++) {
					printk(" 0x%02X", ((u8*)xfer->tx_buf)[cnt]);
				}
			}
			if (xfer->rx_buf) {
				printk(" -- Rx[%d]:", xfer->len);
				for (cnt = 0; cnt < xfer->len && cnt < DATA_DUMP_MAX_LEN; cnt++) {
					printk(" 0x%02X", ((u8*)xfer->rx_buf)[cnt]);
				}
			}
		}
		printk("\n");
	}

err:
	gpio_set_value(FPC1080_SPI_CS, 1);
	fjdev_spi_bus4_softcs_mutex(0);
	DBG_PRINT_FUNC("[FPC1080]%s: OUT ret=%d\n", __func__, error);
	return error;
}

void fpc1080_finger_present(struct fpc1080_data *fpc1080)
{
	int error;

	pm_qos_add_request(&qos_req_list, PM_QOS_CPU_DMA_LATENCY,1);

	fpc1080->spi->mode = fpc1080_spi_mode;
	error = spi_setup(fpc1080->spi);
	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_setup failed. %d\n", error);
	}

	DBG_LED_ON(FPC1080_LED_B);
}
void fpc1080_finger_up(struct fpc1080_data *fpc1080)
{
	if (pm_qos_request_active(&qos_req_list))
		pm_qos_remove_request(&qos_req_list);

	DBG_LED_OFF(FPC1080_LED_B);
}

int fpc1080_open_fj(struct fpc1080_data *fpc1080)
{
	int ret = 0;

	DBG_PRINT_FULL("[FPC1080]%s: mode:%d clk:%d spi_env:%d opened:%d\n", __func__,
			fpc1080_spi_mode, FPC1080_SPI_CLOCK_SPEED, fpc1080_spi_env, fpc1080->opened);
	if (!fpc1080->opened) {
		fpc1080->opened = 1;
		if (fpc1080_spi_env != SPI_CONTROL_ON) {
			spi_env_ctrl(SPI_CONTROL_ON, SPI_CLIENT_FPS);
			fpc1080_spi_env = SPI_CONTROL_ON;
		}
		DBG_LED_ON(FPC1080_LED_G);
	} else {
		dev_err(&fpc1080->spi->dev, "fpc1080 multiple open\n");
		ret = -EBUSY;
	}
	return ret;
}

void fpc1080_release_fj(struct fpc1080_data *fpc1080)
{
	fpc1080->opened = 0;
}

int fpc1080_deep_sleep(struct fpc1080_data *fpc1080)
{
	int ret = 0;

	DBG_PRINT_FULL("[FPC1080]%s: spi_env=%d opened=%d\n", __func__, fpc1080_spi_env, fpc1080->opened);
	if (fpc1080_spi_env != SPI_CONTROL_OFF) {
		ret = fpc1080_spi_wr_reg(fpc1080, FPC1080_ACTIVATE_DEEP_SLEEP, 0, 1);
		spi_env_ctrl(SPI_CONTROL_OFF, SPI_CLIENT_FPS);
		fpc1080_spi_env = SPI_CONTROL_OFF;
	}
	DBG_LED_OFF(FPC1080_LED_G);
	return ret;
}

void fpc1080_selftest_fj(struct fpc1080_data *fpc1080, u8 state)
{
	if (state) {
		if (fpc1080_spi_env != SPI_CONTROL_ON) {
			spi_env_ctrl(SPI_CONTROL_ON, SPI_CLIENT_FPS);
		}
	} else {
		if (fpc1080_spi_env != SPI_CONTROL_ON) {
			spi_env_ctrl(SPI_CONTROL_OFF, SPI_CLIENT_FPS);
		}
	}
}

void fpc1080_cleanup_fj(struct fpc1080_data *fpc1080)
{
	fpc1080_deep_sleep(fpc1080);
	gpio_set_value(FPC1080_GPIO_RST, 0);
	gpio_set_value(PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON), 0);
	usleep(1000);
}

static void fpc1080_get_nvinfo(struct fpc1080_data *fpc1080)
{
	int error;
	u8 fpc1080_debug_nv[FPC1080_NV_SIZE];
	u8 fpc1080_nv3_tmp[FPC1080_NV_SIZE_3];

	error = get_nonvolatile(fpc1080_debug_nv, FPC1080_NV_NUMBER_1, FPC1080_NV_SIZE);
	if (error >= 0) {
		fpc1080_log_flag = fpc1080_debug_nv[0];
		DBG_PRINT_INFO("[FPC1080]%s: get log_flag=0x%02X\n", __func__, fpc1080_log_flag);
	} else {
		pr_err("[FPC1080]%s: get_nonvolatile failed. %d ID=%d[0x%04X]\n", __func__, error, FPC1080_NV_NUMBER_1, FPC1080_NV_NUMBER_1);
	}
	error = get_nonvolatile(fpc1080_debug_nv, FPC1080_NV_NUMBER_2, FPC1080_NV_SIZE);
	if (error >= 0) {
		fpc1080_debug_flag = fpc1080_debug_nv[0];
		DBG_PRINT_INFO("[FPC1080]%s: get debug_flag=0x%02X\n", __func__, fpc1080_debug_flag);
	} else {
		pr_err("[FPC1080]%s: get_nonvolatile failed. %d ID=%d[0x%04X]\n", __func__, error, FPC1080_NV_NUMBER_2, FPC1080_NV_NUMBER_2);
	}
	error = get_nonvolatile(fpc1080_nv3_tmp, FPC1080_NV_NUMBER_3, FPC1080_NV_SIZE_3);
	if (error < 0) {
		pr_err("[FPC1080]%s: get_nonvolatile failed. %d ID=%d[0x%04X]\n", __func__, error, FPC1080_NV_NUMBER_3, FPC1080_NV_NUMBER_3);
		memset(fpc1080_nv3_tmp, 0, FPC1080_NV_SIZE_3);
	}
	FPC1080_SPI_CLOCK_SPEED = GET_SPI_CLOCK(fpc1080_nv3_tmp[0]);
	fpc1080_spi_mode = GET_SPI_MODE(fpc1080_nv3_tmp[1]);
	DBG_PRINT_FULL("[FPC1080]%s: get param=0x%02X,0x%02X,0x%02X,0x%02X SPI_Clock=%d SPI_Mode=%d\n", __func__,
			fpc1080_nv3_tmp[0], fpc1080_nv3_tmp[1], fpc1080_nv3_tmp[2], fpc1080_nv3_tmp[3],
			FPC1080_SPI_CLOCK_SPEED, fpc1080_spi_mode);
	return;
}

static void fpc1080_gpio_config(void)
{
	int ret;

	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);

	/* FPC1080-Power */
	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON), "fpc1080_Pon");
	if (ret) {
		pr_err("[FPC1080]%s: PM8921_FGR_REG18_ON gpio_request failed. %d\n", __func__, ret);
	}
	gpio_direction_output(PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON), 0);

	/* SPI-CS */
	ret = gpio_tlmm_config(GPIO_CFG(FPC1080_SPI_CS, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (ret) {
		pr_err("[FPC1080]%s: FPC1080_SPI_CS gpio_tlmm_config failed. %d\n", __func__, ret);
	}
	ret = gpio_request(FPC1080_SPI_CS, "fpc1080_CS");
	if (ret) {
		pr_err("[FPC1080]%s: FPC1080_SPI_CS gpio_request failed. %d\n", __func__, ret);
	}
	gpio_direction_output(FPC1080_SPI_CS, 0);

	/* FPC1080-RESET */
	ret = gpio_tlmm_config( GPIO_CFG(FPC1080_GPIO_RST, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if (ret) {
		pr_err("[FPC1080]%s: FPC1080_GPIO_RST gpio_tlmm_config failed. %d\n", __func__, ret);
	}
	ret = gpio_request(FPC1080_GPIO_RST, "fpc1080_RST");
	if (ret) {
		pr_err("[FPC1080]%s: FPC1080_GPIO_RST gpio_request failed. %d\n", __func__, ret);
	}
	gpio_direction_output(FPC1080_GPIO_RST, 0);

	/* FPC1080-INT */
	ret = gpio_tlmm_config(GPIO_CFG(FPC1080_GPIO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (ret) {
		pr_err("[FPC1080]%s: FPC1080_GPIO_INT gpio_tlmm_config failed. %d\n", __func__, ret);
	}
	ret = gpio_request(FPC1080_GPIO_INT, "fpc1080_INT");
	if (ret) {
		pr_err("[FPC1080]%s: FPC1080_GPIO_INT gpio_request failed. %d\n", __func__, ret);
	}
	gpio_direction_input(FPC1080_GPIO_INT);

	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
}

static void fpc1080_prepare_power_on(void)
{
	struct regulator *vreg_l17;
	int ret;

	vreg_l17 = regulator_get( &apq8064_device_qup_spi_gsbi4.dev, SUPPLY_NAME);
	if( !vreg_l17 || IS_ERR(vreg_l17) ) {
		pr_err("[FPC1080]%s: regulator_get() failed. %ld\n", __func__, PTR_ERR(vreg_l17));
		goto err;
	}

	ret = regulator_set_voltage( vreg_l17, SUPPLY_MIN_UV, SUPPLY_MAX_UV );
	if (ret) {
		pr_err("[FPC1080]%s: regulator_set_voltage() failed. %d\n", __func__, ret);
		goto err;
	}
	ret = regulator_set_optimum_mode( vreg_l17, SUPPLY_UA_LOAD );
	if (ret < 0) {
		pr_err("[FPC1080]%s: regulator_set_optimum_mode() failed. %d\n", __func__, ret);
		goto err;
	}
	ret = regulator_enable( vreg_l17 );
	if (ret < 0) {
		pr_err("[FPC1080]%s: regulator_enable() failed. %d\n", __func__, ret);
		goto err;
	}
err:
	usleep(2000);
	DBG_PRINT_INFO("[FPC1080]%s: MSM_UART_AUX POWER ON\n", __func__);
}


static void fpc1080_power_on(void)
{
	/* FPC1080 Reset -> Assert(L) */
	gpio_set_value(FPC1080_GPIO_RST, 0);
	/* FPC1080 Power ON */
	gpio_set_value(PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON), 1);
	gpio_set_value(FPC1080_SPI_CS, 1);
	/* Delay 10ms */
	msleep(10);
	DBG_PRINT_INFO("[FPC1080]%s: POWER ON\n", __func__);
}

void fpc1080_probe_fj(struct fpc1080_data *fpc1080)
{
	fpc1080_get_nvinfo(fpc1080);
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);

	fpc1080_gpio_config();
	DBG_LED_ON(FPC1080_LED_G);
	fpc1080_prepare_power_on();
	spi_env_ctrl(SPI_CONTROL_ON, SPI_CLIENT_FPS);
	fpc1080_spi_env = SPI_CONTROL_ON;
	fpc1080_power_on();
	fpc1080->count.open = 0;
	fpc1080->opened = 0;

	memset(&qos_req_list, 0, sizeof(qos_req_list));

	if (fpc1080_debug_flag & 0x02) {
		if (sysfs_create_file(&fpc1080->spi->dev.kobj, &fpc1080_attr_dbg.attr) < 0)
			dev_err(&fpc1080->spi->dev, "sysfs_create_file failed.\"%s\"\n", fpc1080_attr_dbg.attr.name);
	}
}

static struct fpc1080_platform_data fpc1080_pdata = {
	.irq_gpio		= FPC1080_GPIO_INT,
	.reset_gpio		= FPC1080_GPIO_RST,
	.adc_setup = {
		.offset		= DEFAULT_ADC_OFFSET,
		.gain		= DEFAULT_ADC_GAIN,
		.pxl_setup	= DEFAULT_PIXEL_SETUP,
	},
};
static struct spi_board_info fpc1080_spi __initdata = {
	.modalias		= "fpc1080",
	.bus_num		= 4,
	.chip_select		= 2,
	.max_speed_hz		= 16000000,
	.controller_data	= NULL,
	.irq			= 0,
	.platform_data		= &fpc1080_pdata,
};

int __init fpc1080_spi_assign(void)
{
	int ret = spi_register_board_info(&fpc1080_spi, 1);
	if (ret)
		pr_err("[FPC1080]%s: spi_register_board_info() error. %d\n", __func__, ret);
	return ret;
}

static ssize_t fpc1080_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpc1080_data *fpc1080 = dev_get_drvdata(dev);
	ssize_t ret = 0;

	gpiolib_dbg_allgpio_out(1);
	ret = scnprintf(buf, PAGE_SIZE,
			"avail_data=%u current_frame=%u\n"
			"capture_time=%u frames_captured=%u frames_stored=%u\n"
			"open=%u read=%u write=%u poll=%u ioctl=%u interrupt=%u\n"
			"gain=%d[0x%02X] offset=%d[0x%02X] pixel=%d[0x%02X]\n"
			"SPI-Speed=%d SPI-Mode=%d spi_env=%d opened=%d\n"
			"BuffSize=%lu\n",
			fpc1080->avail_data, fpc1080->current_frame,
			fpc1080->diag.capture_time, fpc1080->diag.frames_captured, fpc1080->diag.frames_stored,
			fpc1080->count.open, fpc1080->count.read, fpc1080->count.write,
			fpc1080->count.poll, fpc1080->count.ioctl, fpc1080->count.interrupt,
			fpc1080->adc_setup.gain, fpc1080->adc_setup.gain, fpc1080->adc_setup.offset,
			fpc1080->adc_setup.offset, fpc1080->adc_setup.pxl_setup, fpc1080->adc_setup.pxl_setup,
			FPC1080_SPI_CLOCK_SPEED, fpc1080_spi_mode, fpc1080_spi_env, fpc1080->opened,
			PAGE_SIZE
			);
	return ret;
}

static ssize_t fpc1080_dbg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1080_data *fpc1080 = dev_get_drvdata(dev);
	u8 tmp[2];
	int ret;

	ret = sscanf(buf, "%hhu %hhu", &tmp[0], &tmp[1]);
	if (ret <= 0)
		return -EINVAL;

	FPC1080_SPI_CLOCK_SPEED = GET_SPI_CLOCK(tmp[0]);
	if (ret == 2)
		fpc1080_spi_mode = GET_SPI_MODE(tmp[1]);

	DBG_PRINT_FULL("[FPC1080]%s: SPI-Speed:%d  SPI-Mode:%d\n", __func__, FPC1080_SPI_CLOCK_SPEED, fpc1080_spi_mode);
	return strnlen(buf, count);
}

struct device_attribute fpc1080_attr_dbg = __ATTR(dbg, (S_IRUGO|S_IWUGO), fpc1080_dbg_show, fpc1080_dbg_store);

void fpc1080_fj_dump(const char *pFunc, u8 *pData, unsigned int nDataLen)
{

	#define	FPC1080_DUMP_LEN		32		/* number of data per line */
	#define	FPC1080_DUMP_LINE		256		/* line buffer size */

	unsigned int ii, nLen = 0;
	char *tmp_buf;

	tmp_buf = kzalloc(FPC1080_DUMP_LINE, GFP_KERNEL);
	if (!tmp_buf) {
		pr_err("[FPC1080]%s: failed to allocate memory for buffer dump\n", __func__);
		return;
	}

	printk(KERN_INFO "[FPC1080]%s: DUMP [%s] len = %d\n", __func__, pFunc, nDataLen);

	for (ii = 0; ii < nDataLen; ii++, pData++) {
		if ((ii % FPC1080_DUMP_LEN) == 0) {
			nLen = snprintf( tmp_buf + nLen, (FPC1080_DUMP_LINE - nLen), "%08X | ", ii);
		}
		nLen += snprintf( tmp_buf + nLen, (FPC1080_DUMP_LINE - nLen), "%02X ", *pData);
		if ((ii % FPC1080_DUMP_LEN) == (FPC1080_DUMP_LEN - 1)) {
			printk(KERN_INFO "%s\n", tmp_buf);
			nLen = 0;
			memset (tmp_buf, 0, sizeof(unsigned char) * FPC1080_DUMP_LINE);
		}
	}

	if ((ii % FPC1080_DUMP_LEN) != 0) {
		printk(KERN_INFO "%s\n", tmp_buf);
	}
	kfree(tmp_buf);
}
