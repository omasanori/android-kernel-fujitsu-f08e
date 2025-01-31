/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mcdevif.c
 *
 *		Description	: MC Driver device interface
 *
 *		Version		: 2.0.0 	2011.01.25
 *
 ****************************************************************************/


#include "mcdefs.h"
#include "mcdevif.h"
#include "mcservice.h"
#include "mcresctrl.h"
#include "mcdevprof.h"
#include "mcmachdep.h"
#if MCDRV_DEBUG_LEVEL
#include "mcdebuglog.h"
#endif



static MCDRV_PACKET*	gpsPacket	= NULL;

/****************************************************************************
 *	McDevIf_AllocPacketBuf
 *
 *	Description:
 *			allocate the buffer for register setting packets.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR
 *
 ****************************************************************************/
SINT32	McDevIf_AllocPacketBuf
(
	void
)
{
	SINT32	sdRet	= MCDRV_SUCCESS;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McDevIf_AllocPacketBuf");
#endif

	gpsPacket	= McResCtrl_AllocPacketBuf();
	if(gpsPacket == NULL)
	{
		sdRet	= MCDRV_ERROR;
	}
	else
	{
		McDevIf_ClearPacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McDevIf_AllocPacketBuf", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McDevIf_ReleasePacketBuf
 *
 *	Description:
 *			Release the buffer for register setting packets.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDevIf_ReleasePacketBuf
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McDevIf_ReleasePacketBuf");
#endif

	McResCtrl_ReleasePacketBuf();

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McDevIf_ReleasePacketBuf", 0);
#endif
}

/****************************************************************************
 *	McDevIf_ClearPacket
 *
 *	Description:
 *			Clear packets.
 *	Arguments:
 *			none
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDevIf_ClearPacket
(
	void
)
{
#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet;
	McDebugLog_FuncIn("McDevIf_ClearPacket");
#endif

	if(gpsPacket == NULL)
	{
		#if (MCDRV_DEBUG_LEVEL>=4)
			sdRet	= MCDRV_ERROR;
			McDebugLog_FuncOut("McDevIf_ClearPacket", &sdRet);
		#endif
		return;
	}

	gpsPacket[0].dDesc = MCDRV_PACKET_TYPE_TERMINATE;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McDevIf_ClearPacket", 0);
#endif
}

/****************************************************************************
 *	McDevIf_AddPacket
 *
 *	Description:
 *			Add a packet.
 *	Arguments:
 *			dDesc		packet info
 *			bData		packet data
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDevIf_AddPacket
(
	UINT32			dDesc,
	UINT8			bData
)
{
	UINT32	i;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McDevIf_AddPacket");
#endif

	if(gpsPacket == NULL)
	{
	}
	else
	{
		for(i = 0; i < MCDRV_MAX_PACKETS; i++)
		{
			if(gpsPacket[i].dDesc == MCDRV_PACKET_TYPE_TERMINATE)
			{
				break;
			}
		}
		if(i >= MCDRV_MAX_PACKETS)
		{
			McDevIf_ExecutePacket();
			i	= 0;
		}

		gpsPacket[i].dDesc		= dDesc;
		gpsPacket[i].bData		= bData;
		gpsPacket[i+1UL].dDesc	= MCDRV_PACKET_TYPE_TERMINATE;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McDevIf_AddPacket", 0);
#endif
}

/****************************************************************************
 *	McDevIf_AddPacketRepeat
 *
 *	Description:
 *			Add packets to set data at same register over agian.
 *	Arguments:
 *			dDesc			packet info
 *			pbData			poointer to packet data
 *			dDataCount		packet data count
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDevIf_AddPacketRepeat
(
	UINT32			dDesc,
	const UINT8*	pbData,
	UINT32			dDataCount
)
{
	UINT32	dCount;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McDevIf_AddPacketRepeat");
#endif

	for(dCount = 0; dCount < dDataCount; dCount++)
	{
		McDevIf_AddPacket(dDesc, pbData[dCount]);
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McDevIf_AddPacketRepeat", 0);
#endif
}

/****************************************************************************
 *	McDevIf_ExecutePacket
 *
 *	Description:
 *			Execute sequence for register setting.
 *	Arguments:
 *			none
 *	Return:
 *			MCDRV_SUCCESS
 *			MCDRV_ERROR_ARGUMENT
 *
 ****************************************************************************/
SINT32	McDevIf_ExecutePacket
(
	void
)
{
	SINT32	sdRet;
	SINT16	swPacketIndex;
	UINT32	dPacketType;
	UINT32	dParam1;
	UINT32	dParam2;
	UINT16	wAddress;
	UINT16	wRegType;
	MCDRV_UPDATE_MODE	eUpdateMode;

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncIn("McDevIf_ExecutePacket");
#endif

	if(gpsPacket == NULL)
	{
		sdRet	= MCDRV_ERROR_RESOURCEOVER;
	}
	else
	{
		sdRet	= MCDRV_SUCCESS;

		McResCtrl_InitRegUpdate();
		swPacketIndex = 0;
		while ((MCDRV_PACKET_TYPE_TERMINATE != (gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_TYPE_MASK)) && (sdRet == MCDRV_SUCCESS))
		{
			dPacketType = gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_TYPE_MASK;
			switch (dPacketType)
			{
			case MCDRV_PACKET_TYPE_WRITE:
			case MCDRV_PACKET_TYPE_FORCE_WRITE:
				wRegType = (UINT16)(gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_REGTYPE_MASK);
				wAddress = (UINT16)(gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_ADR_MASK);
				if (MCDRV_PACKET_TYPE_WRITE == dPacketType)
				{
					eUpdateMode = eMCDRV_UPDATE_NORMAL;
				}
				else if (MCDRV_PACKET_TYPE_FORCE_WRITE == dPacketType)
				{
					eUpdateMode = eMCDRV_UPDATE_FORCE;
				}
				else
				{
					eUpdateMode = eMCDRV_UPDATE_DUMMY;
				}
				McResCtrl_AddRegUpdate(wRegType, wAddress, gpsPacket[swPacketIndex].bData, eUpdateMode);
				break;

			case MCDRV_PACKET_TYPE_TIMWAIT:
				McResCtrl_ExecuteRegUpdate();
				McResCtrl_InitRegUpdate();
				dParam1 = gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_TIME_MASK;
				McSrv_Sleep(dParam1);
				break;

			case MCDRV_PACKET_TYPE_EVTWAIT:
				McResCtrl_ExecuteRegUpdate();
				McResCtrl_InitRegUpdate();
				dParam1 = gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_EVT_MASK;
				dParam2 = gpsPacket[swPacketIndex].dDesc & MCDRV_PACKET_EVTPRM_MASK;
				sdRet = McResCtrl_WaitEvent(dParam1, dParam2);
				break;

			default:
				sdRet	= MCDRV_ERROR;
				break;
			}

			swPacketIndex++;
		}
		if(sdRet == MCDRV_SUCCESS)
		{
			McResCtrl_ExecuteRegUpdate();
		}
		McDevIf_ClearPacket();
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	McDebugLog_FuncOut("McDevIf_ExecutePacket", &sdRet);
#endif

	return sdRet;
}

/****************************************************************************
 *	McDevIf_ReadDirect
 *
 *	Description:
 *			Read data from register directly
 *	Arguments:
 *			dDesc	register info
 *			pbData	poointer to data
 *	Return:
 *			none
 *
 ****************************************************************************/
void	McDevIf_ReadDirect
(
	UINT32	dDesc,
	UINT8*	pbData
)
{
	UINT8	bSlaveAddr;
	UINT8	abData[2];
	UINT8	bAddr	= (UINT8)(dDesc&MCDRV_PACKET_ADR_MASK);

#if (MCDRV_DEBUG_LEVEL>=4)
	SINT32	sdRet	= 0;
	McDebugLog_FuncIn("McDevIf_ReadDirect");
#endif

	switch(dDesc&MCDRV_PACKET_REGTYPE_MASK)
	{
	case	MCDRV_PACKET_REGTYPE_A:
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		*pbData	= McSrv_ReadI2C(bSlaveAddr, bAddr);
		break;

	case	MCDRV_PACKET_REGTYPE_B_CDSP:
		bSlaveAddr	= McDevProf_GetSlaveAddr(eMCDRV_SLAVE_ADDR_DIG);
		abData[0]	= MCI_CDSP_ADR<<1;
		abData[1]	= bAddr;
		McSrv_WriteI2C(bSlaveAddr, abData, 2);
		bAddr	= MCI_CDSP_WINDOW;
		*pbData	= McSrv_ReadI2C(bSlaveAddr, bAddr);
		break;

	default:
		*pbData	= McResCtrl_GetRegVal((UINT16)(dDesc&MCDRV_PACKET_REGTYPE_MASK), bAddr);
		break;
	}

#if (MCDRV_DEBUG_LEVEL>=4)
	sdRet	= *pbData;
	McDebugLog_FuncOut("McDevIf_ReadDirect", &sdRet);
#endif
}


