/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcdriver.c
 *
 *		Description	: MC Driver
 *
 *		Version		: 2.0.3 	2011.11.01
 *
 ****************************************************************************/


#include "mcdriver.h"
#include "mcservice.h"
#include "mcdevif.h"
#include "mcresctrl.h"
#include "mcdefs.h"
#include "mcdevprof.h"
#include "mcmachdep.h"
#if MCDRV_DEBUG_LEVEL
#include "mcdebuglog.h"
#endif
#ifdef	MCDRV_USE_CDSP_DRIVER
#include "mccdspdrv.h"
#endif




#define	MCDRV_MAX_WAIT_TIME	((UINT32)0x0FFFFFFF)
#define	MCDRV_PM_OVERFLOW	(0x7F00)

static const UINT16 gawPeakLevel[] =
{
	0x8000, 0x9431, 0x9A36, 0x9DBC, 0xA03C, 0xA22C, 0xA3C1, 0xA518,
	0xA641, 0xA747, 0xA831, 0xA905, 0xA9C6, 0xAA78, 0xAB1D, 0xABB7,
	0xAC46, 0xAD4C, 0xAE36, 0xAF0A, 0xAFCC, 0xB07E, 0xB123, 0xB1BC,
	0xB24B, 0xB351, 0xB43C, 0xB510, 0xB5D1, 0xB683, 0xB728, 0xB7C1,
	0xB851, 0xB957, 0xBA41, 0xBB15, 0xBBD6, 0xBC88, 0xBD2D, 0xBDC6,
	0xBE56, 0xBF5C, 0xC046, 0xC11A, 0xC1DC, 0xC28E, 0xC332, 0xC3CC,
	0xC45B, 0xC561, 0xC64B, 0xC71F, 0xC7E1, 0xC893, 0xC938, 0xC9D1,
	0xCA61, 0xCB66, 0xCC51, 0xCD25, 0xCDE6, 0xCE98, 0xCF3D, 0xCFD6,
	0xD066, 0xD16C, 0xD256, 0xD32A, 0xD3EB, 0xD49D, 0xD542, 0xD5DC,
	0xD66B, 0xD771, 0xD85B, 0xD92F, 0xD9F1, 0xDAA3, 0xDB47, 0xDBE1,
	0xDC70, 0xDD76, 0xDE61, 0xDF34, 0xDFF6, 0xE0A8, 0xE14D, 0xE1E6,
	0xE276, 0xE37C, 0xE466, 0xE53A, 0xE5FB, 0xE6AD, 0xE752, 0xE7EB,
	0xE87B, 0xE981, 0xEA6B, 0xEB3F, 0xEC00, 0xECB2, 0xED57, 0xEDF1,
	0xEE80, 0xEF86, 0xF070, 0xF144, 0xF206, 0xF2B8, 0xF35D, 0xF3F6,
	0xF485, 0xF58B, 0xF676, 0xF74A, 0xF80B, 0xF8BD, 0xF962, 0xF9FB,
	0xFA8B, 0xFB91, 0xFC7B, 0xFD4F, 0xFE10, 0xFEC2, 0xFF67, 0x0000
};

static SINT32	init					(const MCDRV_INIT_INFO* psInitInfo);
static SINT32	term					(void);

static SINT32	read_reg				(MCDRV_REG_INFO* psRegInfo);
static SINT32	write_reg				(const MCDRV_REG_INFO* psRegInfo);

static SINT32	update_clock			(const MCDRV_CLOCK_INFO* psClockInfo);
static SINT32	switch_clock			(const MCDRV_CLKSW_INFO* psClockInfo);

static SINT32	get_path				(MCDRV_PATH_INFO* psPathInfo);
static SINT32	set_path				(const MCDRV_PATH_INFO* psPathInfo);

static SINT32	get_volume				(MCDRV_VOL_INFO* psVolInfo);
static SINT32	set_volume				(const MCDRV_VOL_INFO *psVolInfo);

static SINT32	get_digitalio			(MCDRV_DIO_INFO* psDioInfo);
static SINT32	set_digitalio			(const MCDRV_DIO_INFO* psDioInfo, UINT32 dUpdateInfo);

static SINT32	get_dac					(MCDRV_DAC_INFO* psDacInfo);
static SINT32	set_dac					(const MCDRV_DAC_INFO* psDacInfo, UINT32 dUpdateInfo);

static SINT32	get_adc					(MCDRV_ADC_INFO* psAdcInfo);
static SINT32	set_adc					(const MCDRV_ADC_INFO* psAdcInfo, UINT32 dUpdateInfo);

static SINT32	get_adc_ex				(MCDRV_ADC_EX_INFO* psAdcExInfo);
static SINT32	set_adc_ex				(const MCDRV_ADC_EX_INFO* psAdcExInfo, UINT32 dUpdateInfo);

static SINT32	get_sp					(MCDRV_SP_INFO* psSpInfo);
static SINT32	set_sp					(const MCDRV_SP_INFO* psSpInfo);

static SINT32	get_dng					(MCDRV_DNG_INFO* psDngInfo);
static SINT32	set_dng					(const MCDRV_DNG_INFO* psDngInfo, UINT32 dUpdateInfo);

static SINT32	set_ae					(const MCDRV_AE_INFO* psAeInfo, UINT32 dUpdateInfo);
static SINT32	set_ae_ex				(const UINT8* pbPrm, UINT32 dPrm);
static SINT32	set_ex_param			(MCDRV_EXPARAM* psPrm);

static SINT32	set_cdsp				(const UINT8* pbPrm, UINT32 dPrm);
static SINT32	set_cdspprm				(MCDRV_CDSPPARAM* psPrm);

static SINT32	get_pdm					(MCDRV_PDM_INFO* psPdmInfo);
static SINT32	set_pdm					(const MCDRV_PDM_INFO* psPdmInfo, UINT32 dUpdateInfo);

static SINT32	get_pdm_ex				(MCDRV_PDM_EX_INFO* psPdmExInfo);
static SINT32	set_pdm_ex				(const MCDRV_PDM_EX_INFO* psPdmExInfo, UINT32 dUpdateInfo);

static SINT32	get_dtmf				(MCDRV_DTMF_INFO* psDtmfInfo);
static SINT32	set_dtmf				(const MCDRV_DTMF_INFO* psDtmfInfo, UINT32 dUpdateInfo);

static SINT32	config_gp				(const MCDRV_GP_MODE* psGpMode);
static SINT32	mask_gp					(UINT8* pbMask, UINT32 dPadNo);
static SINT32	getset_gp				(UINT8* pbGpio, UINT32 dPadNo);

static SINT32	get_peak				(MCDRV_PEAK* psPeak);

static SINT32	get_syseq				(MCDRV_SYSEQ_INFO* psSysEq);
static SINT32	set_syseq				(const MCDRV_SYSEQ_INFO* psSysEq, UINT32 dUpdateInfo);

static SINT32	get_ditswap				(MCDRV_DITSWAP_INFO* psDitSwap);
static SINT32	set_ditswap				(const MCDRV_DITSWAP_INFO* psDitSwap, UINT32 dUpdateInfo);

static	SINT32	irq_proc				(void);

static SINT32	ValidateInitParam		(const MCDRV_INIT_INFO* psInitInfo);
static SINT32	ValidateClockParam		(const MCDRV_CLOCK_INFO* psClockInfo);
static SINT32	ValidateReadRegParam	(const MCDRV_REG_INFO* psRegInfo);
static SINT32	ValidateWriteRegParam	(const MCDRV_REG_INFO* psRegInfo);
static SINT32	ValidatePathParam		(const MCDRV_PATH_INFO* psPathInfo);
static SINT32	ValidatePathParam_Dig	(const MCDRV_PATH_INFO* psPathInfo);
static SINT32	ValidateDioParam		(const MCDRV_DIO_INFO* psDioInfo, UINT32 dUpdateInfo);
static SINT32	ValidateDacParam		(const MCDRV_DAC_INFO* psDacInfo, UINT32 dUpdateInfo);
static SINT32	ValidateAdcParam		(const MCDRV_ADC_INFO* psAdcInfo, UINT32 dUpdateInfo);
static SINT32	ValidateAdcExParam		(const MCDRV_ADC_EX_INFO* psAdcExInfo, UINT32 dUpdateInfo);
static SINT32	ValidateSpParam			(const MCDRV_SP_INFO* psSpInfo);
static SINT32	ValidateDngParam		(const MCDRV_DNG_INFO* psDngInfo, UINT32 dUpdateInfo);
static SINT32	ValidatePdmParam		(const MCDRV_PDM_INFO* psPdmInfo, UINT32 dUpdateInfo);
static SINT32	ValidatePdmExParam		(const MCDRV_PDM_EX_INFO* psPdmExInfo, UINT32 dUpdateInfo);
static SINT32	ValidateDtmfParam		(const MCDRV_DTMF_INFO* psDtmfInfo, UINT32 dUpdateInfo);
static SINT32	ValidateGpParam			(const MCDRV_GP_MODE* psGpMode);
static SINT32	ValidateMaskGp			(UINT8 bMask);
static SINT32	ValidateSysEqParam		(const MCDRV_SYSEQ_INFO* psSysEqInfo, UINT32 dUpdateInfo);
static SINT32	ValidateDitSwapParam	(const MCDRV_DITSWAP_INFO* psDitSwapInfo, UINT32 dUpdateInfo);

static SINT32	CheckDIOCommon			(const MCDRV_DIO_INFO*	psDioInfo, UINT8 bPort);
static SINT32	CheckDIODIR				(const MCDRV_DIO_INFO*	psDioInfo, UINT8 bPort, UINT8 bInterface);
static SINT32	CheckDIODIT				(const MCDRV_DIO_INFO*	psDioInfo, UINT8 bPort, UINT8 bInterface);

static SINT32	SetVol					(UINT32 dUpdate, MCDRV_VOLUPDATE_MODE eMode, UINT32* pdSVolDoneParam);
static SINT32	PreUpdatePath			(UINT16* pwDACMuteParam, UINT16* pwDITMuteParam);
static SINT32	SavePower				(void);

/****************************************************************************
 *	init
 *
 *	Description:
 *			Initialize.
 *	Arguments:
 *			psInitInfo	initialize information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	init
(
	const MCDRV_INIT_INFO	*psInitInfo
)
{
	SINT32	sdRet;
	UINT8	bReg;
	UINT8	abData[4];

	if(NULL == psInitInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT != McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McSrv_SystemInit();
	McSrv_Lock();
	McResCtrl_Init();

	bReg	= McSrv_ReadI2C(MCDRV_SLAVEADDR_3AH, (UINT32)MCI_HW_ID);
	if(bReg == MCDRV_HWID_14H)
	{
		if((psInitInfo->bLdo == MCDRV_LDO_AOFF_DON) || (psInitInfo->bLdo == MCDRV_LDO_AON_DON))
		{
			abData[0]	= MCI_ANA_ADR<<1;
			abData[1]	= MCI_ANA_RST;
			abData[2]	= MCI_ANA_WINDOW<<1;
			abData[3]	= MCI_ANA_RST_DEF;
			McSrv_WriteI2C(MCDRV_SLAVEADDR_3AH, abData, 4);
			abData[3]	= 0;
			McSrv_WriteI2C(MCDRV_SLAVEADDR_3AH, abData, 4);
			abData[1]	= MCI_PWM_ANALOG_0;
			abData[3]	= MCI_PWM_ANALOG_0_DEF;
			McSrv_WriteI2C(MCDRV_SLAVEADDR_3AH, abData, 4);
			McSrv_Sleep(1000UL);	/*	wait 1ms	*/
			abData[0]	= MCI_RST;
			abData[1]	= MCB_RST;
			McSrv_WriteI2C(MCDRV_SLAVEADDR_11H, abData, 4);
			abData[1]	= 0;
			McSrv_WriteI2C(MCDRV_SLAVEADDR_11H, abData, 4);
		}
		bReg	= McSrv_ReadI2C(MCDRV_SLAVEADDR_11H, (UINT32)MCI_HW_ID);
	}
	sdRet	= McResCtrl_SetHwId(bReg);
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= ValidateInitParam(psInitInfo);
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		McResCtrl_SetInitInfo(psInitInfo);
		sdRet	= McDevIf_AllocPacketBuf();
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McPacket_AddInit();
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McDevIf_ExecutePacket();
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		McResCtrl_UpdateState(eMCDRV_STATE_READY);
	}
	else
	{
		McDevIf_ReleasePacketBuf();
	}

	McSrv_Unlock();

	if(sdRet != MCDRV_SUCCESS)
	{
		McSrv_SystemTerm();
	}

	return sdRet;
}

/****************************************************************************
 *	term
 *
 *	Description:
 *			Terminate.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	term
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bCh, bSrcIdx;
	UINT8	abOnOff[SOURCE_BLOCK_NUM];
	MCDRV_INIT_INFO		sInitInfo;
	MCDRV_PATH_INFO		sPathInfo;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;

	if(eMCDRV_STATE_READY != McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McSrv_Lock();

	McResCtrl_GetInitInfo(&sInitInfo);
	sInitInfo.bPowerMode	= MCDRV_POWMODE_FULL;
	McResCtrl_SetInitInfo(&sInitInfo);

	abOnOff[0]	= (MCDRV_SRC0_MIC1_OFF|MCDRV_SRC0_MIC2_OFF|MCDRV_SRC0_MIC3_OFF);
	abOnOff[1]	= (MCDRV_SRC1_LINE1_L_OFF|MCDRV_SRC1_LINE1_R_OFF|MCDRV_SRC1_LINE1_M_OFF);
	abOnOff[2]	= (MCDRV_SRC2_LINE2_L_OFF|MCDRV_SRC2_LINE2_R_OFF|MCDRV_SRC2_LINE2_M_OFF);
	abOnOff[3]	= (MCDRV_SRC3_DIR0_OFF|MCDRV_SRC3_DIR1_OFF|MCDRV_SRC3_DIR2_OFF|MCDRV_SRC3_DIR2_DIRECT_OFF);
	abOnOff[4]	= (MCDRV_SRC4_DTMF_OFF|MCDRV_SRC4_PDM_OFF|MCDRV_SRC4_ADC0_OFF|MCDRV_SRC4_ADC1_OFF);
	abOnOff[5]	= (MCDRV_SRC5_DAC_L_OFF|MCDRV_SRC5_DAC_R_OFF|MCDRV_SRC5_DAC_M_OFF);
	abOnOff[6]	= (MCDRV_SRC6_MIX_OFF|MCDRV_SRC6_AE_OFF|MCDRV_SRC6_CDSP_OFF|MCDRV_SRC6_CDSP_DIRECT_OFF);

	for(bSrcIdx = 0; bSrcIdx < SOURCE_BLOCK_NUM; bSrcIdx++)
	{
		for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asHpOut[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asSpOut[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asRcOut[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asLout1[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asLout2[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asPeak[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asDit0[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asDit1[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asDit2[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asDac[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asAe[bCh].abSrcOnOff[bSrcIdx]		= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asCdsp[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asAdc0[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asAdc1[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asMix[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < BIAS_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asBias[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
		for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++)
		{
			sPathInfo.asMix2[bCh].abSrcOnOff[bSrcIdx]	= abOnOff[bSrcIdx];
		}
	}
	sdRet	= set_path(&sPathInfo);
	if(sdRet == MCDRV_SUCCESS)
	{
		if(McResCtrl_GetAeEx() != NULL)
		{
			set_ae_ex(NULL, 0);
		}
		if(McResCtrl_GetCdspFw() != NULL)
		{
			set_cdsp(NULL, 0);
		}

		sPowerInfo.dDigital			= (MCDRV_POWINFO_DIGITAL_DP0
									  |MCDRV_POWINFO_DIGITAL_DP1
									  |MCDRV_POWINFO_DIGITAL_DP2
									  |MCDRV_POWINFO_DIGITAL_DPB
									  |MCDRV_POWINFO_DIGITAL_DPC
									  |MCDRV_POWINFO_DIGITAL_DPDI0
									  |MCDRV_POWINFO_DIGITAL_DPDI1
									  |MCDRV_POWINFO_DIGITAL_DPDI2
									  |MCDRV_POWINFO_DIGITAL_DPCD
									  |MCDRV_POWINFO_DIGITAL_DPPDM
									  |MCDRV_POWINFO_DIGITAL_DPBDSP
									  |MCDRV_POWINFO_DIGITAL_DPADIF
									  |MCDRV_POWINFO_DIGITAL_PLLRST0);
		sPowerInfo.abAnalog[0]		=
		sPowerInfo.abAnalog[1]		=
		sPowerInfo.abAnalog[2]		=
		sPowerInfo.abAnalog[3]		=
		sPowerInfo.abAnalog[4]		= 
		sPowerInfo.abAnalog[5]		= (UINT8)0xFF;
		sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
		sPowerUpdate.abAnalog[0]	= (UINT8)MCDRV_POWUPDATE_ANALOG0_ALL;
		sPowerUpdate.abAnalog[1]	= (UINT8)MCDRV_POWUPDATE_ANALOG1_ALL;
		sPowerUpdate.abAnalog[2]	= (UINT8)MCDRV_POWUPDATE_ANALOG2_ALL;
		sPowerUpdate.abAnalog[3]	= (UINT8)MCDRV_POWUPDATE_ANALOG3_ALL;
		sPowerUpdate.abAnalog[4]	= (UINT8)MCDRV_POWUPDATE_ANALOG4_ALL;
		sPowerUpdate.abAnalog[5]	= (UINT8)MCDRV_POWUPDATE_ANALOG5_ALL;
		sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}

	McDevIf_ReleasePacketBuf();

	McResCtrl_UpdateState(eMCDRV_STATE_NOTINIT);

	McSrv_Unlock();

	McSrv_SystemTerm();

	return sdRet;
}

/****************************************************************************
 *	read_reg
 *
 *	Description:
 *			read register.
 *	Arguments:
 *			psRegInfo	register information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	read_reg
(
	MCDRV_REG_INFO*	psRegInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bSlaveAddr;
	UINT8	bAddr;
	UINT8	abData[2];
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_INFO	sCurPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;

	if(NULL == psRegInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateReadRegParam(psRegInfo);
	if(sdRet != MCDRV_SUCCESS)
	{
		return sdRet;
	}

	/*	get current power info	*/
	McResCtrl_GetCurPowerInfo(&sCurPowerInfo);

	/*	power up	*/
	McResCtrl_GetPowerInfoRegAccess(psRegInfo, &sPowerInfo);
	sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
	sPowerUpdate.abAnalog[0]	= (UINT8)MCDRV_POWUPDATE_ANALOG0_ALL;
	sPowerUpdate.abAnalog[1]	= (UINT8)MCDRV_POWUPDATE_ANALOG1_ALL;
	sPowerUpdate.abAnalog[2]	= (UINT8)MCDRV_POWUPDATE_ANALOG2_ALL;
	sPowerUpdate.abAnalog[3]	= (UINT8)MCDRV_POWUPDATE_ANALOG3_ALL;
	sPowerUpdate.abAnalog[4]	= (UINT8)MCDRV_POWUPDATE_ANALOG4_ALL;
	sPowerUpdate.abAnalog[5]	= (UINT8)MCDRV_POWUPDATE_ANALOG5_ALL;
	sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	bAddr	= psRegInfo->bAddress;

	if(psRegInfo->bRegType == MCDRV_REGTYPE_A)
	{
		if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
		{
			if((psRegInfo->bAddress == MCI_ANA_ADR)
			|| (psRegInfo->bAddress == MCI_ANA_WINDOW)
			|| (psRegInfo->bAddress == MCI_CD_ADR)
			|| (psRegInfo->bAddress == MCI_CD_WINDOW))
			{
				bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
			}
			else
			{
				bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
			}
		}
		else
		{
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		}
	}
	else
	{
		switch(psRegInfo->bRegType)
		{
		case	MCDRV_REGTYPE_B_BASE:
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
			abData[0]	= MCI_BASE_ADR<<1;
			abData[1]	= bAddr;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			bAddr	= MCI_BASE_WINDOW;
			break;

		case	MCDRV_REGTYPE_B_MIXER:
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
			abData[0]	= MCI_MIX_ADR<<1;
			abData[1]	= bAddr;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			bAddr	= MCI_MIX_WINDOW;
			break;

		case	MCDRV_REGTYPE_B_AE:
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
			abData[0]	= MCI_AE_ADR<<1;
			abData[1]	= bAddr;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			bAddr	= MCI_AE_WINDOW;
			break;

		case	MCDRV_REGTYPE_B_CDSP:
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
			abData[0]	= MCI_CDSP_ADR<<1;
			abData[1]	= bAddr;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			bAddr	= MCI_CDSP_WINDOW;
			break;

		case	MCDRV_REGTYPE_B_CODEC:
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
			abData[0]	= MCI_CD_ADR<<1;
			abData[1]	= bAddr;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			bAddr	= MCI_CD_WINDOW;
			break;

		case	MCDRV_REGTYPE_B_ANALOG:
			bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
			abData[0]	= MCI_ANA_ADR<<1;
			abData[1]	= bAddr;
			McSrv_WriteI2C(bSlaveAddr, abData, 2);
			bAddr	= MCI_ANA_WINDOW;
			break;

		default:
			return MCDRV_ERROR_ARGUMENT;
		}
	}

	/*	read register	*/
	psRegInfo->bData	= McSrv_ReadI2C(bSlaveAddr, bAddr);

	/*	restore power	*/
	sdRet	= McPacket_AddPowerDown(&sCurPowerInfo, &sPowerUpdate);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	write_reg
 *
 *	Description:
 *			Write register.
 *	Arguments:
 *			psWR	register information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_RESOURCEOVER
 *
 ****************************************************************************/
static	SINT32	write_reg
(
	const MCDRV_REG_INFO*	psRegInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_INFO	sCurPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	UINT8	abData[2];

	if(NULL == psRegInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateWriteRegParam(psRegInfo);
	if(sdRet != MCDRV_SUCCESS)
	{
		return sdRet;
	}

	/*	get current power info	*/
	McResCtrl_GetCurPowerInfo(&sCurPowerInfo);

	/*	power up	*/
	McResCtrl_GetPowerInfoRegAccess(psRegInfo, &sPowerInfo);
	sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
	sPowerUpdate.abAnalog[0]	= (UINT8)MCDRV_POWUPDATE_ANALOG0_ALL;
	sPowerUpdate.abAnalog[1]	= (UINT8)MCDRV_POWUPDATE_ANALOG1_ALL;
	sPowerUpdate.abAnalog[2]	= (UINT8)MCDRV_POWUPDATE_ANALOG2_ALL;
	sPowerUpdate.abAnalog[3]	= (UINT8)MCDRV_POWUPDATE_ANALOG3_ALL;
	sPowerUpdate.abAnalog[4]	= (UINT8)MCDRV_POWUPDATE_ANALOG4_ALL;
	sPowerUpdate.abAnalog[5]	= (UINT8)MCDRV_POWUPDATE_ANALOG5_ALL;
	sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	switch(psRegInfo->bRegType)
	{
	case	MCDRV_REGTYPE_A:
		if((psRegInfo->bAddress == MCI_ANA_ADR)
		|| (psRegInfo->bAddress == MCI_ANA_WINDOW)
		|| (psRegInfo->bAddress == MCI_CD_ADR)
		|| (psRegInfo->bAddress == MCI_CD_WINDOW))
		{
			abData[0]	= psRegInfo->bAddress<<1;
			abData[1]	= psRegInfo->bData;
			McSrv_WriteI2C(McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA), abData, 2);
		}
		else
		{
			McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | psRegInfo->bAddress), psRegInfo->bData);
		}
		break;

	case	MCDRV_REGTYPE_B_BASE:
		McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | psRegInfo->bAddress), psRegInfo->bData);
		break;

	case	MCDRV_REGTYPE_B_MIXER:
		McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | psRegInfo->bAddress), psRegInfo->bData);
		break;

	case	MCDRV_REGTYPE_B_AE:
		McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | psRegInfo->bAddress), psRegInfo->bData);
		break;

	case	MCDRV_REGTYPE_B_CDSP:
		McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | psRegInfo->bAddress), psRegInfo->bData);
		break;

	case	MCDRV_REGTYPE_B_CODEC:
		McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | psRegInfo->bAddress), psRegInfo->bData);
		break;

	case	MCDRV_REGTYPE_B_ANALOG:
		McDevIf_AddPacket((MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | psRegInfo->bAddress), psRegInfo->bData);
		break;

	default:
		return MCDRV_ERROR_ARGUMENT;
	}

	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	restore power	*/
	if(psRegInfo->bRegType == MCDRV_REGTYPE_B_BASE)
	{
		if(psRegInfo->bAddress == MCI_PWM_DIGITAL)
		{
			if((psRegInfo->bData & MCB_PWM_DP1) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DP1;
			}
			if((psRegInfo->bData & MCB_PWM_DP2) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DP2;
			}
		}
		else if(psRegInfo->bAddress == MCI_PWM_DIGITAL_1)
		{
			if((psRegInfo->bData & MCB_PWM_DPB) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPB;
			}
			if((psRegInfo->bData & MCB_PWM_DPC) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPC;
			}
			if((psRegInfo->bData & MCB_PWM_DPDI0) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPDI0;
			}
			if((psRegInfo->bData & MCB_PWM_DPDI1) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPDI1;
			}
			if((psRegInfo->bData & MCB_PWM_DPDI2) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPDI2;
			}
			if((psRegInfo->bData & MCB_PWM_DPPDM) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPPDM;
			}
			if((psRegInfo->bData & MCB_PWM_DPCD) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPCD;
			}
		}
		else if(psRegInfo->bAddress == MCI_PWM_DIGITAL_BDSP)
		{
			if((psRegInfo->bData & MCB_PWM_DPBDSP) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPBDSP;
			}
		}
		else
		{
		}
	}
	else if(psRegInfo->bRegType == MCDRV_REGTYPE_B_CODEC)
	{
		if(psRegInfo->bAddress == MCI_CD_DP)
		{
			if((psRegInfo->bData & MCB_DPADIF) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DPADIF;
			}
			if((psRegInfo->bData & MCB_DP0_CLKI0) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DP0;
			}
			if((psRegInfo->bData & MCB_DP0_CLKI1) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_DP0;
			}
		}
		if(psRegInfo->bAddress == MCI_PLL_RST)
		{
			if((psRegInfo->bData & MCB_PLLRST0) == 0)
			{
				sCurPowerInfo.dDigital &= ~MCDRV_POWINFO_DIGITAL_PLLRST0;
			}
		}
	}
	else if(psRegInfo->bRegType == MCDRV_REGTYPE_B_ANALOG)
	{
		if(psRegInfo->bAddress == MCI_PWM_ANALOG_0)
		{
			sCurPowerInfo.abAnalog[0]	= psRegInfo->bData;
		}
		else if(psRegInfo->bAddress == MCI_PWM_ANALOG_1)
		{
			sCurPowerInfo.abAnalog[1]	= psRegInfo->bData;
		}
		else if(psRegInfo->bAddress == MCI_PWM_ANALOG_2)
		{
			sCurPowerInfo.abAnalog[2]	= psRegInfo->bData;
		}
		else if(psRegInfo->bAddress == MCI_PWM_ANALOG_3)
		{
			sCurPowerInfo.abAnalog[3]	= psRegInfo->bData;
		}
		else if(psRegInfo->bAddress == MCI_PWM_ANALOG_4)
		{
			sCurPowerInfo.abAnalog[4]	= psRegInfo->bData;
		}
		else if(psRegInfo->bAddress == MCI_PWM_ANALOG_5)
		{
			sCurPowerInfo.abAnalog[5]	= psRegInfo->bData;
		}
		else
		{
		}
	}
	else
	{
	}

	sdRet	= McPacket_AddPowerDown(&sCurPowerInfo, &sPowerUpdate);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	update_clock
 *
 *	Description:
 *			Update clock info.
 *	Arguments:
 *			psClockInfo	clock info
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	update_clock
(
	const MCDRV_CLOCK_INFO*	psClockInfo
)
{
	SINT32			sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE		eState	= McResCtrl_GetState();
	MCDRV_INIT_INFO	sInitInfo;

	if(NULL == psClockInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetInitInfo(&sInitInfo);
	if((sInitInfo.bPowerMode & MCDRV_POWMODE_CLKON) != 0)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	sdRet	= ValidateClockParam(psClockInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetClockInfo(psClockInfo);
	return sdRet;
}

/****************************************************************************
 *	switch_clock
 *
 *	Description:
 *			Switch clock.
 *	Arguments:
 *			psClockInfo	clock info
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	switch_clock
(
	const MCDRV_CLKSW_INFO*	psClockInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psClockInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if((psClockInfo->bClkSrc != MCDRV_CLKSW_CLKI0) && (psClockInfo->bClkSrc != MCDRV_CLKSW_CLKI1))
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	McResCtrl_SetClockSwitch(psClockInfo);
	
	sdRet	= McPacket_AddClockSwitch();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	get_path
 *
 *	Description:
 *			Get current path setting.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_path
(
	MCDRV_PATH_INFO*	psPathInfo
)
{
	if(NULL == psPathInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetPathInfoVirtual(psPathInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_path
 *
 *	Description:
 *			Set path.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_path
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	SINT32				sdRet	= MCDRV_SUCCESS;
	UINT32				dSVolDoneParam	= 0;
	UINT16				wDACMuteParam	= 0;
	UINT16				wDITMuteParam	= 0;
	MCDRV_STATE			eState	= McResCtrl_GetState();
	MCDRV_PATH_INFO		sPathInfo;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;

	if(NULL == psPathInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sPathInfo	= *psPathInfo;
	McDevProf_MaskPathInfo(&sPathInfo);

	sdRet	= ValidatePathParam(&sPathInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	sdRet	= McResCtrl_SetPathInfo(&sPathInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	unused analog out volume mute	*/
	sdRet	= SetVol(MCDRV_VOLUPDATE_ANAOUT_ALL, eMCDRV_VOLUPDATE_MUTE, &dSVolDoneParam);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	DAC/DIT* mute	*/
	sdRet	= PreUpdatePath(&wDACMuteParam, &wDITMuteParam);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	unused volume mute	*/
	sdRet	= SetVol(MCDRV_VOLUPDATE_ALL&~MCDRV_VOLUPDATE_ANAOUT_ALL, eMCDRV_VOLUPDATE_MUTE, NULL);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	wait XX_BUSY	*/
	if(dSVolDoneParam != (UINT32)0)
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_SVOL_DONE | dSVolDoneParam, 0);
	}
	if(wDACMuteParam != 0)
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_DACMUTE | wDACMuteParam, 0);
	}
	if(wDITMuteParam != 0)
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_DITMUTE | wDITMuteParam, 0);
	}

	/*	stop unused path	*/
	sdRet	= McPacket_AddStop();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_GetPowerInfo(&sPowerInfo);

	/*	used path power up	*/
	sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
	sPowerUpdate.abAnalog[0]	= (UINT8)MCDRV_POWUPDATE_ANALOG0_ALL;
	sPowerUpdate.abAnalog[1]	= (UINT8)MCDRV_POWUPDATE_ANALOG1_ALL;
	sPowerUpdate.abAnalog[2]	= (UINT8)MCDRV_POWUPDATE_ANALOG2_ALL;
	sPowerUpdate.abAnalog[3]	= (UINT8)MCDRV_POWUPDATE_ANALOG3_ALL;
	sPowerUpdate.abAnalog[4]	= (UINT8)MCDRV_POWUPDATE_ANALOG4_ALL;
	sPowerUpdate.abAnalog[5]	= (UINT8)MCDRV_POWUPDATE_ANALOG5_ALL;
	sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	set digital mixer	*/
	sdRet	= McPacket_AddPathSet();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	set analog mixer	*/
	sdRet	= McPacket_AddMixSet();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	unused path power down	*/
	sdRet	= SavePower();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	start	*/
	sdRet	= McPacket_AddStart();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	/*	set volume	*/
	sdRet	= SetVol(MCDRV_VOLUPDATE_ALL&~MCDRV_VOLUPDATE_ANAOUT_ALL, eMCDRV_VOLUPDATE_ALL, NULL);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return SetVol(MCDRV_VOLUPDATE_ANAOUT_ALL, eMCDRV_VOLUPDATE_ALL, &dSVolDoneParam);
}

/****************************************************************************
 *	get_volume
 *
 *	Description:
 *			Get current volume setting.
 *	Arguments:
 *			psVolInfo	volume information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	get_volume
(
	MCDRV_VOL_INFO*	psVolInfo
)
{
	if(NULL == psVolInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetVolInfo(psVolInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_volume
 *
 *	Description:
 *			Set volume.
 *	Arguments:
 *			psVolInfo	volume update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	set_volume
(
	const MCDRV_VOL_INFO*	psVolInfo
)
{
	MCDRV_STATE	eState	= McResCtrl_GetState();
	MCDRV_PATH_INFO	sPathInfo;

	if(NULL == psVolInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_SetVolInfo(psVolInfo);

	McResCtrl_GetPathInfoVirtual(&sPathInfo);
	return 	set_path(&sPathInfo);
}

/****************************************************************************
 *	get_digitalio
 *
 *	Description:
 *			Get current digital IO setting.
 *	Arguments:
 *			psDioInfo	digital IO information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_digitalio
(
	MCDRV_DIO_INFO*	psDioInfo
)
{
	if(NULL == psDioInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetDioInfo(psDioInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_digitalio
 *
 *	Description:
 *			Update digital IO configuration.
 *	Arguments:
 *			psDioInfo	digital IO configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_digitalio
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(NULL == psDioInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) == 0)
	{
		dUpdateInfo	&= ~(MCDRV_DIO2_COM_UPDATE_FLAG | MCDRV_DIO2_DIR_UPDATE_FLAG | MCDRV_DIO2_DIT_UPDATE_FLAG);
	}

	sdRet	= ValidateDioParam(psDioInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetDioInfo(psDioInfo, dUpdateInfo);

	sdRet	= McPacket_AddDigitalIO(dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	get_dac
 *
 *	Description:
 *			Get current DAC setting.
 *	Arguments:
 *			psDacInfo	DAC information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_dac
(
	MCDRV_DAC_INFO*	psDacInfo
)
{
	if(NULL == psDacInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetDacInfo(psDacInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_dac
 *
 *	Description:
 *			Update DAC configuration.
 *	Arguments:
 *			psDacInfo	DAC configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_dac
(
	const MCDRV_DAC_INFO*	psDacInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(NULL == psDacInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateDacParam(psDacInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetDacInfo(psDacInfo, dUpdateInfo);

	sdRet	= McPacket_AddDAC(dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	get_adc
 *
 *	Description:
 *			Get current ADC setting.
 *	Arguments:
 *			psAdcInfo	ADC information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_adc
(
	MCDRV_ADC_INFO*	psAdcInfo
)
{
	if(NULL == psAdcInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetAdcInfo(psAdcInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_adc
 *
 *	Description:
 *			Update ADC configuration.
 *	Arguments:
 *			psAdcInfo	ADC configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_adc
(
	const MCDRV_ADC_INFO*	psAdcInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(NULL == psAdcInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateAdcParam(psAdcInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetAdcInfo(psAdcInfo, dUpdateInfo);

	sdRet	= McPacket_AddADC(dUpdateInfo);
	if(MCDRV_SUCCESS == sdRet)
	{
		sdRet	= McDevIf_ExecutePacket();
	}
	if(MCDRV_SUCCESS == sdRet)
	{
		sdRet	= SetVol(MCDRV_VOLUPDATE_ALL&~MCDRV_VOLUPDATE_ANAOUT_ALL, eMCDRV_VOLUPDATE_ALL, NULL);
	}
	return sdRet;
}

/****************************************************************************
 *	get_adc_ex
 *
 *	Description:
 *			Get current ADC_EX setting.
 *	Arguments:
 *			psAdcExInfo	ADC_EX information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_adc_ex
(
	MCDRV_ADC_EX_INFO*	psAdcExInfo
)
{
	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psAdcExInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetAdcExInfo(psAdcExInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_adc_ex
 *
 *	Description:
 *			Update ADC_EX configuration.
 *	Arguments:
 *			psAdcExInfo	ADC_EX configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_adc_ex
(
	const MCDRV_ADC_EX_INFO*	psAdcExInfo,
	UINT32						dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psAdcExInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateAdcExParam(psAdcExInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetAdcExInfo(psAdcExInfo, dUpdateInfo);

	sdRet	= McPacket_AddADCEx(dUpdateInfo);
	if(MCDRV_SUCCESS == sdRet)
	{
		sdRet	= McDevIf_ExecutePacket();
	}
	return sdRet;
}

/****************************************************************************
 *	get_sp
 *
 *	Description:
 *			Get current SP setting.
 *	Arguments:
 *			psSpInfo	SP information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_sp
(
	MCDRV_SP_INFO*	psSpInfo
)
{
	if(NULL == psSpInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetSpInfo(psSpInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_sp
 *
 *	Description:
 *			Update SP configuration.
 *	Arguments:
 *			psSpInfo	SP configuration
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_sp
(
	const MCDRV_SP_INFO*	psSpInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();
	MCDRV_PATH_INFO	sPathInfo;

	if(NULL == psSpInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateSpParam(psSpInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetSpInfo(psSpInfo);

	McResCtrl_GetPathInfoVirtual(&sPathInfo);
	return 	set_path(&sPathInfo);
}

/****************************************************************************
 *	get_dng
 *
 *	Description:
 *			Get current Digital Noise Gate setting.
 *	Arguments:
 *			psDngInfo	DNG information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_dng
(
	MCDRV_DNG_INFO*	psDngInfo
)
{
	if(McDevProf_IsValid(eMCDRV_FUNC_DNG) == 0)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psDngInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetDngInfo(psDngInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_dng
 *
 *	Description:
 *			Update Digital Noise Gate configuration.
 *	Arguments:
 *			psDngInfo	DNG configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_dng
(
	const MCDRV_DNG_INFO*	psDngInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_DNG) == 0)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psDngInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateDngParam(psDngInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetDngInfo(psDngInfo, dUpdateInfo);

	sdRet	= McPacket_AddDNG(dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	set_ae
 *
 *	Description:
 *			Update Audio Engine configuration.
 *	Arguments:
 *			psAeInfo	AE configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_ae
(
	const MCDRV_AE_INFO*	psAeInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();
	MCDRV_PATH_INFO	sPathInfo;

	if(NULL == psAeInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_SetAeInfo(psAeInfo, dUpdateInfo);

	sdRet	= McPacket_AddAE(dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	sdRet	= McDevIf_ExecutePacket();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_GetPathInfoVirtual(&sPathInfo);
	return 	set_path(&sPathInfo);
}

/****************************************************************************
 *	set_ae_ex
 *
 *	Description:
 *			Set Audio Engine ex program.
 *	Arguments:
 *			pbPrm	pointer to Audio Engine ex program
 *			dPrm	Audio Engine ex program byte size
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static SINT32	set_ae_ex
(
	const UINT8*	pbPrm,
	UINT32			dPrm
)
{
#ifndef MCDRV_USE_CDSP_DRIVER
	(void)pbPrm;
	(void)dPrm;
	return MCDRV_ERROR;
#else
	SINT32				sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE			eState	= McResCtrl_GetState();
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_PATH_INFO		sPathInfo;
	MC_CODER_FIRMWARE	sProg;
	const UINT8*		pbPrmCur;

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if((pbPrm != NULL) && (dPrm == 0UL))
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if((McResCtrl_GetCdspFw() != NULL)
	|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0))
	{
		return MCDRV_ERROR;
	}

	pbPrmCur	= McResCtrl_GetAeEx();
	McResCtrl_SetAeEx(pbPrm);

	sdRet	= McPacket_AddAeEx();
	if(MCDRV_SUCCESS == sdRet)
	{
		sdRet	= McDevIf_ExecutePacket();
	}
	if(MCDRV_SUCCESS == sdRet)
	{
		if(pbPrm != NULL)
		{
			McResCtrl_GetPowerInfo(&sPowerInfo);
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
			if(sdRet == MCDRV_SUCCESS)
			{
				sdRet	= McDevIf_ExecutePacket();
			}
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		sProg.pbFirmware	= pbPrm;
		sProg.dSize			= dPrm & (UINT32)0x0FFFFFFF;
		sdRet	= McCdsp_TransferProgram(eMC_PLAYER_CODER_B, &sProg);
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if(sdRet == MCDRV_SUCCESS)
		{
			McResCtrl_GetPathInfoVirtual(&sPathInfo);
			sdRet	= set_path(&sPathInfo);
		}
		McResCtrl_SetCdspFs(0);
	}
	else
	{
		McResCtrl_SetAeEx(pbPrmCur);
	}

	SavePower();
	return sdRet;
#endif
}

/****************************************************************************
 *	set_ex_param
 *
 *	Description:
 *			Set Audio Enging ex parameter.
 *	Arguments:
 *			psPrm	pointer to Audio Enging ex parameter
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static SINT32	set_ex_param
(
	MCDRV_EXPARAM*	psPrm
)
{
#ifndef MCDRV_USE_CDSP_DRIVER
	(void)psPrm;
	return MCDRV_ERROR;
#else
	SINT32			sdRet	= MCDRV_SUCCESS;
	MC_CODER_PARAMS	sPrm;
	MCDRV_STATE		eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if((McResCtrl_GetCdspFw() != NULL)
	|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0))
	{
		return MCDRV_ERROR;
	}

	if(NULL == psPrm)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if((psPrm->bCommand == CDSP_CMD_HOST2OS_SYS_SET_FORMAT)
	|| (psPrm->bCommand == CDSP_CMD_DRV_SYS_SET_FIFO_CHANNELS))
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	sPrm.bCommandId	= psPrm->bCommand;
	McSrv_MemCopy(psPrm->abParam, sPrm.abParam, CDSP_CMD_PARAM_NUM);
	sdRet	= McCdsp_SetParam(eMC_PLAYER_CODER_B, &sPrm);
	return sdRet;
#endif
}

/****************************************************************************
 *	set_cdsp
 *
 *	Description:
 *			Set C-DSP firmware.
 *	Arguments:
 *			pbPrm	pointer to C-DSP firmware
 *			dPrm	C-DSP firmware byte size.
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static SINT32	set_cdsp
(
	const UINT8*	pbPrm,
	UINT32			dPrm
)
{
#ifndef MCDRV_USE_CDSP_DRIVER
	(void)pbPrm;
	(void)dPrm;
	return MCDRV_ERROR;
#else
	SINT32				sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE			eState	= McResCtrl_GetState();
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MC_CODER_FIRMWARE	sProg;
	const UINT8*		pbPrmCur;

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(McResCtrl_GetAeEx() != NULL)
	{
		return MCDRV_ERROR;
	}

	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
	{
		return MCDRV_ERROR;
	}

	if((pbPrm != NULL) && (dPrm == 0UL))
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	pbPrmCur	= McResCtrl_GetCdspFw();
	McResCtrl_SetCdspFw(pbPrm);

	if(pbPrm != NULL)
	{
		McResCtrl_GetPowerInfo(&sPowerInfo);
		sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
		sPowerUpdate.abAnalog[0]	= 
		sPowerUpdate.abAnalog[1]	= 
		sPowerUpdate.abAnalog[2]	= 
		sPowerUpdate.abAnalog[3]	= 
		sPowerUpdate.abAnalog[4]	= 
		sPowerUpdate.abAnalog[5]	= 0;
		sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		sProg.pbFirmware	= pbPrm;
		sProg.dSize			= dPrm & (UINT32)0x0FFFFFFF;
		sdRet	= McCdsp_TransferProgram(eMC_PLAYER_CODER_A, &sProg);
		if(sdRet == MCDRV_SUCCESS)
		{
			McResCtrl_SetCdspFs(0);
		}
	}
	if(sdRet != MCDRV_SUCCESS)
	{
		McResCtrl_SetCdspFw(pbPrmCur);
	}

	SavePower();
	return sdRet;
#endif
}

/****************************************************************************
 *	set_cdspprm
 *
 *	Description:
 *			Set C-DSP parameter.
 *	Arguments:
 *			psPrm	pointer to C-DSP parameter
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static SINT32	set_cdspprm
(
	MCDRV_CDSPPARAM*	psPrm
)
{
#ifndef MCDRV_USE_CDSP_DRIVER
	(void)psPrm;
	return MCDRV_ERROR;
#else
	SINT32			sdRet	= MCDRV_SUCCESS;
	MC_CODER_PARAMS	sPrm;
	MCDRV_STATE		eState	= McResCtrl_GetState();
	

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(McResCtrl_GetAeEx() != NULL)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psPrm)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(psPrm->bCommand == CDSP_CMD_DRV_SYS_SET_FIFO_CHANNELS)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	sPrm.bCommandId	= psPrm->bCommand;
	McSrv_MemCopy(psPrm->abParam, sPrm.abParam, CDSP_CMD_PARAM_NUM);
	sdRet	= McCdsp_SetParam(eMC_PLAYER_CODER_A, (MC_CODER_PARAMS*)psPrm);
	if((sdRet == MCDRV_SUCCESS) && (sPrm.bCommandId == CDSP_CMD_HOST2OS_SYS_SET_FORMAT))
	{
		McResCtrl_SetCdspFs(sPrm.abParam[0]);
	}
	return sdRet;
#endif
}

/****************************************************************************
 *	get_pdm
 *
 *	Description:
 *			Get current PDM setting.
 *	Arguments:
 *			psPdmInfo	PDM information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_pdm
(
	MCDRV_PDM_INFO*	psPdmInfo
)
{
	if(NULL == psPdmInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetPdmInfo(psPdmInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_pdm
 *
 *	Description:
 *			Update PDM configuration.
 *	Arguments:
 *			psPdmInfo	PDM configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_pdm
(
	const MCDRV_PDM_INFO*	psPdmInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(NULL == psPdmInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidatePdmParam(psPdmInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetPdmInfo(psPdmInfo, dUpdateInfo);

	sdRet	= McPacket_AddPDM(dUpdateInfo);
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McPacket_AddStart();
	}
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	get_pdm_ex
 *
 *	Description:
 *			Get current PDM_EX setting.
 *	Arguments:
 *			psPdmExInfo	PDM_EX information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_pdm_ex
(
	MCDRV_PDM_EX_INFO*	psPdmExInfo
)
{
	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psPdmExInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetPdmExInfo(psPdmExInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_pdm_ex
 *
 *	Description:
 *			Update PDM_EX configuration.
 *	Arguments:
 *			psPdmExInfo	PDM_EX configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_pdm_ex
(
	const MCDRV_PDM_EX_INFO*	psPdmExInfo,
	UINT32						dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(NULL == psPdmExInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidatePdmExParam(psPdmExInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetPdmExInfo(psPdmExInfo, dUpdateInfo);

	sdRet	= McPacket_AddPDMEx(dUpdateInfo);
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McPacket_AddStart();
	}
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	get_dtmf
 *
 *	Description:
 *			Get current DTMF setting.
 *	Arguments:
 *			psDtmfInfo	DTMF information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_dtmf
(
	MCDRV_DTMF_INFO*	psDtmfInfo
)
{
	if(McDevProf_IsValid(eMCDRV_FUNC_DTMF) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psDtmfInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	McResCtrl_GetDtmfInfo(psDtmfInfo);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_dtmf
 *
 *	Description:
 *			Update DTMF configuration.
 *	Arguments:
 *			psDtmfInfo	DTMF configuration
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	set_dtmf
(
	const MCDRV_DTMF_INFO*	psDtmfInfo,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_DTMF) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psDtmfInfo)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	sdRet	= ValidateDtmfParam(psDtmfInfo, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	McResCtrl_SetDtmfInfo(psDtmfInfo, dUpdateInfo);

	sdRet	= McPacket_AddDTMF(dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	config_gp
 *
 *	Description:
 *			Set GPIO mode.
 *	Arguments:
 *			psGpMode	GPIO mode information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
*
 ****************************************************************************/
static	SINT32	config_gp
(
	const MCDRV_GP_MODE*	psGpMode
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(NULL == psGpMode)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	sdRet	= ValidateGpParam(psGpMode);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	McResCtrl_SetGPMode(psGpMode);

	sdRet	= McPacket_AddGPMode();
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	mask_gp
 *
 *	Description:
 *			Set GPIO input mask.
 *	Arguments:
 *			pbMask	mask setting
 *			dPadNo	PAD number
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR
*
 ****************************************************************************/
static	SINT32	mask_gp
(
	UINT8*	pbMask,
	UINT32	dPadNo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_GP_MODE	sGPMode;

	if(NULL == pbMask)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetGPMode(&sGPMode);

	if(dPadNo == MCDRV_GP_PAD0)
	{
		if((sInitInfo.bPad0Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[dPadNo] == MCDRV_GPDDR_OUT))
		{
			return MCDRV_ERROR;
		}
	}
	else if(dPadNo == MCDRV_GP_PAD1)
	{
		if((sInitInfo.bPad1Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[dPadNo] == MCDRV_GPDDR_OUT))
		{
			return MCDRV_ERROR;
		}
	}
	else if(dPadNo == MCDRV_GP_PAD2)
	{
		if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) == 0)
		{
			return MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			if((sInitInfo.bPad2Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[dPadNo] == MCDRV_GPDDR_OUT))
			{
				return MCDRV_ERROR;
			}
		}
	}
	else
	{
		return MCDRV_ERROR;
	}

	sdRet	= ValidateMaskGp(*pbMask);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}
	McResCtrl_SetGPMask(*pbMask, dPadNo);

	sdRet	= McPacket_AddGPMask();
	if(sdRet != MCDRV_SUCCESS)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	getset_gp
 *
 *	Description:
 *			Set or get state of GPIO pin.
 *	Arguments:
 *			pbGpio	pin state
 *			dPadNo	PAD number
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
*
 ****************************************************************************/
static	SINT32	getset_gp
(
	UINT8*	pbGpio,
	UINT32	dPadNo
)
{
	SINT32			sdRet	= MCDRV_SUCCESS;
	UINT8			bSlaveAddr;
	UINT8			abData[2];
	UINT8			bRegData;
	MCDRV_STATE		eState	= McResCtrl_GetState();
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_GP_MODE	sGPMode;

	if(NULL == pbGpio)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetGPMode(&sGPMode);

	if(dPadNo >= (UINT32)GPIO_PAD_NUM)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(((dPadNo == MCDRV_GP_PAD0) && (sInitInfo.bPad0Func != MCDRV_PAD_GPIO))
	|| ((dPadNo == MCDRV_GP_PAD1) && (sInitInfo.bPad1Func != MCDRV_PAD_GPIO)))
	{
		return MCDRV_ERROR;
	}

	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		if((dPadNo == MCDRV_GP_PAD2) && (sInitInfo.bPad2Func != MCDRV_PAD_GPIO))
		{
			return MCDRV_ERROR;
		}
	}
	else
	{
		if(dPadNo == MCDRV_GP_PAD2)
		{
			return MCDRV_ERROR_ARGUMENT;
		}
	}

	if(sGPMode.abGpDdr[dPadNo] == MCDRV_GPDDR_IN)
	{
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		abData[0]	= MCI_BASE_ADR<<1;
		if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
		{
			abData[1]	= MCI_SCU_GP;
		}
		else
		{
			abData[1]	= MCI_PA_SCU_PA;
		}
		McSrv_WriteI2C(bSlaveAddr, abData, 2);
		bRegData	= McSrv_ReadI2C(bSlaveAddr, MCI_BASE_WINDOW);
		*pbGpio		= (UINT8)(bRegData & (UINT8)(1<<dPadNo));
	}
	else
	{
		if(sGPMode.abGpHost[dPadNo] == MCDRV_GPHOST_CDSP)
		{
		}
		else
		{
			if((*pbGpio != MCDRV_GP_LOW)
			&& (*pbGpio != MCDRV_GP_HIGH))
			{
				return MCDRV_ERROR_ARGUMENT;
			}
			McResCtrl_SetGPPad(*pbGpio, dPadNo);
			sdRet	= McPacket_AddGPSet();
			if(sdRet != MCDRV_SUCCESS)
			{
				return sdRet;
			}
			return McDevIf_ExecutePacket();
		}
	}

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	get_peak
 *
 *	Description:
 *			Get peak value.
 *	Arguments:
 *			psPeak	Peak information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	get_peak
(
	MCDRV_PEAK*	psPeak
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_REG_INFO	sRegInfo;

	if(McDevProf_IsValid(eMCDRV_FUNC_PM) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == McResCtrl_GetState())
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psPeak)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	if(McResCtrl_IsDstUsed(eMCDRV_DST_PEAK, eMCDRV_DST_CH0) == 0)
	{
		return MCDRV_ERROR;
	}

	sRegInfo.bRegType	= MCDRV_REGTYPE_B_AE;
	sRegInfo.bAddress	= MCI_PM_L;
	sdRet	= read_reg(&sRegInfo);
	if(sdRet == MCDRV_SUCCESS)
	{
		if((sRegInfo.bData & MCB_PM_OVFL) != 0)
		{
			psPeak->aswPeak[0]	= MCDRV_PM_OVERFLOW;
		}
		else
		{
			psPeak->aswPeak[0]	= (SINT16)gawPeakLevel[sRegInfo.bData];
		}

		sRegInfo.bRegType	= MCDRV_REGTYPE_B_AE;
		sRegInfo.bAddress	= MCI_PM_R;
		sdRet	= read_reg(&sRegInfo);
		if(sdRet == MCDRV_SUCCESS)
		{
			if((sRegInfo.bData & MCB_PM_OVFR) != 0)
			{
				psPeak->aswPeak[1]	= MCDRV_PM_OVERFLOW;
			}
			else
			{
				psPeak->aswPeak[1]	= (SINT16)gawPeakLevel[sRegInfo.bData];
			}
		}
	}
	return sdRet;
}

/****************************************************************************
 *	get_syseq
 *
 *	Description:
 *			Get System Eq.
 *	Arguments:
 *			psSysEq	pointer to MCDRV_SYSEQ_INFO struct
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR_ARGUMENT
*
 ****************************************************************************/
static	SINT32	get_syseq
(
	MCDRV_SYSEQ_INFO*	psSysEq
)
{
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psSysEq)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	McResCtrl_GetSysEq(psSysEq);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_syseq
 *
 *	Description:
 *			Set System Eq.
 *	Arguments:
 *			psSysEq		pointer to MCDRV_SYSEQ_INFO struct
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR_ARGUMENT
*
 ****************************************************************************/
static	SINT32	set_syseq
(
	const MCDRV_SYSEQ_INFO*	psSysEq,
	UINT32					dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psSysEq)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	sdRet	= ValidateSysEqParam(psSysEq, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetSysEq(psSysEq, dUpdateInfo);
	sdRet	= McPacket_AddSysEq(dUpdateInfo);
	if(sdRet != MCDRV_SUCCESS)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	get_ditswap
 *
 *	Description:
 *			Get DIT Swap.
 *	Arguments:
 *			psDitSwap	pointer to MCDRV_DITSWAP_INFO struct
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR_ARGUMENT
*
 ****************************************************************************/
static	SINT32	get_ditswap
(
	MCDRV_DITSWAP_INFO*	psDitSwap
)
{
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_DITSWAP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psDitSwap)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	McResCtrl_GetDitSwap(psDitSwap);

	return MCDRV_SUCCESS;
}

/****************************************************************************
 *	set_ditswap
 *
 *	Description:
 *			Set DIT Swap.
 *	Arguments:
 *			psDitSwap	pointer to MCDRV_DITSWAP_INFO struct
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *			MCDRV_ERROR_ARGUMENT
*
 ****************************************************************************/
static	SINT32	set_ditswap
(
	const MCDRV_DITSWAP_INFO*	psDitSwap,
	UINT32						dUpdateInfo
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	MCDRV_STATE	eState	= McResCtrl_GetState();

	if(McDevProf_IsValid(eMCDRV_FUNC_DITSWAP) == 0)
	{
		return MCDRV_ERROR;
	}

	if(eMCDRV_STATE_NOTINIT == eState)
	{
		return MCDRV_ERROR_STATE;
	}

	if(NULL == psDitSwap)
	{
		return MCDRV_ERROR_ARGUMENT;
	}

	sdRet	= ValidateDitSwapParam(psDitSwap, dUpdateInfo);
	if(MCDRV_SUCCESS != sdRet)
	{
		return sdRet;
	}

	McResCtrl_SetDitSwap(psDitSwap, dUpdateInfo);
	sdRet	= McPacket_AddDitSwap(dUpdateInfo);
	if(sdRet != MCDRV_SUCCESS)
	{
		return sdRet;
	}
	return McDevIf_ExecutePacket();
}

/****************************************************************************
 *	irq_proc
 *
 *	Description:
 *			Clear interrupt flag.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
static	SINT32	irq_proc
(
	void
)
{
	UINT8	bSlaveAddr;
	UINT8	abData[4];

	if(McDevProf_IsValid(eMCDRV_FUNC_IRQ) == 0)
	{
		return MCDRV_ERROR;
	}

	if ( eMCDRV_STATE_NOTINIT == McResCtrl_GetState() )
	{
		return MCDRV_ERROR_STATE;
	}

	bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
	abData[0]	= MCI_BASE_ADR<<1;
	abData[1]	= MCI_PA_FLG;
	abData[2]	= MCI_BASE_WINDOW<<1;
	abData[3]	= MCB_PA2_FLAG | MCB_PA1_FLAG | MCB_PA0_FLAG;
	McSrv_WriteI2C(bSlaveAddr, abData, 4);

	return MCDRV_SUCCESS;
}


/****************************************************************************
 *	ValidateInitParam
 *
 *	Description:
 *			validate init parameters.
 *	Arguments:
 *			psInitInfo	initialize information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	ValidateInitParam
(
	const MCDRV_INIT_INFO*	psInitInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateInitParam");
#endif


	if((MCDRV_CKSEL_CMOS != psInitInfo->bCkSel)
	&& (MCDRV_CKSEL_TCXO != psInitInfo->bCkSel)
	&& (MCDRV_CKSEL_CMOS_TCXO != psInitInfo->bCkSel)
	&& (MCDRV_CKSEL_TCXO_CMOS != psInitInfo->bCkSel))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(((psInitInfo->bDivR0 == 0x00) || (psInitInfo->bDivR0 > 0x3F))
	|| ((psInitInfo->bDivF0 == 0x00) || (psInitInfo->bDivF0 > 0x7F)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(((psInitInfo->bDivR1 == 0x00) || (psInitInfo->bDivR1 > 0x3F))
	|| ((psInitInfo->bDivF1 == 0x00) || (psInitInfo->bDivF1 > 0x7F)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((McDevProf_IsValid(eMCDRV_FUNC_RANGE) != 0) && (((psInitInfo->bRange0 > 0x07) || (psInitInfo->bRange1 > 0x07))))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((McDevProf_IsValid(eMCDRV_FUNC_BYPASS) != 0) && (psInitInfo->bBypass > 0x03))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(((MCDRV_DAHIZ_LOW != psInitInfo->bDioSdo0Hiz) && (MCDRV_DAHIZ_HIZ != psInitInfo->bDioSdo0Hiz))
	|| ((MCDRV_DAHIZ_LOW != psInitInfo->bDioSdo1Hiz) && (MCDRV_DAHIZ_HIZ != psInitInfo->bDioSdo1Hiz)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		if((MCDRV_DAHIZ_LOW != psInitInfo->bDioSdo2Hiz) && (MCDRV_DAHIZ_HIZ != psInitInfo->bDioSdo2Hiz))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(((MCDRV_DAHIZ_LOW != psInitInfo->bDioClk0Hiz) && (MCDRV_DAHIZ_HIZ != psInitInfo->bDioClk0Hiz))
	|| ((MCDRV_DAHIZ_LOW != psInitInfo->bDioClk1Hiz) && (MCDRV_DAHIZ_HIZ != psInitInfo->bDioClk1Hiz)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		if((MCDRV_DAHIZ_LOW != psInitInfo->bDioClk2Hiz) && (MCDRV_DAHIZ_HIZ != psInitInfo->bDioClk2Hiz))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((MCDRV_PCMHIZ_HIZ != psInitInfo->bPcmHiz) && (MCDRV_PCMHIZ_LOW != psInitInfo->bPcmHiz))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((McDevProf_IsValid(eMCDRV_FUNC_LI1) != 0) && (MCDRV_LINE_STEREO != psInitInfo->bLineIn1Dif) && (MCDRV_LINE_DIF != psInitInfo->bLineIn1Dif))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0) && (MCDRV_LINE_STEREO != psInitInfo->bLineIn2Dif) && (MCDRV_LINE_DIF != psInitInfo->bLineIn2Dif))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(((MCDRV_LINE_STEREO != psInitInfo->bLineOut1Dif) && (MCDRV_LINE_DIF != psInitInfo->bLineOut1Dif))
	|| ((MCDRV_LINE_STEREO != psInitInfo->bLineOut2Dif) && (MCDRV_LINE_DIF != psInitInfo->bLineOut2Dif)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_SPMN_ON != psInitInfo->bSpmn) && (MCDRV_SPMN_OFF != psInitInfo->bSpmn))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(((MCDRV_MIC_DIF != psInitInfo->bMic1Sng) && (MCDRV_MIC_SINGLE != psInitInfo->bMic1Sng))
	|| ((MCDRV_MIC_DIF != psInitInfo->bMic2Sng) && (MCDRV_MIC_SINGLE != psInitInfo->bMic2Sng)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(eDevID == eMCDRV_DEV_ID_44H)
	{
		if((MCDRV_MIC_DIF != psInitInfo->bMic3Sng) && (MCDRV_MIC_SINGLE != psInitInfo->bMic3Sng) && (MCDRV_MIC_LINE != psInitInfo->bMic3Sng))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	else
	{
		if((MCDRV_MIC_DIF != psInitInfo->bMic3Sng) && (MCDRV_MIC_SINGLE != psInitInfo->bMic3Sng))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((MCDRV_POWMODE_NORMAL != psInitInfo->bPowerMode)
	&& (MCDRV_POWMODE_CLKON != psInitInfo->bPowerMode)
	&& (MCDRV_POWMODE_VREFON != psInitInfo->bPowerMode)
	&& (MCDRV_POWMODE_CLKVREFON != psInitInfo->bPowerMode)
	&& (MCDRV_POWMODE_FULL != psInitInfo->bPowerMode))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_SPHIZ_PULLDOWN != psInitInfo->bSpHiz) && (MCDRV_SPHIZ_HIZ != psInitInfo->bSpHiz))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_LDO_AOFF_DOFF != psInitInfo->bLdo) && (MCDRV_LDO_AON_DOFF != psInitInfo->bLdo)
	&& (MCDRV_LDO_AOFF_DON != psInitInfo->bLdo) && (MCDRV_LDO_AON_DON != psInitInfo->bLdo))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_PAD_GPIO != psInitInfo->bPad0Func) && (MCDRV_PAD_PDMCK != psInitInfo->bPad0Func) && (MCDRV_PAD_IRQ != psInitInfo->bPad0Func))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((McDevProf_IsValid(eMCDRV_FUNC_IRQ) == 0) && (MCDRV_PAD_IRQ == psInitInfo->bPad0Func))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_PAD_GPIO != psInitInfo->bPad1Func) && (MCDRV_PAD_PDMDI != psInitInfo->bPad1Func) && (MCDRV_PAD_IRQ != psInitInfo->bPad1Func))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((McDevProf_IsValid(eMCDRV_FUNC_IRQ) == 0) && (MCDRV_PAD_IRQ == psInitInfo->bPad1Func))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		if((MCDRV_PAD_GPIO != psInitInfo->bPad2Func)
		&& (MCDRV_PAD_PDMDI != psInitInfo->bPad2Func)
		&& (MCDRV_PAD_IRQ != psInitInfo->bPad2Func))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			if((MCDRV_PAD_PDMDI == psInitInfo->bPad2Func) && (MCDRV_PAD_PDMDI == psInitInfo->bPad1Func))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if((MCDRV_OUTLEV_0 != psInitInfo->bAvddLev) && (MCDRV_OUTLEV_1 != psInitInfo->bAvddLev)
	&& (MCDRV_OUTLEV_2 != psInitInfo->bAvddLev) && (MCDRV_OUTLEV_3 != psInitInfo->bAvddLev)
	&& (MCDRV_OUTLEV_4 != psInitInfo->bAvddLev) && (MCDRV_OUTLEV_5 != psInitInfo->bAvddLev)
	&& (MCDRV_OUTLEV_6 != psInitInfo->bAvddLev) && (MCDRV_OUTLEV_7 != psInitInfo->bAvddLev))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_OUTLEV_0 != psInitInfo->bVrefLev) && (MCDRV_OUTLEV_1 != psInitInfo->bVrefLev)
	&& (MCDRV_OUTLEV_2 != psInitInfo->bVrefLev) && (MCDRV_OUTLEV_3 != psInitInfo->bVrefLev)
	&& (MCDRV_OUTLEV_4 != psInitInfo->bVrefLev) && (MCDRV_OUTLEV_5 != psInitInfo->bVrefLev)
	&& (MCDRV_OUTLEV_6 != psInitInfo->bVrefLev) && (MCDRV_OUTLEV_7 != psInitInfo->bVrefLev))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_DCLGAIN_0 != psInitInfo->bDclGain) && (MCDRV_DCLGAIN_6 != psInitInfo->bDclGain)
	&& (MCDRV_DCLGAIN_12!= psInitInfo->bDclGain) && (MCDRV_DCLGAIN_18!= psInitInfo->bDclGain))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if((MCDRV_DCLLIMIT_0 != psInitInfo->bDclLimit) && (MCDRV_DCLLIMIT_116 != psInitInfo->bDclLimit)
	&& (MCDRV_DCLLIMIT_250 != psInitInfo->bDclLimit) && (MCDRV_DCLLIMIT_602 != psInitInfo->bDclLimit))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_CPMODE) != 0)
	{
		if((MCDRV_CPMOD_ON != psInitInfo->bCpMod) && (MCDRV_CPMOD_OFF != psInitInfo->bCpMod))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		if((psInitInfo->bSdDs != MCDRV_SD_DS_LLL)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_LLH)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_LHL)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_LHH)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_HLL)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_HLH)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_HHL)
		&& (psInitInfo->bSdDs != MCDRV_SD_DS_HHH))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dAdHpf)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dMic1Cin)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dMic2Cin)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dMic3Cin)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dLine1Cin)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dLine2Cin)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dVrefRdy1)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dVrefRdy2)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dHpRdy)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dSpRdy)
	|| (MCDRV_MAX_WAIT_TIME < psInitInfo->sWaitTime.dPdm))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateInitParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateClockParam
 *
 *	Description:
 *			validate clock parameters.
 *	Arguments:
 *			psClockInfo	clock information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	ValidateClockParam
(
	const MCDRV_CLOCK_INFO*	psClockInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateClockParam");
#endif


	if((MCDRV_CKSEL_CMOS != psClockInfo->bCkSel)
	&& (MCDRV_CKSEL_TCXO != psClockInfo->bCkSel)
	&& (MCDRV_CKSEL_CMOS_TCXO != psClockInfo->bCkSel)
	&& (MCDRV_CKSEL_TCXO_CMOS != psClockInfo->bCkSel))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psClockInfo->bDivR0 == 0x00) || (psClockInfo->bDivR0 > 0x3F))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psClockInfo->bDivR1 == 0x00) || (psClockInfo->bDivR1 > 0x3F))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psClockInfo->bDivF0 == 0x00) || (psClockInfo->bDivF0 > 0x7F))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psClockInfo->bDivF1 == 0x00) || (psClockInfo->bDivF1 > 0x7F))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((McDevProf_IsValid(eMCDRV_FUNC_RANGE) != 0)
		 && ((psClockInfo->bRange0 > 0x07) || (psClockInfo->bRange1 > 0x07)))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((McDevProf_IsValid(eMCDRV_FUNC_BYPASS) != 0)
		 && (psClockInfo->bBypass > 0x03))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateClockParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateReadRegParam
 *
 *	Description:
 *			validate read reg parameter.
 *	Arguments:
 *			psRegInfo	register information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	ValidateReadRegParam
(
	const MCDRV_REG_INFO*	psRegInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateReadRegParam");
#endif


	if((McResCtrl_GetRegAccess(psRegInfo) & eMCDRV_CAN_READ) == eMCDRV_ACCESS_DENY)
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if(McDevProf_IsValid(eMCDRV_FUNC_CPMODE) == 0)
	{
		if((psRegInfo->bRegType == MCDRV_REGTYPE_B_ANALOG) && (psRegInfo->bAddress == MCI_CPMOD))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateReadRegParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateWriteRegParam
 *
 *	Description:
 *			validate write reg parameter.
 *	Arguments:
 *			psRegInfo	register information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static	SINT32	ValidateWriteRegParam
(
	const MCDRV_REG_INFO*	psRegInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateWriteRegParam");
#endif


	if((McResCtrl_GetRegAccess(psRegInfo) & eMCDRV_CAN_WRITE) == eMCDRV_ACCESS_DENY)
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if(McDevProf_IsValid(eMCDRV_FUNC_CPMODE) == 0)
	{
		if((psRegInfo->bRegType == MCDRV_REGTYPE_B_ANALOG) && (psRegInfo->bAddress == MCI_CPMOD))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateWriteRegParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidatePathParam
 *
 *	Description:
 *			validate path parameters.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
SINT32	ValidatePathParam
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bBlock;
	UINT8	bCh;
	MCDRV_PATH_INFO	sCurPathInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidatePathParam");
#endif


	McResCtrl_GetPathInfoVirtual(&sCurPathInfo);
	/*	set off to current path info	*/
	for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
	{
		for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++)
		{
			if((psPathInfo->asHpOut[bCh].abSrcOnOff[bBlock] & (0x03)) == 0x02)
			{
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x01;
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	|= 0x02;
			}
			if((psPathInfo->asHpOut[bCh].abSrcOnOff[bBlock] & (0x0C)) == 0x08)
			{
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x04;
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	|= 0x08;
			}
			if((psPathInfo->asHpOut[bCh].abSrcOnOff[bBlock] & (0x30)) == 0x20)
			{
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x10;
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	|= 0x20;
			}
			if((psPathInfo->asHpOut[bCh].abSrcOnOff[bBlock] & (0xC0)) == 0x80)
			{
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x40;
				sCurPathInfo.asHpOut[bCh].abSrcOnOff[bBlock]	|= 0x80;
			}
		}
		for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++)
		{
			if((psPathInfo->asSpOut[bCh].abSrcOnOff[bBlock] & (0x03)) == 0x02)
			{
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x01;
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x02;
			}
			if((psPathInfo->asSpOut[bCh].abSrcOnOff[bBlock] & (0x0C)) == 0x08)
			{
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x04;
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x08;
			}
			if((psPathInfo->asSpOut[bCh].abSrcOnOff[bBlock] & (0x30)) == 0x20)
			{
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x10;
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x20;
			}
			if((psPathInfo->asSpOut[bCh].abSrcOnOff[bBlock] & (0xC0)) == 0x80)
			{
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x40;
				sCurPathInfo.asSpOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x80;
			}
		}
		for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++)
		{
			if((psPathInfo->asRcOut[bCh].abSrcOnOff[bBlock] & (0x03)) == 0x02)
			{
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x01;
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x02;
			}
			if((psPathInfo->asRcOut[bCh].abSrcOnOff[bBlock] & (0x0C)) == 0x08)
			{
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x04;
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x08;
			}
			if((psPathInfo->asRcOut[bCh].abSrcOnOff[bBlock] & (0x30)) == 0x20)
			{
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x10;
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x20;
			}
			if((psPathInfo->asRcOut[bCh].abSrcOnOff[bBlock] & (0xC0)) == 0x80)
			{
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x40;
				sCurPathInfo.asRcOut[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x80;
			}
		}
		for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++)
		{
			if((psPathInfo->asLout1[bCh].abSrcOnOff[bBlock] & (0x03)) == 0x02)
			{
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x01;
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x02;
			}
			if((psPathInfo->asLout1[bCh].abSrcOnOff[bBlock] & (0x0C)) == 0x08)
			{
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x04;
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x08;
			}
			if((psPathInfo->asLout1[bCh].abSrcOnOff[bBlock] & (0x30)) == 0x20)
			{
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x10;
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x20;
			}
			if((psPathInfo->asLout1[bCh].abSrcOnOff[bBlock] & (0xC0)) == 0x80)
			{
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x40;
				sCurPathInfo.asLout1[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x80;
			}
		}
		for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++)
		{
			if((psPathInfo->asLout2[bCh].abSrcOnOff[bBlock] & (0x03)) == 0x02)
			{
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x01;
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	|= 0x02;
			}
			if((psPathInfo->asLout2[bCh].abSrcOnOff[bBlock] & (0x0C)) == 0x08)
			{
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x04;
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	|= 0x08;
			}
			if((psPathInfo->asLout2[bCh].abSrcOnOff[bBlock] & (0x30)) == 0x20)
			{
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x10;
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	|= 0x20;
			}
			if((psPathInfo->asLout2[bCh].abSrcOnOff[bBlock] & (0xC0)) == 0x80)
			{
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x40;
				sCurPathInfo.asLout2[bCh].abSrcOnOff[bBlock]	|= 0x80;
			}
		}
	}

	/*	HP	*/
	if((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
	{
		if(((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		|| ((sCurPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
	{
		if((sCurPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
	{
		if(((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		|| ((sCurPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
	{
		if((sCurPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
	{
		if(((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
		|| ((sCurPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
	{
		if((sCurPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	/*	SP	*/
	if((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
	{
		if(((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		|| ((sCurPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
	{
		if((sCurPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
	{
		if(((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		|| ((sCurPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
	{
		if((sCurPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
	{
		if(((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
		|| ((sCurPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
	{
		if((sCurPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & (MCDRV_SRC1_LINE1_R_ON|MCDRV_SRC1_LINE1_R_OFF)) == MCDRV_SRC1_LINE1_R_ON)
	{
		if(((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		|| ((sCurPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
	{
		if((sCurPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & (MCDRV_SRC1_LINE1_R_ON|MCDRV_SRC1_LINE1_R_OFF)) == MCDRV_SRC1_LINE1_R_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & (MCDRV_SRC2_LINE2_R_ON|MCDRV_SRC2_LINE2_R_OFF)) == MCDRV_SRC2_LINE2_R_ON)
	{
		if(((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		|| ((sCurPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
	{
		if((sCurPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & (MCDRV_SRC2_LINE2_R_ON|MCDRV_SRC2_LINE2_R_OFF)) == MCDRV_SRC2_LINE2_R_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & (MCDRV_SRC5_DAC_R_ON|MCDRV_SRC5_DAC_R_OFF)) == MCDRV_SRC5_DAC_R_ON)
	{
		if(((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
		|| ((sCurPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
	{
		if((sCurPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & (MCDRV_SRC5_DAC_R_ON|MCDRV_SRC5_DAC_R_OFF)) == MCDRV_SRC5_DAC_R_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	/*	RCV	*/

	/*	LOUT1	*/
	if((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
	{
		if(((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		|| ((sCurPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
	{
		if((sCurPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
	{
		if(((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		|| ((sCurPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
	{
		if((sCurPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
	{
		if(((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
		|| ((sCurPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
	{
		if((sCurPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	/*	LOUT2	*/
	if((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
	{
		if(((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		|| ((sCurPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
	{
		if((sCurPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
	{
		if(((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		|| ((sCurPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
	{
		if((sCurPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
	{
		if(((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
		|| ((sCurPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((psPathInfo->asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & (MCDRV_SRC5_DAC_M_ON|MCDRV_SRC5_DAC_M_OFF)) == MCDRV_SRC5_DAC_M_ON)
	{
		if((sCurPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & (MCDRV_SRC5_DAC_L_ON|MCDRV_SRC5_DAC_L_OFF)) == MCDRV_SRC5_DAC_L_ON)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= ValidatePathParam_Dig(psPathInfo);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidatePathParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidatePathParam_Dig
 *
 *	Description:
 *			Validate digital path parameters.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
SINT32	ValidatePathParam_Dig
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bBlock, bDstBlock;
	UINT8	bCh;
	MCDRV_PATH_INFO	sCurPathInfo;
	MCDRV_INIT_INFO	sInitInfo;
	UINT8	bValidCDSP	= 0;

	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sSrcInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,			MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,			MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,			MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,			MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,			MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,			MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_DIRECT_ON,	MCDRV_SRC3_DIR2_DIRECT_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,			MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,			MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,			MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,			MCDRV_SRC6_CDSP_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_DIRECT_ON,	MCDRV_SRC6_CDSP_DIRECT_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidatePathParam_Dig");
#endif


	McResCtrl_GetInitInfo(&sInitInfo);

	if(McResCtrl_GetCdspFw() != NULL)
	{/*	C-DSP loaded	*/
		bValidCDSP	= 1;
	}

	/*	PeakMeter	*/
	for(bCh = 0; (bCh < PEAK_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asPeak[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0) && ((psPathInfo->asPeak[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((psPathInfo->asPeak[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
			{
				for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
				{
					if((psPathInfo->asPeak[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
			}
		}
	}

	/*	DIT0	*/
	for(bCh = 0; (bCh < DIT0_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asDit0[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0) && ((psPathInfo->asDit0[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((psPathInfo->asDit0[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
			{
				for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
				{
					if((psPathInfo->asDit0[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
			}
		}
	}

	/*	DIT1	*/
	for(bCh = 0; (bCh < DIT1_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asDit1[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0) && ((psPathInfo->asDit1[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((psPathInfo->asDit1[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
			{
				for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
				{
					if((psPathInfo->asDit1[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
			}
		}
	}

	/*	DIT2	*/
	for(bCh = 0; (bCh < DIT2_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asDit2[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0)
			&& (((psPathInfo->asDit2[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
			 || ((psPathInfo->asDit2[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_DIRECT_ON) == MCDRV_SRC6_CDSP_DIRECT_ON)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((psPathInfo->asDit2[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
			{
				for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
				{
					if((psPathInfo->asDit2[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
			}
		}
	}

	/*	DAC	*/
	for(bCh = 0; (bCh < DAC_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asDac[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0) && ((psPathInfo->asDac[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((bCh == 0) && (sSrcInfo[bBlock].bBlock == MCDRV_SRC_DTMF_BLOCK) && (sSrcInfo[bBlock].bOn == MCDRV_SRC4_DTMF_ON))
			{
			}
			else
			{
				if((psPathInfo->asDac[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
				{
					for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
					{
						if((psPathInfo->asDac[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
						{
							if((bCh == 0) && (sSrcInfo[bDstBlock].bBlock == MCDRV_SRC_DTMF_BLOCK) && (sSrcInfo[bDstBlock].bOn == MCDRV_SRC4_DTMF_ON))
							{
							}
							else
							{
								sdRet	= MCDRV_ERROR_ARGUMENT;
							}
						}
					}
				}
			}
		}
	}

	/*	AE	*/
	for(bCh = 0; (bCh < AE_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asAe[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0) && ((psPathInfo->asAe[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((psPathInfo->asAe[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
			{
				for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
				{
					if((psPathInfo->asAe[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
			}
		}
	}

	/*	CDSP	*/
	for(bCh = 0; (bCh < CDSP_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if((psPathInfo->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((psPathInfo->asCdsp[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & (sSrcInfo[bBlock].bOn|sSrcInfo[bBlock].bOff)) == sSrcInfo[bBlock].bOn)
			{
				if(bValidCDSP == 0)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
				for(bDstBlock = bBlock+1; (bDstBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bDstBlock++)
				{
					if((psPathInfo->asCdsp[bCh].abSrcOnOff[sSrcInfo[bDstBlock].bBlock] & (sSrcInfo[bDstBlock].bOn|sSrcInfo[bDstBlock].bOff)) == sSrcInfo[bDstBlock].bOn)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
			}
		}
	}

	for(bCh = 0; (bCh < MIX_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
	{
		if(((psPathInfo->asMix[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((psPathInfo->asMix2[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON))
		{
			if((sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
			|| ((sInitInfo.bPad1Func != MCDRV_PAD_PDMDI) && (sInitInfo.bPad2Func != MCDRV_PAD_PDMDI)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((bValidCDSP == 0)
			&& (((psPathInfo->asMix[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
			 || ((psPathInfo->asMix2[bCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

	/*	ADC0	*/
	McResCtrl_GetPathInfoVirtual(&sCurPathInfo);
	/*	set off to current path info	*/
	for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++)
	{
		for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++)
		{
			if((psPathInfo->asAdc0[bCh].abSrcOnOff[bBlock] & (0x03)) == 0x02)
			{
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x01;
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x02;
			}
			if((psPathInfo->asAdc0[bCh].abSrcOnOff[bBlock] & (0x0C)) == 0x08)
			{
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x04;
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x08;
			}
			if((psPathInfo->asAdc0[bCh].abSrcOnOff[bBlock] & (0x30)) == 0x20)
			{
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x10;
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x20;
			}
			if((psPathInfo->asAdc0[bCh].abSrcOnOff[bBlock] & (0xC0)) == 0x80)
			{
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	&= (UINT8)~0x40;
				sCurPathInfo.asAdc0[bCh].abSrcOnOff[bBlock]	|= (UINT8)0x80;
			}
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
		{
			if(((psPathInfo->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
			|| ((sCurPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		{
			if((sCurPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & (MCDRV_SRC1_LINE1_L_ON|MCDRV_SRC1_LINE1_L_OFF)) == MCDRV_SRC1_LINE1_L_ON)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
		{
			if(((psPathInfo->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
			|| ((sCurPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		{
			if((sCurPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & (MCDRV_SRC2_LINE2_L_ON|MCDRV_SRC2_LINE2_L_OFF)) == MCDRV_SRC2_LINE2_L_ON)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & (MCDRV_SRC1_LINE1_R_ON|MCDRV_SRC1_LINE1_R_OFF)) == MCDRV_SRC1_LINE1_R_ON)
		{
			if(((psPathInfo->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
			|| ((sCurPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & (MCDRV_SRC1_LINE1_M_ON|MCDRV_SRC1_LINE1_M_OFF)) == MCDRV_SRC1_LINE1_M_ON)
		{
			if((sCurPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & (MCDRV_SRC1_LINE1_R_ON|MCDRV_SRC1_LINE1_R_OFF)) == MCDRV_SRC1_LINE1_R_ON)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & (MCDRV_SRC2_LINE2_R_ON|MCDRV_SRC2_LINE2_R_OFF)) == MCDRV_SRC2_LINE2_R_ON)
		{
			if(((psPathInfo->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
			|| ((sCurPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((psPathInfo->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & (MCDRV_SRC2_LINE2_M_ON|MCDRV_SRC2_LINE2_M_OFF)) == MCDRV_SRC2_LINE2_M_ON)
		{
			if((sCurPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & (MCDRV_SRC2_LINE2_R_ON|MCDRV_SRC2_LINE2_R_OFF)) == MCDRV_SRC2_LINE2_R_ON)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidatePathParam_Dig", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateDioParam
 *
 *	Description:
 *			validate digital IO parameters.
 *	Arguments:
 *			psDioInfo	digital IO information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateDioParam
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT32					dUpdateInfo
)
{
	SINT32			sdRet	= MCDRV_SUCCESS;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateDioParam");
#endif


	McResCtrl_GetDioInfo(&sDioInfo);

	if(((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != 0UL) || ((dUpdateInfo & MCDRV_DIO0_DIR_UPDATE_FLAG) != 0UL))
	{
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX0_START) & MCB_DIR0_START) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != 0UL) || ((dUpdateInfo & MCDRV_DIO0_DIT_UPDATE_FLAG) != 0UL))
	{
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX0_START) & MCB_DIT0_START) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != 0UL) || ((dUpdateInfo & MCDRV_DIO1_DIR_UPDATE_FLAG) != 0UL))
	{
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX1_START) & MCB_DIR1_START) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != 0UL) || ((dUpdateInfo & MCDRV_DIO1_DIT_UPDATE_FLAG) != 0UL))
	{
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX1_START) & MCB_DIT1_START) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != 0UL) || ((dUpdateInfo & MCDRV_DIO2_DIR_UPDATE_FLAG) != 0UL))
	{
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START) & MCB_DIR2_START) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if(((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != 0UL) || ((dUpdateInfo & MCDRV_DIO2_DIT_UPDATE_FLAG) != 0UL))
	{
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START) & MCB_DIT2_START) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIOCommon(psDioInfo, 0);
			if(sdRet == MCDRV_SUCCESS)
			{
				sDioInfo.asPortInfo[0].sDioCommon.bInterface	= psDioInfo->asPortInfo[0].sDioCommon.bInterface;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIOCommon(psDioInfo, 1);
			if(sdRet == MCDRV_SUCCESS)
			{
				sDioInfo.asPortInfo[1].sDioCommon.bInterface	= psDioInfo->asPortInfo[1].sDioCommon.bInterface;
			}
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIOCommon(psDioInfo, 2);
			if(sdRet == MCDRV_SUCCESS)
			{
				sDioInfo.asPortInfo[2].sDioCommon.bInterface	= psDioInfo->asPortInfo[2].sDioCommon.bInterface;
			}
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO0_DIR_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIODIR(psDioInfo, 0, sDioInfo.asPortInfo[0].sDioCommon.bInterface);
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO1_DIR_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIODIR(psDioInfo, 1, sDioInfo.asPortInfo[1].sDioCommon.bInterface);
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO2_DIR_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIODIR(psDioInfo, 2, sDioInfo.asPortInfo[2].sDioCommon.bInterface);
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO0_DIT_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIODIT(psDioInfo, 0, sDioInfo.asPortInfo[0].sDioCommon.bInterface);
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO1_DIT_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIODIT(psDioInfo, 1, sDioInfo.asPortInfo[1].sDioCommon.bInterface);
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdateInfo & MCDRV_DIO2_DIT_UPDATE_FLAG) != 0UL)
		{
			sdRet	= CheckDIODIT(psDioInfo, 2, sDioInfo.asPortInfo[2].sDioCommon.bInterface);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateDioParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateDacParam
 *
 *	Description:
 *			validate DAC parameters.
 *	Arguments:
 *			psDacInfo	DAC information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateDacParam
(
	const MCDRV_DAC_INFO*	psDacInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateDacParam");
#endif


	if((dUpdateInfo & MCDRV_DAC_MSWP_UPDATE_FLAG) != 0UL)
	{
		if((psDacInfo->bMasterSwap != MCDRV_DSWAP_OFF)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_SWAP)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_MUTE)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_RMVCENTER)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_MONO)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_MONOHALF)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_BOTHL)
		&& (psDacInfo->bMasterSwap != MCDRV_DSWAP_BOTHR))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_DAC_VSWP_UPDATE_FLAG) != 0UL)
	{
		if((psDacInfo->bVoiceSwap != MCDRV_DSWAP_OFF)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_SWAP)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_MUTE)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_RMVCENTER)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_MONO)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_MONOHALF)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_BOTHL)
		&& (psDacInfo->bVoiceSwap != MCDRV_DSWAP_BOTHR))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_DAC_HPF_UPDATE_FLAG) != 0UL)
	{
		if((psDacInfo->bDcCut != MCDRV_DCCUT_ON) && (psDacInfo->bDcCut != MCDRV_DCCUT_OFF))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateDacParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateAdcParam
 *
 *	Description:
 *			validate ADC parameters.
 *	Arguments:
 *			psAdcInfo	ADC information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateAdcParam
(
	const MCDRV_ADC_INFO*	psAdcInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateAdcParam");
#endif


	if((dUpdateInfo & MCDRV_ADCADJ_UPDATE_FLAG) != 0UL)
	{
		if((psAdcInfo->bAgcAdjust != MCDRV_AGCADJ_24)
		&& (psAdcInfo->bAgcAdjust != MCDRV_AGCADJ_18)
		&& (psAdcInfo->bAgcAdjust != MCDRV_AGCADJ_12)
		&& (psAdcInfo->bAgcAdjust != MCDRV_AGCADJ_0))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_ADCAGC_UPDATE_FLAG) != 0UL)
	{
		if((psAdcInfo->bAgcOn != MCDRV_AGC_OFF)
		&& (psAdcInfo->bAgcOn != MCDRV_AGC_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_ADCMONO_UPDATE_FLAG) != 0UL)
	{
		if((psAdcInfo->bMono != MCDRV_ADC_STEREO)
		&& (psAdcInfo->bMono != MCDRV_ADC_MONO))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateAdcParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateAdcExParam
 *
 *	Description:
 *			validate ADC_EX parameters.
 *	Arguments:
 *			psAdcExInfo	ADC_EX information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateAdcExParam
(
	const MCDRV_ADC_EX_INFO*	psAdcExInfo,
	UINT32						dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateAdcExParam");
#endif


	if((dUpdateInfo & MCDRV_AGCENV_UPDATE_FLAG) != 0UL)
	{
		if((psAdcExInfo->bAgcEnv != MCDRV_AGCENV_5310)
		&& (psAdcExInfo->bAgcEnv != MCDRV_AGCENV_10650)
		&& (psAdcExInfo->bAgcEnv != MCDRV_AGCENV_21310)
		&& (psAdcExInfo->bAgcEnv != MCDRV_AGCENV_85230))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCLVL_UPDATE_FLAG) != 0UL)
	{
		if((psAdcExInfo->bAgcLvl != MCDRV_AGCLVL_3)
		&& (psAdcExInfo->bAgcLvl != MCDRV_AGCLVL_6)
		&& (psAdcExInfo->bAgcLvl != MCDRV_AGCLVL_9)
		&& (psAdcExInfo->bAgcLvl != MCDRV_AGCLVL_12))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCUPTIM_UPDATE_FLAG) != 0UL)
	{
		if((psAdcExInfo->bAgcUpTim != MCDRV_AGCUPTIM_341)
		&& (psAdcExInfo->bAgcUpTim != MCDRV_AGCUPTIM_683)
		&& (psAdcExInfo->bAgcUpTim != MCDRV_AGCUPTIM_1365)
		&& (psAdcExInfo->bAgcUpTim != MCDRV_AGCUPTIM_2730))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCDWTIM_UPDATE_FLAG) != 0UL)
	{
		if((psAdcExInfo->bAgcDwTim != MCDRV_AGCDWTIM_5310)
		&& (psAdcExInfo->bAgcDwTim != MCDRV_AGCDWTIM_10650)
		&& (psAdcExInfo->bAgcDwTim != MCDRV_AGCDWTIM_21310)
		&& (psAdcExInfo->bAgcDwTim != MCDRV_AGCDWTIM_85230))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTLVL_UPDATE_FLAG) != 0UL)
	{
		if((psAdcExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_OFF)
		&& (psAdcExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_80)
		&& (psAdcExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_70)
		&& (psAdcExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_60))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTTIM_UPDATE_FLAG) != 0UL)
	{
		if((psAdcExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_5310)
		&& (psAdcExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_10650)
		&& (psAdcExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_341000)
		&& (psAdcExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_2730000))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateAdcExParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateSpParam
 *
 *	Description:
 *			validate SP parameters.
 *	Arguments:
 *			psSpInfo	SP information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateSpParam
(
	const MCDRV_SP_INFO*	psSpInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateSpParam");
#endif


	if((psSpInfo->bSwap != MCDRV_SPSWAP_OFF)
	&& (psSpInfo->bSwap != MCDRV_SPSWAP_SWAP))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateSpParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateDngParam
 *
 *	Description:
 *			validate DNG parameters.
 *	Arguments:
 *			psDngInfo	DNG information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateDngParam
(
	const MCDRV_DNG_INFO*	psDngInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bItem;
	MCDRV_DNG_INFO	sDngInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateDngParam");
#endif


	McResCtrl_GetDngInfo(&sDngInfo);

	for(bItem = 0; bItem < DNG_ITEM_NUM; bItem++)
	{
		if((dUpdateInfo & (MCDRV_DNGSW_HP_UPDATE_FLAG<<(bItem*8))) != 0UL)
		{
			if((psDngInfo->abOnOff[bItem] != MCDRV_DNG_OFF)
			&& (psDngInfo->abOnOff[bItem] != MCDRV_DNG_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
			else
			{
				sDngInfo.abOnOff[bItem]	= psDngInfo->abOnOff[bItem];
			}
		}
		if((dUpdateInfo & (MCDRV_DNGTHRES_HP_UPDATE_FLAG<<(bItem*8))) != 0UL)
		{
			if(sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
			{
				if((psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_30)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_36)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_42)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_48)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_54)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_60)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_66)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_72)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_78)
				&& (psDngInfo->abThreshold[bItem] != MCDRV_DNG_THRES_84))
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
		if((dUpdateInfo & (MCDRV_DNGHOLD_HP_UPDATE_FLAG<<(bItem*8))) != 0UL)
		{
			if(sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
			{
				if((psDngInfo->abHold[bItem] != MCDRV_DNG_HOLD_30)
				&& (psDngInfo->abHold[bItem] != MCDRV_DNG_HOLD_120)
				&& (psDngInfo->abHold[bItem] != MCDRV_DNG_HOLD_500))
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
		if((dUpdateInfo & (MCDRV_DNGATK_HP_UPDATE_FLAG<<(bItem*8))) != 0UL)
		{
			if(sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
			{
				if((psDngInfo->abAttack[bItem] != MCDRV_DNG_ATTACK_25)
				&& (psDngInfo->abAttack[bItem] != MCDRV_DNG_ATTACK_100)
				&& (psDngInfo->abAttack[bItem] != MCDRV_DNG_ATTACK_400)
				&& (psDngInfo->abAttack[bItem] != MCDRV_DNG_ATTACK_800))
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
		if((dUpdateInfo & (MCDRV_DNGREL_HP_UPDATE_FLAG<<(bItem*8))) != 0UL)
		{
			if(sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
			{
				if((psDngInfo->abRelease[bItem] != MCDRV_DNG_RELEASE_7950)
				&& (psDngInfo->abRelease[bItem] != MCDRV_DNG_RELEASE_470)
				&& (psDngInfo->abRelease[bItem] != MCDRV_DNG_RELEASE_940))
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
		if((dUpdateInfo & (MCDRV_DNGTARGET_HP_UPDATE_FLAG<<(bItem*8))) != 0UL)
		{
			if((psDngInfo->abTarget[bItem] != MCDRV_DNG_TARGET_6)
			&& (psDngInfo->abTarget[bItem] != MCDRV_DNG_TARGET_9)
			&& (psDngInfo->abTarget[bItem] != MCDRV_DNG_TARGET_12)
			&& (psDngInfo->abTarget[bItem] != MCDRV_DNG_TARGET_15)
			&& (psDngInfo->abTarget[bItem] != MCDRV_DNG_TARGET_18)
			&& (psDngInfo->abTarget[bItem] != MCDRV_DNG_TARGET_MUTE))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

	if((dUpdateInfo & MCDRV_DNGFW_FLAG) != 0UL)
	{
		if((psDngInfo->bFw != MCDRV_DNG_FW_OFF)
		&& (psDngInfo->bFw != MCDRV_DNG_FW_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateDngParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidatePdmParam
 *
 *	Description:
 *			validate PDM parameters.
 *	Arguments:
 *			psPdmInfo	PDM information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidatePdmParam
(
	const MCDRV_PDM_INFO*	psPdmInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidatePdmParam");
#endif



	if((dUpdateInfo & MCDRV_PDMCLK_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bClk != MCDRV_PDM_CLK_128)
		&& (psPdmInfo->bClk != MCDRV_PDM_CLK_64)
		&& (psPdmInfo->bClk != MCDRV_PDM_CLK_32))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_PDMADJ_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bAgcAdjust != MCDRV_AGCADJ_24)
		&& (psPdmInfo->bAgcAdjust != MCDRV_AGCADJ_18)
		&& (psPdmInfo->bAgcAdjust != MCDRV_AGCADJ_12)
		&& (psPdmInfo->bAgcAdjust != MCDRV_AGCADJ_0))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_PDMAGC_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bAgcOn != MCDRV_AGC_OFF)
		&& (psPdmInfo->bAgcOn != MCDRV_AGC_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_PDMEDGE_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bPdmEdge != MCDRV_PDMEDGE_LH)
		&& (psPdmInfo->bPdmEdge != MCDRV_PDMEDGE_HL))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_PDMWAIT_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bPdmWait != MCDRV_PDMWAIT_0)
		&& (psPdmInfo->bPdmWait != MCDRV_PDMWAIT_1)
		&& (psPdmInfo->bPdmWait != MCDRV_PDMWAIT_10)
		&& (psPdmInfo->bPdmWait != MCDRV_PDMWAIT_20))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_PDMSEL_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bPdmSel != MCDRV_PDMSEL_L1R2)
		&& (psPdmInfo->bPdmSel != MCDRV_PDMSEL_L2R1)
		&& (psPdmInfo->bPdmSel != MCDRV_PDMSEL_L1R1)
		&& (psPdmInfo->bPdmSel != MCDRV_PDMSEL_L2R2))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_PDMMONO_UPDATE_FLAG) != 0UL)
	{
		if((psPdmInfo->bMono != MCDRV_PDM_STEREO)
		&& (psPdmInfo->bMono != MCDRV_PDM_MONO))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidatePdmParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidatePdmExParam
 *
 *	Description:
 *			validate PDM_EX parameters.
 *	Arguments:
 *			psPdmExInfo	PDM_EX information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidatePdmExParam
(
	const MCDRV_PDM_EX_INFO*	psPdmExInfo,
	UINT32						dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidatePdmExParam");
#endif


	if((dUpdateInfo & MCDRV_AGCENV_UPDATE_FLAG) != 0UL)
	{
		if((psPdmExInfo->bAgcEnv != MCDRV_AGCENV_5310)
		&& (psPdmExInfo->bAgcEnv != MCDRV_AGCENV_10650)
		&& (psPdmExInfo->bAgcEnv != MCDRV_AGCENV_21310)
		&& (psPdmExInfo->bAgcEnv != MCDRV_AGCENV_85230))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCLVL_UPDATE_FLAG) != 0UL)
	{
		if((psPdmExInfo->bAgcLvl != MCDRV_AGCLVL_3)
		&& (psPdmExInfo->bAgcLvl != MCDRV_AGCLVL_6)
		&& (psPdmExInfo->bAgcLvl != MCDRV_AGCLVL_9)
		&& (psPdmExInfo->bAgcLvl != MCDRV_AGCLVL_12))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCUPTIM_UPDATE_FLAG) != 0UL)
	{
		if((psPdmExInfo->bAgcUpTim != MCDRV_AGCUPTIM_341)
		&& (psPdmExInfo->bAgcUpTim != MCDRV_AGCUPTIM_683)
		&& (psPdmExInfo->bAgcUpTim != MCDRV_AGCUPTIM_1365)
		&& (psPdmExInfo->bAgcUpTim != MCDRV_AGCUPTIM_2730))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCDWTIM_UPDATE_FLAG) != 0UL)
	{
		if((psPdmExInfo->bAgcDwTim != MCDRV_AGCDWTIM_5310)
		&& (psPdmExInfo->bAgcDwTim != MCDRV_AGCDWTIM_10650)
		&& (psPdmExInfo->bAgcDwTim != MCDRV_AGCDWTIM_21310)
		&& (psPdmExInfo->bAgcDwTim != MCDRV_AGCDWTIM_85230))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTLVL_UPDATE_FLAG) != 0UL)
	{
		if((psPdmExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_OFF)
		&& (psPdmExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_80)
		&& (psPdmExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_70)
		&& (psPdmExInfo->bAgcCutLvl != MCDRV_AGCCUTLVL_60))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTTIM_UPDATE_FLAG) != 0UL)
	{
		if((psPdmExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_5310)
		&& (psPdmExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_10650)
		&& (psPdmExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_341000)
		&& (psPdmExInfo->bAgcCutTim != MCDRV_AGCCUTTIM_2730000))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateAPdmExParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateDtmfParam
 *
 *	Description:
 *			validate DTMF parameters.
 *	Arguments:
 *			psDtmfInfo	DTMF information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateDtmfParam
(
	const MCDRV_DTMF_INFO*	psDtmfInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_DTMF_INFO	sDtmfInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateDtmfParam");
#endif

	McResCtrl_GetDtmfInfo(&sDtmfInfo);

	if((dUpdateInfo & MCDRV_DTMFHOST_UPDATE_FLAG) != 0UL)
	{
		if((psDtmfInfo->bSinGenHost != MCDRV_DTMFHOST_REG)
		&& (psDtmfInfo->bSinGenHost != MCDRV_DTMFHOST_PAD0)
		&& (psDtmfInfo->bSinGenHost != MCDRV_DTMFHOST_PAD1)
		&& (psDtmfInfo->bSinGenHost != MCDRV_DTMFHOST_PAD2))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_DTMFONOFF_UPDATE_FLAG) != 0UL)
	{
		if((psDtmfInfo->bOnOff != MCDRV_DTMF_OFF)
		&& (psDtmfInfo->bOnOff != MCDRV_DTMF_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			sDtmfInfo.bOnOff	= psDtmfInfo->bOnOff;
		}
	}

	if(sDtmfInfo.bOnOff == MCDRV_DTMF_ON)
	{
		if((dUpdateInfo & MCDRV_DTMFFREQ0_UPDATE_FLAG) != 0UL)
		{
			if((psDtmfInfo->sParam.wSinGen0Freq < MCDRV_SINGENFREQ_MIN)
			|| (psDtmfInfo->sParam.wSinGen0Freq > MCDRV_SINGENFREQ_MAX))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		if((dUpdateInfo & MCDRV_DTMFFREQ1_UPDATE_FLAG) != 0UL)
		{
			if((psDtmfInfo->sParam.wSinGen1Freq < MCDRV_SINGENFREQ_MIN)
			|| (psDtmfInfo->sParam.wSinGen1Freq > MCDRV_SINGENFREQ_MAX))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}

		if((dUpdateInfo & MCDRV_DTMFLOOP_UPDATE_FLAG) != 0UL)
		{
			if((psDtmfInfo->sParam.bSinGenLoop != MCDRV_SINGENLOOP_OFF)
			&& (psDtmfInfo->sParam.bSinGenLoop != MCDRV_SINGENLOOP_ON))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateDtmfParam", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	ValidateGpParam
 *
 *	Description:
 *			validate GP parameters.
 *	Arguments:
 *			psGpInfo	GP information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateGpParam
(
	const MCDRV_GP_MODE*	psGpMode
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bPad;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateGpParam");
#endif

	for(bPad = 0; (bPad < GPIO_PAD_NUM) && (sdRet == MCDRV_SUCCESS); bPad++)
	{
		if((bPad == MCDRV_GP_PAD2)
		&& (McDevProf_IsValid(eMCDRV_FUNC_PAD2) == 0))
		{
		}
		else
		{
			if((psGpMode->abGpDdr[bPad] != MCDRV_GPDDR_IN)
			&& (psGpMode->abGpDdr[bPad] != MCDRV_GPDDR_OUT))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
			else
			{
				if(McDevProf_IsValid(eMCDRV_FUNC_IRQ) != 0)
				{
					if((psGpMode->abGpDdr[bPad] == MCDRV_GPDDR_IN)
					&& (psGpMode->abGpIrq[bPad] != MCDRV_GPIRQ_DISABLE)
					&& (psGpMode->abGpIrq[bPad] != MCDRV_GPIRQ_ENABLE))
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
				}
				if(sdRet == MCDRV_SUCCESS)
				{
					if(McDevProf_IsValid(eMCDRV_FUNC_GPMODE) != 0)
					{
						if((psGpMode->abGpDdr[bPad] == MCDRV_GPDDR_IN)
						&& (psGpMode->abGpMode[bPad] != MCDRV_GPMODE_RISING)
						&& (psGpMode->abGpMode[bPad] != MCDRV_GPMODE_FALLING)
						&& (psGpMode->abGpMode[bPad] != MCDRV_GPMODE_BOTH))
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
						else if((psGpMode->abGpHost[bPad] != MCDRV_GPHOST_SCU)
						&& (psGpMode->abGpHost[bPad] != MCDRV_GPHOST_CDSP))
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
						else if((psGpMode->abGpInvert[bPad] != MCDRV_GPINV_NORMAL)
						&& (psGpMode->abGpInvert[bPad] != MCDRV_GPINV_INVERT))
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
						else
						{
						}
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateGpParam", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	ValidateMaskGp
 *
 *	Description:
 *			validate GP parameters.
 *	Arguments:
 *			bMask	MaskGP information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateMaskGp
(
	UINT8	bMask
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateMaskGp");
#endif

	if((bMask != MCDRV_GPMASK_ON) && (bMask != MCDRV_GPMASK_OFF))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateMaskGp", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	ValidateSysEqParam
 *
 *	Description:
 *			validate SysEQ parameters.
 *	Arguments:
 *			psSysEqInfo	SysEQ information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateSysEqParam
(
	const MCDRV_SYSEQ_INFO*	psSysEqInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateSysEqParam");
#endif


	if((dUpdateInfo & MCDRV_SYSEQ_ONOFF_UPDATE_FLAG) != 0UL)
	{
		if((psSysEqInfo->bOnOff != MCDRV_SYSEQ_OFF)
		&& (psSysEqInfo->bOnOff != MCDRV_SYSEQ_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateSysEqParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	ValidateDitSwapParam
 *
 *	Description:
 *			validate DitSwap parameters.
 *	Arguments:
 *			psDitSwapInfo	DitSwap information
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	ValidateDitSwapParam
(
	const MCDRV_DITSWAP_INFO*	psDitSwapInfo,
	UINT32					dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateDitSwapParam");
#endif


	if((dUpdateInfo & MCDRV_DITSWAP_L_UPDATE_FLAG) != 0UL)
	{
		if((psDitSwapInfo->bSwapL != MCDRV_DITSWAP_OFF)
		&& (psDitSwapInfo->bSwapL != MCDRV_DITSWAP_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	if((dUpdateInfo & MCDRV_DITSWAP_R_UPDATE_FLAG) != 0UL)
	{
		if((psDitSwapInfo->bSwapR != MCDRV_DITSWAP_OFF)
		&& (psDitSwapInfo->bSwapR != MCDRV_DITSWAP_ON))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateDitSwapParam", &sdRet);
#endif

	return sdRet;
}


/****************************************************************************
 *	CheckDIOCommon
 *
 *	Description:
 *			validate Digital IO Common parameters.
 *	Arguments:
 *			psDioInfo	digital IO information
 *			bPort		port number
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	CheckDIOCommon
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT8	bPort
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CheckDIOCommon");
#endif


	if((psDioInfo->asPortInfo[bPort].sDioCommon.bMasterSlave != MCDRV_DIO_SLAVE)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bMasterSlave != MCDRV_DIO_MASTER))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psDioInfo->asPortInfo[bPort].sDioCommon.bAutoFs != MCDRV_AUTOFS_OFF)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bAutoFs != MCDRV_AUTOFS_ON))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_48000)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_44100)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_32000)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_24000)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_22050)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_16000)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_12000)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_11025)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs != MCDRV_FS_8000))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_64)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_48)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_32)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_512)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_256)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_128)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs != MCDRV_BCKFS_16))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psDioInfo->asPortInfo[bPort].sDioCommon.bInterface != MCDRV_DIO_DA)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bInterface != MCDRV_DIO_PCM))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if((psDioInfo->asPortInfo[bPort].sDioCommon.bBckInvert != MCDRV_BCLK_NORMAL)
	&& (psDioInfo->asPortInfo[bPort].sDioCommon.bBckInvert != MCDRV_BCLK_INVERT))
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else if(psDioInfo->asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHizTim != MCDRV_PCMHIZTIM_FALLING)
		&& (psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHizTim != MCDRV_PCMHIZTIM_RISING))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDioCommon.bPcmClkDown != MCDRV_PCM_CLKDOWN_OFF)
		&& (psDioInfo->asPortInfo[bPort].sDioCommon.bPcmClkDown != MCDRV_PCM_CLKDOWN_HALF))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDioCommon.bPcmFrame != MCDRV_PCM_SHORTFRAME)
		&& (psDioInfo->asPortInfo[bPort].sDioCommon.bPcmFrame != MCDRV_PCM_LONGFRAME))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if(psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHighPeriod > 31)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
		}
	}
	else
	{
	}

	if(psDioInfo->asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_48000)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_44100)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_32000)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_24000)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_22050)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_12000)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_11025))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}

		if((psDioInfo->asPortInfo[bPort].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		&& (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_16000))
		{
			if(psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_512)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}
	else
	{
		if((psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_512)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_256)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_128)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_16))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CheckDIOCommon", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	CheckDIODIR
 *
 *	Description:
 *			validate Digital IO DIR parameters.
 *	Arguments:
 *			psDioInfo	digital IO information
 *			bPort		port number
 *			bInterface	Interface(DA/PCM)
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	CheckDIODIR
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT8	bPort,
	UINT8	bInterface
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bDIOCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CheckDIODIR");
#endif


	if(bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bMono != MCDRV_PCM_STEREO)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bMono != MCDRV_PCM_MONO))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder != MCDRV_PCM_MSB_FIRST)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder != MCDRV_PCM_LSB_FIRST)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder != MCDRV_PCM_MSB_FIRST_SIGN)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder != MCDRV_PCM_LSB_FIRST_SIGN)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder != MCDRV_PCM_MSB_FIRST_ZERO)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder != MCDRV_PCM_LSB_FIRST_ZERO))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw != MCDRV_PCM_LINEAR)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw != MCDRV_PCM_ALAW)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw != MCDRV_PCM_MULAW))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_8)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_13)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_14)
		&& (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_16))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw == MCDRV_PCM_ALAW)
			|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw == MCDRV_PCM_MULAW))
			{
				if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_13)
				|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_14)
				|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_16))
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bDIOCh = 0; bDIOCh < DIO_CHANNELS; bDIOCh++)
			{
				if(psDioInfo->asPortInfo[bPort].sDir.abSlot[bDIOCh] > MCDRV_PCM_SLOT_MAX)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}
	else
	{
		if((psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel != MCDRV_BITSEL_16)
		&& (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel != MCDRV_BITSEL_20)
		&& (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel != MCDRV_BITSEL_24))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode != MCDRV_DAMODE_HEADALIGN)
		&& (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode != MCDRV_DAMODE_I2S)
		&& (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode != MCDRV_DAMODE_TAILALIGN))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			for(bDIOCh = 0; bDIOCh < DIO_CHANNELS; bDIOCh++)
			{
				if(psDioInfo->asPortInfo[bPort].sDir.abSlot[bDIOCh] > MCDRV_DIR_SLOT_MAX)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CheckDIODIR", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	CheckDIDIT
 *
 *	Description:
 *			validate Digital IO DIT parameters.
 *	Arguments:
 *			psDioInfo	digital IO information
 *			bPort		port number
 *			bInterface	Interface(DA/PCM)
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	CheckDIODIT
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT8	bPort,
	UINT8	bInterface
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bDIOCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CheckDIODIT");
#endif


	if(bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bMono != MCDRV_PCM_STEREO)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bMono != MCDRV_PCM_MONO))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder != MCDRV_PCM_MSB_FIRST)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder != MCDRV_PCM_LSB_FIRST)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder != MCDRV_PCM_MSB_FIRST_SIGN)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder != MCDRV_PCM_LSB_FIRST_SIGN)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder != MCDRV_PCM_MSB_FIRST_ZERO)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder != MCDRV_PCM_LSB_FIRST_ZERO))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw != MCDRV_PCM_LINEAR)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw != MCDRV_PCM_ALAW)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw != MCDRV_PCM_MULAW))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_8)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_13)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_14)
		&& (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel != MCDRV_PCM_BITSEL_16))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw == MCDRV_PCM_ALAW)
			|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw == MCDRV_PCM_MULAW))
			{
				if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_13)
				|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_14)
				|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_16))
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bDIOCh = 0; bDIOCh < DIO_CHANNELS; bDIOCh++)
			{
				if(psDioInfo->asPortInfo[bPort].sDit.abSlot[bDIOCh] > MCDRV_PCM_SLOT_MAX)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}
	else
	{
		if((psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel != MCDRV_BITSEL_16)
		&& (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel != MCDRV_BITSEL_20)
		&& (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel != MCDRV_BITSEL_24))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		if((psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode != MCDRV_DAMODE_HEADALIGN)
		&& (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode != MCDRV_DAMODE_I2S)
		&& (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode != MCDRV_DAMODE_TAILALIGN))
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			for(bDIOCh = 0; bDIOCh < DIO_CHANNELS; bDIOCh++)
			{
				if(psDioInfo->asPortInfo[bPort].sDit.abSlot[bDIOCh] > MCDRV_DIT_SLOT_MAX)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CheckDIODIT", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	SetVol
 *
 *	Description:
 *			set volume.
 *	Arguments:
 *			dUpdate			target volume items
 *			eMode			update mode
 *			pdSVolDoneParam	wait soft volume complete flag
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *
 ****************************************************************************/
static SINT32	SetVol
(
	UINT32					dUpdate,
	MCDRV_VOLUPDATE_MODE	eMode,
	UINT32*					pdSVolDoneParam
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetVol");
#endif

	sdRet	= McPacket_AddVol(dUpdate, eMode, pdSVolDoneParam);
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McDevIf_ExecutePacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetVol", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	PreUpdatePath
 *
 *	Description:
 *			Preprocess update path.
 *	Arguments:
 *			pwDACMuteParam	wait DAC mute complete flag
 *			pwDITMuteParam	wait DIT mute complete flag
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *
 ****************************************************************************/
static	SINT32	PreUpdatePath
(
	UINT16*	pwDACMuteParam,
	UINT16*	pwDITMuteParam
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg	= 0;
	UINT8	bReadReg= 0;
	UINT8	bLAT	= 0;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("PreUpdatePath");
#endif

	*pwDACMuteParam	= 0;
	*pwDITMuteParam	= 0;

	switch(McResCtrl_GetDACSource(eMCDRV_DAC_MASTER))
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_DAC_SOURCE_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_DAC_SOURCE_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_DAC_SOURCE_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_DAC_SOURCE_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_DAC_SOURCE_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_DAC_SOURCE_MIX;
		break;
	default:
		bReg	= 0;
	}
	bReadReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SOURCE);
	if(((bReadReg & 0xF0) != 0) && (bReg != (bReadReg & 0xF0)))
	{/*	DAC source changed	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_MASTER_OUTL)&MCB_MASTER_OUTL) != 0)
		{
			if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_MASTER_OUTR) != 0)
			{
				bLAT	= MCB_MASTER_OLAT;
				*pwDACMuteParam	|= MCB_MASTER_OFLAGR;
			}
			else
			{
				bLAT	= 0;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_MASTER_OUTL, bLAT);
			*pwDACMuteParam	|= (UINT16)(MCB_MASTER_OFLAGL<<8);
		}
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_MASTER_OUTR) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_MASTER_OUTR, 0);
			*pwDACMuteParam	|= MCB_MASTER_OFLAGR;
		}
	}

	switch(McResCtrl_GetDACSource(eMCDRV_DAC_VOICE))
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_VOICE_SOURCE_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_VOICE_SOURCE_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_VOICE_SOURCE_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_VOICE_SOURCE_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_VOICE_SOURCE_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_VOICE_SOURCE_MIX;
		break;
	default:
		bReg	= 0;
	}
	if(((bReadReg & 0x0F) != 0) && (bReg != (bReadReg & 0x0F)))
	{/*	VOICE source changed	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_VOICE_ATTL)&MCB_VOICE_ATTL) != 0)
		{
			if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_VOICE_ATTR) != 0)
			{
				bLAT	= MCB_VOICE_LAT;
				*pwDACMuteParam	|= MCB_VOICE_FLAGR;
			}
			else
			{
				bLAT	= 0;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_VOICE_ATTL, bLAT);
			*pwDACMuteParam	|= (UINT16)(MCB_VOICE_FLAGL<<8);
		}
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_VOICE_ATTR) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_VOICE_ATTR, 0);
			*pwDACMuteParam	|= MCB_VOICE_FLAGR;
		}
	}

	switch(McResCtrl_GetDITSource(eMCDRV_DIO_0))
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_DIT0_SOURCE_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_DIT0_SOURCE_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_DIT0_SOURCE_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_DIT0_SOURCE_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_DIT0_SOURCE_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_DIT0_SOURCE_MIX;
		break;
	default:
		bReg	= 0;
	}
	bReadReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SRC_SOURCE);
	if(((bReadReg & 0xF0) != 0) && (bReg != (bReadReg & 0xF0)))
	{/*	DIT source changed	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT0_INVOLL)&MCB_DIT0_INVOLL) != 0)
		{
			if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT0_INVOLR) != 0)
			{
				bLAT	= MCB_DIT0_INLAT;
				*pwDITMuteParam	|= MCB_DIT0_INVFLAGR;
			}
			else
			{
				bLAT	= 0;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT0_INVOLL, bLAT);
			*pwDITMuteParam	|= (UINT16)(MCB_DIT0_INVFLAGL<<8);
		}
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT0_INVOLR) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT0_INVOLR, 0);
			*pwDITMuteParam	|= MCB_DIT0_INVFLAGR;
		}
	}

	switch(McResCtrl_GetDITSource(eMCDRV_DIO_1))
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_DIT1_SOURCE_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_DIT1_SOURCE_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_DIT1_SOURCE_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_DIT1_SOURCE_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_DIT1_SOURCE_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_DIT1_SOURCE_MIX;
		break;
	default:
		bReg	= 0;
	}
	if(((bReadReg & 0x0F) != 0) && (bReg != (bReadReg & 0x0F)))
	{/*	DIT source changed	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT1_INVOLL)&MCB_DIT1_INVOLL) != 0)
		{
			if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT1_INVOLR) != 0)
			{
				bLAT	= MCB_DIT1_INLAT;
				*pwDITMuteParam	|= MCB_DIT1_INVFLAGR;
			}
			else
			{
				bLAT	= 0;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT1_INVOLL, bLAT);
			*pwDITMuteParam	|= (UINT16)(MCB_DIT1_INVFLAGL<<8);
		}
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT1_INVOLR) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT1_INVOLR, 0);
			*pwDITMuteParam	|= MCB_DIT1_INVFLAGR;
		}
	}

	eSrcType	= McResCtrl_GetDITSource(eMCDRV_DIO_2);
	if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_CDSP_DIRECT))
	{
		eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
		if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
		{
			eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
		}
	}
	switch(eSrcType)
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_DIT2_SOURCE_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_DIT2_SOURCE_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_DIT2_SOURCE_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_DIT2_SOURCE_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_DIT2_SOURCE_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_DIT2_SOURCE_MIX;
		break;
	default:
		bReg	= 0;
	}
	bReadReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SRC_SOURCE_1);
	if(((bReadReg & 0x0F) != 0) && (bReg != (bReadReg & 0x0F)))
	{/*	DIT source changed	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2_INVOLL)&MCB_DIT2_INVOLL) != 0)
		{
			if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2_INVOLR) != 0)
			{
				bLAT	= MCB_DIT2_INLAT;
				*pwDITMuteParam	|= MCB_DIT2_INVFLAGR;
			}
			else
			{
				bLAT	= 0;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2_INVOLL, bLAT);
			*pwDITMuteParam	|= (UINT16)(MCB_DIT2_INVFLAGL<<8);
		}
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2_INVOLR) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2_INVOLR, 0);
			*pwDITMuteParam	|= MCB_DIT2_INVFLAGR;
		}
	}

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
	switch(eSrcType)
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_DIT2SRC_SOURCE1L_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_DIT2SRC_SOURCE1L_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_DIT2SRC_SOURCE1L_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_DIT2SRC_SOURCE1L_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_DIT2SRC_SOURCE1L_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_DIT2SRC_SOURCE1L_MIX;
		break;
	default:
		bReg	= 0;
	}
	bReadReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2SRC_SOURCE) & 0x70;
	if((bReadReg != 0) && (bReg != bReadReg))
	{/*	source changed	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2_INVOLL1)&MCB_DIT2_INVOLL1) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2_INVOLL1, 0);
			*pwDITMuteParam	|= (UINT16)(MCB_DIT2_INVFLAGL1<<8);
		}
	}

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
	switch(eSrcType)
	{
	case	eMCDRV_SRC_PDM:
	case	eMCDRV_SRC_ADC0:
	case	eMCDRV_SRC_DTMF:
		bReg	= MCB_DIT2SRC_SOURCE1R_AD;
		break;
	case	eMCDRV_SRC_ADC1:
		bReg	= MCB_DIT2SRC_SOURCE1R_ADC1;
		break;
	case	eMCDRV_SRC_DIR0:
		bReg	= MCB_DIT2SRC_SOURCE1R_DIR0;
		break;
	case	eMCDRV_SRC_DIR1:
		bReg	= MCB_DIT2SRC_SOURCE1R_DIR1;
		break;
	case	eMCDRV_SRC_DIR2:
	case	eMCDRV_SRC_CDSP:
		bReg	= MCB_DIT2SRC_SOURCE1R_DIR2;
		break;
	case	eMCDRV_SRC_MIX:
		bReg	= MCB_DIT2SRC_SOURCE1R_MIX;
		break;
	default:
		bReg	= 0;
	}
	bReadReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2SRC_SOURCE) & 0x07;
	if((bReadReg != 0) && (bReg != bReadReg))
	{/*	source changed	*/
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2_INVOLR1) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2_INVOLR1, 0);
			*pwDITMuteParam	|= MCB_DIT2_INVFLAGR1;
		}
	}

	sdRet	= McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("PreUpdatePath", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	SavePower
 *
 *	Description:
 *			Save power.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	SavePower
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SavePower");
#endif

	/*	unused path power down	*/
	McResCtrl_GetPowerInfo(&sPowerInfo);
	sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
	sPowerUpdate.abAnalog[0]	= (UINT8)MCDRV_POWUPDATE_ANALOG0_ALL;
	sPowerUpdate.abAnalog[1]	= (UINT8)MCDRV_POWUPDATE_ANALOG1_ALL;
	sPowerUpdate.abAnalog[2]	= (UINT8)MCDRV_POWUPDATE_ANALOG2_ALL;
	sPowerUpdate.abAnalog[3]	= (UINT8)MCDRV_POWUPDATE_ANALOG3_ALL;
	sPowerUpdate.abAnalog[4]	= (UINT8)MCDRV_POWUPDATE_ANALOG4_ALL;
	sPowerUpdate.abAnalog[5]	= (UINT8)MCDRV_POWUPDATE_ANALOG5_ALL;
	sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McDevIf_ExecutePacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SavePower", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McDrv_Ctrl
 *
 *	Description:
 *			MC Driver I/F function.
 *	Arguments:
 *			dCmd		command #
 *			pvPrm		parameter
 *			dPrm		update info
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_STATE
 *
 ****************************************************************************/
SINT32	McDrv_Ctrl
(
	UINT32	dCmd,
	void*	pvPrm,
	UINT32	dPrm
)
{
	SINT32	sdRet	= MCDRV_ERROR;

#if MCDRV_DEBUG_LEVEL
	McDebugLog_CmdIn(dCmd, pvPrm, dPrm);
#endif

	if((UINT32)MCDRV_INIT == dCmd)
	{
		sdRet	= init((MCDRV_INIT_INFO*)pvPrm);
	}
	else if((UINT32)MCDRV_TERM == dCmd)
	{
		sdRet	= term();
	}
	else if((UINT32)MCDRV_IRQ == dCmd)
	{
		sdRet	= irq_proc();
	}
	else
	{
		McSrv_Lock();

		switch (dCmd)
		{
		case	MCDRV_UPDATE_CLOCK:
			sdRet	= update_clock((MCDRV_CLOCK_INFO*)pvPrm);
			break;
		case	MCDRV_SWITCH_CLOCK:
			sdRet	= switch_clock((MCDRV_CLKSW_INFO*)pvPrm);
			break;

		case	MCDRV_GET_PATH:
			sdRet	= get_path((MCDRV_PATH_INFO*)pvPrm);
			break;
		case	MCDRV_SET_PATH:
			sdRet	= set_path((MCDRV_PATH_INFO*)pvPrm);
			break;

		case	MCDRV_GET_VOLUME:
			sdRet	= get_volume((MCDRV_VOL_INFO*)pvPrm);
			break;
		case	MCDRV_SET_VOLUME:
			sdRet	= set_volume((MCDRV_VOL_INFO*)pvPrm);
			break;

		case	MCDRV_GET_DIGITALIO:
			sdRet	= get_digitalio((MCDRV_DIO_INFO*)pvPrm);
			break;
		case	MCDRV_SET_DIGITALIO:
			sdRet	= set_digitalio((MCDRV_DIO_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_DAC:
			sdRet	= get_dac((MCDRV_DAC_INFO*)pvPrm);
			break;
		case	MCDRV_SET_DAC:
			sdRet	= set_dac((MCDRV_DAC_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_ADC:
			sdRet	= get_adc((MCDRV_ADC_INFO*)pvPrm);
			break;
		case	MCDRV_SET_ADC:
			sdRet	= set_adc((MCDRV_ADC_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_ADC_EX:
			sdRet	= get_adc_ex((MCDRV_ADC_EX_INFO*)pvPrm);
			break;
		case	MCDRV_SET_ADC_EX:
			sdRet	= set_adc_ex((MCDRV_ADC_EX_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_SP:
			sdRet	= get_sp((MCDRV_SP_INFO*)pvPrm);
			break;
		case	MCDRV_SET_SP:
			sdRet	= set_sp((MCDRV_SP_INFO*)pvPrm);
			break;

		case	MCDRV_GET_DNG:
			sdRet	= get_dng((MCDRV_DNG_INFO*)pvPrm);
			break;
		case	MCDRV_SET_DNG:
			sdRet	= set_dng((MCDRV_DNG_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_SET_AUDIOENGINE:
			sdRet	= set_ae((MCDRV_AE_INFO*)pvPrm, dPrm);
			break;
		case	MCDRV_SET_AUDIOENGINE_EX:
			sdRet	= set_ae_ex((UINT8*)pvPrm, dPrm);
			break;
		case	MCDRV_SET_EX_PARAM:
			sdRet	= set_ex_param((MCDRV_EXPARAM*)pvPrm);
			break;

		case	MCDRV_SET_CDSP:
			sdRet	= set_cdsp((UINT8*)pvPrm, dPrm);
			break;
		case	MCDRV_SET_CDSP_PARAM:
			sdRet	= set_cdspprm((MCDRV_CDSPPARAM*)pvPrm);
			break;

		case	MCDRV_GET_PDM:
			sdRet	= get_pdm((MCDRV_PDM_INFO*)pvPrm);
			break;
		case	MCDRV_SET_PDM:
			sdRet	= set_pdm((MCDRV_PDM_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_PDM_EX:
			sdRet	= get_pdm_ex((MCDRV_PDM_EX_INFO*)pvPrm);
			break;
		case	MCDRV_SET_PDM_EX:
			sdRet	= set_pdm_ex((MCDRV_PDM_EX_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_DTMF:
			sdRet	= get_dtmf((MCDRV_DTMF_INFO*)pvPrm);
			break;
		case	MCDRV_SET_DTMF:
			sdRet	= set_dtmf((MCDRV_DTMF_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_CONFIG_GP:
			sdRet	= config_gp((MCDRV_GP_MODE*)pvPrm);
			break;
		case	MCDRV_MASK_GP:
			sdRet	= mask_gp((UINT8*)pvPrm, dPrm);
			break;
		case	MCDRV_GETSET_GP:
			sdRet	= getset_gp((UINT8*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_PEAK:
			sdRet	= get_peak((MCDRV_PEAK*)pvPrm);
			break;

		case	MCDRV_GET_SYSEQ:
			sdRet	= get_syseq((MCDRV_SYSEQ_INFO*)pvPrm);
			break;
		case	MCDRV_SET_SYSEQ:
			sdRet	= set_syseq((MCDRV_SYSEQ_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_GET_DITSWAP:
			sdRet	= get_ditswap((MCDRV_DITSWAP_INFO*)pvPrm);
			break;
		case	MCDRV_SET_DITSWAP:
			sdRet	= set_ditswap((MCDRV_DITSWAP_INFO*)pvPrm, dPrm);
			break;

		case	MCDRV_READ_REG :
			sdRet	= read_reg((MCDRV_REG_INFO*)pvPrm);
			break;
		case	MCDRV_WRITE_REG :
			sdRet	= write_reg((MCDRV_REG_INFO*)pvPrm);
			break;

		default:
			break;
		}

		McSrv_Unlock();
	}

#if MCDRV_DEBUG_LEVEL
	McDebugLog_CmdOut(dCmd, &sdRet, pvPrm);
#endif

	return sdRet;
}

