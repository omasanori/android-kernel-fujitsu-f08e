/*
 * FJSEC LSM module
 *
 * based on deckard.c
 * based on root_plug.c
 * Copyright (C) 2002 Greg Kroah-Hartman <greg@kroah.com>
 *
 * _xx_is_valid(), _xx_encode(), _xx_realpath_from_path()
 * is ported from security/tomoyo/realpath.c in linux-2.6.32
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2010-2013
/*----------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/security.h>
#include <linux/moduleparam.h>
#include <linux/mount.h>
#include <linux/mnt_namespace.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/magic.h>
#include <asm/mman.h>
#include <linux/msdos_fs.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/limits.h>
#include <linux/fs.h>
#include <asm/memory.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <linux/securebits.h>
#include <linux/capability.h>
#include <linux/binfmts.h>
#include <linux/personality.h>
#include <linux/audit.h>
#include <linux/memblock.h>

#ifdef CONFIG_DETECT_RH
#include "detect_rh.h"
#endif//CONFIG_DETECT_RH

#include "fjsec_config.h"
#include "fjsec_capability.h"
#include "../arch/arm/mm/pageattr.h"

//#define PRCONFIG
#define DUMP_PRINFO
//#define DUMP_PTRACE_REQUEST

//#define dprintk printk
#define dprintk(...)

static enum boot_mode_type boot_mode = BOOT_MODE_NONE;
static bool mount_flag = true;
#ifdef DUMP_PTRACE_REQUEST
static long prev_request = -1;
#endif

//#define ROOTFS_RW_REMOUNT
enum {
	PG_LEVEL_NONE,
	PG_LEVEL_4K,
	PG_LEVEL_2M,
	PG_LEVEL_NUM
};

static inline bool _xx_is_valid(const unsigned char c)
{
	return c > ' ' && c < 127;
}

static int _xx_encode(char *buffer, int buflen, const char *str)
{
	while (1) {
		const unsigned char c = *(unsigned char *) str++;

		if (_xx_is_valid(c)) {
			if (--buflen <= 0)
				break;
			*buffer++ = (char) c;
			if (c != '\\')
				continue;
			if (--buflen <= 0)
				break;
			*buffer++ = (char) c;
			continue;
		}
		if (!c) {
			if (--buflen <= 0)
				break;
			*buffer = '\0';
			return 0;
		}
		buflen -= 4;
		if (buflen <= 0)
			break;
		*buffer++ = '\\';
		*buffer++ = (c >> 6) + '0';
		*buffer++ = ((c >> 3) & 7) + '0';
		*buffer++ = (c & 7) + '0';
	}
	return -ENOMEM;
}

static int _xx_realpath_from_path(struct path *path, char *newname,
				  int newname_len)
{
	struct dentry *dentry = path->dentry;
	struct super_block *sb = path->dentry->d_sb;
	int error = -ENOMEM;
	char *sp;

	if (!dentry || !path->mnt || !newname || newname_len <= 2048)
		return -EINVAL;
	if (dentry->d_op && dentry->d_op->d_dname) {
		/* For "socket:[\$]" and "pipe:[\$]". */
		static const int offset = 1536;
		sp = dentry->d_op->d_dname(dentry, newname + offset,
					   newname_len - offset);
	}
	else {
		/* go to whatever namespace root we are under */
		sp = d_absolute_path(path, newname, newname_len);

		/* Prepend "/proc" prefix if using internal proc vfs mount. */
		if (!IS_ERR(sp) && (path->mnt->mnt_flags & MNT_INTERNAL) &&
			(sb->s_magic == PROC_SUPER_MAGIC)) {
			sp -= 5;
			if (sp >= newname)
				memcpy(sp, "/proc", 5);
			else
				sp = ERR_PTR(-ENOMEM);
		}
	}
	if (IS_ERR(sp)) {
		error = PTR_ERR(sp);
	}
	else {
		error = _xx_encode(newname, sp - newname, sp);
	}
	/* Append trailing '/' if dentry is a directory. */
	if (!error && dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode)
	    && *newname) {
		sp = newname + strlen(newname);
		if (*(sp - 1) != '/') {
			if (sp < newname + newname_len - 4) {
				*sp++ = '/';
				*sp = '\0';
			} else {
				error = -ENOMEM;
			}
		}
	}
	return error;
}

static int __init setup_mode(char *str)
{
	if (strcmp(str, BOOT_ARGS_MODE_FOTA) == 0) {
		boot_mode = BOOT_MODE_FOTA;
	} else if (strcmp(str, BOOT_ARGS_MODE_SDDOWNLOADER) == 0) {
		boot_mode = BOOT_MODE_SDDOWNLOADER;
//	} else if (strcmp(str, BOOT_ARGS_MODE_RECOVERY) == 0) {
//		boot_mode = BOOT_MODE_RECOVERY;
	} else if (strcmp(str, BOOT_ARGS_MODE_MASTERCLEAR) == 0) {
		boot_mode = BOOT_MODE_MASTERCLEAR;
	} else if (strcmp(str, BOOT_ARGS_MODE_MAKERCMD) == 0) {
		boot_mode = BOOT_MODE_MAKERCMD;
	} else if (strcmp(str, BOOT_ARGS_MODE_OSUPDATE) == 0) {
		boot_mode = BOOT_MODE_OSUPDATE;
	} else if (strcmp(str, BOOT_ARGS_MODE_RECOVERYMENU) == 0) {
		boot_mode = BOOT_MODE_SDDOWNLOADER;
	} else if (strcmp(str, BOOT_ARGS_MODE_KERNEL) == 0) {
		boot_mode = BOOT_MODE_MAKERCMD;
	}

	dprintk(KERN_INFO "boot mode=<%d>\n", boot_mode);
	return 0;
}
early_param("mode", setup_mode);

static char *get_process_path(struct task_struct *task, char *buf, size_t size)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	char *cp = NULL;

	mm = task->mm;
	if (!mm) {
		dprintk(KERN_INFO "%s:%d mm is null.\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) && vma->vm_file) {
			cp = d_path(&vma->vm_file->f_path, buf, size);
			break;
		}
	}
	up_read(&mm->mmap_sem);

	return cp;
}

static char *get_process_path_mapping(struct task_struct *task, char *buf, size_t size)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	char *cp = NULL;

	mm = task->mm;
	if (!mm) {
		dprintk(KERN_INFO "%s:%d mm is null.\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) && vma->vm_file) {
			cp = d_path(&vma->vm_file->f_path, buf, size);
			break;
		}
	}

	return cp;
}

#ifdef DUMP_PRINFO
static void dump_prinfo(const char *function_name, int line_number)
{
	char *binname;
	char *buf = kzalloc(PATH_MAX, GFP_NOFS);

	if (!buf) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return;
	}
	binname = get_process_path(current, buf, PATH_MAX-1);
	printk(KERN_INFO "%s:%d: current process name=<%s>, path=<%s>, uid=<%d>\n"
			, function_name, line_number, current->comm, binname, current->cred->uid);
	kfree(buf);
}

static void dump_prinfo_mapping(const char *function_name, int line_number)
{
	char *binname;
	char *buf = kzalloc(PATH_MAX, GFP_NOFS);

	if (!buf) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return;
	}
	binname = get_process_path_mapping(current, buf, PATH_MAX-1);
	printk(KERN_INFO "%s:%d: current process name=<%s>, path=<%s>, uid=<%d>\n"
			, function_name, line_number, current->comm, binname, current->cred->uid);
	kfree(buf);
}
#else
#define dump_prinfo(a, b)
#define dump_prinfo_mapping(a, b)
#endif


static int fjsec_check_access_process_path(char *process_path)
{
	char *buf = kzalloc(PATH_MAX, GFP_NOFS);
	char *binname;

	if (!buf) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	binname = get_process_path(current, buf, PATH_MAX-1);
	if (binname == NULL || IS_ERR(binname)) {
		printk(KERN_INFO "%s:%d: Failed getting process path. process name=%s, uid=%d, pid=%d\n"
			   , __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(buf);
		return -EPERM;
	}

	dprintk(KERN_INFO "%s:%d: process path=%s\n", __FUNCTION__, __LINE__, binname);

	if (strcmp(binname, process_path) == 0) {
		kfree(buf);
		return 0;
	}

	dprintk(KERN_INFO "%s:%d: mismatched process path. config=<%s>, current=<%s>\n"
			, __FUNCTION__, __LINE__, process_path, binname);

	kfree(buf);
	return -EPERM;
}

static int fjsec_check_access_process_path_mapping(char *process_path)
{
	char *buf = kzalloc(PATH_MAX, GFP_NOFS);
	char *binname;


	if (!buf) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	binname = get_process_path_mapping(current, buf, PATH_MAX-1);
	if (binname == NULL || IS_ERR(binname)) {
		printk(KERN_INFO "%s:%d: Failed getting process path. process name=%s, uid=%d, pid=%d\n"
			   , __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(buf);
		return -EPERM;
	}

	dprintk(KERN_INFO "%s:%d: process path=%s\n", __FUNCTION__, __LINE__, binname);

	if (strcmp(binname, process_path) == 0) {
		kfree(buf);
		return 0;
	}

	dprintk(KERN_INFO "%s:%d: mismatched process path. config=<%s>, current=<%s>\n"
			, __FUNCTION__, __LINE__, process_path, binname);

	kfree(buf);
	return -EPERM;
}

static int fjsec_check_access_process(char *process_name, char *process_path)
{
	dprintk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);
	if (strcmp(current->comm, process_name) == 0) {
		if (fjsec_check_access_process_path(process_path) == 0) {
			return 0;
		}
	} else {
		dprintk(KERN_INFO "%s:%d: mismatched process name. config=<%s>, current=<%s>\n"
				, __FUNCTION__, __LINE__, process_name, current->comm);
	}

	return -EPERM;
}

static int fjsec_check_access_process_mapping(char *process_name, char *process_path)
{
	dprintk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);
	if (strcmp(current->comm, process_name) == 0) {
		if (fjsec_check_access_process_path_mapping(process_path) == 0) {
			return 0;
		}
	} else {
		dprintk(KERN_INFO "%s:%d: mismatched process name. config=<%s>, current=<%s>\n"
				, __FUNCTION__, __LINE__, process_name, current->comm);
	}

	return -EPERM;
}

int fjsec_check_mmcdl_access_process(void)
{
	int index;

	for (index = 0; mmcdl_device_list[index].process_name; index++) {
		if (boot_mode == mmcdl_device_list[index].boot_mode) {
			if (fjsec_check_access_process(mmcdl_device_list[index].process_name,
											mmcdl_device_list[index].process_path) == 0) {
				return 0;
			}
		}
	}

	printk(KERN_INFO "%s:%d: REJECT Failed accessing mmcdl device. process name=%s, uid=%d, pid=%d\n",
		 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
	return -1;
}

int fjsec_check_mkdrv_access_process(void)
{
	int index;

	for (index = 0; mkdrv_device_list[index].process_name; index++) {
		if (boot_mode == mkdrv_device_list[index].boot_mode) {
			if (fjsec_check_access_process(mkdrv_device_list[index].process_name,
											mkdrv_device_list[index].process_path) == 0) {
				return 0;
			}
		}
	}

	printk(KERN_INFO "%s:%d: REJECT Failed accessing mkdrv device. process name=%s, uid=%d, pid=%d\n",
		 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
	return -1;
}

int fjsec_check_devmem_access(unsigned long pfn)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(devmem_acl); index++) {
		if ((pfn >= devmem_acl[index].head) && (pfn <= devmem_acl[index].tail)) {
			if (fjsec_check_access_process_mapping(devmem_acl[index].process_name, devmem_acl[index].process_path) == 0) {
				dprintk(KERN_INFO "%s:%d: GRANTED accessing devmem. pfn=<0x%lx> process name=<%s> process path=<%s>\n"
							, __FUNCTION__, __LINE__, pfn, devmem_acl[index].process_name, devmem_acl[index].process_path);
				return 0;
			}
		}
	}

	dump_prinfo_mapping(__FUNCTION__, __LINE__);
	printk(KERN_INFO "%s:%d: REJECT Failed accessing devmem. pfn=<0x%lx> process name=%s, uid=%d, pid=%d\n",
		 __FUNCTION__, __LINE__, pfn, current->comm, current->cred->uid, current->pid);
	return -1;
}

static int fjsec_check_disk_device_access_offset(struct accessible_area_disk_dev *accessible_areas, loff_t head, loff_t length)
{
	struct accessible_area_disk_dev *accessible_area;
	loff_t tail;

	tail = head + (length - 1);

	if (tail < head) {
		tail = head;
	}

	dprintk(KERN_INFO "%s:%d: head=<0x%llx>, length=<0x%llx>: tail=<0x%llx>\n"
		, __FUNCTION__, __LINE__, head, length, tail);

	for (accessible_area = accessible_areas; accessible_area->tail; accessible_area++) {
		if ((accessible_area->head <= head) && (accessible_area->tail >= head)) {
			if ((accessible_area->head <= tail) && (accessible_area->tail >= tail)) {
				dprintk(KERN_INFO "%s:%d: SUCCESS accessing disk device.\n", __FUNCTION__, __LINE__);
				return 0;
			}
		}
	}

	printk(KERN_INFO "%s:%d: REJECT Failed accessing disk device. process name=%s, uid=%d, pid=%d\n",
		 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
	return -EPERM;
}

static int fjsec_check_disk_device_access(char *realpath, loff_t offset, loff_t length)
{
	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_DISK_DEV_PATH) == 0) {
		dprintk(KERN_INFO "%s:%d:boot mode=<%d>\n", __FUNCTION__, __LINE__, boot_mode);

		if (boot_mode == BOOT_MODE_FOTA) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH) == 0) {
				if(fjsec_check_disk_device_access_offset(accessible_areas_fota, offset, length) == 0) {
					return 0;
				}
			}
		}
		else if (boot_mode == BOOT_MODE_SDDOWNLOADER) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH) == 0) {
				if(fjsec_check_disk_device_access_offset(accessible_areas_sddownloader, offset, length) == 0) {
					return 0;
				}
			}
		}
		else if (boot_mode == BOOT_MODE_OSUPDATE) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_PATH) == 0) {
				if(fjsec_check_disk_device_access_offset(accessible_areas_osupdate, offset, length) == 0) {
					return 0;
				}
			}
		}
//		else if (boot_mode == BOOT_MODE_RECOVERY) {
		else if (boot_mode == BOOT_MODE_MASTERCLEAR) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH) == 0) {
				if(fjsec_check_disk_device_access_offset(accessible_areas_recovery, offset, length) == 0) {
					return 0;
				}
			}
		}
		else if (boot_mode == BOOT_MODE_MAKERCMD) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH) == 0) {
				if(fjsec_check_disk_device_access_offset(accessible_areas_maker, offset, length) == 0) {
					return 0;
				}
			}
		}

		return -EPERM;
	}

	return 0;
}

static int fjsec_check_system_access_process(void)
{
	if (boot_mode == BOOT_MODE_FOTA) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}
#if 0
	if (boot_mode == BOOT_MODE_SDDOWNLOADER) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}

	if (boot_mode == BOOT_MODE_OSUPDATE) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}

//	if (boot_mode == BOOT_MODE_RECOVERY) {
	if (boot_mode == BOOT_MODE_MASTERCLEAR) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}
#endif
	if (!current->mm) {
		dprintk(KERN_INFO "%s:%d (!current->mm)\n", __FUNCTION__, __LINE__);
		return 0;
	}
#ifdef CONFIG_DETECT_RH
	set_rhflag();
#endif//CONFIG_DETECT_RH
	return -1;
}

static int fjsec_check_system_directory_access_process(char *realpath)
{
	if (strncmp(realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH,
					strlen(CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH)) == 0) {

		if (fjsec_check_system_access_process() == 0) {
			return 0;
		}

		return -EPERM;
	}

	return 0;
}

static int fjsec_check_system_device_access_process(char *realpath)
{
	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DEV_PATH) == 0) {

		if (fjsec_check_system_access_process() == 0) {
			return 0;
		}

		return -EPERM;
	}

	return 0;
}

#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
static int fjsec_check_secure_storage_access_process(void)
{
#if 0
	if (boot_mode == BOOT_MODE_FOTA) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}
	else 
#endif
	if (boot_mode == BOOT_MODE_SDDOWNLOADER) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}
	else if (boot_mode == BOOT_MODE_OSUPDATE) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}
//	else if (boot_mode == BOOT_MODE_RECOVERY) {
	else if (boot_mode == BOOT_MODE_MASTERCLEAR) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}
	else if (boot_mode == BOOT_MODE_MAKERCMD) {
		if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
		}
	}

	if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_SECURE_STORAGE_ACCESS_PROCESS_NAME,
										CONFIG_SECURITY_FJSEC_SECURE_STORAGE_ACCESS_PROCESS_PATH) == 0) {
			return 0;
	}

	if (fjsec_check_access_process(INIT_PROCESS_NAME, INIT_PROCESS_PATH) == 0) {
		return 0;
	}

	return -1;
}

static int fjsec_check_secure_storage_directory_access_process(char *realpath)
{
	if (strncmp(realpath, CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH,
					strlen(CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH)) == 0) {
		if (fjsec_check_secure_storage_access_process() == 0) {
			return 0;
		}

		return -EPERM;
	}

	return 0;
}

static int fjsec_check_secure_storage_device_access_process(char *realpath)
{
	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DEV_PATH) == 0) {
		if (fjsec_check_secure_storage_access_process () == 0) {
			return 0;
		}

		return -EPERM;
	}

	return 0;
}

#else

#define fjsec_check_secure_storage_directory_access_process(a) (0)
#define fjsec_check_secure_storage_device_access_process(a) (0)

#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */

#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING

static int fjsec_check_kitting_access_process(char *process_name, char *process_path)
{
	dprintk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);

	if (strcmp(current->group_leader->comm, process_name) == 0) {
		if (fjsec_check_access_process_path(process_path) == 0) {
			return 0;
		}
	} else {
		dprintk(KERN_INFO "%s:%d: mismatched process name. config=<%s>, current=<%s>\n"
				, __FUNCTION__, __LINE__, process_name, current->group_leader->comm);
	}

	return -EPERM;
}

static int fjsec_check_kitting_directory_access_process(char *realpath)
{
	if (strncmp(realpath, CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH,
					strlen(CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH)) == 0) {
		if (boot_mode == BOOT_MODE_NONE) {
			struct kitting_directory_access_process *kd_access_control_process;
			
			for (kd_access_control_process = kitting_directory_access_process_list; kd_access_control_process->process_path; kd_access_control_process++) {
				char *process_name;
				int process_name_len;

				if (kd_access_control_process->uid != UID_NO_CHECK && kd_access_control_process->uid != current->cred->uid) {
					dprintk(KERN_INFO "%s:%d: mismatched process UID\n", __FUNCTION__, __LINE__);
					continue;
				}

				process_name_len = strlen(kd_access_control_process->process_name);
				process_name = kd_access_control_process->process_name;

				if (process_name_len > (TASK_COMM_LEN - 1)) {
					process_name += (process_name_len - (TASK_COMM_LEN - 1));
				}

				if (fjsec_check_kitting_access_process(process_name,
												kd_access_control_process->process_path) != 0) {
					dprintk(KERN_INFO "%s:%d: mismatched process name, process path\n", __FUNCTION__, __LINE__);
					continue;
				}

				dprintk(KERN_INFO "%s:%d: SUCCESS realpath=%s\n", __FUNCTION__, __LINE__, realpath);
				return 0;
			}
		}

		if (fjsec_check_access_process(INIT_PROCESS_NAME, INIT_PROCESS_PATH) == 0) {
			return 0;
		}

		return -EPERM;
	}

	return 0;
}

static int fjsec_check_kitting_device_access_process(char *realpath)
{
	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_KITTING_DEV_PATH) == 0) {
		if (boot_mode == BOOT_MODE_SDDOWNLOADER) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH) == 0) {
				return 0;
			}
		} 
//		else if (boot_mode == BOOT_MODE_RECOVERY) {
		else if (boot_mode == BOOT_MODE_MASTERCLEAR) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME,
											CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH) == 0) {
				return 0;
			}
		}

		return -EPERM;
	}

	return 0;
}

#else

#define fjsec_check_kitting_directory_access_process(a) (0)
#define fjsec_check_kitting_device_access_process(a) (0)

#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */

static int fjsec_check_device_access_process(char *realpath, struct ac_config *acl)
{
	struct ac_config *config;
	int result = 0;

	for (config = acl; config->prefix; config++) {
		int length = strlen(config->prefix);

		if (config->prefix[length - 1] == '*') {
			if (strncmp(realpath, config->prefix, length - 1) != 0) {
				continue;
			}
		} else {
			if (strcmp(realpath, config->prefix) != 0) {
				continue;
			}
		}

		if (!config->boot_mode && !config->process_name && !config->process_path) {
			printk(KERN_INFO "%s:%d: <%s> is banned access.\n", __FUNCTION__, __LINE__, config->prefix);
			return -EPERM;
		}

		if (boot_mode == config->boot_mode) {
			if (fjsec_check_access_process(config->process_name, config->process_path) == 0) {
				return 0;
			}
		}

		result = -EPERM;
	}

	return result;
}

static int fjsec_read_write_access_control(char *realpath)
{
	struct read_write_access_control *rw_access_control;
	struct read_write_access_control_process *rw_access_control_process;
	int result = 1;

//	if (!(boot_mode == BOOT_MODE_NONE || boot_mode == BOOT_MODE_RECOVERY)) {
	if (!(boot_mode == BOOT_MODE_NONE || boot_mode == BOOT_MODE_MASTERCLEAR)) {
		return 1;
	}

	for (rw_access_control = rw_access_control_list; rw_access_control->prefix; rw_access_control++) {
		int length = strlen(rw_access_control->prefix);

		if (rw_access_control->prefix[length - 1] == '*') {
			if (strncmp(rw_access_control->prefix, realpath, length - 1) != 0) {
				continue;
			}
		} else {
			if (strcmp(rw_access_control->prefix, realpath) != 0) {
				continue;
			}
		}

		if (current->cred->uid == AID_ROOT || current->cred->uid == AID_SYSTEM) {
			return 0;
		}

		result = -EPERM;

		for (rw_access_control_process = rw_access_control->process_list; rw_access_control_process->process_path; rw_access_control_process++) {
			char *process_name;
			int process_name_len;

			dprintk (KERN_INFO "  config=<%s><%s><%s><%d>\n"
					, rw_access_control->prefix, rw_access_control_process->process_name, rw_access_control_process->process_path, rw_access_control_process->uid);
			if (rw_access_control_process->uid != UID_NO_CHECK && rw_access_control_process->uid != current->cred->uid) {
				dprintk(KERN_INFO "%s:%d: mismatched process UID\n", __FUNCTION__, __LINE__);
				continue;
			}

			if (rw_access_control_process->process_name != PRNAME_NO_CHECK) {
				process_name_len = strlen(rw_access_control_process->process_name);
				process_name = rw_access_control_process->process_name;

				if (process_name_len > (TASK_COMM_LEN - 1)) {
					process_name += (process_name_len - (TASK_COMM_LEN - 1));
				}

				if (fjsec_check_access_process(process_name,
												rw_access_control_process->process_path) != 0) {
					dprintk(KERN_INFO "%s:%d: mismatched process name, process path\n", __FUNCTION__, __LINE__);
					continue;
				}
			} else {
				if (fjsec_check_access_process_path(rw_access_control_process->process_path) != 0) {
					dprintk(KERN_INFO "%s:%d: mismatched process path\n", __FUNCTION__, __LINE__);
					continue;
				}
			}

			dprintk(KERN_INFO "%s:%d: SUCCESS realpath=%s\n", __FUNCTION__, __LINE__, realpath);
			return 0;
		}
	}
	return result;
}

static void fjsec_print_path(struct path *path)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer.\n", __FUNCTION__, __LINE__);
		return;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath.\n", __FUNCTION__, __LINE__);
		kfree(realpath);
		return;
	}
	printk(KERN_INFO "%s:%d: REJECT realpath=%s.\n", __FUNCTION__, __LINE__, realpath);
	kfree(realpath);
}

static int fjsec_check_path_from_path(struct path *path)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	r = fjsec_read_write_access_control(realpath);
	if (r == -EPERM) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s, process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}
	else if (r == 0) {
		kfree(realpath);
		return 0;
	}

	if (fjsec_check_system_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_device_access_process(realpath, fs_acl) != 0) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_secure_storage_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
		__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_kitting_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
		__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_check_chmod_from_path(struct path *path, mode_t mode)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;
	struct fs_path_config *pc;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	dprintk(KERN_INFO "(%s:%d) path=<%s> mode=<%d>\n", __FUNCTION__, __LINE__, realpath, mode);

	for (pc = devs; pc->prefix; pc++) {
		if (strcmp(realpath, pc->prefix) == 0) {
			if (mode == (pc->mode & S_IALLUGO)) {
				break;
			}

			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
	    		 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}
	}

	kfree(realpath);
	return 0;
}

static int fjsec_check_chmod_from_mode(struct path *path, mode_t mode)
{
	struct inode *inode = path->dentry->d_inode;

	if(boot_mode == BOOT_MODE_FOTA) {
		return 0;
	}

	if((inode->i_uid == 0) && !(inode->i_mode & S_ISUID)) {
		if (mode  & S_ISUID) {
			fjsec_print_path(path);
			printk(KERN_INFO "%s:%d: REJECT process name=%s file name=%s current mode=0%o \n",
				   __FUNCTION__, __LINE__, current->comm, path->dentry->d_iname,inode->i_mode);
			return -EPERM;
		}
	}

	if((inode->i_gid == 0) && !(inode->i_mode & S_ISGID)) {
		if (mode  & S_ISGID) {
			fjsec_print_path(path);
			printk(KERN_INFO "%s:%d: REJECT process name=%s file name=%s current mode=0%o \n",
				   __FUNCTION__, __LINE__, current->comm, path->dentry->d_iname,inode->i_mode);
			return -EPERM;
		}
	}

	return 0;
}


static int fjsec_check_chown_from_path(struct path *path, uid_t uid, gid_t gid)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;
	struct fs_path_config *pc;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	dprintk(KERN_INFO "(%s:%d) <%s>\n", __FUNCTION__, __LINE__, realpath);

	for (pc = devs; pc->prefix; pc++) {
		if (strcmp(realpath, pc->prefix) == 0) {
			if (((uid == pc->uid) || (uid == -1)) && ((gid == pc->gid) || (gid == -1))) {
				break;
			}

			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
		    	 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}
	}

	kfree(realpath);
	return 0;
}

static int fjsec_check_chown_from_id(struct path *path, uid_t uid, gid_t gid)
{
	struct inode *inode = path->dentry->d_inode;

	if(boot_mode == BOOT_MODE_FOTA) {
		return 0;
	}

	if((inode->i_mode & S_ISUID) && (inode->i_uid != 0)) {
		if (uid == 0) {
			fjsec_print_path(path);
			printk(KERN_INFO "%s:%d: REJECT process name=%s file name=%s current uid=%d \n",
				   __FUNCTION__, __LINE__, current->comm, path->dentry->d_iname, inode->i_uid);
			return -EPERM;
		}
	}

	if((inode->i_mode & S_ISGID) && (inode->i_gid != 0)) {
		if (gid == 0) {
			fjsec_print_path(path);
			printk(KERN_INFO "%s:%d: REJECT process name=%s file name=%s current gid=%d \n",
				   __FUNCTION__, __LINE__, current->comm, path->dentry->d_iname, inode->i_gid);
			return -EPERM;
		}
	}

	return 0;
}

static int fjsec_check_mknod(struct path *path, struct dentry *dentry,
			     int mode, unsigned int dev)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;
	struct fs_path_config *pc;
	u32 rdev;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	if (PATH_MAX > strlen(realpath) + strlen(dentry->d_name.name)) {
		strcat(realpath, dentry->d_name.name);
	} else {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -ENOMEM;
	}

	dprintk(KERN_INFO "(%s:%d) <%s> <%s>\n", __FUNCTION__, __LINE__, realpath, dentry->d_name.name);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	rdev = new_encode_dev(dev);

	for (pc = devs; pc->prefix; pc++) {
		if (rdev != pc->rdev) {
			continue;
		}

		if (mode != pc->mode) {
			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			     __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}

		if (strcmp(realpath, pc->prefix) != 0) {
			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			     __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}

		// rdev, mode, realpath all matched
		kfree(realpath);
		return 0;
	}

	for (pc = devs; pc->prefix; pc++) {
		if (strcmp(realpath, pc->prefix) != 0) {
			continue;
		}

		if (mode != pc->mode) {
			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
    			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}

		if (rdev != pc->rdev) {
			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
	    		 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}

		// realpath, mode, rdev all matched
		kfree(realpath);
		return 0;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_check_mount_point(char *realpath, char *dev_realpath)
{
	if (strcmp(dev_realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DEV_PATH) == 0) {
		if (strcmp(realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH) == 0) {
			return MATCH_SYSTEM_MOUNT_POINT;
		}
		else {
			return MISMATCH_MOUNT_POINT;
		}
	}
	else if (strncmp(realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH,
						strlen(CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH)) == 0) {
		return MISMATCH_MOUNT_POINT;
	}

#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	if (strcmp(dev_realpath, CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DEV_PATH) == 0) {
		if (strcmp(realpath, CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH) == 0) {
			return MATCH_SECURE_STORAGE_MOUNT_POINT;
		}
		else {
			return MISMATCH_MOUNT_POINT;
		}
	}
	else if (strncmp(realpath, CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH,
						strlen(CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH)) == 0) {
		return MISMATCH_MOUNT_POINT;
	}
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */

#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING
	if (strcmp(dev_realpath, CONFIG_SECURITY_FJSEC_KITTING_DEV_PATH) == 0) {
		if (strcmp(realpath, CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH) == 0) {
			return MATCH_KITTING_MOUNT_POINT;
		}
		else {
			return MISMATCH_MOUNT_POINT;
		}
	}
	else if (strncmp(realpath, CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH,
						strlen(CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH)) == 0) {
		return MISMATCH_MOUNT_POINT;
	}
#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */

	if (strcmp(realpath, ROOT_DIR) == 0) {
		return MATCH_ROOT_MOUNT_POINT;
	}
#if 0
	if (strcmp(dev_realpath, FOTA_PTN_PATH) == 0) {
		if (strcmp(realpath, FOTA_MOUNT_POINT) == 0) {
			return MATCH_MOUNT_POINT;
		}
		else {
			return MISMATCH_MOUNT_POINT;
		}
	}
	else if (strcmp(dev_realpath, FOTA2_PTN_PATH) == 0) {
		if (strcmp(realpath, FOTA_MOUNT_POINT) == 0) {
			return MATCH_MOUNT_POINT;
		}
		else {
			return MISMATCH_MOUNT_POINT;
		}
	}
	else if (strncmp(realpath, FOTA_MOUNT_POINT, strlen(FOTA_MOUNT_POINT)) == 0) {
		return MISMATCH_MOUNT_POINT;
	}
#endif
#ifdef DEBUG_ACCESS_CONTROL
        if (strcmp(dev_realpath, TEST_PTN_PATH) == 0) {
                if (strcmp(realpath, TEST_MOUNT_POINT) == 0) {
                        return MATCH_MOUNT_POINT;
                }
                else {
                        return MISMATCH_MOUNT_POINT;
                }
        }
        else if (strncmp(realpath, TEST_MOUNT_POINT, strlen(TEST_MOUNT_POINT)) == 0) {
                return MISMATCH_MOUNT_POINT;
        }
#endif

	return UNRELATED_MOUNT_POINT;
}

static int fjsec_ptrace_request_check(long request)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(ptrace_read_request_policy); index++) {
		if (request == ptrace_read_request_policy[index]) {
#ifdef DUMP_PTRACE_REQUEST
			if (prev_request != request) {
				printk(KERN_INFO "%s:%d: GRANTED read request. request=<%ld>, process name=<%s>, pid=<%d>.\n", __FUNCTION__, __LINE__, request, current->comm, current->pid);
		 		prev_request = request;
			}
#endif
			return 0;
		}
	}

	printk(KERN_INFO "%s:%d: REJECT write request. request=<%ld>, process name=%s, uid=%d, pid=%d\n",
		 __FUNCTION__, __LINE__, request, current->comm, current->cred->uid, current->pid);
	return -EPERM;
}

static int fjsec_ptrace_traceme(struct task_struct *parent)
{
	printk(KERN_DEBUG "%s:%d: REJECT pid=%d process name=%s, uid=%d, pid=%d\n",
		__FUNCTION__, __LINE__, parent->pid, current->comm, current->cred->uid, current->pid);
	return -EPERM;
}

static int fjsec_sb_mount(char *dev_name, struct path *path,
			    char *type, unsigned long flags, void *data)
{
	char *realpath;
	char *dev_realpath;
	int r;
	enum result_mount_point result;
	struct path dev_path;

	realpath = kzalloc(PATH_MAX, GFP_NOFS);
	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	dev_realpath = kzalloc(PATH_MAX, GFP_NOFS);
	if (!dev_realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -ENOMEM;
	}

	r = kern_path(dev_name, LOOKUP_FOLLOW, &dev_path);
	if (r == 0) {
		r = _xx_realpath_from_path(&dev_path, dev_realpath, PATH_MAX-1);
		path_put(&dev_path);
		if (r != 0) {
			printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
		    	 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			kfree(dev_realpath);
			return r;
		}
	}

	dprintk(KERN_INFO "(%s:%d) <%s> <%s>\n", __FUNCTION__, __LINE__, dev_name, dev_realpath);

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		kfree(dev_realpath);
		return r;
	}

	dprintk(KERN_INFO "(%s:%d) <%s>\n", __FUNCTION__, __LINE__, realpath);

	result = fjsec_check_mount_point(realpath, dev_realpath);

	dprintk(KERN_INFO "(%s:%d) mount point check result=<%d>\n", __FUNCTION__, __LINE__, result);

	if (result == MATCH_SYSTEM_MOUNT_POINT) {
		if (boot_mode == BOOT_MODE_FOTA || boot_mode == BOOT_MODE_SDDOWNLOADER || boot_mode == BOOT_MODE_OSUPDATE || boot_mode == BOOT_MODE_MASTERCLEAR) {
			kfree(realpath);
			kfree(dev_realpath);
			return 0;
		}

		if ((flags & MS_RDONLY) == 0) {
			if ((flags & MS_REMOUNT) == 0) {
				if (mount_flag == false) {
					mount_flag = true;
				}
				else {
					printk(KERN_INFO "%s:%d: REJECT R/W MOUNT dev_realpath=%s realpath=%s process name=%s, uid=%d, pid=%d\n",
						__FUNCTION__, __LINE__, dev_realpath, realpath, current->comm, current->cred->uid, current->pid);
					kfree(realpath);
					kfree(dev_realpath);
					return -EPERM;
				}
			}
			else {
				printk(KERN_INFO "%s:%d: REJECT R/W REMOUNT dev_realpath=%s realpath=%s process name=%s, uid=%d, pid=%d\n",
					__FUNCTION__, __LINE__, dev_realpath, realpath, current->comm, current->cred->uid, current->pid);
				kfree(realpath);
				kfree(dev_realpath);
				return -EPERM;
			}
		}

		kfree(realpath);
		kfree(dev_realpath);
		return 0;
	}
#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	else if (result == MATCH_SECURE_STORAGE_MOUNT_POINT) {
		kfree(realpath);
		kfree(dev_realpath);
		return 0;
	}
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */
#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING
	else if (result == MATCH_KITTING_MOUNT_POINT) {
		kfree(realpath);
		kfree(dev_realpath);
		return 0;
	}
#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */
	else if (result == MATCH_MOUNT_POINT || result == UNRELATED_MOUNT_POINT) {
		kfree(realpath);
		kfree(dev_realpath);
		return 0;
	}
	else if (result == MATCH_ROOT_MOUNT_POINT) {
#ifdef ROOTFS_RW_REMOUNT
		printk(KERN_INFO "%s:%d: rootfs is allowed to remount with rw flag for CT.",__FUNCTION__, __LINE__);
		kfree(realpath);
		kfree(dev_realpath);
		return 0;
#endif
		if (flags & MS_RDONLY) {
			kfree(realpath);
			kfree(dev_realpath);
			return 0;
		}		

		if (flags & MS_REMOUNT) {		
			printk(KERN_INFO "%s:%d: REJECT R/W REMOUNT dev_realpath=%s realpath=%s process name=%s, uid=%d, pid=%d\n",
						__FUNCTION__, __LINE__, dev_realpath, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			kfree(dev_realpath);
			return -EPERM;
		}else {
			kfree(realpath);
			kfree(dev_realpath);
			return 0;

		}
	}

    printk(KERN_INFO "%s:%d: REJECT MOUNT dev_realpath=%s realpath=%s process name=%s, uid=%d, pid=%d\n",
		__FUNCTION__, __LINE__, dev_realpath, realpath, current->comm, current->cred->uid, current->pid);
	kfree(realpath);
	kfree(dev_realpath);
	return -EPERM;
}

static int fjsec_sb_umount(struct vfsmount *mnt, int flags)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	struct path path = { mnt, mnt->mnt_root };
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(&path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	dprintk(KERN_INFO "%s:%d:(%s).\n", __FUNCTION__, __LINE__, realpath);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH) == 0) {
		if (boot_mode == BOOT_MODE_FOTA) {
			if (fjsec_check_access_process(CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME,
										   CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH) == 0) {
				kfree(realpath);
				return 0;
			}
			if (fjsec_check_access_process(SECURITY_FJSEC_RECOVERY_PROCESS_NAME,
										   SECURITY_FJSEC_RECOVERY_PROCESS_PATH) == 0) {
				kfree(realpath);
				return 0;
			}
		}

		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}
#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */
#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING
	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}
#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */

	kfree(realpath);
	return 0;
}

static int fjsec_sb_pivotroot(struct path *old_path, struct path *new_path)
{
	char *old_realpath;
	char *new_realpath;
	int r;

	old_realpath = kzalloc(PATH_MAX, GFP_NOFS);
	if (!old_realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	new_realpath = kzalloc(PATH_MAX, GFP_NOFS);
	if (!new_realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(old_realpath);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(old_path, old_realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath from old_path. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(old_realpath);
		kfree(new_realpath);
		return r;
	}

	r = _xx_realpath_from_path(new_path, new_realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath from new_path. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(old_realpath);
		kfree(new_realpath);
		return r;
	}

	printk(KERN_INFO "%s:%d: REJECT old_path=%s new_path=%s process name=%s, uid=%d, pid=%d\n",
		__FUNCTION__, __LINE__, old_realpath, new_realpath, current->comm, current->cred->uid, current->pid);
	kfree(old_realpath);
	kfree(new_realpath);
	return -EPERM;
}

#ifdef CONFIG_SECURITY_FJSEC_PROTECT_CHROOT
static int fjsec_path_chroot(struct path *path)
{
	char *realpath;
	char *tmp;
	char *p, *p2;
	int r;

	realpath = kzalloc(PATH_MAX, GFP_NOFS);
	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	tmp = kzalloc(PATH_MAX, GFP_NOFS);
	if (!tmp) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		kfree(tmp);
		return r;
	}

	p = CONFIG_SECURITY_FJSEC_CHROOT_PATH;
	while (*p) {
		p2 = strchr(p, ':');
		if (p2) {
			strncpy(tmp, p, (p2 - p));
			tmp[p2 - p] = 0;
		}
		else {
			strcpy(tmp, p);
		}

		if (strcmp(tmp, realpath) == 0) {
			kfree(realpath);
			kfree(tmp);
			return 0;
		}

		if (p2) {
			p = p2 + 1;
		}
		else {
			p += strlen(p);
		}
	}

	printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
	kfree(realpath);
	kfree(tmp);
	return -EPERM;
}
#endif	/* CONFIG_SECURITY_FJSEC_PROTECT_CHROOT */

static int fjsec_file_permission(struct file *file, int mask, loff_t offset, loff_t length)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(&file->f_path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	r = fjsec_read_write_access_control(realpath);
	if (r == -EPERM) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}
	else if (r == 0) {
		kfree(realpath);
		return 0;
	}

	if (fjsec_check_secure_storage_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_kitting_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	/* read mode -> OK! */
	if ((mask & MAY_WRITE) == 0) {
		kfree(realpath);
		return 0;
	}

	/* write mode -> check... */
	if (fjsec_check_disk_device_access(realpath, offset, length) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s offset=0x%llx length=0x%llx process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, offset, length, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_system_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_device_access_process(realpath, fs_acl) != 0) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

int fjsec_file_mmap(struct file *file, unsigned long reqprot,
			unsigned long prot, unsigned long flags,
			unsigned long addr, unsigned long addr_only)
{
	char *realpath;
	int r = 0;

	if (addr < dac_mmap_min_addr) {
		r = cap_capable(current_cred(), &init_user_ns, CAP_SYS_RAWIO,
				  SECURITY_CAP_AUDIT);
		/* set PF_SUPERPRIV if it turns out we allow the low mmap */
		if (r == 0)
			current->flags |= PF_SUPERPRIV;
	}
	if (r) {
		printk(KERN_INFO "%s:%d: REJECT mmap. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -EPERM;
	}

	realpath = kzalloc(PATH_MAX, GFP_NOFS);
	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	if (!file) {
		dprintk(KERN_INFO "%s:%d: file is null. process name =<%s>\n", __FUNCTION__, __LINE__, current->comm);
		kfree(realpath);
		return 0;
	}

	/* read only mode -> OK! */
	if ((prot & PROT_WRITE) == 0) {
		kfree(realpath);
		return 0;
	}

	dprintk(KERN_INFO "%s:%d: prot=<%ld>\n", __FUNCTION__, __LINE__, prot);

	r = _xx_realpath_from_path(&file->f_path, realpath, PATH_MAX-1);
	if (r != 0) {
		if (file->f_path.mnt->mnt_flags == 0) {
			kfree(realpath);
			return 0;
		} else {
			printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
				 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return r;
		}
	}

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_DISK_DEV_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_dentry_open(struct file *file, const struct cred *cred)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = _xx_realpath_from_path(&file->f_path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	r = fjsec_read_write_access_control(realpath);
	if (r == -EPERM) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s, process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}
	else if (r == 0) {
		kfree(realpath);
		return 0;
	}

	if (fjsec_check_secure_storage_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_secure_storage_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_kitting_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_kitting_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	/* read only mode -> OK! */
	if ((file->f_mode & FMODE_WRITE) == 0) {
		kfree(realpath);
		return 0;
	}

	if (fjsec_check_system_directory_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_device_access_process(realpath, fs_acl) != 0) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_system_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_device_access_process(realpath, ptn_acl) != 0) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

#ifdef CONFIG_SECURITY_PATH
static int fjsec_path_mknod(struct path *path, struct dentry *dentry, umode_t mode,
			    unsigned int dev)
{
	int r;

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	r = fjsec_check_mknod(path, dentry, mode, dev);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	return 0;
}

static int fjsec_path_mkdir(struct path *path, struct dentry *dentry, umode_t mode)
{
	int r;

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	return 0;
}

static int fjsec_path_rmdir(struct path *path, struct dentry *dentry)
{
	int r;

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	return 0;
}

static int fjsec_path_unlink(struct path *path, struct dentry *dentry)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		kfree(realpath);
		return r;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	if (PATH_MAX > strlen(realpath) + strlen(dentry->d_name.name)) {
		strcat(realpath, dentry->d_name.name);
	} else {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			   __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -ENOMEM;
	}

	dprintk(KERN_INFO "(%s:%d) <%s> <%s>\n", __FUNCTION__, __LINE__, realpath, dentry->d_name.name);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH) == 0 || 
		strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH) == 0 ) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_path_symlink(struct path *path, struct dentry *dentry,
			      const char *old_name)
{
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	int r;

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		kfree(realpath);
		return r;
	}

	r = _xx_realpath_from_path(path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	if (PATH_MAX > strlen(realpath) + strlen(dentry->d_name.name)) {
		strcat(realpath, dentry->d_name.name);
	} else {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			   __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -ENOMEM;
	}

	dprintk(KERN_INFO "(%s:%d) <%s> <%s> <%s>\n", __FUNCTION__, __LINE__, realpath, dentry->d_name.name, old_name);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH) == 0 ) {
		if (strcmp(old_name, CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH) != 0) {
			printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
				 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
			kfree(realpath);
			return -EPERM;
		}
	}

	kfree(realpath);
	return 0;
}

static int fjsec_path_link(struct dentry *old_dentry, struct path *new_dir,
			   struct dentry *new_dentry)
{
	int r;
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	struct path old_path = {new_dir->mnt, old_dentry};
	struct path new_path = {new_dir->mnt, new_dentry};

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = fjsec_check_path_from_path(new_dir);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		kfree(realpath);
		return r;
	}

	r = _xx_realpath_from_path(&old_path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	dprintk(KERN_INFO "(%s:%d) <%s>\n", __FUNCTION__, __LINE__, realpath);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_DISK_DEV_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_system_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_secure_storage_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_kitting_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_device_access_process(realpath, ptn_acl) != 0) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	r = _xx_realpath_from_path(&new_path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	dprintk(KERN_INFO "(%s:%d) <%s>\n", __FUNCTION__, __LINE__, realpath);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH) == 0 ) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_path_rename(struct path *old_dir, struct dentry *old_dentry,
			     struct path *new_dir, struct dentry *new_dentry)
{
	int r;
	char *realpath = kzalloc(PATH_MAX, GFP_NOFS);
	struct path old_path = {old_dir->mnt, old_dentry};
	struct path new_path = {new_dir->mnt, new_dentry};

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	r = fjsec_check_path_from_path(new_dir);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		kfree(realpath);
		return r;
	}

	r = _xx_realpath_from_path(&old_path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	dprintk(KERN_INFO "(%s:%d) <%s> <%s>\n", __FUNCTION__, __LINE__, realpath, old_dentry->d_name.name);

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_DISK_DEV_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_system_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_secure_storage_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_kitting_device_access_process(realpath) != 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (fjsec_check_device_access_process(realpath, ptn_acl) != 0) {
		dump_prinfo(__FUNCTION__, __LINE__);
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			__FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH) == 0 || 
		strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	r = _xx_realpath_from_path(&new_path, realpath, PATH_MAX-1);
	if (r != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return r;
	}

	if (strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH) == 0 || 
		strcmp(realpath, CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH) == 0 ||
		strcmp(realpath, CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH) == 0) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, realpath, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_path_truncate(struct path *path)
{
	int r;

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	return 0;
}

static int fjsec_path_chmod(struct path *path, umode_t mode)
{
	int r;

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	r = fjsec_check_chmod_from_path(path, mode);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	r = fjsec_check_chmod_from_mode(path, mode);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}
	return 0;
}

static int fjsec_path_chown(struct path *path, uid_t uid, gid_t gid)
{
	int r;

	r = fjsec_check_path_from_path(path);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	r = fjsec_check_chown_from_path(path, uid, gid);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	r = fjsec_check_chown_from_id(path, uid, gid);
	if (r) {
		printk(KERN_INFO "%s:%d: r=%d\n", __FUNCTION__, __LINE__, r);
		return r;
	}

	return 0;
}
#endif	/* CONFIG_SECURITY_PATH */

/* fjsec_vmalloc_to_sg() is ported from drivers/media/video/videobuf-dma-sg.c
 *
 * Return a scatterlist for some vmalloc()'ed memory
 * block (NULL on errors).  Memory for the scatterlist is allocated
 * using kmalloc.  The caller must free the memory.
 */
static struct scatterlist *fjsec_vmalloc_to_sg(void *buf, unsigned long buflen)
{
	struct scatterlist *sglist;
	struct page *pg;
	int index;
	int pages;
	int size;

	pages = PAGE_ALIGN(buflen) >> PAGE_SHIFT;

	sglist = kmalloc((pages * sizeof(*sglist)), GFP_NOFS);
	if (sglist == NULL) {
		printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	sg_init_table(sglist, pages);
	for (index = 0; index < pages; index++, buf += PAGE_SIZE) {
		pg = vmalloc_to_page(buf);
		if (pg == NULL) {
			printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
			kfree(sglist);
			return NULL;
		}

		size = buflen - (index * PAGE_SIZE);
		if (size > PAGE_SIZE) {
			size = PAGE_SIZE;
		}

		sg_set_page(&sglist[index], pg, size, 0);
	}
	return sglist;
}

static char *get_checksum(void *buf, unsigned long buflen)
{
	int ret;
	char *output;
	struct crypto_hash *tfm;
	struct scatterlist *sg;
	struct hash_desc desc;

	tfm = crypto_alloc_hash("sha256", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		 printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
		 return NULL;
	}

	output = kmalloc(crypto_hash_digestsize(tfm), GFP_NOFS);
	if (output == NULL) {
		crypto_free_hash(tfm);
		printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	desc.tfm = tfm;
	ret = crypto_hash_init(&desc);
	if (ret != 0) {
		kfree(output);
		crypto_free_hash(tfm);
		printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	sg = fjsec_vmalloc_to_sg(buf, buflen);
	if (sg == NULL) {
		kfree(output);
		crypto_free_hash(tfm);
		printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	ret = crypto_hash_digest(&desc, sg, buflen, output);
	if (ret != 0) {
		kfree(sg);
		kfree(output);
		crypto_free_hash(tfm);
		printk(KERN_INFO "%s:%d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	kfree(sg);
	crypto_free_hash(tfm);
	return output;
}

static int fjsec_kernel_load_module(char *name, void __user *umod, unsigned long len)
{
	const struct module_list *p = modlist;
	void *kmod;
	char *checksum = NULL;
	bool match_modname = false;

	if (!name) {
		printk(KERN_INFO "%s:%d: REJECT name=NULL process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -EINVAL;
	}

	dprintk(KERN_INFO "%s:%d: name=%s\n", __FUNCTION__, __LINE__, name);

	for (; p->name; p++) {
		if (strcmp(name, p->name) == 0) {
			match_modname = true;
			break;
		}
	}

	if (match_modname == false) {
		printk(KERN_INFO "%s:%d: REJECT Mismatched name=<%s> process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, name, current->comm, current->cred->uid, current->pid);
		return -EPERM;
	}

	kmod = vmalloc(len);
	if (!kmod) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}

	if (copy_from_user(kmod, umod, len) != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed copying from user. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		vfree(kmod);
		return -EFAULT;
	}

	checksum = get_checksum(kmod, len);
	if (checksum == NULL) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating checksum. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		vfree(kmod);
		return -EPERM;
	}

	if (memcmp((const void *)checksum, (const void *)p->checksum, SHA256_DIGEST_SIZE) != 0) {
		printk(KERN_INFO "%s:%d: REJECT Mismatched checksum. name=<%s> process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, name, current->comm, current->cred->uid, current->pid);
		print_hex_dump(KERN_INFO, "checksum: ", DUMP_PREFIX_OFFSET, 16, 1, checksum, SHA256_DIGEST_SIZE, false);
		vfree(kmod);
		kfree(checksum);
		return -EPERM;
	}

	vfree(kmod);
	kfree(checksum);
	return 0;
}

static void fjsec_check_update_module_list(void *start, unsigned int size, int mode)
{
	void *addr = start;
	unsigned long pfn, flags;

	spin_lock_irqsave(&page_module_list_lock, flags);

	set_memory_rw2((unsigned long) page_module_list, MAPPING_LIST_PAGE);

	do {
		pfn = vmalloc_to_pfn(addr) - PHYS_PFN_OFFSET;

		dprintk(KERN_INFO "%s:%d: pfn=%lu mode=%d BEFORE=%d\n", __FUNCTION__, __LINE__,
			   pfn, mode, (page_module_list[pfn >> 3] & (1 << (pfn & 7))? 1: 0));
		if (mode == FJSEC_LAOD)
			page_module_list[pfn >> 3] |= (1 << (pfn & 7));
		else if (mode == FJSEC_DELETE)
			page_module_list[pfn >> 3] &= ~(1 << (pfn & 7));
		dprintk(KERN_INFO "%s:%d: pfn=%lu mode=%d AFTER=%d\n", __FUNCTION__, __LINE__,
			   pfn, mode, (page_module_list[pfn >> 3] & (1 << (pfn & 7))? 1: 0));

	} while (addr += PAGE_SIZE, addr < start + size);

	set_memory_ro2((unsigned long) page_module_list, MAPPING_LIST_PAGE);

	spin_unlock_irqrestore(&page_module_list_lock, flags);
}

static void fjsec_init_module(struct module *mod)
{
	fjsec_check_update_module_list(mod->module_core, mod->core_size, FJSEC_LAOD);
}

static void fjsec_kernel_delete_module(struct module *mod)
{
	fjsec_check_update_module_list(mod->module_core, mod->core_size, FJSEC_DELETE);
}

void fjsec_make_page_acl(char *id, unsigned long addr, unsigned long size)
{
	struct ac_config_page *pc;
	int i =0;
	unsigned long set_start_pfn, set_end_pfn;

	set_start_pfn = __phys_to_pfn(addr) - PHYS_PFN_OFFSET;
	set_end_pfn = set_start_pfn + (PAGE_ALIGN(size) >> PAGE_SHIFT);

	for (pc = page_acl_pre; strncmp(pc->process_name, "0", 1) != 0; pc++,i++) {
		if (strcmp(id, pc->id) != 0)
			continue;

		if (pc->start_pfn) {
			if(pc->start_pfn > set_start_pfn) 
				pc->start_pfn = set_start_pfn;

			if(pc->end_pfn < set_end_pfn)
				pc->end_pfn = set_end_pfn;
		} else {
			pc->start_pfn = set_start_pfn;
			pc->end_pfn = set_end_pfn;
		}

		if (page_acl != NULL) {
			set_memory_rw2((unsigned long) page_acl, ALIGN(sizeof(page_acl_pre), PAGE_SIZE) >> PAGE_SHIFT);
			page_acl[i].start_pfn = pc->start_pfn;
			page_acl[i].end_pfn = pc->end_pfn;
			set_memory_ro2((unsigned long) page_acl, ALIGN(sizeof(page_acl_pre), PAGE_SIZE) >> PAGE_SHIFT);
		}
	}
}

static void fjsec_copy_page_acl(void)
{
	page_acl = vmalloc_exec(sizeof(page_acl_pre));
	if (!page_acl) {
		printk(KERN_INFO "%s:%d: Failed creating page_acl\n", __FUNCTION__, __LINE__);
		return;
	}
	memset(page_acl, 0, sizeof(page_acl_pre));

	memcpy(page_acl, page_acl_pre, sizeof(page_acl_pre));

	set_memory_ro2((unsigned long) page_acl, ALIGN(sizeof(page_acl_pre), PAGE_SIZE) >> PAGE_SHIFT);
	set_memory_nx2((unsigned long) page_acl, ALIGN(sizeof(page_acl_pre), PAGE_SIZE) >> PAGE_SHIFT);
}

void fjsec_init_list(void)
{
	page_free_list = vmalloc_exec(MAPPING_LIST_SIZE);
	memset(page_free_list, 0, MAPPING_LIST_SIZE);
	set_memory_ro2((unsigned long) page_free_list, MAPPING_LIST_PAGE);
	set_memory_nx2((unsigned long) page_free_list, MAPPING_LIST_PAGE);

	page_write_list = vmalloc_exec(MAPPING_LIST_SIZE);
	memset(page_write_list, 0, MAPPING_LIST_SIZE);
	set_memory_ro2((unsigned long) page_write_list, MAPPING_LIST_PAGE);
	set_memory_nx2((unsigned long) page_write_list, MAPPING_LIST_PAGE);

	page_exec_list = vmalloc_exec(MAPPING_LIST_SIZE);
	memset(page_exec_list, 0, MAPPING_LIST_SIZE);
	set_memory_ro2((unsigned long) page_exec_list, MAPPING_LIST_PAGE);
	set_memory_nx2((unsigned long) page_exec_list, MAPPING_LIST_PAGE);

	page_module_list = vmalloc_exec(MAPPING_LIST_SIZE);
	memset(page_module_list, 0, MAPPING_LIST_SIZE);
	set_memory_ro2((unsigned long) page_module_list, MAPPING_LIST_PAGE);
	set_memory_nx2((unsigned long) page_module_list, MAPPING_LIST_PAGE);

	spin_lock_init(&page_module_list_lock);

	fjsec_copy_page_acl();
}

pte_t *lookup_address(unsigned long address, unsigned int *level)
{
	pgd_t *pgd = pgd_offset_k(address);
	pte_t *pte;
	pmd_t *pmd;

	/* pmds are folded into pgds on ARM */
	*level = PG_LEVEL_NONE;

	if (pgd == NULL || pgd_none(*pgd))
		return NULL;

	/* pmds are folded into pgds on ARM */
	pmd = (pmd_t *)pgd;

	if (pmd == NULL || pmd_none(*pmd) || !pmd_present(*pmd))
		return NULL;

	if (((pmd_val(*pmd) & (PMD_TYPE_SECT | PMD_SECT_SUPER))
		 == (PMD_TYPE_SECT | PMD_SECT_SUPER)) || !pmd_present(*pmd)) {
		return NULL;
	} else if (pmd_val(*pmd) & PMD_TYPE_SECT) {
		*level = PG_LEVEL_2M;
		return (pte_t *)pmd;
	}

	pte = pte_offset_kernel(pmd, address);

	if ((pte == NULL) || pte_none(*pte))
		return NULL;

	*level = PG_LEVEL_4K;

	return pte;
}

void fjsec_make_freepage_list(void)
{
	unsigned long pfn = 0;
	void *free_address;
	pte_t *free_pte;
    unsigned int level;

	set_memory_rw2((unsigned long) page_free_list, MAPPING_LIST_PAGE);
	set_memory_rw2((unsigned long) page_write_list, MAPPING_LIST_PAGE);
	set_memory_rw2((unsigned long) page_exec_list, MAPPING_LIST_PAGE);
	
	do {
		if(page_count(pfn_to_page(pfn+PHYS_PFN_OFFSET)))
			continue;

		page_free_list[pfn >> 3] |= (1 << (pfn & 7));

		free_address = page_address(pfn_to_page(pfn+PHYS_PFN_OFFSET));
		if(!free_address)
			continue;

		free_pte = lookup_address((unsigned long)free_address,&level);
		if(level == PG_LEVEL_4K) {
			if(pte_write(*free_pte)) {
				page_write_list[pfn >> 3] |= (1 << (pfn & 7));
			}
			if(pte_exec(*free_pte)) {
				page_exec_list[pfn >> 3] |= (1 << (pfn & 7));
			}
		}else if (level == PG_LEVEL_2M) {
			unsigned long pmd;
			pmd = pmd_val(*free_pte);
			if (!(pmd & PMD_SECT_XN)){
				page_exec_list[pfn >> 3] |= (1 << (pfn & 7));
			}
			if (pmd  & PMD_SECT_AP_WRITE) {
				page_write_list[pfn >> 3] |= (1 << (pfn & 7));
			}
		}else {
			page_write_list[pfn >> 3] |= (1 << (pfn & 7));
			page_exec_list[pfn >> 3] |= (1 << (pfn & 7));
		}
	} while (pfn++, pfn < MAX_LOW_PFN);
	
	set_memory_ro2((unsigned long) page_free_list, MAPPING_LIST_PAGE);
	set_memory_ro2((unsigned long) page_write_list, MAPPING_LIST_PAGE);
	set_memory_ro2((unsigned long) page_exec_list, MAPPING_LIST_PAGE);
}

static int fjsec_check_page_acl(unsigned long pfn, unsigned long size)
{
	struct ac_config_page *pc;

	for (pc = page_acl; strncmp(pc->process_name, "0", 1) != 0; pc++) {
		if (strcmp(pc->process_name, NO_CHECK_STR) != 0 && strcmp(current->comm, pc->process_name) != 0)
			continue;

		if (strcmp(pc->process_path, NO_CHECK_STR) != 0 && fjsec_check_access_process_path_mapping(pc->process_path))
			continue;

		if (pc->uid != UID_NO_CHECK && pc->uid != current->cred->uid)
			continue;

		if (!(pc->start_pfn <= pfn && (pfn + (PAGE_ALIGN(size) >> PAGE_SHIFT)) <= pc->end_pfn))
			continue;

		dprintk(KERN_INFO "%s:%d: pfn=0x%lx size=0x%lx pc->start_pfn=0x%lx pc->end_pfn=0x%lx\n",
				__FUNCTION__, __LINE__, pfn, size, pc->start_pfn, pc->end_pfn);

		return 0;
	}

	return -EPERM;
}

#define is_free(pfn) (page_free_list[pfn >> 3] & (1 << (pfn & 7)))
#define can_write(pfn) (page_write_list[pfn >> 3] & (1 << (pfn & 7)))
#define can_exec(pfn) (page_exec_list[pfn >> 3] & (1 << (pfn & 7)))
#define is_module(pfn) (page_module_list[pfn >> 3] & (1 << (pfn & 7)))

static int fjsec_check_mapping(unsigned long pfn, unsigned long size, pgprot_t prot)
{
	unsigned long offset = 0;

  dprintk(KERN_INFO "LSM_MMAP_DISABLE:fjsec_check_mapping start.\n");
#ifdef CONFIG_SECURITY_MMAP_DISABLE
	dprintk(KERN_INFO "LSM_MMAP_DISABLE=true.\n");
	return 0;
#endif // CONFIG_SECURITY_MMAP_DISABLE

	pfn -= PHYS_PFN_OFFSET;
	do {
		if (!((0 <= pfn) && (pfn < MAX_LOW_PFN)))
			continue;

		if (!is_free(pfn)) {
			if (fjsec_check_page_acl(pfn, size - offset)) {
				printk(KERN_INFO "%s:%d: REJECT Invalid Mapping(free_list) process name=%s pfn=0x%lx size=0x%lx addr=0x%p\n",
					   __FUNCTION__, __LINE__, current->comm, pfn, size, page_address(pfn_to_page(pfn+PHYS_PFN_OFFSET)));
				return -EPERM;
			} else {
				return 0;
			}
		}

		if (is_module(pfn)) {
			printk(KERN_INFO "%s:%d: REJECT Invalid Mapping(module_list) process name=%s pfn=0x%lx size=0x%lx\n",
				   __FUNCTION__, __LINE__, current->comm, pfn, size);
			return -EPERM;
		}

		if (!(pgprot_val(prot) & L_PTE_RDONLY) && !can_write(pfn)) {
			printk(KERN_INFO "%s:%d: REJECT Invalid Mapping(write_list) process name=%s pfn=0x%lx size=0x%lx\n",
				   __FUNCTION__, __LINE__, current->comm, pfn, size);
			return -EPERM;
		}

		if (!(pgprot_val(prot) & L_PTE_XN) && !can_exec(pfn)) {
			printk(KERN_INFO "%s:%d: REJECT Invalid Mapping(exec_list) process name=%s pfn=0x%lx size=0x%lx\n",
				   __FUNCTION__, __LINE__, current->comm, pfn, size);
			return -EPERM;
		}
	} while (offset += PAGE_SIZE, pfn++, offset < size);

	return 0;
}

static int fjsec_remap_pfn_range(unsigned long pfn, unsigned long size,pgprot_t prot)
{
	int ret;

	ret = fjsec_check_mapping(pfn, size, prot);
	if(ret){
		printk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);
		ret = -EFAULT;
	}
	return ret;
}

static int fjsec_ioremap_page_range(unsigned long addr,unsigned long end, phys_addr_t phys_addr, pgprot_t prot)
{
	int ret;

	ret = fjsec_check_mapping(__phys_to_pfn(phys_addr), end - addr, prot);
	if(ret){
		printk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);
		ret = -EFAULT;
	}
	return ret;
}

static int fjsec_insert_page(struct page *page, pgprot_t prot)
{
	int ret;

	ret = fjsec_check_mapping(page_to_pfn(page), PAGE_SIZE, prot);
	if(ret){
		printk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);
		ret = -EFAULT;
	}
	return ret;
}

static int fjsec_insert_pfn(unsigned long pfn, pgprot_t prot)
{
	int ret;

	ret = fjsec_check_mapping(pfn, PAGE_SIZE, prot);
	if(ret){
		printk(KERN_INFO "%s:%d: process name=%s\n", __FUNCTION__, __LINE__, current->comm);
		ret = -EFAULT;
	}
	return ret;
}

static int fjsec_check_setid_access_process(void)
{
	char *buf = NULL;
	char *binname;
	struct ac_config_setid *config;

	if (current->cred->uid != AID_ROOT) {
		return 0;
	}

	buf = kzalloc(PATH_MAX, GFP_NOFS);
	if (!buf) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		return -ENOMEM;
	}
        
	binname = get_process_path(current, buf, PATH_MAX-1);
	if (binname == NULL || IS_ERR(binname)) {
		printk(KERN_INFO "%s:%d: Failed getting process path. process name=%s, uid=%d, pid=%d\n"
			   , __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(buf);
		return -EPERM;
	}

	dprintk(KERN_INFO "%s:%d: process path=%s\n", __FUNCTION__, __LINE__, binname);

	if (strncmp(binname, CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH,
				strlen(CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH)) == 0) {
		kfree(buf);
		return 0;
	}

	for (config = setid_acl; config->process_name; config++) {
		if (strcmp(current->comm, config->process_name) == 0) {
			if (strcmp(binname, config->process_path) == 0) {
				kfree(buf);
				return 0;
			}
		}
	}
	printk(KERN_INFO "%s:%d: REJECT realpath=%s process name=%s\n",
		   __FUNCTION__, __LINE__, binname, current->comm);

	kfree(buf);
	return -EPERM;


}

extern inline void cap_emulate_setxuid(struct cred *new, const struct cred *old);
static int fjsec_task_fix_setuid(struct cred *new, const struct cred *old,int flags)
{
	int ret;

	switch (flags) {
	case LSM_SETID_RE:
	case LSM_SETID_ID:
	case LSM_SETID_RES:
		/* juggle the capabilities to follow [RES]UID changes unless
		 * otherwise suppressed */
		if (!issecure(SECURE_NO_SETUID_FIXUP))
			cap_emulate_setxuid(new, old);
		break;

	case LSM_SETID_FS:
		/* juggle the capabilties to follow FSUID changes, unless
		 * otherwise suppressed
		 *
		 * FIXME - is fsuser used for all CAP_FS_MASK capabilities?
		 *          if not, we might be a bit too harsh here.
		 */
		if (!issecure(SECURE_NO_SETUID_FIXUP)) {
			if (old->fsuid == 0 && new->fsuid != 0)
				new->cap_effective =
					cap_drop_fs_set(new->cap_effective);

			if (old->fsuid != 0 && new->fsuid == 0)
				new->cap_effective =
					cap_raise_fs_set(new->cap_effective,
							 new->cap_permitted);
		}
		break;

	default:
		return -EINVAL;
	}

	ret = fjsec_check_setid_access_process();
	if (ret) {
		printk(KERN_INFO "%s:%d: REJECT flags %d\n", __FUNCTION__, __LINE__, flags);
		return ret;
	}

	return 0;
}

static int fjsec_task_fix_setgid(struct cred *new, const struct cred *old,int flags)
{
	int ret;

	ret = fjsec_check_setid_access_process();
	if (ret) {
		printk(KERN_INFO "%s:%d: REJECT flags %d\n", __FUNCTION__, __LINE__, flags);
		return ret;
	}

	return 0;
}

extern void warn_setuid_and_fcaps_mixed(const char *fname);
extern int get_file_caps(struct linux_binprm *bprm, bool *effective, bool *has_cap);
static int fjsec_bprm_set_creds(struct linux_binprm *bprm)
{
	char *realpath = NULL;
	const struct cred *old = current_cred();
	struct cred *new = bprm->cred;
	bool effective, has_cap = false;
	int ret;

	effective = false;
	ret = get_file_caps(bprm, &effective, &has_cap);
	if (ret < 0)
		return ret;

	if (!issecure(SECURE_NOROOT)) {
		/*
		 * If the legacy file capability is set, then don't set privs
		 * for a setuid root binary run by a non-root user.  Do set it
		 * for a root user just to cause least surprise to an admin.
		 */
		if (has_cap && new->uid != 0 && new->euid == 0) {
			warn_setuid_and_fcaps_mixed(bprm->filename);
			goto skip;
		}
		/*
		 * To support inheritance of root-permissions and suid-root
		 * executables under compatibility mode, we override the
		 * capability sets for the file.
		 *
		 * If only the real uid is 0, we do not set the effective bit.
		 */
		if (new->euid == 0 || new->uid == 0) {
			/* pP' = (cap_bset & ~0) | (pI & ~0) */
			new->cap_permitted = cap_combine(old->cap_bset,
							 old->cap_inheritable);
		}
		if (new->euid == 0)
			effective = true;
	}
skip:

	/* if we have fs caps, clear dangerous personality flags */
	if (!cap_issubset(new->cap_permitted, old->cap_permitted))
		bprm->per_clear |= PER_CLEAR_ON_SETID;


	/* Don't let someone trace a set[ug]id/setpcap binary with the revised
	 * credentials unless they have the appropriate permit
	 */
	if ((new->euid != old->uid ||
	     new->egid != old->gid ||
	     !cap_issubset(new->cap_permitted, old->cap_permitted)) &&
	    bprm->unsafe & ~LSM_UNSAFE_PTRACE_CAP) {
		/* downgrade; they get no more than they had, and maybe less */
		if (!capable(CAP_SETUID)) {
			new->euid = new->uid;
			new->egid = new->gid;
		}
		new->cap_permitted = cap_intersect(new->cap_permitted,
						   old->cap_permitted);
	}

	new->suid = new->fsuid = new->euid;
	new->sgid = new->fsgid = new->egid;

	if (effective)
		new->cap_effective = new->cap_permitted;
	else
		cap_clear(new->cap_effective);
	bprm->cap_effective = effective;

	/*
	 * Audit candidate if current->cap_effective is set
	 *
	 * We do not bother to audit if 3 things are true:
	 *   1) cap_effective has all caps
	 *   2) we are root
	 *   3) root is supposed to have all caps (SECURE_NOROOT)
	 * Since this is just a normal root execing a process.
	 *
	 * Number 1 above might fail if you don't have a full bset, but I think
	 * that is interesting information to audit.
	 */
	if (!cap_isclear(new->cap_effective)) {
		if (!cap_issubset(CAP_FULL_SET, new->cap_effective) ||
		    new->euid != 0 || new->uid != 0 ||
		    issecure(SECURE_NOROOT)) {
			ret = audit_log_bprm_fcaps(bprm, new, old);
			if (ret < 0)
				return ret;
		}
	}

	new->securebits &= ~issecure_mask(SECURE_KEEP_CAPS);

	realpath = kzalloc(PATH_MAX, GFP_NOFS);

	if (!realpath) {
		printk(KERN_INFO "%s:%d: REJECT Failed allocating buffer.\n", __FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	ret = _xx_realpath_from_path(&bprm->file->f_path, realpath, PATH_MAX-1);
	if (ret != 0) {
		printk(KERN_INFO "%s:%d: REJECT Failed creating realpath. process name=%s, uid=%d, pid=%d\n",
			 __FUNCTION__, __LINE__, current->comm, current->cred->uid, current->pid);
		kfree(realpath);
		return ret;
	}

	if (strncmp(realpath, CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH,
				strlen(CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH)) == 0) {
		kfree(realpath);
		return 0;
	}

	if((current->cred->euid != 0) && (bprm->cred->euid == 0)) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s, process name=%s, current euid %d request euid %d\n",
			   __FUNCTION__, __LINE__, realpath, current->comm,current->cred->euid, bprm->cred->euid);
		kfree(realpath);
		return -EPERM;
	}

	if((current->cred->egid != 0) && (bprm->cred->egid == 0)) {
		printk(KERN_INFO "%s:%d: REJECT realpath=%s, process name=%s, current egid %d request egid %d\n",
			   __FUNCTION__, __LINE__, realpath, current->comm,current->cred->egid, bprm->cred->egid);
		kfree(realpath);
		return -EPERM;
	}

	kfree(realpath);
	return 0;
}

static int fjsec_check_pte_range(struct mm_struct *mm, pmd_t *pmd,
            unsigned long addr, unsigned long end, pgprot_t prot)
{
	pte_t *pte;
	spinlock_t *ptl;
	int ret = 0;

	pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
	arch_enter_lazy_mmu_mode();
	do {
		if (pte_present(*pte)) {
			ret = fjsec_check_mapping(pte_pfn(*pte), 1, prot);
			if (ret) {
				printk(KERN_INFO "%s:%d: REJECT process name=%s, pfn=0x%lx, prot=0x%x\n",
					   __FUNCTION__, __LINE__, current->comm, pte_pfn(*pte) - PHYS_PFN_OFFSET, prot);
				break;
			}
		}
	} while (pte++, addr += PAGE_SIZE, addr != end);
	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(pte - 1, ptl);

	return ret;
}

static inline int fjsec_check_pmd_range(struct vm_area_struct *vma, pud_t *pud,
                   unsigned long addr, unsigned long end, pgprot_t prot)
{
	pmd_t *pmd;
	unsigned long next;
	int ret = 0;

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none_or_clear_bad(pmd))
			continue;
		ret = fjsec_check_pte_range(vma->vm_mm, pmd, addr, next, prot);
		if (ret)
			break;
	} while (pmd++, addr = next, addr != end);

	return ret;
}

static inline int fjsec_check_pud_range(struct vm_area_struct *vma, pgd_t *pgd,
                                    unsigned long addr, unsigned long end, pgprot_t prot)
{
	pud_t *pud;
	unsigned long next;
	int ret = 0;

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pud))
			continue;
		ret= fjsec_check_pmd_range(vma, pud, addr, next, prot);
		if(ret)
			break;

	} while (pud++, addr = next, addr != end);

	return ret;
}

static int fjsec_file_mprotect(struct vm_area_struct *vma, unsigned long reqprot,
			   unsigned long prot, unsigned long start, unsigned long end ,unsigned long newflags)
{
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgd;
	unsigned long next;
	int ret = 0;

	return 0;
	
	pgd = pgd_offset(mm, start);
	do {
		next = pgd_addr_end(start, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		ret = fjsec_check_pud_range(vma, pgd, start, next, vm_get_page_prot(newflags));
		if(ret)
			return ret;
	} while (pgd++, start = next, start != end);

	return 0;
}

#ifdef DEBUG_MAPPING
void fjsec_debug_test_area_setting(unsigned long w_addr, unsigned long x_addr, 
				unsigned long nw_addr, unsigned long nx_addr)
{
	unsigned long w_pfn = 0;
	unsigned long x_pfn = 0;
	unsigned long nw_pfn = 0;
	unsigned long nx_pfn = 0;
	int i;

	w_pfn = page_to_pfn(virt_to_page((unsigned long)w_addr));
	w_pfn -= PHYS_PFN_OFFSET;
	
	x_pfn = page_to_pfn(virt_to_page((unsigned long)x_addr));
	x_pfn -= PHYS_PFN_OFFSET;
	
	nw_pfn = page_to_pfn(virt_to_page((unsigned long)nw_addr));
	nw_pfn -= PHYS_PFN_OFFSET;
	
	nx_pfn = page_to_pfn(virt_to_page((unsigned long)nx_addr));
	nx_pfn -= PHYS_PFN_OFFSET;

	set_memory_rw2(page_write_list, MAPPING_LIST_PAGE);
	set_memory_rw2(page_exec_list, MAPPING_LIST_PAGE);
	
	// ## write area ##
	for ( i = 0; i < 10; i++){
		page_write_list[w_pfn >> 3] |= (1 << (w_pfn & 7));
		page_exec_list[w_pfn >> 3] |= (1 << (w_pfn & 7));
		w_pfn++;
	}

	// ## exec area ##
	for ( i = 0; i < 10; i++){
		page_write_list[x_pfn >> 3] &= ~(1 << (x_pfn & 7));
		page_exec_list[x_pfn >> 3] |= (1 << (x_pfn & 7));
		x_pfn++;
	}

	// ## non write area ##
	for ( i = 0; i < 10; i++){
		page_write_list[nw_pfn >> 3] &= ~(1 << (nw_pfn & 7));
		page_exec_list[nw_pfn >> 3] |= (1 << (nw_pfn & 7));
		nw_pfn++;
	}

	// ## non exec area ##
	for ( i = 0; i < 10; i++){
		page_write_list[nx_pfn >> 3] |= (1 << (nx_pfn & 7));
		page_exec_list[nx_pfn >> 3] &= ~(1 << (nx_pfn & 7));
		nx_pfn++;
	}

	set_memory_ro2(page_write_list, MAPPING_LIST_PAGE);
	set_memory_ro2(page_exec_list, MAPPING_LIST_PAGE);
		
}
EXPORT_SYMBOL(fjsec_debug_test_area_setting);

enum pfn_mode {
	MODE_WRITE,
	MODE_EXEC,
	MODE_NWRITE,
	MODE_NEXEC,
	MODE_NFREE,
	MODE_MODULE,
};

unsigned long fjsec_debug_find_pfn(int mode, unsigned long addr)
{
	unsigned long pfn = 0;
	
	switch (mode) {
	case MODE_WRITE:
		pfn = page_to_pfn(virt_to_page((unsigned long)addr));
		pfn -= PHYS_PFN_OFFSET;
		break;
	case MODE_EXEC:
		pfn = page_to_pfn(virt_to_page((unsigned long)addr));
		pfn -= PHYS_PFN_OFFSET;
		break;
	case MODE_NWRITE:
		pfn = page_to_pfn(virt_to_page((unsigned long)addr));
		pfn -= PHYS_PFN_OFFSET;
		break;
	case MODE_NEXEC:
		pfn = page_to_pfn(virt_to_page((unsigned long)addr));
		pfn -= PHYS_PFN_OFFSET;
		break;
	case MODE_NFREE:
		do {
			if (!is_free(pfn))
				break;
		} while (pfn++, pfn < MAX_LOW_PFN);
		break;
	case MODE_MODULE:
		do {
			if (is_module(pfn))
				break;
		} while (pfn++, pfn < MAX_LOW_PFN);
		break;
	default:
		break;
	}

	pfn += PHYS_PFN_OFFSET;

	return pfn;
	
}
EXPORT_SYMBOL(fjsec_debug_find_pfn);
#endif //DEBUG_MAPPING

const struct security_operations fjsec_security_ops = {
	.name =					"fjsec",
	.ptrace_traceme =		fjsec_ptrace_traceme,
	.ptrace_request_check =	fjsec_ptrace_request_check,
	.sb_mount =				fjsec_sb_mount,
	.sb_pivotroot =			fjsec_sb_pivotroot,
#ifdef CONFIG_SECURITY_PATH
#ifdef CONFIG_SECURITY_FJSEC_PROTECT_CHROOT
	.path_chroot =			fjsec_path_chroot,
#endif	/* CONFIG_SECURITY_FJSEC_PROTECT_CHROOT */
#endif	/*  CONFIG_SECURITY_PATH */
	.sb_umount =			fjsec_sb_umount,
	.file_permission =		fjsec_file_permission,
	.file_mmap =			fjsec_file_mmap,
	.dentry_open =			fjsec_dentry_open,
#ifdef CONFIG_SECURITY_PATH
	.path_mknod =			fjsec_path_mknod,
	.path_mkdir =			fjsec_path_mkdir,
	.path_rmdir =			fjsec_path_rmdir,
	.path_unlink =			fjsec_path_unlink,
	.path_symlink =			fjsec_path_symlink,
	.path_link =			fjsec_path_link,
	.path_rename =			fjsec_path_rename,
	.path_truncate =		fjsec_path_truncate,
	.path_chmod =			fjsec_path_chmod,
	.path_chown =			fjsec_path_chown,
#endif	/* CONFIG_SECURITY_PATH */
	.init_module =			fjsec_init_module,
	.kernel_load_module =	fjsec_kernel_load_module,
	.kernel_delete_module =	fjsec_kernel_delete_module,
	.remap_pfn_range =		fjsec_remap_pfn_range,
	.ioremap_page_range =	fjsec_ioremap_page_range,
	.insert_page =			fjsec_insert_page,
	.insert_pfn =			fjsec_insert_pfn,
	.task_fix_setuid = 		fjsec_task_fix_setuid,
	.task_fix_setgid = 		fjsec_task_fix_setgid,
	.bprm_set_creds = 		fjsec_bprm_set_creds,
	.file_mprotect = 		fjsec_file_mprotect,
	.ptrace_access_check = 		cap_ptrace_access_check,
//	.ptrace_traceme = 		cap_ptrace_traceme,
//	.ptrace_request_check = 	cap_ptrace_request_check,
	.capget = 			cap_capget,
	.capset = 			cap_capset,
	.capable = 			cap_capable,
	.quotactl = 			cap_quotactl,
	.quota_on = 			cap_quota_on,
	.syslog = 			cap_syslog,
	.settime = 			cap_settime,
	.vm_enough_memory = 		cap_vm_enough_memory,
//	.bprm_set_creds = 		cap_bprm_set_creds,
	.bprm_committing_creds = 	cap_bprm_committing_creds,
	.bprm_committed_creds = 	cap_bprm_committed_creds,
	.bprm_check_security = 		cap_bprm_check_security,
	.bprm_secureexec = 		cap_bprm_secureexec,
	.sb_alloc_security = 		cap_sb_alloc_security,
	.sb_free_security = 		cap_sb_free_security,
	.sb_copy_data = 		cap_sb_copy_data,
	.sb_remount = 			cap_sb_remount,
	.sb_kern_mount = 		cap_sb_kern_mount,
	.sb_show_options = 		cap_sb_show_options,
	.sb_statfs = 			cap_sb_statfs,
//	.sb_mount = 			cap_sb_mount,
//	.sb_umount = 			cap_sb_umount,
//	.sb_pivotroot = 		cap_sb_pivotroot,
	.sb_set_mnt_opts = 		cap_sb_set_mnt_opts,
	.sb_clone_mnt_opts = 		cap_sb_clone_mnt_opts,
	.sb_parse_opts_str = 		cap_sb_parse_opts_str,
	.inode_alloc_security = 	cap_inode_alloc_security,
	.inode_free_security = 		cap_inode_free_security,
	.inode_init_security = 		cap_inode_init_security,
	.inode_create = 		cap_inode_create,
	.inode_link = 			cap_inode_link,
	.inode_unlink = 		cap_inode_unlink,
	.inode_symlink = 		cap_inode_symlink,
	.inode_mkdir = 			cap_inode_mkdir,
	.inode_rmdir = 			cap_inode_rmdir,
	.inode_mknod = 			cap_inode_mknod,
	.inode_rename = 		cap_inode_rename,
	.inode_readlink = 		cap_inode_readlink,
	.inode_follow_link = 		cap_inode_follow_link,
	.inode_permission = 		cap_inode_permission,
	.inode_setattr = 		cap_inode_setattr,
	.inode_getattr = 		cap_inode_getattr,
	.inode_setxattr = 		cap_inode_setxattr,
	.inode_post_setxattr = 		cap_inode_post_setxattr,
	.inode_getxattr = 		cap_inode_getxattr,
	.inode_listxattr = 		cap_inode_listxattr,
	.inode_removexattr = 		cap_inode_removexattr,
	.inode_need_killpriv = 		cap_inode_need_killpriv,
	.inode_killpriv = 		cap_inode_killpriv,
	.inode_getsecurity = 		cap_inode_getsecurity,
	.inode_setsecurity = 		cap_inode_setsecurity,
	.inode_listsecurity = 		cap_inode_listsecurity,
	.inode_getsecid = 		cap_inode_getsecid,
//#ifdef CONFIG_SECURITY_PATH 
//	.path_mknod = 			cap_path_mknod,
//	.path_mkdir = 			cap_path_mkdir,
//	.path_rmdir = 			cap_path_rmdir,
//	.path_unlink = 			cap_path_unlink,
//	.path_symlink = 		cap_path_symlink,
//	.path_link = 			cap_path_link,
//	.path_rename = 			cap_path_rename,
//	.path_truncate = 		cap_path_truncate,
//	.path_chmod = 			cap_path_chmod,
//	.path_chown = 			cap_path_chown,
//#ifndef CONFIG_SECURITY_FJSEC_PROTECT_CHROOT
//	.path_chroot = 			cap_path_chroot,
//#endif	/* CONFIG_SECURITY_FJSEC_PROTECT_CHROOT */
//#endif 
//	.file_permission = 		cap_file_permission,
	.file_alloc_security = 		cap_file_alloc_security,
	.file_free_security = 		cap_file_free_security,
	.file_ioctl = 			cap_file_ioctl,
//	.file_mmap = 			cap_file_mmap,
//	.file_mprotect = 		cap_file_mprotect,
	.file_lock = 			cap_file_lock,
	.file_fcntl = 			cap_file_fcntl,
	.file_set_fowner = 		cap_file_set_fowner,
	.file_send_sigiotask = 		cap_file_send_sigiotask,
	.file_receive = 		cap_file_receive,
//	.dentry_open = 			cap_dentry_open,
	.task_create = 			cap_task_create,
	.task_free =			cap_task_free,
	.cred_alloc_blank = 		cap_cred_alloc_blank,
	.cred_free = 			cap_cred_free,
	.cred_prepare = 		cap_cred_prepare,
	.cred_transfer = 		cap_cred_transfer,
	.kernel_act_as = 		cap_kernel_act_as,
	.kernel_create_files_as = 	cap_kernel_create_files_as,
	.kernel_module_request = 	cap_kernel_module_request,
//	.kernel_load_module = 		cap_kernel_load_module,
//	.task_fix_setuid = 		cap_task_fix_setuid,
	.task_setpgid = 		cap_task_setpgid,
	.task_getpgid = 		cap_task_getpgid,
	.task_getsid = 			cap_task_getsid,
	.task_getsecid = 		cap_task_getsecid,
	.task_setnice = 		cap_task_setnice,
	.task_setioprio = 		cap_task_setioprio,
	.task_getioprio = 		cap_task_getioprio,
	.task_setrlimit = 		cap_task_setrlimit,
	.task_setscheduler = 		cap_task_setscheduler,
	.task_getscheduler = 		cap_task_getscheduler,
	.task_movememory = 		cap_task_movememory,
	.task_wait = 			cap_task_wait,
	.task_kill = 			cap_task_kill,
	.task_prctl = 			cap_task_prctl,
	.task_to_inode = 		cap_task_to_inode,
	.ipc_permission = 		cap_ipc_permission,
	.ipc_getsecid = 		cap_ipc_getsecid,
	.msg_msg_alloc_security = 	cap_msg_msg_alloc_security,
	.msg_msg_free_security = 	cap_msg_msg_free_security,
	.msg_queue_alloc_security = 	cap_msg_queue_alloc_security,
	.msg_queue_free_security = 	cap_msg_queue_free_security,
	.msg_queue_associate = 		cap_msg_queue_associate,
	.msg_queue_msgctl = 		cap_msg_queue_msgctl,
	.msg_queue_msgsnd = 		cap_msg_queue_msgsnd,
	.msg_queue_msgrcv = 		cap_msg_queue_msgrcv,
	.shm_alloc_security = 		cap_shm_alloc_security,
	.shm_free_security = 		cap_shm_free_security,
	.shm_associate = 		cap_shm_associate,
	.shm_shmctl = 			cap_shm_shmctl,
	.shm_shmat = 			cap_shm_shmat,
	.sem_alloc_security = 		cap_sem_alloc_security,
	.sem_free_security = 		cap_sem_free_security,
	.sem_associate = 		cap_sem_associate,
	.sem_semctl = 			cap_sem_semctl,
	.sem_semop = 			cap_sem_semop,
	.netlink_send = 		cap_netlink_send,
	.d_instantiate = 		cap_d_instantiate,
	.getprocattr = 			cap_getprocattr,
	.setprocattr = 			cap_setprocattr,
	.secid_to_secctx = 		cap_secid_to_secctx,
	.secctx_to_secid = 		cap_secctx_to_secid,
	.release_secctx = 		cap_release_secctx,
	.inode_notifysecctx = 		cap_inode_notifysecctx,
	.inode_setsecctx = 		cap_inode_setsecctx,
	.inode_getsecctx = 		cap_inode_getsecctx,
#ifdef CONFIG_SECURITY_NETWORK 
	.unix_stream_connect = 		cap_unix_stream_connect,
	.unix_may_send = 		cap_unix_may_send,
	.socket_create = 		cap_socket_create,
	.socket_post_create = 		cap_socket_post_create,
	.socket_bind = 			cap_socket_bind,
	.socket_connect = 		cap_socket_connect,
	.socket_listen = 		cap_socket_listen,
	.socket_accept = 		cap_socket_accept,
	.socket_sendmsg = 		cap_socket_sendmsg,
	.socket_recvmsg = 		cap_socket_recvmsg,
	.socket_getsockname = 		cap_socket_getsockname,
	.socket_getpeername = 		cap_socket_getpeername,
	.socket_setsockopt = 		cap_socket_setsockopt,
	.socket_getsockopt = 		cap_socket_getsockopt,
	.socket_shutdown = 		cap_socket_shutdown,
	.socket_sock_rcv_skb = 		cap_socket_sock_rcv_skb,
	.socket_getpeersec_stream = 	cap_socket_getpeersec_stream,
	.socket_getpeersec_dgram = 	cap_socket_getpeersec_dgram,
	.sk_alloc_security = 		cap_sk_alloc_security,
	.sk_free_security = 		cap_sk_free_security,
	.sk_clone_security = 		cap_sk_clone_security,
	.sk_getsecid = 			cap_sk_getsecid,
	.sock_graft = 			cap_sock_graft,
	.inet_conn_request = 		cap_inet_conn_request,
	.inet_csk_clone = 		cap_inet_csk_clone,
	.inet_conn_established = 	cap_inet_conn_established,
	.secmark_relabel_packet = 	cap_secmark_relabel_packet,
	.secmark_refcount_inc = 	cap_secmark_refcount_inc,
	.secmark_refcount_dec = 	cap_secmark_refcount_dec,
	.req_classify_flow = 		cap_req_classify_flow,
	.tun_dev_create = 		cap_tun_dev_create,
	.tun_dev_post_create = 		cap_tun_dev_post_create,
	.tun_dev_attach = 		cap_tun_dev_attach,
#endif	/* CONFIG_SECURITY_NETWORK */ 
#ifdef CONFIG_SECURITY_NETWORK_XFRM 
	.xfrm_policy_alloc_security = 	cap_xfrm_policy_alloc_security,
	.xfrm_policy_clone_security = 	cap_xfrm_policy_clone_security,
	.xfrm_policy_free_security = 	cap_xfrm_policy_free_security,
	.xfrm_policy_delete_security = 	cap_xfrm_policy_delete_security,
	.xfrm_state_alloc_security = 	cap_xfrm_state_alloc_security,
	.xfrm_state_free_security = 	cap_xfrm_state_free_security,
	.xfrm_state_delete_security = 	cap_xfrm_state_delete_security,
	.xfrm_policy_lookup = 		cap_xfrm_policy_lookup,
	.xfrm_state_pol_flow_match = 	cap_xfrm_state_pol_flow_match,
	.xfrm_decode_session = 		cap_xfrm_decode_session,
#endif	/* CONFIG_SECURITY_NETWORK_XFRM */ 
#ifdef CONFIG_KEYS 
	.key_alloc = 			cap_key_alloc,
	.key_free = 			cap_key_free,
	.key_permission = 		cap_key_permission,
	.key_getsecurity = 		cap_key_getsecurity,
#endif	/* CONFIG_KEYS */ 
#ifdef CONFIG_AUDIT 
	.audit_rule_init = 		cap_audit_rule_init,
	.audit_rule_known = 		cap_audit_rule_known,
	.audit_rule_match = 		cap_audit_rule_match,
	.audit_rule_free = 		cap_audit_rule_free,
#endif 
};

#ifdef PRCONFIG

static void fjsec_prconfig(void)
{
	const struct module_list *p = modlist;
	struct fs_path_config *pc;
	struct accessible_area_disk_dev *accessible_areas;
	struct access_control_mmcdl_device *mmcdl_device;
	int index;
	struct read_write_access_control *rw_access_control;
	struct read_write_access_control_process *rw_access_control_process;
	struct ac_config *ac_cfg;

	printk (KERN_INFO " --------------------\n");
	printk (KERN_INFO "boot mode=<%d>\n", boot_mode);

	printk (KERN_INFO "CONFIG_SECURITY_PATH=<%d>\n", CONFIG_SECURITY_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_DISK_DEV_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_DISK_DEV_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_SYSTEM_DIR_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SYSTEM_DEV_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_SYSTEM_DEV_PATH);
#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE=<%d>\n", CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DIR_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DEV_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DEV_PATH);
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */
#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_AC_KITTING=<%d>\n", CONFIG_SECURITY_FJSEC_AC_KITTING);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_KITTING_DIR_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_KITTING_DEV_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_KITTING_DEV_PATH);
#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_FELICA_SYMLINK_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_NFC_SYMLINK_PATH);
#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SECURE_STORAGE_ACCESS_PROCESS_NAME=<%s>\n", CONFIG_SECURITY_FJSEC_SECURE_STORAGE_ACCESS_PROCESS_NAME);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SECURE_STORAGE_ACCESS_PROCESS_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_SECURE_STORAGE_ACCESS_PROCESS_PATH);
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME=<%s>\n", CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME=<%s>\n", CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME=<%s>\n", CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME=<%s>\n", CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_NAME=<%s>\n", CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_NAME);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_PATH);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_PROTECT_CHROOT=<%d>\n", CONFIG_SECURITY_FJSEC_PROTECT_CHROOT);
	printk (KERN_INFO "CONFIG_SECURITY_FJSEC_CHROOT_PATH=<%s>\n", CONFIG_SECURITY_FJSEC_CHROOT_PATH);

	printk (KERN_INFO " devs\n");
	for (pc = devs; pc->prefix; pc++) {
		printk (KERN_INFO "  <0x%08x> <%d> <%d> <0x%08x> <%s>\n", pc->mode, pc->uid, pc->gid, pc->rdev, pc->prefix);
	}

	printk (KERN_INFO " modlist\n");
	for (; p->name; p++) {
		printk (KERN_INFO "  <%s>\n", p->name);
	}

	printk (KERN_INFO " accessible area\n");
	printk (KERN_INFO "  fota mode\n");
	for (accessible_areas = accessible_areas_fota; accessible_areas->tail; accessible_areas++) {
		printk (KERN_INFO "   <0x%llx><0x%llx>\n", accessible_areas->head, accessible_areas->tail);
	}

	printk (KERN_INFO "  sddownloader mode\n");
	for (accessible_areas = accessible_areas_sddownloader; accessible_areas->tail; accessible_areas++) {
		printk (KERN_INFO "   <0x%llx><0x%llx>\n", accessible_areas->head, accessible_areas->tail);
	}

	printk (KERN_INFO "  osupdate mode\n");
	for (accessible_areas = accessible_areas_osupdate; accessible_areas->tail; accessible_areas++) {
		printk (KERN_INFO "   <0x%llx><0x%llx>\n", accessible_areas->head, accessible_areas->tail);
	}

	printk (KERN_INFO "  recovery mode\n");
	for (accessible_areas = accessible_areas_recovery; accessible_areas->tail; accessible_areas++) {
		printk (KERN_INFO "   <0x%llx><0x%llx>\n", accessible_areas->head, accessible_areas->tail);
	}

	printk (KERN_INFO "  maker mode\n");
	for (accessible_areas = accessible_areas_maker; accessible_areas->tail; accessible_areas++) {
		printk (KERN_INFO "   <0x%llx><0x%llx>\n", accessible_areas->head, accessible_areas->tail);
	}

	printk (KERN_INFO " mmcdl_device_list\n");
	for (mmcdl_device = mmcdl_device_list; mmcdl_device->process_name; mmcdl_device++) {
		printk (KERN_INFO "  <%d><%s><%s>\n", mmcdl_device->boot_mode, mmcdl_device->process_name, mmcdl_device->process_path);
	}

	printk(KERN_INFO " ptrace_read_request_policy\n");
	for (index = 0; index < ARRAY_SIZE(ptrace_read_request_policy); index++) {
		printk(KERN_INFO "  <%ld>\n", ptrace_read_request_policy[index]);
	}

	printk (KERN_INFO " rw_access_control_list\n");
	for (rw_access_control = rw_access_control_list; rw_access_control->prefix; rw_access_control++) {
		for (rw_access_control_process = rw_access_control->process_list; rw_access_control_process->process_path; rw_access_control_process++) {
			printk (KERN_INFO "  <%s><%s><%s><%d>\n", rw_access_control->prefix, rw_access_control_process->process_name, rw_access_control_process->process_path, rw_access_control_process->uid);
		}
	}

	printk (KERN_INFO " ptn_acl\n");
	for (ac_cfg = ptn_acl; ac_cfg->prefix; ac_cfg++) {
		printk (KERN_INFO "  <%s><%d><%s><%s>\n", ac_cfg->prefix, ac_cfg->boot_mode, ac_cfg->process_name, ac_cfg->process_path);
	}

	printk (KERN_INFO " fs_acl\n");
	for (ac_cfg = fs_acl; ac_cfg->prefix; ac_cfg++) {
		printk (KERN_INFO "  <%s><%d><%s><%s>\n", ac_cfg->prefix, ac_cfg->boot_mode, ac_cfg->process_name, ac_cfg->process_path);
	}

	printk (KERN_INFO " devmem_acl\n");
	for (index = 0; index < ARRAY_SIZE(devmem_acl); index++) {
		printk (KERN_INFO "  <0x%lx><0x%lx><%s><%s>\n", devmem_acl[index].head, devmem_acl[index].tail, devmem_acl[index].process_name, devmem_acl[index].process_path);
	}

	printk (KERN_INFO " --------------------\n");
}
#else
#define fjsec_prconfig()
#endif

static int __init fjsec_init (void)
{
	printk (KERN_INFO "FJSEC LSM module initialized\n");
	fjsec_prconfig();

	return 0;
}

security_initcall (fjsec_init);
