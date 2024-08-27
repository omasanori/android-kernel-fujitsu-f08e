
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
//!file     si_cra_cfg.c
//!brief    Silicon Image Starter Kit CRA configuration data.
//
// No part of this work may be reproduced, modified, distributed,
// transmitted, transcribed, or translated into any language or computer
// format, in any form or by any means without written permission of
// Silicon Image, Inc., 1140 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2008-2011, Silicon Image, Inc.  All rights reserved.
//***************************************************************************/

#include "si_cra_cfg.h"

//------------------------------------------------------------------------------
// si_cra_cfg.h
//------------------------------------------------------------------------------

// Index to this array is the virtual page number in the MSB of the REG_xxx address values
// Indexed with siiRegPageIndex_t value shifted right 8 bits
// DEV_PAGE values must correspond to the order specified in the siiRegPageIndex_t array

pageConfig_t    g_addrDescriptor[SII_CRA_MAX_DEVICE_INSTANCES][SII_CRA_DEVICE_PAGE_COUNT] =
{
    // Instance 0
    {
    { DEV_I2C_0,  DEV_PAGE_PP_0   },    // Port Processor
    { DEV_I2C_0,  0   },
    { DEV_I2C_0,  DEV_PAGE_PP_2   },    // RX TMDS
    { DEV_I2C_0,  DEV_PAGE_IPV    },    // InstaPreview
    { DEV_I2C_0,  DEV_PAGE_PP_4   },    // RX TMDS
    { DEV_I2C_0,  DEV_PAGE_PP_5   },    // PAUTH0
    { DEV_I2C_0,  DEV_PAGE_PP_6   },    // PAUTH1
    { DEV_I2C_0,  DEV_PAGE_PP_7   },    // PAUTH2
    { DEV_I2C_0,  DEV_PAGE_PP_8   },    // CPI 0
    { DEV_I2C_0,  DEV_PAGE_PP_9   },    // NVRAM
    { DEV_I2C_0,  DEV_PAGE_PP_A   },    // RX TMDS
    { DEV_I2C_0,  DEV_PAGE_PP_B   },    // TX TMDS
    { DEV_I2C_0,  DEV_PAGE_PP_C   },    // CBUS 0
    { DEV_I2C_0,  DEV_PAGE_HEAC   },    // HEAC
    { DEV_I2C_0,  DEV_PAGE_OSD    },    // OSD
    { DEV_I2C_0,  DEV_PAGE_AUDIO  },    // Audio

    { DEV_I2C_0,  DEV_PAGE_TPI_0    },  // TPI 0
    { DEV_I2C_0,  DEV_PAGE_TX_L0_0  },  // TX 0 Legacy 0
    { DEV_I2C_0,  DEV_PAGE_TX_L1_0  },  // TX 0 Legacy 1
    { DEV_I2C_0,  DEV_PAGE_TX_2_0   },  // TX 0 Page 2 (not legacy)

    { DEV_DDC_0,  DEV_PAGE_DDC_EDID },  // TX EDID DDC 0
    { DEV_DDC_0,  DEV_PAGE_DDC_SEGM },  // TX EDID DDC 0
    },

    // Instance 1
    {
    { DEV_I2C_0,  DEV_PAGE_PP_0     },
    { DEV_I2C_0,  0   },
    { DEV_I2C_0,  DEV_PAGE_PP_2     },
    { DEV_I2C_0,  0   },
    { DEV_I2C_0,  DEV_PAGE_PP_4     },
    { DEV_I2C_0,  DEV_PAGE_PP_5     },
    { DEV_I2C_0,  DEV_PAGE_PP_6     },
    { DEV_I2C_0,  DEV_PAGE_PP_7     },
    { DEV_I2C_0,  DEV_PAGE_CPI_TX   },    // CPI 1
    { DEV_I2C_0,  DEV_PAGE_PP_9     },
    { DEV_I2C_0,  DEV_PAGE_PP_A     },
    { DEV_I2C_0,  DEV_PAGE_PP_B     },
    { DEV_I2C_0,  DEV_PAGE_CBUS_1   },    // CBUS 1
    { DEV_I2C_0,  DEV_PAGE_HEAC     },
    { DEV_I2C_0,  DEV_PAGE_OSD      },
    { DEV_I2C_0,  DEV_PAGE_AUDIO    },

    { DEV_I2C_0,  DEV_PAGE_TPI_1    },  // TPI 1
    { DEV_I2C_0,  DEV_PAGE_TX_L0_1  },  // TX 1 Legacy 0
    { DEV_I2C_0,  DEV_PAGE_TX_L1_1  },  // TX 1 Legacy 1
    { DEV_I2C_0,  DEV_PAGE_TX_2_1   },  // TX 1 Page 2 (not legacy)

    { DEV_DDC_1,  DEV_PAGE_DDC_EDID },  // TX EDID DDC 1
    { DEV_DDC_1,  DEV_PAGE_DDC_SEGM },  // TX EDID DDC 1
    }
};
