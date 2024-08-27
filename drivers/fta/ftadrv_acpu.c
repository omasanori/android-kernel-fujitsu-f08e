/*
 * ftadrv_acpu.c - FTA (Fault Trace Assistant) 
 *                 driver for acpu proc entry
 *
 * Copyright(C) 2011 FUJITSU LIMITED
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "fta_config.h"
extern struct procDataType *ftadrv_prepare_proc(int type);
extern void ftadrv_free_proc(struct procDataType *pd);

/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */
#define WRITE_SIZE PAGE_SIZE

/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */
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

static struct seq_operations acpu_seq_op = {
    .start = as_start,
    .next  = as_next,
    .stop  = as_stop,
    .show  = as_show,
};

/*!
 @brief acpu_proc_open

 open function for /proc/fta/current_a

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int acpu_proc_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;
    pd = ftadrv_prepare_proc(FTA_CA);

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&acpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

static struct file_operations acpu_op = {
    .open = acpu_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

/*!
 @brief acpu_proc_reboot_open

 open function for /proc/fta/reboot_a

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int acpu_proc_reboot_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;
    pd = ftadrv_prepare_proc(FTA_RA);

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&acpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

static struct file_operations acpu_reboot_op = {
    .open = acpu_proc_reboot_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

/*!
 @brief acpu_proc_open

 open function for /proc/fta/current_a

 @param [in]   inode   pointer of the inode struct
 @param [in]   file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int db_proc_open(struct inode *inode, struct file *file)
{
    int ret;
    struct procDataType *pd;
    pd = ftadrv_prepare_proc(FTA_CD);

    if(pd == 0)
      return -ENOENT;
    ret = seq_open(file,&acpu_seq_op);
    if(ret == 0) {
      ((struct seq_file *)file->private_data)->private = (void *)pd;
    } else {
      printk(KERN_ERR "seq open error\n");
      ftadrv_free_proc(pd);
    }
    return ret;
}

static struct file_operations db_op = {
    .open = db_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

/*!
 @brief ftadrv_acpu_init

 initialize /proc/fta/current_a and /proc/fta/reboot_a

 @param [in]   proc_dir_entry pointer of the parent of procfs

 @retval      0: success
 @retval     <0: failed
*/
int __init ftadrv_acpu_init(struct proc_dir_entry *parent)
{
    struct proc_dir_entry *entry;
    entry = create_proc_entry("current_a",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &acpu_op;

    entry = create_proc_entry("reboot_a",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &acpu_reboot_op;

    entry = create_proc_entry("current_d",0400,parent);
    if ( entry == NULL) {
        printk( KERN_ERR "create_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->proc_fops = &db_op;

    return 0;
}

MODULE_DESCRIPTION("FTA Driver");
MODULE_LICENSE("GPL v2");
