 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |   Copyright (C) 2006-2011, AuthenTec, Inc. - All Rights Reserved.     |
  * |                                                                       |
  * |  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF    |
  * |  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO  |
  * |  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A       |
  * |  PARTICULAR PURPOSE.                                                  |
  * \=======================================================================/
  */

/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

/* *************************************************************************

   File:            TransportCommon.h

   Description:     This file contains common definitions for both transport driver and transport bridge.

   Author:          James Deng

   ************************************************************************* */
   
#ifndef _TRANSPORT_COMMON_
#define _TRANSPORT_COMMON_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */


#ifndef IN
#define IN
#endif

#ifndef INOUT
#define INOUT
#endif

#ifndef OUT
#define OUT
#endif

typedef struct  _tag_tsTrInterfaceHeader
{
    uint32 uiVersion;  //version of the interface
} tsTrInterfaceHeader;


/*****************
 * TrOperationId *
 *****************
 *
 * This enumerated type defines the operation performed by the IOCTLs. There is
 * not necessarily a 1-to-1 mapping between an item in this list and existing
 * IOCTLs.
 */
typedef enum
{
    TROPID_OPEN = 1,
    TROPID_CLOSE,
    TROPID_RESET,
    TROPID_CHECK_PROTOCOL,
    TROPID_GET_DRIVER_INFO,
    TROPID_GET_DRIVER_POWER,
    TROPID_SET_DRIVER_POWER,
    TROPID_SETUP_TRANSFER,
    TROPID_CANCEL_TRANSFER,
    TROPID_STOP_TRANSFER,
    TROPID_SET_READ_TIMEOUT,
    TROPID_SEND_MOUSE_EVENT,
    TROPID_SEND_KEY_EVENT
} eTrOperationId;

/* Interface Types */
#define TR_IFACE_UNDEF 0x00000000 /* Undefined   */
#define TR_IFACE_SPI_S 0x00000001 /* SPI Slave   */
#define TR_IFACE_SPI_M 0x00000002 /* SPI Master  */
#define TR_IFACE_MCBSP 0x00000004 /* McBSP       */
#define TR_IFACE_I2C   0x00000008 /* I2C         */
#define TR_IFACE_USB   0x00000010 /* USB         */



/****************************************************************************
*                                                                           *
*                            INTERFACE V2                                   *
*                                                                           *
*****************************************************************************/


#define MAX_ABORT_SEQ_LEN_V1 32

typedef struct _tsTrInitParams
{
    uint32 uiVersion;
    uint32 uiStructSize;
    uint32 uiInterruptPolarity;
    uint32 uiMaxDataLen;
    uint32 uiAbortSequenceLen;
    uint8  pucAbortSequence[MAX_ABORT_SEQ_LEN_V1];
} tsTrInitParams;


typedef struct _tsDriverInfo
{
    uint32 uiVersion;
    uint32 uiStructSize;
    uint32 uiDriverVersion;
    uint32 uiInterfaceType;
    uint32 uiInterfaceSpeed;
    uint32 uiSupportsSensorPowerOff;
} tsDriverInfo;

/* Values of TrMouseEvent flags*/
#define TR_MOUSE_MOVE       0x00000001
#define TR_MOUSE_LEFTDOWN   0x00000002
#define TR_MOUSE_LEFTUP     0x00000004
#define TR_MOUSE_RIGHTDOWN  0x00000008
#define TR_MOUSE_RIGHTUP    0x00000010

/* Values of TrKeyEvent key events */
#define TR_KEY_EVENT_DOWN   0x01
#define TR_KEY_EVENT_UP     0x02
#define TR_KEY_EVENT_PRESS  0x03

/* Values of TrKeyEvent key codes */
#define TR_KEY_CODE_ENTER   0x1c
#define TR_KEY_CODE_LEFT    0x69
#define TR_KEY_CODE_RIGHT   0x6a
#define TR_KEY_CODE_UP      0x67
#define TR_KEY_CODE_DOWN    0x6c

typedef struct _tsTrMouseEvent
{
    uint32  uiFlags;        /* TR_MOUSE_* values should be put here */
    int32   iDx;
    int32   iDy;
} tsTrMouseEvent;

typedef struct _tsTrKeyEvent
{
    uint32 uiKeyEvent;      /* TR_KEY_EVENT_* values should be put here */
    uint32 uiKeyCode;       /* TR_KEY_CODE_*  values should be put here */
} tsTrKeyEvent;

/* The following symbols represent the error codes used by this library. */
#define TR_OK                        (0)
#define TR_ERROR                    (-1)
#define TR_FAILURE_DEV_OPEN         (-2)
#define TR_ERROR_MEMORY_ALLOCATION  (-3)
#define TR_BAD_CONTEXT_POINTER      (-4)
#define TR_NOT_SUPPORTED            (-5)
#define TR_OPERATION_ABORTED        (-6)   //special sticky error code 
#define TR_INVALID_PARAMETER        (-7)
#define TR_NEED_MORE_MEMORY         (-8)
#define TR_NOT_IMPLEMENTED          (-9)
#define TR_TIMEOUT                 (-10)


/* The following are base types. */
#define IN
#define OUT
#define INOUT


/* Read Policy values. */
#define RP_DO_NOT_READ                  0
#define RP_READ_ON_INT                  1
#define RP_READ_ON_REQ                  2

/* Interrupt control flags */
#define TR_FLAG_RETURN_INTERRUPT_PACKET 0x1


/* Power Modes. */
#define POWER_MODE_OFF                  0
#define POWER_MODE_SLEEP                1
#define POWER_MODE_STANDBY              2
#define POWER_MODE_ACTIVE               3

/* Interface Types */
#define TR_IFACE_UNDEF 0x00000000 /* Undefined   */
#define TR_IFACE_SPI_S 0x00000001 /* SPI Slave   */
#define TR_IFACE_SPI_M 0x00000002 /* SPI Master  */
#define TR_IFACE_MCBSP 0x00000004 /* McBSP       */
#define TR_IFACE_I2C   0x00000008 /* I2C         */
#define TR_IFACE_USB   0x00000010 /* USB         */



/* The following are the Protocol IDs supported by this transport bridge. */
#define AUTH_PROTOCOL_UNKNOWN           0   /* Unknown protocol.                */
#define AUTH_PROTOCOL_1750              1   /* 1750 and earlier swipe sensors.  */
#define AUTH_PROTOCOL_0850              2   /*  850 (Saturn) sensor.            */



/******************************************************************************
 * Notes:
 *
 * IN:    Parameter used to send information from the transport bridge to the
 *        transport driver.
 *
 * OUT:   Parameter used to receive information from the transport driver.
 *
 * INOUT: Parameter used to send information to the transport driver an to
 *        receive a response back from the transport driver.
 *****************************************************************************/

/******* Header for all structures **********/

typedef struct _tag_TrIoctlHeader
{
    uint32 uiSize;          /* INOUT: Total size of the structure including header.         */
                            /*        On input it contains the size of the input buffer.    */
                            /*        On output it contains the size of the output buffer.  */

    uint32 uiOperationId;   /*    IN: ID of the desired operation. Needed for TransportIOCL */
                            /*        or on systems where everything is tunneled through a  */
                            /*        single IOCTL. See TrOperationId.                      */

    int32  retCode;         /*   OUT: Return code of the IOCTL.                             */
} tsTrIoctlHeader;


/******* Transport Open **********************/

typedef struct _tag_TrOpenCloseIoctlBuffer
{
    tsTrIoctlHeader sHeader;        /* INOUT:   Header.                     */
    uint32          uiProtocolId;   /*    IN:   Selected protocol ID.       */
    tsTrInitParams  sParams;        /*    IN:   Intiialization parameters.  */
} tsTrOpenCloseIoctlBuffer;

/******* Transport Reset *********************/

typedef struct _tag_TrResetIoctlBuffer
{
    tsTrIoctlHeader sHeader;  /* INOUT: Header.    */
} tsTrResetIoctlBuffer;

/******* Transport GetSupportedDrivers *********************/

typedef struct _tag_TrCheckProtocolIoctlBuffer
{
    tsTrIoctlHeader sHeader;      /* INOUT: Header.                                        */
    uint32          uiProtocolID; /* INOUT: On input : Protocol to be checked for support. */
                                  /*        On output: Same protocol ID if supported, 0 if */
                                  /*                   not supported.                      */
} tsTrCheckProtocolIoctlBuffer;


/******* Transport GetDriverInfo *********************/

typedef struct _tag_TrDriverInfoIoctlBuffer
{
   tsTrIoctlHeader sHeader;              /* INOUT: Header.                                   */
    uint32         uiRequestedVersion;   /*    IN: requested driver info structure version.  */
    uint32         uiBufferSize;         /*    INOUT: size of the following buffer on input. */
                                         /*           size of the returned structure or expected buffer size on output */
    uint8          aucBuffer[1];         /*   OUT: buffer to receive the structure.          */
                                         /*        (size = uiBufferSize)                     */
} tsTrDriverInfoIoctlBuffer;


/******* Transport SetDriverPower/GetDriverPower *********************/

typedef struct _tag_TrDriverPowerIoctlBuffer
{
    tsTrIoctlHeader sHeader;      /* INOUT: Header.                                */
    uint32          uiPowerMode;  /* INOUT: Input power mode for SetDriverPower.   */
                                  /*        Output power mode for GetDriver Power. */
} tsTrDriverPowerIoctlBuffer;

/******* Transport SetupTransfer *********************/

typedef struct _tag_TrSetupTransferIoctlBuffer
{
    tsTrIoctlHeader sHeader;          /* INOUT: Header.        */
    uint32          uiPacketLen;      /*    IN: Packet length. */
    uint32          uiNshots;         /*    IN: NShots.        */
    bool32          bKeepOn;          /*    IN: Keep on bit.   */
    uint32          uiReadPolicy;     /*    IN: Read policy.   */
    uint32          uiOptionFlags;    /*    IN: Option flags.  */
} tsTrSetupTransferIoctlBuffer;

/******* Transport CancelTransfer *********************/

typedef struct _tag_TrCancelIoctlBuffer
{
    tsTrIoctlHeader sHeader;  /* INOUT: Header.    */
} tsTrCancelIoctlBuffer;

/******* Transport StopTransfer *********************/

typedef struct _tag_TrStopIoctlBuffer
{
    tsTrIoctlHeader sHeader;  /* INOUT: Header. */
} tsTrStopIoctlBuffer;


/******* Transport Write *********************/

// NO DEDICATED IOCTL

/******* Transport Read *********************/

// NO DEDICATED IOCTL for reading

typedef struct _tag_TrTimeoutIoctlBuffer
{
    tsTrIoctlHeader sHeader;   /* INOUT: Header.         */
    uint32          uiTimeout; /*    IN: Timeout value.  */
} tsTrTimeoutIoctlBuffer;

/******* Transport Ioctl *********************/

typedef struct _tag_TrIoctlBuffer
{
    tsTrIoctlHeader sHeader;      /* INOUT: Header.                                      */
    uint32          uiTimeout;    /*    IN: Timeout value.                               */
    uint32          uiBufLen;     /*    IN: Buffer length. Should be the maximum between */
                                  /*        the input and output buffer lengths.         */
    uint32          uiDataLen;    /* INOUT: As input:  Input data length.                */
                                  /*        As output: Received bytes.                   */
    uint8           aucBuffer[1]; /* INOUT: As input:  Contains the input data.          */
                                  /*        As output: Contains the output data.         */
} tsTrIoctlBuffer;



#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _TRANSPORT_COMMON_ */
