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
#ifndef __SWITCH_LOGOUT__
#define __SWITCH_LOGOUT__

// SWITCH_LOGOUT=Invalidate
// Not operate custom_pringk.
#define SWITCH_LOGOUT
//#define POWER_ON_BENCH_LOG

// Support 32 kinds of up to log.
#define GpioLogType					0x00000001	// GPIO Dump Log.
#define Sus_Reg_BenchMarkLogType	0x00000002	// Suspend Resume Log.
#define PW_Off_BenchMarkLogType		0x00000004	// Power off Log.

#define Suspend_LogType				0x00000010	// Suspend(Callback) Log.
#define Suspend_Late_LogType		0x00000020	// Suspend Late(Callback) Log.
#define Suspend_Noirq_LogType		0x00000040	// Suspend Noirq(Callback) Log.
#define ClockLogType            	0x00000080  // Clock Log.

#define Resume_LogType				0x00000100	// Resume(Callback) Log.
#define Resume_Early_LogType		0x00000200	// Resume Early(Callback) Log.
#define Resume_Noirq_LogType		0x00000400	// Resume Noirq(Callback) Log.
#define RegulatorLogType 			0x00000800	// powercollapse Log
#ifdef SWITCH_LOGOUT

// To support 32 types, please to change the type to long.
unsigned int check_and_get_table_from_NV(void);

void switch_printk(unsigned int logtype, const char *fmt, ...);
void switch_gpio_dump(unsigned int);
void switch_gpio_dump_op(void);
void switch_powercollapse_dump(void);

// Power on log output by changing POWER_ON_BENCH_LOG.
#ifdef POWER_ON_BENCH_LOG
#define poweron_bench_printk(fmt, ...) \
			printk("[BenchLog(PW_ON)]: %s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__) 
#else
#define poweron_bench_printk(fmt, ...) \
			do { } while(0)
#endif //POWER_ON_BENCH_LOG

#else // SWITCH_LOGOUT
static inline int check_and_get_table_from_NV(void) {return 0;}
static inline void switch_printk(unsigned int logtype, const char *fmt, ...) {}
static inline void switch_gpio_dump(unsigned int detail) {}

#define poweron_bench_printk(fmt, ...) \
			do { } while(0)
#endif // SWITCH_LOGOUT

// Log change by changing NV Value.
#define suspend_bench_NV_Switch_printk(fmt, ...) 								\
({																				\
if (check_and_get_table_from_NV() & Sus_Reg_BenchMarkLogType)					\
printk("[BenchLog(SUSPEND)]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define resume_bench_NV_Switch_printk(fmt, ...) 								\
({																				\
if (check_and_get_table_from_NV() & Sus_Reg_BenchMarkLogType)					\
printk("[BenchLog(RESUME)]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define poweroff_bench_NV_Switch_printk(fmt, ...) \
({																				\
if (check_and_get_table_from_NV() & PW_Off_BenchMarkLogType)					\
printk("[BenchLog(PW_OFF)]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define gpio_NV_Swich_dump(detail) \
			switch_gpio_dump(detail)

#define suspend_NV_switch_printk(fmt, ...) 							\
({																	\
if (check_and_get_table_from_NV() & Suspend_LogType)				\
printk("[Suspend]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define suspend_late_NV_switch_printk(fmt, ...) 							\
({																			\
if (check_and_get_table_from_NV() & Suspend_Late_LogType)					\
printk("[Suspend Late]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define suspend_noirq_NV_switch_printk(fmt, ...) 							\
({																			\
if (check_and_get_table_from_NV() & Suspend_Noirq_LogType)					\
printk("[Suspend Noirq]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define resume_NV_switch_printk(fmt, ...)							\
({																	\
if (check_and_get_table_from_NV() & Resume_LogType)					\
printk("[Resume]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define resume_early_NV_switch_printk(fmt, ...)								\
({																			\
if (check_and_get_table_from_NV() & Resume_Early_LogType)					\
printk("[Resume Early]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#define resume_noirq_NV_switch_printk(fmt, ...)								\
({																			\
if (check_and_get_table_from_NV() & Resume_Noirq_LogType)					\
printk("[Resume Noirq]:%s(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
})

#endif // __SWITCH_LOGOUT__
