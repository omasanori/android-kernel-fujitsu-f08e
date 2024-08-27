




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
// COPYRIGHT(C) FUJITSU LIMITED 2011 - 2012
/*----------------------------------------------------------------------------*/
/*********************************************************************************/
/*
   @file si_osdebug.h
 */

#ifndef __SI_OSDEBUG_H__
#define __SI_OSDEBUG_H__

typedef enum
{
	 SII_OS_DEBUG_FORMAT_SIMPLE   	= 0x0000u
	,SII_OS_DEBUG_FORMAT_FILEINFO 	= 0x0001u
	,SII_OS_DEBUG_FORMAT_CHANNEL  	= 0x0002u
	,SII_OS_DEBUG_FORMAT_TIMESTAMP	= 0x0004u
}SiiOsDebugFormat_e;


#define MODULE_SET(name) \
    SII_OSAL_DEBUG_##name, \
    SII_OSAL_DEBUG_##name##_DBG =SII_OSAL_DEBUG_##name, \
    SII_OSAL_DEBUG_##name##_ERR, \
    SII_OSAL_DEBUG_##name##_TRACE, \
    SII_OSAL_DEBUG_##name##_ALWAYS, \
    SII_OSAL_DEBUG_##name##_USER1, \
    SII_OSAL_DEBUG_##name##_USER2, \
    SII_OSAL_DEBUG_##name##_USER3, \
    SII_OSAL_DEBUG_##name##_USER4, \
    SII_OSAL_DEBUG_##name##_MASK = SII_OSAL_DEBUG_##name##_USER4,

//The list above must produce values in groups of powers of two (2, 4, 8, 16,...)

typedef enum
{
	MODULE_SET(APP)
	MODULE_SET(TRACE)
    MODULE_SET(POWER_MAN)
    MODULE_SET(TX)
    MODULE_SET(EDID)
    MODULE_SET(HDCP)
    MODULE_SET(AV_CONFIG)
    MODULE_SET(ENTRY_EXIT)
    MODULE_SET(CBUS)
    MODULE_SET(SCRATCHPAD)
    MODULE_SET(SCHEDULER)
    MODULE_SET(CRA)
    MODULE_SET(MIPI)
    MODULE_SET(HDMI)

    // this one MUST be last in the list
    SII_OSAL_DEBUG_NUM_CHANNELS
}SiiOsalDebugChannels_e;

#ifndef SII_DEBUG_CONFIG_RESOURCE_CONSTRAINED //(
typedef void SiiOsDebugChannel_t;
uint32_t SiiOsDebugChannelAdd(uint32_t numChannels, SiiOsDebugChannel_t *paChannelList);
#endif //)

void SiiOsDebugChannelEnable(SiiOsalDebugChannels_e channel);
#define SI_OS_ENABLE_DEBUG_CHANNEL(channel) SiiOsDebugChannelEnable(channel)

void SiiOsDebugChannelDisable(SiiOsalDebugChannels_e channel);
#define SI_OS_DISABLE_DEBUG_CHANNEL(channel) SiiOsDebugChannelDisable(channel)

bool_t SiiOsDebugChannelIsEnabled(SiiOsalDebugChannels_e channel);
void SiiOsDebugSetConfig(uint16_t flags);
#define SiiOsDebugConfig(flags) SiiOsDebugSetConfig(flags)
uint16_t SiiOsDebugGetConfig(void);

void SiiOsDebugPrintAlways(char *pszFormat,...);
void SiiOsDebugPrintAlwaysShort(char *pszFormat,...);
#ifndef C99_VA_ARG_SUPPORT //(
extern unsigned int g_debugLineNo;
extern char *g_debugFileName;
extern SiiOsalDebugChannels_e g_channelArg;
void SiiOsDebugPrintUseGlobal(
#ifndef __KERNEL__
const
#endif
char *pszFormat, ...);
#endif //)

void SiiOsDebugPrintSimple(SiiOsalDebugChannels_e channel, char *pszFormat,...);
void SiiOsDebugPrintShort(SiiOsalDebugChannels_e channel, char *pszFormat,...);
void SiiOsDebugPrint(const char *pFileName, uint32_t iLineNum, SiiOsalDebugChannels_e channel, const char *pszFormat, ...);

#ifdef C99_VA_ARG_SUPPORT //(
#ifdef SII_DEBUG_CONFIG_NO_FILE_LINE //(
#define SII_DEBUG_PRINT(channel,...) SiiOsDebugPrintShort(channel,__VA_ARGS__)
#else //)(
#define SII_DEBUG_PRINT(channel,...) SiiOsDebugPrint(__FILE__,__LINE__,channel,__VA_ARGS__)
#endif //)

#define SII_PRINT_FULL(channel,...) SiiOsDebugPrint(__FILE__,__LINE__,channel,__VA_ARGS__)
#define SII_PRINT(channel,...) SiiOsDebugPrintShort(channel,__VA_ARGS__)
#define SII_PRINT_PLAIN(channel,...) SiiOsDebugPrintSimple(channel,__VA_ARGS__)

#else //)(

#define SII_PRINT_FULL(channel,x) SiiOsDebugPrintUseGlobal x
#define SII_PRINT(channel,x) SiiOsDebugPrintShortUseGlobal x
#define SII_PRINT_PLAIN(channel,x) SiiOsDebugPrintSimpleUseGlobal x

#ifdef SII_DEBUG_CONFIG_NO_FILE_LINE //(
#define SII_DEBUG_PRINT(channel,x) {  g_channelArg = channel; SiiOsDebugPrintShortUseGlobal x ; }
#else //)(
#define SII_DEBUG_PRINT(channel,x) { g_debugLineNo = __LINE__; g_debugFileName = __FILE__; g_channelArg = channel; SiiOsDebugPrintUseGlobal x; }
#endif //)

#endif //)
#define SII_DEBUG(channel,x) if (SiiOsDebugChannelIsEnabled(channel) {x}


enum mhl_log_level {
	MHL_LOG_NONE = 0,
	MHL_LOG_FJ  = (1 << 0), 
	MHL_LOG_SII = (1 << 1),
	MHL_LOG_MAX = (1 << 2),
};

void fj_SiiDebugPrint(const char *format,...);
#endif // #ifndef __SI_OSDEBUG_H__


