/*
 *  drivers/cpufreq/cpufreq_ondemand.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
/* FUJITSU:2013-01-30 boostup start */
#include <linux/spinlock.h>
/* FUJITSU:2013-01-30 boostup end */

/* FUJITSU:2013-01-30 resume boost add start */
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
/* FUJITSU:2013-01-30 resume boost add end */

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)
#define MIN_FREQUENCY_DOWN_DIFFERENTIAL		(1)

/* FUJITSU:2013-04-19 table ondemand mod start */
#define MAX_BOOST_FREQ	(1728000)
#define HIGH_PERF_INPUT_BOOST_FREQ   (1350000)
#define NORMAL_PERF_INPUT_BOOST_FREQ (1026000)

#define TYPE_NUM 8
#define LEVEL_NUM 14
static int threshold_table[TYPE_NUM][LEVEL_NUM][LEVEL_NUM-1] = {
		{/* Type=0 */
         /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{ 89,  95,  95, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  384 */
			{ 40,  87,  87, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{  0,  41,  72, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{  0,   0,  43,  90,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  702 */
			{  0,   0,   0,  52,  90,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{  0,   0,   0,  46,  53,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{  0,   0,   0,   0,   0,  61,  95, 150, 150, 150, 150, 150, 150 },	/* 1026 */
			{  0,   0,   0,   0,   0,  55,  61,  95, 150, 150, 150, 150, 150 },	/* 1134 */
			{  0,   0,   0,   0,   0,  50,  56,  62,  95, 150, 150, 150, 150 },	/* 1242 */
			{  0,   0,   0,   0,   0,  46,  51,  57,  62,  95, 150, 150, 150 },	/* 1350 */
			{  0,   0,   0,   0,   0,  43,  48,  53,  58,  63,  95, 150, 150 },	/* 1458 */
			{  0,   0,   0,   0,   0,  41,  46,  51,  56,  61,  65,  95, 150 },	/* 1566 */
			{  0,   0,   0,   0,   0,  38,  42,  48,  53,  57,  61,  65,  95 },	/* 1674 */
			{  0,   0,   0,   0,   0,  36,  40,  45,  50,  55,  58,  62,  65 },	/* 1728 */
		},
		{/* Type=1 */
         /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{ 80, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  384 */
			{ 40,  80, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{ 40,  50,  80, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{ 40,  50,  55,  80, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  702 */
			{ 30,  40,  50,  60,  99, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{ 33,  42,  52,  61,  71,  99, 150, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{ 30,  38,  46,  55,  63,  72,  99, 150, 150, 150, 150, 150, 150 },	/* 1026 */
			{ 27,  34,  42,  50,  57,  65,  73,  99, 150, 150, 150, 150, 150 },	/* 1134 */
			{ 25,  31,  38,  45,  52,  59,  66,  73,  99, 150, 150, 150, 150 },	/* 1242 */
			{ 23,  29,  35,  42,  48,  55,  61,  68,  74,  99, 150, 150, 150 },	/* 1350 */
			{ 21,  27,  33,  39,  45,  51,  57,  63,  69,  75,  99, 150, 150 },	/* 1458 */
			{ 20,  26,  31,  37,  43,  49,  54,  60,  66,  72,  78,  99, 150 },	/* 1566 */
			{ 19,  24,  29,  35,  40,  45,  50,  56,  62,  68,  75,  78,  99 },	/* 1674 */
			{ 18,  23,  28,  33,  38,  42,  46,  53,  57,  65,  72,  75,  78 },	/* 1728 */

		},
		{/* Type=2 */
         /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{ 89,  95,  95, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  384 */
			{ 40,  87,  87, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{  0,  41,  72, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{  0,   0,  43,  90,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  702 */
			{  0,   0,   0,  52,  90,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{  0,   0,   0,  46,  53,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{  0,   0,   0,   0,   0,  61,  95, 150, 150, 150, 150, 150, 150 },	/* 1026 */
			{  0,   0,   0,   0,   0,  55,  61,  95, 150, 150, 150, 150, 150 },	/* 1134 */
			{  0,   0,   0,   0,   0,  50,  56,  62,  95, 150, 150, 150, 150 },	/* 1242 */
			{  0,   0,   0,   0,   0,  46,  51,  57,  62,  95, 150, 150, 150 },	/* 1350 */
			{  0,   0,   0,   0,   0,  43,  48,  53,  58,  63,  95,  95, 150 },	/* 1458 */
			{  0,   0,   0,   0,   0,  41,  46,  51,  56,  61,  65,  65,  95 },	/* 1566 */
			{  0,   0,   0,   0,   0,  38,  42,  48,  53,  57,  61,  65,  95 },	/* 1674 */
			{  0,   0,   0,   0,   0,  36,  40,  45,  50,  55,  58,  62,  65 },	/* 1728 */
		},
		{/* BoostType=0, Type=3 Activity launch boost table   lower limit clock 1.1GHz*/
         /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/*  384 */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/*  702 */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{  0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150, 150, 150 },	/* 1026 */
			{  0,   0,   0,   0,   0,   0,   0,  95, 150, 150, 150, 150, 150 },	/* 1134 */
			{  0,   0,   0,   0,   0,   0,   0,  62,  95, 150, 150, 150, 150 },	/* 1242 */
			{  0,   0,   0,   0,   0,   0,   0,  57,  62,  95, 150, 150, 150 },	/* 1350 */
			{  0,   0,   0,   0,   0,   0,   0,  53,  58,  63,  95,  95, 150 },	/* 1458 */
			{  0,   0,   0,   0,   0,   0,   0,  51,  56,  61,  65,  65,  95 },	/* 1566 */
			{  0,   0,   0,   0,   0,   0,   0,  48,  53,  57,  61,  65,  95 },	/* 1674 */
			{  0,   0,   0,   0,   0,   0,   0,  45,  50,  55,  58,  62,  65 },	/* 1728 */
		},
		{/* BoostType=1, Type=4, InputType=1 Input boost table */
	     /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{ 90,  95,  95,  95,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  384 */
			{ 38,  90,  95,  95,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{  0,  39,  90,  95,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{  0,   0,  41,  90,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  702 */
			{  0,   0,   0,  42,  90,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{  0,   0,   0,   0,  42,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{  0,   0,   0,   0,   0,  61,  95, 150, 150, 150, 150, 150, 150 },	/* 1026 */
			{  0,   0,   0,   0,   0,  55,  61,  95, 150, 150, 150, 150, 150 },	/* 1134 */
			{  0,   0,   0,   0,   0,  50,  56,  62,  95, 150, 150, 150, 150 },	/* 1242 */
			{  0,   0,   0,   0,   0,  46,  51,  57,  62,  95, 150, 150, 150 },	/* 1350 */
			{  0,   0,   0,   0,   0,  43,  48,  53,  58,  63,  95, 150, 150 },	/* 1458 */
			{  0,   0,   0,   0,   0,  41,  46,  51,  56,  61,  65,  95, 150 },	/* 1566 */
			{  0,   0,   0,   0,   0,  38,  43,  48,  53,  57,  61,  65,  95 },	/* 1674 */
			{  0,   0,   0,   0,   0,  36,  41,  45,  50,  54,  58,  61,  65 },	/* 1728 */

		},
		{/* BoostType=2, Type=5 Late resume boost table */
	     /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{ 90,  95,  95,  95,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  384 */
			{ 38,  90,  95,  95,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{  0,  39,  90,  95,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{  0,   0,  41,  90,  95,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  702 */
			{  0,   0,   0,  42,  90,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{  0,   0,   0,   0,  42,  95, 150, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{  0,   0,   0,   0,   0,  61,  95, 150, 150, 150, 150, 150, 150 },	/* 1026 */
			{  0,   0,   0,   0,   0,  55,  61,  95, 150, 150, 150, 150, 150 },	/* 1134 */
			{  0,   0,   0,   0,   0,  50,  56,  62,  95, 150, 150, 150, 150 },	/* 1242 */
			{  0,   0,   0,   0,   0,  46,  51,  57,  62,  95, 150, 150, 150 },	/* 1350 */
			{  0,   0,   0,   0,   0,  43,  48,  53,  58,  63,  95, 150, 150 },	/* 1458 */
			{  0,   0,   0,   0,   0,  41,  46,  51,  56,  61,  65,  95, 150 },	/* 1566 */
			{  0,   0,   0,   0,   0,  38,  43,  48,  53,  57,  61,  65,  95 },	/* 1674 */
			{  0,   0,   0,   0,   0,  36,  41,  45,  50,  54,  58,  61,  65 },	/* 1728 */
		},
		{/* BoostType=3, Type=6, InputType=2 High performance input boost table    lower limit clock 1.3GHz*/
	     /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/*  384 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/*  486 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/*  594 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/*  702 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/*  810 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/*  918 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/* 1026 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/* 1134 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0, 150, 150, 150, 150 },	/* 1242 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0,  95, 150, 150, 150 },	/* 1350 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0,  63,  95, 150, 150 },	/* 1458 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0,  61,  65,  95, 150 },	/* 1566 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0,  57,  61,  65,  95 },	/* 1674 */
			{  0,   0,   0,   0,   0,   0,   0,   0,   0,  54,  58,  61,  65 },	/* 1728 */
		},
		{/*  BoostType=4, Type=7, InputType=3 High performance input boost table median 1.3GHz*/
	     /*  486  594  702  810  918 1026 1134 1242 1350 1458 1566 1674 1728        (kHz) */
			{ 70,  70,  70,  70,  70,  70,  70,  70,  70,  95,  95, 150, 150 },	/*  384 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  486 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  594 */
			{ 33,  33,  33,  70,  70,  70,  70,  70,  70,  95,  95, 150, 150 },	/*  702 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  810 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/*  918 */
			{ 22,  22,  22,  41,  41,  41,  70,  70,  70,  85,  85,  92,  92 },	/* 1026 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/* 1134 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/* 1242 */
			{ 17,  17,  17,  31,  31,  31,  46,  46,  46,  67,  67,  73,  73 },	/* 1350 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/* 1458 */
			{ 15,  15,  15,  27,  27,  27,  39,  39,  39,  52,  52,  65,  65 },	/* 1566 */
			{150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150 },	/* 1674 */
			{ 13,  13,  13,  24,  24,  24,  36,  36,  36,  47,  47,  54,  54 },	/* 1728 */
		},
};
static int loadavr_table[LEVEL_NUM][LEVEL_NUM];
static int sampling_rate_table[TYPE_NUM][LEVEL_NUM] = {
	/*  384    486    594    702    810    918   1026   1134   1242   1350   1458   1566   1674   1728 */
	{ 80000, 80000, 80000, 80000, 80000, 80000, 80000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 90000, 90000, 90000, 90000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 80000, 80000, 80000, 80000, 80000, 80000, 80000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 75000, 75000, 75000, 75000, 75000, 75000, 75000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 75000, 75000, 75000, 75000, 75000, 75000, 75000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 75000, 75000, 75000, 75000, 75000, 75000, 75000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 75000, 75000, 75000, 75000, 75000, 75000, 75000, 50000, 50000, 50000, 50000, 50000, 50000, 50000 },
	{ 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000 },
};

#define CPUFREQ_ONDEMAND_TYPE_BOOST_BASE 3
#define CPUFREQ_ONDEMAND_TYPE_BOOST_NUM 5
#define CPUFREQ_ONDEMAND_BOOST_TYPE_MAX (CPUFREQ_ONDEMAND_TYPE_BOOST_NUM -1)
#define CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT  1
#define CPUFREQ_ONDEMAND_BOOST_TYPE_RESUME 2
#define CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT2 3
#define CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT3 4

enum INPUT_BOOST_ID{
	INPUT_BOOST_NORMAL = 0,		/* Normal */
	INPUT_BOOST_RESERVE,		/* Reserve, Not Use */
	INPUT_BOOST_HIGH,			/* High */
	INPUT_BOOST_LONG};			/* Long */

typedef struct boost_type_inf_st {
	unsigned long type;
	unsigned long priority;
	unsigned long         duration;   /* Jiffies (10msec) */
	unsigned long         boost_freq[NR_CPUS];
	unsigned long         timeout;
	bool         request_b;
/* FUJITSU:2013-03-29 core boost start */
    int          up_cores;
    int          core_cap_duration;   /* Jiffies (10msec) */
/* FUJITSU:2013-03-29 core boost end */
}boost_type_inf;

/* FUJITSU:2013-03-29 core boost start */
static struct boost_inf_st{
	int		type_tmp;
	int		boost_type_cur;
	
	boost_type_inf  type_inf[CPUFREQ_ONDEMAND_TYPE_BOOST_NUM];
}boost_inf_ins ={
	.type_tmp = -1,
	.boost_type_cur = -1,
	.type_inf = 
	{
		{/* Activity launch boost */
			.type = 3,
			.priority = 3,
			.duration = 150, 
			.boost_freq = {MAX_BOOST_FREQ,MAX_BOOST_FREQ,MAX_BOOST_FREQ,MAX_BOOST_FREQ},
			.timeout = 0,
			.request_b = false,
            .up_cores = 2,
            .core_cap_duration = 150,
		},
		{/* Input boost */
			.type = 4,
			.priority = 0,
			.duration = 250,
			.boost_freq = {NORMAL_PERF_INPUT_BOOST_FREQ,NORMAL_PERF_INPUT_BOOST_FREQ,NORMAL_PERF_INPUT_BOOST_FREQ,NORMAL_PERF_INPUT_BOOST_FREQ},
			.timeout = 0,
			.request_b = false,
            .up_cores = 0,
            .core_cap_duration = 0,
		},
		{/* Late resume boost */
			.type = 5,
			.priority = 2,
			.duration = 500,
			.boost_freq = {MAX_BOOST_FREQ,MAX_BOOST_FREQ,MAX_BOOST_FREQ,MAX_BOOST_FREQ},
			.timeout = 0,
			.request_b = false,
            .up_cores = 0,
            .core_cap_duration = 0,
		},
		{/* High performance input boost */
			.type = 6,
			.priority = 0,
			.duration = 100,
			.boost_freq = {HIGH_PERF_INPUT_BOOST_FREQ,HIGH_PERF_INPUT_BOOST_FREQ,HIGH_PERF_INPUT_BOOST_FREQ,HIGH_PERF_INPUT_BOOST_FREQ},
			.timeout = 0,
			.request_b = false,
            .up_cores = 2,
            .core_cap_duration = 100,
		},
		{/* High performance input boost */
			.type = 7,
			.priority = 4,
			.duration = 60000,
			.boost_freq = {HIGH_PERF_INPUT_BOOST_FREQ,HIGH_PERF_INPUT_BOOST_FREQ,HIGH_PERF_INPUT_BOOST_FREQ,HIGH_PERF_INPUT_BOOST_FREQ},
			.timeout = 0,
			.request_b = false,
            .up_cores = 0,
            .core_cap_duration = 0,
		},
	},
};
/* FUJITSU:2013-03-29 core boost end */

DEFINE_SPINLOCK(boost_inf_lock);

static bool boost_priority_is_high(unsigned int boost_type);
static bool boost_is_timeout(unsigned long timeout);
static bool boost_is_boost(void);
static int boost_update_timeout_state(void); /* return:next boost type <0:not boost */
static void boost_set_data(unsigned int  boost_type);
extern int cpufreq_get_level(struct cpufreq_policy *policy,int freq);
extern int cpufreq_get_freq(struct cpufreq_policy *policy,int level);
/* FUJITSU:2013-04-19 table ondemand mod end */
/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static unsigned int min_sampling_rate;

#define LATENCY_MULTIPLIER			(1000)
#define MIN_LATENCY_MULTIPLIER			(100)
#define TRANSITION_LATENCY_LIMIT		(10 * 1000 * 1000)

#define POWERSAVE_BIAS_MAXLEVEL			(1000)
#define POWERSAVE_BIAS_MINLEVEL			(-1000)

static void do_dbs_timer(struct work_struct *work);
static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				unsigned int event);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
static
#endif
struct cpufreq_governor cpufreq_gov_ondemand = {
       .name                   = "ondemand",
       .governor               = cpufreq_governor_dbs,
       .max_transition_latency = TRANSITION_LATENCY_LIMIT,
       .owner                  = THIS_MODULE,
};

/* Sampling types */
enum {DBS_NORMAL_SAMPLE, DBS_SUB_SAMPLE};

struct cpu_dbs_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_iowait;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
	struct cpufreq_frequency_table *freq_table;
	unsigned int freq_lo;
	unsigned int freq_lo_jiffies;
	unsigned int freq_hi_jiffies;
	unsigned int rate_mult;
	unsigned int prev_load;
	unsigned int max_load;
	int cpu;
/* FUJITSU:2013-01-30 table ondemand start */
	int cur_level_value;
/* FUJITSU:2013-01-30 table ondemand end */
	unsigned int sample_type:1;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_dbs_timer invocation. We do not want do_dbs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
static DEFINE_PER_CPU(struct cpu_dbs_info_s, od_cpu_dbs_info);

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info);
static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info);

static unsigned int dbs_enable;	/* number of CPUs using this policy */

/*
 * dbs_mutex protects dbs_enable in governor start/stop.
 */
static DEFINE_MUTEX(dbs_mutex);

static struct workqueue_struct *input_wq;

/* FUJITSU:2013-01-30 table ondemand mod start */
#if 0
static DEFINE_PER_CPU(struct work_struct, dbs_refresh_work);
#else
static DEFINE_PER_CPU(struct work_struct, dbs_refresh_work_bstup);
#endif
/* FUJITSU:2013-01-30 table ondemand mod end */

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int up_threshold;
	unsigned int up_threshold_multi_core;
	unsigned int down_differential;
	unsigned int down_differential_multi_core;
	unsigned int optimal_freq;
	unsigned int up_threshold_any_cpu_load;
	unsigned int sync_freq;
	unsigned int ignore_nice;
	unsigned int sampling_down_factor;
	int          powersave_bias;
	unsigned int io_is_busy;
/* FUJITSU:2013-01-30 table ondemand add start */
#if 0
	unsigned int boost;
#endif
	unsigned int debug;
	unsigned int type;
	unsigned int pri_level;
/* FUJITSU:2013-03-29 core boost start */
	unsigned int min_cap;
	unsigned int input_type;
/* FUJITSU:2013-03-29 core boost end */
/* FUJITSU:2013-01-30 table ondemand add end */
} dbs_tuners_ins = {
	.up_threshold_multi_core = DEF_FREQUENCY_UP_THRESHOLD,
	.up_threshold = DEF_FREQUENCY_UP_THRESHOLD,
	.sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
	.down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL,
	.down_differential_multi_core = MICRO_FREQUENCY_DOWN_DIFFERENTIAL,
	.up_threshold_any_cpu_load = DEF_FREQUENCY_UP_THRESHOLD,
	.ignore_nice = 0,
	.powersave_bias = 0,
	.sync_freq = 0,
	.optimal_freq = 0,
/* FUJITSU:2013-01-30 table ondemand add start */
	.type = 0,
	.pri_level = 0,
/* FUJITSU:2013-03-29 core boost start */
	.min_cap = 0,
	.input_type = 0,
/* FUJITSU:2013-03-29 core boost end */
/* FUJITSU:2013-01-30 table ondemand add end */
};

/* FUJITSU:2013-05-10 add start */
static int get_freq_max(int freq_max)
{
    struct cpu_dbs_info_s *cpu0_dbs_info = &per_cpu(od_cpu_dbs_info, 0);

    if (cpu0_dbs_info && cpu0_dbs_info->cur_policy) {
        if (freq_max > cpu0_dbs_info->cur_policy->max){
            return cpu0_dbs_info->cur_policy->max;
        }else{
            return freq_max;
        }
    }else{
        return freq_max;
    }
}
/* FUJITSU:2013-05-10 add end */


/* FUJITSU:2013-03-29 core boost start */
static unsigned long min_cap_timeout;
/* FUJITSU:2013-03-29 core boost end */
static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int powersave_bias_target(struct cpufreq_policy *policy,
					  unsigned int freq_next,
					  unsigned int relation)
{
	unsigned int freq_req, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	int freq_reduc;
	struct cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info,
						   policy->cpu);

	if (!dbs_info->freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_next,
			relation, &index);
	freq_req = dbs_info->freq_table[index].frequency;
	freq_reduc = freq_req * dbs_tuners_ins.powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_H, &index);
	freq_lo = dbs_info->freq_table[index].frequency;
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_L, &index);
	freq_hi = dbs_info->freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_lo;
	}
	jiffies_total = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_jiffies = jiffies_lo;
	dbs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static int ondemand_powersave_bias_setspeed(struct cpufreq_policy *policy,
					    struct cpufreq_policy *altpolicy,
					    int level)
{
	if (level == POWERSAVE_BIAS_MAXLEVEL) {
		/* maximum powersave; set to lowest frequency */
		__cpufreq_driver_target(policy,
			(altpolicy) ? altpolicy->min : policy->min,
			CPUFREQ_RELATION_L);
		return 1;
	} else if (level == POWERSAVE_BIAS_MINLEVEL) {
		/* minimum powersave; set to highest frequency */
		__cpufreq_driver_target(policy,
			(altpolicy) ? altpolicy->max : policy->max,
			CPUFREQ_RELATION_H);
		return 1;
	}
	return 0;
}

static void ondemand_powersave_bias_init_cpu(int cpu)
{
	struct cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

static void ondemand_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		ondemand_powersave_bias_init_cpu(i);
	}
}

/************************** sysfs interface ************************/

static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}

define_one_global_ro(sampling_rate_min);

/* cpufreq_ondemand Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);
show_one(up_threshold, up_threshold);
show_one(up_threshold_multi_core, up_threshold_multi_core);
show_one(down_differential, down_differential);
show_one(sampling_down_factor, sampling_down_factor);
show_one(ignore_nice_load, ignore_nice);
show_one(optimal_freq, optimal_freq);
show_one(up_threshold_any_cpu_load, up_threshold_any_cpu_load);
show_one(sync_freq, sync_freq);
/* FUJITSU:2013-01-30 table ondemand add start */
#if 0
show_one(boost, boost);
#endif
show_one(debug, debug);
show_one(type, type);
#if 0   /* FUJITSU:2013-03-13 del start */
show_one(pri_level, pri_level);
#endif  /* FUJITSU:2013-03-13 del end */

/* FUJITSU:2013-03-29 core boost start */
show_one(min_cap, min_cap);
show_one(input_type, input_type);
/* FUJITSU:2013-03-29 core boost end */

static ssize_t show_threshold(struct kobject *kobj, struct attribute *attr, char *buf)
{
	unsigned int level,len,type;
	struct cpu_dbs_info_s *dbs_info = 0;
	for_each_online_cpu(len) {
		dbs_info = &per_cpu(od_cpu_dbs_info, len);
		dbs_info->freq_table = cpufreq_frequency_get_table(len);
		if(dbs_info->freq_table)
			break;
	}
	if(!dbs_info->freq_table)
		return 0;
	len = 0;
	type = dbs_tuners_ins.type;
	len += sprintf(&buf[len],"type: %d\n",type);
	for(level=0;level<LEVEL_NUM;level++) {
		len += sprintf(&buf[len],"%2u(%7u): %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %u\n",
						level,dbs_info->freq_table[level].frequency,
						threshold_table[type][level][0],
						threshold_table[type][level][1],
						threshold_table[type][level][2],
						threshold_table[type][level][3],
						threshold_table[type][level][4],
						threshold_table[type][level][5],
						threshold_table[type][level][6],
						threshold_table[type][level][7],
						threshold_table[type][level][8],
						threshold_table[type][level][9],
						threshold_table[type][level][10],
						threshold_table[type][level][11],
						threshold_table[type][level][12],
						sampling_rate_table[type][level]);
		if(dbs_tuners_ins.debug == 0)
			continue;
		len += sprintf(&buf[len],"           : %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
						loadavr_table[level][0],
						loadavr_table[level][1],
						loadavr_table[level][2],
						loadavr_table[level][3],
						loadavr_table[level][4],
						loadavr_table[level][5],
						loadavr_table[level][6],
						loadavr_table[level][7],
						loadavr_table[level][8],
						loadavr_table[level][9],
						loadavr_table[level][10],
						loadavr_table[level][11],
						loadavr_table[level][12],
						loadavr_table[level][13]);
	}
	return len;
}

static ssize_t show_bstup(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", boost_inf_ins.boost_type_cur);\
}

static ssize_t show_boost_info(struct kobject *kobj, struct attribute *attr, char *buf)
{
	unsigned int i,len;
/* FUJITSU:2013-03-29 core boost start */
	len = sprintf(buf,"type pri bst0 bst1 bst2 bst3 dur core coredur\n");
	for(i=0;i<CPUFREQ_ONDEMAND_TYPE_BOOST_NUM;i++) {
		len += sprintf(&buf[len],"%2lu %2lu %3lu %3lu %3lu %3lu %3lu %3d %3d\n", 
				boost_inf_ins.type_inf[i].type, boost_inf_ins.type_inf[i].priority,
				boost_inf_ins.type_inf[i].boost_freq[0], boost_inf_ins.type_inf[i].boost_freq[1],
				boost_inf_ins.type_inf[i].boost_freq[2], boost_inf_ins.type_inf[i].boost_freq[3],
				boost_inf_ins.type_inf[i].duration,
				boost_inf_ins.type_inf[i].up_cores, boost_inf_ins.type_inf[i].core_cap_duration
                );
	}
/* FUJITSU:2013-03-29 core boost end */
	return len;
}
/*!
 @brief set_loadavr

 set loadavr_table

 @param [in] level   cpu level
 @param [in] pointer of the threshold

 @retval     none

 @note set the loadavr_table for the cpu level.
*/
static void set_loadavr(int level,int *threshold)
{
	unsigned int j;
	struct cpu_dbs_info_s *dbs_info = 0;
	int i,type;

	for_each_online_cpu(j) {
		dbs_info = &per_cpu(od_cpu_dbs_info, j);
		dbs_info->freq_table = cpufreq_frequency_get_table(j);
		if(dbs_info->freq_table)
			break;
	}
	if(!dbs_info->freq_table)
		return;
	type = dbs_tuners_ins.type;
	for(i=0;i<LEVEL_NUM-1;i++) {
		if(threshold)
			threshold_table[type][level][i] = threshold[i];
		if(threshold_table[type][level][i] > 100)
			loadavr_table[level][i+1] = 0x7fffffff;
		else {
			loadavr_table[level][i+1] = threshold_table[type][level][i] * dbs_info->freq_table[level].frequency;
		}
	}
}

/* FUJITSU:2013-01-30 table ondemand add end */

static ssize_t show_powersave_bias
(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", dbs_tuners_ins.powersave_bias);
}

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * dbs_tuners_int.sampling_rate might not be appropriate. For example,
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from ondemand
 * governor, but not sure if higher frequency will be required or not,
 * then, the governor may change the sampling rate too late; up to 1 second
 * later. Thus, if we are reducing the sampling rate, we need to make the
 * new value effective immediately.
 */
static void update_sampling_rate(unsigned int new_rate)
{
	int cpu;

	dbs_tuners_ins.sampling_rate = new_rate
				     = max(new_rate, min_sampling_rate);

	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		dbs_info = &per_cpu(od_cpu_dbs_info, policy->cpu);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->timer_mutex);

		if (!delayed_work_pending(&dbs_info->work)) {
			mutex_unlock(&dbs_info->timer_mutex);
			continue;
		}

		next_sampling  = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->work.timer.expires;


		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&dbs_info->timer_mutex);
			cancel_delayed_work_sync(&dbs_info->work);
			mutex_lock(&dbs_info->timer_mutex);

			schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work,
						 usecs_to_jiffies(new_rate));

		}
		mutex_unlock(&dbs_info->timer_mutex);
	}
}

static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	update_sampling_rate(input);
	return count;
}

static ssize_t store_sync_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.sync_freq = input;
	return count;
}

static ssize_t store_io_is_busy(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.io_is_busy = !!input;
	return count;
}

static ssize_t store_optimal_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.optimal_freq = input;
	return count;
}

static ssize_t store_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold = input;
	return count;
}

static ssize_t store_up_threshold_multi_core(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold_multi_core = input;
	return count;
}

static ssize_t store_up_threshold_any_cpu_load(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	dbs_tuners_ins.up_threshold_any_cpu_load = input;
	return count;
}

static ssize_t store_down_differential(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input >= dbs_tuners_ins.up_threshold ||
			input < MIN_FREQUENCY_DOWN_DIFFERENTIAL) {
		return -EINVAL;
	}

	dbs_tuners_ins.down_differential = input;

	return count;
}

static ssize_t store_sampling_down_factor(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	dbs_tuners_ins.sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(od_cpu_dbs_info, j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == dbs_tuners_ins.ignore_nice) { /* nothing to do */
		return count;
	}
	dbs_tuners_ins.ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(od_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&dbs_info->prev_cpu_wall);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	int input  = 0;
	int bypass = 0;
	int ret, cpu, reenable_timer, j;
	struct cpu_dbs_info_s *dbs_info;

	struct cpumask cpus_timer_done;
	cpumask_clear(&cpus_timer_done);

	ret = sscanf(buf, "%d", &input);

	if (ret != 1)
		return -EINVAL;

	if (input >= POWERSAVE_BIAS_MAXLEVEL) {
		input  = POWERSAVE_BIAS_MAXLEVEL;
		bypass = 1;
	} else if (input <= POWERSAVE_BIAS_MINLEVEL) {
		input  = POWERSAVE_BIAS_MINLEVEL;
		bypass = 1;
	}

	if (input == dbs_tuners_ins.powersave_bias) {
		/* no change */
		return count;
	}

	reenable_timer = ((dbs_tuners_ins.powersave_bias ==
				POWERSAVE_BIAS_MAXLEVEL) ||
				(dbs_tuners_ins.powersave_bias ==
				POWERSAVE_BIAS_MINLEVEL));

	dbs_tuners_ins.powersave_bias = input;
	if (!bypass) {
		if (reenable_timer) {
			/* reinstate dbs timer */
			for_each_online_cpu(cpu) {
				if (lock_policy_rwsem_write(cpu) < 0)
					continue;

				dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

				for_each_cpu(j, &cpus_timer_done) {
					if (!dbs_info->cur_policy) {
						pr_err("Dbs policy is NULL\n");
						goto skip_this_cpu;
					}
					if (cpumask_test_cpu(j, dbs_info->
							cur_policy->cpus))
						goto skip_this_cpu;
				}

				cpumask_set_cpu(cpu, &cpus_timer_done);
				if (dbs_info->cur_policy) {
					/* restart dbs timer */
					dbs_timer_init(dbs_info);
				}
skip_this_cpu:
				unlock_policy_rwsem_write(cpu);
			}
		}
		ondemand_powersave_bias_init();
	} else {
		/* running at maximum or minimum frequencies; cancel
		   dbs timer as periodic load sampling is not necessary */
		for_each_online_cpu(cpu) {
			if (lock_policy_rwsem_write(cpu) < 0)
				continue;

			dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

			for_each_cpu(j, &cpus_timer_done) {
				if (!dbs_info->cur_policy) {
					pr_err("Dbs policy is NULL\n");
					goto skip_this_cpu_bypass;
				}
				if (cpumask_test_cpu(j, dbs_info->
							cur_policy->cpus))
					goto skip_this_cpu_bypass;
			}

			cpumask_set_cpu(cpu, &cpus_timer_done);

			if (dbs_info->cur_policy) {
				/* cpu using ondemand, cancel dbs timer */
				mutex_lock(&dbs_info->timer_mutex);
				dbs_timer_exit(dbs_info);

				ondemand_powersave_bias_setspeed(
					dbs_info->cur_policy,
					NULL,
					input);

				mutex_unlock(&dbs_info->timer_mutex);
			}
skip_this_cpu_bypass:
			unlock_policy_rwsem_write(cpu);
		}
	}

	return count;
}

/* FUJITSU:2013-01-30 table ondemand add start */
#if 0
static ssize_t store_boost(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;
	if (input > MAX_BOOST_FREQ)
		return -EINVAL;

	dbs_tuners_ins.boost = input;

	return count;
}
#endif
static ssize_t store_debug(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	dbs_tuners_ins.debug = input;

	return count;
}

static ssize_t store_type(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret,level;
	unsigned long flags;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;
	if (input >= CPUFREQ_ONDEMAND_TYPE_BOOST_BASE )
		return -EINVAL;

	spin_lock_irqsave(&boost_inf_lock, flags);
	if(boost_is_boost()){
		boost_inf_ins.type_tmp = input;
		spin_unlock_irqrestore(&boost_inf_lock, flags);
		return count;
	}
	spin_unlock_irqrestore(&boost_inf_lock, flags);

	dbs_tuners_ins.type = input;
	for(level=0;level<LEVEL_NUM;level++)
		set_loadavr(level,NULL);

	return count;
}

#if 0   /* FUJITSU:2013-03-13 del start */
static ssize_t store_pri_level(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.type = input;
	return count;
}
#endif  /* FUJITSU:2013-03-13 del end */

static ssize_t store_bstup(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input1;
	unsigned int input2;
	int ret;

	ret = sscanf(buf, "%u %u", &input1, &input2);

	if (ret != 2)
		return -EINVAL;

	printk(KERN_DEBUG "store_bstup() boost_type:%d\n", input1);
	if(!(input1 >=0 &&input1 <= CPUFREQ_ONDEMAND_BOOST_TYPE_MAX)){
		return count;
	}
	boost_set_data(input1);
	return count;
}

static ssize_t store_threshold(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int level,threshold[LEVEL_NUM-1],delay;
	int ret;
	ret = sscanf(buf, "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
			&level,
			&threshold[0],&threshold[1],&threshold[2],&threshold[3],&threshold[4],
			&threshold[5],&threshold[6],&threshold[7],&threshold[8],&threshold[9],
			&threshold[10],&threshold[11],&threshold[12],
			&delay);

	if (ret != ((LEVEL_NUM-1)+2))
		return -EINVAL;

	if (level >= LEVEL_NUM)
		level = LEVEL_NUM-1;

	set_loadavr(level,threshold);
	sampling_rate_table[dbs_tuners_ins.type][level] = delay;

	return count;
}

static ssize_t store_boost_info(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
/* FUJITSU:2013-03-29 core boost start */
	unsigned long type,priority,bst[NR_CPUS],duration;
	int ret,i,up_cores,core_cap_duration;
	ret = sscanf(buf, "%lu %lu %lu %lu %lu %lu %lu %d %d",
			&type,&priority,&bst[0],&bst[1],&bst[2],&bst[3],&duration,&up_cores,&core_cap_duration);

	if (ret != 9/*the num of members of boost_type_inf_st*/)
		return -EINVAL;
/* FUJITSU:2013-03-29 core boost end */

	for(i=0;i<CPUFREQ_ONDEMAND_TYPE_BOOST_NUM;i++) {
		int m;
		if(type != boost_inf_ins.type_inf[i].type)
			continue;
		boost_inf_ins.type_inf[i].priority = priority;
		for(m=0;m<NR_CPUS;m++)
			boost_inf_ins.type_inf[i].boost_freq[m] = bst[m];
		boost_inf_ins.type_inf[i].duration = duration;
/* FUJITSU:2013-03-29 core boost start */
		boost_inf_ins.type_inf[i].up_cores = up_cores;
		boost_inf_ins.type_inf[i].core_cap_duration = core_cap_duration;
/* FUJITSU:2013-03-29 core boost end */
		return count;
	}
	return -EINVAL;
}
#if 0  /* FUJITSU:2013-05-14 lv_stat del start */
extern ssize_t show_lv_stat(struct kobject *kobj, struct attribute *attr, char *buf);
extern ssize_t store_lv_stat(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count);
#endif /* FUJITSU:2013-05-14 lv_stat del end */
/* FUJITSU:2013-04-19 core boost start */
static void set_min_cap(int min)
{
	unsigned long flags;
	int notify = 0;

	if(dbs_tuners_ins.debug > 1)
		printk(KERN_INFO "set_min_cap(%d)\n",min);

	spin_lock_irqsave(&boost_inf_lock, flags);
	if(dbs_tuners_ins.min_cap != min) {
		notify = 1;
		dbs_tuners_ins.min_cap = min;
	}

	if(min > 0) {
		min_cap_timeout = jiffies + boost_inf_ins.type_inf[boost_inf_ins.boost_type_cur].core_cap_duration;
	} else {
		min_cap_timeout = 0;
	}
	spin_unlock_irqrestore(&boost_inf_lock, flags);
	if(notify)
		sysfs_notify(cpufreq_global_kobject, "ondemand", "min_cap");
}

static ssize_t store_min_cap(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	set_min_cap(input);
	if(input <= 0) {
		unsigned long flags;
		spin_lock_irqsave(&boost_inf_lock, flags);
		if(boost_inf_ins.type_inf[CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT].request_b == true
		   || boost_inf_ins.type_inf[CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT2].request_b == true) {
			int cpu;
			boost_inf_ins.type_inf[CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT].timeout = 0;
			boost_inf_ins.type_inf[CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT2].timeout = 0;
			spin_unlock_irqrestore(&boost_inf_lock, flags);
			for_each_online_cpu(cpu) {
				struct cpufreq_policy *policy;
				struct cpu_dbs_info_s *dbs_info;

				policy = cpufreq_cpu_get(cpu);
				if (!policy)
					continue;
				dbs_info = &per_cpu(od_cpu_dbs_info, policy->cpu);
				cpufreq_cpu_put(policy);

				mutex_lock(&dbs_info->timer_mutex);

				if (!delayed_work_pending(&dbs_info->work)) {
					mutex_unlock(&dbs_info->timer_mutex);
					continue;
				}
				mutex_unlock(&dbs_info->timer_mutex);
				cancel_delayed_work_sync(&dbs_info->work);
				mutex_lock(&dbs_info->timer_mutex);
				schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work,1);
				mutex_unlock(&dbs_info->timer_mutex);
		        if(dbs_tuners_ins.debug)
			        printk(KERN_INFO "store_min_cap() schedule dbs_info %d\n", cpu);
			}
		} else 
			spin_unlock_irqrestore(&boost_inf_lock, flags);
	}
	return count;
}

static ssize_t store_input_type(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	switch(input){
	case INPUT_BOOST_NORMAL:	/* Norma */
	case INPUT_BOOST_HIGH:		/* High */
	case INPUT_BOOST_LONG:		/* Long */
			dbs_tuners_ins.input_type = input;
			break;
	default:
			dbs_tuners_ins.input_type = 0;
			break;
    }

	return count;
}
/* FUJITSU:2013-04-19 core boost end */

/* FUJITSU:2013-01-30 table ondemand add end */
define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);
define_one_global_rw(up_threshold);
define_one_global_rw(down_differential);
define_one_global_rw(sampling_down_factor);
define_one_global_rw(ignore_nice_load);
define_one_global_rw(powersave_bias);
define_one_global_rw(up_threshold_multi_core);
define_one_global_rw(optimal_freq);
define_one_global_rw(up_threshold_any_cpu_load);
define_one_global_rw(sync_freq);
/* FUJITSU:2013-01-30 table ondemand add start */
#if 0
define_one_global_rw(boost);
#endif
define_one_global_rw(debug);
define_one_global_rw(type);
define_one_global_rw(threshold);
define_one_global_rw(boost_info);
#if 0   /* FUJITSU:2013-03-13 del start */
define_one_global_rw(pri_level);
#endif  /* FUJITSU:2013-03-13 del end */
define_one_global_rw(bstup);
#if 0   /* FUJITSU:2013-05-14 lv_stat del start */
define_one_global_rw(lv_stat);
#endif  /* FUJITSU:2013-05-14 lv_stat del end */
/* FUJITSU:2013-01-30 table ondemand add end */
/* FUJITSU:2013-03-29 core boost start */
define_one_global_rw(min_cap);
define_one_global_rw(input_type);
/* FUJITSU:2013-03-29 core boost end */

static struct attribute *dbs_attributes[] = {
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&up_threshold.attr,
	&down_differential.attr,
	&sampling_down_factor.attr,
	&ignore_nice_load.attr,
	&powersave_bias.attr,
	&io_is_busy.attr,
	&up_threshold_multi_core.attr,
	&optimal_freq.attr,
	&up_threshold_any_cpu_load.attr,
	&sync_freq.attr,
/* FUJITSU:2013-01-30 table ondemand add start */
#if 0
	&boost.attr,
#endif
	&debug.attr,
	&type.attr,
	&threshold.attr,
	&boost_info.attr,
#if 0   /* FUJITSU:2013-03-13 del start */
	&pri_level.attr,
#endif  /* FUJITSU:2013-03-13 del end */
	&bstup.attr,
#if 0  /* FUJITSU:2013-05-14 lv_stat del start */
	&lv_stat.attr,
#endif /* FUJITSU:2013-05-14 lv_stat del end */
/* FUJITSU:2013-01-30 table ondemand add end */
/* FUJITSU:2013-03-29 core boost start */
	&min_cap.attr,
	&input_type.attr,
/* FUJITSU:2013-03-29 core boost end */
	NULL
};

static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "ondemand",
};

/************************** sysfs end ************************/

/* FUJITSU:2013-01-30 table ondemand del start */
#if 0
static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	if (dbs_tuners_ins.powersave_bias)
		freq = powersave_bias_target(p, freq, CPUFREQ_RELATION_H);
	else if (p->cur == p->max)
		return;

	__cpufreq_driver_target(p, freq, dbs_tuners_ins.powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}
#endif
/* FUJITSU:2013-01-30 table ondemand del end */

static void dbs_check_cpu(struct cpu_dbs_info_s *this_dbs_info)
{
	/* Extrapolated load of this CPU */
	unsigned int load_at_max_freq = 0;
	unsigned int max_load_freq;
	/* Current load across this CPU */
	unsigned int cur_load = 0;
	unsigned int max_load_other_cpu = 0;
	struct cpufreq_policy *policy;
	unsigned int j;
/* FUJITSU:2013-01-30 table ondemand add start */
	int level,next,freq_next,relation;
/* FUJITSU:2013-01-30 table ondemand add end */
/* FUJITSU:2013-05-10 add start */
    int local_max;
/* FUJITSU:2013-05-10 end start */

	this_dbs_info->freq_lo = 0;
	policy = this_dbs_info->cur_policy;

	/*
	 * Every sampling_rate, we check, if current idle time is less
	 * than 20% (default), then we try to increase frequency
	 * Every sampling_rate, we look for a the lowest
	 * frequency which can sustain the load while keeping idle time over
	 * 30%. If such a frequency exist, we try to decrease to this frequency.
	 *
	 * Any frequency increase takes it to the maximum frequency.
	 * Frequency reduction happens at minimum steps of
	 * 5% (default) of current frequency
	 */

	/* Get Absolute Load - in terms of freq */
	max_load_freq = 0;

	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info_s *j_dbs_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load_freq;
		int freq_avg;

		j_dbs_info = &per_cpu(od_cpu_dbs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);
		cur_iowait_time = get_cpu_iowait_time(j, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - j_dbs_info->prev_cpu_wall);
		j_dbs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - j_dbs_info->prev_cpu_iowait);
		j_dbs_info->prev_cpu_iowait = cur_iowait_time;

		if (dbs_tuners_ins.ignore_nice) {
			u64 cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] -
					 j_dbs_info->prev_cpu_nice;
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}

		/*
		 * For the purpose of ondemand, waiting for disk IO is an
		 * indication that you're performance critical, and not that
		 * the system is actually idle. So subtract the iowait time
		 * from the cpu idle time.
		 */

		if (dbs_tuners_ins.io_is_busy && idle_time >= iowait_time)
			idle_time -= iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		cur_load = 100 * (wall_time - idle_time) / wall_time;

/* FUJITSU:2013-01-30 table ondemand add start */
		if(dbs_tuners_ins.debug > 2)
			printk(KERN_INFO "%u : %u  %u",cur_load,wall_time,idle_time);
/* FUJITSU:2013-01-30 table ondemand add end */

		j_dbs_info->max_load  = max(cur_load, j_dbs_info->prev_load);
		j_dbs_info->prev_load = cur_load;
		freq_avg = __cpufreq_driver_getavg(policy, j);
		if (freq_avg <= 0)
			freq_avg = policy->cur;

		load_freq = cur_load * freq_avg;
		if (load_freq > max_load_freq)
			max_load_freq = load_freq;
/* FUJITSU:2013-01-30 table ondemand add start */
		if(dbs_tuners_ins.debug > 1)
			printk(KERN_INFO "%u : %u  %u",load_freq,freq_avg,j);
/* FUJITSU:2013-01-30 table ondemand add end */
	}

	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *j_dbs_info;
		j_dbs_info = &per_cpu(od_cpu_dbs_info, j);

		if (j == policy->cpu)
			continue;

		if (max_load_other_cpu < j_dbs_info->max_load)
			max_load_other_cpu = j_dbs_info->max_load;
		/*
		 * The other cpu could be running at higher frequency
		 * but may not have completed it's sampling_down_factor.
		 * For that case consider other cpu is loaded so that
		 * frequency imbalance does not occur.
		 */

		if ((j_dbs_info->cur_policy != NULL)
			&& (j_dbs_info->cur_policy->cur ==
					j_dbs_info->cur_policy->max)) {

			if (policy->cur >= dbs_tuners_ins.optimal_freq)
				max_load_other_cpu =
				dbs_tuners_ins.up_threshold_any_cpu_load;
		}
	}

	/* calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * policy->cur)/policy->cpuinfo.max_freq;

	cpufreq_notify_utilization(policy, load_at_max_freq);

/* FUJITSU:2013-05-10 table ondemand add start */
	level = cpufreq_get_level(policy,policy->cur);
	if(level >= LEVEL_NUM) {
		printk(KERN_ERR "%s:level(%d) >= LEVEL_NUM type=%d", __func__, level, dbs_tuners_ins.type);
		level = LEVEL_NUM - 1;
	}
	for(next=LEVEL_NUM-1;next > 0;next--) {
		if(loadavr_table[level][next] < max_load_freq)
			break;
		if(dbs_tuners_ins.debug > 3 && loadavr_table[level][next] != 0x7fffffff)
			printk(KERN_INFO "%u[%u][%u] > %u",loadavr_table[level][next],level,next,max_load_freq);
	}
	if(level == next)
		return;
	freq_next = cpufreq_get_freq(policy,next);
	if(freq_next == 0) {
		if(dbs_tuners_ins.debug)
			printk(KERN_ERR "cpufreq_get_freq() = 0 next = %d", next);
		return;
	}
	relation = (level < next) ? CPUFREQ_RELATION_H : CPUFREQ_RELATION_L;
	this_dbs_info->cur_level_value = next;
	if(dbs_tuners_ins.debug)
		printk(KERN_INFO "%d:lv %d -> %d (%d)",policy->cpu,level,next, dbs_tuners_ins.type);
	if (!dbs_tuners_ins.powersave_bias) {
			local_max = get_freq_max(policy->max);
			if (freq_next <= local_max){
				__cpufreq_driver_target(policy, freq_next, relation);
			}else{
				if (policy->max > local_max){
					if(dbs_tuners_ins.debug > 2)
						printk(KERN_INFO "MLOG [%d]dbs %d -> %d -> %d\n",policy->cpu, freq_next, policy->max, local_max);
				}
				__cpufreq_driver_target(policy, local_max, relation);
			}
	} else {
		int freq = powersave_bias_target(policy, freq_next,
				relation);
		__cpufreq_driver_target(policy, freq,
			CPUFREQ_RELATION_L);
	}
/* FUJITSU:2013-05-10 table ondemand add end */
/* FUJITSU:2013-01-30 table ondemand del start */
#if 0
	/* Check for frequency increase */
	if (max_load_freq > dbs_tuners_ins.up_threshold * policy->cur) {
		/* If switching to max speed, apply sampling_down_factor */
		if (policy->cur < policy->max)
			this_dbs_info->rate_mult =
				dbs_tuners_ins.sampling_down_factor;
		dbs_freq_increase(policy, policy->max);
		return;
	}

	if (num_online_cpus() > 1) {

		if (max_load_other_cpu >
				dbs_tuners_ins.up_threshold_any_cpu_load) {
			if (policy->cur < dbs_tuners_ins.sync_freq)
				dbs_freq_increase(policy,
						dbs_tuners_ins.sync_freq);
			return;
		}

		if (max_load_freq > dbs_tuners_ins.up_threshold_multi_core *
								policy->cur) {
			if (policy->cur < dbs_tuners_ins.optimal_freq)
				dbs_freq_increase(policy,
						dbs_tuners_ins.optimal_freq);
			return;
		}
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		return;

	/*
	 * The optimal frequency is the frequency that is the lowest that
	 * can support the current CPU usage without triggering the up
	 * policy. To be safe, we focus 10 points under the threshold.
	 */
	if (max_load_freq <
	    (dbs_tuners_ins.up_threshold - dbs_tuners_ins.down_differential) *
	     policy->cur) {
		unsigned int freq_next;
		freq_next = max_load_freq /
				(dbs_tuners_ins.up_threshold -
				 dbs_tuners_ins.down_differential);

		/* No longer fully busy, reset rate_mult */
		this_dbs_info->rate_mult = 1;

		if (freq_next < policy->min)
			freq_next = policy->min;

		if (num_online_cpus() > 1) {
			if (max_load_other_cpu >
			(dbs_tuners_ins.up_threshold_multi_core -
			dbs_tuners_ins.down_differential) &&
			freq_next < dbs_tuners_ins.sync_freq)
				freq_next = dbs_tuners_ins.sync_freq;

			if (max_load_freq >
				 (dbs_tuners_ins.up_threshold_multi_core -
				  dbs_tuners_ins.down_differential_multi_core) *
				  policy->cur)
				freq_next = dbs_tuners_ins.optimal_freq;

		}
		if (!dbs_tuners_ins.powersave_bias) {
			__cpufreq_driver_target(policy, freq_next,
					CPUFREQ_RELATION_L);
		} else {
			int freq = powersave_bias_target(policy, freq_next,
					CPUFREQ_RELATION_L);
			__cpufreq_driver_target(policy, freq,
				CPUFREQ_RELATION_L);
		}
	}
#endif
/* FUJITSU:2013-01-30 table ondemand del end */
}

static void do_dbs_timer(struct work_struct *work)
{
	struct cpu_dbs_info_s *dbs_info =
		container_of(work, struct cpu_dbs_info_s, work.work);
	unsigned int cpu = dbs_info->cpu;
	int sample_type = dbs_info->sample_type;

	int delay;
/* FUJITSU:2013-01-30 boostup start */
	int level;
	int i;
	unsigned long flags;
	int type_tmp;
/* FUJITSU:2013-01-30 boostup end */

	mutex_lock(&dbs_info->timer_mutex);

	/* Common NORMAL_SAMPLE setup */
	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	if (!dbs_tuners_ins.powersave_bias ||
	    sample_type == DBS_NORMAL_SAMPLE) {
		dbs_check_cpu(dbs_info);
		if (dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			dbs_info->sample_type = DBS_SUB_SAMPLE;
			delay = dbs_info->freq_hi_jiffies;
		} else {
			/* We want all CPUs to do sampling nearly on
			 * same jiffy
			 */
			delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate
				* dbs_info->rate_mult);

			if (num_online_cpus() > 1)
				delay -= jiffies % delay;
		}
	} else {
/* FUJITSU:2013-01-30 table ondemand add start */
		if(dbs_tuners_ins.debug)
			printk(KERN_INFO "bias %d\n",dbs_info->freq_lo);
/* FUJITSU:2013-01-30 table ondemand add end */
		__cpufreq_driver_target(dbs_info->cur_policy,
			dbs_info->freq_lo, CPUFREQ_RELATION_H);
		delay = dbs_info->freq_lo_jiffies;
	}
/* FUJITSU:2013-01-30 boostup start */
	spin_lock_irqsave(&boost_inf_lock, flags);
/* FUJITSU:2013-03-29 core boost start */
    if(min_cap_timeout > 0 && time_is_before_jiffies(min_cap_timeout)) {
        if(dbs_tuners_ins.debug)
            printk(KERN_INFO "cap timeout %lu\n",min_cap_timeout);
        spin_unlock_irqrestore(&boost_inf_lock, flags);
        set_min_cap(0);
        spin_lock_irqsave(&boost_inf_lock, flags);
    }
/* FUJITSU:2013-03-29 core boost end */
	if(boost_is_boost() && boost_is_timeout(boost_inf_ins.type_inf[boost_inf_ins.boost_type_cur].timeout)){
/* FUJITSU:2013-04-19 core boost start */
		type_tmp = boost_update_timeout_state();
		spin_unlock_irqrestore(&boost_inf_lock, flags);
		if(type_tmp == -1){
		    if(dbs_tuners_ins.debug)
			    printk(KERN_DEBUG "do_dbs_timer() dbs_refresh_work_bstup\n");

/* FUJITSU:2013-04-19 core boost end */
			for_each_online_cpu(i) {
				queue_work_on(i, input_wq, &per_cpu(dbs_refresh_work_bstup, i));
			}
		}else{
			dbs_tuners_ins.type = type_tmp;
			for(level=0;level<LEVEL_NUM;level++)
				set_loadavr(level,NULL);
		}
		if(dbs_tuners_ins.debug)
			printk(KERN_DEBUG "do_dbs_timer() dbs_tuners_ins.type:%d\n", dbs_tuners_ins.type);
	}else{
		spin_unlock_irqrestore(&boost_inf_lock, flags);
	}
	delay = usecs_to_jiffies(sampling_rate_table[dbs_tuners_ins.type][dbs_info->cur_level_value]);
	if(dbs_tuners_ins.debug > 3)
		printk(KERN_INFO "delay %d\n",delay);
/* FUJITSU:2013-01-30 boostup end */

	schedule_delayed_work_on(cpu, &dbs_info->work, delay);
	mutex_unlock(&dbs_info->timer_mutex);
}

static inline void dbs_timer_init(struct cpu_dbs_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);

	if (num_online_cpus() > 1)
		delay -= jiffies % delay;

	dbs_info->sample_type = DBS_NORMAL_SAMPLE;
	INIT_DELAYED_WORK_DEFERRABLE(&dbs_info->work, do_dbs_timer);
	schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work, delay);
}

static inline void dbs_timer_exit(struct cpu_dbs_info_s *dbs_info)
{
	cancel_delayed_work_sync(&dbs_info->work);
}

/*
 * Not all CPUs want IO time to be accounted as busy; this dependson how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (androidlcom) calis this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) andl later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
	    boot_cpu_data.x86 == 6 &&
	    boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 0;
}

/* FUJITSU:2013-01-30 table ondemand change start */
#if 0
static void dbs_refresh_callback(struct work_struct *unused)
#else
static void __cpuinit dbs_refresh_callback(struct work_struct *work)
#endif
/* FUJITSU:2013-01-30 table ondemand change start */
{
	struct cpufreq_policy *policy;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int cpu = smp_processor_id();
/* FUJITSU:2013-5-10 table ondemand change start */
	int ret = 0;
	unsigned int temp_cur = 0,n;
	unsigned int boost = 0,type;
	unsigned long flags;
	int local_max;
/* FUJITSU:2013-05-10 table ondemand change end */

/* FUJITSU:2013-03-29 core boost start */
    /* Set lower limit of number of cores. */
    set_min_cap(boost_inf_ins.type_inf[boost_inf_ins.boost_type_cur].up_cores);
/* FUJITSU:2013-03-29 core boost end */
	get_online_cpus();

/* FUJITSU:2013-01-30 table ondemand change start */
	spin_lock_irqsave(&boost_inf_lock, flags);
	if(!boost_is_boost()) {
		spin_unlock_irqrestore(&boost_inf_lock, flags);
		goto bail_acq_sema_failed;
	}
	type = boost_inf_ins.boost_type_cur;
	spin_unlock_irqrestore(&boost_inf_lock, flags);
/* FUJITSU:2013-01-30 table ondemand change end */

	if (lock_policy_rwsem_write(cpu) < 0)
		goto bail_acq_sema_failed;

	this_dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
	policy = this_dbs_info->cur_policy;
	if (!policy) {
		/* CPU not using ondemand governor */
		goto bail_incorrect_governor;
	}

/* FUJITSU:2013-01-30 table ondemand change start */
#if 0
	if (policy->cur < policy->max) {
		/*
		 * Arch specific cpufreq driver may fail.
		 * Don't update governor frequency upon failure.
		 */
		if (__cpufreq_driver_target(policy, policy->max,
					CPUFREQ_RELATION_L) >= 0)
			policy->cur = policy->max;

#endif
/* FUJITSU:2013-03-29 core boost start */
#if 0
	if(type == CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT && cpu < 2)
		n = 0;
	else
		n = num_online_cpus()-1;
#endif
    n = num_online_cpus()-1;
/* FUJITSU:2013-03-29 core boost end */
	boost = boost_inf_ins.type_inf[type].boost_freq[(n < NR_CPUS)? n : NR_CPUS-1];
/* FUJITSU:2013-05-10 mod start */
	local_max = get_freq_max(policy->max);
	if (boost > local_max) {
		if(dbs_tuners_ins.debug > 2)
			printk(KERN_INFO "MLOG [%d]boost %d -> %d -> %d\n",policy->cpu, boost, policy->max, local_max);
	 	boost = local_max;
	}
/* FUJITSU:2013-05-10 mod end */

	if (policy->cur < boost) {
		temp_cur = policy->cur;
		ret = __cpufreq_driver_target(policy, boost, CPUFREQ_RELATION_L);
		if(ret != 0) {
			printk(KERN_ERR "__cpufreq_driver_target(policy, %d, CPUFREQ_RELATION_L) = %d cur:%d temp:%d\n", 
				boost, ret, policy->cur, temp_cur);
		}
		else {
			if(dbs_tuners_ins.debug) {
				printk(KERN_INFO "cpu(%d/%d) freq boost temp_cur:%d boost:%d cur:%d\n", cpu, n,
					temp_cur, boost, policy->cur);
			}
		}
/* FUJITSU:2013-01-30 table ondemand change end */
		this_dbs_info->prev_cpu_idle = get_cpu_idle_time(cpu,
				&this_dbs_info->prev_cpu_wall);
/* FUJITSU:2013-03-29 core boost start */
		this_dbs_info->cur_level_value = cpufreq_get_level(policy,policy->cur);
	}

    mutex_lock(&this_dbs_info->timer_mutex);
    if (delayed_work_pending(&this_dbs_info->work)) {
        mutex_unlock(&this_dbs_info->timer_mutex);
        cancel_delayed_work_sync(&this_dbs_info->work);
        mutex_lock(&this_dbs_info->timer_mutex);
        schedule_delayed_work_on(this_dbs_info->cpu, &this_dbs_info->work,
                usecs_to_jiffies(sampling_rate_table[dbs_tuners_ins.type][this_dbs_info->cur_level_value]));
        if(dbs_tuners_ins.debug > 2)
            printk(KERN_INFO "%d: cancel timer:%d\n",cpu , sampling_rate_table[dbs_tuners_ins.type][this_dbs_info->cur_level_value]);
    }
    mutex_unlock(&this_dbs_info->timer_mutex);
/* FUJITSU:2013-03-29 core boost end */

bail_incorrect_governor:
	unlock_policy_rwsem_write(cpu);

bail_acq_sema_failed:
	put_online_cpus();
	return;
}

static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
/* FUJITSU:2013-01-30 boostup mod start */
#if 0
	int i;

	if ((dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MAXLEVEL) ||
		(dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MINLEVEL)) {
		/* nothing to do */
		return;
	}

	for_each_online_cpu(i) {
		queue_work_on(i, input_wq, &per_cpu(dbs_refresh_work, i));
	}
#endif

/* FUJITSU:2013-04-18 core boost start */
	switch(dbs_tuners_ins.input_type){
	case INPUT_BOOST_HIGH: /* High */
		boost_set_data(CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT2);
		break;
	case INPUT_BOOST_LONG: /* Long */
		boost_set_data(CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT3);
		break;
	case INPUT_BOOST_NORMAL: /* Normal */
	default:
		boost_set_data(CPUFREQ_ONDEMAND_BOOST_TYPE_INPUT);
		break;
    }
/* FUJITSU:2013-04-18 core boost end */
/* FUJITSU:2013-01-30 boostup mod end */
}

static int dbs_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

/* FUJITSU:2013-01-30 exclude device start */
	if(dev != NULL) {
		if(dev->name != NULL) {
			if(strcmp(dev->name, "fj-walkmotion") == 0) {
				return 0;
			}
		}
	}
/* FUJITSU:2013-01-30 exclude device end */
	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpufreq";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void dbs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbs_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler dbs_input_handler = {
	.event		= dbs_input_event,
	.connect	= dbs_input_connect,
	.disconnect	= dbs_input_disconnect,
	.name		= "cpufreq_ond",
	.id_table	= dbs_ids,
};

static int cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_dbs_info_s *this_dbs_info;
	unsigned int j;
	int rc;
/* FUJITSU:2013-05-10 add start */
	int local_max;
/* FUJITSU:2013-05-10 add end */

	this_dbs_info = &per_cpu(od_cpu_dbs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&dbs_mutex);

		dbs_enable++;
		for_each_cpu(j, policy->cpus) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(od_cpu_dbs_info, j);
			j_dbs_info->cur_policy = policy;

			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&j_dbs_info->prev_cpu_wall);
			if (dbs_tuners_ins.ignore_nice)
				j_dbs_info->prev_cpu_nice =
						kcpustat_cpu(j).cpustat[CPUTIME_NICE];
		}
		this_dbs_info->cpu = cpu;
		this_dbs_info->rate_mult = 1;
		ondemand_powersave_bias_init_cpu(cpu);

/* FUJITSU:2013-03-29 core boost start */
        if(cpu && (dbs_tuners_ins.min_cap > 0)) {
            struct cpu_dbs_info_s *cpu0_dbs_info = &per_cpu(od_cpu_dbs_info, 0);
            if(cpu0_dbs_info && cpu0_dbs_info->cur_policy) {
                if(policy->cur != cpu0_dbs_info->cur_policy->cur) {
                    if(dbs_tuners_ins.debug > 2)
                        printk(KERN_INFO "%d:chg clk %d -> %d\n",cpu,policy->cur,cpu0_dbs_info->cur_policy->cur);
                    __cpufreq_driver_target(policy,cpu0_dbs_info->cur_policy->cur,CPUFREQ_RELATION_L);
                }
            }
        }
/* FUJITSU:2013-03-29 core boost end */

		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (dbs_enable == 1) {
			unsigned int latency;

			rc = sysfs_create_group(cpufreq_global_kobject,
						&dbs_attr_group);
			if (rc) {
				mutex_unlock(&dbs_mutex);
				return rc;
			}

			/* policy latency is in nS. Convert it to uS first */
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;
			/* Bring kernel and HW constraints together */
			min_sampling_rate = max(min_sampling_rate,
					MIN_LATENCY_MULTIPLIER * latency);
			dbs_tuners_ins.sampling_rate =
				max(min_sampling_rate,
				    latency * LATENCY_MULTIPLIER);
			dbs_tuners_ins.io_is_busy = should_io_be_busy();

			if (dbs_tuners_ins.optimal_freq == 0)
				dbs_tuners_ins.optimal_freq = policy->min;

			if (dbs_tuners_ins.sync_freq == 0)
				dbs_tuners_ins.sync_freq = policy->min;
/* FUJITSU:2013-01-30 table ondemand add start */
			{
				int level;
				for(level=0;level<LEVEL_NUM;level++)
					set_loadavr(level,NULL);
			}
/* FUJITSU:2013-01-30 table ondemand add end */
		}
		if (!cpu)
			rc = input_register_handler(&dbs_input_handler);
		mutex_unlock(&dbs_mutex);


		if (!ondemand_powersave_bias_setspeed(
					this_dbs_info->cur_policy,
					NULL,
					dbs_tuners_ins.powersave_bias))
			dbs_timer_init(this_dbs_info);
		break;

	case CPUFREQ_GOV_STOP:
		dbs_timer_exit(this_dbs_info);

		mutex_lock(&dbs_mutex);
		mutex_destroy(&this_dbs_info->timer_mutex);
		dbs_enable--;
		/* If device is being removed, policy is no longer
		 * valid. */
		this_dbs_info->cur_policy = NULL;
		if (!cpu)
			input_unregister_handler(&dbs_input_handler);
		mutex_unlock(&dbs_mutex);
		if (!dbs_enable)
			sysfs_remove_group(cpufreq_global_kobject,
					   &dbs_attr_group);

		break;

	case CPUFREQ_GOV_LIMITS:
/* FUJITSU:2013-05-14 mod start */
		mutex_lock(&this_dbs_info->timer_mutex);
		if (this_dbs_info->cur_policy != NULL){
			local_max = get_freq_max(policy->max);
			if (local_max < this_dbs_info->cur_policy->cur){
				if(dbs_tuners_ins.debug > 2)
					printk(KERN_INFO "MLOG [%d]limits %d -> %d -> %d\n",
						policy->cpu,
                                        this_dbs_info->cur_policy->cur,
						policy->max, local_max);
				__cpufreq_driver_target(this_dbs_info->cur_policy,
					local_max, CPUFREQ_RELATION_H);
			}else if (policy->min > this_dbs_info->cur_policy->cur){
				__cpufreq_driver_target(this_dbs_info->cur_policy,
					policy->min, CPUFREQ_RELATION_L);
			}else if (dbs_tuners_ins.powersave_bias != 0){
				ondemand_powersave_bias_setspeed(
					this_dbs_info->cur_policy,
					policy,
					dbs_tuners_ins.powersave_bias);
			}
		}
		mutex_unlock(&this_dbs_info->timer_mutex);
		break;
/* FUJITSU:2013-05-14 mod end */
	}
	return 0;
}

/* FUJITSU:2013-01-30 resume boost add start */
#if defined(CONFIG_HAS_EARLYSUSPEND)
static void cpufreq_gov_ondemand_late_resume(struct early_suspend *h)
{
/* FUJITSU:2013-03-13 add start */
	if(dbs_tuners_ins.debug > 5){
		printk( KERN_INFO "cpufreq_ondemand late resume start dbs_enable[%d] \n", dbs_enable );
	}
	if ( dbs_enable != 0 ){
		if(dbs_tuners_ins.debug > 5){
			printk( KERN_INFO "cpufreq_ondemand late resume boost \n" );
		}
/* FUJITSU:2013-03-13 add end */
	boost_set_data(CPUFREQ_ONDEMAND_BOOST_TYPE_RESUME);
/* FUJITSU:2013-03-13 add start */
	}
	if(dbs_tuners_ins.debug > 5){
		printk( KERN_INFO "cpufreq_ondemand late resume end \n" );
	}
/* FUJITSU:2013-03-13 add end */
}
static struct early_suspend cpufreq_gov_ondemand_early_suspend = {
    .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
    .resume = cpufreq_gov_ondemand_late_resume,
};
#endif
/* FUJITSU:2013-01-30 resume boost add end */

static int __init cpufreq_gov_dbs_init(void)
{
	u64 idle_time;
	unsigned int i;
	int cpu = get_cpu();

	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		dbs_tuners_ins.up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		dbs_tuners_ins.down_differential =
					MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		/* For correct statistics, we need 10 ticks for each measure */
		min_sampling_rate =
			MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10);
	}

	input_wq = create_workqueue("iewq");
	if (!input_wq) {
		printk(KERN_ERR "Failed to create iewq workqueue\n");
		return -EFAULT;
	}
	for_each_possible_cpu(i) {
		struct cpu_dbs_info_s *this_dbs_info =
			&per_cpu(od_cpu_dbs_info, i);
		mutex_init(&this_dbs_info->timer_mutex);

/* FUJITSU:2013-01-30 table ondemand add start */
#if 0
		INIT_WORK(&per_cpu(dbs_refresh_work, i), dbs_refresh_callback);
#else
		INIT_WORK(&per_cpu(dbs_refresh_work_bstup, i), dbs_refresh_callback);
#endif
/* FUJITSU:2013-01-30 table ondemand add end */
	}

/* FUJITSU:2013-03-13 add start */
	dbs_enable = 0;
/* FUJITSU:2013-03-13 add end */
/* FUJITSU:2013-01-30 resume boost add start */
#if defined(CONFIG_HAS_EARLYSUSPEND)
        register_early_suspend(&cpufreq_gov_ondemand_early_suspend);
#endif
/* FUJITSU:2013-01-30 resume boost add end */
	return cpufreq_register_governor(&cpufreq_gov_ondemand);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
/* FUJITSU:2013-01-30 resume boost add start */
#if defined(CONFIG_HAS_EARLYSUSPEND)
        unregister_early_suspend(&cpufreq_gov_ondemand_early_suspend);
#endif
/* FUJITSU:2013-01-30 resume boost add end */
	cpufreq_unregister_governor(&cpufreq_gov_ondemand);
	destroy_workqueue(input_wq);
}

/* FUJITSU:2013-01-30 boostup start */
static bool boost_priority_is_high(unsigned int boost_type)
{
	return (boost_inf_ins.boost_type_cur < 0) || 
               ((boost_inf_ins.type_inf[boost_inf_ins.boost_type_cur].priority < boost_inf_ins.type_inf[boost_type].priority) &&
               (dbs_tuners_ins.pri_level <= boost_inf_ins.type_inf[boost_type].priority));
}

static bool boost_is_timeout(unsigned long timeout)
{
	bool b_ret = time_is_before_jiffies(timeout);
	if(dbs_tuners_ins.debug > 4)
		printk(KERN_DEBUG "boost_is_timeout() b_ret:%d,timeout:%lu,jiffies:%lu,boost_type_cur:%d,type_tmp:%d\n", b_ret, timeout, jiffies, boost_inf_ins.boost_type_cur, boost_inf_ins.type_tmp);
	return b_ret;
}

static bool boost_is_boost(void)
{
	return (boost_inf_ins.boost_type_cur >= 0);
}

static int boost_update_timeout_state(void)
{
	int i;
	int max_priority_index = -1;
	int type_tmp = -1;

	/* timeout */
	for(i = 0; i < CPUFREQ_ONDEMAND_TYPE_BOOST_NUM;i++){
		if(boost_inf_ins.type_inf[i].request_b){
			if(boost_is_timeout(boost_inf_ins.type_inf[i].timeout)){
				boost_inf_ins.type_inf[i].timeout = 0;
				boost_inf_ins.type_inf[i].request_b = false;
				if(dbs_tuners_ins.debug)
					printk(KERN_DEBUG "boost_update_timeout_state() clear boost_type:%d\n", i);
			}else if(boost_priority_is_high(i)){
				max_priority_index = i;
			}
		}
	}
	if(max_priority_index < 0){
		type_tmp = boost_inf_ins.type_tmp;
		boost_inf_ins.type_tmp = -1;
		boost_inf_ins.boost_type_cur = -1;
	}else{
		boost_inf_ins.boost_type_cur = max_priority_index;
	}

	if(dbs_tuners_ins.debug)
		printk(KERN_DEBUG "boost_update_timeout_state() boost_type_cur:%d\n", boost_inf_ins.boost_type_cur);
	return type_tmp;
}

static void boost_set_data(unsigned int boost_type)
{
	int i, level;
	unsigned long flags;
	if ((dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MAXLEVEL) ||
		(dbs_tuners_ins.powersave_bias == POWERSAVE_BIAS_MINLEVEL)) {
		/* nothing to do */
		return;
	}
	spin_lock_irqsave(&boost_inf_lock, flags);
	boost_inf_ins.type_inf[boost_type].timeout = jiffies + boost_inf_ins.type_inf[boost_type].duration;
	boost_inf_ins.type_inf[boost_type].request_b = true;
	if(dbs_tuners_ins.type < CPUFREQ_ONDEMAND_TYPE_BOOST_BASE){
		/* not boost */
		boost_inf_ins.type_tmp = dbs_tuners_ins.type;
		dbs_tuners_ins.type = CPUFREQ_ONDEMAND_TYPE_BOOST_BASE + boost_type;
		boost_inf_ins.boost_type_cur = boost_type;
		for(level=0;level<LEVEL_NUM;level++)
			set_loadavr(level,NULL);
	}else{
		if(boost_priority_is_high(boost_type)){
			/* set type */
			dbs_tuners_ins.type = CPUFREQ_ONDEMAND_TYPE_BOOST_BASE + boost_type;
			boost_inf_ins.boost_type_cur = boost_type;
			for(level=0;level<LEVEL_NUM;level++)
				set_loadavr(level,NULL);
		}
	}
	if(dbs_tuners_ins.debug > 2)
		printk(KERN_DEBUG "boost_set_data() jiffies:%lu,timeout:%lu,dbs_tuners_ins.type=%d,boost_type_cur=%d\n",
					jiffies, boost_inf_ins.type_inf[boost_type].timeout,dbs_tuners_ins.type,boost_inf_ins.boost_type_cur);
	spin_unlock_irqrestore(&boost_inf_lock, flags);
	

	for_each_online_cpu(i) {
		queue_work_on(i, input_wq, &per_cpu(dbs_refresh_work_bstup, i));
	}
}
/* FUJITSU:2013-01-30 boostup end */

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_ondemand' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
