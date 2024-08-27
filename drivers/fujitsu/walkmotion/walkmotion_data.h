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

#ifndef __WALKMOTION_DATA__
#define __WALKMOTION_DATA__

/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
#define FJ_WM_WAKELOCK_TIME           2	/* 2.0s */
#define FJ_WM_WAKELOCK_TIME_PREPARE   5	/* 5.0s	*/
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */

/* Walk motion data */
struct fj_wm_data {
    /** Driver state */
    int                state;
    /** Wake lock */
    struct wake_lock   wake_lock;
    /** Motion IRQ wait queue */
    wait_queue_head_t  wait_queue_motion_irq;
    /** Motion IRQ */
    int                motion_irq;
    /** Delay */
    int                mc_init_delay;
    /** Device for debug */
    struct device      *dbg_dev;
    /** enable/disable irq flag */
    int                irq_flag;
/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
	/** prepare flag, spinlock */
	int                prepare_flag;
	spinlock_t         spinlock;
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */
    /** IO lock */
    struct mutex       io_lock;

    int                init_flag;

    int                wakeup_flag;
};

/* debug log */
#undef FJ_WM_DEBUG_LOG
#ifdef FJ_WM_DEBUG_LOG
#define debug_printk(format, arg...) printk("%s" format, KERN_DEBUG, ##arg)
#else
#define debug_printk(format, arg...)
#endif

#endif /* __WALKMOTION_DATA__ */
