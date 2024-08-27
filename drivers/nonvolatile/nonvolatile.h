/*
  * Copyright(C) 2012 FUJITSU LIMITED
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License
  * as published by the Free Software Foundation; version 2
  * of the License.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#ifndef _NONVOLATILE_H
#define _NONVOLATILE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <linux/nonvolatile_common.h>


// ***********************************************************************************************

#define NONVOLATILE_PARTITION_NAME				"appsst1"
#define NONVOLATILE_BACKUP_PARTITION_NAME		"appsst2"
#define NONVOLATILE_MASTER_PARTITION_NAME		"appsst3"

// ***********************************************************************************************

#define NONVOLATILE_BOOT_MODE_PARTITION_NAME	"abost"
#define	KOUTEI_ITEM_MAX_SIZE					768

#define	MC_CMD_BOOT_MODE_ID						41046
#define	KOUTEI_BOOT_MODE_OFFSET					0
#define	KOUTEI_BOOT_MODE_SIZE					1

#define	MC_CMD_KOUTEI_MODE_ID					41000
#define	KOUTEI_MODE_OFFSET						1
#define	KOUTEI_MODE_SIZE						1

#define	MC_CMD_MASETER_CLEAR_ID					41048
#define	KOUTEI_MASETER_CLEAR_OFFSET				2
#define	KOUTEI_MASETER_CLEAR_SIZE				1

#define	KERNEL_POWER_ON_REASON_ID				40036
#define	KERNEL_POWER_ON_REASON_OFFSET			3
#define	KERNEL_POWER_ON_REASON_SIZE				4

#define	MC_CMD_FIRST_START_FLAG_ID				47155
#define	KOUTEI_FIRST_START_FLAG_OFFSET			7
#define	KOUTEI_FIRST_START_FLAG_SIZE			1

#define	MC_CMD_USBDL_BOOT_ID                    40132
#define	KOUTEI_USBDL_BOOT_OFFSET                11
#define	KOUTEI_USBDL_BOOT_SIZE                  1

#define	APNV_CAMERA_SDDL_ID                     40135
#define	APNV_CAMERA_SDDL_OFFSET                 35
#define	APNV_CAMERA_SDDL_SIZE                   1

//--

#define	KOUTEI_FUSE_BLOW_OFFSET					4096

#define	MC_CMD_FUSE_BLOW_1_ID					40032
#define	KOUTEI_FUSE_BLOW_1_OFFSET				KOUTEI_FUSE_BLOW_OFFSET
#define	KOUTEI_FUSE_BLOW_1_SIZE					768

#define	MC_CMD_FUSE_BLOW_2_ID					40033
#define	KOUTEI_FUSE_BLOW_2_OFFSET				KOUTEI_FUSE_BLOW_1_OFFSET + KOUTEI_FUSE_BLOW_1_SIZE
#define	KOUTEI_FUSE_BLOW_2_SIZE					768

#define	MC_CMD_FUSE_BLOW_3_ID					40034
#define	KOUTEI_FUSE_BLOW_3_OFFSET				KOUTEI_FUSE_BLOW_2_OFFSET + KOUTEI_FUSE_BLOW_2_SIZE
#define	KOUTEI_FUSE_BLOW_3_SIZE					768

#define	MC_CMD_FUSE_BLOW_4_ID					40035
#define	KOUTEI_FUSE_BLOW_4_OFFSET				KOUTEI_FUSE_BLOW_3_OFFSET + KOUTEI_FUSE_BLOW_3_SIZE
#define	KOUTEI_FUSE_BLOW_4_SIZE					768


// ***********************************************************************************************

struct nv_manage_area
{
	char			nv_state[4];
	unsigned int	header_size;
	unsigned int	index_size;
	unsigned int	total_item_num;
};


struct nv_item_area_info
{
	unsigned long	data_area_offset;
	int				item_num;
	unsigned long	item_total_size;
	int				reserved;
};


const unsigned int nv_item_flag_master_clear	= 1;
const unsigned int nv_item_flag_clear_type_2	= 1 << 1;
//const unsigned int nv_item_flag_clear_type_1	= 1 << 2;
//const unsigned int nv_item_flag_adjust		= 1 << 3;
//const unsigned int nv_item_flag_valiable		= 1 << 4;
const unsigned int nv_item_flag_secure			= 1 << 5;
const unsigned int nv_item_flag_active			= 1 << 6;
const unsigned int nv_item_flag_enable			= 1 << 7;
//const unsigned int nv_item_flag_early_init_flag	= 1 << 8;
const unsigned int nv_item_flag_backup			= 1 << (8 + 4);


struct nv_backup_area_info
{
	uint8_t			backup_area_state[4];
	int				item_num;
	unsigned long	item_total_size;
	unsigned long	data_area_offset;
};


struct backup_item_list
{
	unsigned int	backup_item_num;
	struct nv_item	*backup_item;
};


struct nv_item_info
{
	unsigned long	item_id;
	int				item_flag;
	unsigned long	item_offset;
	unsigned long	item_size;
};


struct nv_item
{
	struct nv_item_info	item_info;
	uint8_t				*item_data;
};


struct nv_info_area
{
	struct nv_manage_area		nv_manage_area;
	struct nv_item_area_info	item_area_info[4];
	int 						nv_crypt_info;
	int 						reserved[3];
	int 						nv_version_info[12];
};


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif	/* _NONVOLATILE_H */
