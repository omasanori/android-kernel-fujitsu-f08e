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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cryptohash.h>
#include "../arch/arm/mach-msm/include/mach/msm_smsm.h"
#include <linux/nonvolatile_common.h>

/*-----*/
MODULE_DESCRIPTION("Fujitsu Debug USB Driver");
MODULE_LICENSE("GPL");

/*-----*/
static int debugusb_set_password(const char *val, struct kernel_param *kp);
static int debugusb_get_password(char *buffer, struct kernel_param *kp);
static int debugusb_get_boot_pid(char *buffer, struct kernel_param *kp);

#define MAX_STRING_LEN  65
static char password_string[MAX_STRING_LEN] = {0};
#ifdef ENABLE_PWSTRING_IF
static unsigned int desired_password[5] = {0x2543405f,0x8bd90dc7,0x5de9ec18,0x9f8102c1,0x8d8e8efd};
#endif // ENABLE_PWSTRING_IF
static unsigned int output_password[5] = {0}; 
static struct kparam_string kps_pass = {
	.maxlen = MAX_STRING_LEN,
	.string = password_string
};

module_param_call(password_string, debugusb_set_password, debugusb_get_password, &kps_pass, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);	//0644
MODULE_PARM_DESC(password_string, "Password string");

static int boot_pid = 0;
module_param_call(boot_pid, NULL, debugusb_get_boot_pid, &boot_pid, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);	//0644
MODULE_PARM_DESC(boot_pid, "USB debug boot pid");

#define DEBUGUSB_STATE_OK  0
#define DEBUGUSB_STATE_PASS_NG 11

/* NV Item ID */
#define APNV_OPEFLAG_I  41039
/* NV MasterClear Flag */

/* FUJITSU:2013-01-16 USB mod start */
#define APNV_OPEFLAG_ON    1
static unsigned int usb_id_debug = 0;
static int usb_id_debug_store(const char *val, struct kernel_param *kp);
static int usb_id_debug_show(char *buffer, struct kernel_param *kp);
module_param_call(usb_id_debug, usb_id_debug_store,
		usb_id_debug_show, &usb_id_debug, S_IRUGO | S_IWUSR);//0644
MODULE_PARM_DESC(usb_id_debug, "USB Debug Port ID");

static unsigned int usb_id_om = 0;
static int usb_id_om_store(const char *val, struct kernel_param *kp);
static int usb_id_om_show(char *buffer, struct kernel_param *kp);
module_param_call(usb_id_om, usb_id_om_store,
		usb_id_om_show, &usb_id_om, S_IRUGO | S_IWUSR);//0644
MODULE_PARM_DESC(usb_id_om, "USB OM Port ID");
/* FUJITSU:2013-01-16 USB mod start */

unsigned long debugusb_get_pid_from_nv(void);
static int debugusb_set_nv(unsigned long pid);

static int verify_pass = DEBUGUSB_STATE_OK;
module_param(verify_pass, int, S_IRUGO);//0444
MODULE_PARM_DESC(verify_pass, "USB debug correctnesses of pass");

int debugusb_set_pid(unsigned long pid);

/* FUJITSU:2013-01-16 USB add start */
bool debugusb_is_om(unsigned int pid);
/* FUJITSU:2013-01-16 USB add end */

/* FUJITSU:2013-01-16 USB mod start */
unsigned long debugusb_get_pid_from_nv(void)
{
	unsigned long pid	= 0;
	int ret = 0;
	unsigned char buf;

pr_info("[gadget/debug_usb.c] %s\n", __func__);

	ret = get_nonvolatile((void*)&buf, APNV_OPEFLAG_I, 1);
	if(0 >= ret)
	{
		pr_info("%s:ERROR get_nonvolatile() OPEFLAG : ret=[%d]\n", __func__,ret);
		pid = 0;
		goto out;
	}
	switch (buf) {
	case	APNV_OPEFLAG_ON:
		pid = APNV_OPEFLAG_ON;
		break;
	default:	/* other Flag */
		break;
	}
out:
pr_info("[gadget/debug_usb.c] %s pid=0x%lx nv_ope=0x%x\n", __func__,pid,buf);
	return pid;
}

static int debugusb_set_nv(unsigned long pid)
{
	int	ret = 0;
	unsigned char	nv_buf[1];
	#ifdef ENABLE_PWSTRING_IF
	unsigned int	buffer [5]	= {0};
	int	loop	= 0;
	#endif // ENABLE_PWSTRING_IF

pr_info("[gadget/debug_usb.c] %s pid=0x%lx\n", __func__,pid);

	if (pid == usb_id_debug) {
		nv_buf[0] = 0x01;
pr_info("[gadget/debug_usb.c] %s DEBUG nv_ope=[1]\n", __func__);
	} else {
		nv_buf[0] = 0x00;
pr_info("[gadget/debug_usb.c] %s NORMAL nv_ope=[0]\n", __func__);
	}


#ifdef ENABLE_PWSTRING_IF
	debugusb_get_password((char *)buffer, kp);
	for (loop =0; loop<5; loop ++) {
		if( buffer[loop] != desired_password[loop] ) {
			goto out;
		}
	}
#endif // ENABLE_PWSTRING_IF
	ret = set_nonvolatile( nv_buf, APNV_OPEFLAG_I, 1 );
	if (ret <= 0) {
		goto out;
	}

	ret = 1;	/* suceeded */

out:
pr_info("[gadget/debug_usb.c] %s ret=%04x\n", __func__,ret);
	return ret;
}
/* FUJITSU:2013-01-16 USB mod end */

static void debugusb_set_verifypass(bool result){
	if(result){
		verify_pass = DEBUGUSB_STATE_OK;
	}else{
		verify_pass = DEBUGUSB_STATE_PASS_NG;
	}
}

int debugusb_certify_portchange(unsigned int pid)
{
#ifdef ENABLE_PWSTRING_IF
	int ret = 0;
	
/* FUJITSU:2013-01-16 USB mod start */
	if (pid == usb_id_debug) {
		ret = g_certify;
	} else {
		ret = 1;
	}
	return ret;
/* FUJITSU:2013-01-16 USB mod end */
#else
	return 1;
#endif
}
EXPORT_SYMBOL(debugusb_certify_portchange);

static void debugusb_hash_password(const char *kmessage)
{
	unsigned int hash[5], workspace[SHA_WORKSPACE_WORDS];
	char local_buf[MAX_STRING_LEN] = {0};
	int i =0;

	strlcpy(local_buf, kmessage, MAX_STRING_LEN);
	local_buf[64] = '\0';
	sha_init(hash);
	sha_transform(hash, (unsigned char *)local_buf, workspace);
	for (i = 0; i < 5; i++) {
		output_password[i]= hash[i];
	}
}

static int debugusb_set_password(const char *kmessage, struct kernel_param *kp)
{
	int len = 0;
	int i =0;
	#ifdef ENABLE_PWSTRING_IF
	unsigned int	buffer [5] 	= {0};	
	int 		loop 		= 0;
	#endif // ENABLE_PWSTRING_IF
	
	pr_info("[gadget/debug_usb.c] %s\n", __func__);
	debugusb_set_verifypass(false);
	if(kmessage) {
		len = strlen(kmessage);
	}
	else {
		for(i=0;i<5;i++) {
			output_password[i] = 0;
		}
		return 0;
	}
	
	if (len > 64) {
		return -ENOSPC;
	}

	debugusb_hash_password(kmessage);
	#ifdef ENABLE_PWSTRING_IF
	debugusb_get_password((char *)buffer, kp);
	for (loop =0; loop<5; loop ++) {
		if( buffer[loop] != desired_password[loop] ) {
			return = -EINVAL;
		}
	}
	g_certify = 1;
	#endif // ENABLE_PWSTRING_IF

	debugusb_set_verifypass(true);
	
	return 0;
}

static int debugusb_get_password(char *buffer, struct kernel_param *kp)
{
	int ret = 0;
	int i =0;
	unsigned int *sv;

	sv = (unsigned int *)buffer;
	pr_info("[gadget/debug_usb.c] %s\n", __func__);
	for(i = 0 ; i < 5 ; i++) {
		sv[i] = output_password[i];
		output_password[i] = 0;
	}
	return ret;
}

static int debugusb_get_boot_pid(char *buffer, struct kernel_param *kp)
{
	pr_info("%s : %x\n", __func__, boot_pid);
	return sprintf(buffer, "%x", boot_pid);
}

int debugusb_set_pid(unsigned long pid)
{
	int ret = 0;
pr_info("[gadget/debug_usb.c] %s\n", __func__);


	if ((debugusb_set_nv(pid)) != 1) {
		pr_info("[gadget/debug_usb.c]%s:invalid combination %lx\n",__func__,pid);
		ret = -EINVAL;
		goto out;
	}

out:
pr_info("[gadget/debug_usb.c] %s ret=0x%x\n", __func__,ret);
	return ret;
}
EXPORT_SYMBOL(debugusb_set_pid);

/* FUJITSU:2013-01-16 USB add start */

static int usb_id_debug_store(const char *val, struct kernel_param *kp)
{
	unsigned int value;
	
	if (sscanf(val, "%04x\n", &value) == 1) {
		usb_id_debug = value;
pr_info("%s : set D-PID succeeded = 0x%x\n", __func__, usb_id_debug);
		return 0;
	}
pr_info("%s : set D-PID failed = 0x%x\n", __func__, usb_id_debug);
	return -1;
}

static int usb_id_debug_show(char *buffer, struct kernel_param *kp)
{
	return sprintf(buffer, "%04x\n", usb_id_debug);
}

static int usb_id_om_store(const char *val, struct kernel_param *kp)
{
	unsigned int value;

	if (sscanf(val, "%04x\n", &value) == 1) {
		usb_id_om = value;
pr_info("%s : set OM-PID succeeded = 0x%x\n", __func__, usb_id_om);
		return 0;
	}
pr_info("%s : set OM-PID failed = 0x%x\n", __func__, usb_id_om);
	return -1;
}

static int usb_id_om_show(char *buffer, struct kernel_param *kp)
{
	return sprintf(buffer, "%04x\n", usb_id_om);
}

bool debugusb_is_om(unsigned int pid)
{
	return (pid == usb_id_om);
}
EXPORT_SYMBOL(debugusb_is_om);
/* FUJITSU:2013-01-16 USB add end */

static int __init debugusb_init(void)
{
	boot_pid = 0;
	boot_pid = debugusb_get_pid_from_nv();

/* FUJITSU:2013-01-16 USB mod start */
	pr_info("%s : debugusb_get_pid_from_nv() = %x\n", __func__, boot_pid);
/* FUJITSU:2013-01-16 USB mod end */
	return 0;
}
module_init(debugusb_init);

static void __exit debugusb_deinit(void)
{
	return;
}
module_exit(debugusb_deinit);
