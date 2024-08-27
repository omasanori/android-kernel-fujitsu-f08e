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

#ifndef _COMPASS_DEF_H
#define _COMPASS_DEF_H

extern const char CMPS_DEVNAME[];

//// SW ///////////////////////////////////////////////////////
extern int cmps_sw_clear_interrupt(void);
extern int cmps_sw_module_stscheck(const uint8_t sts);
extern irqreturn_t cmps_sw_isr(int irq, void *data);
extern int cmps_sw_module_stschg(const uint8_t sts);

#endif
