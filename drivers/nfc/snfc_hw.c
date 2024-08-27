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

#include <linux/irq.h>
#include <linux/delay.h>

#include "snfc_ioctl.h"
#include "snfc_hw.h"

#define IRQ_WAKE_DISABLE 0
#define IRQ_WAKE_ENABLE 1

static struct pm8xxx_mpp_config_data hvdd_in_config = {
	.type    = PM8XXX_MPP_TYPE_D_INPUT,
	.level   = PM8921_MPP_DIG_LEVEL_S4,
	.control = PM8XXX_MPP_DIN_TO_INT,
};
static struct pm8xxx_mpp_config_data hvdd_out_config = {
	.type    = PM8XXX_MPP_TYPE_D_OUTPUT,
	.level   = PM8921_MPP_DIG_LEVEL_S4,
	.control = PM8XXX_MPP_DOUT_CTRL_LOW,
};

/** common **********************/
int snfc_request_irq(int port, irq_handler_t handler, unsigned long flags, const char *name) {
	int ret = 0;
	unsigned int irq;

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] %d,%ld,%s\n",__func__,port,flags,name);
#endif
	irq = gpio_to_irq(port);
	if(unlikely(request_irq(irq, handler, flags, name, 0))){
		printk(KERN_ERR  "%s:err[%d].\n",__func__, ret );
		return -EINVAL;
	}
	disable_irq(irq);
	return ret;
}

int snfc_hw_irq_enable(int irq) {
	int ret = 0;
	ret = irq_set_irq_wake(irq, IRQ_WAKE_ENABLE);
	if(likely(!ret)) {
		enable_irq(irq);
	} else {
		printk(KERN_ERR  "%s:irq_set_irq_wake[%d].\n", __func__, ret );
	}
	return ret;
}

int snfc_hw_irq_disable(int irq) {
	int ret = 0;
	ret = irq_set_irq_wake(irq, IRQ_WAKE_DISABLE);
	if(unlikely(ret)) {
		printk(KERN_ERR  "%s:irq_set_irq_wake[%d].\n", __func__, ret );
	}
	disable_irq(irq);
	return ret;
}


/** for cen *********************/
int snfc_hw_cen_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_CEN_DATA, "snfc_cen_data");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_CEN_DATA);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_CEN_DATA );
		return ret;
	}
	ret = gpio_request(NFCCTL_GPIO_CEN_CLK, "snfc_cen_clk");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_CEN_CLK);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_CEN_CLK );
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_cen_exit(void) {
	return 0;
}

int snfc_hw_cen_write(int val) {
	gpio_set_value_cansleep( NFCCTL_GPIO_CEN_DATA, val);
	udelay(5);
	gpio_set_value_cansleep( NFCCTL_GPIO_CEN_CLK, SNFC_HW_HIGH);
	udelay(5);
	gpio_set_value_cansleep( NFCCTL_GPIO_CEN_CLK, SNFC_HW_LOW);
	return 0;
}

inline int snfc_hw_cen_read() {
	return gpio_get_value_cansleep(NFCCTL_GPIO_CEN_DATA);
}

/** for pon *********************/
int snfc_hw_pon_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_PON, "snfc_pon");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_PON);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_PON );
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_pon_exit(void) {
	return 0;
}


/** for rfs *********************/
int snfc_hw_rfs_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_RFS, "snfc_rfs");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_RFS);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_RFS );
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_rfs_exit(void) {
	return 0;
}




/** for hsel *********************/
int snfc_hw_hsel_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_HSEL, "snfc_hsel");
	if (unlikely(ret)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_HSEL );
		return ret;
	}
	gpio_direction_output(NFCCTL_GPIO_HSEL, 0);
	snfc_hw_hsel_write(0);

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_hsel_exit(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	gpio_free(NFCCTL_GPIO_HSEL);
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

/** for intu *********************/
int snfc_hw_intu_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_INTU, "snfc_intu");
	if (unlikely(ret)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_INTU );
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_intu_exit(void) {
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	gpio_free(NFCCTL_GPIO_INTU);
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

/** for available_poll *********************/
int snfc_hw_available_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_CEN_DATA, "snfc_cen_data");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_CEN_DATA);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_CEN_DATA );
		return ret;
	}
	ret = gpio_request(NFCCTL_GPIO_CEN_CLK, "snfc_cen_clk");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_CEN_CLK);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_CEN_CLK );
		return ret;
	}
	ret = gpio_request(NFCCTL_GPIO_RFS, "snfc_rfs");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_RFS);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_RFS );
		return ret;
	}

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_available_exit(void) {
	return 0;
}

/** for power_observe *********************/
int snfc_hw_power_observe_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_request(NFCCTL_GPIO_MDM2AP, "snfc_mdm2ap");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_MDM2AP);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_MDM2AP );
		return ret;
	}

	ret = gpio_request(NFCCTL_GPIO_RFS, "snfc_rfs");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_RFS);
	} else if(ret != -EBUSY) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_RFS );
		return ret;
	}

	ret = gpio_request(NFCCTL_GPIO_INT, "snfc_int");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_INT);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_INT );
		return ret;
	}

	ret = gpio_request(NFCCTL_GPIO_INTU, "snfc_intu");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_INTU);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_INTU );
		return ret;
	}

	ret = gpio_request(NFCCTL_GPIO_UART_TX, "snfc_uart_tx");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_UART_TX);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_UART_TX );
		return ret;
	}

	ret = gpio_request(NFCCTL_GPIO_UART_RX, "snfc_uart_rx");
	if (unlikely(!ret)) {
		gpio_free(NFCCTL_GPIO_UART_RX);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_UART_RX );
		return ret;
	}

	ret = gpio_request(NFCCTL_GPIO_HVDD, "snfc_hvdd");
	if (unlikely(ret)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", NFCCTL_GPIO_HVDD );
		return ret;
	}
	ret = pm8xxx_mpp_config( NFCCTL_GPIO_HVDD, &hvdd_in_config);
	if ( unlikely(ret) ) {
		printk(KERN_ERR "%s: NFC HVDD input config failed\n", __func__ );
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

int snfc_hw_power_observe_exit(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	gpio_free(NFCCTL_GPIO_HVDD);
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

/** for others *********************/
int snfc_hw_hvdd_read(void) {
	int hvdd_chk = 0;
	hvdd_chk = (snfc_hw_mstatus_read() > 0 ? 1 : 0);
	hvdd_chk &= (gpio_get_value(NFCCTL_GPIO_HVDD) > 0 ? 1 : 0);
	return hvdd_chk;
}

int snfc_hw_hvdd_write(int val) {
	int ret = 0;
	if(val) {
		ret = pm8xxx_mpp_config( NFCCTL_GPIO_HVDD, &hvdd_in_config);
		if ( ret ) {
			printk(KERN_ERR "%s: NFC HVDD input config failed\n", __func__ );
		}
	} else {
		ret = pm8xxx_mpp_config( NFCCTL_GPIO_HVDD, &hvdd_out_config);
		if ( ret ) {
			printk(KERN_ERR "%s: NFC HVDD output config failed\n", __func__ );
		}
	}
	return ret;
}

int snfc_hw_gpio_config(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = gpio_tlmm_config( GPIO_CFG(NFCCTL_GPIO_RFS, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: NFC FEL_VRO config failed\n", __func__ );
	}

	ret = gpio_tlmm_config( GPIO_CFG(NFCCTL_GPIO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: NFC FEL_IBO_3 config failed\n", __func__ );
	}

	ret = gpio_tlmm_config( GPIO_CFG(NFCCTL_GPIO_INTU, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: NFC FEL_INTU config failed\n", __func__ );
	}
	
	ret = gpio_tlmm_config( GPIO_CFG(NFCCTL_GPIO_UART_TX, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: NFC UART TXD config failed\n", __func__ );
	}

	ret = gpio_tlmm_config( GPIO_CFG(NFCCTL_GPIO_UART_RX, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: NFC UART RXD config failed\n", __func__ );
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}
