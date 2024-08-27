 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |    Copyright (C) 2006-2009, AuthenTec, Inc. - All Rights Reserved.    |
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
 
   File: 			AuthBuffer.c

   Description:		This file contains OS memory allocation dependent code
   					      for use with the AuthenTec Unified Driver Model. 

                    The AuthBuffer functions are responsible for handling the 
                    OS specific function calls that allocate and de-allocate 
                    memory buffers. The memory buffers may need to have both 
                    physical and virtual addresses depending on how they are 
                    accessed. Typically, DMA hardware requires a physical 
                    address for reading or writing data, whereas the device 
                    driver must access the same buffer via a virtual address. 
                    The tsAUTHBUFFER structure within the driver context is 
                    used to store all required buffer information. The buffer 
                    information contained within the tsAUTHBUFFER structure 
                    will be used by other generic support functions due to its 
                    general purpose nature.

   Author:			Joe Tykowski

   ************************************************************************* */

/** include files **/
#include "hfiles.h"

/** local definitions **/

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

/*++
===============================================================================

Function AuthBufferAllocate

Author:  Joe Tykowski

The AuthBufferAllocate() function is responsible for allocating a memory buffer 
of the size indicated by the structure pointed to by lptsAuthBuffer. 

Parameters
lptsAuthBuffer (input/output)	Pointer to a buffer information structure. If 
                                not present, set to NULL.

Return Value                    Returns a TRUE to indicate that the buffer 
                                was allocated, FALSE otherwise.

===============================================================================
--*/

BOOL 
AuthBufferAllocate (lptsAUTHBUFFER	lptsAuthBuffer)
{
    /* typedef u32 dma_addr_t; in include/asm-arm/types.h */
	dma_addr_t    		PhysAddr;	/* For mapping DMA buffer */


    /* Validate the size of the structure / context... */
    if (lptsAuthBuffer->uiStructSize != sizeof(tsAUTHBUFFER))
    {
        DBGERROR((L("AuthBufferAllocate: Error: Invalid Parameter (Struct Size Was %d s/b %d).\n"),
            (int)lptsAuthBuffer->uiStructSize, sizeof(tsAUTHBUFFER)));
        return FALSE;
    }

    /**************************************************************************
    *******************ENTER PLATFORM SPECIFIC CODE HERE **********************
    ***************************************************************************
    **
    ** This is a good place for platform specific implementation code to be 
    ** inserted.
    **
    **
    ***************************************************************************
    *******************START PLATFORM SPECIFIC CODE HERE **********************
    **************************************************************************/
    if (lptsAuthBuffer->uiFlags & AB_NO_PHYSICAL)
    {
        /* 
        ** This is just a buffer off the heap...
        */
        DBPRINT((L("AuthBufferAllocate: Allocate %ld bytes.\n"), lptsAuthBuffer->uiBufferLength));
        lptsAuthBuffer->pvVirtualBuffer = 
            (void *)kmalloc(lptsAuthBuffer->uiBufferLength, GFP_KERNEL);
    }
    else if (lptsAuthBuffer->uiFlags & AB_DMA_PHYSICAL)
    {
		/*
        ** This MUST be a DMA buffer...
        */

        /* allocate dma-related memory (I/O buffers, descriptors) */
        lptsAuthBuffer->pvVirtualBuffer =
            (void *)dma_alloc_coherent(NULL,
                                       lptsAuthBuffer->uiBufferLength,
                                       &PhysAddr,
                                       __GFP_DMA | __GFP_WAIT | __GFP_NOFAIL | GFP_KERNEL);
 
	    /* Keep the physical address of the buffer for the DMA controller... */
	    lptsAuthBuffer->pvPhysicalBuffer = (void *)PhysAddr;
	}
    else if (lptsAuthBuffer->uiFlags & AB_ISR_PHYSICAL)
    {
        /*
        ** This MUST be an ISR shared buffer...
        */

        /* To Be Done... */
        lptsAuthBuffer->pvVirtualBuffer = 
            (void *)kmalloc(lptsAuthBuffer->uiBufferLength, GFP_ATOMIC);
    }
    else if (lptsAuthBuffer->uiFlags & AB_REG_VIRTUAL)
    {
        lptsAuthBuffer->pvVirtualBuffer = ioremap((uint32)lptsAuthBuffer->pvPhysicalBuffer,
                                                  lptsAuthBuffer->uiBufferLength);
    }
    else 
    {
        return FALSE;
    }

    /**************************************************************************
    *********************END PLATFORM SPECIFIC CODE HERE **********************
    **************************************************************************/

    /* Was the buffer allocated? */
    if ( lptsAuthBuffer->pvVirtualBuffer )
    {
        return TRUE;
    }
    else
    {
        DBGERROR((L("AuthBufferAllocate: Error: Memory Allocation Failed.\n")));
        return FALSE;
    }
}


/*++
===============================================================================

Function AuthBufferRelease

Author:  Joe Tykowski

The AuthBufferRelease() function is responsible for freeing a memory buffer 
indicated by the structure pointed to by lptsAuthBuffer. 

Parameters
lptsAuthBuffer (input)	        Pointer to a buffer information structure. If 
                                not present, set to NULL.

Return Value                    Returns a TRUE to indicate that the buffer 
                                was freed, FALSE otherwise.

===============================================================================
--*/

BOOL 
AuthBufferRelease (lptsAUTHBUFFER	lptsAuthBuffer)
{
    /* typedef u32 dma_addr_t; in include/asm-arm/types.h */
	dma_addr_t    		PhysAddr;	/* For mapping DMA buffer */


    /**************************************************************************
    *******************ENTER PLATFORM SPECIFIC CODE HERE **********************
    ***************************************************************************
    **
    ** This is a good place for platform specific implementation code to be 
    ** inserted.
    **
    **
    ***************************************************************************
    *******************START PLATFORM SPECIFIC CODE HERE **********************
    **************************************************************************/

	/* Was the buffer previously allocated? */
    if (lptsAuthBuffer->pvVirtualBuffer)
    {
        /*
        ** Determine what type of memory buffer needs to be allocated...
        */
        if (lptsAuthBuffer->uiFlags & AB_NO_PHYSICAL)
        {
            /* 
            ** This is just a buffer off the heap...
            */
            kfree(lptsAuthBuffer->pvVirtualBuffer);
        }
        else if (lptsAuthBuffer->uiFlags & AB_DMA_PHYSICAL)
        {
            /*
            ** This MUST be a DMA buffer...
            */

		    /* What was the physical address of the DMA buffer again? */
	        PhysAddr = (dma_addr_t)lptsAuthBuffer->pvPhysicalBuffer;

			/*
            ** DMA buffer has both physical and virtual addresses... 
            ** void dma_free_coherent(struct device *dev, size_t size, 
            **     void *cpu_addr, dma_addr_t handle);
            */
			dma_free_coherent(NULL, 
                              lptsAuthBuffer->uiBufferLength, 
                              lptsAuthBuffer->pvVirtualBuffer,
                              PhysAddr);
        }
        else if (lptsAuthBuffer->uiFlags & AB_ISR_PHYSICAL)
        {
            /*
            ** This MUST be an ISR shared buffer...
            */

            /* To Be Done... */
            kfree(lptsAuthBuffer->pvVirtualBuffer);
        }
        else if (lptsAuthBuffer->uiFlags & AB_REG_VIRTUAL)
        {
            iounmap(lptsAuthBuffer->pvVirtualBuffer);
#if 0
            release_mem_region(lptsAuthBuffer->pvPhysicalBuffer, lptsAuthBuffer->uiBufferLength);
#endif
        }
        else
        {
            /* Unknown allocation requested, return an error... */
            return FALSE;
        }

        /* Mark the buffer as freed... */
        lptsAuthBuffer->pvVirtualBuffer  = (void *) NULL;
        lptsAuthBuffer->pvPhysicalBuffer = (void *) NULL;
    }


    /**************************************************************************
    *********************END PLATFORM SPECIFIC CODE HERE **********************
    **************************************************************************/

    return TRUE;
}

