/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wait.h>

#include <linux/msm_audio.h>

bool speakerrecv_pl_flg = false;

static long speakerrecv_pl_ioctl(struct file *file,
				 unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case AUDIO_SET_CONFIG:
		speakerrecv_pl_flg = true;
		break;
	}
	return 0;
}

static int speakerrecv_pl_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int speakerrecv_pl_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations speakerrecv_pl_fops = {
	.owner		= THIS_MODULE,
	.open		= speakerrecv_pl_open,
	.release	= speakerrecv_pl_release,
	.unlocked_ioctl	= speakerrecv_pl_ioctl,
};

struct miscdevice speakerrecv_pl_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_snd_speakerrecv",
	.fops	= &speakerrecv_pl_fops,
};

static int __init speakerrecv_pl_init(void)
{
	return misc_register(&speakerrecv_pl_misc);
}

device_initcall(speakerrecv_pl_init);
