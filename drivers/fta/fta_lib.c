/*
 * fta_lib.c - FTA (Fault Trace Assistant) 
 *             library function
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

/* ==========================================================================
 *  INCLUDE HEADER
 * ========================================================================== */

#include "fta_i.h"


/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */

/* -------------------------------- */
typedef struct _fta_sprintf_option {
  uint32 minwidth;                    /* minimum width of the digit number  */
  uint32 precision;                   /* accuracy of the digit number       */
  boolean alignleft;                  /* align left                         */
  uint8 zerochr;                      /* 0 padded                           */
  uint8 plus_minus;                   /* signed/unsigned                    */
  uint8 hexcase;                      /* output for hexadecimal             */
} fta_sprintf_option;
/* -------------------------------- */
#define FTA_LIB_FORMAT_BUF_LIMIT        0x0000FFFF
#define FTA_LIB_MAX_STRINGLENGTH        (FTA_LIB_FORMAT_BUF_LIMIT)
#define FTA_LIB_NULL_CHANGE_TO_NULLSTR  "(null)"
#define FTA_LIB_STRING_TERM_CHAR        '\0'
#define FTA_LIB_FORMAT_PAD_CHAR         ' '
#define FTA_LIB_MIN_HEX_COLUMN          8
#define FTA_LIB_MAX_WIDTH_SPECIFY       1000
/* -------------------------------- */
static const uint32 fta_const_decimal_figure[] = {
  1L,
  10L,
  100L,
  1000L,
  10000L,
  100000L,
  1000000L,
  10000000L,
  100000000L,
  1000000000L
};
static const uint32 fta_const_decimal_figure_cnt = (sizeof(fta_const_decimal_figure)/sizeof(const uint32));
static const uint8 *fta_const_hex_digit[] = {
  "0123456789abcdef", /* lower case */
  "0123456789ABCDEF"  /* upper case */
};


/* ==========================================================================
 *  FUNCTIONS
 * ========================================================================== */

/*!
 @brief fta_lib_check_parity16

 calcurate party(16bit)

 @param [in]  check_addr  check data
 @param [in]  count       size of array
 @param [in]  start       start address

 @retval      result of calcurate
*/
uint16 fta_lib_check_parity16(uint16 *check_addr, uint32 count, uint16 start)
{
  uint32 i;
  uint16 parity = start;
  /* - - - - - - - - */
  for(i = 0; i < count; i++) {
    parity ^= check_addr[i];
  }
  /* - - - - - - - - */
  return parity;
}
EXPORT_SYMBOL(fta_lib_check_parity16);

/*!
 @brief fta_lib_snprintf

 character string deployment with a form 

 @param [in]  buf          output data
 @param [in]  len          length of output data buffer
 @param [in]  msgformat    format string
 @param [in]  argc         number of arguments
 @param [in]  ...          variable arguments

 @retval      >0:   size of output data
 @retval      -1:   over the len(but write data to buf until len)
*/
int32 fta_lib_snprintf(uint8 *buf, uint32 len, const uint8 *msgformat, uint32 argc, ...)
{
  va_list argv;
  int32 rc;

  va_start(argv, argc);
  rc = fta_lib_vsnprintf(buf, len, msgformat, argc, argv);
  va_end(argv);
  return rc;
}
EXPORT_SYMBOL(fta_lib_snprintf);

/*!
 @brief fta_lib_scprintf

 get the size of character string deployment with a form 

 @param [in]  msgformat    format string
 @param [in]  argc         number of arguments
 @param [in]  ...          variable arguments

 @retval      >0:   size of output data
 @retval      -1:   over the max length(65535)
*/
int32 fta_lib_scprintf(const uint8 *msgformat, uint32 argc, ...)
{
  int32 rc;
  va_list argv;
  va_start(argv, argc);
  rc = fta_lib_vsnprintf(NULL, 0, msgformat, argc, argv);
  va_end(argv);
  return rc;
}
EXPORT_SYMBOL(fta_lib_scprintf);

/*!
 @brief fta_lib_vsnprintf

 character string deployment with a form 

 @param [in]  buf          output data
 @param [in]  len          length of output data buffer
 @param [in]  msgformat    format string
 @param [in]  argc         number of arguments
 @param [in]  ...          variable arguments

 @retval      >0:   size of output data
 @retval      -1:   over the len(but write data to buf until len)
*/
int32 fta_lib_vsnprintf(uint8 *buf, uint32 len, const uint8 *msgformat, uint32 argc, va_list argv)
{
  uint32 chrs = 0;
  uint8 *bottom = buf + len;
  boolean bufout = ((buf != NULL) ? TRUE : FALSE);

  uint8  c;

  if(bufout && len > FTA_LIB_FORMAT_BUF_LIMIT) {
    return -1;
  }
  if(msgformat == NULL)
    return -1;

  /* - - - - - - - - */
  for(c = *msgformat; c != FTA_LIB_STRING_TERM_CHAR && (chrs < len || !bufout);) {

    if(c == '%') {

      fta_sprintf_option opt = {0};

      uint32 sel_width = 0;
      uint8 *buf_anchor = buf;
      uint32 chrs_anchor = chrs;
      boolean ignorechr = FALSE;
      boolean enabled = FALSE;
      boolean zero = FALSE;
      uint32 data; 

      uint32 loopcnt;
      uint32 chrs_count;
      uint32 numdigit;
      uint32 maskdigit;

      c = *(++msgformat);

      /* - - - - - - - - */
      for(; c != FTA_LIB_STRING_TERM_CHAR; c = *(++msgformat)) {
        if(c =='+') {
          opt.plus_minus = '+';
        } else if(c == ' ') {
          if(opt.plus_minus == 0) {
            opt.plus_minus = ' ';
          }
        } else if(c == '-') {
          opt.alignleft = TRUE;
          opt.zerochr = 0;
        } else if(c == '0') {
          if(!opt.alignleft) {
            opt.zerochr = '0';
          }
        } else {
          break;
        }
      }
      /* - - - - - - - - */
      if(c == FTA_LIB_STRING_TERM_CHAR) {
        break;
      }

      if(c == '*') {
        if(argc > 0) {
          argc--;
          opt.minwidth = va_arg(argv, uint32);
          if(opt.minwidth > FTA_LIB_MAX_WIDTH_SPECIFY) {
            opt.minwidth = FTA_LIB_MAX_WIDTH_SPECIFY;
          }
        }
        c = *(++msgformat);
      }
      else
      {
        /* - - - - - - - - */
        for(; c >= '0' && c <= '9'; c = *(++msgformat)) {
          opt.minwidth *= 10;
          opt.minwidth += (c - '0');
          if(opt.minwidth > FTA_LIB_MAX_WIDTH_SPECIFY) {
            opt.minwidth = FTA_LIB_MAX_WIDTH_SPECIFY;
          }
        }
        /* - - - - - - - - */
      }
      if(opt.minwidth == 0) {
        opt.zerochr = 0;
      }
      if(c == FTA_LIB_STRING_TERM_CHAR) {
        break;
      }

      if(c == '.')
      {
        c = *(++msgformat);
        if(c == FTA_LIB_STRING_TERM_CHAR) {
          break;
        }
        if(c == '*') {
          if(argc > 0) {
            argc--;
            opt.precision = va_arg(argv, uint32);
            if(opt.precision > FTA_LIB_MAX_WIDTH_SPECIFY) {
              opt.precision = FTA_LIB_MAX_WIDTH_SPECIFY;
            }
          }
          c = *(++msgformat);
        }
        else
        {
          /* - - - - - - - - */
          for(; c >= '0' && c <= '9'; c = *(++msgformat)) {
            opt.precision *= 10;
            opt.precision += (c - '0');
            if(opt.precision > FTA_LIB_MAX_WIDTH_SPECIFY) {
              opt.precision = FTA_LIB_MAX_WIDTH_SPECIFY;
            }
          }
          /* - - - - - - - - */
        }
      }
      if(c == FTA_LIB_STRING_TERM_CHAR) {
        break;
      }

      /* - - - - - - - - */
      switch(c) {
      case 'd':
      case 'i':
      case 'u':
        if(opt.precision != 0) {
          opt.zerochr = 0;
        }
        if(argc > 0) {
          argc--;
          data = va_arg(argv, uint32);
        } else {
          break;
        }

        if(c == 'u') {
          opt.plus_minus = 0;
        } else {
          if((int32)data < 0) {
            opt.plus_minus = '-';
            data = ~data + 1;
          }
        }

        if(opt.plus_minus != 0) {
          if(chrs++ < len && bufout) {
            *buf++ = opt.plus_minus;
          }
          sel_width++;
        }

        if(opt.zerochr && opt.minwidth > fta_const_decimal_figure_cnt + sel_width) {
          zero = TRUE;
          /* - - - - - - - - */
          for(loopcnt = sel_width; loopcnt < opt.minwidth - fta_const_decimal_figure_cnt; loopcnt++) {
            if(chrs++ < len && bufout) {
              *buf++ = '0';
            }
            sel_width++;
          }
          /* - - - - - - - - */
        }
        
        if(opt.precision > fta_const_decimal_figure_cnt) {
          zero = TRUE;
          /* - - - - - - - - */
          for(loopcnt = 0; loopcnt < opt.precision - fta_const_decimal_figure_cnt; loopcnt++) {
            if(chrs++ < len && bufout) {
              *buf++ = '0';
            }
            sel_width++;
          }
          /* - - - - - - - - */
        }
        
        /* - - - - - - - - */
        for(loopcnt = fta_const_decimal_figure_cnt - 1; loopcnt > 0; loopcnt--) {
          if(data >= fta_const_decimal_figure[loopcnt]) {
            numdigit = (data / fta_const_decimal_figure[loopcnt]);
            data -= fta_const_decimal_figure[loopcnt] * numdigit;
            if(chrs++ < len && bufout) {
              *buf++ = (uint8)(numdigit + '0');
            }
            sel_width++;
            zero = TRUE;
          } else {
            if(!zero) {
              if(opt.zerochr && opt.minwidth > loopcnt + sel_width) {
                zero = TRUE;
              }
             if(opt.precision > loopcnt) {
                zero = TRUE;
              }
            }
            if(zero) {
              if(chrs++ < len && bufout) {
                *buf++ = '0';
              }
              sel_width++;
            }
          }
        }
        /* - - - - - - - - */
        if(chrs++ < len && bufout) {
          *buf++ = (uint8)(data + '0');
        }
        sel_width++;

        opt.zerochr = ' ';

        enabled = TRUE;
        break;

      case 'p':
        opt.precision = 8;
        /* fall-through */

      case 'X':
        opt.hexcase = 1;
        /* fall-through */

      case 'x':
        opt.plus_minus = 0;

        if(opt.precision) {
          opt.zerochr = 0;
        }

        if(argc > 0) {
          argc--;
          data = va_arg(argv, uint32);
        } else {
          break;
        }

        zero = FALSE;

        if(opt.zerochr && opt.minwidth > sel_width + FTA_LIB_MIN_HEX_COLUMN) {
          zero = TRUE;
          /* - - - - - - - - */
          for(loopcnt = sel_width; loopcnt < opt.minwidth - FTA_LIB_MIN_HEX_COLUMN; loopcnt++) {
            if(chrs++ < len && bufout) {
              *buf++ = '0';
            }
            sel_width++;
          }
          /* - - - - - - - - */
        }

        if(opt.precision > FTA_LIB_MIN_HEX_COLUMN) {
          zero = TRUE;
          /* - - - - - - - - */
          for(loopcnt = 0; loopcnt < opt.precision - FTA_LIB_MIN_HEX_COLUMN; loopcnt++) {
            if(chrs++ < len && bufout) {
              *buf++ = '0';
            }
            sel_width++;
          }
          /* - - - - - - - - */
        }

        maskdigit = 0xF0000000L;
        /* - - - - - - - - */
        for(loopcnt = FTA_LIB_MIN_HEX_COLUMN - 1; loopcnt > 0; loopcnt --) {
          numdigit = (data & maskdigit) >> (loopcnt << 2);
          if(numdigit > 0) {
            if(chrs++ < len && bufout) {
              *buf++ = fta_const_hex_digit[opt.hexcase][numdigit];
            }
            sel_width++;
            zero = TRUE;
          } else {
            if(!zero) {
              if(opt.zerochr && opt.minwidth > loopcnt + sel_width) {
                zero = TRUE;
              }
              if(opt.precision > loopcnt) {
                zero = TRUE;
              }
            }
            if(zero) {
              if(chrs++ < len && bufout) {
                *buf++ = '0';
              }
              sel_width++;
            }
          }
          maskdigit >>= 4;
        }
        /* - - - - - - - - */
        if(chrs++ < len && bufout) {
          *buf++ = fta_const_hex_digit[opt.hexcase][data & 0x00000000FL];
        }
        sel_width++;

        opt.zerochr = ' ';

        enabled = TRUE;
        break;

      case 'c':
        opt.plus_minus = 0;
        opt.precision = 0;
        
        if(argc > 0) {
          argc--;
          data = va_arg(argv, uint32);
        } else {
          break;
        }
        
        if(chrs++ < len && bufout) {
          *buf++ = (uint8) data;
        }
        sel_width++;
        
        enabled = TRUE;
        break;

      case 's':
        opt.plus_minus = 0;
        
        if(argc > 0) {
          argc--;
          data = va_arg(argv, uint32);
        } else {
          break;
        }

        if(data == 0) {
          data = (uint32)FTA_LIB_NULL_CHANGE_TO_NULLSTR;
        }

        sel_width = (uint32)fta_lib_strnlen((const uint8 *)data, opt.precision);
        if((int32)sel_width > 0) {
          if(bufout) {
            if(chrs + sel_width <= len) {
              memcpy(buf, (void *)data, sel_width);
              buf += sel_width;
            } else if(chrs < len) {
              memcpy(buf, (void *)data, len - chrs);
              buf = bottom;
            }
          }
          chrs += sel_width;
        }

        enabled = TRUE;
        break;

      default:
        ignorechr = TRUE;
        break;
      }
      /* - - - - - - - - */

      if(!ignorechr) {
        if(sel_width < opt.minwidth && enabled) {
          chrs_count = opt.minwidth - sel_width;
          if(bufout) {
            if(opt.alignleft) {
              if(chrs + chrs_count <= len) {
                memset(buf, FTA_LIB_FORMAT_PAD_CHAR, chrs_count);
                buf += chrs_count;
              }
              else if(chrs < len) {
                memset(buf, FTA_LIB_FORMAT_PAD_CHAR, len - chrs);
                buf = bottom;
              }
            } else {
              if(!opt.zerochr) {
                opt.zerochr = ' ';
              }
              if(chrs + chrs_count <= len) {
                memmove(buf_anchor + chrs_count, buf_anchor, sel_width);
                memset(buf_anchor, opt.zerochr, chrs_count);
                buf += chrs_count;
              } else if(chrs_count < len - chrs_anchor) {
                memmove(buf_anchor + chrs_count, buf_anchor, len - chrs_anchor - chrs_count);
                memset(buf_anchor, opt.zerochr, chrs_count);
                buf = bottom;
              } else {
                memset(buf_anchor, opt.zerochr, len - chrs_anchor);
                buf = bottom;
              }
            }
          }
          chrs += chrs_count;
        }
        
        c = *(++msgformat);
        continue;
      }
    }

    if(chrs++ < len && bufout) {
      *buf++ = *msgformat;
    }
    c = *(++msgformat);
  }
  /* - - - - - - - - */

  if(chrs < len && bufout) {
    *buf++ = FTA_LIB_STRING_TERM_CHAR;
    return chrs;
  }

  /* for calcurate length */
  if(!bufout && chrs <= FTA_LIB_FORMAT_BUF_LIMIT) {
    return chrs;
  }

  /* buffer is full */
  return -1;
}
EXPORT_SYMBOL(fta_lib_vsnprintf);

/*!
 @brief fta_lib_strlen

 get the length of the string

 @param [in]  string       string

 @retval      >0:   size of output data
 @retval      -1:   over the max length(65535)
*/
int32 fta_lib_strlen(const uint8 *string)
{
  return fta_lib_strnlen(string, 0);
}
EXPORT_SYMBOL(fta_lib_strlen);

/*!
 @brief fta_lib_strnlen

 get the length of the string(with maxlen)

 @param [in]  string       string
 @param [in]  maxlen       max size of string

 @retval      >0:   size of output data
 @retval      -1:   over the maxlen
*/
int32 fta_lib_strnlen(const uint8 *string, uint32 maxlen)
{
  uint32 chrs;

  if(maxlen == 0 || maxlen > FTA_LIB_MAX_STRINGLENGTH + 1) {
    maxlen = FTA_LIB_MAX_STRINGLENGTH + 1;
  }
  /* - - - - - - - - */
  for(chrs = 0; chrs < maxlen; chrs++) {
    if(*string++ == FTA_LIB_STRING_TERM_CHAR)
      break;
  }
  /* - - - - - - - - */
  return (int32)(chrs <= FTA_LIB_MAX_STRINGLENGTH ? chrs : -1);
}
EXPORT_SYMBOL(fta_lib_strnlen);

MODULE_DESCRIPTION("FTA Driver library");
MODULE_LICENSE("GPL v2");
