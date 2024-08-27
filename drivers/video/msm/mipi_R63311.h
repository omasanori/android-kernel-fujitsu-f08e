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

#ifndef MIPI_R63311_H
#define MIPI_R63311_H

int mipi_r63311_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

struct r63311_ctrl_funcs {
    int (*on_fnc) (struct platform_device *);
    int (*off_fnc) (struct platform_device *);
    void (*after_video_on_fnc)(void);
    void (*before_video_off_fnc)(void);
/* FUJITSU:2013-04-02 DISP panel name start */
    char* (*name_fnc) (void);
/* FUJITSU:2013-04-02 DISP panel name end */
};

int mipi_r63311_set_powerfuncs( struct r63311_ctrl_funcs *ctrl_funcs );

#endif  /* MIPI_R63311_H */
