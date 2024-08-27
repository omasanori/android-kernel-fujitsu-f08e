/* /=======================================================================\
 * |                  AuthenTec Embedded (AE) Software                     |
 * |                                                                       |
 * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
 * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
 * |                                                                       |
 * |     Copyright (C) 2006-2011, AuthenTec, Inc. - All Rights Reserved.   |
 * |                                                                       |
 * |  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF    |
 * |  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO  |
 * |  THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND/OR FITNESS FOR A       |
 * |  PARTICULAR PURPOSE.                                                  |
 * \=======================================================================\
 */
         /*****************************************************/
         /*                                                   */
         /* ######   #######        #     #  #######  ####### */
         /* #     #  #     #        ##    #  #     #     #    */
         /* #     #  #     #        # #   #  #     #     #    */
         /* #     #  #     #        #  #  #  #     #     #    */
         /* #     #  #     #        #   # #  #     #     #    */
         /* #     #  #     #        #    ##  #     #     #    */
         /* ######   #######        #     #  #######     #    */
         /*                                                   */
         /*                                                   */
         /*           #######  ######   ###  #######          */
         /*           #        #     #   #      #             */
         /*           #        #     #   #      #             */
         /*           #####    #     #   #      #             */
         /*           #        #     #   #      #             */
         /*           #        #     #   #      #             */
         /*           #######  ######   ###     #             */
         /*                                                   */
         /*****************************************************/

/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/
/* *************************************************************************
                                                                            
   File:            platcfg.h
                                                                            
   Description:     This file implements a set of platform specific         
                    definitions for implementing an transport driver       
                                                                              
   ************************************************************************* */ 

#ifndef _AUTH_TRANSPORT_CFG_H_                                             
#define _AUTH_TRANSPORT_CFG_H_                                             
                                                                     
/** include files **/                                                
                                                                     
/** local definitions **/                                                              

/******************************************************************************
*******************************************************************************
**                                                                             
**                          SPI SLAVE CONFIGURATION                            
**                                                                             
*******************************************************************************
**                               TIMING DIAGRAM                                
**                                                                             
**             TXD ------<MSB><  ><  ><  ><  ><  ><  ><LSB>------              
**                                                                             
**             RXD ------<MSB><  ><  ><  ><  ><  ><  ><LSB>------              
**                         _   _   _   _   _   _   _   _                       
**             TXC _______/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \________              
**                 _____                                    _____              
**             FSX      \__________________________________/                   
**                                                                             
** TXD/RXD changes on the rising edge of the TXC.                              
** TXD/RXD is stable and latched on the falling edge of the TXC.               
** TXD and RXD share the same clock and are dependent of each other.           
******************************************************************************/




/*===============begin: symbols to be defined when porting======================*/

/* FUJITSU:2011-12-12 FingerPrintSensor mod start */
/* FUJITSU:2012-07-09 for Naboo-S mod start */
/*The NO of GPIO pin used as sensor interrupt on target system */
//#define SENSOR_GPIO_INT               23    /* 23 -> 23 */
#define SENSOR_GPIO_INT               43    /* 23 -> 23 */

/*Which SPI controller/bus is used by the sensor?*/
//#define TRANSPORT_IO_PORT             11 
#define TRANSPORT_IO_PORT              4    /* GSBI11 -> GSBI4 */

/*Which CS is used?*/
/* 2012.09.03 Driver mod start */
#define TRANSPORT_IO_SPI_CS	          2     /* GSBI11_1 -> GSBI4_2 */
//#define TRANSPORT_IO_SPI_CS	          1     /* GSBI11_1 -> GSBI4_1 */
/* 2012.09.03 Driver mod end */
/* FUJITSU:2012-07-09 for Naboo-S mod end */

/* FUJITSU:2011-12-12 FingerPrintSensor mod end */

/*What is the SPI IO data rate?*/
#define TRANSPORT_IO_DATA_RATE            15060000   

/*The max number of bytes to transfer via spi interface at a time*/
/* FUJITSU:2012-09-10 UPDATE Start */
//#define MAX_SPI_TRANSFER_PER_REQUEST   4000
#define MAX_SPI_TRANSFER_PER_REQUEST   500
/* FUJITSU:2012-09-10 UPDATE End */

/*
*AUTH_DEVICE_NO_MAJOR: the numeric value to use as the major number for our character
* device driver. This can be any unused value as long as an appropriate
* device node is created on the file system, under this major number.
*/
#define AUTH_DEVICE_NO_MAJOR 240

/*
*What sensor is connected to the platform?
*/
#define TRANSPROT_SUPPORTED_PROTOCOL    AUTH_PROTOCOL_0850

/**NOTE: THE SYMBOLS ABOVE NEED TO BE DEFINED ACCORDINGLY FOR THE TARGET PLATFORM **/

/*================end: symbol to be defined when porting======================*/




/*The #define is used to enable basic IO test for AES850*/
//#define SENSOR_IO_TEST_850
/* FUJITSU:2011-12-12 FingerPrintSensor del */

/*The #define is used to enable basic IO test for AES1750*/
//#define SENSOR_IO_TEST_1750


#define TRANSPROT_INTERFACE_USED        TR_IFACE_SPI_S   //SPI slave
                                                         
#define TRANSPORT_ENABLE_SENSOR_POWEROFF


#if (TRANSPROT_SUPPORTED_PROTOCOL == AUTH_PROTOCOL_1750)
#define MAX_BUFFER_NUMBER_IN_QUEUE       40
#define ONE_BUFFER_SIZE_IN_QUEUE            1*1024
#else
#define MAX_BUFFER_NUMBER_IN_QUEUE       1
#define ONE_BUFFER_SIZE_IN_QUEUE             48*1024
#endif

/******************************************************************************
** These #defines are used to insure the code model being compiled matches     
** the desired product configuration. If code is designed for one OS and       
** compiled for another, a #error message will be generated. The most likely   
** reason for the error is either the code needs to be ported to a new OS      
** (and was provided as an example of what needs to be done) or the code was   
** copied from the wrong source directory when creating the driver.            
*/                                                                             
#define TRANSPORT_USE_OS_UNKNOWN  		0                                                                
#define TRANSPORT_USE_OS_LINUX    		1                                                                
#define TRANSPORT_USE_OS_WINMOBILE		2                                                                
#define TRANSPORT_USE_OS_WIN32    		3                                                                
#define TRANSPORT_USE_OS_SYMBIAN  		4                                                                
#define TRANSPORT_USE_OS					TRANSPORT_USE_OS_LINUX                                                     
                                                                                                                                                          



/********************************************************************
** This section handles tracing for the Transport driver. All debug trace  
** is directed to the Transport tracing functions. If you want to enable   
** tracing, then add #define _TRACING_ENABLED_ to hfiles.h before    
** including this file.                                              
********************************************************************/
#ifdef _TRACING_ENABLED_                                             
#define DBPRINT( m )                printk m                  
#define LDBPRINT( m )               printk m                  
#else                                                                
#define DBPRINT( m )                                         
#define LDBPRINT( m )                                                
#endif                                                               
                                                                       
#ifdef _ERROR_TRACING_ENABLED_                         
#define DBGERROR( m )                printk m                 
#else                                                               
#define DBGERROR( m )                     printk m                           
#endif                                                              
                                                                                       

#undef L
#define L(str)         str

                                                                     
/********************************************************************
**   The following #defines are necessary for the code in the        
**   #include below to compile.                                      
**                                                                   
**   In the following parametric #defines the parameters             
**   s, d, and c have the following consistant meaning:              
**                                                                   
**      s = Source Argument.                                         
**      d = Destination Argument.                                    
**      c = Count.                                                   
**                                                                   
*********************************************************************
** The following #defines are PLATFORM DEPENDENT. They are used      
** commonly across all sensor class implementations.                 
********************************************************************/
                                                                     
#define MOVE_MEMORY( d, s, c )      MEMMOVE( d, s, c )               
#define FILL_MEMORY( d, c, s )      MEMSET( d, s, c )                
#define MEMORY_POOL                                                  
//#define L( str )                    _T( str )                        
                                                                               
                                                                     
/** default settings **/                                             
                                                                     
/** external functions **/                                           
                                                                     
/** external data **/                                                
                                                                     
/** internal functions **/                                           
                                                                     
/** public data **/                                                  
                                                                     
/** private data **/                                                 
                                                                     
/** private functions **/                                            
                                                                     
/** public functions **/                                             
                                                                     
                                                                     
#endif /* _AES_PLAT_CFG_H_ */                                        



