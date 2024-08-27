/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mccdspdrv.c
 *
 *		Description	: CDSP Driver
 *
 *		Version		: 2.0.2 	2011.08.27
 *
 ****************************************************************************/
#include "mctypedef.h"
#include "mcdefs.h"
#include "mcdriver.h"
#include "mcdevif.h"
#include "mccdspdrv.h"
#include "mccdspdrv_d.h"
#include "mccdspos.h"
#include "mcmachdep.h"
#if MCDRV_DEBUG_LEVEL
#include "mcdebuglog.h"
#endif



static DEC_INFO gsDecInfo =
{
	CODER_DEC,								/* dCoderId */
	CDSP_STATE_NOTINIT,						/* dState */
	{										/* sProgInfo */
		0,									/* wVendorId */
		0,									/* wFunctionId */
		0,									/* wProgType */
		0,									/* wProgScramble */
		0,									/* wDataScramble */
		0,									/* wEntryAddress */
		0,									/* wFuncTableLoadAddress */
		0,									/* wFuncTableSize */
		0,									/* wPatchTableLoadAddress */
		0,									/* wPatchTableSize */
		0,									/* wProgramLoadAddress */
		0,									/* wProgramSize */
		0,									/* wDataLoadAddress */
		0,									/* wDataSize */
		0,									/* wExtProgramLoadAddress */
		0,									/* wExtProgramSize */
		0,									/* wExtDataLoadAddress */
		0,									/* wExtDataSize */
		0,									/* wDramBeginAddress */
		0,									/* wDramSize */
		0,									/* wStackBeginAddress */
		0,									/* wStackSize */
		0,									/* wVramSel */
		0									/* wIromSel */
	},
	{										/* sFormat */
		CODER_FMT_FS_48000					/* bFs */
	},
	{										/* sParams */
		0,									/* bCommandId */
		{									/* abParam */
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0
		}
	},
	{										/* sBitWidth */
		16,									/* bEfifoBit */
		16									/* bOfifoBit */
	},
	{										/* sFifoSyncCh */
		1,									/* bEfifoSync */
		1,									/* bOfifoSync */
		2,									/* bEfifoCh */
		2									/* bOfifoCh */
	},
	0,										/* wOfifoIrqPnt */
};

static DEC_INFO gsEncInfo =
{
	CODER_ENC,								/* dCoderId */
	CDSP_STATE_NOTINIT,						/* dState */
	{										/* sProgInfo */
		0,									/* wVendorId */
		0,									/* wFunctionId */
		0,									/* wProgType */
		0,									/* wProgScramble */
		0,									/* wDataScramble */
		0,									/* wEntryAddress */
		0,									/* wFuncTableLoadAddress */
		0,									/* wFuncTableSize */
		0,									/* wPatchTableLoadAddress */
		0,									/* wPatchTableSize */
		0,									/* wProgramLoadAddress */
		0,									/* wProgramSize */
		0,									/* wDataLoadAddress */
		0,									/* wDataSize */
		0,									/* wExtProgramLoadAddress */
		0,									/* wExtProgramSize */
		0,									/* wExtDataLoadAddress */
		0,									/* wExtDataSize */
		0,									/* wDramBeginAddress */
		0,									/* wDramSize */
		0,									/* wStackBeginAddress */
		0,									/* wStackSize */
		0,									/* wVramSel */
		0									/* wIromSel */
	},
	{										/* sFormat */
		CODER_FMT_FS_48000					/* bFs */
	},
	{										/* sParams */
		0,									/* bCommandId */
		{									/* abParam */
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0
		}
	},
	{										/* sBitWidth */
		32,									/* bEfifoBit */
		32									/* bOfifoBit */
	},
	{										/* sFifoSyncCh */
		2,									/* bEfifoSync */
		2,									/* bOfifoSync */
		2,									/* bEfifoCh */
		2									/* bOfifoCh */
	},
	0,										/* wOfifoIrqPnt */
};


static DEC_INFO*	GetDecInfo(UINT32 dCoderId);
static SINT32		CommandWaitComplete(UINT32 dCoderId);
static SINT32		CommandInitialize(UINT32 dCoderId);
static SINT32		CommandWriteHost2Os(UINT32 dCoderId, MC_CODER_PARAMS *psParam);
static void			InitializeRegister(void);
static SINT32		InitializeOS(void);
static void			DownloadOS(const UINT8 *pbFirmware);
static void			Initialize(void);
static void			TerminateProgram(UINT32 dCoderId);
static SINT32		CheckProgram(UINT32 dCoderId, const MC_CODER_FIRMWARE *psProgram);
static void			WriteProgram(FSQ_DATA_INFO *psDataInfo);
static SINT32		DownloadProgram(const UINT8 *pbFirmware);
static SINT32		InitializeProgram(UINT32 dCoderId, const MC_CODER_FIRMWARE *psProgram);
static void			FifoInit(UINT32 dTargetFifo);
static void			FifoReset(UINT32 dTargetFifo);
static void			EFifoStart(UINT32 dCoderId);
static SINT32		OFifoStart(UINT32 dCoderId);
static void			FifoStop(void);
static void			DecOpen(UINT32 dCoderId);
static SINT32		DecStandby(UINT32 dCoderId);
static SINT32		DecStart(UINT32 dCoderId);
static SINT32		DecStop(UINT32 dCoderId, UINT8 bVerifyComp);
static SINT32		DecReset(UINT32 dCoderId);
static void			DecClose(UINT32 dCoderId);
static SINT32		SetFormat(UINT32 dCoderId, MC_CODER_PARAMS *psParam);
static SINT32		SetBitWidth(UINT32 dCoderId, MC_CODER_PARAMS *psParam);


/***************************************************************************
 *	GetDecInfo
 *
 *	Function:
 *			Get CDSP decoder/encoder management module's information.
 *	Arguments:
 *			ePlayerId	Coder ID
 *	Return:
 *			DEC_INFO*
 *
 ****************************************************************************/
static DEC_INFO*	GetDecInfo
(
	UINT32	dCoderId
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("GetDecInfo");
#endif

	if ( dCoderId == CODER_DEC )
	{
#if (MCDRV_DEBUG_LEVEL>=4)
		McDebugLog_FuncOut("GetDecInfo", NULL);
#endif
		return &gsDecInfo;
	}
	else
	{
#if (MCDRV_DEBUG_LEVEL>=4)
		McDebugLog_FuncOut("GetDecInfo", NULL);
#endif
		return &gsEncInfo;
	}
}

/****************************************************************************
 *	CommandWaitComplete
 *
 *	Function:
 *			It waits until the command transmission is completed.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			>= 0 		success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	CommandWaitComplete
(
	UINT32	dCoderId
)
{
	SINT32	sdRet;
	UINT8	bAddrSfr;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CommandWaitComplete");
#endif

	if ( CODER_DEC == dCoderId )
	{
		bAddrSfr = MCI_DEC_SFR0;
	}
	else
	{
		bAddrSfr = MCI_ENC_SFR0;
	}

	/* Polling */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CDSPBFLAG_SET | (((UINT32)bAddrSfr) << 8) | (UINT32)CDSP_CMD_HOST2OS_COMPLETION), 0 );

	sdRet = McDevIf_ExecutePacket();
	if ( MCDRV_SUCCESS > sdRet )
	{
		/* Time out */
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)bAddrSfr), CDSP_CMD_HOST2OS_COMPLETION );

		McDevIf_ExecutePacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CommandWaitComplete", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	CommandInitialize
 *
 *	Function:
 *			Initialize register of command sending and receiving.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			>= 0 		success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	CommandInitialize
(
	UINT32	dCoderId
)
{
	SINT32	sdRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CommandInitialize");
#endif

	sdRet = CommandWaitComplete( dCoderId );

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CommandInitialize", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	CommandWriteHost2Os
 *
 *	Function:
 *			Write command (Host -> OS).
 *	Arguments:
 *			dCoderId	Coder ID
 *			psParam		Command ID and parameter data
 *	Return:
 *			>= 0 		success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	CommandWriteHost2Os
(
	UINT32			dCoderId,
	MC_CODER_PARAMS	*psParam
)
{
	UINT8	bData	= 0;
	UINT32	dAddrSfr;
	UINT32	dAddrCtl;
	UINT32	dAddrGpr;
	SINT32	iCount;
	SINT32	i;
	SINT32	sdRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CommandWriteHost2Os");
#endif

	if ( CODER_DEC == dCoderId )
	{
		dAddrSfr = MCI_DEC_SFR0;
		dAddrCtl = MCI_DEC_CTL0;
		dAddrGpr = MCI_DEC_GPR0;
	}
	else
	{
		dAddrSfr = MCI_ENC_SFR0;
		dAddrCtl = MCI_ENC_CTL0;
		dAddrGpr = MCI_ENC_GPR0;
	}

	/* Polling */
	sdRet = CommandWaitComplete( dCoderId );
	if( MCDRV_SUCCESS == sdRet )
	{
		/* Write parameter */
		switch ( psParam->bCommandId )
		{
		case CDSP_CMD_HOST2OS_CMN_NONE:
		case CDSP_CMD_HOST2OS_CMN_RESET:
		case CDSP_CMD_HOST2OS_CMN_CLEAR:
		case CDSP_CMD_HOST2OS_CMN_STANDBY:
		case CDSP_CMD_HOST2OS_CMN_GET_PRG_VER:

		case CDSP_CMD_HOST2OS_SYS_GET_OS_VER:
		case CDSP_CMD_HOST2OS_SYS_VERIFY_STOP_COMP:
		case CDSP_CMD_HOST2OS_SYS_CLEAR_INPUT_DATA_END:
		case CDSP_CMD_HOST2OS_SYS_TERMINATE:
		case CDSP_CMD_HOST2OS_SYS_GET_INPUT_POS:
		case CDSP_CMD_HOST2OS_SYS_RESET_INPUT_POS:
		case CDSP_CMD_HOST2OS_SYS_HALT:
			iCount = 0;
			break;

		case CDSP_CMD_HOST2OS_SYS_INPUT_DATA_END:
		case CDSP_CMD_HOST2OS_SYS_SET_DUAL_MONO:
			iCount = 1;
			break;

		case CDSP_CMD_HOST2OS_SYS_SET_PRG_INFO:
			iCount = 11;
			break;

		case CDSP_CMD_HOST2OS_SYS_SET_FORMAT:
			iCount = 8;
			break;

		default:
			/* Program dependence command */
			iCount = CDSP_CMD_PARAM_ARGUMENT_NUM;
			break;
		}
		for ( i = iCount-1L; i >= 0L; i-- )
		{
			McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (dAddrCtl - i)), psParam->abParam[i] );
		}

		/* Write command */
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | dAddrSfr), psParam->bCommandId );

		McDevIf_ExecutePacket();

		/* Polling */
		sdRet = CommandWaitComplete( dCoderId );
		if( MCDRV_SUCCESS == sdRet )
		{
			/* Error check */
			McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | dAddrSfr), &bData );
			if ( 0xF0 <= bData )
			{
				switch( bData )
				{
				case 0xF1:
				case 0xF2:
				case 0xF3:
				case 0xF4:
					sdRet	= MCDRV_ERROR_ARGUMENT;
					break;
				case 0xF5:
					sdRet	= MCDRV_ERROR_STATE;
					break;
				default:
					sdRet	= MCDRV_ERROR;
					break;
				}
			}
		}
	}

	if( MCDRV_SUCCESS == sdRet )
	{
		/* Get result */
		iCount = (CDSP_CMD_PARAM_RESULT_00 + CDSP_CMD_PARAM_RESULT_NUM);
		for ( i = CDSP_CMD_PARAM_RESULT_00; i < iCount; i++ )
		{
			McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (dAddrGpr - i)), &bData );
			psParam->abParam[i] = bData;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CommandWriteHost2Os", &sdRet);
#endif

	return sdRet;
}

/***************************************************************************
 *	InitializeRegister
 *
 *	Function:
 *			Initialize CDSP registers.
 *	Arguments:
 *			None
 *	Return:
 *			None
 *
 ****************************************************************************/
static void	InitializeRegister
(
	void
)
{
	SINT32	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitializeRegister");
#endif

	/* CDSP SRST */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), MCB_CDSP_SRST );

	/* Disable interrupt */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_CDSP_IRQ_CTRL_REG), 0x00 );

	/* Clear interrupt flag */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_CDSP_IRQ_FLAG_REG), MCB_IRQFLAG_CDSP_ALL );

	/* Other registers */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), 0x00 );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_RST), 0x00 );

	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_PWM_DIGITAL_CDSP), 0x00 );

	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_MAR_H), 0x00 );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_MAR_L), 0x00 );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_SFR0), 0x00 );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_ENC_SFR0), 0x00 );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), 0x00 );
	for ( i = 0; i < (SINT32)CDSP_CMD_PARAM_NUM; i++ )
	{
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | ((UINT32)MCI_DEC_CTL15 + (UINT32)i)), 0x00 );
	}
	for ( i = 0; i < (SINT32)CDSP_CMD_PARAM_NUM; i++ )
	{
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | ((UINT32)MCI_ENC_CTL15 + (UINT32)i)), 0x00 );
	}
	McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitializeRegister", NULL);
#endif
}

/***************************************************************************
 *	InitializeOS
 *
 *	Function:
 *			Initialize CDSP OS.
 *	Arguments:
 *			None
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	InitializeOS
(
	void
)
{
	SINT32	sdRet;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitializeOS");
#endif

	/* Command register Initialize */
	sdRet = CommandInitialize( CODER_DEC );
	if( MCDRV_SUCCESS == sdRet )
	{
		sdRet = CommandInitialize( CODER_ENC );
		if( MCDRV_SUCCESS == sdRet )
		{
			/* READY Polling */
			McDevIf_AddPacket( (MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CDSPBFLAG_SET | (((UINT32)MCI_CDSP_POWER_MODE) << 8) | (UINT32)MCB_CDSP_SLEEP),  0 );
			sdRet = McDevIf_ExecutePacket();
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitializeOS", &sdRet);
#endif
	return sdRet;
}

/***************************************************************************
 *	DownloadOS
 *
 *	Function:
 *			Download CDSP OS.
 *	Arguments:
 *			pbFirmware	OS data
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	DownloadOS
(
	const UINT8	*pbFirmware
)
{
	UINT8		bData;
	UINT8		abMsel;
	UINT8		abAdrH;
	UINT8		abAdrL;
	UINT16		awOsSize;
	const UINT8	*apbOsProg;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DownloadOS");
#endif

	abMsel		= (UINT8)MCB_CDSP_MSEL_DATA;
	abAdrL		= pbFirmware[0];
	abAdrH		= pbFirmware[1];
	awOsSize	= MAKEWORD(pbFirmware[2], pbFirmware[3]);
	apbOsProg	= &pbFirmware[4];

	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), &bData );

	/* CDSP_SRST Set : CDSP OS stop */
	bData &= (UINT8)~MCB_CDSP_FMODE;
	bData |= MCB_CDSP_SRST;
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), bData );

	/* Program & Data Write */
	/* CDSP_MSEL Set */
	bData &= (UINT8)~MCB_CDSP_MSEL;
	bData |= abMsel;
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), bData );

	/* CDSP_MAR Set */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_MAR_H), abAdrH );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_MAR_L), abAdrL );

	McDevIf_ExecutePacket();

	/* FSQ_FIFO Write */
	McDevIf_AddPacketRepeat( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_FSQ_FIFO_REG), apbOsProg, ((UINT32)awOsSize * 2UL) );
	McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DownloadOS", NULL);
#endif
}

/****************************************************************************
 *	Initialize
 *
 *	Function:
 *			Initialize CDSP and SMW.
 *	Arguments:
 *			None
 *	Return:
 *			None
 *
 ****************************************************************************/
static void	Initialize
(
	void
)
{
	SINT32		i;
	SINT32		j;
	DEC_INFO	*psDecInfo;
	const UINT8	*pbFirmware;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("Initialize");
#endif

	if ( (CDSP_STATE_ACTIVE == gsDecInfo.dState)
	  || (CDSP_STATE_ACTIVE == gsEncInfo.dState) )
	{
	}
	else
	{
		/* Global Initialize */
		for( i = 0; i < 2L; i++ )
		{
			if ( i == 0L )
			{
				psDecInfo = &gsDecInfo;
			}
			else
			{
				psDecInfo = &gsEncInfo;
			}
			psDecInfo->dState							= CDSP_STATE_IDLE;

			psDecInfo->sProgInfo.wVendorId				= 0;
			psDecInfo->sProgInfo.wFunctionId			= 0;
			psDecInfo->sProgInfo.wProgType				= 0;
			psDecInfo->sProgInfo.wProgScramble			= 0;
			psDecInfo->sProgInfo.wDataScramble			= 0;
			psDecInfo->sProgInfo.wEntryAddress			= 0;
			psDecInfo->sProgInfo.wFuncTableLoadAddress	= 0;
			psDecInfo->sProgInfo.wFuncTableSize			= 0;
			psDecInfo->sProgInfo.wPatchTableLoadAddress	= 0;
			psDecInfo->sProgInfo.wPatchTableSize		= 0;
			psDecInfo->sProgInfo.wProgramLoadAddress	= 0;
			psDecInfo->sProgInfo.wProgramSize			= 0;
			psDecInfo->sProgInfo.wDataLoadAddress		= 0;
			psDecInfo->sProgInfo.wDataSize				= 0;
			psDecInfo->sProgInfo.wExtProgramLoadAddress	= 0;
			psDecInfo->sProgInfo.wExtProgramSize		= 0;
			psDecInfo->sProgInfo.wExtDataLoadAddress	= 0;
			psDecInfo->sProgInfo.wExtDataSize			= 0;
			psDecInfo->sProgInfo.wDramBeginAddress		= 0;
			psDecInfo->sProgInfo.wDramSize				= 0;
			psDecInfo->sProgInfo.wStackBeginAddress		= 0;
			psDecInfo->sProgInfo.wStackSize				= 0;
			psDecInfo->sProgInfo.wVramSel				= 0;
			psDecInfo->sProgInfo.wIromSel				= 0;

			psDecInfo->sFormat.bFs						= CODER_FMT_FS_48000;
			psDecInfo->sParams.bCommandId				= 0;
			for ( j = 0; j < 16L; j++ )
			{
				psDecInfo->sParams.abParam[j]			= 0;
			}

			if ( i == 0L )
			{
				psDecInfo->sBitWidth.bEfifoBit			= 16;
				psDecInfo->sBitWidth.bOfifoBit			= 16;
			}
			else
			{
				psDecInfo->sBitWidth.bEfifoBit			= 32;
				psDecInfo->sBitWidth.bOfifoBit			= 32;
			}
			psDecInfo->sFifoSyncCh.bEfifoCh				= 2;
			psDecInfo->sFifoSyncCh.bOfifoCh				= 2;

			psDecInfo->wOfifoIrqPnt						= 0;
		}

		InitializeRegister();

		/* CDSP OS Download */
		pbFirmware = gabCdspOs;

		DownloadOS( pbFirmware );
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("Initialize", NULL);
#endif
}

/****************************************************************************
 *	TerminateProgram
 *
 *	Function:
 *			Terminate program.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	TerminateProgram
(
	UINT32	dCoderId
)
{
	DEC_INFO		*psDecInfo;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("TerminateProgram");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_SYS_TERMINATE;
	CommandWriteHost2Os( dCoderId, &sParam );

	psDecInfo->sProgInfo.wVendorId = 0;
	psDecInfo->sProgInfo.wFunctionId = 0;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("TerminateProgram", NULL);
#endif
}

/****************************************************************************
 *	CheckProgram
 *
 *	Function:
 *			Check program.
 *	Arguments:
 *			dCoderId	Coder ID
 *			psProgram	Program information
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	CheckProgram
(
	UINT32					dCoderId,
	const MC_CODER_FIRMWARE	*psProgram
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	UINT16		wProgType;
	UINT32		dTotalSize;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("CheckProgram");
#endif
	psDecInfo = GetDecInfo( dCoderId );

	/* Size Check */
	if ( psProgram->dSize < (UINT32)(PRG_DESC_PROGRAM) )
	{
		sdRet	= MCDRV_ERROR_ARGUMENT;
	}
	else
	{
		dTotalSize	= (UINT32)PRG_DESC_PROGRAM;
		dTotalSize	+= (UINT32)MAKEWORD(psProgram->pbFirmware[PRG_DESC_PRG_SIZE], psProgram->pbFirmware[PRG_DESC_PRG_SIZE+1])*2;
		dTotalSize	+= (UINT32)MAKEWORD(psProgram->pbFirmware[PRG_DESC_DATA_SIZE], psProgram->pbFirmware[PRG_DESC_DATA_SIZE+1])*2;
		dTotalSize	+= (UINT32)MAKEWORD(psProgram->pbFirmware[PRG_DESC_FUNCTABLE_SIZE], psProgram->pbFirmware[PRG_DESC_FUNCTABLE_SIZE+1])*2;
		dTotalSize	+= (UINT32)MAKEWORD(psProgram->pbFirmware[PRG_DESC_PATCHTABLE_SIZE], psProgram->pbFirmware[PRG_DESC_PATCHTABLE_SIZE+1])*2;
		dTotalSize	+= (UINT32)MAKEWORD(psProgram->pbFirmware[PRG_DESC_EXTPRG_SIZE], psProgram->pbFirmware[PRG_DESC_EXTPRG_SIZE+1])*2;
		dTotalSize	+= (UINT32)MAKEWORD(psProgram->pbFirmware[PRG_DESC_EXTDATA_SIZE], psProgram->pbFirmware[PRG_DESC_EXTDATA_SIZE+1])*2;
		if ( (dTotalSize) != psProgram->dSize )
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			/* Program Type Check */
			wProgType = MAKEWORD(psProgram->pbFirmware[PRG_DESC_PRG_TYPE], psProgram->pbFirmware[PRG_DESC_PRG_TYPE+1]);
			if ( (dCoderId == CODER_DEC)
			&& (wProgType != PRG_PRM_TYPE_TASK0) )
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
			if ( (dCoderId == CODER_ENC)
			&& (wProgType != PRG_PRM_TYPE_TASK1) )
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}

			if(MAKEWORD(psProgram->pbFirmware[PRG_DESC_OFIFO_IRQ_PNT], psProgram->pbFirmware[PRG_DESC_OFIFO_IRQ_PNT+1]) > (FIFOSIZE_OFIFO/4))
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("CheckProgram", &sdRet);
#endif
	return sdRet;
}

/***************************************************************************
 *	WriteProgram
 *
 *	Function:
 *			Write CDSP program to FIFO.
 *	Arguments:
 *			psDataInfo	Program information
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	WriteProgram
(
	FSQ_DATA_INFO	*psDataInfo
)
{
	UINT8	bData;
	UINT16	wLoadAddr;
	UINT16	wWriteSize;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("WriteProgram");
#endif

	wWriteSize	= psDataInfo->wSize;
	wLoadAddr	= psDataInfo->wLoadAddr;

	/* CDSP_MSEL Set */
	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), &bData );
	if ( PRG_PRM_SCRMBL_DISABLE == psDataInfo->wScramble )
	{
		bData |= MCB_CDSP_FMODE;
	}
	else
	{
		bData &= (UINT8)~MCB_CDSP_FMODE;
	}
	bData &= (UINT8)~MCB_CDSP_MSEL;
	if ( MSEL_PROG == psDataInfo->bMsel )
	{
		bData |= MCB_CDSP_MSEL_PROG;
	}
	else
	{
		bData |= MCB_CDSP_MSEL_DATA;
	}
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), bData );
	McDevIf_ExecutePacket();

	/* CDSP_MAR Set */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_MAR_H), HIBYTE( wLoadAddr ) );
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_MAR_L), LOBYTE( wLoadAddr ) );
	McDevIf_ExecutePacket();

	McDevIf_AddPacketRepeat( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_A | (UINT32)MCI_FSQ_FIFO_REG), &psDataInfo->pbData[0], (UINT32)wWriteSize * 2UL );
	McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("WriteProgram", NULL);
#endif
}

/***************************************************************************
 *	DownloadProgram
 *
 *	Function:
 *			Download CDSP program.
 *	Arguments:
 *			pbFirmware	Program data
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	DownloadProgram
(
	const UINT8	*pbFirmware
)
{
	SINT32			sdRet;
	UINT8			bData;
	UINT32			dDataOffset;
	FSQ_DATA_INFO	asDataInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DownloadProgram");
#endif

	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), &bData );

	/* CDSP_SRST Set : CDSP OS stop */
	bData &= (UINT8)~MCB_CDSP_FMODE;
	bData |= MCB_CDSP_SRST;
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), bData );

	/* CDSP_SAVEOFF Set */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_PWM_DIGITAL_CDSP), MCB_PWM_CDSP_SAVEOFF );
	McDevIf_ExecutePacket();

	/* CDSP_HALT_MODE Check */
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CDSPBFLAG_RESET | (((UINT32)MCI_CDSP_POWER_MODE) << 8) | (UINT32)MCB_CDSP_HLT_MODE_SLEEP_HALT), 0 );

	sdRet = McDevIf_ExecutePacket();
	if ( sdRet < MCDRV_SUCCESS )
	{
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_PWM_DIGITAL_CDSP), 0x00 );
		McDevIf_ExecutePacket();
	}
	else
	{
		/*	IMEM_NUM	*/
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_POWER_MODE), (UINT8)((pbFirmware[PRG_DESC_VRAMSEL]<<1)&MCB_IMEM_NUM) );
		McDevIf_ExecutePacket();

		/* Transfer Program & Data */
		dDataOffset	= PRG_DESC_PROGRAM;
		asDataInfo.pbData		= &pbFirmware[dDataOffset];
		asDataInfo.wSize		= MAKEWORD(pbFirmware[PRG_DESC_FUNCTABLE_SIZE], pbFirmware[PRG_DESC_FUNCTABLE_SIZE+1]);
		asDataInfo.wLoadAddr	= MAKEWORD(pbFirmware[PRG_DESC_FUNCTABLE_LOAD_ADR], pbFirmware[PRG_DESC_FUNCTABLE_LOAD_ADR+1]);
		asDataInfo.wScramble	= MAKEWORD(pbFirmware[PRG_DESC_DATA_SCRAMBLE], pbFirmware[PRG_DESC_DATA_SCRAMBLE+1]);
		asDataInfo.bMsel		= MSEL_DATA;
		if(asDataInfo.wSize > 0)
		{
			WriteProgram( &asDataInfo );
			dDataOffset	+= (asDataInfo.wSize*2);
		}

		asDataInfo.pbData		= &pbFirmware[dDataOffset];
		asDataInfo.wSize		= MAKEWORD(pbFirmware[PRG_DESC_PATCHTABLE_SIZE], pbFirmware[PRG_DESC_PATCHTABLE_SIZE+1]);
		asDataInfo.wLoadAddr	= MAKEWORD(pbFirmware[PRG_DESC_PATCHTABLE_LOAD_ADR], pbFirmware[PRG_DESC_PATCHTABLE_LOAD_ADR+1]);
		asDataInfo.wScramble	= MAKEWORD(pbFirmware[PRG_DESC_DATA_SCRAMBLE], pbFirmware[PRG_DESC_DATA_SCRAMBLE+1]);
		asDataInfo.bMsel		= MSEL_DATA;
		if(asDataInfo.wSize > 0)
		{
			WriteProgram( &asDataInfo );
			dDataOffset	+= (asDataInfo.wSize*2);
		}

		asDataInfo.pbData		= &pbFirmware[dDataOffset];
		asDataInfo.wSize		= MAKEWORD(pbFirmware[PRG_DESC_PRG_SIZE], pbFirmware[PRG_DESC_PRG_SIZE+1]);
		asDataInfo.wLoadAddr	= MAKEWORD(pbFirmware[PRG_DESC_PRG_LOAD_ADR], pbFirmware[PRG_DESC_PRG_LOAD_ADR+1]);
		asDataInfo.wScramble	= MAKEWORD(pbFirmware[PRG_DESC_PRG_SCRAMBLE], pbFirmware[PRG_DESC_PRG_SCRAMBLE+1]);
		asDataInfo.bMsel		= MSEL_PROG;
		if(asDataInfo.wSize > 0)
		{
			WriteProgram( &asDataInfo );
			dDataOffset	+= (asDataInfo.wSize*2);
		}

		asDataInfo.pbData		= &pbFirmware[dDataOffset];
		asDataInfo.wSize		= MAKEWORD(pbFirmware[PRG_DESC_DATA_SIZE], pbFirmware[PRG_DESC_DATA_SIZE+1]);
		asDataInfo.wLoadAddr	= MAKEWORD(pbFirmware[PRG_DESC_DATA_LOAD_ADR], pbFirmware[PRG_DESC_DATA_LOAD_ADR+1]);
		asDataInfo.wScramble	= MAKEWORD(pbFirmware[PRG_DESC_DATA_SCRAMBLE], pbFirmware[PRG_DESC_DATA_SCRAMBLE+1]);
		asDataInfo.bMsel		= MSEL_DATA;
		if(asDataInfo.wSize > 0)
		{
			WriteProgram( &asDataInfo );
			dDataOffset	+= (asDataInfo.wSize*2);
		}

		asDataInfo.pbData		= &pbFirmware[dDataOffset];
		asDataInfo.wSize		= MAKEWORD(pbFirmware[PRG_DESC_EXTPRG_SIZE], pbFirmware[PRG_DESC_EXTPRG_SIZE+1]);
		asDataInfo.wLoadAddr	= MAKEWORD(pbFirmware[PRG_DESC_EXTPRG_LOAD_ADR], pbFirmware[PRG_DESC_EXTPRG_LOAD_ADR+1]);
		asDataInfo.wScramble	= MAKEWORD(pbFirmware[PRG_DESC_PRG_SCRAMBLE], pbFirmware[PRG_DESC_PRG_SCRAMBLE+1]);
		asDataInfo.bMsel		= MSEL_PROG;
		if(asDataInfo.wSize > 0)
		{
			WriteProgram( &asDataInfo );
			dDataOffset	+= (asDataInfo.wSize*2);
		}

		asDataInfo.pbData		= &pbFirmware[dDataOffset];
		asDataInfo.wSize		= MAKEWORD(pbFirmware[PRG_DESC_EXTDATA_SIZE], pbFirmware[PRG_DESC_EXTDATA_SIZE+1]);
		asDataInfo.wLoadAddr	= MAKEWORD(pbFirmware[PRG_DESC_EXTDATA_LOAD_ADR], pbFirmware[PRG_DESC_EXTDATA_LOAD_ADR+1]);
		asDataInfo.wScramble	= MAKEWORD(pbFirmware[PRG_DESC_DATA_SCRAMBLE], pbFirmware[PRG_DESC_DATA_SCRAMBLE+1]);
		asDataInfo.bMsel		= (UINT8)MSEL_DATA;
		if(asDataInfo.wSize > 0)
		{
			WriteProgram( &asDataInfo );
			dDataOffset	+= (asDataInfo.wSize*2);
		}

		/* CDSP_SAVEOFF Clear */
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_PWM_DIGITAL_CDSP), 0x00 );
		McDevIf_ExecutePacket();

		/* CDSP_SRST Release : CDSP OS start */
		bData &= (UINT8)~MCB_CDSP_SRST;
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), bData );

		/* 100 ns wait */
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_TIMWAIT | 1UL), 0x00 );
		McDevIf_ExecutePacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DownloadProgram", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	InitializeProgram
 *
 *	Function:
 *			Initialize program.
 *	Arguments:
 *			dCoderId	Coder ID
 *			psProgram	Program information
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	InitializeProgram
(
	UINT32					dCoderId,
	const MC_CODER_FIRMWARE	*psProgram
)
{
	SINT32			sdRet;
	const UINT8		*pbFirmware;
	DEC_INFO		*psDecInfo;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("InitializeProgram");
#endif

	sdRet	= InitializeOS();
	if(sdRet == MCDRV_SUCCESS)
	{
		pbFirmware = psProgram->pbFirmware;

		/* SetProgramInfo command */
		sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_SYS_SET_PRG_INFO;
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_00] = pbFirmware[PRG_DESC_DRAMBEGIN_ADR];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_01] = pbFirmware[PRG_DESC_DRAMBEGIN_ADR+1];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_02] = pbFirmware[PRG_DESC_DRAM_SIZE];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_03] = pbFirmware[PRG_DESC_DRAM_SIZE+1];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_04] = pbFirmware[PRG_DESC_ENTRY_ADR];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_05] = pbFirmware[PRG_DESC_ENTRY_ADR+1];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_06] = pbFirmware[PRG_DESC_STACK_BEGIN_ADR];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_07] = pbFirmware[PRG_DESC_STACK_BEGIN_ADR+1];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_08] = pbFirmware[PRG_DESC_STACK_SIZE];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_09] = pbFirmware[PRG_DESC_STACK_SIZE+1];
		sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_10] = pbFirmware[PRG_DESC_IROMSEL];
		sdRet = CommandWriteHost2Os( dCoderId, &sParam );
		if( MCDRV_SUCCESS == sdRet )
		{
			/* Reset command */
		#if 0
			/* GetProgramVersion command */
			sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_CMN_GET_PRG_VER;
			sdRet = CommandWriteHost2Os( dCoderId, &sParam );
			if( MCDRV_SUCCESS == sdRet )
		#endif
			{
				psDecInfo = GetDecInfo( dCoderId );
				psDecInfo->sProgInfo.wVendorId				= MAKEWORD(pbFirmware[PRG_DESC_VENDOR_ID],				pbFirmware[PRG_DESC_VENDOR_ID+1]);
				psDecInfo->sProgInfo.wFunctionId			= MAKEWORD(pbFirmware[PRG_DESC_FUNCTION_ID],			pbFirmware[PRG_DESC_FUNCTION_ID+1]);
				psDecInfo->sProgInfo.wProgType				= MAKEWORD(pbFirmware[PRG_DESC_PRG_TYPE],				pbFirmware[PRG_DESC_PRG_TYPE+1]);
				psDecInfo->sProgInfo.wProgScramble			= MAKEWORD(pbFirmware[PRG_DESC_PRG_SCRAMBLE],			pbFirmware[PRG_DESC_PRG_SCRAMBLE+1]);
				psDecInfo->sProgInfo.wDataScramble			= MAKEWORD(pbFirmware[PRG_DESC_DATA_SCRAMBLE],			pbFirmware[PRG_DESC_DATA_SCRAMBLE+1]);
				psDecInfo->sProgInfo.wEntryAddress			= MAKEWORD(pbFirmware[PRG_DESC_ENTRY_ADR],				pbFirmware[PRG_DESC_ENTRY_ADR+1]);
				psDecInfo->sProgInfo.wFuncTableLoadAddress	= MAKEWORD(pbFirmware[PRG_DESC_FUNCTABLE_LOAD_ADR],		pbFirmware[PRG_DESC_FUNCTABLE_LOAD_ADR+1]);
				psDecInfo->sProgInfo.wFuncTableSize			= MAKEWORD(pbFirmware[PRG_DESC_FUNCTABLE_SIZE],			pbFirmware[PRG_DESC_FUNCTABLE_SIZE+1]);
				psDecInfo->sProgInfo.wPatchTableLoadAddress	= MAKEWORD(pbFirmware[PRG_DESC_PATCHTABLE_LOAD_ADR],	pbFirmware[PRG_DESC_PATCHTABLE_LOAD_ADR+1]);
				psDecInfo->sProgInfo.wPatchTableSize		= MAKEWORD(pbFirmware[PRG_DESC_PATCHTABLE_SIZE],		pbFirmware[PRG_DESC_PATCHTABLE_SIZE+1]);
				psDecInfo->sProgInfo.wProgramLoadAddress	= MAKEWORD(pbFirmware[PRG_DESC_PRG_LOAD_ADR],			pbFirmware[PRG_DESC_PRG_LOAD_ADR+1]);
				psDecInfo->sProgInfo.wProgramSize			= MAKEWORD(pbFirmware[PRG_DESC_PRG_SIZE],				pbFirmware[PRG_DESC_PRG_SIZE+1]);
				psDecInfo->sProgInfo.wDataLoadAddress		= MAKEWORD(pbFirmware[PRG_DESC_DATA_LOAD_ADR],			pbFirmware[PRG_DESC_DATA_LOAD_ADR+1]);
				psDecInfo->sProgInfo.wDataSize				= MAKEWORD(pbFirmware[PRG_DESC_DATA_SIZE],				pbFirmware[PRG_DESC_DATA_SIZE+1]);
				psDecInfo->sProgInfo.wExtProgramLoadAddress	= MAKEWORD(pbFirmware[PRG_DESC_EXTPRG_LOAD_ADR],		pbFirmware[PRG_DESC_EXTPRG_LOAD_ADR+1]);
				psDecInfo->sProgInfo.wExtProgramSize		= MAKEWORD(pbFirmware[PRG_DESC_EXTPRG_SIZE],			pbFirmware[PRG_DESC_EXTPRG_SIZE+1]);
				psDecInfo->sProgInfo.wExtDataLoadAddress	= MAKEWORD(pbFirmware[PRG_DESC_EXTDATA_LOAD_ADR],		pbFirmware[PRG_DESC_EXTDATA_LOAD_ADR+1]);
				psDecInfo->sProgInfo.wExtDataSize			= MAKEWORD(pbFirmware[PRG_DESC_EXTDATA_SIZE],			pbFirmware[PRG_DESC_EXTDATA_SIZE+1]);
				psDecInfo->sProgInfo.wDramBeginAddress		= MAKEWORD(pbFirmware[PRG_DESC_DRAMBEGIN_ADR],			pbFirmware[PRG_DESC_DRAMBEGIN_ADR+1]);
				psDecInfo->sProgInfo.wDramSize				= MAKEWORD(pbFirmware[PRG_DESC_DRAM_SIZE],				pbFirmware[PRG_DESC_DRAM_SIZE+1]);
				psDecInfo->sProgInfo.wStackBeginAddress		= MAKEWORD(pbFirmware[PRG_DESC_STACK_BEGIN_ADR],		pbFirmware[PRG_DESC_STACK_BEGIN_ADR+1]);
				psDecInfo->sProgInfo.wStackSize				= MAKEWORD(pbFirmware[PRG_DESC_STACK_SIZE],				pbFirmware[PRG_DESC_STACK_SIZE+1]);
				psDecInfo->sProgInfo.wVramSel				= MAKEWORD(pbFirmware[PRG_DESC_VRAMSEL],				pbFirmware[PRG_DESC_VRAMSEL+1]);
				psDecInfo->sProgInfo.wIromSel				= MAKEWORD(pbFirmware[PRG_DESC_IROMSEL],				pbFirmware[PRG_DESC_IROMSEL+1]);
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("InitializeProgram", &sdRet);
#endif

	return sdRet;
}

/***************************************************************************
 *	FifoInit
 *
 *	Function:
 *			FIFO Init.
 *	Arguments:
 *			dTargetFifo	Target FIFO
 *	Return:
 *			None
 *
 ****************************************************************************/
static void	FifoInit
(
	UINT32	dTargetFifo
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("FifoInit");
#endif

	FifoReset( dTargetFifo );

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("FifoInit", NULL);
#endif
}

/***************************************************************************
 *	FifoReset
 *
 *	Function:
 *			Reset FIFO.
 *	Arguments:
 *			dTargetFifo	Target FIFO
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	FifoReset
(
	UINT32	dTargetFifo
)
{
	UINT8	bData;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("FifoReset");
#endif

	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_RST), &bData );

	if (FIFO_NONE != (FIFO_EFIFO_MASK & dTargetFifo))
	{
		/* EFIFO Reset */
		bData |= MCB_DEC_EFIFO_RST;
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_RST), bData );
		bData &= (UINT8)~MCB_DEC_EFIFO_RST;
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_RST), bData );
	}

	if (FIFO_NONE != (FIFO_OFIFO_MASK & dTargetFifo))
	{
		/* OFIFO Reset */
		bData |= MCB_DEC_OFIFO_RST;
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_RST), bData );
		bData &= (UINT8)~MCB_DEC_OFIFO_RST;
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_RST), bData );
	}
	McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("FifoReset", NULL);
#endif
}


/***************************************************************************
 *	EFifoStart
 *
 *	Function:
 *			Start EFIFO.
 *	Arguments:
 *			dFifoId		FIFO ID
 *	Return:
 *			None
 *
 ****************************************************************************/
static void	EFifoStart
(
	UINT32	dCoderId
)
{
	UINT8		bData;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("EFifoStart");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), &bData );
	bData &= (UINT8)~(MCB_DEC_EFIFO_CH|MCB_DEC_EFIFO_SYNC);

	if ( 4 == psDecInfo->sFifoSyncCh.bEfifoCh )
	{
		psDecInfo->sBitWidth.bEfifoBit = 16;
		bData |= MCB_DEC_EFIFO_CH_4_16;
	}
	else if ( psDecInfo->sBitWidth.bEfifoBit == 32 )
	{
		bData |= MCB_DEC_EFIFO_CH_2_32;
	}
	else
	{
		bData |= MCB_DEC_EFIFO_CH_2_16;
	}
	bData |= (UINT8)((psDecInfo->sFifoSyncCh.bEfifoSync << 6) & MCB_DEC_EFIFO_SYNC);

	bData &= (UINT8)~MCB_DEC_OFIFO_CH;
	if ( psDecInfo->sBitWidth.bOfifoBit == 32 )
	{
		bData |= MCB_DEC_OFIFO_CH_2_32;
	}
	else
	{
		bData |= MCB_DEC_OFIFO_CH_2_16;
	}

	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), bData );
	McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("EFifoStart", NULL);
#endif
}

/***************************************************************************
 *	OFifoStart
 *
 *	Function:
 *			Start OFIFO.
 *	Arguments:
 *			dFifoId		Coder ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	OFifoStart
(
	UINT32	dCoderId
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	UINT8		bData;
	UINT8		bData2;
	UINT8		bIrqPntH, bIrqPntL;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("OFifoStart");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), &bData );
	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), &bData2 );

	bData &= (UINT8)~MCB_DEC_OFIFO_SYNC;
	bData |= (UINT8)((psDecInfo->sFifoSyncCh.bOfifoSync << 2) & MCB_DEC_OFIFO_SYNC);
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), bData );

	/* OFIFO_IRQ_PNT Set */
	if(psDecInfo->wOfifoIrqPnt > 0)
	{
		bIrqPntH	= (UINT8)(psDecInfo->wOfifoIrqPnt >> 8) & MCB_OFIFO_IRQ_PNT_H;
		bIrqPntL	= (UINT8)(psDecInfo->wOfifoIrqPnt & MCB_OFIFO_IRQ_PNT_L);
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_OFIFO_IRQ_PNT_H), bIrqPntH );
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_OFIFO_IRQ_PNT_L), bIrqPntL );

		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_EVTWAIT | MCDRV_EVT_CDSPBFLAG_SET | (((UINT32)MCI_OFIFO_FLG) << 8) | (UINT32)MCB_OFIFO_FLG_JOPNT), 0 );
	}

	bData2 |= MCB_DEC_OUT_START;
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), bData2 );

	sdRet	= McDevIf_ExecutePacket();

#if 0
	gsFifoInfo.bOutStart = OUTPUT_START_ON;
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("OFifoStart", &sdRet);
#endif
	return sdRet;
}

/***************************************************************************
 *	FifoStop
 *
 *	Function:
 *			Stop FIFO.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	FifoStop
(
	void
)
{
	UINT8	bData;
	UINT8	bData2;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("FifoStop");
#endif

	/* OUT Stop */
	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), &bData );
	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), &bData2 );

	bData &= (UINT8)~MCB_DEC_OUT_START;
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), bData );

	bData2 &= (UINT8)~(MCB_DEC_OFIFO_SYNC|MCB_DEC_EFIFO_SYNC);
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), bData2 );
	McDevIf_ExecutePacket();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("FifoStop", NULL);
#endif
}

/****************************************************************************
 *	DecOpen
 *
 *	Function:
 *			Secure resource.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	DecOpen
(
	UINT32	dCoderId
)
{
	SINT32		i;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DecOpen");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	/* Command register Initialize */
	CommandInitialize( dCoderId );

	/* Initialize */
	psDecInfo->sFormat.bFs				= CODER_FMT_FS_48000;
	psDecInfo->sParams.bCommandId		= 0;
	for ( i = 0; i < (SINT32)CDSP_CMD_PARAM_ARGUMENT_NUM; i++ )
	{
		psDecInfo->sParams.abParam[i]	= 0;
	}
	if ( CODER_DEC == dCoderId )
	{
		psDecInfo->sBitWidth.bEfifoBit	= 16;
		psDecInfo->sBitWidth.bOfifoBit	= 16;
	}
	else
	{
		psDecInfo->sBitWidth.bEfifoBit	= 32;
		psDecInfo->sBitWidth.bOfifoBit	= 32;
	}
	psDecInfo->sFifoSyncCh.bEfifoCh		= 2;
	psDecInfo->sFifoSyncCh.bOfifoCh		= 2;

	FifoInit( (FIFO_EFIFO | FIFO_OFIFO) );

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DecOpen", NULL);
#endif
}

/****************************************************************************
 *	DecStandby
 *
 *	Function:
 *			Standby decoder/encoder process.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	DecStandby
(
	UINT32	dCoderId
)
{
	SINT32			sdRet;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DecStandby");
#endif

	/* Standby command */
	sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_CMN_STANDBY;
	sdRet = CommandWriteHost2Os( dCoderId, &sParam );
	if( MCDRV_SUCCESS == sdRet )
	{
#if 0
		gsFifoInfo.bOutStart = OUTPUT_START_OFF;
#endif
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DecStandby", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	DecStart
 *
 *	Function:
 *			Start decoder/encoder.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	DecStart
(
	UINT32	dCoderId
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	UINT8		bData;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DecStart");
#endif

	EFifoStart( dCoderId );

	/* DEC/ENC Start */
	McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), &bData );
	if ( CODER_DEC == dCoderId )
	{
		bData |= MCB_DEC_DEC_START;
	}
	else
	{
		bData |= MCB_DEC_ENC_START;
	}
	McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), bData );
	McDevIf_ExecutePacket();

#if 0
	if ( gsFifoInfo.dOFifoBufSample == 0 )
	{
		InterruptProcOFifoCore();
	}
#else
	sdRet	= OFifoStart(dCoderId);
#endif

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DecStart", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	DecStop
 *
 *	Function:
 *			Stop decoder/encoder.
 *	Arguments:
 *			dCoderId	Coder ID
 *			bVerifyComp	Verify Stop Complete command flag
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	DecStop
(
	UINT32	dCoderId,
	UINT8	bVerifyComp
)
{
	UINT8			bData;
	SINT32			sdRet	= MCDRV_SUCCESS;
	DEC_INFO		*psDecInfo;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DecStop");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	/* State check */
	if ( CDSP_STATE_ACTIVE != psDecInfo->dState )
	{
		sdRet	= MCDRV_ERROR_STATE;
	}
	else
	{
		/* DEC/ENC Stop */
		McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), &bData );
		if ( CODER_DEC == dCoderId )
		{
			bData &= (UINT8)~MCB_DEC_DEC_START;
		}
		else
		{
			bData &= (UINT8)~MCB_DEC_ENC_START;
		}
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_START), bData );

		McDevIf_ExecutePacket();

		/* VerifyStopCompletion command */
		if ( MADEVCDSP_VERIFY_COMP_ON == bVerifyComp )
		{
			sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_SYS_VERIFY_STOP_COMP;
			sdRet = CommandWriteHost2Os( dCoderId, &sParam );
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DecStop", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	DecReset
 *
 *	Function:
 *			Reset decoder/encoder.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	DecReset
(
	UINT32	dCoderId
)
{
	SINT32			sdRet;
	DEC_INFO		*psDecInfo;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DecReset");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	/* Reset command */
	sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_CMN_RESET;
	sdRet = CommandWriteHost2Os( dCoderId, &sParam );
	if( MCDRV_SUCCESS == sdRet )
	{
		psDecInfo->dState	= CDSP_STATE_REGISTERED;

		/* Command register Initialize */
		CommandInitialize( dCoderId );

		sParam.bCommandId	= CDSP_CMD_HOST2OS_SYS_SET_FORMAT;
		sParam.abParam[0]	= CODER_FMT_FS_48000;
		sdRet				= SetFormat( dCoderId, &sParam);
		if(sdRet == MCDRV_SUCCESS)
		{
			psDecInfo->sFormat.bFs	= CODER_FMT_FS_48000;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DecReset", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	DecClose
 *
 *	Function:
 *			Open resource.
 *	Arguments:
 *			dCoderId	Coder ID
 *	Return:
 *			none
 *
 ****************************************************************************/
static void	DecClose
(
	UINT32	dCoderId
)
{
	DEC_INFO		*psDecInfo;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("DecClose");
#endif

	psDecInfo = GetDecInfo( dCoderId );


	/* Reset command */
	sParam.bCommandId = (UINT8)CDSP_CMD_HOST2OS_CMN_RESET;
	CommandWriteHost2Os( dCoderId, &sParam );

	/* Command register Initialize */
	CommandInitialize( dCoderId );

	psDecInfo->sFormat.bFs = CODER_FMT_FS_48000;


#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("DecClose", NULL);
#endif
}

/****************************************************************************
 *	SetFormat
 *
 *	Function:
 *			Set format.
 *	Arguments:
 *			dCoderId	Coder ID
 *			psParam		Pointer of send parameter.
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	SetFormat
(
	UINT32			dCoderId,
	MC_CODER_PARAMS	*psParam
)
{
	SINT32			sdRet	= MCDRV_SUCCESS;
	DEC_INFO		*psDecInfo;
	MC_CODER_PARAMS	sParam;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetFormat");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	/* State check */
	switch( psDecInfo->dState )
	{
	case CDSP_STATE_REGISTERED:
		break;
	default:
		sdRet	= MCDRV_ERROR_STATE;
		break;
	}
	if ( MCDRV_SUCCESS == sdRet )
	{
		/* Argument check */
		switch( psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00] )
		{
		case CODER_FMT_FS_48000:
		case CODER_FMT_FS_44100:
		case CODER_FMT_FS_32000:
		case CODER_FMT_FS_24000:
		case CODER_FMT_FS_22050:
		case CODER_FMT_FS_16000:
		case CODER_FMT_FS_12000:
		case CODER_FMT_FS_11025:
		case CODER_FMT_FS_8000:
			break;
		default:
			sdRet	= MCDRV_ERROR_ARGUMENT;
			break;
		}

		if ( MCDRV_SUCCESS == sdRet )
		{
			sParam.bCommandId = psParam->bCommandId;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_00] = psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00];
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_01] = (OUT_DEST_OFIFO<<4) | IN_SOURCE_EFIFO;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_02] = 0;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_03] = 0;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_04] = 0;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_05] = 0;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_06] = 0;
			sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_07] = 0;
			sdRet = CommandWriteHost2Os( dCoderId, &sParam );
			if ( MCDRV_SUCCESS == sdRet )
			{
				psDecInfo->sFormat.bFs = sParam.abParam[CDSP_CMD_PARAM_ARGUMENT_00];
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetFormat", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	SetFifoChannels
 *
 *	Function:
 *			
 *	Arguments:
 *			dCoderId	Coder ID
 *			psParam		Pointer of send parameter.
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	SetFifoChannels
(
	UINT32			dCoderId,
	MC_CODER_PARAMS	*psParam
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;
	UINT8		bData;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetFifoChannels");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	/* State check */
	switch( psDecInfo->dState )
	{
	case CDSP_STATE_REGISTERED:
	case CDSP_STATE_READY:
	case CDSP_STATE_ACTIVE:
		break;
	default:
		sdRet	= MCDRV_ERROR_STATE;
		break;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		switch( psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00] )
		{
		case 2:
			break;
		case 4:
			if(psDecInfo->sBitWidth.bEfifoBit == 32)
			{
				sdRet	= MCDRV_ERROR_ARGUMENT;
			}
			break;
		default:
			sdRet	= MCDRV_ERROR_ARGUMENT;
			break;
		}

		switch( psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_01] )
		{
		case 2:
			break;
		default:
			sdRet	= MCDRV_ERROR_ARGUMENT;
			break;
		}

		if(sdRet == MCDRV_SUCCESS)
		{
			psDecInfo->sFifoSyncCh.bEfifoCh = psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00];
			psDecInfo->sFifoSyncCh.bOfifoCh = psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_01];
		}
	}

	if(psDecInfo->dState == CDSP_STATE_ACTIVE)
	{
		McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), &bData );
		bData &= (UINT8)~MCB_DEC_EFIFO_CH;

		if ( 4 == psDecInfo->sFifoSyncCh.bEfifoCh )
		{
			psDecInfo->sBitWidth.bEfifoBit = 16;
			bData |= MCB_DEC_EFIFO_CH_4_16;
		}
		else if ( psDecInfo->sBitWidth.bEfifoBit == 32 )
		{
			bData |= MCB_DEC_EFIFO_CH_2_32;
		}
		else
		{
			bData |= MCB_DEC_EFIFO_CH_2_16;
		}
		bData |= (UINT8)((psDecInfo->sFifoSyncCh.bEfifoSync << 6) & MCB_DEC_EFIFO_SYNC);
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_DEC_FIFO_SYNC_CH), bData );
		McDevIf_ExecutePacket();
		FifoReset(FIFO_EFIFO);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetFifoChannels", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	SetBitWidth
 *
 *	Function:
 *			bit width setting
 *	Arguments:
 *			dCoderId	Coder ID
 *			psParam		Pointer of send parameter.
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
static SINT32	SetBitWidth
(
	UINT32			dCoderId,
	MC_CODER_PARAMS	*psParam
)
{
	SINT32		sdRet	= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("SetBitWidth");
#endif

	psDecInfo = GetDecInfo( dCoderId );

	/* State check */
	switch( psDecInfo->dState )
	{
	case CDSP_STATE_REGISTERED:
		break;
	default:
		sdRet	= MCDRV_ERROR_STATE;
		break;
	}

	switch( psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00] )
	{
	case 16:
		break;
	case 32:
		if ( 2 != psDecInfo->sFifoSyncCh.bEfifoCh )
		{
			sdRet	= MCDRV_ERROR;
		}
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}

	switch( psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_01] )
	{
	case 16:
	case 32:
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		psDecInfo->sBitWidth.bEfifoBit = psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00];
		psDecInfo->sBitWidth.bOfifoBit = psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_01];
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("SetBitWidth", &sdRet);
#endif
	return sdRet;
}





/***************************************************************************
 *	McCdsp_Init
 *
 *	Function:
 *			Initialize CDSP.
 *	Arguments:
 *			None
 *	Return:
 *			None
 *
 ****************************************************************************/
void	McCdsp_Init
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_Init");
#endif

	Initialize();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_Init", NULL);
#endif
}

/***************************************************************************
 *	McCdsp_Term
 *
 *	Function:
 *			Terminate CDSP.
 *	Arguments:
 *			None
 *	Return:
 *			None
 *
 ****************************************************************************/
void	McCdsp_Term
(
	void
)
{
	UINT8	bData;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_Term");
#endif

	/* CDSP stop */
	if ( (CDSP_STATE_NOTINIT != gsDecInfo.dState)
	  || (CDSP_STATE_NOTINIT != gsEncInfo.dState) )
	{
		McDevIf_ReadDirect( (MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), &bData );
		bData |= MCB_CDSP_SRST;
		McDevIf_AddPacket( (MCDRV_PACKET_TYPE_FORCE_WRITE | MCDRV_PACKET_REGTYPE_B_CDSP | (UINT32)MCI_CDSP_RESET), bData );

		McDevIf_ExecutePacket();
	}

	gsDecInfo.dState = CDSP_STATE_NOTINIT;
	gsEncInfo.dState = CDSP_STATE_NOTINIT;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_Term", NULL);
#endif
}

/****************************************************************************
 *	McCdsp_TransferProgram
 *
 *	Function:
 *			Forward and initialize CDSP program.
 *	Arguments:
 *			ePlayerId	Player ID
 *			psProgram	Pointer of program data.
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
SINT32	McCdsp_TransferProgram
(
	MC_PLAYER_ID		ePlayerId,
	MC_CODER_FIRMWARE	*psProgram
)
{
	SINT32		sdRet		= MCDRV_SUCCESS;
	UINT8		bDownLoad	= PROGRAM_NO_DOWNLOAD;
	UINT32		dCoderId	= CODER_DEC;
	UINT16		wFuncId;
	UINT16		wVendorId;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_TransferProgram");
#endif

	/* arg check */
	switch( ePlayerId )
	{
	case eMC_PLAYER_CODER_A:
		dCoderId = CODER_DEC;
		break;
	case eMC_PLAYER_CODER_B:
		dCoderId = CODER_ENC;
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		if ( NULL == psProgram )
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			psDecInfo = GetDecInfo( dCoderId );

			if ( NULL == psProgram->pbFirmware )
			{
				/* State check */
				switch( psDecInfo->dState )
				{
				case CDSP_STATE_NOTINIT:
				case CDSP_STATE_IDLE:
					sdRet	= MCDRV_SUCCESS;
					break;

				case CDSP_STATE_ACTIVE:
					sdRet	= McCdsp_Stop(ePlayerId);
					if(sdRet != MCDRV_SUCCESS)
					{
						break;
					}
				case CDSP_STATE_REGISTERED:
				case CDSP_STATE_READY:
					DecClose( dCoderId );

					/* Terminate current program */
					TerminateProgram( dCoderId );
					psDecInfo->dState = CDSP_STATE_IDLE;
					break;

				default:
					sdRet	= MCDRV_ERROR_STATE;
					break;
				}
			}
			else
			{
				/* arg check */
				if ( 0UL == psProgram->dSize )
				{
					sdRet	= MCDRV_ERROR_ARGUMENT;
				}
				else
				{
					/* State check */
					switch( psDecInfo->dState )
					{
					case CDSP_STATE_IDLE:
						bDownLoad = PROGRAM_DOWNLOAD;
						break;

					case CDSP_STATE_ACTIVE:
					case CDSP_STATE_REGISTERED:
					case CDSP_STATE_READY:
						/* Check Program ID */
						wFuncId		= MAKEWORD(psProgram->pbFirmware[PRG_DESC_FUNCTION_ID], psProgram->pbFirmware[PRG_DESC_FUNCTION_ID+1]);
						wVendorId	= MAKEWORD(psProgram->pbFirmware[PRG_DESC_VENDOR_ID], psProgram->pbFirmware[PRG_DESC_VENDOR_ID+1]);
						if ( (wFuncId != psDecInfo->sProgInfo.wFunctionId) || (wVendorId != psDecInfo->sProgInfo.wVendorId) )
						{
							bDownLoad = PROGRAM_DOWNLOAD;
							if(psDecInfo->dState == CDSP_STATE_ACTIVE)
							{
								sdRet	= McCdsp_Stop(ePlayerId);
							}
						}
						else
						{
							bDownLoad = PROGRAM_NO_DOWNLOAD;
						}
						break;

					default:
						sdRet	= MCDRV_ERROR_STATE;
						break;
					}

					if(sdRet == MCDRV_SUCCESS)
					{
						if ( PROGRAM_DOWNLOAD == bDownLoad )
						{
							/* Check Program */
							sdRet = CheckProgram( dCoderId, psProgram );
							if(sdRet == MCDRV_SUCCESS)
							{
								switch( psDecInfo->dState )
								{
								case CDSP_STATE_REGISTERED:
								case CDSP_STATE_READY:
									DecClose( dCoderId );

									/* Terminate current program */
									TerminateProgram( dCoderId );

									psDecInfo->dState = CDSP_STATE_IDLE;
									break;
								default:
									break;
								}

								/* Download */
								sdRet = DownloadProgram( psProgram->pbFirmware );
							}
							if(sdRet == MCDRV_SUCCESS)
							{
								/* Initialize */
								sdRet = InitializeProgram( dCoderId, psProgram );
							}
							if(sdRet == MCDRV_SUCCESS)
							{
								psDecInfo->wOfifoIrqPnt	= MAKEWORD(psProgram->pbFirmware[PRG_DESC_OFIFO_IRQ_PNT], psProgram->pbFirmware[PRG_DESC_OFIFO_IRQ_PNT+1]);
								DecOpen( dCoderId );
								sdRet	= DecReset( dCoderId );
							}
						}
					}
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_TransferProgram", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McCdsp_SetParam
 *
 *	Function:
 *			Send command.
 *	Arguments:
 *			ePlayerId	Player ID
 *			psParam		Pointer of structure that sends parameter.
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
SINT32	McCdsp_SetParam
(
	MC_PLAYER_ID	ePlayerId,
	MC_CODER_PARAMS	*psParam
)
{
	UINT32		dCoderId	= CODER_DEC;
	SINT32		sdRet		= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_SetParam");
#endif

	/* arg check */
	switch( ePlayerId )
	{
	case eMC_PLAYER_CODER_A:
		dCoderId = CODER_DEC;
		break;
	case eMC_PLAYER_CODER_B:
		dCoderId = CODER_ENC;
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if ( NULL == psParam )
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			psDecInfo = GetDecInfo( dCoderId );
			/* State check */
			switch( psDecInfo->dState )
			{
			case CDSP_STATE_REGISTERED:
			case CDSP_STATE_READY:
			case CDSP_STATE_ACTIVE:
				break;

			default:
				sdRet	= MCDRV_ERROR_STATE;
				break;
			}
			if(sdRet == MCDRV_SUCCESS)
			{
				/* Command check */
				switch( psParam->bCommandId )
				{
				case CDSP_CMD_HOST2OS_SYS_SET_FORMAT:
					sdRet = SetFormat( dCoderId, psParam );
					break;

				case CDSP_CMD_DRV_SYS_SET_FIFO_CHANNELS:
					if(dCoderId == CODER_ENC)
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
					else
					{
						sdRet	= SetFifoChannels( dCoderId, psParam );
					}
					break;

				case CDSP_CMD_DRV_SYS_SET_BIT_WIDTH:
					sdRet = SetBitWidth( dCoderId, psParam );
					break;

				default:
					if ( (psParam->bCommandId < CDSP_CMD_HOST2OS_PRG_MIN)
						|| (psParam->bCommandId > CDSP_CMD_HOST2OS_PRG_MAX) )
					{
						sdRet	= MCDRV_ERROR_ARGUMENT;
					}
					else
					{
						/* Program dependence command */
						sdRet = CommandWriteHost2Os( dCoderId, psParam );
					}
					break;
				}
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_SetParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McCdsp_GetParam
 *
 *	Function:
 *			Get param.
 *	Arguments:
 *			ePlayerId	Player ID
 *			psParam		Pointer to structure of parameter.
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
SINT32	McCdsp_GetParam
(
	MC_PLAYER_ID	ePlayerId,
	MC_CODER_PARAMS	*psParam
)
{
	UINT32		dCoderId	= CODER_DEC;
	SINT32		sdRet		= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_GetParam");
#endif

	/* arg check */
	switch( ePlayerId )
	{
	case eMC_PLAYER_CODER_A:
		dCoderId = CODER_DEC;
		break;
	case eMC_PLAYER_CODER_B:
		dCoderId = CODER_ENC;
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}
	if(sdRet == MCDRV_SUCCESS)
	{
		if ( NULL == psParam )
		{
			sdRet	= MCDRV_ERROR_ARGUMENT;
		}
		else
		{
			psDecInfo = GetDecInfo( dCoderId );
			/* State check */
			switch( psDecInfo->dState )
			{
			case CDSP_STATE_NOTINIT:
				sdRet	= MCDRV_ERROR_STATE;
				break;

			default:
				switch( psParam->bCommandId )
				{
				case CDSP_CMD_DRV_SYS_SET_FIFO_CHANNELS:
					psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00]	= psDecInfo->sFifoSyncCh.bEfifoCh;
					psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_01]	= psDecInfo->sFifoSyncCh.bOfifoCh;
					break;

				case CDSP_CMD_DRV_SYS_SET_BIT_WIDTH:
					psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_00]	= psDecInfo->sBitWidth.bEfifoBit;
					psParam->abParam[CDSP_CMD_PARAM_ARGUMENT_01]	= psDecInfo->sBitWidth.bOfifoBit;
					break;

				default:
					sdRet	= MCDRV_ERROR_ARGUMENT;
					break;
				}
				break;
			}
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_GetParam", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McCdsp_Start
 *
 *	Function:
 *			Begin main process.
 *	Arguments:
 *			ePlayerId	Player ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
SINT32	McCdsp_Start
(
	MC_PLAYER_ID	ePlayerId
)
{
	UINT32		dCoderId	= CODER_DEC;
	SINT32		sdRet		= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_Start");
#endif

	/* arg check */
	switch( ePlayerId )
	{
	case eMC_PLAYER_CODER_A:
		dCoderId = CODER_DEC;
		break;
	case eMC_PLAYER_CODER_B:
		dCoderId = CODER_ENC;
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		psDecInfo = GetDecInfo( dCoderId );

		switch( psDecInfo->dState )
		{
		case CDSP_STATE_REGISTERED:
			sdRet = DecStandby( dCoderId );
			if ( MCDRV_SUCCESS == sdRet )
			{
				FifoReset( (FIFO_EFIFO | FIFO_OFIFO) );
				psDecInfo->dState = CDSP_STATE_READY;
				sdRet	= DecStart( dCoderId );
				if(sdRet == MCDRV_SUCCESS)
				{
					psDecInfo->dState = CDSP_STATE_ACTIVE;
				}
			}
			break;

		case CDSP_STATE_READY:
			FifoReset( FIFO_EFIFO );
			sdRet	= DecStart( dCoderId );
			if(sdRet == MCDRV_SUCCESS)
			{
				psDecInfo->dState = CDSP_STATE_ACTIVE;
			}
			break;

		default:
			sdRet	= MCDRV_ERROR_STATE;
			break;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_Start", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McCdsp_Stop
 *
 *	Function:
 *			Stop main process.
 *	Arguments:
 *			ePlayerId	Player ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
SINT32	McCdsp_Stop
(
	MC_PLAYER_ID	ePlayerId
)
{
	UINT32		dCoderId	= CODER_DEC;
	SINT32		sdRet		= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_Stop");
#endif

	/* arg check */
	switch( ePlayerId )
	{
	case eMC_PLAYER_CODER_A:
		dCoderId = CODER_DEC;
		break;
	case eMC_PLAYER_CODER_B:
		dCoderId = CODER_ENC;
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		psDecInfo = GetDecInfo( dCoderId );

		/* State check */
		switch( psDecInfo->dState )
		{
		case CDSP_STATE_ACTIVE:
			sdRet = DecStop( dCoderId, MADEVCDSP_VERIFY_COMP_ON );
			if(sdRet == MCDRV_SUCCESS)
			{
				FifoStop();
				psDecInfo->dState	= CDSP_STATE_READY;
			}
			break;

		default:
			sdRet	= MCDRV_ERROR_STATE;
			break;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_Stop", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McCdsp_Reset
 *
 *	Function:
 *			Reset main process.
 *	Arguments:
 *			ePlayerId	Player ID
 *	Return:
 *			0			success
 *			< 0			error code
 *
 ****************************************************************************/
SINT32	McCdsp_Reset
(
	MC_PLAYER_ID	ePlayerId
)
{
	UINT32		dCoderId	= CODER_DEC;
	SINT32		sdRet		= MCDRV_SUCCESS;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McCdsp_Reset");
#endif

	/* arg check */
	switch( ePlayerId )
	{
	case eMC_PLAYER_CODER_A:
		dCoderId = CODER_DEC;
		break;
	case eMC_PLAYER_CODER_B:
		dCoderId = CODER_ENC;
		break;
	default:
		sdRet	= MCDRV_ERROR_ARGUMENT;
		break;
	}

	if(sdRet == MCDRV_SUCCESS)
	{
		psDecInfo = GetDecInfo( dCoderId );

		/* State check */
		switch( psDecInfo->dState )
		{
		case CDSP_STATE_READY:
			sdRet = DecReset(dCoderId);
			break;

		default:
			sdRet	= MCDRV_ERROR_STATE;
			break;
		}
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McCdsp_Reset", &sdRet);
#endif
	return sdRet;
}

/****************************************************************************
 *	McCdsp_GetState
 *
 *	Function:
 *			Get state.
 *	Arguments:
 *			ePlayerId	Player ID
 *	Return:
 *			state
 *
 ****************************************************************************/
UINT32	McCdsp_GetState
(
	MC_PLAYER_ID	ePlayerId
)
{
	UINT32		dState	= CDSP_STATE_NOTINIT;
	DEC_INFO	*psDecInfo;

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McCdsp_GetState");
#endif

	if(ePlayerId == eMC_PLAYER_CODER_A)
	{
		psDecInfo	= GetDecInfo(CODER_DEC);
		dState		= psDecInfo->dState;
	}
	else if(ePlayerId == eMC_PLAYER_CODER_B)
	{
		psDecInfo	= GetDecInfo(CODER_ENC);
		dState		= psDecInfo->dState;
	}
	else
	{
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= dState;
	McDebugLog_FuncOut("McCdsp_GetState", &sdRet);
#endif
	return dState;
}


