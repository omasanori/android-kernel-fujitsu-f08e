/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mccdspdrv.h
 *
 *		Description	: CDSP Driver
 *
 *		Version		: 2.0.0 	2011.03.11
 *
 ****************************************************************************/
#ifndef	_MCCDSPDRV_H_
#define	_MCCDSPDRV_H_

/* definition */

/* state */
#define CDSP_STATE_NOTINIT							(0UL)
#define CDSP_STATE_IDLE								(1UL)
#define CDSP_STATE_REGISTERED						(2UL)
#define CDSP_STATE_READY							(3UL)
#define CDSP_STATE_ACTIVE							(4UL)

/* Command (Host -> OS) */
#define CDSP_CMD_HOST2OS_CMN_NONE					(0x00)
#define CDSP_CMD_HOST2OS_CMN_RESET					(0x01)
#define CDSP_CMD_HOST2OS_CMN_CLEAR					(0x02)
#define CDSP_CMD_HOST2OS_CMN_STANDBY				(0x03)
#define CDSP_CMD_HOST2OS_CMN_GET_PRG_VER			(0x04)

#define CDSP_CMD_HOST2OS_SYS_GET_OS_VER				(0x20)
#define CDSP_CMD_HOST2OS_SYS_SET_PRG_INFO			(0x21)
#define CDSP_CMD_HOST2OS_SYS_SET_FORMAT				(0x22)
#define CDSP_CMD_HOST2OS_SYS_VERIFY_STOP_COMP		(0x23)
#define CDSP_CMD_HOST2OS_SYS_INPUT_DATA_END			(0x24)
#define CDSP_CMD_HOST2OS_SYS_CLEAR_INPUT_DATA_END	(0x25)
#define CDSP_CMD_HOST2OS_SYS_TERMINATE				(0x29)
#define CDSP_CMD_HOST2OS_SYS_SET_DUAL_MONO			(0x2A)
#define CDSP_CMD_HOST2OS_SYS_GET_INPUT_POS			(0x2B)
#define CDSP_CMD_HOST2OS_SYS_RESET_INPUT_POS		(0x2C)
#define CDSP_CMD_HOST2OS_SYS_HALT					(0x2D)

#define CDSP_CMD_DRV_SYS_SET_FIFO_CHANNELS			(0x3E)
#define CDSP_CMD_DRV_SYS_SET_BIT_WIDTH				(0x3F)

#define CDSP_CMD_HOST2OS_PRG_MIN					(0x40)
#define CDSP_CMD_HOST2OS_PRG_MAX					(0x6F)

/* Command (OS -> Host) */
#define CDSP_CMD_OS2HOST_CMN_NONE					(0x00)

/* Command parameter */
#define CDSP_CMD_PARAM_ARGUMENT_00					(0)
#define CDSP_CMD_PARAM_ARGUMENT_01					(1)
#define CDSP_CMD_PARAM_ARGUMENT_02					(2)
#define CDSP_CMD_PARAM_ARGUMENT_03					(3)
#define CDSP_CMD_PARAM_ARGUMENT_04					(4)
#define CDSP_CMD_PARAM_ARGUMENT_05					(5)
#define CDSP_CMD_PARAM_ARGUMENT_06					(6)
#define CDSP_CMD_PARAM_ARGUMENT_07					(7)
#define CDSP_CMD_PARAM_ARGUMENT_08					(8)
#define CDSP_CMD_PARAM_ARGUMENT_09					(9)
#define CDSP_CMD_PARAM_ARGUMENT_10					(10)
#define CDSP_CMD_PARAM_ARGUMENT_11					(11)
#define CDSP_CMD_PARAM_ARGUMENT_NUM					(12)
#define CDSP_CMD_PARAM_RESULT_00					(12)
#define CDSP_CMD_PARAM_RESULT_01					(13)
#define CDSP_CMD_PARAM_RESULT_02					(14)
#define CDSP_CMD_PARAM_RESULT_03					(15)
#define CDSP_CMD_PARAM_RESULT_NUM					(4)
#define CDSP_CMD_PARAM_NUM							(CDSP_CMD_PARAM_ARGUMENT_NUM + CDSP_CMD_PARAM_RESULT_NUM)

/* Command Completion */
#define CDSP_CMD_HOST2OS_COMPLETION					(0x80)
#define CDSP_CMD_OS2HOST_COMPLETION					(0x80)

/* Typedef Enum */

typedef enum _MC_PLAYER_ID
{
	eMC_PLAYER_CODER_A,
	eMC_PLAYER_CODER_B
} MC_PLAYER_ID;

/* Typedef Struct */

typedef struct _MC_CODER_FIRMWARE
{
	const UINT8*	pbFirmware;
	UINT32			dSize;
} MC_CODER_FIRMWARE;

typedef struct _MC_CODER_PARAMS
{
	UINT8			bCommandId;
	UINT8			abParam[16];
} MC_CODER_PARAMS;


#if defined (__cplusplus)
extern "C" {
#endif

void	McCdsp_Init( void );
void	McCdsp_Term( void );
SINT32	McCdsp_TransferProgram( MC_PLAYER_ID ePlayerId, MC_CODER_FIRMWARE *psProgram );
SINT32	McCdsp_SetParam( MC_PLAYER_ID ePlayerId, MC_CODER_PARAMS *psParam );
SINT32	McCdsp_GetParam( MC_PLAYER_ID ePlayerId, MC_CODER_PARAMS *psParam );
SINT32	McCdsp_Start( MC_PLAYER_ID ePlayerId );
SINT32	McCdsp_Stop( MC_PLAYER_ID ePlayerId );
SINT32	McCdsp_Reset( MC_PLAYER_ID ePlayerId );
UINT32	McCdsp_GetState( MC_PLAYER_ID ePlayerId );


#if defined (__cplusplus)
}
#endif

#endif /* _MCCDSPDRV_H_ */
