/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcpacking.c
 *
 *		Description	: MC Driver packet packing control
 *
 *		Version		: 2.0.3 	2011.11.10
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/

#include "mcpacking.h"
#include "mcdevif.h"
#include "mcresctrl.h"
#include "mcdefs.h"
#include "mcdevprof.h"
#include "mcservice.h"
#include "mcmachdep.h"
#if MCDRV_DEBUG_LEVEL
#include "mcdebuglog.h"
#endif
#ifdef	MCDRV_USE_CDSP_DRIVER
#include "mccdspdrv.h"
#endif




#define	MCDRV_TCXO_WAIT_TIME	((UINT32)2000)
#define	MCDRV_PLLRST_WAIT_TIME	((UINT32)2000)
#define	MCDRV_PLL1RST_WAIT_TIME	((UINT32)100)
#define	MCDRV_PSW_WAIT_TIME		((UINT32)50)
#define	MCDRV_LDOA_WAIT_TIME	((UINT32)1000)
#define	MCDRV_SP_WAIT_TIME		((UINT32)150)
#define	MCDRV_HP_WAIT_TIME		((UINT32)300)
#define	MCDRV_RC_WAIT_TIME		((UINT32)150)
#define	MCDRV_TRAM_REST_WAIT_TIME	((UINT32)((2000000+48000-1)/48000))

/* SrcRate default */
#define	MCDRV_DIR_SRCRATE_48000	(32768)
#define	MCDRV_DIR_SRCRATE_44100	(30106)
#define	MCDRV_DIR_SRCRATE_32000	(21845)
#define	MCDRV_DIR_SRCRATE_24000	(16384)
#define	MCDRV_DIR_SRCRATE_22050	(15053)
#define	MCDRV_DIR_SRCRATE_16000	(10923)
#define	MCDRV_DIR_SRCRATE_12000	(8192)
#define	MCDRV_DIR_SRCRATE_11025	(7526)
#define	MCDRV_DIR_SRCRATE_8000	(5461)

#define	MCDRV_DIT_SRCRATE_48000	(4096)
#define	MCDRV_DIT_SRCRATE_44100	(4458)
#define	MCDRV_DIT_SRCRATE_32000	(6144)
#define	MCDRV_DIT_SRCRATE_24000	(8192)
#define	MCDRV_DIT_SRCRATE_22050	(8916)
#define	MCDRV_DIT_SRCRATE_16000	(12288)
#define	MCDRV_DIT_SRCRATE_12000	(16384)
#define	MCDRV_DIT_SRCRATE_11025	(17833)
#define	MCDRV_DIT_SRCRATE_8000	(24576)

#ifdef MCDRV_USE_CDSP_DRIVER
static MC_CODER_PARAMS	gsSendPrm	= {0xFF, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
#endif

/* DSP coef */
#define	MCDRV_COMMONCOEF_SIZE	(8)
static const UINT8 gabCommonCoef[MCDRV_COMMONCOEF_SIZE] =
{
	0x40, 0x00, 0x7F, 0xFF,
	0xC0, 0x00, 0x80, 0x00
};

static SINT32	InitBlockB(void);
static void		AddInitDigtalIO(void);
static void		AddInitGPIO(void);
static SINT32	AddAnalogPowerUpAuto(const MCDRV_POWER_INFO* psPowerInfo, const MCDRV_POWER_UPDATE* psPowerUpdate);

static void		AddDIPad(void);
static void		AddPAD(void);
static SINT32	AddSource(void);
static void		AddPMSource(void);
static SINT32	AddCDSPSource(void);
static void		AddDIStart(void);
static UINT8	GetMicMixBit(const MCDRV_CHANNEL* psChannel);
static void		AddDigVolPacket(UINT8 bVolL, UINT8 bVolR, UINT8 bVolLAddr, UINT8 bLAT, UINT8 bVolRAddr, MCDRV_VOLUPDATE_MODE eMode);
static void		AddStopADC(void);
static void		AddStopPDM(void);
static UINT8	IsModifiedDIO(UINT32 dUpdateInfo);
static UINT8	IsModifiedDIOCommon(MCDRV_DIO_PORT_NO ePort);
static UINT8	IsModifiedDIODIR(MCDRV_DIO_PORT_NO ePort);
static UINT8	IsModifiedDIODIT(MCDRV_DIO_PORT_NO ePort);
static void		AddDIOCommon(MCDRV_DIO_PORT_NO ePort);
static void		AddDIODIR(MCDRV_DIO_PORT_NO ePort);
static void		AddDIODIT(MCDRV_DIO_PORT_NO ePort);
static UINT16	GetSinGenFreqReg(UINT16 wFreq);

#define	MCDRV_DPB_KEEP	0
#define	MCDRV_DPB_UP	1
static SINT32	PowerUpDig(UINT8 bDPBUp);

static UINT32	GetMaxWait(UINT8 bRegChange);
#define	MCDRV_REGCHANGE_PWM_LI2	(0x08)

/****************************************************************************
 *	McPacket_AddInit
 *
 *	Description:
 *			Add initialize packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *
 ****************************************************************************/
SINT32	McPacket_AddInit
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	UINT8	bRegAna0;
	MCDRV_INIT_INFO		sInitInfo;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddInit");
#endif


	McResCtrl_GetInitInfo(&sInitInfo);

	bRegAna0	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0);

	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		/*	ANA_RST	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ANA_RST, MCI_ANA_RST_DEF);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ANA_RST, 0);

		if(sInitInfo.bLdo != MCDRV_LDO_AOFF_DOFF)
		{
			/*	AP_LDOA	*/
			if((sInitInfo.bLdo == MCDRV_LDO_AON_DOFF) || (sInitInfo.bLdo == MCDRV_LDO_AON_DON))
			{
				bRegAna0	&= (UINT8)~MCB_PWM_LDOA;
			}
			/*	LDOD	*/
			if((sInitInfo.bLdo == MCDRV_LDO_AOFF_DON) || (sInitInfo.bLdo == MCDRV_LDO_AON_DON))
			{
				bRegAna0	&= (UINT8)~MCB_PWM_LDOD;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bRegAna0);
			/*	1ms wait	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_LDOA_WAIT_TIME, 0);
		}

		/*	RSTA	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, MCB_RST);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, 0);

		/*	CD_RST	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_RST, MCI_CD_RST_DEF);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_RST, 0);

		AddInitDigtalIO();

		/*	CSDIN_MSK	*/
		bReg	= MCI_CSDIN_MSK_DEF & (UINT8)~MCB_CSDIN_MSK;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_CSDIN_MSK, bReg);
		/*	SDI_MSK, BCLKLRCK_MSK	*/
		bReg	= MCI_CD_MSK_DEF & (UINT8)~(MCB_SDI_MSK|MCB_BCLKLRCK_MSK);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_MSK, bReg);
	}
	else
	{
		/*	RSTA	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, MCB_RST);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, 0);
		/*	ANA_RST	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ANA_RST, MCI_ANA_RST_DEF);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ANA_RST, 0);

		AddInitDigtalIO();
	}

	AddInitGPIO();

	/*	CKSEL	*/
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CKSEL_06H, sInitInfo.bCkSel);
	}
	else
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CKSEL, sInitInfo.bCkSel);
	}

	/*	DIVR0, DIVF0	*/
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVR0, sInitInfo.bDivR0);
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVF0, sInitInfo.bDivF0);
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVR1, sInitInfo.bDivR1);
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVF1, sInitInfo.bDivF1);

	/*	Clock Start	*/
	sdRet	= McDevIf_ExecutePacket();
	if(sdRet == MCDRV_SUCCESS)
	{
		McSrv_ClockStart();

		/*	DP0	*/
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_CD_DP);
		bReg	&= (UINT8)~MCB_DP0_CLKI0;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
		if(sInitInfo.bCkSel != MCDRV_CKSEL_CMOS)
		{
			/*	2ms wait	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_TCXO_WAIT_TIME, 0);
		}

		/*	PLLRST0	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_PLL_RST, 0);
		/*	2ms wait	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_PLLRST_WAIT_TIME, 0);

		if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
		{
			/*	CD_DP1	*/
			bReg	&= (UINT8)~MCB_CD_DP1;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);

			/*	DPMCLKO	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DPMCLKO, 0);

			/*	DP0, PLL1	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PLL1RST, 0);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_PLL1RST_WAIT_TIME, 0);
		}

		/*	DP1/DP2 up	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL, 0);
		if((eDevID != eMCDRV_DEV_ID_46H) && (eDevID != eMCDRV_DEV_ID_44H))
		{
			/*	RSTB	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_RSTB, 0);

			/*	Common Coef	*/
			if(McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_BDSP, 0);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ADR, 0);
				McDevIf_AddPacketRepeat(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_WINDOW, gabCommonCoef, MCDRV_COMMONCOEF_SIZE);
			}
		}
		else
		{
			if(sInitInfo.bPowerMode != MCDRV_POWMODE_FULL)
			{
				/*	PSWB	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PSW, 0);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_PSW_WAIT_TIME, 0);
				/*	RSTB	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_RSTB, MCB_RSTC);
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					sdRet	= InitBlockB();
				}
			}
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		/*	DCL_GAIN, DCL_LMT	*/
		bReg	= (sInitInfo.bDclGain<<4) | sInitInfo.bDclLimit;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DCL, bReg);

		/*	DIF_LI, DIF_LO*	*/
		bReg	= (sInitInfo.bLineOut2Dif<<5) | (sInitInfo.bLineOut1Dif<<4);
		if(McDevProf_IsValid(eMCDRV_FUNC_LI1) == 1)
		{
			bReg	|= sInitInfo.bLineIn1Dif;
		}
		if(McDevProf_IsValid(eMCDRV_FUNC_LI2) == 1)
		{
			bReg	|= (sInitInfo.bLineIn2Dif<<1);
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_DIF_LINE, bReg);

		/*	SP*_HIZ, SPMN	*/
		bReg	= (sInitInfo.bSpmn << 1);
		if(MCDRV_SPHIZ_HIZ == sInitInfo.bSpHiz)
		{
			bReg	|= (MCB_SPL_HIZ|MCB_SPR_HIZ);
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SP_MODE, bReg);

		/*	MC*SNG	*/
		bReg	= (sInitInfo.bMic2Sng<<6) | (sInitInfo.bMic1Sng<<2);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC_GAIN, bReg);
		if(sInitInfo.bMic3Sng == MCDRV_MIC_DIF)
		{
			bReg	= 0;
		}
		else if(sInitInfo.bMic3Sng == MCDRV_MIC_SINGLE)
		{
			bReg	= MCB_MC3SNG;
		}
		else
		{
			if(eDevID == eMCDRV_DEV_ID_44H)
			{
				bReg	= MCB_LMSEL;
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC3_GAIN, bReg);

		/*	CPMOD	*/
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_CPMOD, sInitInfo.bCpMod);

		/*	AVDDLEV, VREFLEV	*/
		bReg	= 0x20 | sInitInfo.bAvddLev;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LEV, bReg);

		if(((sInitInfo.bPowerMode & MCDRV_POWMODE_VREFON) != 0) || (McResCtrl_GetAPMode() == eMCDRV_APM_OFF))
		{
			if((sInitInfo.bLdo == MCDRV_LDO_AON_DOFF) || (sInitInfo.bLdo == MCDRV_LDO_AON_DON))
			{
				if((eDevID != eMCDRV_DEV_ID_46H) && (eDevID != eMCDRV_DEV_ID_44H))
				{
					/*	AP_LDOA	*/
					bRegAna0	&= (UINT8)~MCB_PWM_LDOA;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bRegAna0);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_LDOA_WAIT_TIME, 0);
				}
			}
			else
			{
				bRegAna0	&= (UINT8)~MCB_PWM_REFA;
			}
			/*	AP_VR up	*/
			bRegAna0	&= (UINT8)~MCB_PWM_VR;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bRegAna0);
			sdRet	= McDevIf_ExecutePacket();
			if(sdRet == MCDRV_SUCCESS)
			{
				if((sInitInfo.bPowerMode & MCDRV_POWMODE_VREFON) != 0)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | sInitInfo.sWaitTime.dVrefRdy2, 0);
				}
			}
		}
	}

	if(MCDRV_SUCCESS == sdRet)
	{
		if(McResCtrl_GetAPMode() == eMCDRV_APM_OFF)
		{
			bReg	= MCB_APMOFF_SP|MCB_APMOFF_HP|MCB_APMOFF_RC;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_APMOFF, bReg);
		}
		sdRet	= McDevIf_ExecutePacket();
	}
	if(MCDRV_SUCCESS == sdRet)
	{
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
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddInit", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	InitBlockB
 *
 *	Description:
 *			Initialize B Block.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *
 ****************************************************************************/
static SINT32	InitBlockB
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT32	dUpdateFlg	= 0;
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitBlockB");
#endif

	dUpdateFlg	= MCDRV_DIO0_COM_UPDATE_FLAG|MCDRV_DIO0_DIR_UPDATE_FLAG|MCDRV_DIO0_DIT_UPDATE_FLAG
					|MCDRV_DIO1_COM_UPDATE_FLAG|MCDRV_DIO1_DIR_UPDATE_FLAG|MCDRV_DIO1_DIT_UPDATE_FLAG
					|MCDRV_DIO2_COM_UPDATE_FLAG|MCDRV_DIO2_DIR_UPDATE_FLAG|MCDRV_DIO2_DIT_UPDATE_FLAG;
	sdRet	= McPacket_AddDigitalIO(dUpdateFlg);
	if(sdRet == MCDRV_SUCCESS)
	{
		sdRet	= McDevIf_ExecutePacket();
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		dUpdateFlg	= MCDRV_DAC_MSWP_UPDATE_FLAG|MCDRV_DAC_VSWP_UPDATE_FLAG|MCDRV_DAC_HPF_UPDATE_FLAG;
		sdRet	= McPacket_AddDAC(dUpdateFlg);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		dUpdateFlg	= MCDRV_PDMCLK_UPDATE_FLAG|MCDRV_PDMADJ_UPDATE_FLAG|MCDRV_PDMAGC_UPDATE_FLAG|MCDRV_PDMEDGE_UPDATE_FLAG
						|MCDRV_PDMWAIT_UPDATE_FLAG|MCDRV_PDMSEL_UPDATE_FLAG|MCDRV_PDMMONO_UPDATE_FLAG;
		sdRet	= McPacket_AddPDM(dUpdateFlg);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		dUpdateFlg	= MCDRV_AGCENV_UPDATE_FLAG|MCDRV_AGCLVL_UPDATE_FLAG|MCDRV_AGCUPTIM_UPDATE_FLAG
						|MCDRV_AGCDWTIM_UPDATE_FLAG|MCDRV_AGCCUTLVL_UPDATE_FLAG|MCDRV_AGCCUTTIM_UPDATE_FLAG;
		sdRet	= McPacket_AddPDMEx(dUpdateFlg);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PWM_DIGITAL_BDSP);
		if((bReg&MCB_PWM_DPBDSP) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_BDSP, 0);
		}
		if(McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ADR, 0);
			McDevIf_AddPacketRepeat(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_WINDOW, gabCommonCoef, MCDRV_COMMONCOEF_SIZE);
		}
		dUpdateFlg	= MCDRV_AEUPDATE_FLAG_BEXWIDE_ONOFF|MCDRV_AEUPDATE_FLAG_DRC_ONOFF|MCDRV_AEUPDATE_FLAG_EQ5_ONOFF|MCDRV_AEUPDATE_FLAG_EQ3_ONOFF
						|MCDRV_AEUPDATE_FLAG_BEX|MCDRV_AEUPDATE_FLAG_WIDE|MCDRV_AEUPDATE_FLAG_DRC|MCDRV_AEUPDATE_FLAG_EQ5|MCDRV_AEUPDATE_FLAG_EQ3;
		sdRet	= McPacket_AddAE(dUpdateFlg);
		if((bReg&MCB_PWM_DPBDSP) != 0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_BDSP, MCB_PWM_DPBDSP);
		}
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		dUpdateFlg	= MCDRV_DTMFHOST_UPDATE_FLAG|MCDRV_DTMFONOFF_UPDATE_FLAG|MCDRV_DTMFFREQ0_UPDATE_FLAG|MCDRV_DTMFFREQ1_UPDATE_FLAG
						|MCDRV_DTMFVOL0_UPDATE_FLAG|MCDRV_DTMFVOL1_UPDATE_FLAG|MCDRV_DTMFGATE_UPDATE_FLAG|MCDRV_DTMFLOOP_UPDATE_FLAG;
		sdRet	= McPacket_AddDTMF(dUpdateFlg);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		dUpdateFlg	= MCDRV_DITSWAP_L_UPDATE_FLAG|MCDRV_DITSWAP_R_UPDATE_FLAG;
		sdRet	= McPacket_AddDitSwap(dUpdateFlg);
		if(sdRet == MCDRV_SUCCESS)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitBlockB", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	AddInitDigtalIO
 *
 *	Description:
 *			Add ditigal IO initialize packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddInitDigtalIO
(
	void
)
{
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddInitDigtalIO");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);

	/*	SDIN_MSK*, SDO_DDR*	*/
	bReg	= MCB_SDIN_MSK2;
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		if(sInitInfo.bDioSdo2Hiz == MCDRV_DAHIZ_LOW)
		{
			bReg |= MCB_SDO_DDR2;
		}
	}
	bReg |= MCB_SDIN_MSK1;
	if(sInitInfo.bDioSdo1Hiz == MCDRV_DAHIZ_LOW)
	{
		bReg |= MCB_SDO_DDR1;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_SD_MSK, bReg);

	bReg	= MCB_SDIN_MSK0;
	if(sInitInfo.bDioSdo0Hiz == MCDRV_DAHIZ_LOW)
	{
		bReg |= MCB_SDO_DDR0;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_SD_MSK_1, bReg);

	/*	BCLK_MSK*, BCLD_DDR*, LRCK_MSK*, LRCK_DDR*, PCM_HIZ*	*/
	bReg	= 0;
	bReg |= MCB_BCLK_MSK2;
	bReg |= MCB_LRCK_MSK2;
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		if(sInitInfo.bDioClk2Hiz == MCDRV_DAHIZ_LOW)
		{
			bReg |= MCB_BCLK_DDR2;
			bReg |= MCB_LRCK_DDR2;
		}
	}
	bReg |= MCB_BCLK_MSK1;
	bReg |= MCB_LRCK_MSK1;
	if(sInitInfo.bDioClk1Hiz == MCDRV_DAHIZ_LOW)
	{
		bReg |= MCB_BCLK_DDR1;
		bReg |= MCB_LRCK_DDR1;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_BCLK_MSK, bReg);

	bReg	= 0;
	bReg |= MCB_BCLK_MSK0;
	bReg |= MCB_LRCK_MSK0;
	if(sInitInfo.bDioClk0Hiz == MCDRV_DAHIZ_LOW)
	{
		bReg |= MCB_BCLK_DDR0;
		bReg |= MCB_LRCK_DDR0;
	}
	if(sInitInfo.bPcmHiz == MCDRV_PCMHIZ_HIZ)
	{
		bReg |= (MCB_PCMOUT_HIZ2 | MCB_PCMOUT_HIZ1 | MCB_PCMOUT_HIZ0);
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_BCLK_MSK_1, bReg);

	/*	SD*_DS	*/
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		bReg	= MCI_BCKP_DEF | sInitInfo.bSdDs;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_BCKP, bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddInitDigtalIO", NULL);
#endif
}

/****************************************************************************
 *	AddInitGPIO
 *
 *	Description:
 *			Add GPIO initialize packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddInitGPIO
(
	void
)
{
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddInitGPIO");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);

	/*	PA*_MSK, PA*_DDR	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		bReg	= MCI_PA2_MSK_DEF;
		if(sInitInfo.bPad2Func == MCDRV_PAD_IRQ)
		{
			bReg	|= MCB_PA2_DDR;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_MSK, bReg);
	}

	bReg	= MCI_PA_MSK_DEF;
	if((sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
	|| (sInitInfo.bPad0Func == MCDRV_PAD_IRQ))
	{
		bReg	|= MCB_PA0_DDR;
	}
	if((sInitInfo.bPad1Func == MCDRV_PAD_IRQ))
	{
		bReg	|= MCB_PA1_DDR;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_MSK, bReg);

	/*	PA_HOST	*/
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		if(sInitInfo.bPad2Func == MCDRV_PAD_IRQ)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_HOST, MCB_PA2_HOST_IRQ);
		}
		bReg	= 0;
		if(sInitInfo.bPad1Func == MCDRV_PAD_IRQ)
		{
			bReg	|= MCB_PA1_HOST_IRQ;
		}
		if(sInitInfo.bPad0Func == MCDRV_PAD_IRQ)
		{
			bReg	|= MCB_PA0_HOST_IRQ;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_HOST, bReg);
	}

	/*	PA0_OUT	*/
	if(sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_OUT, MCB_PA_OUT);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddInitGPIO", NULL);
#endif
}

/****************************************************************************
 *	McPacket_AddPowerUp
 *
 *	Description:
 *			Add powerup packet.
 *	Arguments:
 *			psPowerInfo		power information
 *			psPowerUpdate	power update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddPowerUp
(
	const MCDRV_POWER_INFO*		psPowerInfo,
	const MCDRV_POWER_UPDATE*	psPowerUpdate
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	UINT8	bRegDPADIF;
	UINT8	bRegDPB;
	UINT8	bRegCur;
	UINT8	bRegAna1;
	UINT8	bRegAna2;
	UINT32	dUpdate;
	UINT8	bRegChange;
	UINT8	bSpHizReg;
	UINT32	dWaitTime;
	UINT32	dWaitTimeDone	= 0;
	UINT8	bWaitVREFRDY	= 0;
	UINT8	bWaitHPVolUp	= 0;
	UINT8	bWaitSPVolUp	= 0;
	UINT8	bInitBlockB		= 0;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddPowerUp");
#endif

	bRegDPADIF	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_CD_DP);


	McResCtrl_GetInitInfo(&sInitInfo);

	/*	Digital Power	*/
	dUpdate	= ~psPowerInfo->dDigital & psPowerUpdate->dDigital;
	if((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) != 0UL)
	{
		if((bRegDPADIF & (MCB_DP0_CLKI1|MCB_DP0_CLKI0)) == (MCB_DP0_CLKI1|MCB_DP0_CLKI0))
		{/*	DP0 changed	*/
			if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
			{
				if(MCDRV_POWMODE_FULL == sInitInfo.bPowerMode)
				{
					if((sInitInfo.bLdo == MCDRV_LDO_AOFF_DON) || (sInitInfo.bLdo == MCDRV_LDO_AON_DON))
					{
						/*	LDOD	*/
						bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0) & (UINT8)~MCB_PWM_LDOD);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
						/*	1ms wait	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_LDOA_WAIT_TIME, 0);
					}

					/*	RSTA	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, MCB_RST);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, 0);

					McResCtrl_InitBaseBlockReg();

					/*	CSDIN_MSK	*/
					bReg	= MCI_CSDIN_MSK_DEF & (UINT8)~MCB_CSDIN_MSK;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_CSDIN_MSK, bReg);

					AddInitDigtalIO();

					/*	SDI_MSK, BCLKLRCK_MSK	*/
					bReg	= MCI_CD_MSK_DEF & (UINT8)~(MCB_SDI_MSK|MCB_BCLKLRCK_MSK);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_MSK, bReg);

					AddInitGPIO();
					sdRet	= McDevIf_ExecutePacket();
					if(sdRet == MCDRV_SUCCESS)
					{
						McPacket_AddGPMode();
						sdRet	= McDevIf_ExecutePacket();
						if(sdRet == MCDRV_SUCCESS)
						{
							McPacket_AddGPMask();
							McPacket_AddGPSet();
							sdRet	= McDevIf_ExecutePacket();
						}
					}
				}
			}

			if(sdRet == MCDRV_SUCCESS)
			{
				/*	CKSEL	*/
				if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CKSEL_06H, sInitInfo.bCkSel);
				}
				else
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CKSEL, sInitInfo.bCkSel);
				}
				/*	DIVR0, DIVF0	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVR0, sInitInfo.bDivR0);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVF0, sInitInfo.bDivF0);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVR1, sInitInfo.bDivR1);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DIVF1, sInitInfo.bDivF1);
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					/*	Clock Start	*/
					McSrv_ClockStart();

					/*	DP0 up	*/
					if((((bRegDPADIF & MCB_CLKSRC) == 0) && ((bRegDPADIF & MCB_CLKINPUT) == 0))
					|| (((bRegDPADIF & MCB_CLKSRC) != 0) && ((bRegDPADIF & MCB_CLKINPUT) != 0)))
					{
						bRegDPADIF	&= (UINT8)~MCB_DP0_CLKI0;
					}
					else
					{
						bRegDPADIF	&= (UINT8)~MCB_DP0_CLKI1;
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);
					if(sInitInfo.bCkSel != MCDRV_CKSEL_CMOS)
					{
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_TCXO_WAIT_TIME, 0);
					}
					/*	PLLRST0 up	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_PLL_RST, 0);
					/*	2ms wait	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_PLLRST_WAIT_TIME, 0);
					if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
					{
						/*	CD_DP1	*/
						bRegDPADIF	&= (UINT8)~MCB_CD_DP1;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);

						/*	DPMCLKO	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DPMCLKO, 0);

						/*	DP0, PLL1	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PLL1RST, 0);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_PLL1RST_WAIT_TIME, 0);
					}
					sdRet	= McDevIf_ExecutePacket();
				}
			}
		}

		if(sdRet == MCDRV_SUCCESS)
		{
			if((dUpdate & MCDRV_POWINFO_DIGITAL_DP2) != 0UL)
			{
				/*	DP1/DP2 up	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL, 0);
				/*	DPB/DPDI* up	*/
				bRegDPB	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PWM_DIGITAL_1);
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPB) != 0UL)
				{
					if((bRegDPB & MCB_PWM_DPB) != 0)
					{
						if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
						{
							if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PSW) & MCB_PSWB) != 0)
							{
								McResCtrl_InitBlockBReg();
								/*	PSWB	*/
								McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PSW, 0);
								McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_PSW_WAIT_TIME, 0);
								/*	RSTB	*/
								bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_RSTB);
								bReg	&= (UINT8)~MCB_RSTB;
								McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_RSTB, bReg);
								bInitBlockB	= 1;
								/*	CDI_CH3:3->2	*/
								McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_CDI_CH, MCI_CDI_CH_DEF);
							}
						}
						bRegDPB	&= (UINT8)~MCB_PWM_DPB;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_1, bRegDPB);
					}
				}

				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPDI0) != 0UL)
				{
					bRegDPB	&= (UINT8)~MCB_PWM_DPDI0;
				}
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPDI1) != 0UL)
				{
					bRegDPB	&= (UINT8)~MCB_PWM_DPDI1;
				}
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPDI2) != 0UL)
				{
					bRegDPB	&= (UINT8)~MCB_PWM_DPDI2;
				}
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPPDM) != 0UL)
				{
					bRegDPB	&= (UINT8)~MCB_PWM_DPPDM;
				}
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPCD) != 0UL)
				{
					bRegDPB	&= (UINT8)~MCB_PWM_DPCD;
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_1, bRegDPB);
				if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
				{
					if((bRegDPB & MCB_PWM_DPCD) == 0)
					{
						/*	CD_DP2	*/
						bRegDPADIF	&= (UINT8)~MCB_CD_DP2;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);
					}
				}
			}

			if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
			{
				/*	DPC	*/
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DPC) != 0UL)
				{
					bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PWM_DPC);
					if((bReg&MCB_PWM_DPC) != 0)
					{
						/*	RSTC	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_RSTB, 0);
						/*	DPC	*/
						bReg	&= (UINT8)~MCB_PWM_DPC;
						bReg	|= MCB_CDSP_FREQ;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DPC, bReg);
						sdRet	= McDevIf_ExecutePacket();
#ifdef MCDRV_USE_CDSP_DRIVER
						if(sdRet == MCDRV_SUCCESS)
						{
							McCdsp_Init();
						}
#endif
					}
				}
			}
		}
		if(sdRet == MCDRV_SUCCESS)
		{
			/*	DPBDSP	*/
			if((dUpdate & MCDRV_POWINFO_DIGITAL_DPBDSP) != 0UL)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_BDSP, 0);
			}
			/*	DPADIF	*/
			if((dUpdate & MCDRV_POWINFO_DIGITAL_DPADIF) != 0UL)
			{
				bRegDPADIF	&= (UINT8)~MCB_DPADIF;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);
			}

			if(bInitBlockB != 0)
			{
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					sdRet	= InitBlockB();
				}
			}
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if(McResCtrl_GetAPMode() == eMCDRV_APM_ON)
		{
			sdRet	= AddAnalogPowerUpAuto(psPowerInfo, psPowerUpdate);
		}
		else
		{
			/*	Analog Power	*/
			dUpdate	= (UINT32)~psPowerInfo->abAnalog[0] & (UINT32)psPowerUpdate->abAnalog[0];
			if((dUpdate & (UINT32)MCB_PWM_VR) != 0UL)
			{
				bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0);
				if((bReg & MCB_PWM_VR) != 0)
				{/*	AP_VR changed	*/
					if((sInitInfo.bLdo == MCDRV_LDO_AON_DOFF) || (sInitInfo.bLdo == MCDRV_LDO_AON_DON))
					{
						/*	AP_LDOA	*/
						bReg	&= (UINT8)~MCB_PWM_LDOA;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_LDOA_WAIT_TIME, 0);
					}
					else
					{
						bReg	&= (UINT8)~MCB_PWM_REFA;
					}
					/*	AP_VR up	*/
					bReg	&= (UINT8)~MCB_PWM_VR;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
					dWaitTimeDone	= sInitInfo.sWaitTime.dVrefRdy1;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTimeDone, 0);
					bWaitVREFRDY	= 1;
				}

				bReg	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[1] & psPowerUpdate->abAnalog[1]) & McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1));
				/*	SP_HIZ control	*/
				if(MCDRV_SPHIZ_HIZ == sInitInfo.bSpHiz)
				{
					bSpHizReg	= 0;
					if((bReg & (MCB_PWM_SPL1 | MCB_PWM_SPL2)) != 0)
					{
						bSpHizReg |= MCB_SPL_HIZ;
					}

					if((bReg & (MCB_PWM_SPR1 | MCB_PWM_SPR2)) != 0)
					{
						bSpHizReg |= MCB_SPR_HIZ;
					}

					bSpHizReg |= (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SP_MODE) & (MCB_SPMN | MCB_SP_SWAP));
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SP_MODE, bSpHizReg);
				}

				bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_3);
				bReg	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[3] & psPowerUpdate->abAnalog[3]) & bRegCur);
				bRegChange	= bReg ^ bRegCur;
				/*	set DACON and NSMUTE before setting 0 to AP_DA	*/
				if(((bRegChange & (MCB_PWM_DAR|MCB_PWM_DAL)) != 0) && ((psPowerInfo->abAnalog[3] & psPowerUpdate->abAnalog[3] & (MCB_PWM_DAR|MCB_PWM_DAL)) != (MCB_PWM_DAR|MCB_PWM_DAL)))
				{
					if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_DAC_CONFIG) & MCB_DACON) == 0)
					{
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, MCB_NSMUTE);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, (MCB_DACON | MCB_NSMUTE));
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, MCB_DACON);
					}
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_3, bReg);
				bRegChange	&= (MCB_PWM_MB1|MCB_PWM_MB2|MCB_PWM_MB3);

				bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_4);
				bReg	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[4] & psPowerUpdate->abAnalog[4]) & bRegCur);
				bRegChange	|= (bReg ^ bRegCur);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_4, bReg);

				/*if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)*/
				{
					bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_5);
					bReg	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[5] & psPowerUpdate->abAnalog[5]) & bRegCur);
					if(bReg != bRegCur)
					{
						bRegChange	|= MCDRV_REGCHANGE_PWM_LI2;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_5, bReg);
					}
				}

				if(bWaitVREFRDY != 0)
				{
					/*	wait VREF_RDY	*/
					dWaitTimeDone	= sInitInfo.sWaitTime.dVrefRdy2 - dWaitTimeDone;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTimeDone, 0);
				}

				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0);
					bReg	= (UINT8)(~dUpdate & bRegCur);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
					if(((bRegCur & MCB_PWM_CP) != 0) && ((bReg & MCB_PWM_CP) == 0))
					{/*	AP_CP up	*/
						dWaitTime		= MCDRV_HP_WAIT_TIME;
					}
					else
					{
						dWaitTime	= 0;
					}

					bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1);
					bRegAna1	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[1] & psPowerUpdate->abAnalog[1]) & bRegCur);
					if((((bRegCur & MCB_PWM_SPL1) != 0) && ((bRegAna1 & MCB_PWM_SPL1) == 0))
					|| (((bRegCur & MCB_PWM_SPR1) != 0) && ((bRegAna1 & MCB_PWM_SPR1) == 0)))
					{/*	AP_SP* up	*/
						bReg	= bRegAna1|(bRegCur&(UINT8)~(MCB_PWM_SPL1|MCB_PWM_SPR1));
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_1, bReg);
						if(dWaitTime == (UINT32)0)
						{
							dWaitTime		= MCDRV_SP_WAIT_TIME;
							bWaitSPVolUp	= 1;
						}
					}
					if((((bRegCur & MCB_PWM_HPL) != 0) && ((bRegAna1 & MCB_PWM_HPL) == 0))
					|| (((bRegCur & MCB_PWM_HPR) != 0) && ((bRegAna1 & MCB_PWM_HPR) == 0)))
					{
						bWaitHPVolUp	= 1;
					}

					bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_2);
					bRegAna2	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[2] & psPowerUpdate->abAnalog[2]) & bRegCur);
					if(((bRegCur & MCB_PWM_RC1) != 0) && ((bRegAna2 & MCB_PWM_RC1) == 0))
					{/*	AP_RC up	*/
						bReg	= bRegAna2|(bRegCur&(UINT8)~MCB_PWM_RC1);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_2, bReg);
						if(dWaitTime == (UINT32)0)
						{
							dWaitTime	= MCDRV_RC_WAIT_TIME;
						}
					}
					if(dWaitTime > (UINT32)0)
					{
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTime, 0);
						dWaitTimeDone	+= dWaitTime;
					}

					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_1, bRegAna1);
					if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1) & (MCB_PWM_ADL|MCB_PWM_ADR)) != (bRegAna1 & (MCB_PWM_ADL|MCB_PWM_ADR)))
					{
						if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START) & MCB_AD_START) != 0)
						{
							McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | sInitInfo.sWaitTime.dAdHpf, 0);
							dWaitTimeDone	+= sInitInfo.sWaitTime.dAdHpf;
						}
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_2, bRegAna2);

					/*	time wait	*/
					dWaitTime	= GetMaxWait(bRegChange);
					if(dWaitTime > dWaitTimeDone)
					{
						dWaitTime	= dWaitTime - dWaitTimeDone;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTime, 0);
						dWaitTimeDone	+= dWaitTime;
					}

					if((bWaitSPVolUp != 0) && (sInitInfo.sWaitTime.dSpRdy > dWaitTimeDone))
					{
						dWaitTime	= sInitInfo.sWaitTime.dSpRdy - dWaitTimeDone;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTime, 0);
						dWaitTimeDone	+= dWaitTime;
					}
					if((bWaitHPVolUp != 0) && (sInitInfo.sWaitTime.dHpRdy > dWaitTimeDone))
					{
						dWaitTime	= sInitInfo.sWaitTime.dHpRdy - dWaitTimeDone;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTime, 0);
						dWaitTimeDone	+= dWaitTime;
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddPowerUp", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	AddAnalogPowerUpAuto
 *
 *	Description:
 *			Add analog auto powerup packet.
 *	Arguments:
 *			psPowerInfo		power information
 *			psPowerUpdate	power update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	AddAnalogPowerUpAuto
(
	const MCDRV_POWER_INFO*		psPowerInfo,
	const MCDRV_POWER_UPDATE*	psPowerUpdate
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	UINT8	bRegCur;
	UINT32	dUpdate;
	UINT8	bRegChange;
	UINT8	bSpHizReg;
	MCDRV_INIT_INFO	sInitInfo;
	UINT32	dWaitTime	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddAnalogPowerUpAuto");
#endif


	McResCtrl_GetInitInfo(&sInitInfo);

	/*	Analog Power	*/
	dUpdate	= (UINT32)~psPowerInfo->abAnalog[0] & psPowerUpdate->abAnalog[0];
	if((dUpdate & (UINT32)MCB_PWM_VR) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0);
		if((bReg & MCB_PWM_VR) != 0)
		{/*	AP_VR changed	*/
			/*	AP_VR up	*/
			if((sInitInfo.bLdo == MCDRV_LDO_AON_DOFF) || (sInitInfo.bLdo == MCDRV_LDO_AON_DON))
			{
				/*	AP_LDOA	*/
				bReg	&= (UINT8)~MCB_PWM_LDOA;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_LDOA_WAIT_TIME, 0);
			}
			else
			{
				bReg	&= (UINT8)~MCB_PWM_REFA;
			}
			bReg	&= (UINT8)~MCB_PWM_VR;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | sInitInfo.sWaitTime.dVrefRdy1, 0);
			if(sInitInfo.sWaitTime.dVrefRdy2 > sInitInfo.sWaitTime.dVrefRdy1)
			{
				dWaitTime	= sInitInfo.sWaitTime.dVrefRdy2 - sInitInfo.sWaitTime.dVrefRdy1;
			}
		}

		bReg	= (UINT8)((UINT8)~((UINT8)~psPowerInfo->abAnalog[1] & psPowerUpdate->abAnalog[1]) & McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1));
		/*	SP_HIZ control	*/
		if(MCDRV_SPHIZ_HIZ == sInitInfo.bSpHiz)
		{
			bSpHizReg	= 0;
			if((bReg & (MCB_PWM_SPL1 | MCB_PWM_SPL2)) != 0)
			{
				bSpHizReg |= MCB_SPL_HIZ;
			}

			if((bReg & (MCB_PWM_SPR1 | MCB_PWM_SPR2)) != 0)
			{
				bSpHizReg |= MCB_SPR_HIZ;
			}

			bSpHizReg |= (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SP_MODE) & (MCB_SPMN | MCB_SP_SWAP));
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SP_MODE, bSpHizReg);
		}

		bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_3);
		bReg	= (UINT8)~((UINT8)~psPowerInfo->abAnalog[3] & psPowerUpdate->abAnalog[3]) & bRegCur;
		bRegChange	= bReg ^ bRegCur;
		/*	set DACON and NSMUTE before setting 0 to AP_DA	*/
		if(((bRegChange & (MCB_PWM_DAR|MCB_PWM_DAL)) != 0) && ((psPowerInfo->abAnalog[3] & psPowerUpdate->abAnalog[3] & (MCB_PWM_DAR|MCB_PWM_DAL)) != (MCB_PWM_DAR|MCB_PWM_DAL)))
		{
			if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_DAC_CONFIG) & MCB_DACON) == 0)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, MCB_NSMUTE);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, (MCB_DACON | MCB_NSMUTE));
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, MCB_DACON);
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_3, bReg);
		bRegChange	&= (MCB_PWM_MB1|MCB_PWM_MB2|MCB_PWM_MB3);

		bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_4);
		bReg	= (UINT8)~((UINT8)~psPowerInfo->abAnalog[4] & psPowerUpdate->abAnalog[4]) & bRegCur;
		bRegChange	|= (bReg ^ bRegCur);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_4, bReg);

		/*if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)*/
		{
			bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_5);
			bReg	= (UINT8)~((UINT8)~psPowerInfo->abAnalog[5] & psPowerUpdate->abAnalog[5]) & bRegCur;
			if(bReg != bRegCur)
			{
				bRegChange	|= MCDRV_REGCHANGE_PWM_LI2;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_5, bReg);
			}
		}

		if(dWaitTime > (UINT32)0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTime, 0);
		}

		sdRet	= McDevIf_ExecutePacket();
		if(sdRet == MCDRV_SUCCESS)
		{
			bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0);
			bReg	= (UINT8)~dUpdate & bRegCur;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);

			bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1);
			bReg	= (UINT8)~((UINT8)~psPowerInfo->abAnalog[1] & psPowerUpdate->abAnalog[1]) & bRegCur;
			if((bRegCur & (MCB_PWM_ADL|MCB_PWM_ADR)) != (bReg & (MCB_PWM_ADL|MCB_PWM_ADR)))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_1, bReg);
				if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START) & MCB_AD_START) != 0)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | sInitInfo.sWaitTime.dAdHpf, 0);
					dWaitTime	+= sInitInfo.sWaitTime.dAdHpf;
				}
			}
			else
			{
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					McResCtrl_SetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1, bReg);
				}
			}
		}

		if(sdRet == MCDRV_SUCCESS)
		{
			bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_2);
			bReg	= (UINT8)~((UINT8)~psPowerInfo->abAnalog[2] & psPowerUpdate->abAnalog[2]) & bRegCur;
			if((bRegCur & (MCB_PWM_ADM|MCB_PWM_LO1L|MCB_PWM_LO1R|MCB_PWM_LO2L|MCB_PWM_LO2R)) != (bReg & (MCB_PWM_ADM|MCB_PWM_LO1L|MCB_PWM_LO1R|MCB_PWM_LO2L|MCB_PWM_LO2R)))
			{
				bReg	= bReg|(bRegCur&(MCB_PWM_RC1|MCB_PWM_RC2));
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_2, bReg);
			}
			else
			{
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					McResCtrl_SetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_2, bReg);
				}
			}
		}

		if(sdRet == MCDRV_SUCCESS)
		{
			/*	time wait	*/
			if(dWaitTime < GetMaxWait(bRegChange))
			{
				dWaitTime	= GetMaxWait(bRegChange) - dWaitTime;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | dWaitTime, 0);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddAnalogPowerUpAuto", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_AddPowerDown
 *
 *	Description:
 *			Add powerdown packet.
 *	Arguments:
 *			psPowerInfo		power information
 *			psPowerUpdate	power update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddPowerDown
(
	const MCDRV_POWER_INFO*		psPowerInfo,
	const MCDRV_POWER_UPDATE*	psPowerUpdate
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	UINT8	bRegDPADIF;
	UINT8	bRegCur;
	UINT32	dUpdate	= psPowerInfo->dDigital & psPowerUpdate->dDigital;
	UINT32	dAPMDoneParam;
	UINT32	dAnaRdyParam;
	UINT32	dADCMuteFlg;
	UINT8	bSpHizReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddPowerDown");
#endif

	bRegDPADIF	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_CD_DP);


	McResCtrl_GetInitInfo(&sInitInfo);

	if(McResCtrl_GetAPMode() == eMCDRV_APM_ON)
	{
		if((((psPowerInfo->abAnalog[0] & psPowerUpdate->abAnalog[0] & MCB_PWM_VR) != 0)
			&& (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0) & MCB_PWM_VR) == 0))
		{
			/*	wait AP_XX_A	*/
			dAPMDoneParam	= ((MCB_AP_CP_A|MCB_AP_HPL_A|MCB_AP_HPR_A)<<8)
							| (MCB_AP_RC1_A|MCB_AP_RC2_A|MCB_AP_SPL1_A|MCB_AP_SPR1_A|MCB_AP_SPL2_A|MCB_AP_SPR2_A);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_APM_DONE | dAPMDoneParam, 0);
		}
	}

	if(((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) != 0UL)
	&& ((bRegDPADIF & (MCB_DP0_CLKI1|MCB_DP0_CLKI0)) != (MCB_DP0_CLKI1|MCB_DP0_CLKI0)))
	{
		/*	wait mute complete	*/
		sdRet	= McDevIf_ExecutePacket();
		if(sdRet == MCDRV_SUCCESS)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_ALLMUTE, 0);
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		/*	Analog Power	*/
		bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1);
		bReg	= (psPowerInfo->abAnalog[1] & psPowerUpdate->abAnalog[1]) | bRegCur;
		if(((psPowerUpdate->abAnalog[1] & MCDRV_POWUPDATE_ANALOG1_OUT) != 0) && (MCDRV_SPHIZ_HIZ == sInitInfo.bSpHiz))
		{
			/*	SP_HIZ control	*/
			bSpHizReg	= 0;
			if((bReg & (MCB_PWM_SPL1 | MCB_PWM_SPL2)) != 0)
			{
				bSpHizReg |= MCB_SPL_HIZ;
			}

			if((bReg & (MCB_PWM_SPR1 | MCB_PWM_SPR2)) != 0)
			{
				bSpHizReg |= MCB_SPR_HIZ;
			}

			bSpHizReg |= (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SP_MODE) & (MCB_SPMN | MCB_SP_SWAP));
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SP_MODE, bSpHizReg);
		}

		if(McResCtrl_GetAPMode() == eMCDRV_APM_ON)
		{
			if((bRegCur & (MCB_PWM_ADL|MCB_PWM_ADR)) != (bReg & (MCB_PWM_ADL|MCB_PWM_ADR)))
			{
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) == 0UL)
				{
					dADCMuteFlg	= 0;
					if(((bRegCur & MCB_PWM_ADL) != MCB_PWM_ADL)
					&& ((bReg & MCB_PWM_ADL) == MCB_PWM_ADL))
					{
						dADCMuteFlg	|= (MCB_ADC_VFLAGL<<8);
					}
					if(((bRegCur & MCB_PWM_ADR) != MCB_PWM_ADR)
					&& ((bReg & MCB_PWM_ADR) == MCB_PWM_ADR))
					{
						dADCMuteFlg	= MCB_ADC_VFLAGR;
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_ADCMUTE | dADCMuteFlg, 0);
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_1, bReg);
			}
			else
			{
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					McResCtrl_SetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_1, bReg);
				}
			}
		}
		else
		{
			if((bRegCur & (MCB_PWM_ADL|MCB_PWM_ADR)) != (bReg & (MCB_PWM_ADL|MCB_PWM_ADR)))
			{
				if((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) == 0UL)
				{
					dADCMuteFlg	= 0;
					if(((bRegCur & MCB_PWM_ADL) != MCB_PWM_ADL)
					&& ((bReg & MCB_PWM_ADL) == MCB_PWM_ADL))
					{
						dADCMuteFlg	|= (MCB_ADC_VFLAGL<<8);
					}
					if(((bRegCur & MCB_PWM_ADR) != MCB_PWM_ADR)
					&& ((bReg & MCB_PWM_ADR) == MCB_PWM_ADR))
					{
						dADCMuteFlg	= MCB_ADC_VFLAGR;
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_ADCMUTE | dADCMuteFlg, 0);
				}
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_1, bReg);
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_2);
		bReg	= (psPowerInfo->abAnalog[2] & psPowerUpdate->abAnalog[2]) | bRegCur;
		if((eDevID != eMCDRV_DEV_ID_46H) && (eDevID != eMCDRV_DEV_ID_44H))
		{
			bReg &= (UINT8)~MCB_PWM_ADM;
		}
		else
		{
			if((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) == 0UL)
			{
				if(((bRegCur & MCB_PWM_ADM) != MCB_PWM_ADM)
				&& ((bReg & MCB_PWM_ADM) == MCB_PWM_ADM))
				{
					dADCMuteFlg	= (MCB_ADC1_VFLAGL<<8)|MCB_ADC1_VFLAGR;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_ADC1MUTE | dADCMuteFlg, 0);
				}
			}
		}
		if(McResCtrl_GetAPMode() == eMCDRV_APM_ON)
		{
			if((bRegCur & (MCB_PWM_ADM|MCB_PWM_LO1L|MCB_PWM_LO1R|MCB_PWM_LO2L|MCB_PWM_LO2R)) != (bReg & (MCB_PWM_ADM|MCB_PWM_LO1L|MCB_PWM_LO1R|MCB_PWM_LO2L|MCB_PWM_LO2R)))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_2, bReg);
			}
			else
			{
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					McResCtrl_SetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_2, bReg);
				}
			}
		}
		else
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_2, bReg);
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		bReg	= (UINT8)((psPowerInfo->abAnalog[3] & psPowerUpdate->abAnalog[3]) | McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_3));
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_3, bReg);
		bReg	= (UINT8)((psPowerInfo->abAnalog[4] & psPowerUpdate->abAnalog[4]) | McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_4));
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_4, bReg);
		if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)
		{
			bReg	= (UINT8)((psPowerInfo->abAnalog[5] & psPowerUpdate->abAnalog[5]) | McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_5));
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_5, bReg);
		}

		/*	set DACON and NSMUTE after setting 1 to AP_DA	*/
		if((psPowerInfo->abAnalog[3] & psPowerUpdate->abAnalog[3] & (MCB_PWM_DAR|MCB_PWM_DAL)) == (MCB_PWM_DAR|MCB_PWM_DAL))
		{
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_DAC_CONFIG);
			if((bReg & MCB_DACON) == MCB_DACON)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_DACMUTE | (UINT32)((MCB_DAC_FLAGL<<8)|MCB_DAC_FLAGR), 0);
			}
			bReg	|= MCB_NSMUTE;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, bReg);
			bReg	&= (UINT8)~MCB_DACON;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_CONFIG, bReg);
		}

		bRegCur	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0);
		bReg	= psPowerInfo->abAnalog[0] & psPowerUpdate->abAnalog[0];
		if(McResCtrl_GetAPMode() == eMCDRV_APM_OFF)
		{
			/*	wait CPPDRDY	*/
			dAnaRdyParam	= 0;
			if(((bRegCur & MCB_PWM_CP) == 0) && ((bReg & MCB_PWM_CP) == MCB_PWM_CP))
			{
				dAnaRdyParam	= MCB_CPPDRDY;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_ANA_RDY | dAnaRdyParam, 0);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, (bRegCur|MCB_PWM_CP));
			}
		}

		if(((bReg & MCB_PWM_VR) != 0) && ((bRegCur & MCB_PWM_VR) == 0))
		{/*	AP_VR changed	*/
			/*	AP_xx down	*/
			bReg	= bRegCur|MCI_PWM_ANALOG_0_DEF;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
		}
		else
		{
			bReg	|= bRegCur;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
		}
		sdRet	= McDevIf_ExecutePacket();
	}


	/*	Digital Power	*/
	if(sdRet == MCDRV_SUCCESS)
	{
		if(((dUpdate & MCDRV_POWINFO_DIGITAL_DPADIF) != 0UL)
		&& (bRegDPADIF != MCB_DPADIF))
		{
			/*	DPADIF	*/
			bRegDPADIF	|= MCB_DPADIF;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);
		}

		if((dUpdate & MCDRV_POWINFO_DIGITAL_DPBDSP) != 0UL)
		{
			/*	DPBDSP	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_BDSP, MCB_PWM_DPBDSP);
		}

		if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
		{
			if((dUpdate & MCDRV_POWINFO_DIGITAL_DPC) != 0UL)
			{
				/*	DPC	*/
				bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PWM_DPC);
				if((bReg&MCB_PWM_DPC) == 0)
				{
#ifdef MCDRV_USE_CDSP_DRIVER
					if(sdRet == MCDRV_SUCCESS)
					{
						McCdsp_Term();
					}
#endif
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DPC, MCI_PWM_DPC_DEF);
				}
				if(sInitInfo.bPowerMode == MCDRV_POWMODE_FULL)
				{
					/*	RSTC	*/
					bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_RSTB);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_RSTB, bReg|MCB_RSTC);
				}
			}
		}

		/*	DPDI*, DPPDM	*/
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PWM_DIGITAL_1);
		if(((dUpdate & MCDRV_POWINFO_DIGITAL_DPDI0) != 0UL) || ((dUpdate & MCDRV_POWINFO_DIGITAL_DP2) != 0UL))
		{
			bReg |= MCB_PWM_DPDI0;
		}
		if(((dUpdate & MCDRV_POWINFO_DIGITAL_DPDI1) != 0UL) || ((dUpdate & MCDRV_POWINFO_DIGITAL_DP2) != 0UL))
		{
			bReg |= MCB_PWM_DPDI1;
		}
		if(((dUpdate & MCDRV_POWINFO_DIGITAL_DPDI2) != 0UL) || ((dUpdate & MCDRV_POWINFO_DIGITAL_DP2) != 0UL))
		{
			bReg |= MCB_PWM_DPDI2;
		}
		if((dUpdate & MCDRV_POWINFO_DIGITAL_DPPDM) != 0UL)
		{
			bReg |= MCB_PWM_DPPDM;
		}
		if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
		{
			if((dUpdate & MCDRV_POWINFO_DIGITAL_DPCD) != 0UL)
			{
				if((bReg & MCB_PWM_DPCD) == 0)
				{
					bReg |= MCB_PWM_DPCD;

					/*	CD_DP2	*/
					bRegDPADIF	|= MCB_CD_DP2;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);
				}
			}
		}
		if(bReg != 0)
		{
			/*	cannot set DP* & DPB at the same time	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_1, bReg);
		}
		/*	DPB	*/
		if((dUpdate & MCDRV_POWINFO_DIGITAL_DPB) != 0UL)
		{
			if((bReg & MCB_PWM_DPB) == 0)
			{
				bReg |= MCB_PWM_DPB;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL_1, bReg);
			}
			if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
			{
				if(sInitInfo.bPowerMode == MCDRV_POWMODE_FULL)
				{
					/*	RSTB	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_RSTB, MCB_RSTB|MCB_RSTC);
					/*	PSWB	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PSW, MCB_PSWB);
				}
			}
		}

		if((dUpdate & MCDRV_POWINFO_DIGITAL_DP2) != 0UL)
		{
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PWM_DIGITAL) | MCB_PWM_DP2;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL, bReg);
		}

		if(((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) != 0UL)
		&& ((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_CD_DP) & (MCB_DP0_CLKI1 | MCB_DP0_CLKI0)) != (MCB_DP0_CLKI1 | MCB_DP0_CLKI0)))
		{
			/*	DP2, DP1	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL, MCB_PWM_DP2);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PWM_DIGITAL, (MCB_PWM_DP2 | MCB_PWM_DP1));
			if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
			{
				/*	DP0, PLL1	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PLL1RST, MCI_PLL1_DEF);

				/*	DPMCLKO	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DPMCLKO, MCB_DPMCLKO);

				/*	CD_DP*	*/
				bRegDPADIF	|= MCB_CD_DP1;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);
			}
			if((dUpdate & MCDRV_POWINFO_DIGITAL_PLLRST0) != 0UL)
			{
				/*	PLLRST0	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_PLL_RST, MCB_PLLRST0);
			}
			/*	DP0	*/
			bRegDPADIF	|= (MCB_DP0_CLKI1 | MCB_DP0_CLKI0);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bRegDPADIF);

			sdRet	= McDevIf_ExecutePacket();
			if(sdRet == MCDRV_SUCCESS)
			{
				McSrv_ClockStop();
			}
		}
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if((dUpdate & MCDRV_POWINFO_DIGITAL_DP0) != 0UL)
		{
			if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
			{
				if(MCDRV_POWMODE_FULL == sInitInfo.bPowerMode)
				{
					if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_RST) == 0)
					{
						/*	GP*_MSK	*/
						bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA2_MSK);
						bReg	|= MCB_PA2_MSK;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_MSK, bReg);
						bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA_MSK);
						bReg	|= (MCB_PA1_MSK|MCB_PA0_MSK);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_MSK, bReg);

						/*	CSDIN_MSK	*/
						bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_CSDIN_MSK);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_CSDIN_MSK, bReg|MCB_CSDIN_MSK);

						/*	SDI_MSK, BCLKLRCK_MSK	*/
						bReg	= (MCB_SDI_MSK|MCB_BCLKLRCK_MSK);
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_MSK, bReg);

						/*	RSTA	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_RST, MCB_RST);
						sdRet	= McDevIf_ExecutePacket();
						if(sdRet == MCDRV_SUCCESS)
						{
							McResCtrl_InitBaseBlockReg();

							/*	LDOD	*/
							if((MCDRV_LDO_AOFF_DON == sInitInfo.bLdo) || (MCDRV_LDO_AON_DON == sInitInfo.bLdo))
							{
								bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_PWM_ANALOG_0) | MCB_PWM_LDOD);
								McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_PWM_ANALOG_0, bReg);
							}
						}
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddPowerDown", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_AddPathSet
 *
 *	Description:
 *			Add path update packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddPathSet
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddPathSet");
#endif

	if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_RST) == 0)
	{
		/*	DI Pad	*/
		AddDIPad();

		/*	PAD(PDM)	*/
		AddPAD();

		/*	Digital Mixer Source	*/
		sdRet	= AddSource();
		if(sdRet == MCDRV_SUCCESS)
		{
			/*	DIR*_START, DIT*_START	*/
			AddDIStart();
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddPathSet", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	AddDIPad
 *
 *	Description:
 *			Add DI Pad setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddDIPad
(
	void
)
{
	UINT8	bReg;
	UINT8	bDIO2Use	= 0;
	UINT8	bIsUsedDIR[3];
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_PATH_INFO	sPathInfo;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddDIPad");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetPathInfo(&sPathInfo);
	McResCtrl_GetDioInfo(&sDioInfo);

	/*	SDIN_MSK2/1	*/
	bReg	= 0;
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2) == 0)
	{
		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2_DIRECT) == 0)
		{
			bReg |= MCB_SDIN_MSK2;
			bIsUsedDIR[2]	= 0;
		}
		else
		{
			bIsUsedDIR[2]	= 0;
		}
	}
	else
	{
		bIsUsedDIR[2]	= 1;
		bDIO2Use		= 1;
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR1) == 0)
	{
		bReg |= MCB_SDIN_MSK1;
		bIsUsedDIR[1]	= 0;
	}
	else
	{
		bIsUsedDIR[1]	= 1;
	}
	/*	SDO_DDR2/1	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		if(McResCtrl_IsDstUsed(eMCDRV_DST_DIT2, eMCDRV_DST_CH0) == 0)
		{
			if(sInitInfo.bDioSdo2Hiz == MCDRV_DAHIZ_LOW)
			{
				bReg |= MCB_SDO_DDR2;
			}
		}
		else
		{
			bReg |= MCB_SDO_DDR2;
			bDIO2Use	= 1;
		}
	}
	if(McResCtrl_IsDstUsed(eMCDRV_DST_DIT1, eMCDRV_DST_CH0) == 0)
	{
		if(sInitInfo.bDioSdo1Hiz == MCDRV_DAHIZ_LOW)
		{
			bReg |= MCB_SDO_DDR1;
		}
	}
	else
	{
		bReg |= MCB_SDO_DDR1;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_SD_MSK, bReg);

	/*	SDIN_MSK0	*/
	bReg	= 0;
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR0) == 0)
	{
		bReg |= MCB_SDIN_MSK0;
		bIsUsedDIR[0]	= 0;
	}
	else
	{
		bIsUsedDIR[0]	= 1;
	}
	/*	SDO_DDR0	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_DIT0, eMCDRV_DST_CH0) == 0)
	{
		if(sInitInfo.bDioSdo0Hiz == MCDRV_DAHIZ_LOW)
		{
			bReg |= MCB_SDO_DDR0;
		}
	}
	else
	{
		bReg |= MCB_SDO_DDR0;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_SD_MSK_1, bReg);

	/*	BCLK_MSK*, BCLD_DDR*, LRCK_MSK*, LRCK_DDR*	*/
	bReg	= 0;
	if((bIsUsedDIR[2] == 0) && (McResCtrl_GetDITSource(eMCDRV_DIO_2) == eMCDRV_SRC_NONE))
	{
		bReg |= MCB_BCLK_MSK2;
		bReg |= MCB_LRCK_MSK2;
		if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
		{
			if(sInitInfo.bDioClk2Hiz == MCDRV_DAHIZ_LOW)
			{
				bReg |= MCB_BCLK_DDR2;
				bReg |= MCB_LRCK_DDR2;
			}
		}
	}
	else
	{
		if(sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_BCLK_DDR2;
			bReg |= MCB_LRCK_DDR2;
		}
	}
	if((bIsUsedDIR[1] == 0) && (McResCtrl_GetDITSource(eMCDRV_DIO_1) == eMCDRV_SRC_NONE))
	{
		bReg |= MCB_BCLK_MSK1;
		bReg |= MCB_LRCK_MSK1;
		if(sInitInfo.bDioClk1Hiz == MCDRV_DAHIZ_LOW)
		{
			bReg |= MCB_BCLK_DDR1;
			bReg |= MCB_LRCK_DDR1;
		}
	}
	else
	{
		if(sDioInfo.asPortInfo[1].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_BCLK_DDR1;
			bReg |= MCB_LRCK_DDR1;
		}
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_BCLK_MSK, bReg);

	/*	ROUTER_MS	*/
	bReg	= 0;
	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
	{
		if((McResCtrl_IsValidCDSPSource() != 0)
		|| ((McResCtrl_IsDstUsed(eMCDRV_DST_AE, eMCDRV_DST_CH0) != 0) && ((McResCtrl_GetAeEx() != NULL))))
		{
			if(bDIO2Use == 0)
			{
				bReg	|= MCB_ROUTER_MS;
			}
		}
	}

	/*	BCLK_MSK*, BCLD_DDR*, LRCK_MSK*, LRCK_DDR*, PCM_HIZ*	*/
	if((bIsUsedDIR[0] == 0) && (McResCtrl_GetDITSource(eMCDRV_DIO_0) == eMCDRV_SRC_NONE))
	{
		bReg |= MCB_BCLK_MSK0;
		bReg |= MCB_LRCK_MSK0;
		if(sInitInfo.bDioClk0Hiz == MCDRV_DAHIZ_LOW)
		{
			bReg |= MCB_BCLK_DDR0;
			bReg |= MCB_LRCK_DDR0;
		}
	}
	else
	{
		if(sDioInfo.asPortInfo[0].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_BCLK_DDR0;
			bReg |= MCB_LRCK_DDR0;
		}
	}
	if(sInitInfo.bPcmHiz == MCDRV_PCMHIZ_HIZ)
	{
		bReg |= (MCB_PCMOUT_HIZ2 | MCB_PCMOUT_HIZ1 | MCB_PCMOUT_HIZ0);
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_BCLK_MSK_1, bReg);

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
	{
		if((bReg & MCB_ROUTER_MS) != 0)
		{
			bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DI_FS2) & (UINT8)~MCB_DIFS2);
			bReg	|= McResCtrl_GetCdspFs();
		}
		else
		{
			bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DI_FS2) & (UINT8)~MCB_DIFS2);
			bReg	|= sDioInfo.asPortInfo[2].sDioCommon.bFs;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DI_FS2, bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddDIPad", 0);
#endif
}

/****************************************************************************
 *	AddPAD
 *
 *	Description:
 *			Add PAD setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddPAD
(
	void
)
{
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddPAD");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);

	/*	PA*_MSK	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA2_MSK);
		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) == 0)
		{
			if(sInitInfo.bPad2Func == MCDRV_PAD_PDMDI)
			{
				bReg	|= MCB_PA2_MSK;
			}
		}
		else
		{
			if(sInitInfo.bPad2Func == MCDRV_PAD_PDMDI)
			{
				bReg	&= (UINT8)~MCB_PA2_MSK;
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_MSK, bReg);
	}

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA_MSK);
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) == 0)
	{
		if(sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
		{
			bReg	|= MCB_PA0_MSK;
		}
		if(sInitInfo.bPad1Func == MCDRV_PAD_PDMDI)
		{
			bReg	|= MCB_PA1_MSK;
		}
	}
	else
	{
		if(sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
		{
			bReg	&= (UINT8)~MCB_PA0_MSK;
		}
		if(sInitInfo.bPad1Func == MCDRV_PAD_PDMDI)
		{
			bReg	&= (UINT8)~MCB_PA1_MSK;
		}
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_MSK, bReg);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddPAD", 0);
#endif
}

/****************************************************************************
 *	AddSource
 *
 *	Description:
 *			Add source setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static	MCDRV_SRC_TYPE	eSrcType;  
static	MCDRV_SRC_TYPE	eAESource; 
static	MCDRV_PATH_INFO	sPathInfo; 
static	MCDRV_DAC_INFO	sDacInfo;  
static	MCDRV_AE_INFO	sAeInfo;   

static SINT32	AddSource
(
	void
)
{
	SINT32	sdRet			= MCDRV_SUCCESS;
	UINT8	bReg;
	UINT8	bAEng6;
	UINT8	bRegAESource	= 0;
	UINT8	bRegDACSource	= 0;
	UINT8	bRegDAC_INS		= 0;
	UINT8	bRegINS			= 0;
	UINT8	bEFIFOSource	= 0;
	UINT32	dXFadeParam		= 0;
	UINT8	bRegDI2Start	= 0;
	UINT8	bEFIFO0L, bEFIFO0R, bEFIFO1L, bEFIFO1R;
/*	MCDRV_SRC_TYPE	eSrcType;   */
/*	MCDRV_SRC_TYPE	eAESource;  */
/*	MCDRV_PATH_INFO	sPathInfo;  */
/*	MCDRV_DAC_INFO	sDacInfo;   */
/*	MCDRV_AE_INFO	sAeInfo;    */

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddSource");
#endif

	bAEng6		= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_AENG6_SOURCE);
	eAESource	= McResCtrl_GetAESource();

	McResCtrl_GetPathInfo(&sPathInfo);
	McResCtrl_GetAeInfo(&sAeInfo);

	switch(eAESource)
	{
	case	eMCDRV_SRC_PDM:		bRegAESource	= MCB_AE_SOURCE_AD;		bAEng6	= MCB_AENG6_PDM;	bRegINS		= MCB_ADC_INS;	break;
	case	eMCDRV_SRC_ADC0:	bRegAESource	= MCB_AE_SOURCE_AD;		bAEng6	= MCB_AENG6_ADC0;	bRegINS		= MCB_ADC_INS;	break;
	case	eMCDRV_SRC_ADC1:	bRegAESource	= MCB_AE_SOURCE_ADC1;								bRegINS		= MCB_ADC1_INS;	break;
	case	eMCDRV_SRC_DIR0:	bRegAESource	= MCB_AE_SOURCE_DIR0;								bRegINS		= MCB_DIR0_INS;	break;
	case	eMCDRV_SRC_DIR1:	bRegAESource	= MCB_AE_SOURCE_DIR1;								bRegINS		= MCB_DIR1_INS;	break;
	case	eMCDRV_SRC_DIR2:	bRegAESource	= MCB_AE_SOURCE_DIR2;								bRegINS		= MCB_DIR2_INS;	break;
	case	eMCDRV_SRC_CDSP:	bRegAESource	= MCB_AE_SOURCE_DIR2;								bRegINS		= MCB_DIR2_INS;	break;
	case	eMCDRV_SRC_MIX:		bRegAESource	= MCB_AE_SOURCE_MIX;								bRegDAC_INS	= MCB_DAC_INS;	break;
	case	eMCDRV_SRC_DTMF:	bRegAESource	= MCB_AE_SOURCE_AD;		bAEng6	= MCB_AENG6_DTMF;	bRegINS		= MCB_ADC_INS;	break;
	default:					bRegAESource	= MCB_AE_SOURCE_MUTE;
	}

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DAC_INS);
	if(bReg != bRegDAC_INS)
	{
		dXFadeParam	= bReg;
		dXFadeParam <<= 8;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DAC_INS, 0);
	}

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_INS);
	if(bReg != bRegINS)
	{
		dXFadeParam	|= bReg;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_INS, 0);
	}
	sdRet	= McDevIf_ExecutePacket();
	if(sdRet == MCDRV_SUCCESS)
	{
		AddPMSource();

		McResCtrl_GetDacInfo(&sDacInfo);

		/*	DAC_SOURCE/VOICE_SOURCE	*/
		bReg	= 0;
		switch(McResCtrl_GetDACSource(eMCDRV_DAC_MASTER))
		{
		case	eMCDRV_SRC_PDM:
			bReg |= MCB_DAC_SOURCE_AD;
			bAEng6	= MCB_AENG6_PDM;
			break;
		case	eMCDRV_SRC_ADC0:
			bReg |= MCB_DAC_SOURCE_AD;
			bAEng6	= MCB_AENG6_ADC0;
			break;
		case	eMCDRV_SRC_ADC1:
			bReg |= MCB_DAC_SOURCE_ADC1;
			break;
		case	eMCDRV_SRC_DIR0:
			bReg |= MCB_DAC_SOURCE_DIR0;
			break;
		case	eMCDRV_SRC_DIR1:
			bReg |= MCB_DAC_SOURCE_DIR1;
			break;
		case	eMCDRV_SRC_DIR2:
		case	eMCDRV_SRC_CDSP:
			bReg |= MCB_DAC_SOURCE_DIR2;
			break;
		case	eMCDRV_SRC_MIX:
			bReg |= MCB_DAC_SOURCE_MIX;
			break;
		case	eMCDRV_SRC_DTMF:
			bReg |= MCB_DAC_SOURCE_AD;
			bAEng6	= MCB_AENG6_DTMF;
			break;
		default:
			break;
		}
		switch(McResCtrl_GetDACSource(eMCDRV_DAC_VOICE))
		{
		case	eMCDRV_SRC_PDM:
			bReg |= MCB_VOICE_SOURCE_AD;
			bAEng6	= MCB_AENG6_PDM;
			break;
		case	eMCDRV_SRC_ADC0:
			bReg |= MCB_VOICE_SOURCE_AD;
			bAEng6	= MCB_AENG6_ADC0;
			break;
		case	eMCDRV_SRC_ADC1:
			bReg |= MCB_VOICE_SOURCE_ADC1;
			break;
		case	eMCDRV_SRC_DIR0:
			bReg |= MCB_VOICE_SOURCE_DIR0;
			break;
		case	eMCDRV_SRC_DIR1:
			bReg |= MCB_VOICE_SOURCE_DIR1;
			break;
		case	eMCDRV_SRC_DIR2:
		case	eMCDRV_SRC_CDSP:
			bReg |= MCB_VOICE_SOURCE_DIR2;
			break;
		case	eMCDRV_SRC_MIX:
			bReg |= MCB_VOICE_SOURCE_MIX;
			break;
		case	eMCDRV_SRC_DTMF:
			bReg |= MCB_VOICE_SOURCE_AD;
			bAEng6	= MCB_AENG6_DTMF;
			break;
		default:
			break;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SOURCE, bReg);
		bRegDACSource	= bReg;
		if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
		{
			if((bRegDACSource != 0)
			|| ((sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON))
			{
				bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_CD_START);
				bReg	|= MCB_CDO_START;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_CD_START, bReg);
			}
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_BI_START);
			if((bRegDACSource != 0)
			|| ((sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON))
			{
				bReg	|= MCB_BIR_START;
			}
			else
			{
				bReg	&= (UINT8)~MCB_BIR_START;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_BI_START, bReg);
			if((bRegDACSource == 0)
			&& ((sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) != MCDRV_SRC4_DTMF_ON))
			{
				bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_CD_START);
				bReg	&= (UINT8)~MCB_CDO_START;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_CD_START, bReg);
			}
		}

		/*	SWP/VOICE_SWP	*/
		bReg	= (sDacInfo.bMasterSwap << 4) | sDacInfo.bVoiceSwap;
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SWP, bReg);

		/*	DIT0SRC_SOURCE/DIT1SRC_SOURCE	*/
		bReg	= 0;
		switch(McResCtrl_GetDITSource(eMCDRV_DIO_0))
		{
		case	eMCDRV_SRC_PDM:
			bReg |= MCB_DIT0_SOURCE_AD;
			bAEng6	= MCB_AENG6_PDM;
			break;
		case	eMCDRV_SRC_ADC0:
			bReg |= MCB_DIT0_SOURCE_AD;
			bAEng6	= MCB_AENG6_ADC0;
			break;
		case	eMCDRV_SRC_ADC1:
			bReg |= MCB_DIT0_SOURCE_ADC1;
			break;
		case	eMCDRV_SRC_DIR0:
			bReg |= MCB_DIT0_SOURCE_DIR0;
			break;
		case	eMCDRV_SRC_DIR1:
			bReg |= MCB_DIT0_SOURCE_DIR1;
			break;
		case	eMCDRV_SRC_DIR2:
		case	eMCDRV_SRC_CDSP:
			bReg |= MCB_DIT0_SOURCE_DIR2;
			break;
		case	eMCDRV_SRC_MIX:
			bReg |= MCB_DIT0_SOURCE_MIX;
			break;
		case	eMCDRV_SRC_DTMF:
			bReg |= MCB_DIT0_SOURCE_AD;
			bAEng6	= MCB_AENG6_DTMF;
			break;
		default:
			break;
		}
		switch(McResCtrl_GetDITSource(eMCDRV_DIO_1))
		{
		case	eMCDRV_SRC_PDM:
			bReg |= MCB_DIT1_SOURCE_AD;
			bAEng6	= MCB_AENG6_PDM;
			break;
		case	eMCDRV_SRC_ADC0:
			bReg |= MCB_DIT1_SOURCE_AD;
			bAEng6	= MCB_AENG6_ADC0;
			break;
		case	eMCDRV_SRC_ADC1:
			bReg |= MCB_DIT1_SOURCE_ADC1;
			break;
		case	eMCDRV_SRC_DIR0:
			bReg |= MCB_DIT1_SOURCE_DIR0;
			break;
		case	eMCDRV_SRC_DIR1:
			bReg |= MCB_DIT1_SOURCE_DIR1;
			break;
		case	eMCDRV_SRC_DIR2:
		case	eMCDRV_SRC_CDSP:
			bReg |= MCB_DIT1_SOURCE_DIR2;
			break;
		case	eMCDRV_SRC_MIX:
			bReg |= MCB_DIT1_SOURCE_MIX;
			break;
		case	eMCDRV_SRC_DTMF:
			bReg |= MCB_DIT1_SOURCE_AD;
			bAEng6	= MCB_AENG6_DTMF;
			break;
		default:
			break;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SRC_SOURCE, bReg);

		/*	AE_SOURCE/DIT2SRC_SOURCE	*/
		if(dXFadeParam != 0UL)
		{
			/*	wait xfade complete	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_INSFLG | dXFadeParam, 0);
		}
		if(McResCtrl_GetAESource() == eMCDRV_SRC_NONE)
		{/*	AE is unused	*/
			/*	BDSP stop & reset	*/
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_BDSP_ST);
			if((bReg&MCB_BDSP_ST) != 0)
			{
				bReg	&= (UINT8)~MCB_BDSP_ST;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ST, bReg);
			}
		}

		if(McResCtrl_IsDstUsed(eMCDRV_DST_MIX2, eMCDRV_DST_CH0) != 0)
		{
			bReg	= MCB_AE_SOURCE_MUTE;
		}
		else
		{
			bReg	= bRegAESource;
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
			bReg |= MCB_DIT2_SOURCE_AD;
			bAEng6	= MCB_AENG6_PDM;
			break;
		case	eMCDRV_SRC_ADC0:
			bReg |= MCB_DIT2_SOURCE_AD;
			bAEng6	= MCB_AENG6_ADC0;
			break;
		case	eMCDRV_SRC_ADC1:
			bReg |= MCB_DIT2_SOURCE_ADC1;
			break;
		case	eMCDRV_SRC_DIR0:
			bReg |= MCB_DIT2_SOURCE_DIR0;
			break;
		case	eMCDRV_SRC_DIR1:
			bReg |= MCB_DIT2_SOURCE_DIR1;
			break;
		case	eMCDRV_SRC_DIR2:
		case	eMCDRV_SRC_CDSP:
			bReg |= MCB_DIT2_SOURCE_DIR2;
			break;
		case	eMCDRV_SRC_MIX:
			bReg |= MCB_DIT2_SOURCE_MIX;
			break;
		case	eMCDRV_SRC_DTMF:
			bReg |= MCB_DIT2_SOURCE_AD;
			bAEng6	= MCB_AENG6_DTMF;
			break;
		default:
			break;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SRC_SOURCE_1, bReg);

		if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
		{
			/*	DIT2SRC_SOURCE1	*/
			if(McDevProf_IsValid(eMCDRV_FUNC_DITSWAP) == 0)
			{
				bReg		= 0;
			}
			else
			{
				bReg		= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2SRC_SOURCE) & (MCB_DIT2SWAPL|MCB_DIT2SWAPR);
			}
			eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
			switch(eSrcType)
			{
			case	eMCDRV_SRC_PDM:
				bReg |= MCB_DIT2SRC_SOURCE1L_AD;
				break;
			case	eMCDRV_SRC_ADC0:
				bReg |= MCB_DIT2SRC_SOURCE1L_AD;
				break;
			case	eMCDRV_SRC_ADC1:
				bReg |= MCB_DIT2SRC_SOURCE1L_ADC1;
				break;
			case	eMCDRV_SRC_DIR0:
				bReg |= MCB_DIT2SRC_SOURCE1L_DIR0;
				break;
			case	eMCDRV_SRC_DIR1:
				bReg |= MCB_DIT2SRC_SOURCE1L_DIR1;
				break;
			case	eMCDRV_SRC_DIR2:
			case	eMCDRV_SRC_CDSP:
				bReg |= MCB_DIT2SRC_SOURCE1L_DIR2;
				break;
			case	eMCDRV_SRC_MIX:
				bReg |= MCB_DIT2SRC_SOURCE1L_MIX;
				break;
			case	eMCDRV_SRC_DTMF:
				bReg |= MCB_DIT2SRC_SOURCE1L_AD;
				break;
			default:
				break;
			}
			eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
			switch(eSrcType)
			{
			case	eMCDRV_SRC_PDM:
				bReg |= MCB_DIT2SRC_SOURCE1R_AD;
				break;
			case	eMCDRV_SRC_ADC0:
				bReg |= MCB_DIT2SRC_SOURCE1R_AD;
				break;
			case	eMCDRV_SRC_ADC1:
				bReg |= MCB_DIT2SRC_SOURCE1R_ADC1;
				break;
			case	eMCDRV_SRC_DIR0:
				bReg |= MCB_DIT2SRC_SOURCE1R_DIR0;
				break;
			case	eMCDRV_SRC_DIR1:
				bReg |= MCB_DIT2SRC_SOURCE1R_DIR1;
				break;
			case	eMCDRV_SRC_DIR2:
			case	eMCDRV_SRC_CDSP:
				bReg |= MCB_DIT2SRC_SOURCE1R_DIR2;
				break;
			case	eMCDRV_SRC_MIX:
				bReg |= MCB_DIT2SRC_SOURCE1R_MIX;
				break;
			case	eMCDRV_SRC_DTMF:
				bReg |= MCB_DIT2SRC_SOURCE1R_AD;
				break;
			default:
				break;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2SRC_SOURCE, bReg);
		}

		/*	check MIX SOURCE for AENG6_SOURCE	*/
		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) != 0)
		{
			bAEng6	= MCB_AENG6_PDM;
		}
		else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) != 0)
		{
			bAEng6	= MCB_AENG6_ADC0;
		}
		else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DTMF) != 0)
		{
			bAEng6	= MCB_AENG6_DTMF;
		}
		else
		{
		}

		/*	AENG6_SOURCE	*/
		if(bAEng6 != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_AENG6_SOURCE))
		{
			AddStopADC();
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START);
			if((bReg & MCB_PDM_START) != 0)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_START, bReg&(UINT8)~MCB_PDM_START);
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_AENG6_SOURCE, bAEng6);

		if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
		{
			bRegDI2Start	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START);

			/*	EFIFO_SOURCE	*/
			if(McResCtrl_GetAeEx() != NULL)
			{
				bEFIFOSource	= MCB_EFIFO0L_AEOUT | MCB_EFIFO0R_AEOUT;
			}
			else
			{
				bEFIFOSource	= McResCtrl_GetEFIFOSourceReg();
			}
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_EFIFO_SOURCE);
			McResCtrl_GetEFIFOSourceRegCur(&bEFIFO0L, &bEFIFO0R, &bEFIFO1L, &bEFIFO1R);
			if(bEFIFOSource != bReg)
			{
				sdRet	= McPacket_StopDI2();
				if(sdRet == MCDRV_SUCCESS)
				{
					bRegDI2Start	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START);

					if(((bEFIFOSource&MCB_EFIFO1L_ESRC1) != (bReg&MCB_EFIFO1L_ESRC1))
					|| ((bEFIFOSource&MCB_EFIFO1R_ESRC1) != (bReg&MCB_EFIFO1R_ESRC1))
					|| ((bEFIFOSource&MCB_EFIFO0L_ESRC0) != (bReg&MCB_EFIFO0L_ESRC0))
					|| ((bEFIFOSource&MCB_EFIFO0R_ESRC0) != (bReg&MCB_EFIFO0R_ESRC0)))
					{
						bRegDI2Start	&= (UINT8)~MCB_DIT2_SRC_START;

						if(((bEFIFOSource&0xC0) == 0x00)	/*	DIR2_DIRECT->C-DSP	*/
						|| ((bEFIFOSource&0x30) == 0x00)
						|| ((bEFIFOSource&0x0C) == 0x00)
						|| ((bEFIFOSource&0x03) == 0x00)
						|| (bEFIFO1L == 0x00)				/*	SRC->C-DSP	*/
						|| (bEFIFO1R == 0x00)
						|| (bEFIFO0L == 0x00)
						|| (bEFIFO0R == 0x00))
						{
							bRegDI2Start	&= (UINT8)~MCB_DIR2_START;
						}
					}

					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bRegDI2Start);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_EFIFO_SOURCE, bEFIFOSource);
				}
			}
			if(sdRet == MCDRV_SUCCESS)
			{
				if(bEFIFO0L != McResCtrl_GetEFIFO0LSource())
				{
					if((bEFIFOSource&0x0C) == 0x00)
					{
						bRegDI2Start	&= (UINT8)~MCB_DIR2_START;
					}
					else
					{
						if((bEFIFOSource&0x0C) == 0x04)
						{
							bRegDI2Start	&= (UINT8)~MCB_DIT2_SRC_START;
						}
					}
				}
				if(bEFIFO0R != McResCtrl_GetEFIFO0RSource())
				{
					if((bEFIFOSource&0x03) == 0x00)
					{
						bRegDI2Start	&= (UINT8)~MCB_DIR2_START;
					}
					else
					{
						if((bEFIFOSource&0x03) == 0x01)
						{
							bRegDI2Start	&= (UINT8)~MCB_DIT2_SRC_START;
						}
					}
				}
				if(bEFIFO1L != McResCtrl_GetEFIFO1LSource())
				{
					if((bEFIFOSource&0xC0) == 0x00)
					{
						bRegDI2Start	&= (UINT8)~MCB_DIR2_START;
					}
					else
					{
						if((bEFIFOSource&0xC0) == 0x40)
						{
							bRegDI2Start	&= (UINT8)~MCB_DIT2_SRC_START;
						}
					}
				}
				if(bEFIFO1R != McResCtrl_GetEFIFO1RSource())
				{
					if((bEFIFOSource&0x30) == 0x00)
					{
						bRegDI2Start	&= (UINT8)~MCB_DIR2_START;
					}
					else
					{
						if((bEFIFOSource&0x30) == 0x10)
						{
							bRegDI2Start	&= (UINT8)~MCB_DIT2_SRC_START;
						}
					}
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bRegDI2Start);

				/*	DIR2SRC_SOURCE	*/
				bReg	= 0;
				if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
				{
					bReg	= MCB_DIR2SRC_SOURCE_OFIFO;
				}
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIR2SRC_SOURCE))
				{
					bRegDI2Start	&= (UINT8)~MCB_DIR2_SRC_START;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bRegDI2Start);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIR2SRC_SOURCE, bReg);
				}

				/*	SDO2_SOURCE	*/
				bReg	= 0;
				eSrcType	= McResCtrl_GetDITSource(eMCDRV_DIO_2);
				if(eSrcType == eMCDRV_SRC_CDSP_DIRECT)
				{
					bReg	= MCB_SDOUT2_SOURCE_OFIFO;
				}
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SDOUT2_SOURCE))
				{
					bRegDI2Start	&= (UINT8)~MCB_DIT2_START;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bRegDI2Start);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SDOUT2_SOURCE, bReg);
				}

				/*	DIT2SRC_EN	*/
				bReg	= 0;
				if((McResCtrl_GetCDSPSource(eMCDRV_DST_CH2) != eMCDRV_SRC_NONE) || (McResCtrl_GetCDSPSource(eMCDRV_DST_CH3) != eMCDRV_SRC_NONE))
				{
					bReg	= 1;
				}
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2SRC_EN))
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2SRC_EN, bReg);
				}

				sdRet	= AddCDSPSource();
			}
		}

		if(McResCtrl_GetAESource() != eMCDRV_SRC_NONE)
		{/*	AE is used	*/
			bReg	= 0;
			if((sAeInfo.bOnOff & MCDRV_EQ5_ON) != 0)
			{
				bReg |= MCB_EQ5ON;
			}
			if((sAeInfo.bOnOff & MCDRV_DRC_ON) != 0)
			{
				bReg |= MCB_DRCON;
				bReg |= MCB_BDSP_ST;
			}
			if((sAeInfo.bOnOff & MCDRV_EQ3_ON) != 0)
			{
				bReg |= MCB_EQ3ON;
			}
			if(McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0)
			{
				if((sAeInfo.bOnOff & MCDRV_BEXWIDE_ON) != 0)
				{
					bReg |= MCB_DBEXON;
					bReg |= MCB_BDSP_ST;
				}
			}
			if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
			{
				if(McResCtrl_GetAeEx() != NULL)
				{
					bReg |= MCB_CDSPINS;
					bReg |= MCB_BDSP_ST;
				}
			}
			if(((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_BDSP_ST)&MCB_BDSP_ST) == 0)
			&& ((bReg&MCB_BDSP_ST) != 0))
			{/*	BDSP_ST:0->1	*/
				/*	Reset TRAM	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_RST, MCB_TRAM_RST);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_RST, 0);
				/*	Wait 2/fs_SY(=42us)	*/
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_TIMWAIT | MCDRV_TRAM_REST_WAIT_TIME, 0);
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ST, bReg);
		}
		else
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ST, 0);
		}

		if(sdRet == MCDRV_SUCCESS)
		{
			/*	xxx_INS	*/
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_INS, bRegINS);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DAC_INS, bRegDAC_INS);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddSource", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	AddPMSource
 *
 *	Description:
 *			Add PM source setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddPMSource
(
	void
)
{
	UINT8	bSrc	= 0;
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddPMSource");
#endif

	if(McDevProf_IsValid(eMCDRV_FUNC_PM) != 0)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_AE, MCI_PM_SOURCE);
		McResCtrl_GetInitInfo(&sInitInfo);

		eSrcType	= McResCtrl_GetPMSource();
		if((eSrcType == eMCDRV_SRC_PDM)
		|| (eSrcType == eMCDRV_SRC_ADC0)
		|| (eSrcType == eMCDRV_SRC_DTMF))
		{
			bSrc |= MCB_PM_SOURCE_ADC;
		}
		else if(eSrcType == eMCDRV_SRC_DIR2)
		{
			bSrc |= MCB_PM_SOURCE_DIR2;
		}
		else if(eSrcType == eMCDRV_SRC_DIR0)
		{
			bSrc |= MCB_PM_SOURCE_DIR0;
		}
		else if(eSrcType == eMCDRV_SRC_DIR1)
		{
			bSrc |= MCB_PM_SOURCE_DIR1;
		}
		else if(eSrcType == eMCDRV_SRC_MIX)
		{
			bSrc |= MCB_PM_SOURCE_MIX;
		}
		else if(eSrcType == eMCDRV_SRC_ADC1)
		{
			bSrc |= MCB_PM_SOURCE_ADC1;
		}
		else if(eSrcType == eMCDRV_SRC_CDSP)
		{
			bSrc |= MCB_PM_SOURCE_DIR2;
		}
		else
		{
		}

		if(bSrc != (bReg&MCB_PM_SOURCE))
		{
			bReg	&= (UINT8)~MCB_PM_ON;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_PM_SOURCE, bReg);
		}
		if(bSrc != 0)
		{
			bReg	= bSrc | MCB_PM_ON;
			if(sInitInfo.bDclGain == MCDRV_DCLGAIN_12)
			{
				bReg	|= MCB_PM_SHIFT_12DB;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_PM_SOURCE, bReg);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddPMSource", 0);
#endif
}

/****************************************************************************
 *	AddCDSPSource
 *
 *	Description:
 *			Add DIT source setup packet.
 *	Arguments:
 *			pbAEng6		AEng6 setup
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR
 *
 ****************************************************************************/
static SINT32	AddCDSPSource
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#ifndef MCDRV_USE_CDSP_DRIVER

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddCDSPSource");
#endif

#else
	UINT8	bReg;
	UINT8	bDI2StartReg;
	UINT8	bEFIFO_CH	= 0,
			bOFIFO_CH	= 0;
	MCDRV_DIO_INFO	sDioInfo;
	MC_CODER_PARAMS	sPrm;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddCDSPSource");
#endif

	if(McResCtrl_GetAeEx() != NULL)
	{
		if(McResCtrl_IsDstUsed(eMCDRV_DST_AE, eMCDRV_DST_CH0) != 0)
		{
			if(McCdsp_GetState(eMC_PLAYER_CODER_B) != CDSP_STATE_ACTIVE)
			{/*	not Active	*/
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					sdRet	= McCdsp_Start(eMC_PLAYER_CODER_B);
				}
			}
		}
		else
		{
			if(McCdsp_GetState(eMC_PLAYER_CODER_B) == CDSP_STATE_ACTIVE)
			{/*	Active	*/
				sdRet	= McDevIf_ExecutePacket();
				if(sdRet == MCDRV_SUCCESS)
				{
					sdRet	= McCdsp_Stop(eMC_PLAYER_CODER_B);
					if(sdRet == MCDRV_SUCCESS)
					{
						sdRet	= McCdsp_Reset(eMC_PLAYER_CODER_B);
					}
				}
			}
		}
	}
	else if(McResCtrl_GetCdspFw() != NULL)
	{
		if((McResCtrl_GetCDSPSource(eMCDRV_DST_CH2) != eMCDRV_SRC_NONE)
		|| (McResCtrl_GetCDSPSource(eMCDRV_DST_CH3) != eMCDRV_SRC_NONE))
		{
			sPrm.bCommandId	= CDSP_CMD_DRV_SYS_SET_BIT_WIDTH;
			if(McCdsp_GetParam(eMC_PLAYER_CODER_A, &sPrm) < 0L)
			{
				sdRet	= MCDRV_ERROR;
			}
			else
			{
				if(sPrm.abParam[0] == 32)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
				else
				{
					bEFIFO_CH	= 4;
				}
			}
		}
		else if((McResCtrl_GetCDSPSource(eMCDRV_DST_CH0) != eMCDRV_SRC_NONE)
		|| (McResCtrl_GetCDSPSource(eMCDRV_DST_CH1) != eMCDRV_SRC_NONE))
		{
			bEFIFO_CH	= 2;
		}
		else
		{
			bEFIFO_CH	= 0;
		}

		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
		{/*	C-DSP->DIR2SRC	*/
			bOFIFO_CH	= 2;
		}
		else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP_DIRECT) != 0)
		{/*	C-DSP->DIT2	*/
			bOFIFO_CH	= 2;
		}
		else
		{
			bOFIFO_CH	= 0;
		}

		if(sdRet == MCDRV_SUCCESS)
		{
			if((bEFIFO_CH != 0) && (bOFIFO_CH != 0))
			{
				McResCtrl_GetDioInfo(&sDioInfo);
				if(sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
				{
					sdRet	= McDevIf_ExecutePacket();
					if(sdRet == MCDRV_SUCCESS)
					{
						bDI2StartReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START);
						bDI2StartReg	|= MCB_DITIM2_START;
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bDI2StartReg);
						sdRet	= McDevIf_ExecutePacket();
					}
				}
				else
				{
					bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_BCLK_MSK);
					if((bReg&(MCB_BCLK_MSK2|MCB_LRCK_MSK2)) == (MCB_BCLK_MSK2|MCB_LRCK_MSK2))
					{
						sdRet	= McDevIf_ExecutePacket();
						if(sdRet == MCDRV_SUCCESS)
						{
							bDI2StartReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START);
							bDI2StartReg	|= MCB_DITIM2_START;
							McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bDI2StartReg);
							sdRet	= McDevIf_ExecutePacket();
						}
					}
				}
				if(sdRet == MCDRV_SUCCESS)
				{
					if(bEFIFO_CH != gsSendPrm.abParam[0])
					{
						sdRet	= McPacket_StopDI2();
						if(sdRet == MCDRV_SUCCESS)
						{
							sPrm.bCommandId	= CDSP_CMD_DRV_SYS_SET_FIFO_CHANNELS;
							sPrm.abParam[0]	= bEFIFO_CH;
							sPrm.abParam[1]	= bOFIFO_CH;
							if(McCdsp_SetParam(eMC_PLAYER_CODER_A, &sPrm) < 0L)
							{
								sdRet	= MCDRV_ERROR;
							}
							else
							{
								gsSendPrm.abParam[0]	= bEFIFO_CH;
							}
						}
					}
				}
				if(sdRet == MCDRV_SUCCESS)
				{
					if(McCdsp_GetState(eMC_PLAYER_CODER_A) != CDSP_STATE_ACTIVE)
					{
						sdRet	= McDevIf_ExecutePacket();
						if(sdRet == MCDRV_SUCCESS)
						{
							sdRet	= McCdsp_Start(eMC_PLAYER_CODER_A);
						}
					}
				}
			}
			else
			{
				if(McCdsp_GetState(eMC_PLAYER_CODER_A) == CDSP_STATE_ACTIVE)
				{/*	Active	*/
					sdRet	= McDevIf_ExecutePacket();
					if(sdRet == MCDRV_SUCCESS)
					{
						sdRet	= McCdsp_Stop(eMC_PLAYER_CODER_A);
						if(sdRet == MCDRV_SUCCESS)
						{
							gsSendPrm.abParam[0]	= 0;
							sdRet	= McCdsp_Reset(eMC_PLAYER_CODER_A);
						}
					}
				}
			}
		}
	}
	else
	{
	}
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddCDSPSource", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_StopDI2
 *
 *	Description:
 *			Stop DI2.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_StopDI2
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg, bRegCur;
	MCDRV_SRC_TYPE	sSrcType;
	MCDRV_SRC_TYPE	asSrcType[4];
	UINT8	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_StopDI2");
#endif

	sdRet	= McDevIf_ExecutePacket();
	if(sdRet == MCDRV_SUCCESS)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START);
		if((bReg & (MCB_DIR2_SRC_START|MCB_DIR2_START|MCB_DIT2_SRC_START|MCB_DIT2_START)) != 0)
		{
			bRegCur	= bReg;

			if((bReg & MCB_DIR2_SRC_START) != 0)
			{
				if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
				{/*	C-DSP->DIR2SRC	*/
					bReg	&= (UINT8)~MCB_DIR2_SRC_START;
				}
			}

			asSrcType[0]	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
			asSrcType[1]	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
			asSrcType[2]	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
			asSrcType[3]	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);

			if((bReg & MCB_DIT2_SRC_START) != 0)
			{
				for(i = 0; i < 4; i++)
				{
					if((asSrcType[i] != eMCDRV_SRC_NONE)
					&& (asSrcType[i] != eMCDRV_SRC_DIR2_DIRECT))
					{/*	DIT2SRC->C-DSP	*/
						bReg	&= (UINT8)~MCB_DIT2_SRC_START;
						break;
					}
				}
			}

			if((bReg & MCB_DIR2_START) != 0)
			{
				for(i = 0; i < 4; i++)
				{
					if(asSrcType[i]== eMCDRV_SRC_DIR2_DIRECT)
					{
						bReg	&= (UINT8)~MCB_DIR2_START;
						break;
					}
				}
			}

			if((bReg & MCB_DIT2_START) != 0)
			{
				sSrcType	= McResCtrl_GetDITSource(eMCDRV_DIO_2);
				if(sSrcType == eMCDRV_SRC_CDSP_DIRECT)
				{/*	C-DSP_DIRECT->DIT2	*/
					bReg	&= (UINT8)~MCB_DIT2_START;
				}
			}

			if(bReg != bRegCur)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bReg);
				sdRet	= McDevIf_ExecutePacket();
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_StopDI2", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	AddDIStart
 *
 *	Description:
 *			Add DIStart setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddDIStart
(
	void
)
{
	UINT8	bReg;
	UINT8	bDIR2ClkMsk	= 0;
	MCDRV_DIO_INFO	sDioInfo;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddDIStart");
#endif

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_BCLK_MSK);
	if((bReg&(MCB_BCLK_MSK2|MCB_LRCK_MSK2)) == (MCB_BCLK_MSK2|MCB_LRCK_MSK2))
	{
		bDIR2ClkMsk	= 1;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	/*	DIR*_START, DIT*_START	*/
	bReg	= 0;
	if(McResCtrl_GetDITSource(eMCDRV_DIO_0) != eMCDRV_SRC_NONE)
	{
		if(sDioInfo.asPortInfo[0].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_DITIM0_START;
		}
		bReg |= MCB_DIT0_SRC_START;
		bReg |= MCB_DIT0_START;
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR0) != 0)
	{
		if(sDioInfo.asPortInfo[0].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_DITIM0_START;
		}
		bReg |= MCB_DIR0_SRC_START;
		bReg |= MCB_DIR0_START;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX0_START, bReg);

	bReg	= 0;
	if(McResCtrl_GetDITSource(eMCDRV_DIO_1) != eMCDRV_SRC_NONE)
	{
		if(sDioInfo.asPortInfo[1].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_DITIM1_START;
		}
		bReg |= MCB_DIT1_SRC_START;
		bReg |= MCB_DIT1_START;
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR1) != 0)
	{
		if(sDioInfo.asPortInfo[1].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
		{
			bReg |= MCB_DITIM1_START;
		}
		bReg |= MCB_DIR1_SRC_START;
		bReg |= MCB_DIR1_START;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX1_START, bReg);

	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		bReg	= 0;
		eSrcType	= McResCtrl_GetDITSource(eMCDRV_DIO_2);
		if(eSrcType != eMCDRV_SRC_NONE)
		{
			if(eSrcType == eMCDRV_SRC_CDSP_DIRECT)
			{
				if(sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
				{
					bReg |= MCB_DITIM2_START;
				}
				bReg |= MCB_DIT2_START;
			}
			else
			{
				if(sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
				{
					bReg |= MCB_DITIM2_START;
				}
				bReg |= MCB_DIT2_SRC_START;
				bReg |= MCB_DIT2_START;
			}
		}
		eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
		if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
		{
			eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
			if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
			{
				eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
				if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
				{
					eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
				}
			}
		}
		if((eSrcType != eMCDRV_SRC_NONE) && (eSrcType != eMCDRV_SRC_DIR2_DIRECT))
		{
			if(bDIR2ClkMsk != 0)
			{
				bReg |= MCB_DITIM2_START;
			}
			bReg |= MCB_DIT2_SRC_START;
		}

		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2) != 0)
		{
			if(sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
			{
				bReg |= MCB_DITIM2_START;
			}
			bReg |= MCB_DIR2_SRC_START;
			bReg |= MCB_DIR2_START;
		}
		else
		{
			if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2_DIRECT) != 0)
			{
				if(sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER)
				{
					bReg |= MCB_DITIM2_START;
				}
				bReg |= MCB_DIR2_START;
			}
			if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
			{
				if(bDIR2ClkMsk != 0)
				{
					bReg |= MCB_DITIM2_START;
				}
				bReg |= MCB_DIR2_SRC_START;
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddDIStart", 0);
#endif
}

/****************************************************************************
 *	McPacket_AddMixSet
 *
 *	Description:
 *			Add analog mixer set packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddMixSet
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_PATH_INFO	sPathInfo;
	MCDRV_SP_INFO	sSpInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddMixSet");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetPathInfo(&sPathInfo);
	McResCtrl_GetSpInfo(&sSpInfo);

	McPacket_AddSP();

	/*	ADL_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asAdc0[0]);
	if(((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		bReg |= MCB_LI1MIX;
	}
	if(((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		bReg |= MCB_LI2MIX;
	}
	if((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
	{
		bReg |= MCB_DAMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADL_MIX, bReg);
	/*	ADL_MONO	*/
	bReg	= 0;
	if((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	{
		bReg |= MCB_MONO_LI1;
	}
	if((sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	{
		bReg |= MCB_MONO_LI2;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADL_MONO, bReg);

	/*	ADR_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asAdc0[1]);
	if(((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
	|| ((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		bReg |= MCB_LI1MIX;
	}
	if(((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
	|| ((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		bReg |= MCB_LI2MIX;
	}
	if((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
	{
		bReg |= MCB_DAMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADR_MIX, bReg);
	/*	ADR_MONO	*/
	bReg	= 0;
	if((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	{
		bReg |= MCB_MONO_LI1;
	}
	if((sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	{
		bReg |= MCB_MONO_LI2;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADR_MONO, bReg);
	/*	ADM_MIX	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
	{
		bReg	= GetMicMixBit(&sPathInfo.asAdc1[0]);
		if((sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		{
			bReg |= MCB_LI1MIX;
		}
		if((sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		{
			bReg |= MCB_LI2MIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADM_MIX, bReg);
	}

	/*	L1L_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asLout1[0]);
	if(((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		bReg |= MCB_LI1MIX;
	}
	if(((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		bReg |= MCB_LI2MIX;
	}
	if(((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
	|| ((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
	{
		bReg |= MCB_DAMIX;
	}
	if((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
	{
		bReg |= MCB_LO1L_DARMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO1L_MIX, bReg);
	/*	L1L_MONO	*/
	bReg	= 0;
	if((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	{
		bReg |= MCB_MONO_LI1;
	}
	if((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	{
		bReg |= MCB_MONO_LI2;
	}
	if((sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
	{
		bReg |= MCB_MONO_DA;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO1L_MONO, bReg);
	/*	L1R_MIX	*/
	if(sInitInfo.bLineOut1Dif != MCDRV_LINE_DIF)
	{
		bReg	= GetMicMixBit(&sPathInfo.asLout1[1]);
		if((sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		{
			bReg |= MCB_LI1MIX;
		}
		if((sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		{
			bReg |= MCB_LI2MIX;
		}
		if((sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		{
			bReg |= MCB_DAMIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO1R_MIX, bReg);
	}

	/*	L2L_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asLout2[0]);
	if(((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		bReg |= MCB_LI1MIX;
	}
	if(((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		bReg |= MCB_LI2MIX;
	}
	if(((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
	|| ((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
	{
		bReg |= MCB_DAMIX;
	}
	if((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
	{
		bReg |= MCB_LO2L_DARMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO2L_MIX, bReg);
	/*	L2L_MONO	*/
	bReg	= 0;
	if((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	{
		bReg |= MCB_MONO_LI1;
	}
	if((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	{
		bReg |= MCB_MONO_LI2;
	}
	if((sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
	{
		bReg |= MCB_MONO_DA;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO2L_MONO, bReg);

	/*	L2R_MIX	*/
	if(sInitInfo.bLineOut2Dif != MCDRV_LINE_DIF)
	{
		bReg	= GetMicMixBit(&sPathInfo.asLout2[1]);
		if((sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		{
			bReg |= MCB_LI1MIX;
		}
		if((sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		{
			bReg |= MCB_LI2MIX;
		}
		if((sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		{
			bReg |= MCB_DAMIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO2R_MIX, bReg);
	}

	/*	HPL_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asHpOut[0]);
	if(((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		bReg |= MCB_LI1MIX;
	}
	if(((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		bReg |= MCB_LI2MIX;
	}
	if(((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
	|| ((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
	{
		bReg |= MCB_DAMIX;
	}
	if((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
	{
		bReg |= MCB_HPL_DARMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_HPL_MIX, bReg);
	/*	HPL_MONO	*/
	bReg	= 0;
	if((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	{
		bReg |= MCB_MONO_LI1;
	}
	if((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	{
		bReg |= MCB_MONO_LI2;
	}
	if((sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
	{
		bReg |= MCB_MONO_DA;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_HPL_MONO, bReg);

	/*	HPR_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asHpOut[1]);
	if((sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
	{
		bReg |= MCB_LI1MIX;
	}
	if((sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
	{
		bReg |= MCB_LI2MIX;
	}
	if((sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
	{
		bReg |= MCB_DAMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_HPR_MIX, bReg);

	if(sSpInfo.bSwap == MCDRV_SPSWAP_OFF)
	{/*	SP_SWAP=OFF	*/
		/*	SPL_MIX	*/
		bReg	= GetMicMixBit(&sPathInfo.asSpOut[0]);
		if(((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
		|| ((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
		{
			bReg |= MCB_LI1MIX;
		}
		if(((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
		|| ((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
		{
			bReg |= MCB_LI2MIX;
		}
		if(((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
		{
			bReg |= MCB_DAMIX;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		{
			bReg |= MCB_SPL_DARMIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPL_MIX, bReg);
		/*	SPL_MONO	*/
		bReg	= 0;
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		{
			bReg |= MCB_MONO_LI1;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		{
			bReg |= MCB_MONO_LI2;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		{
			bReg |= MCB_MONO_DA;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPL_MONO, bReg);

		/*	SPR_MIX	*/
		bReg	= GetMicMixBit(&sPathInfo.asSpOut[1]);
		if(((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		|| ((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
		{
			bReg |= MCB_LI1MIX;
		}
		if(((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		|| ((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
		{
			bReg |= MCB_LI2MIX;
		}
		if(((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
		{
			bReg |= MCB_DAMIX;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		{
			bReg |= MCB_SPR_DALMIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPR_MIX, bReg);
		/*	SPR_MONO	*/
		bReg	= 0;
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		{
			bReg |= MCB_MONO_LI1;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		{
			bReg |= MCB_MONO_LI2;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		{
			bReg |= MCB_MONO_DA;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPR_MONO, bReg);
	}
	else
	{
		/*	SPL_MIX	*/
		bReg	= GetMicMixBit(&sPathInfo.asSpOut[1]);
		if(((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		|| ((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
		{
			bReg |= MCB_LI1MIX;
		}
		if(((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		|| ((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
		{
			bReg |= MCB_LI2MIX;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		{
			bReg |= MCB_DAMIX;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		{
			bReg |= MCB_SPL_DALMIX;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		{
			bReg |= MCB_SPL_DARMIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPL_MIX, bReg);
		/*	SPR_MONO	*/
		bReg	= 0;
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		{
			bReg |= MCB_MONO_LI1;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		{
			bReg |= MCB_MONO_LI2;
		}
		if((sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		{
			bReg |= MCB_MONO_DA;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPL_MONO, bReg);

		/*	SPR_MIX	*/
		bReg	= GetMicMixBit(&sPathInfo.asSpOut[0]);
		if(((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
		|| ((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
		{
			bReg |= MCB_LI1MIX;
		}
		if(((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
		|| ((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
		{
			bReg |= MCB_LI2MIX;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		{
			bReg |= MCB_DAMIX;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		{
			bReg |= MCB_SPR_DARMIX;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		{
			bReg |= MCB_SPR_DALMIX;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPR_MIX, bReg);
		/*	SPL_MONO	*/
		bReg	= 0;
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		{
			bReg |= MCB_MONO_LI1;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		{
			bReg |= MCB_MONO_LI2;
		}
		if((sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		{
			bReg |= MCB_MONO_DA;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPR_MONO, bReg);
	}

	/*	RCV_MIX	*/
	bReg	= GetMicMixBit(&sPathInfo.asRcOut[0]);
	if((sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	{
		bReg |= MCB_LI1MIX;
	}
	if((sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	{
		bReg |= MCB_LI2MIX;
	}
	if((sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
	{
		bReg |= MCB_DALMIX;
	}
	if((sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
	{
		bReg |= MCB_DARMIX;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_RC_MIX, bReg);


#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddMixSet", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	GetMicMixBit
 *
 *	Description:
 *			Get mic mixer bit.
 *	Arguments:
 *			source info
 *	Return:
 *			mic mixer bit
 *
 ****************************************************************************/
static UINT8	GetMicMixBit
(
	const MCDRV_CHANNEL* psChannel
)
{
	UINT8	bMicMix	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetMicMixBit");
#endif

	if((psChannel->abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
	{
		bMicMix |= MCB_M1MIX;
	}
	if((psChannel->abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
	{
		bMicMix |= MCB_M2MIX;
	}
	if((psChannel->abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
	{
		bMicMix |= MCB_M3MIX;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bMicMix;
	McDebugLog_FuncOut("GetMicMixBit", &sdRet);
#endif
	return bMicMix;
}

/****************************************************************************
 *	McPacket_AddStart
 *
 *	Description:
 *			Add start packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddStart
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_PATH_INFO	sPathInfo;
	MCDRV_ADC_INFO	sAdcInfo;
	MCDRV_PDM_INFO	sPdmInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddStart");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetPathInfo(&sPathInfo);
	McResCtrl_GetAdcInfo(&sAdcInfo);
	McResCtrl_GetPdmInfo(&sPdmInfo);

	if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH0) == 1)
	|| (McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH1) == 1))
	{/*	ADC source is used	*/
		if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
		{
			if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START) & MCB_AD_START) == 0)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CDFLAG_RESET | (MCI_CD_VFLAG<<8) | (UINT32)(MCB_CD_ADC_VFLAGM|MCB_CD_ADC_VFLAGR|MCB_CD_ADC_VFLAGL), 0);
			}

			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_CD_START) | MCB_CDI_START;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_CD_START, bReg);
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_BI_START) | MCB_BIT_START;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_BI_START, bReg);
		}
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START) & MCB_AD_START) == 0)
		{
			bReg	= (sAdcInfo.bAgcOn << 2) | sAdcInfo.bAgcAdjust;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AD_AGC, bReg);
		}
		bReg	= (sAdcInfo.bMono << 1) | MCB_AD_START;
		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC1, eMCDRV_DST_CH0) == 0) || (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC1) == 0))
			{
				bReg	|= MCB_ADCSTOP_M;
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AD_START, bReg);
	}
	else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) != 0)
	{/*	PDM is used	*/
		if((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START) & MCB_PDM_START) == 0)
		{
			bReg	= (sPdmInfo.bAgcOn << 2) | sPdmInfo.bAgcAdjust;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_AGC, bReg);
			bReg	= (sPdmInfo.bMono << 1) | MCB_PDM_START;
			if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
			{
				if(sInitInfo.bPad2Func == MCDRV_PAD_PDMDI)
				{
					bReg	|= MCB_PDM_SOURCE_PA2;
				}
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_START, bReg);
		}
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddStart", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddStop
 *
 *	Description:
 *			Add stop packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddStop
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddStop");
#endif

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX0_START);
	if(McResCtrl_GetDITSource(eMCDRV_DIO_0) == eMCDRV_SRC_NONE)
	{/*	DIT is unused	*/
		bReg &= (UINT8)~MCB_DIT0_SRC_START;
		bReg &= (UINT8)~MCB_DIT0_START;
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR0) == 0)
	{/*	DIR is unused	*/
		bReg &= (UINT8)~MCB_DIR0_SRC_START;
		bReg &= (UINT8)~MCB_DIR0_START;
	}
	if((bReg & 0x0F) == 0)
	{
		bReg &= (UINT8)~MCB_DITIM0_START;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX0_START, bReg);

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX1_START);
	if(McResCtrl_GetDITSource(eMCDRV_DIO_1) == eMCDRV_SRC_NONE)
	{/*	DIT is unused	*/
		bReg &= (UINT8)~MCB_DIT1_SRC_START;
		bReg &= (UINT8)~MCB_DIT1_START;
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR1) == 0)
	{/*	DIR is unused	*/
		bReg &= (UINT8)~MCB_DIR1_SRC_START;
		bReg &= (UINT8)~MCB_DIR1_START;
	}
	if((bReg & 0x0F) == 0)
	{
		bReg &= (UINT8)~MCB_DITIM1_START;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX1_START, bReg);

	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX2_START);
		if(McResCtrl_GetDITSource(eMCDRV_DIO_2) == eMCDRV_SRC_NONE)
		{/*	DIT is unused	*/
			bReg &= (UINT8)~MCB_DIT2_START;
			eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
			if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
			{
				eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
				if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
				{
					eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
					if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
					{
						eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
					}
				}
			}
			if(eSrcType == eMCDRV_SRC_NONE)
			{
				bReg &= (UINT8)~MCB_DIT2_SRC_START;
			}
		}
		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2) == 0)
		{/*	DIR is unused	*/
			if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2_DIRECT) == 0)
			{
				bReg &= (UINT8)~MCB_DIR2_START;
			}
			if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) == 0)
			{
				bReg &= (UINT8)~MCB_DIR2_SRC_START;
			}
		}
		if((bReg & 0x0F) == 0)
		{
			bReg &= (UINT8)~MCB_DITIM2_START;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIX2_START, bReg);
	}

	if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH0) == 0)
	&& (McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH1) == 0))
	{/*	ADC0 source is unused	*/
		AddStopADC();
	}
	else if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC1, eMCDRV_DST_CH0) == 0) || (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC1) == 0))
	{
		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			/*	stop ADC1 only	*/
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START);
			if((bReg & MCB_AD_START) != 0)
			{
				bReg	|= MCB_ADCSTOP_M;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AD_START, bReg);
			}
		}
	}
	else
	{
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) == 0)
	{/*	PDM is unused	*/
		AddStopPDM();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddStop", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddVol
 *
 *	Description:
 *			Add volume mute packet.
 *	Arguments:
 *			dUpdate			target volume items
 *			eMode			update mode
 *			pdSVolDoneParam	wait soft volume complete flag
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddVol
(
	UINT32					dUpdate,
	MCDRV_VOLUPDATE_MODE	eMode,
	UINT32*					pdSVolDoneParam
)
{
	SINT32			sdRet	= MCDRV_SUCCESS;
	UINT8			bVolL;
	UINT8			bVolR;
	UINT8			bLAT;
	UINT8			bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_VOL_INFO	sVolInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddVol");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetVolReg(&sVolInfo);

	if((dUpdate & MCDRV_VOLUPDATE_ANAOUT_ALL) != (UINT32)0)
	{
		*pdSVolDoneParam	= 0;

		bVolL	= (UINT8)sVolInfo.aswA_Hp[0]&MCB_HPVOL_L;
		bVolR	= (UINT8)sVolInfo.aswA_Hp[1]&MCB_HPVOL_R;
		if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_HPVOL_L) & MCB_HPVOL_L))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
				&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_HPVOL_R)))
				{
					bLAT	= MCB_ALAT_HP;
				}
				else
				{
					bLAT	= 0;
				}
				bReg	= bLAT|MCB_SVOL_HP|bVolL;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_HPVOL_L, bReg);
				if(bVolL == MCDRV_REG_MUTE)
				{
					*pdSVolDoneParam	|= (MCB_HPL_BUSY<<8);
				}
			}
		}
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
		{
			if((bVolR == MCDRV_REG_MUTE) && (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_HPVOL_R) != 0))
			{
				*pdSVolDoneParam	|= (UINT8)MCB_HPR_BUSY;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_HPVOL_R, bVolR);
		}

		bVolL	= (UINT8)sVolInfo.aswA_Sp[0]&MCB_SPVOL_L;
		bVolR	= (UINT8)sVolInfo.aswA_Sp[1]&MCB_SPVOL_R;
		if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SPVOL_L) & MCB_SPVOL_L))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
				&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SPVOL_R)))
				{
					bLAT	= MCB_ALAT_SP;
				}
				else
				{
					bLAT	= 0;
				}
				bReg	= bLAT|MCB_SVOL_SP|bVolL;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPVOL_L, bReg);
				if(bVolL == MCDRV_REG_MUTE)
				{
					*pdSVolDoneParam	|= (MCB_SPL_BUSY<<8);
				}
			}
		}
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
		{
			if((bVolR == MCDRV_REG_MUTE) && (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SPVOL_R) != 0))
			{
				*pdSVolDoneParam	|= (UINT8)MCB_SPR_BUSY;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SPVOL_R, bVolR);
		}

		bVolL	= (UINT8)sVolInfo.aswA_Rc[0]&MCB_RCVOL;
		if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_RCVOL) & MCB_RCVOL))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				bReg	= MCB_SVOL_RC|bVolL;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_RCVOL, bReg);
				if(bVolL == MCDRV_REG_MUTE)
				{
					*pdSVolDoneParam	|= (MCB_RC_BUSY<<8);
				}
			}
		}

		bVolL	= (UINT8)sVolInfo.aswA_Lout1[0]&MCB_LO1VOL_L;
		bVolR	= (UINT8)sVolInfo.aswA_Lout1[1]&MCB_LO1VOL_R;
		if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LO1VOL_L) & MCB_LO1VOL_L))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
				&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LO1VOL_R)))
				{
					bLAT	= MCB_ALAT_LO1;
				}
				else
				{
					bLAT	= 0;
				}
				bReg	= bLAT|bVolL;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO1VOL_L, bReg);
			}
		}
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO1VOL_R, bVolR);
		}

		bVolL	= (UINT8)sVolInfo.aswA_Lout2[0]&MCB_LO2VOL_L;
		bVolR	= (UINT8)sVolInfo.aswA_Lout2[1]&MCB_LO2VOL_R;
		if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LO2VOL_L) & MCB_LO2VOL_L))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
				&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LO2VOL_R)))
				{
					bLAT	= MCB_ALAT_LO2;
				}
				else
				{
					bLAT	= 0;
				}
				bReg	= bLAT|bVolL;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO2VOL_L, bReg);
			}
		}
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LO2VOL_R, bVolR);
		}

		bVolL	= (UINT8)sVolInfo.aswA_HpGain[0];
		if(bVolL != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_HP_GAIN))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_HP_GAIN, bVolL);
			}
		}
	}
	if((dUpdate & ~MCDRV_VOLUPDATE_ANAOUT_ALL) != (UINT32)0)
	{
		if(McDevProf_IsValid(eMCDRV_FUNC_LI1) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswA_Lin1[0]&MCB_LI1VOL_L;
			bVolR	= (UINT8)sVolInfo.aswA_Lin1[1]&MCB_LI1VOL_R;
			if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LI1VOL_L) & MCB_LI1VOL_L))
			{
				if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
				{
					if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
					&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LI1VOL_R)))
					{
						bLAT	= MCB_ALAT_LI1;
					}
					else
					{
						bLAT	= 0;
					}
					bReg	= bLAT|bVolL;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LI1VOL_L, bReg);
				}
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LI1VOL_R, bVolR);
			}
		}

		if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswA_Lin2[0]&MCB_LI2VOL_L;
			bVolR	= (UINT8)sVolInfo.aswA_Lin2[1]&MCB_LI2VOL_R;
			if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LI2VOL_L) & MCB_LI2VOL_L))
			{
				if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
				{
					if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
					&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_LI2VOL_R)))
					{
						bLAT	= MCB_ALAT_LI2;
					}
					else
					{
						bLAT	= 0;
					}
					bReg	= bLAT|bVolL;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LI2VOL_L, bReg);
				}
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_LI2VOL_R, bVolR);
			}
		}

		bVolL	= (UINT8)sVolInfo.aswA_Mic1[0]&MCB_MC1VOL;
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC1VOL, bVolL);
		}
		bVolL	= (UINT8)sVolInfo.aswA_Mic2[0]&MCB_MC2VOL;
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC2VOL, bVolL);
		}
		bVolL	= (UINT8)sVolInfo.aswA_Mic3[0]&MCB_MC3VOL;
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC3VOL, bVolL);
		}

		bVolL	= (UINT8)sVolInfo.aswA_Ad0[0]&MCB_ADVOL_L;
		bVolR	= (UINT8)sVolInfo.aswA_Ad0[1]&MCB_ADVOL_R;
		if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_ADVOL_L) & MCB_ADVOL_L))
		{
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
				&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_ADVOL_R)))
				{
					bLAT	= MCB_ALAT_AD;
				}
				else
				{
					bLAT	= 0;
				}
				bReg	= bLAT|bVolL;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADVOL_L, bReg);
			}
		}
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADVOL_R, bVolR);
		}

		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswA_Ad1[0]&MCB_ADVOL_M;
			if(bVolL != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_ADVOL_M))
			{
				if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_ADVOL_M, bVolL);
				}
			}
		}

		bVolL	= (UINT8)sVolInfo.aswA_Mic2Gain[0]&0x03;
		bVolL	= (UINT8)((bVolL << 4) & MCB_MC2GAIN) | ((UINT8)sVolInfo.aswA_Mic1Gain[0]&MCB_MC1GAIN);
		bVolL |= ((sInitInfo.bMic2Sng << 6) & MCB_MC2SNG);
		bVolL |= ((sInitInfo.bMic1Sng << 2) & MCB_MC1SNG);
		if(eMode == eMCDRV_VOLUPDATE_MUTE)
		{
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_MC_GAIN);
			if(((bReg & MCB_MC2GAIN) == 0) && ((bReg & MCB_MC1GAIN) == 0))
			{
				;
			}
			else
			{
				if((bReg & MCB_MC2GAIN) == 0)
				{
					bVolL &= (UINT8)~MCB_MC2GAIN;
				}
				else if((bReg & MCB_MC1GAIN) == 0)
				{
					bVolL &= (UINT8)~MCB_MC1GAIN;
				}
				else
				{
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC_GAIN, bVolL);
			}
		}
		else
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC_GAIN, bVolL);
		}

		bVolL	= ((UINT8)sVolInfo.aswA_Mic3Gain[0]&MCB_MC3GAIN);
		if(sInitInfo.bMic3Sng == MCDRV_MIC_LINE)
		{
			if(McDevProf_GetDevId() == eMCDRV_DEV_ID_44H)
			{
				bVolL	|= MCB_LMSEL;
			}
		}
		else if(sInitInfo.bMic3Sng == MCDRV_MIC_SINGLE)
		{
			bVolL	|= MCB_MC3SNG;
		}
		else
		{
		}
		if(eMode == eMCDRV_VOLUPDATE_MUTE)
		{
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_MC3_GAIN);
			if((bReg & MCB_MC3GAIN) != 0)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC3_GAIN, bVolL);
			}
		}
		else
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_MC3_GAIN, bVolL);
		}

		/*	DIT0_INVOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Dit0[0]&MCB_DIT0_INVOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Dit0[1]&MCB_DIT0_INVOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_DIT0_INVOLL, MCB_DIT0_INLAT, MCI_DIT0_INVOLR, eMode);

		/*	DIT1_INVOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Dit1[0]&MCB_DIT1_INVOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Dit1[1]&MCB_DIT1_INVOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_DIT1_INVOLL, MCB_DIT1_INLAT, MCI_DIT1_INVOLR, eMode);

		/*	DIT2_INVOL	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Dit2[0]&MCB_DIT2_INVOLL;
			bVolR	= (UINT8)sVolInfo.aswD_Dit2[1]&MCB_DIT2_INVOLR;
			AddDigVolPacket(bVolL, bVolR, MCI_DIT2_INVOLL, MCB_DIT2_INLAT, MCI_DIT2_INVOLR, eMode);
		}

		/*	DIT2_INVOL1	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Dit21[0]&MCB_DIT2_INVOLL;
			bVolR	= (UINT8)sVolInfo.aswD_Dit21[1]&MCB_DIT2_INVOLR;
			AddDigVolPacket(bVolL, bVolR, MCI_DIT2_INVOLL1, MCB_DIT2_INLAT1, MCI_DIT2_INVOLR1, eMode);
		}

		/*	PDM0_VOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Pdm[0]&MCB_PDM0_VOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Pdm[1]&MCB_PDM0_VOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_PDM0_VOLL, MCB_PDM0_LAT, MCI_PDM0_VOLR, eMode);

		/*	DIR0_VOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Dir0[0]&MCB_DIR0_VOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Dir0[1]&MCB_DIR0_VOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_DIR0_VOLL, MCB_DIR0_LAT, MCI_DIR0_VOLR, eMode);

		/*	DIR1_VOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Dir1[0]&MCB_DIR1_VOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Dir1[1]&MCB_DIR1_VOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_DIR1_VOLL, MCB_DIR1_LAT, MCI_DIR1_VOLR, eMode);

		/*	DIR2_VOL	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Dir2[0]&MCB_DIR2_VOLL;
			bVolR	= (UINT8)sVolInfo.aswD_Dir2[1]&MCB_DIR2_VOLR;
			AddDigVolPacket(bVolL, bVolR, MCI_DIR2_VOLL, MCB_DIR2_LAT, MCI_DIR2_VOLR, eMode);
		}

		/*	ADC1_VOL	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Adc1[0]&MCB_ADC1_VOLL;
			bVolR	= (UINT8)sVolInfo.aswD_Adc1[1]&MCB_ADC1_VOLR;
			AddDigVolPacket(bVolL, bVolR, MCI_ADC1_VOLL, MCB_ADC1_LAT, MCI_ADC1_VOLR, eMode);
		}

		/*	ADC_VOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Ad0[0]&MCB_ADC_VOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Ad0[1]&MCB_ADC_VOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_ADC_VOLL, MCB_ADC_LAT, MCI_ADC_VOLR, eMode);

		/*	DTMFB_VOL	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_DTMF) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Dtmfb[0]&MCB_DTMFB_VOLL;
			bVolR	= (UINT8)sVolInfo.aswD_Dtmfb[1]&MCB_DTMFB_VOLR;
			AddDigVolPacket(bVolL, bVolR, MCI_DTMFB_VOLL, MCB_DTMFB_LAT, MCI_DTMFB_VOLR, eMode);
		}

		/*	AENG6_VOL	*/
		bVolL	= (UINT8)sVolInfo.aswD_Aeng6[0]&MCB_AENG6_VOLL;
		bVolR	= (UINT8)sVolInfo.aswD_Aeng6[1]&MCB_AENG6_VOLR;
		AddDigVolPacket(bVolL, bVolR, MCI_AENG6_VOLL, MCB_AENG6_LAT, MCI_AENG6_VOLR, eMode);

		/*	ADC_ATT	*/
		bVolL	= (UINT8)sVolInfo.aswD_Ad0Att[0]&MCB_ADC_ATTL;
		bVolR	= (UINT8)sVolInfo.aswD_Ad0Att[1]&MCB_ADC_ATTR;
		AddDigVolPacket(bVolL, bVolR, MCI_ADC_ATTL, MCB_ADC_ALAT, MCI_ADC_ATTR, eMode);

		/*	DIR0_ATT	*/
		bVolL	= (UINT8)sVolInfo.aswD_Dir0Att[0]&MCB_DIR0_ATTL;
		bVolR	= (UINT8)sVolInfo.aswD_Dir0Att[1]&MCB_DIR0_ATTR;
		AddDigVolPacket(bVolL, bVolR, MCI_DIR0_ATTL, MCB_DIR0_ALAT, MCI_DIR0_ATTR, eMode);

		/*	DIR1_ATT	*/
		bVolL	= (UINT8)sVolInfo.aswD_Dir1Att[0]&MCB_DIR1_ATTL;
		bVolR	= (UINT8)sVolInfo.aswD_Dir1Att[1]&MCB_DIR1_ATTR;
		AddDigVolPacket(bVolL, bVolR, MCI_DIR1_ATTL, MCB_DIR1_ALAT, MCI_DIR1_ATTR, eMode);

		/*	DIR2_ATT	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Dir2Att[0]&MCB_DIR2_ATTL;
			bVolR	= (UINT8)sVolInfo.aswD_Dir2Att[1]&MCB_DIR2_ATTR;
			AddDigVolPacket(bVolL, bVolR, MCI_DIR2_ATTL, MCB_DIR2_ALAT, MCI_DIR2_ATTR, eMode);
		}

		/*	ADC1_ATT	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Adc1Att[0]&MCB_ADC1_ATTL;
			bVolR	= (UINT8)sVolInfo.aswD_Adc1Att[1]&MCB_ADC1_ATTR;
			AddDigVolPacket(bVolL, bVolR, MCI_ADC1_ATTL, MCB_ADC1_ALAT, MCI_ADC1_ATTR, eMode);
		}

		/*	ST_VOL	*/
		if(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_AENG6_SOURCE) == MCB_AENG6_PDM)
		{
			bVolL	= (UINT8)sVolInfo.aswD_SideTone[0]&MCB_ST_VOLL;
			bVolR	= (UINT8)sVolInfo.aswD_SideTone[1]&MCB_ST_VOLR;
			AddDigVolPacket(bVolL, bVolR, MCI_ST_VOLL, MCB_ST_LAT, MCI_ST_VOLR, eMode);
		}

		/*	MASTER_OUT	*/
		bVolL	= (UINT8)sVolInfo.aswD_DacMaster[0]&MCB_MASTER_OUTL;
		bVolR	= (UINT8)sVolInfo.aswD_DacMaster[1]&MCB_MASTER_OUTR;
		AddDigVolPacket(bVolL, bVolR, MCI_MASTER_OUTL, MCB_MASTER_OLAT, MCI_MASTER_OUTR, eMode);

		/*	VOICE_ATT	*/
		bVolL	= (UINT8)sVolInfo.aswD_DacVoice[0]&MCB_VOICE_ATTL;
		bVolR	= (UINT8)sVolInfo.aswD_DacVoice[1]&MCB_VOICE_ATTR;
		AddDigVolPacket(bVolL, bVolR, MCI_VOICE_ATTL, MCB_VOICE_LAT, MCI_VOICE_ATTR, eMode);

		/*	DTMF_ATT	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_DTMF) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_DtmfAtt[0]&MCB_DTMF_ATTL;
			bVolR	= (UINT8)sVolInfo.aswD_DtmfAtt[1]&MCB_DTMF_ATTR;
			AddDigVolPacket(bVolL, bVolR, MCI_DTMF_ATTL, MCB_DTMF_LAT, MCI_DTMF_ATTR, eMode);
		}

		/*	DAC_ATT	*/
		bVolL	= (UINT8)sVolInfo.aswD_DacAtt[0]&MCB_DAC_ATTL;
		bVolR	= (UINT8)sVolInfo.aswD_DacAtt[1]&MCB_DAC_ATTR;
		AddDigVolPacket(bVolL, bVolR, MCI_DAC_ATTL, MCB_DAC_LAT, MCI_DAC_ATTR, eMode);

		/*	XX_AEMIX	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_MIX2) != 0)
		{
			bVolL	= (UINT8)sVolInfo.aswD_Adc0AeMix[0]&MCB_ADC_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_Adc0AeMix[1]&MCB_ADC_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_ADC_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_ADC_AEMIXR, bVolR);
			}

			bVolL	= (UINT8)sVolInfo.aswD_Dir2AeMix[0]&MCB_DIR2_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_Dir2AeMix[1]&MCB_DIR2_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_DIR2_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_DIR2_AEMIXR, bVolR);
			}

			bVolL	= (UINT8)sVolInfo.aswD_Dir0AeMix[0]&MCB_DIR0_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_Dir0AeMix[1]&MCB_DIR0_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_DIR0_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_DIR0_AEMIXR, bVolR);
			}

			bVolL	= (UINT8)sVolInfo.aswD_Dir1AeMix[0]&MCB_DIR1_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_Dir1AeMix[1]&MCB_DIR1_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_DIR1_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_DIR1_AEMIXR, bVolR);
			}

			bVolL	= (UINT8)sVolInfo.aswD_Adc1AeMix[0]&MCB_ADC1_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_Adc1AeMix[1]&MCB_ADC1_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_ADC1_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_ADC1_AEMIXR, bVolR);
			}

			bVolL	= (UINT8)sVolInfo.aswD_PdmAeMix[0]&MCB_PDM_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_PdmAeMix[1]&MCB_PDM_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_PDM_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_PDM_AEMIXR, bVolR);
			}

			bVolL	= (UINT8)sVolInfo.aswD_MixAeMix[0]&MCB_MIX_AEMIXL;
			bVolR	= (UINT8)sVolInfo.aswD_MixAeMix[1]&MCB_MIX_AEMIXR;
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_MIX_AEMIXL, bVolL);
			}
			if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)MCI_MIX_AEMIXR, bVolR);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddVol", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	AddDigVolPacket
 *
 *	Description:
 *			Add digital vol setup packet.
 *	Arguments:
 *				bVolL		Left volume
 *				bVolR		Right volume
 *				bVolLAddr	Left volume register address
 *				bLAT		LAT
 *				bVolRAddr	Right volume register address
 *				eMode		update mode
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddDigVolPacket
(
	UINT8	bVolL,
	UINT8	bVolR,
	UINT8	bVolLAddr,
	UINT8	bLAT,
	UINT8	bVolRAddr,
	MCDRV_VOLUPDATE_MODE	eMode
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddDigVolPacket");
#endif

	if(bVolL != (McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, bVolLAddr) & (UINT8)~bLAT))
	{
		if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolL == MCDRV_REG_MUTE))
		{
			if(((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
			&& (bVolR != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, bVolRAddr)))
			{
			}
			else
			{
				bLAT	= 0;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)bVolLAddr, bLAT|bVolL);
		}
	}
	if((eMode != eMCDRV_VOLUPDATE_MUTE) || (bVolR == MCDRV_REG_MUTE))
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)bVolRAddr, bVolR);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddDigVolPacket", 0);
#endif
}

/****************************************************************************
 *	AddStopADC
 *
 *	Description:
 *			Add stop ADC packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddStopADC
(
	void
)
{
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddStopADC");
#endif

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START);

	if((bReg & MCB_AD_START) != 0)
	{
		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			bReg	|= MCB_ADCSTOP_M;
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AD_START, bReg&(UINT8)~MCB_AD_START);

		if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
		{
			bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_BI_START) & (UINT8)~MCB_BIT_START);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_BI_START, bReg);
			bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_CD_START) & (UINT8)~MCB_CDI_START);
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_CD_START, bReg);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddStopADC", 0);
#endif
}

/****************************************************************************
 *	AddStopPDM
 *
 *	Description:
 *			Add stop PDM packet.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddStopPDM
(
	void
)
{
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("AddStopPDM");
#endif

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START);

	if((bReg & MCB_PDM_START) != 0)
	{
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_START, bReg&(UINT8)~MCB_PDM_START);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddStopPDM", 0);
#endif
}

/****************************************************************************
 *	McPacket_AddDigitalIO
 *
 *	Description:
 *			Add DigitalI0 setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddDigitalIO
(
	UINT32	dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_DIO_INFO		sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddDigitalIO");
#endif

	if(IsModifiedDIO(dUpdateInfo) != 0)
	{
		McResCtrl_GetCurPowerInfo(&sPowerInfo);
		sdRet	= PowerUpDig(MCDRV_DPB_UP);
		if(sdRet == MCDRV_SUCCESS)
		{
			if((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIOCommon(eMCDRV_DIO_0);
			}
			if((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIOCommon(eMCDRV_DIO_1);
			}
			if((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIOCommon(eMCDRV_DIO_2);
			}

			/*	DI*_BCKP	*/
			if(((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != (UINT32)0))
			{
				McResCtrl_GetDioInfo(&sDioInfo);
				bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_BCKP) & (UINT8)~(MCB_DI2_BCKP|MCB_DI1_BCKP|MCB_DI0_BCKP));
				if(sDioInfo.asPortInfo[0].sDioCommon.bBckInvert == MCDRV_BCLK_INVERT)
				{
					bReg |= MCB_DI0_BCKP;
				}
				if(sDioInfo.asPortInfo[1].sDioCommon.bBckInvert == MCDRV_BCLK_INVERT)
				{
					bReg |= MCB_DI1_BCKP;
				}
				if(sDioInfo.asPortInfo[2].sDioCommon.bBckInvert == MCDRV_BCLK_INVERT)
				{
					bReg |= MCB_DI2_BCKP;
				}
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_BCKP))
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_BCKP, bReg);
				}
			}

			if((dUpdateInfo & MCDRV_DIO0_DIR_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIODIR(eMCDRV_DIO_0);
			}
			if((dUpdateInfo & MCDRV_DIO1_DIR_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIODIR(eMCDRV_DIO_1);
			}
			if((dUpdateInfo & MCDRV_DIO2_DIR_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIODIR(eMCDRV_DIO_2);
			}

			if((dUpdateInfo & MCDRV_DIO0_DIT_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIODIT(eMCDRV_DIO_0);
			}
			if((dUpdateInfo & MCDRV_DIO1_DIT_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIODIT(eMCDRV_DIO_1);
			}
			if((dUpdateInfo & MCDRV_DIO2_DIT_UPDATE_FLAG) != (UINT32)0)
			{
				AddDIODIT(eMCDRV_DIO_2);
			}

			/*	unused path power down	*/
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddDigitalIO", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	IsModifiedDIO
 *
 *	Description:
 *			Is modified DigitalIO.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			0:not modified/1:modified
 *
 ****************************************************************************/
static UINT8	IsModifiedDIO
(
	UINT32		dUpdateInfo
)
{
	UINT8	bModified	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("IsModifiedDIO");
#endif

	if((((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIOCommon(eMCDRV_DIO_0) != 0))
	|| (((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIOCommon(eMCDRV_DIO_1) != 0))
	|| (((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIOCommon(eMCDRV_DIO_2) != 0)))
	{
		bModified	= 1;
	}
	else if((((dUpdateInfo & MCDRV_DIO0_DIR_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIODIR(eMCDRV_DIO_0) != 0))
	|| (((dUpdateInfo & MCDRV_DIO1_DIR_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIODIR(eMCDRV_DIO_1) != 0))
	|| (((dUpdateInfo & MCDRV_DIO2_DIR_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIODIR(eMCDRV_DIO_2) != 0)))
	{
		bModified	= 1;
	}
	else if((((dUpdateInfo & MCDRV_DIO0_DIT_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIODIT(eMCDRV_DIO_0) != 0))
	|| (((dUpdateInfo & MCDRV_DIO1_DIT_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIODIT(eMCDRV_DIO_1) != 0))
	|| (((dUpdateInfo & MCDRV_DIO2_DIT_UPDATE_FLAG) != (UINT32)0) && (IsModifiedDIODIT(eMCDRV_DIO_2) != 0)))
	{
		bModified	= 1;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bModified;
	McDebugLog_FuncOut("IsModifiedDIO", &sdRet);
#endif
	return bModified;
}

/****************************************************************************
 *	IsModifiedDIOCommon
 *
 *	Description:
 *			Is modified DigitalIO Common.
 *	Arguments:
 *			ePort		port number
 *	Return:
 *			0:not modified/1:modified
 *
 ****************************************************************************/
static UINT8	IsModifiedDIOCommon
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	UINT8	bReg;
	UINT8	bRegOffset;
	UINT8	bModified	= 0;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("IsModifiedDIOCommon");
#endif

	if(ePort == eMCDRV_DIO_0)
	{
		bRegOffset	= 0;
	}
	else if(ePort == eMCDRV_DIO_1)
	{
		bRegOffset	= MCI_DIMODE1 - MCI_DIMODE0;
	}
	else if(ePort == eMCDRV_DIO_2)
	{
		bRegOffset	= MCI_DIMODE2 - MCI_DIMODE0;
	}
	else
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("IsModifiedDIOCommon", &sdRet);
		#endif
		return 0;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIMODE0+bRegOffset))
	{
		bModified	= 1;
	}

	bReg	= (sDioInfo.asPortInfo[ePort].sDioCommon.bAutoFs << 7)
			| (sDioInfo.asPortInfo[ePort].sDioCommon.bBckFs << 4)
			| sDioInfo.asPortInfo[ePort].sDioCommon.bFs;
	if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DI_FS0+bRegOffset))
	{
		bModified	= 1;
	}

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DI0_SRC+bRegOffset);
	if((sDioInfo.asPortInfo[ePort].sDioCommon.bAutoFs == 0)
	&& (sDioInfo.asPortInfo[ePort].sDioCommon.bMasterSlave == MCDRV_DIO_SLAVE))
	{
		bReg |= MCB_DICOMMON_SRC_RATE_SET;
	}
	if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DI0_SRC+bRegOffset))
	{
		bModified	= 1;
	}
	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		bReg	= (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmHizTim << 7)
				| (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmClkDown << 6)
				| (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmFrame << 5)
				| (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmHighPeriod);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_HIZ_REDGE0+bRegOffset))
		{
			bModified	= 1;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bModified;
	McDebugLog_FuncOut("IsModifiedDIOCommon", &sdRet);
#endif
	return bModified;
}

/****************************************************************************
 *	IsModifiedDIODIR
 *
 *	Description:
 *			Is modified DigitalIO DIR.
 *	Arguments:
 *			ePort		port number
 *	Return:
 *			0:not modified/1:modified
 *
 ****************************************************************************/
static UINT8	IsModifiedDIODIR
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	UINT8	bReg;
	UINT8	bRegOffset;
	UINT8	bModified	= 0;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("IsModifiedDIODIR");
#endif

	if(ePort == eMCDRV_DIO_0)
	{
		bRegOffset	= 0;
	}
	else if(ePort == eMCDRV_DIO_1)
	{
		bRegOffset	= MCI_DIMODE1 - MCI_DIMODE0;
	}
	else if(ePort == eMCDRV_DIO_2)
	{
		bRegOffset	= MCI_DIMODE2 - MCI_DIMODE0;
	}
	else
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("IsModifiedDIODIR", &sdRet);
		#endif
		return 0;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	bReg	= (UINT8)(sDioInfo.asPortInfo[ePort].sDir.wSrcRate>>8);
	if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIRSRC_RATE0_MSB+bRegOffset))
	{
		bModified	= 1;
	}

	bReg	= (UINT8)sDioInfo.asPortInfo[ePort].sDir.wSrcRate;
	if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIRSRC_RATE0_LSB+bRegOffset))
	{
		bModified	= 1;
	}

	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface == MCDRV_DIO_DA)
	{
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bMode << 6)
				| (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bBitSel << 4)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bMode << 2)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bBitSel);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX0_FMT+bRegOffset))
		{
			bModified	= 1;
		}
		/*	DIR*_CH	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDir.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDir.abSlot[0]);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIR0_CH+bRegOffset))
		{
			bModified	= 1;
		}
	}
	else
	{
		/*	PCM_MONO_RX*, PCM_EXTEND_RX*, PCM_LSBON_RX*, PCM_LAW_RX*, PCM_BIT_RX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bMono << 7)
				| (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bOrder << 4)
				| (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bLaw << 2)
				| (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bBitSel);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PCM_RX0+bRegOffset))
		{
			bModified	= 1;
		}
		/*	PCM_CH1_RX*, PCM_CH0_RX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDir.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDir.abSlot[0]);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PCM_SLOT_RX0+bRegOffset))
		{
			bModified	= 1;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bModified;
	McDebugLog_FuncOut("IsModifiedDIODIR", &sdRet);
#endif
	return bModified;
}

/****************************************************************************
 *	IsModifiedDIODIT
 *
 *	Description:
 *			Is modified DigitalIO DIT.
 *	Arguments:
 *			ePort		port number
 *	Return:
 *			0:not modified/1:modified
 *
 ****************************************************************************/
static UINT8	IsModifiedDIODIT
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	UINT8	bReg;
	UINT8	bRegOffset;
	UINT8	bModified	= 0;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("IsModifiedDIODIT");
#endif

	if(ePort == eMCDRV_DIO_0)
	{
		bRegOffset	= 0;
	}
	else if(ePort == eMCDRV_DIO_1)
	{
		bRegOffset	= MCI_DIMODE1 - MCI_DIMODE0;
	}
	else if(ePort == eMCDRV_DIO_2)
	{
		bRegOffset	= MCI_DIMODE2 - MCI_DIMODE0;
	}
	else
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("IsModifiedDIODIT", &sdRet);
		#endif
		return 0;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	bReg	= (UINT8)(sDioInfo.asPortInfo[ePort].sDit.wSrcRate>>8);
	if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DITSRC_RATE0_MSB+bRegOffset))
	{
		bModified	= 1;
	}
	bReg	= (UINT8)sDioInfo.asPortInfo[ePort].sDit.wSrcRate;
	if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DITSRC_RATE0_LSB+bRegOffset))
	{
		bModified	= 1;
	}

	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface == MCDRV_DIO_DA)
	{
		/*	DIT*_FMT, DIT*_BIT, DIR*_FMT, DIR*_BIT	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bMode << 6)
				| (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bBitSel << 4)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bMode << 2)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bBitSel);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIX0_FMT+bRegOffset))
		{
			bModified	= 1;
		}

		/*	DIT*_SLOT	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDit.abSlot[0]);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT0_SLOT+bRegOffset))
		{
			bModified	= 1;
		}
	}
	else
	{
		/*	PCM_MONO_TX*, PCM_EXTEND_TX*, PCM_LSBON_TX*, PCM_LAW_TX*, PCM_BIT_TX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bMono << 7)
				| (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bOrder << 4)
				| (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bLaw << 2)
				| (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bBitSel);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PCM_TX0+bRegOffset))
		{
			bModified	= 1;
		}

		/*	PCM_CH1_TX*, PCM_CH0_TX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDit.abSlot[0]);
		if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PCM_SLOT_TX0+bRegOffset))
		{
			bModified	= 1;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bModified;
	McDebugLog_FuncOut("IsModifiedDIODIT", &sdRet);
#endif
	return bModified;
}

/****************************************************************************
 *	AddDIOCommon
 *
 *	Description:
 *			Add DigitalI0 Common setup packet.
 *	Arguments:
 *			ePort		port number
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddDIOCommon
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	UINT8	bReg;
	UINT8	bRegOffset;
	UINT8	bSRCRateSet;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("AddDIOCommon");
#endif

	if(ePort == eMCDRV_DIO_0)
	{
		bRegOffset	= 0;
	}
	else if(ePort == eMCDRV_DIO_1)
	{
		bRegOffset	= MCI_DIMODE1 - MCI_DIMODE0;
	}
	else if(ePort == eMCDRV_DIO_2)
	{
		bRegOffset	= MCI_DIMODE2 - MCI_DIMODE0;
	}
	else
	{

		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("AddDIOCommon", &sdRet);
		#endif
		return;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	/*	DIMODE*	*/
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIMODE0+bRegOffset),
							sDioInfo.asPortInfo[ePort].sDioCommon.bInterface);

	/*	DIAUTO_FS*, DIBCK*, DIFS*	*/
	bReg	= (sDioInfo.asPortInfo[ePort].sDioCommon.bAutoFs << 7)
			| (sDioInfo.asPortInfo[ePort].sDioCommon.bBckFs << 4)
			| sDioInfo.asPortInfo[ePort].sDioCommon.bFs;
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DI_FS0+bRegOffset), bReg);

	/*	DI*_SRCRATE_SET	*/
	bSRCRateSet	= McDevProf_GetSRCRateSet();
	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DI0_SRC+bRegOffset);
	if((sDioInfo.asPortInfo[ePort].sDioCommon.bAutoFs == 0)
	&& (sDioInfo.asPortInfo[ePort].sDioCommon.bMasterSlave == MCDRV_DIO_SLAVE))
	{
		bReg |= bSRCRateSet;
	}
	else
	{
		bReg &= (UINT8)~bSRCRateSet;
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DI0_SRC+bRegOffset), bReg);

	/*	HIZ_REDGE*, PCM_CLKDOWN*, PCM_FRAME*, PCM_HPERIOD*	*/
	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		bReg	= (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmHizTim << 7)
				| (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmClkDown << 6)
				| (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmFrame << 5)
				| (sDioInfo.asPortInfo[ePort].sDioCommon.bPcmHighPeriod);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_HIZ_REDGE0+bRegOffset), bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddDIOCommon", 0);
#endif
}

/****************************************************************************
 *	AddDIODIR
 *
 *	Description:
 *			Add DigitalI0 DIR setup packet.
 *	Arguments:
 *			ePort		port number
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddDIODIR
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	UINT8	bReg;
	UINT8	bRegOffset;
	UINT16	wSrcRate;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("AddDIODIR");
#endif

	if(ePort == eMCDRV_DIO_0)
	{
		bRegOffset	= 0;
	}
	else if(ePort == eMCDRV_DIO_1)
	{
		bRegOffset	= MCI_DIMODE1 - MCI_DIMODE0;
	}
	else if(ePort == eMCDRV_DIO_2)
	{
		bRegOffset	= MCI_DIMODE2 - MCI_DIMODE0;
	}
	else
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("AddDIODIR", &sdRet);
		#endif
		return;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	/*	DIRSRC_RATE*	*/
	wSrcRate	= sDioInfo.asPortInfo[ePort].sDir.wSrcRate;
	if(wSrcRate == 0)
	{
		switch(sDioInfo.asPortInfo[ePort].sDioCommon.bFs)
		{
		case MCDRV_FS_48000:
			wSrcRate	= MCDRV_DIR_SRCRATE_48000;
			break;
		case MCDRV_FS_44100:
			wSrcRate	= MCDRV_DIR_SRCRATE_44100;
			break;
		case MCDRV_FS_32000:
			wSrcRate	= MCDRV_DIR_SRCRATE_32000;
			break;
		case MCDRV_FS_24000:
			wSrcRate	= MCDRV_DIR_SRCRATE_24000;
			break;
		case MCDRV_FS_22050:
			wSrcRate	= MCDRV_DIR_SRCRATE_22050;
			break;
		case MCDRV_FS_16000:
			wSrcRate	= MCDRV_DIR_SRCRATE_16000;
			break;
		case MCDRV_FS_12000:
			wSrcRate	= MCDRV_DIR_SRCRATE_12000;
			break;
		case MCDRV_FS_11025:
			wSrcRate	= MCDRV_DIR_SRCRATE_11025;
			break;
		case MCDRV_FS_8000:
			wSrcRate	= MCDRV_DIR_SRCRATE_8000;
			break;
		default:
			/* unreachable */
			wSrcRate = 0;
			break;
		}
	}
	bReg	= (UINT8)(wSrcRate>>8);
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIRSRC_RATE0_MSB+bRegOffset), bReg);
	bReg	= (UINT8)wSrcRate;
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIRSRC_RATE0_LSB+bRegOffset), bReg);

	/*	DIT*_FMT, DIT*_BIT, DIR*_FMT, DIR*_BIT	*/
	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface == MCDRV_DIO_DA)
	{
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bMode << 6)
				| (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bBitSel << 4)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bMode << 2)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bBitSel);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIX0_FMT+bRegOffset), bReg);
		/*	DIR*_CH	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDir.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDir.abSlot[0]);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIR0_CH+bRegOffset), bReg);
	}
	else
	{
		/*	PCM_MONO_RX*, PCM_EXTEND_RX*, PCM_LSBON_RX*, PCM_LAW_RX*, PCM_BIT_RX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bMono << 7)
				| (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bOrder << 4)
				| (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bLaw << 2)
				| (sDioInfo.asPortInfo[ePort].sDir.sPcmFormat.bBitSel);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_PCM_RX0+bRegOffset), bReg);
		/*	PCM_CH1_RX*, PCM_CH0_RX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDir.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDir.abSlot[0]);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_PCM_SLOT_RX0+bRegOffset), bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddDIODIR", 0);
#endif
}

/****************************************************************************
 *	AddDIODIT
 *
 *	Description:
 *			Add DigitalI0 DIT setup packet.
 *	Arguments:
 *			ePort		port number
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	AddDIODIT
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	UINT8	bReg;
	UINT8	bRegOffset;
	UINT16	wSrcRate;
	MCDRV_DIO_INFO	sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("AddDIODIT");
#endif

	if(ePort == eMCDRV_DIO_0)
	{
		bRegOffset	= 0;
	}
	else if(ePort == eMCDRV_DIO_1)
	{
		bRegOffset	= MCI_DIMODE1 - MCI_DIMODE0;
	}
	else if(ePort == eMCDRV_DIO_2)
	{
		bRegOffset	= MCI_DIMODE2 - MCI_DIMODE0;
	}
	else
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("AddDIODIT", &sdRet);
		#endif
		return;
	}

	McResCtrl_GetDioInfo(&sDioInfo);

	wSrcRate	= sDioInfo.asPortInfo[ePort].sDit.wSrcRate;
	if(wSrcRate == 0)
	{
		switch(sDioInfo.asPortInfo[ePort].sDioCommon.bFs)
		{
		case MCDRV_FS_48000:
			wSrcRate	= MCDRV_DIT_SRCRATE_48000;
			break;
		case MCDRV_FS_44100:
			wSrcRate	= MCDRV_DIT_SRCRATE_44100;
			break;
		case MCDRV_FS_32000:
			wSrcRate	= MCDRV_DIT_SRCRATE_32000;
			break;
		case MCDRV_FS_24000:
			wSrcRate	= MCDRV_DIT_SRCRATE_24000;
			break;
		case MCDRV_FS_22050:
			wSrcRate	= MCDRV_DIT_SRCRATE_22050;
			break;
		case MCDRV_FS_16000:
			wSrcRate	= MCDRV_DIT_SRCRATE_16000;
			break;
		case MCDRV_FS_12000:
			wSrcRate	= MCDRV_DIT_SRCRATE_12000;
			break;
		case MCDRV_FS_11025:
			wSrcRate	= MCDRV_DIT_SRCRATE_11025;
			break;
		case MCDRV_FS_8000:
			wSrcRate	= MCDRV_DIT_SRCRATE_8000;
			break;
		default:
			/* unreachable */
			wSrcRate = 0;
			break;
		}
	}
	/*	DITSRC_RATE*	*/
	bReg	= (UINT8)(wSrcRate>>8);
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DITSRC_RATE0_MSB+bRegOffset), bReg);
	bReg	= (UINT8)wSrcRate;
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DITSRC_RATE0_LSB+bRegOffset), bReg);

	if(sDioInfo.asPortInfo[ePort].sDioCommon.bInterface == MCDRV_DIO_DA)
	{
		/*	DIT*_FMT, DIT*_BIT, DIR*_FMT, DIR*_BIT	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bMode << 6)
				| (sDioInfo.asPortInfo[ePort].sDit.sDaFormat.bBitSel << 4)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bMode << 2)
				| (sDioInfo.asPortInfo[ePort].sDir.sDaFormat.bBitSel);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIX0_FMT+bRegOffset), bReg);

		/*	DIT*_SLOT	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDit.abSlot[0]);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_DIT0_SLOT+bRegOffset), bReg);
	}
	else
	{
		/*	PCM_MONO_TX*, PCM_EXTEND_TX*, PCM_LSBON_TX*, PCM_LAW_TX*, PCM_BIT_TX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bMono << 7)
				| (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bOrder << 4)
				| (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bLaw << 2)
				| (sDioInfo.asPortInfo[ePort].sDit.sPcmFormat.bBitSel);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_PCM_TX0+bRegOffset), bReg);

		/*	PCM_CH1_TX*, PCM_CH0_TX*	*/
		bReg	= (sDioInfo.asPortInfo[ePort].sDit.abSlot[1] << 4) | (sDioInfo.asPortInfo[ePort].sDit.abSlot[0]);
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)(MCI_PCM_SLOT_TX0+bRegOffset), bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("AddDIODIT", 0);
#endif
}

/****************************************************************************
 *	McPacket_AddDAC
 *
 *	Description:
 *			Add DAC setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddDAC
(
	UINT32	dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_DAC_INFO	sDacInfo;
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddDAC");
#endif

	McResCtrl_GetDacInfo(&sDacInfo);

	if(((dUpdateInfo & MCDRV_DAC_MSWP_UPDATE_FLAG) != (UINT32)0) || ((dUpdateInfo & MCDRV_DAC_VSWP_UPDATE_FLAG) != (UINT32)0))
	{
		bReg	= (sDacInfo.bMasterSwap<<4)|sDacInfo.bVoiceSwap;
		if(bReg == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SWP))
		{
			dUpdateInfo	&= ~(MCDRV_DAC_MSWP_UPDATE_FLAG|MCDRV_DAC_VSWP_UPDATE_FLAG);
		}
	}
	if((dUpdateInfo & MCDRV_DAC_HPF_UPDATE_FLAG) != (UINT32)0)
	{
		if(sDacInfo.bDcCut == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_DCCUTOFF))
		{
			dUpdateInfo	&= ~(MCDRV_DAC_HPF_UPDATE_FLAG);
		}
	}
	if((dUpdateInfo & MCDRV_DAC_DACSWP_UPDATE_FLAG) != (UINT32)0)
	{
		if(McDevProf_IsValid(eMCDRV_FUNC_DACSWAP) == 0)
		{
			dUpdateInfo	&= ~(MCDRV_DAC_DACSWP_UPDATE_FLAG);
		}
		else
		{
			if(sDacInfo.bDacSwap == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_DAC_SWAP))
			{
				dUpdateInfo	&= ~(MCDRV_DAC_DACSWP_UPDATE_FLAG);
			}
		}
	}

	if(dUpdateInfo != (UINT32)0)
	{
		if(((dUpdateInfo & MCDRV_DAC_MSWP_UPDATE_FLAG) != (UINT32)0) || ((dUpdateInfo & MCDRV_DAC_VSWP_UPDATE_FLAG) != (UINT32)0))
		{
			bReg	= (sDacInfo.bMasterSwap<<4)|sDacInfo.bVoiceSwap;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SWP, bReg);
		}
		if((dUpdateInfo & MCDRV_DAC_HPF_UPDATE_FLAG) != (UINT32)0)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DCCUTOFF, sDacInfo.bDcCut);
		}
		if(McDevProf_IsValid(eMCDRV_FUNC_DACSWAP) != 0)
		{
			if((dUpdateInfo & MCDRV_DAC_DACSWP_UPDATE_FLAG) != (UINT32)0)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_DAC_SWAP, sDacInfo.bDacSwap);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddDAC", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_AddADC
 *
 *	Description:
 *			Add ADC setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddADC
(
	UINT32			dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_ADC_INFO		sAdcInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddADC");
#endif

	McResCtrl_GetAdcInfo(&sAdcInfo);

	if(((dUpdateInfo & MCDRV_ADCADJ_UPDATE_FLAG) != (UINT32)0) || ((dUpdateInfo & MCDRV_ADCAGC_UPDATE_FLAG) != (UINT32)0))
	{
		bReg	= (sAdcInfo.bAgcOn<<2)|sAdcInfo.bAgcAdjust;
		if(bReg == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_AGC))
		{
			dUpdateInfo	&= ~(MCDRV_ADCADJ_UPDATE_FLAG|MCDRV_ADCAGC_UPDATE_FLAG);
		}
	}
	if((dUpdateInfo & MCDRV_ADCMONO_UPDATE_FLAG) != (UINT32)0)
	{
		bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START) & MCB_AD_MN);
		if(bReg == (sAdcInfo.bMono << 1))
		{
			dUpdateInfo	&= ~(MCDRV_ADCMONO_UPDATE_FLAG);
		}
	}

	if(dUpdateInfo != (UINT32)0)
	{
		McResCtrl_GetCurPowerInfo(&sPowerInfo);
		sdRet	= PowerUpDig(MCDRV_DPB_UP);
		if(sdRet == MCDRV_SUCCESS)
		{
			if(((dUpdateInfo & MCDRV_ADCADJ_UPDATE_FLAG) != (UINT32)0) || ((dUpdateInfo & MCDRV_ADCAGC_UPDATE_FLAG) != (UINT32)0))
			{
				bReg	= (sAdcInfo.bAgcOn<<2)|sAdcInfo.bAgcAdjust;
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_AGC))
				{
					AddStopADC();
					sdRet	= McDevIf_ExecutePacket();
				}
				if(MCDRV_SUCCESS == sdRet)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AD_AGC, bReg);
				}
			}
			if(sdRet == MCDRV_SUCCESS)
			{
				if((dUpdateInfo & MCDRV_ADCMONO_UPDATE_FLAG) != (UINT32)0)
				{
					bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AD_START);
					if((bReg & MCB_AD_MN) != (sAdcInfo.bMono << 1))
					{
						AddStopADC();
						sdRet	= McDevIf_ExecutePacket();
					}
					if(sdRet == MCDRV_SUCCESS)
					{
						if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) == 0)
						{
							bReg	= 0;
						}
						else
						{
							bReg	= MCB_ADCSTOP_M;
						}
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AD_START, bReg|(UINT8)(sAdcInfo.bMono << 1));
					}
				}
				if(sdRet == MCDRV_SUCCESS)
				{
					/*	unused path power down	*/
					sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
					sPowerUpdate.abAnalog[0]	= 
					sPowerUpdate.abAnalog[1]	= 
					sPowerUpdate.abAnalog[2]	= 
					sPowerUpdate.abAnalog[3]	= 
					sPowerUpdate.abAnalog[4]	= 
					sPowerUpdate.abAnalog[5]	= 0;
					sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
					if(sdRet == MCDRV_SUCCESS)
					{
						sdRet	= McPacket_AddStart();
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddADC", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddADCEx
 *
 *	Description:
 *			Add ADC_EX setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddADCEx
(
	UINT32			dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_ADC_EX_INFO	sAdcExInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddADCEx");
#endif

	McResCtrl_GetAdcExInfo(&sAdcExInfo);

	if((dUpdateInfo & MCDRV_AGCENV_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AGC);
		if((bReg&MCB_AGC_ENV) == sAdcExInfo.bAgcEnv)
		{
			dUpdateInfo	&= ~MCDRV_AGCENV_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCLVL_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AGC);
		if((bReg&MCB_AGC_LVL) == (sAdcExInfo.bAgcLvl<<2))
		{
			dUpdateInfo	&= ~MCDRV_AGCLVL_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCUPTIM_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AGC);
		if((bReg&MCB_AGC_UPTIM) == (sAdcExInfo.bAgcUpTim<<4))
		{
			dUpdateInfo	&= ~MCDRV_AGCUPTIM_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCDWTIM_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AGC);
		if((bReg&MCB_AGC_DWTIM) == (sAdcExInfo.bAgcDwTim<<6))
		{
			dUpdateInfo	&= ~MCDRV_AGCDWTIM_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTLVL_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AGC_CUT);
		if((bReg&MCB_AGC_CUTLVL) == sAdcExInfo.bAgcCutLvl)
		{
			dUpdateInfo	&= ~MCDRV_AGCCUTLVL_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTTIM_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_AGC_CUT);
		if((bReg&MCB_AGC_CUTTIM) == (sAdcExInfo.bAgcCutTim<<2))
		{
			dUpdateInfo	&= ~MCDRV_AGCCUTTIM_UPDATE_FLAG;
		}
	}

	if(dUpdateInfo != (UINT32)0)
	{
		AddStopADC();
		sdRet	= McDevIf_ExecutePacket();
		if(MCDRV_SUCCESS == sdRet)
		{
			bReg	= (sAdcExInfo.bAgcDwTim<<6) | (sAdcExInfo.bAgcUpTim<<4) | (sAdcExInfo.bAgcLvl<<2) | sAdcExInfo.bAgcEnv;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AGC, bReg);

			bReg	= (sAdcExInfo.bAgcCutTim<<2) | sAdcExInfo.bAgcCutLvl;
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_AGC_CUT, bReg);

			sdRet	= McPacket_AddStart();
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddADCEx", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddSP
 *
 *	Description:
 *			Add SP setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddSP
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_SP_INFO	sSpInfo;
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddSP");
#endif

	bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_SP_MODE) & (UINT8)~MCB_SP_SWAP);

	McResCtrl_GetSpInfo(&sSpInfo);

	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_SP_MODE, bReg|sSpInfo.bSwap);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddSP", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_AddDNG
 *
 *	Description:
 *			Add Digital Noise Gate setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddDNG
(
	UINT32	dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_DNG_INFO	sDngInfo;
	UINT8	bReg;
	UINT8	bRegDNGON;
	UINT8	bItem;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddDNG");
#endif

	McResCtrl_GetDngInfo(&sDngInfo);

	if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
	{
		if((dUpdateInfo & MCDRV_DNGFW_FLAG) != 0UL)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)MCI_DNG_FW, sDngInfo.bFw);
		}
	}

	for(bItem = MCDRV_DNG_ITEM_HP; bItem <= MCDRV_DNG_ITEM_RC; bItem++)
	{
		bRegDNGON	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_DNGON_HP+(bItem*3));

		/*	ReleseTime, Attack Time	*/
		if(sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
		{
			if(((dUpdateInfo & (MCDRV_DNGREL_HP_UPDATE_FLAG<<(8*bItem))) != 0UL) || ((dUpdateInfo & (MCDRV_DNGATK_HP_UPDATE_FLAG<<(8*bItem))) != 0UL))
			{
				bReg	= (sDngInfo.abRelease[bItem]<<4)|sDngInfo.abAttack[bItem];
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_DNGATRT_HP+(bItem*3)))
				{
					if((bRegDNGON&0x01) != 0)
					{/*	DNG on	*/
						/*	DNG off	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGON_HP+(bItem*3)), bRegDNGON&0xFE);
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGATRT_HP+(bItem*3)), bReg);
				}
			}
		}

		/*	Target	*/
		if((dUpdateInfo & (MCDRV_DNGTARGET_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
		{
			bReg	= sDngInfo.abTarget[bItem]<<4;
			if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_DNGTARGET_HP+(bItem*3)))
			{
				if((bRegDNGON&0x01) != 0)
				{/*	DNG on	*/
					/*	DNG off	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGON_HP+(bItem*3)), bRegDNGON&0xFE);
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGTARGET_HP+(bItem*3)), bReg);
			}
		}

		/*	Threshold, HoldTime	*/
		if(sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
		{
			if(((dUpdateInfo & (MCDRV_DNGTHRES_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
			|| ((dUpdateInfo & (MCDRV_DNGHOLD_HP_UPDATE_FLAG<<(8*bItem))) != 0UL))
			{
				bReg	= (sDngInfo.abThreshold[bItem]<<4)|(sDngInfo.abHold[bItem]<<1);
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_DNGON_HP+(bItem*3)))
				{
					if((bRegDNGON&0x01) != 0)
					{/*	DNG on	*/
						/*	DNG off	*/
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGON_HP+(bItem*3)), bRegDNGON&0xFE);
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGON_HP+(bItem*3)), bReg);
				}
				bRegDNGON	= bReg | (bRegDNGON&0x01);
			}
		}

		/*	DNGON	*/
		if((dUpdateInfo & (MCDRV_DNGSW_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
		{
			bRegDNGON	= (bRegDNGON&0xFE) | sDngInfo.abOnOff[bItem];
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_ANA | (UINT32)(MCI_DNGON_HP+(bItem*3)), bRegDNGON);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddDNG", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_AddAE
 *
 *	Description:
 *			Add Audio Engine setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddAE
(
	UINT32	dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	i;
	UINT8	bReg;
	UINT32	dXFadeParam	= 0;
	UINT32	dwDRCDataSize;
	MCDRV_AE_INFO	sAeInfo;
	MCDRV_POWER_INFO	sPowerInfo, sCurPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddAE");
#endif

	McResCtrl_GetAeInfo(&sAeInfo);

	if(McResCtrl_IsDstUsed(eMCDRV_DST_AE, eMCDRV_DST_CH0) == 1)
	{/*	AE is used	*/
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_BDSP_ST);
		if(McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0)
		{
			if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEXWIDE_ONOFF) != (UINT32)0)
			{
				if((((sAeInfo.bOnOff & MCDRV_BEXWIDE_ON) != 0) && ((bReg & MCB_DBEXON) != 0))
				|| (((sAeInfo.bOnOff & MCDRV_BEXWIDE_ON) == 0) && ((bReg & MCB_DBEXON) == 0)))
				{
					dUpdateInfo	&= ~MCDRV_AEUPDATE_FLAG_BEXWIDE_ONOFF;
				}
			}
		}
		else
		{
			dUpdateInfo	&= ~MCDRV_AEUPDATE_FLAG_BEXWIDE_ONOFF;
		}
		if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC_ONOFF) != (UINT32)0)
		{
			if((((sAeInfo.bOnOff & MCDRV_DRC_ON) != 0) && ((bReg & MCB_DRCON) != 0))
			|| (((sAeInfo.bOnOff & MCDRV_DRC_ON) == 0) && ((bReg & MCB_DRCON) == 0)))
			{
				dUpdateInfo	&= ~MCDRV_AEUPDATE_FLAG_DRC_ONOFF;
			}
		}
		if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5_ONOFF) != (UINT32)0)
		{
			if((((sAeInfo.bOnOff & MCDRV_EQ5_ON) != 0) && ((bReg & MCB_EQ5ON) != 0))
			|| (((sAeInfo.bOnOff & MCDRV_EQ5_ON) == 0) && ((bReg & MCB_EQ5ON) == 0)))
			{
				dUpdateInfo	&= ~MCDRV_AEUPDATE_FLAG_EQ5_ONOFF;
			}
		}
		if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3_ONOFF) != (UINT32)0)
		{
			if((((sAeInfo.bOnOff & MCDRV_EQ3_ON) != 0) && ((bReg & MCB_EQ3ON) != 0))
			|| (((sAeInfo.bOnOff & MCDRV_EQ3_ON) == 0) && ((bReg & MCB_EQ3ON) == 0)))
			{
				dUpdateInfo	&= ~MCDRV_AEUPDATE_FLAG_EQ3_ONOFF;
			}
		}
		if(dUpdateInfo != (UINT32)0)
		{
			/*	on/off setting or param changed	*/
			if(((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEX) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_WIDE) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEXWIDE_ONOFF) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC_ONOFF) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5_ONOFF) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3_ONOFF) != (UINT32)0))
			{
				dXFadeParam	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DAC_INS);
				dXFadeParam	<<= 8;
				dXFadeParam	|= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_INS);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DAC_INS, 0);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_INS, 0);
			}
			sdRet	= McDevIf_ExecutePacket();
			if(sdRet == MCDRV_SUCCESS)
			{
				/*	wait xfade complete	*/
				if(dXFadeParam != (UINT32)0)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_INSFLG | dXFadeParam, 0);
					bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_BDSP_ST);
					if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5) != (UINT32)0)
					{
						bReg	&= (UINT8)~MCB_EQ5ON;
					}
					if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3) != (UINT32)0)
					{
						bReg	&= (UINT8)~MCB_EQ3ON;
					}
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ST, bReg);
				}

				bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_A, MCI_BDSP_ST);
				if((bReg & MCB_BDSP_ST) == MCB_BDSP_ST)
				{
					/*	Stop BDSP	*/
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ST, 0);
				}
			}
		}
	}

	if((dUpdateInfo != (UINT32)0) && (sdRet == MCDRV_SUCCESS))
	{
		McResCtrl_GetCurPowerInfo(&sCurPowerInfo);
		if(((McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0) && ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEX) != (UINT32)0))
		|| ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC) != (UINT32)0)
		|| ((McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0) && ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_WIDE) != (UINT32)0)))
		{
			McResCtrl_GetPowerInfo(&sPowerInfo);
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP0;
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP1;
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP2;
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DPBDSP;
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_PLLRST0;
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
			if(MCDRV_SUCCESS == sdRet)
			{
				sdRet	= McDevIf_ExecutePacket();
			}
			if(MCDRV_SUCCESS == sdRet)
			{
				dwDRCDataSize	= McDevProf_GetDRCDataSize();
				if((McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0) && ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEX) != (UINT32)0))
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ADR, McDevProf_GetBDSPAddr(MCDRV_AEUPDATE_FLAG_BEX));
					McDevIf_AddPacketRepeat(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_WINDOW, sAeInfo.abBex, BEX_PARAM_SIZE);
				}
				else
				{
				}
				if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC) != (UINT32)0)
				{
					if((McDevProf_IsValid(eMCDRV_FUNC_DBEX) == 0) || ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEX) == (UINT32)0))
					{
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ADR, McDevProf_GetBDSPAddr(MCDRV_AEUPDATE_FLAG_DRC));
						sdRet	= McDevIf_ExecutePacket();
					}
					if(MCDRV_SUCCESS == sdRet)
					{
						if(McDevProf_GetDevId() != eMCDRV_DEV_ID_46H)
						{
							McDevIf_AddPacketRepeat(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_WINDOW, sAeInfo.abDrc, (UINT16)dwDRCDataSize);
						}
						else
						{
							McDevIf_AddPacketRepeat(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_WINDOW, sAeInfo.abDrc2, (UINT16)dwDRCDataSize);
						}
					}
				}
				if((MCDRV_SUCCESS == sdRet) && (McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0) && ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_WIDE) != (UINT32)0))
				{
					if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC) == (UINT32)0)
					{
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_ADR, McDevProf_GetBDSPAddr(MCDRV_AEUPDATE_FLAG_WIDE));
					}
					McDevIf_AddPacketRepeat(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_BDSP_WINDOW, sAeInfo.abWide, WIDE_PARAM_SIZE);
				}
			}
			if(MCDRV_SUCCESS == sdRet)
			{
				sdRet	= McDevIf_ExecutePacket();
			}
		}

		if(MCDRV_SUCCESS == sdRet)
		{
			if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5) != (UINT32)0)
			{
				for(i = 0; i < EQ5_PARAM_SIZE; i++)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)(MCI_BAND0_CEQ0+i), sAeInfo.abEq5[i]);
				}
			}
			if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3) != (UINT32)0)
			{
				for(i = 0; i < EQ3_PARAM_SIZE; i++)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_AE | (UINT32)(MCI_BAND5_CEQ0+i), sAeInfo.abEq3[i]);
				}
			}

			/*	unused path power down	*/
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerDown(&sCurPowerInfo, &sPowerUpdate);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddAE", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddAeEx
 *
 *	Description:
 *			Add Audio Engine Ex setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddAeEx
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddAeEx");
#endif


#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddAeEx", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddPDM
 *
 *	Description:
 *			Add PDM setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddPDM
(
	UINT32			dUpdateInfo
)
{
	SINT32				sdRet		= MCDRV_SUCCESS;
	UINT8				bReg;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_PDM_INFO		sPdmInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddPDM");
#endif

	McResCtrl_GetPdmInfo(&sPdmInfo);
	if(((dUpdateInfo & MCDRV_PDMADJ_UPDATE_FLAG) != (UINT32)0) || ((dUpdateInfo & MCDRV_PDMAGC_UPDATE_FLAG) != (UINT32)0))
	{
		bReg	= (sPdmInfo.bAgcOn<<2)|sPdmInfo.bAgcAdjust;
		if(bReg == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC))
		{
			dUpdateInfo &= ~(MCDRV_PDMADJ_UPDATE_FLAG|MCDRV_PDMAGC_UPDATE_FLAG);
		}
	}
	if((dUpdateInfo & MCDRV_PDMMONO_UPDATE_FLAG) != (UINT32)0)
	{
		bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START) & (UINT8)MCB_PDM_MN);
		if((sPdmInfo.bMono<<1) == bReg)
		{
			dUpdateInfo &= ~(MCDRV_PDMMONO_UPDATE_FLAG);
		}
	}
	if(((dUpdateInfo & MCDRV_PDMCLK_UPDATE_FLAG) != (UINT32)0)
	|| ((dUpdateInfo & MCDRV_PDMEDGE_UPDATE_FLAG) != (UINT32)0)
	|| ((dUpdateInfo & MCDRV_PDMWAIT_UPDATE_FLAG) != (UINT32)0)
	|| ((dUpdateInfo & MCDRV_PDMSEL_UPDATE_FLAG) != (UINT32)0))
	{
		bReg	= (sPdmInfo.bPdmWait<<5) | (sPdmInfo.bPdmEdge<<4) | (sPdmInfo.bPdmSel<<2) | sPdmInfo.bClk;
		if(bReg == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_STWAIT))
		{
			dUpdateInfo &= ~(MCDRV_PDMCLK_UPDATE_FLAG|MCDRV_PDMEDGE_UPDATE_FLAG|MCDRV_PDMWAIT_UPDATE_FLAG|MCDRV_PDMSEL_UPDATE_FLAG);
		}
	}
	if(dUpdateInfo != (UINT32)0)
	{
		McResCtrl_GetCurPowerInfo(&sPowerInfo);
		sdRet	= PowerUpDig(MCDRV_DPB_UP);
		if(sdRet == MCDRV_SUCCESS)
		{
			if(((dUpdateInfo & MCDRV_PDMADJ_UPDATE_FLAG) != (UINT32)0) || ((dUpdateInfo & MCDRV_PDMAGC_UPDATE_FLAG) != (UINT32)0))
			{
				bReg	= (sPdmInfo.bAgcOn<<2)|sPdmInfo.bAgcAdjust;
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC))
				{
					AddStopPDM();
					sdRet	= McDevIf_ExecutePacket();
				}
				if(sdRet == MCDRV_SUCCESS)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_AGC, bReg);
				}
			}
		}
		if(sdRet == MCDRV_SUCCESS)
		{
			if((dUpdateInfo & MCDRV_PDMMONO_UPDATE_FLAG) != (UINT32)0)
			{
				bReg	= (UINT8)((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START) & (UINT8)~MCB_PDM_MN) | (sPdmInfo.bMono<<1));
				if(bReg != McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START))
				{
					if((bReg&MCB_PDM_START) != 0)
					{
						AddStopPDM();
						sdRet	= McDevIf_ExecutePacket();
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_PDMMUTE | ((MCB_PDM0_VFLAGL<<8)|MCB_PDM0_VFLAGR), 0);
					}
					if(MCDRV_SUCCESS == sdRet)
					{
						bReg	= (UINT8)((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_START) & (UINT8)~MCB_PDM_MN) | (sPdmInfo.bMono<<1));
						McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_START, bReg);
					}
				}
			}
		}
		if(sdRet == MCDRV_SUCCESS)
		{
			if(((dUpdateInfo & MCDRV_PDMCLK_UPDATE_FLAG) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_PDMEDGE_UPDATE_FLAG) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_PDMWAIT_UPDATE_FLAG) != (UINT32)0)
			|| ((dUpdateInfo & MCDRV_PDMSEL_UPDATE_FLAG) != (UINT32)0))
			{
				bReg	= (sPdmInfo.bPdmWait<<5) | (sPdmInfo.bPdmEdge<<4) | (sPdmInfo.bPdmSel<<2) | sPdmInfo.bClk;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_STWAIT, bReg);
			}

			/*	unused path power down	*/
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddPDM", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddPDMEx
 *
 *	Description:
 *			Add PDM_EX setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddPDMEx
(
	UINT32			dUpdateInfo
)
{
	SINT32				sdRet	= MCDRV_SUCCESS;
	UINT8				bReg;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_PDM_EX_INFO	sPdmExInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddPDMEx");
#endif

	McResCtrl_GetPdmExInfo(&sPdmExInfo);

	if((dUpdateInfo & MCDRV_AGCENV_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC_ENV);
		if((bReg&MCB_PDM_AGC_ENV) == sPdmExInfo.bAgcEnv)
		{
			dUpdateInfo	&= ~MCDRV_AGCENV_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCLVL_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC_ENV);
		if((bReg&MCB_PDM_AGC_LVL) == (sPdmExInfo.bAgcLvl<<2))
		{
			dUpdateInfo	&= ~MCDRV_AGCLVL_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCUPTIM_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC_ENV);
		if((bReg&MCB_PDM_AGC_UPTIM) == (sPdmExInfo.bAgcUpTim<<4))
		{
			dUpdateInfo	&= ~MCDRV_AGCUPTIM_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCDWTIM_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC_ENV);
		if((bReg&MCB_PDM_AGC_DWTIM) == (sPdmExInfo.bAgcDwTim<<6))
		{
			dUpdateInfo	&= ~MCDRV_AGCDWTIM_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTLVL_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC_CUT);
		if((bReg&MCB_PDM_AGC_CUTLVL) == sPdmExInfo.bAgcCutLvl)
		{
			dUpdateInfo	&= ~MCDRV_AGCCUTLVL_UPDATE_FLAG;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTTIM_UPDATE_FLAG) != 0UL)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_PDM_AGC_CUT);
		if((bReg&MCB_PDM_AGC_CUTTIM) == (sPdmExInfo.bAgcCutTim<<2))
		{
			dUpdateInfo	&= ~MCDRV_AGCCUTTIM_UPDATE_FLAG;
		}
	}

	if(dUpdateInfo != 0UL)
	{
		McResCtrl_GetCurPowerInfo(&sPowerInfo);
		if((sPowerInfo.dDigital & MCDRV_POWINFO_DIGITAL_DPB) == 0UL)
		{
			AddStopPDM();
			sdRet	= McDevIf_ExecutePacket();
			if(sdRet == MCDRV_SUCCESS)
			{
				bReg	= (sPdmExInfo.bAgcDwTim<<6) | (sPdmExInfo.bAgcUpTim<<4) | (sPdmExInfo.bAgcLvl<<2) | sPdmExInfo.bAgcEnv;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_AGC_ENV, bReg);

				bReg	= (sPdmExInfo.bAgcCutTim<<2) | sPdmExInfo.bAgcCutLvl;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_PDM_AGC_CUT, bReg);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddPDMEx", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddDTMF
 *
 *	Description:
 *			Add DTMF setup packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McPacket_AddDTMF
(
	UINT32	dUpdateInfo
)
{
	SINT32				sdRet	= MCDRV_SUCCESS;
	UINT8				bReg;
	UINT8				bVol0	= 0,
						bVol1	= 0;
	UINT16				wFreq0	= 0,
						wFreq1	= 0;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_DTMF_INFO		sDtmfInfo;

	if(McDevProf_IsValid(eMCDRV_FUNC_DTMF) == 0)
	{
		return MCDRV_ERROR;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddDTMF");
#endif

	McResCtrl_GetDtmfInfo(&sDtmfInfo);
	if((dUpdateInfo & MCDRV_DTMFHOST_UPDATE_FLAG) != 0UL)
	{
		bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FLAG) & MCB_SINGEN_MODE);
		bReg	>>= 1;
		if(bReg == sDtmfInfo.bSinGenHost)
		{
			dUpdateInfo &= ~(MCDRV_DTMFHOST_UPDATE_FLAG);
		}
		else if(sDtmfInfo.bSinGenHost != MCDRV_DTMFHOST_REG)
		{
			/*dUpdateInfo |= MCDRV_DTMFONOFF_UPDATE_FLAG;*/
		}
		else
		{
		}
	}

	if(sDtmfInfo.bOnOff == MCDRV_DTMF_ON)
	{
		if((dUpdateInfo & MCDRV_DTMFFREQ0_UPDATE_FLAG) != 0UL)
		{
			wFreq0	= GetSinGenFreqReg(sDtmfInfo.sParam.wSinGen0Freq);
			if(((wFreq0 >> 8) == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FREQ0_MSB))
			&& ((wFreq0 & 0xFF) == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FREQ0_LSB)))
			{
				dUpdateInfo &= ~MCDRV_DTMFFREQ0_UPDATE_FLAG;
			}
		}
		if((dUpdateInfo & MCDRV_DTMFFREQ1_UPDATE_FLAG) != 0UL)
		{
			wFreq1	= GetSinGenFreqReg(sDtmfInfo.sParam.wSinGen1Freq);
			if(((wFreq1 >> 8) == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FREQ1_MSB))
			&& ((wFreq1 & 0xFF) == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FREQ1_LSB)))
			{
				dUpdateInfo &= ~MCDRV_DTMFFREQ1_UPDATE_FLAG;
			}
		}

		if((dUpdateInfo & MCDRV_DTMFVOL0_UPDATE_FLAG) != 0UL)
		{
			bVol0	= (UINT8)McResCtrl_GetDigitalVolReg(sDtmfInfo.sParam.swSinGen0Vol);
			if(bVol0 == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN0_VOL))
			{
				dUpdateInfo &= ~MCDRV_DTMFVOL0_UPDATE_FLAG;
			}
		}
		if((dUpdateInfo & MCDRV_DTMFVOL1_UPDATE_FLAG) != 0UL)
		{
			bVol1	= (UINT8)McResCtrl_GetDigitalVolReg(sDtmfInfo.sParam.swSinGen1Vol);
			if(bVol1 == McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN1_VOL))
			{
				dUpdateInfo &= ~MCDRV_DTMFVOL1_UPDATE_FLAG;
			}
		}

		if((dUpdateInfo & MCDRV_DTMFGATE_UPDATE_FLAG) != 0UL)
		{
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_GATETIME);
			if(bReg == sDtmfInfo.sParam.bSinGenGate)
			{
				dUpdateInfo &= ~MCDRV_DTMFGATE_UPDATE_FLAG;
			}
		}

		if((dUpdateInfo & MCDRV_DTMFLOOP_UPDATE_FLAG) != 0UL)
		{
			bReg	= (UINT8)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FLAG) & MCB_SINGEN_LOOP);
			if(bReg == sDtmfInfo.sParam.bSinGenLoop)
			{
				dUpdateInfo &= ~(MCDRV_DTMFLOOP_UPDATE_FLAG);
			}
		}
	}
	else
	{
		dUpdateInfo &= ~MCDRV_DTMFFREQ0_UPDATE_FLAG;
		dUpdateInfo &= ~MCDRV_DTMFFREQ1_UPDATE_FLAG;
		dUpdateInfo &= ~MCDRV_DTMFVOL0_UPDATE_FLAG;
		dUpdateInfo &= ~MCDRV_DTMFVOL1_UPDATE_FLAG;
		dUpdateInfo &= ~MCDRV_DTMFGATE_UPDATE_FLAG;
		dUpdateInfo &= ~(MCDRV_DTMFLOOP_UPDATE_FLAG);
	}

	if(dUpdateInfo != (UINT32)0)
	{
		McResCtrl_GetCurPowerInfo(&sPowerInfo);
		sdRet	= PowerUpDig(MCDRV_DPB_UP);
		if(sdRet == MCDRV_SUCCESS)
		{
			if((dUpdateInfo & MCDRV_DTMFVOL0_UPDATE_FLAG) != 0UL)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN0_VOL, bVol0);
			}
			if((dUpdateInfo & MCDRV_DTMFVOL1_UPDATE_FLAG) != 0UL)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN1_VOL, bVol1);
			}
			if((dUpdateInfo & MCDRV_DTMFFREQ0_UPDATE_FLAG) != 0UL)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FREQ0_MSB, (UINT8)(wFreq0 >> 8));
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FREQ0_LSB, (UINT8)(wFreq0 & 0xFF));
			}
			if((dUpdateInfo & MCDRV_DTMFFREQ1_UPDATE_FLAG) != 0UL)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FREQ1_MSB, (UINT8)(wFreq1 >> 8));
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FREQ1_LSB, (UINT8)(wFreq1 & 0xFF));
			}
			if((dUpdateInfo & MCDRV_DTMFGATE_UPDATE_FLAG) != 0UL)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_GATETIME, sDtmfInfo.sParam.bSinGenGate);
			}
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_SINGEN_FLAG);
			if((dUpdateInfo & MCDRV_DTMFLOOP_UPDATE_FLAG) != 0UL)
			{
				bReg	&= (UINT8)~MCB_SINGEN_LOOP;
				bReg	|= (sDtmfInfo.sParam.bSinGenLoop<<6);
			}
			if((dUpdateInfo & MCDRV_DTMFHOST_UPDATE_FLAG) != 0UL)
			{
				bReg	&= (UINT8)~(MCB_SINGEN_MODE|MCB_SINGEN_ON);
				bReg	|= (sDtmfInfo.bSinGenHost<<1);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FLAG, bReg);
			}
			if((dUpdateInfo & MCDRV_DTMFONOFF_UPDATE_FLAG) != 0UL)
			{
				if((bReg&MCB_SINGEN_MODE) == 0)
				{
					bReg	&= (UINT8)~MCB_SINGEN_ON;
					bReg	|= sDtmfInfo.bOnOff;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FLAG, bReg);
				}
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_SINGEN_FLAG, bReg);

			/*	unused path power down	*/
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddDTMF", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	GetSinGenFreqReg
 *
 *	Description:
 *			Get value of DTMF frequency registers.
 *	Arguments:
 *			wFreq	frequency
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static UINT16	GetSinGenFreqReg
(
	UINT16	wFreq
)
{
	UINT32	dRet, dTemp;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("GetSinGenFreqReg");
#endif

	dRet	= ((UINT32)wFreq << 18) / 73728UL;
	dTemp	= (dRet * 536UL) / 1000UL;
	dRet	+= dTemp;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("GetSinGenFreqReg", (SINT32*)&dRet);
#endif
	return (UINT16)dRet;
}

/****************************************************************************
 *	McPacket_AddGPMode
 *
 *	Description:
 *			Add GP mode setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddGPMode
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_GP_MODE	sGPMode;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddGPMode");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetGPMode(&sGPMode);

	/*	EGP*_IRQ	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_IRQ) != 0)
	{
		bReg	= 0;
		if((sInitInfo.bPad2Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[2] == MCDRV_GPDDR_IN))
		{
			bReg	|= (sGPMode.abGpIrq[2]<<2);
		}
		if((sInitInfo.bPad1Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[1] == MCDRV_GPDDR_IN))
		{
			bReg	|= (sGPMode.abGpIrq[1]<<1);
		}
		if((sInitInfo.bPad0Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[0] == MCDRV_GPDDR_IN))
		{
			bReg	|= sGPMode.abGpIrq[0];
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_EPA_IRQ, bReg);
	}

	/*	DDR, IRQMODE	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA2_MSK);
		if(sInitInfo.bPad2Func == MCDRV_PAD_GPIO)
		{
			if(sGPMode.abGpDdr[2] == MCDRV_GPDDR_IN)
			{
				bReg	&= (UINT8)~MCB_PA2_DDR;
				bReg	&= (UINT8)~MCB_PA2_IRQMODE;
				bReg	|= sGPMode.abGpMode[2];
			}
			else
			{
				bReg	|= MCB_PA2_DDR;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_MSK, bReg);
		}
	}

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA_MSK);
	if(sInitInfo.bPad0Func == MCDRV_PAD_GPIO)
	{
		if(sGPMode.abGpDdr[0] == MCDRV_GPDDR_IN)
		{
			bReg	&= (UINT8)~MCB_PA0_DDR;
			if(McDevProf_IsValid(eMCDRV_FUNC_GPMODE) != 0)
			{
				bReg	&= (UINT8)~MCB_PA0_IRQMODE;
				bReg	|= sGPMode.abGpMode[0];
			}
		}
		else
		{
			bReg	|= MCB_PA0_DDR;
		}
	}
	if(sInitInfo.bPad1Func == MCDRV_PAD_GPIO)
	{
		if(sGPMode.abGpDdr[1] == MCDRV_GPDDR_IN)
		{
			bReg	&= (UINT8)~MCB_PA1_DDR;
			if(McDevProf_IsValid(eMCDRV_FUNC_GPMODE) != 0)
			{
				bReg	&= (UINT8)~MCB_PA1_IRQMODE;
				bReg	|= (sGPMode.abGpMode[1]<<4);
			}
		}
		else
		{
			bReg	|= MCB_PA1_DDR;
		}
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_MSK, bReg);

	if(McDevProf_IsValid(eMCDRV_FUNC_GPMODE) != 0)
	{
		/*	HOST	*/
		if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
		{
			if(sInitInfo.bPad2Func == MCDRV_PAD_GPIO)
			{
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_HOST, sGPMode.abGpHost[2]);
			}
		}

		bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA_HOST);
		if(sInitInfo.bPad1Func == MCDRV_PAD_GPIO)
		{
			bReg	&= (UINT8)~MCB_PA1_HOST;
			bReg	|= (sGPMode.abGpHost[1]<<4);
		}
		if(sInitInfo.bPad0Func == MCDRV_PAD_GPIO)
		{
			bReg	&= (UINT8)~MCB_PA0_HOST;
			bReg	|= sGPMode.abGpHost[0];
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_HOST, bReg);

		/*	GP*P	*/
		bReg	= 0;
		if(sInitInfo.bPad2Func == MCDRV_PAD_GPIO)
		{
			bReg	|= (sGPMode.abGpInvert[2]<<2);
		}
		if(sInitInfo.bPad1Func == MCDRV_PAD_GPIO)
		{
			bReg	|= (sGPMode.abGpInvert[1]<<1);
		}
		if(sInitInfo.bPad0Func == MCDRV_PAD_GPIO)
		{
			bReg |= sGPMode.abGpInvert[0];
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_GPP, bReg);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddGPMode", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddGPMask
 *
 *	Description:
 *			Add GP mask setup packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR
 *
 ****************************************************************************/
SINT32	McPacket_AddGPMask
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_GP_MODE	sGPMode;
	UINT8	abMask[GPIO_PAD_NUM];

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddGPMask");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetGPMode(&sGPMode);
	McResCtrl_GetGPMask(abMask);

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA_MSK);
	if((sInitInfo.bPad0Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[0] == MCDRV_GPDDR_IN))
	{
		if(abMask[0] == MCDRV_GPMASK_OFF)
		{
			bReg	&= (UINT8)~MCB_PA0_MSK;
		}
		else
		{
			bReg	|= MCB_PA0_MSK;
		}
	}
	if((sInitInfo.bPad1Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[1] == MCDRV_GPDDR_IN))
	{
		if(abMask[1] == MCDRV_GPMASK_OFF)
		{
			bReg	&= (UINT8)~MCB_PA1_MSK;
		}
		else
		{
			bReg	|= MCB_PA1_MSK;
		}
	}
	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA_MSK, bReg);

	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		if((sInitInfo.bPad2Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[2] == MCDRV_GPDDR_IN))
		{
			bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_BASE, MCI_PA2_MSK);
			if(abMask[2] == MCDRV_GPMASK_OFF)
			{
				bReg	&= (UINT8)~MCB_PA2_MSK;
			}
			else
			{
				bReg	|= MCB_PA2_MSK;
			}
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)MCI_PA2_MSK, bReg);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddGPMask", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddGPSet
 *
 *	Description:
 *			Add GPIO output packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR
 *
 ****************************************************************************/
SINT32	McPacket_AddGPSet
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	UINT8	bAddr;
	MCDRV_INIT_INFO	sInitInfo;
	MCDRV_GP_MODE	sGPMode;
	UINT8	abPad[GPIO_PAD_NUM];

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddGPSet");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetGPMode(&sGPMode);
	McResCtrl_GetGPPad(abPad);

	if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
	{
		bAddr	= MCI_SCU_GP;
	}
	else
	{
		bAddr	= MCI_PA_SCU_PA;
	}

	bReg	= 0;

	if((sInitInfo.bPad0Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[0] == MCDRV_GPDDR_OUT) && (sGPMode.abGpHost[0] == MCDRV_GPHOST_SCU))
	{
		bReg	|= abPad[0];
	}
	if((sInitInfo.bPad1Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[1] == MCDRV_GPDDR_OUT) && (sGPMode.abGpHost[1] == MCDRV_GPHOST_SCU))
	{
		bReg	|= (UINT8)(abPad[1] << 1);
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		if((sInitInfo.bPad2Func == MCDRV_PAD_GPIO) && (sGPMode.abGpDdr[2] == MCDRV_GPDDR_OUT) && (sGPMode.abGpHost[2] == MCDRV_GPHOST_SCU))
		{
			bReg	|= (UINT8)(abPad[2] << 2);
		}
	}

	McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_BASE | (UINT32)bAddr, bReg);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddGPSet", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddSysEq
 *
 *	Description:
 *			Add GPIO output packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddSysEq
(
	UINT32	dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bRegEQOn;
	UINT8	bReg;
	MCDRV_SYSEQ_INFO	sSysEq;
	UINT8	i;
	MCDRV_INIT_INFO		sInitInfo;
	MCDRV_POWER_INFO	sCurPowerInfo, sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_DEV_ID		eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddSysEq");
#endif

	bRegEQOn	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_SYSTEM_EQON);

	McResCtrl_GetInitInfo(&sInitInfo);
	McResCtrl_GetSysEq(&sSysEq);

	if(((dUpdateInfo & MCDRV_SYSEQ_PARAM_UPDATE_FLAG) != 0UL)
	|| (((dUpdateInfo & MCDRV_SYSEQ_ONOFF_UPDATE_FLAG) != 0UL) && ((bRegEQOn&MCB_SYSTEM_EQON) != sSysEq.bOnOff)))
	{
		McResCtrl_GetCurPowerInfo(&sCurPowerInfo);
		sPowerInfo	= sCurPowerInfo;
		sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP0;
		sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP1;
		sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP2;
		sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_PLLRST0;
		sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DPCD;
		if(eDevID == eMCDRV_DEV_ID_79H || eDevID == eMCDRV_DEV_ID_71H)
		{
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DPB;
		}
		sPowerUpdate.dDigital	= MCDRV_POWUPDATE_DIGITAL_ALL;
		sPowerInfo.abAnalog[0] &= (UINT8)~MCB_PWM_VR;
		if ((MCDRV_LDO_AOFF_DON == sInitInfo.bLdo) || (MCDRV_LDO_AOFF_DOFF == sInitInfo.bLdo))
		{
			sPowerInfo.abAnalog[0] &= (UINT8)~MCB_PWM_REFA;
		}
		else
		{
			sPowerInfo.abAnalog[0] &= (UINT8)~MCB_PWM_LDOA;
		}
		sPowerInfo.abAnalog[3] &= (UINT8)~MCB_PWM_DAL;
		sPowerUpdate.abAnalog[0]	= MCDRV_POWUPDATE_ANALOG0_ALL;
		sPowerUpdate.abAnalog[3]	= MCDRV_POWUPDATE_ANALOG3_ALL;
		sPowerUpdate.abAnalog[1]	= 
		sPowerUpdate.abAnalog[2]	= 
		sPowerUpdate.abAnalog[4]	= 
		sPowerUpdate.abAnalog[5]	= 0;
		sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
		if(MCDRV_SUCCESS == sdRet)
		{
			sdRet	= McDevIf_ExecutePacket();
		}
		if(sdRet == MCDRV_SUCCESS)
		{
			if((dUpdateInfo & MCDRV_SYSEQ_PARAM_UPDATE_FLAG) != 0UL)
			{
				if((bRegEQOn & MCB_SYSTEM_EQON) != 0)
				{/*	EQ on	*/
					/*	EQ off	*/
					bReg	= bRegEQOn;
					bReg	&= (UINT8)~MCB_SYSTEM_EQON;
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_SYSTEM_EQON, bReg);
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_SYSEQ_FLAG_RESET, 0);
				}
				/*	EQ coef	*/
				for(i = 0; i < sizeof(sSysEq.abParam); i++)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)(MCI_SYS_CEQ0_19_12+i), sSysEq.abParam[i]);
				}
			}

			if((dUpdateInfo & MCDRV_SYSEQ_ONOFF_UPDATE_FLAG) != 0UL)
			{
				bRegEQOn	&= (UINT8)~MCB_SYSTEM_EQON;
				bRegEQOn	|= sSysEq.bOnOff;
			}

			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_SYSTEM_EQON, bRegEQOn);

			/*	unused path power down	*/
			sdRet	= McPacket_AddPowerDown(&sCurPowerInfo, &sPowerUpdate);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddSysEq", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McPacket_AddClockSwitch
 *
 *	Description:
 *			Add switch clock packet.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddClockSwitch
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_CLKSW_INFO	sClockInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddClockSwitch");
#endif

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_CODEC, MCI_CD_DP);

	McResCtrl_GetClockSwitch(&sClockInfo);

	if((bReg & (MCB_DP0_CLKI1 | MCB_DP0_CLKI0)) != (MCB_DP0_CLKI1 | MCB_DP0_CLKI0))
	{
		if(sClockInfo.bClkSrc == MCDRV_CLKSW_CLKI0)
		{
			if((bReg & MCB_DP0_CLKI1) == 0)
			{/*	CLKI1->CLKI0	*/
				bReg	&= (UINT8)~MCB_DP0_CLKI0;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
				if((bReg & MCB_CLKINPUT) != 0)
				{
					bReg	|= MCB_CLKSRC;
				}
				else
				{
					bReg	&= (UINT8)~MCB_CLKSRC;
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CLKBUSY_RESET, 0);
				if((bReg & MCB_CLKSRC) != 0)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CLKSRC_SET, 0);
				}
				else
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CLKSRC_RESET, 0);
				}
				bReg	|= MCB_DP0_CLKI1;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
			}
		}
		else
		{
			if((bReg & MCB_DP0_CLKI0) == 0)
			{/*	CLKI0->CLKI1	*/
				bReg	&= (UINT8)~MCB_DP0_CLKI1;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
				if((bReg & MCB_CLKINPUT) == 0)
				{
					bReg	|= MCB_CLKSRC;
				}
				else
				{
					bReg	&= (UINT8)~MCB_CLKSRC;
				}
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CLKBUSY_RESET, 0);
				if((bReg & MCB_CLKSRC) != 0)
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CLKSRC_SET, 0);
				}
				else
				{
					McDevIf_AddPacket(MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CLKSRC_RESET, 0);
				}
				bReg	|= MCB_DP0_CLKI0;
				McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
			}
		}
	}
	else
	{
		if(sClockInfo.bClkSrc == MCDRV_CLKSW_CLKI0)
		{
			if((bReg & MCB_CLKSRC) != 0)
			{
				bReg	|= MCB_CLKINPUT;
			}
			else
			{
				bReg	&= (UINT8)~MCB_CLKINPUT;
			}
		}
		else
		{
			if((bReg & MCB_CLKSRC) == 0)
			{
				bReg	|= MCB_CLKINPUT;
			}
			else
			{
				bReg	&= (UINT8)~MCB_CLKINPUT;
			}
		}
		McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CODEC | (UINT32)MCI_CD_DP, bReg);
		sdRet	= McDevIf_ExecutePacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddClockSwitch", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McPacket_AddDitSwap
 *
 *	Description:
 *			Add DIT Swap packet.
 *	Arguments:
 *			dUpdateInfo	update information
 *	Return:
 *			MCDRV_SUCCESS
 *
 ****************************************************************************/
SINT32	McPacket_AddDitSwap
(
	UINT32	dUpdateInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bReg;
	MCDRV_DITSWAP_INFO	sDitSwapInfo;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McPacket_AddDitSwap");
#endif

	bReg	= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_DIT2SRC_SOURCE);

	McResCtrl_GetDitSwap(&sDitSwapInfo);

	if((dUpdateInfo & MCDRV_DITSWAP_L_UPDATE_FLAG) != 0UL)
	{
		if((bReg&MCB_DIT2SWAPL) == (sDitSwapInfo.bSwapL<<7))
		{
			dUpdateInfo	&= ~MCDRV_DITSWAP_L_UPDATE_FLAG;
		}
		else
		{
			bReg	= (UINT8)(bReg & (UINT8)~MCB_DIT2SWAPL) | (sDitSwapInfo.bSwapL<<7);
		}
	}
	if((dUpdateInfo & MCDRV_DITSWAP_R_UPDATE_FLAG) != 0UL)
	{
		if((bReg&MCB_DIT2SWAPR) == (sDitSwapInfo.bSwapR<<3))
		{
			dUpdateInfo	&= ~MCDRV_DITSWAP_R_UPDATE_FLAG;
		}
		else
		{
			bReg	= (UINT8)(bReg & (UINT8)~MCB_DIT2SWAPR) | (sDitSwapInfo.bSwapR<<3);
		}
	}

	if(dUpdateInfo != 0UL)
	{
		McResCtrl_GetCurPowerInfo(&sPowerInfo);
		sdRet	= PowerUpDig(MCDRV_DPB_UP);
		if(sdRet == MCDRV_SUCCESS)
		{
			McDevIf_AddPacket(MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_MIXER | (UINT32)MCI_DIT2SRC_SOURCE, bReg);
			/*	unused path power down	*/
			sPowerUpdate.dDigital		= MCDRV_POWUPDATE_DIGITAL_ALL;
			sPowerUpdate.abAnalog[0]	= 
			sPowerUpdate.abAnalog[1]	= 
			sPowerUpdate.abAnalog[2]	= 
			sPowerUpdate.abAnalog[3]	= 
			sPowerUpdate.abAnalog[4]	= 
			sPowerUpdate.abAnalog[5]	= 0;
			sdRet	= McPacket_AddPowerDown(&sPowerInfo, &sPowerUpdate);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McPacket_AddDitSwap", &sdRet);
#endif

	return sdRet;
}


/****************************************************************************
 *	PowerUpDig
 *
 *	Description:
 *			Digital power up.
 *	Arguments:
 *			bDPBUp		1:DPB power up
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_RESOURCEOVER
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	PowerUpDig
(
	UINT8	bDPBUp
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_POWER_INFO	sPowerInfo;
	MCDRV_POWER_UPDATE	sPowerUpdate;
	MCDRV_INIT_INFO	sInitInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("PowerUpDig");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);

	McResCtrl_GetCurPowerInfo(&sPowerInfo);
	sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP0;
	sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP1;
	sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DP2;
	sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_PLLRST0;
	if(bDPBUp == MCDRV_DPB_UP)
	{
		if(((McDevProf_GetDevId() != eMCDRV_DEV_ID_46H) && (McDevProf_GetDevId() != eMCDRV_DEV_ID_44H))
		|| (sInitInfo.bPowerMode != MCDRV_POWMODE_FULL))
		{
			sPowerInfo.dDigital	&= ~MCDRV_POWINFO_DIGITAL_DPB;
		}
	}
	sPowerUpdate.dDigital	= MCDRV_POWUPDATE_DIGITAL_ALL;
	sdRet	= McPacket_AddPowerUp(&sPowerInfo, &sPowerUpdate);
	if(MCDRV_SUCCESS == sdRet)
	{
		sdRet	= McDevIf_ExecutePacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("PowerUpDig", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	GetMaxWait
 *
 *	Description:
 *			Get maximum wait time.
 *	Arguments:
 *			bRegChange	analog power management register update information
 *	Return:
 *			wait time
 *
 ****************************************************************************/
static UINT32	GetMaxWait
(
	UINT8	bRegChange
)
{
	UINT32	dWaitTimeMax	= 0;
	MCDRV_INIT_INFO	sInitInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetMaxWait");
#endif

	McResCtrl_GetInitInfo(&sInitInfo);

	if((bRegChange & MCB_PWM_LI) != 0)
	{
		if(sInitInfo.sWaitTime.dLine1Cin > dWaitTimeMax)
		{
			dWaitTimeMax	= sInitInfo.sWaitTime.dLine1Cin;
		}
	}
	if((bRegChange & MCDRV_REGCHANGE_PWM_LI2) != 0)
	{
		if(sInitInfo.sWaitTime.dLine2Cin > dWaitTimeMax)
		{
			dWaitTimeMax	= sInitInfo.sWaitTime.dLine2Cin;
		}
	}
	if(((bRegChange & MCB_PWM_MB1) != 0) || ((bRegChange & MCB_PWM_MC1) != 0))
	{
		if(sInitInfo.sWaitTime.dMic1Cin > dWaitTimeMax)
		{
			dWaitTimeMax	= sInitInfo.sWaitTime.dMic1Cin;
		}
	}
	if(((bRegChange & MCB_PWM_MB2) != 0) || ((bRegChange & MCB_PWM_MC2) != 0))
	{
		if(sInitInfo.sWaitTime.dMic2Cin > dWaitTimeMax)
		{
			dWaitTimeMax	= sInitInfo.sWaitTime.dMic2Cin;
		}
	}
	if(((bRegChange & MCB_PWM_MB3) != 0) || ((bRegChange & MCB_PWM_MC3) != 0))
	{
		if(sInitInfo.sWaitTime.dMic3Cin > dWaitTimeMax)
		{
			dWaitTimeMax	= sInitInfo.sWaitTime.dMic3Cin;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)dWaitTimeMax;
	McDebugLog_FuncOut("GetMaxWait", &sdRet);
#endif

	return dWaitTimeMax;
}

