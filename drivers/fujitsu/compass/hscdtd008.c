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
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>

#include <linux/ctype.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "compass_def.h"
#include "hscdtd008.h"

// CMPS Peculiar instance data
cmps_t cmps[1];

// PREVIOUS Measurement Data
compass_data_t pre_meas;

/*== HSCD Basic control function (Reading and writing of register)==*/

/*--------------------------------------------------------------------
cmps_read_(verify_)regs() : reg_data[] Number of arguments of minimum elements.
--------------------------------------------------------------------*/
#define READREGS_MINBUFSIZE     3
static struct mutex xfer_lock;
uint8_t cmps_sts = CMPS_NONE;
spinlock_t cmps_spin_lock;

/////////////////////////////////////////////////////////////


//// HW //////////////////////////////////////////////////////
/*--------------------------------------------------------------------
Function:CMPS of RANDOM READ  register is read.
@param	: (1)start_reg:The first register number.
          (2)n_regs:Number of read registers.
          (3)cmps->max_retry_count:Maximum retrying frequency.
output  :reg_data[0 - n_regs-1]:Read data.
I/O	    :When the reading error occurs, the following variable is updated.
          (1)cmps->error_count:+1 
          (2)cmps->retry_count:The generation frequency is multiplied retrying.
@retval :(1)0:Normal termination.
       	  (2)-EBUSY:I2C Communication fault (time out).
--------------------------------------------------------------------*/
int cmps_hw_read_regs(uint8_t start_reg, uint8_t reg_data[], uint16_t n_regs)
{
	bool error = false;
	unsigned retry_count = 0;
	int ret;
	struct i2c_msg msg[2];
	uint8_t buf0[2];

	compass_printk(KERN_INFO,"%s()\n", __func__);

	//i2c_transfer Parameter setting
	msg[0].addr = cmps->reg.st_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf0;
	buf0[0] = start_reg;		// Reading beginning position setting

	msg[1].addr = cmps->reg.st_client->addr;
	msg[1].flags = I2C_M_RD;	// Read the register value
	msg[1].len = n_regs;		// Reading number
	msg[1].buf = reg_data;		// Area where reading result is put

	for (;;) {

		mutex_lock(&xfer_lock);
		ret = i2c_transfer(cmps->reg.st_client->adapter, msg, 2);
		rmb();
		if (ret >= 0) {
			mutex_unlock(&xfer_lock);
			compass_printk(KERN_INFO, "%s:i2c_transfer ret=(%d)\n", CMPS_DEVNAME, ret);
			break;				// Normal termination
		}
		else{
			compass_printk(KERN_ERR, "%s :i2c_transfer error (%d)\n",CMPS_DEVNAME, ret);
		}
		mutex_unlock(&xfer_lock);

		// When the error occurs
		error = true;
		if (retry_count >= cmps->max_retry_count) {
			// When ..retrying.. frequency reaches the upper bound:error
			compass_printk(KERN_ERR, "%s : I2C receive error (%d)\n",
														CMPS_DEVNAME, ret);
			goto Exit;
		}
		// It retries. 
		msleep(10);		// Waiting time
		retry_count++;
	}

Exit:
	if (error) {
		// The frequency is totaled error/retrying. 
		cmps->error_count++;
		cmps->retry_count += retry_count;
	}

	compass_printk(KERN_INFO,"%s() ok start_reg:0x%02X n_regs:%u ret=%d\n"
									, __func__, start_reg, n_regs ,ret);

	return ret;
}

/*--------------------------------------------------------------------
Function  :CMPS of RANDOM READ register is read.
        	It repeats until the same data can be acquired reading
        	two or more times, and continuously two times.
        	 (The maximum of retrying cmps->max_retry_count)
@param  :(1) start_reg:The first register number.
         (2) n_regs:Number of read registers.
         (3) cmps->max_retry_count:Maximum retrying frequency.
output  :reg_data[0 - n_regs-1]:Read data.
I/O :When the reading error occurs, the following variable is updated.
         (1) cmps->error_count:The error generation frequency is multiplied.
         (2) cmps->retry_count:The generation frequency is multiplied retrying.
@retval: (1) 0:Normal termination.
         (2) -EBUSY:I2C Communication fault (time out).
         (3) -EIO:It doesn't succeed in the reading collation even 
                   if ..retrying.. frequency reaches the upper bound. 
--------------------------------------------------------------------*/
int cmps_hw_read_verify_regs(uint8_t start_reg,
								 uint8_t reg_data[], uint8_t n_regs)
{
	unsigned read_count = 0;	// Read number
	const unsigned max_read_count = cmps->max_retry_count;
	int ret;

	compass_printk(KERN_INFO,"%s()\n", __func__);

	for (;;) {
		ret = cmps_hw_read_regs(start_reg, reg_data, n_regs);
		if (ret >= 0){
			break;
		}
		
		read_count++;
		if (read_count >= max_read_count) {
			compass_printk(KERN_ERR, "%s: %s() error (%d)\n",
								CMPS_DEVNAME, __func__, ret);
			goto RetryFailed;
		}
		
	}
	// Collation success
	ret = OK;

Exit:
	if (read_count > 0) {
		// When retrying
		cmps->retry_count += read_count;
		cmps->error_count++;
	}

	compass_printk(KERN_INFO,"%s() ok start_reg:0x%02X n_regs:%u ret=%d\n"
						, __func__, start_reg, n_regs, ret);

	return ret;

RetryFailed:					// Exaggerated ..retrying.. frequency

	ret = -EIO;
	goto Exit;
}

/*--------------------------------------------------------------------
Function  :CMPS of WRITE It writes it in the register.
@param  :(1) start_reg:The first register number.
         (2) reg_data[0 - n_regs-1]:Data written in register.
         (3) n_regs (<=CMPS_MAX_CONTIGUOUS_REGS):Number of registers.
         (4) cmps->max_retry_count:Maximum retrying frequency.
I/O:When the I/O error occurs, the following variable is updated. .
         (1) cmps->error_count
         (2) cmps->retry_count
@retval: (1) 0:Normal termination.
         (2) -EINVAL:n_regs is too large.
         (3) -EBUSY:I2C Communication fault (time out).
--------------------------------------------------------------------*/
int cmps_hw_write_regs(uint8_t start_reg,
						   const uint8_t reg_data[], uint16_t n_regs)
{
	// buffer[]:Array that adds slave addr(W) 
	// and start_reg in front of reg_data[]. 
	uint8_t buffer[CMPS_MAX_CONTIGUOUS_REGS + 2];
	uint8_t *dest;
	unsigned i;
	int ret;
	unsigned retry_count = 0;

	compass_printk(KERN_INFO,"%s() \n", __func__);

	if (n_regs > CMPS_MAX_CONTIGUOUS_REGS) {
		ret = -EINVAL;
		compass_printk(KERN_ERR,"Too many registers (> %u).\n", CMPS_MAX_CONTIGUOUS_REGS);
		goto Error;
	}

	dest = buffer;
	*dest++ = start_reg;
	for (i = 0; i < n_regs; i++) {
		*dest++ = reg_data[i];
	}

	for (;;) {
		mutex_lock(&xfer_lock);
		ret = i2c_master_send(cmps->reg.st_client, buffer, n_regs + 1);
		rmb();
		if (ret >= 0) {
			mutex_unlock(&xfer_lock);
			compass_printk(KERN_INFO, "%s:i2c_master_send ret=(%d)\n", CMPS_DEVNAME, ret);
			break;				// Normal termination
		}
		else{
			compass_printk(KERN_ERR, "%s: i2c_master_send error (%d)\n", CMPS_DEVNAME, ret);
		}
		mutex_unlock(&xfer_lock);
		// When the error occurs
		if (retry_count >= cmps->max_retry_count) {
			// When ..retrying.. frequency reaches the upper bound:error
			compass_printk(KERN_ERR, "%s: I2C send error (%d)\n", 
													CMPS_DEVNAME, ret);
			goto Error;
		}
		// It retries. 
		msleep(10);		// Waiting time
		retry_count++;
	}

	if (retry_count > 0) {
Error:
		cmps->error_count++;
		cmps->retry_count += retry_count;
	}
	compass_printk(KERN_INFO,"%s() ok start_reg:0x%02X n_regs:%u ret=%d\n"
						, __func__, start_reg, n_regs , ret);
	return ret;
}

/*--------------------------------------------------------------------
Function  :CMPS of WRITE It writes it in the register.
        Whether it was possible to write it correctly by reading afterwards,
        returning, and collating it is confirmed. 
        When it is not possible to write it, it retries. 
@param  :(1) start_reg:The first register number.
         (2) reg_data[0 - n_regs-1]:Data written in register.
         (3) n_regs (<=CMPS_MAX_CONTIGUOUS_REGS):Number of registers.
         (4) verify_mask (0x00):Mask that specifies collated bit about 
                                    reg_data[] each byte. 
              It is checked whether the bit of one in this is corresponding. 
         (5) cmps->max_retry_count:Maximum retrying frequency.
I/O:When the I/O error occurs, the following variable is updated. 
         (1) cmps->error_count
         (2) cmps->retry_count
@retval: (1) 0:Normal termination.
         (2) -EINVAL:n_regs is too large.
         (3) -EBUSY:I2C Communication fault (time out).
         (4) -EIO:It doesn't succeed in the writing collation even 
                   if ..retrying.. frequency reaches the upper bound. 
--------------------------------------------------------------------*/
int cmps_hw_write_verify_regs(uint8_t start_reg, const uint8_t reg_data[],
								  uint16_t n_regs, uint8_t verify_mask)
{
	uint8_t buffer[CMPS_MAX_CONTIGUOUS_REGS];
	unsigned retry_count = 0;
	unsigned i;
	int ret;

	compass_printk(KERN_INFO,"%s() \n", __func__);

	if ((verify_mask & 0xFF) == 0) {
		ret = -EINVAL;
		goto Error;
	}

	for (;;) {
		// reg_data[] writes in the register.
		ret = cmps_hw_write_regs(start_reg, reg_data, n_regs);
		if (ret < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Error;
		}
		// The register is read buffer[].
		if ((ret =  cmps_hw_read_regs(start_reg, buffer, n_regs)) < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Error;
		}
		// collates buffer[0 - n_regs-1] and reg_data[0 - n_regs-1].
		// (Only the bit specified verify_mask.)
		for (i = 0;; i++) {
			if (i >= n_regs){
				compass_printk(KERN_ERR, "(%s %d) success \n", __FUNCTION__, __LINE__);
				goto Exit;		// Colation success
			}
			if ((buffer[i]^reg_data[i])&verify_mask){
				compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
				break;			// disagreement
			}
		}
		// Collation failure
		if (retry_count >= cmps->max_retry_count) {
			compass_printk(KERN_ERR, "%s: Reg write-verify error.\n", 
															CMPS_DEVNAME);
			ret = -EIO;
			goto Error;
		}
		retry_count++;
	}

Exit:
	if (retry_count > 0) {
Error:
		cmps->error_count++;
		cmps->retry_count += retry_count;
	}

	compass_printk(KERN_INFO,"%s() ok start_reg:0x%02X n_regs:%u verify_mask:0x%02X ret=%d\n"
							, __func__, start_reg, n_regs, verify_mask ,ret);

	return ret;
}


/*--------------------------------------------------------------------
Function  :The operational mode of HSCD is set.
@param    :mode:HSCD_MODE_{STANBY,FORCESTATE}.
Attention :When it is active.Begin the temperature survey at the same time. 
@retval   :Success:0,Error: -errno.
error     :(1) EINVAL:mode is illegal.
            (2) other :cmps_hw_write_verify_regs() 
--------------------------------------------------------------------*/
static int hscd_setmode(uint8_t mode)
{
	int ret;
	uint8_t reg_data;
	reg_data = 0x00;

	compass_printk(KERN_INFO,"%s() \n", __func__);
	compass_printk(KERN_DEBUG,"%s() mode:0x%02X\n", __func__, mode);

	if (mode == HSCD_MODE_STANBY) {
		// changes to an active mode
		reg_data = HSCD_ALL_ZERO;
		reg_data = HSCD_MODE_STANBY;
		ret = cmps_hw_write_verify_regs(HSCD_CNTL1, &reg_data, 1, HSCD_MODE_STANBY);
		if (ret < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
		//CNTL3 clear
		reg_data = HSCD_ALL_ZERO;
		reg_data = HSCD_CNTL3_STANBY;
		ret = cmps_hw_write_regs(HSCD_CNTL3, &reg_data, 1);
		if (ret < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
		//CNTL2 clear
		reg_data = HSCD_ALL_ZERO;
		reg_data = HSCD_CNTL2_CLEAR;
		ret = cmps_hw_write_verify_regs(HSCD_CNTL2, &reg_data, 1, HSCD_DRP);
		if (ret < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
	} else if (mode == HSCD_MODE_FORCESTATE) {
		// changes to an active mode
		reg_data = HSCD_ALL_ZERO;
		if (cmps->measurement.dynamic_range == HSCD_RS_DRANGE_14) {
			reg_data = HSCD_CNTL4_RS_DRANGE_14BIT;
			compass_printk(KERN_ERR, "HSCD_CNTL4_RS_DRANGE_14BIT\n");
		} else if (cmps->measurement.dynamic_range == HSCD_RS_DRANGE_15) {
			reg_data = HSCD_CNTL4_RS_DRANGE_15BIT;
			compass_printk(KERN_ERR, "HSCD_CNTL4_RS_DRANGE_15BIT\n");
		} else {
			//error
			ret = -EINVAL;
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
		//CNTL4 RANGE 14BIT,15BIT
		ret = cmps_hw_write_verify_regs(HSCD_CNTL4, &reg_data, 1, HSCD_CNTL4_RS_DRANGE_15BIT);
		if (ret < 0){
			compass_printk(KERN_ERR, "RANGE 14BIT,15BIT VERIFY_REGS ERROR\n");
			goto Exit;
		}
		compass_printk(KERN_ERR, "RANGE 14BIT,15BIT VERIFY_REGS OK [%x]\n",reg_data);
		//CNTL1 FORCESTATE 
		reg_data = HSCD_ALL_ZERO;
		reg_data = HSCD_MODE_FORCESTATE;
		ret = cmps_hw_write_regs(HSCD_CNTL1, &reg_data, 1);
		if (ret < 0){
			compass_printk(KERN_ERR, "HSCD_MODE_FORCESTATE WRITE ERROR\n");
			goto Exit;
		}
		compass_printk(KERN_ERR, "HSCD_MODE_FORCESTATE WRITE_REGS OK\n");

		//CNTL3 FRC->1
		reg_data = HSCD_ALL_ZERO;
		reg_data = HSCD_CNTL3_FORCE;
		ret = cmps_hw_write_regs(HSCD_CNTL3, &reg_data, 1);
		if (ret < 0){
			compass_printk(KERN_ERR, "HSCD_FRC->1 WRITE ERROR\n");
			goto Exit;
		}
		compass_printk(KERN_ERR, "HSCD_FRC->1 WRITE_REGS OK\n");

		//CNTL2 DEN->1 DRP->1
		reg_data = HSCD_ALL_ZERO;
		reg_data = HSCD_DATAREADY;
		ret = cmps_hw_write_verify_regs(HSCD_CNTL2, &reg_data, 1, HSCD_DATAREADY);
		if (ret < 0){
			compass_printk(KERN_ERR, "HSCD_DEN->1 WRITE ERROR\n");
			goto Exit;
		}
		compass_printk(KERN_ERR, "HSCD_DEN->1 VERIFY_REGS OK\n");
	} else {
		//error
		ret = -EINVAL;
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}

	CMPS_SETMODE(mode);

Exit:
	compass_printk(KERN_INFO,"%s() ok reg_data:0x%02X mode:0x%02X ret=%d\n",__func__,reg_data,mode,ret);
	return ret;
}

static int cmps_hw_setmode(uint8_t mode)
{
	int ret = 0;
	compass_printk(KERN_INFO,"%s() \n", __func__);
	ret = hscd_setmode(mode);
	compass_printk(KERN_INFO,"%s() ok ret:[%d]\n",__func__, ret);
	return ret;
}

/*--------------------------------------------------------------------
Function  :register of hscd is read. 
output    :cmps->reg:content of shcd
@retval   :Success:0,Error: -errno.
error     :cmps_{read,write}_verify_regs() 
--------------------------------------------------------------------*/
int cmps_hw_read_reg_all(void)
{
	int ret;
	unsigned nRegs;
	
	uint8_t buffer[CMPS_MAX_CONTIGUOUS_REGS];

	compass_printk(KERN_INFO,"%s()\n", __func__);

	// read TEMP
	ret = cmps_hw_read_verify_regs(HSCD_TEMP, &cmps->reg.temp,
												sizeof(cmps->reg.temp));
	if (ret < 0) {
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}

	// read DATA[XYZ]
	nRegs = sizeof(cmps->reg.data);
	ret = cmps_hw_read_verify_regs(HSCD_XLSB, buffer, nRegs);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read DATA[XYZ](%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}
	// MSB/LSB of buffer is replaced
	cmps->reg.data.x = buffer[0] + (buffer[1] << 8);
	cmps->reg.data.y = buffer[2] + (buffer[3] << 8);
	cmps->reg.data.z = buffer[4] + (buffer[5] << 8);

	// read STAT
	ret = cmps_hw_read_verify_regs(HSCD_STAT, &cmps->reg.stat, 1);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read STAT(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}
	// read CNTL1
	ret = cmps_hw_read_verify_regs(HSCD_CNTL1, &cmps->reg.cntl1, 1);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read CNTL1(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}
	// read CNTL2
	ret = cmps_hw_read_verify_regs(HSCD_CNTL2, &cmps->reg.cntl2, 1);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read CNTL2(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}
	// read CNTL3
	ret = cmps_hw_read_verify_regs(HSCD_CNTL3, &cmps->reg.cntl3, 1);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read CNTL3(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}
	// read CNTL4
	ret = cmps_hw_read_verify_regs(HSCD_CNTL4, &cmps->reg.cntl4, 1);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read CNTL4(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}

	// read OFF[XYZ]
	ret = cmps_hw_read_verify_regs(HSCD_XOFFLSB, buffer, 6);
	if (ret < 0) {
		compass_printk(KERN_ERR, "read OFF[XYZ](%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit;
	}
	// MSB/LSB of buffer is replaced
	cmps->reg.off.x = buffer[0] + (buffer[1] << 8);
	cmps->reg.off.y = buffer[2] + (buffer[3] << 8);
	cmps->reg.off.z = buffer[4] + (buffer[5] << 8);

Exit:
	compass_printk(KERN_INFO,"%s() X:%02X Y:%02X Z:%02X\n"
			, __func__, cmps->reg.off.x , cmps->reg.off.y , cmps->reg.off.z);
	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);

	return ret;
}

/*--------------------------------------------------------------------
Function  :Offset value of HSCD writes in the OFF[XYZ] register. 
@param    :*off:Offset value.
output    :cmps->reg.off:If it is a success, *off is written.
@retval   :Success:0,Error: -errno.
error     :cmps_hw_write_verify_regs()
--------------------------------------------------------------------*/
static int hscd_set_sensor_offset(const cmps_xyz_t * off)
{
	int ret;
	uint8_t buffer[CMPS_MAX_CONTIGUOUS_REGS];

	compass_printk(KERN_INFO,"%s()\n", __func__);

	// MSB/LSB of xyz is replaced
	buffer[0] = (uint8_t) (off->x & 0x00FF);
	buffer[1] = (uint8_t) ((off->x >> 8) & 0x007F);
	buffer[2] = (uint8_t) (off->y & 0x00FF);
	buffer[3] = (uint8_t) ((off->y >> 8) & 0x007F);
	buffer[4] = (uint8_t) (off->z & 0x00FF);
	buffer[5] = (uint8_t) ((off->z >> 8) & 0x007F);

	ret = cmps_hw_write_verify_regs(HSCD_XOFFLSB, buffer, 1, 0x7F);
	if (ret >= 0){
		cmps->reg.off = *off;
	}

	compass_printk(KERN_DEBUG,"%s() buffer: %02X %02X %02X %02X %02X %02X \n"
						, __func__, buffer[0] , buffer[1]
						, buffer[2], buffer[3], buffer[4], buffer[5]);
	compass_printk(KERN_INFO,"%s() ok X:%u Y:%u Z:%u -> %u ret=%d\n",
		__func__, off->x, off->y, off->z, *buffer, ret);

	return ret;
}

/*================== Initialization/Termination ====================*/

/*--------------------------------------------------------------------
Function  :Port that CMPS uses is initialized. 
--------------------------------------------------------------------*/
static int cmps_hw_port_init(void)
{
	int rc = 0;

	compass_printk(KERN_INFO,"%s()\n", __func__);

	/* FUJITSU:2013-05-31 mod_start */
	rc = gpio_tlmm_config( GPIO_CFG(JIKI_DRDY_GPIO, 0, GPIO_CFG_INPUT, 
						GPIOMUX_PULL_NONE, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	/* FUJITSU:2013-05-31 mod_end */

	if(rc < 0){
		compass_printk(KERN_ERR,"gpio_tlmm_config ng \n");
	return rc;
	}

	compass_printk(KERN_INFO, "GPIO CFG RESULT:%d\n", rc);

	rc = gpio_request(JIKI_DRDY_GPIO, "compass");
	if(rc < 0){
		compass_printk(KERN_ERR,"gpio_request ng \n");
	return rc;
	}
	
	compass_printk(KERN_INFO,"gpio_request ok \n");
	
	rc = gpio_direction_input(JIKI_DRDY_GPIO);
	if(rc < 0){
		compass_printk(KERN_ERR,"gpio_direction_input ng \n");
	return rc;
	}

	compass_printk(KERN_INFO,"%s() ok \n", __func__);
	
	return rc;

}

/*--------------------------------------------------------------------
Function  :Reset of the software of CMPS is executed. 
@param    :keep:true ,no change. 
                  false,Reset of the software is executed. 
@retval   :Success:2,Error: -errno.
error     : I/O error
--------------------------------------------------------------------*/
static int cmps_hw_chip_reset(bool keep)
{
	uint8_t regs[READREGS_MINBUFSIZE];
	bool error = false;
	unsigned retry_count = 0;
	int ret = OK;
	uint8_t cntl3 = HSCD_CNTL3_SRST;

	compass_printk(KERN_INFO,"%s()keep:%d\n", __func__ , keep);

	// CNTL3:SRST bit=1
	ret = cmps_hw_write_regs(HSCD_CNTL3, &cntl3, 1);
	if (ret < 0) {
		compass_printk(KERN_ERR, "Error: SoftReset  write cntl3 cntl3=%x.\n",
																		cntl3);
		goto Exit;
	}

/* FUJITSU:2013-05-01 COMPASS add start */	
	//udelay(HSCD_Trnw);			// Reset waiting
	msleep(1);
/* FUJITSU:2013-05-01 COMPASS add end */
	
	for (;;) {
		// if register of CNTL3 is zero,Reset complete
		if ((ret = cmps_hw_read_regs(HSCD_CNTL3, regs, 1)) >= 0) {
			if (0x00 == regs[0]) {
				compass_printk(KERN_INFO, "(%s %d) success \n", __FUNCTION__, __LINE__);
				break;			// Success
			} else {
				compass_printk(KERN_ERR, 
					"SoftReset  read cntl3 NG cntl3=%x.\n", regs[0]);
			}

		}
		// At the error
		error = true;
		if (retry_count >= cmps->max_retry_count) {
			// when retries reach upper limit:error
			compass_printk(KERN_ERR,
						   "Error: Unable to clear SoftReset.\n");
			break;
		}
		// retries
		retry_count++;
	}

Exit:
	if (error) {
		// error/retries total.
		cmps->error_count++;
		cmps->retry_count += retry_count;
	}

	compass_printk(KERN_INFO,"%s() ok keep:%d  ret=%d\n", __func__, keep , ret);

	return ret;
}

/*--------------------------------------------------------------------
Function  :Offset and amplifier gain value of HSCD 
            are reset in the following value. 
           Offset         :(0x0000, 0x0000, 0x0000)
           Amplifier gain :(EHXGA,EHYGA,EHZGA)
@retval   :Success:0,Error: -errno.
error     :cmps_hw_write_verify_regs() 
--------------------------------------------------------------------*/
static int cmps_hw_adjustment_reset(void)
{
	cmps_xyz_t off;
	int ret;

	compass_printk(KERN_INFO,"%s()\n", __func__);

	off.x = off.y = off.z = 0x0000;	// Offset:0
	ret = hscd_set_sensor_offset(&off);

	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);

	return ret;
}

/*--------------------------------------------------------------------
Function  :CMPS reset and initialize the driver.
@param    :first:Initialization : true
                   Reinitialize   : false
--------------------------------------------------------------------*/

int cmps_hw_init(bool first)
{
	int ret = 0;

	compass_printk(KERN_INFO,"%s() first:%d\n", __func__ , first);

	if (first) {
		compass_printk(KERN_DEBUG,"init_waitqueue_head start\n");
		mutex_init(&xfer_lock);
		init_waitqueue_head(&cmps->measure_end_queue);
		compass_printk(KERN_DEBUG,"init_waitqueue_head ok\n");
		cmps_hw_port_init();	// GPIO Initialization
	}

	// Initialize hardware.
	ret = cmps_hw_chip_reset(false);		// CMPS Reset
	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}
	ret = cmps_hw_adjustment_reset();	// Initialize the settings.
	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}
	// read register
	ret = cmps_hw_read_reg_all();
	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}

	compass_printk(KERN_INFO,"%s() ok \n", __func__ );

	return ret;
}

int cmps_hw_exit(void)
{
	int ret = 0;
	compass_printk(KERN_INFO,"%s()\n", __func__);
	// Initialize hardware.
	ret = cmps_hw_chip_reset(false);		// CMPS Reset
	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}
	ret = cmps_hw_adjustment_reset();	// Initialize the settings.
	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}

	// read register
	ret = cmps_hw_read_reg_all();
	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}

	gpio_free(JIKI_DRDY_GPIO);
	compass_printk(KERN_INFO,"%s() ok \n", __func__);
	return ret;
}

/*--------------------------------------------------------------------
Function  :start the measurement of magnetic.
@retval   :(1) 0:Success.
           (2) -EBUSY:I2C Communication error (time out).
           (3) -EIO  :I/O error.
--------------------------------------------------------------------*/
int cmps_hw_measure_start(void)
{
	int ret;
	compass_printk(KERN_INFO,"%s()\n", __func__);

	// Clear interrupt CMPS.
	if ((ret = cmps_sw_clear_interrupt()) >= 0) {
		// Measurement start.
		CMPS_SETSTATE(CMPS_STAT_MEASURING);
		ret = cmps_hw_setmode(HSCD_MODE_FORCESTATE);
	}

	if(ret < 0)
	{
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
	}

	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);

	return ret;
}

/*--------------------------------------------------------------------
Function  :measuring data of terrestrial magnetism read.
output    :cmps->measurement:Result of a measurement.
@retval   :cmps_read_verify_regs() 
--------------------------------------------------------------------*/
static int hscd_measure_end(void)
{
	int ret;
	unsigned nRegs;
	uint8_t buffer[CMPS_MAX_CONTIGUOUS_REGS];
	compass_data_t *const m = &cmps->measurement;

	compass_printk(KERN_INFO,"%s()\n", __func__);

	nRegs = sizeof(cmps->reg.data);
	ret = cmps_hw_read_verify_regs(HSCD_XLSB, buffer, nRegs);
	if (ret >= 0) {
		// MSB/LSB of buffer is replaced
		cmps->reg.data.x = buffer[0] + (buffer[1] << 8);
		cmps->reg.data.y = buffer[2] + (buffer[3] << 8);
		cmps->reg.data.z = buffer[4] + (buffer[5] << 8);

		compass_printk(KERN_DEBUG,"%s():DATA:(0x%04X 0x%04X 0x%04X)\n",
						__func__, cmps->reg.data.x,
						cmps->reg.data.y, cmps->reg.data.z);

		// The measuring data is copied onto cmps->measurement. 
		m->magnetism[0] = cmps->reg.data.x;	// DATAX
		m->magnetism[1] = cmps->reg.data.y;	// DATAY
		m->magnetism[2] = cmps->reg.data.z;	// DATAZ

		// Measurement data acquisition completion.
		CMPS_SETSTATE(CMPS_STAT_AVAILABLE);

		// PREVIOUS Measurement Data SAVE
		memcpy(&pre_meas , m , sizeof(compass_data_t));
		mb();
	}

	compass_printk(KERN_INFO,"%s() ok  nRegs:%02X ret=%d\n", __func__, nRegs , ret);

	return ret;
}

int cmps_hw_measure_end(void)
{
	int ret;

	compass_printk(KERN_INFO,"%s()\n", __func__);
	ret =  hscd_measure_end();
	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);
	return ret;
}


/*=========================== IOCTL ===========================*/

/*--------------------------------------------------------------------
Function  :start the measurement of magnetic,and Getting Results.
output    :result of a measurement, 
			and opening and shutting and state of the turn of the case.
	        *measurement
	        cmps->measurement
	        cmps->reg.temp
	        cmps->reg.data
@retval   :(1) 0:success
           (2) -EBUSY:I2C Communication error (time out).
           (3) -EIO:I/O error retries reach upper limit.
           (4) EINTR:Received a signal while waiting for measurement.
           (5) EFAULT:measurement adress Injustice.
--------------------------------------------------------------------*/
static int hscd_ioctl_measure(compass_data_t __user * measurement)
{
	int ret;
	int measurement_count = 0;

	compass_printk(KERN_INFO,"%s()\n", __func__);
	compass_printk(KERN_DEBUG,"%s() measurement:%p\n", __func__, measurement);
	ret = cmps_sw_module_stscheck(CMPS_MEASURE);
	if (ret == 0) {
		// Exclusive control start
		ret = copy_from_user(&cmps->measurement.dynamic_range,
						&measurement->dynamic_range,
						sizeof(measurement->dynamic_range));
		if (ret != 0) {
			compass_printk(KERN_ERR,"Can't copy to user area.%u\n", ret);
			ret = -EFAULT;
			goto Start_err;
		}
		
		compass_printk(KERN_INFO,"CMPS_STATE0:%d\n", CMPS_STATE());
		if ((ret = cmps_hw_measure_start()) < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Start_err;
		}

		// Measurement start success
		enable_irq(CMPS_IRQ());		// GPIO Permission
		
		// Waiting for  measurement complete
		compass_printk(KERN_DEBUG,"loop in\n");

		for (;;) {
			const long timeout = (CMPS_MAX_MEASUREMENT_TIME * HZ / 1000000)+1;

			//STAT Reading confirmation
			compass_printk(KERN_DEBUG,"CMPS_STATE:%d\n", CMPS_STATE());

			//During the measurement:wait
			ret = wait_event_interruptible_timeout(cmps->measure_end_queue,
				(CMPS_STATE() == CMPS_STAT_MEASURED), timeout);
			if (ret > 0){
				compass_printk(KERN_INFO, "(%s %d) success \n", __FUNCTION__, __LINE__);
				break;			// Measurements completed
			}
			if (ret < 0){
				compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
				goto Exit;
			}
			// time out:condition is revalued. 
			compass_printk(KERN_DEBUG,"CMPS_STATE:%d \n", CMPS_STATE());
			compass_printk(KERN_DEBUG,"%s(): wait_event_interruptible_timeout time:(%ld) ret=%d\n",
													__func__, timeout, ret);
			measurement_count++;
			if (measurement_count > CMPS_MAX_MEASUREMENT_COUNT) {
				compass_printk(KERN_ERR, 
					"%s: measurement wait event error.\n", CMPS_DEVNAME);
				ret = -EBUSY;
				goto Exit;
			}
		}
		compass_printk(KERN_DEBUG,"loop out\n");
		if ((ret = cmps_hw_measure_end()) < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
		CMPS_SETSTATE(CMPS_STAT_IDLE);

		// Return the number of retries and error
		cmps_update_error_count();
		cmps->measurement.total_error_count = cmps->total_error_count;
		cmps->measurement.total_retry_count = cmps->total_retry_count;
		if (copy_to_user(measurement, &cmps->measurement, 
			sizeof(*measurement)) != 0) {
			compass_printk(KERN_ERR,"Can't copy to user area.%u\n", ret);
			ret = -EFAULT;
		}
Exit:
		disable_irq(CMPS_IRQ());
		
Start_err:
		cmps_update_error_count();
		cmps_sw_module_stschg(CMPS_NONE);
	} else {
		/* Previous measurement DATA SET */
		if (copy_to_user(measurement, &pre_meas, 
			sizeof(*measurement)) != 0) {
			compass_printk(KERN_ERR,"Can't copy to user area.%u\n", ret);
			ret = -EFAULT;
		}
	}
	compass_printk(KERN_INFO,"%s() ok measurement_count:%d ret=%d\n", __func__, measurement_count
		, ret);

	return ret;
}

/*========================= File Operations ========================*/

/*--------------------------------------------------------------------
Function  :CMPS device is opened.  
@param    :(1) inode:inode Information of Device file (Unused).
            (2) file:open file Structure.
@retval   :(1) 0:success.
            (2) -EACCES:mode is (O_RDONLY) except.
            (3) -EINTR:Interrupt occurs during execution.
            (4) other : request_irq() 
--------------------------------------------------------------------*/
int cmps_hw_open(void)
{
	int ret = 0;

	compass_printk(KERN_INFO,"%s()\n", __func__);
	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);
	return ret;
}

/*--------------------------------------------------------------------
Function  :CMPS device is close
@param    :(1) inode:inode Information of Device file (Unused).
            (2) file:close file Structure.
@retval   :(1) 0:success.
            (2) -EINTR:Interrupt occurs during execution.
--------------------------------------------------------------------*/
int cmps_hw_release(void)
{
	int ret = 0;

	compass_printk(KERN_INFO,"%s()\n", __func__);

	// CMPS Reset
	ret = cmps_hw_exit();
	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);
	return ret;
}

/*--------------------------------------------------------------------
Function  :start the temperature Getting Results.
output    :result of a temperature shutting and state of the turn of the case.
	        *measurement
	        cmps->reg.temp
@retval   :(1) 0:success
           (2) -EBUSY:I2C Communication error (time out).
           (3) -EIO:I/O error retries reach upper limit.
           (4) EINTR:Received a signal while waiting for measurement.
           (5) EFAULT:measurement adress Injustice.
--------------------------------------------------------------------*/
static int hscd_ioctl_measure_temp(compass_data_t __user * measurement)
{
	int ret,retry_count;
	uint8_t reg_data, buffer;
	compass_data_t *const m = &cmps->measurement;

	retry_count = 0;

	compass_printk(KERN_INFO,"%s()\n",__func__);

	ret = cmps_sw_module_stscheck(CMPS_MSRTEMP);
	if (ret == 0) {
		// changes to an active mode
		reg_data = HSCD_MODE_FORCESTATE;
		ret = cmps_hw_write_verify_regs(HSCD_CNTL1, &reg_data, 1, HSCD_MODE_FORCESTATE);
		if (ret < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
		compass_printk(KERN_INFO,"CMPS_STATE0:%d\n", CMPS_STATE());
		// CNTL3 is temperature and forcestate measurement
		reg_data = HSCD_CNTL3_TCS;
		ret = cmps_hw_write_regs(HSCD_CNTL3, &reg_data, 1);
		if (ret < 0){
			compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
			goto Exit;
		}
	
		for (;;) {
			ret = cmps_hw_read_verify_regs(HSCD_STAT, 
						&buffer, sizeof(cmps->reg.stat));
			if (ret < 0){
				compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
				goto Exit;
			}
			if ((buffer & HSCD_STAT_TRDY) == HSCD_STAT_TRDY) {
				ret = cmps_hw_read_verify_regs(HSCD_TEMP, &cmps->reg.temp,
												sizeof(cmps->reg.temp));
				if (ret < 0){
					compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
					goto Exit;
				}
				compass_printk(KERN_INFO,"temp measure completion\n");
				break;
			}
/* FUJITSU:2013-05-08 COMPASS add start */
			if (retry_count > 5) {
/* FUJITSU:2013-05-08 COMPASS add end */
				compass_printk(KERN_ERR,
					"%s: temp data get error.\n", CMPS_DEVNAME);
				ret = -EIO;
				goto Exit;
			}
			mdelay(1);		// Waiting time
			retry_count++;
		}
		
		// The measuring data is copied onto cmps->measurement. 
		m->temperature = cmps->reg.temp;	// TEMP
		if (copy_to_user(measurement, &cmps->measurement, 
			sizeof(*measurement)) != 0) {
			compass_printk(KERN_ERR," %s Can't copy to user area.\n",__func__);
			ret = -EFAULT;
			goto Exit;
		}
		compass_printk(KERN_INFO,"%s() tmp=[%d]degreed\n", __func__, cmps->reg.temp);
		cmps_sw_module_stschg(CMPS_NONE);
	} else {
		compass_printk(KERN_ERR, "(%s %d)\n", __FUNCTION__, __LINE__);
		goto Exit2;
	}

	ret = 0;
	compass_printk(KERN_INFO,"%s() ok ret=%d\n", __func__, ret);
	return ret;
Exit:
	compass_printk(KERN_ERR,"%s() ng ret=%d\n", __func__, ret);
	cmps_sw_module_stschg(CMPS_NONE);

Exit2:
	compass_printk(KERN_INFO,"%s() busy ret=%d\n", __func__, ret);
	return ret;
}


long cmps_hw_measure(compass_data_t __user * arg)
{
	long ret;
	compass_printk(KERN_INFO,"%s()\n", __func__);

	ret = hscd_ioctl_measure((compass_data_t __user *)arg);
	compass_printk(KERN_INFO,"%s() ok ret:[%ld]\n",__func__, ret);
	return ret;
}

long cmps_hw_measure_temp(compass_data_t __user * arg)
{
	long ret;
	compass_printk(KERN_INFO,"%s()\n", __func__);
	ret = hscd_ioctl_measure_temp((compass_data_t __user *)arg);
	compass_printk(KERN_INFO,"%s() ok ret:[%ld]\n",__func__, ret);
	return ret;
}

MODULE_AUTHOR ("FUJITSU LIMITED");
MODULE_DESCRIPTION ("hscdtd008 driver");
MODULE_LICENSE ("GPL");
