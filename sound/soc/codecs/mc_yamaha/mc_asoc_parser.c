/*
 * MC ASoC codec driver
 *
 * Copyright (c) 2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.	In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/

#include "mc_asoc.h"
#include "mc_asoc_priv.h"
#include "mcdriver.h"

#define	PARSER_CHUNKSIZE_NAME		(4)
#define	PARSER_CHUNKSIZE_DATASIZE	(4)

typedef union {
	char	c[PARSER_CHUNKSIZE_NAME];
	UINT32	ul;
}parser_chunkname;

static UINT32	parser_offset	=0;
static UINT32	parser_size		=0;
static UINT8	*parser_param	=NULL;

static void	parser_init(UINT8* param, UINT32 size)
{
	parser_offset	= 0;
	parser_size		= size;
	parser_param	= param;
}

static UINT32	parser_get_offset(void)
{
	return parser_offset;
}

static int	parser_set_offset(UINT32 offset)
{
	if(offset > parser_size) {
		return -EIO;
	}
	parser_offset	= offset;
	return 0;
}

static int	parser_get_chunkname(parser_chunkname* chunkname)
{
	int		i;

	if((parser_param == NULL) || (chunkname == NULL)) {
		return -EINVAL;
	}
	if(parser_offset == parser_size) {
		/*	data end	*/
		chunkname->ul	= 0;
		return 0;
	}
	if((parser_offset+PARSER_CHUNKSIZE_NAME) > parser_size) {
		return -EINVAL;
	}
	for(i = 0; i < PARSER_CHUNKSIZE_NAME; i++) {
		chunkname->c[i]	= *(parser_param+parser_offset++);
	}
	return 0;
}

static int	parser_get_chunksize(UINT32 *chunksize)
{
	int		i;

	if((parser_param == NULL) || (chunksize == NULL)) {
		return -EINVAL;
	}
	if((parser_offset+PARSER_CHUNKSIZE_DATASIZE) > parser_size) {
		return -EINVAL;
	}

	*chunksize	= 0;
	for(i = 0; i < PARSER_CHUNKSIZE_DATASIZE; i++) {
		*chunksize	|= (*(parser_param+parser_offset++) << i*8);
	}
	return 0;
}

static int	parser_get_byte(UINT8 *byte)
{
	if((parser_param == NULL) || (byte == NULL)) {
		return -EINVAL;
	}
	if((parser_offset+1) > parser_size) {
		return -EINVAL;
	}

	*byte	= *(parser_param+parser_offset++);
	return 0;
}

static int	parser_get_word(UINT16 *word)
{
	if((parser_param == NULL) || (word == NULL)) {
		return -EINVAL;
	}
	if((parser_offset+2) > parser_size) {
		return -EINVAL;
	}

	*word	= *(parser_param+parser_offset++);
	*word	|= (*(parser_param+parser_offset++) << 8);
	return 0;
}

static int	parser_get_dword(UINT32 *dword)
{
	int		i;
	if((parser_param == NULL) || (dword == NULL)) {
		return -EINVAL;
	}
	if((parser_offset+sizeof(UINT32)) > parser_size) {
		return -EINVAL;
	}

	*dword	= 0;
	for(i = 0; i < sizeof(UINT32); i++) {
		*dword	|= (*(parser_param+parser_offset++) << (i*8));
	}
	return 0;
}

static int	parser_get_data(UINT8 *data, UINT32 bytes)
{
	if((parser_param == NULL) || (data == NULL)) {
		return -EINVAL;
	}
	if((parser_offset+bytes) > parser_size) {
		return -EINVAL;
	}

	memcpy(data, (parser_param+parser_offset), bytes);
	parser_offset	+= bytes;
	return 0;
}

static int	parser_skip(UINT32 size)
{
	if(parser_param == NULL) {
		return -EINVAL;
	}
	if((parser_offset+size) > parser_size) {
		return -EINVAL;
	}
	parser_offset	+= size;
	return 0;
}

const parser_chunkname	chunk_root			={{'P','C','Y','D'}};
const parser_chunkname	chunk_dev			={{'D','E','V','_'}};
const parser_chunkname	chunk_aeng			={{'A','E','N','G'}};
const parser_chunkname	chunk_bex			={{'B','E','X','_'}};
const parser_chunkname	chunk_wide			={{'W','I','D','E'}};
const parser_chunkname	chunk_drc			={{'D','R','C','_'}};
const parser_chunkname	chunk_eq5			={{'E','Q','5','_'}};
const parser_chunkname	chunk_eq3			={{'E','Q','3','_'}};
const parser_chunkname	chunk_aeex			={{'A','E','E','X'}};
const parser_chunkname	chunk_aeexprog		={{'P','R','O','G'}};
const parser_chunkname	chunk_aeexparm		={{'P','A','R','M'}};
const parser_chunkname	chunk_hndf			={{'H','N','D','F'}};
const parser_chunkname	chunk_parm			={{'P','A','R','M'}};
const parser_chunkname	chunk_dng			={{'D','N','G','_'}};
const parser_chunkname	chunk_dtmf			={{'D','T','M','F'}};
const parser_chunkname	chunk_agc			={{'A','G','C','_'}};
const parser_chunkname	chunk_android		={{'A','N','D','R'}};
const parser_chunkname	chunk_android_aeg	={{'A','A','E','G'}};


static int	check_DEV(
	UINT8	hwid,
	UINT32	rootsize
)
{
	int		err	= 0;
	UINT8	data;
	parser_chunkname	chunkname;
	UINT32	chunksize;
	UINT32	offset	= parser_get_offset();

	TRACE_FUNC();

	while(rootsize > 0) {
		if((err = parser_get_chunkname(&chunkname)) < 0) {
			dbg_info("get_chunkname err\n");
			return -EINVAL;
		}
		if(chunkname.ul == 0) {
			/*	DEV_ not found	*/
			return -EINVAL;
		}
		else if(chunkname.ul == chunk_dev.ul) {
			/*	'DEV_'	*/
			if((err = parser_get_chunksize(&chunksize)) < 0) {
				return err;
			}
			if(chunksize != 4) {
				dbg_info("chunksize err\n");
				return -EINVAL;
			}
			if((err = parser_get_byte(&data)) != 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			if(data != hwid) {
				dbg_info("H/W ID=0x%02X\n", data);
				return -EINVAL;
			}
			break;
		}
		else {
			if((err = parser_get_chunksize(&chunksize)) < 0) {
				return err;
			}
			if((err = parser_skip(chunksize)) < 0) {
				dbg_info("skip err\n");
				return err;
			}
			rootsize	-= (PARSER_CHUNKSIZE_NAME+PARSER_CHUNKSIZE_DATASIZE+chunksize);
		}
	}

	err	= parser_set_offset(offset);
	return err;
}

static int	parse_AENG_PRM(
	struct mc_asoc_data	*mc_asoc,
	UINT32				option,
	UINT32				param_size,
	UINT8				*abParam[],
	UINT8				*block_onoff
)
{
	int		i;
	int		err	= 0;
	UINT32	chunksize;
	UINT8	param[param_size];
	char	str[128], *p;

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize < 4) {
		dbg_info("chunksize err\n");
		return -EINVAL;
	}
	if((err = parser_get_byte(block_onoff)) != 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	dbg_info("onoff=0x%02X\n", *block_onoff);
	if((err = parser_skip(3)) < 0) {
		dbg_info("skip err\n");
		return err;
	}

	if(*block_onoff == 1) {
		if((chunksize-4) != param_size) {
			return -EINVAL;
		}
		if((err	= parser_get_data(param, param_size)) < 0) {
			return err;
		}
		switch(option&0xFF) {
		case	YMC_DSP_OUTPUT_BASE:
		case	YMC_DSP_OUTPUT_SP:
		case	YMC_DSP_OUTPUT_RC:
		case	YMC_DSP_OUTPUT_HP:
		case	YMC_DSP_OUTPUT_LO1:
		case	YMC_DSP_OUTPUT_LO2:
		case	YMC_DSP_OUTPUT_BT:
			mutex_lock(&mc_asoc->mutex);
			memcpy(abParam[option&0xFF], param, param_size);
			mutex_unlock(&mc_asoc->mutex);
			dbg_info("abParam[%ld]=\n", option&0xFF);
			p = str;
			for(i = 0; i < param_size; i++) {
				p += sprintf(p, "0x%02x ", abParam[option&0xFF][i]);
				if((i%16) == 15) {
					dbg_info("%s\n", str);
					p = str;
				}
			}
			if(p != str) {
				dbg_info("%s\n", str);
			}
			break;

		case	YMC_DSP_INPUT_BASE:
			mutex_lock(&mc_asoc->mutex);
			memcpy(abParam[MC_ASOC_DSP_INPUT_BASE], param, param_size);
			mutex_unlock(&mc_asoc->mutex);
			dbg_info("abParam[%d]=\n", MC_ASOC_DSP_INPUT_BASE);
			p = str;
			for(i = 0; i < param_size; i++) {
				p += sprintf(p, "0x%02x ", abParam[MC_ASOC_DSP_INPUT_BASE][i]);
				if((i%16) == 15) {
					dbg_info("%s\n", str);
					p = str;
				}
			}
			if(p != str) {
				dbg_info("%s\n", str);
			}
			break;
		case	YMC_DSP_INPUT_EXT:
			mutex_lock(&mc_asoc->mutex);
			memcpy(abParam[MC_ASOC_DSP_INPUT_EXT], param, param_size);
			mutex_unlock(&mc_asoc->mutex);
			dbg_info("abParam[%d]=\n", MC_ASOC_DSP_INPUT_EXT);
			p = str;
			for(i = 0; i < param_size; i++) {
				p += sprintf(p, "0x%02x ", abParam[MC_ASOC_DSP_INPUT_EXT][i]);
				if((i%16) == 15) {
					dbg_info("%s\n", str);
					p = str;
				}
			}
			if(p != str) {
				dbg_info("%s\n", str);
			}
			break;

		default:
			dbg_info("option err(%ld)\n", option);
			return	-EINVAL;
		}
	}
	else {
		if((err = parser_skip(chunksize-4)) < 0) {
			dbg_info("skip err\n");
			return err;
		}
	}

	return chunksize+8;
}

static int	store_aeex_param(
	struct mc_asoc_data	*mc_asoc,
	MC_AE_EX_STORE	*aeex_prm,
	MCDRV_EXPARAM	stExParam)
{
	int		i, j;
	int		err	= 0;
	char	str[128], *p;

	TRACE_FUNC();

	mutex_lock(&mc_asoc->mutex);

	for (i = 0; i < aeex_prm->param_count; i++) {
		if (aeex_prm->exparam[i].bCommand == stExParam.bCommand) {
			memcpy(aeex_prm->exparam[i].abParam,
					stExParam.abParam, sizeof(stExParam.abParam));
			break;
		}
	}

	if(i == aeex_prm->param_count) {
		if (i < MC_ASOC_MAX_EXPARAM) {
			aeex_prm->exparam[i].bCommand = stExParam.bCommand;
			memcpy(aeex_prm->exparam[i].abParam,
					stExParam.abParam, sizeof(stExParam.abParam));

			aeex_prm->param_count++;
		}
		else {
			err	= -EINVAL;
			dbg_info("The setting range of the parameter was exceeded. \n");
		}
	}

	dbg_info("Command=0x%02x\n", aeex_prm->exparam[i].bCommand);
	p = str;
	p += sprintf(p, "Param=");
	for(j = 0; j < sizeof(stExParam.abParam); j++) {
		p += sprintf(p, "%02x ", aeex_prm->exparam[i].abParam[j]);
		if((i%16) == 15) {
			dbg_info("%s\n", str);
			p = str;
		}
	}
	if(p != str) {
		dbg_info("%s\n", str);
	}

	mutex_unlock(&mc_asoc->mutex);
	return err;
}

static	UINT8	prog[AE_EX_PROGRAM_SIZE]; /* FUJITSU:2011-11-07  */

static int	parse_AENG_AEEX(
	struct mc_asoc_data	*mc_asoc,
	UINT32				option,
	UINT8				*block_onoff
)
{
	int		i;
	int		err	= 0;
	parser_chunkname	chunkname;
	UINT32	chunksize,
			chunksize_prog,
			chunksize_parm,
			chunk_offset	= 0;
/*	UINT8	prog[AE_EX_PROGRAM_SIZE]; FUJITSU:2011-11-07  */
	MCDRV_EXPARAM	stExParam;
	UINT8	exist_prog	= 0;
	char	str[128], *p;

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize < 4) {
		dbg_info("chunksize err\n");
		return -EINVAL;
	}
	chunk_offset	+= PARSER_CHUNKSIZE_DATASIZE;
	if((err = parser_get_byte(block_onoff)) != 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	chunk_offset++;
	dbg_info("onoff=0x%02X\n", *block_onoff);

	if((err = parser_skip(3)) < 0) {
		dbg_info("skip err\n");
		return err;
	}
	chunk_offset	+= 3;

	if(*block_onoff == 1) {
		switch(option&0xFF) {
		case	YMC_DSP_OUTPUT_BASE:
		case	YMC_DSP_OUTPUT_SP:
		case	YMC_DSP_OUTPUT_RC:
		case	YMC_DSP_OUTPUT_HP:
		case	YMC_DSP_OUTPUT_LO1:
		case	YMC_DSP_OUTPUT_LO2:
		case	YMC_DSP_OUTPUT_BT:
			mc_asoc->aeex_prm[option&0xFF].param_count	= 0;
			break;
		case	YMC_DSP_INPUT_BASE:
			mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].param_count	= 0;
			break;
		case	YMC_DSP_INPUT_EXT:
			mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT].param_count	= 0;
			break;
		default:
			dbg_info("option err(%ld)\n", option);
			return -EINVAL;
		}

		while((chunk_offset < chunksize) && (err == 0)) {
			if((err = parser_get_chunkname(&chunkname)) < 0) {
				dbg_info("get_chunkname err\n");
			}
			chunk_offset	+= PARSER_CHUNKSIZE_NAME;

			if(chunkname.ul == chunk_aeexprog.ul) {	/*	PROG	*/
				if((err = parser_get_chunksize(&chunksize_prog)) < 0) {
					break;
				}
				if(chunksize_prog > AE_EX_PROGRAM_SIZE) {
					err	= -EINVAL;
					break;
				}
				chunk_offset	+= PARSER_CHUNKSIZE_DATASIZE;

				if((err = parser_get_data(prog, chunksize_prog)) < 0) {
					break;
				}
				chunk_offset	+= chunksize_prog;

				switch(option&0xFF) {
				case	YMC_DSP_OUTPUT_BASE:
				case	YMC_DSP_OUTPUT_SP:
				case	YMC_DSP_OUTPUT_RC:
				case	YMC_DSP_OUTPUT_HP:
				case	YMC_DSP_OUTPUT_LO1:
				case	YMC_DSP_OUTPUT_LO2:
				case	YMC_DSP_OUTPUT_BT:
					mutex_lock(&mc_asoc->mutex);
					memcpy(mc_asoc->aeex_prm[option&0xFF].prog, prog, chunksize_prog);
					mc_asoc->aeex_prm[option&0xFF].size	= chunksize_prog;
					mutex_unlock(&mc_asoc->mutex);
					dbg_info("aeex_prm[%ld].prog=\n", option&0xFF);
					p = str;
					for(i = 0; i < mc_asoc->aeex_prm[option&0xFF].size; i++) {
						p += sprintf(p, "%02x ", mc_asoc->aeex_prm[option&0xFF].prog[i]);
						if((i%16) == 15) {
							dbg_info("%s\n", str);
							p = str;
						}
					}
					if(p != str) {
						dbg_info("%s\n", str);
					}
					break;

				case	YMC_DSP_INPUT_BASE:
					mutex_lock(&mc_asoc->mutex);
					memcpy(mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].prog, prog, chunksize_prog);
					mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].size	= chunksize_prog;
					mutex_unlock(&mc_asoc->mutex);
					dbg_info("aeex_prm[%d].prog=\n", MC_ASOC_DSP_INPUT_BASE);
					p = str;
					for(i = 0; i < mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].size; i++) {
						p += sprintf(p, "%02x ", mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].prog[i]);
						if((i%16) == 15) {
							dbg_info("%s\n", str);
							p = str;
						}
					}
					if(p != str) {
						dbg_info("%s\n", str);
					}
					break;
				case	YMC_DSP_INPUT_EXT:
					mutex_lock(&mc_asoc->mutex);
					memcpy(mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT].prog, prog, chunksize_prog);
					mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT].size	= chunksize_prog;
					mutex_unlock(&mc_asoc->mutex);
					dbg_info("aeex_prm[%d].prog=\n", MC_ASOC_DSP_INPUT_EXT);
					p = str;
					for(i = 0; i < mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT].size; i++) {
						p += sprintf(p, "%02x ", mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT].prog[i]);
						if((i%16) == 15) {
							dbg_info("%s\n", str);
							p = str;
						}
					}
					if(p != str) {
						dbg_info("%s\n", str);
					}
					break;

				default:
					dbg_info("option err(%ld)\n", option);
					err	= -EINVAL;
					break;
				}
				exist_prog	= 1;
			}
			else if(chunkname.ul == chunk_aeexparm.ul) {
				if((err = parser_get_chunksize(&chunksize_parm)) < 0) {
					break;
				}
				if((chunksize_parm % sizeof(MCDRV_EXPARAM)) != 0) {
					err	= -EINVAL;
					break;
				}
				chunk_offset	+= PARSER_CHUNKSIZE_DATASIZE;

				while(chunksize_parm >= sizeof(MCDRV_EXPARAM) && (err == 0)) {
					if((err = parser_get_byte(&stExParam.bCommand)) != 0) {
						dbg_info("get_byte err\n");
						break;
					}
					if(stExParam.bCommand < 0x3F || stExParam.bCommand > 0x60) {
						err	= -EINVAL;
						break;
					}
					if((err = parser_get_data(&stExParam.abParam[0], sizeof(stExParam.abParam))) != 0) {
						dbg_info("get_byte err\n");
						break;
					}

					switch(option&0xFF) {
					case	YMC_DSP_OUTPUT_BASE:
					case	YMC_DSP_OUTPUT_SP:
					case	YMC_DSP_OUTPUT_RC:
					case	YMC_DSP_OUTPUT_HP:
					case	YMC_DSP_OUTPUT_LO1:
					case	YMC_DSP_OUTPUT_LO2:
					case	YMC_DSP_OUTPUT_BT:
						err	= store_aeex_param(mc_asoc, &mc_asoc->aeex_prm[option&0xFF], stExParam);
						break;

					case	YMC_DSP_INPUT_BASE:
						err	= store_aeex_param(mc_asoc, &mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE], stExParam);
						break;
					case	YMC_DSP_INPUT_EXT:
						err	= store_aeex_param(mc_asoc, &mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT], stExParam);
						break;

					default:
						dbg_info("option err(%ld)\n", option);
						err	= -EINVAL;
						break;
					}
					chunksize_parm	-= sizeof(MCDRV_EXPARAM);
					chunk_offset	+= sizeof(MCDRV_EXPARAM);
				}
			}
			else {
				dbg_info("parse_AENG_AEEX:unknown chankname=%c%c%c%c\n",
					chunkname.c[0], chunkname.c[1], chunkname.c[2], chunkname.c[3]);
				err	= -EINVAL;
			}
		}
		if(exist_prog == 0) {
			err	= -EINVAL;
		}
	}
	else {
		switch(option&0xFF) {
		case	YMC_DSP_OUTPUT_BASE:
		case	YMC_DSP_OUTPUT_SP:
		case	YMC_DSP_OUTPUT_RC:
		case	YMC_DSP_OUTPUT_HP:
		case	YMC_DSP_OUTPUT_LO1:
		case	YMC_DSP_OUTPUT_LO2:
		case	YMC_DSP_OUTPUT_BT:
			mc_asoc->aeex_prm[option&0xFF].size	= 0;
			break;

		case	YMC_DSP_INPUT_BASE:
			mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].size	= 0;
			break;
		case	YMC_DSP_INPUT_EXT:
			mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT].size	= 0;
			break;

		default:
			dbg_info("option err(%ld)\n", option);
			err	= -EINVAL;
			break;
		}
		if((err = parser_skip(chunksize-4)) < 0) {
			dbg_info("skip err\n");
		}
	}

	if(err < 0) {
		return err;
	}
	return chunksize+8;
}

static int	parse_AENG(
	struct mc_asoc_data	*mc_asoc,
	UINT32				option,
	UINT8				hwid,
	UINT8				block_onoff[MC_ASOC_N_DSP_BLOCK]
)
{
	int		i;
	int		err	= 0;
	int		ret	= 0;
	UINT8	onoff;
	parser_chunkname	chunkname;
	UINT32	chunksize, prmchunksize;
	UINT8	*abParam[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT];

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}

	if((err = parser_get_byte(&onoff)) != 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	switch(option&0xFF) {
	case	YMC_DSP_OUTPUT_BASE:
	case	YMC_DSP_OUTPUT_SP:
	case	YMC_DSP_OUTPUT_RC:
	case	YMC_DSP_OUTPUT_HP:
	case	YMC_DSP_OUTPUT_LO1:
	case	YMC_DSP_OUTPUT_LO2:
	case	YMC_DSP_OUTPUT_BT:
		mc_asoc->ae_onoff[option&0xFF]	= onoff;
		break;
	case	YMC_DSP_INPUT_BASE:
		mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_BASE]	= onoff;
		break;
	case	YMC_DSP_INPUT_EXT:
		mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_EXT]	= onoff;
		break;
	default:
		dbg_info("option err(%ld)\n", option);
		return -EINVAL;
	}
	dbg_info("onoff=0x%02X\n", onoff);
	if(onoff == MC_ASOC_AENG_OFF) {
		if((err = parser_skip(chunksize-1)) < 0) {
			dbg_info("skip err\n");
		}
		return err;
	}

	if((err = parser_skip(3)) < 0) {
		dbg_info("skip err\n");
		return err;
	}
	chunksize -= 4;

	while((chunksize > 0) && (err == 0)) {
		if((err = parser_get_chunkname(&chunkname)) < 0) {
			dbg_info("get_chunkname err\n");
		}
		if(chunkname.ul == chunk_bex.ul) {
			for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
				abParam[i]	= mc_asoc->ae_info[i].abBex;
			}
			ret	= parse_AENG_PRM(mc_asoc, option, BEX_PARAM_SIZE, abParam, &block_onoff[MC_ASOC_DSP_BLOCK_BEX]);
			if(ret < 0) {
				err	= ret;
			}
			else {
				chunksize	-= ret;
			}
		}
		else if(chunkname.ul == chunk_wide.ul) {
			for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
				abParam[i]	= mc_asoc->ae_info[i].abWide;
			}
			ret	= parse_AENG_PRM(mc_asoc, option, WIDE_PARAM_SIZE, abParam, &block_onoff[MC_ASOC_DSP_BLOCK_WIDE]);
			if(ret < 0) {
				err	= ret;
			}
			else {
				chunksize	-= ret;
			}
		}
		else if(chunkname.ul == chunk_drc.ul) {
			if(hwid == MC_ASOC_MC3_HW_ID) {
				for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
					abParam[i]	= mc_asoc->ae_info[i].abDrc2;
				}
				ret	= parse_AENG_PRM(mc_asoc, option, DRC2_PARAM_SIZE, abParam, &block_onoff[MC_ASOC_DSP_BLOCK_DRC]);
			}
			else if(hwid == MC_ASOC_MC1_HW_ID) {
				if((err = parser_get_chunksize(&prmchunksize)) < 0) {
					break;
				}
				if((err = parser_skip(prmchunksize)) < 0) {
					dbg_info("skip err\n");
					break;
				}
				ret	= prmchunksize+PARSER_CHUNKSIZE_NAME+PARSER_CHUNKSIZE_DATASIZE;
			}
			else {
				for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
					abParam[i]	= mc_asoc->ae_info[i].abDrc;
				}
				ret	= parse_AENG_PRM(mc_asoc, option, DRC_PARAM_SIZE, abParam, &block_onoff[MC_ASOC_DSP_BLOCK_DRC]);
			}
			if(ret < 0) {
				err	= ret;
			}
			else {
				chunksize	-= ret;
			}
		}
		else if(chunkname.ul == chunk_eq5.ul) {
			for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
				abParam[i]	= mc_asoc->ae_info[i].abEq5;
			}
			ret	= parse_AENG_PRM(mc_asoc, option, EQ5_PARAM_SIZE, abParam, &block_onoff[MC_ASOC_DSP_BLOCK_EQ5]);
			if(ret < 0) {
				err	= ret;
			}
			else {
				chunksize	-= ret;
			}
		}
		else if(chunkname.ul == chunk_eq3.ul) {
			for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
				abParam[i]	= mc_asoc->ae_info[i].abEq3;
			}
			ret	= parse_AENG_PRM(mc_asoc, option, EQ3_PARAM_SIZE, abParam, &block_onoff[MC_ASOC_DSP_BLOCK_EQ3]);
			if(ret < 0) {
				err	= ret;
			}
			else {
				chunksize	-= ret;
			}
		}
		else if(chunkname.ul == chunk_aeex.ul) {
			if((hwid != MC_ASOC_MC3_HW_ID) && (hwid != MC_ASOC_MA2_HW_ID)) {
				return -EINVAL;
			}
			ret	= parse_AENG_AEEX(mc_asoc, option, &block_onoff[MC_ASOC_DSP_BLOCK_EXTENSION]);
			if(ret < 0) {
				err	= ret;
			}
			else {
				chunksize	-= ret;
			}
		}
		else {
			dbg_info("chunksize=%ld", chunksize);
			dbg_info("unknown chankname=%c%c%c%c\n",
				chunkname.c[0], chunkname.c[1], chunkname.c[2], chunkname.c[3]);
			err	= -EINVAL;
		}
	}

	return err;
}

static int	store_cdsp_param(
	struct mc_asoc_data	*mc_asoc,
	MC_DSP_VOICECALL	*dsp_voicecall,
	int				index,
	MCDRV_CDSPPARAM	stParam)
{
	int		i;
	char	str[128], *p;

	TRACE_FUNC();

	if(dsp_voicecall->param_count[index] >= MC_ASOC_MAX_CDSPPARAM) {
		dbg_info("The setting range of the parameter was exceeded. \n");
		return -EINVAL;
	}

	mutex_lock(&mc_asoc->mutex);

	dsp_voicecall->cdspparam[index][dsp_voicecall->param_count[index]].bCommand	= stParam.bCommand;
	memcpy(dsp_voicecall->cdspparam[index][dsp_voicecall->param_count[index]].abParam,
			stParam.abParam,
			sizeof(stParam.abParam));
	dsp_voicecall->param_count[index]++;

	dbg_info("Command=0x%02x\n",
			dsp_voicecall->cdspparam[index][dsp_voicecall->param_count[index]-1].bCommand);
	p = str;
	p += sprintf(p, "Param=");
	for(i = 0; i < sizeof(stParam.abParam); i++) {
		p += sprintf(p, "%02x ", dsp_voicecall->cdspparam[index][dsp_voicecall->param_count[index]-1].abParam[i]);
		if((i%16) == 15) {
			dbg_info("%s\n", str);
			p = str;
		}
	}
	if(p != str) {
		dbg_info("%s\n", str);
	}

	mutex_unlock(&mc_asoc->mutex);
	return 0;
}


static int	parse_HNDF(
	struct mc_asoc_data	*mc_asoc,
	UINT32				option
)
{
	int		i;
	int		err	= 0;
	UINT8	data;
	UINT8	index;
	parser_chunkname	chunkname;
	UINT32	chunksize, chunksize_parm;
	MCDRV_CDSPPARAM	stCdspParam;

	TRACE_FUNC();

	switch(option&0xFF00) {
	case	YMC_DSP_VOICECALL_BASE_1MIC:
	case	YMC_DSP_VOICECALL_BASE_2MIC:
	case	YMC_DSP_VOICECALL_SP_1MIC:
	case	YMC_DSP_VOICECALL_SP_2MIC:
	case	YMC_DSP_VOICECALL_RC_1MIC:
	case	YMC_DSP_VOICECALL_RC_2MIC:
	case	YMC_DSP_VOICECALL_HP_1MIC:
	case	YMC_DSP_VOICECALL_HP_2MIC:
	case	YMC_DSP_VOICECALL_LO1_1MIC:
	case	YMC_DSP_VOICECALL_LO1_2MIC:
	case	YMC_DSP_VOICECALL_LO2_1MIC:
	case	YMC_DSP_VOICECALL_LO2_2MIC:
	case	YMC_DSP_VOICECALL_HEADSET:
	case	YMC_DSP_VOICECALL_HEADBT:
		index	= ((option&0xFF00)>>8) - 1;
		break;
	default:
		dbg_info("option err(0x%04lX)\n", option);
		return -EINVAL;
		break;
	}

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize < 4) {
		dbg_info("chunksize err\n");
		return -EINVAL;
	}
	if((err = parser_get_byte(&data)) != 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	mc_asoc->dsp_voicecall.fw_ver_major[index]	= data;
	dbg_info("major ver=0x%02X\n", data);

	if((err = parser_get_byte(&data)) != 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	mc_asoc->dsp_voicecall.fw_ver_minor[index]	= data;
	dbg_info("minor ver=0x%02X\n", data);

	if((err = parser_get_byte(&data)) != 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	mc_asoc->dsp_voicecall.onoff[index]	= data;
	dbg_info("onoff=0x%02X\n", data);

	if((err = parser_skip(1)) < 0) {
		dbg_info("skip err\n");
		return err;
	}

	mc_asoc->dsp_voicecall.param_count[index]	= 0;

	if(chunksize > 4) {
		/*	PARM	*/
		if((err = parser_get_chunkname(&chunkname)) != 0) {
			return err;
		}
		if(chunkname.ul != chunk_parm.ul) {
			dbg_info("chankname=%lX\n", chunkname.ul);
			return -EINVAL;
		}
		if((err = parser_get_chunksize(&chunksize_parm)) < 0) {
			return err;
		}
		while(chunksize_parm >= sizeof(MCDRV_CDSPPARAM)) {
			if((err = parser_get_byte(&stCdspParam.bCommand)) != 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			for(i = 0; i < sizeof(stCdspParam.abParam); i++) {
				if((err = parser_get_byte(&stCdspParam.abParam[i])) != 0) {
					dbg_info("get_byte err\n");
					return err;
				}
			}
			if((err	= store_cdsp_param(mc_asoc, &mc_asoc->dsp_voicecall, index, stCdspParam)) < 0) {
				return err;
			}
			chunksize_parm	-= sizeof(MCDRV_CDSPPARAM);
		}
	}

	return err;
}

static int	parse_DNG(void)
{
	int		i;
	int		err	= 0;
	UINT32	chunksize;
	MCDRV_DNG_INFO	stDngInfo;
	UINT32	prm	= 0;

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize > 0) {
		if((err = parser_get_dword(&prm)) < 0) {
			dbg_info("get_dword err\n");
			return err;
		}
		dbg_info("prm=0x%02lX\n", prm);
		chunksize	-= 4;
	}

	for(i = 0; i < DNG_ITEM_NUM && chunksize > 0; i++) {
		if((err = parser_get_byte(&stDngInfo.abOnOff[i])) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize--;
		if(chunksize > 0) {
			if((err = parser_get_byte(&stDngInfo.abThreshold[i])) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			chunksize--;
		}
		if(chunksize > 0) {
			if((err = parser_get_byte(&stDngInfo.abHold[i])) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			chunksize--;
		}
		if(chunksize > 0) {
			if((err = parser_get_byte(&stDngInfo.abAttack[i])) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			chunksize--;
		}
		if(chunksize > 0) {
			if((err = parser_get_byte(&stDngInfo.abRelease[i])) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			chunksize--;
		}
		if(chunksize > 0) {
			if((err = parser_get_byte(&stDngInfo.abTarget[i])) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			chunksize--;
		}
	}
	if(chunksize > 0) {
		if((err = parser_get_byte(&stDngInfo.bFw)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize--;
	}
	if(chunksize > 0) {
		dbg_info("chunksize=%ld\n", chunksize);
		return -EINVAL;
	}

	if(prm != 0) {
		err	= _McDrv_Ctrl(MCDRV_SET_DNG, (void *)&stDngInfo, prm);
		err	= mc_asoc_map_drv_error(err);
	}
	return err;
}

static int	parse_DTMF(
	MCDRV_DTMF_INFO	*dtmf_store
)
{
	int		err	= 0;
	UINT32	chunksize;
	UINT32	prm	= 0;

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize == 0) {
		return 0;
	}

	if(chunksize < sizeof(prm)) {
		dbg_info("chunksize=%ld\n", chunksize);
		return -EINVAL;
	}
	if((err = parser_get_dword(&prm)) < 0) {
		dbg_info("get_dword err\n");
		return err;
	}
	dbg_info("prm=0x%02lX\n", prm);
	if(prm == 0) {
		return 0;
	}
	chunksize-=sizeof(prm);

	if(chunksize < sizeof(dtmf_store->bSinGenHost)) {
		dbg_info("chunksize err\n");
		return -EINVAL;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_DTMF, (void *)dtmf_store, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}

	if((err = parser_get_byte(&dtmf_store->bSinGenHost)) < 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	chunksize-=sizeof(dtmf_store->bSinGenHost);
	if(chunksize > 0) {
		if((err = parser_get_word(&dtmf_store->sParam.wSinGen0Freq)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize-=sizeof(dtmf_store->sParam.wSinGen0Freq);
	}
	if(chunksize > 0) {
		if((err = parser_get_word(&dtmf_store->sParam.wSinGen1Freq)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize-=sizeof(dtmf_store->sParam.wSinGen1Freq);
	}
	if(chunksize > 0) {
		if((err = parser_get_word(&dtmf_store->sParam.swSinGen0Vol)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize-=sizeof(dtmf_store->sParam.swSinGen0Vol);
	}
	if(chunksize > 0) {
		if((err = parser_get_word(&dtmf_store->sParam.swSinGen1Vol)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize-=sizeof(dtmf_store->sParam.swSinGen1Vol);
	}
	if(chunksize > 0) {
		if((err = parser_get_byte(&dtmf_store->sParam.bSinGenGate)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize-=sizeof(dtmf_store->sParam.bSinGenGate);
	}
	if(chunksize > 0) {
		if((err = parser_get_byte(&dtmf_store->sParam.bSinGenLoop)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		chunksize-=sizeof(dtmf_store->sParam.bSinGenLoop);
	}

	if(chunksize > 0) {
		dbg_info("chunksize=%ld\n", chunksize);
		return -EINVAL;
	}

	if(prm != 0) {
		err	= _McDrv_Ctrl(MCDRV_SET_DTMF, (void *)dtmf_store, prm);
		err	= mc_asoc_map_drv_error(err);
	}
	return err;
}

static int	parse_AGC(
	UINT8	hwid
)
{
	int		err	= 0;
	UINT32	chunksize;
	MCDRV_ADC_INFO		stAdcInfo;
	MCDRV_ADC_EX_INFO	stAdcExInfo;
	MCDRV_PDM_INFO		stPdmInfo;
	MCDRV_PDM_EX_INFO	stPdmExInfo;
	UINT32	prm	= 0;
	UINT32	prm_adc;
	UINT32	prm_pdm;
	UINT32	prm_ex;

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize == 0) {
		return 0;
	}

	if(chunksize < sizeof(prm)) {
		dbg_info("chunksize=%ld\n", chunksize);
		return -EINVAL;
	}
	if((err = parser_get_dword(&prm)) < 0) {
		dbg_info("get_dword err\n");
		return err;
	}
	dbg_info("prm=0x%02lX\n", prm);
	if(prm == 0) {
		return 0;
	}
	chunksize-=sizeof(prm);

	if(chunksize == 0) {
		dbg_info("chunksize err\n");
		return -EINVAL;
	}

	prm_adc	= prm & 0x03;
	prm_pdm	= (prm & 0x03) << 1;
	if((hwid == MC_ASOC_MC3_HW_ID) || (hwid == MC_ASOC_MA2_HW_ID)) {
		prm_ex	= prm >> 2;
	}
	else {
		prm_ex	= 0;
	}

	if((err = parser_get_byte(&stAdcInfo.bAgcAdjust)) < 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	stPdmInfo.bAgcAdjust	= stAdcInfo.bAgcAdjust;
	chunksize-=sizeof(stAdcInfo.bAgcAdjust);

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcInfo.bAgcOn)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmInfo.bAgcOn	= stAdcInfo.bAgcOn;
		chunksize-=sizeof(stAdcInfo.bAgcOn);
	}

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcExInfo.bAgcEnv)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmExInfo.bAgcEnv	= stAdcExInfo.bAgcEnv;
		chunksize-=sizeof(stAdcExInfo.bAgcEnv);
	}

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcExInfo.bAgcLvl)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmExInfo.bAgcLvl	= stAdcExInfo.bAgcLvl;
		chunksize-=sizeof(stAdcExInfo.bAgcLvl);
	}

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcExInfo.bAgcUpTim)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmExInfo.bAgcUpTim	= stAdcExInfo.bAgcUpTim;
		chunksize-=sizeof(stAdcExInfo.bAgcUpTim);
	}

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcExInfo.bAgcDwTim)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmExInfo.bAgcDwTim	= stAdcExInfo.bAgcDwTim;
		chunksize-=sizeof(stAdcExInfo.bAgcDwTim);
	}

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcExInfo.bAgcCutLvl)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmExInfo.bAgcCutLvl	= stAdcExInfo.bAgcCutLvl;
		chunksize-=sizeof(stAdcExInfo.bAgcCutLvl);
	}

	if(chunksize > 0) {
		if((err = parser_get_byte(&stAdcExInfo.bAgcCutTim)) < 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		stPdmExInfo.bAgcCutTim	= stAdcExInfo.bAgcCutTim;
		chunksize-=sizeof(stAdcExInfo.bAgcCutTim);
	}

	if(chunksize > 0) {
		dbg_info("chunksize=%ld\n", chunksize);
		return -EINVAL;
	}

	if(prm != 0) {
		if(prm_adc != 0) {
			if((err = _McDrv_Ctrl(MCDRV_SET_ADC, (void *)&stAdcInfo, prm_adc)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
		}
		if(prm_pdm != 0) {
			if((err = _McDrv_Ctrl(MCDRV_SET_PDM, (void *)&stPdmInfo, prm_pdm)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
		}
		if(prm_ex != 0) {
			if((err = _McDrv_Ctrl(MCDRV_SET_ADC_EX, (void *)&stAdcExInfo, prm_ex)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			if((err = _McDrv_Ctrl(MCDRV_SET_PDM_EX, (void *)&stPdmExInfo, prm_ex)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
		}
	}
	return err;
}

static int	parse_ANDR(
	UINT16	*dsp_onoff,
	UINT16	*vendordata,
	UINT8	block_onoff[MC_ASOC_N_DSP_BLOCK])
{
	int		i;
	int		err	= 0;
	UINT8	data;
	parser_chunkname	chunkname;
	UINT32	chunksize, chunksize_aeg;
	char	str[128], *p;

	TRACE_FUNC();

	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if(chunksize > 0) {
		if((err = parser_get_byte(&data)) != 0) {
			dbg_info("get_byte err\n");
			return err;
		}
		dbg_info("ver=0x%02X\n", data);
	}
	if(chunksize > 1) {
		if((err = parser_skip(3)) != 0) {
			dbg_info("skip err\n");
			return err;
		}
	}

	if(chunksize > 4) {
		/*	AAEG	*/
		if((err = parser_get_chunkname(&chunkname)) != 0) {
			return err;
		}
		if(chunkname.ul != chunk_android_aeg.ul) {
			dbg_info("chankname=%lX\n", chunkname.ul);
			return -EINVAL;
		}
		if((err = parser_get_chunksize(&chunksize_aeg)) < 0) {
			return err;
		}
		if(chunksize != (chunksize_aeg+8+4)) {
			dbg_info("chunksize err\n");
			return -EINVAL;
		}
		if(chunksize_aeg > 0) {
			if((err = parser_get_word(dsp_onoff)) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			dbg_info("dsp_onoff=0x%02X\n", *dsp_onoff);
		}
		if(chunksize_aeg > 2) {
			if((err = parser_get_word(vendordata)) < 0) {
				dbg_info("get_byte err\n");
				return err;
			}
			dbg_info("vendordata=0x%02X\n", *vendordata);
		}
		if(chunksize_aeg > 4) {
			if((chunksize_aeg-4) > MC_ASOC_N_DSP_BLOCK) {
				return -EINVAL;
			}
			p = str;
			p += sprintf(p, "block_onoff=");
			for(i = 0; i < (chunksize_aeg-4); i++) {
				if((err = parser_get_byte(&block_onoff[i])) < 0) {
					dbg_info("get_byte err\n");
					return err;
				}
				p += sprintf(p, "%d ", block_onoff[i]);
			}
			dbg_info("%s\n", str);
		}
	}

	return err;
}

int	mc_asoc_parser(
	struct mc_asoc_data	*mc_asoc,
	UINT8	hwid,
	void	*param,
	UINT32	size,
	UINT32	option)
{
	int		i;
	int		err	= 0;
	parser_chunkname	chunkname;
	UINT32	chunksize;
	UINT8	data;
	UINT8	block_onoff[2][MC_ASOC_N_DSP_BLOCK];
	char	str[128], *p;

	if((mc_asoc == NULL) || (param == NULL)) {
		return -EINVAL;
	}

	parser_init(param, size);
	if((err = parser_get_chunkname(&chunkname)) < 0) {
		return err;
	}
	if(chunkname.ul != chunk_root.ul) {
		dbg_info("chankname=%lX\n", chunkname.ul);
		return -EINVAL;
	}
	if((err = parser_get_chunksize(&chunksize)) < 0) {
		return err;
	}
	if((chunksize+8) != size) {
		dbg_info("all chunksize err(size=%ld)\n", size);
		return -EINVAL;
	}
	if((err = parser_get_byte(&data)) < 0) {
		dbg_info("get_byte err\n");
		return err;
	}
	if(chunksize > 1) {
		if((err = parser_skip(3)) < 0) {
			dbg_info("skip err\n");
			return err;
		}
	}

	if((err = check_DEV(hwid, chunksize-4)) < 0) {
		return err;
	}

	memset(block_onoff[0], 0xFF, MC_ASOC_N_DSP_BLOCK);
	memset(block_onoff[1], 0xFF, MC_ASOC_N_DSP_BLOCK);

	while(err == 0) {
		if((err = parser_get_chunkname(&chunkname)) < 0) {
			dbg_info("get_chunkname err\n");
		}
		if(chunkname.ul == 0) {
			/*	data end	*/
			break;
		}
		else if(chunkname.ul == chunk_dev.ul) {
			/*	'DEV_'	*/
			err	= parser_skip(8);
		}
		else if(chunkname.ul == chunk_aeng.ul) {
			/*	'AENG'	*/
			err	= parse_AENG(mc_asoc, option, hwid, block_onoff[0]);
		}
		else if(chunkname.ul == chunk_hndf.ul) {
			/*	'HNDF'	*/
			if((hwid == MC_ASOC_MC3_HW_ID) || (hwid == MC_ASOC_MA2_HW_ID)) {
				err	= parse_HNDF(mc_asoc, option);
			}
			else {
				err	= -EINVAL;
			}
		}
		else if(chunkname.ul == chunk_dng.ul) {
			/*	'DNG_'	*/
			err	= parse_DNG();
		}
		else if(chunkname.ul == chunk_dtmf.ul) {
			/*	'DTMF'	*/
			if((hwid == MC_ASOC_MC3_HW_ID) || (hwid == MC_ASOC_MA2_HW_ID)) {
				err	= parse_DTMF(&mc_asoc->dtmf_store);
			}
			else {
				err	= -EINVAL;
			}
		}
		else if(chunkname.ul == chunk_agc.ul) {
			/*	'AGC_'	*/
			err	= parse_AGC(hwid);
		}
		else if(chunkname.ul == chunk_android.ul) {
			/*	'ANDR'	*/
			switch(option&0xFF) {
			case	YMC_DSP_OUTPUT_BASE:
			case	YMC_DSP_OUTPUT_SP:
			case	YMC_DSP_OUTPUT_RC:
			case	YMC_DSP_OUTPUT_HP:
			case	YMC_DSP_OUTPUT_LO1:
			case	YMC_DSP_OUTPUT_LO2:
			case	YMC_DSP_OUTPUT_BT:
				err	= parse_ANDR(&mc_asoc->dsp_onoff[option&0xFF], &mc_asoc->vendordata[option&0xFF], block_onoff[1]);
				break;
			case	YMC_DSP_INPUT_BASE:
				err	= parse_ANDR(&mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE], &mc_asoc->vendordata[MC_ASOC_DSP_INPUT_BASE], block_onoff[1]);
				break;
			case	YMC_DSP_INPUT_EXT:
				err	= parse_ANDR(&mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT], &mc_asoc->vendordata[MC_ASOC_DSP_INPUT_EXT], block_onoff[1]);
				break;
			default:
				dbg_info("option err(%ld)\n", option);
				err	= -EINVAL;
				break;
			}
		}
		else {
			dbg_info("unknown chankname=%c%c%c%c\n",
				chunkname.c[0], chunkname.c[1], chunkname.c[2], chunkname.c[3]);
			err	= -EINVAL;
		}
	}

	if(err == 0) {
		switch(option&0xFF) {
		case	YMC_DSP_OUTPUT_BASE:
		case	YMC_DSP_OUTPUT_SP:
		case	YMC_DSP_OUTPUT_RC:
		case	YMC_DSP_OUTPUT_HP:
		case	YMC_DSP_OUTPUT_LO1:
		case	YMC_DSP_OUTPUT_LO2:
		case	YMC_DSP_OUTPUT_BT:
			p = str;
			p+= sprintf(p, "block_onoff[%ld]= ", option&0xFF);
			for(i = 0; i < MC_ASOC_N_DSP_BLOCK; i++) {
				if(block_onoff[1][i] == 0) {
					mc_asoc->block_onoff[option&0xFF][i]	= 0;
				}
				else if(block_onoff[1][i] == 2) {
					mc_asoc->block_onoff[option&0xFF][i]	= 2;
				}
				else {
					if((block_onoff[1][i] == 0xFF) && (block_onoff[0][i] == 0xFF)) {
					}
					else if(block_onoff[0][i] == 1) {
						mc_asoc->block_onoff[option&0xFF][i]	= 1;
					}
					else {
						mc_asoc->block_onoff[option&0xFF][i]	= 0;
					}
				}

				p += sprintf(p, "%d ", mc_asoc->block_onoff[option&0xFF][i]);
				if((i%16) == 15) {
					dbg_info("%s\n", str);
					p = str;
				}
			}
			if(p != str) {
				dbg_info("%s\n", str);
			}
			break;
		case	YMC_DSP_INPUT_BASE:
			p = str;
			p+= sprintf(p, "block_onoff[%d]= ", MC_ASOC_DSP_INPUT_BASE);
			for(i = 0; i < MC_ASOC_N_DSP_BLOCK; i++) {
				if(block_onoff[1][i] == 0) {
					mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][i]	= 0;
				}
				else if(block_onoff[1][i] == 2) {
					mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][i]	= 2;
				}
				else {
					if((block_onoff[1][i] == 0xFF) && (block_onoff[0][i] == 0xFF)) {
					}
					else if(block_onoff[0][i] == 1) {
						mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][i]	= 1;
					}
					else {
						mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][i]	= 0;
					}
				}

				p += sprintf(p, "%d ", mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][i]);
				if((i%16) == 15) {
					dbg_info("%s\n", str);
					p = str;
				}
			}
			if(p != str) {
				dbg_info("%s\n", str);
			}
			break;
		case	YMC_DSP_INPUT_EXT:
			p = str;
			p+= sprintf(p, "block_onoff[%d]= ", MC_ASOC_DSP_INPUT_EXT);
			for(i = 0; i < MC_ASOC_N_DSP_BLOCK; i++) {
				if(block_onoff[1][i] == 0) {
					mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][i]	= 0;
				}
				else if(block_onoff[1][i] == 2) {
					mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][i]	= 2;
				}
				else {
					if((block_onoff[1][i] == 0xFF) && (block_onoff[0][i] == 0xFF)) {
					}
					else if(block_onoff[0][i] == 1) {
						mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][i]	= 1;
					}
					else {
						mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][i]	= 0;
					}
				}

				p += sprintf(p, "%d ", mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][i]);
				if((i%16) == 15) {
					dbg_info("%s\n", str);
					p = str;
				}
			}
			if(p != str) {
				dbg_info("%s\n", str);
			}
			break;
		default:
			dbg_info("option err(%ld)\n", option);
			err	= -EINVAL;
			break;
		}
	}

	return err;
}

