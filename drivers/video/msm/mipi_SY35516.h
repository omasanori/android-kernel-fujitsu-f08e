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

#ifndef MIPI_SY35516_H
#define MIPI_SY35516_H

int mipi_sy35516_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

struct sy35516_ctrl_funcs {
    int (*on_fnc) (struct platform_device *);
    int (*off_fnc) (struct platform_device *);
    void (*after_video_on_fnc)(void);
    void (*before_video_off_fnc)(void);
/* FUJITSU:2013-06-21 LCD shutdown start*/
    void (*shutdown)(void);
/* FUJITSU:2013-06-21 LCD shutdown end*/
};

int mipi_sy35516_set_powerfuncs( struct sy35516_ctrl_funcs *ctrl_funcs );

#endif  /* MIPI_SY35516_H */
