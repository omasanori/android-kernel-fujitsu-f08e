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
#include <linux/mm.h>
#include <linux/crashdump.h>
#include <asm/highmem.h>

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

/* copied from bionic/libc/include/sys/system_properties.h */
#define PROP_NAME_MAX   32
#define PROP_VALUE_MAX  92

/* copied from bionic/libc/include/sys/_system_properties.h */
#define PROP_AREA_MAGIC   0x504f5250
#define PROP_AREA_VERSION 0x45434f76

/* copied from system/core/init/property_service.c */
#define PA_COUNT_MAX  503
#define PA_INFO_START 2048
#define PA_SIZE       65536

/* copied from bionic/libc/include/sys/_system_properties.h */
struct prop_area {
    unsigned volatile count;
    unsigned volatile serial;
    unsigned magic;
    unsigned version;
    unsigned reserved[4];
    unsigned toc[1];
};

/* copied from bionic/libc/include/sys/_system_properties.h */
struct prop_info {
    char name[PROP_NAME_MAX];
    unsigned volatile serial;
    char value[PROP_VALUE_MAX];
};

/* copied from bionic/libc/include/sys/_system_properties.h */
#define TOC_TO_INFO(area, toc)  ((struct prop_info*) (((char*) area) + ((toc) & 0xFFFFFF)))

void crashdump_system_properties(void)
{
	struct prop_area *pa;
	struct prop_info *pi;
	int i;
	int page;
	int num;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== System properties ===\n");

	num = DIV_ROUND_UP(PA_SIZE, PAGE_SIZE);
	for (i=0; i<num; i++) {
		dprintk(KERN_EMERG "(%s:%d) (0x%08lx\n", __FUNCTION__, __LINE__, system_properties_addr[i]);

		if (system_properties_addr[i]) {
			system_properties_addr[i] = (unsigned long)kmap((struct page *)system_properties_addr[i]);
		}

		dprintk(KERN_EMERG "(%s:%d) (0x%08lx\n", __FUNCTION__, __LINE__, system_properties_addr[i]);
	}

	if (system_properties_addr[0] == 0) {
		printk(KERN_EMERG "system properties page not available\n");
		return;
	}

	pa = (struct prop_area *)system_properties_addr[0];
	dprintk(KERN_EMERG "pa:0x%08x\n", (unsigned int)pa);

	if (pa->magic != PROP_AREA_MAGIC) {
		printk(KERN_EMERG "system properties magic wrong. Found 0x%08x expected 0x%08x\n", pa->magic, PROP_AREA_MAGIC);
		return;
	}

	if (pa->version != PROP_AREA_VERSION) {
		printk(KERN_EMERG "system properties version wrong. Found 0x%08x expected 0x%08x\n", pa->version, PROP_AREA_VERSION);
		return;
	}

	if (pa->count > PA_COUNT_MAX) {
		printk(KERN_EMERG "too many properties. Found %d expected <=%d\n", pa->count, PA_COUNT_MAX);
		return;
	}

	page = 0;
	pi = TOC_TO_INFO(pa, pa->toc[0]);

	for (i=0; i<pa->count; i++) {
		dprintk2(KERN_EMERG "pi:0x%08x [%s]: [%s]\n", (unsigned int)pi, pi->name, pi->value);

		crashdump_write_str("[%s]: [%s]\n", pi->name, pi->value);

		pi++;
		if ( ( (unsigned int)pi%PAGE_SIZE) == 0 ) {
			pi = (struct prop_info*)system_properties_addr[++page];
		}
	}
}
