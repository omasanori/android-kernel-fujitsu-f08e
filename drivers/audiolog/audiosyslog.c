/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011-2012
/*----------------------------------------------------------------------------*/
/* QNET Audio:2012-03-02 Audio log driver start */

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/timer.h> 
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/timer.h> 
#include <linux/device.h>
#include <linux/interrupt.h>
#include <stdarg.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <linux/audio_log.h>

#include "audiosyslog.h"

/**********************************************************************/
/* macros															  */
/**********************************************************************/
#define FILECHECK_INTERVAL_TIME		5	/* 5sec */
#define	CHECK_COUNTER_LIMIT			5	/* 10times*/

#define	KHEAP_MAX					(1024*30)
/**********************************************************************/
/* prototypes		 												  */
/**********************************************************************/
static void audio_log_func_workqueue(struct work_struct *work);
static void logflag_check_timer_handler( unsigned long data );
void audio_log_system(int level, const char *file, int line, const char *func, const char *format, ...);
static void audio_log_set_outout(void);
static int audio_log_open(struct inode *inode, struct file *filp);

/**********************************************************************/
/* globals															  */
/**********************************************************************/

static int	log_wakeup_flag=0;
static int	filecheck_counter=0;

static char *s_logmem=0;
static int s_logsize=0;

static char option[2048];
static int markcnt=0;
static char *markpnt[300];
static char marktype[300];

#define	LEVEL_VERB		0x0001		//KLOG_LEVEL_VERB
#define	LEVEL_INFO		0x0002		//KLOG_LEVEL_INFO
#define	LEVEL_DEBUG		0x0004		//KLOG_LEVEL_DEBUG
#define	LEVEL_ERROR		0x0008		//KLOG_LEVEL_ERROR

#define	DETER_FILE_PATH	0x0100		//file path
#define	DETER_FILE_NAME	0x0200		//file name
#define	DETER_FILE_LINE	0x0400		//file line
#define	DETER_FUNC_NAME	0x0800		//function
#define	DETER_MESSAGE	0x1000		//message

static int g_level =0;

#define	LEVEL_MARK		"LEVEL="
#define	DETER_MARK		"DETERRENCE="

#define	DET_PATH		"@DET_PATH"
#define	DET_FILE		"@DET_FILE"
#define	DET_FUNC		"@DET_FUNC"
#define	KEYWORD			"@KEYWORD"
#define	FILE_END		"@END"

static struct timer_list logflag_check_timer;
static struct work_struct workq;


/**********************************************************************
 * 
 */
void audio_log_system(int loglevel, const char *file, int line, const char *func, const char *format, ...)
{
	va_list ap;
	int len,i;
	char *pnt;
	char *path_pointer;
	char *func_pointer;
	char *mesg_pointer;
	char *pathp;
	char mem[500];
	char name[500];

	if (log_wakeup_flag==2)
	{
		if (loglevel == KLOG_LEVEL_ERROR)
		{
			va_start(ap, format);
			vsprintf(name, format, ap);
			va_end(ap);

			printk(KERN_ERR "%s", mem);
		}
		return;
	}


	// LEVEL CHECK
	if (loglevel == KLOG_LEVEL_VERB && (g_level & LEVEL_VERB))	return;
	if (loglevel == KLOG_LEVEL_INFO && (g_level & LEVEL_INFO))	return;
	if (loglevel == KLOG_LEVEL_DEBUG && (g_level & LEVEL_DEBUG))	return;
	if (loglevel == KLOG_LEVEL_ERROR && (g_level & LEVEL_ERROR))	return;

	va_start(ap, format);
	vsprintf(name, format, ap);
	va_end(ap);
	// DETERRENCE CHECK
	for (i=0; i<markcnt; i++)
	{
		switch(marktype[i])
		{
			case 1:	// @DET_PATH
				if (strstr(file, markpnt[i]))	return;
				break;
			case 2:	// @DET_FILE
				if (strstr(file, markpnt[i]))	return;
				break;
			case 3:	// @DET_FUNC
				if (strstr(func, markpnt[i]))	return;
				break;
			case 4:	// @KEYWORD
				if (strstr(name, markpnt[i]))	return;
				break;
			default:
				break;
		}
	}
	pnt = name;
	while(*pnt)
	{
		if (*pnt == '\n')
		{
			*pnt = ' ';
		}
		pnt++;
	}
	*pnt++ = '\n';
	*pnt++ = 0;

	path_pointer = (char *)file;
	func_pointer = (char *)func;
	mesg_pointer = (char *)name;

	if (g_level & DETER_FILE_PATH)
	{
		for (;;)
		{
			pathp = strstr(path_pointer, "/");
			if (!pathp)
			{
				break;
			}
			path_pointer = pathp+1;
		}
		if (g_level & DETER_FILE_NAME)
		{
			path_pointer="";
		}
	}
	if (g_level & DETER_FUNC_NAME)	func_pointer="";
	if (g_level & DETER_MESSAGE)	mesg_pointer="";

	if (log_wakeup_flag && g_level & DETER_FILE_LINE)
	{
		sprintf(mem,"%s: %s: %s", (char *)path_pointer, func_pointer, mesg_pointer);
	}
	else
	{
		sprintf(mem,"%s(%d) %s: %s", (char *)path_pointer, line, func_pointer, mesg_pointer);
	}
	if (log_wakeup_flag == 0)
	{
		if (s_logsize < KHEAP_MAX)
		{
			len = strlen(mem);
			s_logmem = krealloc(s_logmem, s_logsize + len + 2, GFP_KERNEL);
			if (s_logmem)
			{
				strcpy(&s_logmem[s_logsize], mem);
				s_logsize += len+1;
				s_logmem[s_logsize]=0;		// 
			}
		}
	}
	else
	{
			printk(KERN_DEBUG "%s", mem);
	}
}
EXPORT_SYMBOL_GPL(audio_log_system);

/**********************************************************************
 * 
 */

int  audio_log_status(void)
{
	return(log_wakeup_flag);
}
EXPORT_SYMBOL_GPL(audio_log_status);

/**********************************************************************
 * 
 * 
 */
static void audio_log_func_workqueue(struct work_struct *work)
{
	struct file *file;
	mm_segment_t fs;
	int nr_read;
	int err;
	char *pnt;
	char *linep;
	char *strpnt;
	char *maker_line;
	int i;
	char maker_type=0;


	if (log_wakeup_flag)
	{
		return;
	}

	printk(KERN_DEBUG "audio_log_func_workqueue start\n");


	file = filp_open("/data/audiolog", O_RDONLY , 0);
	err = IS_ERR(file);
	if (err) 
	{
		if (filecheck_counter >= CHECK_COUNTER_LIMIT)
		{
			log_wakeup_flag = 2;
			if (s_logmem)
			{
				kfree(s_logmem);
				s_logmem=0;
				s_logsize=0;
			}
			return;
		}
		filecheck_counter++;

		init_timer( &logflag_check_timer );
		logflag_check_timer.expires  = jiffies + (HZ * FILECHECK_INTERVAL_TIME);
		logflag_check_timer.data     = ( unsigned long )jiffies;
		logflag_check_timer.function = logflag_check_timer_handler;
		add_timer( &logflag_check_timer );
		return;
	}

	printk(KERN_DEBUG "filp_read\n");
	fs = get_fs();
	set_fs(get_ds());

	nr_read = file->f_op->read(file, option, sizeof(option)-1, &file->f_pos);
	option[nr_read] = '\0';
	filp_close(file,NULL);
	if (!nr_read)
	{
		return;
	}
	g_level=0;
	pnt = option;
	while(*pnt)
	{
		linep = pnt;
		while(*linep >= 0x20)	linep++;
		while(*linep && *linep < 0x20)	*linep++ = 0;
		if (*pnt==0 || *pnt == '#')
		{
			pnt = linep;
			continue;
		}
		maker_line=pnt;
		pnt = linep;

		// #'I'=Info / 'D'=Debug / 'V'=Verbose 'E'=Error
		strpnt = strstr(maker_line, LEVEL_MARK);
		if (strpnt)
		{
			strpnt += strlen(LEVEL_MARK);
			while(*strpnt >=0x20)
			{
				switch(*strpnt)
				{
					case 'I':
						g_level |= LEVEL_INFO;
						break;
					case 'D':
						g_level |= LEVEL_DEBUG;
						break;
					case 'V':
						g_level |= LEVEL_VERB;
						break;
					case 'E':
						g_level |= LEVEL_ERROR;
						break;
					default:
						break;
				}
				strpnt++;
			}
			continue;
		}

		// # 'P'=Filepass / 'L'=Line / 'F'=FunctionName / 'M'=Message
		strpnt = strstr(maker_line, DETER_MARK);
		if (strpnt)
		{
			strpnt += strlen(DETER_MARK);
			while(*strpnt >=0x20)
			{
				switch(*strpnt)
				{
					case 'P':
						g_level |= DETER_FILE_PATH;
						break;
					case 'F':
						g_level |= DETER_FILE_NAME;
						break;
					case 'N':
						g_level |= DETER_FUNC_NAME;
						break;
					case 'I':
						g_level |= DETER_FILE_LINE;
						break;
					case 'M':
						g_level |= DETER_MESSAGE;
						break;
					default:
						break;
				}
				strpnt++;
			}
			continue;
		}

		if (strstr(maker_line, DET_PATH))
		{
			maker_type = 1;
			continue;
		}
		if (strstr(maker_line, DET_FILE))
		{
			maker_type = 2;
			continue;
		}
		if (strstr(maker_line, DET_FUNC))
		{
			maker_type = 3;
			continue;
		}
		if (strstr(maker_line, KEYWORD))
		{
			maker_type = 4;
			continue;
		}
		if (strstr(maker_line, FILE_END))
		{
			break;
		}
		marktype[markcnt]= maker_type;
		markpnt[markcnt++] = maker_line;
	}
	printk("AudioLog Level =%08x \n", g_level);
	for (i=0; i<markcnt; i++)
	{
		printk("Audio log Config =[%d] %s \n", marktype[i], (char *)markpnt[i]);
	}
    audio_log_set_outout();
}

/**********************************************************************
 * 
 */
static void audio_log_set_outout(void)
{
	log_wakeup_flag = 1;
	if (s_logmem)
	{
		int pnt=0;

		printk(KERN_DEBUG "audio log buffer flash start");
		while(s_logmem[pnt])
		{
			printk(KERN_DEBUG "%s", &s_logmem[pnt]);
			pnt += strlen(&s_logmem[pnt])+1;		// strings + null
		}
		kfree(s_logmem);
		s_logmem=0;
		s_logsize=0;
		printk(KERN_DEBUG "audio log buffer flash end");
	}
}

/**********************************************************************
 * 
 */
static void logflag_check_timer_handler( unsigned long data )
{
	schedule_work(&workq);
}

/**********************************************************************
 * 
 */
static int audio_log_open(struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "%s()\n", __func__);
	audio_log_func_workqueue(NULL);
	audio_log_set_outout();
	return 0;
}

//**********************************************************************
static struct file_operations audiolog_fops = {
	.open			= audio_log_open,				/* open Entry */
};

/* driver definition */
static struct miscdevice audiolog_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,			/* auto */
	.name = "audiolog",						/* Driver name */
	.fops = &audiolog_fops,					/* FOPS */
};

/**********************************************************************
 *	audio_log_init
 *
 *	Initialize driver<br>
 *	register misc_driver
 *	Setpu GPIO configration
 *
 *	@return	0	success<br>
 *			<0	fail
 */
static int __init audio_log_init(void)
{
	int ret;
	
	printk(KERN_DEBUG "audio_log_init start\n");

	INIT_WORK(&workq, audio_log_func_workqueue);

	memset(option, 0, sizeof(option));
	memset(marktype, 0, sizeof(marktype));
	ret = misc_register(&audiolog_miscdev);
	if (ret) {
		printk(KERN_ERR "register miscdev: %d\n", ret);
		return ret;
	}

	init_timer( &logflag_check_timer );
	logflag_check_timer.expires  = jiffies + (HZ * FILECHECK_INTERVAL_TIME);
	logflag_check_timer.data     = ( unsigned long )jiffies;
	logflag_check_timer.function = logflag_check_timer_handler;
	add_timer( &logflag_check_timer );
	return 0;
}
module_init(audio_log_init);

/********************************************************************
 *	edc_exit
 *
 *	finalize driver
 *	deregister misc_driver
 */
static void __exit audio_log_exit(void)
{
}
module_exit(audio_log_exit);

MODULE_DESCRIPTION("Audio log system Driver");
MODULE_AUTHOR("Fujitsu");
MODULE_LICENSE("GPL");

/* QNET Audio:2012-03-02 Audio log driver end */
