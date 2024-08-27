/*
 * Copyright(C) 2009-2013 FUJITSU LIMITED
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

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

void crashdump_netcfg(void)
{
	struct net_device *dev;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== netcfg ===\n");

	// walk through net if entries
	for_each_netdev(&init_net, dev){

		unsigned flags = 0;
		struct in_device *pin_device = NULL;
		unsigned addr = 0, mask = 0;
		int bRunning = false;
		char addrstr[16], maskstr[16];

		if( netif_running( dev ) ){
			bRunning = true;
		}

		flags = dev_get_flags( dev );

		pin_device = (struct in_device *)dev->ip_ptr;
		if( pin_device ){
			struct in_ifaddr *ifa;

			ifa = pin_device->ifa_list;
			if(ifa){
				addr = ifa->ifa_address;
				mask = ifa->ifa_mask;

				dprintk(KERN_EMERG "(%s:%s:%d) addr, mask: 0x%08x, 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, addr, mask);
			}
		}

		snprintf(addrstr, sizeof(addrstr), "%d.%d.%d.%d",
				 (addr) & 0xFF,
				 (addr >> 8) & 0xFF,
				 (addr >> 16) & 0xFF,
				 (addr >> 24) & 0xFF);

		snprintf(maskstr, sizeof(maskstr), "%d.%d.%d.%d",
				 (mask) & 0xFF,
				 (mask >> 8) & 0xFF,
				 (mask >> 16) & 0xFF,
				 (mask >> 24) & 0xFF);

		dprintk(KERN_EMERG "(%s:%s:%d) addrstr, maskstr: <%s>, <%s>\n", __FILE__, __FUNCTION__, __LINE__, addrstr, maskstr);

		// print
		crashdump_write_str(
			"%-8s %s  %-16s%-16s0x%08x\n",
			dev->name,
			bRunning ? "UP  " : "DOWN",
			addrstr, maskstr,
			flags );
	}
}
