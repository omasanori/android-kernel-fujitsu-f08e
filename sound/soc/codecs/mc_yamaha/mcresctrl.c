/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcresctrl.c
 *
 *		Description	: MC Driver resource control
 *
 *		Version		: 2.0.2 	2011.08.30
 *
 ****************************************************************************/


#include "mcresctrl.h"
#include "mcdevif.h"
#include "mcservice.h"
#include "mcdriver.h"
#include "mcdefs.h"
#include "mcdevprof.h"
#include "mcmachdep.h"
#if MCDRV_DEBUG_LEVEL
#include "mcdebuglog.h"
#endif



/* wait time */
#define	MCDRV_INTERVAL_MUTE		(1000)
#define	MCDRV_TIME_OUT_MUTE		(1000)
#define	MCDRV_INTERVAL_CDSPFLG	(1000)
#define	MCDRV_TIME_OUT_CDSPFLG	(1000)

/*	C-DSP Init value	*/
#define	MCDRV_CDSP_FS_DEF		(0)
#define	MCDRV_CDSP_RATE_DEF		(100)

static MCDRV_STATE	geState	= eMCDRV_STATE_NOTINIT;

static MCDRV_GLOBAL_INFO	gsGlobalInfo;
static MCDRV_PACKET			gasPacket[MCDRV_MAX_PACKETS+1];

/* register next address */
static const UINT16	gawNextAddressA[MCDRV_A_REG_NUM] =
{
	0,		1,		2,		0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	8,		0xFFFF,	0xFFFF,	0xFFFF,	12,		13,		14,		15,
	16,		17,		18,		19,		20,		21,		22,		23,		24,		25,		0xFFFF,	27,		28,		29,		0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF
};

static const UINT16	gawNextAddressB_BASE[MCDRV_B_BASE_REG_NUM] =
{
	1,		2,		3,		4,		5,		6,		7,		8	,	9,		10,		11,		12,		13,		14	,	15	,	16,
	17	,	18	,	19	,	20	,	21	,	22	,	23	,	24	,	25	,	26,		27	,	28	,	29	,	30	,	31,		32,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF
};

static const UINT16	gawNextAddressB_MIXER[MCDRV_B_MIXER_REG_NUM] =
{
	1,		2,		3,		4,		5,		6,		7,		8,		9,		10,		11,		12,		13,		14,		15,		16,
	17,		18,		19,		20,		21,		22,		23,		24,		25,		26,		27,		28,		29,		30,		31,		32,
	33,		34,		35,		36,		37,		38,		39,		40,		41,		42,		43,		44,		45,		46,		47,		48,
	49,		50,		51,		52,		53,		54,		55,		56,		57,		58,		59,		60,		61,		62,		63,		64,
	65,		66,		67,		68,		69,		70,		71,		72,		73,		74,		75,		76,		77,		78,		79,		80,
	81,		82,		83,		84,		85,		86,		87,		88,		89,		90,		91,		92,		93,		94,		95,		96,
	97,		98,		99,		100,	101,	102,	103,	104,	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,	121,	122,	123,	124,	125,	126,	127,	128,
	129,	130,	131,	132,	133,	134,	135,	136,	137,	138,	139,	140,	141,	142,	143,	144,
	145,	146,	147,	148,	149,	150,	151,	152,	153,	154,	155,	156,	157,	158,	159,	160,
	161,	162,	163,	164,	165,	166,	167,	168,	169,	170,	171,	172,	173,	174,	175,	176,
	177,	178,	179,	180,	181,	182,	183,	184,	185,	186,	187,	188,	189,	190,	191,	192,
	193,	194,	195,	196,	197,	198,	199,	200,	201,	202,	0xFFFF,	204,	0xFFFF,	0xFFFF,	0xFFFF,	0xFFFF,
	0xFFFF,	0xFFFF,	211,	212,	213,	214,	215,	216,	217,	0xFFFF
};

static const UINT16	gawNextAddressB_AE[MCDRV_B_AE_REG_NUM] =
{
	1,		2,		3,		4,		5,		6,		7,		8,		9,		10,		11,		12,		13,		14,		15,		16,
	17,		18,		19,		20,		21,		22,		23,		24,		25,		26,		27,		28,		29,		30,		31,		32,
	33,		34,		35,		36,		37,		38,		39,		40,		41,		42,		43,		44,		45,		46,		47,		48,
	49,		50,		51,		52,		53,		54,		55,		56,		57,		58,		59,		60,		61,		62,		63,		64,
	65,		66,		67,		68,		69,		70,		71,		72,		73,		74,		75,		76,		77,		78,		79,		80,
	81,		82,		83,		84,		85,		86,		87,		88,		89,		90,		91,		92,		93,		94,		95,		96,
	97,		98,		99,		100,	101,	102,	103,	104,	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,	121,	122,	123,	124,	125,	126,	127,	128,
	129,	130,	131,	132,	133,	134,	135,	136,	137,	138,	139,	140,	141,	142,	143,	144,
	145,	146,	147,	148,	149,	150,	151,	152,	153,	154,	155,	156,	157,	158,	159,	160,
	161,	162,	163,	164,	165,	166,	167,	168,	169,	170,	171,	172,	173,	174,	175,	176,
	177,	178,	179,	180,	181,	182,	183,	184,	185,	186,	187,	188,	189,	190,	191,	192,
	193,	194,	195,	196,	197,	198,	199,	200,	201,	202,	203,	204,	205,	206,	207,	208,
	209,	210,	211,	212,	213,	214,	215,	216,	217,	218,	219,	220,	221,	222,	223,	224,
	225,	226,	227,	228,	229,	230,	231,	232,	233,	234,	235,	236,	237,	238,	239,	240,
	241,	242,	243,	244,	245,	246,	247,	248,	249,	250,	251,	252,	253,	254,	0xFFFF
};

static const UINT16	gawNextAddressB_CDSP[MCDRV_B_CDSP_REG_NUM] =
{
	1,		2,		3,		4,		5,		6,		7,		8,		9,		10,		11,		12,		13,		14,		15,		16,
	17,		18,		19,		20,		21,		22,		23,		24,		25,		26,		27,		28,		29,		30,		31,		32,
	33,		34,		35,		36,		37,		38,		39,		40,		41,		42,		43,		44,		45,		46,		47,		48,
	49,		50,		51,		52,		53,		54,		55,		56,		57,		58,		59,		60,		61,		62,		63,		64,
	65,		66,		67,		68,		69,		70,		71,		72,		73,		74,		75,		76,		77,		78,		79,		80,
	81,		82,		83,		84,		85,		86,		87,		88,		89,		90,		91,		92,		93,		94,		95,		96,
	97,		98,		99,		100,	101,	102,	103,	104,	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,	121,	122,	123,	124,	125,	126,	127,	128,
	129,	0xFFFF
};

static const UINT16	gawNextAddressB_CODEC[MCDRV_B_CODEC_REG_NUM] =
{
	1,		2,		3,		4,		5,		6,		7,		8,		9,		10,		11,		12,		13,		14,		15,		16,
	17,		18,		19,		20,		21,		22,		23,		24,		25,		26,		27,		28,		29,		30,		31,		32,
	33,		34,		35,		36,		37,		38,		39,		40,		41,		42,		43,		44,		45,		46,		47,		48,
	49,		50,		51,		52,		53,		54,		55,		56,		57,		58,		59,		60,		61,		62,		63,		64,
	65,		66,		67,		68,		69,		70,		71,		72,		73,		74,		75,		76,		77,		78,		79,		80,
	81,		82,		83,		84,		85,		86,		87,		88,		89,		90,		91,		92,		93,		94,		95,		96,
	97,		98,		99,		100,	101,	102,	103,	104,	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,	121,	122,	123,	124,	125,	126,	127,	0xFFFF
};

static const UINT16	gawNextAddressB_Ana[MCDRV_B_ANA_REG_NUM] =
{
	1,		2,		3,		4,		5,		6,		7,		8,		9,		10,		11,		12,		13,		14,		15,		16,
	17,		18,		19,		20,		21,		22,		23,		24,		25,		26,		27,		28,		29,		30,		31,		32,
	33,		34,		35,		36,		37,		38,		39,		40,		41,		42,		43,		44,		45,		46,		47,		48,
	49,		50,		51,		52,		53,		54,		55,		56,		57,		58,		59,		60,		61,		62,		63,		64,
	65,		66,		67,		68,		69,		70,		71,		72,		73,		74,		75,		76,		77,		0xFFFF,	79,		80,
	81,		82,		83,		84,		85,		86,		87,		88,		89,		90,		91,		92,		93,		94,		95,		96,
	97,		98,		99,		100,	101,	102,	103,	104,	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,	121,	122,	123,	124,	125,	126,	127,	0xFFFF
};



static void		SetRegDefault(void);
static void		InitPathInfo(void);
static void		InitVolInfo(void);
static void		InitDioInfo(void);
static void		InitDacInfo(void);
static void		InitAdcInfo(void);
static void		InitAdcExInfo(void);
static void		InitSpInfo(void);
static void		InitDngInfo(void);
static void		InitAeInfo(void);
static void		InitPdmInfo(void);
static void		InitPdmExInfo(void);
static void		InitDtmfInfo(void);
static void		InitGpMode(void);
static void		InitGpMask(void);
static void		InitSysEq(void);
static void		InitClockSwitch(void);
static void		InitDitSwap(void);

static SINT32	IsValidPath(void);
static void		ValidateADC(void);
static void		ValidateDAC(void);
static void		ValidateAE(void);
static void		ValidateMix(void);

static void		SetHPSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetSPSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetRCVSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetLO1SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetLO2SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetPMSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetDIT0SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetDIT1SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetDIT2SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetDACSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetAESourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetCDSPSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetADC0SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetADC1SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetMixSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetMix2SourceOnOff(const MCDRV_PATH_INFO* psPathInfo);
static void		SetBiasSourceOnOff(const MCDRV_PATH_INFO* psPathInfo);

static void		ClearSourceOnOff(UINT8* pbSrcOnOff);
static void		SetSourceOnOff(const UINT8* pbSrcOnOff, UINT8* pbDstOnOff, UINT8 bBlock, UINT8 bOn, UINT8 bOff);

static void		SetDIOCommon(const MCDRV_DIO_INFO* psDioInfo, UINT8 bPort);
static void		SetDIODIR(const MCDRV_DIO_INFO* psDioInfo, UINT8 bPort);
static void		SetDIODIT(const MCDRV_DIO_INFO* psDioInfo, UINT8 bPort);

static SINT16	GetADVolReg(SINT16 swVol);
static SINT16	GetLIVolReg(SINT16 swVol);
static SINT16	GetMcVolReg(SINT16 swVol);
static SINT16	GetMcGainReg(SINT16 swVol);
static SINT16	GetHpVolReg(SINT16 swVol);
static SINT16	GetHpGainReg(SINT16 swVol);
static SINT16	GetSpVolReg(SINT16 swVol);
static SINT16	GetRcVolReg(SINT16 swVol);
static SINT16	GetLoVolReg(SINT16 swVol);

static SINT32	WaitBitSet(UINT8 bSlaveAddr, UINT16 wRegAddr, UINT8 bBit, UINT32 dCycleTime, UINT32 dTimeOut);
static SINT32	WaitBitRelease(UINT8 bSlaveAddr, UINT16 wRegAddr, UINT8 bBit, UINT32 dCycleTime, UINT32 dTimeOut);
static SINT32	WaitCDSPBitSet(UINT8 bSlaveAddr, UINT16 wRegAddr, UINT8 bBit, UINT32 dCycleTime, UINT32 dTimeOut);
static SINT32	WaitCDSPBitRelease(UINT8 bSlaveAddr, UINT16 wRegAddr, UINT8 bBit, UINT32 dCycleTime, UINT32 dTimeOut);

/****************************************************************************
 *	McResCtrl_SetHwId
 *
 *	Description:
 *			Set hardware ID.
 *	Arguments:
 *			bHwId	hardware ID
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_INIT
 *
 ****************************************************************************/
SINT32	McResCtrl_SetHwId
(
	UINT8	bHwId
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetHwId");
#endif

	gsGlobalInfo.bHwId	= bHwId;

	switch (bHwId)
	{
	case MCDRV_HWID_79H:
		McDevProf_SetDevId(eMCDRV_DEV_ID_79H);
		break;
	case MCDRV_HWID_71H:
		McDevProf_SetDevId(eMCDRV_DEV_ID_71H);
		break;
	case MCDRV_HWID_46H:
		McDevProf_SetDevId(eMCDRV_DEV_ID_46H);
		break;
	case MCDRV_HWID_44H:
		McDevProf_SetDevId(eMCDRV_DEV_ID_44H);
		break;
	default:
		sdRet	= MCDRV_ERROR_INIT;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		SetRegDefault();
		gsGlobalInfo.abRegValA[MCI_HW_ID]	= gsGlobalInfo.bHwId;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetHwId", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McResCtrl_Init
 *
 *	Description:
 *			initialize the resource controller.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_Init
(
	void
)
{
	UINT8	i;
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_Init");
#endif

	gsGlobalInfo.ePacketBufAlloc	= eMCDRV_PACKETBUF_FREE;

	gsGlobalInfo.sInitInfo.bCkSel	= 0;
	gsGlobalInfo.sInitInfo.bDivR0	= 0;
	gsGlobalInfo.sInitInfo.bDivF0	= 0;
	gsGlobalInfo.sInitInfo.bDivR1	= 0;
	gsGlobalInfo.sInitInfo.bDivF1	= 0;
	gsGlobalInfo.sInitInfo.bRange0	= 0;
	gsGlobalInfo.sInitInfo.bRange1	= 0;
	gsGlobalInfo.sInitInfo.bBypass	= 0;
	gsGlobalInfo.sInitInfo.bDioSdo0Hiz	= 0;
	gsGlobalInfo.sInitInfo.bDioSdo1Hiz	= 0;
	gsGlobalInfo.sInitInfo.bDioSdo2Hiz	= 0;
	gsGlobalInfo.sInitInfo.bDioClk0Hiz	= 0;
	gsGlobalInfo.sInitInfo.bDioClk1Hiz	= 0;
	gsGlobalInfo.sInitInfo.bDioClk2Hiz	= 0;
	gsGlobalInfo.sInitInfo.bPcmHiz		= 0;
	gsGlobalInfo.sInitInfo.bLineIn1Dif	= 0;
	gsGlobalInfo.sInitInfo.bLineIn2Dif	= 0;
	gsGlobalInfo.sInitInfo.bLineOut1Dif	= 0;
	gsGlobalInfo.sInitInfo.bLineOut2Dif	= 0;
	gsGlobalInfo.sInitInfo.bSpmn		= 0;
	gsGlobalInfo.sInitInfo.bMic1Sng		= 0;
	gsGlobalInfo.sInitInfo.bMic2Sng		= 0;
	gsGlobalInfo.sInitInfo.bMic3Sng		= 0;
	gsGlobalInfo.sInitInfo.bPowerMode	= 0;
	gsGlobalInfo.sInitInfo.bSpHiz		= 0;
	gsGlobalInfo.sInitInfo.bLdo			= 0;
	gsGlobalInfo.sInitInfo.bPad0Func	= 0;
	gsGlobalInfo.sInitInfo.bPad1Func	= 0;
	gsGlobalInfo.sInitInfo.bPad2Func	= 0;
	gsGlobalInfo.sInitInfo.bAvddLev		= 0;
	gsGlobalInfo.sInitInfo.bVrefLev		= 0;
	gsGlobalInfo.sInitInfo.bDclGain		= 0;
	gsGlobalInfo.sInitInfo.bDclLimit	= 0;
	gsGlobalInfo.sInitInfo.bCpMod		= 1;
	gsGlobalInfo.sInitInfo.bSdDs		= 0;
	gsGlobalInfo.sInitInfo.bReserved1	= 0;
	gsGlobalInfo.sInitInfo.bReserved2	= 0;
	gsGlobalInfo.sInitInfo.bReserved3	= 0;
	gsGlobalInfo.sInitInfo.bReserved4	= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dAdHpf				= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dMic1Cin			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dMic2Cin			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dMic3Cin			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dLine1Cin			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dLine2Cin			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dVrefRdy1			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dVrefRdy2			= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dHpRdy				= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dSpRdy				= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dPdm				= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dAnaRdyInterval	= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval		= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dAnaRdyTimeOut		= 0;
	gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut		= 0;

	InitPathInfo();
	InitVolInfo();
	InitDioInfo();
	InitDacInfo();
	InitAdcInfo();
	InitAdcExInfo();
	InitSpInfo();
	InitDngInfo();
	InitAeInfo();
	InitPdmInfo();
	InitPdmExInfo();
	InitDtmfInfo();
	InitGpMode();
	InitGpMask();
	InitSysEq();
	InitClockSwitch();
	InitDitSwap();

	for(i = 0; i < GPIO_PAD_NUM; i++)
	{
		gsGlobalInfo.abGpPad[i]	= 0;
	}

	McResCtrl_InitRegUpdate();

	gsGlobalInfo.pbAeEx		= NULL;
	gsGlobalInfo.pbCdspFw	= NULL;
	gsGlobalInfo.bCdspFs	= 0;

	gsGlobalInfo.eAPMode = eMCDRV_APM_OFF;

	gsGlobalInfo.sdIrqDisableCount	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_Init", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_InitBaseBlockReg
 *
 *	Description:
 *			Initialize the BASE Block virtual registers.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_InitBaseBlockReg
(
	void
)
{
	UINT16	i;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_InitBaseBlockReg");
#endif

	for(i = 0; i < MCDRV_B_BASE_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValB_BASE[i]	= 0;
	}
	gsGlobalInfo.abRegValB_BASE[MCI_RSTB]				= MCI_RSTB_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL]		= MCI_PWM_DIGITAL_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL_1]		= MCI_PWM_DIGITAL_1_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL_1]	|= MCB_PWM_DPCD;
		gsGlobalInfo.abRegValB_BASE[MCI_PSW]			|= MCB_PSWB;
		gsGlobalInfo.abRegValB_BASE[MCI_PLL1RST]		= MCI_PLL1_DEF;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
	{
		gsGlobalInfo.abRegValB_BASE[MCI_RSTB]			|= MCB_RSTC;
		gsGlobalInfo.abRegValB_BASE[MCI_PWM_DPC]		= MCI_PWM_DPC_DEF;
		gsGlobalInfo.abRegValB_BASE[MCI_CSDIN_MSK]		= MCI_CSDIN_MSK_DEF;
	}
	gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL_BDSP]	= MCI_PWM_DIGITAL_BDSP_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_SD_MSK]				= MCI_SD_MSK_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_SD_MSK_1]			= MCI_SD_MSK_1_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_BCLK_MSK]			= MCI_BCLK_MSK_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_BCLK_MSK_1]			= MCI_BCLK_MSK_1_DEF;
	gsGlobalInfo.abRegValB_BASE[MCI_BCKP]				= MCI_BCKP_DEF;
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		gsGlobalInfo.abRegValB_BASE[MCI_PA2_MSK]		= MCI_PA2_MSK_DEF;
	}
	gsGlobalInfo.abRegValB_BASE[MCI_PA_MSK]				= MCI_PA_MSK_DEF;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_InitBaseBlockReg", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_InitBlockBReg
 *
 *	Description:
 *			Initialize the Block B virtual registers.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_InitBlockBReg
(
	void
)
{
	UINT16	i;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_InitBlockBReg");
#endif

	for(i = 0; i < MCDRV_B_MIXER_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValB_MIXER[i]	= 0;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_DIT_ININTP]	= MCI_DIT_ININTP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_DIT_ININTP]|= MCB_DIT2_ININTP1;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_DIR_INTP]		= MCI_DIR_INTP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_DIR_INTP]	|= MCB_ADC1_INTP;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_ADC_INTP]		= MCI_ADC_INTP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_ADC_INTP]	|= MCB_DTMFB_INTP;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_AINTP]			= MCI_AINTP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_AINTP]		|= MCB_ADC1_AINTP;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_IINTP]			= MCI_IINTP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_IINTP]		|= MCB_ADC1_IINTP;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_DAC_INTP]		= MCI_DAC_INTP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_DAC_INTP]	|= MCB_DTMF_INTP;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_DIR0_CH]		= MCI_DIR0_CH_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_DIT0_SLOT]		= MCI_DIT0_SLOT_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_RX0]		= MCI_PCM_RX0_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_TX0]		= MCI_PCM_TX0_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_SLOT_TX0]	= MCI_PCM_SLOT_TX0_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_DIR1_CH]		= MCI_DIR1_CH_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_DIT1_SLOT]		= MCI_DIT1_SLOT_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_RX1]		= MCI_PCM_RX1_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_TX1]		= MCI_PCM_TX1_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_SLOT_TX1]	= MCI_PCM_SLOT_TX1_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_DIR2_CH]		= MCI_DIR2_CH_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_DIT2_SLOT]		= MCI_DIT2_SLOT_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_RX2]		= MCI_PCM_RX2_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_TX2]		= MCI_PCM_TX2_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PCM_SLOT_TX2]	= MCI_PCM_SLOT_TX2_DEF;
	gsGlobalInfo.abRegValB_MIXER[MCI_PDM_AGC]		= MCI_PDM_AGC_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_MIXER[MCI_PDM_AGC_ENV]= MCI_PDM_AGC_ENV_DEF;
		gsGlobalInfo.abRegValB_MIXER[MCI_PDM_AGC_CUT]= MCI_PDM_AGC_CUT_DEF;
	}
	gsGlobalInfo.abRegValB_MIXER[MCI_PDM_STWAIT]	= MCI_PDM_STWAIT_DEF;

	for(i = 0; i < MCDRV_B_AE_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValB_AE[i]	= 0;
	}
	gsGlobalInfo.abRegValB_AE[MCI_BAND0_CEQ0]		= MCI_BAND0_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND1_CEQ0]		= MCI_BAND1_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND2_CEQ0]		= MCI_BAND2_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND3H_CEQ0]		= MCI_BAND3H_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND4H_CEQ0]		= MCI_BAND4H_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND5_CEQ0]		= MCI_BAND5_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND6H_CEQ0]		= MCI_BAND6H_CEQ0_H_DEF;
	gsGlobalInfo.abRegValB_AE[MCI_BAND7H_CEQ0]		= MCI_BAND7H_CEQ0_H_DEF;

	for(i = 0; i < MCDRV_B_CDSP_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValB_CDSP[i]	= 0;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
	{
		gsGlobalInfo.abRegValB_CDSP[MCI_CDSP_RESET]	= MCB_CDSP_SRST;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_InitBlockBReg", 0);
#endif
}

/****************************************************************************
 *	SetRegDefault
 *
 *	Description:
 *			Initialize the virtual registers.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetRegDefault
(
	void
)
{
	UINT16	i;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetRegDefault");
#endif

	for(i = 0; i < MCDRV_A_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValA[i]	= 0;
	}
	gsGlobalInfo.abRegValA[MCI_RST]		= MCI_RST_DEF;

	McResCtrl_InitBaseBlockReg();

	for(i = 0; i < MCDRV_B_ANA_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValB_ANA[i]	= 0;
	}
	gsGlobalInfo.abRegValB_ANA[MCI_ANA_RST]				= MCI_ANA_RST_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_0]		= MCI_PWM_ANALOG_0_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_0]	|= MCB_PWM_LDOD;
	}
	gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_1]		= MCI_PWM_ANALOG_1_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_2]		= MCI_PWM_ANALOG_2_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_2]	|= MCB_PWM_ADM;
	}
	gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_3]		= MCI_PWM_ANALOG_3_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_4]		= MCI_PWM_ANALOG_4_DEF;
	if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)
	{
		gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_5]	= MCI_PWM_ANALOG_5_DEF;
	}
	gsGlobalInfo.abRegValB_ANA[MCI_HPVOL_L]				= MCI_HPVOL_L_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_SPVOL_L]				= MCI_SPVOL_L_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_RCVOL]				= MCI_RCVOL_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_LEV]					= MCI_LEV_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_ANA[MCI_DNG_FW]			= MCI_DNG_FW_DEF;
	}
	gsGlobalInfo.abRegValB_ANA[MCI_DNGATRT_HP]			= MCI_DNGATRT_HP_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGTARGET_HP]		= MCI_DNGTARGET_HP_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGON_HP]			= MCI_DNGON_HP_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGATRT_SP]			= MCI_DNGATRT_SP_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGTARGET_SP]		= MCI_DNGTARGET_SP_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGON_SP]			= MCI_DNGON_SP_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGATRT_RC]			= MCI_DNGATRT_RC_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGTARGET_RC]		= MCI_DNGTARGET_RC_DEF;
	gsGlobalInfo.abRegValB_ANA[MCI_DNGON_RC]			= MCI_DNGON_RC_DEF;

	for(i = 0; i < MCDRV_B_CODEC_REG_NUM; i++)
	{
		gsGlobalInfo.abRegValB_CODEC[i]	= 0;
	}
	gsGlobalInfo.abRegValB_CODEC[MCI_CD_DP]				= MCI_CD_DP_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_CODEC[MCI_CD_DP]			|= (MCB_CD_DP2|MCB_CD_DP1);
		gsGlobalInfo.abRegValB_CODEC[MCI_CD_MSK]		|= MCI_CD_MSK_DEF;
		gsGlobalInfo.abRegValB_CODEC[MCI_DPMCLKO]		|= MCB_DPMCLKO;
	}
	gsGlobalInfo.abRegValB_CODEC[MCI_PLL_RST]			= MCI_PLL_RST_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_DIVR0]				= MCI_DIVR0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_DIVF0]				= MCI_DIVF0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_DIVR1]				= MCI_DIVR1_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_DIVF1]				= MCI_DIVF1_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_AD_AGC]			= MCI_AD_AGC_DEF;
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.abRegValB_CODEC[MCI_AGC]			= MCI_AGC_DEF;
		gsGlobalInfo.abRegValB_CODEC[MCI_AGC_CUT]		= MCI_AGC_CUT_DEF;
	}
	gsGlobalInfo.abRegValB_CODEC[MCI_DAC_CONFIG]		= MCI_DAC_CONFIG_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ0_19_12]	= MCI_SYS_CEQ0_19_12_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ0_11_4]		= MCI_SYS_CEQ0_11_4_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ0_3_0]		= MCI_SYS_CEQ0_3_0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ1_19_12]	= MCI_SYS_CEQ1_19_12_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ1_11_4]		= MCI_SYS_CEQ1_11_4_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ1_3_0]		= MCI_SYS_CEQ1_3_0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ2_19_12]	= MCI_SYS_CEQ2_19_12_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ2_11_4]		= MCI_SYS_CEQ2_11_4_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ2_3_0]		= MCI_SYS_CEQ2_3_0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ3_19_12]	= MCI_SYS_CEQ3_19_12_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ3_11_4]		= MCI_SYS_CEQ3_11_4_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ3_3_0]		= MCI_SYS_CEQ3_3_0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ4_19_12]	= MCI_SYS_CEQ4_19_12_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ4_11_4]		= MCI_SYS_CEQ4_11_4_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYS_CEQ4_3_0]		= MCI_SYS_CEQ4_3_0_DEF;
	gsGlobalInfo.abRegValB_CODEC[MCI_SYSTEM_EQON]		= MCI_SYSTEM_EQON_DEF;

	McResCtrl_InitBlockBReg();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetRegDefault", 0);
#endif
}

/****************************************************************************
 *	InitPathInfo
 *
 *	Description:
 *			Initialize path info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitPathInfo
(
	void
)
{
	UINT8	bCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitPathInfo");
#endif

	for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asHpOut[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asSpOut[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asRcOut[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asLout1[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asLout2[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asAe[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asAdc0[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asAdc1[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asMix[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < BIAS_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asBias[bCh].abSrcOnOff);
	}
	for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++)
	{
		ClearSourceOnOff(gsGlobalInfo.sPathInfo.asMix2[bCh].abSrcOnOff);
	}
	gsGlobalInfo.sPathInfoVirtual	= gsGlobalInfo.sPathInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitPathInfo", 0);
#endif
}

/****************************************************************************
 *	InitVolInfo
 *
 *	Description:
 *			Initialize volume info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitVolInfo
(
	void
)
{
	UINT8	bCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitVolInfo");
#endif

	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Ad0[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	gsGlobalInfo.sVolInfo.aswD_Reserved1[0]			= MCDRV_LOGICAL_VOL_MUTE;
	for(bCh = 0; bCh < AENG6_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Aeng6[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Pdm[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dtmfb[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dir0[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dir1[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dir2[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Ad0Att[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	gsGlobalInfo.sVolInfo.aswD_Reserved2[0]			= MCDRV_LOGICAL_VOL_MUTE;
	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dir0Att[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dir1Att[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dir2Att[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_SideTone[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DTFM_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_DtmfAtt[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_DacMaster[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_DacVoice[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_DacAtt[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dit0[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dit1[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Dit2[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_Dit21[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Ad0[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < AD1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Ad1[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < LIN1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Lin1[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < LIN2_VOL_CHANNELS; bCh++)	
	{
		gsGlobalInfo.sVolInfo.aswA_Lin2[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)	
	{
		gsGlobalInfo.sVolInfo.aswA_Mic1[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)	
	{
		gsGlobalInfo.sVolInfo.aswA_Mic2[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)	
	{
		gsGlobalInfo.sVolInfo.aswA_Mic3[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < HP_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Hp[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < SP_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Sp[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < RC_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Rc[bCh]			= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < LOUT1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Lout1[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < LOUT2_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Lout2[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Mic1Gain[bCh]	= MCDRV_LOGICAL_MICGAIN_DEF;
	}
	for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Mic2Gain[bCh]	= MCDRV_LOGICAL_MICGAIN_DEF;
	}
	for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_Mic3Gain[bCh]	= MCDRV_LOGICAL_MICGAIN_DEF;
	}
	for(bCh = 0; bCh < HPGAIN_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswA_HpGain[bCh]		= MCDRV_LOGICAL_HPGAIN_DEF;
	}
	for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Adc1[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}
	for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Adc1Att[bCh]		= MCDRV_LOGICAL_VOL_MUTE;
	}

	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		gsGlobalInfo.sVolInfo.aswD_Adc0AeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_Adc1AeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_Dir0AeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_Dir1AeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_Dir2AeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_PdmAeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
		gsGlobalInfo.sVolInfo.aswD_MixAeMix[bCh]	= MCDRV_LOGICAL_VOL_MUTE;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitVolInfo", 0);
#endif
}

/****************************************************************************
 *	InitDioInfo
 *
 *	Description:
 *			Initialize Digital I/O info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitDioInfo
(
	void
)
{
	UINT8	bDioIdx, bDioCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitDioInfo");
#endif

	for(bDioIdx = 0; bDioIdx < 3; bDioIdx++)
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bMasterSlave	= MCDRV_DIO_SLAVE;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bAutoFs		= MCDRV_AUTOFS_ON;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bFs			= MCDRV_FS_48000;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bBckFs			= MCDRV_BCKFS_64;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bInterface		= MCDRV_DIO_DA;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bBckInvert		= MCDRV_BCLK_NORMAL;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bPcmHizTim		= MCDRV_PCMHIZTIM_FALLING;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bPcmClkDown	= MCDRV_PCM_CLKDOWN_OFF;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bPcmFrame		= MCDRV_PCM_SHORTFRAME;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDioCommon.bPcmHighPeriod	= 0;

		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.wSrcRate				= 0x8000;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.sDaFormat.bBitSel	= MCDRV_BITSEL_16;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.sDaFormat.bMode		= MCDRV_DAMODE_HEADALIGN;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.sPcmFormat.bMono		= MCDRV_PCM_MONO;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.sPcmFormat.bOrder	= MCDRV_PCM_MSB_FIRST;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.sPcmFormat.bLaw		= MCDRV_PCM_LINEAR;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.sPcmFormat.bBitSel	= MCDRV_PCM_BITSEL_8;
		for(bDioCh = 0; bDioCh < DIO_CHANNELS; bDioCh++)
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDir.abSlot[bDioCh]	= bDioCh;
		}

		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.wSrcRate				= 0x1000;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.sDaFormat.bBitSel	= MCDRV_BITSEL_16;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.sDaFormat.bMode		= MCDRV_DAMODE_HEADALIGN;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.sPcmFormat.bMono		= MCDRV_PCM_MONO;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.sPcmFormat.bOrder	= MCDRV_PCM_MSB_FIRST;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.sPcmFormat.bLaw		= MCDRV_PCM_LINEAR;
		gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.sPcmFormat.bBitSel	= MCDRV_PCM_BITSEL_8;
		for(bDioCh = 0; bDioCh < DIO_CHANNELS; bDioCh++)
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bDioIdx].sDit.abSlot[bDioCh]	= bDioCh;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitDioInfo", 0);
#endif
}

/****************************************************************************
 *	InitDacInfo
 *
 *	Description:
 *			Initialize Dac info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitDacInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitDacInfo");
#endif

	gsGlobalInfo.sDacInfo.bMasterSwap	= MCDRV_DSWAP_OFF;
	gsGlobalInfo.sDacInfo.bVoiceSwap	= MCDRV_DSWAP_OFF;
	gsGlobalInfo.sDacInfo.bDcCut		= MCDRV_DCCUT_ON;
	gsGlobalInfo.sDacInfo.bDacSwap		= MCDRV_DSWAP_OFF;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitDacInfo", 0);
#endif
}

/****************************************************************************
 *	InitAdcInfo
 *
 *	Description:
 *			Initialize Adc info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitAdcInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitAdcInfo");
#endif

	gsGlobalInfo.sAdcInfo.bAgcAdjust	= MCDRV_AGCADJ_0;
	gsGlobalInfo.sAdcInfo.bAgcOn		= MCDRV_AGC_OFF;
	gsGlobalInfo.sAdcInfo.bMono			= MCDRV_ADC_STEREO;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitAdcInfo", 0);
#endif
}

/****************************************************************************
 *	InitAdcExInfo
 *
 *	Description:
 *			Initialize AdcEx info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitAdcExInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitAdcExInfo");
#endif

	gsGlobalInfo.sAdcExInfo.bAgcEnv		= MCDRV_AGCENV_10650;
	gsGlobalInfo.sAdcExInfo.bAgcLvl		= MCDRV_AGCLVL_3;
	gsGlobalInfo.sAdcExInfo.bAgcUpTim	= MCDRV_AGCUPTIM_1365;
	gsGlobalInfo.sAdcExInfo.bAgcDwTim	= MCDRV_AGCDWTIM_10650;
	gsGlobalInfo.sAdcExInfo.bAgcCutLvl	= MCDRV_AGCCUTLVL_OFF;
	gsGlobalInfo.sAdcExInfo.bAgcCutTim	= MCDRV_AGCCUTTIM_5310;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitAdcExInfo", 0);
#endif
}

/****************************************************************************
 *	InitSpInfo
 *
 *	Description:
 *			Initialize SP info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitSpInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitSpInfo");
#endif

	gsGlobalInfo.sSpInfo.bSwap	= MCDRV_SPSWAP_OFF;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitSpInfo", 0);
#endif
}

/****************************************************************************
 *	InitDngInfo
 *
 *	Description:
 *			Initialize DNG info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitDngInfo
(
	void
)
{
	UINT8	bItem;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitDngInfo");
#endif

	for(bItem = MCDRV_DNG_ITEM_HP; bItem <= MCDRV_DNG_ITEM_RC; bItem++)
	{
		gsGlobalInfo.sDngInfo.abOnOff[bItem]		= MCDRV_DNG_OFF;
		gsGlobalInfo.sDngInfo.abThreshold[bItem]	= MCDRV_DNG_THRES_60;
		gsGlobalInfo.sDngInfo.abHold[bItem]		= MCDRV_DNG_HOLD_500;
		gsGlobalInfo.sDngInfo.abAttack[bItem]	= MCDRV_DNG_ATTACK_800;
		gsGlobalInfo.sDngInfo.abRelease[bItem]	= MCDRV_DNG_RELEASE_940;
		gsGlobalInfo.sDngInfo.abTarget[bItem]	= MCDRV_DNG_TARGET_MUTE;
	}
	gsGlobalInfo.sDngInfo.bFw	= MCDRV_DNG_FW_ON;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitDngInfo", 0);
#endif
}

/****************************************************************************
 *	InitAeInfo
 *
 *	Description:
 *			Initialize Audio Engine info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitAeInfo
(
	void
)
{
	UINT32	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitAeInfo");
#endif

	gsGlobalInfo.sAeInfo.bOnOff		= 0;
	for(i = 0; i < BEX_PARAM_SIZE; i++)
	{
		gsGlobalInfo.sAeInfo.abBex[i]	= 0;
	}
	for(i = 0; i < WIDE_PARAM_SIZE; i++)
	{
		gsGlobalInfo.sAeInfo.abWide[i]	= 0;
	}
	for(i = 0; i < DRC_PARAM_SIZE; i++)
	{
		gsGlobalInfo.sAeInfo.abDrc[i]	= 0;
	}
	for(i = 0; i < EQ5_PARAM_SIZE; i++)
	{
		gsGlobalInfo.sAeInfo.abEq5[i]	= 0;
	}
	for(i = 0; i < EQ3_PARAM_SIZE; i++)
	{
		gsGlobalInfo.sAeInfo.abEq3[i]	= 0;
	}
	for(i = 0; i < DRC2_PARAM_SIZE; i++)
	{
		gsGlobalInfo.sAeInfo.abDrc2[i]	= 0;
	}


#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitAeInfo", 0);
#endif
}

/****************************************************************************
 *	InitPdmInfo
 *
 *	Description:
 *			Initialize Pdm info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitPdmInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitPdmInfo");
#endif

	gsGlobalInfo.sPdmInfo.bClk			= MCDRV_PDM_CLK_64;
	gsGlobalInfo.sPdmInfo.bAgcAdjust	= MCDRV_AGCADJ_0;
	gsGlobalInfo.sPdmInfo.bAgcOn		= MCDRV_AGC_OFF;
	gsGlobalInfo.sPdmInfo.bPdmEdge		= MCDRV_PDMEDGE_LH;
	gsGlobalInfo.sPdmInfo.bPdmWait		= MCDRV_PDMWAIT_10;
	gsGlobalInfo.sPdmInfo.bPdmSel		= MCDRV_PDMSEL_L1R2;
	gsGlobalInfo.sPdmInfo.bMono			= MCDRV_PDM_STEREO;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitPdmInfo", 0);
#endif
}

/****************************************************************************
 *	InitPdmExInfo
 *
 *	Description:
 *			Initialize PdmEx info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitPdmExInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitPdmExInfo");
#endif

	gsGlobalInfo.sPdmExInfo.bAgcEnv		= MCDRV_AGCENV_10650;
	gsGlobalInfo.sPdmExInfo.bAgcLvl		= MCDRV_AGCLVL_3;
	gsGlobalInfo.sPdmExInfo.bAgcUpTim	= MCDRV_AGCUPTIM_1365;
	gsGlobalInfo.sPdmExInfo.bAgcDwTim	= MCDRV_AGCDWTIM_10650;
	gsGlobalInfo.sPdmExInfo.bAgcCutLvl	= MCDRV_AGCCUTLVL_OFF;
	gsGlobalInfo.sPdmExInfo.bAgcCutTim	= MCDRV_AGCCUTTIM_5310;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitPdmExInfo", 0);
#endif
}

/****************************************************************************
 *	InitDtmfInfo
 *
 *	Description:
 *			Initialize DTMF info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitDtmfInfo
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitDtmfInfo");
#endif

	gsGlobalInfo.sDtmfInfo.bSinGenHost			= MCDRV_DTMFHOST_REG;
	gsGlobalInfo.sDtmfInfo.bOnOff				= MCDRV_DTMF_OFF;
	gsGlobalInfo.sDtmfInfo.sParam.wSinGen0Freq	= 1000;
	gsGlobalInfo.sDtmfInfo.sParam.wSinGen1Freq	= 1000;
	gsGlobalInfo.sDtmfInfo.sParam.swSinGen0Vol	= MCDRV_LOGICAL_VOL_MUTE;
	gsGlobalInfo.sDtmfInfo.sParam.swSinGen1Vol	= MCDRV_LOGICAL_VOL_MUTE;
	gsGlobalInfo.sDtmfInfo.sParam.bSinGenGate	= 0;
	gsGlobalInfo.sDtmfInfo.sParam.bSinGenLoop	= MCDRV_SINGENLOOP_OFF;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitDtmfInfo", 0);
#endif
}

/****************************************************************************
 *	InitGpMode
 *
 *	Description:
 *			Initialize Gp mode.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitGpMode
(
	void
)
{
	UINT8	bGpioIdx;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitGpMode");
#endif

	for(bGpioIdx = 0; bGpioIdx < GPIO_PAD_NUM; bGpioIdx++)
	{
		gsGlobalInfo.sGpMode.abGpDdr[bGpioIdx]		= MCDRV_GPDDR_IN;
		gsGlobalInfo.sGpMode.abGpMode[bGpioIdx]		= MCDRV_GPMODE_RISING;
		gsGlobalInfo.sGpMode.abGpIrq[bGpioIdx]		= MCDRV_GPIRQ_DISABLE;
		gsGlobalInfo.sGpMode.abGpHost[bGpioIdx]		= MCDRV_GPHOST_SCU;
		gsGlobalInfo.sGpMode.abGpInvert[bGpioIdx]	= MCDRV_GPINV_NORMAL;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitGpMode", 0);
#endif
}

/****************************************************************************
 *	InitGpMask
 *
 *	Description:
 *			Initialize Gp mask.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitGpMask
(
	void
)
{
	UINT8	bGpioIdx;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitGpMask");
#endif

	for(bGpioIdx = 0; bGpioIdx < GPIO_PAD_NUM; bGpioIdx++)
	{
		gsGlobalInfo.abGpMask[bGpioIdx]		= MCDRV_GPMASK_ON;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitGpMask", 0);
#endif
}

/****************************************************************************
 *	InitSysEq
 *
 *	Description:
 *			Initialize System EQ.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitSysEq
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitSysEq");
#endif

	gsGlobalInfo.sSysEq.bOnOff		= MCDRV_SYSEQ_ON;
	gsGlobalInfo.sSysEq.abParam[0]	= 0x10;
	gsGlobalInfo.sSysEq.abParam[1]	= 0xC4;
	gsGlobalInfo.sSysEq.abParam[2]	= 0x50;
	gsGlobalInfo.sSysEq.abParam[3]	= 0x12;
	gsGlobalInfo.sSysEq.abParam[4]	= 0xC4;
	gsGlobalInfo.sSysEq.abParam[5]	= 0x40;
	gsGlobalInfo.sSysEq.abParam[6]	= 0x02;
	gsGlobalInfo.sSysEq.abParam[7]	= 0xA9;
	gsGlobalInfo.sSysEq.abParam[8]	= 0x60;
	gsGlobalInfo.sSysEq.abParam[9]	= 0xED;
	gsGlobalInfo.sSysEq.abParam[10]	= 0x3B;
	gsGlobalInfo.sSysEq.abParam[11]	= 0xC0;
	gsGlobalInfo.sSysEq.abParam[12]	= 0xFC;
	gsGlobalInfo.sSysEq.abParam[13]	= 0x92;
	gsGlobalInfo.sSysEq.abParam[14]	= 0x40;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitSysEq", 0);
#endif
}

/****************************************************************************
 *	InitClockSwitch
 *
 *	Description:
 *			Initialize switch clock info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitClockSwitch
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitClockSwitch");
#endif

	gsGlobalInfo.sClockSwitch.bClkSrc	= MCDRV_CLKSW_CLKI0;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitClockSwitch", 0);
#endif
}

/****************************************************************************
 *	InitDitSwap
 *
 *	Description:
 *			Initialize DIT Swap info.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	InitDitSwap
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitDitSwap");
#endif

	gsGlobalInfo.sDitSwapInfo.bSwapL	= MCDRV_DITSWAP_OFF;
	gsGlobalInfo.sDitSwapInfo.bSwapR	= MCDRV_DITSWAP_OFF;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitDitSwap", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_UpdateState
 *
 *	Description:
 *			update state.
 *	Arguments:
 *			eState	state
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_UpdateState
(
	MCDRV_STATE	eState
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_UpdateState");
#endif

	geState = eState;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_UpdateState", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetState
 *
 *	Description:
 *			Get state.
 *	Arguments:
 *			none
 *	Return:
 *			current state
 *
 ****************************************************************************/
MCDRV_STATE	McResCtrl_GetState
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet	= geState;
	McDebugLog_FuncIn("McResCtrl_GetState");
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetState", &sdRet);
#endif

	return geState;
}

/****************************************************************************
 *	McResCtrl_GetRegVal
 *
 *	Description:
 *			Get register value.
 *	Arguments:
 *			wRegType	register type
 *			wRegAddr	address
 *	Return:
 *			register value
 *
 ****************************************************************************/
UINT8	McResCtrl_GetRegVal
(
	UINT16	wRegType,
	UINT16	wRegAddr
)
{
	UINT8	bVal	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetRegVal");
#endif

	switch(wRegType)
	{
	case	MCDRV_PACKET_REGTYPE_A:
		bVal	= gsGlobalInfo.abRegValA[wRegAddr];
		break;
	case	MCDRV_PACKET_REGTYPE_B_BASE:
		bVal	= gsGlobalInfo.abRegValB_BASE[wRegAddr];
		break;
	case	MCDRV_PACKET_REGTYPE_B_MIXER:
		bVal	= gsGlobalInfo.abRegValB_MIXER[wRegAddr];
		break;
	case	MCDRV_PACKET_REGTYPE_B_AE:
		bVal	= gsGlobalInfo.abRegValB_AE[wRegAddr];
		break;
	case	MCDRV_PACKET_REGTYPE_B_CDSP:
		bVal	= gsGlobalInfo.abRegValB_CDSP[wRegAddr];
		break;
	case	MCDRV_PACKET_REGTYPE_B_CODEC:
		bVal	= gsGlobalInfo.abRegValB_CODEC[wRegAddr];
		break;
	case	MCDRV_PACKET_REGTYPE_B_ANA:
		bVal	= gsGlobalInfo.abRegValB_ANA[wRegAddr];
		break;
	default:
		break;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bVal;
	McDebugLog_FuncOut("McResCtrl_GetRegVal", &sdRet);
#endif

	return bVal;
}

/****************************************************************************
 *	McResCtrl_SetRegVal
 *
 *	Description:
 *			Set register value.
 *	Arguments:
 *			wRegType	register type
 *			wRegAddr	address
 *			bRegVal		register value
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetRegVal
(
	UINT16	wRegType,
	UINT16	wRegAddr,
	UINT8	bRegVal
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetRegVal");
#endif

	switch(wRegType)
	{
	case	MCDRV_PACKET_REGTYPE_A:
		gsGlobalInfo.abRegValA[wRegAddr]		= bRegVal;
		break;
	case	MCDRV_PACKET_REGTYPE_B_BASE:
		gsGlobalInfo.abRegValB_BASE[wRegAddr]	= bRegVal;
		break;
	case	MCDRV_PACKET_REGTYPE_B_MIXER:
		gsGlobalInfo.abRegValB_MIXER[wRegAddr]	= bRegVal;
		break;
	case	MCDRV_PACKET_REGTYPE_B_AE:
		gsGlobalInfo.abRegValB_AE[wRegAddr]		= bRegVal;
		break;
	case	MCDRV_PACKET_REGTYPE_B_CDSP:
		gsGlobalInfo.abRegValB_CDSP[wRegAddr]	= bRegVal;
		break;
	case	MCDRV_PACKET_REGTYPE_B_CODEC:
		gsGlobalInfo.abRegValB_CODEC[wRegAddr]	= bRegVal;
		break;
	case	MCDRV_PACKET_REGTYPE_B_ANA:
		gsGlobalInfo.abRegValB_ANA[wRegAddr]	= bRegVal;
		break;
	default:
		break;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetRegVal", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetInitInfo
 *
 *	Description:
 *			Set Initialize information.
 *	Arguments:
 *			psInitInfo	Initialize information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetInitInfo
(
	const MCDRV_INIT_INFO*	psInitInfo
)
{
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetInitInfo");
#endif

	gsGlobalInfo.sInitInfo.bCkSel	= psInitInfo->bCkSel;
	gsGlobalInfo.sInitInfo.bDivR0	= psInitInfo->bDivR0;
	gsGlobalInfo.sInitInfo.bDivF0	= psInitInfo->bDivF0;
	gsGlobalInfo.sInitInfo.bDivR1	= psInitInfo->bDivR1;
	gsGlobalInfo.sInitInfo.bDivF1	= psInitInfo->bDivF1;
	if(McDevProf_IsValid(eMCDRV_FUNC_RANGE) != 0)
	{
		gsGlobalInfo.sInitInfo.bRange0	= psInitInfo->bRange0;
		gsGlobalInfo.sInitInfo.bRange1	= psInitInfo->bRange1;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bRange0	= 0;
		gsGlobalInfo.sInitInfo.bRange1	= 0;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_BYPASS) != 0)
	{
		gsGlobalInfo.sInitInfo.bBypass	= psInitInfo->bBypass;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bBypass	= 0;
	}
	gsGlobalInfo.sInitInfo.bDioSdo0Hiz	= psInitInfo->bDioSdo0Hiz;
	gsGlobalInfo.sInitInfo.bDioSdo1Hiz	= psInitInfo->bDioSdo1Hiz;
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		gsGlobalInfo.sInitInfo.bDioSdo2Hiz	= psInitInfo->bDioSdo2Hiz;
	}
	gsGlobalInfo.sInitInfo.bDioClk0Hiz	= psInitInfo->bDioClk0Hiz;
	gsGlobalInfo.sInitInfo.bDioClk1Hiz	= psInitInfo->bDioClk1Hiz;
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) != 0)
	{
		gsGlobalInfo.sInitInfo.bDioClk2Hiz	= psInitInfo->bDioClk2Hiz;
	}
	gsGlobalInfo.sInitInfo.bPcmHiz		= psInitInfo->bPcmHiz;
	if(McDevProf_IsValid(eMCDRV_FUNC_LI1) != 0)
	{
		gsGlobalInfo.sInitInfo.bLineIn1Dif	= psInitInfo->bLineIn1Dif;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bLineIn1Dif	= MCDRV_LINE_STEREO;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)
	{
		gsGlobalInfo.sInitInfo.bLineIn2Dif	= psInitInfo->bLineIn2Dif;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bLineIn2Dif	= MCDRV_LINE_STEREO;
	}
	gsGlobalInfo.sInitInfo.bLineOut1Dif	= psInitInfo->bLineOut1Dif;
	gsGlobalInfo.sInitInfo.bLineOut2Dif	= psInitInfo->bLineOut2Dif;
	gsGlobalInfo.sInitInfo.bSpmn		= psInitInfo->bSpmn;
	gsGlobalInfo.sInitInfo.bMic1Sng		= psInitInfo->bMic1Sng;
	gsGlobalInfo.sInitInfo.bMic2Sng		= psInitInfo->bMic2Sng;
	gsGlobalInfo.sInitInfo.bMic3Sng		= psInitInfo->bMic3Sng;
	gsGlobalInfo.sInitInfo.bPowerMode	= psInitInfo->bPowerMode;
	gsGlobalInfo.sInitInfo.bSpHiz		= psInitInfo->bSpHiz;
	gsGlobalInfo.sInitInfo.bLdo			= psInitInfo->bLdo;
	gsGlobalInfo.sInitInfo.bPad0Func	= psInitInfo->bPad0Func;
	gsGlobalInfo.sInitInfo.bPad1Func	= psInitInfo->bPad1Func;
	if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
	{
		gsGlobalInfo.sInitInfo.bPad2Func	= psInitInfo->bPad2Func;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bPad2Func	= MCDRV_PAD_GPIO;
	}
	gsGlobalInfo.sInitInfo.bAvddLev		= psInitInfo->bAvddLev;
	gsGlobalInfo.sInitInfo.bVrefLev		= psInitInfo->bVrefLev;
	gsGlobalInfo.sInitInfo.bDclGain		= psInitInfo->bDclGain;
	gsGlobalInfo.sInitInfo.bDclLimit	= psInitInfo->bDclLimit;
	if(McDevProf_IsValid(eMCDRV_FUNC_CPMODE) != 0)
	{
		gsGlobalInfo.sInitInfo.bCpMod		= psInitInfo->bCpMod;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bCpMod		= MCDRV_CPMOD_OFF;
	}
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		gsGlobalInfo.sInitInfo.bSdDs		= psInitInfo->bSdDs;
	}
	else
	{
		gsGlobalInfo.sInitInfo.bSdDs		= 0;
	}
	gsGlobalInfo.sInitInfo.bReserved1	= psInitInfo->bReserved1;
	gsGlobalInfo.sInitInfo.bReserved2	= psInitInfo->bReserved2;
	gsGlobalInfo.sInitInfo.bReserved3	= psInitInfo->bReserved3;
	gsGlobalInfo.sInitInfo.bReserved4	= psInitInfo->bReserved4;
	gsGlobalInfo.sInitInfo.sWaitTime	= psInitInfo->sWaitTime;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetInitInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetInitInfo
 *
 *	Description:
 *			Get Initialize information.
 *	Arguments:
 *			psInitInfo	Initialize information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetInitInfo
(
	MCDRV_INIT_INFO*	psInitInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetInitInfo");
#endif

	*psInitInfo	= gsGlobalInfo.sInitInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetInitInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetClockInfo
 *
 *	Description:
 *			Set clock information.
 *	Arguments:
 *			psClockInfo	clock information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetClockInfo
(
	const MCDRV_CLOCK_INFO* psClockInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetClockInfo");
#endif


	gsGlobalInfo.sInitInfo.bCkSel	= psClockInfo->bCkSel;
	gsGlobalInfo.sInitInfo.bDivR0	= psClockInfo->bDivR0;
	gsGlobalInfo.sInitInfo.bDivF0	= psClockInfo->bDivF0;
	gsGlobalInfo.sInitInfo.bDivR1	= psClockInfo->bDivR1;
	gsGlobalInfo.sInitInfo.bDivF1	= psClockInfo->bDivF1;
	if(McDevProf_IsValid(eMCDRV_FUNC_RANGE) != 0)
	{
		gsGlobalInfo.sInitInfo.bRange0	= psClockInfo->bRange0;
		gsGlobalInfo.sInitInfo.bRange1	= psClockInfo->bRange1;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_BYPASS) != 0)
	{
		gsGlobalInfo.sInitInfo.bBypass	= psClockInfo->bBypass;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetClockInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetPathInfo
 *
 *	Description:
 *			Set path information.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
SINT32	McResCtrl_SetPathInfo
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	MCDRV_PATH_INFO	sCurPathInfo		= gsGlobalInfo.sPathInfo;
	MCDRV_PATH_INFO	sCurPathInfoVirtual	= gsGlobalInfo.sPathInfoVirtual;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetPathInfo");
#endif


	gsGlobalInfo.bEFIFO0L	= McResCtrl_GetEFIFO0LSource();
	gsGlobalInfo.bEFIFO0R	= McResCtrl_GetEFIFO0RSource();
	gsGlobalInfo.bEFIFO1L	= McResCtrl_GetEFIFO1LSource();
	gsGlobalInfo.bEFIFO1R	= McResCtrl_GetEFIFO1RSource();
	gsGlobalInfo.sPathInfo	= gsGlobalInfo.sPathInfoVirtual;

	/*	HP source on/off	*/
	SetHPSourceOnOff(psPathInfo);

	/*	SP source on/off	*/
	SetSPSourceOnOff(psPathInfo);

	/*	RCV source on/off	*/
	SetRCVSourceOnOff(psPathInfo);

	/*	LOut1 source on/off	*/
	SetLO1SourceOnOff(psPathInfo);

	/*	LOut2 source on/off	*/
	SetLO2SourceOnOff(psPathInfo);

	/*	Peak source on/off	*/
	SetPMSourceOnOff(psPathInfo);

	/*	DIT0 source on/off	*/
	SetDIT0SourceOnOff(psPathInfo);

	/*	DIT1 source on/off	*/
	SetDIT1SourceOnOff(psPathInfo);

	/*	DIT2 source on/off	*/
	SetDIT2SourceOnOff(psPathInfo);

	/*	DAC source on/off	*/
	SetDACSourceOnOff(psPathInfo);

	/*	AE source on/off	*/
	SetAESourceOnOff(psPathInfo);

	/*	CDSP source on/off	*/
	SetCDSPSourceOnOff(psPathInfo);

	/*	ADC0 source on/off	*/
	SetADC0SourceOnOff(psPathInfo);

	/*	ADC1 source on/off	*/
	SetADC1SourceOnOff(psPathInfo);

	/*	Mix source on/off	*/
	SetMixSourceOnOff(psPathInfo);

	/*	Mix2 source on/off	*/
	SetMix2SourceOnOff(psPathInfo);

	/*	Bias source on/off	*/
	SetBiasSourceOnOff(psPathInfo);

	sdRet	= IsValidPath();
	if(sdRet != MCDRV_SUCCESS)
	{
		gsGlobalInfo.sPathInfo	= sCurPathInfo;
	}
	else
	{
		gsGlobalInfo.sPathInfoVirtual	= gsGlobalInfo.sPathInfo;

		ValidateADC();
		ValidateDAC();

		ValidateAE();
		ValidateMix();
		ValidateAE();

		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			if((McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) == 0)
			&& ((McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC1) != 0) && (McResCtrl_IsDstUsed(eMCDRV_DST_ADC1, eMCDRV_DST_CH0) != 0)))
			{
				gsGlobalInfo.sPathInfo			= sCurPathInfo;
				gsGlobalInfo.sPathInfoVirtual	= sCurPathInfoVirtual;
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetPathInfo", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	IsValidPath
 *
 *	Description:
 *			Check path is valid.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
static SINT32	IsValidPath
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT8	bBlock, bCh;
	UINT8	bMIXCh;
	UINT8	bAESrcInfoIndex	= 0xFF;
	MCDRV_SRC_TYPE	eSrcType, eSrcType2;
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
	McDebugLog_FuncIn("IsValidPath");
#endif

	for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (bAESrcInfoIndex == 0xFF); bBlock++)
	{
		if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
		{
			bAESrcInfoIndex	= bBlock;
		}
	}

	/*	MIX2	*/
	for(bMIXCh = 0; (bMIXCh < MIX2_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bMIXCh++)
	{
		for(bBlock = 0; (bBlock < sizeof(sSrcInfo)/sizeof(sSrcInfo[0])) && (sdRet == MCDRV_SUCCESS); bBlock++)
		{
			if((gsGlobalInfo.sPathInfo.asMix2[bMIXCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
			{
				if(bAESrcInfoIndex != 0xFF)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
				else
				{
					for(bCh = 0; (bCh < PEAK_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
					}
					for(bCh = 0; (bCh < DIT0_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
					}
					for(bCh = 0; (bCh < DIT1_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
					}
					for(bCh = 0; (bCh < DIT2_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
					}
					for(bCh = 0; (bCh < DAC_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							if((bCh == 0) && (sSrcInfo[bBlock].bBlock == MCDRV_SRC_DTMF_BLOCK) && (sSrcInfo[bBlock].bOn == MCDRV_SRC4_DTMF_ON))
							{
							}
							else
							{
								sdRet	= MCDRV_ERROR_ARGUMENT;
							}
						}
					}
					for(bCh = 0; (bCh < CDSP_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
					}
					for(bCh = 0; (bCh < MIX_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
					{
						if((gsGlobalInfo.sPathInfo.asMix[bCh].abSrcOnOff[sSrcInfo[bBlock].bBlock] & sSrcInfo[bBlock].bOn) == sSrcInfo[bBlock].bOn)
						{
							sdRet	= MCDRV_ERROR_ARGUMENT;
						}
					}
				}
			}
		}
	}

	/*	PDM, ADC0, DTMF	*/
	if(sdRet == MCDRV_SUCCESS)
	{
		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) != 0)
		{
			if((McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) != 0)
			|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_DTMF) != 0))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			if((McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) != 0)
			&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_DTMF) != 0))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

	/*	AE, PM, DIT0-2, DAC, C-DSP, MIX	*/
	if(sdRet == MCDRV_SUCCESS)
	{
		if(bAESrcInfoIndex != 0xFF)
		{
			for(bCh = 0; (bCh < PEAK_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bCh = 0; (bCh < DIT0_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bCh = 0; (bCh < DIT1_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bCh = 0; (bCh < DIT2_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bCh = 0; (bCh < DAC_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bCh = 0; (bCh < CDSP_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
			for(bCh = 0; (bCh < MIX_PATH_CHANNELS) && (sdRet == MCDRV_SUCCESS); bCh++)
			{
				if((gsGlobalInfo.sPathInfo.asMix[bCh].abSrcOnOff[sSrcInfo[bAESrcInfoIndex].bBlock] & sSrcInfo[bAESrcInfoIndex].bOn) == sSrcInfo[bAESrcInfoIndex].bOn)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}

	/*	C-DSP[0], [1]	*/
	if(sdRet == MCDRV_SUCCESS)
	{
		eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
		if(eSrcType != eMCDRV_SRC_DIR2_DIRECT)
		{
			eSrcType2	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
			if(eSrcType2 != eMCDRV_SRC_DIR2_DIRECT)
			{
				if(eSrcType != eSrcType2)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}

	/*	AE<->MIX	*/
	if(sdRet == MCDRV_SUCCESS)
	{
		if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & (MCDRV_SRC6_MIX_ON|MCDRV_SRC6_MIX_OFF)) == MCDRV_SRC6_MIX_ON)
		{
			if((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & (MCDRV_SRC6_AE_ON|MCDRV_SRC6_AE_OFF)) == MCDRV_SRC6_AE_ON)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

	/*	DIR2, CDSP_SRC0	*/
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2) != 0)
	{
		if(McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
	}

	/*	DIT2SRC_SOURCE	*/
	eSrcType	= McResCtrl_GetDITSource(eMCDRV_DIO_2);
	if((eSrcType != eMCDRV_SRC_NONE)
	&& (eSrcType != eMCDRV_SRC_CDSP_DIRECT))
	{/*	DIT2SRC0->DIT2	*/
		eSrcType2	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
		if((eSrcType2 != eMCDRV_SRC_NONE)
		&& (eSrcType2 != eMCDRV_SRC_DIR2_DIRECT))
		{/*	DIT2SRC0->EFIFO0L	*/
			if(eSrcType != eSrcType2)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
		else
		{
			eSrcType2	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
			if((eSrcType2 != eMCDRV_SRC_NONE)
			&& (eSrcType2 != eMCDRV_SRC_DIR2_DIRECT))
			{/*	DIT2SRC0->EFIFO0R	*/
				if(eSrcType != eSrcType2)
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
			}
		}
	}

	#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("IsValidPath", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	ValidateADC
 *
 *	Description:
 *			Validate ADC setting.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	ValidateADC
(
	void
)
{
	UINT8	bCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateADC");
#endif

	if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH0) == 0)
	&& (McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH1) == 0))
	{/*	ADC0 source all off	*/
		gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]		&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;

		gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]		|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;

		for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
		{
			gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		}
	}
	else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) == 0)
	{
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK]		&= (UINT8)~MCDRV_SRC0_MIC1_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK]		&= (UINT8)~MCDRV_SRC0_MIC2_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK]		&= (UINT8)~MCDRV_SRC0_MIC3_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= (UINT8)~MCDRV_SRC1_LINE1_L_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	&= (UINT8)~MCDRV_SRC1_LINE1_M_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= (UINT8)~MCDRV_SRC2_LINE2_L_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	&= (UINT8)~MCDRV_SRC2_LINE2_M_ON;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]		&= (UINT8)~MCDRV_SRC5_DAC_L_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK]		&= (UINT8)~MCDRV_SRC0_MIC1_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK]		&= (UINT8)~MCDRV_SRC0_MIC2_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK]		&= (UINT8)~MCDRV_SRC0_MIC3_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= (UINT8)~MCDRV_SRC1_LINE1_R_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	&= (UINT8)~MCDRV_SRC1_LINE1_M_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= (UINT8)~MCDRV_SRC2_LINE2_R_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	&= (UINT8)~MCDRV_SRC2_LINE2_M_ON;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]		&= (UINT8)~MCDRV_SRC5_DAC_R_ON;

		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK]		|= MCDRV_SRC0_MIC1_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK]		|= MCDRV_SRC0_MIC2_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK]		|= MCDRV_SRC0_MIC3_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	|= MCDRV_SRC1_LINE1_L_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	|= MCDRV_SRC1_LINE1_M_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	|= MCDRV_SRC2_LINE2_L_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	|= MCDRV_SRC2_LINE2_M_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]		|= MCDRV_SRC5_DAC_L_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK]		|= MCDRV_SRC0_MIC1_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK]		|= MCDRV_SRC0_MIC2_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK]		|= MCDRV_SRC0_MIC3_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	|= MCDRV_SRC1_LINE1_R_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	|= MCDRV_SRC1_LINE1_M_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	|= MCDRV_SRC2_LINE2_R_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	|= MCDRV_SRC2_LINE2_M_OFF;
		gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]		|= MCDRV_SRC5_DAC_R_OFF;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateADC", 0);
#endif
}

/****************************************************************************
 *	ValidateDAC
 *
 *	Description:
 *			Validate DAC setting.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	ValidateDAC
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateDAC");
#endif

	if((McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_L) == 0)
	&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_M) == 0)
	&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_R) == 0))
	{/*	DAC is unused	*/
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		&= (UINT8)~MCDRV_SRC4_PDM_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC1_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR0_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR1_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR2_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= (UINT8)~MCDRV_SRC4_DTMF_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= (UINT8)~MCDRV_SRC6_CDSP_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		&= (UINT8)~MCDRV_SRC4_PDM_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC1_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR0_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR1_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR2_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= (UINT8)~MCDRV_SRC4_DTMF_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= (UINT8)~MCDRV_SRC6_CDSP_ON;

		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		|= MCDRV_SRC4_PDM_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		|= MCDRV_SRC4_PDM_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;

		ValidateADC();
	}

	if((McResCtrl_GetDACSource(eMCDRV_DAC_MASTER) == eMCDRV_SRC_NONE)
	&& (McResCtrl_GetDACSource(eMCDRV_DAC_VOICE) == eMCDRV_SRC_NONE))
	{
		if((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) != MCDRV_SRC4_DTMF_ON)
		{
			/*	DAC source all off	*/
			gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_L_ON;
			gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_M_ON;
			gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_L_ON;
			gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_M_ON;
			gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_M_ON;
			gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_L_ON;
			gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_L_ON;
			gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_M_ON;
			gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_L_ON;
			gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_M_ON;
			gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;
			gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	&= (UINT8)~MCDRV_SRC5_DAC_R_ON;

			gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_OFF;
			gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_OFF;
			gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_OFF;
			gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_OFF;
			gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_OFF;
			gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_OFF;
			gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_OFF;
			gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_OFF;
			gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	|= MCDRV_SRC5_DAC_L_OFF;
			gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK]	|= MCDRV_SRC5_DAC_M_OFF;
			gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;
			gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK]	|= MCDRV_SRC5_DAC_R_OFF;

			ValidateADC();
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateDAC", 0);
#endif
}

/****************************************************************************
 *	VlidateAE
 *
 *	Description:
 *			Validate AE path setting.
 *	Arguments:
 *			eCh	AE path channel
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	ValidateAE
(
	void
)
{
	UINT8	bCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateAE");
#endif

	if(McResCtrl_GetAESource() == eMCDRV_SRC_NONE)
	{/*	AE source all off	*/
		gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;

		gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;

		for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
		{
			gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= (UINT8)~MCDRV_SRC6_AE_ON;
			gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
		}

		ValidateDAC();
	}
	else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_AE) == 0)
	{
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	&= (UINT8)~MCDRV_SRC4_PDM_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC1_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR0_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR1_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR2_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= (UINT8)~MCDRV_SRC4_DTMF_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= (UINT8)~MCDRV_SRC6_CDSP_ON;

		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
	}
	else
	{
	}

	ValidateMix();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateAE", 0);
#endif
}

/****************************************************************************
 *	ValidateMix
 *
 *	Description:
 *			Validate Mix path setting.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	ValidateMix
(
	void
)
{
	UINT8	bCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ValidateMix");
#endif

	if(McResCtrl_IsDstUsed(eMCDRV_DST_MIX, eMCDRV_DST_CH0) == 0)
	{/*	MIX source all off	*/
		gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		&= (UINT8)~MCDRV_SRC6_MIX_ON;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		&= (UINT8)~MCDRV_SRC6_MIX_ON;

		gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_OFF;
		gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]		|= MCDRV_SRC6_MIX_OFF;

		for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
		{
			gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= (UINT8)~MCDRV_SRC6_MIX_ON;
			gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
		}

		ValidateDAC();
	}
	else if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIX) == 0)
	{
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		&= (UINT8)~MCDRV_SRC4_PDM_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC1_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR0_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR1_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= (UINT8)~MCDRV_SRC3_DIR2_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		&= (UINT8)~MCDRV_SRC6_AE_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= (UINT8)~MCDRV_SRC4_DTMF_ON;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= (UINT8)~MCDRV_SRC6_CDSP_ON;

		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]		|= MCDRV_SRC4_PDM_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
		gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ValidateMix", 0);
#endif
}

/****************************************************************************
 *	SetHPSourceOnOff
 *
 *	Description:
 *			Set HP source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetHPSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,	MCDRV_SRC0_MIC1_ON,	MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,	MCDRV_SRC0_MIC2_ON,	MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,	MCDRV_SRC0_MIC3_ON,	MCDRV_SRC0_MIC3_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetHPSourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++)
		{
			SetSourceOnOff(psPathInfo->asHpOut[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[bCh].abSrcOnOff,
						sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_LINE1_L_BLOCK, MCDRV_SRC1_LINE1_L_ON, MCDRV_SRC1_LINE1_L_OFF);
	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_LINE1_M_BLOCK, MCDRV_SRC1_LINE1_M_ON, MCDRV_SRC1_LINE1_M_OFF);
	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_LINE2_L_BLOCK, MCDRV_SRC2_LINE2_L_ON, MCDRV_SRC2_LINE2_L_OFF);
	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_LINE2_M_BLOCK, MCDRV_SRC2_LINE2_M_ON, MCDRV_SRC2_LINE2_M_OFF);

	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_DAC_L_BLOCK, MCDRV_SRC5_DAC_L_ON, MCDRV_SRC5_DAC_L_OFF);
	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_DAC_M_BLOCK, MCDRV_SRC5_DAC_M_ON, MCDRV_SRC5_DAC_M_OFF);
	SetSourceOnOff(psPathInfo->asHpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff,
					MCDRV_SRC_DAC_R_BLOCK, MCDRV_SRC5_DAC_R_ON, MCDRV_SRC5_DAC_R_OFF);

	SetSourceOnOff(psPathInfo->asHpOut[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff,
					MCDRV_SRC_LINE1_R_BLOCK, MCDRV_SRC1_LINE1_R_ON, MCDRV_SRC1_LINE1_R_OFF);
	SetSourceOnOff(psPathInfo->asHpOut[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff,
					MCDRV_SRC_LINE2_R_BLOCK, MCDRV_SRC2_LINE2_R_ON, MCDRV_SRC2_LINE2_R_OFF);

	SetSourceOnOff(psPathInfo->asHpOut[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff,
					MCDRV_SRC_DAC_R_BLOCK, MCDRV_SRC5_DAC_R_ON, MCDRV_SRC5_DAC_R_OFF);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetHPSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetSPSourceOnOff
 *
 *	Description:
 *			Set SP source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetSPSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF},
		{MCDRV_SRC_LINE1_M_BLOCK,	MCDRV_SRC1_LINE1_M_ON,	MCDRV_SRC1_LINE1_M_OFF},
		{MCDRV_SRC_LINE2_M_BLOCK,	MCDRV_SRC2_LINE2_M_ON,	MCDRV_SRC2_LINE2_M_OFF},
		{MCDRV_SRC_DAC_L_BLOCK,		MCDRV_SRC5_DAC_L_ON,	MCDRV_SRC5_DAC_L_OFF},
		{MCDRV_SRC_DAC_R_BLOCK,		MCDRV_SRC5_DAC_R_ON,	MCDRV_SRC5_DAC_R_OFF},
		{MCDRV_SRC_DAC_M_BLOCK,		MCDRV_SRC5_DAC_M_ON,	MCDRV_SRC5_DAC_M_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetSPSourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++)
		{
			SetSourceOnOff(psPathInfo->asSpOut[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asSpOut[bCh].abSrcOnOff,
							sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

	SetSourceOnOff(psPathInfo->asSpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff,
					MCDRV_SRC_LINE1_L_BLOCK, MCDRV_SRC1_LINE1_L_ON, MCDRV_SRC1_LINE1_L_OFF);
	SetSourceOnOff(psPathInfo->asSpOut[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff,
					MCDRV_SRC_LINE2_L_BLOCK, MCDRV_SRC2_LINE2_L_ON, MCDRV_SRC2_LINE2_L_OFF);

	SetSourceOnOff(psPathInfo->asSpOut[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff,
					MCDRV_SRC_LINE1_R_BLOCK, MCDRV_SRC1_LINE1_R_ON, MCDRV_SRC1_LINE1_R_OFF);
	SetSourceOnOff(psPathInfo->asSpOut[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff,
					MCDRV_SRC_LINE2_R_BLOCK, MCDRV_SRC2_LINE2_R_ON, MCDRV_SRC2_LINE2_R_OFF);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetSPSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetRCVSourceOnOff
 *
 *	Description:
 *			Set RCV source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetRCVSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,		MCDRV_SRC0_MIC1_ON,		MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,		MCDRV_SRC0_MIC2_ON,		MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF},
		{MCDRV_SRC_LINE1_M_BLOCK,	MCDRV_SRC1_LINE1_M_ON,	MCDRV_SRC1_LINE1_M_OFF},
		{MCDRV_SRC_LINE2_M_BLOCK,	MCDRV_SRC2_LINE2_M_ON,	MCDRV_SRC2_LINE2_M_OFF},
		{MCDRV_SRC_DAC_L_BLOCK,		MCDRV_SRC5_DAC_L_ON,	MCDRV_SRC5_DAC_L_OFF},
		{MCDRV_SRC_DAC_R_BLOCK,		MCDRV_SRC5_DAC_R_ON,	MCDRV_SRC5_DAC_R_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetRCVSourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++)
		{
			SetSourceOnOff(psPathInfo->asRcOut[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asRcOut[bCh].abSrcOnOff,
						sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetRCVSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetLO1SourceOnOff
 *
 *	Description:
 *			Set LOut1 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetLO1SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,		MCDRV_SRC0_MIC1_ON,		MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,		MCDRV_SRC0_MIC2_ON,		MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF},
		{MCDRV_SRC_DAC_R_BLOCK,		MCDRV_SRC5_DAC_R_ON,	MCDRV_SRC5_DAC_R_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetLO1SourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
					sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		if(gsGlobalInfo.sInitInfo.bLineOut1Dif != MCDRV_LINE_DIF)
		{
			SetSourceOnOff(psPathInfo->asLout1[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff,
						sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

	SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
				MCDRV_SRC_LINE1_L_BLOCK, MCDRV_SRC1_LINE1_L_ON, MCDRV_SRC1_LINE1_L_OFF);
	SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
				MCDRV_SRC_LINE1_M_BLOCK, MCDRV_SRC1_LINE1_M_ON, MCDRV_SRC1_LINE1_M_OFF);

	SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
				MCDRV_SRC_LINE2_L_BLOCK, MCDRV_SRC2_LINE2_L_ON, MCDRV_SRC2_LINE2_L_OFF);
	SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
				MCDRV_SRC_LINE2_M_BLOCK, MCDRV_SRC2_LINE2_M_ON, MCDRV_SRC2_LINE2_M_OFF);

	SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
				MCDRV_SRC_DAC_L_BLOCK, MCDRV_SRC5_DAC_L_ON, MCDRV_SRC5_DAC_L_OFF);
	SetSourceOnOff(psPathInfo->asLout1[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff,
				MCDRV_SRC_DAC_M_BLOCK, MCDRV_SRC5_DAC_M_ON, MCDRV_SRC5_DAC_M_OFF);

	if(gsGlobalInfo.sInitInfo.bLineOut1Dif != MCDRV_LINE_DIF)
	{
		SetSourceOnOff(psPathInfo->asLout1[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff,
					MCDRV_SRC_LINE1_R_BLOCK, MCDRV_SRC1_LINE1_R_ON, MCDRV_SRC1_LINE1_R_OFF);
		SetSourceOnOff(psPathInfo->asLout1[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff,
					MCDRV_SRC_LINE2_R_BLOCK, MCDRV_SRC2_LINE2_R_ON, MCDRV_SRC2_LINE2_R_OFF);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetLO1SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetLO2SourceOnOff
 *
 *	Description:
 *			Set LOut2 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetLO2SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,		MCDRV_SRC0_MIC1_ON,		MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,		MCDRV_SRC0_MIC2_ON,		MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF},
		{MCDRV_SRC_DAC_R_BLOCK,		MCDRV_SRC5_DAC_R_ON,	MCDRV_SRC5_DAC_R_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetLO2SourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
					sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		if(gsGlobalInfo.sInitInfo.bLineOut2Dif != MCDRV_LINE_DIF)
		{
			SetSourceOnOff(psPathInfo->asLout2[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff,
						sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

	SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
				MCDRV_SRC_LINE1_L_BLOCK, MCDRV_SRC1_LINE1_L_ON, MCDRV_SRC1_LINE1_L_OFF);
	SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
				MCDRV_SRC_LINE1_M_BLOCK, MCDRV_SRC1_LINE1_M_ON, MCDRV_SRC1_LINE1_M_OFF);

	SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
				MCDRV_SRC_LINE2_L_BLOCK, MCDRV_SRC2_LINE2_L_ON, MCDRV_SRC2_LINE2_L_OFF);
	SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
				MCDRV_SRC_LINE2_M_BLOCK, MCDRV_SRC2_LINE2_M_ON, MCDRV_SRC2_LINE2_M_OFF);

	SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
				MCDRV_SRC_DAC_L_BLOCK, MCDRV_SRC5_DAC_L_ON, MCDRV_SRC5_DAC_L_OFF);
	SetSourceOnOff(psPathInfo->asLout2[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff,
				MCDRV_SRC_DAC_M_BLOCK, MCDRV_SRC5_DAC_M_ON, MCDRV_SRC5_DAC_M_OFF);

	if(gsGlobalInfo.sInitInfo.bLineOut2Dif != MCDRV_LINE_DIF)
	{
		SetSourceOnOff(psPathInfo->asLout2[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff,
					MCDRV_SRC_LINE1_R_BLOCK, MCDRV_SRC1_LINE1_R_ON, MCDRV_SRC1_LINE1_R_OFF);
		SetSourceOnOff(psPathInfo->asLout2[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff,
					MCDRV_SRC_LINE2_R_BLOCK, MCDRV_SRC2_LINE2_R_ON, MCDRV_SRC2_LINE2_R_OFF);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetLO2SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetPMSourceOnOff
 *
 *	Description:
 *			Set PeakMeter source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetPMSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,	MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,	MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetPMSourceOnOff");
#endif

	for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asPeak[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asPeak[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((psPathInfo->asPeak[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asPeak[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asPeak[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetPMSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetDIT0SourceOnOff
 *
 *	Description:
 *			Set DIT0 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDIT0SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,	MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,	MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDIT0SourceOnOff");
#endif

	for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asDit0[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asDit0[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((psPathInfo->asDit0[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asDit0[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDit0[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDIT0SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetDIT1SourceOnOff
 *
 *	Description:
 *			Set DIT1 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDIT1SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,	MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,	MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDIT1SourceOnOff");
#endif

	for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asDit1[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asDit1[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((psPathInfo->asDit1[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asDit1[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDit1[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDIT1SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetDIT2SourceOnOff
 *
 *	Description:
 *			Set DIT2 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDIT2SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,			MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,			MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,			MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,			MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,			MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,			MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,			MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,			MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,			MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,			MCDRV_SRC6_CDSP_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_DIRECT_ON,	MCDRV_SRC6_CDSP_DIRECT_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDIT2SourceOnOff");
#endif

	for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asDit2[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asDit2[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((psPathInfo->asDit2[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asDit2[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDit2[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDIT2SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetDACSourceOnOff
 *
 *	Description:
 *			Set DAC source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDACSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	UINT8	bMaster_DTMF_OnOff	= gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,	MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,	MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDACSourceOnOff");
#endif

	for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asDac[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asDac[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((bCh == 0) && (sBlockInfo[bBlock].bBlock == MCDRV_SRC_DTMF_BLOCK) && (sBlockInfo[bBlock].bOn == MCDRV_SRC4_DTMF_ON))
				{
					continue;
				}
				if((psPathInfo->asDac[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asDac[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asDac[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}
	if(bMaster_DTMF_OnOff == MCDRV_SRC4_DTMF_ON)
	{
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= (UINT8)~MCDRV_SRC4_DTMF_OFF;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_ON;
	}
	else
	{
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= (UINT8)~MCDRV_SRC4_DTMF_ON;
		gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
	}
	SetSourceOnOff(psPathInfo->asDac[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff,
				MCDRV_SRC_DTMF_BLOCK, MCDRV_SRC4_DTMF_ON, MCDRV_SRC4_DTMF_OFF);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDACSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetAESourceOnOff
 *
 *	Description:
 *			Set AE source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetAESourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_MIX_BLOCK,	MCDRV_SRC6_MIX_ON,	MCDRV_SRC6_MIX_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetAESourceOnOff");
#endif

	for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asAe[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asAe[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asAe[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asAe[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((psPathInfo->asAe[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asAe[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asAe[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asAe[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetAESourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetCDSPSourceOnOff
 *
 *	Description:
 *			Set CDSP source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetCDSPSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
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
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,			MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetCDSPSourceOnOff");
#endif

	for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++)
	{
		if((psPathInfo->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & (MCDRV_SRC4_PDM_ON|MCDRV_SRC4_PDM_OFF)) == MCDRV_SRC4_PDM_ON)
		{
			if(gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_PDMCK)
			{
				ClearSourceOnOff(gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff);
				SetSourceOnOff(psPathInfo->asCdsp[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff,
							MCDRV_SRC_PDM_BLOCK, MCDRV_SRC4_PDM_ON, MCDRV_SRC4_PDM_OFF);
			}
		}
		else
		{
			for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
			{
				if((psPathInfo->asCdsp[bCh].abSrcOnOff[sBlockInfo[bBlock].bBlock] & (sBlockInfo[bBlock].bOn|sBlockInfo[bBlock].bOff)) == sBlockInfo[bBlock].bOn)
				{
					ClearSourceOnOff(gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff);
				}
				SetSourceOnOff(psPathInfo->asCdsp[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asCdsp[bCh].abSrcOnOff,
								sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetCDSPSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetADC0SourceOnOff
 *
 *	Description:
 *			Set ADC0 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetADC0SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,		MCDRV_SRC0_MIC1_ON,		MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,		MCDRV_SRC0_MIC2_ON,		MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF},
		{MCDRV_SRC_LINE1_M_BLOCK,	MCDRV_SRC1_LINE1_M_ON,	MCDRV_SRC1_LINE1_M_OFF},
		{MCDRV_SRC_LINE2_M_BLOCK,	MCDRV_SRC2_LINE2_M_ON,	MCDRV_SRC2_LINE2_M_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetADC0SourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++)
		{
			SetSourceOnOff(psPathInfo->asAdc0[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[bCh].abSrcOnOff,
						sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

	SetSourceOnOff(psPathInfo->asAdc0[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff,
				MCDRV_SRC_LINE1_L_BLOCK, MCDRV_SRC1_LINE1_L_ON, MCDRV_SRC1_LINE1_L_OFF);
	SetSourceOnOff(psPathInfo->asAdc0[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff,
				MCDRV_SRC_LINE2_L_BLOCK, MCDRV_SRC2_LINE2_L_ON, MCDRV_SRC2_LINE2_L_OFF);

	SetSourceOnOff(psPathInfo->asAdc0[0].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff,
				MCDRV_SRC_DAC_L_BLOCK, MCDRV_SRC5_DAC_L_ON, MCDRV_SRC5_DAC_L_OFF);

	SetSourceOnOff(psPathInfo->asAdc0[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff,
				MCDRV_SRC_LINE1_R_BLOCK, MCDRV_SRC1_LINE1_R_ON, MCDRV_SRC1_LINE1_R_OFF);
	SetSourceOnOff(psPathInfo->asAdc0[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff,
				MCDRV_SRC_LINE2_R_BLOCK, MCDRV_SRC2_LINE2_R_ON, MCDRV_SRC2_LINE2_R_OFF);

	SetSourceOnOff(psPathInfo->asAdc0[1].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff,
				MCDRV_SRC_DAC_R_BLOCK, MCDRV_SRC5_DAC_R_ON, MCDRV_SRC5_DAC_R_OFF);

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetADC0SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetADC1SourceOnOff
 *
 *	Description:
 *			Set ADC1 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetADC1SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,		MCDRV_SRC0_MIC1_ON,		MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,		MCDRV_SRC0_MIC2_ON,		MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF},
		{MCDRV_SRC_LINE1_M_BLOCK,	MCDRV_SRC1_LINE1_M_ON,	MCDRV_SRC1_LINE1_M_OFF},
		{MCDRV_SRC_LINE2_M_BLOCK,	MCDRV_SRC2_LINE2_M_ON,	MCDRV_SRC2_LINE2_M_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetADC1SourceOnOff");
#endif

	for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
	{
		for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++)
		{
			SetSourceOnOff(psPathInfo->asAdc1[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asAdc1[bCh].abSrcOnOff,
						sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}


#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetADC1SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetMixSourceOnOff
 *
 *	Description:
 *			Set Mix source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetMixSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_AE_BLOCK,	MCDRV_SRC6_AE_ON,	MCDRV_SRC6_AE_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetMixSourceOnOff");
#endif

	for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
		{
			SetSourceOnOff(psPathInfo->asMix[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asMix[bCh].abSrcOnOff,
							sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetMixSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetMix2SourceOnOff
 *
 *	Description:
 *			Set Mix2 source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetMix2SourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_PDM_BLOCK,	MCDRV_SRC4_PDM_ON,	MCDRV_SRC4_PDM_OFF},
		{MCDRV_SRC_ADC0_BLOCK,	MCDRV_SRC4_ADC0_ON,	MCDRV_SRC4_ADC0_OFF},
		{MCDRV_SRC_ADC1_BLOCK,	MCDRV_SRC4_ADC1_ON,	MCDRV_SRC4_ADC1_OFF},
		{MCDRV_SRC_DIR0_BLOCK,	MCDRV_SRC3_DIR0_ON,	MCDRV_SRC3_DIR0_OFF},
		{MCDRV_SRC_DIR1_BLOCK,	MCDRV_SRC3_DIR1_ON,	MCDRV_SRC3_DIR1_OFF},
		{MCDRV_SRC_DIR2_BLOCK,	MCDRV_SRC3_DIR2_ON,	MCDRV_SRC3_DIR2_OFF},
		{MCDRV_SRC_DTMF_BLOCK,	MCDRV_SRC4_DTMF_ON,	MCDRV_SRC4_DTMF_OFF},
		{MCDRV_SRC_CDSP_BLOCK,	MCDRV_SRC6_CDSP_ON,	MCDRV_SRC6_CDSP_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetMix2SourceOnOff");
#endif

	for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
		{
			SetSourceOnOff(psPathInfo->asMix2[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asMix2[bCh].abSrcOnOff,
							sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetMix2SourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetExtBiasOnOff
 *
 *	Description:
 *			Set Bias source On/Off.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetBiasSourceOnOff
(
	const MCDRV_PATH_INFO*	psPathInfo
)
{
	UINT8	bBlock, bCh;
	struct
	{
		UINT8	bBlock;
		UINT8	bOn;
		UINT8	bOff;
	} sBlockInfo[]=
	{
		{MCDRV_SRC_MIC1_BLOCK,		MCDRV_SRC0_MIC1_ON,		MCDRV_SRC0_MIC1_OFF},
		{MCDRV_SRC_MIC2_BLOCK,		MCDRV_SRC0_MIC2_ON,		MCDRV_SRC0_MIC2_OFF},
		{MCDRV_SRC_MIC3_BLOCK,		MCDRV_SRC0_MIC3_ON,		MCDRV_SRC0_MIC3_OFF}
	};

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetBiasSourceOnOff");
#endif

	for(bCh = 0; bCh < BIAS_PATH_CHANNELS; bCh++)
	{
		for(bBlock = 0; bBlock < sizeof(sBlockInfo)/sizeof(sBlockInfo[0]); bBlock++)
		{
			SetSourceOnOff(psPathInfo->asBias[bCh].abSrcOnOff, gsGlobalInfo.sPathInfo.asBias[bCh].abSrcOnOff,
							sBlockInfo[bBlock].bBlock, sBlockInfo[bBlock].bOn, sBlockInfo[bBlock].bOff);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetBiasSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	ClearSourceOnOff
 *
 *	Description:
 *			Clear source On/Off.
 *	Arguments:
 *			pbSrcOnOff	source On/Off info
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	ClearSourceOnOff
(
	UINT8*	pbSrcOnOff
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("ClearSourceOnOff");
#endif

	if(pbSrcOnOff == NULL)
	{
	}
	else
	{
		pbSrcOnOff[0]	= (MCDRV_SRC0_MIC1_OFF|MCDRV_SRC0_MIC2_OFF|MCDRV_SRC0_MIC3_OFF);
		pbSrcOnOff[1]	= (MCDRV_SRC1_LINE1_L_OFF|MCDRV_SRC1_LINE1_R_OFF|MCDRV_SRC1_LINE1_M_OFF);
		pbSrcOnOff[2]	= (MCDRV_SRC2_LINE2_L_OFF|MCDRV_SRC2_LINE2_R_OFF|MCDRV_SRC2_LINE2_M_OFF);
		pbSrcOnOff[3]	= (MCDRV_SRC3_DIR0_OFF|MCDRV_SRC3_DIR1_OFF|MCDRV_SRC3_DIR2_OFF|MCDRV_SRC3_DIR2_DIRECT_OFF);
		pbSrcOnOff[4]	= (MCDRV_SRC4_DTMF_OFF|MCDRV_SRC4_PDM_OFF|MCDRV_SRC4_ADC0_OFF|MCDRV_SRC4_ADC1_OFF);
		pbSrcOnOff[5]	= (MCDRV_SRC5_DAC_L_OFF|MCDRV_SRC5_DAC_R_OFF|MCDRV_SRC5_DAC_M_OFF);
		pbSrcOnOff[6]	= (MCDRV_SRC6_MIX_OFF|MCDRV_SRC6_AE_OFF|MCDRV_SRC6_CDSP_OFF|MCDRV_SRC6_CDSP_DIRECT_OFF);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("ClearSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	SetSourceOnOff
 *
 *	Description:
 *			Set source On/Off.
 *	Arguments:
 *			pbSrcOnOff	source On/Off info
 *			pbDstOnOff	destination
 *			bBlock		Block
 *			bOn			On bit
 *			bOff		Off bit
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetSourceOnOff
(
	const UINT8*	pbSrcOnOff,
	UINT8*			pbDstOnOff,
	UINT8			bBlock,
	UINT8			bOn,
	UINT8			bOff
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetSourceOnOff");
#endif

	if((pbSrcOnOff == NULL) || (pbDstOnOff == NULL))
	{
	}
	else
	{
		if((pbSrcOnOff[bBlock] & bOn) != 0)
		{
			if((bBlock == MCDRV_SRC_PDM_BLOCK) && (bOn == MCDRV_SRC4_PDM_ON))
			{
				if((gsGlobalInfo.sInitInfo.bPad0Func != MCDRV_PAD_PDMCK)
				/*|| (gsGlobalInfo.sInitInfo.bPad1Func != MCDRV_PAD_PDMDI)*/)
				{
#if (MCDRV_DEBUG_LEVEL>=4)
					McDebugLog_FuncOut("SetSourceOnOff", 0);
#endif
					return;
				}
			}
			pbDstOnOff[bBlock]	&= (UINT8)~bOff;
			pbDstOnOff[bBlock]	|= bOn;
		}
		else if((pbSrcOnOff[bBlock] & (bOn|bOff)) == bOff)
		{
			pbDstOnOff[bBlock]	&= (UINT8)~bOn;
			pbDstOnOff[bBlock]	|= bOff;
		}
		else
		{
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetSourceOnOff", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetPathInfo
 *
 *	Description:
 *			Get path information.
 *	Arguments:
 *			psPathInfo	path information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetPathInfo
(
	MCDRV_PATH_INFO*	psPathInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetPathInfo");
#endif

	*psPathInfo	= gsGlobalInfo.sPathInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetPathInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetPathInfoVirtual
 *
 *	Description:
 *			Get virtaul path information.
 *	Arguments:
 *			psPathInfo	virtaul path information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetPathInfoVirtual
(
	MCDRV_PATH_INFO*	psPathInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetPathInfoVirtual");
#endif

	*psPathInfo	= gsGlobalInfo.sPathInfoVirtual;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetPathInfoVirtual", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetDioInfo
 *
 *	Description:
 *			Set digital io information.
 *	Arguments:
 *			psDioInfo	digital io information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetDioInfo
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetDioInfo");
#endif


	if((dUpdateInfo & MCDRV_DIO0_COM_UPDATE_FLAG) != 0UL)
	{
		SetDIOCommon(psDioInfo, 0);
	}
	if((dUpdateInfo & MCDRV_DIO1_COM_UPDATE_FLAG) != 0UL)
	{
		SetDIOCommon(psDioInfo, 1);
	}
	if((dUpdateInfo & MCDRV_DIO2_COM_UPDATE_FLAG) != 0UL)
	{
		SetDIOCommon(psDioInfo, 2);
	}

	if((dUpdateInfo & MCDRV_DIO0_DIR_UPDATE_FLAG) != 0UL)
	{
		SetDIODIR(psDioInfo, 0);
	}
	if((dUpdateInfo & MCDRV_DIO1_DIR_UPDATE_FLAG) != 0UL)
	{
		SetDIODIR(psDioInfo, 1);
	}
	if((dUpdateInfo & MCDRV_DIO2_DIR_UPDATE_FLAG) != 0UL)
	{
		SetDIODIR(psDioInfo, 2);
	}

	if((dUpdateInfo & MCDRV_DIO0_DIT_UPDATE_FLAG) != 0UL)
	{
		SetDIODIT(psDioInfo, 0);
	}
	if((dUpdateInfo & MCDRV_DIO1_DIT_UPDATE_FLAG) != 0UL)
	{
		SetDIODIT(psDioInfo, 1);
	}
	if((dUpdateInfo & MCDRV_DIO2_DIT_UPDATE_FLAG) != 0UL)
	{
		SetDIODIT(psDioInfo, 2);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetDioInfo", 0);
#endif
}

/****************************************************************************
 *	SetDIOCommon
 *
 *	Description:
 *			Set digital io common information.
 *	Arguments:
 *			psDioInfo	digital io information
 *			bPort		port number
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDIOCommon
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT8	bPort
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDIOCommon");
#endif

	if((psDioInfo->asPortInfo[bPort].sDioCommon.bMasterSlave == MCDRV_DIO_SLAVE)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bMasterSlave == MCDRV_DIO_MASTER))
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bMasterSlave	= psDioInfo->asPortInfo[bPort].sDioCommon.bMasterSlave;
	}
	if((psDioInfo->asPortInfo[bPort].sDioCommon.bAutoFs == MCDRV_AUTOFS_OFF)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bAutoFs == MCDRV_AUTOFS_ON))
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bAutoFs	= psDioInfo->asPortInfo[bPort].sDioCommon.bAutoFs;
	}
	if((psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_48000)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_44100)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_32000)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_24000)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_22050)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_16000)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_12000)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_11025)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bFs == MCDRV_FS_8000))
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bFs	= psDioInfo->asPortInfo[bPort].sDioCommon.bFs;
	}
	if((psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_64)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_48)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_32)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_512)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_256)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_128)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs == MCDRV_BCKFS_16))
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bBckFs	= psDioInfo->asPortInfo[bPort].sDioCommon.bBckFs;
	}
	if((psDioInfo->asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_DA)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_PCM))
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bInterface	= psDioInfo->asPortInfo[bPort].sDioCommon.bInterface;
	}
	if((psDioInfo->asPortInfo[bPort].sDioCommon.bBckInvert == MCDRV_BCLK_NORMAL)
	|| (psDioInfo->asPortInfo[bPort].sDioCommon.bBckInvert == MCDRV_BCLK_INVERT))
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bBckInvert	= psDioInfo->asPortInfo[bPort].sDioCommon.bBckInvert;
	}
	if(psDioInfo->asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHizTim == MCDRV_PCMHIZTIM_FALLING)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHizTim == MCDRV_PCMHIZTIM_RISING))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bPcmHizTim	= psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHizTim;
		}
		if((psDioInfo->asPortInfo[bPort].sDioCommon.bPcmClkDown == MCDRV_PCM_CLKDOWN_OFF)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bPcmClkDown == MCDRV_PCM_CLKDOWN_HALF))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bPcmClkDown	= psDioInfo->asPortInfo[bPort].sDioCommon.bPcmClkDown;
		}
		if((psDioInfo->asPortInfo[bPort].sDioCommon.bPcmFrame == MCDRV_PCM_SHORTFRAME)
		|| (psDioInfo->asPortInfo[bPort].sDioCommon.bPcmFrame == MCDRV_PCM_LONGFRAME))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bPcmFrame	= psDioInfo->asPortInfo[bPort].sDioCommon.bPcmFrame;
		}
		if(psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHighPeriod <= 31)
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bPcmHighPeriod	= psDioInfo->asPortInfo[bPort].sDioCommon.bPcmHighPeriod;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDIOCommon", 0);
#endif
}

/****************************************************************************
 *	SetDIODIR
 *
 *	Description:
 *			Set digital io dir information.
 *	Arguments:
 *			psDioInfo	digital io information
 *			bPort		port number
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDIODIR
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT8	bPort
)
{
	UINT8	bDIOCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDIODIR");
#endif

	gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.wSrcRate	= psDioInfo->asPortInfo[bPort].sDir.wSrcRate;
	if(gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_DA)
	{
		if((psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel == MCDRV_BITSEL_16)
		|| (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel == MCDRV_BITSEL_20)
		|| (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel == MCDRV_BITSEL_24))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.sDaFormat.bBitSel	= psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bBitSel;
		}
		if((psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode == MCDRV_DAMODE_HEADALIGN)
		|| (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode == MCDRV_DAMODE_I2S)
		|| (psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode == MCDRV_DAMODE_TAILALIGN))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.sDaFormat.bMode	= psDioInfo->asPortInfo[bPort].sDir.sDaFormat.bMode;
		}
	}
	else if(gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bMono == MCDRV_PCM_STEREO)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bMono == MCDRV_PCM_MONO))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.sPcmFormat.bMono	= psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bMono;
		}
		if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder == MCDRV_PCM_MSB_FIRST)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder == MCDRV_PCM_LSB_FIRST)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder == MCDRV_PCM_MSB_FIRST_SIGN)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder == MCDRV_PCM_LSB_FIRST_SIGN)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder == MCDRV_PCM_MSB_FIRST_ZERO)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder == MCDRV_PCM_LSB_FIRST_ZERO))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.sPcmFormat.bOrder	= psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bOrder;
		}
		if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw == MCDRV_PCM_LINEAR)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw == MCDRV_PCM_ALAW)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw == MCDRV_PCM_MULAW))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.sPcmFormat.bLaw	= psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bLaw;
		}
		if((psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_8)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_13)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_14)
		|| (psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_16))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.sPcmFormat.bBitSel	= psDioInfo->asPortInfo[bPort].sDir.sPcmFormat.bBitSel;
		}
	}
	else
	{
	}

	for(bDIOCh = 0; bDIOCh < DIO_CHANNELS; bDIOCh++)
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDir.abSlot[bDIOCh]	= psDioInfo->asPortInfo[bPort].sDir.abSlot[bDIOCh];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDIODIR", 0);
#endif
}

/****************************************************************************
 *	SetDIODIT
 *
 *	Description:
 *			Set digital io dit information.
 *	Arguments:
 *			psDioInfo	digital io information
 *			bPort		port number
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	SetDIODIT
(
	const MCDRV_DIO_INFO*	psDioInfo,
	UINT8	bPort
)
{
	UINT8	bDIOCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetDIODIT");
#endif

	gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.wSrcRate	= psDioInfo->asPortInfo[bPort].sDit.wSrcRate;
	if(gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_DA)
	{
		if((psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel == MCDRV_BITSEL_16)
		|| (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel == MCDRV_BITSEL_20)
		|| (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel == MCDRV_BITSEL_24))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.sDaFormat.bBitSel	= psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bBitSel;
		}
		if((psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode == MCDRV_DAMODE_HEADALIGN)
		|| (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode == MCDRV_DAMODE_I2S)
		|| (psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode == MCDRV_DAMODE_TAILALIGN))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.sDaFormat.bMode	= psDioInfo->asPortInfo[bPort].sDit.sDaFormat.bMode;
		}
	}
	else if(gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDioCommon.bInterface == MCDRV_DIO_PCM)
	{
		if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bMono == MCDRV_PCM_STEREO)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bMono == MCDRV_PCM_MONO))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.sPcmFormat.bMono	= psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bMono;
		}
		if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder == MCDRV_PCM_MSB_FIRST)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder == MCDRV_PCM_LSB_FIRST)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder == MCDRV_PCM_MSB_FIRST_SIGN)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder == MCDRV_PCM_LSB_FIRST_SIGN)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder == MCDRV_PCM_MSB_FIRST_ZERO)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder == MCDRV_PCM_LSB_FIRST_ZERO))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.sPcmFormat.bOrder	= psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bOrder;
		}
		if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw == MCDRV_PCM_LINEAR)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw == MCDRV_PCM_ALAW)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw == MCDRV_PCM_MULAW))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.sPcmFormat.bLaw	= psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bLaw;
		}
		if((psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_8)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_13)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_14)
		|| (psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel == MCDRV_PCM_BITSEL_16))
		{
			gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.sPcmFormat.bBitSel	= psDioInfo->asPortInfo[bPort].sDit.sPcmFormat.bBitSel;
		}
	}
	else
	{
	}

	for(bDIOCh = 0; bDIOCh < DIO_CHANNELS; bDIOCh++)
	{
		gsGlobalInfo.sDioInfo.asPortInfo[bPort].sDit.abSlot[bDIOCh]	= psDioInfo->asPortInfo[bPort].sDit.abSlot[bDIOCh];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetDIODIT", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetDioInfo
 *
 *	Description:
 *			Get digital io information.
 *	Arguments:
 *			psDioInfo	digital io information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetDioInfo
(
	MCDRV_DIO_INFO*	psDioInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetDioInfo");
#endif

	*psDioInfo	= gsGlobalInfo.sDioInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetDioInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetVolInfo
 *
 *	Description:
 *			Update volume.
 *	Arguments:
 *			psVolInfo		volume setting
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetVolInfo
(
	const MCDRV_VOL_INFO*	psVolInfo
)
{
	UINT8	bCh;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetVolInfo");
#endif


	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Ad0[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Ad0[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Ad0[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Ad0Att[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Ad0Att[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Ad0Att[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswA_Ad0[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Ad0[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Ad0[bCh] & 0xFFFE);
		}
	}
	for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Adc1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Adc1[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Adc1[bCh] & 0xFFFE);
		}
	}
	for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Adc1Att[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Adc1Att[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Adc1Att[bCh] & 0xFFFE);
		}
	}
	for(bCh = 0; bCh < AD1_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Ad1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Ad1[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Ad1[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < AENG6_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Aeng6[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Aeng6[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Aeng6[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Pdm[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Pdm[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Pdm[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_SideTone[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_SideTone[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_SideTone[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Dtmfb[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dtmfb[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dtmfb[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_DtmfAtt[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_DtmfAtt[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_DtmfAtt[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Dir0[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir0[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir0[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dir0Att[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir0Att[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir0Att[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dit0[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dit0[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dit0[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Dir1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir1[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir1[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dir1Att[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir1Att[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir1Att[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dit1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dit1[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dit1[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Dir2[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir2[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir2[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dir2Att[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir2Att[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir2Att[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dit2[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dit2[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dit2[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dit21[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dit21[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dit21[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_DacMaster[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_DacMaster[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_DacMaster[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_DacVoice[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_DacVoice[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_DacVoice[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_DacAtt[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_DacAtt[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_DacAtt[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < LIN1_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Lin1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Lin1[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Lin1[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < LIN2_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Lin2[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Lin2[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Lin2[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Mic1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Mic1[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Mic1[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Mic2[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Mic2[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Mic2[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Mic3[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Mic3[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Mic3[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < HP_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Hp[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Hp[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Hp[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < SP_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Sp[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Sp[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Sp[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < RC_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Rc[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Rc[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Rc[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < LOUT1_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Lout1[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Lout1[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Lout1[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < LOUT2_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Lout2[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Lout2[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Lout2[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Mic1Gain[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Mic1Gain[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Mic1Gain[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Mic2Gain[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Mic2Gain[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Mic2Gain[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_Mic3Gain[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_Mic3Gain[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_Mic3Gain[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < HPGAIN_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswA_HpGain[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswA_HpGain[bCh]	= (SINT16)((UINT16)psVolInfo->aswA_HpGain[bCh] & 0xFFFE);
		}
	}

	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		if(((UINT16)psVolInfo->aswD_Adc0AeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Adc0AeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Adc0AeMix[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Adc1AeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Adc1AeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Adc1AeMix[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dir0AeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir0AeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir0AeMix[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dir1AeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir1AeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir1AeMix[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_Dir2AeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_Dir2AeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_Dir2AeMix[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_PdmAeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_PdmAeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_PdmAeMix[bCh] & 0xFFFE);
		}
		if(((UINT16)psVolInfo->aswD_MixAeMix[bCh] & 0x01) != 0)
		{
			gsGlobalInfo.sVolInfo.aswD_MixAeMix[bCh]	= (SINT16)((UINT16)psVolInfo->aswD_MixAeMix[bCh] & 0xFFFE);
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetVolInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetVolInfo
 *
 *	Description:
 *			Get volume setting.
 *	Arguments:
 *			psVolInfo		volume setting
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetVolInfo
(
	MCDRV_VOL_INFO*	psVolInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetVolInfo");
#endif

	*psVolInfo	= gsGlobalInfo.sVolInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetVolInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetDacInfo
 *
 *	Description:
 *			Set DAC information.
 *	Arguments:
 *			psDacInfo	DAC information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetDacInfo
(
	const MCDRV_DAC_INFO*	psDacInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetDacInfo");
#endif


	if((dUpdateInfo & MCDRV_DAC_MSWP_UPDATE_FLAG) != 0UL)
	{
		switch(psDacInfo->bMasterSwap)
		{
		case	MCDRV_DSWAP_OFF:
		case	MCDRV_DSWAP_SWAP:
		case	MCDRV_DSWAP_MUTE:
		case	MCDRV_DSWAP_RMVCENTER:
		case	MCDRV_DSWAP_MONO:
		case	MCDRV_DSWAP_MONOHALF:
		case	MCDRV_DSWAP_BOTHL:
		case	MCDRV_DSWAP_BOTHR:
			gsGlobalInfo.sDacInfo.bMasterSwap	= psDacInfo->bMasterSwap;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_DAC_VSWP_UPDATE_FLAG) != 0UL)
	{
		switch(psDacInfo->bVoiceSwap)
		{
		case	MCDRV_DSWAP_OFF:
		case	MCDRV_DSWAP_SWAP:
		case	MCDRV_DSWAP_MUTE:
		case	MCDRV_DSWAP_RMVCENTER:
		case	MCDRV_DSWAP_MONO:
		case	MCDRV_DSWAP_MONOHALF:
		case	MCDRV_DSWAP_BOTHL:
		case	MCDRV_DSWAP_BOTHR:
			gsGlobalInfo.sDacInfo.bVoiceSwap	= psDacInfo->bVoiceSwap;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_DAC_HPF_UPDATE_FLAG) != 0UL)
	{
		if((psDacInfo->bDcCut == MCDRV_DCCUT_ON) || (psDacInfo->bDcCut == MCDRV_DCCUT_OFF))
		{
			gsGlobalInfo.sDacInfo.bDcCut	= psDacInfo->bDcCut;
		}
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_DACSWAP) != 0)
	{
		if((dUpdateInfo & MCDRV_DAC_DACSWP_UPDATE_FLAG) != 0UL)
		{
			switch(psDacInfo->bDacSwap)
			{
			case	MCDRV_DSWAP_OFF:
			case	MCDRV_DSWAP_SWAP:
			case	MCDRV_DSWAP_MUTE:
			case	MCDRV_DSWAP_RMVCENTER:
			case	MCDRV_DSWAP_MONO:
			case	MCDRV_DSWAP_MONOHALF:
			case	MCDRV_DSWAP_BOTHL:
			case	MCDRV_DSWAP_BOTHR:
				gsGlobalInfo.sDacInfo.bDacSwap	= psDacInfo->bDacSwap;
				break;
			default:
				break;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetDacInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetDacInfo
 *
 *	Description:
 *			Get DAC information.
 *	Arguments:
 *			psDacInfo	DAC information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetDacInfo
(
	MCDRV_DAC_INFO*	psDacInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetDacInfo");
#endif

	*psDacInfo	= gsGlobalInfo.sDacInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetDacInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetAdcInfo
 *
 *	Description:
 *			Set ADC information.
 *	Arguments:
 *			psAdcInfo	ADC information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetAdcInfo
(
	const MCDRV_ADC_INFO*	psAdcInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetAdcInfo");
#endif


	if((dUpdateInfo & MCDRV_ADCADJ_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcInfo->bAgcAdjust)
		{
		case	MCDRV_AGCADJ_24:
		case	MCDRV_AGCADJ_18:
		case	MCDRV_AGCADJ_12:
		case	MCDRV_AGCADJ_0:
			gsGlobalInfo.sAdcInfo.bAgcAdjust	= psAdcInfo->bAgcAdjust;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_ADCAGC_UPDATE_FLAG) != 0UL)
	{
		if((psAdcInfo->bAgcOn == MCDRV_AGC_OFF) || (psAdcInfo->bAgcOn == MCDRV_AGC_ON))
		{
			gsGlobalInfo.sAdcInfo.bAgcOn	= psAdcInfo->bAgcOn;
		}
	}
	if((dUpdateInfo & MCDRV_ADCMONO_UPDATE_FLAG) != 0UL)
	{
		if((psAdcInfo->bMono == MCDRV_ADC_STEREO) || (psAdcInfo->bMono == MCDRV_ADC_MONO))
		{
			gsGlobalInfo.sAdcInfo.bMono	= psAdcInfo->bMono;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetAdcInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetAdcInfo
 *
 *	Description:
 *			Get ADC information.
 *	Arguments:
 *			psAdcInfo	ADC information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetAdcInfo
(
	MCDRV_ADC_INFO*	psAdcInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetAdcInfo");
#endif

	*psAdcInfo	= gsGlobalInfo.sAdcInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetAdcInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetAdcExInfo
 *
 *	Description:
 *			Set ADC_EX information.
 *	Arguments:
 *			psAdcExInfo	ADC_EX information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetAdcExInfo
(
	const MCDRV_ADC_EX_INFO*	psAdcExInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetAdcExInfo");
#endif


	if((dUpdateInfo & MCDRV_AGCENV_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcExInfo->bAgcEnv)
		{
		case	MCDRV_AGCENV_5310:
		case	MCDRV_AGCENV_10650:
		case	MCDRV_AGCENV_21310:
		case	MCDRV_AGCENV_85230:
			gsGlobalInfo.sAdcExInfo.bAgcEnv	= psAdcExInfo->bAgcEnv;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCLVL_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcExInfo->bAgcLvl)
		{
		case	MCDRV_AGCLVL_3:
		case	MCDRV_AGCLVL_6:
		case	MCDRV_AGCLVL_9:
		case	MCDRV_AGCLVL_12:
			gsGlobalInfo.sAdcExInfo.bAgcLvl	= psAdcExInfo->bAgcLvl;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCUPTIM_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcExInfo->bAgcUpTim)
		{
		case	MCDRV_AGCUPTIM_341:
		case	MCDRV_AGCUPTIM_683:
		case	MCDRV_AGCUPTIM_1365:
		case	MCDRV_AGCUPTIM_2730:
			gsGlobalInfo.sAdcExInfo.bAgcUpTim	= psAdcExInfo->bAgcUpTim;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCDWTIM_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcExInfo->bAgcDwTim)
		{
		case	MCDRV_AGCDWTIM_5310:
		case	MCDRV_AGCDWTIM_10650:
		case	MCDRV_AGCDWTIM_21310:
		case	MCDRV_AGCDWTIM_85230:
			gsGlobalInfo.sAdcExInfo.bAgcDwTim	= psAdcExInfo->bAgcDwTim;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTLVL_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcExInfo->bAgcCutLvl)
		{
		case	MCDRV_AGCCUTLVL_OFF:
		case	MCDRV_AGCCUTLVL_80:
		case	MCDRV_AGCCUTLVL_70:
		case	MCDRV_AGCCUTLVL_60:
			gsGlobalInfo.sAdcExInfo.bAgcCutLvl	= psAdcExInfo->bAgcCutLvl;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTTIM_UPDATE_FLAG) != 0UL)
	{
		switch(psAdcExInfo->bAgcCutTim)
		{
		case	MCDRV_AGCCUTTIM_5310:
		case	MCDRV_AGCCUTTIM_10650:
		case	MCDRV_AGCCUTTIM_341000:
		case	MCDRV_AGCCUTTIM_2730000:
			gsGlobalInfo.sAdcExInfo.bAgcCutTim	= psAdcExInfo->bAgcCutTim;
			break;
		default:
			break;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetAdcExInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetAdcExInfo
 *
 *	Description:
 *			Get ADC_EX information.
 *	Arguments:
 *			psAdcExInfo	ADC_EX information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetAdcExInfo
(
	MCDRV_ADC_EX_INFO*	psAdcExInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetAdcExInfo");
#endif

	*psAdcExInfo	= gsGlobalInfo.sAdcExInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetAdcExInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetSpInfo
 *
 *	Description:
 *			Set SP information.
 *	Arguments:
 *			psSpInfo	SP information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetSpInfo
(
	const MCDRV_SP_INFO*	psSpInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetSpInfo");
#endif


	if((psSpInfo->bSwap == MCDRV_SPSWAP_OFF) || (psSpInfo->bSwap == MCDRV_SPSWAP_SWAP))
	{
		gsGlobalInfo.sSpInfo.bSwap	= psSpInfo->bSwap;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetSpInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetSpInfo
 *
 *	Description:
 *			Get SP information.
 *	Arguments:
 *			psSpInfo	SP information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetSpInfo
(
	MCDRV_SP_INFO*	psSpInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetSpInfo");
#endif

	*psSpInfo	= gsGlobalInfo.sSpInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetSpInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetDngInfo
 *
 *	Description:
 *			Set Digital Noise Gate information.
 *	Arguments:
 *			psDngInfo	DNG information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetDngInfo
(
	const MCDRV_DNG_INFO*	psDngInfo,
	UINT32	dUpdateInfo
)
{
	UINT8	bItem;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetDngInfo");
#endif


	for(bItem = MCDRV_DNG_ITEM_HP; bItem <= MCDRV_DNG_ITEM_RC; bItem++)
	{
		if((dUpdateInfo & (MCDRV_DNGSW_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
		{
			if((psDngInfo->abOnOff[bItem] == MCDRV_DNG_OFF) || (psDngInfo->abOnOff[bItem] == MCDRV_DNG_ON))
			{
				gsGlobalInfo.sDngInfo.abOnOff[bItem]	= psDngInfo->abOnOff[bItem];
			}
		}
		if(gsGlobalInfo.sDngInfo.abOnOff[bItem] == MCDRV_DNG_ON)
		{
			if((dUpdateInfo & (MCDRV_DNGTHRES_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
			{
				switch(psDngInfo->abThreshold[bItem])
				{
				case	MCDRV_DNG_THRES_30:
				case	MCDRV_DNG_THRES_36:
				case	MCDRV_DNG_THRES_42:
				case	MCDRV_DNG_THRES_48:
				case	MCDRV_DNG_THRES_54:
				case	MCDRV_DNG_THRES_60:
				case	MCDRV_DNG_THRES_66:
				case	MCDRV_DNG_THRES_72:
				case	MCDRV_DNG_THRES_78:
				case	MCDRV_DNG_THRES_84:
					gsGlobalInfo.sDngInfo.abThreshold[bItem]	= psDngInfo->abThreshold[bItem];
					break;
				default:
					break;
				}
			}
			if((dUpdateInfo & (MCDRV_DNGHOLD_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
			{
				switch(psDngInfo->abHold[bItem])
				{
				case	MCDRV_DNG_HOLD_30:
				case	MCDRV_DNG_HOLD_120:
				case	MCDRV_DNG_HOLD_500:
					gsGlobalInfo.sDngInfo.abHold[bItem]	= psDngInfo->abHold[bItem];
					break;
				default:
					break;
				}
			}
			if((dUpdateInfo & (MCDRV_DNGATK_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
			{
				switch(psDngInfo->abAttack[bItem])
				{
				case	MCDRV_DNG_ATTACK_25:
				case	MCDRV_DNG_ATTACK_100:
				case	MCDRV_DNG_ATTACK_400:
				case	MCDRV_DNG_ATTACK_800:
					gsGlobalInfo.sDngInfo.abAttack[bItem]	= psDngInfo->abAttack[bItem];
					break;
				default:
					break;
				}
			}
			if((dUpdateInfo & (MCDRV_DNGREL_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
			{
				switch(psDngInfo->abRelease[bItem])
				{
				case	MCDRV_DNG_RELEASE_7950:
				case	MCDRV_DNG_RELEASE_470:
				case	MCDRV_DNG_RELEASE_940:
					gsGlobalInfo.sDngInfo.abRelease[bItem]	= psDngInfo->abRelease[bItem];
					break;
				default:
					break;
				}
			}
		}
		if((dUpdateInfo & (MCDRV_DNGTARGET_HP_UPDATE_FLAG<<(8*bItem))) != 0UL)
		{
			switch(psDngInfo->abTarget[bItem])
			{
			case	MCDRV_DNG_TARGET_6:
			case	MCDRV_DNG_TARGET_9:
			case	MCDRV_DNG_TARGET_12:
			case	MCDRV_DNG_TARGET_15:
			case	MCDRV_DNG_TARGET_18:
			case	MCDRV_DNG_TARGET_MUTE:
				gsGlobalInfo.sDngInfo.abTarget[bItem]	= psDngInfo->abTarget[bItem];
				break;
			default:
				break;
			}
		}
	}

	if((McDevProf_GetDevId() == eMCDRV_DEV_ID_46H) || (McDevProf_GetDevId() == eMCDRV_DEV_ID_44H))
	{
		if((dUpdateInfo & MCDRV_DNGFW_FLAG) != 0UL)
		{
			if((psDngInfo->bFw == MCDRV_DNG_FW_OFF) || (psDngInfo->bFw == MCDRV_DNG_FW_ON))
			{
				gsGlobalInfo.sDngInfo.bFw	= psDngInfo->bFw;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetDngInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetDngInfo
 *
 *	Description:
 *			Get Digital Noise Gate information.
 *	Arguments:
 *			psDngInfo	DNG information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetDngInfo
(
	MCDRV_DNG_INFO*	psDngInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetDngInfo");
#endif

	*psDngInfo	= gsGlobalInfo.sDngInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetDngInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetAeInfo
 *
 *	Description:
 *			Set Audio Engine information.
 *	Arguments:
 *			psAeInfo	AE information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetAeInfo
(
	const MCDRV_AE_INFO*	psAeInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetAeInfo");
#endif


	if((McDevProf_IsValid(eMCDRV_FUNC_DBEX) != 0)
	&& ((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEXWIDE_ONOFF) != 0UL))
	{
		if((psAeInfo->bOnOff & MCDRV_BEXWIDE_ON) != 0)
		{
			gsGlobalInfo.sAeInfo.bOnOff	|= MCDRV_BEXWIDE_ON;
		}
		else
		{
			gsGlobalInfo.sAeInfo.bOnOff &= (UINT8)~MCDRV_BEXWIDE_ON;
		}
	}
	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC_ONOFF) != 0UL)
	{
		if((psAeInfo->bOnOff & MCDRV_DRC_ON) != 0)
		{
			gsGlobalInfo.sAeInfo.bOnOff	|= MCDRV_DRC_ON;
		}
		else
		{
			gsGlobalInfo.sAeInfo.bOnOff &= (UINT8)~MCDRV_DRC_ON;
		}
	}
	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5_ONOFF) != 0UL)
	{
		if((psAeInfo->bOnOff & MCDRV_EQ5_ON) != 0)
		{
			gsGlobalInfo.sAeInfo.bOnOff	|= MCDRV_EQ5_ON;
		}
		else
		{
			gsGlobalInfo.sAeInfo.bOnOff &= (UINT8)~MCDRV_EQ5_ON;
		}
	}
	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3_ONOFF) != 0UL)
	{
		if((psAeInfo->bOnOff & MCDRV_EQ3_ON) != 0)
		{
			gsGlobalInfo.sAeInfo.bOnOff	|= MCDRV_EQ3_ON;
		}
		else
		{
			gsGlobalInfo.sAeInfo.bOnOff &= (UINT8)~MCDRV_EQ3_ON;
		}
	}

	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_BEX) != 0UL)
	{
		McSrv_MemCopy(psAeInfo->abBex, gsGlobalInfo.sAeInfo.abBex, BEX_PARAM_SIZE);
	}

	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_WIDE) != 0UL)
	{
		McSrv_MemCopy(psAeInfo->abWide, gsGlobalInfo.sAeInfo.abWide, WIDE_PARAM_SIZE);
	}

	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_DRC) != 0UL)
	{
		McSrv_MemCopy(psAeInfo->abDrc, gsGlobalInfo.sAeInfo.abDrc, DRC_PARAM_SIZE);
		McSrv_MemCopy(psAeInfo->abDrc2, gsGlobalInfo.sAeInfo.abDrc2, DRC2_PARAM_SIZE);
	}

	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ5) != 0UL)
	{
		McSrv_MemCopy(psAeInfo->abEq5, gsGlobalInfo.sAeInfo.abEq5, EQ5_PARAM_SIZE);
	}

	if((dUpdateInfo & MCDRV_AEUPDATE_FLAG_EQ3) != 0UL)
	{
		McSrv_MemCopy(psAeInfo->abEq3, gsGlobalInfo.sAeInfo.abEq3, EQ3_PARAM_SIZE);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetAeInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetAeInfo
 *
 *	Description:
 *			Get Audio Engine information.
 *	Arguments:
 *			psAeInfo	AE information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetAeInfo
(
	MCDRV_AE_INFO*	psAeInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetAeInfo");
#endif

	*psAeInfo	= gsGlobalInfo.sAeInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetAeInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetAeEx
 *
 *	Description:
 *			Set AudioEngine ex program.
 *	Arguments:
 *			pbPrm	pointer to Audio Engine ex program
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetAeEx
(
	const UINT8*	pbPrm
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetAeEx");
#endif

	gsGlobalInfo.pbAeEx	= pbPrm;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetAeEx", (SINT32*)pbPrm);
#endif
}

/****************************************************************************
 *	McResCtrl_GetAeEx
 *
 *	Description:
 *			Get AudioEngine ex program.
 *	Arguments:
 *			none
 *	Return:
 *			pointer to Audio Engine ex program
 *
 ****************************************************************************/
const UINT8*	McResCtrl_GetAeEx
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetAeEx");
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetAeEx", (SINT32*)gsGlobalInfo.pbAeEx);
#endif
	return gsGlobalInfo.pbAeEx;
}

/****************************************************************************
 *	McResCtrl_SetCdspFw
 *
 *	Description:
 *			Set C-DSP program.
 *	Arguments:
 *			pbPrm	pointer to C-DSP program
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetCdspFw
(
	const UINT8*	pbPrm
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetCdspFw");
#endif

	gsGlobalInfo.pbCdspFw	= pbPrm;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetCdspFw", (SINT32*)pbPrm);
#endif
}

/****************************************************************************
 *	McResCtrl_GetCdspFw
 *
 *	Description:
 *			Get C-DSP program.
 *	Arguments:
 *			none
 *	Return:
 *			pointer to C-DSP program
 *
 ****************************************************************************/
const UINT8*	McResCtrl_GetCdspFw
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetCdspFw");
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetCdspFw", (SINT32*)gsGlobalInfo.pbCdspFw);
#endif
	return gsGlobalInfo.pbCdspFw;
}

/****************************************************************************
 *	McResCtrl_SetCdspFs
 *
 *	Description:
 *			Set C-DSP Fs.
 *	Arguments:
 *			bCdspFs	C-DSP Fs
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetCdspFs
(
	UINT8	bCdspFs
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet	= bCdspFs;
	McDebugLog_FuncIn("McResCtrl_SetCdspFs");
#endif

	gsGlobalInfo.bCdspFs	= bCdspFs;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetCdspFs", &sdRet);
#endif
}

/****************************************************************************
 *	McResCtrl_GetCdspFs
 *
 *	Description:
 *			Get C-DSP Fs.
 *	Arguments:
 *			none
 *	Return:
 *			C-DSP Fs
 *
 ****************************************************************************/
UINT8	McResCtrl_GetCdspFs
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet	= gsGlobalInfo.bCdspFs;
	McDebugLog_FuncIn("McResCtrl_GetCdspFs");
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetCdspFs", &sdRet);
#endif
	return gsGlobalInfo.bCdspFs;
}

/****************************************************************************
 *	McResCtrl_SetPdmInfo
 *
 *	Description:
 *			Set PDM information.
 *	Arguments:
 *			psPdmInfo	PDM information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetPdmInfo
(
	const MCDRV_PDM_INFO*	psPdmInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetPdmInfo");
#endif


	if((dUpdateInfo & MCDRV_PDMCLK_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bClk)
		{
		case	MCDRV_PDM_CLK_128:
		case	MCDRV_PDM_CLK_64:
		case	MCDRV_PDM_CLK_32:
			gsGlobalInfo.sPdmInfo.bClk	= psPdmInfo->bClk;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_PDMADJ_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bAgcAdjust)
		{
		case	MCDRV_AGCADJ_24:
		case	MCDRV_AGCADJ_18:
		case	MCDRV_AGCADJ_12:
		case	MCDRV_AGCADJ_0:
			gsGlobalInfo.sPdmInfo.bAgcAdjust	= psPdmInfo->bAgcAdjust;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_PDMAGC_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bAgcOn)
		{
		case	MCDRV_AGC_OFF:
		case	MCDRV_AGC_ON:
			gsGlobalInfo.sPdmInfo.bAgcOn	= psPdmInfo->bAgcOn;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_PDMEDGE_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bPdmEdge)
		{
		case	MCDRV_PDMEDGE_LH:
		case	MCDRV_PDMEDGE_HL:
			gsGlobalInfo.sPdmInfo.bPdmEdge	= psPdmInfo->bPdmEdge;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_PDMWAIT_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bPdmWait)
		{
		case	MCDRV_PDMWAIT_0:
		case	MCDRV_PDMWAIT_1:
		case	MCDRV_PDMWAIT_10:
		case	MCDRV_PDMWAIT_20:
			gsGlobalInfo.sPdmInfo.bPdmWait	= psPdmInfo->bPdmWait;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_PDMSEL_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bPdmSel)
		{
		case	MCDRV_PDMSEL_L1R2:
		case	MCDRV_PDMSEL_L2R1:
		case	MCDRV_PDMSEL_L1R1:
		case	MCDRV_PDMSEL_L2R2:
			gsGlobalInfo.sPdmInfo.bPdmSel	= psPdmInfo->bPdmSel;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_PDMMONO_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmInfo->bMono)
		{
		case	MCDRV_PDM_STEREO:
		case	MCDRV_PDM_MONO:
			gsGlobalInfo.sPdmInfo.bMono	= psPdmInfo->bMono;
			break;
		default:
			break;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetPdmInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetPdmInfo
 *
 *	Description:
 *			Get PDM information.
 *	Arguments:
 *			psPdmInfo	PDM information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetPdmInfo
(
	MCDRV_PDM_INFO*	psPdmInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetPdmInfo");
#endif

	*psPdmInfo	= gsGlobalInfo.sPdmInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetPdmInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetPdmExInfo
 *
 *	Description:
 *			Set PDM_EX information.
 *	Arguments:
 *			psPdmExInfo	PDM_EX information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetPdmExInfo
(
	const MCDRV_PDM_EX_INFO*	psPdmExInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetPdmExInfo");
#endif


	if((dUpdateInfo & MCDRV_AGCENV_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmExInfo->bAgcEnv)
		{
		case	MCDRV_AGCENV_5310:
		case	MCDRV_AGCENV_10650:
		case	MCDRV_AGCENV_21310:
		case	MCDRV_AGCENV_85230:
			gsGlobalInfo.sPdmExInfo.bAgcEnv	= psPdmExInfo->bAgcEnv;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCLVL_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmExInfo->bAgcLvl)
		{
		case	MCDRV_AGCLVL_3:
		case	MCDRV_AGCLVL_6:
		case	MCDRV_AGCLVL_9:
		case	MCDRV_AGCLVL_12:
			gsGlobalInfo.sPdmExInfo.bAgcLvl	= psPdmExInfo->bAgcLvl;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCUPTIM_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmExInfo->bAgcUpTim)
		{
		case	MCDRV_AGCUPTIM_341:
		case	MCDRV_AGCUPTIM_683:
		case	MCDRV_AGCUPTIM_1365:
		case	MCDRV_AGCUPTIM_2730:
			gsGlobalInfo.sPdmExInfo.bAgcUpTim	= psPdmExInfo->bAgcUpTim;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCDWTIM_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmExInfo->bAgcDwTim)
		{
		case	MCDRV_AGCDWTIM_5310:
		case	MCDRV_AGCDWTIM_10650:
		case	MCDRV_AGCDWTIM_21310:
		case	MCDRV_AGCDWTIM_85230:
			gsGlobalInfo.sPdmExInfo.bAgcDwTim	= psPdmExInfo->bAgcDwTim;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTLVL_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmExInfo->bAgcCutLvl)
		{
		case	MCDRV_AGCCUTLVL_OFF:
		case	MCDRV_AGCCUTLVL_80:
		case	MCDRV_AGCCUTLVL_70:
		case	MCDRV_AGCCUTLVL_60:
			gsGlobalInfo.sPdmExInfo.bAgcCutLvl	= psPdmExInfo->bAgcCutLvl;
			break;
		default:
			break;
		}
	}
	if((dUpdateInfo & MCDRV_AGCCUTTIM_UPDATE_FLAG) != 0UL)
	{
		switch(psPdmExInfo->bAgcCutTim)
		{
		case	MCDRV_AGCCUTTIM_5310:
		case	MCDRV_AGCCUTTIM_10650:
		case	MCDRV_AGCCUTTIM_341000:
		case	MCDRV_AGCCUTTIM_2730000:
			gsGlobalInfo.sPdmExInfo.bAgcCutTim	= psPdmExInfo->bAgcCutTim;
			break;
		default:
			break;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetPdmExInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetPdmExInfo
 *
 *	Description:
 *			Get PDM_EX information.
 *	Arguments:
 *			psPdmExInfo	PDM_EX information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetPdmExInfo
(
	MCDRV_PDM_EX_INFO*	psPdmExInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetPdmExInfo");
#endif

	*psPdmExInfo	= gsGlobalInfo.sPdmExInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetPdmExInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetDtmfInfo
 *
 *	Description:
 *			Set DTMF information.
 *	Arguments:
 *			psDtmfInfo	DTMF information
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetDtmfInfo
(
	const MCDRV_DTMF_INFO*	psDtmfInfo,
	UINT32	dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetDtmfInfo");
#endif


	if((dUpdateInfo & MCDRV_DTMFHOST_UPDATE_FLAG) != 0UL)
	{
		switch(psDtmfInfo->bSinGenHost)
		{
		case	MCDRV_DTMFHOST_PAD0:
		case	MCDRV_DTMFHOST_PAD1:
		case	MCDRV_DTMFHOST_PAD2:
			/*gsGlobalInfo.sDtmfInfo.bOnOff	= MCDRV_DTMF_OFF;*/
		case	MCDRV_DTMFHOST_REG:
			gsGlobalInfo.sDtmfInfo.bSinGenHost	= psDtmfInfo->bSinGenHost;
			break;
		default:
			break;
		}
	}

	if((dUpdateInfo & MCDRV_DTMFONOFF_UPDATE_FLAG) != 0UL)
	{
		if(gsGlobalInfo.sDtmfInfo.bSinGenHost == MCDRV_DTMFHOST_REG)
		{
			gsGlobalInfo.sDtmfInfo.bOnOff	= psDtmfInfo->bOnOff;
		}
	}

	if(gsGlobalInfo.sDtmfInfo.bOnOff == MCDRV_DTMF_ON)
	{
		if((dUpdateInfo & MCDRV_DTMFFREQ0_UPDATE_FLAG) != 0UL)
		{
			gsGlobalInfo.sDtmfInfo.sParam.wSinGen0Freq	= psDtmfInfo->sParam.wSinGen0Freq;
		}
		if((dUpdateInfo & MCDRV_DTMFFREQ1_UPDATE_FLAG) != 0UL)
		{
			gsGlobalInfo.sDtmfInfo.sParam.wSinGen1Freq	= psDtmfInfo->sParam.wSinGen1Freq;
		}

		if((dUpdateInfo & MCDRV_DTMFVOL0_UPDATE_FLAG) != 0UL)
		{
			gsGlobalInfo.sDtmfInfo.sParam.swSinGen0Vol	= psDtmfInfo->sParam.swSinGen0Vol;
		}
		if((dUpdateInfo & MCDRV_DTMFVOL1_UPDATE_FLAG) != 0UL)
		{
			gsGlobalInfo.sDtmfInfo.sParam.swSinGen1Vol	= psDtmfInfo->sParam.swSinGen1Vol;
		}

		if((dUpdateInfo & MCDRV_DTMFGATE_UPDATE_FLAG) != 0UL)
		{
			gsGlobalInfo.sDtmfInfo.sParam.bSinGenGate	= psDtmfInfo->sParam.bSinGenGate;
		}

		if((dUpdateInfo & MCDRV_DTMFLOOP_UPDATE_FLAG) != 0UL)
		{
			gsGlobalInfo.sDtmfInfo.sParam.bSinGenLoop	= psDtmfInfo->sParam.bSinGenLoop;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetDtmfInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetDtmfInfo
 *
 *	Description:
 *			Get DTMF information.
 *	Arguments:
 *			psDtmfInfo	DTMF information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetDtmfInfo
(
	MCDRV_DTMF_INFO*	psDtmfInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetDtmfInfo");
#endif

	*psDtmfInfo	= gsGlobalInfo.sDtmfInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetDtmfInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetGPMode
 *
 *	Description:
 *			Set GP mode.
 *	Arguments:
 *			psGpMode	GP mode
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetGPMode
(
	const MCDRV_GP_MODE*	psGpMode
)
{
	UINT8	bPad;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetGPMode");
#endif


	for(bPad = 0; bPad < GPIO_PAD_NUM; bPad++)
	{
		if(bPad == MCDRV_GP_PAD2)
		{
			if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) == 0)
			{
				continue;
			}
		}

		if((psGpMode->abGpDdr[bPad] == MCDRV_GPDDR_IN)
		|| (psGpMode->abGpDdr[bPad] == MCDRV_GPDDR_OUT))
		{
			gsGlobalInfo.sGpMode.abGpDdr[bPad]	= psGpMode->abGpDdr[bPad];
			if(psGpMode->abGpDdr[bPad] == MCDRV_GPDDR_IN)
			{
				gsGlobalInfo.abGpPad[bPad]	= 0;
			}
		}
		if(McDevProf_IsValid(eMCDRV_FUNC_IRQ) != 0)
		{
			if((psGpMode->abGpIrq[bPad] == MCDRV_GPIRQ_DISABLE)
			|| (psGpMode->abGpIrq[bPad] == MCDRV_GPIRQ_ENABLE))
			{
				gsGlobalInfo.sGpMode.abGpIrq[bPad]	= psGpMode->abGpIrq[bPad];
			}
		}
		if(McDevProf_IsValid(eMCDRV_FUNC_GPMODE) != 0)
		{
			if((psGpMode->abGpMode[bPad] == MCDRV_GPMODE_RISING)
			|| (psGpMode->abGpMode[bPad] == MCDRV_GPMODE_FALLING)
			|| (psGpMode->abGpMode[bPad] == MCDRV_GPMODE_BOTH))
			{
				gsGlobalInfo.sGpMode.abGpMode[bPad]	= psGpMode->abGpMode[bPad];
			}
			if((psGpMode->abGpHost[bPad] == MCDRV_GPHOST_SCU)
			|| (psGpMode->abGpHost[bPad] == MCDRV_GPHOST_CDSP))
			{
				gsGlobalInfo.sGpMode.abGpHost[bPad]	= psGpMode->abGpHost[bPad];
			}
			if((psGpMode->abGpInvert[bPad] == MCDRV_GPINV_NORMAL)
			|| (psGpMode->abGpInvert[bPad] == MCDRV_GPINV_INVERT))
			{
				gsGlobalInfo.sGpMode.abGpInvert[bPad]	= psGpMode->abGpInvert[bPad];
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetGPMode", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetGPMode
 *
 *	Description:
 *			Get GP mode.
 *	Arguments:
 *			psGpMode	GP mode
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetGPMode
(
	MCDRV_GP_MODE*	psGpMode
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetGPMode");
#endif

	*psGpMode	= gsGlobalInfo.sGpMode;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetGPMode", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetGPMask
 *
 *	Description:
 *			Set GP mask.
 *	Arguments:
 *			bMask	GP mask
 *			dPadNo	PAD Number
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetGPMask
(
	UINT8	bMask,
	UINT32	dPadNo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetGPMask");
#endif

	if(dPadNo == MCDRV_GP_PAD0)
	{
		if((gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_GPIO)
		&& (gsGlobalInfo.sGpMode.abGpDdr[dPadNo] == MCDRV_GPDDR_IN))
		{
			if((bMask == MCDRV_GPMASK_ON) || (bMask == MCDRV_GPMASK_OFF))
			{
				gsGlobalInfo.abGpMask[dPadNo]	= bMask;
			}
		}
	}
	else if(dPadNo == MCDRV_GP_PAD1)
	{
		if((gsGlobalInfo.sInitInfo.bPad1Func == MCDRV_PAD_GPIO)
		&& (gsGlobalInfo.sGpMode.abGpDdr[dPadNo] == MCDRV_GPDDR_IN))
		{
			if((bMask == MCDRV_GPMASK_ON) || (bMask == MCDRV_GPMASK_OFF))
			{
				gsGlobalInfo.abGpMask[dPadNo]	= bMask;
			}
		}
	}
	else
	{
		if(dPadNo == MCDRV_GP_PAD2)
		{
			if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
			{
				if((gsGlobalInfo.sInitInfo.bPad2Func == MCDRV_PAD_GPIO)
				&& (gsGlobalInfo.sGpMode.abGpDdr[dPadNo] == MCDRV_GPDDR_IN))
				{
					if((bMask == MCDRV_GPMASK_ON) || (bMask == MCDRV_GPMASK_OFF))
					{
						gsGlobalInfo.abGpMask[dPadNo]	= bMask;
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetGPMask", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetGPMask
 *
 *	Description:
 *			Get GP mask.
 *	Arguments:
 *			pabMask	GP mask
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetGPMask
(
	UINT8*	pabMask
)
{
	UINT8	bPadNo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetGPMask");
#endif

	for(bPadNo = 0; bPadNo < GPIO_PAD_NUM; bPadNo++)
	{
		pabMask[bPadNo]	= gsGlobalInfo.abGpMask[bPadNo];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetGPMask", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetGPPad
 *
 *	Description:
 *			Set GP pad.
 *	Arguments:
 *			bPad	GP pad value
 *			dPadNo	PAD Number
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetGPPad
(
	UINT8	bPad,
	UINT32	dPadNo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetGPPad");
#endif

	if(dPadNo == MCDRV_GP_PAD0)
	{
		if((gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_GPIO)
		&& (gsGlobalInfo.sGpMode.abGpDdr[dPadNo] == MCDRV_GPDDR_OUT)
		&& (gsGlobalInfo.sGpMode.abGpHost[dPadNo] == MCDRV_GPHOST_SCU))
		{
			if((bPad == MCDRV_GP_LOW) || (bPad == MCDRV_GP_HIGH))
			{
				gsGlobalInfo.abGpPad[dPadNo]	= bPad;
			}
		}
	}
	else if(dPadNo == MCDRV_GP_PAD1)
	{
		if((gsGlobalInfo.sInitInfo.bPad1Func == MCDRV_PAD_GPIO)
		&& (gsGlobalInfo.sGpMode.abGpDdr[dPadNo] == MCDRV_GPDDR_OUT)
		&& (gsGlobalInfo.sGpMode.abGpHost[dPadNo] == MCDRV_GPHOST_SCU))
		{
			if((bPad == MCDRV_GP_LOW) || (bPad == MCDRV_GP_HIGH))
			{
				gsGlobalInfo.abGpPad[dPadNo]	= bPad;
			}
		}
	}
	else
	{
		if(dPadNo == MCDRV_GP_PAD2)
		{
			if(McDevProf_IsValid(eMCDRV_FUNC_PAD2) != 0)
			{
				if((gsGlobalInfo.sInitInfo.bPad2Func == MCDRV_PAD_GPIO)
				&& (gsGlobalInfo.sGpMode.abGpDdr[dPadNo] == MCDRV_GPDDR_OUT)
				&& (gsGlobalInfo.sGpMode.abGpHost[dPadNo] == MCDRV_GPHOST_SCU))
				{
					if((bPad == MCDRV_GP_LOW) || (bPad == MCDRV_GP_HIGH))
					{
						gsGlobalInfo.abGpPad[dPadNo]	= bPad;
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetGPPad", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetGPPad
 *
 *	Description:
 *			Get GP pad value.
 *	Arguments:
 *			pabPad	GP pad value
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetGPPad
(
	UINT8*	pabPad
)
{
	UINT8	bPadNo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetGPPad");
#endif

	for(bPadNo = 0; bPadNo < GPIO_PAD_NUM; bPadNo++)
	{
		pabPad[bPadNo]	= gsGlobalInfo.abGpPad[bPadNo];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetGPPad", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetSysEq
 *
 *	Description:
 *			Set System EQ.
 *	Arguments:
 *			psSysEq		pointer to MCDRV_SYSEQ_INFO struct
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetSysEq
(
	const MCDRV_SYSEQ_INFO*	psSysEq,
	UINT32					dUpdateInfo
)
{
	UINT8	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetSysEq");
#endif

	if((dUpdateInfo & MCDRV_SYSEQ_ONOFF_UPDATE_FLAG) != 0UL)
	{
		if((psSysEq->bOnOff == MCDRV_SYSEQ_OFF) || (psSysEq->bOnOff == MCDRV_SYSEQ_ON))
		{
			gsGlobalInfo.sSysEq.bOnOff	= psSysEq->bOnOff;
		}
	}

	if((dUpdateInfo & MCDRV_SYSEQ_PARAM_UPDATE_FLAG) != 0UL)
	{
		for(i = 0; i < sizeof(gsGlobalInfo.sSysEq.abParam); i++)
		{
			gsGlobalInfo.sSysEq.abParam[i]	= psSysEq->abParam[i];
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetSysEq", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetSysEq
 *
 *	Description:
 *			Get System EQ.
 *	Arguments:
 *			psSysEq		pointer to MCDRV_SYSEQ_INFO struct
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetSysEq
(
	MCDRV_SYSEQ_INFO*	psSysEq
)
{
	UINT8	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetSysEq");
#endif

	psSysEq->bOnOff	= gsGlobalInfo.sSysEq.bOnOff;
	for(i = 0; i < sizeof(gsGlobalInfo.sSysEq.abParam); i++)
	{
		psSysEq->abParam[i]	= gsGlobalInfo.sSysEq.abParam[i];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetSysEq", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetClockSwitch
 *
 *	Description:
 *			Get switch clock info.
 *	Arguments:
 *			psClockInfo		pointer to MCDRV_CLKSW_INFO struct
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetClockSwitch
(
	const MCDRV_CLKSW_INFO*	psClockInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetClockSwitch");
#endif

	gsGlobalInfo.sClockSwitch.bClkSrc	= psClockInfo->bClkSrc;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetClockSwitch", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetClockSwitch
 *
 *	Description:
 *			Get switch clock info.
 *	Arguments:
 *			psClockInfo		pointer to MCDRV_CLKSW_INFO struct
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetClockSwitch
(
	MCDRV_CLKSW_INFO*	psClockInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetClockSwitch");
#endif

	psClockInfo->bClkSrc	= gsGlobalInfo.sClockSwitch.bClkSrc;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetClockSwitch", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_SetDitSwap
 *
 *	Description:
 *			Get DIT Swap info.
 *	Arguments:
 *			psDitSwapInfo	pointer to MCDRV_DITSWAP_INFO struct
 *			dUpdateInfo	update information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_SetDitSwap
(
	const MCDRV_DITSWAP_INFO*	psDitSwapInfo,
	UINT32						dUpdateInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_SetDitSwap");
#endif


	if((dUpdateInfo & MCDRV_DITSWAP_L_UPDATE_FLAG) != 0UL)
	{
		if((psDitSwapInfo->bSwapL == MCDRV_DITSWAP_OFF) || (psDitSwapInfo->bSwapL == MCDRV_DITSWAP_ON))
		{
			gsGlobalInfo.sDitSwapInfo.bSwapL	= psDitSwapInfo->bSwapL;
		}
	}
	if((dUpdateInfo & MCDRV_DITSWAP_R_UPDATE_FLAG) != 0UL)
	{
		if((psDitSwapInfo->bSwapR == MCDRV_DITSWAP_OFF) || (psDitSwapInfo->bSwapR == MCDRV_DITSWAP_ON))
		{
			gsGlobalInfo.sDitSwapInfo.bSwapR	= psDitSwapInfo->bSwapR;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_SetDitSwap", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetDitSwap
 *
 *	Description:
 *			Get DIT Swap info.
 *	Arguments:
 *			psDitSwapInfo	pointer to MCDRV_DITSWAP_INFO struct
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetDitSwap
(
	MCDRV_DITSWAP_INFO*	psDitSwapInfo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetDitSwap");
#endif

	psDitSwapInfo->bSwapL	= gsGlobalInfo.sDitSwapInfo.bSwapL;
	psDitSwapInfo->bSwapR	= gsGlobalInfo.sDitSwapInfo.bSwapR;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetDitSwap", 0);
#endif
}


/****************************************************************************
 *	McResCtrl_GetVolReg
 *
 *	Description:
 *			Get value of volume registers.
 *	Arguments:
 *			psVolInfo	volume information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetVolReg
(
	MCDRV_VOL_INFO*	psVolInfo
)
{
	UINT8	bCh;
	MCDRV_DST_CH	abDSTCh[]	= {eMCDRV_DST_CH0, eMCDRV_DST_CH1};
	SINT16	swGainUp;
	MCDRV_SRC_TYPE	eSrcType;
	MCDRV_SRC_TYPE	eAESource	= McResCtrl_GetAESource();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetVolReg");
#endif


	*psVolInfo	= gsGlobalInfo.sVolInfo;

	if(gsGlobalInfo.sInitInfo.bDclGain == MCDRV_DCLGAIN_6)
	{
		swGainUp	= 6 * 256;
	}
	else if(gsGlobalInfo.sInitInfo.bDclGain == MCDRV_DCLGAIN_12)
	{
		swGainUp	= 12 * 256;
	}
	else if(gsGlobalInfo.sInitInfo.bDclGain == MCDRV_DCLGAIN_18)
	{
		swGainUp	= 18 * 256;
	}
	else
	{
		swGainUp	= 0;
	}

	psVolInfo->aswA_HpGain[0]	= MCDRV_REG_MUTE;
	if(McResCtrl_IsDstUsed(eMCDRV_DST_HP, eMCDRV_DST_CH0) == 0)
	{
		psVolInfo->aswA_Hp[0]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Hp[0]		= GetHpVolReg(gsGlobalInfo.sVolInfo.aswA_Hp[0]);
		psVolInfo->aswA_HpGain[0]	= GetHpGainReg(gsGlobalInfo.sVolInfo.aswA_HpGain[0]);
	}
	if(McResCtrl_IsDstUsed(eMCDRV_DST_HP, eMCDRV_DST_CH1) == 0)
	{
		psVolInfo->aswA_Hp[1]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Hp[1]	= GetHpVolReg(gsGlobalInfo.sVolInfo.aswA_Hp[1]);
		psVolInfo->aswA_HpGain[0]	= GetHpGainReg(gsGlobalInfo.sVolInfo.aswA_HpGain[0]);
	}

	if(gsGlobalInfo.sSpInfo.bSwap == MCDRV_SPSWAP_OFF)
	{/*	SP_SWAP=OFF	*/
		if(McResCtrl_IsDstUsed(eMCDRV_DST_SP, eMCDRV_DST_CH0) == 0)
		{
			psVolInfo->aswA_Sp[0]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswA_Sp[0]	= GetSpVolReg(gsGlobalInfo.sVolInfo.aswA_Sp[0]);
		}
		if(McResCtrl_IsDstUsed(eMCDRV_DST_SP, eMCDRV_DST_CH1) == 0)
		{
			psVolInfo->aswA_Sp[1]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswA_Sp[1]	= GetSpVolReg(gsGlobalInfo.sVolInfo.aswA_Sp[1]);
		}
	}
	else
	{
		if(McResCtrl_IsDstUsed(eMCDRV_DST_SP, eMCDRV_DST_CH0) == 0)
		{
			psVolInfo->aswA_Sp[1]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswA_Sp[1]	= GetSpVolReg(gsGlobalInfo.sVolInfo.aswA_Sp[1]);
		}
		if(McResCtrl_IsDstUsed(eMCDRV_DST_SP, eMCDRV_DST_CH1) == 0)
		{
			psVolInfo->aswA_Sp[0]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswA_Sp[0]	= GetSpVolReg(gsGlobalInfo.sVolInfo.aswA_Sp[0]);
		}
	}

	if(McResCtrl_IsDstUsed(eMCDRV_DST_RCV, eMCDRV_DST_CH0) == 0)
	{
		psVolInfo->aswA_Rc[0]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Rc[0]	= GetRcVolReg(gsGlobalInfo.sVolInfo.aswA_Rc[0]);
	}

	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT1, eMCDRV_DST_CH0) == 0)
	{
		psVolInfo->aswA_Lout1[0]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Lout1[0]	= GetLoVolReg(gsGlobalInfo.sVolInfo.aswA_Lout1[0]);
	}
	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT1, eMCDRV_DST_CH1) == 0)
	{
		psVolInfo->aswA_Lout1[1]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Lout1[1]	= GetLoVolReg(gsGlobalInfo.sVolInfo.aswA_Lout1[1]);
	}

	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT2, eMCDRV_DST_CH0) == 0)
	{
		psVolInfo->aswA_Lout2[0]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Lout2[0]	= GetLoVolReg(gsGlobalInfo.sVolInfo.aswA_Lout2[0]);
	}
	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT2, eMCDRV_DST_CH1) == 0)
	{
		psVolInfo->aswA_Lout2[1]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswA_Lout2[1]	= GetLoVolReg(gsGlobalInfo.sVolInfo.aswA_Lout2[1]);
	}

	for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
	{
		if(McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, abDSTCh[bCh]) == 0)
		{/*	ADC0 source all off	*/
			psVolInfo->aswA_Ad0[bCh]	= MCDRV_REG_MUTE;
			psVolInfo->aswD_Ad0[bCh]	= MCDRV_REG_MUTE;
			if(bCh == 1)
			{
				if(gsGlobalInfo.sAdcInfo.bMono == MCDRV_ADC_MONO)
				{
					if(McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH0) != 0)
					{
						psVolInfo->aswD_Ad0[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Ad0[bCh] - swGainUp);
					}
				}
			}
		}
		else
		{
			psVolInfo->aswA_Ad0[bCh]	= GetADVolReg(gsGlobalInfo.sVolInfo.aswA_Ad0[bCh]);
			psVolInfo->aswD_Ad0[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Ad0[bCh] - swGainUp);
		}
	}
	if(((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
	|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
	|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	|| (((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		&& ((eAESource == eMCDRV_SRC_PDM) || (eAESource == eMCDRV_SRC_ADC0) || (eAESource == eMCDRV_SRC_DTMF))))
	{
		for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Ad0Att[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Ad0Att[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < AD0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Ad0Att[bCh]	= MCDRV_REG_MUTE;
		}
	}

	if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
	{
		if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC1, eMCDRV_DST_CH0) == 0) || (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC1) == 0))
		{/*	ADC1 source all off	*/
			for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Adc1[bCh]	= MCDRV_REG_MUTE;
			}
			for(bCh = 0; bCh < AD1_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswA_Ad1[bCh]	= MCDRV_REG_MUTE;
			}
		}
		else
		{
			for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Adc1[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Adc1[bCh] - swGainUp);
			}
			for(bCh = 0; bCh < AD1_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswA_Ad1[bCh]	= GetADVolReg(gsGlobalInfo.sVolInfo.aswA_Ad1[bCh]);
			}
		}
		if((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		{
			for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Adc1Att[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Adc1Att[bCh]);
			}
		}
		else if(((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
			 && (eAESource == eMCDRV_SRC_ADC1))
		{
			for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Adc1Att[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Adc1Att[bCh]);
			}
		}
		else
		{
			for(bCh = 0; bCh < AD1_DVOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Adc1Att[bCh]	= MCDRV_REG_MUTE;
			}
		}
	}

	if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		psVolInfo->aswA_Lin1[0]	= GetLIVolReg(gsGlobalInfo.sVolInfo.aswA_Lin1[0]);
	}
	else
	{
		psVolInfo->aswA_Lin1[0]	= MCDRV_REG_MUTE;
	}

	if(((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
	{
		if(gsGlobalInfo.sInitInfo.bLineIn1Dif == MCDRV_LINE_DIF)
		{
			psVolInfo->aswA_Lin1[0]	= GetLIVolReg(gsGlobalInfo.sVolInfo.aswA_Lin1[0]);
			psVolInfo->aswA_Lin1[1]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswA_Lin1[1]	= GetLIVolReg(gsGlobalInfo.sVolInfo.aswA_Lin1[1]);
		}
	}
	else
	{
		psVolInfo->aswA_Lin1[1]	= MCDRV_REG_MUTE;
	}

	if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
	|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		psVolInfo->aswA_Lin2[0]	= GetLIVolReg(gsGlobalInfo.sVolInfo.aswA_Lin2[0]);
	}
	else
	{
		psVolInfo->aswA_Lin2[0]	= MCDRV_REG_MUTE;
	}

	if(((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
	|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
	|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
	{
		if(gsGlobalInfo.sInitInfo.bLineIn2Dif == MCDRV_LINE_DIF)
		{
			psVolInfo->aswA_Lin2[0]	= GetLIVolReg(gsGlobalInfo.sVolInfo.aswA_Lin2[0]);
			psVolInfo->aswA_Lin2[1]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswA_Lin2[1]	= GetLIVolReg(gsGlobalInfo.sVolInfo.aswA_Lin2[1]);
		}
	}
	else
	{
		psVolInfo->aswA_Lin2[1]	= MCDRV_REG_MUTE;
	}

	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIC1) == 0)
	{/*	MIC1 is unused	*/
		for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswA_Mic1[bCh]		= MCDRV_REG_MUTE;
			psVolInfo->aswA_Mic1Gain[bCh]	= (SINT16)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_MC_GAIN) & MCB_MC1GAIN);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIC1_VOL_CHANNELS; bCh++)
		{
			if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
			&& ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
			&& ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON))
			{
				psVolInfo->aswA_Mic1[bCh]	= MCDRV_REG_MUTE;
			}
			else
			{
				psVolInfo->aswA_Mic1[bCh]	= GetMcVolReg(gsGlobalInfo.sVolInfo.aswA_Mic1[bCh]);
			}
			psVolInfo->aswA_Mic1Gain[bCh]	= GetMcGainReg(gsGlobalInfo.sVolInfo.aswA_Mic1Gain[bCh]);
		}
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIC2) == 0)
	{/*	MIC2 is unused	*/
		for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswA_Mic2[bCh]		= MCDRV_REG_MUTE;
			psVolInfo->aswA_Mic2Gain[bCh]	= (SINT16)((McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_MC_GAIN) & MCB_MC2GAIN) >> 4);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIC2_VOL_CHANNELS; bCh++)
		{
			if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
			&& ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
			&& ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON))
			{
				psVolInfo->aswA_Mic2[bCh]	= MCDRV_REG_MUTE;
			}
			else
			{
				psVolInfo->aswA_Mic2[bCh]	= GetMcVolReg(gsGlobalInfo.sVolInfo.aswA_Mic2[bCh]);
			}
			psVolInfo->aswA_Mic2Gain[bCh]	= GetMcGainReg(gsGlobalInfo.sVolInfo.aswA_Mic2Gain[bCh]);
		}
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIC3) == 0)
	{/*	MIC3 is unused	*/
		for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswA_Mic3[bCh]		= MCDRV_REG_MUTE;
			psVolInfo->aswA_Mic3Gain[bCh]	= (SINT16)(McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_ANA, MCI_MC3_GAIN) & MCB_MC3GAIN);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIC3_VOL_CHANNELS; bCh++)
		{
			if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
			&& ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON))
			{
				psVolInfo->aswA_Mic3[bCh]	= MCDRV_REG_MUTE;
			}
			else
			{
				psVolInfo->aswA_Mic3[bCh]	= GetMcVolReg(gsGlobalInfo.sVolInfo.aswA_Mic3[bCh]);
			}
			psVolInfo->aswA_Mic3Gain[bCh]	= GetMcGainReg(gsGlobalInfo.sVolInfo.aswA_Mic3Gain[bCh]);
		}
	}

	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR0) == 0)
	{/*	DIR0 is unused	*/
		for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir0[bCh]		= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir0[bCh]		= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir0[bCh] - swGainUp);
		}
	}
	if(((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
	|| (((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		&& (eAESource == eMCDRV_SRC_DIR0)))
	{
		for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir0Att[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir0Att[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir0Att[bCh]	= MCDRV_REG_MUTE;
		}
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR1) == 0)
	{/*	DIR1 is unused	*/
		for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir1[bCh]		= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir1[bCh]		= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir1[bCh] - swGainUp);
		}
	}
	if(((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
	|| (((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		&& (eAESource == eMCDRV_SRC_DIR1)))
	{
		for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir1Att[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir1Att[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir1Att[bCh]	= MCDRV_REG_MUTE;
		}
	}
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2) == 0)
	{/*	DIR2 is unused	*/
		if((McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0) && (McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0))
		{
			for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Dir2[bCh]		= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir2[bCh] - swGainUp);
			}
		}
		else
		{
			for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_Dir2[bCh]		= MCDRV_REG_MUTE;
			}
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir2[bCh]		= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir2[bCh] - swGainUp);
		}
	}
	if(((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
	|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
	|| (((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		&& ((eAESource == eMCDRV_SRC_DIR2) || (eAESource == eMCDRV_SRC_CDSP))))
	{
		for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir2Att[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir2Att[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir2Att[bCh]	= MCDRV_REG_MUTE;
		}
	}

	if(McResCtrl_GetDITSource(eMCDRV_DIO_0) == eMCDRV_SRC_NONE)
	{/*	DIT0 source all off	*/
		for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dit0[bCh]	= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO0_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dit0[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit0[bCh] + swGainUp);
		}
	}
	if(McResCtrl_GetDITSource(eMCDRV_DIO_1) == eMCDRV_SRC_NONE)
	{/*	DIT1 source all off	*/
		for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dit1[bCh]	= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO1_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dit1[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit1[bCh] + swGainUp);
		}
	}
	/*	DIT2_INVOL	*/
	eSrcType	= McResCtrl_GetDITSource(eMCDRV_DIO_2);
	if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_CDSP_DIRECT))
	{
		eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
		if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
		{
			psVolInfo->aswD_Dit2[0]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswD_Dit2[0]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit2[0] + swGainUp);
		}
		eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
		if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
		{
			psVolInfo->aswD_Dit2[1]	= MCDRV_REG_MUTE;
		}
		else
		{
			psVolInfo->aswD_Dit2[1]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit2[1] + swGainUp);
		}
	}
	else
	{
		for(bCh = 0; bCh < DIO2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dit2[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit2[bCh] + swGainUp);
		}
	}

	/*	DIT2_INVOL1	*/
	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
	if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
	{
		psVolInfo->aswD_Dit21[0]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswD_Dit21[0]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit21[0] + swGainUp);
	}
	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
	if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_DIR2_DIRECT))
	{
		psVolInfo->aswD_Dit21[1]	= MCDRV_REG_MUTE;
	}
	else
	{
		psVolInfo->aswD_Dit21[1]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dit21[1] + swGainUp);
	}

	if((McResCtrl_GetDACSource(eMCDRV_DAC_MASTER) == eMCDRV_SRC_NONE)
	&& (McResCtrl_GetDACSource(eMCDRV_DAC_VOICE) == eMCDRV_SRC_NONE))
	{
		for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_DacAtt[bCh]		= MCDRV_REG_MUTE;
			psVolInfo->aswD_DacMaster[bCh]	= MCDRV_REG_MUTE;
			psVolInfo->aswD_DacVoice[bCh]	= MCDRV_REG_MUTE;
		}
	}
	else
	{
		if(McResCtrl_GetDACSource(eMCDRV_DAC_MASTER) == eMCDRV_SRC_NONE)
		{
			for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_DacMaster[bCh]	= MCDRV_REG_MUTE;
			}
		}
		else
		{
			for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_DacMaster[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_DacMaster[bCh]);
			}
		}
		if(McResCtrl_GetDACSource(eMCDRV_DAC_VOICE) == eMCDRV_SRC_NONE)
		{
			for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_DacVoice[bCh]	= MCDRV_REG_MUTE;
			}
		}
		else
		{
			for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
			{
				psVolInfo->aswD_DacVoice[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_DacVoice[bCh]);
			}
		}
		for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_DacAtt[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_DacAtt[bCh]);
		}
	}

	if((McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) == 0)
	&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) == 0)
	&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_DTMF) == 0))
	{/*	PDM&ADC0 is unused	*/
		for(bCh = 0; bCh < AENG6_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Aeng6[bCh]	= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < AENG6_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Aeng6[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Aeng6[bCh]);
		}
	}

	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) == 0)
	{/*	PDM is unused	*/
		for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Pdm[bCh]		= MCDRV_REG_MUTE;
			psVolInfo->aswD_SideTone[bCh]	= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < PDM_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Pdm[bCh]		= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Pdm[bCh] - swGainUp);
			psVolInfo->aswD_SideTone[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_SideTone[bCh] - swGainUp);
		}
	}

	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DTMF) == 0)
	{/*	DTMF is unused	*/
		for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dtmfb[bCh]		= MCDRV_REG_MUTE;
		}
	}
	else
	{
		for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dtmfb[bCh]		= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dtmfb[bCh] - swGainUp);
		}
	}

	if((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	{
		for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_DtmfAtt[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_DtmfAtt[bCh] - swGainUp);
		}
		for(bCh = 0; bCh < DAC_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_DacAtt[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_DacAtt[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < DTMF_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_DtmfAtt[bCh]	= MCDRV_REG_MUTE;
		}
	}

	for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
	{
		psVolInfo->aswD_PdmAeMix[bCh]	= MCDRV_REG_MUTE;
		psVolInfo->aswD_MixAeMix[bCh]	= MCDRV_REG_MUTE;
	}
	if(((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
	|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
	|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON))
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Adc0AeMix[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Adc0AeMix[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Adc0AeMix[bCh]	= MCDRV_REG_MUTE;
		}
	}
	if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Adc1AeMix[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Adc1AeMix[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Adc1AeMix[bCh]	= MCDRV_REG_MUTE;
		}
	}
	if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir0AeMix[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir0AeMix[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir0AeMix[bCh]	= MCDRV_REG_MUTE;
		}
	}
	if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir1AeMix[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir1AeMix[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir1AeMix[bCh]	= MCDRV_REG_MUTE;
		}
	}
	if(((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
	|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC6_CDSP_ON))
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir2AeMix[bCh]	= McResCtrl_GetDigitalVolReg(gsGlobalInfo.sVolInfo.aswD_Dir2AeMix[bCh]);
		}
	}
	else
	{
		for(bCh = 0; bCh < MIX2_VOL_CHANNELS; bCh++)
		{
			psVolInfo->aswD_Dir2AeMix[bCh]	= MCDRV_REG_MUTE;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetVolReg", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetDigitalVolReg
 *
 *	Description:
 *			Get value of digital volume registers.
 *	Arguments:
 *			sdVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
SINT16	McResCtrl_GetDigitalVolReg
(
	SINT32	sdVol
)
{
	SINT32	sdRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetDigitalVolReg");
#endif

	if(sdVol < (-74*256))
	{
		sdRet	= 0;
	}
	else if(sdVol < 0L)
	{
		sdRet	= 96L + (sdVol-128L)/256L;
	}
	else
	{
		sdRet	= 96L + (sdVol+128L)/256L;
	}

	if(sdRet < 22L)
	{
		sdRet	= 0;
	}

	if(sdRet > 114L)
	{
		sdRet	= 114;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetDigitalVolReg", &sdRet);
#endif

	return (SINT16)sdRet;
}

/****************************************************************************
 *	GetADVolReg
 *
 *	Description:
 *			Get update value of analog AD volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetADVolReg
(
	SINT16	swVol
)
{
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetADVolReg");
#endif

	if(swVol < (-27*256))
	{
		swRet	= 0;
	}
	else if(swVol < 0)
	{
		swRet	= 19 + (swVol-192) * 2 / (256*3);
	}
	else
	{
		swRet	= 19 + (swVol+192) * 2 / (256*3);
	}

	if(swRet < 0)
	{
		swRet	= 0;
	}

	if(swRet > 31)
	{
		swRet	= 31;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetADVolReg", &sdRet);
#endif

	return swRet;
}

/****************************************************************************
 *	GetLIVolReg
 *
 *	Description:
 *			Get update value of analog LIN volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetLIVolReg
(
	SINT16	swVol
)
{
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetLIVolReg");
#endif

	if(swVol < (-30*256))
	{
		swRet	= 0;
	}
	else if(swVol < 0)
	{
		swRet	= 21 + (swVol-192) * 2 / (256*3);
	}
	else
	{
		swRet	= 21 + (swVol+192) * 2 / (256*3);
	}

	if(swRet < 0)
	{
		swRet	= 0;
	}
	if(swRet > 31)
	{
		swRet	= 31;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetLIVolReg", &sdRet);
#endif
	return swRet;
}

/****************************************************************************
 *	GetMcVolReg
 *
 *	Description:
 *			Get update value of analog Mic volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetMcVolReg
(
	SINT16	swVol
)
{
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetMcVolReg");
#endif

	if(swVol < (-30*256))
	{
		swRet	= 0;
	}
	else if(swVol < 0)
	{
		swRet	= 21 + (swVol-192) * 2 / (256*3);
	}
	else
	{
		swRet	= 21 + (swVol+192) * 2 / (256*3);
	}

	if(swRet < 0)
	{
		swRet	= 0;
	}
	if(swRet > 31)
	{
		swRet	= 31;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetMcVolReg", &sdRet);
#endif
	return swRet;
}

/****************************************************************************
 *	GetMcGainReg
 *
 *	Description:
 *			Get update value of analog Mic gain registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetMcGainReg
(
	SINT16	swVol
)
{
	SINT16	swGain	= (swVol+128)/256;
	SINT16	swRet	= 3;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetMcGainReg");
#endif

	if(swGain < 18)
	{
		swRet	= 0;
	}
	else if(swGain < 23)
	{
		swRet	= 1;
	}
	else if(swGain < 28)
	{
		swRet	= 2;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetMcGainReg", &sdRet);
#endif

	return swRet;
}

/****************************************************************************
 *	GetHpVolReg
 *
 *	Description:
 *			Get update value of analog Hp volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetHpVolReg
(
	SINT16	swVol
)
{
	SINT16	swDB	= swVol/256;
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetHpVolReg");
#endif

	if(swVol >= 0)
	{
		swRet	= 31;
	}
	else if(swDB <= -8)
	{
		if(swVol < (-36*256))
		{
			swRet	= 0;
		}
		else if(swDB <= -32)
		{
			swRet	= 1;
		}
		else if(swDB <= -26)
		{
			swRet	= 2;
		}
		else if(swDB <= -23)
		{
			swRet	= 3;
		}
		else if(swDB <= -21)
		{
			swRet	= 4;
		}
		else if(swDB <= -19)
		{
			swRet	= 5;
		}
		else if(swDB <= -17)
		{
			swRet	= 6;
		}
		else
		{
			swRet	= 23+(swVol-128)/256;
		}
	}
	else
	{
		swRet	= 31 + (swVol-64)*2/256;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetHpVolReg", &sdRet);
#endif

	return swRet;
}

/****************************************************************************
 *	GetHpGainReg
 *
 *	Description:
 *			Get update value of analog Hp gain registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetHpGainReg
(
	SINT16	swVol
)
{
	SINT16	swDB	= swVol/(256/4);
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetHpGainReg");
#endif

	if(swDB < 3)
	{
		swRet	= 0;
	}
	else if(swDB < 9)
	{
		swRet	= 1;
	}
	else if(swDB < 18)
	{
		swRet	= 2;
	}
	else
	{
		swRet	= 3;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetHpGainReg", &sdRet);
#endif

	return swRet;
}

/****************************************************************************
 *	GetSpVolReg
 *
 *	Description:
 *			Get update value of analog Sp volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetSpVolReg
(
	SINT16	swVol
)
{
	SINT16	swDB	= swVol/256;
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetSpVolReg");
#endif

	if(swVol >= 0)
	{
		swRet	= 31;
	}
	else if(swDB <= -8)
	{
		if(swVol < (-36*256))
		{
			swRet	= 0;
		}
		else if(swDB <= -32)
		{
			swRet	= 1;
		}
		else if(swDB <= -26)
		{
			swRet	= 2;
		}
		else if(swDB <= -23)
		{
			swRet	= 3;
		}
		else if(swDB <= -21)
		{
			swRet	= 4;
		}
		else if(swDB <= -19)
		{
			swRet	= 5;
		}
		else if(swDB <= -17)
		{
			swRet	= 6;
		}
		else
		{
			swRet	= 23+(swVol-128)/256;
		}
	}
	else
	{
		swRet	= 31 + (swVol-64)*2/256;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetSpVolReg", &sdRet);
#endif

	return swRet;
}

/****************************************************************************
 *	GetRcVolReg
 *
 *	Description:
 *			Get update value of analog Rcv volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetRcVolReg
(
	SINT16	swVol
)
{
	SINT16	swDB	= swVol/256;
	SINT16	swRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetRcVolReg");
#endif

	if(swVol >= 0)
	{
		swRet	= 31;
	}
	else if(swDB <= -8)
	{
		if(swVol < (-36*256))
		{
			swRet	= 0;
		}
		else if(swDB <= -32)
		{
			swRet	= 1;
		}
		else if(swDB <= -26)
		{
			swRet	= 2;
		}
		else if(swDB <= -23)
		{
			swRet	= 3;
		}
		else if(swDB <= -21)
		{
			swRet	= 4;
		}
		else if(swDB <= -19)
		{
			swRet	= 5;
		}
		else if(swDB <= -17)
		{
			swRet	= 6;
		}
		else
		{
			swRet	= 23+(swVol-128)/256;
		}
	}
	else
	{
		swRet	= 31 + (swVol-64)*2/256;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetRcVolReg", &sdRet);
#endif

	return swRet;
}

/****************************************************************************
 *	GetLoVolReg
 *
 *	Description:
 *			Get update value of analog Lout volume registers.
 *	Arguments:
 *			swVol	volume(dB value*256)
 *	Return:
 *			value of registers
 *
 ****************************************************************************/
static SINT16	GetLoVolReg
(
	SINT16	swVol
)
{
	SINT16	swRet	= 31 + (swVol-128)/256;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("GetLoVolReg");
#endif

	if(swVol < (-30*256))
	{
		swRet	= 0;
	}
	if(swRet < 0)
	{
		swRet	= 0;
	}
	if(swRet > 31)
	{
		swRet	= 31;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= swRet;
	McDebugLog_FuncOut("GetLoVolReg", &sdRet);
#endif
	return swRet;
}

/****************************************************************************
 *	McResCtrl_GetPowerInfo
 *
 *	Description:
 *			Get power information.
 *	Arguments:
 *			psPowerInfo	power information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetPowerInfo
(
	MCDRV_POWER_INFO*	psPowerInfo
)
{
	UINT8	i;
	UINT8	bAnalogOn	= 0;
	UINT8	bPowerMode	= gsGlobalInfo.sInitInfo.bPowerMode;
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetPowerInfo");
#endif

	/*	Digital power	*/
	psPowerInfo->dDigital	= 0;
	if((bPowerMode & MCDRV_POWMODE_CLKON) == 0)
	{
		psPowerInfo->dDigital |= (MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1 | MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0);
	}
	if((McResCtrl_GetDITSource(eMCDRV_DIO_0)		!= eMCDRV_SRC_NONE)
	|| (McResCtrl_GetDITSource(eMCDRV_DIO_1)		!= eMCDRV_SRC_NONE)
	|| (McResCtrl_GetDITSource(eMCDRV_DIO_2)		!= eMCDRV_SRC_NONE)
	|| (McResCtrl_GetDACSource(eMCDRV_DAC_MASTER)	!= eMCDRV_SRC_NONE)
	|| (McResCtrl_GetDACSource(eMCDRV_DAC_VOICE)	!= eMCDRV_SRC_NONE)
	|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	|| (McResCtrl_GetPMSource()						!= eMCDRV_SRC_NONE)
	|| (McResCtrl_GetAESource()						!= eMCDRV_SRC_NONE)
	|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0)		!= 0)
	|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC1)		!= 0)
	|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_MIX)			!= 0)
	|| (gsGlobalInfo.pbAeEx							!= NULL)
	|| (gsGlobalInfo.pbCdspFw						!= NULL))
	{
		/*	DP0-2, PLLRST0 on	*/
		psPowerInfo->dDigital &= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1 | MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0);
	}
	else
	{
		if(McResCtrl_IsEnableIRQ() != 0)
		{
			/*	DP0-2, PLLRST0 on	*/
			psPowerInfo->dDigital &= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1 | MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0);
		}
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPB;
	}

	if((McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP) != 0)
	|| (McResCtrl_IsSrcUsed(eMCDRV_SRC_CDSP_DIRECT) != 0)
	|| (McResCtrl_IsValidCDSPSource() != 0)
	|| (gsGlobalInfo.pbAeEx != NULL)
	|| (gsGlobalInfo.pbCdspFw != NULL))
	{
	}
	else
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPC;
	}

	/*	DPBDSP	*/
	if((McResCtrl_IsSrcUsed(eMCDRV_SRC_AE) == 0)
	|| (McResCtrl_IsDstUsed(eMCDRV_DST_AE, eMCDRV_DST_CH0) == 0)
	|| ((gsGlobalInfo.sAeInfo.bOnOff&(MCDRV_BEXWIDE_ON|MCDRV_DRC_ON)) == 0))
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPBDSP;
	}

	/*	DPADIF	*/
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC0) == 0)
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPADIF;
	}
	/*	DPCD	*/
	if((McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_L) == 0)
	&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_R) == 0)
	&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_M) == 0)
	&& (McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH0) == 0)
	&& (McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH1) == 0))
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPCD;
	}

	/*	DPPDM	*/
	if((gsGlobalInfo.sInitInfo.bPad0Func != MCDRV_PAD_PDMCK) || (McResCtrl_IsSrcUsed(eMCDRV_SRC_PDM) == 0))
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPPDM;
	}

	/*	DPDI*	*/
	if((gsGlobalInfo.sDioInfo.asPortInfo[0].sDioCommon.bMasterSlave == MCDRV_DIO_SLAVE)
	|| (((McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR0) == 0) && (McResCtrl_GetDITSource(eMCDRV_DIO_0) == eMCDRV_SRC_NONE))))
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPDI0;
	}
	if((gsGlobalInfo.sDioInfo.asPortInfo[1].sDioCommon.bMasterSlave == MCDRV_DIO_SLAVE)
	|| (((McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR1) == 0) && (McResCtrl_GetDITSource(eMCDRV_DIO_1) == eMCDRV_SRC_NONE))))
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPDI1;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_DIO2) == 0)
	{
		psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPDI2;
	}
	else if(gsGlobalInfo.sDioInfo.asPortInfo[2].sDioCommon.bMasterSlave == MCDRV_DIO_SLAVE)
	{
		if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
		{
			if((psPowerInfo->dDigital & MCDRV_POWINFO_DIGITAL_DPC) == MCDRV_POWINFO_DIGITAL_DPC)
			{
				psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPDI2;
			}
		}
		else
		{
			psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPDI2;
		}
	}
	else if((McResCtrl_IsSrcUsed(eMCDRV_SRC_DIR2) == 0) && (McResCtrl_GetDITSource(eMCDRV_DIO_2) == eMCDRV_SRC_NONE))
	{
		if((psPowerInfo->dDigital & MCDRV_POWINFO_DIGITAL_DPC) == MCDRV_POWINFO_DIGITAL_DPC)
		{
			psPowerInfo->dDigital |= MCDRV_POWINFO_DIGITAL_DPDI2;
		}
	}
	else
	{
	}

	/*	Analog power	*/
	for(i = 0; i < 6; i++)
	{
		psPowerInfo->abAnalog[i]	= 0;
	}

	/*	SPL*	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_SP, eMCDRV_DST_CH0) == 0)
	{
		if(gsGlobalInfo.sSpInfo.bSwap == MCDRV_SPSWAP_OFF)
		{/*	SP_SWAP=OFF	*/
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPL1;
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPL2;
		}
		else
		{
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPR1;
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPR2;
		}
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	SPR*	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_SP, eMCDRV_DST_CH1) == 0)
	{
		if(gsGlobalInfo.sSpInfo.bSwap == MCDRV_SPSWAP_OFF)
		{/*	SP_SWAP=OFF	*/
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPR1;
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPR2;
		}
		else
		{
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPL1;
			psPowerInfo->abAnalog[1] |= MCB_PWM_SPL2;
		}
	}
	else
	{
		bAnalogOn	= 1;
	}

	/*	HPL	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_HP, eMCDRV_DST_CH0) == 0)
	{
		psPowerInfo->abAnalog[1] |= MCB_PWM_HPL;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	HPR	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_HP, eMCDRV_DST_CH1) == 0)
	{
		psPowerInfo->abAnalog[1] |= MCB_PWM_HPR;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	CP	*/
	if(((psPowerInfo->abAnalog[1] & MCB_PWM_HPL) != 0) && ((psPowerInfo->abAnalog[1] & MCB_PWM_HPR) != 0))
	{
		psPowerInfo->abAnalog[0] |= MCB_PWM_CP;
	}

	/*	LOUT1L	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT1, eMCDRV_DST_CH0) == 0)
	{
		psPowerInfo->abAnalog[2] |= MCB_PWM_LO1L;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	LOUT1R	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT1, eMCDRV_DST_CH1) == 0)
	{
		psPowerInfo->abAnalog[2] |= MCB_PWM_LO1R;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	LOUT2L	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT2, eMCDRV_DST_CH0) == 0)
	{
		psPowerInfo->abAnalog[2] |= MCB_PWM_LO2L;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	LOUT2R	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_LOUT2, eMCDRV_DST_CH1) == 0)
	{
		psPowerInfo->abAnalog[2] |= MCB_PWM_LO2R;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	RCV	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_RCV, eMCDRV_DST_CH0) == 0)
	{
		psPowerInfo->abAnalog[2] |= MCB_PWM_RC1;
		psPowerInfo->abAnalog[2] |= MCB_PWM_RC2;
	}
	else
	{
		bAnalogOn	= 1;
	}

	/*	DA	*/
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_DAC_M) == 0)
	{
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON))
		{
		}
		else
		{
			if(((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) != MCDRV_SRC5_DAC_L_ON)
			&& ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) != MCDRV_SRC5_DAC_L_ON))
			{
				psPowerInfo->abAnalog[3] |= MCB_PWM_DAL;
			}
		}
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
		{
		}
		else
		{
			if(((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) != MCDRV_SRC5_DAC_R_ON)
			&& ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) != MCDRV_SRC5_DAC_R_ON))
			{
				psPowerInfo->abAnalog[3] |= MCB_PWM_DAR;
			}
		}
	}
	if(((psPowerInfo->abAnalog[3] & MCB_PWM_DAL) == 0) || ((psPowerInfo->abAnalog[3] & MCB_PWM_DAR) == 0))
	{
		bAnalogOn	= 1;
	}

	/*	ADC0L	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH0) == 1)
	{
		bAnalogOn	= 1;
	}
	else
	{
		psPowerInfo->abAnalog[1] |= MCB_PWM_ADL;
	}
	/*	ADC0R	*/
	if(McResCtrl_IsDstUsed(eMCDRV_DST_ADC0, eMCDRV_DST_CH1) == 1)
	{
		bAnalogOn	= 1;
	}
	else
	{
		psPowerInfo->abAnalog[1] |= MCB_PWM_ADR;
	}
	if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
	{
		if((McResCtrl_IsDstUsed(eMCDRV_DST_ADC1, eMCDRV_DST_CH0) == 1)
		&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_ADC1) == 1)
		&& ((psPowerInfo->abAnalog[1] & (MCB_PWM_ADL|MCB_PWM_ADR)) != (MCB_PWM_ADL|MCB_PWM_ADR)))
		{
			bAnalogOn	= 1;
		}
		else
		{
			psPowerInfo->abAnalog[2] |= MCB_PWM_ADM;
		}
	}
	/*	LI	*/
	if(McDevProf_IsValid(eMCDRV_FUNC_LI1) != 0)
	{
		if((McResCtrl_IsSrcUsed(eMCDRV_SRC_LINE1_L) == 0)
		&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_LINE1_M) == 0)
		&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_LINE1_R) == 0))
		{
			psPowerInfo->abAnalog[4] |= MCB_PWM_LI;
		}
		else
		{
			bAnalogOn	= 1;
		}
	}
	else
	{
		psPowerInfo->abAnalog[4] |= MCB_PWM_LI;
	}
	if(McDevProf_IsValid(eMCDRV_FUNC_LI2) != 0)
	{
		if((McResCtrl_IsSrcUsed(eMCDRV_SRC_LINE2_L) == 0)
		&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_LINE2_M) == 0)
		&& (McResCtrl_IsSrcUsed(eMCDRV_SRC_LINE2_R) == 0))
		{
			psPowerInfo->abAnalog[5] |= MCB_PWM_LI2;
		}
		else
		{
			bAnalogOn	= 1;
		}
	}
	else
	{
		psPowerInfo->abAnalog[5] |= MCB_PWM_LI2;
	}
	/*	MC1	*/
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIC1) == 0)
	{
		psPowerInfo->abAnalog[4] |= MCB_PWM_MC1;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	MC2	*/
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIC2) == 0)
	{
		psPowerInfo->abAnalog[4] |= MCB_PWM_MC2;
	}
	else
	{
		bAnalogOn	= 1;
	}
	/*	MC3	*/
	if(McResCtrl_IsSrcUsed(eMCDRV_SRC_MIC3) == 0)
	{
		psPowerInfo->abAnalog[4] |= MCB_PWM_MC3;
	}
	else
	{
		bAnalogOn	= 1;
	}
	if((gsGlobalInfo.sPathInfo.asBias[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) != MCDRV_SRC0_MIC1_ON)
	{
		psPowerInfo->abAnalog[3] |= MCB_PWM_MB1;
	}
	else
	{
		bAnalogOn	= 1;
	}
	if((gsGlobalInfo.sPathInfo.asBias[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) != MCDRV_SRC0_MIC2_ON)
	{
		psPowerInfo->abAnalog[3] |= MCB_PWM_MB2;
	}
	else
	{
		bAnalogOn	= 1;
	}
	if((gsGlobalInfo.sPathInfo.asBias[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) != MCDRV_SRC0_MIC3_ON)
	{
		psPowerInfo->abAnalog[3] |= MCB_PWM_MB3;
	}
	else
	{
		bAnalogOn	= 1;
	}

	/*	VR/LDOA/REFA	*/
	if ((0 == bAnalogOn) && ((bPowerMode & MCDRV_POWMODE_VREFON) == 0))
	{
		psPowerInfo->abAnalog[0] |= MCB_PWM_VR;
		psPowerInfo->abAnalog[0] |= MCB_PWM_REFA;
		psPowerInfo->abAnalog[0] |= MCB_PWM_LDOA;
	}
	else
	{
		if ((MCDRV_LDO_AOFF_DON == gsGlobalInfo.sInitInfo.bLdo) || (MCDRV_LDO_AOFF_DOFF == gsGlobalInfo.sInitInfo.bLdo))
		{
			psPowerInfo->abAnalog[0] |= MCB_PWM_LDOA;
		}
		else
		{
			psPowerInfo->abAnalog[0] |= MCB_PWM_REFA;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetPowerInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetPowerInfoRegAccess
 *
 *	Description:
 *			Get power information to access register.
 *	Arguments:
 *			psRegInfo	register information
 *			psPowerInfo	power information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetPowerInfoRegAccess
(
	const MCDRV_REG_INFO*	psRegInfo,
	MCDRV_POWER_INFO*		psPowerInfo
)
{
	MCDRV_DEV_ID	eDevID	= McDevProf_GetDevId();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetPowerInfoRegAccess");
#endif


	McResCtrl_GetPowerInfo(psPowerInfo);

	switch(psRegInfo->bRegType)
	{
	default:
	case	MCDRV_REGTYPE_B_BASE:
		break;

	case	MCDRV_REGTYPE_A:
		switch(psRegInfo->bAddress)
		{
		case	MCI_BDSP_ST:
		case	MCI_BDSP_RST:
			psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
										| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPB);
			break;
		case	MCI_BDSP_ADR:
		case	MCI_BDSP_WINDOW:
			psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
										| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPBDSP);
			break;
		case	MCI_CDSP_IRQ_FLAG_REG:
		case	MCI_CDSP_IRQ_CTRL_REG:
		case	MCI_FSQ_FIFO_REG:
			psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
										| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPC);
			break;
		default:
			break;
		}
		break;

	case	MCDRV_REGTYPE_B_MIXER:
		switch(psRegInfo->bAddress)
		{
		case	MCI_SWP:
		case	MCI_DIMODE0:
		case	MCI_DIRSRC_RATE0_MSB:
		case	MCI_DIRSRC_RATE0_LSB:
		case	MCI_DI_FS0:
		case	MCI_DI0_SRC:
		case	MCI_DIX0_FMT:
		case	MCI_DIR0_CH:
		case	MCI_DIT0_SLOT:
		case	MCI_HIZ_REDGE0:
		case	MCI_PCM_RX0:
		case	MCI_PCM_SLOT_RX0:
		case	MCI_PCM_TX0:
		case	MCI_PCM_SLOT_TX0:
		case	MCI_DIMODE1:
		case	MCI_DIRSRC_RATE1_MSB:
		case	MCI_DIRSRC_RATE1_LSB:
		case	MCI_DI_FS1:
		case	MCI_DI1_SRC:
		case	MCI_DIX1_FMT:
		case	MCI_DIR1_CH:
		case	MCI_DIT1_SLOT:
		case	MCI_HIZ_REDGE1:
		case	MCI_PCM_RX1:
		case	MCI_PCM_SLOT_RX1:
		case	MCI_PCM_TX1:
		case	MCI_PCM_SLOT_TX1:
		case	MCI_DIMODE2:
		case	MCI_DI_FS2:
		case	MCI_DI2_SRC:
		case	MCI_DIRSRC_RATE2_MSB:
		case	MCI_DIRSRC_RATE2_LSB:
		case	MCI_DIX2_FMT:
		case	MCI_DIR2_CH:
		case	MCI_DIT2_SLOT:
		case	MCI_HIZ_REDGE2:
		case	MCI_PCM_RX2:
		case	MCI_PCM_SLOT_RX2:
		case	MCI_PCM_TX2:
		case	MCI_PCM_SLOT_TX2:
		case	MCI_CDI_CH:
		case	MCI_CDO_CH:
		case	MCI_PDM_AGC:
		case	MCI_PDM_STWAIT:
			break;

		case	MCI_DITSRC_RATE0_MSB:
		case	MCI_DITSRC_RATE0_LSB:
		case	MCI_DITSRC_RATE1_MSB:
		case	MCI_DITSRC_RATE1_LSB:
		case	MCI_DITSRC_RATE2_MSB:
		case	MCI_DITSRC_RATE2_LSB:
			if((eDevID != eMCDRV_DEV_ID_46H) && (eDevID != eMCDRV_DEV_ID_44H))
			{
				psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
											| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPB);
			}
			break;

		default:
			psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
										| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPB);
			break;
		}
		break;

	case	MCDRV_REGTYPE_B_AE:
		switch(psRegInfo->bAddress)
		{
		case	MCI_PM_SOURCE:
		case	MCI_PM_L:
		case	MCI_PM_R:
			psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
										| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPB);
			break;
		default:
			break;
		}
		break;

	case	MCDRV_REGTYPE_B_CDSP:
			psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
										| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPC);
		break;

	case	MCDRV_REGTYPE_B_ANALOG:
		switch(psRegInfo->bAddress)
		{
		case	MCI_HPVOL_L:
		case	MCI_HPVOL_R:
		case	MCI_SPVOL_L:
		case	MCI_SPVOL_R:
		case	MCI_RCVOL:
			psPowerInfo->abAnalog[0]	&= (UINT8)~MCB_PWM_VR;
			if ((MCDRV_LDO_AOFF_DON == gsGlobalInfo.sInitInfo.bLdo) || (MCDRV_LDO_AOFF_DOFF == gsGlobalInfo.sInitInfo.bLdo))
			{
				psPowerInfo->abAnalog[0] &= (UINT8)~MCB_PWM_REFA;
			}
			else
			{
				psPowerInfo->abAnalog[0] &= (UINT8)~MCB_PWM_LDOA;
			}
			break;

		case	MCI_HPL_MIX:
		case	MCI_HPL_MONO:
		case	MCI_HPR_MIX:
		case	MCI_SPL_MIX:
		case	MCI_SPL_MONO:
		case	MCI_SPR_MIX:
		case	MCI_SPR_MONO:
		case	MCI_RC_MIX:
			if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
			{
			}
			else
			{
				psPowerInfo->abAnalog[0]	&= (UINT8)~MCB_PWM_VR;
				if ((MCDRV_LDO_AOFF_DON == gsGlobalInfo.sInitInfo.bLdo) || (MCDRV_LDO_AOFF_DOFF == gsGlobalInfo.sInitInfo.bLdo))
				{
					psPowerInfo->abAnalog[0] &= (UINT8)~MCB_PWM_REFA;
				}
				else
				{
					psPowerInfo->abAnalog[0] &= (UINT8)~MCB_PWM_LDOA;
				}
			}
			break;
		default:
			break;
		}
		break;

	case	MCDRV_REGTYPE_B_CODEC:
		switch(psRegInfo->bAddress)
		{
		case	MCI_AD_START:
		case	MCI_DAC_CONFIG:
		case	MCI_SYSTEM_EQON:
			if((psRegInfo->bAddress >= MCI_SYS_CEQ0_19_12) && (psRegInfo->bAddress <= MCI_SYS_CEQ4_3_0))
			{
				break;
			}
			if((eDevID == eMCDRV_DEV_ID_46H) || (eDevID == eMCDRV_DEV_ID_44H))
			{
				psPowerInfo->abAnalog[0]	&= (UINT8)~MCB_PWM_VR;
				if ((MCDRV_LDO_AOFF_DON == gsGlobalInfo.sInitInfo.bLdo) || (MCDRV_LDO_AOFF_DOFF == gsGlobalInfo.sInitInfo.bLdo))
				{
					psPowerInfo->abAnalog[0] &= (UINT8)~MCB_PWM_REFA;
				}
				else
				{
					psPowerInfo->abAnalog[0] &= (UINT8)~MCB_PWM_LDOA;
				}
				psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
											| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPCD);
			}
			else
			{
				psPowerInfo->dDigital	&= ~(MCDRV_POWINFO_DIGITAL_DP0 | MCDRV_POWINFO_DIGITAL_DP1
											| MCDRV_POWINFO_DIGITAL_DP2 | MCDRV_POWINFO_DIGITAL_PLLRST0 | MCDRV_POWINFO_DIGITAL_DPB);
			}
			break;
		default:
			break;
		}
		break;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetPowerInfoRegAccess", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetCurPowerInfo
 *
 *	Description:
 *			Get current power setting.
 *	Arguments:
 *			psPowerInfo	power information
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_GetCurPowerInfo
(
	MCDRV_POWER_INFO*	psPowerInfo
)
{
	UINT8	bReg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_GetCurPowerInfo");
#endif


	psPowerInfo->abAnalog[0]	= gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_0];
	psPowerInfo->abAnalog[1]	= gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_1];
	psPowerInfo->abAnalog[2]	= gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_2];
	psPowerInfo->abAnalog[3]	= gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_3];
	psPowerInfo->abAnalog[4]	= gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_4];
	psPowerInfo->abAnalog[5]	= gsGlobalInfo.abRegValB_ANA[MCI_PWM_ANALOG_5];

	psPowerInfo->dDigital	= 0;
	bReg	= gsGlobalInfo.abRegValB_CODEC[MCI_CD_DP];
	if((bReg & (MCB_DP0_CLKI1|MCB_DP0_CLKI0)) == (MCB_DP0_CLKI1|MCB_DP0_CLKI0))
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DP0;
	}
	bReg	= gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL];
	if((bReg & MCB_PWM_DP1) == MCB_PWM_DP1)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DP1;
	}
	if((bReg & MCB_PWM_DP2) == MCB_PWM_DP2)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DP2;
	}

	bReg	= gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL_1];
	if((bReg & MCB_PWM_DPB) == MCB_PWM_DPB)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPB;
	}
	if((bReg & MCB_PWM_DPDI0) == MCB_PWM_DPDI0)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPDI0;
	}
	if((bReg & MCB_PWM_DPDI1) == MCB_PWM_DPDI1)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPDI1;
	}
	if((bReg & MCB_PWM_DPDI2) == MCB_PWM_DPDI2)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPDI2;
	}
	if((bReg & MCB_PWM_DPPDM) == MCB_PWM_DPPDM)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPPDM;
	}
	if((bReg & MCB_PWM_DPCD) == MCB_PWM_DPCD)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPCD;
	}

	if(McDevProf_IsValid(eMCDRV_FUNC_CDSP) != 0)
	{
		bReg	= gsGlobalInfo.abRegValB_BASE[MCI_PWM_DPC];
		if((bReg & MCB_PWM_DPC) == MCB_PWM_DPC)
		{
			psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPC;
		}
	}

	bReg	= gsGlobalInfo.abRegValB_BASE[MCI_PWM_DIGITAL_BDSP];
	if((bReg & MCB_PWM_DPBDSP) == MCB_PWM_DPBDSP)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPBDSP;
	}

	bReg	= gsGlobalInfo.abRegValB_CODEC[MCI_PLL_RST];
	if((bReg & MCB_PLLRST0) == MCB_PLLRST0)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_PLLRST0;
	}

	bReg	= gsGlobalInfo.abRegValB_CODEC[MCI_CD_DP];
	if((bReg & MCB_DPADIF) == MCB_DPADIF)
	{
		psPowerInfo->dDigital	|= MCDRV_POWINFO_DIGITAL_DPADIF;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetCurPowerInfo", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_GetPMSource
 *
 *	Description:
 *			Get PM source information.
 *	Arguments:
 *			none
 *	Return:
 *			path source(MCDRV_SRC_TYPE)
 *
 ****************************************************************************/
MCDRV_SRC_TYPE	McResCtrl_GetPMSource
(
	void
)
{
	MCDRV_SRC_TYPE	eSrcType	= eMCDRV_SRC_NONE;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetPMSource");
#endif

	if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
	{
		eSrcType	= eMCDRV_SRC_PDM;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC0;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC1;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR0;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR1;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR2;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
	{
		eSrcType	= eMCDRV_SRC_MIX;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
	{
		eSrcType	= McResCtrl_GetAESource();
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	{
		eSrcType	= eMCDRV_SRC_DTMF;
	}
	else if((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
	{
		if(McResCtrl_IsValidCDSPSource() != 0)
		{
			eSrcType	= eMCDRV_SRC_CDSP;
		}
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= eSrcType;
	McDebugLog_FuncOut("McResCtrl_GetPMSource", &sdRet);
#endif

	return eSrcType;
}

/****************************************************************************
 *	McResCtrl_GetDITSource
 *
 *	Description:
 *			Get DIT source information.
 *	Arguments:
 *			ePort	port number
 *	Return:
 *			path source(MCDRV_SRC_TYPE)
 *
 ****************************************************************************/
MCDRV_SRC_TYPE	McResCtrl_GetDITSource
(
	MCDRV_DIO_PORT_NO	ePort
)
{
	MCDRV_SRC_TYPE	eSrcType	= eMCDRV_SRC_NONE;
	MCDRV_CHANNEL*	pasDit		= NULL;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetDITSource");
#endif

	if(ePort == 0)
	{
		pasDit	= &gsGlobalInfo.sPathInfo.asDit0[0];
	}
	else if(ePort == 1)
	{
		pasDit	= &gsGlobalInfo.sPathInfo.asDit1[0];
	}
	else if(ePort == 2)
	{
		pasDit	= &gsGlobalInfo.sPathInfo.asDit2[0];
	}
	else
	{
	}

	if(pasDit != NULL)
	{
		if((pasDit->abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			eSrcType	= eMCDRV_SRC_PDM;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		{
			eSrcType	= eMCDRV_SRC_ADC0;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		{
			eSrcType	= eMCDRV_SRC_ADC1;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		{
			eSrcType	= eMCDRV_SRC_DIR0;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		{
			eSrcType	= eMCDRV_SRC_DIR1;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		{
			eSrcType	= eMCDRV_SRC_DIR2;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		{
			eSrcType	= eMCDRV_SRC_MIX;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		{
			eSrcType	= McResCtrl_GetAESource();
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		{
			eSrcType	= eMCDRV_SRC_DTMF;
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		{
			if(McResCtrl_IsValidCDSPSource() != 0)
			{
				eSrcType	= eMCDRV_SRC_CDSP;
			}
		}
		else if((pasDit->abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_DIRECT_ON) == MCDRV_SRC6_CDSP_DIRECT_ON)
		{
			eSrcType	= eMCDRV_SRC_CDSP_DIRECT;
		}
		else
		{
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= eSrcType;
	McDebugLog_FuncOut("McResCtrl_GetDITSource", &sdRet);
#endif

	return eSrcType;
}

/****************************************************************************
 *	McResCtrl_GetDACSource
 *
 *	Description:
 *			Get DAC source information.
 *	Arguments:
 *			eCh	0:Master/1:Voice
 *	Return:
 *			path source(MCDRV_SRC_TYPE)
 *
 ****************************************************************************/
MCDRV_SRC_TYPE	McResCtrl_GetDACSource
(
	MCDRV_DAC_CH	eCh
)
{
	MCDRV_SRC_TYPE	eSrcType	= eMCDRV_SRC_NONE;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetDACSource");
#endif


	if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
	{
		eSrcType	= eMCDRV_SRC_PDM;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC0;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC1;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR0;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR1;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR2;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
	{
		eSrcType	= eMCDRV_SRC_MIX;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
	{
		eSrcType	= McResCtrl_GetAESource();
	}
	else if((eCh == eMCDRV_DAC_VOICE) && (gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	{
		eSrcType	= eMCDRV_SRC_DTMF;
	}
	else if((gsGlobalInfo.sPathInfo.asDac[eCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
	{
		if(McResCtrl_IsValidCDSPSource() != 0)
		{
			eSrcType	= eMCDRV_SRC_CDSP;
		}
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= eSrcType;
	McDebugLog_FuncOut("McResCtrl_GetDACSource", &sdRet);
#endif
	return eSrcType;
}

/****************************************************************************
 *	McResCtrl_GetAESource
 *
 *	Description:
 *			Get AE source information.
 *	Arguments:
 *			none
 *	Return:
 *			path source(MCDRV_SRC_TYPE)
 *
 ****************************************************************************/
MCDRV_SRC_TYPE	McResCtrl_GetAESource
(
	void
)
{
	MCDRV_SRC_TYPE	eSrcType	= eMCDRV_SRC_NONE;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetAESource");
#endif

	if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
	{
		eSrcType	= eMCDRV_SRC_PDM;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC0;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC1;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR0;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR1;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR2;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
	{
		eSrcType	= eMCDRV_SRC_MIX;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	{
		eSrcType	= eMCDRV_SRC_DTMF;
	}
	else if((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
	{
		if(McResCtrl_IsValidCDSPSource() != 0)
		{
			eSrcType	= eMCDRV_SRC_CDSP;
		}
	}
	else
	{
		if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		{
			eSrcType	= eMCDRV_SRC_PDM;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		{
			eSrcType	= eMCDRV_SRC_ADC0;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		{
			eSrcType	= eMCDRV_SRC_ADC1;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		{
			eSrcType	= eMCDRV_SRC_DIR0;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		{
			eSrcType	= eMCDRV_SRC_DIR1;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		{
			eSrcType	= eMCDRV_SRC_DIR2;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		{
			eSrcType	= eMCDRV_SRC_DTMF;
		}
		else if((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		{
			if(McResCtrl_IsValidCDSPSource() != 0)
			{
				eSrcType	= eMCDRV_SRC_CDSP;
			}
		}
		else
		{
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= eSrcType;
	McDebugLog_FuncOut("McResCtrl_GetAESource", &sdRet);
#endif

	return eSrcType;
}

/****************************************************************************
 *	McResCtrl_GetCDSPSource
 *
 *	Description:
 *			Get C-DSP source information.
 *	Arguments:
 *			eCh		channel
 *	Return:
 *			path source(MCDRV_SRC_TYPE)
 *
 ****************************************************************************/
MCDRV_SRC_TYPE	McResCtrl_GetCDSPSource
(
	MCDRV_DST_CH	eCh
)
{
	MCDRV_SRC_TYPE	eSrcType	= eMCDRV_SRC_NONE;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetCDSPSource");
#endif

	if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
	{
		eSrcType	= eMCDRV_SRC_PDM;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC0;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
	{
		eSrcType	= eMCDRV_SRC_ADC1;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR0;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR1;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR2;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_DIRECT_ON) == MCDRV_SRC3_DIR2_DIRECT_ON)
	{
		eSrcType	= eMCDRV_SRC_DIR2_DIRECT;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
	{
		eSrcType	= eMCDRV_SRC_MIX;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
	{
		eSrcType	= McResCtrl_GetAESource();
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
	{
		eSrcType	= eMCDRV_SRC_DTMF;
	}
	else if((gsGlobalInfo.sPathInfo.asCdsp[eCh].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
	{
		eSrcType	= eMCDRV_SRC_CDSP;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= eSrcType;
	McDebugLog_FuncOut("McResCtrl_GetCDSPSource", &sdRet);
#endif

	return eSrcType;
}

/****************************************************************************
 *	McResCtrl_IsValidCDSPSource
 *
 *	Description:
 *			Is C-DSP Src valid
 *	Arguments:
 *			none
 *	Return:
 *			0:invalid/1:valid
 *
 ****************************************************************************/
UINT8	McResCtrl_IsValidCDSPSource
(
	void
)
{
	UINT8	bValid	= 0;
	MCDRV_SRC_TYPE	eSrcType	= eMCDRV_SRC_NONE;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_IsValidCDSPSource");
#endif

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
	if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_CDSP))
	{
		eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
		if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_CDSP))
		{
			eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
			if((eSrcType == eMCDRV_SRC_NONE) || (eSrcType == eMCDRV_SRC_CDSP))
			{
				eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
			}
		}
	}
	if((eSrcType != eMCDRV_SRC_NONE) && (eSrcType != eMCDRV_SRC_CDSP))
	{
		bValid	= 1;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bValid;
	McDebugLog_FuncOut("McResCtrl_IsValidCDSPSource", &sdRet);
#endif

	return bValid;
}

/****************************************************************************
 *	McResCtrl_IsSrcUsed
 *
 *	Description:
 *			Is Src used
 *	Arguments:
 *			ePathSrc	path src type
 *	Return:
 *			0:unused/1:used
 *
 ****************************************************************************/
UINT8	McResCtrl_IsSrcUsed
(
	MCDRV_SRC_TYPE	ePathSrc
)
{
	UINT8	bUsed	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_IsSrcUsed");
#endif

	switch(ePathSrc)
	{
	case	eMCDRV_SRC_MIC1:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_MIC2:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_MIC3:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_LINE1_L:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_LINE1_R:
		if(((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_LINE1_M:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_LINE2_L:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_LINE2_R:
		if(((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_LINE2_M:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DIR0:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DIR1:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DIR2:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DTMF:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_PDM:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_ADC0:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_ADC1:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DAC_L:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DAC_R:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
		|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DAC_M:
		if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
		|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_AE:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_CDSP:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON))
		{
			if(McResCtrl_IsValidCDSPSource() != 0)
			{
				bUsed	= 1;
			}
		}
		break;

	case	eMCDRV_SRC_MIX:
		if(((gsGlobalInfo.sPathInfo.asPeak[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit0[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit1[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asDac[1].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON)
		|| ((gsGlobalInfo.sPathInfo.asAe[0].abSrcOnOff[MCDRV_SRC_MIX_BLOCK] & MCDRV_SRC6_MIX_ON) == MCDRV_SRC6_MIX_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_DIR2_DIRECT:
		if(((gsGlobalInfo.sPathInfo.asCdsp[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_DIRECT_ON) == MCDRV_SRC3_DIR2_DIRECT_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[1].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_DIRECT_ON) == MCDRV_SRC3_DIR2_DIRECT_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[2].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_DIRECT_ON) == MCDRV_SRC3_DIR2_DIRECT_ON)
		|| ((gsGlobalInfo.sPathInfo.asCdsp[3].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_DIRECT_ON) == MCDRV_SRC3_DIR2_DIRECT_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_SRC_CDSP_DIRECT:
		if((gsGlobalInfo.sPathInfo.asDit2[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_DIRECT_ON) == MCDRV_SRC6_CDSP_DIRECT_ON)
		{
			bUsed	= 1;
		}
		break;

	default:
		break;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bUsed;
	McDebugLog_FuncOut("McResCtrl_IsSrcUsed", &sdRet);
#endif

	return bUsed;
}

/****************************************************************************
 *	McResCtrl_IsDstUsed
 *
 *	Description:
 *			Is Destination used
 *	Arguments:
 *			eType	path destination
 *			eCh		channel
 *	Return:
 *			0:unused/1:used
 *
 ****************************************************************************/
UINT8	McResCtrl_IsDstUsed
(
	MCDRV_DST_TYPE	eType,
	MCDRV_DST_CH	eCh
)
{
	UINT8	bUsed	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_IsDstUsed");
#endif

	switch(eType)
	{
	case	eMCDRV_DST_HP:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
			{
				bUsed	= 1;
			}
		}
		else if(eCh == eMCDRV_DST_CH1)
		{
			if(((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asHpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_SP:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
			{
				bUsed	= 1;
			}
		}
		else if(eCh == eMCDRV_DST_CH1)
		{
			if(((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asSpOut[1].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_RCV:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asRcOut[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) ==MCDRV_SRC5_DAC_R_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_LOUT1:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
			{
				bUsed	= 1;
			}
		}
		else if(eCh == eMCDRV_DST_CH1)
		{
			if(((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout1[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_LOUT2:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[0].abSrcOnOff[MCDRV_SRC_DAC_M_BLOCK] & MCDRV_SRC5_DAC_M_ON) == MCDRV_SRC5_DAC_M_ON))
			{
				bUsed	= 1;
			}
		}
		else if(eCh == eMCDRV_DST_CH1)
		{
			if(((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asLout2[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_PEAK:
		if(eCh < PEAK_PATH_CHANNELS)
		{
			if(McResCtrl_GetPMSource() != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_DIT0:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(McResCtrl_GetDITSource(eMCDRV_DIO_0) != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_DIT1:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(McResCtrl_GetDITSource(eMCDRV_DIO_1) != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_DIT2:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(McResCtrl_GetDITSource(eMCDRV_DIO_2) != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_DAC:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(McResCtrl_GetDACSource(eMCDRV_DAC_MASTER) != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
			else if((gsGlobalInfo.sPathInfo.asDac[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON)
			{
				bUsed	= 1;
			}
		}
		else if(eCh == eMCDRV_DST_CH1)
		{
			if(McResCtrl_GetDACSource(eMCDRV_DAC_VOICE) != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_AE:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(McResCtrl_GetAESource() != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_CDSP:
		if(eCh <= eMCDRV_DST_CH3)
		{
			if(McResCtrl_GetCDSPSource(eCh) != eMCDRV_SRC_NONE)
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_ADC0:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & MCDRV_SRC1_LINE1_L_ON) == MCDRV_SRC1_LINE1_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & MCDRV_SRC2_LINE2_L_ON) == MCDRV_SRC2_LINE2_L_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[0].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK] & MCDRV_SRC5_DAC_L_ON) == MCDRV_SRC5_DAC_L_ON))
			{
				bUsed	= 1;
			}
		}
		else if(eCh == eMCDRV_DST_CH1)
		{
			if(((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK] & MCDRV_SRC1_LINE1_R_ON) == MCDRV_SRC1_LINE1_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK] & MCDRV_SRC2_LINE2_R_ON) == MCDRV_SRC2_LINE2_R_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON)
			|| ((gsGlobalInfo.sPathInfo.asAdc0[1].abSrcOnOff[MCDRV_SRC_DAC_R_BLOCK] & MCDRV_SRC5_DAC_R_ON) == MCDRV_SRC5_DAC_R_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	case	eMCDRV_DST_ADC1:
		if(McDevProf_IsValid(eMCDRV_FUNC_ADC1) != 0)
		{
			if(eCh == eMCDRV_DST_CH0)
			{
				if(((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
				|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
				|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON)
				|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & MCDRV_SRC1_LINE1_M_ON) == MCDRV_SRC1_LINE1_M_ON)
				|| ((gsGlobalInfo.sPathInfo.asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & MCDRV_SRC2_LINE2_M_ON) == MCDRV_SRC2_LINE2_M_ON))
				{
					bUsed	= 1;
				}
			}
			else
			{
			}
		}
		break;

	case	eMCDRV_DST_MIX:
		if(eCh != eMCDRV_DST_CH0)
		{
			break;
		}
		if(((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) == MCDRV_SRC6_AE_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_DST_MIX2:
		if(eCh != eMCDRV_DST_CH0)
		{
			break;
		}
		if(((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_PDM_BLOCK] & MCDRV_SRC4_PDM_ON) == MCDRV_SRC4_PDM_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) == MCDRV_SRC4_ADC0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK] & MCDRV_SRC4_ADC1_ON) == MCDRV_SRC4_ADC1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK] & MCDRV_SRC3_DIR0_ON) == MCDRV_SRC3_DIR0_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK] & MCDRV_SRC3_DIR1_ON) == MCDRV_SRC3_DIR1_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK] & MCDRV_SRC3_DIR2_ON) == MCDRV_SRC3_DIR2_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) == MCDRV_SRC6_CDSP_ON)
		|| ((gsGlobalInfo.sPathInfo.asMix2[0].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK] & MCDRV_SRC4_DTMF_ON) == MCDRV_SRC4_DTMF_ON))
		{
			bUsed	= 1;
		}
		break;

	case	eMCDRV_DST_BIAS:
		if(eCh == eMCDRV_DST_CH0)
		{
			if(((gsGlobalInfo.sPathInfo.asBias[0].abSrcOnOff[MCDRV_SRC_MIC1_BLOCK] & MCDRV_SRC0_MIC1_ON) == MCDRV_SRC0_MIC1_ON)
			|| ((gsGlobalInfo.sPathInfo.asBias[0].abSrcOnOff[MCDRV_SRC_MIC2_BLOCK] & MCDRV_SRC0_MIC2_ON) == MCDRV_SRC0_MIC2_ON)
			|| ((gsGlobalInfo.sPathInfo.asBias[0].abSrcOnOff[MCDRV_SRC_MIC3_BLOCK] & MCDRV_SRC0_MIC3_ON) == MCDRV_SRC0_MIC3_ON))
			{
				bUsed	= 1;
			}
		}
		else
		{
		}
		break;

	default:
		break;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (SINT32)bUsed;
	McDebugLog_FuncOut("McResCtrl_IsDstUsed", &sdRet);
#endif

	return bUsed;
}

/****************************************************************************
 *	McResCtrl_GetEFIFOSourceReg
 *
 *	Description:
 *			Get EFIFO_SOURCE register setting.
 *	Arguments:
 *			none
 *	Return:
 *			EFIFO_SOURCE register setting
 *
 ****************************************************************************/
UINT8	McResCtrl_GetEFIFOSourceReg
(
	void
)
{
	UINT8	bReg, bEFIFO0L, bEFIFO0R, bEFIFO1L, bEFIFO1R;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetEFIFOSourceReg");
#endif

	bReg		= McResCtrl_GetRegVal(MCDRV_PACKET_REGTYPE_B_MIXER, MCI_EFIFO_SOURCE);
	bEFIFO0L	= McResCtrl_GetEFIFO0LSource();
	bEFIFO0R	= McResCtrl_GetEFIFO0RSource();
	bEFIFO1L	= McResCtrl_GetEFIFO1LSource();
	bEFIFO1R	= McResCtrl_GetEFIFO1RSource();

	if(bEFIFO0L == 0xFF)
	{
		bEFIFO0L	= bReg & MCB_EFIFO0L_SOURCE;
	}
	if(bEFIFO0R == 0xFF)
	{
		bEFIFO0R	= bReg & MCB_EFIFO0R_SOURCE;
	}
	if(bEFIFO1L == 0xFF)
	{
		bEFIFO1L	= bReg & MCB_EFIFO1L_SOURCE;
	}
	if(bEFIFO1R == 0xFF)
	{
		bEFIFO1R	= bReg & MCB_EFIFO1R_SOURCE;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= (bEFIFO1L | bEFIFO1R | bEFIFO0L | bEFIFO0R);
	McDebugLog_FuncOut("McResCtrl_GetEFIFOSourceReg", &sdRet);
#endif
	return (bEFIFO1L | bEFIFO1R | bEFIFO0L | bEFIFO0R);
}

/****************************************************************************
 *	McResCtrl_GetEFIFOSourceRegCur
 *
 *	Description:
 *			Get EFIFO_SOURCE register setting.
 *	Arguments:
 *				
 *	Return:
 *			EFIFO_SOURCE register setting
 *
 ****************************************************************************/
void	McResCtrl_GetEFIFOSourceRegCur
(
	UINT8*	pbEFIFO0L,
	UINT8*	pbEFIFO0R,
	UINT8*	pbEFIFO1L,
	UINT8*	pbEFIFO1R
)
{
	*pbEFIFO0L	= gsGlobalInfo.bEFIFO0L;
	*pbEFIFO0R	= gsGlobalInfo.bEFIFO0R;
	*pbEFIFO1L	= gsGlobalInfo.bEFIFO1L;
	*pbEFIFO1R	= gsGlobalInfo.bEFIFO1R;
}

/****************************************************************************
 *	McResCtrl_IsEFIFOSourceChanged
 *
 *	Description:
 *			Is EFIFO_SOURCE register setting changed.
 *	Arguments:
 *			none
 *	Return:
 *			1:EFIFO_SOURCE register setting changed
 *
 ****************************************************************************/
/*UINT8	McResCtrl_IsEFIFOSourceChanged
(
	void
)
{
	UINT8	bRet	= 0;
	UINT8	bEFIFO0L, bEFIFO0R, bEFIFO1L, bEFIFO1R;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_IsEFIFOSourceChanged");
#endif

	bEFIFO0L	= McResCtrl_GetEFIFO0LSource();
	bEFIFO0R	= McResCtrl_GetEFIFO0RSource();
	bEFIFO1L	= McResCtrl_GetEFIFO1LSource();
	bEFIFO1R	= McResCtrl_GetEFIFO1RSource();

	if((bEFIFO0L != gsGlobalInfo.bEFIFO0L) || (bEFIFO0R != gsGlobalInfo.bEFIFO0R)
	|| (bEFIFO1L != gsGlobalInfo.bEFIFO1L) || (bEFIFO1R != gsGlobalInfo.bEFIFO1R))
	{
		bRet	= 1;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bRet;
	McDebugLog_FuncOut("McResCtrl_IsEFIFOSourceChanged", &sdRet);
#endif
	return bRet;
}
*/
/****************************************************************************
 *	McResCtrl_GetEFIFO0LSource
 *
 *	Description:
 *			Get EFIFO0L_SOURCE register setting.
 *	Arguments:
 *			none
 *	Return:
 *			EFIFO0L_SOURCE register setting
 *
 ****************************************************************************/
UINT8	McResCtrl_GetEFIFO0LSource
(
	void
)
{
	UINT8	bEFIFO0L	= 0xFF;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetEFIFO0LSource");
#endif

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH0);
	if(eSrcType == eMCDRV_SRC_DIR2_DIRECT)
	{
		bEFIFO0L	= MCB_EFIFO0L_DIR2;
	}
/*
	else if(eSrcType == )
	{
		bEFIFO0L	= MCB_EFIFO0L_AEOUT;
	}
*/
	else if(eSrcType != eMCDRV_SRC_NONE)
	{
		bEFIFO0L	= MCB_EFIFO0L_ESRC0;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bEFIFO0L;
	McDebugLog_FuncOut("McResCtrl_GetEFIFO0LSource", &sdRet);
#endif
	return bEFIFO0L;
}

/****************************************************************************
 *	McResCtrl_GetEFIFO0RSource
 *
 *	Description:
 *			Get EFIFO0R_SOURCE register setting.
 *	Arguments:
 *			none
 *	Return:
 *			EFIFO0R_SOURCE register setting
 *
 ****************************************************************************/
UINT8	McResCtrl_GetEFIFO0RSource
(
	void
)
{
	UINT8	bEFIFO0R	= 0xFF;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetEFIFO0RSource");
#endif

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH1);
	if(eSrcType  == eMCDRV_SRC_DIR2_DIRECT)
	{
		bEFIFO0R	= MCB_EFIFO0R_DIR2;
	}
/*
	else if(eSrcType == )
	{
		bEFIFO0R	= MCB_EFIFO0R_AEOUT;
	}
*/
	else if(eSrcType != eMCDRV_SRC_NONE)
	{
		bEFIFO0R	= MCB_EFIFO0R_ESRC0;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bEFIFO0R;
	McDebugLog_FuncOut("McResCtrl_GetEFIFO0RSource", &sdRet);
#endif
	return bEFIFO0R;
}

/****************************************************************************
 *	McResCtrl_GetEFIFO1LSource
 *
 *	Description:
 *			Get EFIFO1L_SOURCE register setting.
 *	Arguments:
 *			none
 *	Return:
 *			EFIFO1L_SOURCE register setting
 *
 ****************************************************************************/
UINT8	McResCtrl_GetEFIFO1LSource
(
	void
)
{
	UINT8	bEFIFO1L	= 0xFF;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetEFIFO1LSource");
#endif

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH2);
	if(eSrcType == eMCDRV_SRC_DIR2_DIRECT)
	{
		bEFIFO1L	= MCB_EFIFO1L_DIR2;
	}
/*
	else if(eSrcType == )
	{
		bEFIFO1L	= MCB_EFIFO1L_AEOUT;
	}
*/
	else if(eSrcType != eMCDRV_SRC_NONE)
	{
		bEFIFO1L	= MCB_EFIFO1L_ESRC1;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bEFIFO1L;
	McDebugLog_FuncOut("McResCtrl_GetEFIFO1LSource", &sdRet);
#endif
	return bEFIFO1L;
}

/****************************************************************************
 *	McResCtrl_GetEFIFO1RSource
 *
 *	Description:
 *			Get EFIFO1R_SOURCE register setting.
 *	Arguments:
 *			none
 *	Return:
 *			EFIFO1R_SOURCE register setting
 *
 ****************************************************************************/
UINT8	McResCtrl_GetEFIFO1RSource
(
	void
)
{
	UINT8	bEFIFO1R	= 0xFF;
	MCDRV_SRC_TYPE	eSrcType;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetEFIFO1RSource");
#endif

	eSrcType	= McResCtrl_GetCDSPSource(eMCDRV_DST_CH3);
	if(eSrcType == eMCDRV_SRC_DIR2_DIRECT)
	{
		bEFIFO1R	= MCB_EFIFO1R_DIR2;
	}
/*
	else if(eSrcType == )
	{
		bEFIFO1R	= MCB_EFIFO1R_AEOUT;
	}
*/
	else if(eSrcType != eMCDRV_SRC_NONE)
	{
		bEFIFO1R	= MCB_EFIFO1R_ESRC1;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bEFIFO1R;
	McDebugLog_FuncOut("McResCtrl_GetEFIFO1RSource", &sdRet);
#endif
	return bEFIFO1R;
}

/****************************************************************************
 *	McResCtrl_GetRegAccess
 *
 *	Description:
 *			Get register access availability
 *	Arguments:
 *			psRegInfo	register information
 *	Return:
 *			MCDRV_REG_ACCSESS
 *
 ****************************************************************************/
MCDRV_REG_ACCSESS	McResCtrl_GetRegAccess
(
	const MCDRV_REG_INFO*	psRegInfo
)
{
	MCDRV_REG_ACCSESS	eAccess	= eMCDRV_ACCESS_DENY;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_GetRegAccess");
#endif

	eAccess	= McDevProf_GetRegAccess(psRegInfo);

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= eAccess;
	McDebugLog_FuncOut("McResCtrl_GetRegAccess", &sdRet);
#endif

	return eAccess;
}

/****************************************************************************
 *	McResCtrl_GetAPMode
 *
 *	Description:
 *			get auto power management mode.
 *	Arguments:
 *			none
 *	Return:
 *			eMCDRV_APM_ON
 *			eMCDRV_APM_OFF
 *
 ****************************************************************************/
MCDRV_PMODE	McResCtrl_GetAPMode
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet	= gsGlobalInfo.eAPMode;
	McDebugLog_FuncIn("McResCtrl_GetAPMode");
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_GetAPMode", &sdRet);
#endif

	return gsGlobalInfo.eAPMode;
}


/****************************************************************************
 *	McResCtrl_AllocPacketBuf
 *
 *	Description:
 *			allocate the buffer for register setting packets.
 *	Arguments:
 *			none
 *	Return:
 *			pointer to the area to store packets
 *
 ****************************************************************************/
MCDRV_PACKET*	McResCtrl_AllocPacketBuf
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_AllocPacketBuf");
#endif

	if(eMCDRV_PACKETBUF_ALLOCATED == gsGlobalInfo.ePacketBufAlloc)
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= 0;
			McDebugLog_FuncOut("McResCtrl_AllocPacketBuf", &sdRet);
		#endif
		return NULL;
	}

	gsGlobalInfo.ePacketBufAlloc = eMCDRV_PACKETBUF_ALLOCATED;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_AllocPacketBuf", 0);
#endif
	return gasPacket;
}

/****************************************************************************
 *	McResCtrl_ReleasePacketBuf
 *
 *	Description:
 *			Release the buffer for register setting packets.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_ReleasePacketBuf
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_ReleasePacketBuf");
#endif

	gsGlobalInfo.ePacketBufAlloc = eMCDRV_PACKETBUF_FREE;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_ReleasePacketBuf", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_InitRegUpdate
 *
 *	Description:
 *			Initialize the process of register update.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_InitRegUpdate
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_InitRegUpdate");
#endif

	gsGlobalInfo.sCtrlPacket.wDataNum	= 0;
	gsGlobalInfo.wCurSlaveAddress		= 0xFFFF;
	gsGlobalInfo.wCurRegType			= 0xFFFF;
	gsGlobalInfo.wCurRegAddress			= 0xFFFF;
	gsGlobalInfo.wDataContinueCount		= 0;
	gsGlobalInfo.wPrevAddressIndex		= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_InitRegUpdate", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_AddRegUpdate
 *
 *	Description:
 *			Add register update packet and save register value.
 *	Arguments:
 *			wRegType	register type
 *			wAddress	address
 *			bData		write data
 *			eUpdateMode	updete mode
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_AddRegUpdate
(
	UINT16	wRegType,
	UINT16	wAddress,
	UINT8	bData,
	MCDRV_UPDATE_MODE	eUpdateMode
)
{
	UINT8			*pbRegVal;
	UINT8			bAddressADR;
	UINT8			bAddressWINDOW;
	UINT8			*pbCtrlData;
	UINT16			*pwCtrlDataNum;
	const UINT16	*pwNextAddress;
	UINT16			wNextAddress;
	UINT16			wSlaveAddress;
	MCDRV_REG_INFO	sRegInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_AddRegUpdate");
#endif

	switch (wRegType)
	{
	case MCDRV_PACKET_REGTYPE_A:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		pbRegVal			= gsGlobalInfo.abRegValA;
		pwNextAddress		= gawNextAddressA;
		bAddressADR			= (UINT8)wAddress;
		bAddressWINDOW		= bAddressADR;
		sRegInfo.bRegType	= MCDRV_REGTYPE_A;
		break;

	case MCDRV_PACKET_REGTYPE_B_BASE:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		pbRegVal			= gsGlobalInfo.abRegValB_BASE;
		pwNextAddress		= gawNextAddressB_BASE;
		bAddressADR			= MCI_BASE_ADR;
		bAddressWINDOW		= MCI_BASE_WINDOW;
		sRegInfo.bRegType	= MCDRV_REGTYPE_B_BASE;
		break;

	case MCDRV_PACKET_REGTYPE_B_CODEC:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		pbRegVal			= gsGlobalInfo.abRegValB_CODEC;
		pwNextAddress		= gawNextAddressB_CODEC;
		bAddressADR			= MCI_CD_ADR;
		bAddressWINDOW		= MCI_CD_WINDOW;
		sRegInfo.bRegType	= MCDRV_REGTYPE_B_CODEC;
		break;

	case MCDRV_PACKET_REGTYPE_B_ANA:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		pbRegVal			= gsGlobalInfo.abRegValB_ANA;
		pwNextAddress		= gawNextAddressB_Ana;
		bAddressADR			= MCI_ANA_ADR;
		bAddressWINDOW		= MCI_ANA_WINDOW;
		sRegInfo.bRegType	= MCDRV_REGTYPE_B_ANALOG;
		break;

	case MCDRV_PACKET_REGTYPE_B_MIXER:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		pbRegVal			= gsGlobalInfo.abRegValB_MIXER;
		pwNextAddress		= gawNextAddressB_MIXER;
		bAddressADR			= MCI_MIX_ADR;
		bAddressWINDOW		= MCI_MIX_WINDOW;
		sRegInfo.bRegType	= MCDRV_REGTYPE_B_MIXER;
		break;

	case MCDRV_PACKET_REGTYPE_B_AE:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		pbRegVal			= gsGlobalInfo.abRegValB_AE;
		pwNextAddress		= gawNextAddressB_AE;
		bAddressADR			= MCI_AE_ADR;
		bAddressWINDOW		= MCI_AE_WINDOW;
		sRegInfo.bRegType	= MCDRV_REGTYPE_B_AE;
		break;

	case MCDRV_PACKET_REGTYPE_B_CDSP:
		wSlaveAddress		= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		pbRegVal			= gsGlobalInfo.abRegValB_CDSP;
		pwNextAddress		= gawNextAddressB_CDSP;
		bAddressADR			= MCI_CDSP_ADR;
		bAddressWINDOW		= MCI_CDSP_WINDOW;
		sRegInfo.bRegType	= MCDRV_REGTYPE_B_CDSP;
		break;

	default:
		#if (MCDRV_DEBUG_LEVEL>=4)
			McDebugLog_FuncOut("McResCtrl_AddRegUpdate", 0);
		#endif
		return;
	}

	sRegInfo.bAddress	= (UINT8)wAddress;
	if((McResCtrl_GetRegAccess(&sRegInfo) & eMCDRV_CAN_WRITE) == 0)
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			McDebugLog_FuncOut("McResCtrl_AddRegUpdate", 0);
		#endif
		return;
	}

	if((gsGlobalInfo.wCurSlaveAddress != 0xFFFF) && (gsGlobalInfo.wCurSlaveAddress != wSlaveAddress))
	{
		McResCtrl_ExecuteRegUpdate();
		McResCtrl_InitRegUpdate();
	}

	if((gsGlobalInfo.wCurRegType != 0xFFFF) && (gsGlobalInfo.wCurRegType != wRegType))
	{
		McResCtrl_ExecuteRegUpdate();
		McResCtrl_InitRegUpdate();
	}

	if((eMCDRV_UPDATE_FORCE == eUpdateMode) || (bData != pbRegVal[wAddress]))
	{
		if(gsGlobalInfo.wCurRegAddress == 0xFFFF)
		{
			gsGlobalInfo.wCurRegAddress	= wAddress;
		}

		if (eMCDRV_UPDATE_DUMMY != eUpdateMode)
		{
			pbCtrlData		= gsGlobalInfo.sCtrlPacket.abData;
			pwCtrlDataNum	= &(gsGlobalInfo.sCtrlPacket.wDataNum);
			wNextAddress	= pwNextAddress[gsGlobalInfo.wCurRegAddress];

			if ((wSlaveAddress == gsGlobalInfo.wCurSlaveAddress)
			&& (wRegType == gsGlobalInfo.wCurRegType)
			&& (0xFFFF != wNextAddress)
			&& (wAddress != wNextAddress))
			{
				if (pwNextAddress[wNextAddress] == wAddress)
				{
					if (0 == gsGlobalInfo.wDataContinueCount)
					{
						pbCtrlData[gsGlobalInfo.wPrevAddressIndex] |= MCDRV_BURST_WRITE_ENABLE;
					}
					pbCtrlData[*pwCtrlDataNum]	= pbRegVal[wNextAddress];
					(*pwCtrlDataNum)++;
					gsGlobalInfo.wDataContinueCount++;
					wNextAddress				= pwNextAddress[wNextAddress];
				}
				else if ((0xFFFF != pwNextAddress[wNextAddress])
						&& (pwNextAddress[pwNextAddress[wNextAddress]] == wAddress))
				{
					if (0 == gsGlobalInfo.wDataContinueCount)
					{
						pbCtrlData[gsGlobalInfo.wPrevAddressIndex] |= MCDRV_BURST_WRITE_ENABLE;
					}
					pbCtrlData[*pwCtrlDataNum]	= pbRegVal[wNextAddress];
					(*pwCtrlDataNum)++;
					pbCtrlData[*pwCtrlDataNum]	= pbRegVal[pwNextAddress[wNextAddress]];
					(*pwCtrlDataNum)++;
					gsGlobalInfo.wDataContinueCount += 2;
					wNextAddress				= pwNextAddress[pwNextAddress[wNextAddress]];
				}
				else
				{
				}
			}

			if ((0 == *pwCtrlDataNum) || (wAddress != wNextAddress))
			{
				if (0 != gsGlobalInfo.wDataContinueCount)
				{
					McResCtrl_ExecuteRegUpdate();
					McResCtrl_InitRegUpdate();
				}

				if (MCDRV_PACKET_REGTYPE_A == (UINT32)wRegType)
				{
					pbCtrlData[*pwCtrlDataNum]		= (bAddressADR << 1);
					gsGlobalInfo.wPrevAddressIndex	= *pwCtrlDataNum;
					(*pwCtrlDataNum)++;
				}
				else
				{
					pbCtrlData[(*pwCtrlDataNum)++]	= (bAddressADR << 1);
					pbCtrlData[(*pwCtrlDataNum)++]	= (UINT8)wAddress;
					pbCtrlData[*pwCtrlDataNum]		= (bAddressWINDOW << 1);
					gsGlobalInfo.wPrevAddressIndex	= (*pwCtrlDataNum)++;
				}
			}
			else
			{
				if (0 == gsGlobalInfo.wDataContinueCount)
				{
					pbCtrlData[gsGlobalInfo.wPrevAddressIndex] |= MCDRV_BURST_WRITE_ENABLE;
				}
				gsGlobalInfo.wDataContinueCount++;
			}

			pbCtrlData[(*pwCtrlDataNum)++]	= bData;
		}

		gsGlobalInfo.wCurSlaveAddress	= wSlaveAddress;
		gsGlobalInfo.wCurRegType		= wRegType;
		gsGlobalInfo.wCurRegAddress		= wAddress;

		/* save register value */
		pbRegVal[wAddress] = bData;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_AddRegUpdate", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_ExecuteRegUpdate
 *
 *	Description:
 *			Add register update packet and save register value.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McResCtrl_ExecuteRegUpdate
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_ExecuteRegUpdate");
#endif

	if (0 != gsGlobalInfo.sCtrlPacket.wDataNum)
	{
		McSrv_WriteI2C((UINT8)gsGlobalInfo.wCurSlaveAddress, gsGlobalInfo.sCtrlPacket.abData, gsGlobalInfo.sCtrlPacket.wDataNum);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_ExecuteRegUpdate", 0);
#endif
}

/****************************************************************************
 *	McResCtrl_WaitEvent
 *
 *	Description:
 *			Wait event.
 *	Arguments:
 *			dEvent	event to wait
 *			dParam	event parameter
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
SINT32	McResCtrl_WaitEvent
(
	UINT32	dEvent,
	UINT32	dParam
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;
	UINT32	dInterval;
	UINT32	dTimeOut;
	UINT8	bSlaveAddr;
	UINT8	abWriteData[2];

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McResCtrl_WaitEvent");
#endif


	switch(dEvent)
	{
	case	MCDRV_EVT_INSFLG:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DAC_INS_FLAG;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, MCI_MIX_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		else if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_INS_FLAG;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam&(UINT32)0xFF), dInterval, dTimeOut);
		}
		else
		{
		}
		break;

	case	MCDRV_EVT_ALLMUTE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_DIT_INVFLAGL;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_DIT0_INVFLAGL|MCB_DIT1_INVFLAGL|MCB_DIT2_INVFLAGL|MCB_DIT2_INVFLAGL1), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_DIT_INVFLAGR;
		McSrv_WriteI2C(McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG), abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_DIT0_INVFLAGR|MCB_DIT1_INVFLAGR|MCB_DIT2_INVFLAGR|MCB_DIT2_INVFLAGR1), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_DIR_VFLAGL;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_PDM0_VFLAGL|MCB_DIR0_VFLAGL|MCB_DIR1_VFLAGL|MCB_DIR2_VFLAGL|MCB_ADC1_VFLAGL), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_DIR_VFLAGR;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_PDM0_VFLAGR|MCB_DIR0_VFLAGR|MCB_DIR1_VFLAGR|MCB_DIR2_VFLAGR|MCB_ADC1_VFLAGR), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_AD_VFLAGL;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_ADC_VFLAGL|MCB_DTMFB_VFLAGL|MCB_AENG6_VFLAGL), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_AD_VFLAGR;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_ADC_VFLAGR|MCB_DTMFB_VFLAGR|MCB_AENG6_VFLAGR), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_AFLAGL;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_ADC1_AFLAGL|MCB_ADC_AFLAGL|MCB_DIR0_AFLAGL|MCB_DIR1_AFLAGL|MCB_DIR2_AFLAGL), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_AFLAGR;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_ADC1_AFLAGR|MCB_ADC_AFLAGR|MCB_DIR0_AFLAGR|MCB_DIR1_AFLAGR|MCB_DIR2_AFLAGR), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_DAC_FLAGL;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_ST_FLAGL|MCB_MASTER_OFLAGL|MCB_VOICE_FLAGL|MCB_DTMF_FLAGL|MCB_DAC_FLAGL), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_MIX_ADR<<1;
		abWriteData[1]	= MCI_DAC_FLAGR;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (MCB_ST_FLAGR|MCB_MASTER_OFLAGR|MCB_VOICE_FLAGR|MCB_DTMF_FLAGL|MCB_DAC_FLAGR), dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_DITMUTE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DIT_INVFLAGL;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DIT_INVFLAGR;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam&(UINT32)0xFF), dInterval, dTimeOut);
		}
		break;

	case	MCDRV_EVT_DACMUTE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DAC_FLAGL;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DAC_FLAGR;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam&(UINT32)0xFF), dInterval, dTimeOut);
		}
		break;

	case	MCDRV_EVT_ADCMUTE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_AD_VFLAGL;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_AD_VFLAGR;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam&(UINT32)0xFF), dInterval, dTimeOut);
		}
		break;

	case	MCDRV_EVT_ADC1MUTE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DIR_VFLAGL;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DIR_VFLAGR;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam&(UINT32)0xFF), dInterval, dTimeOut);
		}
		break;

	case	MCDRV_EVT_PDMMUTE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DIR_VFLAGL;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_MIX_ADR<<1;
			abWriteData[1]	= MCI_DIR_VFLAGR;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_MIX_WINDOW, (UINT8)(dParam&(UINT32)0xFF), dInterval, dTimeOut);
		}
		break;

	case	MCDRV_EVT_SVOL_DONE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		if((dParam>>8) != (UINT32)0)
		{
			abWriteData[0]	= MCI_ANA_ADR<<1;
			abWriteData[1]	= MCI_BUSY1;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_ANA_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
			if(MCDRV_SUCCESS != sdRet)
			{
				break;
			}
		}
		if((dParam&(UINT32)0xFF) != (UINT32)0)
		{
			abWriteData[0]	= MCI_ANA_ADR<<1;
			abWriteData[1]	= MCI_BUSY2;
			McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
			sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_ANA_WINDOW, (UINT8)(dParam&(UINT8)0xFF), dInterval, dTimeOut);
		}
		break;

	case	MCDRV_EVT_APM_DONE:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_ANA_ADR<<1;
		abWriteData[1]	= MCI_AP_A1;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitSet(bSlaveAddr, (UINT16)MCI_ANA_WINDOW, (UINT8)(dParam>>8), dInterval, dTimeOut);
		if(MCDRV_SUCCESS != sdRet)
		{
			break;
		}
		abWriteData[0]	= MCI_ANA_ADR<<1;
		abWriteData[1]	= MCI_AP_A2;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitSet(bSlaveAddr, (UINT16)MCI_ANA_WINDOW, (UINT8)(dParam&(UINT8)0xFF), dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_ANA_RDY:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dAnaRdyInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dAnaRdyTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_ANA_ADR<<1;
		abWriteData[1]	= MCI_RDY_FLAG;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitSet(bSlaveAddr, (UINT16)MCI_ANA_WINDOW, (UINT8)dParam, dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_SYSEQ_FLAG_RESET:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_CD_ADR<<1;
		abWriteData[1]	= MCI_SYSTEM_EQON;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_CD_WINDOW, MCB_SYSEQ_FLAG, dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CLKBUSY_RESET:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_CD_ADR<<1;
		abWriteData[1]	= MCI_CD_DP;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_CD_WINDOW, MCB_CLKBUSY, dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CLKSRC_SET:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_CD_ADR<<1;
		abWriteData[1]	= MCI_CD_DP;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitSet(bSlaveAddr, (UINT16)MCI_CD_WINDOW, MCB_CLKSRC, dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CLKSRC_RESET:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_CD_ADR<<1;
		abWriteData[1]	= MCI_CD_DP;
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_CD_WINDOW, MCB_CLKSRC, dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CDSPBFLAG_SET:
		dInterval	= MCDRV_INTERVAL_CDSPFLG;
		dTimeOut	= MCDRV_TIME_OUT_CDSPFLG;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		sdRet	= WaitCDSPBitSet(bSlaveAddr, (UINT16)(dParam>>8), (UINT8)(dParam&(UINT8)0xFF), dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CDSPBFLAG_RESET:
		dInterval	= MCDRV_INTERVAL_CDSPFLG;
		dTimeOut	= MCDRV_TIME_OUT_CDSPFLG;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		sdRet	= WaitCDSPBitRelease(bSlaveAddr, (UINT16)(dParam>>8), (UINT8)(dParam&(UINT8)0xFF), dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CDFLAG_SET:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_CD_ADR<<1;
		abWriteData[1]	= (UINT8)(dParam>>8);
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitSet(bSlaveAddr, (UINT16)MCI_CD_WINDOW, (UINT8)(dParam&(UINT8)0xFF), dInterval, dTimeOut);
		break;

	case	MCDRV_EVT_CDFLAG_RESET:
		dInterval	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolInterval;
		dTimeOut	= gsGlobalInfo.sInitInfo.sWaitTime.dSvolTimeOut;
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_ANA);
		abWriteData[0]	= MCI_CD_ADR<<1;
		abWriteData[1]	= (UINT8)(dParam>>8);
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		sdRet	= WaitBitRelease(bSlaveAddr, (UINT16)MCI_CD_WINDOW, (UINT8)(dParam&(UINT8)0xFF), dInterval, dTimeOut);
		break;

	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McResCtrl_WaitEvent", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	WaitBitSet
 *
 *	Description:
 *			Wait register bit to set.
 *	Arguments:
 *			bSlaveAddr	slave address
 *			wRegAddr	register address
 *			bBit		bit
 *			dCycleTime	cycle time to poll [us]
 *			dTimeOut	number of read cycles for time out
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	WaitBitSet
(
	UINT8	bSlaveAddr,
	UINT16	wRegAddr,
	UINT8	bBit,
	UINT32	dCycleTime,
	UINT32	dTimeOut
)
{
	UINT8	bData;
	UINT32	dCycles;
	SINT32	sdRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("WaitBitSet");
#endif


	dCycles	= 0;
	sdRet	= MCDRV_ERROR_TIMEOUT;

	while(dCycles < dTimeOut)
	{
		bData	= McSrv_ReadI2C(bSlaveAddr, wRegAddr);
		if((bData & bBit) == bBit)
		{
			sdRet	= MCDRV_SUCCESS;
			break;
		}

		McSrv_Sleep(dCycleTime);
		dCycles++;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("WaitBitSet", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	WaitBitRelease
 *
 *	Description:
 *			Wait register bit to release.
 *	Arguments:
 *			bSlaveAddr	slave address
 *			wRegAddr	register address
 *			bBit		bit
 *			dCycleTime	cycle time to poll [us]
 *			dTimeOut	number of read cycles for time out
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	WaitBitRelease
(
	UINT8	bSlaveAddr,
	UINT16	wRegAddr,
	UINT8	bBit,
	UINT32	dCycleTime,
	UINT32	dTimeOut
)
{
	UINT8	bData;
	UINT32	dCycles;
	SINT32	sdRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("WaitBitRelease");
#endif


	dCycles	= 0;
	sdRet	= MCDRV_ERROR_TIMEOUT;

	while(dCycles < dTimeOut)
	{
		bData	= McSrv_ReadI2C(bSlaveAddr, wRegAddr);
		if(0 == (bData & bBit))
		{
			sdRet	= MCDRV_SUCCESS;
			break;
		}

		McSrv_Sleep(dCycleTime);
		dCycles++;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("WaitBitRelease", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	WaitCDSPBitSet
 *
 *	Description:
 *			Wait C-DSP register bit to set.
 *	Arguments:
 *			bSlaveAddr	slave address
 *			wRegAddr	register address
 *			bBit		bit
 *			dCycleTime	cycle time to poll [us]
 *			dTimeOut	number of read cycles for time out
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	WaitCDSPBitSet
(
	UINT8	bSlaveAddr,
	UINT16	wRegAddr,
	UINT8	bBit,
	UINT32	dCycleTime,
	UINT32	dTimeOut
)
{
	UINT8	bData;
	UINT32	dCycles;
	SINT32	sdRet;
	UINT8	abWriteData[2];

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("WaitCDSPBitSet");
#endif


	dCycles	= 0;
	sdRet	= MCDRV_ERROR_TIMEOUT;

	abWriteData[0]	= MCI_CDSP_ADR<<1;
	abWriteData[1]	= (UINT8)wRegAddr;

	while(dCycles < dTimeOut)
	{
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		bData	= McSrv_ReadI2C(bSlaveAddr, MCI_CDSP_WINDOW);
		if((bData & bBit) == bBit)
		{
			sdRet	= MCDRV_SUCCESS;
			break;
		}

		McSrv_Sleep(dCycleTime);
		dCycles++;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("WaitCDSPBitSet", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	WaitBitCDSPRelease
 *
 *	Description:
 *			Wait C-DSP register bit to release.
 *	Arguments:
 *			bSlaveAddr	slave address
 *			wRegAddr	register address
 *			bBit		bit
 *			dCycleTime	cycle time to poll [us]
 *			dTimeOut	number of read cycles for time out
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_TIMEOUT
 *
 ****************************************************************************/
static SINT32	WaitCDSPBitRelease
(
	UINT8	bSlaveAddr,
	UINT16	wRegAddr,
	UINT8	bBit,
	UINT32	dCycleTime,
	UINT32	dTimeOut
)
{
	UINT8	bData;
	UINT32	dCycles;
	SINT32	sdRet;
	UINT8	abWriteData[2];

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("WaitCDSPBitRelease");
#endif


	dCycles	= 0;
	sdRet	= MCDRV_ERROR_TIMEOUT;

	abWriteData[0]	= MCI_CDSP_ADR<<1;
	abWriteData[1]	= (UINT8)wRegAddr;

	while(dCycles < dTimeOut)
	{
		McSrv_WriteI2C(bSlaveAddr, abWriteData, 2);
		bData	= McSrv_ReadI2C(bSlaveAddr, MCI_CDSP_WINDOW);
		if(0 == (bData & bBit))
		{
			sdRet	= MCDRV_SUCCESS;
			break;
		}

		McSrv_Sleep(dCycleTime);
		dCycles++;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("WaitCDSPBitRelease", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McResCtrl_IsEnableIRQ
 *
 *	Description:
 *			Is enable IRQ.
 *	Arguments:
 *			none
 *	Return:
 *			0:Disable/1:Enable
 *
 ****************************************************************************/
UINT8	McResCtrl_IsEnableIRQ
(
	void
)
{
	UINT8	bRet	= 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_IsEnableIRQ");
#endif

	if(McDevProf_IsValid(eMCDRV_FUNC_IRQ) != 0)
	{
		if((gsGlobalInfo.sInitInfo.bPad0Func == MCDRV_PAD_IRQ)
		|| (gsGlobalInfo.sInitInfo.bPad1Func == MCDRV_PAD_IRQ)
		|| (gsGlobalInfo.sInitInfo.bPad2Func == MCDRV_PAD_IRQ))
		{
			bRet	= 1;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= bRet;
	McDebugLog_FuncOut("McResCtrl_IsEnableIRQ", &sdRet);
#endif
	return bRet;
}

/****************************************************************************
 *	McResCtrl_IncIrqDisableCount
 *
 *	Description:
 *			Increment IrqDisableCount and return current value.
 *	Arguments:
 *			none
 *	Return:
 *			current value
 *
 ****************************************************************************/
SINT32	McResCtrl_IncIrqDisableCount
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_IncIrqDisableCount");
#endif

	if ( (SINT32)0x7FFFFFFF > gsGlobalInfo.sdIrqDisableCount )
	{
		gsGlobalInfo.sdIrqDisableCount++;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= gsGlobalInfo.sdIrqDisableCount;
	McDebugLog_FuncOut("McResCtrl_IncIrqDisableCount", &sdRet);
#endif
	return gsGlobalInfo.sdIrqDisableCount;
}

/****************************************************************************
 *	McResCtrl_DecIrqDisableCount
 *
 *	Description:
 *			Decrement IrqDisableCount and return current value.
 *	Arguments:
 *			none
 *	Return:
 *			current value
 *
 ****************************************************************************/
SINT32	McResCtrl_DecIrqDisableCount
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McResCtrl_DecIrqDisableCount");
#endif

	if ( (SINT32)0x80000000 < gsGlobalInfo.sdIrqDisableCount )
	{
		gsGlobalInfo.sdIrqDisableCount--;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= gsGlobalInfo.sdIrqDisableCount;
	McDebugLog_FuncOut("McResCtrl_DecIrqDisableCount", &sdRet);
#endif
	return gsGlobalInfo.sdIrqDisableCount;
}

