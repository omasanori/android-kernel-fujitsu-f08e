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

/*============================================================================

                    Camera Sensor GPIO Driver Source File

============================================================================*/

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <asm/mach-types.h>
#ifndef CAMSENSOR_GPIO_BACK_H
#include "camsensor_gpio_back.h"
#endif // CAMSENSOR_GPIO_BACK_H

/*============================================================================
                        INTERNAL FEATURES
============================================================================*/

/*============================================================================
                        CONSTANTS
============================================================================*/
#define TRUE                    1
#define FALSE                   0

#if defined(CONFIG_MACH_F11APO) || defined(CONFIG_MACH_FJI12)
#define SCL                     3          // I2C SCL GPIO No
#define SDA                     2          // I2C SDA GPIO No
#elif defined(CONFIG_MACH_TAT)
#define SCL                     17         // I2C SCL GPIO No
#define SDA                     16         // I2C SDA GPIO No
#else
#define SCL                     109        // I2C SCL GPIO No
#define SDA                     108        // I2C SDA GPIO No
#endif

#define SCL_Low()               gpio_direction_output(SCL, 0)
#define SCL_High()              gpio_direction_input(SCL)
#define SDA_Low()               gpio_direction_output(SDA, 0)
#define SDA_High()              gpio_direction_input(SDA)
//#define SDA_Read()              gpio_get_value(SDA)
#define SDA_Read()              gpio_get_value_cansleep(SDA)
//#define SCL_Read()              gpio_get_value(SCL)
#define SCL_Read()              gpio_get_value_cansleep(SCL)

//#define LOGI(fmt, args...)      printk(KERN_INFO "camI2C: " fmt, ##args)
#define LOGE(fmt, args...)      printk(KERN_ERR "camI2C: " fmt, ##args)
#define LOGI(fmt, args...)      do {} while (0)
//#define LOGE(fmt, args...)      do {} while (0)

/*============================================================================
                        EXTERNAL VARIABLES DEFINITIONS
============================================================================*/
int _I2C_WAIT_ = 12;

/*============================================================================
                        INTERNAL API DECLARATIONS
============================================================================*/
//static void wait_high_speed(int us) {usleep(us);}
static void wait_high_speed(int us) {}
static void (*__wait)(int us) = wait_high_speed;

static boolean SCL_IsHigh(void)
{
    int32_t cnt;
    // Check SCL High
//    for (cnt = 1000 * 2; cnt; cnt--) {
	for (cnt = 1000 * _I2C_WAIT_; cnt; cnt--) {
        udelay(3);
        if (SCL_Read())    return TRUE;    // SCL High
    }
    return FALSE;
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_START_CONDITION
===========================================================================*/
static boolean camsensor_gpio_start_condition(void)
{
    // In : SCL In (Pull-UP), SDA In (Pull-UP)
    // Out: SCL Out (Drive-Low), SDA Out (Drive-Low)

	// Bus free Check(SCL Low!)
	if (!SCL_IsHigh()) {
        LOGE("start_condition Error (SCL Low) !\n");
        return FALSE;
    }
    // SDA Low (Drive-Low)
    SDA_Low();
    __wait(5);

    // SCL Low (Drive-Low)
    SCL_Low();
    __wait(2);

    return TRUE;
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_STOP_CONDITION
===========================================================================*/
static boolean camsensor_gpio_stop_condition(void)
{
    // In : SCL Out (Drive-Low), SDA In/Out
    // Out: SCL In (Pull-UP), SDA In (Pull-UP)

    // SCL Low (Drive-Low)
    SCL_Low();
    __wait(2);

    // SDA Low (Drive-Low)
    SDA_Low();
    __wait(2);

    // SCL High (Pull-UP)
    SCL_High();

    // SCL High wait
	if (!SCL_IsHigh()) {
        LOGE("stop_condition Error (SCL Low) !\n");
    }
    __wait(3);

    // SDA HIGH (Pull-UP)
    SDA_High();
    __wait(2);
    return TRUE;
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_RESTART_CONDITION
===========================================================================*/
static boolean camsensor_gpio_restart_condition(void)
{
    // In : SCL Out (Drive-Low), SDA Out (Drive-Low)
    // Out: SCL Out (Drive-Low), SDA Out (Drive-Low)

    // SDA High (Pull-UP)
    SDA_High();
    __wait(2);

    // SCL High (Pull-UP)
    SCL_High();

    // SCL High Check
	if (!SCL_IsHigh()) {
        LOGE("restart_condition Error (SCL Low) !\n");
        return FALSE;
    }
    __wait(3);

    // SDA Low (Drive-Low)
    SDA_Low();
    __wait(3);

    // SCL Low (Drive Low)
    SCL_Low();
    __wait(3);

    return TRUE;
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_CHK_ACK
===========================================================================*/
static boolean camsensor_gpio_chk_ack(void)
{
    int sda;

    // In : SCL Out (Drive-Low), SDA In (Pull-UP)
    // Out: SCL Out (Drive-Low), SDA In (Pull-UP)

    // SCL High (Pull-UP)
    SCL_High();

    // SCL High Check
	if (!SCL_IsHigh()) {
        LOGE("chk_ack Error (SCL Low) !\n");
        return FALSE;
    }

    // SDA Signal Read
    sda = SDA_Read();
    __wait(3);

    // SCL Low (Drive-Low)
    SCL_Low();
    __wait(3);

    // response check
	if (sda) {
        // Nack recv
        LOGE("#chk_ack Error (Nack. Receive)!\n");
        return FALSE;
    }

    return TRUE;            // Ack Recv
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_SEND_NACK
===========================================================================*/
static boolean camsensor_gpio_send_ack(uint16_t len)
{
    // In : SCL Out (Drive-Low), SDA In/Out
    // Out: SCL Out (Drive-Low), SDA Out (Drive-Low)

	if (!len) {
        // Nack
        // SDA High (Pull-UP)
        SDA_High();
        __wait(2);
	} else {
        // Ack
        // SDA Low (Drive-Low)
        SDA_Low();
        __wait(2);
    }

    // SCL High (Pull-UP)
    SCL_High();
    __wait(5);

    // SCL Low (Drive-Low)
    SCL_Low();
    __wait(2);

    // SDA Low (Drive-Low)
    SDA_Low();
    __wait(2);

    return TRUE;
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_SEND_BYTE
===========================================================================*/
static boolean camsensor_gpio_send_byte(uint8_t val)
{
    int dir;
    uint8_t mask;

    // In : SCL Out (Drive-Low), SDA In/Out
    // Out: SCL Out (Drive-Low), SDA In (Pull-UP)

    mask = 0x80;
    // MSB Bit Output
    dir = val & mask;
	if (dir) {
        // SDA High (Pull-UP)
        SDA_High();
	} else {
        // SDA Low (Drive-Low)
        SDA_Low();
    }
    __wait(2);

    // SCL High (Pull-UP)
    SCL_High();

    // SCL High Check
	if (!SCL_IsHigh()) {
        LOGE("send_byte SCL Error !\n");
        return FALSE;
    }
    __wait(3);

    // SCL Low (Drive-Low)
    SCL_Low();
    __wait(3);

    // 7bit output
    mask >>= 1;
	while (mask) {
        // SDA 1Bit out
        dir = val & mask;
		if (dir) {
            // SDA High (Pull-UP)
            SDA_High();
		} else {
            // SDA Low (Drive-Low)
            SDA_Low();
        }
        __wait(2);

        // SCL High (Pull-UP)
        SCL_High();
        __wait(4);

        // SCL Low (Drive-Low)
        SCL_Low();
        __wait(2);

        mask >>= 1;
    }

    // SDA High (Pull-UP)
    SDA_High();
    __wait(4);

    return TRUE;
}

/*===========================================================================
    FUNCTION  CAMSENSOR_GPIO_RECV_BYTE
===========================================================================*/
static boolean camsensor_gpio_recv_byte(uint8_t *val)
{
    int sda;
    uint8_t mask = 0x80;
    uint8_t data = 0x00;

    // In : SCL Out (Drive-Low), SDA Out (Drive-Low)
    // Out: SCL Out (Drive-Low), SDA In (Pull-UP)

    // SDA Hige (Pull-UP)
    SDA_High();
    __wait(2);

    // SCL High (Pull-UP)
    SCL_High();

    // SCL High Check
	if (!SCL_IsHigh()) {
        LOGE("recv_byte SCL Error !\n");
        return FALSE;
    }
    __wait(3);

    // SDA in
    sda = SDA_Read();
    if (sda)
        data |= mask;

    // SCL Low (Drive-Low)
    SCL_Low();
    __wait(3);

    // 7bit output
    mask >>= 1;
	while (mask) {
        // SCL High (Pull-UP)
        SCL_High();
        __wait(2);

        // SDA in
        sda = SDA_Read();
        if (sda)
          data |= mask;
        __wait(2);

        // SCL Low (Drive-Low)
        SCL_Low();
        __wait(4);

        mask >>= 1;
    }
    // Done, SCL is in LOW.
    *val = data;

    __wait(2);

    return TRUE;
}

/*============================================================================
                        EXTERNAL API DEFINITIONS
============================================================================*/

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_WRITE
===========================================================================*/
int camsensor_gpioi2c_write(struct camsensor_i2c_command *cmd)
{
    int rc = TRUE;
    uint16_t len;
    uint8_t *pd;
	uint8_t slave;

    // 1.Start Condition
    if (!(rc = camsensor_gpio_start_condition()))  goto fault;

	slave = cmd->slave_addr << 1;

    // 2.SEND - Slave Address(Camera Device)
    if (!(rc = camsensor_gpio_send_byte(slave))) goto fault;
    // Chk Acknowledge
	if (!(rc = camsensor_gpio_chk_ack())) {
        LOGE("write. fault Slave addr\n");
        goto fault;
    }

    // 3.SEND - Data
	for(len = cmd->wlen, pd = cmd->pwdata ; len > 0 ; --len, ++pd) {
        if (!(rc = camsensor_gpio_send_byte(*pd)))    goto fault;
        // Chk Acknowledge
		if (!(rc = camsensor_gpio_chk_ack())) {
            LOGE("write. fault Data\n");
            goto fault;
        }
    }

    // 4.Stop Condition
    camsensor_gpio_stop_condition();
    goto exit;

fault : ;
    camsensor_gpio_stop_condition();

exit : ;
    if (!rc)
		LOGE("%s Error!", __func__);

    return rc ? RET_OK : RET_ERR;
}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_READ
===========================================================================*/
int camsensor_gpioi2c_read(struct camsensor_i2c_command *cmd)
{
    int rc = TRUE;
    uint16_t len;
    uint8_t  *pd;
	uint8_t slave;

    // 1.Start Condition
    if (!(rc = camsensor_gpio_start_condition()))  goto fault;

	slave = cmd->slave_addr << 1;

    // 2. SEND - Slave Address(Camera Device)
    if (!(rc = camsensor_gpio_send_byte(slave))) goto fault;
    // Chk Acknowledge
	if (!(rc = camsensor_gpio_chk_ack())) {
        LOGE("read. fault Slave[W] addr\n");
        goto fault;
    }

    // 3.SEND - Data
	for (len = cmd->wlen, pd = cmd->pwdata ; len > 0 ; --len, ++pd) {
        if (!(rc = camsensor_gpio_send_byte(*pd)))    goto fault;
        // Chk Acknowledge
		if (!(rc = camsensor_gpio_chk_ack())) {
            LOGE("raed. fault Write Data\n");
            goto fault;
        }
    }

    // 4.Restart Condition
    if (!(rc = camsensor_gpio_restart_condition()))    goto fault;

    // 5.SEND - Slave Address again(Camera Device)
    if (!(rc = camsensor_gpio_send_byte(slave | 0x01)))  goto fault;
    // Chk Acknowledge
	if (!(rc = camsensor_gpio_chk_ack())) {
        LOGE("read. fault Slave[R] addr\n");
        goto fault;
    }

    // 6.RCV - Data
	for(len = cmd->rlen, pd = cmd->prdata ; len > 0 ; --len, ++pd) {
        if (!(rc = camsensor_gpio_recv_byte(pd)))    goto fault;
        // Send Ack
		if (!(rc = camsensor_gpio_send_ack(len-1))) {
            LOGE("read. fault ack \n");
            goto fault;
        }
    }

    // 7.Stop Condition
    camsensor_gpio_stop_condition();
    goto exit;

fault : ;
    camsensor_gpio_stop_condition();

exit : ;
    if (!rc)
		LOGE("%s Error!", __func__);

    return rc ? RET_OK : RET_ERR;
}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_WRITE_ISP
===========================================================================*/
int camsensor_gpioi2c_write_isp(struct camsensor_i2c_command_isp *cmd)
{
    int rc = TRUE;
    uint16_t len;
    uint8_t  *pd;
	char req_data[12]; // Max send buffer
	char* req_buf;
	int i = 0;
	uint8_t slave;

    // 1.Start Condition
    if (!(rc = camsensor_gpio_start_condition()))  goto fault;

	slave = cmd->slave_addr << 1;

    // 2.SEND - Slave Address(Camera Device)
    if (!(rc = camsensor_gpio_send_byte(slave))) goto fault;

    // Chk Acknowledge
	if (!(rc = camsensor_gpio_chk_ack())) {
        LOGE("write. fault Slave addr\n");
        goto fault;
    }

	// for M6Mo Parameter
	req_data[0] = 4 + cmd->wlen;
	req_data[1] = 0x02; // 0x02 is Write cmd.
	req_data[2] = cmd->category;
	req_data[3] = cmd->byte;
	req_buf = &req_data[4];
    LOGI("<write> s:%02x [c:%02x b:%02x]\n", slave, cmd->category, cmd->byte);

	for (i=0; i<cmd->wlen; i++) {
		req_buf[i] = cmd->pwdata[i];
    	LOGI("<write> - data:%02x\n", req_buf[i]);
	}
	cmd->wlen += 4; // Length is no send. because +3byte.

    // 3.SEND - Data
	for(len = cmd->wlen, pd = req_data ; len > 0 ; --len, ++pd) {
        if (!(rc = camsensor_gpio_send_byte(*pd)))    goto fault;
        // Chk Acknowledge
		if (!(rc = camsensor_gpio_chk_ack())) {
            LOGE("write. fault Data\n");
            goto fault;
        }
    }

    // 4.Stop Condition
    camsensor_gpio_stop_condition();
    goto exit;

fault : ;
    camsensor_gpio_stop_condition();

exit : ;
    if (!rc)
		LOGE("%s Error!", __func__);

    return rc ? RET_OK : RET_ERR;
}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_READ_ISP
===========================================================================*/
int camsensor_gpioi2c_read_isp(struct camsensor_i2c_command_isp *cmd)
{
    int rc = TRUE;
    uint16_t len;
    uint8_t  *pd;
	char req_data[12];
	char read_data[256];
	int i = 0;
	uint8_t slave;

    // 1.Start Condition
    if (!(rc = camsensor_gpio_start_condition()))  goto fault;

	slave = cmd->slave_addr << 1;

    // 2. SEND - Slave Address(Camera Device)
    if (!(rc = camsensor_gpio_send_byte(slave))) goto fault;

    // Chk Acknowledge
	if (!(rc = camsensor_gpio_chk_ack())) {
        LOGE("read. fault Slave[W] addr\n");
        goto fault;
    }

	// for M6Mo Parameter
	req_data[0] = 5;
	req_data[1] = 0x01; // 0x01 is Read cmd.
	req_data[2] = cmd->category;
	req_data[3] = cmd->byte;
	req_data[4] = cmd->rlen;
    LOGI("<read:w> s:%02x [c:%02x b:%02x] l:%d\n", slave, cmd->category, cmd->byte, cmd->rlen);
	cmd->wlen = 5; // Length is no send. because +4byte.

    // 3.SEND - Data
	for(len = cmd->wlen, pd = req_data ; len > 0 ; --len, ++pd) {
        if (!(rc = camsensor_gpio_send_byte(*pd)))    goto fault;
        // Chk Acknowledge
		if (!(rc = camsensor_gpio_chk_ack())) {
            LOGE("raed. fault Write Data\n");
            goto fault;
        }
    }

#if 1
	// 4-1.Stop Condition
    if (!(rc = camsensor_gpio_stop_condition()))    goto fault;
    __wait(10);
	// 4-2.Start Condition
    if (!(rc = camsensor_gpio_start_condition()))    goto fault;

#else
	// 4.Restart Condition
    if (!(rc = camsensor_gpio_restart_condition()))    goto fault;
#endif

    // 5.SEND - Slave Address again(Camera Device)
    if (!(rc = camsensor_gpio_send_byte(slave | 0x01)))  goto fault;
    // Chk Acknowledge
	if (!(rc = camsensor_gpio_chk_ack())) {
        LOGE("read. fault Slave[R] addr\n");
        goto fault;
    }

    // 6.RCV - Data
	for(len = cmd->rlen + 1, pd = read_data ; len > 0 ; --len, ++pd) {
        if (!(rc = camsensor_gpio_recv_byte(pd)))    goto fault;
        // Send Ack
		if (!(rc = camsensor_gpio_send_ack(len-1))) {
            LOGE("read. fault ack \n");
            goto fault;
        }
    }

    // 7.Stop Condition
    camsensor_gpio_stop_condition();

    for (i=0; i<cmd->rlen; i++) {
    	cmd->prdata[i] = read_data[i+1];
    	LOGI("<read:r> r:%d [c:%02x b:%02x]0x%02x\n", rc, cmd->category, cmd->byte, cmd->prdata[i]);
    }

    goto exit;

fault : ;
    camsensor_gpio_stop_condition();

exit : ;
    if (!rc)
		LOGE("%s Error!", __func__);

    return rc ? RET_OK : RET_ERR;
}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_READ_ISP
===========================================================================*/
int camsensor_i2c_read_isp(struct i2c_client* client, struct camsensor_i2c_command_isp* cmd)
{

    int rc = 0;
    struct i2c_msg msg[2];
    unsigned char read_data[256];
    unsigned char req_data[5];
    int i = 0;

	if (!client->adapter) {
        LOGE("- %s err:client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

    req_data[0] = 5;
    req_data[1] = 0x01;
    req_data[2] = cmd->category;
    req_data[3] = cmd->byte;
    req_data[4] = cmd->rlen;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 5;
    msg[0].buf = req_data;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = cmd->rlen + 1;
    msg[1].buf = read_data;

    rc = i2c_transfer(client->adapter, msg, 2);
    if (rc < 0) {
        LOGE("%s:i2c_transfer Err(%d) Read[%02x:0x%x-0x%x]\n",
        	__func__, rc, client->addr, cmd->category, cmd->byte);
        rc = -EIO;
        return rc;
    }

    for (i=0; i<cmd->rlen; i++) {
    	cmd->prdata[i] = read_data[i+1];
    	LOGI("<read:r> r:%d [c:%02x b:%02x]0x%02x\n", rc, cmd->category, cmd->byte, cmd->prdata[i]);
    }

    LOGI("%s OK %d Read:[0x%x-0x%x] %02x\n",
    	__func__, rc, cmd->category, cmd->byte, cmd->prdata[0]);

    return 0;

}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_WRITE_ISP
===========================================================================*/
int camsensor_i2c_write_isp(struct i2c_client* client, struct camsensor_i2c_command_isp* cmd)
{

    int rc = 0;
    struct i2c_msg msg;
    unsigned char req_data[256];
    int i = 0;

	if (!client->adapter) {
        LOGE("- %s err:client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

    req_data[0] = 4 + cmd->wlen;
    req_data[1] = 0x02;
    req_data[2] = cmd->category;
    req_data[3] = cmd->byte;
    LOGI("<write> s:%02x [c:%02x b:%02x]\n", client->addr, cmd->category, cmd->byte);

    for (i=0; i<cmd->wlen; i++) {
    	req_data[i+4] = cmd->pwdata[i];
    	LOGI("<write> - data:%02x\n", req_data[i+4]);
    }

    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = 4 + cmd->wlen;  /* add address bytes */
    msg.buf = req_data;

    rc = i2c_transfer(client->adapter, &msg, 1);
    if (rc < 0) {
        LOGE("%s:i2c_transfer Err(%d) Write[%02x:%02x-%02x]\n",
        	__func__, rc, client->addr, cmd->category, cmd->byte);
        rc = -EIO;
        return rc;
    }

    LOGI("%s OK %d write:[0x%x-0x%x] %02x\n",
    	__func__, rc, cmd->category, cmd->byte, req_data[4]);

	return 0;
}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_READ
===========================================================================*/
int camsensor_i2c_read(struct i2c_client* client, struct camsensor_i2c_command* cmd)
{

    int rc = 0;
    struct i2c_msg msg[2];
    unsigned char read_data[256];
    unsigned char req_data[256];
    int i = 0;

	if (!client->adapter) {
        LOGE("- %s err:client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = cmd->wlen;
    for (i=0; i<cmd->wlen; i++) {
	    LOGI("I2C Transfer_normal 0x%02x \n", cmd->pwdata[i]);
    	req_data[i] = cmd->pwdata[i];
    }
    msg[0].buf = req_data;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = cmd->rlen;
    msg[1].buf = read_data;

    rc = i2c_transfer(client->adapter, msg, 2);
    if (rc < 0) {
        LOGE("%s:i2c_transfer Err(%d) Read(%02x,%02x)\n",
        	__func__, rc, msg[1].addr, msg[1].len);
        rc = -EIO;
        return rc;
    }

	for (i=0; i<cmd->rlen; i++) {
    	cmd->prdata[i] = read_data[i];
    }

    LOGI("%s OK %d Read: 0x%02x\n", __func__, rc, cmd->prdata[0]);

    return 0;
}

/*===========================================================================
    FUNCTION      CAMSENSOR_GPIOI2C_WRITE
===========================================================================*/
int camsensor_i2c_write(struct i2c_client* client, struct camsensor_i2c_command* cmd)
{

    int rc = 0;
    struct i2c_msg msg;
    unsigned char req_data[256];
    int i = 0;

	if (!client->adapter) {
        LOGE("- %s err:client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = cmd->wlen;  /* add address bytes */
    for (i=0; i<cmd->wlen; i++) {
	    LOGI("I2C Transfer_normal 0x%02x \n", cmd->pwdata[i]);
    	req_data[i] = cmd->pwdata[i];
    }
    msg.buf = req_data;

    rc = i2c_transfer(client->adapter, &msg, 1);
    if (rc < 0) {
        LOGE("%s:i2c_transfer Err(%d) Write[%02x,%02x]\n",
        	__func__, rc, msg.addr, msg.len);
        rc = -EIO;
        return rc;
    }

    LOGI("%s OK %d write \n", __func__, rc);

	return 0;
}
