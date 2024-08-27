/*
 * Copyright(C) 2011-2013 FUJITSU LIMITED
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

#ifndef __MSM_SERIAL_AUX_HW__
#define __MSM_SERIAL_AUX_HW__

/* UART MemBase */
#define THIS_UART_MEMBASE   0x1A240000  /* APQ:GSBI5_UART_DM_BASE */
#define THIS_UART_MEMSIZE   4096        /* align page size        */


#define MSM_UART_L17_MIN_UV		2850000		/* MIN micro-voltage = 2.9V  */
#define MSM_UART_L17_MAX_UV		2850000		/* MAX micro-voltage = 2.9V  */
#define MSM_UART_L17_LOAD_UA	300000		/* Load micro-ampere = 300mA */
#define MSM_UART_VREG_ON		1
#define MSM_UART_VREG_OFF		0
#define MSM_UART_L17_OFF_UV		0

/* UART DM regs offset */
#define UART_MR1            0x0000
#define UART_MR2            0x0004
#define UART_SR             0x0008
#define UART_TF             0x000C
#define UART_CR             0x0010
#define UART_IMR            0x0014
#define UART_IPR            0x0018
#define UART_TFWR           0x001C
#define UART_RFWR           0x0020
#define UART_HCR            0x0024
#define UART_MREG           0x0028
#define UART_NREG           0x002C
#define UART_DREG           0x0030
#define UART_DMRX	 		0x0034
#define UART_IRDA           0x0038
#define UART_DMEN			0x003C
#define UART_NO_CHARS_TX	0x0040
#define UART_BADR		 	0x0044
#define UART_MISR_RESET		0x0064
#define UART_MISR_EXPORT	0x0068
#define UART_MISR_VAL		0x006c

/* ReadOnlyRegister */
#define UART_RF				0x0070
#define UART_RF2			0x0074
#define UART_RF3			0x0078
#define UART_RF4			0x007C
#define UART_MISR           0x0010
#define UART_ISR            0x0014
#define UART_RX_TOTAL_SNAP  0x0038
#define UART_TXFS			0x004C
#define UART_RXFS			0x0050

/* PMIC GPIO PORT */
#define PM8921_UART1_IRDA_OUT_21        (21-1)
#define PM8921_UART1_IRDA_IN_33         (33-1)
#define PM8921_IRSD_37                  (37-1)
#define PM8921_UART_PMIC_TXD_8          (8-1)
#define PM8921_UART_PMIC_RXD_38         (38-1)
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio + NR_GPIO_IRQS)

/* Prototype declaration */
int msm_uart_aux_hw_uart_enable(void);
int msm_uart_aux_hw_uart_disable(void);
int msm_uart_aux_hw_chg_irda(void);
int msm_uart_aux_hw_chg_uart(void);
int msm_uart_aux_hw_irda_start(void);
int msm_uart_aux_hw_irda_stop(void);
int msm_uart_aux_hw_power_on(void);
int msm_uart_aux_hw_power_off(void);
int msm_uart_aux_hw_init(void);
int msm_uart_aux_hw_exit(void);

#endif /* __MSM_SERIAL_AUX_HW__ */
