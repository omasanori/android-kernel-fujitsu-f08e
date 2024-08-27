/****************************************************************************
 *
 *		Copyright(c) 2011 Yamaha Corporation. All rights reserved.
 *
 *		Module		: mccdspdrv_d.h
 *
 *		Description	: CDSP Driver
 *
 *		Version		: 2.0.0 	2011.02.21
 *
 ****************************************************************************/
#ifndef	_MCCDSPDRV_D_H_
#define	_MCCDSPDRV_D_H_

/* macro */
#ifndef	LOBYTE
#define	LOBYTE( w )						(UINT8)((UINT32)(w) & 0xFFUL)
#endif	/* LOBYTE */
#ifndef	HIBYTE
#define	HIBYTE( w )						(UINT8)((UINT32)(w) >> 8)
#endif	/* HIBYTE */
#ifndef	MAKEWORD
#define	MAKEWORD( l, h )				((UINT16)((UINT32)(l) & 0xff) | (UINT16)(((UINT32)(h) & 0xff) << 8))
#endif	/* LOBYTE */
#ifndef	MAKEBYTE
#define	MAKEBYTE( l, h )				(UINT8)(((l) & 0x0f) | (((h) << 4) & 0xf0))
#endif	/* LOBYTE */

/* definition */
#define CODER_DEC						(0UL)
#define CODER_ENC						(1UL)
#define CODER_NUM						(2UL)

/* Program download flag */
#define PROGRAM_NO_DOWNLOAD				(0)
#define PROGRAM_DOWNLOAD				(1)

/* Program parameter */
#define PRG_DESC_VENDOR_ID				(0)
#define PRG_DESC_FUNCTION_ID			(2)
#define PRG_DESC_PRG_TYPE				(4)
#define PRG_DESC_PRG_SCRAMBLE			(6)
#define PRG_DESC_DATA_SCRAMBLE			(8)
#define PRG_DESC_ENTRY_ADR				(10)
#define PRG_DESC_FUNCTABLE_LOAD_ADR		(12)
#define PRG_DESC_FUNCTABLE_SIZE			(14)
#define PRG_DESC_PATCHTABLE_LOAD_ADR	(16)
#define PRG_DESC_PATCHTABLE_SIZE		(18)
#define PRG_DESC_PRG_LOAD_ADR			(20)
#define PRG_DESC_PRG_SIZE				(22)
#define PRG_DESC_DATA_LOAD_ADR			(24)
#define PRG_DESC_DATA_SIZE				(26)
#define PRG_DESC_EXTPRG_LOAD_ADR		(28)
#define PRG_DESC_EXTPRG_SIZE			(30)
#define PRG_DESC_EXTDATA_LOAD_ADR		(32)
#define PRG_DESC_EXTDATA_SIZE			(34)
#define PRG_DESC_DRAMBEGIN_ADR			(36)
#define PRG_DESC_DRAM_SIZE				(38)
#define PRG_DESC_STACK_BEGIN_ADR		(40)
#define PRG_DESC_STACK_SIZE				(42)
#define PRG_DESC_VRAMSEL				(44)
#define PRG_DESC_IROMSEL				(46)
#define PRG_DESC_OFIFO_IRQ_PNT			(48)

#define PRG_DESC_PROGRAM				(50)

/* Program data parameter */
#define	PRG_PRM_TYPE_TASK0				(0x0001)
#define	PRG_PRM_TYPE_TASK1				(0x0002)
#define	PRG_PRM_SCRMBL_DISABLE			(0x0000)
#define	PRG_PRM_SCRMBL_ENABLE			(0x0001)

/* CDSP MSEL */
#define MSEL_PROG						(0x00)
#define MSEL_DATA						(0x01)

/* FIFO ID */
#define FIFO_NONE						(0x00000000UL)
#define FIFO_EFIFO_MASK					(0x0000FF00UL)
#define FIFO_EFIFO						(0x00000100UL)
#define FIFO_OFIFO_MASK					(0x00FF0000UL)
#define FIFO_OFIFO						(0x00010000UL)

/* FIFO size */
#define FIFOSIZE_OFIFO					(2048)

/* format */
#define CODER_FMT_FS_48000				(0)
#define CODER_FMT_FS_44100				(1)
#define CODER_FMT_FS_32000				(2)
#define CODER_FMT_FS_24000				(4)
#define CODER_FMT_FS_22050				(5)
#define CODER_FMT_FS_16000				(6)
#define CODER_FMT_FS_12000				(8)
#define CODER_FMT_FS_11025				(9)
#define CODER_FMT_FS_8000				(10)

#define IN_SOURCE_EFIFO					(1)
#define IN_SOURCE_NONE					(3)

#define OUT_DEST_OFIFO					(0)
#define OUT_DEST_NONE					(3)

/* Start Flag */
#define OUTPUT_START_OFF				(0)
#define OUTPUT_START_ON					(1)

/* Stop: verify stop completion */
#define MADEVCDSP_VERIFY_COMP_OFF		(0)
#define MADEVCDSP_VERIFY_COMP_ON		(1)

/* typdef struct */
typedef struct _FSQ_DATA_INFO
{
	const UINT8		*pbData;
	UINT16			wSize;
	UINT16			wLoadAddr;
	UINT16			wScramble;
	UINT8			bMsel;
} FSQ_DATA_INFO;

typedef struct _PROGRAM_INFO
{
	UINT16					wVendorId;
	UINT16					wFunctionId;
	UINT16					wProgType;
	UINT16					wProgScramble;
	UINT16					wDataScramble;
	UINT16					wEntryAddress;
	UINT16					wFuncTableLoadAddress;
	UINT16					wFuncTableSize;
	UINT16					wPatchTableLoadAddress;
	UINT16					wPatchTableSize;
	UINT16					wProgramLoadAddress;
	UINT16					wProgramSize;
	UINT16					wDataLoadAddress;
	UINT16					wDataSize;
	UINT16					wExtProgramLoadAddress;
	UINT16					wExtProgramSize;
	UINT16					wExtDataLoadAddress;
	UINT16					wExtDataSize;
	UINT16					wDramBeginAddress;
	UINT16					wDramSize;
	UINT16					wStackBeginAddress;
	UINT16					wStackSize;
	UINT16					wVramSel;
	UINT16					wIromSel;
} PROGRAM_INFO;

typedef struct _FORMAT_INFO
{
	UINT8					bFs;
} FORMAT_INFO, *PFORMAT_INFO;

typedef struct _FIFO_SYNC_CH_INFO
{
	UINT8					bEfifoSync;
	UINT8					bOfifoSync;
	UINT8					bEfifoCh;
	UINT8					bOfifoCh;
} FIFO_SYNC_CH_INFO;

typedef struct _BIT_WIDTH_INFO
{
	UINT8					bEfifoBit;
	UINT8					bOfifoBit;
} BIT_WIDTH_INFO;

typedef struct _DEC_INFO
{
	UINT32					dCoderId;			/* ID (Do not change.) */
	UINT32					dState;				/* State */

	PROGRAM_INFO			sProgInfo;			/* Program information */
	FORMAT_INFO				sFormat;			/* Format */
	MC_CODER_PARAMS			sParams;			/* C-DSP Command Parameter */
	BIT_WIDTH_INFO			sBitWidth;			/* Bit Width */
	FIFO_SYNC_CH_INFO		sFifoSyncCh;		/* FIFO Sync Ch */

	UINT16					wOfifoIrqPnt;		/* OFIFO IRQ Point */
} DEC_INFO;


#endif /* _MCCDSPDRV_D_H_ */
