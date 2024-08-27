/*
 *  linux/arch/arm/mm/pageattr.c
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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>

#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/tlbflush.h>
#include <asm/memory.h>

#include "mm.h"

//#define MEM_VIRT_ALIAS_CHK
//#define DEBUG_MEMATTR
//#define CONFIG_SECTIONMEM_ATTR_SUPPORT

/*
* Debug print
*/
#ifdef DEBUG_MEMATTR
#define memattr_debug(x, ...) printk(x, __VA_ARGS__)
#else //DEBUG_MEMATTR
#define memattr_debug(x, ...)
#endif //DEBUG_MEMATTR

/*
* Spin lock def
*/
static DEFINE_SPINLOCK(chgattr_lock);

/*
 * Lock the state
 */
static void change_page_attr_spinlock(unsigned long *flags)
{
	spin_lock_irqsave(&chgattr_lock, *flags);
}

/*
 * Unlock the state
 */
static void change_page_attr_spinunlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&chgattr_lock, *flags);
}

PTE_BIT_FUNC(nxprotect, |= L_PTE_XN);
PTE_BIT_FUNC(mkexec, &= ~L_PTE_XN);
static int change_page_attributes(unsigned long virt, int numpages, pte_t (*func)(pte_t), int chk_alias);

/*
 * Search the page table entry for a virtual address. Return a pointer
 * to the entry and memory size type(PMD_TYPE).
 * Input: 
	unsigned long virt	:The virtual memory start address to search
	unsigned long *type	:The memory size type is returned
 *
 */
/*
 * If pmd is there and pte doesn't exist, then how we handle this situation.
 * Currently, this is handled as if it is the invalid situation, and return NULL.
 */
#define PMD_TYPE_UNKNOWN	0UL
static pte_t *search_tblentry(unsigned long virt, unsigned long *type)
{
	pmd_t *pmd;
	pte_t *pte;

	if( type == NULL ) {
		memattr_debug("%s", "search_tblentry err type param null");
		return NULL;
	}

	/* Default type is unknown */
	*type = PMD_TYPE_UNKNOWN;
	pmd = pmd_off_k(virt);

	/* pmd(entry) is not present, then return NULL */
	if (pmd == NULL) {
		memattr_debug("%s", "search_tblentry err pmd null");
		return NULL;
	}
	else if(pmd_none(*pmd)) {
		memattr_debug("%s", "search_tblentry err pmd 0");
		return NULL; //ALL 0 = pmdval initial state
	} else {
		/* pmd entry is section memory, then return pmd pointer and PMD_TYPE_SECT */
		if ( (pmd_val(*pmd) & PMD_TYPE_MASK) == PMD_TYPE_SECT ) {
			*type = PMD_TYPE_SECT; // 2MB section memory management
			return NULL;
		} else if ( (pmd_val(*pmd) & PMD_TYPE_MASK) == PMD_TYPE_TABLE ) {
			*type = PMD_TYPE_TABLE; // 4KB page memory management
			pte = pte_offset_kernel(pmd, virt);
			/* pmd exists, but pte doesn't exist. */
			if (pte == NULL) {
				memattr_debug("%s", "search_tblentry err pte null");
				return NULL;
			} else if (pte_none(*pte)) {
				memattr_debug("%s", "search_tblentry err pte 0");
				return NULL; //ALL 0 = pteval initial state
			}
		} else {
			memattr_debug("%s", "search_tblentry err pmd type");
			return NULL;
		}
	}

	return pte;
}

/*
 * Flush the tlb and cpu cache when updating the pmd/pte info
 */
static void flush_cache_range_ex(unsigned long start, unsigned long end)
{
	unsigned long addr;
	unsigned long type;

	/* Flush tlb entry between the specified address range */
	flush_tlb_kernel_range(start, end);

	/* Flush the L1/L2 cache based on the current address info */
	for (addr = start; addr < end; addr += PAGE_SIZE) {
		pte_t *pte = search_tblentry(addr, &type);
		if (pte) {
			/* vitual address base cache */
			__cpuc_flush_dcache_area((void *) addr, PAGE_SIZE);
			/* physical address base cache. There is no outer cache, this operates nothing. */
			outer_flush_range(__pa((void *)addr),
					__pa((void *)addr) + PAGE_SIZE);
		}
	}
}


#ifdef MEM_VIRT_ALIAS_CHK
/*
 * virtual address may have alias( one physmem->multi virmem mapping )
 * The memory region between PAGE_OFFSET and highmemory - 1 maps directly phys->virt.
 * There is no alias on the kernel side virtual mapping. User side has alias and we should check its existense.
 *
 * [ARMv7 and Page Colouring]
 *
 * ARMv7 processors have PIPT data caches (or at least are required to behave as if they do).
 * That is, they have physically indexed, physically tagged data caches, and no page colouring restrictions apply.
 * However, they can have VIPT instruction caches and even VIVT instruction caches are allowed to an extent.
 * They can also have PIPT instruction caches. Cortex-A8 and Cortex-A9 both have VIPT instruction caches. 
 *
 * For most use-cases, a VIPT instruction cache behaves very much like a PIPT instruction cache.
 * Instruction caches are read-only, so the most obvious coherence issues cannot occur.
 * However, VIPT instruction caches may still need special handling when invalidating cache lines by virtual address: aliased entries with different colours will not
 * be invalidated. Actually, the Architecture Reference Manual gives the following statement: 
 *
 * The only architecturally-guaranteed way to invalidate all aliases of a physical address from a VIPT instruction cache is to invalidate the entire instruction cache. 
 *
 * In practice, you should be able to simply invalidate all the possible colour variants of a given virtual address.
 * Of course, if you don't care about all virtual aliases, then it doesn't matter that others might exist.
 *
 * Note that writes to instruction data through the data cache — for example, in self-modifying code — always require
 * explicit cache maintenance operations.
 * This is because there is no coherency between instruction and data caches in the ARM architecture. 
 */
static int check_alias(unsigned long virt, int numpages, pte_t (*func)(pte_t), unsigned long pfn)
{
	unsigned long alias_addr;

	/* this is fixed mapping memory region. and there is no alias.
	 * arm_lowmem_limit physical address stands for the 8MB hole memory boundary.(highmemory = __va(arm_lowmem_limit - 1)+1)
	 * The memory region above the arm_lowmem_limit is the fixed mapping area.
	 */
	if( pfn >= (arm_lowmem_limit >> PAGE_SHIFT) ) return 0;

	/*
	 * If addres is not in the kernel direct mapping space(pa<->va mapping is 1:1), then check alias.
	 */
	if( !virt_addr_valid(virt) ) {
		alias_addr = pfn_to_kaddr(pfn);
		ret = change_page_attributes(alias_addr, numpages, func, 0);
		if (ret)
			return ret;
	}

	return 0;
}
#endif // MEM_VIRT_ALIAS_CHK


/*
*   Change the virtual memory page attributes.
*   
*   Input: 
	unsigned long virt	:The virtual memory start address to change its attributes
	int numpages		:The number of the virtual memory page
	pte_t (*func)(pte_t)	:The PTE attribute setting function pointer.
	int chk_alias		:Alias check flag. if 1 then check, else no check.
*
*/
/* (Func)			(srcpath)
*  kmap_flush_unused		kernel/mm/highmem.c, kernel/include/linux/highmem.h
*  vm_unmap_aliases		kernel/mm/vmalloc.c, kernel/include/linux/vmalloc.h
*/
static int change_page_attributes(unsigned long virt, int numpages, pte_t (*func)(pte_t), int chk_alias)
{
	int ret = 0;
	pte_t *pte;
	unsigned long start;
	unsigned long end;
	unsigned long pmd_type;

	memattr_debug("chpg_attr vaddr=%08lX, numpage=%d\n", virt, numpages);
	
	/* Func pointer check */
	if( func == NULL ) return -EINVAL;

	/* Alignment check */
	if (virt & ~PAGE_MASK) {
		virt &= PAGE_MASK;
		/*
		 * unaligned address is invalid.
		 */
		WARN_ON_ONCE(1);
	}

	/* Save start address in order to flash cache later */
	start = virt;
	end = virt + (numpages << PAGE_SHIFT);

	memattr_debug("chpg_attr startv=%08lX, endv=%08lX\n", start, end);

	/*
	 * flush all unused kmap mappings in order to remove stray mappings
	 */
	kmap_flush_unused();

	/**
	 * vm_unmap_aliases - unmap outstanding lazy aliases in the vmap layer
	 *
	 * The vmap/vmalloc layer lazily flushes kernel virtual mappings primarily
	 * to amortize TLB flushing overheads. What this means is that any page you
	 * have now, may, in a former life, have been mapped into kernel virtual
	 * address by the vmap layer and so there might be some CPUs with TLB entries
	 * still referencing that page (additional to the regular 1:1 kernel mapping).
	 *
	 * vm_unmap_aliases flushes all such lazy mappings. After it returns, we can
	 * be sure that none of the pages we have control over will have any aliases
	 * from the vmap layer.
	 */
	vm_unmap_aliases();

	/*
	 * If 4k page entry is found, then set attributes.
	 */
	while( numpages ) {
		pte = search_tblentry(virt, &pmd_type);
		if( pte == NULL || pmd_type == PMD_TYPE_UNKNOWN || pmd_type == PMD_TYPE_SECT ) {
			virt = end;
			numpages = 0;
			continue;
		} else {
			/* if pmd_type == PMD_TYPE_TABLE then , set attribute */
			set_pte_ext(pte, func(*pte), 0);
#ifdef MEM_VIRT_ALIAS_CHK
			if( chk_alias ) {
				if( (ret = check_alias(virt, numpages, func, pte_pfn(pteval(*pte)))) != 0 ) {
					memattr_debug("check_alias err = %d", ret);
				}
			}
#endif // MEM_VIRT_ALIAS_CHK
			virt += PAGE_SIZE;
			numpages--;
		}
	}

	/* flush tlb entry and cpu cache */
	flush_cache_range_ex(start, end);

	return ret;
}

/*
*   Change the virtual memory attributes into readonly.
*   
*   Input: 
	unsigned long virt	:The virtual memory start address to change its attributes
	int numpages		:The number of the virtual memory page
*
*	pte_wrprotect function is defined in the "pgtable.h" such like "PTE_BIT_FUNC(wrprotect, |= L_PTE_RDONLY);"
*/
int set_memory_ro2(unsigned long virt, int numpages)
{
	unsigned long flag;
	int ret;

	change_page_attr_spinlock(&flag);
	ret = change_page_attributes(virt, numpages, pte_wrprotect, 1);
	change_page_attr_spinunlock(&flag);

	return ret;
}
EXPORT_SYMBOL(set_memory_ro2);

/*
*   Change the virtual memory attributes into read-writable.
*   
*   Input: 
	unsigned long virt	:The virtual memory start address to change its attributes
	int numpages		:The number of the virtual memory page
*
*	pte_mkwrite function is defined in the "pgtable.h" such like "PTE_BIT_FUNC(mkwrite,   &= ~L_PTE_RDONLY);"
*/
int set_memory_rw2(unsigned long virt, int numpages)
{
	unsigned long flag;
	int ret;

	change_page_attr_spinlock(&flag);
	ret = change_page_attributes(virt, numpages, pte_mkwrite, 1);
	change_page_attr_spinunlock(&flag);

	return ret;
}
EXPORT_SYMBOL(set_memory_rw2);

/*
*   Change the virtual memory attributes into executable.
*   
*   Input: 
	unsigned long virt	:The virtual memory start address to change its attributes
	int numpages		:The number of the virtual memory page
*
*/
int set_memory_x2(unsigned long virt, int numpages)
{
	unsigned long flag;
	int ret;

	change_page_attr_spinlock(&flag);
	ret = change_page_attributes(virt, numpages, pte_mkexec, 0);
	change_page_attr_spinunlock(&flag);

	return ret;
}
EXPORT_SYMBOL(set_memory_x2);

/*
*   Change the virtual memory attributes into non-executable.
*   
*   Input: 
	unsigned long virt	:The virtual memory start address to change its attributes
	int numpages		:The number of the virtual memory page
*
*/
int set_memory_nx2(unsigned long virt, int numpages)
{
	unsigned long flag;
	int ret;

	change_page_attr_spinlock(&flag);
	ret = change_page_attributes(virt, numpages, pte_nxprotect, 0);
	change_page_attr_spinunlock(&flag);

	return ret;
}
EXPORT_SYMBOL(set_memory_nx2);
