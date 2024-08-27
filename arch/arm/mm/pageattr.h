/*
 *  linux/arch/arm/mm/pageattr.h
 *
 *  Copyright (C) 2013 Fujitsu, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PAGEATTR_H__
#define __PAGEATTR_H__

extern int set_memory_ro2(unsigned long virt, int numpages);
extern int set_memory_rw2(unsigned long virt, int numpages);
extern int set_memory_x2(unsigned long virt, int numpages);
extern int set_memory_nx2(unsigned long virt, int numpages);

#endif //__PAGEATTR_H__
