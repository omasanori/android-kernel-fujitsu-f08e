/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/clkdev.h>
#include <mach/clk-provider.h>

#include "clock.h"

static int clock_debug_rate_set(void *data, u64 val)
{
	struct clk *clock = data;
	int ret;

	/* Only increases to max rate will succeed, but that's actually good
	 * for debugging purposes so we don't check for error. */
	if (clock->flags & CLKFLAG_MAX)
		clk_set_max_rate(clock, val);
	ret = clk_set_rate(clock, val);
	if (ret)
		pr_err("clk_set_rate(%s, %lu) failed (%d)\n", clock->dbg_name,
				(unsigned long)val, ret);

	return ret;
}

static int clock_debug_rate_get(void *data, u64 *val)
{
	struct clk *clock = data;
	*val = clk_get_rate(clock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_rate_fops, clock_debug_rate_get,
			clock_debug_rate_set, "%llu\n");

static struct clk *measure;

static int clock_debug_measure_get(void *data, u64 *val)
{
	struct clk *clock = data;
	int ret, is_hw_gated;

	/* Check to see if the clock is in hardware gating mode */
	if (clock->ops->in_hwcg_mode)
		is_hw_gated = clock->ops->in_hwcg_mode(clock);
	else
		is_hw_gated = 0;

	ret = clk_set_parent(measure, clock);
	if (!ret) {
		/*
		 * Disable hw gating to get accurate rate measurements. Only do
		 * this if the clock is explictly enabled by software. This
		 * allows us to detect errors where clocks are on even though
		 * software is not requesting them to be on due to broken
		 * hardware gating signals.
		 */
		if (is_hw_gated && clock->count)
			clock->ops->disable_hwcg(clock);
		*val = clk_get_rate(measure);
		/* Reenable hwgating if it was disabled */
		if (is_hw_gated && clock->count)
			clock->ops->enable_hwcg(clock);
	}

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_measure_fops, clock_debug_measure_get,
			NULL, "%lld\n");

static int clock_debug_enable_set(void *data, u64 val)
{
	struct clk *clock = data;
	int rc = 0;

	if (val)
		rc = clk_prepare_enable(clock);
	else
		clk_disable_unprepare(clock);

	return rc;
}

static int clock_debug_enable_get(void *data, u64 *val)
{
	struct clk *clock = data;
	int enabled;

	if (clock->ops->is_enabled)
		enabled = clock->ops->is_enabled(clock);
	else
		enabled = !!(clock->count);

	*val = enabled;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_enable_fops, clock_debug_enable_get,
			clock_debug_enable_set, "%lld\n");

static int clock_debug_local_get(void *data, u64 *val)
{
	struct clk *clock = data;

	if (!clock->ops->is_local)
		*val = true;
	else
		*val = clock->ops->is_local(clock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_local_fops, clock_debug_local_get,
			NULL, "%llu\n");

static int clock_debug_hwcg_get(void *data, u64 *val)
{
	struct clk *clock = data;
	if (clock->ops->in_hwcg_mode)
		*val = !!clock->ops->in_hwcg_mode(clock);
	else
		*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_hwcg_fops, clock_debug_hwcg_get,
			NULL, "%llu\n");

static int fmax_rates_show(struct seq_file *m, void *unused)
{
	struct clk *clock = m->private;
	int level = 0;

	int vdd_level = find_vdd_level(clock, clock->rate);
	if (vdd_level < 0) {
		seq_printf(m, "could not find_vdd_level for %s, %ld\n",
			   clock->dbg_name, clock->rate);
		return 0;
	}
	for (level = 0; level < clock->num_fmax; level++) {
		if (vdd_level == level)
			seq_printf(m, "[%lu] ", clock->fmax[level]);
		else
			seq_printf(m, "%lu ", clock->fmax[level]);
	}
	seq_printf(m, "\n");

	return 0;
}

static int fmax_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, fmax_rates_show, inode->i_private);
}

static const struct file_operations fmax_rates_fops = {
	.open		= fmax_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int list_rates_show(struct seq_file *m, void *unused)
{
	struct clk *clock = m->private;
	int rate, level, fmax = 0, i = 0;

	/* Find max frequency supported within voltage constraints. */
	if (!clock->vdd_class) {
		fmax = INT_MAX;
	} else {
		for (level = 0; level < clock->num_fmax; level++)
			if (clock->fmax[level])
				fmax = clock->fmax[level];
	}

	/*
	 * List supported frequencies <= fmax. Higher frequencies may appear in
	 * the frequency table, but are not valid and should not be listed.
	 */
	while ((rate = clock->ops->list_rate(clock, i++)) >= 0) {
		if (rate <= fmax)
			seq_printf(m, "%u\n", rate);
	}

	return 0;
}

static int list_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, list_rates_show, inode->i_private);
}

static const struct file_operations list_rates_fops = {
	.open		= list_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static struct dentry *debugfs_base;
static u32 debug_suspend;

struct clk_table {
	struct list_head node;
	struct clk_lookup *clocks;
	size_t num_clocks;
};

static int clock_debug_add(struct clk *clock)
{
	char temp[50], *ptr;
	struct dentry *clk_dir;

	if (!debugfs_base)
		return -ENOMEM;

	strlcpy(temp, clock->dbg_name, ARRAY_SIZE(temp));
	for (ptr = temp; *ptr; ptr++)
		*ptr = tolower(*ptr);

	clk_dir = debugfs_create_dir(temp, debugfs_base);
	if (!clk_dir)
		return -ENOMEM;

	if (!debugfs_create_file("rate", S_IRUGO | S_IWUSR, clk_dir,
				clock, &clock_rate_fops))
		goto error;

	if (!debugfs_create_file("enable", S_IRUGO | S_IWUSR, clk_dir,
				clock, &clock_enable_fops))
		goto error;

	if (!debugfs_create_file("is_local", S_IRUGO, clk_dir, clock,
				&clock_local_fops))
		goto error;

	if (!debugfs_create_file("has_hw_gating", S_IRUGO, clk_dir, clock,
				&clock_hwcg_fops))
		goto error;

	if (measure &&
	    !clk_set_parent(measure, clock) &&
	    !debugfs_create_file("measure", S_IRUGO, clk_dir, clock,
				&clock_measure_fops))
		goto error;

	if (clock->ops->list_rate)
		if (!debugfs_create_file("list_rates",
				S_IRUGO, clk_dir, clock, &list_rates_fops))
			goto error;

	if (clock->vdd_class && !debugfs_create_file("fmax_rates",
				S_IRUGO, clk_dir, clock, &fmax_rates_fops))
			goto error;

	return 0;
error:
	debugfs_remove_recursive(clk_dir);
	return -ENOMEM;
}
static LIST_HEAD(clk_list);
static DEFINE_SPINLOCK(clk_list_lock);

/**
 * clock_debug_register() - Add additional clocks to clock debugfs hierarchy
 * @table: Table of clocks to create debugfs nodes for
 * @size: Size of @table
 *
 * Use this function to register additional clocks in debugfs. The clock debugfs
 * hierarchy must have already been initialized with clock_debug_init() prior to
 * calling this function. Unlike clock_debug_init(), this may be called multiple
 * times with different clock lists and can be used after the kernel has
 * finished booting.
 */
int clock_debug_register(struct clk_lookup *table, size_t size)
{
	struct clk_table *clk_table;
	unsigned long flags;
	int i;

	clk_table = kmalloc(sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table)
		return -ENOMEM;

	clk_table->clocks = table;
	clk_table->num_clocks = size;

	spin_lock_irqsave(&clk_list_lock, flags);
	list_add_tail(&clk_table->node, &clk_list);
	spin_unlock_irqrestore(&clk_list_lock, flags);

	for (i = 0; i < size; i++)
		clock_debug_add(table[i].clk);

	return 0;
}

/**
 * clock_debug_init() - Initialize clock debugfs
 */
int __init clock_debug_init(void)
{
	debugfs_base = debugfs_create_dir("clk", NULL);
	if (!debugfs_base)
		return -ENOMEM;
	if (!debugfs_create_u32("debug_suspend", S_IRUGO | S_IWUSR,
				debugfs_base, &debug_suspend)) {
		debugfs_remove_recursive(debugfs_base);
		return -ENOMEM;
	}

	measure = clk_get_sys("debug", "measure");
	if (IS_ERR(measure))
		measure = NULL;

	return 0;
}

static int clock_debug_print_clock(struct clk *c)
{
	char *start = "";

	if (!c || !c->prepare_count)
		return 0;

	pr_info("\t");
	do {
		if (c->vdd_class)
			pr_cont("%s%s:%u:%u [%ld, %lu]", start, c->dbg_name,
				c->prepare_count, c->count, c->rate,
				c->vdd_class->cur_level);
		else
			pr_cont("%s%s:%u:%u [%ld]", start, c->dbg_name,
				c->prepare_count, c->count, c->rate);
		start = " -> ";
	} while ((c = clk_get_parent(c)));

	pr_cont("\n");

	return 1;
}

/**
 * clock_debug_print_enabled() - Print names of enabled clocks for suspend debug
 *
 * Print the names of enabled clocks and their parents if debug_suspend is set
 */
void clock_debug_print_enabled(void)
{
	struct clk_table *table;
	unsigned long flags;
	int i, cnt = 0;

	if (likely(!debug_suspend))
		return;

	pr_info("Enabled clocks:\n");
	spin_lock_irqsave(&clk_list_lock, flags);
	list_for_each_entry(table, &clk_list, node) {
		for (i = 0; i < table->num_clocks; i++)
			cnt += clock_debug_print_clock(table->clocks[i].clk);
	}
	spin_unlock_irqrestore(&clk_list_lock, flags);

	if (cnt)
		pr_info("Enabled clock count: %d\n", cnt);
	else
		pr_info("No clocks enabled.\n");

}

/* FUJITSU:2013-01-25 gpiodump start */
extern struct clock_init_data dump_clock_init_data;
void clock_dump_info(void)
{
	struct clock_init_data *data = &dump_clock_init_data;
	unsigned n;
	u64 rate;
	u64 enable;
	u64 is_local;
	u64 has_hw_gating;

	if (!data || data->size > 1024){
		printk(KERN_INFO "DCLK, ERRARG1\n");
		return;
	}

	printk(KERN_INFO "DCLK,clock,enable,has_hw_gating,is_local,rate,\n");
	for (n = 0; n < data->size; n++) {
		if (data->table){
			struct clk *clk = data->table[n].clk;
			if (clk){
				clock_debug_rate_get(clk, &rate);
				clock_debug_enable_get(clk, &enable);
				clock_debug_local_get(clk, &is_local);
				clock_debug_hwcg_get(clk, &has_hw_gating);
				printk(KERN_INFO "DCLK,%s,%lld,%lld,%lld,%lld,\n",
					clk->dbg_name,enable,has_hw_gating,is_local,rate);
			}else{
				printk(KERN_INFO "DCLK,ERR,clk=NULL\n");
				return;
			}
		}else{
			printk(KERN_INFO "DCLK,ERR,data->table=NULL\n");
			return;
		}
	}
}
/* FUJITSU:2013-01-25 for gpiodump end */
