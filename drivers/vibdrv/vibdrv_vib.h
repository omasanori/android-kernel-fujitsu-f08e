/*
  * Copyright(C) 2013 FUJITSU LIMITED
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


#ifndef _VIBDRV_VIB_H_
#define _VIBDRV_VIB_H_

/* haptics firmware */
#define HAPTICS_FIRMWARE_POS        0x04

/* haptics waveform */
#define HAPTICS_ACCESS_TABLE_POS    0x28
#define HAPTICS_ACCESS_TABLE_ID     0x08
#define HAPTICS_ACCESS_VIB_ADR      0x0000
#define HAPTICS_ACCESS_HP1_ADR      0x0080
#define HAPTICS_ACCESS_HP2_ADR      0x0100
#define HAPTICS_ACCESS_VER_ADR      0x03FF
#define HAPTICS_ACCESS_MARK_UDATA   0x55
#define HAPTICS_ACCESS_MARK_DDATA   0xAA

/* GET_VER command transfer */
#define VIBDRV_GET_VER_CMD_USIZE    0x00
#define VIBDRV_GET_VER_CMD_DSIZE    0x04
#define VIBDRV_GET_VER_CMD          0x14
#define VIBDRV_GET_VER_CMD_SUM      0x18

/* WRITE command transfer */
#define VIBDRV_WRITE_CMD_USIZE      0x00
#define VIBDRV_WRITE_CMD_DSIZE      0x05
#define VIBDRV_WRITE_CMD            0x05
#define VIBDRV_WRITE_RETRY          0x03

/* DATA command transfer */
#define VIBDRV_DATA_CMD_USIZE       0x00
#define VIBDRV_DATA_CMD_DSIZE       0x04
#define VIBDRV_DATA_CMD             0x06
#define VIBDRV_DATA_CMD_SUM         0x0A

/* DATA  transfer */
#define VIBDRV_DATA_TRANS_USIZE     0x04
#define VIBDRV_DATA_TRANS_DSIZE     0x03
#define VIBDRV_DATA_TRANS_MAX       0x400

/* VERIFY command transfer */
#define VIBDRV_VERIFY_CMD_USIZE     0x00
#define VIBDRV_VERIFY_CMD_DSIZE     0x05
#define VIBDRV_VERIFY_CMD           0x0B

/* READ command transfer */
#define VIBDRV_READ_CMD_USIZE       0x00
#define VIBDRV_READ_CMD_DSIZE       0x06
#define VIBDRV_READ_CMD             0x0A
#define VIBDRV_READ_PAGE_MAX        8

/* READ DATA transfer */
#define VIBDRV_READ_DATA_TRANS_USIZE 0x00
#define VIBDRV_READ_DATA_TRANS_DSIZE 0x83
#define VIBDRV_READ_DATA_TRANS_MAX   0x80

/* BOOTSWAP transfer */
#define VIBDRV_BOOTSWAP_USIZE       0x00
#define VIBDRV_BOOTSWAP_DSIZE       0x04
#define VIBDRV_BOOTSWAP_CMD         0x08
#define VIBDRV_BOOTSWAP_SUM         0x0C

/* SET_THEME_V transfer */
#define VIBDRV_SET_THEME_USIZE      0x00
#define VIBDRV_SET_THEME_DSIZE      0x05
#define VIBDRV_SET_THEME_CMD        0x12

/* GET_THEME_V transfer */
#define VIBDRV_GET_THEME_USIZE      0x00
#define VIBDRV_GET_THEME_DSIZE      0x04
#define VIBDRV_GET_THEME_CMD        0x13
#define VIBDRV_GET_THEME_SUM        0x17

/* GET_MODE transfer */
#define VIBDRV_GET_MODE_USIZE       0x00
#define VIBDRV_GET_MODE_DSIZE       0x04
#define VIBDRV_GET_MODE_CMD         0x15
#define VIBDRV_GET_MODE_SUM         0x19

/* SET_CAL transfer */
#define VIBDRV_SET_CAL_USIZE        0x00
#define VIBDRV_SET_CAL_DSIZE        0x05
#define VIBDRV_SET_CAL_CMD          0x16

/* GET_CAL transfer */
#define VIBDRV_GET_CAL_USIZE        0x00
#define VIBDRV_GET_CAL_DSIZE        0x04
#define VIBDRV_GET_CAL_CMD          0x17
#define VIBDRV_GET_CAL_SUM          0x1B


/* i2c command structure */
struct _command_req {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   command;
    int8_t   sum;
}__attribute__((packed));

struct _write_req {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   command;
    int8_t   data;
    int8_t   sum;
}__attribute__((packed));

struct _status {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   type;
    int8_t   status;
    int8_t   sum;
}__attribute__((packed));

struct _data_transfer {
    int8_t   ubytes;
    int8_t   dbytes;
    unsigned char     data[VIBDRV_DATA_TRANS_MAX];
    int8_t   sum;
}__attribute__((packed));

struct _read_req {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   command;
    int8_t   blockno;
    int8_t   pageno;
    int8_t   sum;
}__attribute__((packed));

struct _read_transfer {
    int8_t   ubytes;
    int8_t   dbytes;
    unsigned char     data[VIBDRV_READ_DATA_TRANS_MAX];
    int8_t   sum;
}__attribute__((packed));

struct _firm_transfer {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   cmd;
    int8_t   uver;
    int8_t   dver;
    int8_t   sum;
}__attribute__((packed));

struct _waveform_table {
    int8_t    blockno;
    int8_t    maxblockcnt;
    int8_t    trg_type;
    int8_t    id_value;
    int8_t    id_type;
}__attribute__((packed));

struct _waveform_data_table {
    int8_t trg_type;
    int8_t id_value;
    int    waveform_size;
    char   waveform_data[HAPTICS_WAVEFORM_SIZE];
};

extern int vibdrv_haptics_suspend(void);
extern int vibdrv_haptics_resume(void);
extern int vibdrv_haptics_enable_amp(void);
extern int vibdrv_haptics_disable_amp(void);
extern int vibdrv_write_firmware(char* data, int size);
extern int vibdrv_get_firmware_version(int8_t *major, int8_t *minor);
extern int vibdrv_set_int_status(int8_t in_status, int8_t *out_status);
extern int vibdrv_write_waveform(struct _waveform_data_table *waveform, int waveform_cnt);
extern int vibdrv_check_waveform(int8_t type, int8_t table_id);
extern uint8_t vibdrv_get_gpio_rst(void);
extern void vibdrv_set_state(int8_t state);
extern bool vibdrv_check_state(void);
extern int8_t vibdrv_get_state(void);
extern bool vibdrv_check_waveform_param(int8_t trg_type, int8_t id_value);

#if VIBDRV_DEBUG
extern int vibdrv_read_firmware(char* data, int size, int8_t blockno);
#endif

#endif // _VIBDRV_VIB_H_

