/*
 * MC ASoC codec driver
 *
 * Copyright (c) 2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.	In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012 - 2013.
/*----------------------------------------------------------------------------*/

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <sound/hwdep.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "mcresctrl.h"
#include "mc_asoc_cfg.h"
#include "mc_asoc.h"
#include "mc_asoc_priv.h"
#include "mc_asoc_parser.h"
#include "ladonhf30.h"
/* QNET Audio:2012-02-09 Clock Supply start */
#include <mach/msm_xo.h>
/* QNET Audio:2012-02-09 Clock Supply end */
#include <linux/audio_log.h>

/* FUJITSU:2013-04-08 Spk/Rcv for him start */
#include <linux/gpio.h>
/* FUJITSU:2013-04-08 Spk/Rcv for him end */
/* FUJITSU:2013-04-23 Rcv amp for him st */
#include <asm/system.h>
/* FUJITSU:2013-04-23 Rcv amp for him end */

/* kernelmode confirmation */
extern int makercmd_mode; /* defined in init/main.c */

#define MC_ASOC_DRIVER_VERSION		"2.0.3"

#define MC_ASOC_I2S_RATE			(SNDRV_PCM_RATE_8000_48000)
#define MC_ASOC_I2S_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE | \
									 SNDRV_PCM_FMTBIT_S20_3LE | \
									 SNDRV_PCM_FMTBIT_S24_3LE)

#define MC_ASOC_PCM_RATE			(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)
#define MC_ASOC_PCM_FORMATS			(SNDRV_PCM_FMTBIT_S8 | \
									 SNDRV_PCM_FMTBIT_S16_LE | \
									 SNDRV_PCM_FMTBIT_S16_BE | \
									 SNDRV_PCM_FMTBIT_A_LAW | \
									 SNDRV_PCM_FMTBIT_MU_LAW)

#define MC_ASOC_HWDEP_ID 			"mc_asoc"

#define mc_asoc_is_in_playback(p)	((p)->stream & (1 << SNDRV_PCM_STREAM_PLAYBACK))
#define mc_asoc_is_in_capture(p)	((p)->stream & (1 << SNDRV_PCM_STREAM_CAPTURE))
#ifdef KERNEL_2_6_35_OMAP
#define get_port_id(id) ((id)/2)
#else
#define get_port_id(id) (id-1)
#endif
/* QNET Audio:2012-01-17 Basic Feat start */
#define DEBUGPRINT0(str)		DBG_PRINTK(str)
#define DEBUGPRINT1(str,d1)		DBG_PRINTK(str "%d",d1)
//#define DEBUGPRINT0(str)		pr_info("YAMAHA debug: %s(%d) %s[%s]\n", __FILE__, __LINE__, __FUNCTION__, str)
//#define DEBUGPRINT1(str,d1)		pr_info("YAMAHA debug: %s(%d) %s[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, str, d1)
/* QNET Audio:2012-01-17 Basic Feat   end */

struct mc_asoc_info_store {
	UINT32	get;
	UINT32	set;
	size_t	offset;
	UINT32	flags;
};
struct mc_asoc_info_store	mc_asoc_info_store_tbl[]	= {
	{MCDRV_GET_DIGITALIO,	MCDRV_SET_DIGITALIO,	offsetof(struct mc_asoc_data, dio_store),		0x1ff},
	{MCDRV_GET_DAC,			MCDRV_SET_DAC,			offsetof(struct mc_asoc_data, dac_store),		0x7},
	{MCDRV_GET_ADC,			MCDRV_SET_ADC,			offsetof(struct mc_asoc_data, adc_store),		0x7},
	{MCDRV_GET_ADC_EX,		MCDRV_SET_ADC_EX,		offsetof(struct mc_asoc_data, adc_ex_store),	0x3f},
	{MCDRV_GET_SP,			MCDRV_SET_SP,			offsetof(struct mc_asoc_data, sp_store),		0},
	{MCDRV_GET_DNG,			MCDRV_SET_DNG,			offsetof(struct mc_asoc_data, dng_store),		0x7f3f3f},
	{MCDRV_GET_SYSEQ,		MCDRV_SET_SYSEQ,		offsetof(struct mc_asoc_data, syseq_store),		0x3},
	{0,						MCDRV_SET_AUDIOENGINE,	offsetof(struct mc_asoc_data, ae_store),		0x1ff},
	{MCDRV_GET_PDM,			MCDRV_SET_PDM,		 	offsetof(struct mc_asoc_data, pdm_store),		0x7f},
	{MCDRV_GET_PDM_EX,		MCDRV_SET_PDM_EX,	 	offsetof(struct mc_asoc_data, pdm_ex_store),	0x3f},
	{MCDRV_GET_PATH,		MCDRV_SET_PATH,		 	offsetof(struct mc_asoc_data, path_store),		0},
	{MCDRV_GET_VOLUME, 		MCDRV_SET_VOLUME,	 	offsetof(struct mc_asoc_data, vol_store),		0},
	{MCDRV_GET_DTMF,		MCDRV_SET_DTMF,		 	offsetof(struct mc_asoc_data, dtmf_store),		0xff},
	{MCDRV_GET_DITSWAP,		MCDRV_SET_DITSWAP,	 	offsetof(struct mc_asoc_data, ditswap_store),	0x3},
};
#define MC_ASOC_N_INFO_STORE \
			(sizeof(mc_asoc_info_store_tbl) / sizeof(struct mc_asoc_info_store))

/* volmap for Digital Volumes */
static SINT16	mc_asoc_vol_digital[]	= {
	0xa000, 0xb600, 0xb700, 0xb800, 0xb900, 0xba00, 0xbb00, 0xbc00,
	0xbd00, 0xbe00, 0xbf00, 0xc000, 0xc100, 0xc200, 0xc300, 0xc400,
	0xc500, 0xc600, 0xc700, 0xc800, 0xc900, 0xca00, 0xcb00, 0xcc00,
	0xcd00, 0xce00, 0xcf00, 0xd000, 0xd100, 0xd200, 0xd300, 0xd400,
	0xd500, 0xd600, 0xd700, 0xd800, 0xd900, 0xda00, 0xdb00, 0xdc00,
	0xdd00, 0xde00, 0xdf00, 0xe000, 0xe100, 0xe200, 0xe300, 0xe400,
	0xe500, 0xe600, 0xe700, 0xe800, 0xe900, 0xea00, 0xeb00, 0xec00,
	0xed00, 0xee00, 0xef00, 0xf000, 0xf100, 0xf200, 0xf300, 0xf400,
	0xf500, 0xf600, 0xf700, 0xf800, 0xf900, 0xfa00, 0xfb00, 0xfc00,
	0xfd00, 0xfe00, 0xff00, 0x0000, 0x0100, 0x0200, 0x0300, 0x0400,
	0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00,
	0x0d00, 0x0e00, 0x0f00, 0x1000, 0x1100, 0x1200,
};

/* volmap for ADC Analog Volume */
static SINT16	mc_asoc_vol_adc[]	= {
	0xa000, 0xe500, 0xe680, 0xe800, 0xe980, 0xeb00, 0xec80, 0xee00,
	0xef80, 0xf100, 0xf280, 0xf400, 0xf580, 0xf700, 0xf880, 0xfa00,
	0xfb80, 0xfd00, 0xfe80, 0x0000, 0x0180, 0x0300, 0x0480, 0x0600,
	0x0780, 0x0900, 0x0a80, 0x0c00, 0x0d80, 0x0f00, 0x1080, 0x1200,
};

/* volmap for LINE/MIC Input Volumes */
static SINT16	mc_asoc_vol_ain[]	= {
	0xa000, 0xe200, 0xe380, 0xe500, 0xe680, 0xe800, 0xe980, 0xeb00,
	0xec80, 0xee00, 0xef80, 0xf100, 0xf280, 0xf400, 0xf580, 0xf700,
	0xf880, 0xfa00, 0xfb80, 0xfd00, 0xfe80, 0x0000, 0x0180, 0x0300,
	0x0480, 0x0600, 0x0780, 0x0900, 0x0a80, 0x0c00, 0x0d80, 0x0f00,
};

/* volmap for HP/SP/RC Output Volumes */
static SINT16	mc_asoc_vol_hpsprc[]	= {
	0xa000, 0xdc00, 0xe400, 0xe800, 0xea00, 0xec00, 0xee00, 0xf000,
	0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0xf600, 0xf700, 0xf800,
	0xf880, 0xf900, 0xf980, 0xfa00, 0xfa80, 0xfb00, 0xfb80, 0xfc00,
	0xfc80, 0xfd00, 0xfd80, 0xfe00, 0xfe80, 0xff00, 0xff80, 0x0000,
};

/* volmap for LINE Output Volumes */
static SINT16	mc_asoc_vol_aout[]	= {
	0xa000, 0xe200, 0xe300, 0xe400, 0xe500, 0xe600, 0xe700, 0xe800,
	0xe900, 0xea00, 0xeb00, 0xec00, 0xed00, 0xee00, 0xef00, 0xf000,
	0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0xf600, 0xf700, 0xf800,
	0xf900, 0xfa00, 0xfb00, 0xfc00, 0xfd00, 0xfe00, 0xff00, 0x0000,
};

/* volmap for MIC Gain Volumes */
static SINT16	mc_asoc_vol_micgain[]	= {
	0x0f00, 0x1400, 0x1900, 0x1e00,
};

/* volmap for HP Gain Volume */
static SINT16	mc_asoc_vol_hpgain[]	= {
	0x0000, 0x0180, 0x0300, 0x0600,
};

struct mc_asoc_vreg_info {
	size_t	offset;
	SINT16	*volmap;
	UINT8	channels;
};
struct mc_asoc_vreg_info	mc_asoc_vreg_map[MC_ASOC_N_VOL_REG]	= {
	{offsetof(MCDRV_VOL_INFO, aswD_Ad0),		mc_asoc_vol_digital,	AD0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Adc1),		mc_asoc_vol_digital,	AD1_DVOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Aeng6),		mc_asoc_vol_digital,	AENG6_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Pdm),		mc_asoc_vol_digital,	PDM_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dtmfb),		mc_asoc_vol_digital,	DTMF_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir0),		mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1),		mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir2),		mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Ad0Att),		mc_asoc_vol_digital,	AD0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Adc1Att),	mc_asoc_vol_digital,	AD1_DVOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DtmfAtt),	mc_asoc_vol_digital,	DTFM_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir0Att),	mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1Att),	mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir2Att),	mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_SideTone),	mc_asoc_vol_digital,	PDM_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DacMaster),	mc_asoc_vol_digital,	DAC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DacVoice),	mc_asoc_vol_digital,	DAC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DacAtt),		mc_asoc_vol_digital,	DAC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit0),		mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit1),		mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit2),		mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit21),		mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswD_Dir0Att),	mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1Att),	mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{(size_t)-1,								mc_asoc_vol_digital,	AD0_VOL_CHANNELS},
	{(size_t)-1,								mc_asoc_vol_digital,	AD0_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswA_Ad0),		mc_asoc_vol_adc,		AD0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Ad1),		mc_asoc_vol_adc,		AD1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lin1),		mc_asoc_vol_ain,		LIN1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lin2),		mc_asoc_vol_ain,		LIN2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic1),		mc_asoc_vol_ain,		MIC1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic2),		mc_asoc_vol_ain,		MIC2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic3),		mc_asoc_vol_ain,		MIC3_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Hp),			mc_asoc_vol_hpsprc,		HP_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Sp),			mc_asoc_vol_hpsprc,		SP_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Rc),			mc_asoc_vol_hpsprc,		RC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lout1),	 	mc_asoc_vol_aout,		LOUT1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lout2),	 	mc_asoc_vol_aout,		LOUT2_VOL_CHANNELS},

	{(size_t)-1,								mc_asoc_vol_adc,		AD0_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswA_Mic1Gain),	mc_asoc_vol_micgain,	MIC1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic2Gain),	mc_asoc_vol_micgain,	MIC2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic3Gain),	mc_asoc_vol_micgain,	MIC3_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_HpGain),		mc_asoc_vol_hpgain,		HPGAIN_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswD_Adc0AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Adc1AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir0AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir2AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_PdmAeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_MixAeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS}
};

#ifdef ALSA_VER_1_0_23
#else
struct mc_asoc_priv {
	struct mc_asoc_data		data;
/*	struct mc_asoc_setup	setup;*/
};
#endif

static struct i2c_client	*mc_asoc_i2c_d;
static struct i2c_client	*mc_asoc_i2c_a;

static UINT8	mc_asoc_hwid	= 0;

static UINT8	mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_AE_EX_OFF;
static UINT8	mc_asoc_ae_dir			= MC_ASOC_AE_NONE;
static UINT8	mc_asoc_cdsp_dir		= MC_ASOC_CDSP_NONE;
static UINT8	mc_asoc_hold			= YMC_NOTITY_HOLD_OFF;

static MCDRV_VOL_INFO	mc_asoc_vol_info_mute;

#if (defined ALSA_VER_1_0_23) || (defined KERNEL_2_6_35_36_COMMON)
static struct snd_soc_codec	*mc_asoc_codec	= 0;
#endif

static UINT8	mc_asoc_main_mic		= MAIN_MIC;
static UINT8	mc_asoc_sub_mic			= SUB_MIC;
static UINT8	mc_asoc_hs_mic			= HEADSET_MIC;
static UINT8	mc_asoc_ap_play_port	= AP_PLAY_PORT;
static UINT8	mc_asoc_ap_cap_port		= AP_CAP_PORT;
static UINT8	mc_asoc_cp_port			= CP_PORT;
static UINT8	mc_asoc_bt_port			= BT_PORT;
static UINT8	mc_asoc_hf				= HANDSFREE;
static UINT8	mc_asoc_mic1_bias		= MIC1_BIAS;
static UINT8	mc_asoc_mic2_bias		= MIC2_BIAS;
static UINT8	mc_asoc_mic3_bias		= MIC3_BIAS;
static UINT8	mc_asoc_sp				= SP_TYPE;
static UINT8	mc_asoc_ae_src_play		= AE_SRC_PLAY;
static UINT8	mc_asoc_ae_src_cap		= AE_SRC_CAP;
static UINT8	mc_asoc_ae_src_cap_def	= AE_SRC_CAP;
static UINT8	mc_asoc_ae_priority		= AE_PRIORITY;
static UINT8	mc_asoc_cdsp_priority	= CDSP_PRIORITY;
static UINT8	mc_asoc_sidetone		= SIDETONE;

/* QNET Audio:2012-01-23 Earphone bias control start */
static UINT8	mc_asoc_hpsw_flag;
static UINT8	mc_asoc_hpmic_flag;
/* QNET Audio:2012-01-23 Earphone bias control end */

/* FUJITSU: 2013-01-18 ADD-S */
static DEFINE_MUTEX(interface_mutex);
/* FUJITSU: 2013-01-18 ADD-E */

/* QNET Audio:2012-05-24 YAMAHA MUTE  start */
#define	MUTE_TIME	10					// interval
/* QNET Audio:2012-06-03 start */
#define MUTE_COUNT	20					// MUTE time(MUTE_TIME*MUTE_COUNT ms)
/* QNET Audio:2012-06-03 end */
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
#define MUTE_COUNT_ENFORCED	20			// MUTE time(MUTE_TIME*MUTE_COUNT ms)
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */

static	int	old_OutputDevice;			// pre-output devices
static struct timer_list MUTE_timer;	// struct timer
struct work_struct workq;				// MUTE cancel workqueue 
static int mute_timer_flag=0;			// timer triger flg(For double registration prevention )
static int mute_time_count=0;			// MUTE timer counter
static int mute_yamaha_flag=0;			// MUTE flg
/* QNET Audio:2012-05-24 YAMAHA MUTE end */

/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
struct work_struct disdev_workq;		// disable devices workqueue.

static struct timer_list disdev_timer;	// struct timer.

static int disdev_timer_flag=0;			// timer triger flg(For double registration prevention).
static int disdev_time_count=0;			// disable devices timer counter.
static int disdev_yamaha_flag=1;		// disable devices flg(-1:enable / 0:enable->disable / 1:disable).

#define	DISDEV_TIME		10				// interval.
#define DISDEV_COUNT	50				// disable devices time(DISDEV_TIME*DISDEV_COUNT ms).
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */

/*
 * Function
 */
 
/* QNET Audio:2012-01-17 Basic Feat start */

#define	EARPHONE_R			0x0001			// earphone R
#define	EARPHONE_L			0x0002			// earphone L
#define	SPEAKER				0x0004			// speaker
#define	RECEIVER			0x0008			// Receiver
/* QNET Audio:2012-01-24 YAMAHA Loopback control start */
#define LINEOUT			0x0010			// line output
/* QNET Audio:2012-01-24 YAMAHA Loopback control end */
static	int	OutputDevice;

#define	MIC_MAIN			0x0001			// main mic
#define	MIC_SUB				0x0002			// sub mic
#define	MIC_EARPHONE		0x0004			// earphone mic
#define MIC_GAIN_DEFAULT	0x0008			// volume default
static	int	InputDevice;

#define	WOLFSON_INPUT_USE		0x0001			// INPUT WM0010 USE
#define	WOLFSON_OUTPUT_USE		0x0002			// OUTPUT WM0010 USE
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
#define VOLUME_MUTE				0x0010			// MUTE output
#define FORCE_MUTE				0x0100			// FORCE MUTE OUTPUT
/* QNET Audio:2012-02-29 YAMAHA MUTE end */
static	int	DspDevice;
/* QNET Audio:2012-04-10 11_2nd Feat start */
static int g_force_mute = 0;
/* QNET Audio:2012-04-10 11_2nd Feat   end */

/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
static int g_stream_enforced = 0;
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */

static struct msm_xo_voter *xo_cxo;

/* QNET Audio:2012-02-14 YAMAHA default volume set start */
static int ymu_volume_l(int amp, int vol);

/* QNET Audio:2012-03-18/ 2012-04-10 YAMAHA Input Gain set start */
/* FUJITSU: 20130-01-17 modify start */
#define DEFAULT_MICGAIN         150
#define DEFAULT_ADVOL           0
/* FUJITSU: 20130-01-17 modify end */
/* QNET Audio:2012-03-18/ 2012-04-10 YAMAHA Input Gain set end */
/* QNET Audio:2012-02-14 YAMAHA default volume set end */

/* QNET Audio:2012-03-14 Clock Supply start */
static void mc_yamaha_clock_set(int mode, int pmr/*obsolete*/)
{
	int rc;
	unsigned int r;
	static unsigned int refcnt;

	r = refcnt;
	if (mode == MSM_XO_MODE_ON) {
		refcnt++;
		if (!refcnt) {
			refcnt--;
		}
	} else {
		if (refcnt) {
			refcnt--;
		}
	}
	if (!r ^ !refcnt) {
		rc = msm_xo_mode_vote(xo_cxo, mode);
		if (rc < 0) {
			pr_err("%s: Configuring MSM_XO_MODE_ON failed (%d)\n",__func__, rc);
		} else if (mode == MSM_XO_MODE_ON) {
			usleep(1000);
		}
	}
}
//EXPORT_SYMBOL_GPL(mc_yamaha_clock_set);
/* QNET Audio:2012-03-14  Clock Supply end */

static void clk_ctrl_for_wolfson(int dsp)
{
    static bool curr = false;
    dsp &= WOLFSON_OUTPUT_USE | WOLFSON_INPUT_USE;
    if (!curr && dsp) {
        curr = true;
        mc_yamaha_clock_set(MSM_XO_MODE_ON,2);
    } else if (curr && !dsp) {
        curr = false;
        mc_yamaha_clock_set(MSM_XO_MODE_OFF,2);
    }
}

/****************************************************************************************
 FUJITSU:2013/01/18

 FUNCTION : Volume Cache Controls

 Remarks: described with only using volumes
*****************************************************************************************/
static MCDRV_VOL_INFO volcache;
enum {
	VOLGRP_IN  = 1,
	VOLGRP_OUT = 2,
	VOLGRP_ALL = VOLGRP_IN | VOLGRP_OUT
};
#define YMUVOLINFO(g,mbr,db) {g,offsetof(MCDRV_VOL_INFO,mbr),ARRAY_SIZE(((MCDRV_VOL_INFO*)0)->mbr),(db) << 8}
static const struct {
	int grp;
	int offset;
	int num;
	int resetv;
} ymu_volume_info[] = {
	YMUVOLINFO(VOLGRP_IN,  aswA_Mic1Gain, 15),
	YMUVOLINFO(VOLGRP_IN,  aswA_Mic2Gain, 15),
	YMUVOLINFO(VOLGRP_IN,  aswA_Mic3Gain, 15),
	YMUVOLINFO(VOLGRP_IN,  aswA_Ad0,       0),
	YMUVOLINFO(VOLGRP_IN,  aswA_Mic1,      0),
	YMUVOLINFO(VOLGRP_IN,  aswA_Mic2,      0),
	YMUVOLINFO(VOLGRP_IN,  aswA_Mic3,      0),
	YMUVOLINFO(VOLGRP_IN,  aswA_Lout1,     0),
	YMUVOLINFO(VOLGRP_IN,  aswA_Lout2,     0),
	YMUVOLINFO(VOLGRP_IN,  aswD_Ad0,       0),
	YMUVOLINFO(VOLGRP_IN,  aswD_Aeng6,     0),
	YMUVOLINFO(VOLGRP_IN,  aswD_Dit1,      0),
	YMUVOLINFO(VOLGRP_OUT, aswA_Lin1,      3),
	YMUVOLINFO(VOLGRP_OUT, aswA_HpGain,    0),
	YMUVOLINFO(VOLGRP_OUT, aswA_Hp,        0),
	YMUVOLINFO(VOLGRP_OUT, aswA_Sp,        0),
	YMUVOLINFO(VOLGRP_OUT, aswA_Rc,        0),
	YMUVOLINFO(VOLGRP_OUT, aswD_Dir1,      0),
	YMUVOLINFO(VOLGRP_OUT, aswD_Dir1Att,   0),
	YMUVOLINFO(VOLGRP_OUT, aswD_DacMaster, 0),
	YMUVOLINFO(VOLGRP_OUT, aswD_DacAtt,    0),
};
#define VOLV(v,ti,ai) ((SINT16*)((char*)(v) + ymu_volume_info[ti].offset))[ai]

static void reset_volcache(int grp) {
	int i,j;
	for (i = 0;i < ARRAY_SIZE(ymu_volume_info);i++) {
		if (ymu_volume_info[i].grp & grp) {
			for (j = 0;j < ymu_volume_info[i].num;j++) {
				VOLV(&volcache,i,j) = ymu_volume_info[i].resetv;
			}
		}
	}
}

static void update_volcache(const MCDRV_VOL_INFO *source) {
	int i,j,v;
	for (i = 0;i < ARRAY_SIZE(ymu_volume_info);i++) {
		for (j = 0;j < ymu_volume_info[i].num;j++) {
			v = VOLV(source,i,j);
			if (v & MCDRV_VOL_UPDATE) {
				VOLV(&volcache,i,j) = v & ~MCDRV_VOL_UPDATE;
			}
		}
	}
}

static void prepare_volinfo_by_volcache(MCDRV_VOL_INFO *volinfo) {
	int i,j,v;
	MCDRV_VOL_INFO curr;
	McDrv_Ctrl(MCDRV_GET_VOLUME,&curr,0);
	memset(volinfo,0,sizeof*volinfo);
	for (i = 0;i < ARRAY_SIZE(ymu_volume_info);i++) {
		for (j = 0;j < ymu_volume_info[i].num;j++) {
			v = VOLV(&volcache,i,j);
			if (VOLV(&curr,i,j) != v) {
				VOLV(volinfo,i,j) = v | MCDRV_VOL_UPDATE;
			}
		}
	}
}

/* FUJITSU:2013-05-08 him speaker pl st */
#include <linux/kthread.h>
extern bool speakerrecv_pl_flg;
static int sp_vol_step_up_thread(void * unused){
	int    volume;
	bool   exitflg = false;
	MCDRV_VOL_INFO	vol_set;		// volume
	memset(&vol_set, 0, sizeof(vol_set));

	usleep(500*1000);
	volume = -160;
	vol_set.aswA_Sp[0] = 
	vol_set.aswA_Sp[1] = ((volume * 256) / 10) | MCDRV_VOL_UPDATE;
	if(((volume * 256) / 10) > volcache.aswA_Sp[0]) {
		vol_set.aswA_Sp[0] = 
		vol_set.aswA_Sp[1] = volcache.aswA_Sp[0] | MCDRV_VOL_UPDATE;
		exitflg = true;
	}
	_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
    printk("speakerrecv_pl step1\n");
	if(exitflg) return 0;

	usleep(500*1000);
	volume = -100;
	vol_set.aswA_Sp[0] = 
	vol_set.aswA_Sp[1] = ((volume * 256) / 10) | MCDRV_VOL_UPDATE;
	if(((volume * 256) / 10) > volcache.aswA_Sp[0]) {
		vol_set.aswA_Sp[0] = 
		vol_set.aswA_Sp[1] = volcache.aswA_Sp[0] | MCDRV_VOL_UPDATE;
		exitflg = true;
	}
	_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
    printk("speakerrecv_pl step2\n");
	if(exitflg) return 0;

	usleep(500*1000);
	volume = -60;
	vol_set.aswA_Sp[0] = 
	vol_set.aswA_Sp[1] = ((volume * 256) / 10) | MCDRV_VOL_UPDATE;
	if(((volume * 256) / 10) > volcache.aswA_Sp[0]) {
		vol_set.aswA_Sp[0] = 
		vol_set.aswA_Sp[1] = volcache.aswA_Sp[0] | MCDRV_VOL_UPDATE;
		exitflg = true;
	}
	_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
    printk("speakerrecv_pl step3\n");
	if(exitflg) return 0;

	usleep(500*1000);
	volume = -30;
	vol_set.aswA_Sp[0] = 
	vol_set.aswA_Sp[1] = ((volume * 256) / 10) | MCDRV_VOL_UPDATE;
	if(((volume * 256) / 10) > volcache.aswA_Sp[0]) {
		vol_set.aswA_Sp[0] = 
		vol_set.aswA_Sp[1] = volcache.aswA_Sp[0] | MCDRV_VOL_UPDATE;
		exitflg = true;
	}
	_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
    printk("speakerrecv_pl step4\n");
	if(exitflg) return 0;

	usleep(500*1000);
	volume = 0;
	vol_set.aswA_Sp[0] = 
	vol_set.aswA_Sp[1] = ((volume * 256) / 10) | MCDRV_VOL_UPDATE;
	if(((volume * 256) / 10) > volcache.aswA_Sp[0]) {
		vol_set.aswA_Sp[0] = 
		vol_set.aswA_Sp[1] = volcache.aswA_Sp[0] | MCDRV_VOL_UPDATE;
		exitflg = true;
	}
	_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
    printk("speakerrecv_pl step5\n");

	return 0;
}
/* FUJITSU:2013-05-08 him speaker pl ed */

#define MCDRV_VOL_MUTE_UPDATE (-96 << 8 | MCDRV_VOL_UPDATE)

 /****************************************************************************************
 FUJITSU:2011/12/13 
 
 FUNCTION : ymu_selecter_work_init((MCDRV_CHANNEL *)channel)
 Details  : All the sauce inputted into the selector of YMU829 is canceled.

*****************************************************************************************/
void ymu_selecter_work_init(MCDRV_CHANNEL * channel)
{
	channel->abSrcOnOff[0]=MCDRV_SRC0_MIC1_OFF | MCDRV_SRC0_MIC2_OFF | MCDRV_SRC0_MIC3_OFF;									//MIC
	channel->abSrcOnOff[1]=MCDRV_SRC1_LINE1_L_OFF | MCDRV_SRC1_LINE1_R_OFF | MCDRV_SRC1_LINE1_M_OFF;						//LINE1
	channel->abSrcOnOff[2]=MCDRV_SRC2_LINE2_L_OFF | MCDRV_SRC2_LINE2_R_OFF | MCDRV_SRC2_LINE2_M_OFF;						//LINE2
	channel->abSrcOnOff[3]=MCDRV_SRC3_DIR0_OFF | MCDRV_SRC3_DIR1_OFF | MCDRV_SRC3_DIR2_OFF | MCDRV_SRC3_DIR2_DIRECT_OFF;	//DIR
	channel->abSrcOnOff[4]=MCDRV_SRC4_DTMF_OFF | MCDRV_SRC4_PDM_OFF | MCDRV_SRC4_ADC0_OFF | MCDRV_SRC4_ADC1_OFF;			// DTMF PDM ADC0 ADC1
	channel->abSrcOnOff[5]=MCDRV_SRC5_DAC_L_OFF | MCDRV_SRC5_DAC_R_OFF | MCDRV_SRC5_DAC_M_OFF;								// DAC
	channel->abSrcOnOff[6]=MCDRV_SRC6_MIX_OFF | MCDRV_SRC6_AE_OFF | MCDRV_SRC6_CDSP_OFF | MCDRV_SRC6_CDSP_DIRECT_OFF; 		// MIX AE C-DSP C-DSP Direct
}

/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
static void disdev_timer_regist(void);
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */

/* QNET Audio:2012-05-24 YAMAHA MUTE start */
static void mute_timer_regist(void);

void mute_func(struct work_struct *work)
{
	MCDRV_VOL_INFO	vol_set;
	mutex_lock(&interface_mutex);/* FUJITSU: 2013-01-18 ADD */
/* QNET Audio:2012-06-03 start */
	//if (g_force_mute != 1){
/* QNET Audio:2012-06-03 end */
		DBG_PRINTK("OUTPUT DEVICE MUTE RELEASE");
		memset(&vol_set, 0, sizeof(vol_set));
		//vol_set.aswA_Hp[1] = volcache.aswA_Hp[1] | MCDRV_VOL_UPDATE;
		//vol_set.aswA_Hp[0] = volcache.aswA_Hp[0] | MCDRV_VOL_UPDATE;
		//vol_set.aswA_Sp[0] = volcache.aswA_Sp[0] | MCDRV_VOL_UPDATE;
		//vol_set.aswA_Rc[0] = volcache.aswA_Rc[0] | MCDRV_VOL_UPDATE;
		vol_set.aswD_DacAtt[1] = volcache.aswD_DacAtt[1] | MCDRV_VOL_UPDATE;
		vol_set.aswD_DacAtt[0] = volcache.aswD_DacAtt[0] | MCDRV_VOL_UPDATE;
		_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
/* QNET Audio:2012-06-03 start */
	//}
/* QNET Audio:2012-06-03 end */
	mute_yamaha_flag = 0;
	mutex_unlock(&interface_mutex);/* FUJITSU: 2013-01-18 ADD */
}

static void MUTE_timer_handler( unsigned long data )
{
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
	int mute_time_upper =0;
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
	mute_timer_flag=0;
	mute_time_count++;
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
	if(g_stream_enforced)
	{
		mute_time_upper = MUTE_COUNT_ENFORCED;
	}
	else
	{
		mute_time_upper = MUTE_COUNT;
	}
//	if (mute_time_count > MUTE_COUNT)
	if (mute_time_count > mute_time_upper)
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
	{
		DBG_PRINTK("Audio work que start timer=[%d]",mute_time_upper);
		schedule_work(&workq);
	}
	else
	{
		mute_timer_regist();
	}
}

static void mute_timer_regist()
{
	if (!mute_timer_flag)
	{
		mute_timer_flag=1;
		init_timer( &MUTE_timer );
		MUTE_timer.expires  = jiffies + (HZ / (1000/MUTE_TIME));
		MUTE_timer.data     = ( unsigned long )jiffies;
		MUTE_timer.function = MUTE_timer_handler;
		add_timer( &MUTE_timer );
		mute_yamaha_flag = 1;
	}
}
/* QNET Audio:2012-05-24 YAMAHA MUTE end */

/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
void disdev_func(struct work_struct *work)
{
	mutex_lock(&interface_mutex);/* FUJITSU: 2013-01-18 ADD */
	if(disdev_yamaha_flag == 0){
		DBG_PRINTK("OUTPUT DEVICE is Disable");
		disdev_yamaha_flag = 1;		//disdev flag.
	}
	mutex_unlock(&interface_mutex);/* FUJITSU: 2013-01-18 ADD */
}

static void disdev_timer_handler( unsigned long data )
{
	disdev_timer_flag=0;
	disdev_time_count++;
	if (disdev_time_count > DISDEV_COUNT)
	{
		DBG_PRINTK("Audio work disdev que start");
		schedule_work(&disdev_workq);
	}
	else
	{
		disdev_timer_regist();
	}
}

static void disdev_timer_regist()
{
	if (!disdev_timer_flag)
	{
		disdev_timer_flag=1;
		init_timer( &disdev_timer );
		disdev_timer.expires  = jiffies + (HZ / (1000/DISDEV_TIME));
		disdev_timer.data     = ( unsigned long )jiffies;
		disdev_timer.function = disdev_timer_handler;
		add_timer( &disdev_timer );
	}
}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */

/****************************************************************************************
 FUJITSU:2011/12/13 

 FUNCTION : ymu_control(int scene)
 EXPLAIN  : YMU829 Scene Select
 
 INPUT    : scene		

 OUTPUT   : error status
*****************************************************************************************/
static int ymu_control_l(int scene, int param)
{
	int		err = MCDRV_SUCCESS;
	struct mc_asoc_setup	*setup;
	UINT32	update;
	MCDRV_PATH_INFO path_set;		// peth
	MCDRV_VOL_INFO	vol_set;		// volume

	DEBUGPRINT1("start",scene);
	switch(scene)
	{
		//////////////////////////////////////////////////////////////////////////////////
		//  COMMAND for CONTROL
		//////////////////////////////////////////////////////////////////////////////////
		//--------------------------------------------------------------------------------
		case YMU_INIT:					// initializes at the time of starting
			DEBUGPRINT0("YMU_INIT");
			InputDevice = 0;
			OutputDevice = 0;
			DspDevice = 0;
			clk_ctrl_for_wolfson(DspDevice);
			setup	 = &mc_asoc_cfg_setup;
			update	= 0;

/* QNET Audio:2012-01-23 Earphone bias control start */
			mc_asoc_hpsw_flag = 0;
			mc_asoc_hpmic_flag = 0;
/* QNET Audio:2012-01-23 Earphone bias control end */

			err	= _McDrv_Ctrl(MCDRV_INIT, &setup->init, 0);
			update |= (MCDRV_DIO1_COM_UPDATE_FLAG | MCDRV_DIO1_DIR_UPDATE_FLAG | MCDRV_DIO1_DIT_UPDATE_FLAG);
			err	= _McDrv_Ctrl(MCDRV_SET_DIGITALIO, (void *)&stDioInfo_Default, update);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("MCDRV_SET_DIGITALIO");
				break;
			}
			err	= _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&stDacInfo_Default, 0x7);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("MCDRV_SET_DAC");
				break;
			}
			err	= _McDrv_Ctrl(MCDRV_SET_ADC, (void *)&stAdcInfo_Default, 0x7);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("MCDRV_SET_ADC");
				break;
			}
			err	= _McDrv_Ctrl(MCDRV_SET_SP, (void *)&stSpInfo_Default, 0);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("MCDRV_SET_SP");
				break;
			}
			err	= _McDrv_Ctrl(MCDRV_SET_DNG, (void *)&stDngInfo_Default, 0x7F3F3F);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("MCDRV_SET_DNG");
				break;
			}
			err	= _McDrv_Ctrl(MCDRV_SET_SYSEQ, (void *)&stSyseqInfo_Default, 0x3);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("MCDRV_SET_SYSEQ");
				break;
			}
			err	= ymu_control_l(YMU_CIRCUIT_ALLOFF,0);
			break;

		//--------------------------------------------------------------------------------
		case YMU_TERM:					// end
			DEBUGPRINT0("YMU_TERM");
			InputDevice = 0;
			OutputDevice = 0;
			err = ymu_control_l(YMU_CIRCUIT_ALLOFF, 0);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT0("YMU_INTERNAL_CIRCUIT_OFF");
				break;
			}
			err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
			break;

		//////////////////////////////////////////////////////////////////////////////////
		// SETTING for OUTPUT
		//////////////////////////////////////////////////////////////////////////////////
		case YMU_EARPHONE_R:				// earphone R
			DEBUGPRINT0("YMU_EARPHONE_R");
			OutputDevice |= EARPHONE_R;
			break;
		case YMU_EARPHONE_L:				// earphone L
			DEBUGPRINT0("YMU_EARPHONE_L");
			OutputDevice |= EARPHONE_L;
			break;
		case YMU_SPEAKER:					// speaker
			DEBUGPRINT0("YMU_SPEAKER");
			OutputDevice |= SPEAKER;
			break;
		case YMU_RECEIVER:					// receiver
			DEBUGPRINT0("YMU_RECEIVER");
			OutputDevice |= RECEIVER;
			break;
/* QNET Audio:2012-01-24 YAMAHA Loopback control start */
		case YMU_LINEOUT:
			DEBUGPRINT0("YMU_LINEOUT");
			OutputDevice |= LINEOUT;
			break;
/* QNET Audio:2012-01-24 YAMAHA Loopback control end */
		case YMU_OUTPUT_OFF:				// output devices OFF
			DEBUGPRINT0("YMU_OUTPUT_OFF");
/* FUJITSU Audio:2013-07-10 HI-S-2335 receiver amp off start */
			if((system_rev & 0xf0) == 0x10/*him*/ &&
			   (system_rev & 0x0f) >= 0x03/*1.3*/) {
				gpio_set_value(33,0);
				printk( "[spk_rcv] GPIO33 set L %d \n",gpio_get_value(33));
			} else {
				gpio_set_value(70,0);
				printk( "[spk_rcv] GPIO70 set L %d \n",gpio_get_value(70));
			}
/* FUJITSU Audio:2013-07-10 HI-S-2335 receiver amp off end */
			OutputDevice = 0;
			memset(&path_set, 0, sizeof(path_set));
			path_set.asLout1[0].abSrcOnOff[5] |= MCDRV_SRC5_DAC_L_OFF;
			path_set.asLout1[1].abSrcOnOff[5] |= MCDRV_SRC5_DAC_R_OFF;
			path_set.asLout1[0].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_L_OFF;
			path_set.asLout1[1].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_R_OFF;
			ymu_selecter_work_init(&path_set.asHpOut[0]);	// earphone L
			ymu_selecter_work_init(&path_set.asHpOut[1]);	// earphone R
			ymu_selecter_work_init(&path_set.asSpOut[0]);	// speaker
			ymu_selecter_work_init(&path_set.asRcOut[0]);	// receiver
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
			DspDevice &= ~VOLUME_MUTE;
/* QNET Audio:2012-02-29 YAMAHA MUTE end */
			DspDevice &= ~WOLFSON_OUTPUT_USE;
			if (!DspDevice)
			{
/* FUJITSU:2012-11-29  MOD-S uncommented */
				ymu_selecter_work_init(&path_set.asDit1[0]);	// IO#1
/* FUJITSU:2012-11-29  MOD-E */
				ymu_selecter_work_init(&path_set.asMix[0]);		// mixer input
				ymu_selecter_work_init(&path_set.asDac[0]); 	// MASTER route
				ymu_selecter_work_init(&path_set.asDac[1]);		// VOICE route
			}
/* QNET Audio:2012-03-14  Clock Supply start */
			mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
			err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
			mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply end */
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
			if(disdev_yamaha_flag != 1){
				disdev_yamaha_flag = 0;
				disdev_time_count = 0;
				disdev_timer_regist();
			}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
			clk_ctrl_for_wolfson(DspDevice);
			reset_volcache(VOLGRP_OUT);
			break;

		case YMU_OUTPUT_USE_WM0010:
			DEBUGPRINT0("YMU_OUTPUT_USE_WM0010");
			DspDevice |= WOLFSON_OUTPUT_USE;
			clk_ctrl_for_wolfson(DspDevice);
			break;

		//////////////////////////////////////////////////////////////////////////////////
		// SETTING for INPUT
		//////////////////////////////////////////////////////////////////////////////////
		case YMU_MIC_MAIN:					// MAIN MIC
			DEBUGPRINT0("YMU_MIC_MAIN");
			InputDevice |= MIC_MAIN;
			break;
		case YMU_MIC_SUB:					// SUB MIC
			DEBUGPRINT0("YMU_MIC_SUB");
			InputDevice |= MIC_SUB;
			break;
		case YMU_MIC_EARPHONE:				// EARPHONE MIC
			DEBUGPRINT0("YMU_MIC_EARPHONE");
			InputDevice |= MIC_EARPHONE;
			break;
/* QNET Audio:2012-02-14 YAMAHA default volume set start */
		case YMU_GAIN_DEFAULT_SET:
			DEBUGPRINT0("YMU_VOLUME_DEFAULT");
			InputDevice |= MIC_GAIN_DEFAULT;
			break;
/* QNET Audio:2012-02-14 YAMAHA default volume set end */
		case YMU_INPUT_OFF:					// input devices OFF
			DEBUGPRINT0("YMU_INPUT_OFF");
			InputDevice = 0;
			memset(&path_set, 0, sizeof(path_set));
			path_set.asLout1[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC1_OFF | MCDRV_SRC0_MIC2_OFF |MCDRV_SRC0_MIC3_OFF;
			path_set.asLout1[1].abSrcOnOff[0] |= MCDRV_SRC0_MIC1_OFF | MCDRV_SRC0_MIC2_OFF |MCDRV_SRC0_MIC3_OFF;
			ymu_selecter_work_init(&path_set.asAdc0[0]);	// ADCL
			ymu_selecter_work_init(&path_set.asAdc0[1]);	// ADCR
			ymu_selecter_work_init(&path_set.asBias[0]);	
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
			DspDevice &= ~VOLUME_MUTE;
/* QNET Audio:2012-02-29 YAMAHA MUTE end */
			DspDevice &= ~WOLFSON_INPUT_USE;
			if (!DspDevice)
			{
				ymu_selecter_work_init(&path_set.asDit1[0]);	// IO#1
				ymu_selecter_work_init(&path_set.asMix[0]);		// mixer input
				ymu_selecter_work_init(&path_set.asDac[0]); 	// MASTER route
				ymu_selecter_work_init(&path_set.asDac[1]);		// VOICE route
			}
/* QNET Audio:2012-01-23 Earphone bias control start */
			mc_asoc_hpmic_flag = 0;
			if (mc_asoc_hpsw_flag)
			{
				path_set.asBias[0].abSrcOnOff[0]  |= MCDRV_SRC0_MIC3_ON; /* MBS3 on */
			}
/* QNET Audio:2012-01-23 Earphone bias control end */
			
/* QNET Audio:2012-03-14  Clock Supply start */
			mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
			err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
			mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply start */
			clk_ctrl_for_wolfson(DspDevice);
			reset_volcache(VOLGRP_IN);
			break;

		case YMU_INPUT_USE_WM0010:
			DEBUGPRINT0("YMU_INPUT_USE_WM0010");
			DspDevice |= WOLFSON_INPUT_USE;
			clk_ctrl_for_wolfson(DspDevice);
			break;

		//////////////////////////////////////////////////////////////////////////////////
		// COMMAND for CONTROL
		//////////////////////////////////////////////////////////////////////////////////
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
		case YMU_STREAM_ENFORCED:
			if (param==1){
				DEBUGPRINT0("YMU_STREAM_ENFORCED ON");
				g_stream_enforced = 1;
			}else if (param==0){
				DEBUGPRINT0("YMU_STREAM_ENFORCED OFF");
				g_stream_enforced = 0;
			}
			break;
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
		case YMU_VOLUME_MUTE:
/* QNET Audio:2012-04-10 11_2nd Feat start */
			if (param==1)
			{
				DEBUGPRINT0("YMU_FORCE_MUTE_ON");
				g_force_mute =1;
				break;
			}
			if (param==2)
			{
				DEBUGPRINT0("YMU_FORCE_MUTE_OFF");
				g_force_mute =2;
				break;
			}
/* QNET Audio:2012-04-10 11_2nd Feat   end */
			DEBUGPRINT0("YMU_VOLUME_MUTE");
			DspDevice |= VOLUME_MUTE;
			break;
/* QNET Audio:2012-02-29 YAMAHA MUTE end */

		case YMU_CIRCUIT_ALLOFF:			
			DEBUGPRINT0("YMU_CIRCUIT_ALLOFF");
			memset(&path_set, 0, sizeof(path_set));
			ymu_selecter_work_init(&path_set.asHpOut[0]);	// earphone L
			ymu_selecter_work_init(&path_set.asHpOut[1]);	// earphone R
			ymu_selecter_work_init(&path_set.asSpOut[0]);	// speaker
			ymu_selecter_work_init(&path_set.asSpOut[1]);	// with no devices
			ymu_selecter_work_init(&path_set.asRcOut[0]);	// receiver
			ymu_selecter_work_init(&path_set.asLout1[0]);	// TO WCD9310_MIC IN1 P/N
			ymu_selecter_work_init(&path_set.asLout1[1]);	// TO WCD9310_MIC IN1 P/N
			ymu_selecter_work_init(&path_set.asLout2[0]);	// with no devices
			ymu_selecter_work_init(&path_set.asLout2[1]);	// with no devices
			ymu_selecter_work_init(&path_set.asDit0[0]);	// with no devices
			ymu_selecter_work_init(&path_set.asDit1[0]);	// IO#1
			ymu_selecter_work_init(&path_set.asDit2[0]);	// with no devices
			ymu_selecter_work_init(&path_set.asDac[0]); 	// MASTER route
			ymu_selecter_work_init(&path_set.asDac[1]);		// VOICE route
			ymu_selecter_work_init(&path_set.asAe[0]);		// DSP's source (Digital)
			ymu_selecter_work_init(&path_set.asAdc0[0]);	// ADCL(digital)
			ymu_selecter_work_init(&path_set.asAdc0[1]);	// ADCR(digital)
			ymu_selecter_work_init(&path_set.asMix[0]);		// mixer input
			ymu_selecter_work_init(&path_set.asBias[0]);	// mic bias
/* QNET Audio:2012-01-23 Earphone bias control start */
			mc_asoc_hpmic_flag = 0;
			if (mc_asoc_hpsw_flag)
			{
				path_set.asBias[0].abSrcOnOff[0]  |= MCDRV_SRC0_MIC3_ON; /* MBS3 on */
			}
/* QNET Audio:2012-01-23 Earphone bias control end */
/* QNET Audio:2012-03-14  Clock Supply start */
			mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
			err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
			mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply end */
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
			if(disdev_yamaha_flag != 1){
				disdev_yamaha_flag = 0;
				disdev_time_count = 0;
				disdev_timer_regist();
			}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
			reset_volcache(VOLGRP_ALL);
			break;

		case YMU_CIRCUIT_SET:				// CIRCUIT_SET
			DEBUGPRINT0("YMU_CIRCUIT_SET");
			printk( "YMU_CIRCUIT_SET\n");

			/////////////////////////////////////////////////////////////////
			// CONNECT to OUTPUT route
			/////////////////////////////////////////////////////////////////
			memset(&path_set, 0, sizeof(path_set));
//			memset(&vol_set, 0, sizeof(vol_set));
			prepare_volinfo_by_volcache(&vol_set);

			// earphone R
			if (OutputDevice & EARPHONE_R)
			{
				if (DspDevice & WOLFSON_OUTPUT_USE)
				{
					path_set.asHpOut[1].abSrcOnOff[5] |= MCDRV_SRC5_DAC_R_ON;
				}
				else
				{
//					vol_set.aswA_Lin1[0] = 
//					vol_set.aswA_Lin1[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
					path_set.asHpOut[1].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_R_ON;
				}
//				vol_set.aswA_Hp[1] =
//				vol_set.aswA_HpGain[0] = 0 | MCDRV_VOL_UPDATE; /* 0(dB) * 256 | MCDRV_VOL_UPDATE */
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
				if (DspDevice & VOLUME_MUTE)
				{
					vol_set.aswA_Hp[1] = MCDRV_VOL_MUTE_UPDATE;
				}
/* QNET Audio:2012-02-29 YAMAHA MUTE end */

			}
			// eraphone L
			if (OutputDevice & EARPHONE_L)
			{
				if (DspDevice & WOLFSON_OUTPUT_USE)
				{
					path_set.asHpOut[0].abSrcOnOff[5] |= MCDRV_SRC5_DAC_L_ON;
				}
				else
				{
//					vol_set.aswA_Lin1[0] = 
//					vol_set.aswA_Lin1[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
					path_set.asHpOut[0].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_L_ON;
				}
//				vol_set.aswA_Hp[0] =
//				vol_set.aswA_HpGain[0] = 0 | MCDRV_VOL_UPDATE; /* 0(dB) * 256 | MCDRV_VOL_UPDATE */
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
				if (DspDevice & VOLUME_MUTE)
				{
					vol_set.aswA_Hp[0] = MCDRV_VOL_MUTE_UPDATE;
				}
/* QNET Audio:2012-02-29 YAMAHA MUTE end */
			}
			// speaker
			if (OutputDevice & SPEAKER)
			{
/* FUJITSU:2013-04-08 Spk/Rcv for him start */
                printk( "[spk_rcv] YMH speaker!\n");
                gpio_set_value(71,0);
                printk( "[spk_rcv] GPIO71 set L %d \n",gpio_get_value(71));
/* FUJITSU:2013-04-23 Rcv amp for him st */
                if((system_rev & 0xf0) == 0x10/*him*/ &&
                   (system_rev & 0x0f) >= 0x03/*1.3*/) {
                    gpio_set_value(33,0);
                    printk( "[spk_rcv] GPIO33 set L %d \n",gpio_get_value(33));
                }else {
                    gpio_set_value(70,0);
                    printk( "[spk_rcv] GPIO70 set L %d \n",gpio_get_value(70));
                }
/* FUJITSU:2013-04-23 Rcv amp for him end */
/* FUJITSU:2013-04-08 Spk/Rcv for him end */

				if (DspDevice & WOLFSON_OUTPUT_USE)
				{
					// speaker is CONNEDTED to DAC L and DAC R
					path_set.asSpOut[0].abSrcOnOff[5] |= MCDRV_SRC5_DAC_M_ON;
				}
				else
				{
//					vol_set.aswA_Lin1[0] = 
//					vol_set.aswA_Lin1[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
					path_set.asSpOut[0].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_M_ON;
				}
//				vol_set.aswA_Sp[0] = 
//				vol_set.aswA_Sp[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
				if (DspDevice & VOLUME_MUTE)
				{
					vol_set.aswA_Sp[0] = MCDRV_VOL_MUTE_UPDATE;
				}
/* QNET Audio:2012-02-29 YAMAHA MUTE end */
/* FUJITSU:2013-05-08 him speaker pl st */
				{
					if(speakerrecv_pl_flg){
						struct task_struct *sp_vol_step_up_task;
						SINT16 setvolume;
						int		volume = -1000; //mute volume
						setvolume = (volume * 256) / 10;
						setvolume |= MCDRV_VOL_UPDATE;
						vol_set.aswA_Sp[0] = 
						vol_set.aswA_Sp[1] = setvolume;
						speakerrecv_pl_flg = false;
						sp_vol_step_up_task = kthread_run(sp_vol_step_up_thread,NULL,"volstepup");
					}
				}
/* FUJITSU:2013-05-08 him speaker pl ed */
			}
			// receiver
			if (OutputDevice & RECEIVER)
			{
/* FUJITSU:2013-04-08 Spk/Rcv for him start */
                printk( "[spk_rcv] YMH receiver!\n");
                gpio_set_value(71,1);
                printk( "[spk_rcv] GPIO71 set H %d \n",gpio_get_value(71));
/* FUJITSU:2013-04-23 Rcv amp for him st */
                if((system_rev & 0xf0) == 0x10/*him*/ &&
                   (system_rev & 0x0f) >= 0x03/*1.3*/) {
                    gpio_set_value(33,1);
                    printk( "[spk_rcv] GPIO33 set H %d \n",gpio_get_value(33));
                }else {
                    gpio_set_value(70,1);
                    printk( "[spk_rcv] GPIO70 set H %d \n",gpio_get_value(70));
                }
/* FUJITSU:2013-04-23 Rcv amp for him end */
/* FUJITSU:2013-04-08 Spk/Rcv for him end */

				if (DspDevice & WOLFSON_OUTPUT_USE)
				{
					// receiver is CONNECTED to DAC L and DAC R
					path_set.asRcOut[0].abSrcOnOff[5] |= MCDRV_SRC5_DAC_L_ON | MCDRV_SRC5_DAC_R_ON;
				}
				else
				{
//					vol_set.aswA_Lin1[0] = 
//					vol_set.aswA_Lin1[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
					path_set.asRcOut[0].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_M_ON;
				}
//				vol_set.aswA_Rc[0] =  0 | MCDRV_VOL_UPDATE; /* 0(dB) * 256 | MCDRV_VOL_UPDATE */
/* QNET Audio:2012-02-29 YAMAHA MUTE start */
				if (DspDevice & VOLUME_MUTE)
				{
					vol_set.aswA_Rc[0] = MCDRV_VOL_MUTE_UPDATE;
				}
/* QNET Audio:2012-02-29 YAMAHA MUTE end */
			}
/* QNET Audio:2012-01-24 YAMAHA Loopback control start */
			// line output
			if (OutputDevice & LINEOUT)
			{
				if (DspDevice & WOLFSON_OUTPUT_USE)
				{
					// line output is CONNECTED to DAC L and DAC R
					path_set.asLout1[0].abSrcOnOff[5] |= MCDRV_SRC5_DAC_L_ON ;
					path_set.asLout1[1].abSrcOnOff[5] |= MCDRV_SRC5_DAC_R_ON;
				}
				else
				{
//					vol_set.aswA_Lin1[0] = 
//					vol_set.aswA_Lin1[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
					path_set.asLout1[0].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_L_ON;
					path_set.asLout1[1].abSrcOnOff[1] |= MCDRV_SRC1_LINE1_R_ON;
				}
//				vol_set.aswA_Lout1[0] =  0 | MCDRV_VOL_UPDATE; /* 0(dB) * 256 | MCDRV_VOL_UPDATE */
//				vol_set.aswA_Lout1[1] =  0 | MCDRV_VOL_UPDATE; /* 0(dB) * 256 | MCDRV_VOL_UPDATE */
			}
/* QNET Audio:2012-01-24 YAMAHA Loopback control end */
			// SETTING to Wolfson route
			if (DspDevice & WOLFSON_OUTPUT_USE)
			{
				path_set.asMix[0].abSrcOnOff[3]  = MCDRV_SRC3_DIR1_ON;			// DIR1 is connected to MIX
				path_set.asDac[0].abSrcOnOff[6]   = MCDRV_SRC6_MIX_ON;			// DAC is connected to MIX
//				path_set.asDac[1].abSrcOnOff[6]   = MCDRV_SRC6_MIX_ON;			// DAC is connected to MIX

//				vol_set.aswD_Dir1[0] = 
//				vol_set.aswD_Dir1[1] = 
//				vol_set.aswD_Dir1Att[0] = 
//				vol_set.aswD_Dir1Att[1] = 
//				vol_set.aswD_DacMaster[0] = 
//				vol_set.aswD_DacMaster[1] = 
//				vol_set.aswD_DacAtt[0] = 
//				vol_set.aswD_DacAtt[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
			}

			/////////////////////////////////////////////////////////////////
			// CONNECTE to INPUT route
			/////////////////////////////////////////////////////////////////

			// main mic
			if (InputDevice & MIC_MAIN)
			{
//				vol_set.aswA_Mic1Gain[0] = 0 | MCDRV_VOL_UPDATE;
				if (DspDevice & WOLFSON_INPUT_USE)
				{
					// mic1 is CONNECTED to ADC0's channel 1
					path_set.asAdc0[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC1_ON;
//					vol_set.aswA_Ad0[0] = 0 | MCDRV_VOL_UPDATE;
				}
				else
				{
//					vol_set.aswA_Mic1[0] = 0 | MCDRV_VOL_UPDATE;
					path_set.asLout1[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC1_ON;
//					vol_set.aswA_Lout1[0] = 0 | MCDRV_VOL_UPDATE;
				}
			}
			// sub mic
			if (InputDevice & MIC_SUB)
			{
//				vol_set.aswA_Mic2Gain[0] = 0 | MCDRV_VOL_UPDATE;
				if (DspDevice & WOLFSON_INPUT_USE)
				{
					// mic 2 is CONNECTED to ADC0's channel 2
//					vol_set.aswA_Ad0[1] = 0 | MCDRV_VOL_UPDATE;
					path_set.asAdc0[1].abSrcOnOff[0] |= MCDRV_SRC0_MIC2_ON;
					if ((InputDevice & MIC_MAIN)==0)
					{
						// If only sub mic, mic 2 is CONNECTED to ADC0's channel 1, too.
//						vol_set.aswA_Ad0[0] = 0 | MCDRV_VOL_UPDATE;
						path_set.asAdc0[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC2_ON;
					}
				}
				else
				{
//					vol_set.aswA_Mic2[0] = 0 | MCDRV_VOL_UPDATE;
					if ((InputDevice & MIC_MAIN)==0)
					{
						// If only sub mic, CONNECTE to LINE's L
						path_set.asLout1[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC2_ON;
//						vol_set.aswA_Lout1[0] = 0 | MCDRV_VOL_UPDATE;
					}
					else
					{
						path_set.asLout1[1].abSrcOnOff[0] |= MCDRV_SRC0_MIC2_ON;
//						vol_set.aswA_Lout1[1] = 0 | MCDRV_VOL_UPDATE;
					}
				}
			}
			// earphone mic
			if (InputDevice & MIC_EARPHONE)
			{
/* QNET Audio:2012-01-23 Earphone bias control start */
				mc_asoc_hpmic_flag = 1;
/* QNET Audio:2012-01-23 Earphone bias control end */
//				vol_set.aswA_Mic3Gain[0] = 0 | MCDRV_VOL_UPDATE;
				if (DspDevice & WOLFSON_INPUT_USE)
				{
					// mic 3 is CONNECTED to ADC0's channel 1 and channel 2
					path_set.asAdc0[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC3_ON;
//					vol_set.aswA_Mic3[0] = 
//					vol_set.aswA_Ad0[0] = 
//					vol_set.aswA_Ad0[1] = 0 | MCDRV_VOL_UPDATE;
				}
				else
				{
//					vol_set.aswA_Mic3[0] = 
//					vol_set.aswA_Lout1[0] = 
//					vol_set.aswA_Lout1[1] = 0 | MCDRV_VOL_UPDATE;
					path_set.asLout1[0].abSrcOnOff[0] |= MCDRV_SRC0_MIC3_ON;
					path_set.asLout1[1].abSrcOnOff[0] |= MCDRV_SRC0_MIC3_ON;
				}
				path_set.asBias[0].abSrcOnOff[0]  |= MCDRV_SRC0_MIC3_ON; /* MBS3 on */

			}
			// Wolfson or not
			if (DspDevice & WOLFSON_INPUT_USE)
			{
				path_set.asDit1[0].abSrcOnOff[4] = MCDRV_SRC4_ADC0_ON;

//				vol_set.aswD_Ad0[0] =
//				vol_set.aswD_Ad0[1] =
//				vol_set.aswD_Aeng6[0]=
//				vol_set.aswD_Aeng6[1]=
//				vol_set.aswD_Dit1[0]=
//				vol_set.aswD_Dit1[1]= 0 | MCDRV_VOL_UPDATE;	
			}
/* QNET Audio:2012-03-14  Clock Supply start */
/* QNET Audio:2012-03-14  Clock Supply end */

/* QNET Audio:2012-05-24 YAMAHA MUTE start */
/* QNET Audio:2012-06-08 start */

			// if kernel mode, not mute
			if (!makercmd_mode)
			{

				// if output devices change, sets mute
				if (OutputDevice && 										// output devices
				    (OutputDevice != old_OutputDevice))						// pre output devices is not now devices
				{
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
					if (disdev_yamaha_flag != 1){
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
						if (!((old_OutputDevice == RECEIVER) && (OutputDevice == SPEAKER)))	
						{
							DBG_PRINTK("OUTPUT DEVICE MUTEING %04x, %04x", old_OutputDevice, OutputDevice);

							//vol_set.aswA_Hp[1] =
							//vol_set.aswA_Hp[0] =
							//vol_set.aswA_Sp[0] =
							//vol_set.aswA_Rc[0] = MCDRV_VOL_MUTE_UPDATE;
							vol_set.aswD_DacAtt[1] =
							vol_set.aswD_DacAtt[0] = MCDRV_VOL_MUTE_UPDATE;
							_McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
							memset(&vol_set, 0, sizeof(vol_set));
							mute_time_count=0;
							mute_timer_regist();
						}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
					}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
					// setting for outputdevices
					old_OutputDevice = OutputDevice;
				}
			}
// QNET Audio:2012-06-21 LO1VOL_L/R -4dB Start
			else
			{
				// if kernel mode, don't set volume.
				vol_set.aswA_Lout1[0] = 
				vol_set.aswA_Lout1[1] = 0 ;
			}
// QNET Audio:2012-06-21 LO1VOL_L/R -4dB End
/* QNET Audio:2012-06-08 end */

/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
			if (OutputDevice){
				DBG_PRINTK("OUTPUT DEVICE is enable");
				disdev_yamaha_flag = -1;
			}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
/* QNET Audio:2012-05-24 YAMAHA MUTE end */

/* QNET Audio:2012-03-14  Clock Supply start */
			mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
			err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
			mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply start */
			if (err)
			{
				DEBUGPRINT1("ERROR:Circuit Rooting ", err);
				break;
			}
/* QNET Audio:2012-04-10 11_2nd Feat start */
			if (g_force_mute ==1)
			{
				// on
				vol_set.aswA_Hp[1] =
				vol_set.aswA_Hp[0] =
				vol_set.aswA_Sp[0] =
				vol_set.aswA_Rc[0] = MCDRV_VOL_MUTE_UPDATE;
			}
			if (g_force_mute ==2)
			{
				// off
//				vol_set.aswA_Hp[1] =0 | MCDRV_VOL_UPDATE; /* -96dB */
//				vol_set.aswA_Hp[0] =0 | MCDRV_VOL_UPDATE; /* MUTE */
//				vol_set.aswA_Sp[0] =0 | MCDRV_VOL_UPDATE; /* -96dB */
//				vol_set.aswA_Rc[0] =0 | MCDRV_VOL_UPDATE; /* -96dB */
				g_force_mute=0;
			}
/* QNET Audio:2012-04-10 11_2nd Feat   end */
/* QNET Audio:2012-05-24 YAMAHA MUTE start */
			if (mute_yamaha_flag)
			{
/* FUJITSU Audio:2013-05-06 TH-S-621 add g_force_mute check Start */
				if (g_force_mute != 1)
				{
					DBG_PRINTK("yamaha: Volume MUTE");
				/* mute-timer working, so suppress userend volume updating */
				vol_set.aswA_Hp[1] =0;
				vol_set.aswA_Hp[0] =0;
				vol_set.aswA_Sp[0] =0;
				vol_set.aswA_Rc[0] =0;
				}
				else
				{
					DBG_PRINTK("g_force_mute_ON: Not yamaha Volume Set");
				}
/* FUJITSU Audio:2013-05-06 TH-S-621 add g_force_mute check End */
			}
/* QNET Audio:2012-05-24 YAMAHA MUTE end */
			err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
			if (err)
			{
				DEBUGPRINT1("ERROR:Volume setting ", err);
				break;
			}
/* QNET Audio:2012-02-14 YAMAHA default volume set start */
			if (InputDevice & MIC_GAIN_DEFAULT)
			{
				if (InputDevice & MIC_MAIN)
				{
					ymu_volume_l(MC1GAIN, DEFAULT_MICGAIN);
				}
				if (InputDevice & MIC_SUB)
				{
					ymu_volume_l(MC2GAIN, DEFAULT_MICGAIN);
				}
				if (InputDevice & MIC_EARPHONE)
				{
					ymu_volume_l(MC3GAIN, DEFAULT_MICGAIN);
				}

/* QNET Audio:2012-03-18/ 2012-04-10 YAMAHA Input Gain set start */
				if (DspDevice & WOLFSON_INPUT_USE)
				{
					ymu_volume_l(ADVOL_L, DEFAULT_ADVOL);
					ymu_volume_l(ADVOL_R, DEFAULT_ADVOL);
				}
/* QNET Audio:2012-03-18/ 2012-04-10 YAMAHA Input Gain set end */
			}

/* QNET Audio:2012-02-14 YAMAHA default volume set end */
			break;

		case YMU_DEGITAL_OUTPUT:			// digital output(test form)
			DEBUGPRINT0("YMU_DEGITAL_OUTPUT");
			// cuts to all input and output path
			err = ymu_control_l(YMU_INTERNAL_CIRCUIT_OFF, 0);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT1("Line Cut Error", err);
				break;
			}
			// connect to output route
			memset(&path_set, 0, sizeof(path_set));
			path_set.asMix[0].abSrcOnOff[3]  = MCDRV_SRC3_DIR1_ON;
			path_set.asDac[0].abSrcOnOff[6]   = MCDRV_SRC6_MIX_ON;
//			path_set.asDac[1].abSrcOnOff[6]   = MCDRV_SRC6_MIX_ON;
			path_set.asSpOut[0].abSrcOnOff[5] = MCDRV_SRC5_DAC_M_ON;
			mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
			err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
			mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT1("Rooting Error", err);
				break;
			}
			// setting to volume
			memset(&vol_set, 0, sizeof(vol_set));
			vol_set.aswD_Dir1[0] = 
			vol_set.aswD_Dir1[1] = 
			vol_set.aswD_Dir1Att[0] = 
			vol_set.aswD_Dir1Att[1] = 
			vol_set.aswD_DacMaster[0] = 
			vol_set.aswD_DacMaster[1] = 
			vol_set.aswD_DacAtt[0] = 
			vol_set.aswD_DacAtt[1] = 
			vol_set.aswA_Sp[0] = 
			vol_set.aswA_Sp[1] = 0 | MCDRV_VOL_UPDATE;		/* 0(dB) * 256 | MCDRV_VOL_UPDATE */
			update_volcache(&vol_set);
			err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
			if (err != MCDRV_SUCCESS)
			{
				DEBUGPRINT1("Volume Change Error", err);
				break;
			}
			break;




		//--------------------------------------------------------------------------------
		case YMU_INTERNAL_CIRCUIT_OFF:
			// cuts to all input and output paths

			ymu_selecter_work_init(&path_set.asHpOut[0]);	// earphone L
			ymu_selecter_work_init(&path_set.asHpOut[1]);	// earphone R
			ymu_selecter_work_init(&path_set.asSpOut[0]);	// speaker
			ymu_selecter_work_init(&path_set.asSpOut[1]);	// no devices
			ymu_selecter_work_init(&path_set.asRcOut[0]);	// reciever
			ymu_selecter_work_init(&path_set.asLout1[0]);	// to WCD9310_MIC IN1 P/N
			ymu_selecter_work_init(&path_set.asLout1[1]);	// to WCD9310_MIC IN1 P/N
			ymu_selecter_work_init(&path_set.asLout2[0]);	// no devices
			ymu_selecter_work_init(&path_set.asLout2[1]);	// no devices
			ymu_selecter_work_init(&path_set.asDit0[0]);	// no devices
			ymu_selecter_work_init(&path_set.asDit1[0]);	// IO#1
			ymu_selecter_work_init(&path_set.asDit2[0]);	// no devices
			ymu_selecter_work_init(&path_set.asDac[0]); 	// MASTER route
			ymu_selecter_work_init(&path_set.asDac[1]);		// VOICE route
			ymu_selecter_work_init(&path_set.asAe[0]);		// digital DSP sourc
			ymu_selecter_work_init(&path_set.asAdc0[0]);	// ADCL(to digital)
			ymu_selecter_work_init(&path_set.asAdc0[1]);	// ADCR(to digital)
			ymu_selecter_work_init(&path_set.asMix[0]);		// mixser input
			ymu_selecter_work_init(&path_set.asBias[0]);	// unknown
/* QNET Audio:2012-03-14  Clock Supply start */
			mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
			err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
			mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply end */
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
			if(disdev_yamaha_flag != 1){
				disdev_yamaha_flag = 0;
				disdev_time_count = 0;
				disdev_timer_regist();
			}
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */
			break;
		//--------------------------------------------------------------------------------

/* QNET Audio:2012-01-23 Earphone bias control start */
		case YMU_MICBIAS_ON:
			DEBUGPRINT0("YMU_MICBIAS_ON");
			if (mc_asoc_hpmic_flag==0 && mc_asoc_hpsw_flag==0)
			{
				memset(&path_set, 0, sizeof(path_set));
				path_set.asBias[0].abSrcOnOff[0]  |= MCDRV_SRC0_MIC3_ON; /* MBS3 on */
/* QNET Audio:2012-03-14  Clock Supply start */
				mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
				err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
				mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply end */
				if (err)
				{
					DEBUGPRINT1("ERROR:Circuit Rooting ", err);
					break;
				}
			}
			mc_asoc_hpsw_flag = 1;
			break;

		case YMU_MICBIAS_OFF:
			DEBUGPRINT0("YMU_MICBIAS_OFF");
			if (mc_asoc_hpmic_flag==0 && mc_asoc_hpsw_flag)
			{
				memset(&path_set, 0, sizeof(path_set));
				path_set.asBias[0].abSrcOnOff[0]  |= MCDRV_SRC0_MIC3_OFF; /* MBS3 off */
/* QNET Audio:2012-03-14  Clock Supply start */
				mc_yamaha_clock_set(MSM_XO_MODE_ON, 1);
				err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&path_set, 0);
				mc_yamaha_clock_set(MSM_XO_MODE_OFF, 1);
/* QNET Audio:2012-03-14  Clock Supply start */
				if (err)
				{
					DEBUGPRINT1("ERROR:Circuit Rooting ", err);
					break;
				}
			}
			mc_asoc_hpsw_flag = 0;
			break;

/* QNET Audio:2012-01-23 Earphone bias control end */

/* FUJITSU nab audio: VOIP ADD-S */
/* Noah for 8k/16k sample rate start */
		case YMU_FS_SWITCH: {
/* FUJITSU:2012-11-29  MOD-S */
			static const struct {
				unsigned short offset;
				unsigned char on_mask;
				unsigned char off_mask;
			} dio1cnn[] = {
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]),MCDRV_SRC3_DIR0_ON,MCDRV_SRC3_DIR0_OFF},
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]),MCDRV_SRC3_DIR2_ON,MCDRV_SRC3_DIR2_OFF},
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK ]),MCDRV_SRC4_PDM_ON ,MCDRV_SRC4_PDM_OFF },
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]),MCDRV_SRC4_ADC0_ON,MCDRV_SRC4_ADC0_OFF},
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK ]),MCDRV_SRC6_MIX_ON ,MCDRV_SRC6_MIX_OFF },
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK  ]),MCDRV_SRC6_AE_ON  ,MCDRV_SRC6_AE_OFF  },
				{offsetof(MCDRV_PATH_INFO,asDit1[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
				{offsetof(MCDRV_PATH_INFO,asDit0[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
				{offsetof(MCDRV_PATH_INFO,asDit2[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
				{offsetof(MCDRV_PATH_INFO,asDac[0].abSrcOnOff [MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
				{offsetof(MCDRV_PATH_INFO,asDac[1].abSrcOnOff [MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
				{offsetof(MCDRV_PATH_INFO,asAe[0].abSrcOnOff  [MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
				{offsetof(MCDRV_PATH_INFO,asMix[0].abSrcOnOff [MCDRV_SRC_DIR1_BLOCK]),MCDRV_SRC3_DIR1_ON,MCDRV_SRC3_DIR1_OFF},
			};
			#define DIO1CNN(struc,index)    (*(UINT8*)((char*)&(struc) + dio1cnn[index].offset))

			MCDRV_DIO_INFO dio;
			UINT8 path_save[ARRAY_SIZE(dio1cnn)];
			int i = param == 0 ? MCDRV_FS_8000 : MCDRV_FS_16000;
			err = _McDrv_Ctrl(MCDRV_GET_DIGITALIO, &dio, 0);
			if (err != MCDRV_SUCCESS) {
				printk("ERROR: %s: YMU_FS_SWITCH: McDrv_Ctrl(MCDRV_GET_DIGITALIO)=%d\n",__func__,err);
				break;
			}

			if (dio.asPortInfo[1].sDioCommon.bFs == i) {
				printk("%s: YMU_FS_SWITCH: bFs=%d not changed\n",__func__,dio.asPortInfo[1].sDioCommon.bFs);
				break;
			}
			dio.asPortInfo[1].sDioCommon.bFs = i;

			err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_set ,0);
			if (err != MCDRV_SUCCESS) {
				printk("ERROR: %s: YMU_FS_SWITCH: McDrv_Ctrl(MCDRV_GET_PATH)=%d\n",__func__,err);
				break;
			}
			for (i = 0;i < ARRAY_SIZE(dio1cnn);i++) {
				path_save[i] = DIO1CNN(path_set,i);
			}

			memset(&path_set,0,sizeof path_set);
			for (i = 0;i < ARRAY_SIZE(dio1cnn);i++) {
				if (path_save[i] & dio1cnn[i].on_mask) {
					DIO1CNN(path_set,i) |= dio1cnn[i].off_mask;
				}
			}
			err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_set ,0);
			if (err != MCDRV_SUCCESS) {
				printk("ERROR: %s: YMU_FS_SWITCH: McDrv_Ctrl(MCDRV_SET_PATH)=%d\n",__func__,err);
				break;
			}

			err = _McDrv_Ctrl(MCDRV_SET_DIGITALIO, &dio, MCDRV_DIO1_COM_UPDATE_FLAG);
			if (err != MCDRV_SUCCESS) {
				printk("ERROR: %s: YMU_FS_SWITCH: McDrv_Ctrl(MCDRV_SET_PATH)=%d\n",__func__,err);
				break;
			}

			memset(&path_set,0,sizeof path_set);
			for (i = 0;i < ARRAY_SIZE(dio1cnn);i++) {
				if (path_save[i] & dio1cnn[i].on_mask) {
					DIO1CNN(path_set,i) |= dio1cnn[i].on_mask;
				}
			}
			err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_set ,0);
			if (err != MCDRV_SUCCESS) {
				printk("ERROR: %s: YMU_FS_SWITCH: McDrv_Ctrl(MCDRV_SET_PATH)=%d\n",__func__,err);
				break;
			}

			printk("%s: YMU_FS_SWITCH: bFs=%d changed\n",__func__,dio.asPortInfo[1].sDioCommon.bFs);
/* FUJITSU:2012-11-29  MOD-E */
			break;
		}
/* Noah for 8k/16k sample rate end */
/* FUJITSU nab audio: VOIP ADD-E */

		default:
			DEBUGPRINT1("default", scene );
			break;
	}
	if (err != MCDRV_SUCCESS) {
		pr_err("Error scene %d = %d\n",scene,err);
	}
	return err;
}

/* FUJITSU: 2013-01-18 ADD-S */
int ymu_control(int scene, int param)
{
	int ret;
	mutex_lock(&interface_mutex);
	ret = ymu_control_l(scene, param);
	mutex_unlock(&interface_mutex);
	return ret;
}
/* FUJITSU: 2013-01-18 ADD-E */
EXPORT_SYMBOL_GPL(ymu_control);

/****************************************************************************************
 FUJITSU:2011/12/21

 FUNCTION : ymu_volume(int amp, int vol)
 EXPLAIN  : YMU829 volume change
 
 INPUT    :	amp		amp select(enum YMU_VOLUME)
 			vol		volume*10(dB)

 OUTPUT   : error status
*****************************************************************************************/
static int ymu_volume_l(int amp, int vol)
{
	int		err = MCDRV_SUCCESS;
	MCDRV_VOL_INFO	vol_set;		// set volume
	SINT16 setvolume = 0;

	memset(&vol_set, 0, sizeof(vol_set));

	setvolume = (vol * 256) / 10;
	setvolume |= MCDRV_VOL_UPDATE;

	DEBUGPRINT1("volume param ", vol);
	DEBUGPRINT1("sign 7-7 ", setvolume);

	// change volume
	switch(amp)
	{
		case LIVOL_L:
			vol_set.aswA_Lin1[0] = setvolume;
			break;
		case LIVOL_R:
			vol_set.aswA_Lin1[1] = setvolume;
			break;
		case MC1_VOL:
			vol_set.aswA_Mic1[0] = setvolume;
			break;
		case MC2_VOL:
			vol_set.aswA_Mic2[0] = setvolume;
			break;
		case MC3_VOL:
			vol_set.aswA_Mic3[0] = setvolume;
			break;
		case ADVOL_L:
			vol_set.aswA_Ad0[0] = setvolume;
			break;
		case ADVOL_R:
			vol_set.aswA_Ad0[1] = setvolume;
			break;
		case HPVOL_L:
			vol_set.aswA_Hp[0] = setvolume;
			break;
		case HPVOL_R:
			vol_set.aswA_Hp[1] = setvolume;
			break;
		case SPVOL_L:
			vol_set.aswA_Sp[0] = setvolume;
			break;
		case SPVOL_R:
			vol_set.aswA_Sp[1] = setvolume;
			break;
		case RCVOL:
			vol_set.aswA_Rc[0] = setvolume;
			break;
		case VO1VOL_L:
			vol_set.aswA_Lout1[0] = setvolume;
			break;
		case VO1VOL_R:
			vol_set.aswA_Lout1[1] = setvolume;
			break;
		case VO2VOL_L:
			vol_set.aswA_Lout2[0] = setvolume;
			break;
		case VO2VOL_R:
			vol_set.aswA_Lout2[1] = setvolume;
			break;
		case MC1GAIN:
			vol_set.aswA_Mic1Gain[0] = setvolume;
			break;
		case MC2GAIN:
			vol_set.aswA_Mic2Gain[0] = setvolume;
			break;
		case MC3GAIN:
			vol_set.aswA_Mic3Gain[0] = setvolume;
			break;
		case HP_GAIN:
			vol_set.aswA_HpGain[0] = setvolume;
	}
	update_volcache(&vol_set);
	err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&vol_set, 0);
	if (err != MCDRV_SUCCESS)
	{
		DEBUGPRINT1("Volume Change Error", err);
	}
	return(0);
}

/* FUJITSU: 2013-01-18 ADD-S */
int ymu_volume(int amp, int vol)
{
	int ret;
	mutex_lock(&interface_mutex);
	ret = ymu_volume_l(amp,vol);
	mutex_unlock(&interface_mutex);
	return ret;
}
/* FUJITSU: 2013-01-18 ADD-E */

EXPORT_SYMBOL_GPL(ymu_volume);

/****************************************************************************************
*****************************************************************************************/
/* QNET Audio:2012-01-17 Basic Feat   end */

#if (defined ALSA_VER_1_0_23) || (defined KERNEL_2_6_35_36_COMMON)
static struct snd_soc_codec* mc_asoc_get_codec_data(void)
{
	return mc_asoc_codec;
}

static void mc_asoc_set_codec_data(struct snd_soc_codec *codec)
{
	mc_asoc_codec	= codec;
}
#endif

static struct mc_asoc_data* mc_asoc_get_mc_asoc(const struct snd_soc_codec *codec)
{
#if (defined ALSA_VER_1_0_23)
	if(codec == NULL) {
		return NULL;
	}
	return codec->private_data;
#elif (defined KERNEL_2_6_35_36_COMMON)
	if(codec == NULL) {
		return NULL;
	}
	return snd_soc_codec_get_drvdata((struct snd_soc_codec *)codec);
#else
	struct mc_asoc_priv	*priv;

	if(codec == NULL) {
		return NULL;
	}
	priv	= snd_soc_codec_get_drvdata((struct snd_soc_codec *)codec);
	if(priv != NULL) {
		return &priv->data;
	}
#endif
	return NULL;
}

/* deliver i2c access to machdep */
struct i2c_client* mc_asoc_get_i2c_client(int slave)
{
	if (slave == 0x11){
		return mc_asoc_i2c_d;
	}
	if (slave == 0x3a){
		return mc_asoc_i2c_a;
	}

#ifdef ALSA_VER_1_0_23
	if(mc_asoc_codec != NULL) {
		return mc_asoc_codec->control_data;
	}
#endif
	return NULL;
}

int mc_asoc_map_drv_error(int err)
{
	switch (err) {
	case MCDRV_SUCCESS:
		return 0;
	case MCDRV_ERROR_ARGUMENT:
		return -EINVAL;
	case MCDRV_ERROR_STATE:
		return -EBUSY;
	case MCDRV_ERROR_TIMEOUT:
		return -EIO;
	default:
		/* internal error */
		return -EIO;
	}
}


static int read_cache(
	struct snd_soc_codec *codec,
	unsigned int reg)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
	if(reg < MC_ASOC_N_REG) {
		return ((u16 *)codec->reg_cache)[reg];
	}
	return -EINVAL;
#else
	int		ret;
	unsigned int	val;

	ret	= snd_soc_cache_read(codec, reg, &val);
	if (ret != 0) {
		dev_err(codec->dev, "Cache read to %x failed: %d\n", reg, ret);
		return -EIO;
	}
	return val;
#endif
}

static int write_cache(
	struct snd_soc_codec *codec, 
	unsigned int reg,
	unsigned int value)
{
	if(reg == MC_ASOC_INCALL_MIC_SP
	|| reg == MC_ASOC_INCALL_MIC_RC
	|| reg == MC_ASOC_INCALL_MIC_HP
	|| reg == MC_ASOC_INCALL_MIC_LO1
	|| reg == MC_ASOC_INCALL_MIC_LO2) {
dbg_info("write cache(%d, %d)\n", reg, value);
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
	if(reg < MC_ASOC_N_REG) {
		((u16 *)codec->reg_cache)[reg]	= value;
	} else {
		return -EINVAL;
	}
	return 0;
#else
	return (snd_soc_cache_write(codec, reg, value));
#endif
}

static int get_port_block(UINT8 port)
{
	switch(port) {
	case	DIO_0:
		return MCDRV_SRC_DIR0_BLOCK;
	case	DIO_1:
		return MCDRV_SRC_DIR1_BLOCK;
	case	DIO_2:
		return MCDRV_SRC_DIR2_BLOCK;
	default:
		return -1;
	}
}
static int get_port_block_on(UINT8 port)
{
	switch(port) {
	case	DIO_0:
		return MCDRV_SRC3_DIR0_ON;
	case	DIO_1:
		return MCDRV_SRC3_DIR1_ON;
	case	DIO_2:
		return MCDRV_SRC3_DIR2_ON;
	default:
		return -1;
	}
}
static size_t get_port_srconoff_offset(UINT8 port)
{
	switch(port) {
	case	DIO_0:
		return offsetof(MCDRV_PATH_INFO, asDit0);
	case	DIO_1:
		return offsetof(MCDRV_PATH_INFO, asDit1);
	case	DIO_2:
		return offsetof(MCDRV_PATH_INFO, asDit2);
	default:
		return -1;
	}
}
static int get_ap_play_port(void)
{
	switch(mc_asoc_ap_play_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return mc_asoc_ap_play_port;
	default:
		return -1;
	}
}
static int get_ap_cap_port(void)
{
	switch(mc_asoc_ap_cap_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return mc_asoc_ap_cap_port;
	default:
		return -1;
	}
}
static int get_ap_port_block(void)
{
	switch(mc_asoc_ap_play_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return get_port_block(mc_asoc_ap_play_port);
	default:
		return MCDRV_SRC_ADC0_BLOCK;
	}
}
static int get_ap_port_block_on(void)
{
	switch(mc_asoc_ap_play_port) {
	case	DIO_0:
		return MCDRV_SRC3_DIR0_ON;
	case	DIO_1:
		return MCDRV_SRC3_DIR1_ON;
	case	DIO_2:
		return MCDRV_SRC3_DIR2_ON;
	default:
		return MCDRV_SRC4_ADC0_ON;
	}
}
static size_t get_ap_port_srconoff_offset(void)
{
	return get_port_srconoff_offset(mc_asoc_ap_cap_port);
}

static int get_cp_port(void)
{
	switch(mc_asoc_cp_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return mc_asoc_cp_port;
	default:
		return -1;
	}
}

static int get_cp_port_block(void)
{
	switch(mc_asoc_cp_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return get_port_block(mc_asoc_cp_port);
	default:
		return MCDRV_SRC_ADC1_BLOCK;
	}
}
static int get_cp_port_block_on(void)
{
	switch(mc_asoc_cp_port) {
	case	DIO_0:
		return MCDRV_SRC3_DIR0_ON;
	case	DIO_1:
		return MCDRV_SRC3_DIR1_ON;
	case	DIO_2:
		return MCDRV_SRC3_DIR2_ON;
	default:
		return MCDRV_SRC4_ADC1_ON;
	}
}
static size_t get_cp_port_srconoff_offset(void)
{
	return get_port_srconoff_offset(mc_asoc_cp_port);
}

static int get_bt_port(void)
{
	return mc_asoc_bt_port;
}
static int get_bt_port_block(void)
{
	return get_port_block(mc_asoc_bt_port);
}
static int get_bt_port_block_on(void)
{
	return get_port_block_on(mc_asoc_bt_port);
}
static size_t get_bt_port_srconoff_offset(void)
{
	return get_port_srconoff_offset(mc_asoc_bt_port);
}

static size_t get_bt_DIR_ATT_offset(void)
{
	switch(mc_asoc_bt_port) {
	case	DIO_0:
		return mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset;
	case	DIO_1:
		return mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset;
	case	DIO_2:
		return mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset;
	default:
		return -1;
	}
}

static int get_mic_block_on(UINT8 mic)
{
	switch(mic) {
	case	MIC_1:
		return MCDRV_SRC0_MIC1_ON;
	case	MIC_2:
		return MCDRV_SRC0_MIC2_ON;
	case	MIC_3:
		return MCDRV_SRC0_MIC3_ON;
	default:
		return -1;
	}
}

static int get_main_mic_block_on(void)
{
	return get_mic_block_on(mc_asoc_main_mic);
}
static int get_sub_mic_block_on(void)
{
	return get_mic_block_on(mc_asoc_sub_mic);
}
static int get_hs_mic_block_on(void)
{
	return get_mic_block_on(mc_asoc_hs_mic);
}


static UINT8 get_incall_mic(
	struct snd_soc_codec *codec,
	int	output_path
)
{
	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		return read_cache(codec, MC_ASOC_INCALL_MIC_SP);
		break;
	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		return read_cache(codec, MC_ASOC_INCALL_MIC_RC);
		break;
	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		return read_cache(codec, MC_ASOC_INCALL_MIC_HP);
		break;
	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		return read_cache(codec, MC_ASOC_INCALL_MIC_LO1);
		break;
	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		return read_cache(codec, MC_ASOC_INCALL_MIC_LO2);
		break;
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		return MC_ASOC_INCALL_MIC_MAINMIC;
	default:
		break;
	}
	return -EIO;
}

static void get_voicecall_index(
	int	output_path,
	int	incall_mic,
	int	*base,
	int	*user
)
{
	*base	= 0;
	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		if((incall_mic == 0) || (incall_mic == 1)) {
			/*	MainMIC or SubMIC	*/
			*user	= 2;
		}
		else {
			/*	2MIC	*/
			*user	= 3;
			*base	= 1;
		}
		break;
	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		if((incall_mic == 0) || (incall_mic == 1)) {
			/*	MainMIC or SubMIC	*/
			*user	= 4;
		}
		else {
			/*	2MIC	*/
			*user	= 5;
			*base	= 1;
		}
		break;
	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		if((incall_mic == 0) || (incall_mic == 1)) {
			/*	MainMIC or SubMIC	*/
			*user	= 6;
		}
		else {
			/*	2MIC	*/
			*user	= 7;
			*base	= 1;
		}
		break;
	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		if((incall_mic == 0) || (incall_mic == 1)) {
			/*	MainMIC or SubMIC	*/
			*user	= 8;
		}
		else {
			/*	2MIC	*/
			*user	= 9;
			*base	= 1;
		}
		break;
	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		if((incall_mic == 0) || (incall_mic == 1)) {
			/*	MainMIC or SubMIC	*/
			*user	= 10;
		}
		else {
			/*	2MIC	*/
			*user	= 11;
			*base	= 1;
		}
		break;
	case	MC_ASOC_OUTPUT_PATH_HS:
		*user	= 12;
		break;
	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		*user	= 13;
		break;

	default:
		break;
	}
}

static void set_vol_mute_flg(size_t offset, UINT8 lr, UINT8 mute)
{
	SINT16	*vp	= (SINT16*)((void*)&mc_asoc_vol_info_mute+offset);

	if(offset == (size_t)-1) {
		return;
	}
	if(mute == 1) {
		*(vp+lr)	= MCDRV_LOGICAL_VOL_MUTE;
	}
	else {
		*(vp+lr)	= 0;
	}
}

static UINT8 get_vol_mute_flg(size_t offset, UINT8 lr)
{
	SINT16	*vp;

	if(offset == (size_t)-1) {
		return 1;	/*	mute	*/
	}

	vp	= (SINT16*)((void*)&mc_asoc_vol_info_mute+offset);
	return (*(vp+lr) != 0);
}

static int set_volume(struct snd_soc_codec *codec)
{
	MCDRV_VOL_INFO	vol_info;
	SINT16	*vp;
	int		err, i;
	int		reg;
	int		cache;

	TRACE_FUNC();

	memset(&vol_info, 0, sizeof(MCDRV_VOL_INFO));

	for(reg = MC_ASOC_DVOL_AD0; reg < MC_ASOC_N_VOL_REG; reg++) {
		if(mc_asoc_vreg_map[reg].offset != (size_t)-1) {
			vp		= (SINT16 *)((void *)&vol_info + mc_asoc_vreg_map[reg].offset);
			cache	= read_cache(codec, reg);
			if(cache < 0) {
				return -EIO;
			}

			for (i = 0; i < mc_asoc_vreg_map[reg].channels; i++, vp++) {
				unsigned int	v	= (cache >> (i*8)) & 0xff;
				int		sw, vol;
				SINT16	db;
				sw	= (reg < MC_ASOC_AVOL_MIC1_GAIN) ? (v & 0x80) : 1;
				vol	= sw ? (v & 0x7f) : 0;
				if(get_vol_mute_flg(mc_asoc_vreg_map[reg].offset, i) != 0) {
					db	= mc_asoc_vreg_map[reg].volmap[0];
				}
				else {
					db	= mc_asoc_vreg_map[reg].volmap[vol];
				}
				*vp	= db | MCDRV_VOL_UPDATE;
			}
		}
	}

	err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, &vol_info, 0);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in MCDRV_SET_VOLUME\n", err);
		return -EIO;
	}

	return 0;
}

static int set_vreg_map(struct snd_soc_codec *codec)
{
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		ain_play	= 0;
	int		base	= 0;
	int		user	= 0;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}
	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}

	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	dbg_info("ain_play=%d\n", ain_play);

	mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset			= (size_t)-1;
	mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= (size_t)-1;
	mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= (size_t)-1;
	mc_asoc_vreg_map[MC_ASOC_AVOL_AD0].offset			= offsetof(MCDRV_VOL_INFO, aswA_Ad0);
	mc_asoc_vreg_map[MC_ASOC_DVOL_AD0].offset			= offsetof(MCDRV_VOL_INFO, aswD_Ad0);
	mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir0);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir1);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir2);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);

	switch(get_ap_play_port()) {
	case	DIO_0:
		mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset	= (size_t)-1;
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
		break;
	case	DIO_1:
		mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset	= (size_t)-1;
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
		break;
	case	DIO_2:
		mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset	= (size_t)-1;
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
		break;
	case	-1:
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= (size_t)-1;
		break;
	default:
		break;
	}

	if(get_cp_port() == -1) {
		mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset	= (size_t)-1;
		if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
				if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
					if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
					|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
					|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
						return 0;
					}
				}
				else {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
					return 0;
				}
			}
			mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
		}
	}
	else if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
		 || (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
			get_voicecall_index(output_path, incall_mic, &base, &user);
			if((mc_asoc_hf == HF_ON) && (mc_asoc->dsp_voicecall.onoff[base] == 1)) {
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2_1].offset	= (size_t)-1;
				mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dit21);
				return 0;
			}
		}
		switch(get_cp_port()) {
		case	-1:
			break;
		case	DIO_0:
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
			break;
		case	DIO_1:
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
			break;
		case	DIO_2:
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
			break;
		default:
			break;
		}
	}

	if((audio_mode != MC_ASOC_AUDIO_MODE_INCALL)
	&& (audio_mode != MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	&& (audio_mode != MC_ASOC_AUDIO_MODE_INCOMM)) {
		if(ain_play != 0) {
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF)
			|| (input_path == MC_ASOC_INPUT_PATH_BT)
			|| ((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC))
			|| ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC))
			|| ((msmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_2MIC))
			|| ((hsmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_HS))
			|| ((lin1_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN1))
			|| ((lin2_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN2))) {
				mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset	= offsetof(MCDRV_VOL_INFO, aswA_Ad0);
				mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0);
				mc_asoc_vreg_map[MC_ASOC_AVOL_AD0].offset	= (size_t)-1;
				mc_asoc_vreg_map[MC_ASOC_DVOL_AD0].offset	= (size_t)-1;
			}
			mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
			mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset		= (size_t)-1;
		}
		if(btmic_play != 0) {
			switch(get_bt_port()) {
			case	DIO_0:
				mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir0);
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0].offset			= (size_t)-1;
				mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset		= (size_t)-1;
				break;
			case	DIO_1:
				mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir1);
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1].offset			= (size_t)-1;
				mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset		= (size_t)-1;
				break;
			case	DIO_2:
				mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir2);
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2].offset			= (size_t)-1;
				mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
				mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset		= (size_t)-1;
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

static UINT8 use_cdsp4cap(
	UINT8*	onoff,
	int	audio_mode,
	int	audio_mode_cap,
	int	input_path,
	int	output_path,
	int	ain_play_cap
)
{
	if((mc_asoc_hwid != MC_ASOC_MC3_HW_ID) && (mc_asoc_hwid != MC_ASOC_MA2_HW_ID)) {
		return 0;
	}
	if(mc_asoc_hf == HF_OFF) {
		return 0;
	}
	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL)) {
		return 0;
	}
	if(onoff[input_path==MC_ASOC_INPUT_PATH_2MIC?1:0] == 0) {
		return 0;
	}

	if(get_ap_cap_port() == -1) {
		if((audio_mode == MC_ASOC_AUDIO_MODE_AUDIO)
		|| (ain_play_cap == 1)) {
			if(output_path != MC_ASOC_OUTPUT_PATH_BT) {
				return 0;
			}
		}
	}

	if(((input_path == MC_ASOC_INPUT_PATH_MAINMIC)
	  ||(input_path == MC_ASOC_INPUT_PATH_SUBMIC)
	  ||(input_path == MC_ASOC_INPUT_PATH_2MIC)
	  ||(input_path == MC_ASOC_INPUT_PATH_HS))
	&& (mc_asoc_cdsp_dir == MC_ASOC_CDSP_CAP)) {
		return 1;
	}
	return 0;
}


static UINT16 dsp_onoff(
	UINT16	*dsp_onoff,
	int		output_path
)
{
	UINT16	onoff	= dsp_onoff[0];

	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		if(dsp_onoff[YMC_DSP_OUTPUT_SP] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_SP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		if(dsp_onoff[YMC_DSP_OUTPUT_RC] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_RC];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		if(dsp_onoff[YMC_DSP_OUTPUT_HP] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_HP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		if(dsp_onoff[YMC_DSP_OUTPUT_LO1] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_LO1];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		if(dsp_onoff[YMC_DSP_OUTPUT_LO2] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_LO2];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		if(dsp_onoff[YMC_DSP_OUTPUT_BT] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_BT];
		}
		break;

	default:
		break;
	}
	return onoff;
}

static UINT8 ae_onoff(
	UINT8	*ae_onoff,
	int		output_path
)
{
	UINT8	onoff	= ae_onoff[0];

	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		if(ae_onoff[YMC_DSP_OUTPUT_SP] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_SP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		if(ae_onoff[YMC_DSP_OUTPUT_RC] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_RC];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		if(ae_onoff[YMC_DSP_OUTPUT_HP] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_HP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		if(ae_onoff[YMC_DSP_OUTPUT_LO1] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_LO1];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		if(ae_onoff[YMC_DSP_OUTPUT_LO2] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_LO2];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		if(ae_onoff[YMC_DSP_OUTPUT_BT] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_BT];
		}
		break;

	default:
		break;
	}
	return onoff;
}

static void set_audioengine_param(
	struct mc_asoc_data	*mc_asoc,
	MCDRV_AE_INFO	*ae_info,
	int	inout
)
{
	TRACE_FUNC();

	if(inout < MC_ASOC_DSP_N_OUTPUT) {
		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_BEX] != MC_ASOC_DSP_BLOCK_ON))
		|| (mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_WIDE] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_BEXWIDE_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_BEXWIDE_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abBex, mc_asoc->ae_info[inout].abBex, BEX_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abBex, mc_asoc->ae_info[0].abBex, BEX_PARAM_SIZE);
			}
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abWide, mc_asoc->ae_info[inout].abWide, WIDE_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abWide, mc_asoc->ae_info[0].abWide, WIDE_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_DRC] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_DRC_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_DRC_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[inout].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[inout].abDrc2, DRC2_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[0].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[0].abDrc2, DRC2_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EQ5] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ5_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ5_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[inout].abEq5, EQ5_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[0].abEq5, EQ5_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EQ3] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ3_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ3_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[inout].abEq3, EQ3_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[0].abEq3, EQ3_PARAM_SIZE);
			}
		}
	}
	else {
		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_BEX] != MC_ASOC_DSP_BLOCK_ON))
		|| (mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_WIDE] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_BEXWIDE_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_BEXWIDE_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abBex, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abBex, BEX_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abBex, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abBex, BEX_PARAM_SIZE);
			}
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abWide, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abWide, WIDE_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abWide, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abWide, WIDE_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_DRC] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_DRC_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_DRC_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abDrc2, DRC2_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abDrc2, DRC2_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_EQ5] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ5_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ5_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abEq5, EQ5_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abEq5, EQ5_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_EQ3] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ3_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ3_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abEq3, EQ3_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abEq3, EQ3_PARAM_SIZE);
			}
		}
	}
}

static int set_audioengine(struct snd_soc_codec *codec)
{
	MCDRV_AE_INFO	ae_info;
	int		ret;
	int		err;
	int		audio_mode;
	int		output_path;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if((codec == NULL) || (mc_asoc == NULL)) {
		return -EINVAL;
	}

	memcpy(&ae_info, &mc_asoc->ae_store, sizeof(MCDRV_AE_INFO));

	if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
		if((ret = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
			return ret;
		}
	}
	else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
		if((ret = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
			return ret;
		}
	}
	else {
		return 0;
	}
	switch(ret) {
	case	MC_ASOC_AUDIO_MODE_AUDIO:
	case	MC_ASOC_AUDIO_MODE_OFF:
		audio_mode	= 0;	/*	Normal	*/
		break;
	case	MC_ASOC_AUDIO_MODE_INCALL:
	case	MC_ASOC_AUDIO_MODE_AUDIO_INCALL:
		audio_mode	= 2;	/*	Incall	*/
		break;
	case	MC_ASOC_AUDIO_MODE_INCOMM:
		audio_mode	= 3;	/*	Incommunication	*/
		break;
	default:
		return -EIO;
	}

	if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
		if((ret = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
			return ret;
		}
		output_path	= ret;

		switch(output_path) {
		case	MC_ASOC_OUTPUT_PATH_SP:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_SP);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_RC:
		case	MC_ASOC_OUTPUT_PATH_SP_RC:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_RC);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_HP:
		case	MC_ASOC_OUTPUT_PATH_HS:
		case	MC_ASOC_OUTPUT_PATH_SP_HP:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_HP);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_LO1:
		case	MC_ASOC_OUTPUT_PATH_SP_LO1:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_LO1);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_LO2:
		case	MC_ASOC_OUTPUT_PATH_SP_LO2:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_LO2);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_BT:
		case	MC_ASOC_OUTPUT_PATH_SP_BT:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_BT);
			}
			break;

		default:
			return -EIO;
		}
	}
	else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
		if(((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] != 0x8000)
			&& ((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] & (1<<audio_mode)) == 0))
		|| ((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] == 0x8000)
			&& ((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE] & (1<<audio_mode)) == 0))) {
			ae_info.bOnOff	= 0;
		}
		else {
			set_audioengine_param(mc_asoc, &ae_info, MC_ASOC_DSP_INPUT_BASE);
		}
	}

	if(memcmp(&ae_info, &mc_asoc->ae_store, sizeof(MCDRV_AE_INFO)) == 0) {
dbg_info("memcmp\n");
		return 0;
	}
	err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE, &ae_info, 0x1ff);
	if(err == MCDRV_SUCCESS) {
		memcpy(&mc_asoc->ae_store, &ae_info, sizeof(MCDRV_AE_INFO));
	}
	else {
		err	= mc_asoc_map_drv_error(err);
	}
	return err;
}

static	MC_AE_EX_STORE	ae_ex; /* FUJITSU:2011-11-07 Compile error correspondence  */

static int set_audioengine_ex(struct snd_soc_codec *codec)
{
	/*MC_AE_EX_STORE	ae_ex; FUJITSU:2011-11-07 Compile error correspondence */
	int		i;
	int		ret;
	int		err	= 0;
	int		audio_mode;
	int		output_path;
	int		input_path;
	int		lin1_play, lin2_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	MCDRV_PATH_INFO	path_info, path_info_cur;

	TRACE_FUNC();

	if((codec == NULL) || (mc_asoc == NULL)) {
		return -EINVAL;
	}

	if((mc_asoc_hwid != MC_ASOC_MC3_HW_ID) && (mc_asoc_hwid != MC_ASOC_MA2_HW_ID)) {
		return -EIO;
	}

	memset(&ae_ex, 0, sizeof(MC_AE_EX_STORE));

	if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
		if((ret = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
			return ret;
		}
	}
	else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
		if((ret = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
			return ret;
		}
	}
	else {
		return 0;
	}
	switch(ret) {
	case	MC_ASOC_AUDIO_MODE_AUDIO:
	case	MC_ASOC_AUDIO_MODE_OFF:
		audio_mode	= 0;	/*	Normal	*/
		break;
	case	MC_ASOC_AUDIO_MODE_INCALL:
	case	MC_ASOC_AUDIO_MODE_AUDIO_INCALL:
		audio_mode	= 2;	/*	Incall	*/
		break;
	case	MC_ASOC_AUDIO_MODE_INCOMM:
		audio_mode	= 3;	/*	Incommunication	*/
		break;
	default:
		return -EIO;
	}

	if((ret = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return ret;
	}
	input_path	= ret;
	if((ret = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return ret;
	}
	lin1_play	= ret;
	if((ret = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return ret;
	}
	lin2_play	= ret;
	if((ret = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return ret;
	}
	output_path	= ret;

	if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
		dbg_info("mc_asoc->dsp_onoff=0x%x, mc_asoc_ae_dir=%d\n",
			dsp_onoff(mc_asoc->dsp_onoff, output_path), mc_asoc_ae_dir);

		if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) != 0) {
			switch(output_path) {
			case	MC_ASOC_OUTPUT_PATH_SP:
				if((mc_asoc->block_onoff[1][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[1][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[1][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[1], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_RC:
			case	MC_ASOC_OUTPUT_PATH_SP_RC:
				if((mc_asoc->block_onoff[2][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[2][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[2][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[2], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_HP:
			case	MC_ASOC_OUTPUT_PATH_HS:
			case	MC_ASOC_OUTPUT_PATH_SP_HP:
				if((mc_asoc->block_onoff[3][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[3][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[3][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[3], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_LO1:
			case	MC_ASOC_OUTPUT_PATH_SP_LO1:
				if((mc_asoc->block_onoff[4][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[4][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[4][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[4], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_LO2:
			case	MC_ASOC_OUTPUT_PATH_SP_LO2:
				if((mc_asoc->block_onoff[5][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[5][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[5][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[5], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_BT:
			case	MC_ASOC_OUTPUT_PATH_SP_BT:
				if((mc_asoc->block_onoff[6][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[6][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[6][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[6], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;

			default:
				return -EIO;
				break;
			}
		}
	}
	else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
		dbg_info("mc_asoc->dsp_onoff[BASE]=0x%x, mc_asoc->dsp_onoff[EXT]=0x%x, mc_asoc_ae_dir=%d\n",
			mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE],
			mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT],
			mc_asoc_ae_dir);

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
		}
		else {
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
				if((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] & (1<<audio_mode)) != 0) {
					memcpy(&ae_ex, &mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT], sizeof(MC_AE_EX_STORE));
				}
			}
			else {
				if((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE] & (1<<audio_mode)) != 0) {
					memcpy(&ae_ex, &mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE], sizeof(MC_AE_EX_STORE));
				}
			}
		}
	}

	if(memcmp(&ae_ex, &mc_asoc->ae_ex_store, sizeof(MC_AE_EX_STORE)) == 0) {
dbg_info("memcmp\n");
		return 0;
	}

	if((ae_ex.size != mc_asoc->ae_ex_store.size)
	|| (memcmp(&ae_ex.prog, &mc_asoc->ae_ex_store.prog, ae_ex.size))) {
		memset(&mc_asoc->ae_ex_store, 0, sizeof(MC_AE_EX_STORE));
		if(ae_ex.size == 0) {
			err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0);
		}
		else {
			if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
			for(i = 0; i < AE_PATH_CHANNELS; i++) {
				memset(path_info.asAe[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
			}
			if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, ae_ex.prog, ae_ex.size);
		}
		if(err == MCDRV_SUCCESS) {
			memcpy(mc_asoc->ae_ex_store.prog, ae_ex.prog, ae_ex.size);
			mc_asoc->ae_ex_store.size	= ae_ex.size;
		}
		else {
			return mc_asoc_map_drv_error(err);
		}
		if(ae_ex.size > 0) {
			for(i = 0; i < ae_ex.param_count && err == 0; i++) {
				err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, &ae_ex.exparam[i], 0);
				if(err == MCDRV_SUCCESS) {
					memcpy(&mc_asoc->ae_ex_store.exparam[i], &ae_ex.exparam[i], sizeof(MCDRV_EXPARAM));
					mc_asoc->ae_ex_store.param_count	= i+1;
				}
				else {
					return mc_asoc_map_drv_error(err);
				}
			}
			if(err == MCDRV_SUCCESS) {
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
			}
		}
	}
	else {
		if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
			if(ae_ex.param_count > mc_asoc->aeex_prm[0].param_count) {
				if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
				for(i = 0; i < AE_PATH_CHANNELS; i++) {
					memset(path_info.asAe[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				for(i = mc_asoc->aeex_prm[0].param_count; i < ae_ex.param_count && err == 0; i++) {
					err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, &ae_ex.exparam[i], 0);
					if(err == MCDRV_SUCCESS) {
						memcpy(&mc_asoc->ae_ex_store.exparam[i], &ae_ex.exparam[i], sizeof(MCDRV_EXPARAM));
						mc_asoc->ae_ex_store.param_count	= i+1;
					}
					else {
						return mc_asoc_map_drv_error(err);
					}
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
			}
		}
		else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
			if(ae_ex.param_count > mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].param_count) {
				if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
				for(i = 0; i < AE_PATH_CHANNELS; i++) {
					memset(path_info.asAe[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				for(i = mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].param_count; i < ae_ex.param_count && err == 0; i++) {
					err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, &ae_ex.exparam[i], 0);
					if(err == MCDRV_SUCCESS) {
						memcpy(&mc_asoc->ae_ex_store.exparam[i], &ae_ex.exparam[i], sizeof(MCDRV_EXPARAM));
						mc_asoc->ae_ex_store.param_count	= i+1;
					}
					else {
						return mc_asoc_map_drv_error(err);
					}
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
			}
		}
	}
	if(err == 0) {
		if(mc_asoc->ae_ex_store.size > 0) {
			mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_AE_EX_ON;
		}
		else {
			mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_AE_EX_OFF;
		}
	}
	return err;
}

static int set_cdsp_off(
	struct mc_asoc_data	*mc_asoc
)
{
	int		i, j;
	int		err;
	MCDRV_PATH_INFO	path_info;

	TRACE_FUNC();

	if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
		return 0;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
		for(j = 0; j < SOURCE_BLOCK_NUM; j++) {
			if((path_info.asCdsp[i].abSrcOnOff[j] & 0x55) != 0) {
				memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
				for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
					path_info.asPeak[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
					path_info.asDit0[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
					path_info.asDit1[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
					path_info.asDit2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DAC_PATH_CHANNELS; i++) {
					path_info.asDac[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]		|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < AE_PATH_CHANNELS; i++) {
					path_info.asAe[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]		|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < MIX_PATH_CHANNELS; i++) {
					path_info.asMix[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]		|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
					path_info.asMix2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
					memset(path_info.asCdsp[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
					path_info.asCdsp[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				break;
			}
		}
	}
	if((err = _McDrv_Ctrl(MCDRV_SET_CDSP, NULL, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	mc_asoc->cdsp_store.size	= 0;
	mc_asoc->cdsp_store.prog	= NULL;
	mc_asoc_cdsp_ae_ex_flag		= MC_ASOC_CDSP_AE_EX_OFF;

	return 0;
}

static int is_cdsp_used(
	void
)
{
	int		i, j;
	int		err;
	MCDRV_PATH_INFO	path_info;

	TRACE_FUNC();

	if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
		return 0;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
		for(j = 0; j < SOURCE_BLOCK_NUM; j++) {
			if((path_info.asCdsp[i].abSrcOnOff[j] & 0x55) != 0) {
				return 1;
			}
		}
	}
	for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
		if((path_info.asPeak[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
		if((path_info.asDit0[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
		if((path_info.asDit1[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
		if((path_info.asDit2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DAC_PATH_CHANNELS; i++) {
		if((path_info.asDac[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < AE_PATH_CHANNELS; i++) {
		if((path_info.asAe[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < MIX_PATH_CHANNELS; i++) {
		if((path_info.asMix[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
		if((path_info.asMix2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}

	return 0;
}

static int set_cdsp_param(struct snd_soc_codec *codec)
{
	int		i;
	int		ret;
	int		err	= 0;
	int		output_path;
	int		incall_mic;
	int		base	= 0;
	int		user	= 0;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc_hf == HF_OFF) {
		return 0;
	}

	if((codec == NULL) || (mc_asoc == NULL)) {
		return -EINVAL;
	}

	if((ret = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return ret;
	}
	output_path	= ret;
	if((ret = get_incall_mic(codec, output_path)) < 0) {
		return ret;
	}
	incall_mic	= ret;

	if((ret = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return ret;
	}
	if((ret == MC_ASOC_AUDIO_MODE_INCALL)
	|| (ret == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (ret == MC_ASOC_AUDIO_MODE_INCOMM)) {
		get_voicecall_index(output_path, incall_mic, &base, &user);
	}
	else {
		if((ret = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
			return ret;
		}
		if(ret != MC_ASOC_AUDIO_MODE_OFF) {
			if((ret = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
				return ret;
			}
			if(ret == MC_ASOC_INPUT_PATH_2MIC) {
				base	= 1;
			}
			else {
				base	= 0;
			}
			user	= -1;
		}
		else {
			return 0;
		}
	}

	if(mc_asoc->dsp_voicecall.onoff[base] == 0) {
		/*	off	*/
		return set_cdsp_off(mc_asoc);
	}

	if((mc_asoc->cdsp_store.size == 0)
	|| (mc_asoc->cdsp_store.prog == NULL)) {
		err	= _McDrv_Ctrl(MCDRV_SET_CDSP, (UINT8*)gabLadonHf30, sizeof(gabLadonHf30));
		if(err != MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(err);
		}
		mc_asoc->cdsp_store.prog	= (UINT8*)gabLadonHf30;
		mc_asoc->cdsp_store.size	= sizeof(gabLadonHf30);

		mc_asoc->cdsp_store.param_count	= 0;
		for(i = 0; i < mc_asoc->dsp_voicecall.param_count[base] && err == 0; i++) {
			err	= _McDrv_Ctrl(MCDRV_SET_CDSP_PARAM, &mc_asoc->dsp_voicecall.cdspparam[base][i], 0);
			if(err == MCDRV_SUCCESS) {
				memcpy(&mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count++],
						&mc_asoc->dsp_voicecall.cdspparam[base][i],
						sizeof(MCDRV_CDSPPARAM));
			}
			else {
				_McDrv_Ctrl(MCDRV_SET_CDSP, NULL, 0);
				return mc_asoc_map_drv_error(err);
			}
		}
	}

	if((user > 0) && (mc_asoc->dsp_voicecall.onoff[user] == 1) && (mc_asoc->cdsp_store.size > 0)) {
		dbg_info("user=%d\n", user);
		for(i = 0; i < mc_asoc->dsp_voicecall.param_count[user] && err == 0; i++) {
			err	= _McDrv_Ctrl(MCDRV_SET_CDSP_PARAM, &mc_asoc->dsp_voicecall.cdspparam[user][i], 0);
			if(err == MCDRV_SUCCESS) {
				memcpy(&mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count++],
						&mc_asoc->dsp_voicecall.cdspparam[user][i],
						sizeof(MCDRV_CDSPPARAM));
			}
			else {
				err	= mc_asoc_map_drv_error(err);
			}
		}
	}
	if(err == 0) {
		mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_ON;
	}
	return err;
}

static void set_DIR(
	UINT8	*AeSrcOnOff,
	UINT8	*SrcOnOff,
	int		port,
	int		port_block,
	int		port_block_on
)
{
	if(port == DIO_0) {
		if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_DIR0)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_DIR0)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else {
			SrcOnOff[port_block]			|= port_block_on;
		}
	}
	else if(port == DIO_1) {
		if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_DIR1)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_DIR1)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else {
			SrcOnOff[port_block]			|= port_block_on;
		}
	}
	else if(port == DIO_2) {
		if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_DIR2)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_DIR2)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else {
			SrcOnOff[port_block]			|= port_block_on;
		}
	}
	else {
		if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC1)) {
			AeSrcOnOff[port_block]			|= port_block_on;
			SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else {
			SrcOnOff[port_block]			|= port_block_on;
		}
	}
}

static void set_ADC0(
	UINT8	*AeSrcOnOff,
	UINT8	*SrcOnOff
)
{
	if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_cap == AE_SRC_PLAY_ADC0)) {
		AeSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
		SrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
	}
	else if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
		AeSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
		SrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
	}
	else {
		SrcOnOff[MCDRV_SRC_ADC0_BLOCK]		|= MCDRV_SRC4_ADC0_ON;
	}
}

static void set_ADC1(
	UINT8	*AeSrcOnOff,
	UINT8	*SrcOnOff
)
{
	if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC1)) {
		AeSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_ON;
		SrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
	}
	else {
		SrcOnOff[MCDRV_SRC_ADC1_BLOCK]		|= MCDRV_SRC4_ADC1_ON;
	}
}

static void set_PDM(
	UINT8	*AeSrcOnOff,
	UINT8	*SrcOnOff
)
{
	if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_PDM)) {
		AeSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
	}
	else if((AeSrcOnOff != NULL) && (mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_PDM)) {
		AeSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		SrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
	}
	else {
		SrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
	}
}

static void set_BIAS_src(
	MCDRV_PATH_INFO	*path_info,
	UINT8	mic
)
{
	switch(mic) {
	case	MIC_1:
		if((mc_asoc_mic1_bias == BIAS_ON_ALWAYS)
		|| (mc_asoc_mic1_bias == BIAS_SYNC_MIC)) {
			path_info->asBias[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK]	|= MCDRV_SRC0_MIC1_ON;
		}
		break;
	case	MIC_2:
		if((mc_asoc_mic2_bias == BIAS_ON_ALWAYS)
		|| (mc_asoc_mic2_bias == BIAS_SYNC_MIC)) {
			path_info->asBias[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK]	|= MCDRV_SRC0_MIC2_ON;
		}
		break;
	case	MIC_3:
		if((mc_asoc_mic3_bias == BIAS_ON_ALWAYS)
		|| (mc_asoc_mic3_bias == BIAS_SYNC_MIC)) {
			path_info->asBias[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK]	|= MCDRV_SRC0_MIC3_ON;
		}
		break;
	default:
		break;
	}
}

static void set_CDSP_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic
)
{
	int		base	= 0;
	int		user	= 0;

	if((mc_asoc_hwid == MC_ASOC_MC1_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA1_HW_ID)) {
		return;
	}

	if(mc_asoc_hf == HF_OFF) {
		return;
	}

	get_voicecall_index(output_path, incall_mic, &base, &user);
	if(mc_asoc->dsp_voicecall.onoff[base] != 1) {
		return;
	}

	if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
		return;
	}

	if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
		if(get_bt_port() == DIO_0) {
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff,
					get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff,
					get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
		}
		else if(get_bt_port() == DIO_1) {
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff,
					get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff,
					get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
		}
		else if(get_bt_port() == DIO_2) {
			path_info->asCdsp[0].abSrcOnOff[get_bt_port_block()]	|= MCDRV_SRC3_DIR2_DIRECT_ON;
			path_info->asCdsp[1].abSrcOnOff[get_bt_port_block()]	|= MCDRV_SRC3_DIR2_DIRECT_ON;
		}
		if(audio_mode == MC_ASOC_AUDIO_MODE_INCOMM) {
			if(get_ap_play_port() == -1) {
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
			}
			else {
				set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff,
						get_ap_play_port(), get_ap_port_block(), get_ap_port_block_on());
			}
		}
		else {
			if(get_cp_port() == -1) {
				if(audio_mode_cap != MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				}
				else {
					if(input_path == MC_ASOC_INPUT_PATH_MAINMIC) {
						if((mc_asoc_main_mic == MIC_1)
						|| (mc_asoc_main_mic == MIC_2)
						|| (mc_asoc_main_mic == MIC_3)) {
							set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
						}
						else {
							set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
						}
					}
					else if(input_path == MC_ASOC_INPUT_PATH_SUBMIC) {
						if((mc_asoc_sub_mic == MIC_1)
						|| (mc_asoc_sub_mic == MIC_2)
						|| (mc_asoc_sub_mic == MIC_3)) {
							set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
						}
						else {
							set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
						}
					}
					else if(input_path == MC_ASOC_INPUT_PATH_2MIC) {
						set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					}
					else {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					}
				}
			}
			else {
				set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff,
						get_cp_port(), get_cp_port_block(), get_cp_port_block_on());
			}
		}
		return;
	}
	else if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
		if((mc_asoc_hs_mic == MIC_1)
		|| (mc_asoc_hs_mic == MIC_2)
		|| (mc_asoc_hs_mic == MIC_3)) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
			}
		}
	}
	else if(incall_mic == MC_ASOC_INCALL_MIC_2MIC) {
		if((mc_asoc_main_mic == MIC_1)
		|| (mc_asoc_main_mic == MIC_2)
		|| (mc_asoc_main_mic == MIC_3)) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
			}
		}
		else if(mc_asoc_main_mic == MIC_PDM) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_PDM)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
			}
		}
	}
	else {
		if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
			if((mc_asoc_main_mic == MIC_1)
			|| (mc_asoc_main_mic == MIC_2)
			|| (mc_asoc_main_mic == MIC_3)) {
				if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				}
			}
			else if(mc_asoc_main_mic == MIC_PDM) {
				if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_PDM)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		|= MCDRV_SRC4_PDM_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				}
			}
		}
		else {
			if((mc_asoc_sub_mic == MIC_1)
			|| (mc_asoc_sub_mic == MIC_2)
			|| (mc_asoc_sub_mic == MIC_3)) {
				if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				}
			}
			else if(mc_asoc_sub_mic == MIC_PDM) {
				if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_PDM)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asCdsp[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
					path_info->asCdsp[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				}
			}
		}
	}

	if(audio_mode == MC_ASOC_AUDIO_MODE_INCOMM) {
		if(get_ap_play_port() == -1) {
			set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
		}
		else {
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff,
					get_ap_play_port(), get_ap_port_block(), get_ap_port_block_on());
		}
	}
	else {
		if(get_cp_port() == DIO_2) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_DIR2)) {
				path_info->asAe[0].abSrcOnOff[get_cp_port_block()]	|= get_cp_port_block_on();
				path_info->asCdsp[2].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asCdsp[2].abSrcOnOff[get_cp_port_block()]	|= MCDRV_SRC3_DIR2_DIRECT_ON;
			}
		}
		else if(get_cp_port() == -1) {
			if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
				set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
			}
			else {
				if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
					if((mc_asoc_main_mic == MIC_1)
					|| (mc_asoc_main_mic == MIC_2)
					|| (mc_asoc_main_mic == MIC_3)) {
						set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					}
					else {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					}
				}
				else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
					if((mc_asoc_sub_mic == MIC_1)
					|| (mc_asoc_sub_mic == MIC_2)
					|| (mc_asoc_sub_mic == MIC_3)) {
						set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					}
					else {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					}
				}
				else {
					set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				}
			}
		}
		else {
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff,
					get_cp_port(), get_cp_port_block(), get_cp_port_block_on());
		}
	}
}

static void set_DAC_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode,
	int	output_path,
	int	incall_mic,
	int	ain_play
)
{
	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if(output_path != MC_ASOC_OUTPUT_PATH_BT) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_ON;
			}
		}
		if(audio_mode != MC_ASOC_AUDIO_MODE_INCOMM) {
			if(get_cp_port() == -1) {
				if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_ON;
				}
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
						path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
						path_info->asDac[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
					}
					else {
						path_info->asDac[1].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
					}
				}
				else {
					if((output_path == MC_ASOC_OUTPUT_PATH_BT) 
					|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)){
						set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asDac[1].abSrcOnOff,
								get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
					}
					else {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[1].abSrcOnOff);
					}
				}
			}
		}
	}
	else if(output_path != MC_ASOC_OUTPUT_PATH_BT) {
		if(ain_play != 0) {
			/*	AnalogIn Playback	*/
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_ON;
			}
		}
		if(audio_mode == MC_ASOC_AUDIO_MODE_AUDIO) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_ON;
			}
		}
	}
}

static void set_AnaOut_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode_play,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic,
	int	ain_play,
	int	dtmf_control
)
{
	int		st_block=MCDRV_SRC_MIC1_BLOCK;
	int		st_block_on1=0;
	int		st_block_on2=0;

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if((mc_asoc_cp_port == LIN1_LOUT1) 
		|| (mc_asoc_cp_port == LIN2_LOUT1)) {
			path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
		}
		else if((mc_asoc_cp_port == LIN1_LOUT2) 
		|| (mc_asoc_cp_port == LIN2_LOUT2)) {
			path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
		}
	}

	if(mc_asoc_sidetone == SIDETONE_ANALOG) {
		if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		|| (audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)) {
			if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
			}
			else if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
				if(get_hs_mic_block_on() != -1) {
					st_block_on1	= get_hs_mic_block_on();
				}
			}
			else {
				if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
					if(get_main_mic_block_on() != -1) {
						st_block_on1	= get_main_mic_block_on();
					}
				}
				else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
					if(get_sub_mic_block_on() != -1) {
						st_block_on1	= get_sub_mic_block_on();
					}
				}
				else {
					if(get_main_mic_block_on() != -1) {
						st_block_on1	= get_main_mic_block_on();
					}
					if(get_sub_mic_block_on() != -1) {
						st_block_on2	= get_sub_mic_block_on();
					}
				}
			}
		}
	}

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		&& (mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON)
		&& (get_cp_port() != -1)
		&& ((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
		 || (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
		 || (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK))) {
		 	/*	SWAP	*/
			if((output_path == MC_ASOC_OUTPUT_PATH_SP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_RC)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_HP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
				if(mc_asoc_sp == SP_STEREO) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
				}
				else if(mc_asoc_sp == SP_MONO_L) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				}
				else {
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
				}
			}
			switch(output_path) {
			case	MC_ASOC_OUTPUT_PATH_SP:
				break;
			case	MC_ASOC_OUTPUT_PATH_RC:
			case	MC_ASOC_OUTPUT_PATH_SP_RC:
				path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
				path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asRcOut[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asRcOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			case	MC_ASOC_OUTPUT_PATH_HP:
			case	MC_ASOC_OUTPUT_PATH_HS:
			case	MC_ASOC_OUTPUT_PATH_SP_HP:
				path_info->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
				path_info->asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asHpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asHpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asHpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asHpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			case	MC_ASOC_OUTPUT_PATH_LO1:
			case	MC_ASOC_OUTPUT_PATH_SP_LO1:
				path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
				path_info->asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout1[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout1[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asLout1[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout1[1].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			case	MC_ASOC_OUTPUT_PATH_LO2:
			case	MC_ASOC_OUTPUT_PATH_SP_LO2:
				path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
				path_info->asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout2[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout2[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asLout2[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout2[1].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			default:
				break;
			}
			return;
		}
	}

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON)
		|| (get_cp_port() == -1)) {
			if((output_path == MC_ASOC_OUTPUT_PATH_SP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_RC)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_HP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
				if(mc_asoc_sp == SP_STEREO) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
				}
				else if(mc_asoc_sp == SP_MONO_L) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				}
				else {
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
					path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
				}
			}
			switch(output_path) {
			case	MC_ASOC_OUTPUT_PATH_SP:
				break;
			case	MC_ASOC_OUTPUT_PATH_RC:
				if(mc_asoc_sidetone == SIDETONE_DIGITAL) {
					if(get_cp_port() == -1) {
						path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					}
				}
			case	MC_ASOC_OUTPUT_PATH_SP_RC:
				path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asRcOut[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asRcOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			case	MC_ASOC_OUTPUT_PATH_HP:
			case	MC_ASOC_OUTPUT_PATH_HS:
				if(mc_asoc_sidetone == SIDETONE_DIGITAL) {
					if(get_cp_port() == -1) {
						path_info->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
						path_info->asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					}
				}
			case	MC_ASOC_OUTPUT_PATH_SP_HP:
				path_info->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asHpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asHpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asHpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asHpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			case	MC_ASOC_OUTPUT_PATH_LO1:
				if(mc_asoc_sidetone == SIDETONE_DIGITAL) {
					if(get_cp_port() == -1) {
						path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
						path_info->asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					}
				}
			case	MC_ASOC_OUTPUT_PATH_SP_LO1:
				path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout1[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout1[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asLout1[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout1[1].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			case	MC_ASOC_OUTPUT_PATH_LO2:
				if(mc_asoc_sidetone == SIDETONE_DIGITAL) {
					if(get_cp_port() == -1) {
						path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
						path_info->asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					}
				}
			case	MC_ASOC_OUTPUT_PATH_SP_LO2:
				path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout2[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout2[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asLout2[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asLout2[1].abSrcOnOff[st_block]	|= st_block_on2;
				break;
			default:
				break;
			}
			return;
		}
	}
	if(audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM) {
		if((mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) 
		|| (get_ap_cap_port() == -1)) {
			if((output_path == MC_ASOC_OUTPUT_PATH_SP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_RC)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_HP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)
			|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
				if(mc_asoc_sp == SP_STEREO) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				}
				else if(mc_asoc_sp == SP_MONO_L) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				}
				else {
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				}
			}
			switch(output_path) {
			case	MC_ASOC_OUTPUT_PATH_SP:
				break;
			case	MC_ASOC_OUTPUT_PATH_RC:
			case	MC_ASOC_OUTPUT_PATH_SP_RC:
				path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				break;
			case	MC_ASOC_OUTPUT_PATH_HP:
			case	MC_ASOC_OUTPUT_PATH_HS:
			case	MC_ASOC_OUTPUT_PATH_SP_HP:
				path_info->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				break;
			case	MC_ASOC_OUTPUT_PATH_LO1:
			case	MC_ASOC_OUTPUT_PATH_SP_LO1:
				path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				break;
			case	MC_ASOC_OUTPUT_PATH_LO2:
			case	MC_ASOC_OUTPUT_PATH_SP_LO2:
				path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				break;
			default:
				break;
			}
			return;
		}
	}

	if((audio_mode_play != MC_ASOC_AUDIO_MODE_OFF)
	|| (ain_play != 0)
	|| (dtmf_control == 1)) {
		if((output_path == MC_ASOC_OUTPUT_PATH_SP)
		|| (output_path == MC_ASOC_OUTPUT_PATH_SP_RC)
		|| (output_path == MC_ASOC_OUTPUT_PATH_SP_HP)
		|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)
		|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)
		|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
			if(mc_asoc_sp == SP_STEREO) {
				path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
				path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
				path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
			}
			else if(mc_asoc_sp == SP_MONO_L) {
				if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				}
				else {
					path_info->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_ON;
				}
				path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asSpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
			}
			else {
				if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				}
				else {
					path_info->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_ON;
				}
				path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
				path_info->asSpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
			}
		}
		switch(output_path) {
		case	MC_ASOC_OUTPUT_PATH_SP:
			break;
		case	MC_ASOC_OUTPUT_PATH_RC:
		case	MC_ASOC_OUTPUT_PATH_SP_RC:
			path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			path_info->asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
			path_info->asRcOut[0].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asRcOut[0].abSrcOnOff[st_block]	|= st_block_on2;
			break;
		case	MC_ASOC_OUTPUT_PATH_HP:
		case	MC_ASOC_OUTPUT_PATH_HS:
		case	MC_ASOC_OUTPUT_PATH_SP_HP:
			path_info->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			path_info->asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
			path_info->asHpOut[0].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asHpOut[0].abSrcOnOff[st_block]	|= st_block_on2;
			path_info->asHpOut[1].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asHpOut[1].abSrcOnOff[st_block]	|= st_block_on2;
			break;
		case	MC_ASOC_OUTPUT_PATH_LO1:
		case	MC_ASOC_OUTPUT_PATH_SP_LO1:
			path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			path_info->asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
			path_info->asLout1[0].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asLout1[0].abSrcOnOff[st_block]	|= st_block_on2;
			path_info->asLout1[1].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asLout1[1].abSrcOnOff[st_block]	|= st_block_on2;
			break;
		case	MC_ASOC_OUTPUT_PATH_LO2:
		case	MC_ASOC_OUTPUT_PATH_SP_LO2:
			path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			path_info->asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
			path_info->asLout2[0].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asLout2[0].abSrcOnOff[st_block]	|= st_block_on2;
			path_info->asLout2[1].abSrcOnOff[st_block]	|= st_block_on1;
			path_info->asLout2[1].abSrcOnOff[st_block]	|= st_block_on2;
			break;
		default:
			break;
		}
	}
}

static void set_ADC_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode_play,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic,
	int	mainmic_play,
	int	submic_play,
	int	msmic_play,
	int	hsmic_play,
	int	lin1_play,
	int	lin2_play
)
{
	int		i;
	int		main_mic_block_on	= get_main_mic_block_on();
	int		sub_mic_block_on	= get_sub_mic_block_on();
	int		hs_mic_block_on		= get_hs_mic_block_on();

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if((output_path != MC_ASOC_OUTPUT_PATH_BT) && (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
			/*	Analog	*/
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;

			if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
				if(hs_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~hs_mic_block_on;
					path_info->asAdc0[0].abSrcOnOff[0]	|= hs_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~hs_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	|= hs_mic_block_on;
					set_BIAS_src(path_info, mc_asoc_hs_mic);
				}
			}
			else {
				if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
					if(main_mic_block_on != -1) {
						path_info->asAdc0[0].abSrcOnOff[0]	&= ~main_mic_block_on;
						path_info->asAdc0[0].abSrcOnOff[0]	|= main_mic_block_on;
						path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
						path_info->asAdc0[1].abSrcOnOff[0]	|= main_mic_block_on;
						set_BIAS_src(path_info, mc_asoc_main_mic);
					}
				}
				else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
					if(sub_mic_block_on != -1) {
						path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
						path_info->asAdc0[0].abSrcOnOff[0]	|= sub_mic_block_on;
						path_info->asAdc0[1].abSrcOnOff[0]	&= ~sub_mic_block_on;
						path_info->asAdc0[1].abSrcOnOff[0]	|= sub_mic_block_on;
						set_BIAS_src(path_info, mc_asoc_sub_mic);
					}
				}
				else {
					/*	2MIC	*/
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~(main_mic_block_on|sub_mic_block_on);
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~(main_mic_block_on|sub_mic_block_on);
					if(main_mic_block_on != -1) {
						path_info->asAdc0[0].abSrcOnOff[0]	|= main_mic_block_on;
						set_BIAS_src(path_info, mc_asoc_main_mic);
					}
					if(sub_mic_block_on != -1) {
						path_info->asAdc0[1].abSrcOnOff[0]	|= sub_mic_block_on;
						set_BIAS_src(path_info, mc_asoc_sub_mic);
					}
				}
			}
			if(audio_mode_play != MC_ASOC_AUDIO_MODE_INCOMM) {
				if(get_cp_port() == -1) {
					if((mc_asoc_cp_port == LIN1_LOUT1) || (mc_asoc_cp_port == LIN1_LOUT2)) {
						path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	|= MCDRV_SRC1_LINE1_M_ON;
					}
					else {
						path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	|= MCDRV_SRC2_LINE2_M_ON;
					}
				}
			}
			else if(audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM) {
				if(get_ap_play_port() == -1) {
					if(mc_asoc_ap_play_port == LIN1) {
						path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	|= MCDRV_SRC1_LINE1_M_ON;
					}
					else {
						path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	|= MCDRV_SRC2_LINE2_M_ON;
					}
				}
			}
			return;
		}
		else if(audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM) {
			if(get_ap_play_port() == -1) {
				if(audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM) {
					if(mc_asoc_ap_play_port == LIN1) {
						path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	|= MCDRV_SRC1_LINE1_L_ON;
						path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	|= MCDRV_SRC1_LINE1_R_ON;
					}
					else {
						path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	|= MCDRV_SRC2_LINE2_L_ON;
						path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	|= MCDRV_SRC2_LINE2_R_ON;
					}
				}
				return;
			}
		}
	}
	else {
		if(((audio_mode_cap != MC_ASOC_AUDIO_MODE_AUDIO) && (audio_mode_cap != MC_ASOC_AUDIO_MODE_INCOMM))
		|| (input_path == MC_ASOC_INPUT_PATH_BT)) {
			if(lin1_play == 1) {
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	|= MCDRV_SRC1_LINE1_L_ON;
				path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	|= MCDRV_SRC1_LINE1_R_ON;
			}
			if(lin2_play == 1) {
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	|= MCDRV_SRC2_LINE2_L_ON;
				path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	|= MCDRV_SRC2_LINE2_R_ON;
			}
			if(mainmic_play == 1) {
				if(main_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	|= main_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	|= main_mic_block_on;
					set_BIAS_src(path_info, mc_asoc_main_mic);
				}
			}
			if(submic_play == 1) {
				if(sub_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	|= sub_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	|= sub_mic_block_on;
					set_BIAS_src(path_info, mc_asoc_sub_mic);
				}
			}
			if(msmic_play == 1) {
				if(main_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	|= main_mic_block_on;
					set_BIAS_src(path_info, mc_asoc_main_mic);
				}
				if(sub_mic_block_on != -1) {
					path_info->asAdc0[1].abSrcOnOff[0]	|= sub_mic_block_on;
					set_BIAS_src(path_info, mc_asoc_sub_mic);
				}
			}
			if(hsmic_play == 1) {
				if(hs_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	|= hs_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	|= hs_mic_block_on;
					set_BIAS_src(path_info, mc_asoc_hs_mic);
				}
			}
		}
	}

	if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		switch(input_path) {
		case	MC_ASOC_INPUT_PATH_MAINMIC:
			if(main_mic_block_on != -1) {
				path_info->asAdc0[0].abSrcOnOff[0]	|= main_mic_block_on;
				path_info->asAdc0[1].abSrcOnOff[0]	|= main_mic_block_on;
				set_BIAS_src(path_info, mc_asoc_main_mic);
			}
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
			break;
		case	MC_ASOC_INPUT_PATH_SUBMIC:
			if(sub_mic_block_on != -1) {
				path_info->asAdc0[0].abSrcOnOff[0]	|= sub_mic_block_on;
				path_info->asAdc0[1].abSrcOnOff[0]	|= sub_mic_block_on;
				set_BIAS_src(path_info, mc_asoc_sub_mic);
			}
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
			break;
		case	MC_ASOC_INPUT_PATH_2MIC:
			if(main_mic_block_on != -1) {
				path_info->asAdc0[0].abSrcOnOff[0]	|= main_mic_block_on;
				path_info->asAdc0[1].abSrcOnOff[0]	|= main_mic_block_on;
				set_BIAS_src(path_info, mc_asoc_main_mic);
			}
			if(sub_mic_block_on != -1) {
				path_info->asAdc0[0].abSrcOnOff[0]	|= sub_mic_block_on;
				path_info->asAdc0[1].abSrcOnOff[0]	|= sub_mic_block_on;
				set_BIAS_src(path_info, mc_asoc_sub_mic);
			}
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
			break;
		case	MC_ASOC_INPUT_PATH_HS:
			if(hs_mic_block_on != -1) {
				path_info->asAdc0[0].abSrcOnOff[0]	|= hs_mic_block_on;
				path_info->asAdc0[1].abSrcOnOff[0]	|= hs_mic_block_on;
				set_BIAS_src(path_info, mc_asoc_hs_mic);
			}
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
			break;
		case	MC_ASOC_INPUT_PATH_LIN1:
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	|= MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	|= MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
			break;
		case	MC_ASOC_INPUT_PATH_LIN2:
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	|= MCDRV_SRC2_LINE2_L_ON;
			path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	|= MCDRV_SRC2_LINE2_R_ON;
			break;
		default:
			break;
		}
	}

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
			if(get_cp_port() == -1) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					if((path_info->asAdc0[0].abSrcOnOff[i] & 0x55) != 0) {
						if((mc_asoc_cp_port == LIN1_LOUT1) || (mc_asoc_cp_port == LIN1_LOUT2)) {
							path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	|= MCDRV_SRC1_LINE1_M_ON;
						}
						else {
							path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	|= MCDRV_SRC2_LINE2_M_ON;
						}
						break;
					}
				}
				if(i >= SOURCE_BLOCK_NUM) {
					if((mc_asoc_cp_port == LIN1_LOUT1) || (mc_asoc_cp_port == LIN1_LOUT2)) {
						path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	|= MCDRV_SRC1_LINE1_L_ON;
						path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	|= MCDRV_SRC1_LINE1_R_ON;
					}
					else {
						path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	|= MCDRV_SRC2_LINE2_L_ON;
						path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	|= MCDRV_SRC2_LINE2_R_ON;
					}
				}
			}
		}
	}
}

static void set_DIT_AP_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic
)
{
	MCDRV_CHANNEL	*asOut;
	int		i;

	TRACE_FUNC();

	if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_INPUT_PATH)) {
		asOut	= (MCDRV_CHANNEL*)((void*)path_info+get_ap_port_srconoff_offset());
		asOut[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;;
		asOut	= (MCDRV_CHANNEL*)((void*)path_info+offsetof(MCDRV_PATH_INFO, asAe));
	}
	else {
		asOut	= (MCDRV_CHANNEL*)((void*)path_info+get_ap_port_srconoff_offset());
	}

	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
		|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
			|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
			|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
						path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
						asOut[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]				|= MCDRV_SRC6_AE_ON;
					}
					else {
						asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
					}
				}
				else {
					if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
						asOut[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
					}
					else {
						asOut[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
					}
				}
			}
			else {
				if((output_path != MC_ASOC_OUTPUT_PATH_BT) && (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
				/*	Analog	*/
					if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
						/*	Headset	*/
						if((mc_asoc_hs_mic == MIC_1)
						|| (mc_asoc_hs_mic == MIC_2)
						|| (mc_asoc_hs_mic == MIC_3)) {
							set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
						}
					}
					else {
						if((incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) || (incall_mic == MC_ASOC_INCALL_MIC_2MIC)) {
							if((mc_asoc_main_mic == MIC_1)
							|| (mc_asoc_main_mic == MIC_2)
							|| (mc_asoc_main_mic == MIC_3)) {
								set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
							}
							else if(mc_asoc_main_mic == MIC_PDM) {
								set_PDM(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
							}
						}
						else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
							if((mc_asoc_sub_mic == MIC_1)
							|| (mc_asoc_sub_mic == MIC_2)
							|| (mc_asoc_sub_mic == MIC_3)) {
								set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
							}
							else if(mc_asoc_sub_mic == MIC_PDM) {
								set_PDM(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
							}
						}
					}
				}
				else {
					/*	BlueTooth	*/
					if(input_path == MC_ASOC_INPUT_PATH_BT) {
						if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
							if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
								path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
								asOut[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]				|= MCDRV_SRC6_AE_ON;
							}
							else {
								asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
							}
						}
						else {
							set_DIR(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff,
									get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
						}
					}
					else {
						set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
					}
				}
			}
			return;
		}
	}

	if((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)
	&& (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
		return;
	}

	switch(input_path) {
	case	MC_ASOC_INPUT_PATH_MAINMIC:
		if((mc_asoc_main_mic == MIC_1)
		|| (mc_asoc_main_mic == MIC_2)
		|| (mc_asoc_main_mic == MIC_3)) {
			if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
				asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
			}
			else {
				set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
			}
		}
		else if(mc_asoc_main_mic == MIC_PDM) {
			if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
				asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
			}
			else {
				set_PDM(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
			}
		}
		break;

	case	MC_ASOC_INPUT_PATH_SUBMIC:
		if((mc_asoc_sub_mic == MIC_1)
		|| (mc_asoc_sub_mic == MIC_2)
		|| (mc_asoc_sub_mic == MIC_3)) {
			if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
				asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
			}
			else {
				set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
			}
		}
		else if(mc_asoc_sub_mic == MIC_PDM) {
			if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
				asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
			}
			else {
				set_PDM(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
			}
		}
		break;

	case	MC_ASOC_INPUT_PATH_2MIC:
		if((mc_asoc_main_mic == MIC_1)
		|| (mc_asoc_main_mic == MIC_2)
		|| (mc_asoc_main_mic == MIC_3)) {
			if((mc_asoc_sub_mic == MIC_1)
			|| (mc_asoc_sub_mic == MIC_2)
			|| (mc_asoc_sub_mic == MIC_3)) {
				if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
					asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
				else {
					set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
				}
			}
		}
		else if(mc_asoc_main_mic == MIC_PDM) {
			if(mc_asoc_sub_mic == MIC_PDM) {
				set_PDM(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
			}
		}
		break;

	case	MC_ASOC_INPUT_PATH_HS:
		if((mc_asoc_hs_mic == MIC_1)
		|| (mc_asoc_hs_mic == MIC_2)
		|| (mc_asoc_hs_mic == MIC_3)) {
			if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
				asOut[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
			}
			else {
				set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
			}
		}
		break;

	case	MC_ASOC_INPUT_PATH_LIN1:
	case	MC_ASOC_INPUT_PATH_LIN2:
		if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_ADC0)) {
			asOut[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else {
			set_ADC0(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff);
		}
		break;

	case	MC_ASOC_INPUT_PATH_BT:
		set_DIR(path_info->asAe[0].abSrcOnOff, asOut[0].abSrcOnOff,
				get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
		break;
	default:	/*	VoiceXXX	*/
		break;
	}

	for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
		if(asOut[0].abSrcOnOff[i] != 0xAA) {
			break;
		}
	}
	if(i >= SOURCE_BLOCK_NUM) {
		asOut[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_ON;
		switch(get_ap_cap_port()) {
		case	DIO_0:
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 0, 1);
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 1);
			break;
		case	DIO_1:
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 0, 1);
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 1);
			break;
		case	DIO_2:
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 0, 1);
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 1);
			break;
		default:
			break;
		}
	}
}

static void set_DIT_CP_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode_cap,
	int	output_path,
	int	incall_mic
)
{
	MCDRV_CHANNEL	*asDit;

	TRACE_FUNC();

	if(audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM) {
		if(get_ap_cap_port() == -1) {
			if(mc_asoc_ap_cap_port == LOUT1) {
				path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			}
			else {
				path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			}
			return;
		}
		asDit	= (MCDRV_CHANNEL*)((void*)path_info+get_ap_port_srconoff_offset());
		if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]				|= MCDRV_SRC6_AE_ON;
			}
			else {
				if(get_ap_cap_port() == DIO_2) {
					asDit[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_DIRECT_ON;
				}
				else {
					asDit[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
			}
			return;
		}
	}
	else {
		if(get_cp_port() == -1) {
			if((mc_asoc_cp_port == LIN1_LOUT1) || (mc_asoc_cp_port == LIN2_LOUT1)) {
				path_info->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			}
			else {
				path_info->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
			}
			return;
		}

		asDit	= (MCDRV_CHANNEL*)((void*)path_info+get_cp_port_srconoff_offset());
		if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]				|= MCDRV_SRC6_AE_ON;
			}
			else {
				if(get_cp_port() == DIO_2) {
					asDit[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_DIRECT_ON;
				}
				else {
					asDit[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
			}
			return;
		}
	}

	if((output_path != MC_ASOC_OUTPUT_PATH_BT) && (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
		/*	Analog	*/
		if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
			/*	Headset	*/
			if((mc_asoc_hs_mic == MIC_1)
			|| (mc_asoc_hs_mic == MIC_2)
			|| (mc_asoc_hs_mic == MIC_3)) {
				set_ADC0(path_info->asAe[0].abSrcOnOff, asDit[0].abSrcOnOff);
			}
		}
		else {
			if((incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) || (incall_mic == MC_ASOC_INCALL_MIC_2MIC)) {
				if((mc_asoc_main_mic == MIC_1)
				|| (mc_asoc_main_mic == MIC_2)
				|| (mc_asoc_main_mic == MIC_3)) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, asDit[0].abSrcOnOff);
				}
				else if(mc_asoc_main_mic == MIC_PDM) {
					set_PDM(path_info->asAe[0].abSrcOnOff, asDit[0].abSrcOnOff);
				}
			}
			else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
				if((mc_asoc_sub_mic == MIC_1)
				|| (mc_asoc_sub_mic == MIC_2)
				|| (mc_asoc_sub_mic == MIC_3)) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, asDit[0].abSrcOnOff);
				}
				else if(mc_asoc_sub_mic == MIC_PDM) {
					set_PDM(path_info->asAe[0].abSrcOnOff, asDit[0].abSrcOnOff);
				}
			}
		}
	}
	else {
		/*	BlueTooth	*/
		set_DIR(path_info->asAe[0].abSrcOnOff, asDit[0].abSrcOnOff,
				get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
	}
}

static void set_DIT_BT_src(
	MCDRV_PATH_INFO		*path_info,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode,
	int	output_path,
	int	ain_play
)
{
	MCDRV_CHANNEL	*asDit;

	TRACE_FUNC();

	asDit	= (MCDRV_CHANNEL*)((void*)path_info+get_bt_port_srconoff_offset());
	if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
		/*	BlueTooth	*/
		if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
				asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
			}
			else {
				asDit[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
			}
		}
		else {
			if(ain_play == 1) {
				/*	AnalogIn Playback	*/
				if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
					asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
				}
				else {
					asDit[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
				}
			}
			else if(audio_mode == MC_ASOC_AUDIO_MODE_AUDIO) {
				if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
					asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_ON;
				}
				else {
					asDit[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
				}
			}
		}
	}
}

static int set_Incall_path(
	MCDRV_PATH_INFO			*path_info,
	struct snd_soc_codec	*codec
)
{
	int		i;
	int		ain_play;
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}

	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (btmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	else {
		ain_play	= 0;
	}

	/*	ADC	*/
	set_ADC_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, mainmic_play, submic_play, msmic_play, hsmic_play, lin1_play, lin2_play);

	/*	C-DSP	*/
	set_CDSP_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic);

	/*	MIX	*/
	if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
		if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
			path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
			path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
		}
		else {
			path_info->asMix[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
		}
	}
	else {
		if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			if(get_cp_port() == -1) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					if((path_info->asAdc1[0].abSrcOnOff[i] & 0x55) != 0) {
						set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						break;
					}
				}
				if(i >= SOURCE_BLOCK_NUM) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
				}
			}
			else {
				set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff,
						get_cp_port(), get_cp_port_block(), get_cp_port_block_on());
			}
		}
		else {
			if(get_ap_play_port() == -1) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					/*	BT	*/
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
				}
				else {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
					set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
				}
			}
			else {
				set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff,
						get_ap_play_port(), get_ap_port_block(), get_ap_port_block_on());
			}
		}
		if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
			/*	BT	*/
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff,
					get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
		}
	}

	/*	AE	*/
	if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
		path_info->asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
	}

	/*	DAC	*/
	set_DAC_src(path_info, mc_asoc, audio_mode, output_path, incall_mic, ain_play);

	/*	AnaOut	*/
	set_AnaOut_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, ain_play, 0);

	/*	DIT_BT	*/
	set_DIT_BT_src(path_info, mc_asoc, audio_mode, output_path, ain_play);

	return 0;
}

static int set_AnaInPlayback_path(
	MCDRV_PATH_INFO			*path_info,
	struct snd_soc_codec	*codec
)
{
	int		ain_play;
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}

	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF)
		|| (input_path == MC_ASOC_INPUT_PATH_BT) ) {
			if(((mainmic_play == 1) && (mc_asoc_main_mic == MIC_PDM))
			|| ((submic_play == 1) && (mc_asoc_sub_mic == MIC_PDM))) {
				/*	PDM-(AE)-MIX	*/
				if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_PDM)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
					path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
					path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				}
			}
			else {
				/*	ADC	*/
				set_ADC_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, mainmic_play, submic_play, msmic_play, hsmic_play, lin1_play, lin2_play);
				/*	ADC-(AE)-MIX	*/
				if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_ADC0)) {
					path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
					path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
						path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
					}
					else {
						path_info->asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
					}
				}
			}
		}
		else if(((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC) && (mc_asoc_main_mic == MIC_PDM))
			 || ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC) && (mc_asoc_sub_mic == MIC_PDM))) {
			/*	PDM-(AE)-MIX	*/
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_PDM)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
				path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
			}
		}
		else if(((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC))
			 || ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC))
			 || ((msmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_2MIC))
			 || ((hsmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_HS))
			 || ((lin1_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN1))
			 || ((lin2_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN2))) {
			/*	ADC	*/
			set_ADC_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, mainmic_play, submic_play, msmic_play, hsmic_play, lin1_play, lin2_play);
			/*	ADC-(AE)-MIX	*/
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_ADC0)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_ADC0)) {
					path_info->asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
				}
				else {
					path_info->asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
				}
			}
		}
	}
	else {
		if(btmic_play == 0) {
			return 0;
		}
	}

	if(btmic_play == 1) {
		ain_play	= 1;
		/*	DIR(BT)-(AE)-MIX	*/
		set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff,
				get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
	}

	/*	AE	*/
	if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
		path_info->asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
	}

	/*	DAC	*/
	set_DAC_src(path_info, mc_asoc, audio_mode, output_path, incall_mic, ain_play);

	/*	AnaOut	*/
	set_AnaOut_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, ain_play, 0);

	/*	DIT_BT	*/
	set_DIT_BT_src(path_info, mc_asoc, audio_mode, output_path, ain_play);

	return 0;
}

static int set_LIN_AnaOut(
	MCDRV_PATH_INFO	*path_info,
	int		output_path
)
{
	int		lin_block, lin_block_on_l, lin_block_on_r;

	TRACE_FUNC();

	if(mc_asoc_ap_play_port == LIN1) {
		lin_block	= MCDRV_SRC_LINE1_L_BLOCK;
		lin_block_on_l	= MCDRV_SRC1_LINE1_L_ON;
		lin_block_on_r	= MCDRV_SRC1_LINE1_R_ON;
	}
	else if(mc_asoc_ap_play_port == LIN2) {
		lin_block	= MCDRV_SRC_LINE2_L_BLOCK;
		lin_block_on_l	= MCDRV_SRC2_LINE2_L_ON;
		lin_block_on_r	= MCDRV_SRC2_LINE2_R_ON;
	}
	else {
		return -EIO;
	}

	if((output_path == MC_ASOC_OUTPUT_PATH_SP)
	|| (output_path == MC_ASOC_OUTPUT_PATH_SP_RC)
	|| (output_path == MC_ASOC_OUTPUT_PATH_SP_HP)
	|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)
	|| (output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)
	|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
		if(mc_asoc_sp == SP_STEREO) {
			path_info->asSpOut[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
			path_info->asSpOut[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
		}
		else if(mc_asoc_sp == SP_MONO_L) {
			path_info->asSpOut[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
		}
		else {
			path_info->asSpOut[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
		}
	}
	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		break;
	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		path_info->asRcOut[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
		path_info->asRcOut[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
		break;
	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		path_info->asHpOut[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
		path_info->asHpOut[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
		break;
	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		path_info->asLout1[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
		path_info->asLout1[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
		break;
	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		path_info->asLout2[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
		path_info->asLout2[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
		break;
	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		break;
	default:
		return -EIO;
	}
	return 0;
}

static int set_AudioPlayback_path(
	MCDRV_PATH_INFO			*path_info,
	struct snd_soc_codec	*codec
)
{
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	int		ain_play	= 0;
	int		lin_block, lin_block_on_l, lin_block_on_r, lin_block_on_m;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (btmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	else {
		ain_play	= 0;
	}

	if(get_ap_play_port() == -1) {
		if(mc_asoc_ap_play_port == LIN1) {
			lin_block		= MCDRV_SRC_LINE1_L_BLOCK;
			lin_block_on_l	= MCDRV_SRC1_LINE1_L_ON;
			lin_block_on_r	= MCDRV_SRC1_LINE1_R_ON;
			lin_block_on_m	= MCDRV_SRC1_LINE1_M_ON;
		}
		else if(mc_asoc_ap_play_port == LIN2) {
			lin_block		= MCDRV_SRC_LINE2_L_BLOCK;
			lin_block_on_l	= MCDRV_SRC2_LINE2_L_ON;
			lin_block_on_r	= MCDRV_SRC2_LINE2_R_ON;
			lin_block_on_m	= MCDRV_SRC2_LINE2_M_ON;
		}
		else {
			return -EIO;
		}
		if(0/*mc_asoc_hwid == MC_ASOC_MA2_HW_ID*/) {
			if((audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
			&& ((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL) || (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(get_cp_port() == -1) {
						/*	LIN->ADC1->MIX	*/
						path_info->asAdc1[0].abSrcOnOff[lin_block]	|= lin_block_on_m;
						set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
					}
					else {
						/*	LIN->ADC0->MIX	*/
						path_info->asAdc0[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
						path_info->asAdc0[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
					}
				}
				else {
					if(get_cp_port() == -1) {
						return set_LIN_AnaOut(path_info, output_path);
					}
					else {
						/*	LIN->ADC1->MIX	*/
						path_info->asAdc1[0].abSrcOnOff[lin_block]	|= lin_block_on_m;
						set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
					}
				}
			}
			else {
				if(ain_play == 0) {
					/*	LIN->ADC0->MIX	*/
					path_info->asAdc0[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
					path_info->asAdc0[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
				}
				else {
					/*	LIN->ADC1->MIX	*/
					path_info->asAdc1[0].abSrcOnOff[lin_block]	|= lin_block_on_m;
					set_ADC1(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
				}
			}
		}
		else {
			if(audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(get_cp_port() == -1) {
						return set_LIN_AnaOut(path_info, output_path);
					}
					else {
						/*	LIN->ADC0->MIX	*/
						path_info->asAdc0[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
						path_info->asAdc0[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
					}
				}
				else {
					return set_LIN_AnaOut(path_info, output_path);
				}
			}
			else {
				if(ain_play == 0) {
					/*	LIN->ADC0->MIX	*/
					path_info->asAdc0[0].abSrcOnOff[lin_block]	|= lin_block_on_l;
					path_info->asAdc0[1].abSrcOnOff[lin_block]	|= lin_block_on_r;
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
				}
				else {
					return set_LIN_AnaOut(path_info, output_path);
				}
			}
		}
	}
	else {
		/*	MIX	*/
		set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff,
				get_ap_play_port(), get_ap_port_block(), get_ap_port_block_on());
	}

	/*	MIX->AE	*/
	if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
		path_info->asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
	}

	/*	DAC	*/
	set_DAC_src(path_info, mc_asoc, audio_mode, output_path, incall_mic, ain_play);

	/*	AnaOut	*/
	set_AnaOut_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, ain_play, 0);

	/*	DIT_BT	*/
	set_DIT_BT_src(path_info, mc_asoc, audio_mode, output_path, ain_play);

	return 0;
}

static int set_AudioCapture_path(
	MCDRV_PATH_INFO			*path_info,
	struct snd_soc_codec	*codec
)
{
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	MCDRV_CHANNEL	*asLout;
	int		mic_block_on	= 0;

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}

	/*	MIX	*/
	if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
		if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
			if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
			|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
			|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					/*	BT	*/
					set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff,
							get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
				}
				else if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
					/*	Headset	*/
					if((mc_asoc_hs_mic == MIC_1)
					|| (mc_asoc_hs_mic == MIC_2)
					|| (mc_asoc_hs_mic == MIC_3)) {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
					}
				}
				else {
					if((incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) || (incall_mic == MC_ASOC_INCALL_MIC_2MIC)) {
						if((mc_asoc_main_mic == MIC_1)
						|| (mc_asoc_main_mic == MIC_2)
						|| (mc_asoc_main_mic == MIC_3)) {
							set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						}
						else if(mc_asoc_main_mic == MIC_PDM) {
							set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						}
					}
					else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
						if((mc_asoc_sub_mic == MIC_1)
						|| (mc_asoc_sub_mic == MIC_2)
						|| (mc_asoc_sub_mic == MIC_3)) {
							set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						}
						else if(mc_asoc_sub_mic == MIC_PDM) {
							set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asMix[0].abSrcOnOff);
						}
					}
				}
			}
		}
	}

	if(get_ap_cap_port() == -1) {
		if(mc_asoc_ap_cap_port == LOUT1) {
			asLout	= (MCDRV_CHANNEL*)((void*)path_info+offsetof(MCDRV_PATH_INFO, asLout1));
		}
		else if(mc_asoc_ap_cap_port == LOUT2) {
			asLout	= (MCDRV_CHANNEL*)((void*)path_info+offsetof(MCDRV_PATH_INFO, asLout2));
		}
		else {
			return -EIO;
		}
		if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
				if((output_path != MC_ASOC_OUTPUT_PATH_BT) && (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
						mic_block_on	= get_hs_mic_block_on();
					}
					else {
						if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
							mic_block_on	= get_main_mic_block_on();
						}
						else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
							mic_block_on	= get_sub_mic_block_on();
						}
						else if(incall_mic == MC_ASOC_INCALL_MIC_2MIC) {
							mic_block_on	= get_main_mic_block_on();
							mic_block_on	|= get_sub_mic_block_on();
						}
					}
					if(input_path == MC_ASOC_INPUT_PATH_VOICECALL) {
						if(mic_block_on > 0) {
							asLout[0].abSrcOnOff[0]	|= mic_block_on;
						}
						if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_INPUT_PATH)) {
							path_info->asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
							path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
						}
						asLout[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					}
					else if(input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK) {
						if(mic_block_on > 0) {
							asLout[0].abSrcOnOff[0]	|= mic_block_on;
							asLout[1].abSrcOnOff[0]	|= mic_block_on;
						}
					}
					else if(input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK) {
						if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_cap == AE_SRC_CAP_INPUT_PATH)) {
							path_info->asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
							path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
						}
						if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
							asLout[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
						}
						else {
							asLout[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
						}
						asLout[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
					}
					else {
						if(mic_block_on > 0) {
							asLout[0].abSrcOnOff[0]	|= mic_block_on;
							asLout[1].abSrcOnOff[0]	|= mic_block_on;
						}
					}
				}
				else {
					if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
					|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
					|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
						if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
							if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
								path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
							}
							else if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_play == AE_SRC_CAP_INPUT_PATH)) {
								path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
							}
							else {
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
							}
						}
						else {
							if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_MIX)) {
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
							}
							else if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_play == AE_SRC_CAP_INPUT_PATH)) {
								path_info->asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
							}
							else {
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_ON;
							}
						}
					}
					else {
						if(input_path == MC_ASOC_INPUT_PATH_BT) {
							if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
								if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_CDSP)) {
									path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
									path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
								}
								else if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_play == AE_SRC_CAP_INPUT_PATH)) {
									path_info->asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
									path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
								}
								else {
									path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
								}
							}
							else {
								if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_play == AE_SRC_CAP_INPUT_PATH)) {
									path_info->asAe[0].abSrcOnOff[get_bt_port_block()]	|= get_bt_port_block_on();
									path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
								}
								else {
									set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff,
											get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
								}
							}
						}
						else {
							if((mc_asoc_ae_dir == MC_ASOC_AE_CAP) && (mc_asoc_ae_src_play == AE_SRC_CAP_INPUT_PATH)) {
								path_info->asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_ON;
								path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
							}
							else {
								set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
							}
						}
					}
					asLout[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
					asLout[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
				}
			}
			return 0;
		}
		else if(audio_mode == MC_ASOC_AUDIO_MODE_INCOMM) {
			if(audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM) {
				return 0;
			}
		}
		if((audio_mode == MC_ASOC_AUDIO_MODE_AUDIO)
		|| ((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC))
		|| ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC))
		|| ((msmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_2MIC))
		|| ((hsmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_HS))
		|| ((lin1_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN1))
		|| ((lin2_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN2))) {
			if(output_path != MC_ASOC_OUTPUT_PATH_BT) {
				/*	DAC in use	*/
				switch(input_path) {
				case	MC_ASOC_INPUT_PATH_MAINMIC:
					if(get_main_mic_block_on() != -1) {
						asLout[0].abSrcOnOff[0]	|= get_main_mic_block_on();
						asLout[1].abSrcOnOff[0]	|= get_main_mic_block_on();
					}
					break;
				case	MC_ASOC_INPUT_PATH_SUBMIC:
					if(get_sub_mic_block_on() != -1) {
						asLout[0].abSrcOnOff[0]	|= get_sub_mic_block_on();
						asLout[1].abSrcOnOff[0]	|= get_sub_mic_block_on();
					}
					break;
				case	MC_ASOC_INPUT_PATH_2MIC:
					if(get_main_mic_block_on() != -1) {
						if(get_sub_mic_block_on() != -1) {
							asLout[0].abSrcOnOff[0]	|= get_main_mic_block_on();
							asLout[1].abSrcOnOff[0]	|= get_sub_mic_block_on();
						}
					}
					break;
				case	MC_ASOC_INPUT_PATH_HS:
					if(get_hs_mic_block_on() != -1) {
						asLout[0].abSrcOnOff[0]	|= get_hs_mic_block_on();
						asLout[1].abSrcOnOff[0]	|= get_hs_mic_block_on();
					}
					break;
				case	MC_ASOC_INPUT_PATH_LIN1:
					asLout[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	|= MCDRV_SRC1_LINE1_L_ON;
					asLout[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	|= MCDRV_SRC1_LINE1_R_ON;
					break;
				case	MC_ASOC_INPUT_PATH_LIN2:
					asLout[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	|= MCDRV_SRC2_LINE2_L_ON;
					asLout[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	|= MCDRV_SRC2_LINE2_R_ON;
					break;
				case	MC_ASOC_INPUT_PATH_BT:
					break;
				default:	/*	VoiceXXX	*/
					break;
				}
				return 0;
			}
		}
		/*	ADC	*/
		set_ADC_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, mainmic_play, submic_play, msmic_play, hsmic_play, lin1_play, lin2_play);
		switch(input_path) {
		case	MC_ASOC_INPUT_PATH_MAINMIC:
			if((mc_asoc_main_mic == MIC_1)
			|| (mc_asoc_main_mic == MIC_2)
			|| (mc_asoc_main_mic == MIC_3)) {
				if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
				else {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
				}
			}
			else if(mc_asoc_main_mic == MIC_PDM) {
				if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
				else {
					set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
				}
			}
			break;
		case	MC_ASOC_INPUT_PATH_SUBMIC:
			if((mc_asoc_sub_mic == MIC_1)
			|| (mc_asoc_sub_mic == MIC_2)
			|| (mc_asoc_sub_mic == MIC_3)) {
				if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
				else {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
				}
			}
			else if(mc_asoc_sub_mic == MIC_PDM) {
				if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
				else {
					set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
				}
			}
			break;
		case	MC_ASOC_INPUT_PATH_2MIC:
			if((mc_asoc_main_mic == MIC_1)
			|| (mc_asoc_main_mic == MIC_2)
			|| (mc_asoc_main_mic == MIC_3)) {
				if((mc_asoc_sub_mic == MIC_1)
				|| (mc_asoc_sub_mic == MIC_2)
				|| (mc_asoc_sub_mic == MIC_3)) {
					if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
						path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
					}
					else {
						set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
					}
				}
			}
			else if(mc_asoc_main_mic == MIC_PDM) {
				if(mc_asoc_sub_mic == MIC_PDM) {
					set_PDM(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
				}
			}
			break;
		case	MC_ASOC_INPUT_PATH_HS:
			if((mc_asoc_hs_mic == MIC_1)
			|| (mc_asoc_hs_mic == MIC_2)
			|| (mc_asoc_hs_mic == MIC_3)) {
				if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, 0) != 0) {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[0].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[1].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[2].abSrcOnOff);
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asCdsp[3].abSrcOnOff);
					path_info->asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_ON;
				}
				else {
					set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
				}
			}
			break;
		case	MC_ASOC_INPUT_PATH_LIN1:
		case	MC_ASOC_INPUT_PATH_LIN2:
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_ADC0)) {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				set_ADC0(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff);
			}
			break;

		case	MC_ASOC_INPUT_PATH_BT:
			set_DIR(path_info->asAe[0].abSrcOnOff, path_info->asDac[0].abSrcOnOff,
					get_bt_port(), get_bt_port_block(), get_bt_port_block_on());
			break;
		default:	/*	VoiceXXX	*/
			return 0;
		}
		asLout[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_ON;
		asLout[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_ON;
	}
	else {
		/*	ADC	*/
		set_ADC_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, mainmic_play, submic_play, msmic_play, hsmic_play, lin1_play, lin2_play);
		/*	DIT_AP	*/
		set_DIT_AP_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic);
	}
	return 0;
}

static int get_path_info(
	struct snd_soc_codec	*codec,
	MCDRV_PATH_INFO			*path_info
)
{
	int		err	= 0;
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	int		ain_play	= 0;
	int		dtmf_control;
	int		dtmf_output;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}
	if((dtmf_control = read_cache(codec, MC_ASOC_DTMF_CONTROL)) < 0) {
		return -EIO;
	}
	if((dtmf_output = read_cache(codec, MC_ASOC_DTMF_OUTPUT)) < 0) {
		return -EIO;
	}

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (btmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	else {
		ain_play	= 0;
	}

	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if((err = set_Incall_path(path_info, codec)) < 0) {
			return err;
		}
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
			/*	DIT_CP	*/
			set_DIT_CP_src(path_info, mc_asoc, audio_mode_cap, output_path, incall_mic);
		}
	}
	else {
		if(ain_play == 1) {
			if((err = set_AnaInPlayback_path(path_info, codec)) < 0) {
				return err;
			}
		}
	}

	if((audio_mode == MC_ASOC_AUDIO_MODE_AUDIO)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((err = set_AudioPlayback_path(path_info, codec)) < 0) {
			return err;
		}
	}

	if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if((err = set_AudioCapture_path(path_info, codec)) < 0) {
			return err;
		}
	}

	if(dtmf_control == 1) {
		if((dtmf_output == MC_ASOC_DTMF_OUTPUT_SP)
		&& (output_path != MC_ASOC_OUTPUT_PATH_SP)) {
		}
		else {
			if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY) && (mc_asoc_ae_src_play == AE_SRC_PLAY_DTMF)) {
				path_info->asAe[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_ON;
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_ON;
			}
			else {
				path_info->asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_ON;
			}
			/*	AnaOut	*/
			set_AnaOut_src(path_info, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, ain_play, dtmf_control);
		}
	}

	if(mc_asoc_mic1_bias == BIAS_ON_ALWAYS) {
		path_info->asBias[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK]	|= MCDRV_SRC0_MIC1_ON;
	}
	if(mc_asoc_mic2_bias == BIAS_ON_ALWAYS) {
		path_info->asBias[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK]	|= MCDRV_SRC0_MIC2_ON;
	}
	if(mc_asoc_mic3_bias == BIAS_ON_ALWAYS) {
		path_info->asBias[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK]	|= MCDRV_SRC0_MIC3_ON;
	}

	return 0;
}

static void set_incall_mute_flg(
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic
)
{
	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
			switch(get_cp_port()) {
			case	DIO_0:
				set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 1);
				break;
			case	DIO_1:
				set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 1);
				break;
			case	DIO_2:
				set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 1);
				break;
			default:
				break;
			}
		}
		if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
			if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
			|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
			|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
				if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
					if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
						/*	DIR(BT)_ATTR	*/
						set_vol_mute_flg(get_bt_DIR_ATT_offset(), 1, 1);
					}
					else {
						/*	ADC_ATTR	*/
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset, 1, 1);
					}
					if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
					|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
						/*	VOICE_ATTL	*/
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset, 0, 1);
					}
					else {
						/*	DIR(AP)_ATTL	*/
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset, 0, 1);
					}
				}
				if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
				|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
					/*	DIR(AP)_ATTL	*/
					set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset, 0, 1);
				}
			}
			else {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
						switch(get_ap_cap_port()) {
						case	DIO_0:
							set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 1);
							break;
						case	DIO_1:
							set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 1);
							break;
						case	DIO_2:
							set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 1);
							break;
						default:
							break;
						}
					}
				}
			}
		}
	}
}

static int get_ain_play(
	struct snd_soc_codec *codec
)
{
	int		ain_play	= 0;
	int		cache;

	TRACE_FUNC();

	if((cache = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	if((cache = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	if((cache = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	if((cache = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	if((cache = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	if((cache = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	if((cache = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	ain_play	|= (cache>0);
	return ain_play;
}

static int is_same_cdsp_param(
	struct mc_asoc_data	*mc_asoc,
	int	base,
	int	user
)
{
	int		i;

	if(mc_asoc->cdsp_store.param_count < mc_asoc->dsp_voicecall.param_count[base]) {
		dbg_info("cdsp_store.param_count < dsp_voicecall.param_count[base]\n");
		return 0;
	}
	for(i = 0; i < mc_asoc->dsp_voicecall.param_count[base]; i++) {
		dbg_info("cdsp_store.cdspparam[%d]=%02X %02X %02X %02X ...\n",
			i,
			mc_asoc->cdsp_store.cdspparam[i].bCommand,
			mc_asoc->cdsp_store.cdspparam[i].abParam[0],
			mc_asoc->cdsp_store.cdspparam[i].abParam[1],
			mc_asoc->cdsp_store.cdspparam[i].abParam[2]);
		dbg_info("dsp_voicecall.cdspparam[base][%d]=%02X %02X %02X %02X ...\n",
			i,
			mc_asoc->dsp_voicecall.cdspparam[base][i].bCommand,
			mc_asoc->dsp_voicecall.cdspparam[base][i].abParam[0],
			mc_asoc->dsp_voicecall.cdspparam[base][i].abParam[1],
			mc_asoc->dsp_voicecall.cdspparam[base][i].abParam[2]);
		if(memcmp(&mc_asoc->cdsp_store.cdspparam[i],
					&mc_asoc->dsp_voicecall.cdspparam[base][i],
					sizeof(MCDRV_CDSPPARAM)) != 0) {
			return 0;
		}
	}

	if(user > 0) {
		if(mc_asoc->cdsp_store.param_count <
			(mc_asoc->dsp_voicecall.param_count[base]+mc_asoc->dsp_voicecall.param_count[user])) {
			dbg_info("cdsp_store.param_count < (dsp_voicecall.param_count[base]+dsp_voicecall.param_count[user])\n");
			return 0;
		}
		for(i = 0; i < mc_asoc->dsp_voicecall.param_count[user]; i++) {
			dbg_info("cdsp_store.cdspparam[%d]=%02X %02X %02X %02X ...\n",
				mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i,
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].bCommand,
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].abParam[0],
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].abParam[1],
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].abParam[2]);
			dbg_info("dsp_voicecall.cdspparam[user][%d]=%02X %02X %02X %02X ...\n",
				i,
				mc_asoc->dsp_voicecall.cdspparam[user][i].bCommand,
				mc_asoc->dsp_voicecall.cdspparam[user][i].abParam[0],
				mc_asoc->dsp_voicecall.cdspparam[user][i].abParam[1],
				mc_asoc->dsp_voicecall.cdspparam[user][i].abParam[2]);
			if(memcmp(
				&mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i],
				&mc_asoc->dsp_voicecall.cdspparam[user][i],
				sizeof(MCDRV_CDSPPARAM)) != 0) {
				return 0;
			}
		}
	}
	else {
		if(mc_asoc->cdsp_store.param_count != mc_asoc->dsp_voicecall.param_count[base]) {
dbg_info("cdsp_store.param_count:%d != dsp_voicecall.param_count[base]:%d\n",
			mc_asoc->cdsp_store.param_count, mc_asoc->dsp_voicecall.param_count[base]);
			return 0;
		}
	}
	return 1;
}

static int connect_path(
	struct snd_soc_codec *codec
)
{
	int		err;
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	int		ain_play	= 0;
	int		ain_play_cap	= 0;
	int		base	= 0;
	int		user	= 0;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	MCDRV_PATH_INFO	path_info;
	MCDRV_DAC_INFO	dac_info;
	UINT8	bMasterSwap=MCDRV_DSWAP_OFF;

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (btmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	else {
		ain_play	= 0;
	}

	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
			get_voicecall_index(output_path, incall_mic, &base, &user);
			if((mc_asoc_hf == HF_ON) && (mc_asoc->dsp_voicecall.onoff[base] == 1)) {
				/*	C-DSP enable	*/
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
					if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
						return mc_asoc_map_drv_error(err);
					}
					mc_asoc->ae_ex_store.size	= 0;
				}
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if(is_same_cdsp_param(mc_asoc, base, user) == 0) {
						if((err = set_cdsp_off(mc_asoc)) < 0) {
							return err;
						}
						if((err = set_cdsp_param(codec)) < 0) {
							return err;
						}
					}
				}
				else {
					if((err = set_cdsp_param(codec)) < 0) {
						return err;
					}
				}
			}
			else {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((err = set_cdsp_off(mc_asoc)) < 0) {
						return err;
					}
				}
			}
		}
		if(mc_asoc_sidetone == SIDETONE_DIGITAL) {
			if((output_path == MC_ASOC_OUTPUT_PATH_RC)
			|| (output_path == MC_ASOC_OUTPUT_PATH_HS)
			|| (output_path == MC_ASOC_OUTPUT_PATH_HP)
			|| (output_path == MC_ASOC_OUTPUT_PATH_LO1)
			|| (output_path == MC_ASOC_OUTPUT_PATH_LO2)) {
				if(get_cp_port() != -1) {
					bMasterSwap	= MCDRV_DSWAP_MONO;
				}
			}
		}
		else if(output_path != MC_ASOC_OUTPUT_PATH_BT) {
			if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
				|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
				|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
					if((mc_asoc_hf == HF_OFF) || (mc_asoc->dsp_voicecall.onoff[base] == 0)) {
						if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
						|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
							if(get_cp_port() != -1) {
								bMasterSwap	= MCDRV_DSWAP_BOTHR;
							}
						}
						else {
							bMasterSwap	= MCDRV_DSWAP_BOTHR;
						}
					}
				}
			}
		}
		else {
			if(get_ap_cap_port() == -1) {
				if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
				|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
					if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
					|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
						if(input_path == MC_ASOC_INPUT_PATH_VOICECALL) {
						}
						else if(input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK) {
							bMasterSwap	= MCDRV_DSWAP_BOTHL;
						}
						else if(input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK) {
							bMasterSwap	= MCDRV_DSWAP_BOTHR;
						}
						else {
							bMasterSwap	= MCDRV_DSWAP_BOTHL;
						}
					}
				}
			}
		}
	}
	else {
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO) || (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
			if(((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC))
			|| ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC))
			|| ((msmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_2MIC))
			|| ((hsmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_HS))
			|| ((lin1_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN1))
			|| ((lin2_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN2))) {
				ain_play_cap	= 1;
			}
		}

		if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
			if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
				if((use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, ain_play_cap) == 0)
				|| (is_same_cdsp_param(mc_asoc, input_path==MC_ASOC_INPUT_PATH_2MIC?1:0, -1) == 0)) {
					if((err = set_cdsp_off(mc_asoc)) < 0) {
						return err;
					}
				}
			}
		}
	}

	switch(get_ap_cap_port()) {
	case	DIO_0:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 0, 0);
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 0);
		break;
	case	DIO_1:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 0, 0);
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 0);
		break;
	case	DIO_2:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 0, 0);
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 0);
		break;
	default:
		break;
	}
	set_incall_mute_flg(mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic);

	/*	AE_EX	*/
	if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
		if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
			if(use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, ain_play_cap) != 0) {
				/*	C-DSP enable	*/
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
					if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
						return mc_asoc_map_drv_error(err);
					}
					mc_asoc->ae_ex_store.size	= 0;
				}
				if((err = set_cdsp_param(codec)) < 0) {
					return err;
				}
			}
			if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
				if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
					if((audio_mode != MC_ASOC_AUDIO_MODE_OFF)
					|| (ain_play != 0)) {
						if((err = set_audioengine_ex(codec)) != 0) {
							return err;
						}
					}
				}
				else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
					if(audio_mode_cap != MC_ASOC_AUDIO_MODE_OFF) {
						if((err = set_audioengine_ex(codec)) != 0) {
							return err;
						}
					}
				}
			}
		}
	}
	if((err = set_audioengine(codec)) < 0) {
		return err;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_DAC, (void *)&dac_info, 0)) < MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	if((dac_info.bMasterSwap == MCDRV_DSWAP_OFF) && (bMasterSwap != dac_info.bMasterSwap)) {
		dac_info.bMasterSwap	= bMasterSwap;
		if((err = _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&dac_info, MCDRV_DAC_MSWP_UPDATE_FLAG)) < MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(err);
		}
	}

	memset(&path_info, 0xaa, sizeof(MCDRV_PATH_INFO));
	if((err = get_path_info(codec, &path_info)) < 0) {
		return err;
	}
	set_volume(codec);
	if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}

	if(bMasterSwap != dac_info.bMasterSwap) {
		dac_info.bMasterSwap	= bMasterSwap;
		if((err = _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&dac_info, MCDRV_DAC_MSWP_UPDATE_FLAG)) < MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(err);
		}
	}

	if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
		if((audio_mode != MC_ASOC_AUDIO_MODE_INCALL)
		&& (audio_mode != MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		&& (audio_mode != MC_ASOC_AUDIO_MODE_INCOMM)) {
			if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
				if((is_cdsp_used() != 1)
				|| (use_cdsp4cap(mc_asoc->dsp_voicecall.onoff, audio_mode, audio_mode_cap, input_path, output_path, ain_play_cap) == 0)) {
					if((err = set_cdsp_off(mc_asoc)) < 0) {
						return err;
					}
				}
			}
		}
		if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_NONE) {
				if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				mc_asoc->ae_ex_store.size	= 0;
				mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_AE_EX_OFF;
			}
		}
	}

	switch(get_cp_port()) {
	case	DIO_0:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 0);
		break;
	case	DIO_1:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 0);
		break;
	case	DIO_2:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 0);
		break;
	default:
		break;
	}
	/*	DIR(BT)_ATTR	*/
	set_vol_mute_flg(get_bt_DIR_ATT_offset(), 1, 0);
	/*	ADC_ATTR	*/
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset, 1, 0);
	/*	ADC0_ATTL	*/
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset, 0, 0);
	/*	ADC1_ATTL	*/
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset, 0, 0);
	/*	VOICE_ATTL	*/
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset, 0, 0);
	/*	DIR(AP)_ATTL	*/
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset, 0, 0);
	switch(get_ap_cap_port()) {
	case	DIO_0:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1,
						get_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 0));
		break;
	case	DIO_1:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1,
						get_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 0));
		break;
	case	DIO_2:
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1,
						get_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 0));
		break;
	default:
		break;
	}

	set_incall_mute_flg(mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic);

	set_volume(codec);
	return err;
}


/*
 * DAI (PCM interface)
 */
/* SRC_RATE settings @ 73728000Hz (ideal PLL output) */
static int	mc_asoc_src_rate[][SNDRV_PCM_STREAM_LAST+1]	= {
	/* DIR, DIT */
	{32768, 4096},					/* MCDRV_FS_48000 */
	{30106, 4458},					/* MCDRV_FS_44100 */
	{21845, 6144},					/* MCDRV_FS_32000 */
	{0, 0},							/* N/A */
	{0, 0},							/* N/A */
	{15053, 8916},					/* MCDRV_FS_22050 */
	{10923, 12288},					/* MCDRV_FS_16000 */
	{0, 0},							/* N/A */
	{0, 0},							/* N/A */
	{7526, 17833},					/* MCDRV_FS_11025 */
	{5461, 24576},					/* MCDRV_FS_8000 */
};
#define mc_asoc_fs_to_srcrate(rate,dir)	mc_asoc_src_rate[(rate)][(dir)];

static int is_dio_modified(
	const MCDRV_DIO_PORT	*port,
	const struct mc_asoc_setup	*setup,
	int	id,
	int	mode,
	UINT32	update
)
{
	int		i, err;
	MCDRV_DIO_INFO	cur_dio;

	if((err = _McDrv_Ctrl(MCDRV_GET_DIGITALIO, &cur_dio, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}

	if((update & (MCDRV_DIO0_COM_UPDATE_FLAG|MCDRV_DIO1_COM_UPDATE_FLAG|MCDRV_DIO2_COM_UPDATE_FLAG)) != 0) {
		if((cur_dio.asPortInfo[id].sDioCommon.bMasterSlave	!= port->sDioCommon.bMasterSlave)
		|| (cur_dio.asPortInfo[id].sDioCommon.bAutoFs		!= port->sDioCommon.bAutoFs)
		|| (cur_dio.asPortInfo[id].sDioCommon.bFs			!= port->sDioCommon.bFs)
		|| (cur_dio.asPortInfo[id].sDioCommon.bBckFs		!= port->sDioCommon.bBckFs)
		|| (cur_dio.asPortInfo[id].sDioCommon.bInterface	!= port->sDioCommon.bInterface)
		|| (cur_dio.asPortInfo[id].sDioCommon.bBckInvert	!= port->sDioCommon.bBckInvert)) {
			return 1;
		}
		if(mode == MCDRV_DIO_PCM) {
			if((cur_dio.asPortInfo[id].sDioCommon.bPcmHizTim	!= port->sDioCommon.bPcmHizTim)
			|| (cur_dio.asPortInfo[id].sDioCommon.bPcmClkDown	!= port->sDioCommon.bPcmClkDown)
			|| (cur_dio.asPortInfo[id].sDioCommon.bPcmFrame		!= port->sDioCommon.bPcmFrame)
			|| (cur_dio.asPortInfo[id].sDioCommon.bPcmHighPeriod!= port->sDioCommon.bPcmHighPeriod)) {
				return 1;
			}
		}
	}

	if((update & (MCDRV_DIO0_DIR_UPDATE_FLAG|MCDRV_DIO1_DIR_UPDATE_FLAG|MCDRV_DIO2_DIR_UPDATE_FLAG)) != 0) {
		if(cur_dio.asPortInfo[id].sDir.wSrcRate	!= port->sDir.wSrcRate) {
			return 1;
		}
		if (mode == MCDRV_DIO_DA) {
			if((cur_dio.asPortInfo[id].sDir.sDaFormat.bBitSel	!= port->sDir.sDaFormat.bBitSel)
			|| (cur_dio.asPortInfo[id].sDir.sDaFormat.bMode		!= port->sDir.sDaFormat.bMode)) {
				return 1;
			}
		}
		else {
			if((cur_dio.asPortInfo[id].sDir.sPcmFormat.bMono	!= port->sDir.sPcmFormat.bMono)
			|| (cur_dio.asPortInfo[id].sDir.sPcmFormat.bOrder	!= port->sDir.sPcmFormat.bOrder)
			|| (cur_dio.asPortInfo[id].sDir.sPcmFormat.bLaw		!= port->sDir.sPcmFormat.bLaw)
			|| (cur_dio.asPortInfo[id].sDir.sPcmFormat.bBitSel	!= port->sDir.sPcmFormat.bBitSel)) {
				return 1;
			}
			if (setup->pcm_extend[id]) {
				if(cur_dio.asPortInfo[id].sDir.sPcmFormat.bOrder	!= port->sDir.sPcmFormat.bOrder) {
					return 1;
				}
			}
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			if(cur_dio.asPortInfo[id].sDir.abSlot[i]	!= port->sDir.abSlot[i]) {
				return 1;
			}
		}
	}

	if((update & (MCDRV_DIO0_DIT_UPDATE_FLAG|MCDRV_DIO1_DIT_UPDATE_FLAG|MCDRV_DIO2_DIT_UPDATE_FLAG)) != 0) {
		if(cur_dio.asPortInfo[id].sDit.wSrcRate	!= port->sDit.wSrcRate) {
			return 1;
		}
		if (mode == MCDRV_DIO_DA) {
			if((cur_dio.asPortInfo[id].sDit.sDaFormat.bBitSel	!= port->sDit.sDaFormat.bBitSel)
			|| (cur_dio.asPortInfo[id].sDit.sDaFormat.bMode		!= port->sDit.sDaFormat.bMode)) {
				return 1;
			}
		}
		else {
			if((cur_dio.asPortInfo[id].sDit.sPcmFormat.bMono	!= port->sDit.sPcmFormat.bMono)
			|| (cur_dio.asPortInfo[id].sDit.sPcmFormat.bOrder	!= port->sDit.sPcmFormat.bOrder)
			|| (cur_dio.asPortInfo[id].sDit.sPcmFormat.bLaw		!= port->sDit.sPcmFormat.bLaw)
			|| (cur_dio.asPortInfo[id].sDit.sPcmFormat.bBitSel	!= port->sDit.sPcmFormat.bBitSel)) {
				return 1;
			}
			if (setup->pcm_extend[id]) {
				if(cur_dio.asPortInfo[id].sDir.sPcmFormat.bOrder	!= port->sDir.sPcmFormat.bOrder) {
					return 1;
				}
			}
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			if(cur_dio.asPortInfo[id].sDit.abSlot[i]	!= port->sDit.abSlot[i]) {
				return 1;
			}
		}
	}
	return 0;
}

static int setup_dai(
	struct snd_soc_codec	*codec,
	struct mc_asoc_data	*mc_asoc,
	int	id,
	int	mode,
	int	dir
)
{
	MCDRV_DIO_INFO	dio;
	MCDRV_DIO_PORT	*port	= &dio.asPortInfo[id];
	struct mc_asoc_setup		*setup		= &mc_asoc->setup;
	struct mc_asoc_port_params	*port_prm	= &mc_asoc->port[id];
	UINT32	update	= 0;
	int		i, err, modify;
	MCDRV_PATH_INFO	path_info, tmp_path_info;
	MCDRV_CHANNEL	*asDit;

	if((get_ap_play_port() == -1) && (get_ap_cap_port() == -1)) {
		return 0;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}

	asDit	= (MCDRV_CHANNEL*)((void*)&path_info+get_ap_port_srconoff_offset());

	if((get_cp_port() == get_ap_play_port()) || (get_bt_port() == get_ap_play_port())) {
		for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
			if((path_info.asDit0[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
			if((path_info.asDit1[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
			if((path_info.asDit2[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < DAC_PATH_CHANNELS; i++) {
			if((path_info.asDac[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < AE_PATH_CHANNELS; i++) {
			if((path_info.asAe[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < MIX_PATH_CHANNELS; i++) {
			if((path_info.asMix[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
			if((path_info.asPeak[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
			if((path_info.asCdsp[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
		for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
			if((path_info.asMix2[i].abSrcOnOff[get_ap_port_block()] & get_ap_port_block_on()) != 0) {
				return 0;
			}
		}
	}
	else if((get_cp_port() == get_ap_cap_port()) || (get_bt_port() == get_ap_cap_port())) {
		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			if((asDit[0].abSrcOnOff[i] & 0x55) != 0) {
				return 0;
			}
		}
	}

	memset(&dio, 0, sizeof(MCDRV_DIO_INFO));

	if (port_prm->stream == 0) {
		port->sDioCommon.bMasterSlave	= port_prm->master;
		port->sDioCommon.bAutoFs		= MCDRV_AUTOFS_OFF;
		port->sDioCommon.bFs			= port_prm->rate;
		port->sDioCommon.bBckFs			= port_prm->bckfs;
		port->sDioCommon.bInterface		= mode;
		port->sDioCommon.bBckInvert		= port_prm->inv;
		if (mode == MCDRV_DIO_PCM) {
			port->sDioCommon.bPcmHizTim		= setup->pcm_hiz_redge[id];
			port->sDioCommon.bPcmClkDown	= port_prm->pcm_clkdown;
			port->sDioCommon.bPcmFrame		= port_prm->format;
			port->sDioCommon.bPcmHighPeriod	= setup->pcm_hperiod[id];
		}
		if (dir == SNDRV_PCM_STREAM_PLAYBACK) {
			switch(get_ap_play_port()) {
			case	DIO_0:
				update |= MCDRV_DIO0_COM_UPDATE_FLAG;
				break;
			case	DIO_1:
				update |= MCDRV_DIO1_COM_UPDATE_FLAG;
				break;
			case	DIO_2:
				update |= MCDRV_DIO2_COM_UPDATE_FLAG;
				break;
			default:
				break;
			}
		}
		else {
			switch(get_ap_cap_port()) {
			case	DIO_0:
				update |= MCDRV_DIO0_COM_UPDATE_FLAG;
				break;
			case	DIO_1:
				update |= MCDRV_DIO1_COM_UPDATE_FLAG;
				break;
			case	DIO_2:
				update |= MCDRV_DIO2_COM_UPDATE_FLAG;
				break;
			default:
				break;
			}
		}
	}

	if (dir == SNDRV_PCM_STREAM_PLAYBACK) {
		port->sDir.wSrcRate	= mc_asoc_fs_to_srcrate(port_prm->rate, dir);
		if (mode == MCDRV_DIO_DA) {
			port->sDir.sDaFormat.bBitSel	= port_prm->bits[dir];
			port->sDir.sDaFormat.bMode		= port_prm->format;
		} 
		else {
			port->sDir.sPcmFormat.bMono		= port_prm->pcm_mono[dir];
			port->sDir.sPcmFormat.bOrder	= port_prm->pcm_order[dir];
			if (setup->pcm_extend[id]) {
				port->sDir.sPcmFormat.bOrder |=
					(1 << setup->pcm_extend[id]);
			}
			port->sDir.sPcmFormat.bLaw		= port_prm->pcm_law[dir];
			port->sDir.sPcmFormat.bBitSel	= port_prm->bits[dir];
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			if ((i > 0) && port_prm->channels == 1) {
				port->sDir.abSlot[i]	= port->sDir.abSlot[i-1];
			} 
			else {
				port->sDir.abSlot[i]	= setup->slot[id][dir][i];
			}
		}
		switch(get_ap_play_port()) {
		case	DIO_0:
			update |= MCDRV_DIO0_DIR_UPDATE_FLAG;
			break;
		case	DIO_1:
			update |= MCDRV_DIO1_DIR_UPDATE_FLAG;
			break;
		case	DIO_2:
			update |= MCDRV_DIO2_DIR_UPDATE_FLAG;
			break;
		default:
			break;
		}
	}

	if (dir == SNDRV_PCM_STREAM_CAPTURE) {
		port->sDit.wSrcRate	= mc_asoc_fs_to_srcrate(port_prm->rate, dir);
		if (mode == MCDRV_DIO_DA) {
			port->sDit.sDaFormat.bBitSel	= port_prm->bits[dir];
			port->sDit.sDaFormat.bMode		= port_prm->format;
		} 
		else {
			port->sDit.sPcmFormat.bMono		= port_prm->pcm_mono[dir];
			port->sDit.sPcmFormat.bOrder	= port_prm->pcm_order[dir];
			if (setup->pcm_extend[id]) {
				port->sDit.sPcmFormat.bOrder |= (1 << setup->pcm_extend[id]);
			}
			port->sDit.sPcmFormat.bLaw		= port_prm->pcm_law[dir];
			port->sDit.sPcmFormat.bBitSel	= port_prm->bits[dir];
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			port->sDit.abSlot[i]	= setup->slot[id][dir][i];
		}
		switch(get_ap_cap_port()) {
		case	DIO_0:
			update |= MCDRV_DIO0_DIT_UPDATE_FLAG;
			break;
		case	DIO_1:
			update |= MCDRV_DIO1_DIT_UPDATE_FLAG;
			break;
		case	DIO_2:
			update |= MCDRV_DIO2_DIT_UPDATE_FLAG;
			break;
		default:
			break;
		}
	}

	if((modify = is_dio_modified(port, setup, id, mode, update)) < 0) {
		return -EIO;
	}
	if(modify == 0) {
		dbg_info("modify == 0\n");
		return 0;
	}

	memcpy(&tmp_path_info, &path_info, sizeof(MCDRV_PATH_INFO));
	if((dir == SNDRV_PCM_STREAM_PLAYBACK)
	|| ((port_prm->stream == 0) && (get_ap_play_port() == get_ap_cap_port()))) {
		for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
			tmp_path_info.asDit0[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asDit0[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
			tmp_path_info.asDit1[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asDit1[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
			tmp_path_info.asDit2[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asDit2[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < DAC_PATH_CHANNELS; i++) {
			tmp_path_info.asDac[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asDac[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < AE_PATH_CHANNELS; i++) {
			tmp_path_info.asAe[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asAe[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < MIX_PATH_CHANNELS; i++) {
			tmp_path_info.asMix[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asMix[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
			tmp_path_info.asPeak[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asPeak[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
			tmp_path_info.asCdsp[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asCdsp[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
		for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
			tmp_path_info.asMix2[i].abSrcOnOff[get_ap_port_block()]	&= ~(get_ap_port_block_on());
			tmp_path_info.asMix2[i].abSrcOnOff[get_ap_port_block()]	|= (get_ap_port_block_on()<<1);
		}
	}
	if((dir == SNDRV_PCM_STREAM_CAPTURE)
	|| ((port_prm->stream == 0) && (get_ap_play_port() == get_ap_cap_port()))) {
		asDit	= (MCDRV_CHANNEL*)((void*)&tmp_path_info+get_ap_port_srconoff_offset());
		asDit[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	&= ~MCDRV_SRC4_PDM_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= ~MCDRV_SRC4_ADC0_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= ~MCDRV_SRC4_ADC1_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= ~MCDRV_SRC3_DIR0_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= ~MCDRV_SRC3_DIR1_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= ~MCDRV_SRC3_DIR2_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		&= ~MCDRV_SRC6_AE_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= ~MCDRV_SRC6_MIX_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= ~MCDRV_SRC4_DTMF_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
		asDit[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= ~MCDRV_SRC6_CDSP_ON;
		asDit[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
	}

	if(memcmp(&tmp_path_info, &path_info, sizeof(MCDRV_PATH_INFO)) == 0) {
		modify	= 0;
	}
	else {
		if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &tmp_path_info, 0)) != MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(err);
		}
		if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
			if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			mc_asoc->ae_ex_store.size	= 0;
			mc_asoc_cdsp_ae_ex_flag		= MC_ASOC_CDSP_AE_EX_OFF;
		}
	}

	if((err = _McDrv_Ctrl(MCDRV_SET_DIGITALIO, &dio, update)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	if(modify != 0) {
		return connect_path(codec);
	}
	return 0;
}

static int update_clock(struct mc_asoc_data *mc_asoc)
{
	MCDRV_CLOCK_INFO info;

	memset(&info, 0, sizeof(MCDRV_CLOCK_INFO));
	info.bCkSel	= mc_asoc->setup.init.bCkSel;
	info.bDivR0	= mc_asoc->setup.init.bDivR0;
	info.bDivF0	= mc_asoc->setup.init.bDivF0;
	info.bDivR1	= mc_asoc->setup.init.bDivR1;
	info.bDivF1	= mc_asoc->setup.init.bDivF1;

	return _McDrv_Ctrl(MCDRV_UPDATE_CLOCK, &info, 0);
}

static int set_clkdiv_common(
	struct mc_asoc_data *mc_asoc,
	int div_id,
	int div)
{
	struct mc_asoc_setup *setup	= &mc_asoc->setup;

	switch (div_id) {
	case MC_ASOC_CKSEL:
		switch (div) {
		case 0:
			setup->init.bCkSel	= MCDRV_CKSEL_CMOS;
			break;
		case 1:
			setup->init.bCkSel	= MCDRV_CKSEL_TCXO;
			break;
		case 2:
			setup->init.bCkSel	= MCDRV_CKSEL_CMOS_TCXO;
			break;
		case 3:
			setup->init.bCkSel	= MCDRV_CKSEL_TCXO_CMOS;
			break;
		default:
			return -EINVAL;
		}
		break;
	case MC_ASOC_DIVR0:
		if ((div < 1) || (div > 127)) {
			return -EINVAL;
		}
		setup->init.bDivR0	= div;
		break;
	case MC_ASOC_DIVF0:
		if ((div < 1) || (div > 255)) {
			return -EINVAL;
		}
		setup->init.bDivF0	= div;
		break;
	case MC_ASOC_DIVR1:
		if ((div < 1) || (div > 127)) {
			return -EINVAL;
		}
		setup->init.bDivR1	= div;
		break;
	case MC_ASOC_DIVF1:
		if ((div < 1) || (div > 255)) {
			return -EINVAL;
		}
		setup->init.bDivF1	= div;
		break;
	default:
		return -EINVAL;
	}

/*	mc_asoc->clk_update	= 1;*/

	return 0;
}

static int set_fmt_common(
	struct mc_asoc_port_params *port,
	unsigned int fmt)
{
	/* master */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		port->master	= MCDRV_DIO_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		port->master	= MCDRV_DIO_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	/* inv */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		port->inv	= MCDRV_BCLK_NORMAL;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		port->inv	= MCDRV_BCLK_INVERT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mc_asoc_i2s_set_clkdiv(
	struct snd_soc_dai *dai,
	int div_id,
	int div)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port		= &mc_asoc->port[get_port_id(dai->id)];

	switch (div_id) {
	case MC_ASOC_BCLK_MULT:
		switch (div) {
		case MC_ASOC_LRCK_X32:
			port->bckfs	= MCDRV_BCKFS_32;
			break;
		case MC_ASOC_LRCK_X48:
			port->bckfs	= MCDRV_BCKFS_48;
			break;
		case MC_ASOC_LRCK_X64:
			port->bckfs	= MCDRV_BCKFS_64;
			break;
		default:
			return -EINVAL;
		}
		break;

	default:
		return set_clkdiv_common(mc_asoc, div_id, div);
	}

	return 0;
}

static int mc_asoc_i2s_set_fmt(
	struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		port->format	= MCDRV_DAMODE_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		port->format	= MCDRV_DAMODE_TAILALIGN;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		port->format	= MCDRV_DAMODE_HEADALIGN;
		break;
	default:
		return -EINVAL;
	}

	return set_fmt_common(port, fmt);
}

static int mc_asoc_i2s_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_pcm_runtime	*runtime	= snd_pcm_substream_chip(substream);
#endif
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;
	int		dir	= substream->stream;
	int		rate;
	int		err	= 0;

	TRACE_FUNC();

	if((substream == NULL)
	|| (dai == NULL)
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	|| (runtime == NULL)
#endif
	) {
		return -EINVAL;
	}

	if((get_port_id(dai->id) < 0) || (get_port_id(dai->id) >= IOPORT_NUM)) {
		dbg_info("dai->id=%d\n", get_port_id(dai->id));
		return -EINVAL;
	}

	dbg_info("hw_params: [%d] name=%s, dir=%d, rate=%d, bits=%d, ch=%d\n",
		 get_port_id(dai->id),
		 substream->name,
		 dir,
		 params_rate(params),
		 params_format(params),
		 params_channels(params));

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec	= runtime->socdev->card->codec;
#else
	codec	= dai->codec;
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if((codec == NULL)
	|| (mc_asoc == NULL)) {
		dbg_info("mc_asoc=NULL\n");
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* format (bits) */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		port->bits[dir]	= MCDRV_BITSEL_16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		port->bits[dir]	= MCDRV_BITSEL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		port->bits[dir]	= MCDRV_BITSEL_24;
		break;
	default:
		return -EINVAL;
	}

	/* rate */
	switch (params_rate(params)) {
	case 8000:
		rate	= MCDRV_FS_8000;
		break;
	case 11025:
		rate	= MCDRV_FS_11025;
		break;
	case 16000:
		rate	= MCDRV_FS_16000;
		break;
	case 22050:
		rate	= MCDRV_FS_22050;
		break;
	case 32000:
		rate	= MCDRV_FS_32000;
		break;
	case 44100:
		rate	= MCDRV_FS_44100;
		break;
	case 48000:
		rate	= MCDRV_FS_48000;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&mc_asoc->mutex);

	if ((port->stream & ~(1 << dir)) && (rate != port->rate)) {
		err	= -EBUSY;
		goto error;
	}

	port->rate	= rate;
	port->channels	= params_channels(params);

	err	= update_clock(mc_asoc);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in update_clock\n", err);
		err	= -EIO;
		goto error;
	}

	err	= setup_dai(codec, mc_asoc, get_port_id(dai->id), MCDRV_DIO_DA, dir);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in setup_dai\n", err);
		err	= -EIO;
		goto error;
	}

	port->stream |= (1 << dir);

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static int mc_asoc_pcm_set_clkdiv(
	struct snd_soc_dai *dai,
	int div_id,
	int div)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	switch (div_id) {
	case MC_ASOC_BCLK_MULT:
		switch (div) {
		case MC_ASOC_LRCK_X8:
			port->bckfs	= MCDRV_BCKFS_16;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_HALF;
			break;
		case MC_ASOC_LRCK_X16:
			port->bckfs	= MCDRV_BCKFS_16;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X24:
			port->bckfs	= MCDRV_BCKFS_48;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_HALF;
			break;
		case MC_ASOC_LRCK_X32:
			port->bckfs	= MCDRV_BCKFS_32;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X48:
			port->bckfs	= MCDRV_BCKFS_48;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X64:
			port->bckfs	= MCDRV_BCKFS_64;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X128:
			port->bckfs	= MCDRV_BCKFS_128;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X256:
			port->bckfs	= MCDRV_BCKFS_256;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X512:
			port->bckfs	= MCDRV_BCKFS_512;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		default:
			return -EINVAL;
		}
		break;

	default:
		return set_clkdiv_common(mc_asoc, div_id, div);
	}

	return 0;
}

static int mc_asoc_pcm_set_fmt(
	struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		port->format	= MCDRV_PCM_SHORTFRAME;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		port->format	= MCDRV_PCM_LONGFRAME;
		break;
	default:
		return -EINVAL;
	}

	return set_fmt_common(port, fmt);
}

static int mc_asoc_pcm_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_pcm_runtime	*runtime	= snd_pcm_substream_chip(substream);
#endif
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;
	int		dir	= substream->stream;
	int		rate;
	int		err	= 0;

	TRACE_FUNC();

	if((substream == NULL)
	|| (dai == NULL)
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	|| (runtime == NULL)
#endif
	) {
		return -EINVAL;
	}

	if((get_port_id(dai->id) < 0) || (get_port_id(dai->id) >= IOPORT_NUM)) {
		dbg_info("dai->id=%d\n", get_port_id(dai->id));
		return -EINVAL;
	}

	dbg_info("hw_params: [%d] name=%s, dir=%d, rate=%d, bits=%d, ch=%d\n",
		 get_port_id(dai->id),
		 substream->name,
		 dir,
		 params_rate(params),
		 params_format(params),
		 params_channels(params));

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec	= runtime->socdev->card->codec;
#else
	codec	= dai->codec;
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if((codec == NULL)
	|| (mc_asoc == NULL)) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* channels */
	switch (params_channels(params)) {
	case 1:
		port->pcm_mono[dir]	= MCDRV_PCM_MONO;
		break;
	case 2:
		port->pcm_mono[dir]	= MCDRV_PCM_STEREO;
		break;
	default:
		return -EINVAL;
	}

	/* format (bits) */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		port->bits[dir]	= MCDRV_PCM_BITSEL_8;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_LINEAR;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		port->bits[dir]	= MCDRV_PCM_BITSEL_16;
		port->pcm_order[dir]	= MCDRV_PCM_LSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_LINEAR;
		break;
	case SNDRV_PCM_FORMAT_S16_BE:
		port->bits[dir]	= MCDRV_PCM_BITSEL_16;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_LINEAR;
		break;
	case SNDRV_PCM_FORMAT_A_LAW:
		port->bits[dir]	= MCDRV_PCM_BITSEL_8;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_ALAW;
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		port->bits[dir]	= MCDRV_PCM_BITSEL_8;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_MULAW;
		break;
	default:
		return -EINVAL;
	}

	/* rate */
	switch (params_rate(params)) {
	case 8000:
		rate	= MCDRV_FS_8000;
		break;
	case 16000:
		rate	= MCDRV_FS_16000;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&mc_asoc->mutex);

	if ((port->stream & ~(1 << dir)) && (rate != port->rate)) {
		err	= -EBUSY;
		goto error;
	}

	port->rate	= rate;
	port->channels	= params_channels(params);

	err	= update_clock(mc_asoc);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in update_clock\n", err);
		err	= -EIO;
		goto error;
	}

	err	= setup_dai(codec, mc_asoc, get_port_id(dai->id), MCDRV_DIO_PCM, dir);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in setup_dai\n", err);
		err	= -EIO;
		goto error;
	}

	port->stream |= (1 << dir);

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static int mc_asoc_hw_free(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_pcm_runtime	*runtime	= snd_pcm_substream_chip(substream);
#endif
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;
	int		dir	= substream->stream;
	int		err	= 0;

	TRACE_FUNC();

	if((substream == NULL)
	|| (dai == NULL)
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	|| (runtime == NULL)
#endif
	)
	{
		return -EINVAL;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec	= runtime->socdev->card->codec;
#else
	codec	= dai->codec;
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if((codec == NULL)
	|| (mc_asoc == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	mutex_lock(&mc_asoc->mutex);

	if (!(port->stream & (1 << dir))) {
		err	= 0;
		goto error;
	}

	port->stream &= ~(1 << dir);

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static struct snd_soc_dai_ops	mc_asoc_dai_ops[]	= {
	{
		.set_clkdiv	= mc_asoc_i2s_set_clkdiv,
		.set_fmt	= mc_asoc_i2s_set_fmt,
		.hw_params	= mc_asoc_i2s_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_pcm_set_clkdiv,
		.set_fmt	= mc_asoc_pcm_set_fmt,
		.hw_params	= mc_asoc_pcm_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_i2s_set_clkdiv,
		.set_fmt	= mc_asoc_i2s_set_fmt,
		.hw_params	= mc_asoc_i2s_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_pcm_set_clkdiv,
		.set_fmt	= mc_asoc_pcm_set_fmt,
		.hw_params	= mc_asoc_pcm_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_i2s_set_clkdiv,
		.set_fmt	= mc_asoc_i2s_set_fmt,
		.hw_params	= mc_asoc_i2s_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_pcm_set_clkdiv,
		.set_fmt	= mc_asoc_pcm_set_fmt,
		.hw_params	= mc_asoc_pcm_hw_params,
		.hw_free	= mc_asoc_hw_free,
	}
};

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
struct snd_soc_dai			mc_asoc_dai[]	= 
#else
struct snd_soc_dai_driver	mc_asoc_dai[]	= 
#endif
{
	{
		.name	= MC_ASOC_NAME "-da0i",
		.id	= 1,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[0]
	},
	{
		.name	= MC_ASOC_NAME "-da0p",
		.id	= 1,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[1]
	},
	{
		.name	= MC_ASOC_NAME "-da1i",
		.id	= 2,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[2]
	},
	{
		.name	= MC_ASOC_NAME "-da1p",
		.id	= 2,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[3]
	},
	{
		.name	= MC_ASOC_NAME "-da2i",
		.id	= 3,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[4]
	},
	{
		.name	= MC_ASOC_NAME "-da2p",
		.id	= 3,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[5]
	},
};
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
EXPORT_SYMBOL_GPL(mc_asoc_dai);
#endif

/*
 * Control interface
 */
/*
 * Virtual register
 *
 * 16bit software registers are implemented for volumes and mute
 * switches (as an exception, no mute switches for MIC and HP gain).
 * Register contents are stored in codec's register cache.
 *
 *	15	14							8	7	6						0
 *	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *	|swR|			volume-R		|swL|			volume-L		|
 *	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 */

static unsigned int mc_asoc_read_reg(
	struct snd_soc_codec *codec,
	unsigned int reg)
{
	int	ret;

	if(codec == NULL) {
		return -EINVAL;
	}
	ret	= read_cache(codec, reg);
	if (ret < 0) {
		return -EIO;
	}
	return (unsigned int)ret;
}

static int write_reg_vol(
	struct snd_soc_codec *codec,
	  unsigned int reg,
	  unsigned int value)
{
	MCDRV_VOL_INFO	update;
	SINT16	*vp;
	int		err, i;
	int		cache;

	if(mc_asoc_vreg_map[reg].offset != (size_t)-1) {
		memset(&update, 0, sizeof(MCDRV_VOL_INFO));
		vp		= (SINT16 *)((void *)&update + mc_asoc_vreg_map[reg].offset);
		cache	= read_cache(codec, reg);
		if(cache < 0) {
			return -EIO;
		}

		for (i = 0; i < 2; i++, vp++) {
			unsigned int	v	= (value >> (i*8)) & 0xff;
			unsigned int	c	= (cache >> (i*8)) & 0xff;

			if (v != c) {
				int		sw, vol;
				SINT16	db;
				sw	= (reg < MC_ASOC_AVOL_MIC1_GAIN) ? (v & 0x80) : 1;
				vol	= sw ? (v & 0x7f) : 0;
				if(get_vol_mute_flg(mc_asoc_vreg_map[reg].offset, i) == 0) {
					db	= mc_asoc_vreg_map[reg].volmap[vol];
					*vp	= db | MCDRV_VOL_UPDATE;
				}
			}
		}

		err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, &update, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in MCDRV_SET_VOLUME\n", err);
			return -EIO;
		}
	}

	err	= write_cache(codec, reg, value);
	if (err != 0) {
		dev_err(codec->dev, "Cache write to %x failed: %d\n", reg, err);
	}

	return 0;
}


static void set_ae_dir(
	struct snd_soc_codec *codec,
	struct mc_asoc_data	*mc_asoc,
	int	audio_mode_play,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic,
	int	dir
)
{
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	int		ain_play;
	int		base, user;

	TRACE_FUNC();

	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return;
	}
	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	else {
		ain_play	= 0;
	}

	if(dir == MC_ASOC_AE_PLAY) {
		if(mc_asoc_ae_src_play == AE_SRC_PLAY_NONE) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			return;
		}
		if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
		&& (ain_play == 0)
		&& (btmic_play == 0)) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			return;
		}
		if(ae_onoff(mc_asoc->ae_onoff, output_path) == MC_ASOC_AENG_OFF) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			return;
		}
		if((mc_asoc_ae_dir == MC_ASOC_AE_CAP)
		&& (mc_asoc_ae_priority != AE_PRIORITY_PLAY)
		&& (mc_asoc_ae_priority != AE_PRIORITY_LAST)) {
			return;
		}

		switch(mc_asoc_ae_src_play) {
		case	AE_SRC_PLAY_DIR0:
			if(audio_mode_play == MC_ASOC_AUDIO_MODE_OFF) {
				if(btmic_play != 0) {
					if(get_bt_port() == DIO_0) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			else if(audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO) {
				if(get_ap_play_port() == DIO_0) {
					mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
				}
				else if(btmic_play != 0) {
					if(get_bt_port() == DIO_0) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			break;
		case	AE_SRC_PLAY_DIR1:
			if(audio_mode_play == MC_ASOC_AUDIO_MODE_OFF) {
				if(btmic_play != 0) {
					if(get_bt_port() == DIO_1) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			else if(audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO) {
				if(get_ap_play_port() == DIO_1) {
					mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
				}
				else if(btmic_play != 0) {
					if(get_bt_port() == DIO_1) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			break;
		case	AE_SRC_PLAY_DIR2:
			if(audio_mode_play == MC_ASOC_AUDIO_MODE_OFF) {
				if(btmic_play != 0) {
					if(get_bt_port() == DIO_2) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			else if(audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO) {
				if(get_ap_play_port() == DIO_2) {
					mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					break;
				}
				else if(btmic_play != 0) {
					if(get_bt_port() == DIO_2) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			break;
		case	AE_SRC_PLAY_MIX:
			mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
			break;
		case	AE_SRC_PLAY_PDM:
			if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
			|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
				if(mainmic_play != 0) {
					if(mc_asoc_main_mic == MIC_PDM) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
						break;
					}
				}
				if(submic_play != 0) {
					if(mc_asoc_sub_mic == MIC_PDM) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			break;
		case	AE_SRC_PLAY_ADC0:
			if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
			|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
				if((mainmic_play != 0)
				|| (msmic_play != 0)) {
					if(get_main_mic_block_on() != -1) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
						break;
					}
				}
				if(submic_play != 0) {
					if(get_sub_mic_block_on() != -1) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
						break;
					}
				}
				if((hsmic_play != 0)
				|| (lin1_play != 0)
				|| (lin2_play != 0)) {
					mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					break;
				}
			}
			break;
		case	AE_SRC_PLAY_DTMF:
			if(read_cache(codec, MC_ASOC_DTMF_CONTROL) == 1) {
				mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
			}
			break;
		case	AE_SRC_PLAY_CDSP:
			if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
			|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
			|| (audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
					get_voicecall_index(output_path, incall_mic, &base, &user);
					if((mc_asoc_hf == HF_ON) && (mc_asoc->dsp_voicecall.onoff[base] == 1)) {
						mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
					}
				}
			}
			break;
		default:
			break;
		}
	}
	else {
		mc_asoc_ae_src_cap = mc_asoc_ae_src_cap_def;
		if(mc_asoc_ae_src_cap == AE_SRC_CAP_NONE) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			return;
		}
		if(audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			return;
		}
		if((mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_EXT] == MC_ASOC_AENG_OFF)
		|| (mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_BASE] == MC_ASOC_AENG_OFF)) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			return;
		}
		if((mc_asoc_ae_dir == MC_ASOC_AE_PLAY)
		&& (mc_asoc_ae_priority != AE_PRIORITY_CAP)
		&& (mc_asoc_ae_priority != AE_PRIORITY_LAST)) {
			return;
		}

		switch(mc_asoc_ae_src_cap) {
		case	AE_SRC_CAP_PDM:
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
					if(input_path == MC_ASOC_INPUT_PATH_MAINMIC) {
						if(mc_asoc_main_mic == MIC_PDM) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
					if(input_path == MC_ASOC_INPUT_PATH_SUBMIC) {
						if(mc_asoc_sub_mic == MIC_PDM) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						}
					}
				}
			}
			else if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if(output_path != MC_ASOC_OUTPUT_PATH_HS) {
					if(input_path == MC_ASOC_INPUT_PATH_MAINMIC) {
						if(mc_asoc_main_mic == MIC_PDM) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
					else if(input_path == MC_ASOC_INPUT_PATH_SUBMIC) {
						if(mc_asoc_sub_mic == MIC_PDM) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						}
					}
					else if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
						 || (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)) {
						if((output_path != MC_ASOC_OUTPUT_PATH_BT)
						&& (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
							if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
								if(mc_asoc_main_mic == MIC_PDM) {
									mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
									break;
								}
							} else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
								if(mc_asoc_sub_mic == MIC_PDM) {
									mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
									break;
								}
							}
						}
					}
				}
			}
			break;
		case	AE_SRC_CAP_ADC0:
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
					if((input_path == MC_ASOC_INPUT_PATH_MAINMIC)
					|| (input_path == MC_ASOC_INPUT_PATH_2MIC)) {
						if(get_main_mic_block_on() != -1) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
					if(input_path == MC_ASOC_INPUT_PATH_SUBMIC) {
						if(get_sub_mic_block_on() != -1) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
					if((input_path == MC_ASOC_INPUT_PATH_LIN1)
					|| (input_path == MC_ASOC_INPUT_PATH_LIN2)) {
						mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						break;
					}
				}
			}
			else if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
					mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
					break;
				}
				else {
					if(input_path == MC_ASOC_INPUT_PATH_MAINMIC) {
						if(get_main_mic_block_on() != -1) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
					else if(input_path == MC_ASOC_INPUT_PATH_SUBMIC) {
						if(get_sub_mic_block_on() != -1) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
					else if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
						 || (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)) {
						if((output_path != MC_ASOC_OUTPUT_PATH_BT)
						&& (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
							if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
								if(get_main_mic_block_on() != -1) {
									mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
									break;
								}
							} else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
								if(get_sub_mic_block_on() != -1) {
									mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
									break;
								}
							}
						}
					}
				}
				if(get_cp_port() == -1) {
					if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
					|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
						if((output_path != MC_ASOC_OUTPUT_PATH_BT)
						&& (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
						else if(output_path != MC_ASOC_OUTPUT_PATH_HS) {
							if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
								if(mc_asoc_main_mic == MIC_PDM) {
									mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
									break;
								}
							}
							else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
								if(mc_asoc_sub_mic == MIC_PDM) {
									mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
									break;
								}
							}
						}
					}
				}
			}
			break;
		case	AE_SRC_CAP_ADC1:
			if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if(get_cp_port() == -1) {
					if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
					|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
						if((output_path == MC_ASOC_OUTPUT_PATH_BT)
						|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
							break;
						}
						else if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
						if(incall_mic == MC_ASOC_INCALL_MIC_2MIC) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
						else if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
							if(get_main_mic_block_on() != -1) {
								mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
								break;
							}
						}
						else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
							if(get_sub_mic_block_on() != -1) {
								mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
								break;
							}
						}
					}
				}
			}
			break;
		case	AE_SRC_CAP_DIR0:
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
					if(input_path == MC_ASOC_INPUT_PATH_BT) {
						if(get_bt_port() == DIO_0) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
				}
			}
			else if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT)
				|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK) {
						mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						break;
					}
				}
			}
			break;
		case	AE_SRC_CAP_DIR1:
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
					if(input_path == MC_ASOC_INPUT_PATH_BT) {
						if(get_bt_port() == DIO_1) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
				}
			}
			else if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT)
				|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK) {
						mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						break;
					}
				}
			}
			break;
		case	AE_SRC_CAP_DIR2:
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
					if(input_path == MC_ASOC_INPUT_PATH_BT) {
						if(get_bt_port() == DIO_2) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
							break;
						}
					}
				}
			}
			else if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				if((output_path == MC_ASOC_OUTPUT_PATH_BT)
				|| (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK) {
						mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						break;
					}
				}
			}
			break;
		case	AE_SRC_CAP_INPUT_PATH:
dbg_info("audio_mode_cap=%d, input_path=%d, output_path=%d, incall_mic=%d\n", audio_mode_cap, input_path, output_path, incall_mic);
			if(get_ap_cap_port() == -1) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
					if((output_path != MC_ASOC_OUTPUT_PATH_BT) && (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
						if(input_path == MC_ASOC_INPUT_PATH_VOICECALL) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						}
						else if(input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK) {
						}
						else if(input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK) {
							mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
						}
					}
					else {
						mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
					}
					break;
				}
				else if(audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM) {
					if(audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM) {
						break;
					}
				}
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
				|| ((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC))
				|| ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC))
				|| ((msmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_2MIC))
				|| ((hsmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_HS))
				|| ((lin1_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN1))
				|| ((lin2_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN2))) {
					if(output_path != MC_ASOC_OUTPUT_PATH_BT) {
						break;
					}
				}
				break;
			}
			if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
			|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
				if((audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
				|| (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)) {
					mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
					break;
				}
			}
			if(audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
				mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
				if((input_path == MC_ASOC_INPUT_PATH_VOICECALL)
				|| (input_path == MC_ASOC_INPUT_PATH_VOICEUPLINK)
				|| (input_path == MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
				//	break;
				}
				if((output_path == MC_ASOC_OUTPUT_PATH_BT) || (output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
					if(get_bt_port() == DIO_0) {
						mc_asoc_ae_src_cap	= AE_SRC_CAP_DIR0;
					}
					else if(get_bt_port() == DIO_1) {
						mc_asoc_ae_src_cap	= AE_SRC_CAP_DIR1;
					}
					else if(get_bt_port() == DIO_2) {
						mc_asoc_ae_src_cap	= AE_SRC_CAP_DIR2;
					}
				}
				else if(output_path == MC_ASOC_OUTPUT_PATH_HS) {
					mc_asoc_ae_src_cap	= AE_SRC_CAP_ADC0;
				}
				else {
					if(incall_mic == MC_ASOC_INCALL_MIC_2MIC) {
						mc_asoc_ae_src_cap	= AE_SRC_CAP_ADC0;
					}
					else if(incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) {
						if(mc_asoc_main_mic == MIC_PDM) {
							mc_asoc_ae_src_cap	= AE_SRC_CAP_PDM;
						}
						else {
							mc_asoc_ae_src_cap	= AE_SRC_CAP_ADC0;
						}
					}
					else if(incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) {
						if(mc_asoc_sub_mic == MIC_PDM) {
							mc_asoc_ae_src_cap	= AE_SRC_CAP_PDM;
						}
						else {
							mc_asoc_ae_src_cap	= AE_SRC_CAP_ADC0;
						}
					}
				}
				break;
			}
			break;
		default:
			break;
		}
	}
}

static int set_audio_mode_play(
	struct snd_soc_codec *codec,
	unsigned int value
)
{
	int		ret;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		ain_play;

	TRACE_FUNC();
	dbg_info("audio_mode=%d\n", value);

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}
	if((ain_play = get_ain_play(codec)) < 0) {
		return -EIO;
	}

	if((ret = write_cache(codec, MC_ASOC_AUDIO_MODE_PLAY, value)) < 0) {
		return ret;
	}

	if(value == MC_ASOC_AUDIO_MODE_OFF) {
		if((ain_play == 0)
		|| (ae_onoff(mc_asoc->ae_onoff, output_path) == MC_ASOC_AENG_OFF)) {
			if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
		}
		if(ain_play == 0) {
			if(mc_asoc_cdsp_dir == MC_ASOC_CDSP_PLAY) {
				if(audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF) {
					mc_asoc_cdsp_dir	= MC_ASOC_CDSP_NONE;
				}
				else {
					mc_asoc_cdsp_dir	= MC_ASOC_CDSP_CAP;
				}
			}
		}
	}
	else {
		set_ae_dir(codec, mc_asoc, value, audio_mode_cap, output_path, input_path, incall_mic, MC_ASOC_AE_PLAY);
		if((mc_asoc_cdsp_dir == MC_ASOC_CDSP_NONE)
		|| (mc_asoc_cdsp_priority == CDSP_PRIORITY_PLAY)
		|| (mc_asoc_cdsp_priority == CDSP_PRIORITY_LAST)) {
			mc_asoc_cdsp_dir	= MC_ASOC_CDSP_PLAY;
		}
	}
	set_ae_dir(codec, mc_asoc, value, audio_mode_cap, output_path, input_path, incall_mic, MC_ASOC_AE_CAP);
dbg_info("mc_asoc_ae_dir=%d, mc_asoc_cdsp_dir=%d, mc_asoc_ae_src_cap=%d\n", mc_asoc_ae_dir, mc_asoc_cdsp_dir, mc_asoc_ae_src_cap);

	if((value == MC_ASOC_AUDIO_MODE_INCALL)
	|| (value == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (value == MC_ASOC_AUDIO_MODE_INCOMM)) {
		/*	xxx->incall	*/
		if(get_cp_port() == -1) {
			/*	MASTER_OUTL	*/
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DAC_MASTER].offset, 0, 1);
			/*	VOICE_ATTR	*/
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DAC_VOICE].offset, 1, 1);
		}
	}
	else {
		/*	MASTER_OUTL	*/
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DAC_MASTER].offset, 0, 0);
		/*	VOICE_ATTR	*/
		set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DAC_VOICE].offset, 1, 0);
	}

	if((ret = set_vreg_map(codec)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	return connect_path(codec);
}

static int set_audio_mode_cap(
	struct snd_soc_codec *codec,
	unsigned int value
)
{
	int		ret;
	int		audio_mode;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		ain_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();
	dbg_info("audio_mode=%d\n", value);

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}
	if((ain_play = get_ain_play(codec)) < 0) {
		return -EIO;
	}

	if((ret = write_cache(codec, MC_ASOC_AUDIO_MODE_CAP, value)) < 0) {
		return ret;
	}

	if(value == MC_ASOC_AUDIO_MODE_OFF) {
		if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
			mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
		}
		if(mc_asoc_cdsp_dir == MC_ASOC_CDSP_CAP) {
			if((audio_mode == MC_ASOC_AUDIO_MODE_OFF)
			&& (ain_play == 0)) {
				mc_asoc_cdsp_dir	= MC_ASOC_CDSP_NONE;
			}
			else {
				mc_asoc_cdsp_dir	= MC_ASOC_CDSP_PLAY;
			}
		}
	}
	else {
		set_ae_dir(codec, mc_asoc, audio_mode, value, output_path, input_path, incall_mic, MC_ASOC_AE_CAP);
		if((mc_asoc_cdsp_dir == MC_ASOC_CDSP_NONE)
		|| (mc_asoc_cdsp_priority == CDSP_PRIORITY_CAP)
		|| (mc_asoc_cdsp_priority == CDSP_PRIORITY_LAST)) {
			mc_asoc_cdsp_dir	= MC_ASOC_CDSP_CAP;
		}
	}
	set_ae_dir(codec, mc_asoc, audio_mode, value, output_path, input_path, incall_mic, MC_ASOC_AE_PLAY);
dbg_info("mc_asoc_ae_dir=%d, mc_asoc_cdsp_dir=%d, mc_asoc_ae_src_cap=%d\n", mc_asoc_ae_dir, mc_asoc_cdsp_dir, mc_asoc_ae_src_cap);

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}
	return connect_path(codec);
}

static int set_incall_mic(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;

	TRACE_FUNC();

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}
	return connect_path(codec);
}

static int set_ain_playback(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;
	int		audio_mode;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		ain_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	dbg_info("ain_playback=%d\n", value);

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((incall_mic = get_incall_mic(codec, output_path)) < 0) {
		return -EIO;
	}

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
		return 0;
	}

	if((ret = set_vreg_map(codec)) < 0) {
		return ret;
	}

	if(value == 1) {
		/*	start	*/
		set_ae_dir(codec, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, MC_ASOC_AE_PLAY);
		if((mc_asoc_cdsp_dir == MC_ASOC_CDSP_NONE)
		|| (mc_asoc_cdsp_priority == CDSP_PRIORITY_PLAY)
		|| (mc_asoc_cdsp_priority == CDSP_PRIORITY_LAST)) {
			mc_asoc_cdsp_dir	= MC_ASOC_CDSP_PLAY;
		}
	}
	else {
		/*	stop	*/
		if((ain_play = get_ain_play(codec)) < 0) {
			return -EIO;
		}
		if(ain_play == 0) {
			mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			if(mc_asoc_cdsp_dir == MC_ASOC_CDSP_PLAY) {
				if(audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF) {
					mc_asoc_cdsp_dir	= MC_ASOC_CDSP_NONE;
				}
				else {
					mc_asoc_cdsp_dir	= MC_ASOC_CDSP_CAP;
				}
			}
		}
	}
	set_ae_dir(codec, mc_asoc, audio_mode, audio_mode_cap, output_path, input_path, incall_mic, MC_ASOC_AE_CAP);
dbg_info("mc_asoc_ae_dir=%d, mc_asoc_cdsp_dir=%d, mc_asoc_ae_src_cap=%d\n", mc_asoc_ae_dir, mc_asoc_cdsp_dir, mc_asoc_ae_src_cap);

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	return connect_path(codec);
}

static int set_dtmf_control(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if(value == 1) {
		/*	On	*/
		mc_asoc->dtmf_store.bOnOff	= MCDRV_DTMF_ON;
		if((ret = _McDrv_Ctrl(MCDRV_SET_DTMF, (void *)&mc_asoc->dtmf_store, mc_asoc_info_store_tbl[12].flags)) != MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(ret);
		}
	}

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	ret	= connect_path(codec);
	if(value == 1) {
		/*	On	*/
	}
	else {
		if(ret == 0) {
			mc_asoc->dtmf_store.bOnOff	= MCDRV_DTMF_OFF;
			ret	= mc_asoc_map_drv_error(_McDrv_Ctrl(MCDRV_SET_DTMF, (void *)&mc_asoc->dtmf_store, MCDRV_DTMFONOFF_UPDATE_FLAG));
		}
	}

	return ret;
}

static int set_dtmf_output(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;

	TRACE_FUNC();

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	return connect_path(codec);
}

static int mc_asoc_write_reg(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value)
{
	int		err	= 0;

	if(codec == NULL) {
		return -EINVAL;
	}
	if (reg < MC_ASOC_N_VOL_REG) {
		err	= write_reg_vol(codec, reg, value);
	}
	else {
		switch(reg) {
		case	MC_ASOC_AUDIO_MODE_PLAY:
			err	= set_audio_mode_play(codec, value);
			break;
		case	MC_ASOC_AUDIO_MODE_CAP:
			err	= set_audio_mode_cap(codec, value);
			break;
		case	MC_ASOC_OUTPUT_PATH:
		case	MC_ASOC_INPUT_PATH:
			err	= write_cache(codec, reg, value);
			break;
		case	MC_ASOC_INCALL_MIC_SP:
		case	MC_ASOC_INCALL_MIC_RC:
		case	MC_ASOC_INCALL_MIC_HP:
		case	MC_ASOC_INCALL_MIC_LO1:
		case	MC_ASOC_INCALL_MIC_LO2:
			err	= set_incall_mic(codec, reg, value);
			break;
		case	MC_ASOC_MAINMIC_PLAYBACK_PATH:
		case	MC_ASOC_SUBMIC_PLAYBACK_PATH:
		case	MC_ASOC_2MIC_PLAYBACK_PATH:
		case	MC_ASOC_HSMIC_PLAYBACK_PATH:
		case	MC_ASOC_BTMIC_PLAYBACK_PATH:
		case	MC_ASOC_LIN1_PLAYBACK_PATH:
		case	MC_ASOC_LIN2_PLAYBACK_PATH:
			err	= set_ain_playback(codec, reg, value);
			break;
		case	MC_ASOC_PARAMETER_SETTING:
			break;
		case	MC_ASOC_DTMF_CONTROL:
			err	= set_dtmf_control(codec, reg, value);
			break;
		case	MC_ASOC_DTMF_OUTPUT:
			err	= set_dtmf_output(codec, reg, value);
			break;

#ifdef MC_ASOC_TEST
		case	MC_ASOC_MAIN_MIC:
			mc_asoc_main_mic	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_SUB_MIC:
			mc_asoc_sub_mic	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_HS_MIC:
			mc_asoc_hs_mic	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_AP_PLAY_PORT:
			mc_asoc_ap_play_port	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_AP_CAP_PORT:
			mc_asoc_ap_cap_port	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_CP_PORT:
			if(mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
				if(value == LIN1_LOUT1 || value == LIN1_LOUT2) {
					return -EINVAL;
				}
			}
			mc_asoc_cp_port	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_BT_PORT:
			mc_asoc_bt_port	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_HF:
			if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
				mc_asoc_hf	= value;
				write_cache(codec, reg, value);
			}
			break;
		case	MC_ASOC_MIC1_BIAS:
			mc_asoc_mic1_bias	= value;
			write_cache(codec, reg, value);
			err	= connect_path(codec);
			break;
		case	MC_ASOC_MIC2_BIAS:
			mc_asoc_mic2_bias	= value;
			write_cache(codec, reg, value);
			err	= connect_path(codec);
			break;
		case	MC_ASOC_MIC3_BIAS:
			mc_asoc_mic3_bias	= value;
			write_cache(codec, reg, value);
			err	= connect_path(codec);
			break;
		case	MC_ASOC_SP:
			mc_asoc_sp	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_AE_SRC_PLAY:
			mc_asoc_ae_src_play	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_AE_SRC_CAP:
			mc_asoc_ae_src_cap_def	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_AE_PRIORITY:
			mc_asoc_ae_priority	= value;
			write_cache(codec, reg, value);
			if(mc_asoc_ae_priority == AE_PRIORITY_PLAY) {
				mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
			}
			else if(mc_asoc_ae_priority == AE_PRIORITY_CAP) {
				mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
			}
			else {
				mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
			}
			break;
		case	MC_ASOC_CDSP_PRIORITY:
			mc_asoc_cdsp_priority	= value;
			write_cache(codec, reg, value);
			if(mc_asoc_cdsp_priority == CDSP_PRIORITY_PLAY) {
				mc_asoc_cdsp_dir	= MC_ASOC_CDSP_PLAY;
			}
			else if(mc_asoc_cdsp_priority == CDSP_PRIORITY_CAP) {
				mc_asoc_cdsp_dir	= MC_ASOC_CDSP_CAP;
			}
			else {
				mc_asoc_cdsp_dir	= MC_ASOC_CDSP_NONE;
			}
			break;
		case	MC_ASOC_SIDETONE:
			mc_asoc_sidetone	= value;
			write_cache(codec, reg, value);
			break;
#endif

		default:
			err	= -EINVAL;
			break;
		}
	}

	if(err < 0) {
		dbg_info("err=%d\n", err);
	}
	return err;
}

static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_digital, -7500, 100, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_adc, -2850, 150, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_ain, -3150, 150, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_aout, -3100, 100, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_micgain, 1500, 500, 0);

static unsigned int	mc_asoc_tlv_hpsp[]	= {
	TLV_DB_RANGE_HEAD(5),
	0, 2, TLV_DB_SCALE_ITEM(-4400, 800, 1),
	2, 3, TLV_DB_SCALE_ITEM(-2800, 400, 0),
	3, 7, TLV_DB_SCALE_ITEM(-2400, 200, 0),
	7, 15, TLV_DB_SCALE_ITEM(-1600, 100, 0),
	15, 31, TLV_DB_SCALE_ITEM(-800, 50, 0),
};

static unsigned int	mc_asoc_tlv_hpgain[]	= {
	TLV_DB_RANGE_HEAD(2),
	0, 2, TLV_DB_SCALE_ITEM(0, 150, 0),
	2, 3, TLV_DB_SCALE_ITEM(300, 300, 0),
};

static const struct snd_kcontrol_new	mc_asoc_snd_controls[]	= {
	/*
	 * digital volumes and mute switches
	 */
	SOC_DOUBLE_TLV("AD#0 Digital Volume",
				MC_ASOC_DVOL_AD0, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#0 Digital Switch",
				MC_ASOC_DVOL_AD0, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AD#1 Digital Volume",
				MC_ASOC_DVOL_AD1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#1 Digital Switch",
				MC_ASOC_DVOL_AD1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AENG6 Volume",
				MC_ASOC_DVOL_AENG6, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AENG6 Switch",
				MC_ASOC_DVOL_AENG6, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("PDM Volume",
				MC_ASOC_DVOL_PDM, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("PDM Switch",
				MC_ASOC_DVOL_PDM, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DTMF_B Volume",
				MC_ASOC_DVOL_DTMF_B, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DTMF_B Switch",
				MC_ASOC_DVOL_DTMF_B, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#0 Volume",
				MC_ASOC_DVOL_DIR0, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#0 Switch",
				MC_ASOC_DVOL_DIR0, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#1 Volume",
				MC_ASOC_DVOL_DIR1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#1 Switch",
				MC_ASOC_DVOL_DIR1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#2 Volume",
				MC_ASOC_DVOL_DIR2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#2 Switch",
				MC_ASOC_DVOL_DIR2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AD#0 ATT Volume",
				MC_ASOC_DVOL_AD0_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#0 ATT Switch",
				MC_ASOC_DVOL_AD0_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AD#1 ATT Volume",
				MC_ASOC_DVOL_AD1_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#1 ATT Switch",
				MC_ASOC_DVOL_AD1_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DTMF Volume",
				MC_ASOC_DVOL_DTMF, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DTMF Switch",
				MC_ASOC_DVOL_DTMF, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#0 ATT Volume",
				MC_ASOC_DVOL_DIR0_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#0 ATT Switch",
				MC_ASOC_DVOL_DIR0_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#1 ATT Volume",
				MC_ASOC_DVOL_DIR1_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#1 ATT Switch",
				MC_ASOC_DVOL_DIR1_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#2 ATT Volume",
				MC_ASOC_DVOL_DIR2_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#2 ATT Switch",
				MC_ASOC_DVOL_DIR2_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Side Tone Playback Volume",
				MC_ASOC_DVOL_SIDETONE, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("Side Tone Playback Switch",
				MC_ASOC_DVOL_SIDETONE, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DAC Master Playback Volume",
				MC_ASOC_DVOL_DAC_MASTER, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DAC Master Playback Switch",
				MC_ASOC_DVOL_DAC_MASTER, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DAC Voice Playback Volume",
				MC_ASOC_DVOL_DAC_VOICE, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DAC Voice Playback Switch",
				MC_ASOC_DVOL_DAC_VOICE, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DAC Playback Volume",
				MC_ASOC_DVOL_DAC_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DAC Playback Switch",
				MC_ASOC_DVOL_DAC_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#0 Capture Volume",
				MC_ASOC_DVOL_DIT0, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#0 Capture Switch",
				MC_ASOC_DVOL_DIT0, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#1 Capture Volume",
				MC_ASOC_DVOL_DIT1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#1 Capture Switch",
				MC_ASOC_DVOL_DIT1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#2 Capture Volume",
				MC_ASOC_DVOL_DIT2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#2 Capture Switch",
				MC_ASOC_DVOL_DIT2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#2_1 Capture Volume",
				MC_ASOC_DVOL_DIT2_1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#2_1 Capture Switch",
				MC_ASOC_DVOL_DIT2_1, 7, 15, 1, 0),
	
	SOC_DOUBLE_TLV("AD#0 MIX2 Volume",
				MC_ASOC_DVOL_AD0_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#0 MIX2 Switch",
				MC_ASOC_DVOL_AD0_MIX2, 7, 15, 1, 0),
	
	SOC_DOUBLE_TLV("AD#1 MIX2 Volume",
				MC_ASOC_DVOL_AD1_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#1 MIX2 Switch",
				MC_ASOC_DVOL_AD1_MIX2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#0 MIX2 Volume",
				MC_ASOC_DVOL_DIR0_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#0 MIX2 Switch",
				MC_ASOC_DVOL_DIR0_MIX2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#1 MIX2 Volume",
				MC_ASOC_DVOL_DIR1_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#1 MIX2 Switch",
				MC_ASOC_DVOL_DIR1_MIX2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#2 MIX2 Volume",
				MC_ASOC_DVOL_DIR2_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#2 MIX2 Switch",
				MC_ASOC_DVOL_DIR2_MIX2, 7, 15, 1, 0),


	SOC_DOUBLE_TLV("Master Playback Volume",
				MC_ASOC_DVOL_MASTER, 0, 8, 75, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("Master Playback Switch",
				MC_ASOC_DVOL_MASTER, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Voice Playback Volume",
				MC_ASOC_DVOL_VOICE, 0, 8, 75, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("Voice Playback Switch",
				MC_ASOC_DVOL_VOICE, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AnalogIn Playback Digital Volume",
				MC_ASOC_DVOL_AIN, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AnalogIn Playback Digital Switch",
				MC_ASOC_DVOL_AIN, 7, 15, 1, 0),
	SOC_DOUBLE_TLV("AnalogIn Playback Volume",
				MC_ASOC_DVOL_AINPLAYBACK, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AnalogIn Playback Switch",
				MC_ASOC_DVOL_AINPLAYBACK, 7, 15, 1, 0),

	/*
	 * analog volumes and mute switches
	 */
	SOC_DOUBLE_TLV("AD#0 Analog Volume",
				MC_ASOC_AVOL_AD0, 0, 8, 31, 0, mc_asoc_tlv_adc),
	SOC_DOUBLE("AD#0 Analog Switch",
				MC_ASOC_AVOL_AD0, 7, 15, 1, 0),

	SOC_SINGLE_TLV("AD#1 Analog Volume",
				MC_ASOC_AVOL_AD1, 0, 31, 0, mc_asoc_tlv_adc),
	SOC_SINGLE("AD#1 Analog Switch",
				MC_ASOC_AVOL_AD1, 7, 1, 0),

	SOC_DOUBLE_TLV("Line 1 Bypass Playback Volume",
				MC_ASOC_AVOL_LIN1_BYPASS, 0, 8, 31, 0, mc_asoc_tlv_ain),
	SOC_DOUBLE("Line 1 Bypass Playback Switch",
				MC_ASOC_AVOL_LIN1_BYPASS, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Line 2 Bypass Playback Volume",
				MC_ASOC_AVOL_LIN2_BYPASS, 0, 8, 31, 0, mc_asoc_tlv_ain),
	SOC_DOUBLE("Line 2 Bypass Playback Switch",
				MC_ASOC_AVOL_LIN2_BYPASS, 7, 15, 1, 0),

	SOC_SINGLE_TLV("Mic 1 Bypass Playback Volume",
				MC_ASOC_AVOL_MIC1, 0, 31, 0, mc_asoc_tlv_ain),
	SOC_SINGLE("Mic 1 Bypass Playback Switch",
				MC_ASOC_AVOL_MIC1, 7, 1, 0),

	SOC_SINGLE_TLV("Mic 2 Bypass Playback Volume",
				MC_ASOC_AVOL_MIC2, 0, 31, 0, mc_asoc_tlv_ain),
	SOC_SINGLE("Mic 2 Bypass Playback Switch",
				MC_ASOC_AVOL_MIC2, 7, 1, 0),

	SOC_SINGLE_TLV("Mic 3 Bypass Playback Volume",
				MC_ASOC_AVOL_MIC3, 0, 31, 0, mc_asoc_tlv_ain),
	SOC_SINGLE("Mic 3 Bypass Playback Switch",
				MC_ASOC_AVOL_MIC3, 7, 1, 0),

	SOC_DOUBLE_TLV("Headphone Playback Volume",
				MC_ASOC_AVOL_HP, 0, 8, 31, 0, mc_asoc_tlv_hpsp),
	SOC_DOUBLE("Headphone Playback Switch",
				MC_ASOC_AVOL_HP, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Speaker Playback Volume",
				MC_ASOC_AVOL_SP, 0, 8, 31, 0, mc_asoc_tlv_hpsp),
	SOC_DOUBLE("Speaker Playback Switch",
				MC_ASOC_AVOL_SP, 7, 15, 1, 0),

	SOC_SINGLE_TLV("Receiver Playback Volume",
				MC_ASOC_AVOL_RC, 0, 31, 0, mc_asoc_tlv_hpsp),
	SOC_SINGLE("Receiver Playback Switch",
				MC_ASOC_AVOL_RC, 7, 1, 0),

	SOC_DOUBLE_TLV("Line 1 Playback Volume",
				MC_ASOC_AVOL_LOUT1, 0, 8, 31, 0, mc_asoc_tlv_aout),
	SOC_DOUBLE("Line 1 Playback Switch",
				MC_ASOC_AVOL_LOUT1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Line 2 Playback Volume",
				MC_ASOC_AVOL_LOUT2, 0, 8, 31, 0, mc_asoc_tlv_aout),
	SOC_DOUBLE("Line 2 Playback Switch",
				MC_ASOC_AVOL_LOUT2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AnalogIn Playback Analog Volume",
				MC_ASOC_AVOL_AIN, 0, 8, 31, 0, mc_asoc_tlv_adc),
	SOC_DOUBLE("AnalogIn Playback Analog Switch",
				MC_ASOC_AVOL_AIN, 7, 15, 1, 0),


	SOC_SINGLE_TLV("Mic 1 Gain Volume",
				MC_ASOC_AVOL_MIC1_GAIN, 0, 3, 0, mc_asoc_tlv_micgain),

	SOC_SINGLE_TLV("Mic 2 Gain Volume",
				MC_ASOC_AVOL_MIC2_GAIN, 0, 3, 0, mc_asoc_tlv_micgain),

	SOC_SINGLE_TLV("Mic 3 Gain Volume",
				MC_ASOC_AVOL_MIC3_GAIN, 0, 3, 0, mc_asoc_tlv_micgain),

	SOC_SINGLE_TLV("HP Gain Playback Volume",
				MC_ASOC_AVOL_HP_GAIN, 0, 3, 0, mc_asoc_tlv_hpgain),
};

/*
 * Same as snd_soc_add_controls supported in alsa-driver 1.0.19 or later.
 * This function is implimented for compatibility with linux 2.6.29.
 */
static int mc_asoc_add_controls(
	struct snd_soc_codec *codec,
	const struct snd_kcontrol_new *controls,
	int n)
{
	int		err, i;
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
#else
	struct snd_soc_card			*soc_card		= NULL;
#endif
	struct snd_card				*card			= NULL;
	struct soc_mixer_control	*mixer_control	= NULL;

	if(codec != NULL) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
		card	= codec->card;
#else
		soc_card	= codec->card;
		if(soc_card != NULL) {
			card	= soc_card->snd_card;
		}
#endif
	}
	if(controls != NULL) {
		mixer_control	=  (struct soc_mixer_control *)controls->private_value;
	}
	if((card == NULL) || (mixer_control == NULL)) {
		return -EINVAL;
	}

	/* mc_asoc not use mixer control */
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		for (i	= 0; i < n; i++, controls++, mixer_control++) {
			if ((mixer_control->reg != MC_ASOC_DVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_ATT) &&
				(mixer_control->reg != MC_ASOC_AVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_AVOL_LIN1_BYPASS) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_MIX2)
				) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
				if ((err = snd_ctl_add(card, snd_soc_cnew(controls, codec, NULL))) < 0) {
					return err;
				}
#else
				if ((err = snd_ctl_add((struct snd_card *)codec->card->snd_card,
							snd_soc_cnew(controls, codec, NULL, NULL))) < 0) {
					return err;
				}
#endif
			}
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		for (i	= 0; i < n; i++, controls++, mixer_control++) {
			if ((mixer_control->reg != MC_ASOC_DVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_ATT) &&
				(mixer_control->reg != MC_ASOC_DVOL_DTMF_B) &&
				(mixer_control->reg != MC_ASOC_DVOL_DTMF) &&
				(mixer_control->reg != MC_ASOC_AVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_AVOL_LIN2_BYPASS) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIT2_1) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD0_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIR0_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIR1_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIR2_MIX2)
				) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
				if ((err = snd_ctl_add(card, snd_soc_cnew(controls, codec, NULL))) < 0) {
					return err;
				}
#else
				if ((err = snd_ctl_add((struct snd_card *)codec->card->snd_card,
							snd_soc_cnew(controls, codec, NULL, NULL))) < 0) {
					return err;
				}
#endif
			}
		}
	}
	else {
		for (i	= 0; i < n; i++, controls++) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
			if ((err = snd_ctl_add(card, snd_soc_cnew(controls, codec, NULL))) < 0) {
				return err;
			}
#else
			if ((err = snd_ctl_add((struct snd_card *)codec->card->snd_card,
						snd_soc_cnew(controls, codec, NULL, NULL))) < 0) {
				return err;
			}
#endif
		}
	}
	return 0;
}

/*
 * dapm_mixer_set
 */
/* Audio Mode */
static const char	*audio_mode_param_text[] = {
	"off", "audio", "incall", "audio+incall", "incommunication"
};
static const struct soc_enum	audio_mode_play_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AUDIO_MODE_PLAY, 0,
		ARRAY_SIZE(audio_mode_param_text), audio_mode_param_text);
static const struct snd_kcontrol_new	audio_mode_play_param_mux = 
	SOC_DAPM_ENUM("Audio Mode Playback", audio_mode_play_param_enum);
static const struct soc_enum	audio_mode_cap_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AUDIO_MODE_CAP, 0,
		ARRAY_SIZE(audio_mode_param_text), audio_mode_param_text);
static const struct snd_kcontrol_new	audio_mode_cap_param_mux = 
	SOC_DAPM_ENUM("Audio Mode Capture", audio_mode_cap_param_enum);

/* Output Path */
static const char	*output_path_param_text[] = {
	"SP", "RC", "HP", "HS", "LO1", "LO2", "BT",
	"SP+RC", "SP+HP", "SP+LO1", "SP+LO2", "SP+BT"
};
static const struct soc_enum	output_path_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_OUTPUT_PATH, 0,
		ARRAY_SIZE(output_path_param_text), output_path_param_text);
static const struct snd_kcontrol_new	output_path_param_mux = 
	SOC_DAPM_ENUM("Output Path", output_path_param_enum);

/* Input Path */
static const char	*input_path_param_text[] = {
	"MainMIC", "SubMIC", "2MIC", "Headset", "Bluetooth",
	"VoiceCall", "VoiceUplink", "VoiceDownlink", "Linein1", "Linein2"
};
static const struct soc_enum	input_path_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INPUT_PATH, 0,
		ARRAY_SIZE(input_path_param_text), input_path_param_text);
static const struct snd_kcontrol_new	input_path_param_mux = 
	SOC_DAPM_ENUM("Input Path", input_path_param_enum);

/* Incall Mic */
static const char	*incall_mic_param_text[] = {
	"MainMIC", "SubMIC", "2MIC"
};
static const struct soc_enum	incall_mic_sp_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_SP, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_sp_param_mux = 
	SOC_DAPM_ENUM("Incall Mic Speaker", incall_mic_sp_param_enum);
static const struct soc_enum	incall_mic_rc_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_RC, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_rc_param_mux = 
	SOC_DAPM_ENUM("Incall Mic Receiver", incall_mic_rc_param_enum);
static const struct soc_enum	incall_mic_hp_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_HP, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_hp_param_mux = 
	SOC_DAPM_ENUM("Incall Mic Headphone", incall_mic_hp_param_enum);
static const struct soc_enum	incall_mic_lo1_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_LO1, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_lo1_param_mux = 
	SOC_DAPM_ENUM("Incall Mic LineOut1", incall_mic_lo1_param_enum);
static const struct soc_enum	incall_mic_lo2_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_LO2, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_lo2_param_mux = 
	SOC_DAPM_ENUM("Incall Mic LineOut2", incall_mic_lo2_param_enum);

/* Playback Path */
static const struct snd_kcontrol_new	mainmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_MAINMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	submic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_SUBMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	msmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_2MIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	hsmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_HSMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	btmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_BTMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	lin1_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_LIN1_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	lin2_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_LIN2_PLAYBACK_PATH, 0, 1, 0);

/* Parameter Setting */
static const char	*parameter_setting_param_text[] = {
	"DUMMY"
};
static const struct soc_enum	parameter_setting_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_PARAMETER_SETTING, 0,
		ARRAY_SIZE(parameter_setting_param_text), parameter_setting_param_text);
static const struct snd_kcontrol_new	parameter_setting_param_mux = 
	SOC_DAPM_ENUM("Parameter Setting", parameter_setting_param_enum);

/* DTMF Control */
static const struct snd_kcontrol_new	dtmf_control_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_DTMF_CONTROL, 0, 1, 0);

/* DTMF Output */
static const char	*dtmf_output_param_text[] = {
	"SP", "NORMAL"
};
static const struct soc_enum	dtmf_output_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_DTMF_OUTPUT, 0,
		ARRAY_SIZE(dtmf_output_param_text), dtmf_output_param_text);
static const struct snd_kcontrol_new	dtmf_output_param_mux = 
	SOC_DAPM_ENUM("DTMF Output", dtmf_output_param_enum);

#ifdef MC_ASOC_TEST
static const char	*mic_param_text[] = {
	"NONE", "MIC1", "MIC2", "MIC3", "PDM"
};

/*	Main Mic	*/
static const struct soc_enum	main_mic_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MAIN_MIC, 0, ARRAY_SIZE(mic_param_text), mic_param_text);
static const struct snd_kcontrol_new	main_mic_param_mux = 
	SOC_DAPM_ENUM("Main Mic", main_mic_param_enum);
/*	Sub Mic	*/
static const struct soc_enum	sub_mic_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_SUB_MIC, 0, ARRAY_SIZE(mic_param_text), mic_param_text);
static const struct snd_kcontrol_new	sub_mic_param_mux = 
	SOC_DAPM_ENUM("Sub Mic", sub_mic_param_enum);
/*	Headset Mic	*/
static const struct soc_enum	hs_mic_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_HS_MIC, 0, ARRAY_SIZE(mic_param_text)-1, mic_param_text);
static const struct snd_kcontrol_new	hs_mic_param_mux = 
	SOC_DAPM_ENUM("Headset Mic", hs_mic_param_enum);

/*	AP_PLAY_PORT	*/
static const char	*ap_play_port_param_text[] = {
	"DIO_0", "DIO_1", "DIO_2", "LIN1", "LIN2"
};
static const struct soc_enum	ap_play_port_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AP_PLAY_PORT, 0, 5, ap_play_port_param_text);
static const struct snd_kcontrol_new	ap_play_port_param_mux = 
	SOC_DAPM_ENUM("AP Play Port", ap_play_port_param_enum);
/*	AP_CAP_PORT	*/
static const char	*ap_cap_port_param_text[] = {
	"DIO_0", "DIO_1", "DIO_2", "LOUT1", "LOUT2"
};
static const struct soc_enum	ap_cap_port_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AP_CAP_PORT, 0, 5, ap_cap_port_param_text);
static const struct snd_kcontrol_new	ap_cap_port_param_mux = 
	SOC_DAPM_ENUM("AP Capture Port", ap_cap_port_param_enum);
/*	CP_PORT	*/
static const char	*cp_port_param_text[] = {
	"DIO_0", "DIO_1", "DIO_2", "LIN1_LOUT1", "LIN2_LOUT1", "LIN1_LOUT2", "LIN2_LOUT2"
};
static const struct soc_enum	cp_port_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_CP_PORT, 0, ARRAY_SIZE(cp_port_param_text), cp_port_param_text);
static const struct snd_kcontrol_new	cp_port_param_mux = 
	SOC_DAPM_ENUM("CP Port", cp_port_param_enum);
/*	BT_PORT	*/
static const char	*bt_port_param_text[] = {
	"DIO_0", "DIO_1", "DIO_2"
};
static const struct soc_enum	bt_port_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_BT_PORT, 0, 3, bt_port_param_text);
static const struct snd_kcontrol_new	bt_port_param_mux = 
	SOC_DAPM_ENUM("BT Port", bt_port_param_enum);

/*	HANDSFREE	*/
static const char	*hf_param_text[] = {
	"OFF", "ON"
};
static const struct soc_enum	hf_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_HF, 0, ARRAY_SIZE(hf_param_text), hf_param_text);
static const struct snd_kcontrol_new	hf_param_mux = 
	SOC_DAPM_ENUM("Handsfree3.0", hf_param_enum);

/*	MIC1_BIAS	*/
static const char	*mic_bias_param_text[] = {
	"OFF", "ALWAYS_ON", "SYNC_MIC"
};
static const struct soc_enum	mic1_bias_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MIC1_BIAS, 0, ARRAY_SIZE(mic_bias_param_text), mic_bias_param_text);
static const struct snd_kcontrol_new	mic1_bias_param_mux = 
	SOC_DAPM_ENUM("MIC1 BIAS", mic1_bias_param_enum);
static const struct soc_enum	mic2_bias_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MIC2_BIAS, 0, ARRAY_SIZE(mic_bias_param_text), mic_bias_param_text);
static const struct snd_kcontrol_new	mic2_bias_param_mux = 
	SOC_DAPM_ENUM("MIC2 BIAS", mic2_bias_param_enum);
static const struct soc_enum	mic3_bias_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MIC3_BIAS, 0, ARRAY_SIZE(mic_bias_param_text), mic_bias_param_text);
static const struct snd_kcontrol_new	mic3_bias_param_mux = 
	SOC_DAPM_ENUM("MIC3 BIAS", mic3_bias_param_enum);

/*	SP	*/
static const char	*sp_param_text[] = {
	"STEREO", "MONO_L", "MONO_R"
};
static const struct soc_enum	sp_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_SP, 0, ARRAY_SIZE(sp_param_text), sp_param_text);
static const struct snd_kcontrol_new	sp_param_mux = 
	SOC_DAPM_ENUM("SP", sp_param_enum);

/*	AE SRC PLAY	*/
static const char	*ae_src_play_param_text[] = {
	"NONE", "DIR0", "DIR1", "DIR2", "MIX", "PDM", "ADC0", "DTMF", "CDSP"
};
static const struct soc_enum	ae_src_play_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AE_SRC_PLAY, 0, ARRAY_SIZE(ae_src_play_param_text), ae_src_play_param_text);
static const struct snd_kcontrol_new	ae_src_play_param_mux = 
	SOC_DAPM_ENUM("AE SRC PLAY", ae_src_play_param_enum);

/*	AE SRC CAP	*/
static const char	*ae_src_cap_param_text[] = {
	"NONE", "PDM", "ADC0", "ADC1", "DIR0", "DIR1", "DIR2", "INPUT_PATH"
};
static const struct soc_enum	ae_src_cap_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AE_SRC_CAP, 0, ARRAY_SIZE(ae_src_cap_param_text), ae_src_cap_param_text);
static const struct snd_kcontrol_new	ae_src_cap_param_mux = 
	SOC_DAPM_ENUM("AE SRC CAP", ae_src_cap_param_enum);

/*	AE PRIORITY	*/
static const char	*ae_priority_param_text[] = {
	"1ST", "LAST", "PLAY", "CAP"
};
static const struct soc_enum	ae_priority_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AE_PRIORITY, 0, ARRAY_SIZE(ae_priority_param_text), ae_priority_param_text);
static const struct snd_kcontrol_new	ae_priority_param_mux = 
	SOC_DAPM_ENUM("AE PRIORITY", ae_priority_param_enum);

/*	C-DSP PRIORITY	*/
static const char	*cdsp_priority_param_text[] = {
	"1ST", "LAST", "PLAY", "CAP"
};
static const struct soc_enum	cdsp_priority_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_CDSP_PRIORITY, 0, ARRAY_SIZE(cdsp_priority_param_text), cdsp_priority_param_text);
static const struct snd_kcontrol_new	cdsp_priority_param_mux = 
	SOC_DAPM_ENUM("C-DSP PRIORITY", cdsp_priority_param_enum);

/*	SIDETONE	*/
static const char	*sidetone_param_text[] = {
	"OFF", "ANALOG", "DIGITAL"
};
static const struct soc_enum	sidetone_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_SIDETONE, 0, ARRAY_SIZE(sidetone_param_text), sidetone_param_text);
static const struct snd_kcontrol_new	sidetone_param_mux = 
	SOC_DAPM_ENUM("SIDETONE", sidetone_param_enum);
#endif

static const struct snd_soc_dapm_widget	mc_asoc_widgets_common[] = {
	/* Audio Mode */
	SND_SOC_DAPM_INPUT("OFF"),
	SND_SOC_DAPM_INPUT("AUDIO"),
	SND_SOC_DAPM_INPUT("INCALL"),
	SND_SOC_DAPM_INPUT("AUDIO+INCALL"),
	SND_SOC_DAPM_INPUT("INCOMMUNICATION"),
	SND_SOC_DAPM_MUX("Audio Mode Playback", SND_SOC_NOPM, 0, 0, &audio_mode_play_param_mux),
	SND_SOC_DAPM_MUX("Audio Mode Capture", SND_SOC_NOPM, 0, 0, &audio_mode_cap_param_mux),

	/* Output Path */
	SND_SOC_DAPM_INPUT("SP"),
	SND_SOC_DAPM_INPUT("RC"),
	SND_SOC_DAPM_INPUT("HP"),
	SND_SOC_DAPM_INPUT("HS"),
	SND_SOC_DAPM_INPUT("LO1"),
	SND_SOC_DAPM_INPUT("LO2"),
	SND_SOC_DAPM_INPUT("BT"),
	SND_SOC_DAPM_INPUT("SP+RC"),
	SND_SOC_DAPM_INPUT("SP+HP"),
	SND_SOC_DAPM_INPUT("SP+LO1"),
	SND_SOC_DAPM_INPUT("SP+LO2"),
	SND_SOC_DAPM_INPUT("SP+BT"),
	SND_SOC_DAPM_MUX("Output Path", SND_SOC_NOPM, 0, 0, &output_path_param_mux),

	/* Input Path */
	SND_SOC_DAPM_INPUT("MainMIC"),
	SND_SOC_DAPM_INPUT("SubMIC"),
	SND_SOC_DAPM_INPUT("2MIC"),
	SND_SOC_DAPM_INPUT("Headset"),
	SND_SOC_DAPM_INPUT("Bluetooth"),
	SND_SOC_DAPM_INPUT("VoiceCall"),
	SND_SOC_DAPM_INPUT("VoiceUplink"),
	SND_SOC_DAPM_INPUT("VoiceDownlink"),
	SND_SOC_DAPM_INPUT("Linein1"),
	SND_SOC_DAPM_INPUT("Linein2"),
	SND_SOC_DAPM_MUX("Input Path", SND_SOC_NOPM, 0, 0, &input_path_param_mux),

	/* Incall Mic */
	SND_SOC_DAPM_INPUT("MainMIC"),
	SND_SOC_DAPM_INPUT("SubMIC"),
	SND_SOC_DAPM_INPUT("2MIC"),
	SND_SOC_DAPM_MUX("Incall Mic Speaker", SND_SOC_NOPM, 0, 0, &incall_mic_sp_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic Receiver", SND_SOC_NOPM, 0, 0, &incall_mic_rc_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic Headphone", SND_SOC_NOPM, 0, 0, &incall_mic_hp_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic LineOut1", SND_SOC_NOPM, 0, 0, &incall_mic_lo1_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic LineOut2", SND_SOC_NOPM, 0, 0, &incall_mic_lo2_param_mux),

	/* Playback Path */
	SND_SOC_DAPM_INPUT("ONOFF"),
	SND_SOC_DAPM_INPUT("MainMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("MainMIC Playback Path", SND_SOC_NOPM, 0, 0, &mainmic_playback_path_sw),
	SND_SOC_DAPM_INPUT("SubMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("SubMIC Playback Path", SND_SOC_NOPM, 0, 0, &submic_playback_path_sw),
	SND_SOC_DAPM_INPUT("2MIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("2MIC Playback Path", SND_SOC_NOPM, 0, 0, &msmic_playback_path_sw),
	SND_SOC_DAPM_INPUT("HeadsetMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("HeadsetMIC Playback Path", SND_SOC_NOPM, 0, 0, &hsmic_playback_path_sw),
	SND_SOC_DAPM_INPUT("BluetoothMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("BluetoothMIC Playback Path", SND_SOC_NOPM, 0, 0, &btmic_playback_path_sw),

	/* Parameter Setting */
	SND_SOC_DAPM_INPUT("DUMMY"),
	SND_SOC_DAPM_MUX("Parameter Setting",
		SND_SOC_NOPM, 0, 0, &parameter_setting_param_mux),

	/* DTMF Output */
	SND_SOC_DAPM_INPUT("SP"),
	SND_SOC_DAPM_INPUT("NORMAL"),
	SND_SOC_DAPM_MUX("DTMF Output", SND_SOC_NOPM, 0, 0, &dtmf_output_param_mux)
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_LIN1[] = {
	/* LIN 1 Playback Path */
	SND_SOC_DAPM_INPUT("ONOFF"),
	SND_SOC_DAPM_INPUT("LIN 1 Playback Switch"),
	SND_SOC_DAPM_SWITCH("LIN 1 Playback Path",
		SND_SOC_NOPM, 0, 0, &lin1_playback_path_sw)
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_LIN2[] = {
	/* LIN 2 Playback Path */
	SND_SOC_DAPM_INPUT("ONOFF"),
	SND_SOC_DAPM_INPUT("LIN 2 Playback Switch"),
	SND_SOC_DAPM_SWITCH("LIN 2 Playback Path",
		SND_SOC_NOPM, 0, 0, &lin2_playback_path_sw)
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_DTMF[] = {
	/* DTMF Control */
	SND_SOC_DAPM_INPUT("DTMF Control Switch"),
	SND_SOC_DAPM_SWITCH("DTMF Control",
		SND_SOC_NOPM, 0, 0, &dtmf_control_sw)
};

#ifdef MC_ASOC_TEST
static const struct snd_soc_dapm_widget	mc_asoc_widgets_test[] = {
	SND_SOC_DAPM_INPUT("NONE"),
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("MIC3"),
	SND_SOC_DAPM_INPUT("PDM"),
	/*	Main Mic	*/
	SND_SOC_DAPM_MUX("Main Mic", SND_SOC_NOPM, 0, 0, &main_mic_param_mux),
	/*	Sub Mic	*/
	SND_SOC_DAPM_MUX("Sub Mic", SND_SOC_NOPM, 0, 0, &sub_mic_param_mux),
	/*	Headset Mic	*/
	SND_SOC_DAPM_MUX("Headset Mic", SND_SOC_NOPM, 0, 0, &hs_mic_param_mux),

	SND_SOC_DAPM_INPUT("DIO_0"),
	SND_SOC_DAPM_INPUT("DIO_1"),
	SND_SOC_DAPM_INPUT("DIO_2"),
	SND_SOC_DAPM_INPUT("LIN1"),
	SND_SOC_DAPM_INPUT("LIN2"),
	SND_SOC_DAPM_INPUT("LOUT1"),
	SND_SOC_DAPM_INPUT("LOUT2"),
	SND_SOC_DAPM_INPUT("LIN1_LOUT1"),
	SND_SOC_DAPM_INPUT("LIN2_LOUT1"),
	SND_SOC_DAPM_INPUT("LIN1_LOUT2"),
	SND_SOC_DAPM_INPUT("LIN2_LOUT2"),

	/*	AP Play Port	*/
	SND_SOC_DAPM_MUX("AP Play Port", SND_SOC_NOPM, 0, 0, &ap_play_port_param_mux),
	/*	AP Cap Port	*/
	SND_SOC_DAPM_MUX("AP Cap Port", SND_SOC_NOPM, 0, 0, &ap_cap_port_param_mux),
	/*	CP Port	*/
	SND_SOC_DAPM_MUX("CP Port", SND_SOC_NOPM, 0, 0, &cp_port_param_mux),
	/*	CP Port	*/
	SND_SOC_DAPM_MUX("BT Port", SND_SOC_NOPM, 0, 0, &bt_port_param_mux),

	/*	HANDSFREE	*/
	SND_SOC_DAPM_INPUT("OFF"),
	SND_SOC_DAPM_INPUT("ON"),
	SND_SOC_DAPM_MUX("Handsfree3.0", SND_SOC_NOPM, 0, 0, &hf_param_mux),

	/*	MICx BIAS	*/
	SND_SOC_DAPM_INPUT("OFF"),
	SND_SOC_DAPM_INPUT("ALWAYS_ON"),
	SND_SOC_DAPM_INPUT("SYNC_MIC"),
	SND_SOC_DAPM_MUX("MIC1 BIAS", SND_SOC_NOPM, 0, 0, &mic1_bias_param_mux),
	SND_SOC_DAPM_MUX("MIC2 BIAS", SND_SOC_NOPM, 0, 0, &mic2_bias_param_mux),
	SND_SOC_DAPM_MUX("MIC3 BIAS", SND_SOC_NOPM, 0, 0, &mic3_bias_param_mux),

	/*	SP	*/
	SND_SOC_DAPM_INPUT("STEREO"),
	SND_SOC_DAPM_INPUT("MONO_L"),
	SND_SOC_DAPM_INPUT("MONO_R"),
	SND_SOC_DAPM_MUX("SP", SND_SOC_NOPM, 0, 0, &sp_param_mux),

	/*	AE SRC PLAY	*/
	SND_SOC_DAPM_INPUT("NONE"),
	SND_SOC_DAPM_INPUT("DIR0"),
	SND_SOC_DAPM_INPUT("DIR1"),
	SND_SOC_DAPM_INPUT("DIR2"),
	SND_SOC_DAPM_INPUT("MIX"),
	SND_SOC_DAPM_INPUT("PDM"),
	SND_SOC_DAPM_INPUT("ADC0"),
	SND_SOC_DAPM_INPUT("DTMF"),
	SND_SOC_DAPM_INPUT("CDSP"),
	SND_SOC_DAPM_MUX("AE SRC PLAY", SND_SOC_NOPM, 0, 0, &ae_src_play_param_mux),

	/*	AE SRC CAP	*/
	SND_SOC_DAPM_INPUT("NONE"),
	SND_SOC_DAPM_INPUT("PDM"),
	SND_SOC_DAPM_INPUT("ADC0"),
	SND_SOC_DAPM_INPUT("ADC1"),
	SND_SOC_DAPM_INPUT("DIR0"),
	SND_SOC_DAPM_INPUT("DIR1"),
	SND_SOC_DAPM_INPUT("DIR2"),
	SND_SOC_DAPM_INPUT("INPUT_PATH"),
	SND_SOC_DAPM_MUX("AE SRC CAP", SND_SOC_NOPM, 0, 0, &ae_src_cap_param_mux),

	/*	AE PRIORITY	*/
	SND_SOC_DAPM_INPUT("1ST"),
	SND_SOC_DAPM_INPUT("LAST"),
	SND_SOC_DAPM_INPUT("PLAY"),
	SND_SOC_DAPM_INPUT("CAP"),
	SND_SOC_DAPM_MUX("AE PRIORITY", SND_SOC_NOPM, 0, 0, &ae_priority_param_mux),

	/*	CDSP PRIORITY	*/
	SND_SOC_DAPM_MUX("C-DSP PRIORITY", SND_SOC_NOPM, 0, 0, &cdsp_priority_param_mux),

	/*	SIDETONE	*/
	SND_SOC_DAPM_INPUT("OFF"),
	SND_SOC_DAPM_INPUT("ANALOG"),
	SND_SOC_DAPM_INPUT("DIGITAL"),
	SND_SOC_DAPM_MUX("SIDETONE", SND_SOC_NOPM, 0, 0, &sidetone_param_mux),
};
#endif

static const struct snd_soc_dapm_route	mc_asoc_intercon_common[] = {
	{"Audio Mode Playback",	"off",				"OFF"},
	{"Audio Mode Playback",	"audio",			"AUDIO"},
	{"Audio Mode Playback",	"incall",			"INCALL"},
	{"Audio Mode Playback",	"audio+incall",		"AUDIO+INCALL"},
	{"Audio Mode Playback",	"incommunication",	"INCOMMUNICATION"},
	{"Audio Mode Capture",	"off",				"OFF"},
	{"Audio Mode Capture",	"audio",			"AUDIO"},
	{"Audio Mode Capture",	"incall",			"INCALL"},
	{"Audio Mode Capture",	"audio+incall",		"AUDIO+INCALL"},
	{"Audio Mode Capture",	"incommunication",	"INCOMMUNICATION"},

	{"Output Path",			"SP",				"SP"},
	{"Output Path",			"RC",				"RC"},
	{"Output Path",			"HP",				"HP"},
	{"Output Path",			"HS",				"HS"},
	{"Output Path",			"LO1",				"LO1"},
	{"Output Path",			"LO2",				"LO2"},
	{"Output Path",			"BT",				"BT"},
	{"Output Path",			"SP+RC",			"SP+RC"},
	{"Output Path",			"SP+HP",			"SP+HP"},
	{"Output Path",			"SP+LO1",			"SP+LO1"},
	{"Output Path",			"SP+LO2",			"SP+LO2"},
	{"Output Path",			"SP+BT",			"SP+BT"},

	{"Input Path",			"MainMIC",			"MainMIC"},
	{"Input Path",			"SubMIC",			"SubMIC"},
	{"Input Path",			"2MIC",				"2MIC"},
	{"Input Path",			"Headset",			"Headset"},
	{"Input Path",			"Bluetooth",		"Bluetooth"},
	{"Input Path",			"VoiceCall",		"VoiceCall"},
	{"Input Path",			"VoiceUplink",		"VoiceUplink"},
	{"Input Path",			"VoiceDownlink",	"VoiceDownlink"},
	{"Input Path",			"Linein1",			"Linein1"},
	{"Input Path",			"Linein2",			"Linein2"},

	{"Incall Mic Speaker",	"MainMIC",			"MainMIC"},
	{"Incall Mic Speaker",	"SubMIC",			"SubMIC"},
	{"Incall Mic Speaker",	"2MIC",				"2MIC"},
	{"Incall Mic Receiver",	"MainMIC",			"MainMIC"},
	{"Incall Mic Receiver",	"SubMIC",			"SubMIC"},
	{"Incall Mic Receiver",	"2MIC",				"2MIC"},
	{"Incall Mic Headphone","MainMIC",			"MainMIC"},
	{"Incall Mic Headphone","SubMIC",			"SubMIC"},
	{"Incall Mic Headphone","2MIC",				"2MIC"},
	{"Incall Mic LineOut1",	"MainMIC",			"MainMIC"},
	{"Incall Mic LineOut1",	"SubMIC",			"SubMIC"},
	{"Incall Mic LineOut1",	"2MIC",				"2MIC"},
	{"Incall Mic LineOut2",	"MainMIC",			"MainMIC"},
	{"Incall Mic LineOut2",	"SubMIC",			"SubMIC"},
	{"Incall Mic LineOut2",	"2MIC",				"2MIC"},

	{"MainMIC Playback Path",		"Switch",	"ONOFF"},
	{"MainMIC Playback Switch",		NULL,		"MainMIC Playback Path"},
	{"SubMIC Playback Path",		"Switch",	"ONOFF"},
	{"SubMIC Playback Switch",		NULL,		"SubMIC Playback Path"},
	{"2MIC Playback Path",			"Switch",	"ONOFF"},
	{"2MIC Playback Switch",		NULL,		"2MIC Playback Path"},
	{"HeadsetMIC Playback Path",	"Switch",	"ONOFF"},
	{"HeadsetMIC Playback Switch",	NULL,		"HeadsetMIC Playback Path"},
	{"BluetoothMIC Playback Path",	"Switch",	"ONOFF"},
	{"BluetoothMIC Playback Switch",NULL,		"BluetoothMIC Playback Path"},

	{"Parameter Setting",	"DUMMY",			"DUMMY"},

	{"DTMF Output",			"SP",				"SP"},
	{"DTMF Output",			"NORMAL",			"NORMAL"}
};

static const struct snd_soc_dapm_route	mc_asoc_intercon_LIN1[] = {
	{"LIN 1 Playback Path",	"Switch",			"ONOFF"},
	{"LIN 1 Playback Switch",NULL,				"LIN 1 Playback Path"}
};

static const struct snd_soc_dapm_route	mc_asoc_intercon_LIN2[] = {
	{"LIN 2 Playback Path",	"Switch",			"ONOFF"},
	{"LIN 2 Playback Switch",NULL,				"LIN 2 Playback Path"}
};

static const struct snd_soc_dapm_route	mc_asoc_intercon_DTMF[] = {
	{"DTMF Control",		"Switch",			"ONOFF"},
	{"DTMF Control Switch",	NULL,				"DTMF Control"}
};

#ifdef MC_ASOC_TEST
static const struct snd_soc_dapm_route	mc_asoc_intercon_test[] = {
	{"Main Mic",			"NONE",				"NONE"},
	{"Main Mic",			"MIC1",				"MIC1"},
	{"Main Mic",			"MIC2",				"MIC2"},
	{"Main Mic",			"MIC3",				"MIC3"},
	{"Main Mic",			"PDM",				"PDM"},
	{"Sub Mic",				"NONE",				"NONE"},
	{"Sub Mic",				"MIC1",				"MIC1"},
	{"Sub Mic",				"MIC2",				"MIC2"},
	{"Sub Mic",				"MIC3",				"MIC3"},
	{"Sub Mic",				"PDM",				"PDM"},
	{"Headset Mic",			"NONE",				"NONE"},
	{"Headset Mic",			"MIC1",				"MIC1"},
	{"Headset Mic",			"MIC2",				"MIC2"},
	{"Headset Mic",			"MIC3",				"MIC3"},
	{"AP Play Port",		"DIO_0",			"DIO_0"},
	{"AP Play Port",		"DIO_1",			"DIO_1"},
	{"AP Play Port",		"DIO_2",			"DIO_2"},
	{"AP Play Port",		"LIN1",				"LIN1"},
	{"AP Play Port",		"LIN2",				"LIN2"},
	{"AP Cap Port",			"DIO_0",			"DIO_0"},
	{"AP Cap Port",			"DIO_1",			"DIO_1"},
	{"AP Cap Port",			"DIO_2",			"DIO_2"},
	{"AP Cap Port",			"LOUT1",			"LOUT1"},
	{"AP Cap Port",			"LOUT2",			"LOUT2"},
	{"CP Port",				"DIO_0",			"DIO_0"},
	{"CP Port",				"DIO_1",			"DIO_1"},
	{"CP Port",				"DIO_2",			"DIO_2"},
	{"CP Port",				"LIN1_LOUT1",		"LIN1_LOUT1"},
	{"CP Port",				"LIN2_LOUT1",		"LIN2_LOUT1"},
	{"CP Port",				"LIN1_LOUT2",		"LIN1_LOUT2"},
	{"CP Port",				"LIN2_LOUT2",		"LIN2_LOUT2"},
	{"BT Port",				"DIO_0",			"DIO_0"},
	{"BT Port",				"DIO_1",			"DIO_1"},
	{"BT Port",				"DIO_2",			"DIO_2"},
	{"Handsfree3.0",		"OFF",				"OFF"},
	{"Handsfree3.0",		"ON",				"ON"},
	{"MIC1 BIAS",			"OFF",				"OFF"},
	{"MIC1 BIAS",			"ALWAYS_ON",		"ALWAYS_ON"},
	{"MIC1 BIAS",			"SYNC_MIC",			"SYNC_MIC"},
	{"MIC2 BIAS",			"OFF",				"OFF"},
	{"MIC2 BIAS",			"ALWAYS_ON",		"ALWAYS_ON"},
	{"MIC2 BIAS",			"SYNC_MIC",			"SYNC_MIC"},
	{"MIC3 BIAS",			"OFF",				"OFF"},
	{"MIC3 BIAS",			"ALWAYS_ON",		"ALWAYS_ON"},
	{"MIC3 BIAS",			"SYNC_MIC",			"SYNC_MIC"},
	{"SP",					"STEREO",			"STEREO"},
	{"SP",					"MONO_L",			"MONO_L"},
	{"SP",					"MONO_R",			"MONO_R"},
	{"AE SRC PLAY",			"NONE",				"NONE"},
	{"AE SRC PLAY",			"DIR0",				"DIR0"},
	{"AE SRC PLAY",			"DIR1",				"DIR1"},
	{"AE SRC PLAY",			"DIR2",				"DIR2"},
	{"AE SRC PLAY",			"MIX",				"MIX"},
	{"AE SRC PLAY",			"PDM",				"ADC0"},
	{"AE SRC PLAY",			"ADC0",				"ADC0"},
	{"AE SRC PLAY",			"DTMF",				"DTMF"},
	{"AE SRC PLAY",			"CDSP",				"CDSP"},
	{"AE SRC CAP",			"NONE",				"NONE"},
	{"AE SRC CAP",			"PDM",				"PDM"},
	{"AE SRC CAP",			"ADC0",				"ADC0"},
	{"AE SRC CAP",			"ADC1",				"ADC1"},
	{"AE SRC CAP",			"DIR0",				"DIR0"},
	{"AE SRC CAP",			"DIR1",				"DIR1"},
	{"AE SRC CAP",			"DIR2",				"DIR2"},
	{"AE SRC CAP",			"INPUT_PATH",		"INPUT_PATH"},
	{"AE PRIORITY",			"1ST",				"1ST"},
	{"AE PRIORITY",			"LAST",				"LAST"},
	{"AE PRIORITY",			"PLAY",				"PLAY"},
	{"AE PRIORITY",			"CAP",				"CAP"},
	{"C-DSP PRIORITY",		"1ST",				"1ST"},
	{"C-DSP PRIORITY",		"LAST",				"LAST"},
	{"C-DSP PRIORITY",		"PLAY",				"PLAY"},
	{"C-DSP PRIORITY",		"CAP",				"CAP"},
	{"SIDETONE",			"OFF",				"OFF"},
	{"SIDETONE",			"ANALOG",			"ANALOG"},
	{"SIDETONE",			"DIGITAL",			"DIGITAL"},
};
#endif

static int mc_asoc_add_widgets(struct snd_soc_codec *codec)
{
	int		err;

	if(codec == NULL) {
		return -EINVAL;
	}


#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_common, ARRAY_SIZE(mc_asoc_widgets_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
	}

	if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_common, ARRAY_SIZE(mc_asoc_intercon_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}

  #ifdef MC_ASOC_TEST
	if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_test, ARRAY_SIZE(mc_asoc_widgets_test))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_test, ARRAY_SIZE(mc_asoc_intercon_test))) < 0) {
		return err;
	}
  #endif

	if((err = snd_soc_dapm_new_widgets(codec)) < 0) {
		return err;
	}

#elif defined(KERNEL_2_6_35_OMAP)
	if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_common, ARRAY_SIZE(mc_asoc_widgets_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
	}

	if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_common, ARRAY_SIZE(mc_asoc_intercon_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}

  #ifdef MC_ASOC_TEST
	if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_test, ARRAY_SIZE(mc_asoc_widgets_test))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_test, ARRAY_SIZE(mc_asoc_intercon_test))) < 0) {
		return err;
	}
  #endif

	if((err = snd_soc_dapm_new_widgets(codec->dapm)) < 0) {
		return err;
	}
#else
	if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_common, ARRAY_SIZE(mc_asoc_widgets_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
	}

	if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_common, ARRAY_SIZE(mc_asoc_intercon_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}

  #ifdef MC_ASOC_TEST
	if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_test, ARRAY_SIZE(mc_asoc_widgets_test))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_test, ARRAY_SIZE(mc_asoc_intercon_test))) < 0) {
		return err;
	}
  #endif

	if((err = snd_soc_dapm_new_widgets(&codec->dapm)) < 0) {
		return err;
	}
#endif
	return 0;
}

/*
 * Hwdep interface
 */
static int mc_asoc_hwdep_open(struct snd_hwdep * hw, struct file *file)
{
	/* Nothing to do */
	return 0;
}

static int mc_asoc_hwdep_release(struct snd_hwdep *hw, struct file *file)
{
	/* Nothing to do */
	return 0;
}

static int hwdep_ioctl_read_reg(MCDRV_REG_INFO *args)
{
	int		err;
	MCDRV_REG_INFO	reg_info;

	if (!access_ok(VERIFY_WRITE, args, sizeof(MCDRV_REG_INFO))) {
		return -EFAULT;
	}
	if (copy_from_user(&reg_info, args, sizeof(MCDRV_REG_INFO)) != 0) {
		return -EFAULT;
	}

	err	= _McDrv_Ctrl(MCDRV_READ_REG, &reg_info, 0);
	if (err != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	else {
		if (copy_to_user(args, &reg_info, sizeof(MCDRV_REG_INFO)) != 0) {
			err	= -EFAULT;
		}
	}

	return 0;
}

static int hwdep_ioctl_write_reg(const MCDRV_REG_INFO *args)
{
	int		err;
	MCDRV_REG_INFO	reg_info;

	if (!access_ok(VERIFY_READ, args, sizeof(MCDRV_REG_INFO))) {
		return -EFAULT;
	}

	if (copy_from_user(&reg_info, args, sizeof(MCDRV_REG_INFO)) != 0) {
		return -EFAULT;
	}

	err	= _McDrv_Ctrl(MCDRV_WRITE_REG, &reg_info, 0);
	if (err != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	return 0;
}

static int mc_asoc_hwdep_ioctl(
	struct snd_hwdep *hw,
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int		err	= 0;
	int		audio_mode;
	int		output_path;
	void	*param	= NULL;
	struct snd_soc_codec	*codec		= NULL;
	struct mc_asoc_data		*mc_asoc	= NULL;

	if(hw != NULL) {
		codec	= hw->private_data;
	}
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}

	switch (cmd) {
	case YMC_IOCTL_SET_CTRL:
		if (!access_ok(VERIFY_READ, (ymc_ctrl_args_t *)arg, sizeof(ymc_ctrl_args_t))) {
			return -EFAULT;
		}
		if (!(param	= kzalloc(((ymc_ctrl_args_t *)arg)->size, GFP_KERNEL))) {
			return -ENOMEM;
		}
		if (copy_from_user(param,
						   ((ymc_ctrl_args_t *)arg)->param,
						   ((ymc_ctrl_args_t *)arg)->size) != 0) {
			err	= -EFAULT;
			break;
		}
		err	= mc_asoc_parser(mc_asoc,
							 mc_asoc_hwid,
							 param,
							 ((ymc_ctrl_args_t *)arg)->size,
							 ((ymc_ctrl_args_t *)arg)->option);
		if(err == 0) {
			if(((((ymc_ctrl_args_t *)arg)->option&0xFF00) == YMC_DSP_VOICECALL_BASE_1MIC)
			|| ((((ymc_ctrl_args_t *)arg)->option&0xFF00) == YMC_DSP_VOICECALL_BASE_2MIC)) {
				if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
				|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
				|| (audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)) {
					if((err = set_audio_mode_play(codec, audio_mode)) < 0) {
						break;
					}
				}
			}
			else if((((ymc_ctrl_args_t *)arg)->option&0xFF00) != 0) {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((err = set_cdsp_param(codec)) < 0) {
						break;
					}
				}
			}
			if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
				if(ae_onoff(mc_asoc->ae_onoff, output_path) == MC_ASOC_AENG_OFF) {
					break;
				}
			}
			else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
				if((mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_EXT] == MC_ASOC_AENG_OFF)
				|| ((mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_EXT] == 0x80)
					&& (mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_BASE] == MC_ASOC_AENG_OFF))) {
					break;
				}
			}
			if((((ymc_ctrl_args_t *)arg)->option == 0)
			|| ((((ymc_ctrl_args_t *)arg)->option&0xFF) != 0)) {
				if((err = set_audioengine(codec)) < 0) {
					break;
				}
				if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
					if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
						if((err = set_audioengine_ex(codec)) < 0) {
							break;
						}
					}
				}
			}
		}
		break;

	case YMC_IOCTL_READ_REG:
		err	= hwdep_ioctl_read_reg((MCDRV_REG_INFO*)arg);
		dbg_info("err=%d, RegType=%d, Addr=%d, Data=0x%02X\n",
				err,
				((MCDRV_REG_INFO*)arg)->bRegType,
				((MCDRV_REG_INFO*)arg)->bAddress,
				((MCDRV_REG_INFO*)arg)->bData);
		break;

	case YMC_IOCTL_WRITE_REG:
		err	= hwdep_ioctl_write_reg((MCDRV_REG_INFO*)arg);
		dbg_info("err=%d, RegType=%d, Addr=%d, Data=0x%02X\n",
				err,
				((MCDRV_REG_INFO*)arg)->bRegType,
				((MCDRV_REG_INFO*)arg)->bAddress,
				((MCDRV_REG_INFO*)arg)->bData);
		break;

	case YMC_IOCTL_NOTIFY_HOLD:
		dbg_info("arg=%ld\n", *(UINT32*)arg);
		switch(*(UINT32*)arg) {
		case	YMC_NOTITY_HOLD_OFF:
			err	= connect_path(codec);
		case	YMC_NOTITY_HOLD_ON:
			mc_asoc_hold	= (UINT8)*(UINT32*)arg;
			break;
		default:
			err	= -EINVAL;
			break;
		}
		break;
	default:
		err	= -EINVAL;
	}

	if(param != NULL) {
		kfree(param);
	}
	return err;
}

static int mc_asoc_add_hwdep(struct snd_soc_codec *codec)
{
	struct snd_hwdep	*hw;
	struct mc_asoc_data	*mc_asoc	= NULL;
	int		err;

	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	err	= snd_hwdep_new(codec->card, MC_ASOC_HWDEP_ID, 0, &hw);
#else
	err	= snd_hwdep_new((struct snd_card *)codec->card->snd_card, 
						MC_ASOC_HWDEP_ID, 0, &hw);
#endif
	if (err < 0) {
		return err;
	}

#if 0 /* FUJITSU:2011-11-07 Provisional work*/
	hw->iface			= SNDRV_HWDEP_IFACE_MC_YAMAHA;
#endif /* FUJITSU:2011-11-07 Provisional work */
	hw->private_data	= codec;
	hw->ops.open		= mc_asoc_hwdep_open;
	hw->ops.release		= mc_asoc_hwdep_release;
	hw->ops.ioctl		= mc_asoc_hwdep_ioctl;
	hw->exclusive		= 1;
	strcpy(hw->name, MC_ASOC_HWDEP_ID);
	mc_asoc->hwdep		= hw;

	return 0;
}

/*
 * HW_ID
 */
static int mc_asoc_i2c_detect(
	struct i2c_client *client_a,
	struct i2c_client *client_d)
{
	UINT8	bHwid	= mc_asoc_i2c_read_byte(client_a, 8);

	if (bHwid == MCDRV_HWID_14H) {
		bHwid	= mc_asoc_i2c_read_byte(client_d, 8);
		if (bHwid != MC_ASOC_MC3_HW_ID && bHwid != MC_ASOC_MA2_HW_ID) {
			return -ENODEV;
		}
	}
	else {
		if (bHwid != MC_ASOC_MC1_HW_ID && bHwid != MC_ASOC_MA1_HW_ID) {
			return -ENODEV;
		}
	}
	mc_asoc_hwid	= bHwid;

	return 0;
}

/* FUJITSU:2011-11-07 Provisional work start */
static MCDRV_PATH_INFO stPathInfo_Default;
static MCDRV_VOL_INFO  stVolInfo_Default;
/* FUJITSU:2011-11-07 Provisional work end */

/*
 * Codec device
 */
static int mc_asoc_probe(
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct platform_device *pdev
#else
	struct snd_soc_codec *codec
#endif
)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_device	*socdev		= platform_get_drvdata(pdev);
	struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();
#endif
	struct mc_asoc_data		*mc_asoc	= NULL;
	struct device			*dev		= NULL;
	struct mc_asoc_setup	*setup		= &mc_asoc_cfg_setup;
	int		err;
	UINT32	update	= 0;

	TRACE_FUNC();
 if(0){ /* FUJITSU:2011-11-07 Provisional work */
	if (codec == NULL) {
/*		printk(KERN_ERR "I2C bus is not probed successfully\n");*/
		err	= -ENODEV;
		goto error_codec_data;
	}

	mc_asoc	= mc_asoc_get_mc_asoc(codec);
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if(socdev != NULL) {
		dev	= socdev->dev;
	}
#else
	dev	= codec->dev;
#endif
	if (mc_asoc == NULL || dev == NULL) {
		err	= -ENODEV;
		goto error_codec_data;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	socdev->codec_data	= setup;
	socdev->card->codec	= codec;
#endif
 } /* FUJITSU:2011-11-07 Provisional work */
	/* init hardware */
#if 0 /* FUJITSU:2011-11-07 Provisional work */
//	memcpy(&mc_asoc->setup, setup, sizeof(struct mc_asoc_setup));
//	err	= _McDrv_Ctrl(MCDRV_INIT, &mc_asoc->setup.init, 0);
#else /* FUJITSU:2011-11-07 Provisional work start */
	err	= _McDrv_Ctrl(MCDRV_INIT, &setup->init, 0);
#endif/* FUJITSU:2011-11-07 Provisional work end */
	if (err != MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_INIT\n", err);
		err	= -EIO;
		goto error_init_hw;
	}

	/* HW_ID */
	if ((err = mc_asoc_i2c_detect(mc_asoc_i2c_a, mc_asoc_i2c_d)) < 0) {
		dev_err(dev, "err=%d: failed to MC_ASOC HW_ID\n", err);
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
		kfree(mc_asoc);
		kfree(codec);
#endif
		return err;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	/* pcm */
	err	= snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (err < 0) {
		dev_err(dev, "%d: Error in snd_soc_new_pcms\n", err);
		goto error_new_pcm;
	}
#endif

 if(0){ /* FUJITSU:2011-11-07 Provisional work */
	/* controls */
	err	= mc_asoc_add_controls(codec, mc_asoc_snd_controls,
				ARRAY_SIZE(mc_asoc_snd_controls));
	if (err < 0) {
		dev_err(dev, "%d: Error in mc_asoc_add_controls\n", err);
		goto error_add_ctl;
	}

	err	= mc_asoc_add_widgets(codec);
	if (err < 0) {
		dev_err(dev, "%d: Error in mc_asoc_add_widgets\n", err);
		goto error_add_ctl;
	}

	/* hwdep */
	err	= mc_asoc_add_hwdep(codec);
	if (err < 0) {
		dev_err(dev, "%d: Error in mc_asoc_add_hwdep\n", err);
		goto error_add_hwdep;
	}

	write_cache(codec, MC_ASOC_INCALL_MIC_SP, INCALL_MIC_SP);
	write_cache(codec, MC_ASOC_INCALL_MIC_RC, INCALL_MIC_RC);
	write_cache(codec, MC_ASOC_INCALL_MIC_HP, INCALL_MIC_HP);
	write_cache(codec, MC_ASOC_INCALL_MIC_LO1, INCALL_MIC_LO1);
	write_cache(codec, MC_ASOC_INCALL_MIC_LO2, INCALL_MIC_LO2);
 } /* FUJITSU:2011-11-07 Provisional work */

#ifdef MC_ASOC_TEST
	write_cache(codec, MC_ASOC_MAIN_MIC, mc_asoc_main_mic);
	write_cache(codec, MC_ASOC_SUB_MIC, mc_asoc_sub_mic);
	write_cache(codec, MC_ASOC_HS_MIC, mc_asoc_hs_mic);
	write_cache(codec, MC_ASOC_AP_PLAY_PORT, mc_asoc_ap_play_port);
	write_cache(codec, MC_ASOC_AP_CAP_PORT, mc_asoc_ap_cap_port);
	write_cache(codec, MC_ASOC_CP_PORT, mc_asoc_cp_port);
	write_cache(codec, MC_ASOC_BT_PORT, mc_asoc_bt_port);
	if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
		write_cache(codec, MC_ASOC_HF, mc_asoc_hf);
	}
	write_cache(codec, MC_ASOC_MIC1_BIAS, mc_asoc_mic1_bias);
	write_cache(codec, MC_ASOC_MIC2_BIAS, mc_asoc_mic2_bias);
	write_cache(codec, MC_ASOC_MIC3_BIAS, mc_asoc_mic3_bias);
	write_cache(codec, MC_ASOC_SP, mc_asoc_sp);
	write_cache(codec, MC_ASOC_AE_SRC_PLAY, mc_asoc_ae_src_play);
	write_cache(codec, MC_ASOC_AE_SRC_CAP, mc_asoc_ae_src_cap_def);
	write_cache(codec, MC_ASOC_AE_PRIORITY, mc_asoc_ae_priority);
	write_cache(codec, MC_ASOC_CDSP_PRIORITY, mc_asoc_cdsp_priority);
	write_cache(codec, MC_ASOC_SIDETONE, mc_asoc_sidetone);
#endif

	if((get_ap_play_port() != DIO_0) && (get_ap_cap_port() != DIO_0)) {
		update |= (MCDRV_DIO0_COM_UPDATE_FLAG | MCDRV_DIO0_DIR_UPDATE_FLAG | MCDRV_DIO0_DIT_UPDATE_FLAG);
	}
	if((get_ap_play_port() != DIO_1) && (get_ap_cap_port() != DIO_1)) {
		update |= (MCDRV_DIO1_COM_UPDATE_FLAG | MCDRV_DIO1_DIR_UPDATE_FLAG | MCDRV_DIO1_DIT_UPDATE_FLAG);
	}
	if((get_ap_play_port() != DIO_2) && (get_ap_cap_port() != DIO_2)) {
		update |= (MCDRV_DIO2_COM_UPDATE_FLAG | MCDRV_DIO2_DIR_UPDATE_FLAG | MCDRV_DIO2_DIT_UPDATE_FLAG);
	}
	err	= _McDrv_Ctrl(MCDRV_SET_DIGITALIO, (void *)&stDioInfo_Default, update);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_DIGITALIO\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&stDacInfo_Default, 0x7);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_DAC\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_ADC, (void *)&stAdcInfo_Default, 0x7);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_ADC\n", err);
		goto error_set_mode;
	}

	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID || mc_asoc_hwid == MC_ASOC_MA2_HW_ID) {
		err = _McDrv_Ctrl(MCDRV_SET_ADC_EX, (void *)&stAdcExInfo_Default, 0x3F);
		if (err < 0) {
			dev_err(dev, "%d: Error in MCDRV_SET_ADC_EX\n", err);
			goto error_set_mode;
		}
	}

	err	= _McDrv_Ctrl(MCDRV_SET_SP, (void *)&stSpInfo_Default, 0);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_SP\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_DNG, (void *)&stDngInfo_Default, 0x7F3F3F);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_DNG\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_SYSEQ, (void *)&stSyseqInfo_Default, 0x3);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_SYSEQ\n", err);
		goto error_set_mode;
	}

	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID || mc_asoc_hwid == MC_ASOC_MA2_HW_ID) {
		err	= _McDrv_Ctrl(MCDRV_SET_DTMF, (void *)&stDtmfInfo_Default, 0xff);
		if (err < MCDRV_SUCCESS) {
			dev_err(dev, "%d: Error in MCDRV_SET_DTMF\n", err);
			goto error_set_mode;
		}
		else {
			memcpy(&mc_asoc->dtmf_store, &stDtmfInfo_Default, sizeof(MCDRV_DTMF_PARAM));
		}

		err	= _McDrv_Ctrl(MCDRV_SET_DITSWAP, (void *)&stDitswapInfo_Default, 0x3);
		if (err < MCDRV_SUCCESS) {
			dev_err(dev, "%d: Error in MCDRV_SET_DITSWAP\n", err);
			goto error_set_mode;
		}
	}

 if(0){ /* FUJITSU:2011-11-07 Provisional work */
	if((err = connect_path(codec)) < 0) {
		dev_err(dev, "%d: Error in coeenct_path\n", err);
		goto error_set_mode;
	}
 } /* FUJITSU:2011-11-07 Provisional work */

/* FUJITSU:2011-11-07 Provisional work start */
	memset(&stPathInfo_Default,0,sizeof(stPathInfo_Default));

	stPathInfo_Default.asSpOut[0].abSrcOnOff[1] = MCDRV_SRC1_LINE1_L_ON;
	stPathInfo_Default.asSpOut[1].abSrcOnOff[1] = MCDRV_SRC1_LINE1_R_ON;

	stPathInfo_Default.asLout1[0].abSrcOnOff[0] = MCDRV_SRC0_MIC1_ON;
	stPathInfo_Default.asLout1[1].abSrcOnOff[0] = MCDRV_SRC0_MIC1_ON;

	err	= _McDrv_Ctrl(MCDRV_SET_PATH, (void *)&stPathInfo_Default, 0);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_PATH\n", err);
		goto error_set_mode;
	}

	memset(&stVolInfo_Default,0,sizeof(stVolInfo_Default));

	stVolInfo_Default.aswA_Lin1[0] = 0 | MCDRV_VOL_UPDATE;
	stVolInfo_Default.aswA_Lin1[1] = 0 | MCDRV_VOL_UPDATE;
	stVolInfo_Default.aswA_Sp[0] = 0 | MCDRV_VOL_UPDATE;
	stVolInfo_Default.aswA_Sp[1] = 0 | MCDRV_VOL_UPDATE;

	stVolInfo_Default.aswA_Mic1[0] = 0 | MCDRV_VOL_UPDATE;
	stVolInfo_Default.aswA_Mic1Gain[0] = (15 << 8) | MCDRV_VOL_UPDATE;
	stVolInfo_Default.aswA_Lout1[0] = 0 | MCDRV_VOL_UPDATE;
	stVolInfo_Default.aswA_Lout1[1] = 0 | MCDRV_VOL_UPDATE;

	err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, (void *)&stVolInfo_Default, 0);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_VOLUME\n", err);
		goto error_set_mode;
	}
/* FUJITSU:2011-11-07 Provisional work end */
	reset_volcache(VOLGRP_ALL);
	return 0;

error_set_mode:
error_add_hwdep:
error_add_ctl:
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	snd_soc_free_pcms(socdev);
error_new_pcm:
#endif
	_McDrv_Ctrl(MCDRV_TERM, NULL, 0);
error_init_hw:
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	socdev->card->codec	= NULL;
#endif
error_codec_data:
	return err;
}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
static int mc_asoc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev	= platform_get_drvdata(pdev);
	int err;

	TRACE_FUNC();

	if (socdev->card->codec)
	{
		snd_soc_free_pcms(socdev);

		err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(socdev->dev, "%d: Error in MCDRV_TERM\n", err);
			return -EIO;
		}
	}

	return 0;
}
#else
static int mc_asoc_remove(struct snd_soc_codec *codec)
{
	int err;

	TRACE_FUNC();

	if (codec) {
		err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in MCDRV_TERM\n", err);
			return -EIO;
		}
	}
	return 0;
}
#endif

static int mc_asoc_suspend(
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct platform_device *pdev
#else
	struct snd_soc_codec *codec
#endif
)/* pm_message_t state *//* FUJITSU:2012-07-19 1120to1311 DEL */
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_device	*socdev		= platform_get_drvdata(pdev);
	struct snd_soc_codec	*codec		= NULL;
#endif
	struct mc_asoc_data		*mc_asoc	= NULL;
	int		err, i;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if(socdev != NULL) {
		codec	= socdev->card->codec;
	}
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	mutex_lock(&mc_asoc->mutex);

	if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		for (i	= 0; i < MC_ASOC_N_INFO_STORE; i++) {
			if((mc_asoc_info_store_tbl[i].set == MCDRV_SET_DTMF)
			|| (mc_asoc_info_store_tbl[i].set == MCDRV_SET_DITSWAP)
			|| (mc_asoc_info_store_tbl[i].set == MCDRV_SET_ADC_EX)
			|| (mc_asoc_info_store_tbl[i].set == MCDRV_SET_PDM_EX)) {
				mc_asoc_info_store_tbl[i].get	= 0;
				mc_asoc_info_store_tbl[i].set	= 0;
			}
		}
	}

	/* store parameters */
	for (i	= 0; i < MC_ASOC_N_INFO_STORE; i++) {
		struct mc_asoc_info_store	*store	= &mc_asoc_info_store_tbl[i];
		if (store->get) {
			err	= _McDrv_Ctrl(store->get, (void *)mc_asoc + store->offset, 0);
			if (err != MCDRV_SUCCESS) {
				dev_err(codec->dev, "%d: Error in mc_asoc_suspend\n", err);
				err	= -EIO;
				goto error;
			}
			else {
				err	= 0;
			}
		}
	}

	err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in MCDRV_TERM\n", err);
		err	= -EIO;
	}
	else {
		err	= 0;
	}

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static int mc_asoc_resume(
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct platform_device *pdev
#else
	struct snd_soc_codec *codec
#endif
)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_device	*socdev		= platform_get_drvdata(pdev);
	struct snd_soc_codec	*codec		= NULL;
#endif
	struct mc_asoc_data		*mc_asoc	= NULL;
	SINT16	*vol	= NULL;
	int		err, i;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if(socdev != NULL) {
		codec	= socdev->card->codec;
	}
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	vol	= (SINT16 *)&mc_asoc->vol_store;

	mutex_lock(&mc_asoc->mutex);

	err	= _McDrv_Ctrl(MCDRV_INIT, &mc_asoc->setup.init, 0);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in MCDRV_INIT\n", err);
		err	= -EIO;
		goto error;
	}
	else {
		err	= 0;
	}

	/* restore parameters */
	for (i	= 0; i < sizeof(MCDRV_VOL_INFO)/sizeof(SINT16); i++, vol++) {
		*vol |= 0x0001;
	}

	/* When pvPrm is "NULL" ,dPrm is "0" */
	if (mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
		err	= _McDrv_Ctrl(MCDRV_SET_CDSP, mc_asoc->cdsp_store.prog,
							mc_asoc->cdsp_store.size);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
			err	= -EIO;
			goto error;
		}
		for (i	= 0; i < mc_asoc->cdsp_store.param_count; i++) {
			err	= _McDrv_Ctrl(MCDRV_SET_CDSP_PARAM, 
								&(mc_asoc->cdsp_store.cdspparam[i]), 0);
			if (err != MCDRV_SUCCESS) {
				dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
				err	= -EIO;
				goto error;
			}
		}
	}
	else if (mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
		err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, mc_asoc->ae_ex_store.prog,
							mc_asoc->ae_ex_store.size);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
			err	= -EIO;
			goto error;
		}
		for (i	= 0; i < mc_asoc->ae_ex_store.param_count; i++) {
			err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, 
							&(mc_asoc->ae_ex_store.exparam[i]), 0);
			if (err != MCDRV_SUCCESS) {
				dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
				err	= -EIO;
				goto error;
			}
		}
	}

	for (i	= 0; i < MC_ASOC_N_INFO_STORE; i++) {
		struct mc_asoc_info_store	*store	= &mc_asoc_info_store_tbl[i];
		if (store->set) {
			err	= _McDrv_Ctrl(store->set, (void *)mc_asoc + store->offset,
						store->flags);
			if (err != MCDRV_SUCCESS) {
				dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
				err	= -EIO;
				goto error;
			}
			else {
				err	= 0;
			}
		}
	}

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
struct snd_soc_codec_device	mc_asoc_codec_dev	= {
	.probe			= mc_asoc_probe,
	.remove			= mc_asoc_remove,
	.suspend		= mc_asoc_suspend,
	.resume			= mc_asoc_resume
};
EXPORT_SYMBOL_GPL(mc_asoc_codec_dev);
#else
struct snd_soc_codec_driver	mc_asoc_codec_dev	= {
	.probe			= mc_asoc_probe,
	.remove			= mc_asoc_remove,
	.suspend		= mc_asoc_suspend,
	.resume			= mc_asoc_resume,
	.read			= mc_asoc_read_reg,
	.write			= mc_asoc_write_reg,
	.reg_cache_size	= MC_ASOC_N_REG,
	.reg_word_size	= sizeof(u16),
	.reg_cache_step	= 1
};
#endif

/*
 * I2C client
 */
static int mc_asoc_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_codec	*codec;
#else
	struct mc_asoc_priv		*mc_asoc_priv;
#endif
	int		i, j;
	struct mc_asoc_data		*mc_asoc;
	int		err;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	/* setup codec data */
	if (!(codec	= kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL))) {
		err	= -ENOMEM;
		goto err_alloc_codec;
	}
	codec->name		= MC_ASOC_NAME;
/*	codec->owner	= THIS_MODULE;*/
	mutex_init(&codec->mutex);
	codec->dev	= &client->dev;

	if (!(mc_asoc	= kzalloc(sizeof(struct mc_asoc_data), GFP_KERNEL))) {
		err	= -ENOMEM;
		goto err_alloc_data;
	}
  #ifdef ALSA_VER_1_0_23
	codec->private_data	= mc_asoc;
  #else
	snd_soc_codec_set_drvdata(codec, mc_asoc);
  #endif
	mutex_init(&mc_asoc->mutex);

	/* setup i2c client data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err	= -ENODEV;
		goto err_i2c;
	}
	i2c_set_clientdata(client, codec);

	codec->control_data	= client;
	codec->read			= mc_asoc_read_reg;
	codec->write		= mc_asoc_write_reg;
	codec->hw_write		= NULL;
	codec->hw_read		= NULL;
	codec->reg_cache	= kzalloc(sizeof(u16) * MC_ASOC_N_REG, GFP_KERNEL);
	if (codec->reg_cache == NULL) {
		err	= -ENOMEM;
		goto err_alloc_cache;
	}
	codec->reg_cache_size	= MC_ASOC_N_REG;
	codec->reg_cache_step	= 1;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	codec->dai		= mc_asoc_dai;
	codec->num_dai	= ARRAY_SIZE(mc_asoc_dai);
	mc_asoc_set_codec_data(codec);
	if ((err = snd_soc_register_codec(codec)) < 0) {
		goto err_reg_codec;
	}

	/* setup DAI data */
	for (i	= 0; i < ARRAY_SIZE(mc_asoc_dai); i++) {
		mc_asoc_dai[i].dev	= &client->dev;
	}
	if ((err = snd_soc_register_dais(mc_asoc_dai, ARRAY_SIZE(mc_asoc_dai))) < 0) {
		goto err_reg_dai;
	}
#else
	if (!(mc_asoc_priv	= kzalloc(sizeof(struct mc_asoc_priv), GFP_KERNEL))) {
		err	= -ENOMEM;
		goto err_alloc_priv;
	}
	mc_asoc	= &mc_asoc_priv->data;
	mutex_init(&mc_asoc->mutex);
	i2c_set_clientdata(client, mc_asoc_priv);
	if(0) /* FUJITSU:2011-11-07 Provisional : SKIP */
	if ((err = snd_soc_register_codec(&client->dev, &mc_asoc_codec_dev,
									mc_asoc_dai, ARRAY_SIZE(mc_asoc_dai))) < 0) {
		goto err_reg_codec;
	}
#endif

	mc_asoc_i2c_a	= client;

	for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
		for(j = 0; j < MC_ASOC_N_DSP_BLOCK; j++) {
			mc_asoc->block_onoff[i][j]	= MC_ASOC_DSP_BLOCK_THRU;
		}
		mc_asoc->dsp_onoff[i]	= 0x8000;
		mc_asoc->ae_onoff[i]	= 0x80;
	}
	mc_asoc->dsp_onoff[0]	= 0xF;
	mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE]	= 0xF;
	mc_asoc->ae_onoff[0]	= 1;
	mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_BASE]	= 1;

	memset(&mc_asoc_vol_info_mute, 0, sizeof(MCDRV_VOL_INFO));
	if(mc_asoc_ae_priority == AE_PRIORITY_PLAY) {
		mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
	}
	else if(mc_asoc_ae_priority == AE_PRIORITY_CAP) {
		mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
	}
	if(mc_asoc_cdsp_priority == CDSP_PRIORITY_PLAY) {
		mc_asoc_cdsp_dir	= MC_ASOC_CDSP_PLAY;
	}
	else if(mc_asoc_cdsp_priority == CDSP_PRIORITY_CAP) {
		mc_asoc_cdsp_dir	= MC_ASOC_CDSP_CAP;
	}
	if((mc_asoc_hwid == MC_ASOC_MC1_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA1_HW_ID)) {
		mc_asoc_hf	= HF_OFF;
	}
	return 0;

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
err_reg_dai:
	snd_soc_unregister_codec(codec);
#else
	snd_soc_unregister_codec(&client->dev);
#endif

err_reg_codec:
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	kfree(codec->reg_cache);
err_alloc_cache:
#endif
	i2c_set_clientdata(client, NULL);

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
err_i2c:
	kfree(mc_asoc);
err_alloc_data:
	kfree(codec);
err_alloc_codec:
#else
err_alloc_priv:
	kfree(mc_asoc_priv);
#endif
	dev_err(&client->dev, "err=%d: failed to probe MC_ASOC\n", err);
	return err;
}

static int mc_asoc_i2c_remove(struct i2c_client *client)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_codec *codec	= i2c_get_clientdata(client);
#endif
	struct mc_asoc_data *mc_asoc;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if (codec) {
		mc_asoc	= mc_asoc_get_mc_asoc(codec);
		snd_soc_unregister_dais(mc_asoc_dai, ARRAY_SIZE(mc_asoc_dai));
		snd_soc_unregister_codec(codec);
		mutex_destroy(&mc_asoc->mutex);
		kfree(mc_asoc);
		mutex_destroy(&codec->mutex);
		kfree(codec);
	}
#else
	mc_asoc	= &((struct mc_asoc_priv*)(i2c_get_clientdata(client)))->data;
	mutex_destroy(&mc_asoc->mutex);
	snd_soc_unregister_codec(&client->dev);
#endif

	return 0;
}
	
static int mc_asoc_i2c_probe_d(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	mc_asoc_i2c_d	= client;
	return 0;
}

static int mc_asoc_i2c_remove_d(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id	mc_asoc_i2c_id_a[] = {
	{"mc_asoc_a", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mc_asoc_i2c_id_a);

static const struct i2c_device_id	mc_asoc_i2c_id_d[] = {
	{"mc_asoc_d", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mc_asoc_i2c_id_d);

static struct i2c_driver	mc_asoc_i2c_driver_a = {
	.driver	= {
		.name	= "mc_asoc_a",
		.owner	= THIS_MODULE,
	},
	.probe		= mc_asoc_i2c_probe,
	.remove		= mc_asoc_i2c_remove,
	.id_table	= mc_asoc_i2c_id_a,
};

static struct i2c_driver	mc_asoc_i2c_driver_d = {
	.driver	= {
		.name	= "mc_asoc_d",
		.owner	= THIS_MODULE,
	},
	.probe		= mc_asoc_i2c_probe_d,
	.remove		= mc_asoc_i2c_remove_d,
	.id_table	= mc_asoc_i2c_id_d,
};

/*
 * Module init and exit
 */
static int __init mc_asoc_init(void)
{
	int		err	= 0;

	err	= i2c_add_driver(&mc_asoc_i2c_driver_a);
	if (err >= 0) {
		err	= i2c_add_driver(&mc_asoc_i2c_driver_d);
	}

/* QNET Audio:2012-05-24 YAMAHA MUTE start */
	INIT_WORK( &workq, mute_func );
/* QNET Audio:2012-05-24 YAMAHA MUTE end */
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED start */
	INIT_WORK( &disdev_workq, disdev_func );
/* QNET Audio:2012-07-04 YAMAHA STREAM_ENFORCED end */

/* QNET Audio:2012-02-09 Clock Supply start */
/*	xo_cxo = msm_xo_get(MSM_XO_TCXO_D1, "audio_mach1_clk"); FUJITSU:2012-5-15 del */
	xo_cxo = msm_xo_get(MSM_XO_TCXO_A1, "audio_mach1_clk"); /* FUJITSU:2012-5-15 nab audio */
	if (IS_ERR(xo_cxo)) {
		pr_err("%s: msm_xo_get(CXO) failed.\n", __func__);
		err =  -1;
	}
/* QNET Audio:2012-02-09 Clock Supply end */

	return err;
}
module_init(mc_asoc_init);

static void __exit mc_asoc_exit(void)
{
	i2c_del_driver(&mc_asoc_i2c_driver_a);
	i2c_del_driver(&mc_asoc_i2c_driver_d);
}
module_exit(mc_asoc_exit);

MODULE_AUTHOR("Yamaha Corporation");
MODULE_DESCRIPTION("Yamaha MC ALSA SoC codec driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MC_ASOC_DRIVER_VERSION);
