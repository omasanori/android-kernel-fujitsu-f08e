/*
 * Copyright(C) 2012 FUJITSU LIMITED
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

#ifndef _LEDS_NOTIFICATION_HW_H_
#define _LEDS_NOTIFICATION_HW_H_

/* ------------------------------------------------------------------------ */
/* extern                                                                   */
/* ------------------------------------------------------------------------ */
extern int notification_led_hw_set(int red, int green, int blue);
extern int notification_led_hw_init(struct platform_device * pdev);
extern int notification_led_hw_exit(struct platform_device * pdev);

/*LED GPIO*/
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio + NR_GPIO_IRQS)

#define LED_R	 0			/* GPIO Number  1 */
#define LED_G	 1 			/* GPIO Number  2 */
#define LED_B	 2			/* GPIO Number  3 */

#endif // _LEDS_NOTIFICATION_HW_H_
