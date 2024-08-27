/*
 * ftadrv.c - FTA (Fault Trace Assistant) 
 *            driver main
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
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/uio.h>
#include <linux/list.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <linux/reboot.h>  /* emargency_restart */

#include <asm/cacheflush.h>
#include <linux/bootmem.h>
#include <linux/kmemleak.h>
#include <linux/vmalloc.h>
#include <linux/delay.h> /* mdelay */

#include <linux/zlib.h>
#include <linux/crc32.h>

#include <linux/syscalls.h> /* sys_* */

#include "fta_i.h"
#include "fta_config.h"

/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */
#define FTA_ALOG_EXPORT_ADDR 0x88000000

/* 2012-08-20 add start */
#define FTA_CLOG_R_SIZE      0x00100020
#define FTA_CLOG_SIZE        0x00080000
#define FTA_NLOG_SIZE        0x00080000

const char fta_dummy_header_c[] = {
                                  0x7E,0x6C,0x63,0x6D,0x20,0x00,0x00,0x00,0x01,0x00,0x14,0x00,0x00,0x00,0x14,0x00,
                                  0x00,0x00,0xCD,0xC4,0x64,0x2E,0x8D,0x54,0xDF,0xCD,0x81,0x4D,0x79,0x21,0x3F,0x1E,
                                  0x66,0x33,0x64,0x30,0x20,0x00,0x08,0x00,0x01,0x00,0x18,0x00,0x01,0x00,0x01,0x00,
                                  0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x08,0x00,
                                  };
const char fta_dummy_header_n[] = {
                                  0x7E,0x6C,0x63,0x6D,0x20,0x00,0x00,0x00,0x01,0x00,0x14,0x00,0x00,0x00,0x14,0x00,
                                  0x00,0x00,0xCD,0xC4,0x64,0x2E,0x8D,0x54,0xDF,0xCD,0x81,0x4D,0x79,0x21,0x3F,0x1E,
                                  0x66,0x33,0x64,0x30,0x20,0x00,0x08,0x00,0x01,0x00,0x18,0x00,0x01,0x00,0x01,0x00,
                                  0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x08,0x00,
                                  };
#if 0
#define DBGPRINT(msg...) printk(KERN_INFO "ftadrv:" msg)
#define IS_DBGPRINT 1
#else
#define DBGPRINT(msg...)
#define IS_DBGPRINT 0
#endif
/* 2012-08-20 add end */

static DEFINE_SPINLOCK(work_lock);
static struct workqueue_struct *work_queue;
static struct work_struct write_work;
static struct work_struct proc_work;
static struct list_head write_head;
static struct list_head proc_head;
#define MAX_BUF_SIZE 3072
struct writeDataType {
    struct list_head list;
    int len;
    char buf[MAX_BUF_SIZE];
};

struct readDataMngType {
    int init;
    wait_queue_head_t wq;
    struct list_head head;
};
static struct readDataMngType readDataMng;
static DEFINE_SPINLOCK(read_lock);

struct readDataType {
    struct list_head list;
    char buf[128];
};

char *fta_log_export;
char *fta_log_area;
static int ftadrv_cnt;

/* 2012-08-20 add start */
enum {
  FTA_LOG_EXPORT_MODE_EMPTY = 0,
  FTA_LOG_EXPORT_MODE_ACCEPT,
  FTA_LOG_EXPORT_MODE_DEFLATE,
  FTA_LOG_EXPORT_MODE_SUBMIT,
};
/* 2012-08-20 add end */
/* 2012-09-12 add start*/
struct exportLogSrvType {
  char *save_ptr;
  int size;
  int mode;
};
enum {
  FTA_LOG_EXPORT_SRV_CR = 0,
  FTA_LOG_EXPORT_SRV_CC,
  FTA_LOG_EXPORT_SRV_NR,
  FTA_LOG_EXPORT_SRV_NC,
  FTA_LOG_EXPORT_SRV_MAX
};
enum {
  FTA_LOG_EXPORT_SRV_DR,
  FTA_LOG_EXPORT_SRV_DW,
  FTA_LOG_EXPORT_SRV_DMAX
};
static struct exportLogSrvType mlogsrv[FTA_LOG_EXPORT_SRV_MAX];
static const char *logsrv_dummy_header[FTA_LOG_EXPORT_SRV_MAX] = {
  fta_dummy_header_c, fta_dummy_header_c,
  fta_dummy_header_n, fta_dummy_header_n,
};
static const int logsrv_dummy_header_size[FTA_LOG_EXPORT_SRV_MAX] = {
  sizeof(fta_dummy_header_c), sizeof(fta_dummy_header_c),
  sizeof(fta_dummy_header_n), sizeof(fta_dummy_header_n),
};
static int logsrv_alloc_size[FTA_LOG_EXPORT_SRV_MAX] = {
  FTA_CLOG_R_SIZE, FTA_CLOG_SIZE,
  FTA_NLOG_SIZE, FTA_NLOG_SIZE,
};
/* 2012-09-12 add end*/

extern void fta_log_message_kmsg(const unsigned char *srcname, unsigned long info,  const unsigned char *time, const unsigned char *msg,unsigned long msglen);
extern void fta_initialize(void);
extern void fta_terminate(void);
extern unsigned char *fta_var_alloc_log(unsigned long key,unsigned long len);

extern int ftadrv_acpu_init(struct proc_dir_entry *entry);
extern int ftadrv_ccpu_init(struct proc_dir_entry *entry);

extern int fta_convert_buf(unsigned char *buf);
extern void fta_write(int area,unsigned char *buf,unsigned long len);

/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */
/*!
 @brief ftadrv_write_work

 process for the write event

 @param [in]  work   pointer of the workqueue struct

 @retval      none
*/
static void ftadrv_write_work(struct work_struct *work)
{
    struct writeDataType *p,*tmp;
    unsigned long irqflags;
    
    spin_lock_irqsave(&work_lock, irqflags);
    list_for_each_entry_safe(p,tmp,&(write_head),list) {
        int area = fta_convert_buf(p->buf);
        if(area < 0) {
          /* silent discard */
        } else {
            fta_write(area,&p->buf[4],p->len-4);
        }
        list_del(&p->list);
        kfree(p);
    }
    spin_unlock_irqrestore(&work_lock, irqflags);
}

static void ftadrv_deflate(struct procDataType *pd,char *ptr,int size)
{
   struct z_stream_s zs;
   char *outbuf  = 0;
   int outsize = size,err;
   unsigned int crc;

   memset(&zs,0,sizeof(zs)); /* 2012-08-20 add */

   outbuf = vmalloc(outsize);
   if(unlikely(outbuf == 0)) {
      printk(KERN_ERR "no memory\n");
      goto no_compress;
   }

   zs.workspace = vzalloc(zlib_deflate_workspacesize(-15, 8));
   if (unlikely(!zs.workspace)) {
      printk(KERN_ERR "no memory\n");
      goto no_compress;
   }
   err = zlib_deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                   -15, 8, Z_DEFAULT_STRATEGY);
   if (unlikely(err != Z_OK)) {
      printk(KERN_ERR "deflateInit err:%d\n",err);
      goto no_compress;
   }

   crc = ~crc32(0xffffffff,ptr,size);

   err = zlib_deflateReset(&zs);
   if (unlikely(err != Z_OK)) {
      printk(KERN_ERR "deflateReset err:%d\n",err);
      goto no_compress;
   }

   zs.next_in = (u8 *)ptr;
   zs.avail_in = size;
   zs.next_out = (u8 *)outbuf;
   zs.avail_out = outsize;

   err = zlib_deflate(&zs, Z_FINISH);
   if (unlikely(err != Z_STREAM_END)) {
      printk(KERN_ERR "deflate err:%d\n",err);
      goto no_compress;
   }
   outsize = zs.total_out;
   vfree(zs.workspace);

   memcpy(&outbuf[outsize],(unsigned char *)&crc,4);
   outsize += 4;
   memcpy(&outbuf[outsize],(unsigned char *)&size,4);
   outsize += 4;
   pd->ptr = outbuf;
   pd->size = outsize;
   return;
no_compress:
   if(outbuf)
     vfree(outbuf);
   if(zs.workspace)
     vfree(zs.workspace);
   pd->ptr = ptr;
   pd->size = size;
   return;
}

/* 2012-09-12 add start */
static int ftadrv_logsrv_type_to_srv(int type)
{
    int srv = -1;
    switch(type)
    {
    case FTA_RC:
    case FTA_WC:
      srv = FTA_LOG_EXPORT_SRV_CR;
      break;
    case FTA_CC:
    case FTA_SC:
      srv = FTA_LOG_EXPORT_SRV_CC;
      break;
    case FTA_RN:
    case FTA_WN:
      srv = FTA_LOG_EXPORT_SRV_NR;
      break;
    case FTA_CN:
    case FTA_SN:
      srv = FTA_LOG_EXPORT_SRV_NC;
      break;
    default:
      DBGPRINT("unsupported operation.\n");
      break;
    }
    return srv;
}
static int ftadrv_logsrv_type_to_rw(int type)
{
    int rw = -1;
    switch(type)
    {
    case FTA_RC:
    case FTA_CC:
    case FTA_RN:
    case FTA_CN:
      rw = FTA_LOG_EXPORT_SRV_DR;
      break;
    case FTA_WC:
    case FTA_SC:
    case FTA_WN:
    case FTA_SN:
      rw = FTA_LOG_EXPORT_SRV_DW;
      break;
    default:
      DBGPRINT("unsupported operation.\n");
      break;
    }
    return rw;
}
static const char *ftadrv_logsrv_type_name(int srv, int rw)
{
    const char *tn[FTA_LOG_EXPORT_SRV_DMAX][FTA_LOG_EXPORT_SRV_MAX] =
    {
      {"WC","SC","WN","SN"},
      {"RC","CC","RN","CN"},
    };
    if(srv < 0 || rw < 0)
      return NULL;
    return tn[rw][srv];
}

static void ftadrv_proc_work_logsrv(struct procDataType *pd)
{
    int srv, rw;
    const char *tn;
    char *ptr;

    srv = ftadrv_logsrv_type_to_srv(pd->type);
    rw  = ftadrv_logsrv_type_to_rw(pd->type);
    if(srv < 0 || rw < 0)
      return;
    tn = ftadrv_logsrv_type_name(srv, rw);

    switch(rw) {
    case FTA_LOG_EXPORT_SRV_DW:
       if (mlogsrv[srv].mode == FTA_LOG_EXPORT_MODE_EMPTY) {
          DBGPRINT("(%s)empty->\n",tn);
          ptr = vmalloc(logsrv_alloc_size[srv]+logsrv_dummy_header_size[srv]);
          if (ptr == 0) {
             printk(KERN_ERR "vmalloc error\n");
             pd->ret = -ENOSPC;
             break;
          }
          pd->ptr = ptr;
          pd->size = logsrv_alloc_size[srv]+logsrv_dummy_header_size[srv];
          mlogsrv[srv].save_ptr = pd->ptr;
          mlogsrv[srv].size     = pd->size;
          memcpy(ptr,logsrv_dummy_header[srv],logsrv_dummy_header_size[srv]);
          pd->ptr += logsrv_dummy_header_size[srv]; /* adjust dummy header */
          pd->size -= logsrv_dummy_header_size[srv];
          mlogsrv[srv].mode     = FTA_LOG_EXPORT_MODE_ACCEPT;
          DBGPRINT("(%s)->accept. pd->ptr=0x%p,pd->size=%d\n",tn, pd->ptr, pd->size);
       } else {
          printk(KERN_ERR "already allocated\n");
          pd->ret = -EINVAL;
          break;
       }
       break;

    case FTA_LOG_EXPORT_SRV_DR:
       if (mlogsrv[srv].mode == FTA_LOG_EXPORT_MODE_DEFLATE) {
          DBGPRINT("(%s)deflate->\n",tn);
          pd->ptr = mlogsrv[srv].save_ptr;
          pd->size = mlogsrv[srv].size;
          mlogsrv[srv].mode = FTA_LOG_EXPORT_MODE_SUBMIT;
          DBGPRINT("(%s)->submit. pd->ptr=0x%p,pd->size=%d\n",tn, pd->ptr, pd->size);
       } else {
          printk(KERN_WARNING "access denied\n");
          pd->ret = -EINVAL;
          break;
       }
       break;
    }
}
/* 2012-09-12 add end */

static void ftadrv_proc_work(struct work_struct *work)
{
    unsigned long irqflags;
    struct procDataType *pd;
    char *ptr;

start:
    spin_lock_irqsave(&work_lock, irqflags);
    list_for_each_entry(pd,&proc_head,list) {
      if (pd->state == 0) {
         pd->state = 1;
         spin_unlock_irqrestore(&work_lock, irqflags);
         goto next;
      }
    }
    spin_unlock_irqrestore(&work_lock, irqflags);
    return;

next:
    switch(pd->type) {
    case FTA_RA:
       if (fta_log_export == 0) {
          printk(KERN_INFO "no log(a)\n");
          pd->ret = -ENOENT;
          break;
       }
       ptr = fta_log_export;
       fta_log_export = 0;
       ftadrv_deflate(pd,ptr,FTA_CONFIG_STORE_MEMORY_SIZE);
       if(pd->ptr != ptr) vfree(ptr); /* 2012-08-20 add */
       break;
    case FTA_CA:
       ptr = vmalloc(FTA_CONFIG_STORE_MEMORY_SIZE);
       if (ptr == 0) {
          printk(KERN_ERR "vmalloc error\n");
          pd->ptr = fta_log_area;
          pd->size = FTA_CONFIG_STORE_MEMORY_SIZE;
          break;
       }
       FTA_ENTER_LOCK();
       memcpy(ptr,fta_log_area,FTA_CONFIG_STORE_MEMORY_SIZE);
       FTA_LEAVE_LOCK();
       ftadrv_deflate(pd,ptr,FTA_CONFIG_STORE_MEMORY_SIZE);
       if(pd->ptr != ptr) vfree(ptr); /* 2012-08-20 add */
       break;
/* 2012-08-20 add start */ /* 2012-09-12 mod start */
    case FTA_WC:
    case FTA_RC:
    case FTA_WN:
    case FTA_RN:
    case FTA_CC:
    case FTA_SC:
    case FTA_CN:
    case FTA_SN:
      ftadrv_proc_work_logsrv(pd);
      break;
/* 2012-08-20 add end */ /* 2012-09-12 mod end */
    case FTA_CD:
       {
          int fd = sys_open("/data/data/com.android.providers.settings/databases/settings.db",O_RDONLY,0);
          struct stat64 st; /* struct stat st; */
          int rc;
          if(fd < 0) {
             printk(KERN_ERR "db open failed\n");
             pd->ret = -ENOENT;
             break;
          }
          rc = sys_fstat64(fd,&st); /* rc = sys_newfstat(fd,&st); */
          if (rc != 0) {
             printk(KERN_ERR "sys_fstat64(%d) error(%d)\n",fd,rc);
             sys_close(fd);
             pd->ret = -ENOENT;
             break;
          }
          ptr = vmalloc(st.st_size);
          if (ptr == 0) {
             printk(KERN_ERR "vmalloc error\n");
             sys_close(fd);
             pd->ret = -ENOSPC;
             break;
          }
          sys_read(fd,ptr,st.st_size);
          sys_close(fd);
          ftadrv_deflate(pd,ptr,st.st_size);
          if(pd->ptr != ptr) vfree(ptr); /* 2012-08-20 add */
          break;
       }
    default:
       pd->ret = -EINVAL;
    }
    complete(&pd->comp);
    goto start;
}

/* 2012-09-12 add start */
static void ftadrv_free_work_logsrv(struct procDataType *pd)
{
    int srv, rw;
    const char *tn;
    char *ptr;

    srv = ftadrv_logsrv_type_to_srv(pd->type);
    rw  = ftadrv_logsrv_type_to_rw(pd->type);
    if(srv < 0 || rw < 0)
      return;
    tn = ftadrv_logsrv_type_name(srv, rw);

    switch(rw)
    {
    case FTA_LOG_EXPORT_SRV_DW:
      ptr = pd->ptr;
      if(ptr) {
        DBGPRINT("(%s).ptr==0x%p,fta_log_export=0x%p,adjust=0x%x\n",tn,ptr,mlogsrv[srv].save_ptr,logsrv_dummy_header_size[srv]);
        ptr -= logsrv_dummy_header_size[srv]; /* adjust dummy header */
        pd->size += logsrv_dummy_header_size[srv];
        if(ptr == mlogsrv[srv].save_ptr) {
          ftadrv_deflate(pd,ptr,pd->size);
          if(pd->ptr != ptr) vfree(ptr);
          mlogsrv[srv].save_ptr = pd->ptr;
          mlogsrv[srv].size     = pd->size;
          mlogsrv[srv].mode     = FTA_LOG_EXPORT_MODE_DEFLATE;
          DBGPRINT("(%s)->deflate. pd->ptr=0x%p,pd->size=%d\n",tn, pd->ptr, pd->size);
        } else {
          DBGPRINT("(%s).ptr!=fta_log_export\n",tn);
          vfree(ptr);
        }
      }
      else DBGPRINT("(%s).ptr==0\n",tn);
      break;
    case FTA_LOG_EXPORT_SRV_DR:
      ptr = pd->ptr;
      if(ptr) {
        DBGPRINT("(%s).ptr==0x%p,fta_log_export=0x%p\n",tn,ptr,mlogsrv[srv].save_ptr);
        vfree(ptr);
        if(ptr == mlogsrv[srv].save_ptr) {
          mlogsrv[srv].save_ptr = 0;
          mlogsrv[srv].size     = 0;
          mlogsrv[srv].mode     = FTA_LOG_EXPORT_MODE_EMPTY;
          DBGPRINT("(%s)->empty.\n",tn);
        }
        else DBGPRINT("(%s).ptr!=fta_log_export\n",tn);
      }
      else DBGPRINT("(%s).ptr==0\n",tn);
      break;
    }
}
/* 2012-09-12 add end */

/*!
 @brief ftadrv_open

 driver open function

 @param [in]  inode   pointer of the inode struct
 @param [in]  file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ftadrv_open(struct inode *inode, struct file *file)
{
    int ret = 0;

    if (file->f_mode & FMODE_READ) {
        if(readDataMng.init) {
            return -EBUSY;
        }
        INIT_LIST_HEAD(&readDataMng.head);
        init_waitqueue_head(&readDataMng.wq);
        file->private_data = &readDataMng;
        readDataMng.init = 1;
        return 0;
    }
    /* non blocking */
    ret = nonseekable_open(inode, file);
    return ret;
}

/*!
 @brief ftadrv_close

 driver close function

 @param [in]  inode   pointer of the inode struct
 @param [in]  file    pointer of the file struct

 @retval      0: success
 @retval    <>0: failed
*/
static int ftadrv_close(struct inode *inode, struct file *file)
{
    int ret = 0;
    if (file->f_mode & FMODE_READ) {
        struct readDataMngType *mng = file->private_data;
        struct readDataType *p,*tmp;
        unsigned long irqflags;
        spin_lock_irqsave(&read_lock, irqflags);
        if(!mng->init) {
            ret = -EBADF;
            goto end;
        }
        list_for_each_entry_safe(p,tmp,&(mng->head),list) {
            list_del(&p->list);
            kfree(p);
        }
        mng->init = 0;
end:
        spin_unlock_irqrestore(&read_lock, irqflags);
    }
    return ret;
}

/*!
 @brief exec_write

 set the write event to workqueue 

 @param [in/out]  wd   pointer of the write data struct
 @param [in]      len  length of the write data

 @retval      0: success
            <>0: failed
*/
static ssize_t exec_write(struct writeDataType *wd,unsigned long len)
{
    unsigned long irqflags;

    wd->len = len;
    spin_lock_irqsave(&work_lock, irqflags);
    INIT_LIST_HEAD(&wd->list);
    list_add_tail(&wd->list,&write_head);
    queue_work(work_queue, &write_work);
    spin_unlock_irqrestore(&work_lock, irqflags);
    return len;
}

/*!
 @brief ftadrv_aio_write

 driver write function

 @param [in]  kiocb    pointer of the asynchronous IO struct
 @param [in]  iov      pointer of the write data
 @param [in]  nr_segs  number of the iov
 @param [in]  loff_t   offset

 @retval     >0: success
             <0: failed
*/
static ssize_t ftadrv_aio_write(struct kiocb *iocb, const struct iovec *iov,
                                unsigned long nr_segs, loff_t ppos)
{
    ssize_t total = 0;
    struct writeDataType *wd;

    wd = kmalloc(sizeof(struct writeDataType),GFP_KERNEL);
    if (!wd) {
       return -ENOSPC;
    }
    while(nr_segs-- > 0) {
        size_t len;
        len = min_t(size_t,iov->iov_len,MAX_BUF_SIZE-total);
        if (copy_from_user(&wd->buf[total],iov->iov_base,len)) {
            kfree(wd);
            return -EFAULT;
        }
        iov++;
        total += len;
        if(total >= MAX_BUF_SIZE) break;
    }
    return exec_write(wd,total);
}

#if 0
/*!
 @brief ftadrv_ioctl

 driver ioctl function

 @param [in]  inode    pointer of the inode struct
 @param [in]  file     pointer of the file struct
 @param [in]  iocmd    ioctl command no
 @param [in]  ioarg    ioctl command arguments

 @retval      0: success
*/
static int ftadrv_ioctl(struct inode *inode, struct file *filp, unsigned int iocmd, unsigned long ioarg)
{
    return 0;
}
#endif

/*!
 @brief ftadrv_read

 driver read function

 @param [in]  file     pointer of the file struct
 @param [in]  buf      pointer of the read data
 @param [in]  count    length of the read data
 @param [out] pos      pointer of the offset of the read data

 @retval      >=0: success
               <0: failed
*/
static ssize_t ftadrv_read(struct file *file, char __user *buf,
                           size_t count, loff_t *pos)
{
    struct readDataMngType *mng = file->private_data;
    struct readDataType *p,*tmp;
    ssize_t ret;
    unsigned long irqflags;

    DEFINE_WAIT(wait);

start:
    while (1) {
        prepare_to_wait(&mng->wq, &wait, TASK_INTERRUPTIBLE);

        spin_lock_irqsave(&read_lock, irqflags);
        ret = list_empty(&mng->head);
        spin_unlock_irqrestore(&read_lock, irqflags);
        if (!ret)
            break;

        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            break;
        }

        if (signal_pending(current)) {
            ret = -EINTR;
            break;
        }
        schedule();
    }

    finish_wait(&mng->wq, &wait);
    if (ret) 
        return ret; /* error return */

    spin_lock_irqsave(&read_lock, irqflags);
    list_for_each_entry_safe(p,tmp,&(mng->head),list) {
        int len = min(count, sizeof(p->buf));
        if (copy_to_user(buf, p->buf, len))
            ret = -EFAULT;
        else
            ret = len;
        list_del(&p->list);
        kfree(p);
        break;
    }
    spin_unlock_irqrestore(&read_lock, irqflags);

    if(!ret)
        goto start;
    return ret;
}

/*!
 @brief ftadrv_poll

 driver poll function

 @param [in]  file     pointer of the file struct
 @param [in]  wait     pointer of the poll struct

 @retval       >0: success
               <0: failed
*/
static unsigned int ftadrv_poll(struct file *file, poll_table *wait)
{
    struct readDataMngType *mng = file->private_data;
    unsigned int ret = POLLOUT | POLLWRNORM;
    unsigned long irqflags;

    if (!(file->f_mode & FMODE_READ))
        return ret;

    poll_wait(file, &mng->wq, wait);

    spin_lock_irqsave(&read_lock, irqflags);
    if (!list_empty(&mng->head))
        ret |= POLLIN | POLLRDNORM;
    spin_unlock_irqrestore(&read_lock, irqflags);

    return ret;
}

/*!
 @brief chrnul

 search for the character or NULL in string

 @param  [in]  str     pointer of string
 @param  [in]  ch      charcter of searching for

 @retval       >0:     length to the found character or NULL
*/
static unsigned long chrnul(const char *str,const char ch,unsigned long *info)
{
  const unsigned char *p = str;
  for(;*p && *p != ch;p++);
  if (*p != ch) {
    *info |= 0x00000080;
  }
  return (unsigned long)p - (unsigned long)str;
}

/* ==========================================================================
 *  DRIVER INITIALIZE/TERMINATE FUNCTIONS
 * ========================================================================== */
static const struct file_operations ftadrv_fops = {
    .owner   = THIS_MODULE,
    .open    = ftadrv_open,
    .release = ftadrv_close,
    .aio_write = ftadrv_aio_write,
    .read    = ftadrv_read,
    .poll    = ftadrv_poll,
//    .ioctl   = ftadrv_ioctl,
};

static struct miscdevice ftadrv_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "log_fta",
    .fops  = &ftadrv_fops,
};

/*!
 @brief ftadrv_shutdown

 resume function

 @param none

 @retval 0:   success

 @note called when bbctl is resumed.
*/
static int shutdown;
void ftadrv_shutdown(void)
{
    printk(KERN_INFO "fta stop\n");
    fta_terminate();
    shutdown = 1;
    return;
} /* ftadrv_shutdown */
EXPORT_SYMBOL(ftadrv_shutdown);

/*!
 @brief ftadrv_init

 driver initialize

 @param        none

 @retval        0: success
 @retval       <0: failed
*/
static int __init ftadrv_init(void)
{
   int ret;

   struct proc_dir_entry *entry;

   memset(mlogsrv, 0, sizeof(mlogsrv)); /* 2012-09-12 add */

   work_queue = create_singlethread_workqueue("ftadrv_queue");
   INIT_WORK(&write_work, ftadrv_write_work);
   INIT_WORK(&proc_work, ftadrv_proc_work);
   INIT_LIST_HEAD(&write_head);
   INIT_LIST_HEAD(&proc_head);

   entry = proc_mkdir( "fta", NULL );
   if ( entry == NULL) {
      printk( KERN_ERR "create_proc_entry failed\n" );
      return -EBUSY;
   }
   ftadrv_acpu_init(entry);
   ftadrv_ccpu_init(entry);

   ret = misc_register(&ftadrv_dev);
   if (ret) {
      printk(KERN_ERR "ftadrv: %d = misc_register() error\n",ret);
      return ret;
   }

   printk( KERN_INFO "start ftadrv %d\n",ret);
   return 0;

}

/*!
 @brief ftadrv_exit

 driver terminate

 @param        none

 @retval        0: success
 @retval       <0: failed
*/
static void __exit ftadrv_exit(void)
{
    misc_deregister(&ftadrv_dev);
    fta_terminate();
}

module_init(ftadrv_init);
module_exit(ftadrv_exit);

/*!
 @brief ftadrv_bootmem_init

 allocate fta log area

 @param        none

 @retval       none
*/
void __init ftadrv_bootmem_init(void)
{
    if(reserve_bootmem(FTA_ALOG_EXPORT_ADDR,FTA_CONFIG_STORE_MEMORY_SIZE,BOOTMEM_EXCLUSIVE) != 0) {
        printk(KERN_INFO "bootmem failed");
        return;
    }
}

void __init ftadrv_export(void)
{
    fta_log_area = phys_to_virt(FTA_ALOG_EXPORT_ADDR);

    if(_fta_manage_check_exportable((fta_manager_store_header *)fta_log_area, 
                                     FTA_CALC_MANAGER_SIZE(FTA_CONFIG_STORE_INITIAL_COUNT))) {
       fta_log_export = vmalloc(FTA_CONFIG_STORE_MEMORY_SIZE);
       if(fta_log_export == 0) {
         printk(KERN_ERR "FTA:cannot export - no buffer\n");
       } else {
         printk(KERN_INFO "export(a): %p\n",fta_log_export);
         memcpy(fta_log_export,fta_log_area,FTA_CONFIG_STORE_MEMORY_SIZE);
       }
    }

    memset((char *)fta_log_area,0,FTA_CONFIG_STORE_MEMORY_SIZE);
    fta_initialize();
}

/* ==========================================================================
 *  EXTERNAL FUNCTIONS
 * ========================================================================== */
/*!
 @brief ftadrv_exec_cmd

 execute the command for inclusion message

 @param  [in]  cmd     pointer of the commad string
 @param  [in]  cmdlen  length of the command string

 @retval       none
*/
void ftadrv_exec_cmd(const unsigned char *cmd,int cmdlen)
{

    struct readDataType *rd;
    unsigned long irqflags;

    if(!readDataMng.init) {
        return;
    }
    rd = kmalloc(sizeof(struct readDataType),GFP_ATOMIC);
    if (!rd) {
        return;
    }
    rd->buf[0] = strlen(cmd);
    memcpy(&rd->buf[1],cmd,min_t(int,cmdlen, sizeof(rd->buf)-1));
    INIT_LIST_HEAD(&rd->list);
    spin_lock_irqsave(&read_lock, irqflags);
    list_add_tail(&rd->list,&readDataMng.head);
    spin_unlock_irqrestore(&read_lock, irqflags);

    wake_up_interruptible(&readDataMng.wq);

}
EXPORT_SYMBOL(ftadrv_exec_cmd);

/*!
 @brief ftadrv_send_str

 execute the command for inclusion message

 @param  [in]  cmd     pointer of the commad string
 @param  [in]  cmdlen  length of the command string

 @retval       none
*/
void ftadrv_send_str(const unsigned char *cmd)
{
    struct readDataType *rd;
    int cmdlen = strlen(cmd);
    unsigned long irqflags;

    if(!readDataMng.init) {
        return;
    }
    rd = kmalloc(sizeof(struct readDataType),GFP_ATOMIC);
    if (!rd) {
        return;
    }
    rd->buf[0] = 0xb0;
    cmdlen = min_t(int,cmdlen, sizeof(rd->buf)-2);
    memcpy(&rd->buf[1],cmd,cmdlen);
    rd->buf[cmdlen+1] = 0;
    INIT_LIST_HEAD(&rd->list);
    spin_lock_irqsave(&read_lock, irqflags);
    list_add_tail(&rd->list,&readDataMng.head);
    spin_unlock_irqrestore(&read_lock, irqflags);

    wake_up_interruptible(&readDataMng.wq);

}
EXPORT_SYMBOL(ftadrv_send_str);

/*!
 @brief ftadrv_alloc

 allocate in the android log area

 @param  [in]  area    area for the android log
 @param  [in]  size    length of the area

 @retval       <>0:    pointer of the allocated area
 @retval         0:    failed
*/
unsigned char *ftadrv_alloc(int area,int size)
{
  int areasize[] = {  FTA_LOG_MAIN_SIZE,  /* log/main */
                      FTA_LOG_EVENT_SIZE, /* log/event */
                      FTA_LOG_RADIO_SIZE, /* log/radio */
                      FTA_LOG_SYSTEM_SIZE, /* log/system */
                      FTA_LOG_USER0_SIZE, /* log/user0 */
                      FTA_LOG_USER1_SIZE, /* log/user1 */
                   };

  if(area >= sizeof(areasize)/sizeof(int))
    return 0;
  size += areasize[area];
  return fta_var_alloc_log(area,size);
}
EXPORT_SYMBOL(ftadrv_alloc);

/*!
 @brief ftadrv_printk

 output printk message

 @param  [in]  srcname   pointer of the filename
 @param  [in]  line      line no
 @param  [in]  level     log level
 @param  [in]  time      pointer of the output time string
 @param  [in]  msg       pointer of the output message

 @retval       none

 @note         call by printk()
*/
int ftadrv_printk(const unsigned char *srcname, unsigned long line, unsigned long level,  const unsigned char *time,const unsigned char * msg)
{
  unsigned long info = (line << 16) | (0xfe00 | level);
  unsigned long msglen;

  ftadrv_cnt++;
  if(fta_log_area == 0)
      return 0;
  
  msglen = chrnul(msg,'\n',&info);
  fta_log_message_kmsg(srcname,info,time,msg,msglen+1);
  return (info & 0x80) ? msglen : msglen+1;
}
EXPORT_SYMBOL(ftadrv_printk);

void ftadrv_syslog(const char *start,const char *end)
{
  char *msg = (char *)start;
  while((int)msg < (int)end) {
    unsigned long info = 0;
    unsigned long msglen = chrnul(msg,'\n',&info);
    fta_log_message_kmsg(0,info,0,msg,msglen+1);
    msg += (msglen+1);
  }

}
EXPORT_SYMBOL(ftadrv_syslog);

/*!
 @brief ftadrv_get_time

 get output time

 @param  [in]  ptr   pointer of the time structure

 @retval       none
*/
void ftadrv_get_time(struct timespec *ptr)
{
   extern int timekeeping_suspended;
   static struct timespec tm;
   if(timekeeping_suspended) {
     goto end;
   }
   if(ftadrv_cnt < 200) {
     unsigned long long t;
     t = cpu_clock(smp_processor_id());
     ptr->tv_nsec = do_div(t, 1000000000);
     ptr->tv_sec = t;
     return;
   }
   getnstimeofday(&tm);
end:
   *ptr = tm;
   return;
}
EXPORT_SYMBOL(ftadrv_get_time);


/*!
 @brief ftadrv_flush

 flush fta area

 @param        none

 @retval       none
*/
void ftadrv_flush(void)
{
    clean_caches((unsigned int)fta_log_area,FTA_CONFIG_STORE_MEMORY_SIZE,virt_to_phys(fta_log_area));
}
EXPORT_SYMBOL(ftadrv_flush);

void ftadrv_free_proc(struct procDataType *pd)
{
    unsigned long irqflags;

/* 2012-09-12 add start */
    switch(pd->type) {
    case FTA_WC:
    case FTA_RC:
    case FTA_WN:
    case FTA_RN:
    case FTA_CC:
    case FTA_SC:
    case FTA_CN:
    case FTA_SN:
      ftadrv_free_work_logsrv(pd);
      break;
    }
/* 2012-09-12 add end */

    spin_lock_irqsave(&work_lock, irqflags);
    list_del(&pd->list);
    spin_unlock_irqrestore(&work_lock, irqflags);
    if(pd->ptr == 0)
       goto end;
    switch(pd->type) {
    case FTA_RA:
      vfree(pd->ptr);
      break;
    case FTA_CA:
      if(pd->ptr != fta_log_area) {
        vfree(pd->ptr);
      }
      break;
    case FTA_CD:
      vfree(pd->ptr);
      break;
    default:
      break;
    }
    pd->ptr = 0;
end:
    kfree(pd);
}
EXPORT_SYMBOL(ftadrv_free_proc);

struct procDataType *ftadrv_prepare_proc(int type)
{
    unsigned long irqflags;
    struct procDataType *pd;
    unsigned short tid = current->pid;

    spin_lock_irqsave(&work_lock, irqflags);
    list_for_each_entry(pd,&proc_head,list) {
       printk(KERN_INFO "0x%p: %d %d\n",pd,pd->tid,pd->type);
       if (pd->tid == tid && pd->type == type) {
          printk(KERN_ERR "proc is busy %d %d\n",tid,type);
          spin_unlock_irqrestore(&work_lock, irqflags);
          return 0;
       }
    }
    spin_unlock_irqrestore(&work_lock, irqflags);

    pd = kmalloc(sizeof(struct procDataType),GFP_KERNEL);
    if (!pd) {
       printk(KERN_ERR "no memory\n");
       return 0;
    }
    
    init_completion(&pd->comp);
    pd->ptr = 0;
    pd->size = 0;
    pd->ret = 0;
    pd->type = type;
    pd->tid = tid;
    pd->state = 0;

    spin_lock_irqsave(&work_lock, irqflags);
    INIT_LIST_HEAD(&pd->list);
    list_add_tail(&pd->list,&proc_head);
    queue_work(work_queue, &proc_work);
    spin_unlock_irqrestore(&work_lock, irqflags);

    wait_for_completion(&pd->comp);
    if (pd->ret < 0) {
       DBGPRINT("cleaning pd\n");
       ftadrv_free_proc(pd);
       return 0;
    }
    return pd;
}
EXPORT_SYMBOL(ftadrv_prepare_proc);

#include <asm/stacktrace.h> 

void ftadrv_stackdump(void)
{
    extern void print_cpu_info(void);
    extern int is_task_curr(struct task_struct *p);
    extern void dump_backtrace_tiny(unsigned long where, unsigned long frame);

    struct task_struct *g, *t;
    int i = 0;
    printk(KERN_INFO "--stack dump start ------------------\n");
    rcu_read_lock();
    do_each_thread(g, t) {
       struct stackframe frame;
       int n;
       printk(KERN_INFO "%d pid:tid %d:%d name:%s cpu: %d state: %ld in:%lld out:%lld\n",i++,
           t->tgid,t->pid,t->comm,task_cpu(t),t->state,t->fta_in_timestamp,t->fta_out_timestamp);
       if(is_task_curr(t))
         continue;
       frame.fp = thread_saved_fp(t);
       frame.sp = thread_saved_sp(t);
       frame.lr = 0;
       frame.pc = thread_saved_pc(t);
       for(n=0;n<16;n++) {
         int urc;
         unsigned long where = frame.pc;

         urc = unwind_frame(&frame);
         if (urc < 0)
            break;
         dump_backtrace_tiny(where, frame.sp - 4);
       }
       if(n == 16) {
          printk("...more\n");
       }
    } while_each_thread(g, t);
    rcu_read_unlock();
    printk(KERN_INFO "--stack dump end --------------------\n");
    print_cpu_info();
    clean_caches((unsigned int)fta_log_area,FTA_CONFIG_STORE_MEMORY_SIZE,virt_to_phys(fta_log_area));
}
EXPORT_SYMBOL(ftadrv_stackdump);

/*!
 @brief ftadrv_restart

 fta restart 

 @param        none

 @retval       none

 @note  call by machine_restart()
*/
#include "smd_private.h"
void ftadrv_restart(void)
{
    if(shutdown)
      return;
    preempt_disable();
    ftadrv_stackdump();
    fta_stop(FTA_STOP_REASON_ABNORMAL);
    preempt_enable();
}
EXPORT_SYMBOL(ftadrv_restart);

/*!
 @brief ftadrv_cpu

 get cpu id

 @param        none

 @retval       cpu id
*/
#include <linux/interrupt.h> /* in_interrupt() */
int ftadrv_cpu(void)
{
  return (smp_processor_id()+1) | (in_interrupt() ? 0x100 : 0);
}
EXPORT_SYMBOL(ftadrv_cpu);

MODULE_DESCRIPTION("FTA Driver");
MODULE_LICENSE("GPL v2");
