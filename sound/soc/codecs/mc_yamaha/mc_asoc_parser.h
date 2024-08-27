/*
 * MC ASoC codec driver - private header
 *
 * Copyright (c) 2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef MC_ASOC_PARSER_H
#define MC_ASOC_PARSER_H

#include <sound/soc.h>
#include "mctypedef.h"

extern int	mc_asoc_parser(	struct mc_asoc_data	*mc_asoc,
						   UINT8				hwid,
						   void					*param,
						   UINT32				size,
						   UINT32				option);


#endif /* MC_ASOC_PARSER_H */
