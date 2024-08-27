/*
 * fta.h - FTA (Fault Trace Assistant) 
 *         external API header
 *
 * Copyright(C) 2011 FUJITSU LIMITED
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

#ifndef _FTA_H
#define _FTA_H
#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------
 *  definition for message function
 * -------------------------------- */
/* level */
#define FTA_LOG_LEVEL_LOW                0
#define FTA_LOG_LEVEL_MED                1
#define FTA_LOG_LEVEL_HIGH               2
#define FTA_LOG_LEVEL_CRITICAL           3
#define FTA_LOG_LEVEL_NOTICE             4
#define FTA_LOG_LEVEL_WARNING            5
#define FTA_LOG_LEVEL_ERROR              6
#define FTA_LOG_LEVEL_FATAL              7
#define FTA_LOG_LEVEL_TRACE              8
#define FTA_LOG_LEVEL_VERIFY             9
#define FTA_LOG_LEVEL_WORKFLOW          10
#define FTA_LOG_LEVEL_MONITOR           11
#define FTA_LOG_LEVEL_INSPECT           12
#define FTA_LOG_LEVEL_PERFORM           13
#define FTA_LOG_LEVEL_FIELD             14
#define FTA_LOG_LEVEL_REVIEW            15

/* category */
/* 240-255 is reserved for FTA */
#define FTA_LOG_CATEGORY_ENABLED(CMD, PARMs)  CMD PARMs
#define FTA_LOG_CATEGORY_DISABLED(CMD, PARMs) /* no effect */
#define FTA_LOG_CATEGORY_FTA_DEFAULT    0
#define FTA_LOG_CATEGORY_SW_FTA_DEFAULT FTA_LOG_CATEGORY_ENABLED

/* filter */
#define FTA_LOG_SCENE_FILTER_MASK       0x80
#define FTA_LOG_SCENE_NORMAL            0
#define FTA_LOG_SCENE_DEVEL             (FTA_LOG_SCENE_FILTER_MASK)

/* info structure
 * msb|31            16|15         8|   7    |6      4|3        0|lsb
 *    |<---- line ---->|<-category->|filter  |reserve |   level  |    */
#define FTA_LOG_MESSAGE_SERIAL_I(LINE, CATEGORY, LEVEL) \
        (((LINE) << 16) \
        |(((CATEGORY) & 0x000000FF) << 8) \
        |((LEVEL) & 0xFF))
#define FTA_LOG_MESSAGE_INFO_GET_LINE(SER)  (unsigned short)((SER) >> 16)
#define FTA_LOG_MESSAGE_INFO_GET_LEVEL(SER) (unsigned short)((SER) & 0x0000FFFF)

/* default log definition */
#define FTA_LOG_PAUSE_DEFAULT_API       fta_log_message_pause_null
#define FTA_LOG_RESUME_DEFAULT_API      fta_log_message_resume_null
#define FTA_LOG_MESSAGE_DEFAULT_API     fta_log_message

/* stop reason */
typedef enum _fta_stop_reason_enum {
  FTA_STOP_REASON_NORMAL,               /* normal end   */
  FTA_STOP_REASON_ABNORMAL,             /* abnormal end */
  FTA_STOP_REASON_INTENTIONAL,          /* intentional  */
} fta_stop_reason_enum;

/* internal MACRO */
#define FTA_LOG_MESSAGE_I(API, SWITCH, CATEGORY, LEVEL, FORMAT, P1, P2, P3) \
        SWITCH(API, (__FILE__, \
            (unsigned long)FTA_LOG_MESSAGE_SERIAL_I(__LINE__, CATEGORY, LEVEL), \
            (const unsigned char *)(FORMAT), (unsigned long)(P1), (unsigned long)(P2), (unsigned long)(P3)))

/* --------------------------------
 *  API
 * -------------------------------- */
#define FTA_LOG_MESSAGE(CATEGTAG, LEVEL, FORMAT, P1, P2, P3) \
        FTA_LOG_MESSAGE_I(FTA_LOG_MESSAGE_DEFAULT_API, FTA_LOG_CATEGORY_SW_##CATEGTAG, FTA_LOG_CATEGORY_##CATEGTAG, FTA_LOG_SCENE_NORMAL|(LEVEL), FORMAT, P1, P2, P3)
#define FTA_LOG_MESSAGE_DEVEL(CATEGTAG, LEVEL, FORMAT, P1, P2, P3) \
        FTA_LOG_MESSAGE_I(FTA_LOG_MESSAGE_DEFAULT_API, FTA_LOG_CATEGORY_SW_##CATEGTAG, FTA_LOG_CATEGORY_##CATEGTAG, FTA_LOG_SCENE_DEVEL|(LEVEL), FORMAT, P1, P2, P3)
#define FTA_LOG_PAUSE()   FTA_LOG_PAUSE_DEFAULT_API()
#define FTA_LOG_RESUME()      FTA_LOG_RESUME_DEFAULT_API()

#define FTA_LOG_ISENABLED(CATEGTAG) (FTA_LOG_CATEGORY_SW_##CATEGTAG(,1+) 0)

extern void fta_initialize(void);
extern void fta_terminate(void);
extern void fta_stop(fta_stop_reason_enum reason);

#ifdef CONFIG_FTA
extern void fta_log_message(const unsigned char *srcname, unsigned long info, const unsigned char *msgformat,
                            unsigned long arg1, unsigned long arg2, unsigned long arg3);
extern void fta_log_struct_log0(const unsigned char *srcname, unsigned long info, const unsigned char *msgtitle, 
                                unsigned long addr, const void *data, unsigned short data_len,
                                const unsigned char *dataformat);

extern void ftadrv_send_str(const unsigned char *cmd);
#else
static inline void fta_log_message(const unsigned char *srcname, unsigned long info, const unsigned char *msgformat,
                            unsigned long arg1, unsigned long arg2, unsigned long arg3) { return; }
static inline void fta_log_struct_log0(const unsigned char *srcname, unsigned long info, const unsigned char *msgtitle, 
                                unsigned long addr, const void *data, unsigned short data_len,
                                const unsigned char *dataformat) { return; }

static inline void ftadrv_send_str(const unsigned char *cmd) { return; }
#endif /* CONFIG_FTA */
/* STUB */
static inline unsigned long fta_log_message_pause_null(void) { /* do nothing */ return 0xFFFFFFFF; }
static inline unsigned long fta_log_message_resume_null(void) { /* do nothing */ return 0xFFFFFFFF; }

#ifdef __cplusplus
}
#endif
#endif  /* _FTA_H */
