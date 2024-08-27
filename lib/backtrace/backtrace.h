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


#ifndef __LINUX_BACKTRACE_H
#define __LINUX_BACKTRACE_H

#include <linux/miscdevice.h>
#include <linux/mtd/mtd.h>


#define TAG "backtrace"

typedef enum    BacktraceLogType
{
    LogTypeEMERG,           /* "<0>"  KERN_EMERG    system is unusable               */
    LogTypeALERT,           /* "<1>"  KERN_ALERT    action must be taken immediately */
    LogTypeCRIT,            /* "<2>"  KERN_CRIT     critical conditions              */
    LogTypeERR,             /* "<3>"  KERN_ERR      error conditions                 */
    LogTypeWARNING,         /* "<4>"  KERN_WARNING  warning conditions               */
    LogTypeNOTICE,          /* "<5>"  KERN_NOTICE   normal but significant condition */
    LogTypeINFO,            /* "<6>"  KERN_INFO     informational                    */
    LogTypeDEBUG,           /* "<7>"  KERN_DEBUG    debug-level messages             */
    LogTypeALL,
}   BacktraceLogType;
//#define BACKTRACE_LOG_OK    LogTypeEMERG    //  all stop
//#define BACKTRACE_LOG_OK    LogTypeALL      //  all OK
//#define BACKTRACE_LOG_OK    LogTypeDEBUG    
/* FUJITSU:2013-02-27 KERNEL LOG start */
/* FUJITSU:2013-04-08 MUTEX start */
#define BACKTRACE_LOG_OK    LogTypeNOTICE     
/* FUJITSU:2013-04-08 MUTEX end */
/* FUJITSU:2013-02-27 KERNEL LOG end */

#define dprintk(logtype,fmt,...)    if(logtype < BACKTRACE_LOG_OK){ printk("<%d>" TAG ":%s<%d>" fmt,logtype, __FUNCTION__ , __LINE__ , ## __VA_ARGS__);}


extern asmlinkage void get_asm_backtrace(unsigned long fp, int pmode);

#endif // __LINUX_BACKTRACE_H
