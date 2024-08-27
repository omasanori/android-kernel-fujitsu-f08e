/*
 * ftadrv_ccpu.c - FTA (Fault Trace Assistant) 
 *                 driver for ccpu proc entry
 *
 * Copyright(C) 2011-2012 FUJITSU LIMITED
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

/* ==========================================================================
 *  INCLUDE HEADER
 * ========================================================================== */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/dirent.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "fta_config.h"
extern struct procDataType *ftadrv_prepare_proc(int type);
extern void ftadrv_free_proc(struct procDataType *pd);

/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */
/* 2012-08-20 add start */
#if 0
#define DBGPRINT(msg...) printk(KERN_INFO "ftadrv_ccpu:" msg)
#else
#define DBGPRINT(msg...)
#endif
/* 2012-08-20 add end */

/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */
#include <linux/seq_file.h>
#define WRITE_SIZE PAGE_SIZE

/*!
 @brief as_start

 start function for seq_file

 @param [in]   m     pointer of the seq_file struct
 @param [in]   pos   pointer of the offset

 @retval       none
*/
static void *as_start(struct seq_file *m, loff_t *pos)
{
    int n = *pos;
    struct procDataType *pd = (struct procDataType *)(m->private);

    if(pd == NULL)
      return 0;
    if(pd->size > (n*WRITE_SIZE)) {
      return (void *)(n + 1);
   }

   m->private = 0;
   ftadrv_free_proc(pd);
   return 0;
}

/*!
 @brief as_next

 next function for seq_file

 @param [in]   m     pointer of the seq_file struct
 @param [in]   p     pointer of the output data
 @param [out]  pos   pointer of the offset

 @retval       none
*/
static void *as_next(struct seq_file *m,void *p,loff_t *pos)
{
   int n = (int)p;
   struct procDataType *pd = (struct procDataType *)(m->private);

   if(pd == NULL)
      return 0;
   (*pos)++;
   if(pd->size > (n*WRITE_SIZE)) {
     return (void *)(n + 1);
   }

   m->private = 0;
   ftadrv_free_proc(pd);
   return 0;
}

/*!
 @brief as_stop

 stop function for seq_file

 @param [in]   m     pointer of the seq_file struct
 @param [in]   p     pointer of the output data

 @retval       none
*/
static void as_stop(struct seq_file *m,void *p)
{
    // do nothing
}

/*!
 @brief as_show

 output function for seq_file

 @param [in]   m     pointer of the seq_file struct
 @param [in]   p     pointer of the output data

 @retval       none
*/
static int as_show(struct seq_file *m,void *p)
{
    struct procDataType *pd = (struct procDataType *)(m->private);
    char *ptr;
    int n = (int)p - 1;
    size_t size;

    if(pd == NULL || n < 0)
      return 0;

    ptr = pd->ptr;
       n *= WRITE_SIZE;
       size = min_t(size_t,pd->size - n,WRITE_SIZE);
    seq_write(m,&ptr[n],size);
    return 0;
}

static struct seq_operations ccpu_seq_op = {
    .start = as_start,
    .next  = as_next,
    .stop  = as_stop,
    .show  = as_show,
};

/*!
 @brief ccpu_proc_open

 open function for /proc/fta/current_c

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ccpu_proc_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;

/* 2012-09-12 mod start */
    DBGPRINT("%s.\n", __FUNCTION__);
    if ((file->f_mode & FMODE_READ) && !(file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_CC);
    else if (!(file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_SC);
    else
      return -EACCES;
/* 2012-09-12 mod end */

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&ccpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

/* 2012-08-20 add start */
/*!
 @brief ccpu_proc_release

 release function for /proc/fta/current_c, /proc/fta/reboot_c,
                      /proc/fta/current_n, /proc/fta/reboot_n

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ccpu_proc_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = (struct seq_file *)file->private_data;
    struct procDataType *pd = (struct procDataType *)(m->private);
    DBGPRINT("%s.\n", __FUNCTION__);
    m->private = 0;
    if(pd != 0)
        ftadrv_free_proc(pd);
    return seq_release(inode, file);
}
/* 2012-08-20 add end */

/* 2012-08-20 add start */
/*!
 @brief ccpu_proc_write

 write function for /proc/fta/reboot_c, /proc/fta/reboot_n

 @param [in]  file   pointer of the file struct
 @param [in]  buf    pointer of the write data
 @param [in]  size   size of the write data
 @param [in]  ppos   pointer of the offset

 @retval     >0: success
 @retval     <0: failed
*/
static ssize_t ccpu_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    struct seq_file *m = (struct seq_file *)file->private_data;
    struct procDataType *pd = (struct procDataType *)(m->private);
    int ret = 0;

    DBGPRINT("%s(0x%p,0x%p,%d,%lld). pd=0x%p\n", __FUNCTION__, file,buf,size,*ppos,pd);
    if(pd == NULL)
      return -ENOSPC;
    if(size == 0)
      return 0;

    if((*ppos) >= pd->size) {
      DBGPRINT("%s:if((*ppos) >= pd->size) == TRUE ->ENOSPC\n", __FUNCTION__);
      return -ENOSPC;
    }
    if((*ppos) + size > pd->size) {
      ret = pd->size - (*ppos);
    } else {
      ret = size;
    }
    if(copy_from_user(&pd->ptr[(*ppos)], buf, ret)) {
      DBGPRINT("%s:if(copy_from_user(&pd->ptr[(*ppos)], buf, ret)) == TRUE ->EFAULT\n", __FUNCTION__);
      return -EFAULT;
    }
    (*ppos) += size;
    return ret;
}
/* 2012-08-20 add end */

static struct file_operations ccpu_op = {
    .open = ccpu_proc_open,
    .read = seq_read,
    .write = ccpu_proc_write, /* 2012-09-12 add */
    .llseek = seq_lseek,
    .release = ccpu_proc_release, /* 2012-08-20 mod */
};

/*!
 @brief ccpu_proc_reboot_open

 open function for /proc/fta/reboot_c

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ccpu_proc_reboot_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;

/* 2012-08-20 mod start */
    DBGPRINT("%s.\n", __FUNCTION__);
    if ((file->f_mode & FMODE_READ) && !(file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_RC);
    else if (!(file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_WC);
    else
      return -EACCES;
/* 2012-08-20 mod end */

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&ccpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

static struct file_operations ccpu_reboot_op = {
    .open = ccpu_proc_reboot_open,
    .read = seq_read,
    .write = ccpu_proc_write, /* 2012-08-20 add */
    .llseek = seq_lseek,
    .release = ccpu_proc_release, /* 2012-08-20 mod */
};

/* 2012-08-20 add start */
/*!
 @brief ncpu_proc_open

 open function for /proc/fta/current_n

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ncpu_proc_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;

/* 2012-09-12 mod start */
    DBGPRINT("%s.\n", __FUNCTION__);
    if ((file->f_mode & FMODE_READ) && !(file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_CN);
    else if (!(file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_SN);
    else
      return -EACCES;
/* 2012-09-12 mod end */

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&ccpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

static struct file_operations ncpu_op = {
    .open = ncpu_proc_open,
    .read = seq_read,
    .write = ccpu_proc_write, /* 2012-09-12 add */
    .llseek = seq_lseek,
    .release = ccpu_proc_release,
};

/*!
 @brief ncpu_proc_reboot_open

 open function for /proc/fta/reboot_n

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ncpu_proc_reboot_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;

    DBGPRINT("%s.\n", __FUNCTION__);
    if ((file->f_mode & FMODE_READ) && !(file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_RN);
    else if (!(file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))
      pd = ftadrv_prepare_proc(FTA_WN);
    else
      return -EACCES;

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&ccpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

static struct file_operations ncpu_reboot_op = {
    .open = ncpu_proc_reboot_open,
    .read = seq_read,
    .write = ccpu_proc_write,
    .llseek = seq_lseek,
    .release = ccpu_proc_release,
};
/* 2012-08-20 add end */

/*!
 @brief ftadrv_ccpu_init

 initialize /proc/fta/current_c and /proc/fta/reboot_c

 @param [in]   proc_dir_entry pointer of the parent of procfs

 @retval      0: success
 @retval     <0: failed
*/
int __init ftadrv_ccpu_init(struct proc_dir_entry *parent)
{
   struct proc_dir_entry *entry;

    entry = create_proc_entry("current_c",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &ccpu_op;

    entry = create_proc_entry("reboot_c",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &ccpu_reboot_op;

/* 2012-08-20 add start */
    entry = create_proc_entry("current_n",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &ncpu_op;

    entry = create_proc_entry("reboot_n",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &ncpu_reboot_op;
/* 2012-08-20 add end */

    return 0;
}

MODULE_DESCRIPTION("FTA Driver");
MODULE_LICENSE("GPL v2");
