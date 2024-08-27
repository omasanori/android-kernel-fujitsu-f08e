/*
 * MC ASoC codec driver
 *
 * Copyright (c) 2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef MC_ASOC_H
#define MC_ASOC_H

#include <sound/pcm.h>
#include <sound/soc.h>
#include "mcdriver.h"

#define MC_ASOC_NAME				"mc_asoc"
/*
 * dai: set_sysclk
 */
/* clk_id */
#define MC_ASOC_CLKI				0

/* default freq for MC_ASOC_CLKI */
#define MC_ASOC_DEFAULT_CLKI		19200000

/*
 * dai: set_clkdiv
 */
/* div_id */
#define MC_ASOC_CKSEL				0
#define MC_ASOC_DIVR0				1
#define MC_ASOC_DIVF0				2
#define MC_ASOC_DIVR1				3
#define MC_ASOC_DIVF1				4
#define MC_ASOC_BCLK_MULT			5

/* div for MC_ASOC_BCLK_MULT */
#define MC_ASOC_LRCK_X8				0
#define MC_ASOC_LRCK_X16			1
#define MC_ASOC_LRCK_X24			2
#define MC_ASOC_LRCK_X32			3
#define MC_ASOC_LRCK_X48			4
#define MC_ASOC_LRCK_X64			5
#define MC_ASOC_LRCK_X128			6
#define MC_ASOC_LRCK_X256			7
#define MC_ASOC_LRCK_X512			8

/*
 * hwdep: ioctl
 */
#define MC_ASOC_MAGIC				'N'
#define MC_ASOC_IOCTL_SET_CTRL		1
#define MC_ASOC_IOCTL_READ_REG		2
#define MC_ASOC_IOCTL_WRITE_REG		3
#define MC_ASOC_IOCTL_NOTIFY_HOLD	4

#define YMC_IOCTL_SET_CTRL \
	_IOW(MC_ASOC_MAGIC, MC_ASOC_IOCTL_SET_CTRL, ymc_ctrl_args_t)

#define YMC_IOCTL_READ_REG \
	_IOWR(MC_ASOC_MAGIC, MC_ASOC_IOCTL_READ_REG, MCDRV_REG_INFO)

#define YMC_IOCTL_WRITE_REG \
	_IOWR(MC_ASOC_MAGIC, MC_ASOC_IOCTL_WRITE_REG, MCDRV_REG_INFO)

#define YMC_IOCTL_NOTIFY_HOLD \
	_IOWR(MC_ASOC_MAGIC, MC_ASOC_IOCTL_NOTIFY_HOLD, unsigned long)

typedef struct _ymc_ctrl_args {
	void*			param;
	unsigned long	size;
	unsigned long	option;
} ymc_ctrl_args_t;

#define YMC_DSP_OUTPUT_BASE			(0x00000000)
#define YMC_DSP_OUTPUT_SP			(0x00000001)
#define YMC_DSP_OUTPUT_RC			(0x00000002)
#define YMC_DSP_OUTPUT_HP			(0x00000003)
#define YMC_DSP_OUTPUT_LO1			(0x00000004)
#define YMC_DSP_OUTPUT_LO2			(0x00000005)
#define YMC_DSP_OUTPUT_BT			(0x00000006)

#define YMC_DSP_INPUT_BASE			(0x00000010)
#define YMC_DSP_INPUT_EXT			(0x00000020)

#define YMC_DSP_AIN_PLAYBACK_MAINMIC_SP		(0x00010000)
#define YMC_DSP_AIN_PLAYBACK_MAINMIC_RC		(0x00020000)
#define YMC_DSP_AIN_PLAYBACK_MAINMIC_HP		(0x00030000)
#define YMC_DSP_AIN_PLAYBACK_MAINMIC_LO1	(0x00040000)
#define YMC_DSP_AIN_PLAYBACK_MAINMIC_LO2	(0x00050000)
#define YMC_DSP_AIN_PLAYBACK_MAINMIC_BT		(0x00060000)
#define YMC_DSP_AIN_PLAYBACK_SUBMIC_SP		(0x00070000)
#define YMC_DSP_AIN_PLAYBACK_SUBMIC_RC		(0x00080000)
#define YMC_DSP_AIN_PLAYBACK_SUBMIC_HP		(0x00090000)
#define YMC_DSP_AIN_PLAYBACK_SUBMIC_LO1		(0x000A0000)
#define YMC_DSP_AIN_PLAYBACK_SUBMIC_LO2		(0x000B0000)
#define YMC_DSP_AIN_PLAYBACK_SUBMIC_BT		(0x000C0000)
#define YMC_DSP_AIN_PLAYBACK_2MIC_SP		(0x000D0000)
#define YMC_DSP_AIN_PLAYBACK_2MIC_RC		(0x000E0000)
#define YMC_DSP_AIN_PLAYBACK_2MIC_HP		(0x000F0000)
#define YMC_DSP_AIN_PLAYBACK_2MIC_LO1		(0x00100000)
#define YMC_DSP_AIN_PLAYBACK_2MIC_LO2		(0x00110000)
#define YMC_DSP_AIN_PLAYBACK_2MIC_BT		(0x00120000)
#define YMC_DSP_AIN_PLAYBACK_HEADSET_SP		(0x00130000)
#define YMC_DSP_AIN_PLAYBACK_HEADSET_RC		(0x00140000)
#define YMC_DSP_AIN_PLAYBACK_HEADSET_HP		(0x00150000)
#define YMC_DSP_AIN_PLAYBACK_HEADSET_LO1	(0x00160000)
#define YMC_DSP_AIN_PLAYBACK_HEADSET_LO2	(0x00170000)
#define YMC_DSP_AIN_PLAYBACK_HEADSET_BT		(0x00180000)
#define YMC_DSP_AIN_PLAYBACK_BLUETOOTH_SP	(0x00190000)
#define YMC_DSP_AIN_PLAYBACK_BLUETOOTH_RC	(0x001A0000)
#define YMC_DSP_AIN_PLAYBACK_BLUETOOTH_HP	(0x001B0000)
#define YMC_DSP_AIN_PLAYBACK_BLUETOOTH_LO1	(0x001C0000)
#define YMC_DSP_AIN_PLAYBACK_BLUETOOTH_LO2	(0x001D0000)
#define YMC_DSP_AIN_PLAYBACK_BLUETOOTH_BT	(0x001E0000)

#define YMC_DSP_VOICECALL_BASE_1MIC	(0x00000100)
#define YMC_DSP_VOICECALL_BASE_2MIC	(0x00000200)
#define YMC_DSP_VOICECALL_SP_1MIC	(0x00000300)
#define YMC_DSP_VOICECALL_SP_2MIC	(0x00000400)
#define YMC_DSP_VOICECALL_RC_1MIC	(0x00000500)
#define YMC_DSP_VOICECALL_RC_2MIC	(0x00000600)
#define YMC_DSP_VOICECALL_HP_1MIC	(0x00000700)
#define YMC_DSP_VOICECALL_HP_2MIC	(0x00000800)
#define YMC_DSP_VOICECALL_LO1_1MIC	(0x00000900)
#define YMC_DSP_VOICECALL_LO1_2MIC	(0x00000A00)
#define YMC_DSP_VOICECALL_LO2_1MIC	(0x00000B00)
#define YMC_DSP_VOICECALL_LO2_2MIC	(0x00000C00)
#define YMC_DSP_VOICECALL_HEADSET	(0x00000D00)
#define YMC_DSP_VOICECALL_HEADBT	(0x00000E00)


#define YMC_NOTITY_HOLD_OFF			(0)
#define YMC_NOTITY_HOLD_ON			(1)

/* QNET Audio:2012-01-23 Earphone bias control start */
#define YMC_SET_MICBIAS_OFF			(0)
#define YMC_SET_MICBIAS_ON			(1)
/* QNET Audio:2012-01-23 Earphone bias control end */

/*
 * Exported symbols
 */
#if defined(KERNEL_2_6_35_OMAP) || defined(KERNEL_3_0_COMMON)
#else
extern struct snd_soc_dai			mc_asoc_dai[];
extern struct snd_soc_codec_device	mc_asoc_codec_dev;
#endif

#endif
