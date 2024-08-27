/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

#include <linux/waveif.h>

#include "max9867ctrl.h"
#include "wm0010.h"

/* local debugging */
#if 1
#ifdef  dev_err
#undef  dev_err
#endif
#define dev_err(dev,x,...)  printk("ERROR: auddsp(%04d):[%s] " x, __LINE__, dev_name(dev), ##__VA_ARGS__)
#ifdef  dev_dbg
#undef  dev_dbg
#endif
#define dev_dbg(dev,x,...)  printk("auddsp(%04d):[%s] " x, __LINE__, dev_name(dev), ##__VA_ARGS__)
#ifdef  pr_err
#undef  pr_err
#endif
#define pr_err(x,...)       printk("ERROR: auddsp(%04d): " x, __LINE__, ##__VA_ARGS__)
#endif

#define AUDDSP_STATUS_SIZE  8
#define AUDDSP_STATUS_COUNT 4

#define SPI_HZ_BOOT_SLOW     1100000
#define SPI_HZ_BOOT_FAST    25600000
#define SPI_HZ_RW            1100000

/*
** following block planned to move to another file
*/
/**********************************************************************
resource info
**********************************************************************/
#include <linux/mfd/pm8xxx/pm8921.h>
#include "../../arch/arm/mach-msm/board-8064.h"
#include <mach/msm_xo.h>
struct auddsp_env {
	struct device *dev;
	int reset;
	int ready;
	int request;
	int xirq;
	struct msm_xo_voter *aud_dsp_clk;
};

/**********************************************************************
POWER(INCLUDING SPI CONFIG)
**********************************************************************/
void spi_env_ctrl(int runtype, int csno);
#define SPI_CONTROL_OFF       0
#define SPI_CONTROL_ON        1
#define SPI_CONTROL_SHUTDOWN  2
#define SPI_CLIENT_AUDDSP     0

int auddsp_power_ctrl(void *env, enum auddsp_ctrl ctrl)
{
	struct auddsp_env *e = env;

	switch (ctrl) {
	case AUDDSP_CTRL_INIT:
		spi_env_ctrl(SPI_CONTROL_OFF,SPI_CLIENT_AUDDSP);
		break;

	case AUDDSP_CTRL_TERM:
		spi_env_ctrl(SPI_CONTROL_SHUTDOWN,SPI_CLIENT_AUDDSP);
		break;

	case AUDDSP_CTRL_ENABLE:
		spi_env_ctrl(SPI_CONTROL_ON,SPI_CLIENT_AUDDSP);
		break;

	case AUDDSP_CTRL_DISABLE:
		spi_env_ctrl(SPI_CONTROL_OFF,SPI_CLIENT_AUDDSP);
		break;

	default:
		break;
	}
	dev_dbg(e->dev, "%s(): ctrl=%d, succeeded\n", __func__, ctrl);
	return 0;
}

/**********************************************************************
CLOCK
**********************************************************************/
int auddsp_clk_ctrl(void *env, enum auddsp_ctrl ctrl)
{
	struct auddsp_env *e = env;
	int ret;

	switch (ctrl) {
	case AUDDSP_CTRL_INIT:
		e->aud_dsp_clk = msm_xo_get(MSM_XO_TCXO_A2, "aud_dsp_clk");
		if (IS_ERR(e->aud_dsp_clk)) {
			e->aud_dsp_clk = 0;
			dev_err(e->dev, "%s(): ctrl=%d, msm_xo_get(MSM_XO_TCXO_A2) failed\n", __func__, ctrl);
			return -ENODEV;
		}
		break;

	case AUDDSP_CTRL_TERM:
		if (e->aud_dsp_clk) {
			msm_xo_put(e->aud_dsp_clk);
			e->aud_dsp_clk = 0;
		}
		break;

	case AUDDSP_CTRL_ENABLE:
		if (e->aud_dsp_clk) {
			ret = msm_xo_mode_vote(e->aud_dsp_clk, MSM_XO_MODE_ON);
			if (ret < 0) {
				dev_err(e->dev, "%s(): ctrl=%d, msm_xo_mode_vote(MSM_XO_MODE_ON)=%d\n", __func__, ctrl, ret);
				return ret;
			}
		}
		break;

	case AUDDSP_CTRL_DISABLE:
		if (e->aud_dsp_clk) {
			msm_xo_mode_vote(e->aud_dsp_clk, MSM_XO_MODE_OFF);
		}
		break;

	default:
		break;
	}
	dev_dbg(e->dev, "%s(): ctrl=%d, succeeded\n", __func__, ctrl);
	return 0;
}

/**********************************************************************
GPIO
**********************************************************************/
#define	RESET_GPIO         28
#define	RESET_GPIO_CFG     GPIO_CFG(RESET_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
#define	RESET_GPIO_POL     0

#define	READY_GPIO         37
#define	READY_GPIO_CFG     GPIO_CFG(READY_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define	READY_GPIO_POL     1

#define	REQUEST_GPIO       31
#define	REQUEST_GPIO_CFG   GPIO_CFG(REQUEST_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define	REQUEST_GPIO_POL   1

#define	XIRQ_GPIO_PMIC     36
#define	XIRQ_GPIO          PM8921_GPIO_PM_TO_SYS(XIRQ_GPIO_PMIC)
#define	XIRQ_GPIO_POL      0
#define	XIRQ_IRQ           PM8921_GPIO_IRQ(PM8921_IRQ_BASE,XIRQ_GPIO_PMIC)

#define PM_GPIO_CONFIG(_dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{\
	.direction	= _dir, \
	.output_buffer	= _buf, \
	.output_value	= _val, \
	.pull	= _pull, \
	.vin_sel	= _vin, \
	.out_strength	= _out_strength, \
	.function	= _func, \
	.inv_int_pol	= _inv, \
	.disable_pin	= _disable, \
}
#define PM_GPIO_DISABLE \
	PM_GPIO_CONFIG(PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
		0, 0, 0, 1)
#define PM_GPIO_INPUT(_pull) \
	PM_GPIO_CONFIG(PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
		_pull, PM_GPIO_VIN_S4, \
		PM_GPIO_STRENGTH_NO, \
		PM_GPIO_FUNC_NORMAL, 0, 0)

static struct {
	struct pm_gpio idle,active,disabled;
} xirq_pm_config = {
	.idle     = PM_GPIO_INPUT(PM_GPIO_PULL_DN),
	.active   = PM_GPIO_INPUT(PM_GPIO_PULL_UP_30),
	.disabled = PM_GPIO_DISABLE,
};

int auddsp_gpio_ctrl(void *env, enum auddsp_ctrl ctrl)
{
	struct auddsp_env *e = env;
	int ret;

	switch (ctrl) {
	case AUDDSP_CTRL_INIT:
		#define GPIO_TLMM_CONFIG_ENABLE(name) do {\
			ret = gpio_tlmm_config(name##_CFG, GPIO_CFG_ENABLE);\
			if (ret) {\
				dev_err(e->dev, "%s(): ctrl=%d, gpio_tlmm_config(" #name ")=%d\n", __func__, ctrl, ret);\
				return ret;\
			}\
		}while(0)
		GPIO_TLMM_CONFIG_ENABLE(RESET_GPIO);
		GPIO_TLMM_CONFIG_ENABLE(READY_GPIO);
		GPIO_TLMM_CONFIG_ENABLE(REQUEST_GPIO);
		ret = pm8xxx_gpio_config(XIRQ_GPIO,&xirq_pm_config.idle);
		if (ret) {
			dev_err(e->dev, "%s(): ctrl=%d, pm8xxx_gpio_config(XIRQ_GPIO)=%d\n", __func__, ctrl, ret);
			return ret;
		}
		#define GPIO_INIT(pin,name,str,io) do{\
			if ((ret = gpio_request(name, str)) != 0) {\
				dev_err(e->dev, "%s(): ctrl=%d, gpio_request(" #name ") = %d\n", __func__, ctrl, ret);\
				return ret;\
			}\
			if ((ret = io) != 0)  {\
				dev_err(e->dev, "%s(): ctrl=%d, " #io "=%d\n", __func__, ctrl, ret);\
				gpio_free(name);\
				return ret;\
			}\
			e->pin = name;\
		} while(0)
		GPIO_INIT(reset  ,RESET_GPIO  , "DSP reset"  , gpio_direction_output(RESET_GPIO,RESET_GPIO_POL));
		GPIO_INIT(ready  ,READY_GPIO  , "DSP ready"  , gpio_direction_input(READY_GPIO));
		GPIO_INIT(request,REQUEST_GPIO, "DSP request", gpio_direction_input(REQUEST_GPIO));
		GPIO_INIT(xirq   ,XIRQ_GPIO   , "DSP xirq"   , gpio_direction_input(XIRQ_GPIO));
		break;

	case AUDDSP_CTRL_TERM:
		#define GPIO_TERM(pin) do{\
			if (e->pin != -1) {\
				gpio_free(e->pin);\
				e->pin = -1;\
			}\
		}while(0)
		GPIO_TERM(reset);
		GPIO_TERM(ready);
		GPIO_TERM(request);
		GPIO_TERM(xirq);
		gpio_tlmm_config(RESET_GPIO_CFG, GPIO_CFG_DISABLE);
		gpio_tlmm_config(READY_GPIO_CFG, GPIO_CFG_DISABLE);
		gpio_tlmm_config(REQUEST_GPIO_CFG, GPIO_CFG_DISABLE);
		pm8xxx_gpio_config(XIRQ_GPIO,&xirq_pm_config.disabled);
		break;

	case AUDDSP_CTRL_ENABLE:
		ret = pm8xxx_gpio_config(XIRQ_GPIO,&xirq_pm_config.active);
		if (ret) {
			dev_err(e->dev, "%s(): ctrl=%d, pm8xxx_gpio_config(XIRQ_GPIO)=%d\n", __func__, ctrl, ret);
			return ret;
		}
		break;

	case AUDDSP_CTRL_DISABLE:
		pm8xxx_gpio_config(XIRQ_GPIO,&xirq_pm_config.idle);
		break;

	default:
		break;
	}
	dev_dbg(e->dev, "%s(): ctrl=%d, succeeded\n", __func__, ctrl);
	return 0;
}

int auddsp_gpio_get_value(void* env, enum auddsp_gpio gpio)
{
	int ret;
	switch (gpio) {
	case AUDDSP_GPIO_READY:
		ret = gpio_get_value(READY_GPIO);
		break;
	case AUDDSP_GPIO_REQUEST:
		ret = gpio_get_value(REQUEST_GPIO);
		break;
	case AUDDSP_GPIO_XIRQ:
		ret = gpio_get_value(XIRQ_GPIO);
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

void auddsp_gpio_set_value(void* env, enum auddsp_gpio gpio,int value)
{
	switch (gpio) {
	case AUDDSP_GPIO_RESET:
		if (value == AUDDSP_GPIO_VALUE_INACTIVE) {
			value = !RESET_GPIO_POL;
		} else if (value == AUDDSP_GPIO_VALUE_ACTIVE) {
			value = RESET_GPIO_POL;
		}
		gpio_set_value(RESET_GPIO, value);
		break;
	default:
		break;
	}
}

/**********************************************************************
spi communication with exclusion
**********************************************************************/
void fjdev_spi_bus4_softcs_mutex(int enable);
int auddsp_spi_sync(struct spi_device *spi,struct spi_message *message)
{
	int ret;
	fjdev_spi_bus4_softcs_mutex(1);
	ret = spi_sync(spi,message);
	fjdev_spi_bus4_softcs_mutex(0);
	return ret;
}

/**********************************************************************
resource control
**********************************************************************/
void *auddsp_getenv(struct device *dev,struct auddsp_irqinfo *irqinfo)
{
	struct auddsp_env *env = kzalloc(sizeof(*env),GFP_KERNEL);
	if (env) {
		env->dev = dev;
		env->reset = -1;
		env->ready = -1;
		env->request = -1;
		env->xirq = -1;
		env->aud_dsp_clk = 0;

		irqinfo->ready.irq = gpio_to_irq(READY_GPIO);
		irqinfo->ready.pol = READY_GPIO_POL;
		irqinfo->request.irq = gpio_to_irq(REQUEST_GPIO);
		irqinfo->request.pol = REQUEST_GPIO_POL;
		irqinfo->xirq.irq = XIRQ_IRQ;
		irqinfo->xirq.pol = XIRQ_GPIO_POL;
	}
	return env;
}
void auddsp_putenv(void *env)
{
	kfree(env);
}
/*
** above block planned to move to another file
*/

/**********************************************************************
PLATFORM DATA
**********************************************************************/
struct auddsp_priv {

	struct mutex lock;
	struct device *dev;

	struct auddsp_irqinfo irqinfo;
	void *env;

	struct {
		int irq;
		bool irq_enabled;
		bool onboot;
		spinlock_t lock;
		struct spi_message m;
		int spi_sync_ret;
		struct completion completion;
	} pin_ready;
	struct {
		int irq;
		bool irq_enabled;
		spinlock_t lock;
		u8 data[AUDDSP_STATUS_COUNT][AUDDSP_STATUS_SIZE];
		int pos;
		int cnt;
		int retry;
		struct spi_transfer t;
		struct spi_message m;
		int spi_sync_ret[AUDDSP_STATUS_COUNT];
		struct completion completion;
	} pin_request;
	struct {
		int irq;
		struct completion completion;
	} pin_xirq;

	int running;
	int cancel_read;
	int fw_loaded;
	int fw_select;
		#define FW_SELECT_MASK_TH  0x10000
		#define FW_SELECT_MASK_SR  0xffff

	struct {
		struct dfw_binrec *binrec;
		u8 *resp;
		const struct firmware *fw[2];
		const char *fw_type[2];
		int stage;
		int speed;
		u32 status;
		int ret;
	} boot;
};

/**********************************************************************
IRQ
**********************************************************************/
static irqreturn_t auddsp_ready_thread(int irq, void *data)
{
	struct auddsp_priv *priv = data;
	struct spi_device *spi = to_spi_device(priv->dev);
	unsigned long flags;
	int ret;
	u8 c;

	if (!priv->pin_ready.onboot) {
		c = *(const u8*)(list_first_entry(&priv->pin_ready.m.transfers, struct spi_transfer, transfer_list)->tx_buf);
		ret = auddsp_spi_sync(spi, &priv->pin_ready.m);
	} else {
		c = 0;
		ret = 0;
	}

	spin_lock_irqsave(&priv->pin_ready.lock, flags);
	if (priv->pin_ready.irq_enabled) {
		priv->pin_ready.irq_enabled = false;
		disable_irq_nosync(priv->pin_ready.irq);
	}
	priv->pin_ready.spi_sync_ret = ret;
	spin_unlock_irqrestore(&priv->pin_ready.lock, flags);

	complete(&priv->pin_ready.completion);
	dev_dbg(priv->dev,"%s(): IRQ_HANDLED spi_sync=%d tx[0]=0x%02x\n", __func__, ret, c);
	return IRQ_HANDLED;
}

static irqreturn_t auddsp_request_thread(int irq, void *data)
{
	struct auddsp_priv *priv = data;
	struct spi_device *spi = to_spi_device(priv->dev);
	unsigned long flags;
	int pos,ret;

	spi_message_init(&priv->pin_request.m);
	spi_message_add_tail(&priv->pin_request.t, &priv->pin_request.m);

	ret = auddsp_spi_sync(spi, &priv->pin_request.m);

	spin_lock_irqsave(&priv->pin_request.lock, flags);
	if (priv->pin_request.cnt < AUDDSP_STATUS_COUNT) {
		pos = priv->pin_request.pos + priv->pin_request.cnt++;
		if (AUDDSP_STATUS_COUNT <= pos) {
			pos -= AUDDSP_STATUS_COUNT;
		}
	} else {
		pos = priv->pin_request.pos;
		if (AUDDSP_STATUS_COUNT <= ++priv->pin_request.pos) {
			priv->pin_request.pos -= AUDDSP_STATUS_COUNT;
		}
	}
	if (ret) {
		memset(priv->pin_request.data[pos], 0, AUDDSP_STATUS_SIZE);
	} else {
		memcpy(priv->pin_request.data[pos], priv->pin_request.t.rx_buf, AUDDSP_STATUS_SIZE);
	}
	priv->pin_request.spi_sync_ret[pos] = ret;
	spin_unlock_irqrestore(&priv->pin_request.lock, flags);

	complete(&priv->pin_request.completion);
	dev_dbg(priv->dev,"%s(): IRQ_HANDLED spi_sync=%d rx[0]=0x%02x\n", __func__, ret, *(const u8*)priv->pin_request.t.rx_buf);
	return IRQ_HANDLED;
}

static int auddsp_resp_check(struct device *dev,const void *resp, int reclen,
                             u32 *last_status, int stage, int chunk)
{
	int i;
	const u32 *r;
	u32 status;
	bool failed,pll_running;

	r = resp;
	reclen /= sizeof*r;
	stage++;
	pll_running = false;
	for (i = 0, failed = false; i < reclen && !failed; i++) {

		status = be32_to_cpu(r[i]);
		switch (status) {
		case 0xe0e0e0e0:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: ROM error reported in stage 2\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x55555555:
			if (stage != 1) {
				dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: ROM bootloader running in stage 2\n",__func__ ,stage , chunk , reclen , i, status);
				failed = true;
			} else if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: ROM bootloader running\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed0000:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Stage2 loader running\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed0002:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Customer fuses clear\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed0003:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: UART comms enabled\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed0007:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: CODE_HDR packet received\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed0008:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: CODE_DATA packet received\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed0009:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Download complete\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed000c:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Application start\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed000d:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: PLL packet received\n",__func__ ,stage , chunk , reclen , i, status);
			break;

		case 0x0fed000e:
			if (*last_status != status)
				dev_dbg(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: PLL running\n",__func__ ,stage , chunk , reclen , i, status);
			pll_running = true;
			break;

		case 0x0fed0025:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports image too long\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed002c:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports bad SPI packet\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed0032:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports SPI read overflow\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed0033:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports SPI underclock\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed0034:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports bad header packet\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed0035:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports invalid packet type\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed0036:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports data before header error\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed0039:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports invalid PLL packet\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		case 0x0fed003b:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Device reports packet alignment error\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;

		default:
			dev_err(dev, "%s(): stage=%d, chunk=%d, length=%d, offset=%d, code=0x%08x: Unrecognised return code\n",__func__ ,stage , chunk , reclen , i, status);
			failed = true;
			break;
		}
		*last_status = status;
	}
	return failed ? -1 : pll_running ? 1 : 0;
}

#define MAX_FRAME_SIZE    8192
#define DEVICE_ID_WM0011    11
enum dfw_cmd {
	DFW_CMD_FUSE = 0x01,
	DFW_CMD_CODE_HDR,
	DFW_CMD_CODE_DATA,
	DFW_CMD_PLL,
	DFW_CMD_INFO = 0xff
};
struct dfw_binrec {
	u8 cmd;
	u8 length[3];
	u8 address[4];
	u8 data[0];
} __packed;
static irqreturn_t auddsp_xirq_thread(int irq, void *data)
{
	struct auddsp_priv *priv = data;
	struct spi_device *spi = to_spi_device(priv->dev);
	int ret,pos,chunk,reclen;
	struct spi_message m;
	struct spi_transfer t;
	const struct dfw_binrec *binrec_src;
	struct dfw_binrec *binrec;
	u8 *resp;
	const struct firmware *fw;
	const char *fw_type;

	if (2 <= priv->boot.stage) {
		dev_dbg(priv->dev,"%s(): IRQ_HANDLED with nop\n", __func__);
		return IRQ_HANDLED;
	}

	complete(&priv->pin_xirq.completion); /* synchronization on entry */

	if (priv->boot.stage == 0) {
		priv->boot.speed = SPI_HZ_BOOT_SLOW;
		priv->boot.status = 0;
	}

	binrec = priv->boot.binrec;
	resp = priv->boot.resp;
	fw = priv->boot.fw[priv->boot.stage];
	fw_type = priv->boot.fw_type[priv->boot.stage];
	for (pos = chunk = 0;pos < fw->size;pos += reclen, chunk++) {

		binrec_src = (const struct dfw_binrec*)(fw->data + pos);
		reclen = sizeof*binrec_src + binrec_src->length[0] + (binrec_src->length[1] << 8) + (binrec_src->length[2] << 16);
		if (MAX_FRAME_SIZE < reclen) {
			dev_err(priv->dev, "%s(): fw_type=%s, size=%d, chunk=%d, too large chunk\n", __func__, fw_type, reclen, chunk);
			ret = -EINVAL;
			goto abort;
		}
		memcpy(binrec, binrec_src, reclen);

		switch (binrec->cmd) {
		case DFW_CMD_INFO:
			if (reclen != 16 || binrec->data[0] != DEVICE_ID_WM0011) {
				dev_err(priv->dev, "%s(): fw_type=%s, length=%d, id=%d, invalid device info\n", __func__, fw_type, reclen, sizeof*binrec < reclen ? binrec->data[0] : 0);
				ret = -EINVAL;
				goto abort;
			}
			break;
		case DFW_CMD_CODE_HDR:
		case DFW_CMD_CODE_DATA:
		case DFW_CMD_PLL:
			memset(&t, 0, sizeof(t));
			t.len = reclen;
			t.tx_buf = binrec;
			t.rx_buf = resp;
			t.bits_per_word = 8;
			t.speed_hz = priv->boot.speed;
			spi_message_init(&m);
			spi_message_add_tail(&t, &m);
			ret = auddsp_spi_sync(spi, &m);
			if (ret) {
				dev_err(priv->dev, "%s(): spi_sync()=%d\n", __func__, ret);
				goto abort;
			}
			ret = auddsp_resp_check(priv->dev, resp, reclen, &priv->boot.status, priv->boot.stage, chunk);
			if (ret < 0) {
				dev_err(priv->dev, "%s(): illegal spi response\n", __func__);
				ret = -EINVAL;
				goto abort;
			}
			if (ret) {
				priv->boot.speed = SPI_HZ_BOOT_FAST;
				if (spi->max_speed_hz < priv->boot.speed) {
					priv->boot.speed = spi->max_speed_hz;
				}
				dev_dbg(priv->dev,"%s(): spi speed set to %d\n", __func__, priv->boot.speed);
			}
			break;
		default:
			dev_err(priv->dev, "%s(): fw_type=%s, cmd [%d] unavailable, ignored\n", __func__, fw_type, binrec->cmd);
			break;
		}
	}

	priv->boot.stage++;
	dev_dbg(priv->dev,"%s(): IRQ_HANDLED fw_type=%s, size=%d, total chunks=%d, download succeeded\n", __func__, fw_type, fw->size, chunk);
	goto end;

abort:
	priv->boot.ret = ret;
	priv->boot.stage = 2;
end:
	complete(&priv->pin_xirq.completion); /* synchronization on exit */
	return IRQ_HANDLED;
}

static int auddsp_irq_ctrl(struct auddsp_priv *priv, enum auddsp_ctrl ctrl)
{
	int ret,irq,trig;

	switch (ctrl) {
	case AUDDSP_CTRL_INIT:
		priv->pin_ready.irq = -1;
		priv->pin_request.irq = -1;
		priv->pin_xirq.irq = -1;

		irq = priv->irqinfo.ready.irq;
		trig = priv->irqinfo.ready.pol ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
		ret = request_threaded_irq(irq, NULL, auddsp_ready_thread,
		                           trig | IRQF_ONESHOT,
		                           "DSP_READY", priv);
		if (ret) {
			dev_err(priv->dev, "%s(): ctrl=%d, request_threaded_irq(DSP_READY)=%d\n", __func__, ctrl, ret);
			return ret;
		}
		disable_irq(irq);
		priv->pin_ready.irq = irq;
		spin_lock_init(&priv->pin_ready.lock);

		priv->pin_request.t.rx_buf = kzalloc(AUDDSP_STATUS_SIZE, GFP_KERNEL);
		if (!priv->pin_request.t.rx_buf) {
			ret = -ENOMEM;
			dev_err(priv->dev, "%s(): ctrl=%d, ENOMEM\n", __func__, ctrl);
			return ret;
		}
		irq = priv->irqinfo.request.irq;
		trig = priv->irqinfo.request.pol ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
		ret = request_threaded_irq(irq, NULL, auddsp_request_thread,
		                           trig  | IRQF_ONESHOT,
		                           "DSP_REQUEST", priv);
		if (ret) {
			dev_err(priv->dev, "%s(): ctrl=%d, request_threaded_irq(DSP_REQUEST)=%d\n", __func__, ctrl, ret);
			kfree(priv->pin_request.t.rx_buf);
			return ret;
		}
		disable_irq(irq);
		priv->pin_request.irq = irq;
		priv->pin_request.irq_enabled = false;
		priv->pin_request.t.len = AUDDSP_STATUS_SIZE;
		priv->pin_request.t.bits_per_word = 8;
		priv->pin_request.t.speed_hz = SPI_HZ_RW;
		spin_lock_init(&priv->pin_request.lock);

		irq = priv->irqinfo.xirq.irq;
		trig = priv->irqinfo.xirq.pol ? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
		ret = request_threaded_irq(irq, NULL, auddsp_xirq_thread,
		                           trig,
		                           "DSP_XIRQ", priv);
		if (ret) {
			dev_err(priv->dev, "%s(): ctrl=%d, request_threaded_irq(DSP_XIRQ)=%d\n", __func__, ctrl, ret);
			return ret;
		}
		disable_irq(irq);
		priv->pin_xirq.irq = irq;
		break;

	case AUDDSP_CTRL_TERM:
		if (priv->pin_ready.irq != -1) {
			free_irq(priv->pin_ready.irq, priv);
			priv->pin_ready.irq = -1;
		}
		if (priv->pin_request.irq != -1) {
			free_irq(priv->pin_request.irq, priv);
			priv->pin_request.irq = -1;
			kfree(priv->pin_request.t.rx_buf);
		}
		if (priv->pin_xirq.irq != -1) {
			free_irq(priv->pin_xirq.irq, priv);
			priv->pin_xirq.irq = -1;
		}
		break;

	default:
		break;
	}
	dev_dbg(priv->dev, "%s(): ctrl=%d, succeeded\n", __func__, ctrl);
	return 0;
}

/**********************************************************************/
/* macros															  */
/**********************************************************************/
#define	BOOT_IMAGE_1ST         "2b.bin"       /* firmware of 1st stage */
#define	BOOT_IMAGE_2ND_SND     "vw.bin"       /* firmware of 2nd stage */
#define	BOOT_IMAGE_2ND_TH      "vw_th.bin"    /* firmware of 2nd stage */
#define	BOOT_IMAGE_2ND_SND_VP  "vp_vw.bin"    /* firmware of 2nd stage */
#define	BOOT_IMAGE_2ND_TH_VP   "vp_vw_th.bin" /* firmware of 2nd stage */

/**
 *	auddsp_read
 *
 *	read the status from DSP
 *
 *	@param	*filp	[in]	file descriptor
 *	@param	*data	[in]	buffer
 *	@param	len		[in]	max size
 *	@param	*off	[in]	read offset in buffer
 *	@return	>0	bytes read<br>
 *			==0	canceled by Close() is invoked<br>
 *			<0	error
 */
static ssize_t auddsp_read(struct file *filp, char __user *data, size_t len, loff_t *off)
{
	struct auddsp_priv *priv;
	unsigned long flags;
	int ret,pos,cnt;

	if (filp == NULL) {
		pr_err("%s(): unable to enter with EBADF\n", __func__);
		return -EBADF;
	}
	priv = ((struct miscdevice*)(filp->private_data))->parent->platform_data;
	dev_dbg(priv->dev,"%s(): enter\n", __func__);

	if (len < AUDDSP_STATUS_SIZE) {
		dev_err(priv->dev,"%s(): exit with EINVAL\n", __func__);
		return -EINVAL;
	}

	if (priv->cancel_read) {
		dev_dbg(priv->dev,"%s(): exit with EOF before waiting\n", __func__);
		return 0;
	}

	do {
		if (wait_for_completion_interruptible(&priv->pin_request.completion)) {
			/* handle interrupt prior to entering suspend state */
			dev_dbg(priv->dev,"%s(): exit with EINTR\n", __func__);
			return -EINTR;
		}

		if (priv->cancel_read) {
			dev_dbg(priv->dev,"%s(): exit with EOF after waiting\n", __func__);
			return 0;
		}

		spin_lock_irqsave(&priv->pin_request.lock, flags);
		cnt = priv->pin_request.cnt;
		ret = 0;/* foolproof for compiler */
		if (cnt) {
			pos = priv->pin_request.pos;
			ret = copy_to_user(data ,priv->pin_request.data[pos], AUDDSP_STATUS_SIZE);
			if (ret) {
				dev_err(priv->dev,"%s(): copy_to_user()=%d, retry=%d\n", __func__,ret,priv->pin_request.retry);
				priv->pin_request.retry = !priv->pin_request.retry;
			} else {
				ret = priv->pin_request.spi_sync_ret[pos];
				if (ret) {
					dev_err(priv->dev,"%s(): spi_sync()=%d\n", __func__,ret);
				}
				priv->pin_request.retry = 0;
			}
			if (priv->pin_request.retry) {
				complete(&priv->pin_request.completion);
			} else {
				priv->pin_request.cnt--;
				if (AUDDSP_STATUS_COUNT <= ++priv->pin_request.pos) {
					priv->pin_request.pos -= AUDDSP_STATUS_COUNT;
				}
			}
		}
		spin_unlock_irqrestore(&priv->pin_request.lock, flags);
	} while (!cnt);

	if (ret) {
		return -EFAULT;
	}

	dev_dbg(priv->dev,"%s(): exit len=%d, cnt=%d\n", __func__, AUDDSP_STATUS_SIZE, cnt);
	return AUDDSP_STATUS_SIZE;
}

/**
 *	auddsp_write
 *
 *	write() sends command/parameter to DSP.
 *
 *	@param	*filp	[in]	file descriptor
 *	@param	*data	[in]	buffer
 *	@param	len		[in]	data size
 *	@param	*off	[in]	offset in buffer
 *	@return	>0	written size<br>
 *			<=0	error
 */
static ssize_t auddsp_write(struct file *filp, const char __user *data, size_t len, loff_t *off)
{
	struct auddsp_priv *priv;
	u8 *buf = 0;
	struct spi_transfer t;
	int ret,wait;
	unsigned long flags;
	bool di;

	if (filp == NULL) {
		pr_err("%s(): unable to enter with EBADF\n", __func__);
		return -EBADF;
	}
	priv = ((struct miscdevice*)(filp->private_data))->parent->platform_data;

	mutex_lock(&priv->lock);

	dev_dbg(priv->dev,"%s(): enter len=%d, ready=%d, request=%d\n", __func__,len,
	           auddsp_gpio_get_value(priv->env, AUDDSP_GPIO_READY), auddsp_gpio_get_value(priv->env, AUDDSP_GPIO_REQUEST));

	if (!priv->fw_loaded) {
		dev_err(priv->dev,"%s(): exit with EINVAL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		dev_err(priv->dev,"%s(): exit with ENOMEM\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	if (copy_from_user(buf, data, len)) {
		dev_err(priv->dev,"%s(): exit with EFAULT\n", __func__);
		ret = -EFAULT;
		goto err;
	}

	memset(&t, 0, sizeof(t));
	t.tx_buf = buf;
	t.len = len;
	t.bits_per_word = 8;
	t.speed_hz = SPI_HZ_RW;
	spi_message_init(&priv->pin_ready.m);
	spi_message_add_tail(&t, &priv->pin_ready.m);

	init_completion(&priv->pin_ready.completion);
	priv->pin_ready.onboot = false;
	priv->pin_ready.irq_enabled = true;
	enable_irq(priv->pin_ready.irq);

	wait = wait_for_completion_timeout(&priv->pin_ready.completion,msecs_to_jiffies(100));

	spin_lock_irqsave(&priv->pin_ready.lock, flags);
	di = false;
	if (priv->pin_ready.irq_enabled) {
		priv->pin_ready.irq_enabled = false;
		di = true;
		disable_irq_nosync(priv->pin_ready.irq);
	}
	ret = priv->pin_ready.spi_sync_ret;
	spin_unlock_irqrestore(&priv->pin_ready.lock, flags);
	if (di) {
		synchronize_irq(priv->pin_ready.irq);
	}

	if (!wait) {
		dev_err(priv->dev,"%s(): exit with ETIMEDOUT in 100ms period ready=%d, request=%d\n",__func__,
		        auddsp_gpio_get_value(priv->env, AUDDSP_GPIO_READY), auddsp_gpio_get_value(priv->env, AUDDSP_GPIO_REQUEST));
		ret = -ETIMEDOUT;
		goto err;
	}

	if (ret) {
		dev_err(priv->dev,"%s(): spi_sync()=%d\n",__func__,ret);
		goto err;
	}

	dev_dbg(priv->dev,"%s(): exit successfully len=%d\n", __func__, len);
	ret = len;

err:
	kfree(buf);
	mutex_unlock(&priv->lock);
	return ret;
}

/**
 *	auddsp_open
 *
 *	initialize powers,clocks,ios and variables
 *
 *	@param	inode	[in]	inode
 *	@param	filp	[in]	filp
 *	@return	status<br>
 *			==0	OK<br>
 *			<0	error
 */
static int auddsp_open(struct inode *inode, struct file *filp)
{
	struct auddsp_priv *priv;
	int	ret;

	if (filp == NULL) {
		pr_err("%s(): unable to enter with EBADF\n", __func__);
		return -EBADF;
	}
	priv = ((struct miscdevice*)(filp->private_data))->parent->platform_data;

	if (!try_module_get(THIS_MODULE)) {
		dev_err(priv->dev,"%s(): exit with ENODEV\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&priv->lock);

	/* runnning == 0 means enabled first */
	if (!priv->running++) {

		ret = auddsp_power_ctrl(priv->env, AUDDSP_CTRL_ENABLE);
		if (ret) {
			dev_err(priv->dev,"%s(): auddsp_power_ctrl(ENABLE)=%d\n",__func__, ret);
			goto epower;
		}
		ret = auddsp_clk_ctrl(priv->env, AUDDSP_CTRL_ENABLE);
		if (ret) {
			dev_err(priv->dev,"%s(): auddsp_clk_ctrl(ENABLE)=%d\n",__func__, ret);
			goto eclk;
		}
		ret = auddsp_gpio_ctrl(priv->env,AUDDSP_CTRL_ENABLE);
		if (ret) {
			dev_err(priv->dev,"%s(): auddsp_gpio_ctrl(ENABLE)=%d\n",__func__, ret);
			goto egpio;
		}
		msleep(10);

		priv->cancel_read = 0;
		priv->pin_request.pos = 0;
		priv->pin_request.cnt = 0;
		priv->pin_request.retry = 0;
		init_completion(&priv->pin_request.completion);

		dev_dbg(priv->dev,"%s(): succeeded\n", __func__);
	} else {
		dev_dbg(priv->dev,"%s(): succeeded with no operation\n", __func__);
	}
	ret = 0;
	goto end;

egpio:
	auddsp_clk_ctrl(priv->env,AUDDSP_CTRL_DISABLE);
eclk:
	auddsp_power_ctrl(priv->env, AUDDSP_CTRL_DISABLE);
epower:
	priv->running--;
end:
	mutex_unlock(&priv->lock);
	return ret;
}

/**
 *	auddsp_release
 *
 *	disable DSP<br>
 *
 *	@param	inode	[in]	inode
 *	@param	filp	[in]	filp
 *	@return	error code<br>
 *			0	success<br>
 *			<0	fail
 */
static int auddsp_release(struct inode *inode, struct file *filp)
{
	struct auddsp_priv *priv;

	if (filp == NULL) {
		pr_err("%s(): unable to enter with EBADF\n", __func__);
		return -EBADF;
	}
	priv = ((struct miscdevice*)(filp->private_data))->parent->platform_data;

	mutex_lock(&priv->lock);

	if (priv->running == 1) {

		max9867stop();

		if (priv->pin_request.irq_enabled) {
			priv->pin_request.irq_enabled = false;
			disable_irq(priv->pin_request.irq);
		}
		priv->fw_loaded = 0;

		auddsp_gpio_set_value(priv->env, AUDDSP_GPIO_RESET, AUDDSP_GPIO_VALUE_ACTIVE);

		auddsp_gpio_ctrl(priv->env, AUDDSP_CTRL_DISABLE);
		auddsp_clk_ctrl(priv->env, AUDDSP_CTRL_DISABLE);
		auddsp_power_ctrl(priv->env, AUDDSP_CTRL_DISABLE);

		module_put(THIS_MODULE);

		dev_dbg(priv->dev,"%s(): succeeded\n", __func__);
	} else {
		dev_dbg(priv->dev,"%s(): succeeded with no operation\n", __func__);
	}
	priv->running--;

	mutex_unlock(&priv->lock);
	return 0;
}

/**
 * auddsp_boot
 *
 *	load the firmware
 *
 */
static int auddsp_boot(struct auddsp_priv *priv)
{
	int ret;
	unsigned long flags;
	bool di;

	struct fw_info {
		const char *name;
		const char *title;
	};
	const struct fw_info boot = {
		BOOT_IMAGE_1ST, "bootloader",
	};
	const struct fw_info voice[][2] = {
		{{BOOT_IMAGE_2ND_SND   , "effective for CScall"},
		 {BOOT_IMAGE_2ND_TH    , "direct for CScall"}},
		{{BOOT_IMAGE_2ND_SND_VP, "effective for VoIP"},
		 {BOOT_IMAGE_2ND_TH_VP , "direct for VoIP"}},
	};
	const struct fw_info *fw_info;

	if (priv->pin_request.irq_enabled) {
		priv->pin_request.irq_enabled = false;
		disable_irq(priv->pin_request.irq);
	}
	priv->fw_loaded = 0;

	auddsp_gpio_set_value(priv->env, AUDDSP_GPIO_RESET, AUDDSP_GPIO_VALUE_ACTIVE);

	priv->boot.fw[0] = priv->boot.fw[1] = 0;
	priv->boot.binrec = kzalloc(MAX_FRAME_SIZE, GFP_KERNEL);
	priv->boot.resp = kzalloc(MAX_FRAME_SIZE, GFP_KERNEL);
	if (!priv->boot.binrec || !priv->boot.resp) {
		dev_err(priv->dev,"%s(): cannot allocate workmemory\n", __func__);
		ret = -ENOMEM;
		goto abort;
	}
	fw_info = &boot;
	ret = request_firmware(&priv->boot.fw[0], fw_info->name , priv->dev);
	if (ret) {
		dev_err(priv->dev, "%s(): request_firmware(%s)=%d\n", __func__, fw_info->title, ret);
		goto abort;
	}
	priv->boot.fw_type[0] = fw_info->title;
	fw_info = &voice[(priv->fw_select & FW_SELECT_MASK_SR) == 16]
	                [(priv->fw_select & FW_SELECT_MASK_TH) != 0];
	ret = request_firmware(&priv->boot.fw[1], fw_info->name , priv->dev);
	if (ret) {
		dev_err(priv->dev, "%s(): request_firmware(%s)=%d\n", __func__, fw_info->title, ret);
		goto abort;
	}
	priv->boot.fw_type[1] = fw_info->title;
	priv->boot.stage = 0;
	priv->boot.ret = 0;

	init_completion(&priv->pin_xirq.completion);
	enable_irq(priv->pin_xirq.irq);

	init_completion(&priv->pin_ready.completion);
	priv->pin_ready.onboot = true;
	priv->pin_ready.irq_enabled = true;
	enable_irq(priv->pin_ready.irq);

	auddsp_gpio_set_value(priv->env, AUDDSP_GPIO_RESET, AUDDSP_GPIO_VALUE_INACTIVE);

	/* fw downloading processed in auddsp_xirq_thread twice */

	for (flags = 0;flags < 2;flags++) {
		/* synchronization on entry */
		if (!wait_for_completion_timeout(&priv->pin_xirq.completion,msecs_to_jiffies(100))) {
			dev_err(priv->dev,"%s(): XIRQ no response in 100 ms\n", __func__);
			ret = -ETIMEDOUT;
			goto abort;
		}
		/* synchronization on exit */
		wait_for_completion(&priv->pin_xirq.completion);
		if (priv->boot.ret) {
			ret = priv->boot.ret;
			goto abort;
		}
	}
	if (!wait_for_completion_timeout(&priv->pin_ready.completion,msecs_to_jiffies(200))) {
		dev_err(priv->dev,"%s(): READY no response in 200 ms\n", __func__);
		ret = -ETIMEDOUT;
		goto abort;
	}
	ret = 0;
	priv->fw_loaded = 1;
	priv->pin_request.irq_enabled = true;
	enable_irq(priv->pin_request.irq);
	dev_dbg(priv->dev,"%s(): download succeeded\n", __func__);

abort:
	disable_irq(priv->pin_xirq.irq);

	spin_lock_irqsave(&priv->pin_ready.lock, flags);
	di = false;
	if (priv->pin_ready.irq_enabled) {
		priv->pin_ready.irq_enabled = false;
		di = true;
		disable_irq_nosync(priv->pin_ready.irq);
	}
	spin_unlock_irqrestore(&priv->pin_ready.lock, flags);
	if (di) {
		synchronize_irq(priv->pin_ready.irq);
	}

	release_firmware(priv->boot.fw[0]);
	release_firmware(priv->boot.fw[1]);
	kfree(priv->boot.resp);
	kfree(priv->boot.binrec);

	if (ret) {
		auddsp_gpio_set_value(priv->env, AUDDSP_GPIO_RESET, AUDDSP_GPIO_VALUE_ACTIVE);
	}

	return ret;
}

/**
 *	auddsp_ioctl
 *
 *	control DSP<br>
 *
 *	@param	filp			[in]	filp
 *	@param	unsigned int	[in]	cmd
 *									WAVEIF_CANCEL_READ : cancel read()
 *	@param	unsigned long	[in]	arg
 *	@return	error code<br>
 *			0	success<br>
 *			<0	fail
 */
static long auddsp_ioctl(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	struct auddsp_priv *priv;
	int ret = 0;

	if (filp == NULL) {
		pr_err("%s(): unable to enter with EBADF\n", __func__);
		return -EBADF;
	}
	priv = ((struct miscdevice*)(filp->private_data))->parent->platform_data;

	mutex_lock(&priv->lock);

	switch (cmd) {
	case WAVEIF_CANCEL_READ:
		dev_dbg(priv->dev,"%s(): succeeded WAVEIF_CANCEL_READ\n", __func__);
		priv->cancel_read = 1;		/* mark cancel request */
		complete(&priv->pin_request.completion);
		break;
	case WAVEIF_LOAD_FIRM:
		priv->fw_select = (int)arg;
		max9867start((priv->fw_select & FW_SELECT_MASK_SR) == 16 ? 1 : 0);
		if ((ret = auddsp_boot(priv)) != 0) {
			dev_err(priv->dev,"%s(): WAVEIF_LOAD_FIRM fw_select=%d, auddsp_boot()=%d\n", __func__, priv->fw_select, ret);
			max9867stop();
			ret = -ENOMEM;
		} else {
			dev_dbg(priv->dev,"%s(): WAVEIF_LOAD_FIRM fw_select=%d, succeeded\n", __func__, priv->fw_select);
		}
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&priv->lock);
	return  ret;
}

/* auddsp driver FOPS */
static struct file_operations auddsp_fops = {
	.read			= auddsp_read,				/* read Entry */
	.write			= auddsp_write,				/* write Entry */
	.unlocked_ioctl	= auddsp_ioctl,				/* ioctl Entry */
	.open			= auddsp_open,				/* open Entry */
	.release		= auddsp_release,			/* release Entry */
};

/* driver definition */
static struct miscdevice auddsp_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,			/* auto */
	.name = "auddsp",						/* Driver name */
	.fops = &auddsp_fops,					/* FOPS */
};

/**
 *	auddsp_spi_probe
 *
 *	Probe<br>
 *	initialize DSP
 *
 *	@param	spi	[in]	spi_device
 *	@return	0	success<br>
 *			<0	fail
 */
static int __devinit auddsp_spi_probe(struct spi_device *spi)
{
	int ret;
	struct auddsp_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&spi->dev, "%s(): kzalloc(sizeof(*priv))=ENOMEM\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&priv->lock);
	priv->dev = &spi->dev;
	spi->dev.platform_data = priv;

	priv->env = auddsp_getenv(priv->dev, &priv->irqinfo);
	if (!priv->env) {
		dev_err(&spi->dev, "%s(): auddsp_getenv()=ENOMEM\n", __func__);
		ret = -ENOMEM;
		goto eenv;
	}

	ret = auddsp_power_ctrl(priv->env, AUDDSP_CTRL_INIT);
	if (ret) {
		dev_err(&spi->dev, "%s(): auddsp_power_ctrl(INIT)=%d\n", __func__, ret);
		goto epower;
	}
	ret = auddsp_clk_ctrl(priv->env, AUDDSP_CTRL_INIT);
	if (ret) {
		dev_err(&spi->dev, "%s(): auddsp_clk_ctrl(INIT)=%d\n", __func__, ret);
		goto eclk;
	}
	ret = auddsp_gpio_ctrl(priv->env, AUDDSP_CTRL_INIT);
	if (ret) {
		dev_err(&spi->dev, "%s(): auddsp_gpio_ctrl(INIT)=%d\n", __func__, ret);
		goto egpio;
	}
	ret = auddsp_irq_ctrl(priv, AUDDSP_CTRL_INIT);
	if (ret) {
		dev_err(&spi->dev, "%s(): auddsp_irq_ctrl(INIT)=%d\n", __func__, ret);
		goto eirq;
	}

	auddsp_miscdev.parent = &spi->dev;
	ret = misc_register(&auddsp_miscdev);
	if (ret < 0) {
		dev_err(&spi->dev, "%s(): misc_register()=%d\n", __func__, ret);
		goto ereg;
	}

/* FUJITSU:2013-04-04 ADD start */
	max9867vddon();
/* FUJITSU:2013-04-04 ADD end */

	dev_dbg(&spi->dev, "%s(): succeeded\n",__func__);
	return 0;

ereg:
	auddsp_irq_ctrl(priv, AUDDSP_CTRL_TERM);
eirq:
	auddsp_gpio_ctrl(priv->env, AUDDSP_CTRL_TERM);
egpio:
	auddsp_clk_ctrl(priv->env, AUDDSP_CTRL_TERM);
eclk:
	auddsp_power_ctrl(priv->env, AUDDSP_CTRL_TERM);
epower:
	auddsp_putenv(priv->env);
eenv:
	kfree(priv);
	spi->dev.platform_data = 0;
	return ret;
}

/**
 *	auddsp_spi_remove
 *
 *	remove<br>
 *	deregister driver it was registered in Probe
 *
 *	@param	spi	[in]	spi_device*
 *	@return	0	success<br>
 */
static int __devexit auddsp_spi_remove(struct spi_device *spi)
{
	struct auddsp_priv *priv = spi->dev.platform_data;

	if (priv) {
		auddsp_irq_ctrl(priv, AUDDSP_CTRL_TERM);
		auddsp_gpio_ctrl(priv->env, AUDDSP_CTRL_TERM);
		auddsp_clk_ctrl(priv->env, AUDDSP_CTRL_TERM);
		auddsp_power_ctrl(priv->env, AUDDSP_CTRL_TERM);
		auddsp_putenv(priv->env);
		kfree(priv);
		spi->dev.platform_data = 0;
	}

	misc_deregister(&auddsp_miscdev);

	return 0;
}

/*
 *	auddsp_suspend
 *
 *	goto suspend
 *
 *	@param	spi	[in]		spi_device*
 *	@param	mesg	[in]	pm_message_t
 *	@return	0	success
 */
static int auddsp_suspend(struct spi_device *spi, pm_message_t mesg)
{
	struct auddsp_priv *priv = spi->dev.platform_data;
	if (priv) {
		mutex_lock(&priv->lock);
		if (priv->pin_request.irq_enabled) {
			priv->pin_request.irq_enabled = false;
			disable_irq(priv->pin_request.irq);
			dev_dbg(priv->dev,"%s(): suspended\n", __func__);
		}
		mutex_unlock(&priv->lock);
	}
	return 0;
}

/*
 *	auddsp_resume
 *
 *	resume from suspend
 *
 *	@param	spi	[in]	spi_device*
 *	@return	0	success
 */
static int auddsp_resume(struct spi_device *spi)
{
	struct auddsp_priv *priv = spi->dev.platform_data;
	if (priv) {
		mutex_lock(&priv->lock);
		if (priv->fw_loaded && !priv->pin_request.irq_enabled) {
			priv->pin_request.irq_enabled = true;
			enable_irq(priv->pin_request.irq);
			dev_dbg(priv->dev,"%s(): resumed\n", __func__);
		}
		mutex_unlock(&priv->lock);
	}
	return 0;
}

/* auddsp driver as spi child driver */
static struct spi_driver auddsp_spi_driver = {
	.driver = {
		.name	= "wolfson_auddsp",						/* device object which connect to */
		.bus 	= &spi_bus_type,						/* driver bus type */
		.owner	= THIS_MODULE,							/* owner */
	},
	.probe		= auddsp_spi_probe,						/* Probe Entry */
	.remove		= __devexit_p(auddsp_spi_remove),		/* Remove Entry */
	.suspend	= auddsp_suspend,						/* Suspend Entry */
	.resume		= auddsp_resume,						/* Resume Entry */
};

module_spi_driver(auddsp_spi_driver)

MODULE_DESCRIPTION("ASoC AUDDSP driver");
MODULE_AUTHOR("Fujitsu");
MODULE_LICENSE("GPL");
