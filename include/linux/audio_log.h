/* QNET Audio:2012-03-02 Audio log driver start */
#ifndef	__AUDIO_LOG_H__
#define	__AUDIO_LOG_H__

#define KLOG_LEVEL_VERB		0
#define KLOG_LEVEL_INFO		1
#define KLOG_LEVEL_DEBUG	2
#define KLOG_LEVEL_ERROR	3

extern void audio_log_system(int level, const char *file, int line, const char *func, const char *format, ...);
#define VERB_PRINTK(format, ...)	audio_log_system(KLOG_LEVEL_VERB, __FILE__,__LINE__,__func__, format, ##__VA_ARGS__)
#define INFO_PRINTK(format, ...)	audio_log_system(KLOG_LEVEL_INFO, __FILE__,__LINE__,__func__, format, ##__VA_ARGS__)
#define DBG_PRINTK(format, ...)	audio_log_system(KLOG_LEVEL_DEBUG, __FILE__,__LINE__,__func__, format, ##__VA_ARGS__)
#define ERR_PRINTK(format, ...)	audio_log_system(KLOG_LEVEL_ERROR, __FILE__,__LINE__,__func__, format, ##__VA_ARGS__)

#ifdef pr_info
#undef pr_info
#define	pr_info(format, ...)	audio_log_system(KLOG_LEVEL_INFO, __FILE__,__LINE__,__func__, format, ##__VA_ARGS__)
#endif

#endif	//	__AUDIO_LOG_H__
/* QNET Audio:2012-03-02 Audio log driver end */
