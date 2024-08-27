






/*
 SiI8334 Linux Driver

 Copyright (C) 2011 Silicon Image Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation version 2.

 This program is distributed .as is. WITHOUT ANY WARRANTY of any
 kind, whether express or implied; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the
 GNU General Public License for more details.
*/
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/
/*
   @file si_mhl_tx_rcp_table.c
*/

#include "si_c99support.h"
#include "si_memsegsupport.h"

#define	MHL_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHL_DEV_LD_VIDEO					(0x01 << 1)
#define	MHL_DEV_LD_AUDIO					(0x01 << 2)
#define	MHL_DEV_LD_MEDIA					(0x01 << 3)
#define	MHL_DEV_LD_TUNER					(0x01 << 4)
#define	MHL_DEV_LD_RECORD					(0x01 << 5)
#define	MHL_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHL_DEV_LD_GUI						(0x01 << 7)


#define	MHL_MAX_RCP_KEY_CODE	(0x7F + 1)	// inclusive


#if defined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC)
PLACE_IN_CODE_SEG uint8_t rcpSupportTable [MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),		// 0x00 = Select
	(MHL_DEV_LD_GUI),		// 0x01 = Up
	(MHL_DEV_LD_GUI),		// 0x02 = Down
	(MHL_DEV_LD_GUI),		// 0x03 = Left
	(MHL_DEV_LD_GUI),		// 0x04 = Right
	0,						// 0x05 = Right-Up
	0,						// 0x06 = Right-Down
	0,						// 0x07 = Left-Up
	0,						// 0x08 = Left-Down
	(MHL_DEV_LD_GUI),		// 0x09 = Root Menu
	0,						// 0x0A = Setup Menu
	0,						// 0x0B = Contents Menu
	0,						// 0x0C = Favorite Menu
	(MHL_DEV_LD_GUI),		// 0x0D = Exit
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x0E - 0x1F Reserved
	(MHL_DEV_LD_GUI),		// 0x20 = Numeric 0
	(MHL_DEV_LD_GUI),		// 0x21 = Numeric 1
	(MHL_DEV_LD_GUI),		// 0x22 = Numeric 2
	(MHL_DEV_LD_GUI),		// 0x23 = Numeric 3
	(MHL_DEV_LD_GUI),		// 0x24 = Numeric 4
	(MHL_DEV_LD_GUI),		// 0x25 = Numeric 5
	(MHL_DEV_LD_GUI),		// 0x26 = Numeric 6
	(MHL_DEV_LD_GUI),		// 0x27 = Numeric 7
	(MHL_DEV_LD_GUI),		// 0x28 = Numeric 8
	(MHL_DEV_LD_GUI),		// 0x29 = Numeric 9
	0,						// 0x2A = Dot
	(MHL_DEV_LD_GUI),		// 0x2B = Enter
	(MHL_DEV_LD_GUI),		// 0x2C = Clear
	0, 0, 0,				// 0x2D - 0x2F Reserved
	0,						// 0x30 = Channel Up
	0,						// 0x31 = Channel Down
	0,						// 0x32 = Previous Channel
	0,						// 0x33 = Sound Select
	0,						// 0x34 = Input Select
	0,						// 0x35 = Show Information
	0,						// 0x36 = Help
	0,						// 0x37 = Page Up
	0,						// 0x38 = Page Down
	0, 0, 0, 0, 0, 0, 0,	// 0x39 - 0x3F Reserved
	0,						// 0x40 = Reserved
	0,						// 0x41 = Volume Up
	0,						// 0x42 = Volume Down
	0,						// 0x43 = Mute
	(MHL_DEV_LD_GUI),		// 0x44 = Play
	(MHL_DEV_LD_GUI),		// 0x45 = Stop
	(MHL_DEV_LD_GUI),		// 0x46 = Pause
	0,						// 0x47 = Record
	(MHL_DEV_LD_GUI),		// 0x48 = Rewind
	(MHL_DEV_LD_GUI),		// 0x49 = Fast Forward
	0,						// 0x4A = Eject
	(MHL_DEV_LD_GUI),		// 0x4B = Forward
	(MHL_DEV_LD_GUI),		// 0x4C = Backward
	0, 0, 0,				// 0x4D - 0x4F Reserved
	0,						// 0x50 = Angle
	0,						// 0x51 = Subpicture
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x52 - 0x5F Reserved
	0,						// 0x60 = Play_Function
	0,						// 0x61 = Pause__Play_Function
	0,						// 0x62 = Record_Function
	0,						// 0x63 = Pause_Record_Function
	0,						// 0x64 = Stop_Function
	0,						// 0x65 = Mute_Function
	0,						// 0x66 = Restore_Volume_Function
	0,						// 0x67 = Tune_Function
	0,						// 0x68 = Slect_Media_Function
	0, 0, 0, 0, 0, 0, 0, 0,	// 0x69 - 0x70 Reserved
	(MHL_DEV_LD_GUI),		// 0x71 = F1
	(MHL_DEV_LD_GUI),		// 0x72 = F2
	(MHL_DEV_LD_GUI),		// 0x73 = F3
	(MHL_DEV_LD_GUI),		// 0x74 = F4
	0,						// 0x75 = F5
	0, 0, 0, 0, 0, 0, 0 ,0,	// 0x76 - 0x7D = Reserved
	0,						// 0x7E = Vendor_Specific
	0						// 0x7F = Reserved
};
#else
PLACE_IN_CODE_SEG uint8_t rcpSupportTable [MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),		// 0x00 = Select
	(MHL_DEV_LD_GUI),		// 0x01 = Up
	(MHL_DEV_LD_GUI),		// 0x02 = Down
	(MHL_DEV_LD_GUI),		// 0x03 = Left
	(MHL_DEV_LD_GUI),		// 0x04 = Right
	0, 0, 0, 0,				// 05-08 Reserved
	(MHL_DEV_LD_GUI),		// 0x09 = Root Menu
	0, 0, 0,				// 0A-0C Reserved
	(MHL_DEV_LD_GUI),		// 0x0D = Select
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0E-1F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Numeric keys 0x20-0x29
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,						// 0x2A = Dot
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Enter key = 0x2B
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Clear key = 0x2C
	0, 0, 0,				// 2D-2F Reserved
	(MHL_DEV_LD_TUNER),		// 0x30 = Channel Up
	(MHL_DEV_LD_TUNER),		// 0x31 = Channel Dn
	(MHL_DEV_LD_TUNER),		// 0x32 = Previous Channel
	(MHL_DEV_LD_AUDIO),		// 0x33 = Sound Select
	0,						// 0x34 = Input Select
	0,						// 0x35 = Show Information
	0,						// 0x36 = Help
	0,						// 0x37 = Page Up
	0,						// 0x38 = Page Down
	0, 0, 0, 0, 0, 0, 0,	// 0x39-0x3F Reserved
	0,						// 0x40 = Undefined

	(MHL_DEV_LD_SPEAKER),	// 0x41 = Volume Up
	(MHL_DEV_LD_SPEAKER),	// 0x42 = Volume Down
	(MHL_DEV_LD_SPEAKER),	// 0x43 = Mute
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x44 = Play
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x45 = Stop
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x46 = Pause
	(MHL_DEV_LD_RECORD),	// 0x47 = Record
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x48 = Rewind
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x49 = Fast Forward
	(MHL_DEV_LD_MEDIA),		// 0x4A = Eject
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4B = Forward
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4C = Backward
	0, 0, 0,				// 4D-4F Reserved
	0,						// 0x50 = Angle
	0,						// 0x51 = Subpicture
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 52-5F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x60 = Play Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x61 = Pause the Play Function
	(MHL_DEV_LD_RECORD),	// 0x62 = Record Function
	(MHL_DEV_LD_RECORD),	// 0x63 = Pause the Record Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x64 = Stop Function

	(MHL_DEV_LD_SPEAKER),	// 0x65 = Mute Function
	(MHL_DEV_LD_SPEAKER),	// 0x66 = Restore Mute Function
	0, 0, 0, 0, 0, 0, 0, 0, 0, 	                        // 0x67-0x6F Undefined or reserved
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 		// 0x70-0x7F Undefined or reserved
};
#endif /* deined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC) */



