/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcresctrl.h
 *
 *		Description	: MC Driver resource control header
 *
 *		Version		: 2.0.1 	2011.07.14
 *
 ****************************************************************************/

#ifndef _MCRESCTRL_H_
#define _MCRESCTRL_H_

#include "mcdevif.h"
#include "mcpacking.h"

#define	MCDRV_BURST_WRITE_ENABLE	(0x01)

/*	Slave Address	*/
#define	MCDRV_SLAVEADDR_3AH			(0x3A)
#define	MCDRV_SLAVEADDR_11H			(0x11)

/*	HW_ID	*/
#define	MCDRV_HWID_79H				(0x79)
#define	MCDRV_HWID_71H				(0x71)
#define	MCDRV_HWID_14H				(0x14)
#define	MCDRV_HWID_46H				(0x46)
#define	MCDRV_HWID_44H				(0x44)

/*	eState setting	*/
typedef enum _MCDRV_STATE
{
	eMCDRV_STATE_NOTINIT,
	eMCDRV_STATE_READY
} MCDRV_STATE;

/*	volume setting	*/
#define	MCDRV_LOGICAL_VOL_MUTE		(-24576)	/*	-96dB	*/
#define	MCDRV_LOGICAL_MICGAIN_DEF	(3840)		/*	15dB	*/
#define	MCDRV_LOGICAL_HPGAIN_DEF	(0)			/*	0dB		*/

#define	MCDRV_VOLUPDATE_ALL			(0xFFFFFFFFUL)
#define	MCDRV_VOLUPDATE_ANAOUT_ALL	(0x00000001UL)

#define	MCDRV_REG_MUTE	(0x00)

/*	DAC source setting	*/
typedef enum _MCDRV_DAC_CH
{
	eMCDRV_DAC_MASTER	= 0,
	eMCDRV_DAC_VOICE
} MCDRV_DAC_CH;

/*	DIO port setting	*/
typedef enum _MCDRV_DIO_PORT_NO
{
	eMCDRV_DIO_0	= 0,
	eMCDRV_DIO_1,
	eMCDRV_DIO_2
} MCDRV_DIO_PORT_NO;

/*	Path source setting	*/
typedef enum _MCDRV_SRC_TYPE
{
	eMCDRV_SRC_NONE			= (0),
	eMCDRV_SRC_MIC1			= (1<<0),
	eMCDRV_SRC_MIC2			= (1<<1),
	eMCDRV_SRC_MIC3			= (1<<2),
	eMCDRV_SRC_LINE1_L		= (1<<3),
	eMCDRV_SRC_LINE1_R		= (1<<4),
	eMCDRV_SRC_LINE1_M		= (1<<5),
	eMCDRV_SRC_LINE2_L		= (1<<6),
	eMCDRV_SRC_LINE2_R		= (1<<7),
	eMCDRV_SRC_LINE2_M		= (1<<8),
	eMCDRV_SRC_DIR0			= (1<<9),
	eMCDRV_SRC_DIR1			= (1<<10),
	eMCDRV_SRC_DIR2			= (1<<11),
	eMCDRV_SRC_DTMF			= (1<<12),
	eMCDRV_SRC_PDM			= (1<<13),
	eMCDRV_SRC_ADC0			= (1<<14),
	eMCDRV_SRC_ADC1			= (1<<15),
	eMCDRV_SRC_DAC_L		= (1<<16),
	eMCDRV_SRC_DAC_R		= (1<<17),
	eMCDRV_SRC_DAC_M		= (1<<18),
	eMCDRV_SRC_AE			= (1<<19),
	eMCDRV_SRC_CDSP			= (1<<20),
	eMCDRV_SRC_MIX			= (1<<21),
	eMCDRV_SRC_DIR2_DIRECT	= (1<<22),
	eMCDRV_SRC_CDSP_DIRECT	= (1<<23)
} MCDRV_SRC_TYPE;

/*	Path destination setting	*/
typedef enum _MCDRV_DST_CH
{
	eMCDRV_DST_CH0	= 0,
	eMCDRV_DST_CH1,
	eMCDRV_DST_CH2,
	eMCDRV_DST_CH3
} MCDRV_DST_CH;
typedef enum _MCDRV_DST_TYPE
{
	eMCDRV_DST_HP	= 0,
	eMCDRV_DST_SP,
	eMCDRV_DST_RCV,
	eMCDRV_DST_LOUT1,
	eMCDRV_DST_LOUT2,
	eMCDRV_DST_PEAK,
	eMCDRV_DST_DIT0,
	eMCDRV_DST_DIT1,
	eMCDRV_DST_DIT2,
	eMCDRV_DST_DAC,
	eMCDRV_DST_AE,
	eMCDRV_DST_CDSP,
	eMCDRV_DST_ADC0,
	eMCDRV_DST_ADC1,
	eMCDRV_DST_MIX,
	eMCDRV_DST_MIX2,
	eMCDRV_DST_BIAS
} MCDRV_DST_TYPE;

/*	Register accsess availability	*/
typedef enum _MCDRV_REG_ACCSESS
{
	eMCDRV_ACCESS_DENY	= 0,
	eMCDRV_CAN_READ		= 0x01,
	eMCDRV_CAN_WRITE	= 0x02,
	eMCDRV_READ_WRITE	= eMCDRV_CAN_READ | eMCDRV_CAN_WRITE
} MCDRV_REG_ACCSESS;


/*	UpdateReg parameter	*/
typedef enum _MCDRV_UPDATE_MODE
{
	eMCDRV_UPDATE_NORMAL,
	eMCDRV_UPDATE_FORCE,
	eMCDRV_UPDATE_DUMMY
} MCDRV_UPDATE_MODE;

/*	ePacketBufAlloc setting	*/
typedef enum _MCDRV_PACKETBUF_ALLOC
{
	eMCDRV_PACKETBUF_FREE,
	eMCDRV_PACKETBUF_ALLOCATED
} MCDRV_PACKETBUF_ALLOC;

/* power management sequence mode */
typedef enum _MCDRV_PMODE
{
	eMCDRV_APM_ON,
	eMCDRV_APM_OFF
} MCDRV_PMODE;


#define	MCDRV_A_REG_NUM			(64)
#define	MCDRV_B_BASE_REG_NUM	(244)
#define	MCDRV_B_MIXER_REG_NUM	(218)
#define	MCDRV_B_AE_REG_NUM		(255)
#define	MCDRV_B_CDSP_REG_NUM	(130)
#define	MCDRV_B_CODEC_REG_NUM	(128)
#define	MCDRV_B_ANA_REG_NUM		(128)

/* control packet for serial host interface */
#define	MCDRV_MAX_CTRL_DATA_NUM	(1024)
typedef struct
{
	UINT8	abData[MCDRV_MAX_CTRL_DATA_NUM];
	UINT16	wDataNum;
} MCDRV_SERIAL_CTRL_PACKET;

/*	global information	*/
typedef struct
{
	UINT8						bHwId;
	MCDRV_PACKETBUF_ALLOC		ePacketBufAlloc;
	UINT8						abRegValA[MCDRV_A_REG_NUM];
	UINT8						abRegValB_BASE[MCDRV_B_BASE_REG_NUM];
	UINT8						abRegValB_MIXER[MCDRV_B_MIXER_REG_NUM];
	UINT8						abRegValB_AE[MCDRV_B_AE_REG_NUM];
	UINT8						abRegValB_CDSP[MCDRV_B_CDSP_REG_NUM];
	UINT8						abRegValB_CODEC[MCDRV_B_CODEC_REG_NUM];
	UINT8						abRegValB_ANA[MCDRV_B_ANA_REG_NUM];

	MCDRV_INIT_INFO				sInitInfo;
	MCDRV_PATH_INFO				sPathInfo;
	MCDRV_PATH_INFO				sPathInfoVirtual;
	MCDRV_VOL_INFO				sVolInfo;
	MCDRV_DIO_INFO				sDioInfo;
	MCDRV_DAC_INFO				sDacInfo;
	MCDRV_ADC_INFO				sAdcInfo;
	MCDRV_ADC_EX_INFO			sAdcExInfo;
	MCDRV_SP_INFO				sSpInfo;
	MCDRV_DNG_INFO				sDngInfo;
	MCDRV_AE_INFO				sAeInfo;
	MCDRV_PDM_INFO				sPdmInfo;
	MCDRV_PDM_EX_INFO			sPdmExInfo;
	MCDRV_DTMF_INFO				sDtmfInfo;
	MCDRV_GP_MODE				sGpMode;
	UINT8						abGpMask[GPIO_PAD_NUM];
	UINT8						abGpPad[GPIO_PAD_NUM];
	MCDRV_SYSEQ_INFO			sSysEq;
	MCDRV_CLKSW_INFO			sClockSwitch;
	MCDRV_DITSWAP_INFO			sDitSwapInfo;
	const UINT8*				pbAeEx;
	const UINT8*				pbCdspFw;
	UINT8						bCdspFs;
	UINT8						bEFIFO0L, bEFIFO0R, bEFIFO1L, bEFIFO1R;

	MCDRV_SERIAL_CTRL_PACKET	sCtrlPacket;
	UINT16						wCurSlaveAddress;
	UINT16						wCurRegType;
	UINT16						wCurRegAddress;
	UINT16						wDataContinueCount;
	UINT16						wPrevAddressIndex;

	MCDRV_PMODE					eAPMode;

	SINT32						sdIrqDisableCount;
} MCDRV_GLOBAL_INFO;



SINT32			McResCtrl_SetHwId				(UINT8 bHwId);
void			McResCtrl_Init					(void);
void			McResCtrl_InitBaseBlockReg		(void);
void			McResCtrl_InitBlockBReg			(void);
void			McResCtrl_UpdateState			(MCDRV_STATE eState);
MCDRV_STATE		McResCtrl_GetState				(void);
UINT8			McResCtrl_GetRegVal				(UINT16 wRegType, UINT16 wRegAddr);
void			McResCtrl_SetRegVal				(UINT16 wRegType, UINT16 wRegAddr, UINT8 bRegVal);

void			McResCtrl_SetInitInfo			(const MCDRV_INIT_INFO* psInitInfo);
void			McResCtrl_GetInitInfo			(MCDRV_INIT_INFO* psInitInfo);
void			McResCtrl_SetClockInfo			(const MCDRV_CLOCK_INFO* psClockInfo);
SINT32			McResCtrl_SetPathInfo			(const MCDRV_PATH_INFO* psPathInfo);
void			McResCtrl_GetPathInfo			(MCDRV_PATH_INFO* psPathInfo);
void			McResCtrl_GetPathInfoVirtual	(MCDRV_PATH_INFO* psPathInfo);
void			McResCtrl_SetDioInfo			(const MCDRV_DIO_INFO* psDioInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetDioInfo			(MCDRV_DIO_INFO* psDioInfo);
void			McResCtrl_SetVolInfo			(const MCDRV_VOL_INFO* psVolInfo);
void			McResCtrl_GetVolInfo			(MCDRV_VOL_INFO* psVolInfo);
void			McResCtrl_SetDacInfo			(const MCDRV_DAC_INFO* psDacInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetDacInfo			(MCDRV_DAC_INFO* psDacInfo);
void			McResCtrl_SetAdcInfo			(const MCDRV_ADC_INFO* psAdcInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetAdcInfo			(MCDRV_ADC_INFO* psAdcInfo);
void			McResCtrl_SetAdcExInfo			(const MCDRV_ADC_EX_INFO* psAdcExInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetAdcExInfo			(MCDRV_ADC_EX_INFO* psAdcExInfo);
void			McResCtrl_SetSpInfo				(const MCDRV_SP_INFO* psSpInfo);
void			McResCtrl_GetSpInfo				(MCDRV_SP_INFO* psSpInfo);
void			McResCtrl_SetDngInfo			(const MCDRV_DNG_INFO* psDngInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetDngInfo			(MCDRV_DNG_INFO* psDngInfo);
void			McResCtrl_SetAeInfo				(const MCDRV_AE_INFO* psAeInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetAeInfo				(MCDRV_AE_INFO* psAeInfo);
void			McResCtrl_SetAeEx				(const UINT8* pbPrm);
const UINT8*	McResCtrl_GetAeEx				(void);
void			McResCtrl_SetCdspFw				(const UINT8* pbPrm);
const UINT8*	McResCtrl_GetCdspFw				(void);
void			McResCtrl_SetCdspFs				(UINT8 bCdspFs);
UINT8			McResCtrl_GetCdspFs				(void);
void			McResCtrl_SetPdmInfo			(const MCDRV_PDM_INFO* psPdmInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetPdmInfo			(MCDRV_PDM_INFO* psPdmInfo);
void			McResCtrl_SetPdmExInfo			(const MCDRV_PDM_EX_INFO* psPdmExInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetPdmExInfo			(MCDRV_PDM_EX_INFO* psPdmExInfo);
void			McResCtrl_SetDtmfInfo			(const MCDRV_DTMF_INFO* psDtmfInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetDtmfInfo			(MCDRV_DTMF_INFO* psDtmfInfo);
void			McResCtrl_SetGPMode				(const MCDRV_GP_MODE* psGpMode);
void			McResCtrl_GetGPMode				(MCDRV_GP_MODE* psGpMode);
void			McResCtrl_SetGPMask				(UINT8 bMask, UINT32 dPadNo);
void			McResCtrl_GetGPMask				(UINT8* pabMask);
void			McResCtrl_SetGPPad				(UINT8 bPad, UINT32 dPadNo);
void			McResCtrl_GetGPPad				(UINT8* pabPad);
void			McResCtrl_SetSysEq				(const MCDRV_SYSEQ_INFO* psSysEq, UINT32 dUpdateInfo);
void			McResCtrl_GetSysEq				(MCDRV_SYSEQ_INFO* psSysEq);
void			McResCtrl_SetClockSwitch		(const MCDRV_CLKSW_INFO* psClockInfo);
void			McResCtrl_GetClockSwitch		(MCDRV_CLKSW_INFO* psClockInfo);
void			McResCtrl_SetDitSwap			(const MCDRV_DITSWAP_INFO* psDitSwapInfo, UINT32 dUpdateInfo);
void			McResCtrl_GetDitSwap			(MCDRV_DITSWAP_INFO* psDitSwapInfo);

void			McResCtrl_GetVolReg				(MCDRV_VOL_INFO* psVolInfo);
SINT16			McResCtrl_GetDigitalVolReg		(SINT32 sdVol);
void			McResCtrl_GetPowerInfo			(MCDRV_POWER_INFO* psPowerInfo);
void			McResCtrl_GetPowerInfoRegAccess	(const MCDRV_REG_INFO* psRegInfo, MCDRV_POWER_INFO* psPowerInfo);
void			McResCtrl_GetCurPowerInfo		(MCDRV_POWER_INFO* psPowerInfo);
MCDRV_SRC_TYPE	McResCtrl_GetPMSource			(void);
MCDRV_SRC_TYPE	McResCtrl_GetDITSource			(MCDRV_DIO_PORT_NO ePort);
MCDRV_SRC_TYPE	McResCtrl_GetDACSource			(MCDRV_DAC_CH eCh);
MCDRV_SRC_TYPE	McResCtrl_GetAESource			(void);
MCDRV_SRC_TYPE	McResCtrl_GetCDSPSource			(MCDRV_DST_CH eCh);
UINT8			McResCtrl_IsValidCDSPSource		(void);
UINT8			McResCtrl_IsSrcUsed				(MCDRV_SRC_TYPE ePathSrc);
UINT8			McResCtrl_IsDstUsed				(MCDRV_DST_TYPE eType, MCDRV_DST_CH eCh);
UINT8			McResCtrl_GetEFIFOSourceReg		(void);
void			McResCtrl_GetEFIFOSourceRegCur	(UINT8* pbEFIFO0L, UINT8* pbEFIFO0R, UINT8* pbEFIFO1L, UINT8* pbEFIFO1R);
UINT8			McResCtrl_IsEFIFOSourceChanged	(void);
UINT8			McResCtrl_GetEFIFO0LSource		(void);
UINT8			McResCtrl_GetEFIFO0RSource		(void);
UINT8			McResCtrl_GetEFIFO1LSource		(void);
UINT8			McResCtrl_GetEFIFO1RSource		(void);

MCDRV_REG_ACCSESS	McResCtrl_GetRegAccess		(const MCDRV_REG_INFO* psRegInfo);

MCDRV_PMODE		McResCtrl_GetAPMode				(void);

MCDRV_PACKET*	McResCtrl_AllocPacketBuf		(void);
void			McResCtrl_ReleasePacketBuf		(void);

void			McResCtrl_InitRegUpdate			(void);
void			McResCtrl_AddRegUpdate			(UINT16 wRegType, UINT16 wAddress, UINT8 bData, MCDRV_UPDATE_MODE eUpdateMode);
void			McResCtrl_ExecuteRegUpdate		(void);
SINT32			McResCtrl_WaitEvent				(UINT32 dEvent, UINT32 dParam);

UINT8			McResCtrl_IsEnableIRQ			(void);

SINT32			McResCtrl_IncIrqDisableCount	(void);
SINT32			McResCtrl_DecIrqDisableCount	(void);


#endif /* _MCRESCTRL_H_ */
