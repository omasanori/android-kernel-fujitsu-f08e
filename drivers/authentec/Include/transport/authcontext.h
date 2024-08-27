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

   File:            AuthCONTEXT.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

                    As part of the effort to standardize the driver
                    to the greatest extent possible, the various
                    interfaces typically contained within a device
                    driver context have been abstracted. The resulting
                    context will be used as the basis for all Transport
                    drivers. The structure format for the overall Transport
                    context is detailed in this file.

   Author:          Joe Tykowski

   ************************************************************************* */
#ifndef _AUTH_CONTEXT_H_
#define _AUTH_CONTEXT_H_

/** include files **/

/** local definitions **/

/* Generic device driver entry point information: */
typedef struct tsAUTHCONTEXT_s
{
    /*
    ** Contains all transport related structures, variables and
    ** state information.
    */
    tsAUTHTRANSPORT           sAuthTRANSPORT;

    /*
    ** Contains all driver entry point specific information.
    */
    tsAUTHAES           sAuthAES;

    /*
    ** Contains buffer information for transmitting data
    ** to a sensor, possibly used for DMA.
    */
    tsAUTHBUFFER        sAuthBufferTX;

    /*
    ** Contains buffer information for receiving data from
    ** a sensor, possibly used for DMA.
    */
    tsAUTHBUFFER        sAuthBufferRX;

    /*
    ** Contains all information related to installing an
    ** interrupt service routine on a platform.
    */
    tsAUTHISR           sAuthISR;

    /*
    ** Contains all information required for performing a
    ** DMA transfer to a sensor.
    */
    tsAUTHDMA           sAuthDmaTX;

    /*
    ** Contains all information required for performing a
    ** DMA transfer from a sensor.
    */
    tsAUTHDMA           sAuthDmaRX;

    /*
    ** Contains all information required for handling GPIO
    ** on a platform.
    */
    tsAUTHGPIO          sAuthGPIO;

    /*
    ** Contains all information required to read and write
    ** to a sensor on a platform.
    */
    tsAUTHIO            sAuthIO;

    /*
    ** Contains user defined information.
    */
    tsAUTHPLATFORM      sAuthPLATFORM;

} tsAUTHCONTEXT;
typedef tsAUTHCONTEXT FAR *lptsAUTHCONTEXT;

/*
** The following macro definitions can be used to get a pointer
** to one of the context structures:
*/
#define PCONTEXT(p)         (lptsAUTHCONTEXT)   p

#define PTRANSPORT(p)             (lptsAUTHTRANSPORT)       (&((lptsAUTHCONTEXT) p)->sAuthTRANSPORT)

#define PAES(p)             (lptsAUTHAES)       (&((lptsAUTHCONTEXT) p)->sAuthAES)
#define PBUFFERTX(p)        (lptsAUTHBUFFER)    (&((lptsAUTHCONTEXT) p)->sAuthBufferTX)
#define PBUFFERRX(p)        (lptsAUTHBUFFER)    (&((lptsAUTHCONTEXT) p)->sAuthBufferRX)
#define PISR(p)             (lptsAUTHISR)       (&((lptsAUTHCONTEXT) p)->sAuthISR)
#define PDMATX(p)           (lptsAUTHDMA)       (&((lptsAUTHCONTEXT) p)->sAuthDmaTX)
#define PDMARX(p)           (lptsAUTHDMA)       (&((lptsAUTHCONTEXT) p)->sAuthDmaRX)
#define PGPIO(p)            (lptsAUTHGPIO)      (&((lptsAUTHCONTEXT) p)->sAuthGPIO)
#define PIO(p)              (lptsAUTHIO)        (&((lptsAUTHCONTEXT) p)->sAuthIO)
#define PPLATFORM(p)        (lptsAUTHPLATFORM)  (&((lptsAUTHCONTEXT) p)->sAuthPLATFORM)

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

#endif /* _AUTH_CONTEXT_H_ */
