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

#ifndef MIPI_OTM1282B_H
#define MIPI_OTM1282B_H

int mipi_otm1282b_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

struct otm1282b_ctrl_funcs {
    int (*on_fnc) (struct platform_device *);
    int (*off_fnc) (struct platform_device *);
    void (*display_on_fnc)(void);
    ssize_t (*get_wcabc)(char *buf);                    /* FUJITSU:2013-05-17 mode_change add */
    ssize_t (*set_wcabc)(const char *buf,size_t count); /* FUJITSU:2013-05-17 mode_change add */
/* FUJITSU:2013-06-21 LCD shutdown start*/
    void (*shutdown)(void);
/* FUJITSU:2013-06-21 LCD shutdown end*/
};

int mipi_otm1282b_set_powerfuncs( struct otm1282b_ctrl_funcs *ctrl_funcs );

#endif  /* MIPI_OTM1282B_H */
