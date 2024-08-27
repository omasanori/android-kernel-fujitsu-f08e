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
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/

#ifndef MC_ASOC_CFG_H
#define MC_ASOC_CFG_H

#include "mcdriver.h"
#include "mc_asoc_priv.h"

/*
 * ALSA Version
 */
/*#define	ALSA_VER_1_0_23*/
/*#define	KERNEL_2_6_35_36_COMMON*/
/*#define	KERNEL_2_6_35_OMAP*/
#define KERNEL_3_0_COMMON /* FUJITSU:2011-11-07 change */

#define INCALL_MIC_SP		MC_ASOC_INCALL_MIC_MAINMIC
#define INCALL_MIC_RC		MC_ASOC_INCALL_MIC_MAINMIC
#define INCALL_MIC_HP		MC_ASOC_INCALL_MIC_MAINMIC
#define INCALL_MIC_LO1		MC_ASOC_INCALL_MIC_MAINMIC
#define INCALL_MIC_LO2		MC_ASOC_INCALL_MIC_MAINMIC

#define	MIC_NONE			(0)
#define	MIC_1				(1)
#define	MIC_2				(2)
#define	MIC_3				(3)
#define	MIC_PDM				(4)

#define	MAIN_MIC			MIC_1
#define	SUB_MIC				MIC_3
#define	HEADSET_MIC			MIC_2

#define	DIO_0				(0)
#define	DIO_1				(1)
#define	DIO_2				(2)
#define	LIN1				(3)
#define	LIN2				(4)
#define	LOUT1				(3)
#define	LOUT2				(4)
#define	LIN1_LOUT1			(3)
#define	LIN2_LOUT1			(4)
#define	LIN1_LOUT2			(5)
#define	LIN2_LOUT2			(6)

#define	AP_PLAY_PORT		DIO_0
#define	AP_CAP_PORT			DIO_0
#define	CP_PORT				DIO_1
#define	BT_PORT				DIO_2

#define	HF_OFF				(0)
#define	HF_ON				(1)
#define	HANDSFREE			HF_ON

#define	BIAS_OFF			(0)
#define	BIAS_ON_ALWAYS		(1)
#define	BIAS_SYNC_MIC		(2)
#define	MIC1_BIAS			BIAS_SYNC_MIC
#define	MIC2_BIAS			BIAS_SYNC_MIC
#define	MIC3_BIAS			BIAS_SYNC_MIC

#define	SP_STEREO			(0)
#define	SP_MONO_L			(1)
#define	SP_MONO_R			(2)
#define	SP_TYPE				SP_STEREO

#define	AE_SRC_PLAY_NONE	(0)
#define	AE_SRC_PLAY_DIR0	(1)
#define	AE_SRC_PLAY_DIR1	(2)
#define	AE_SRC_PLAY_DIR2	(3)
#define	AE_SRC_PLAY_MIX		(4)
#define	AE_SRC_PLAY_PDM		(5)
#define	AE_SRC_PLAY_ADC0	(6)
#define	AE_SRC_PLAY_DTMF	(7)
#define	AE_SRC_PLAY_CDSP	(8)
#define	AE_SRC_PLAY			AE_SRC_PLAY_MIX

#define	AE_SRC_CAP_NONE		(0)
#define	AE_SRC_CAP_PDM		(1)
#define	AE_SRC_CAP_ADC0		(2)
#define	AE_SRC_CAP_ADC1		(3)
#define	AE_SRC_CAP_DIR0		(4)
#define	AE_SRC_CAP_DIR1		(5)
#define	AE_SRC_CAP_DIR2		(6)
#define	AE_SRC_CAP_INPUT_PATH	(7)
#define	AE_SRC_CAP			AE_SRC_CAP_NONE

#define	AE_PRIORITY_1ST		(0)
#define	AE_PRIORITY_LAST	(1)
#define	AE_PRIORITY_PLAY	(2)
#define	AE_PRIORITY_CAP		(3)
#define	AE_PRIORITY			AE_PRIORITY_1ST

#define	CDSP_PRIORITY_1ST	(0)
#define	CDSP_PRIORITY_LAST	(1)
#define	CDSP_PRIORITY_PLAY	(2)
#define	CDSP_PRIORITY_CAP	(3)
#define	CDSP_PRIORITY		CDSP_PRIORITY_1ST

#define	SIDETONE_OFF		(0)
#define	SIDETONE_ANALOG		(1)
#define	SIDETONE_DIGITAL	(2)
#define	SIDETONE			SIDETONE_OFF


static struct mc_asoc_setup mc_asoc_cfg_setup = {
	{
/* QNET Audio:2012-01-17 Basic Feat start */
		MCDRV_CKSEL_CMOS,	// bCkSel
		25,					// bDivR0 + in case of 19.2MHz
		96,					// bDivF0 +
/* QNET Audio:2012-01-17 Basic Feat   end */
		2,					/*	bDivR1	*/
		72,					/*	bDivF1	*/
		0,					/*	bRange0 (unused)	*/
		0,					/*	bRange1 (unused)	*/
		0,					/*	bBypass (unused)	*/
		0,					/*	bDioSdo0Hiz	*/
		0,					/*	bDioSdo1Hiz	*/
		0,					/*	bDioSdo2Hiz	*/
		0,					/*	bDioClk0Hiz	*/
		MCDRV_DAHIZ_HIZ,	/*	bDioClk1Hiz	*/
		0,					/*	bDioClk2Hiz	*/
		0,					/*	bPcmHiz	*/
		0,					/*	bLineIn1Dif	*/
		0,					/*	bLineIn2Dif	*/
		MCDRV_LINE_DIF,		/*	bLineOut1Dif	*/
		0,					/*	bLineOut2Dif	*/
		0,					/*	bSpmn	*/
		0,					/*	bMic1Sng	*/
		0,					/*	bMic2Sng	*/
		0,					/*	bMic3Sng	*/
		MCDRV_POWMODE_FULL,	/*	bPowerMode	*/
		0,					/*	bSpHiz	*/
		MCDRV_LDO_AON_DON,	/*	bLdo	*/
		0,					/*	bPad0Func	*/
		0,					/*	bPad1Func	*/
		0,					/*	bPad2Func	*/
		MCDRV_OUTLEV_4,		/*	bAvddLev	*/
		0,					/*	bVrefLev	*/
		MCDRV_DCLGAIN_12,	/*	bDclGain	*/
		0,					/*	bDclLimit	*/
		0,					/*	bCpMod	*/
		0,					/*	bSdDs	*/
		0,					/*	bReserved1	*/
		0,					/*	bReserved2	*/
		0,					/*	bReserved3	*/
		0,					/*	bReserved4	*/
		{
			0,				/*	dAdHpf (unused)	*/
			5000,			/*	dMic1Cin	*/
			5000,			/*	dMic2Cin	*/
			5000,			/*	dMic3Cin	*/
			25000,			/*	dLine1Cin	*/
			25000,			/*	dLine2Cin	*/
			5000,			/*	dVrefRdy1	*/
			15000,			/*	dVrefRdy2	*/
			9000,			/*	dHpRdy	*/
			13000,			/*	dSpRdy	*/
			0,				/*	dPdm	*/
			1000,			/*	dAnaRdyInterval	*/
			1000,			/*	dSvolInterval	*/
			25,				/*	dAnaRdyTimeOut	*/
			25,				/*	dSvolTimeOut	*/
		},
	},
	/*	pcm_extend	*/
	{
	},
	/*	pcm_hiz_redge	*/
	{
	},
	/*	pcm_hperiod	*/
	{
	},
	/*	slot	*/
	{
		{{0, 1}, {0, 1}},
		{{0, 1}, {0, 1}},
		{{0, 1}, {0, 1}},
	},
};

static const MCDRV_DIO_INFO stDioInfo_Default = {
	{
		/*	DIO port 0	*/
		{
			/*	sDioCommon	*/
			{
				/*	bMasterSlave : Master / Slave Setting	*/
				/*	 MCDRV_DIO_SLAVE	(0)	: Slave		*/
				/*	 MCDRV_DIO_MASTER	(1)	: Master	*/
				MCDRV_DIO_MASTER,
				/*	bAutoFs : Sampling frequency automatic measurement ON/OFF Setting in slave mode	*/
				/*	 MCDRV_AUTOFS_OFF	(0)	: OFF	*/
				/*	 MCDRV_AUTOFS_ON	(1)	: ON	*/
				MCDRV_AUTOFS_ON ,
				/*	bFs : Sampling Rate Setting	*/
				/*	 MCDRV_FS_48000	(0)		: 48kHz		*/
				/*	 MCDRV_FS_44100	(1)		: 44.1kHz	*/
				/*	 MCDRV_FS_32000	(2)		: 32kHz		*/
				/*	 MCDRV_FS_24000	(4)		: 24kHz		*/
				/*	 MCDRV_FS_22050	(5)		: 22.05kHz	*/
				/*	 MCDRV_FS_16000	(6)		: 16kHz		*/
				/*	 MCDRV_FS_12000	(8)		: 12kHz		*/
				/*	 MCDRV_FS_11025	(9)		: 11.025kHz	*/
				/*	 MCDRV_FS_8000	(10)	: 8kHz		*/
				MCDRV_FS_44100,
				/*	bBckFs : Bit Clock Frequency Setting	*/
				/*	 MCDRV_BCKFS_64		(0)	: LRCK x 64		*/
				/*	 MCDRV_BCKFS_48		(1)	: LRCK x 48		*/
				/*	 MCDRV_BCKFS_32		(2)	: LRCK x 32		*/
				/*	 MCDRV_BCKFS_512	(4)	: LRCK x 512	*/
				/*	 MCDRV_BCKFS_256	(5)	: LRCK x 256	*/
				/*	 MCDRV_BCKFS_128	(6)	: LRCK x 128	*/
				/*	 MCDRV_BCKFS_16		(7)	: LRCK x 16		*/
				MCDRV_BCKFS_32,
				/*	bInterface : Interface Selection	*/
				/*	 MCDRV_DIO_DA	(0)	: Digital Audio	*/
				/*	 MCDRV_DIO_PCM	(1)	: PCM			*/
				MCDRV_DIO_DA,
				/*	bBckInvert : Bit Clock Inversion Setting	*/
				/*	 MCDRV_BCLK_NORMAL	(0)	: Normal Operation	*/
				/*	 MCDRV_BCLK_INVERT	(1)	: Clock Inverted	*/
				MCDRV_BCLK_NORMAL,
				/*	bPcmHizTim : High Impedance transition timing after transmitting the last PCM I/F data	*/
				/*	 MCDRV_PCMHIZTIM_FALLING	(0)	: BCLK#* Falling Edge	*/
				/*	 MCDRV_PCMHIZTIM_RISING		(1)	: BCLK#* Rising Edge	*/
				MCDRV_PCMHIZTIM_FALLING,
				/*	bPcmClkDown : Bit Clock Setting with PCM selected and Master selected	*/
				/*	 MCDRV_PCM_CLKDOWN_OFF	(0)	: A bit clock value specified with bBckFs				*/
				/*	 MCDRV_PCM_CLKDOWN_HALF	(1)	: A half of the bit clock value specified with bBckFs	*/
				MCDRV_PCM_CLKDOWN_OFF,
				/*	bPcmFrame : Frame Mode Setting with PCM interface	*/
				/*	 MCDRV_PCM_SHORTFRAME	(0)	: Short Frame			*/
				/*	 MCDRV_PCM_LONGFRAME	(1)	: Long Frame			*/
				MCDRV_PCM_SHORTFRAME,
				/*	bPcmHighPeriod : LR clock High time setting with PCM selected and Master selected	*/
				/*	 0 to 31	: High level keeps during the period of time of			*/
				/*				(setting value + 1) of the bit clock.					*/
				0,
			},
			/*	sDir	*/
			{
				/*	wSrcRate : Sampling Rate Converter Setting	*/
				0,
				/*	sDaFormat : Digital Audio Format Information	*/
				{
					/*	bBitSel : Bit Width Setting	*/
					/*	 MCDRV_BITSEL_16	(0)	: 16bit	*/
					/*	 MCDRV_BITSEL_20	(1)	: 20bit	*/
					/*	 MCDRV_BITSEL_24	(2)	: 24bit	*/
					MCDRV_BITSEL_16,
					/*	bMode : Data Format Setting	*/
					/*	 MCDRV_DAMODE_HEADALIGN	(0)	: Left-justified Format		*/
					/*	 MCDRV_DAMODE_I2S		(1)	: I2S						*/
					/*	 MCDRV_DAMODE_TAILALIGN	(2)	: Right-justified Format	*/
					MCDRV_DAMODE_I2S
				},
				/*	sPcmFormat : PCM Format Information	*/
				{
					/*	bMono : Mono / Stereo Setting	*/
					/*	 MCDRV_PCM_STEREO	(0)	: Stereo	*/
					/*	 MCDRV_PCM_MONO		(1)	: Mono		*/
					MCDRV_PCM_MONO ,
					/*	bOrder : Bit Order Setting										*/
					/*	 MCDRV_PCM_MSB_FIRST		(0)	: MSB First						*/
					/*	 MCDRV_PCM_LSB_FIRST		(1)	: LSB First						*/
					/*	 MCDRV_PCM_MSB_FIRST_SIGN	(2)	: MSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_LSB_FIRST_SIGN	(3)	: LSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_MSB_FIRST_ZERO	(4)	: MSB First (Zeros Padding)		*/
					/*	 MCDRV_PCM_LSB_FIRST_ZERO	(5)	: LSB First (Zeros Padding)		*/
					MCDRV_PCM_MSB_FIRST,
					/*	bLaw : Data Format Setting		*/
					/*	 MCDRV_PCM_LINEAR	(0)	: Linear	*/
					/*	 MCDRV_PCM_ALAW		(1)	: A-Law		*/
					/*	 MCDRV_PCM_MULAW	(2)	: u-Law		*/
					MCDRV_PCM_LINEAR,
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_PCM_BITSEL_8		(0)	8 bits	*/
					/*	 MCDRV_PCM_BITSEL_13	(1) 13 bits	*/
					/*	 MCDRV_PCM_BITSEL_14	(2) 14 bits	*/
					/*	 MCDRV_PCM_BITSEL_16	(3) 16 bits	*/
					MCDRV_PCM_BITSEL_8
				},
				/*	asSlot : Setting of a slot number of data to be fed to each channel	*/
				{0, 1}
			},
			/*	sDit	*/
			{
				/*	wSrcRate : Sampling Rate Converter Setting	*/
				0,
				/*	sDaFormat : Digital Audio Format Information	*/
				{
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_BITSEL_16	(0)	: 16bit	*/
					/*	 MCDRV_BITSEL_20	(1)	: 20bit	*/
					/*	 MCDRV_BITSEL_24	(2)	: 24bit	*/
					MCDRV_BITSEL_16,
					/*	bMode : Data Format Setting								*/
					/*	 MCDRV_DAMODE_HEADALIGN	(0)	: Left-justified Format	*/
					/*	 MCDRV_DAMODE_I2S		(1)	: I2S					*/
					/*	 MCDRV_DAMODE_TAILALIGN	(2)	: Right-justified Format	*/
					MCDRV_DAMODE_I2S
				},
				/*	sPcmFormat : PCM Format Information	*/
				{
					/*	bMono : Mono / Stereo Setting	*/
					/*	 MCDRV_PCM_STEREO	(0)	: Stereo	*/
					/*	 MCDRV_PCM_MONO		(1)	: Mono		*/
					MCDRV_PCM_MONO ,
					/*	bOrder : Bit Order Setting										*/
					/*	 MCDRV_PCM_MSB_FIRST		(0)	: MSB First						*/
					/*	 MCDRV_PCM_LSB_FIRST		(1)	: LSB First						*/
					/*	 MCDRV_PCM_MSB_FIRST_SIGN	(2)	: MSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_LSB_FIRST_SIGN	(3)	: LSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_MSB_FIRST_ZERO	(4)	: MSB First (Zeros Padding)		*/
					/*	 MCDRV_PCM_LSB_FIRST_ZERO	(5)	: LSB First (Zeros Padding)		*/
					MCDRV_PCM_MSB_FIRST,
					/*	bLaw : Data Format Setting	*/
					/*	 MCDRV_PCM_LINEAR	(0)	: Linear	*/
					/*	 MCDRV_PCM_ALAW		(1)	: A-Law		*/
					/*	 MCDRV_PCM_MULAW	(2)	: u-Law		*/
					MCDRV_PCM_LINEAR,
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_PCM_BITSEL_8		(0)	8 bits	*/
					/*	 MCDRV_PCM_BITSEL_13	(1) 13 bits	*/
					/*	 MCDRV_PCM_BITSEL_14	(2) 14 bits	*/
					/*	 MCDRV_PCM_BITSEL_16	(3) 16 bits	*/
					MCDRV_PCM_BITSEL_8
				},
				/*	asSlot Setting of a slot number of data to be transmitted from each channel	*/
				{0, 1}
			}
		},
		/*	DIO port 1	*/
		{
			/*	sDioCommon	*/
			{
				/*	bMasterSlave : Master / Slave Setting	*/
				/*	 MCDRV_DIO_SLAVE	(0)	: Slave		*/
				/*	 MCDRV_DIO_MASTER	(1)	: Master	*/
/* QNET Audio:2012-01-17 Basic Feat start */
				MCDRV_DIO_MASTER,
/* QNET Audio:2012-01-17 Basic Feat   end */
				/*	bAutoFs : Sampling frequency automatic measurement ON/OFF Setting in slave mode	*/
				/*	 MCDRV_AUTOFS_OFF	(0)	: OFF	*/
				/*	 MCDRV_AUTOFS_ON	(1)	: ON	*/
				MCDRV_AUTOFS_ON ,
				/*	bFs : Sampling Rate Setting			*/
				/*	 MCDRV_FS_48000	(0)		: 48kHz		*/
				/*	 MCDRV_FS_44100	(1)		: 44.1kHz	*/
				/*	 MCDRV_FS_32000	(2)		: 32kHz		*/
				/*	 MCDRV_FS_24000	(4)		: 24kHz		*/
				/*	 MCDRV_FS_22050	(5)		: 22.05kHz	*/
				/*	 MCDRV_FS_16000	(6)		: 16kHz		*/
				/*	 MCDRV_FS_12000	(8)		: 12kHz		*/
				/*	 MCDRV_FS_11025	(9)		: 11.025kHz	*/
				/*	 MCDRV_FS_8000	(10)	: 8kHz		*/
				MCDRV_FS_8000,
				/*	bBckFs : Bit Clock Frequency Setting	*/
				/*	 MCDRV_BCKFS_64		(0)	: LRCK x 64		*/
				/*	 MCDRV_BCKFS_48		(1)	: LRCK x 48		*/
				/*	 MCDRV_BCKFS_32		(2)	: LRCK x 32		*/
				/*	 MCDRV_BCKFS_512	(4)	: LRCK x 512	*/
				/*	 MCDRV_BCKFS_256	(5)	: LRCK x 256	*/
				/*	 MCDRV_BCKFS_128	(6)	: LRCK x 128	*/
				/*	 MCDRV_BCKFS_16		(7)	: LRCK x 16		*/
				MCDRV_BCKFS_32,
				/*	bInterface : Interface Selection		*/
				/*	 MCDRV_DIO_DA	(0)	: Digital Audio	*/
				/*	 MCDRV_DIO_PCM	(1)	: PCM			*/
/* QNET Audio:2012-01-17 Basic Feat start */
				MCDRV_DIO_DA,
/* QNET Audio:2012-01-17 Basic Feat   end */
				/*	bBckInvert : Bit Clock Inversion Setting		*/
				/*	 MCDRV_BCLK_NORMAL	(0)	: Normal Operation	*/
				/*	 MCDRV_BCLK_INVERT	(1)	: Clock Inverted	*/
				MCDRV_BCLK_NORMAL,
				/*	bPcmHizTim : High Impedance transition timing after transmitting the last PCM I/F data	*/
				/*	 MCDRV_PCMHIZTIM_FALLING	(0)	: BCLK#* Falling Edge	*/
				/*	 MCDRV_PCMHIZTIM_RISING		(1)	: BCLK#* Rising Edge	*/
				MCDRV_PCMHIZTIM_FALLING,
				/*	bPcmClkDown : Bit Clock Setting with PCM selected and Master selected				*/
				/*	 MCDRV_PCM_CLKDOWN_OFF	(0)	: A bit clock value specified with bBckFs				*/
				/*	 MCDRV_PCM_CLKDOWN_HALF	(1)	: A half of the bit clock value specified with bBckFs	*/
				MCDRV_PCM_CLKDOWN_OFF,
				/*	bPcmFrame : Frame Mode Setting with PCM interface	*/
				/*	 MCDRV_PCM_SHORTFRAME	(0)	: Short Frame	*/
				/*	 MCDRV_PCM_LONGFRAME	(1)	: Long Frame	*/
				MCDRV_PCM_SHORTFRAME,
				/*	bPcmHighPeriod : LR clock High time setting with PCM selected and Master selected	*/
				/*	 0 to 31	: High level keeps during the period of time of			*/
				/*				(setting value + 1) of the bit clock.					*/
				0,
			},
			/*	sDir	*/
			{
				/*	wSrcRate : Sampling Rate Converter Setting	*/
				0,
				/*	sDaFormat : Digital Audio Format Information	*/	
				{
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_BITSEL_16	(0)	: 16bit	*/
					/*	 MCDRV_BITSEL_20	(1)	: 20bit	*/
					/*	 MCDRV_BITSEL_24	(2)	: 24bit	*/
					MCDRV_BITSEL_16,
					/*	bMode : Data Format Setting								*/
					/*	 MCDRV_DAMODE_HEADALIGN	(0)	: Left-justified Format	*/
					/*	 MCDRV_DAMODE_I2S		(1)	: I2S					*/
					/*	 MCDRV_DAMODE_TAILALIGN	(2)	: Right-justified Format	*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_DAMODE_I2S,
/* QNET Audio:2012-01-17 Basic Feat   end */
				},
				/*	sPcmFormat : PCM Format Information	*/
				{
					/*	bMono : Mono / Stereo Setting	*/
					/*	 MCDRV_PCM_STEREO(0) Stereo	*/
					/*	 MCDRV_PCM_MONO	(1) Mono	*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_STEREO,
/* QNET Audio:2012-01-17 Basic Feat   end */
					/*	bOrder : Bit Order Setting										*/
					/*	 MCDRV_PCM_MSB_FIRST		(0)	: MSB First						*/
					/*	 MCDRV_PCM_LSB_FIRST		(1)	: LSB First						*/
					/*	 MCDRV_PCM_MSB_FIRST_SIGN	(2)	: MSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_LSB_FIRST_SIGN	(3)	: LSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_MSB_FIRST_ZERO	(4)	: MSB First (Zeros Padding)		*/
					/*	 MCDRV_PCM_LSB_FIRST_ZERO	(5)	: LSB First (Zeros Padding)		*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_MSB_FIRST,
/* QNET Audio:2012-01-17 Basic Feat   end */
					/*	bLaw : Data Format Setting	*/
					/*	 MCDRV_PCM_LINEAR	(0)	: Linear	*/
					/*	 MCDRV_PCM_ALAW		(1)	: A-Law		*/
					/*	 MCDRV_PCM_MULAW	(2)	: u-Law		*/
					MCDRV_PCM_LINEAR,
					/*	bBitSel : Bit Width Setting	*/
					/*	 MCDRV_PCM_BITSEL_8		(0)	8 bits	*/
					/*	 MCDRV_PCM_BITSEL_13	(1) 13 bits	*/
					/*	 MCDRV_PCM_BITSEL_14	(2) 14 bits	*/
					/*	 MCDRV_PCM_BITSEL_16	(3) 16 bits	*/
					MCDRV_PCM_BITSEL_16
				},
				/*	asSlot : Setting of a slot number of data to be fed to each channel	*/
/* FUJITSU: 2013-01-16 MOD-S */
				{0, 0}
/* FUJITSU: 2013-01-16 MOD-E */
			},
			/*	sDit	*/
			{
				/*	wSrcRate : Sampling Rate Converter Setting	*/
				0,
				/*	sDaFormat : Digital Audio Format Information	*/
				{
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_BITSEL_16	(0)	: 16bit	*/
					/*	 MCDRV_BITSEL_20	(1)	: 20bit	*/
					/*	 MCDRV_BITSEL_24	(2)	: 24bit	*/
					MCDRV_BITSEL_16,
					/*	bMode : Data Format Setting								*/
					/*	 MCDRV_DAMODE_HEADALIGN	(0)	: Left-justified Format		*/
					/*	 MCDRV_DAMODE_I2S		(1)	: I2S						*/
					/*	 MCDRV_DAMODE_TAILALIGN	(2)	: Right-justified Format	*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_DAMODE_I2S,
/* QNET Audio:2012-01-17 Basic Feat   end */
				},
				/*	sPcmFormat : PCM Format Information	*/
				{
					/*	bMono : Mono / Stereo Setting	*/
					/*	 MCDRV_PCM_STEREO	(0)	: Stereo	*/
					/*	 MCDRV_PCM_MONO		(1)	: Mono		*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_STEREO,
/* QNET Audio:2012-01-17 Basic Feat   end */
					/*	bOrder : Bit Order Setting	*/
					/*	 MCDRV_PCM_MSB_FIRST		(0)	: MSB First						*/
					/*	 MCDRV_PCM_LSB_FIRST		(1)	: LSB First						*/
					/*	 MCDRV_PCM_MSB_FIRST_SIGN	(2)	: MSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_LSB_FIRST_SIGN	(3)	: LSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_MSB_FIRST_ZERO	(4)	: MSB First (Zeros Padding)		*/
					/*	 MCDRV_PCM_LSB_FIRST_ZERO	(5)	: LSB First (Zeros Padding)		*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_MSB_FIRST,
/* QNET Audio:2012-01-17 Basic Feat   end */
					/*	bLaw : Data Format Setting	*/
					/*	 MCDRV_PCM_LINEAR	(0)	: Linear	*/
					/*	 MCDRV_PCM_ALAW		(1)	: A-Law		*/
					/*	 MCDRV_PCM_MULAW 	(2)	: u-Law		*/
					MCDRV_PCM_LINEAR,
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_PCM_BITSEL_8		(0)	: 8 bits	*/
					/*	 MCDRV_PCM_BITSEL_13	(1)	: 13 bits	*/
					/*	 MCDRV_PCM_BITSEL_14	(2)	: 14 bits	*/
					/*	 MCDRV_PCM_BITSEL_16	(3)	: 16 bits	*/
					MCDRV_PCM_BITSEL_16
				},
				/*	asSlot Setting of a slot number of data to be transmitted from each channel	*/
				{0, 1}
			}
		},
		/*	DIO port 2	*/
		{
			/*	sDioCommon	*/
			{
				/*	bMasterSlave : Master / Slave Setting	*/
				/*	 MCDRV_DIO_SLAVE	(0)	: Slave		*/
				/*	 MCDRV_DIO_MASTER	(1)	: Master	*/
/* QNET Audio:2012-01-17 Basic Feat start */
				MCDRV_DIO_SLAVE,
/* QNET Audio:2012-01-17 Basic Feat   end */
				/*	bAutoFs : Sampling frequency automatic measurement ON/OFF Setting in slave mode	*/
				/*	 MCDRV_AUTOFS_OFF	(0)	: OFF	*/
				/*	 MCDRV_AUTOFS_ON	(1)	: ON	*/
				MCDRV_AUTOFS_ON ,
				/*	bFs : Sampling Rate Setting			*/
				/*	 MCDRV_FS_48000	(0)		: 48kHz		*/
				/*	 MCDRV_FS_44100	(1)		: 44.1kHz	*/
				/*	 MCDRV_FS_32000	(2)		: 32kHz		*/
				/*	 MCDRV_FS_24000	(4)		: 24kHz		*/
				/*	 MCDRV_FS_22050	(5)		: 22.05kHz	*/
				/*	 MCDRV_FS_16000	(6)		: 16kHz		*/
				/*	 MCDRV_FS_12000	(8)		: 12kHz		*/
				/*	 MCDRV_FS_11025	(9)		: 11.025kHz	*/
				/*	 MCDRV_FS_8000	(10)	: 8kHz		*/
				MCDRV_FS_8000,
				/*	bBckFs : Bit Clock Frequency Setting	*/
				/*	 MCDRV_BCKFS_64		(0)	: LRCK x 64		*/
				/*	 MCDRV_BCKFS_48		(1)	: LRCK x 48		*/
				/*	 MCDRV_BCKFS_32		(2)	: LRCK x 32		*/
				/*	 MCDRV_BCKFS_512	(4)	: LRCK x 512	*/
				/*	 MCDRV_BCKFS_256	(5)	: LRCK x 256	*/
				/*	 MCDRV_BCKFS_128	(6)	: LRCK x 128	*/
				/*	 MCDRV_BCKFS_16		(7)	: LRCK x 16		*/
/* QNET Audio:2012-01-17 Basic Feat start */
				MCDRV_BCKFS_128,
/* QNET Audio:2012-01-17 Basic Feat   end */
				/*	bInterface : Interface Selection	*/
				/*	 MCDRV_DIO_DA	(0)	: Digital Audio	*/
				/*	 MCDRV_DIO_PCM	(1)	: PCM			*/
				MCDRV_DIO_PCM,
				/*	bBckInvert : Bit Clock Inversion Setting	*/
				/*	 MCDRV_BCLK_NORMAL	(0)	: Normal Operation	*/
				/*	 MCDRV_BCLK_INVERT	(1)	: Clock Inverted	*/
				MCDRV_BCLK_NORMAL,
				/*	bPcmHizTim : High Impedance transition timing after transmitting the last PCM I/F data	*/
				/*	 MCDRV_PCMHIZTIM_FALLING	(0)	: BCLK#* Falling Edge	*/
				/*	 MCDRV_PCMHIZTIM_RISING		(1)	: BCLK#* Rising Edge	*/
				MCDRV_PCMHIZTIM_FALLING,
				/*	bPcmClkDown : Bit Clock Setting with PCM selected and Master selected				*/
				/*	 MCDRV_PCM_CLKDOWN_OFF	(0)	: A bit clock value specified with bBckFs				*/
				/*	 MCDRV_PCM_CLKDOWN_HALF	(1)	: A half of the bit clock value specified with bBckFs	*/
				MCDRV_PCM_CLKDOWN_OFF,
				/*	bPcmFrame : Frame Mode Setting with PCM interface	*/
				/*	 MCDRV_PCM_SHORTFRAME	(0)	: Short Frame	*/
				/*	 MCDRV_PCM_LONGFRAME	(1)	: Long Frame	*/
				MCDRV_PCM_SHORTFRAME,
				/*	bPcmHighPeriod : LR clock High time setting with PCM selected and Master selected	*/
				/*	 0 to 31	: High level keeps during the period of time of	*/
				/*				(setting value + 1) of the bit clock.			*/
				0,
			},
			/*	sDir	*/
			{
				/*	wSrcRate : Sampling Rate Converter Setting	*/
				0,
				/*	sDaFormat : Digital Audio Format Information	*/
				{
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_BITSEL_16	(0)	: 16bit	*/
					/*	 MCDRV_BITSEL_20	(1)	: 20bit	*/
					/*	 MCDRV_BITSEL_24	(2)	: 24bit	*/
					MCDRV_BITSEL_16,
					/*	bMode : Data Format Setting								*/
					/*	 MCDRV_DAMODE_HEADALIGN	(0)	: Left-justified Format		*/
					/*	 MCDRV_DAMODE_I2S		(1)	: I2S						*/
					/*	 MCDRV_DAMODE_TAILALIGN	(2)	: Right-justified Format	*/
					MCDRV_DAMODE_HEADALIGN
				},
				/*	sPcmFormat : PCM Format Information	*/
				{
					/*	bMono : Mono / Stereo Setting	*/
					/*	 MCDRV_PCM_STEREO(0) Stereo	*/
					/*	 MCDRV_PCM_MONO	(1) Mono	*/
					MCDRV_PCM_MONO ,
					/*	bOrder : Bit Order Setting	*/
					/*	 MCDRV_PCM_MSB_FIRST		(0)	: MSB First						*/
					/*	 MCDRV_PCM_LSB_FIRST		(1)	: LSB First						*/
					/*	 MCDRV_PCM_MSB_FIRST_SIGN	(2)	: MSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_LSB_FIRST_SIGN	(3)	: LSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_MSB_FIRST_ZERO	(4)	: MSB First (Zeros Padding)		*/
					/*	 MCDRV_PCM_LSB_FIRST_ZERO	(5)	: LSB First (Zeros Padding)		*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_MSB_FIRST_ZERO,
/* QNET Audio:2012-01-17 Basic Feat end */
					/*	bLaw : Data Format Setting	*/
					/*	 MCDRV_PCM_LINEAR	(0)	: Linear	*/
					/*	 MCDRV_PCM_ALAW		(1)	: A-Law		*/
					/*	 MCDRV_PCM_MULAW 	(2)	: u-Law		*/
					MCDRV_PCM_LINEAR,
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_PCM_BITSEL_8 	(0)	: 8 bits	*/
					/*	 MCDRV_PCM_BITSEL_13	(1)	: 13 bits	*/
					/*	 MCDRV_PCM_BITSEL_14	(2)	: 14 bits	*/
					/*	 MCDRV_PCM_BITSEL_16	(3)	: 16 bits	*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_BITSEL_13
/* QNET Audio:2012-01-17 Basic Feat end */
				},
				/*	asSlot : Setting of a slot number of data to be fed to each channel	*/
				{0, 1}
			},
			/*	sDit	*/
			{
				/*	wSrcRate : Sampling Rate Converter Setting	*/
				0,
				/*	sDaFormat : Digital Audio Format Information	*/
				{
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_BITSEL_16	(0)	: 16bit	*/
					/*	 MCDRV_BITSEL_20	(1)	: 20bit	*/
					/*	 MCDRV_BITSEL_24	(2)	: 24bit	*/
					MCDRV_BITSEL_16,
					/*	bMode : Data Format Setting								*/
					/*	 MCDRV_DAMODE_HEADALIGN	(0)	: Left-justified Format	*/
					/*	 MCDRV_DAMODE_I2S		(1)	: I2S					*/
					/*	 MCDRV_DAMODE_TAILALIGN	(2)	: Right-justified Format	*/
					MCDRV_DAMODE_HEADALIGN
				},
				/*	sPcmFormat : PCM Format Information	*/
				{
					/*	bMono : Mono / Stereo Setting	*/
					/*	 MCDRV_PCM_STEREO	(0)	: Stereo	*/
					/*	 MCDRV_PCM_MONO		(1)	: Mono		*/
					MCDRV_PCM_MONO ,
					/*	bOrder : Bit Order Setting	*/
					/*	 MCDRV_PCM_MSB_FIRST		(0)	: MSB First						*/
					/*	 MCDRV_PCM_LSB_FIRST		(1)	: LSB First						*/
					/*	 MCDRV_PCM_MSB_FIRST_SIGN	(2)	: MSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_LSB_FIRST_SIGN	(3)	: LSB First (Sign Extension)	*/
					/*	 MCDRV_PCM_MSB_FIRST_ZERO	(4)	: MSB First (Zeros Padding)		*/
					/*	 MCDRV_PCM_LSB_FIRST_ZERO	(5)	: LSB First (Zeros Padding)		*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_MSB_FIRST_ZERO,
/* QNET Audio:2012-01-17 Basic Feat end */
					/*	bLaw : Data Format Setting		*/
					/*	 MCDRV_PCM_LINEAR	(0)	: Linear	*/
					/*	 MCDRV_PCM_ALAW		(1)	: A-Law		*/
					/*	 MCDRV_PCM_MULAW	(2)	: u-Law		*/
					MCDRV_PCM_LINEAR,
					/*	bBitSel : Bit Width Setting		*/
					/*	 MCDRV_PCM_BITSEL_8 	(0)	8 bits	*/
					/*	 MCDRV_PCM_BITSEL_13	(1) 13 bits	*/
					/*	 MCDRV_PCM_BITSEL_14	(2) 14 bits	*/
					/*	 MCDRV_PCM_BITSEL_16	(3) 16 bits	*/
/* QNET Audio:2012-01-17 Basic Feat start */
					MCDRV_PCM_BITSEL_13
/* QNET Audio:2012-01-17 Basic Feat end */
				},
				/*	asSlot Setting of a slot number of data to be transmitted from each channel	*/
				{0, 1}
			}
		}
	}
};

/* ========================================
	DAC settings
	========================================*/
static const MCDRV_DAC_INFO stDacInfo_Default = {
	/*	bMasterSwap : DAC Master Path SWAP Setting							*/
	/*	 MCDRV_DSWAP_OFF		(0)	: No SWAP								*/
	/*	 MCDRV_DSWAP_SWAP		(1)	: SWAP									*/
	/*	 MCDRV_DSWAP_MUTE		(2)	: MUTE									*/
	/*	 MCDRV_DSWAP_RMVCENTER	(3)	: Center Removed						*/
	/*	 MCDRV_DSWAP_MONO	 	(4)	: Mono									*/
	/*	 MCDRV_DSWAP_MONOHALF	(5)	: Reserved (do not use this setting)	*/
	/*	 MCDRV_DSWAP_BOTHL		(6)	: Lch data output in both Lch and Rch	*/
	/*	 MCDRV_DSWAP_BOTHR		(7)	: Rch data output in both Lch and Rch	*/
	MCDRV_DSWAP_OFF,
	/*	bVoiceSwap : DAC Voice Path SWAP Setting							*/
	/*	 MCDRV_DSWAP_OFF		(0)	: No SWAP								*/
	/*	 MCDRV_DSWAP_SWAP		(1)	: SWAP									*/
	/*	 MCDRV_DSWAP_MUTE		(2)	: MUTE									*/
	/*	 MCDRV_DSWAP_RMVCENTER	(3)	: Center Removed						*/
	/*	 MCDRV_DSWAP_MONO	 	(4)	: Mono (-6dB)							*/
	/*	 MCDRV_DSWAP_MONOHALF	(5)	: Reserved (do not use this setting)	*/
	/*	 MCDRV_DSWAP_BOTHL		(6)	: Lch data output in both Lch and Rch	*/
	/*	 MCDRV_DSWAP_BOTHR		(7)	: Rch data output in both Lch and Rch	*/
	MCDRV_DSWAP_OFF,
	/*	bDcCut : HP, SP Protection DC-ct Filter Setting	*/
	/*	 MCDRV_DCCUT_ON		(0)	: DC-cut Filter ON	*/
	/*	 MCDRV_DCCUT_OFF	(1)	: DC-cut Filter OFF	*/
	MCDRV_DCCUT_ON
};

/* ========================================
	ADC settings
	========================================*/
static const MCDRV_ADC_INFO stAdcInfo_Default = {
	/*	bAgcAdjust : AGC Gain Control Range	*/
	/*	 MCDRV_AGCADJ_24	(0)	: -3dB to +24dB	*/
	/*	 MCDRV_AGCADJ_18	(1)	: -3dB to +18dB	*/
	/*	 MCDRV_AGCADJ_12	(2)	: -3dB to +12dB	*/
	/*	 MCDRV_AGCADJ_0		(3)	: -3dB to +0dB	*/
	MCDRV_AGCADJ_0,
	/*	bAgcOn : AGC ON/OFF Setting	*/
	/*	 MCDRV_AGC_OFF	(0)	: OFF	*/
	/*	 MCDRV_AGC_ON	(1)	: ON	*/
	MCDRV_AGC_OFF,
	/*	bMonot : Mono / Stereo Setting	*/
	/*	 MCDRV_ADC_STEREO	(0)	: Stereo	*/
	/*	 MCDRV_ADC_MONO		(1)	: Mono		*/
	MCDRV_ADC_STEREO
};

/* ========================================
   ADC Ex settings
   ========================================*/
static const MCDRV_ADC_EX_INFO stAdcExInfo_Default = {
	/*	bAgcEnv : AGC envelope	*/
	/*		MCDRV_AGCENV_5310	(0)	:  5.31ms	*/
	/*		MCDRV_AGCENV_10650	(1)	: 10.65ms	*/
	/*		MCDRV_AGCENV_21310	(2)	: 21.31ms	*/
	/*		MCDRV_AGCENV_85230	(3)	: 85.23ms	*/
	MCDRV_AGCENV_10650,
	/*	bAgcLvl : AGC target level	*/
	/*		MCDRV_AGCLVL_3	(0)	: -3dBFS	*/
	/*		MCDRV_AGCLVL_6	(1)	: -6dBFS	*/
	/*		MCDRV_AGCLVL_9	(2)	: -9dBFS	*/
	/*		MCDRV_AGCLVL_12	(3)	: -12dBFS	*/
	MCDRV_AGCLVL_3,
	/*	bAgcUpTim : AGC gain up time	*/
	/*		MCDRV_AGCUPTIM_341	(0)	:  341ms	*/
	/*		MCDRV_AGCUPTIM_683	(1)	:  683ms	*/
	/*		MCDRV_AGCUPTIM_1365	(2)	: 1.365s	*/
	/*		MCDRV_AGCUPTIM_2730	(3)	:  2.73s	*/
	MCDRV_AGCUPTIM_1365,
	/*	bAgcDwTim : AGC gain down time	*/
	/*		MCDRV_AGCDWTIM_5310		(0)	:  5.31ms	*/
	/*		MCDRV_AGCDWTIM_10650	(1)	: 10.65ms	*/
	/*		MCDRV_AGCDWTIM_21310	(2)	: 21.31ms	*/
	/*		MCDRV_AGCDWTIM_85230	(3)	: 85.23ms	*/
	MCDRV_AGCDWTIM_10650,
	/*	bAgcCutLvl : AGC cut level	*/
	/*		MCDRV_AGCCUTLVL_OFF	(0)	:     OFF	*/
	/*		MCDRV_AGCCUTLVL_80	(1)	: -80dBFS	*/
	/*		MCDRV_AGCCUTLVL_70	(2)	: -70dBFS	*/
	/*		MCDRV_AGCCUTLVL_60	(3)	: -60dBFS	*/
	MCDRV_AGCCUTLVL_OFF,
	/*	bAgcCutTim : AGC cut time	*/
	/*		MCDRV_AGCCUTTIM_5310	(0)	:  5.31ms	*/
	/*		MCDRV_AGCCUTTIM_10650	(1)	: 10.65ms	*/
	/*		MCDRV_AGCCUTTIM_341000	(2)	:   341ms	*/
	/*		MCDRV_AGCCUTTIM_2730000	(3)	:   2730s	*/
	MCDRV_AGCCUTTIM_5310
};

/* ========================================
	SP settings
	========================================*/
static const MCDRV_SP_INFO stSpInfo_Default = {
	/*	bSwap : Swap setting	*/
	/*	 MCDRV_SPSWAP_OFF	(0)	: No SWAP	*/
	/*	 MCDRV_SPSWAP_SWAP	(1)	: SWAP		*/
	MCDRV_SPSWAP_OFF
};

/* ========================================
	DNG settings
	========================================*/
static const MCDRV_DNG_INFO stDngInfo_Default = {
	/*	bOnOff[] : Digital Noise Gate On/Off Setting	*/
	/*	 MCDRV_DNG_OFF	(0)	: OFF	*/
	/*	 MCDRV_DNG_ON	(1)	: ON	*/
	{MCDRV_DNG_OFF, MCDRV_DNG_OFF, MCDRV_DNG_OFF},

	/*	bThreshold[] : Threshold Setting	*/
	/*	 MCDRV_DNG_THRES_30	(0)	*/
	/*	 MCDRV_DNG_THRES_36	(1)	*/
	/*	 MCDRV_DNG_THRES_42	(2)	*/
	/*	 MCDRV_DNG_THRES_48	(3)	*/
	/*	 MCDRV_DNG_THRES_54	(4)	*/
	/*	 MCDRV_DNG_THRES_60	(5)	*/
	/*	 MCDRV_DNG_THRES_66	(6)	*/
	/*	 MCDRV_DNG_THRES_72	(7)	*/
	/*	 MCDRV_DNG_THRES_78	(8)	*/
	/*	 MCDRV_DNG_THRES_84	(9)	*/
	{MCDRV_DNG_THRES_60, MCDRV_DNG_THRES_60, MCDRV_DNG_THRES_60},

	/*	bHold[] : Hold Time Setting			*/
	/*	 MCDRV_DNG_HOLD_30	(0)	:	30ms	*/
	/*	 MCDRV_DNG_HOLD_120	(1)	:	120ms	*/
	/*	 MCDRV_DNG_HOLD_500	(2)	:	500ms	*/
	{MCDRV_DNG_HOLD_500, MCDRV_DNG_HOLD_500, MCDRV_DNG_HOLD_500},

	/*	bAttack[] : Attack Time Setting			*/
	/*	 MCDRV_DNG_ATTACK_25	(0)	:	25ms	*/
	/*	 MCDRV_DNG_ATTACK_100	(1)	:	100ms	*/
	/*	 MCDRV_DNG_ATTACK_400	(2)	:	400ms	*/
	/*	 MCDRV_DNG_ATTACK_800	(3)	:	800ms	*/
	{MCDRV_DNG_ATTACK_800, MCDRV_DNG_ATTACK_800, MCDRV_DNG_ATTACK_800},

	/*	bRelease[] : Release Time Setting		*/
	/*	 MCDRV_DNG_RELEASE_7950	(0)	: 7.95ms	*/
	/*	 MCDRV_DNG_RELEASE_470	(1)	: 0.47ms	*/
	/*	 MCDRV_DNG_RELEASE_940	(2)	: 0.94ms	*/
	{MCDRV_DNG_RELEASE_940, MCDRV_DNG_RELEASE_940, MCDRV_DNG_RELEASE_940},

	/*	bTarget[] : Target Volume Setting	*/
	/*	 MCDRV_DNG_TARGET_6		(0)	: -6dB	*/
	/*	 MCDRV_DNG_TARGET_9		(1)	: -9dB	*/
	/*	 MCDRV_DNG_TARGET_12	(2)	: -12dB	*/
	/*	 MCDRV_DNG_TARGET_15	(3)	: -15dB	*/
	/*	 MCDRV_DNG_TARGET_18	(4)	: -18dB	*/
	/*	 MCDRV_DNG_TARGET_MUTE	(5)	: Mute	*/
	{MCDRV_DNG_TARGET_MUTE, MCDRV_DNG_TARGET_MUTE, MCDRV_DNG_TARGET_MUTE},
};

/* ========================================
	System EQ settings
	========================================*/
static MCDRV_SYSEQ_INFO stSyseqInfo_Default = {
	/*	On/Off	*/
	0x00,
	/*	EQ	*/
	{
		0x10,0xc4,0x50,0x12,0xc4,0x40,0x02,0xa9,
		0x60,0xed,0x3b,0xc0,0xfc,0x92,0x40,
	},
};

/* ========================================
	DTMF settings
	========================================*/
static MCDRV_DTMF_INFO stDtmfInfo_Default = {
	/*	DTMF HOST On/Off	*/
	MCDRV_DTMFHOST_REG,
	/*	DTMF On/Off	*/
	MCDRV_DTMF_OFF,
	/*	Param	*/
	{
		1000,
		1000,
		0xa000,
		0xa000,
		0,
		MCDRV_SINGENLOOP_OFF,
	},
};

/* ========================================
	DIT SWAP settings
	========================================*/
static MCDRV_DITSWAP_INFO stDitswapInfo_Default = {
	/*	swap L	*/
	MCDRV_DITSWAP_OFF,
	/*	swap R	*/
	MCDRV_DITSWAP_OFF,
};

#endif
