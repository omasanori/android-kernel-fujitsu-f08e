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

#ifndef __LINUX_COREDUMP_EXT_H
#define __LINUX_COREDUMP_EXT_H

/******************************/
/*      default setting       */
/******************************/
#define DEFAULT_CORE_LIMITSIZE (0)
#define DEFAULT_NUM    (0)
#define DEFAULT_WHERE  "/dev/null"

/******************************/
/*     nonvolatile access     */
/******************************/
#define APNV_COREDUMP_I 40038

#define MAX_CORE_WHERE (56)           // output path

/* struct coredump extention in nonvolatile */
struct coredump_struct {
    unsigned long limits;
    unsigned long num;
    unsigned char where[MAX_CORE_WHERE];
};

/******************************/
/*         other size         */
/******************************/
#define MAX_FILE_NAME (128)
#define MAX_EXEC_NAME (16)    /* TASK_COMM_LEN(16) */
#define BLOCK (512)

#endif /* __LINUX_COREDUMP_EXT_H */
