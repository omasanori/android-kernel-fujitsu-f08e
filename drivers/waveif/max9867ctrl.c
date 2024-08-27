/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock_types.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h> /* FUJITSU:2012-07-19 1120to1311 ADD */
/* FUJITSU:2012-07-19 1120to1311 move start */
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
/* FUJITSU:2012-07-19 1120to1311 move end */

#include "max9867ctrl.h"

/*********** register config ***********/
#define M_8K  1
#define M_16K 2
#define M_ANY (M_8K | M_16K)
struct reg_setting {
    int apply_mask;
    int reg;
    int val;
};
static const struct reg_setting reg_setting_seq_start[] = {
    {M_ANY, 0x17, 0x0A},         /* device is in shutdown */
    {M_8K,  0x05, 0x1E},         /* System Clock 8k */
    {M_16K, 0x05, 0x1F},         /* System Clock 16k */
    {M_ANY, 0x06, 0x80},         /* Stereo Audio clock H */
    {M_ANY, 0x07, 0x00},         /* Stereo Audio clock L */
    {M_ANY, 0x08, 0x18},         /* Configure Digital Audio Interface */
    {M_ANY, 0x09, 0x00},         /* Configure Digital Audio Interface */
    {M_ANY, 0x0A, 0x00},         /* Codec Filters */
//    {M_ANY, 0x14, 0x40},         /* ADC Input */
    {M_ANY, 0x14, 0x4E},         /* ADC Input */
    {M_ANY, 0x15, 0x00},         /* Microphone */
    {M_ANY, 0x16, 0xAD},         /* Mode */
    {M_ANY, 0x10, 0x49},         /* Left Volume Control(Mute) */
    {M_ANY, 0x11, 0x49},         /* Right Volume Control(Mute) */
    {M_ANY, 0x12, 0x1F},         /* Left Microphone Gain(Mute) */
    {M_ANY, 0x13, 0x1F},         /* Right Microphone Gain(Mute) */
    {M_ANY, 0x0B, 0x00},         /* Sidetone */
    {M_ANY, 0x0C, 0x00},         /* DAC Level */
    {M_ANY, 0x0D, 0xBF},         /* ADC Level */
    {M_ANY, 0x0E, 0x4F},         /* Left-Line Input Level */
    {M_ANY, 0x0F, 0x4F},         /* Right-Line Input Level */
//    {M_ANY, 0x17, 0x8A},         /* device is in active */
    {M_ANY, 0x17, 0x8B},         /* device is in active */
    {M_ANY, 0xFF,   10},         /* wait 10ms */
    {M_ANY, 0x10, 0x09},         /* Left Volume Control */
    {M_ANY, 0x11, 0x09},         /* Right Volume Control */
//    {M_ANY, 0x12, 0x34},         /* Left Microphone Gain */
    {M_ANY, 0x12, 0x2E},         /* Left Microphone Gain */
    {M_ANY, 0x13, 0x1F},         /* Right Microphone Gain */
};
static const struct reg_setting reg_setting_seq_stop[] = {
    {M_ANY, 0x10, 0x7F},         /* Left Volume Control(Mute) */
    {M_ANY, 0x11, 0x7F},         /* Right Volume Control(Mute) */
    {M_ANY, 0x12, 0x1F},         /* Left Microphone Gain(Mute) */
    {M_ANY, 0x13, 0x1F},         /* Left Microphone Gain(Mute) */
    {M_ANY, 0xFF,   10},         /* wait 10ms */
    {M_ANY, 0x17, 0x00},         /* device is in shutdown */
};

/* I2C settings */
#define I2C_SLAVE_ADDR	(0x18) /* responds to the slave address 0x30 for write command and 0x31 for read ope. */
#define I2C_BUSNUM		(3)    /* i2c bus number */

static int i2c_reg_write(int reg, unsigned char value){
    int ret = 0,i;
    struct i2c_adapter *i2c;
    struct i2c_msg msg[1];
    unsigned char buf[2];

    msg[0].addr    = I2C_SLAVE_ADDR;
    msg[0].buf     = buf;
    msg[0].len     = 2;
    msg[0].flags   = 0;
   
    i2c = i2c_get_adapter(I2C_BUSNUM);
    if(i2c == NULL){ printk(KERN_ERR "I2C get_adapter ERROR\n"); return -1; }

    buf[0] = reg;
    buf[1] = value;

    for(i=0;i<3;i++){
        ret = i2c_transfer(i2c, msg, 1);
        if(ret >= 0) break;
        printk(KERN_ERR "I2C ERROR ret=%d retry=%d\n",ret,i);
        if(i==2) return -1;
    }
    printk(KERN_WARNING "max9867reg %x %x\n",reg,value);
    return 0;
}

static void i2c_reg_check(void){
    int ret = 0, i;
    struct i2c_adapter *i2c;
    struct i2c_msg msg[2];
    unsigned char buf[2];
    unsigned char rbuf[0x18];

    msg[0].addr    = I2C_SLAVE_ADDR;
    msg[0].buf     = buf;
    msg[0].len     = 1;
    msg[0].flags   = 0;

    msg[1].addr    = I2C_SLAVE_ADDR;
    msg[1].buf     = rbuf;
    msg[1].len     = 1;
    msg[1].flags   = 1;
   
    i2c = i2c_get_adapter(I2C_BUSNUM);
    if(i2c == NULL){ printk(KERN_ERR "I2C get_adapter ERROR\n"); return; }

    for(i=0;i<0x18;i++){
        buf[0] = i/*+1 bug!!*/;
        msg[1].buf = rbuf+i;
        ret = i2c_transfer(i2c, msg, 2);
        if(ret < 0){ printk(KERN_ERR "I2C ERROR %x ret=%d\n",i,ret); return; }
    }
    printk(KERN_WARNING "+++++++ max9867 reg +++++++\n");
    printk(KERN_WARNING "STATUS %x %x %x %x %x\n",rbuf[0],rbuf[1],rbuf[2],rbuf[3],rbuf[4]);
    printk(KERN_WARNING "CLOCK CONTROL %x %x %x\n",rbuf[5],rbuf[6],rbuf[7]);
    printk(KERN_WARNING "DIGITAL AUDIO INTERFACE %x %x\n",rbuf[8],rbuf[9]);
    printk(KERN_WARNING "DIGITAL FILTER %x\n",rbuf[0xA]);
    printk(KERN_WARNING "LEVEL CONTROL %x %x %x %x %x\n",rbuf[0xB],rbuf[0xC],rbuf[0xD],rbuf[0xE],rbuf[0xF]);
    printk(KERN_WARNING "LEVEL CONTROL %x %x %x %x\n",rbuf[0x10],rbuf[0x11],rbuf[0x12],rbuf[0x13]);
    printk(KERN_WARNING "CONFIGURATION %x %x %x\n",rbuf[0x14],rbuf[0x15],rbuf[0x16]);
    printk(KERN_WARNING "POWER MANAGEMENT %x\n",rbuf[0x17]);
}

static void setting_sequencer(const struct reg_setting* seq, int len, int mask){
	int i;
	for (i = 0;i < len;i++) {
		if (seq[i].apply_mask & mask) {
			if (seq[i].reg == 0xFF) {
				msleep(seq[i].val);
			} else {
				i2c_reg_write(seq[i].reg, seq[i].val);
			}
		}
	}
}

static int reg_setting_mask = 0;

void max9867start(int conf_num){
	int mask = conf_num ? M_16K : M_8K;
	if (reg_setting_mask != mask) {
		if (reg_setting_mask) {
			setting_sequencer(reg_setting_seq_stop,ARRAY_SIZE(reg_setting_seq_stop),reg_setting_mask);
		}
		reg_setting_mask = mask;
		setting_sequencer(reg_setting_seq_start,ARRAY_SIZE(reg_setting_seq_start),reg_setting_mask);
	}
}

void max9867stop(){
	if (reg_setting_mask) {
		setting_sequencer(reg_setting_seq_stop,ARRAY_SIZE(reg_setting_seq_stop),reg_setting_mask);
		reg_setting_mask = 0;
	}
}

void max9867debug(){
	int i,ret;
	struct regulator *pm_18v_auddbv;
	struct clk* clkin;

	printk(KERN_WARNING "max9867debug start\n");

	pm_18v_auddbv = regulator_get(NULL, "8921_l9");
	if (IS_ERR(pm_18v_auddbv)) {
		printk(KERN_ERR "Failed regulator_get(8921_l9) = %d\n", (int)(pm_18v_auddbv));
		return;
	}
	ret = regulator_set_voltage(pm_18v_auddbv, 1800000, 1800000);
	if (ret) {
		printk(KERN_ERR "Failed regulator_set_voltage(pm_18v_auddbv) = %d\n", ret);
		regulator_put(pm_18v_auddbv);
		return;
	}
	clkin = clk_get(NULL, "gp2_clk");
	if (IS_ERR(clkin)) {
		printk(KERN_ERR "ERROR: clk_get(gp2_clk)\n");
		return;
	}
	ret = clk_set_rate(clkin, 19200000);
	if (ret) {
		printk(KERN_ERR "ERROR: clk_set_rate(clkin, %d) = %d\n", 19200000, ret);
		return;
	}

	for(i=0;i<5;i++){
		printk(KERN_WARNING "test roop %d\n",i);
		/* power on PM8921 VERG_L9 */
		ret = regulator_enable(pm_18v_auddbv);
		if (ret) {
			printk(KERN_ERR "Failed regulator_enable(pm_18v_auddbv) ret=%d\n", ret);
			return;
		}
		/* clock on APQ8064(GP_CLK_2A) */
		ret = clk_prepare_enable(clkin);
		if (ret) {
			printk(KERN_ERR "ERROR: clk_enable(clkin) = %d\n", ret);
			return;
		}

		/* test1 */
		i2c_reg_check();
		max9867start(0);
		i2c_reg_check();
		/* test2 */
		max9867stop();
		i2c_reg_check();
		/* clock off */
		clk_disable_unprepare(clkin);
		/* power off */
		ret = regulator_disable(pm_18v_auddbv);
		if (ret) {
			printk(KERN_ERR "Failed regulator_disable(pm_18v_auddbv) ret=%d\n", ret);
			return;
		}
	}
	printk(KERN_WARNING "max9867debug end\n");
}
/* FUJITSU:2013-04-04 ADD start */
#include <asm/system.h>
void max9867vddon(void)
{
	if ((system_rev & 0xf0) == 0x50/*can*/ &&
		(system_rev & 0x0f) >= 0x06/*2.4*/) {
		struct regulator *regulator = regulator_get(NULL,"8921_lvs4");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s: Failed regulator_get()=%d\n", __func__, (int)regulator);
		} else {
			int ret = regulator_set_voltage(regulator, 1800000, 1800000);
			if (ret) {
				printk(KERN_ERR "%s: Failed regulator_set_voltage()=%d\n" ,__func__, ret);
			}
			ret = regulator_enable(regulator);
			if (ret) {
				printk(KERN_ERR "%s: Failed regulator_enable()=%d\n", __func__, ret);
			}
		}
	} else if ((system_rev & 0xf0) == 0x60/*tha*/ &&
			   (system_rev & 0x0f) >= 0x03/*2.3*/) {
		struct regulator *regulator = regulator_get(NULL,"8921_l16");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s: Failed regulator_get()=%d\n", __func__, (int)regulator);
		} else {
			int ret = regulator_set_voltage(regulator, 1800000, 1800000);
			if (ret) {
				printk(KERN_ERR "%s: Failed regulator_set_voltage()=%d\n" ,__func__, ret);
			}
			ret = regulator_enable(regulator);
			if (ret) {
				printk(KERN_ERR "%s: Failed regulator_enable()=%d\n", __func__, ret);
			}
		}
	} else if ((system_rev & 0xf0) == 0x10/*him*/ &&
			   (system_rev & 0x0f) >= 0x02/*1.2*/) {
		struct regulator *regulator = regulator_get(NULL,"8921_lvs4");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "%s: Failed regulator_get()=%d\n", __func__, (int)regulator);
		} else {
			int ret = regulator_set_voltage(regulator, 1800000, 1800000);
			if (ret) {
				printk(KERN_ERR "%s: Failed regulator_set_voltage()=%d\n" ,__func__, ret);
			}
			ret = regulator_enable(regulator);
			if (ret) {
				printk(KERN_ERR "%s: Failed regulator_enable()=%d\n", __func__, ret);
			}
		}
	}
}
/* FUJITSU:2013-04-04 ADD end */

MODULE_DESCRIPTION("Max9867 driver");
MODULE_AUTHOR("Fujitsu");
MODULE_LICENSE("GPL");
