/*
 * Copyright(C) 2012-2013 FUJITSU LIMITED
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


#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <linux/regulator/consumer.h>
#include "../../../arch/arm/mach-msm/devices.h"
#include "msm_serial_aux.h"
#include "msm_serial_aux_hw.h"

/* Device name definition */
#define DEV_NAME    MSM_SERIAL_AUX_DEV_NAME

/* <PMIC configuration>------------------------------------------------------*/
/* [GPIO_37] IrDA POWER On/Off */
struct pm_gpio irda_pwdown_37 = {
 .direction      = PM_GPIO_DIR_OUT,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 1,
};

/* [GPIO_21] IrDA UART Switch */
struct pm_gpio pmic_uart1_irda_out_en_21 = {
 .direction      = PM_GPIO_DIR_OUT,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_2,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* [GPIO_33] IrDA UART Switch */
struct pm_gpio pmic_uart1_irda_in_en_33 = {
 .direction      = PM_GPIO_DIR_IN,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_1,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_NO,
 .output_value   = 0,
};

/* [GPIO_8] APQ UART Switch */
struct pm_gpio uart_pmic_txd_en_8 = {
 .direction      = PM_GPIO_DIR_IN,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_2,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* [GPIO_38] APQ UART Switch */
struct pm_gpio uart_pmic_rxd_en_38 = {
 .direction      = PM_GPIO_DIR_OUT,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_2,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

static unsigned long uart_membase;

static struct regulator *vreg_l17;
static char *irda_vreg_name = "irda_l17";
static int irda_vreg_on = 0;

/* Prototype declaration */
static int msm_uart_aux_hw_power(int on, int min_microvolts );

/*
 * (HW) msm_uart_aux_hw_uart_enable
 *
 *  @return     0       Success
 */
int msm_uart_aux_hw_uart_enable( void )
{
    /* set IRDA_EN=1 */
    iowrite32( 0x1, (void *)(uart_membase + UART_IRDA) );
    printk( KERN_INFO DEV_NAME ": %s set IRDA_EN=1\n", __func__ );

    return 0;
}

/*
 * (HW) msm_uart_aux_hw_uart_disable
 *
 *  @return     0       Success
 */
int msm_uart_aux_hw_uart_disable( void )
{
    /* set IRDA_EN=0 */
    iowrite32( 0x0, (void *)(uart_membase + UART_IRDA) );
    printk( KERN_INFO DEV_NAME ": %s set IRDA_EN=0\n", __func__ );

    return 0;
}

/*
 * (HW) msm_uart_aux_hw_chg_irda
 *
 *  @return     0           Success
 *  @return     -EIO        Setting Error
 */
int msm_uart_aux_hw_chg_irda( void )
{
    int rc = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    /* TX */
    pmic_uart1_irda_out_en_21.function = PM_GPIO_FUNC_2;
    rc = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_UART1_IRDA_OUT_21)
                                                , &pmic_uart1_irda_out_en_21 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[21] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s PM8921_UART1_IRDA_OUT config failed ret = %d\n", __func__, rc );
        return -EIO;
    }
    /* RX */
    rc = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_UART1_IRDA_IN_33)
                                                , &pmic_uart1_irda_in_en_33 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[33] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s PM8921_UART1_IRDA_IN config failed ret = %d\n", __func__, rc );
        return -EIO;
    }
    /* PMIC */
    rc = pm8xxx_uart_gpio_mux_ctrl( UART_TX1_RX1 );
    pr_debug( DEV_NAME ": %s pm8xxx_uart_gpio_mux_ctrl ret(%d)\n", __func__, rc );
    if (unlikely(rc)) {
        pr_err( DEV_NAME ": %s pm8xxx_uart_gpio_mux_ctrl failed ret = %d\n", __func__, rc );
        return -EIO;
    }
    printk( KERN_INFO DEV_NAME ": %s UART Change -> IRDA\n", __func__ );
    /* TXD */
    rc = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_UART_PMIC_TXD_8)
                                                , &uart_pmic_txd_en_8 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[08] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s PM8921_UART_PMIC_TXD config failed ret = %d\n", __func__, rc );
        return -EIO;
    }
    /* RXD */
    rc = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_UART_PMIC_RXD_38)
                                                , &uart_pmic_rxd_en_38 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[38] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s PM8921_UART_PMIC_RXD config failed ret = %d\n", __func__, rc );
        return -EIO;
    }

    /* APQ */
    rc = gpio_tlmm_config( GPIO_CFG( 51, GPIOMUX_FUNC_2, GPIO_CFG_OUTPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
    pr_debug( DEV_NAME ": %s gpio_tlmm_config[51] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s APQ8064_UART_TXD config failed ret = %d\n", __func__, rc );
        return -EIO;
    }
    rc = gpio_tlmm_config( GPIO_CFG( 52, GPIOMUX_FUNC_2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
    pr_debug( DEV_NAME ": %s gpio_tlmm_config[52] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s APQ8064_UART_RXD config failed ret = %d\n", __func__, rc );
        return -EIO;
    }

    /* SD */
    rc = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_IRSD_37), &irda_pwdown_37 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[37] ret(%d)\n", __func__, rc );
    if (unlikely(rc < 0)) {
        pr_err( DEV_NAME ": %s PM8921_IRSD config failed ret = %d\n", __func__, rc );
        return -EIO;
    }

    printk( KERN_INFO DEV_NAME ": %s IRDA GPIO Config Completed\n", __func__ );
    return 0;
}

/*
 * (HW) msm_uart_aux_hw_chg_uart
 *
 *  @return     0           Success
 *  @return     -EIO        Setting Error
 */
int msm_uart_aux_hw_chg_uart( void )
{
    int rc = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    /* TX -> Low */
    pmic_uart1_irda_out_en_21.function = PM_GPIO_FUNC_NORMAL;
    rc = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_UART1_IRDA_OUT_21)
                                            , &pmic_uart1_irda_out_en_21 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[21] ret(%d)\n", __func__, rc );
    if (unlikely(rc)) {
        pr_err( DEV_NAME ": %s PM8921_UART1_IRDA_OUT config failed ret = %d\n", __func__, rc );
        return -EIO;
    }
    gpio_set_value_cansleep( PM8921_GPIO_PM_TO_SYS( PM8921_UART1_IRDA_OUT_21 ), 0 );
    printk( KERN_INFO DEV_NAME ": %s IRDA GPIO TX -> Low\n", __func__ );

    /* set UART_RX_DISABLE=1 */
    iowrite32( 0x2, (void *)(uart_membase + UART_CR) );
    pr_debug( DEV_NAME ": %s set UART_RX_DISABLE=1\n", __func__ );

    return 0;
}

/*
 * (HW) msm_uart_aux_hw_irda_start
 *
 *  @return     0       Success
 */
int msm_uart_aux_hw_irda_start( void )
{
    pr_debug( DEV_NAME ": %s start\n", __func__ );

    /* SD(GPIO_37): Low */
    gpio_set_value_cansleep( PM8921_GPIO_PM_TO_SYS( PM8921_IRSD_37 ), 0 );
    /* Sleep 500 us */
    usleep( 500 );

    /* SD(GPIO_37): HIGH */
    gpio_set_value_cansleep( PM8921_GPIO_PM_TO_SYS( PM8921_IRSD_37 ), 1 );
    /* delay 1 us */
    udelay( 1 );
    
    /* SD(GPIO_37): Low */
    gpio_set_value_cansleep( PM8921_GPIO_PM_TO_SYS( PM8921_IRSD_37 ), 0 );
    /* Sleep 500 us */
    usleep( 500 );
    
    printk( KERN_INFO DEV_NAME ": %s Start IRDA Mode SD -> Low\n", __func__ );

    /* set IRDA_EN=1 INVERT_IRDA_RX=1 */
    iowrite32( 0x3, (void *)(uart_membase + UART_IRDA) );
    pr_debug(DEV_NAME ": %s set IRDA_EN=1 INVERT_IRDA_RX=1\n", __func__ );

    /* set UART_RX_EN=1 */
    iowrite32( 0x1, (void *)(uart_membase + UART_CR) );
    pr_debug(DEV_NAME ": %s set UART_RX_EN=1\n", __func__ );

    pr_debug( DEV_NAME ": %s end\n", __func__ );
    return 0;
}

/*
 * (HW) msm_uart_aux_hw_irda_stop
 *
 *  @return     0       Success
 */
int msm_uart_aux_hw_irda_stop( void )
{
    /* SD(GPIO_37): High */
    gpio_set_value_cansleep( PM8921_GPIO_PM_TO_SYS( PM8921_IRSD_37 ), 1 );
    printk( KERN_INFO DEV_NAME ": %s Stop IRDA Mode SD -> High\n", __func__ );

    return 0;
}


/*
 * (HW) msm_uart_aux_hw_power   [local]
 *
 *  @param      on              0... off / 1...on
 *  @param      min_microvolts  min micro-voltage
 *  @return     0               Success
 *  @return     !0              Error
 */
static int msm_uart_aux_hw_power(int on, int min_microvolts )
{
    int rc = 0;
    
    pr_debug( DEV_NAME ": %s(%d, %d) start\n", __func__, on, min_microvolts );

    if (!vreg_l17) {
        vreg_l17 = regulator_get( &apq8064_device_uart_gsbi5.dev, irda_vreg_name);
        pr_debug( DEV_NAME ": %s regulator_get[gsbi5] ret(%ld)\n", __func__, IS_ERR(vreg_l17) );
        if(unlikely(!vreg_l17 || IS_ERR(vreg_l17))) {
            pr_err( DEV_NAME ": %s regulator_get failed. vreg_l17 = %ld\n", __func__, PTR_ERR(vreg_l17) );
            vreg_l17 = NULL;
            return -EIO;
        }
    }

    if (unlikely(!vreg_l17)) {
        return -EINVAL;
    }
    
    if (on && !irda_vreg_on) {
        /* enable the regulator after setting voltage and current */
        rc = regulator_set_voltage( vreg_l17, min_microvolts, MSM_UART_L17_MAX_UV );
        pr_debug( DEV_NAME ": %s regulator_set_voltage(on) ret(%d)\n", __func__, rc );
        if (unlikely(rc)) {
            pr_err( DEV_NAME ": %s failed to set voltage for %s, rc=%d\n"
                                            , __func__, irda_vreg_name, rc);
            goto done;
        }

        rc = regulator_set_optimum_mode( vreg_l17, MSM_UART_L17_LOAD_UA );
        pr_debug( DEV_NAME ": %s regulator_set_optimum_mode(on) ret(%d)\n", __func__, rc );
        if (unlikely(rc < 0)) {
            pr_err( DEV_NAME ": %s failed to set optimum mode for %s, rc=%d\n"
                                            , __func__, irda_vreg_name, rc);
            goto done;
        }

        rc = regulator_enable( vreg_l17 );
        pr_debug( DEV_NAME ": %s regulator_enable ret(%d)\n", __func__, rc );
        if (unlikely(rc < 0)) {
            pr_err( DEV_NAME ": %s failed to enable %s, rc=%d\n"
                                            , __func__, irda_vreg_name, rc);
            goto done;
        }
        
        usleep(2000);
        irda_vreg_on = 1;
    }
    else if ( !on &&  irda_vreg_on ) {
        /* disable the regulator after setting voltage and current */
        rc = regulator_set_voltage( vreg_l17, 0, MSM_UART_L17_MAX_UV );
        pr_debug( DEV_NAME ": %s regulator_set_voltage(off) ret(%d)\n", __func__, rc );
        if (unlikely(rc)) {
            pr_err( DEV_NAME ": %s failed to set voltage for %s, rc=%d\n"
                                            , __func__, irda_vreg_name, rc);
            goto done;
        }

        rc = regulator_set_optimum_mode( vreg_l17, 0 );
        pr_debug( DEV_NAME ": %s regulator_set_optimum_mode(off) ret(%d)\n", __func__, rc );
        if (unlikely(rc < 0)) {
            pr_err( DEV_NAME ": %s failed to set optimum mode for %s, rc=%d\n"
                                            , __func__, irda_vreg_name, rc);
            goto done;
        }
    
        rc = regulator_disable( vreg_l17 );
        pr_debug( DEV_NAME ": %s regulator_disable ret(%d)\n", __func__, rc );
        if (unlikely(rc < 0)) {
            pr_err( DEV_NAME ": %s failed to disable %s, rc=%d\n"
                                            , __func__, irda_vreg_name, rc);
            goto done;
        }
        irda_vreg_on = 0;
    }
    
done:
    printk( KERN_INFO DEV_NAME ": %s ret = %d.\n", __func__, rc );
    return rc;
}

/*
 * (HW) msm_uart_aux_hw_power_on
 *
 *  @return     0               Success
 *  @return     !0              Error
 */
int msm_uart_aux_hw_power_on(void)
{
    pr_debug( DEV_NAME ": %s called\n", __func__ );
    return msm_uart_aux_hw_power(MSM_UART_VREG_ON, MSM_UART_L17_MIN_UV);
}

/*
 * (HW) msm_uart_aux_hw_power_off
 *
 *  @return     0               Success
 *  @return     !0              Error
 */
int msm_uart_aux_hw_power_off(void)
{
    pr_debug( DEV_NAME ": %s called\n", __func__ );
    return msm_uart_aux_hw_power(MSM_UART_VREG_OFF, MSM_UART_L17_OFF_UV);
}

/* 
 * (HW) msm_uart_aux_hw_init
 *
 *  @return     0           Success
 *  @return     -EBUSY      Busy Error
 *  @return     -EIO        Setting Error
 */
int msm_uart_aux_hw_init(void)
{
    int ret = 0;
    void *res;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    /* Set UART MemBase */
    res = request_mem_region(THIS_UART_MEMBASE, THIS_UART_MEMSIZE, "sir_iomem" );
    pr_debug( DEV_NAME ": %s request_mem_region res(%ld)\n", __func__, IS_ERR(res) );
    if (unlikely(res < 0)) {
        pr_err( DEV_NAME ": %s request_mem_region failed.\n", __func__ );
        return -EBUSY;
    }
    uart_membase = (unsigned long)ioremap_nocache(THIS_UART_MEMBASE,THIS_UART_MEMSIZE);
    pr_debug( DEV_NAME ": %s ioremap_nocache ret(%x)\n", __func__, (int)uart_membase );
    if (unlikely(uart_membase == 0)) {
        pr_err( DEV_NAME ": %s ioremap_nocache failed.\n", __func__ );
        release_mem_region( THIS_UART_MEMBASE, THIS_UART_MEMSIZE );
        return -EBUSY;
    }

    ret = msm_uart_aux_hw_power_on();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_power_on ret(%d)\n", __func__, ret );
    if (unlikely(ret)) {
        pr_err( DEV_NAME ": %s regulator power on failed. ret = %d\n", __func__, ret );
        return -EIO;
    }

    /* RX Config */
    ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_UART_PMIC_RXD_38), &uart_pmic_rxd_en_38 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[38] ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s PM8921_UART_PMIC_RXD config failed. ret = %d\n", __func__, ret );
        return -EIO;
    }
    /* SD -> High */
    ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(PM8921_IRSD_37), &irda_pwdown_37 );
    pr_debug( DEV_NAME ": %s pm8xxx_gpio_config[37] ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s IRSD config failed. ret = %d\n", __func__, ret );
        return -EIO;
    }
    gpio_set_value_cansleep( PM8921_GPIO_PM_TO_SYS( PM8921_IRSD_37 ), 1 );
    pr_debug( DEV_NAME ": %s gpio_set_value_cansleep call\n", __func__ );
    
    /* Init Normally */
    pr_debug( DEV_NAME ": %s end\n", __func__ );

    return 0;
}

/* 
 * (HW) msm_uart_aux_hw_exit
 *
 *  @return     0           Success
 *  @return     -EIO        Setting Error
 */
int msm_uart_aux_hw_exit(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_hw_power_off();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_power_off ret(%d)\n", __func__, ret );
    if (unlikely(ret)) {
        pr_err( DEV_NAME ": %s regulator power off failed. ret = %d\n", __func__, ret );
    }

    regulator_put( vreg_l17 );
    vreg_l17 = NULL;

    iounmap( (void *)uart_membase );
    release_mem_region( THIS_UART_MEMBASE, THIS_UART_MEMSIZE );

    pr_debug( DEV_NAME ": %s end\n", __func__ );
    return ret;
}
