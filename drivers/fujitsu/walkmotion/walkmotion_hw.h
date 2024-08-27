/*
 * Copyright(C) 2012-2013 FUJITSU LIMITED
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

#ifndef __WALKMOTION_HW__
#define __WALKMOTION_HW__

/* Motion IRQ */
#define FJ_WM_GPIO_MOTION_IRQ           (26)

/* Reset */
#define FJ_WM_GPIO_RESET                (BU1852_EXP2_GPIO_BASE+13)

/* Wakeup */
#define FJ_WM_GPIO_HOSU_WUP             (BU1852_EXP1_GPIO_BASE+0)

/* Model */
#define FJ_WM_GPIO_MODEL_1              (BU1852_EXP2_GPIO_BASE+0)
#define FJ_WM_GPIO_MODEL_2              (BU1852_EXP2_GPIO_BASE+4)
#define FJ_WM_GPIO_MODEL_3              (BU1852_EXP2_GPIO_BASE+5)

/* Board */
#define FJ_WM_GPIO_BOARD_1              (BU1852_EXP1_GPIO_BASE+2)
#define FJ_WM_GPIO_BOARD_2              (BU1852_EXP1_GPIO_BASE+4)
#define FJ_WM_GPIO_BOARD_3              (BU1852_EXP1_GPIO_BASE+5)

#define FJ_WM_MODEL_CHAMPAGNE           (7)
#define FJ_WM_MODEL_LUPIN               (4)
#define FJ_WM_BOARD_1_1                 (0)

#define FJ_WM_PM8921_SENS_HCE_LDO_ON    (30)
#define FJ_WM_CHAMPAGNE_1_1_SLEEP_TIME  (100)
#define FJ_WM_LUPIN_SLEEP_TIME          (500)

#define FJ_WM_HCE_RESET_IRQ_WAIT_TIME   (250)

/* High */
#define FJ_WM_GPIO_HIGH                 (1)
/* Low */
#define FJ_WM_GPIO_LOW                  (0)

/* Prototype declaration */
void fj_wm_hw_motion_irq(struct work_struct *ws);
int fj_wm_hw_init(void);
int fj_wm_hw_exit(void);
int fj_wm_hw_probe(void);
int fj_wm_hw_remove(void);
int fj_wm_hw_release(void);
long fj_wm_hw_hce_reset(void);
long fj_wm_hw_hce_request_irq(unsigned long value);
long fj_wm_hw_hce_cancel_irq(void);
long fj_wm_hw_hce_wakeup_set(unsigned long value);

#endif /* __WALKMOTION_HW__ */
