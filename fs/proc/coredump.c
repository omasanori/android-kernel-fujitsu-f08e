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

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/nonvolatile_common.h>
#include <linux/uaccess.h>
#include <asm/resource.h>

#include <linux/coredump_ext.h>

//#define dprintt printk
#define dprintt(...)

struct coredump_struct coredump_proc_param;

static ssize_t coredump_limits_write(struct file *file,
                      const char __user *buf,
                      size_t count,
                      loff_t *ppos)
{
    char buffer[PROC_NUMBUF], *end;
    unsigned long val;
    int ret;

    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);

    memset(buffer, 0, sizeof(buffer));
    if (count > sizeof(buffer) - 1) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EINVAL;
        goto coredump_limits_write_out;
    }
    if (copy_from_user(buffer, buf, count)) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EFAULT;
        goto coredump_limits_write_out;
    }
    if ((buffer[count - 1] == '\r')||(buffer[count - 1] == '\n'))  {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        buffer[count - 1] = 0;
    }
    if (strcmp(buffer, "unlimited") == 0) {
        val = RLIM_INFINITY;
    } else {
        val = (unsigned long)simple_strtoul(buffer, &end, 0);
        if (end - buffer == 0) {
            dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            goto coredump_limits_write_out;
        }
    }

    dprintt(KERN_EMERG "(%s %d %lu)\n", __FUNCTION__, __LINE__, val);

    ret = get_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if (ret < 0) {
        printk(KERN_ERR "error %s get_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_limits_write_out;
    }

    coredump_proc_param.limits = val;

    ret = set_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if (ret < 0) {
        printk(KERN_ERR "error %s set_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_limits_write_out;
    }
    ret = count;

coredump_limits_write_out:
    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, ret);
    return ret;
}

static int coredump_limits_show(struct seq_file *m, void *v)
{
    int ret;

    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);

    ret = get_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if ( ret < 0 ) {
        printk(KERN_ERR "error %s get_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_limits_show_out;
    }

    dprintt(KERN_EMERG "(%s %d %lu)\n", __FUNCTION__, __LINE__, coredump_proc_param.limits);
    if (coredump_proc_param.limits == RLIM_INFINITY) {
        seq_printf(m, "%s\n", "unlimited");
    } else {
        seq_printf(m, "%lu\n", coredump_proc_param.limits);
    }

    ret = 0;

coredump_limits_show_out:
    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, ret);
    return ret;
}

static int coredump_limits_open(struct inode *inode, struct file *file)
{
    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
    return single_open(file, coredump_limits_show, NULL);
}

static const struct file_operations coredump_limits_proc_fops = {
    .open       = coredump_limits_open,
    .read       = seq_read,
    .write      = coredump_limits_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static int __init proc_coredump_limits_init(void)
{
    proc_create("coredump_limits", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, NULL, &coredump_limits_proc_fops);
    return 0;
}
module_init(proc_coredump_limits_init);

static ssize_t coredump_num_write(struct file *file,
                      const char __user *buf,
                      size_t count,
                      loff_t *ppos)
{
    char buffer[PROC_NUMBUF], *end;
    unsigned long val;
    int ret;

    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);

    memset(buffer, 0, sizeof(buffer));
    if (count > sizeof(buffer) - 1) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EINVAL;
        goto coredump_num_write_out;
    }
    if (copy_from_user(buffer, buf, count)) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EFAULT;
        goto coredump_num_write_out;
    }
    val = (unsigned long)simple_strtoul(buffer, &end, 0);
    if (end - buffer == 0) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EINVAL;
        goto coredump_num_write_out;
    }

    ret = get_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if (ret < 0) {
        printk(KERN_ERR "error %s get_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_num_write_out;
    }

    coredump_proc_param.num = val;

    ret = set_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if (ret < 0) {
        printk(KERN_ERR "error %s set_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_num_write_out;
    }
    ret = count;

coredump_num_write_out:
    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, ret);
    return ret;
}

static int coredump_num_show(struct seq_file *m, void *v)
{
    int ret;

    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);

    ret = get_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if ( ret < 0 ) {
        printk(KERN_ERR "error %s get_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_num_show_out;
    }
    dprintt(KERN_EMERG "(%s %d %lu)\n", __FUNCTION__, __LINE__, coredump_proc_param.num);
    seq_printf(m, "%lu\n", coredump_proc_param.num);

    ret = 0;

coredump_num_show_out:
    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, ret);
    return ret;
}

static int coredump_num_open(struct inode *inode, struct file *file)
{
    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
    return single_open(file, coredump_num_show, NULL);
}

static const struct file_operations coredump_num_proc_fops = {
    .open       = coredump_num_open,
    .read       = seq_read,
    .write      = coredump_num_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static int __init proc_coredump_num_init(void)
{
    proc_create("coredump_num"   , S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, NULL, &coredump_num_proc_fops);
    return 0;
}
module_init(proc_coredump_num_init);

static ssize_t coredump_where_write(struct file *file,
                      const char __user *buf,
                      size_t count,
                      loff_t *ppos)
{
    int ret;
    char buffer[MAX_CORE_WHERE+1];

    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, count);

    memset(buffer, 0, sizeof(buffer));
    if (count > sizeof(buffer)) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EINVAL;
        goto coredump_where_write_out;
    }

    if (copy_from_user(buffer, buf, count)) {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        ret = -EFAULT;
        goto coredump_where_write_out;
    }

    if ((buffer[count - 1] == '\r')||(buffer[count - 1] == '\n'))  {
        dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
        buffer[count - 1] = 0;
    } else {
        if (count >= sizeof(buffer)) {
            dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            goto coredump_where_write_out;
        }
    }

    ret = get_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if (ret < 0) {
        printk(KERN_ERR "error %s get_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_where_write_out;
    }

    memset(coredump_proc_param.where, 0, sizeof(coredump_proc_param.where));
    strncpy(coredump_proc_param.where, buffer, sizeof(coredump_proc_param.where));

    ret = set_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if (ret < 0) {
        ret = -EFAULT;
        printk(KERN_ERR "error %s set_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        goto coredump_where_write_out;
    }

    ret = count;
    dprintt(KERN_EMERG "(%s %d %d %s)\n", __FUNCTION__, __LINE__, ret, coredump_proc_param.where);

coredump_where_write_out:
    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, ret);
    return ret;
}

static int coredump_where_show(struct seq_file *m, void *v)
{
    int ret;

    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);

    ret = get_nonvolatile((void *)&coredump_proc_param, (unsigned long)APNV_COREDUMP_I, sizeof(struct coredump_struct));
    if ( ret < 0 ) {
        printk(KERN_ERR "error %s get_nonvolatile() returned (%d)\n", __FUNCTION__, ret);
        ret = -EFAULT;
        goto coredump_where_show_out;
    }
    dprintt(KERN_EMERG "(%s %d %d) \n", __FUNCTION__, __LINE__, ret);
    dprintt(KERN_EMERG "(%s %d %s) \n", __FUNCTION__, __LINE__, coredump_proc_param.where);
    seq_printf(m, "%s\n", coredump_proc_param.where);

    ret = 0;

coredump_where_show_out:
    dprintt(KERN_EMERG "(%s %d %d)\n", __FUNCTION__, __LINE__, ret);
    return ret;
}

static int coredump_where_open(struct inode *inode, struct file *file)
{
    dprintt(KERN_EMERG "(%s %d)\n", __FUNCTION__, __LINE__);
    return single_open(file, coredump_where_show, NULL);
}

static const struct file_operations coredump_where_proc_fops = {
    .open       = coredump_where_open,
    .read       = seq_read,
    .write      = coredump_where_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static int __init proc_coredump_where_init(void)
{
    proc_create("coredump_where" , S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, NULL, &coredump_where_proc_fops);
    return 0;
}
module_init(proc_coredump_where_init);
