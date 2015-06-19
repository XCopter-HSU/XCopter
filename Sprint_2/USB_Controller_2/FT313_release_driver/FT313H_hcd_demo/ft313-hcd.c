/*
 * FT313 Host Controller Driver.
 *
 * Copyright (C) 2011 Chang Yang <chang.yang@ftdichip.com>
 *
 * This code is *strongly* based on EHCI-HCD code by David Brownell since
 * the chip is a quasi-EHCI compatible.
 *
 * Licensed under GPL version 2 only.
 */

#define DRIVER_AUTHOR "Chang Yang"
#define DRIVER_DESC "FT313 USB 2.0 Host Controller Driver"

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/dmapool.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/moduleparam.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/fs.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>

#ifdef USE_UDEV
/* Device file related */
static struct class* ftdi_class = NULL;
static struct device* ftdi_device = NULL;
#endif

/* magic numbers that can affect system performance */
#define	EHCI_TUNE_CERR		3	/* 0-3 qtd retries; 0 == don't stop */
#define	EHCI_TUNE_RL_HS		4	/* nak throttle; see 4.9 */
#define	EHCI_TUNE_RL_TT		0
#define	EHCI_TUNE_MULT_HS	1	/* 1-3 transactions/uframe; 4.10.3 */
#define	EHCI_TUNE_MULT_TT	1
/*
 * Some drivers think it's safe to schedule isochronous transfers more than
 * 256 ms into the future (partly as a result of an old bug in the scheduling
 * code).  In an attempt to avoid trouble, we will use a minimum scheduling
 * length of 512 frames instead of 256.
 */
#define	EHCI_TUNE_FLS		1	/* (medium) 512-frame schedule */

#define EHCI_IAA_MSECS		10		/* arbitrary */
#define EHCI_IO_JIFFIES		(HZ/10)		/* io watchdog > irq_thresh */
#define EHCI_ASYNC_JIFFIES	(HZ/20)		/* async idle timeout */
#define EHCI_SHRINK_JIFFIES	(DIV_ROUND_UP(HZ, 200) + 1)
						/* 200-ms async qh unlink delay */

/* Initial IRQ latency:  faster than hw default */
static int log2_irq_thresh = 0;		// 0 to 6
module_param (log2_irq_thresh, int, S_IRUGO);
MODULE_PARM_DESC (log2_irq_thresh, "log2 IRQ latency, 1-64 microframes");

/* initial park setting:  slower than hw default */
static unsigned park = 0;
module_param (park, uint, S_IRUGO); // FixMe: We may not want to use module parameter!
MODULE_PARM_DESC (park, "park setting; 1-3 back-to-back async packets");

/* for flakey hardware, ignore overcurrent indicators */
static int ignore_oc = 0;
module_param (ignore_oc, bool, S_IRUGO);
MODULE_PARM_DESC (ignore_oc, "ignore bogus hardware overcurrent indications");

/* Default BCD Mode Initilization */
static char *bcd_mode = "Disabled";
module_param(bcd_mode, charp, 0000);
MODULE_PARM_DESC(bcd_mode, "Indicate BCD mode when HCD inserted");
#include "ft313_app.h"
#include "ft313_def.h"
#include "ft313.h"
#include "ft313-dbg.c"

#define INTR_MASK (INT_OAA_EN | H_SYSERR_EN | PO_CHG_DET_EN | USBERR_INT_EN | USB_INT_EN)

static inline void safe_spin_lock(spinlock_t* lock, unsigned long* flags)
{
	if (in_interrupt()) {
		spin_lock(lock);
	} else {
		spin_lock_irqsave(lock, *flags);
	}
}

static inline void safe_spin_unlock(spinlock_t* lock, unsigned long* flags)
{
	if (in_interrupt()) {
		spin_unlock(lock);
	} else {
		spin_unlock_irqrestore(lock, *flags);
	}
}

/* Low level access APIs */
static inline u8 ft313_reg_read8(const struct ft313_hcd *ft313,
				   void __iomem * regs)
{
	u8 val;

	val = ioread8(regs);

#ifdef LOG_ON
	print_reg_access_info(IO_READ, regs, val);
#endif
	return val;
}

static inline u16 ft313_reg_read16(const struct ft313_hcd *ft313,
		void __iomem * regs)
{
	unsigned short val = 0;

//	if (IS_8_BIT_MODE(ioread8(&ft313->cfg->sw_reset)))
#ifdef FT313_IN_8_BIT_MODE
	{ // 8 bit mode
		val  = ioread8(regs);
		val += ioread8(regs + 1) << 8;
	}
#else //	else // 16 bit mode
	{
		val = ioread16(regs);
	}
#endif

#ifdef LOG_ON
	print_reg_access_info(IO_READ, regs, val);
#endif
	return val;
}

static inline void ft313_reg_write16(const struct ft313_hcd *ft313, u16 val,
		void __iomem * regs)
{
//	if (IS_8_BIT_MODE(ioread8(&ft313->cfg->sw_reset)))
#ifdef FT313_IN_8_BIT_MODE
	{ // 8 bit mode
		iowrite8(val, regs);
		iowrite8(val >>  8, regs + 1);
	}
#else //	else // 16 bit mode
	{
		iowrite16(val, regs);
	}
#endif

#ifdef LOG_ON
	print_reg_access_info(IO_WRITE, regs, val);
#endif

}

static inline u32 ft313_reg_read32(const struct ft313_hcd *ft313,
		void __iomem * regs)
{
	unsigned int val = 0;

//	if (IS_8_BIT_MODE(ioread8(&ft313->cfg->sw_reset)))
#ifdef FT313_IN_8_BIT_MODE
	{ // 8 bit mode
		val  = ioread8(regs);
		val += ioread8(regs + 1) << 8;
		val += ioread8(regs + 2) << 16;
		val += ioread8(regs + 3) << 24;
	}
#else //	else // 16 bit mode
	{
		val = ioread16(regs);
		val += ioread16(regs + 2) << 16;
	}
#endif

#ifdef LOG_ON
	print_reg_access_info(IO_READ, regs, val);
#endif

	return val;
}

static inline void ft313_reg_write32(const struct ft313_hcd *ft313, u32 val,
		void __iomem * regs)
{
//	if (IS_8_BIT_MODE(ioread8(&ft313->cfg->sw_reset)))
#ifdef FT313_IN_8_BIT_MODE
	{ // 8 bit mode
		iowrite8(val, regs);
		iowrite8(val >>  8, regs + 1);
		iowrite8(val >> 16, regs + 2);
		iowrite8(val >> 24, regs + 3);
	}
#else //	else // 16 bit mode
	{
		iowrite16(val, regs);
		iowrite16(val >> 16, regs + 2);
	}
#endif

#ifdef LOG_ON
	print_reg_access_info(IO_WRITE, regs, val);
#endif
}

void ft313_mem_read(struct ft313_hcd *ft313, void *buf, u16 length, u16 offset)
{
	int i;
	unsigned long flags = 0;

	if (NULL == buf) {
		printk("Null buffer used as destination address!\n");
		return;
	}

	if (in_interrupt()) {
		spin_lock(&ft313->dataport_lock);
	} else {
		spin_lock_irqsave(&ft313->dataport_lock, flags);
	}
//	if (IS_8_BIT_MODE(ioread8(&ft313->cfg->sw_reset)))
#ifdef FT313_IN_8_BIT_MODE
	{ // 8 bit mode
		ft313_reg_write16(ft313, 0x8000 | length, &ft313->cfg->data_session_len); //Set direction as read
		ft313_reg_write16(ft313, offset, &ft313->cfg->mem_addr);
		for (i = 0; i < length; i++)
			*((u8*)(buf + i)) = ioread8(&ft313->cfg->data_port);

	}
#else // 16 bit mode
	{
		if (0 != (length % 2)) length++; // Software need to adjust length
		ft313_reg_write16(ft313, 0x8000 | length, &ft313->cfg->data_session_len); //Set direction as read
		ft313_reg_write16(ft313, offset, &ft313->cfg->mem_addr);

		for (i = 0; i < length; i += 2)
			*((u16*)(buf + i)) = (u16)ioread16(&ft313->cfg->data_port);
	}
#endif
	if (in_interrupt()) {
		spin_unlock(&ft313->dataport_lock);
	} else {
		spin_unlock_irqrestore(&ft313->dataport_lock, flags);
	}

#ifdef LOG_ON
	print_mem_access_info(IO_READ, offset, length, buf);
	for (i = 0; i < min((u16)64, length); i += 4) {
		printk("%08X ", *(u32*)(buf + i));
		printk("\n");
	}
#endif

}

void ft313_mem_write(struct ft313_hcd *ft313, void *buf, u16 length, u16 offset)
{
	int i;
	unsigned long flags = 0;

	if (in_interrupt()) {
		spin_lock(&ft313->dataport_lock);
	} else {
		spin_lock_irqsave(&ft313->dataport_lock, flags);
	}

//	if (IS_8_BIT_MODE(ioread8(&ft313->cfg->sw_reset)))
#ifdef FT313_IN_8_BIT_MODE
	{ // 8 bit mode
		ft313_reg_write16(ft313, length, &ft313->cfg->data_session_len);
		ft313_reg_write16(ft313, offset, &ft313->cfg->mem_addr);
		for (i = 0; i < length; i++)
			iowrite8(*((u8*)(buf + i)), &ft313->cfg->data_port);
	}
#else // 16 bit mode
	{
		if (0 != (length % 2)) length++; // Software need to adjust length
		ft313_reg_write16(ft313, length, &ft313->cfg->data_session_len);
		ft313_reg_write16(ft313, offset, &ft313->cfg->mem_addr);


		for (i = 0; i < length; i += 2)
			iowrite16(*((u16*)(buf + i)), &ft313->cfg->data_port);
	}
#endif
	if (in_interrupt()) {
		spin_unlock(&ft313->dataport_lock);
	} else {
		spin_unlock_irqrestore(&ft313->dataport_lock, flags);
	}

#ifdef LOG_ON
	print_mem_access_info(IO_WRITE, offset, length, buf);
	for (i = 0; i < min((u16)64, length); i += 4) {
		printk("%08X ", *(u32*)(buf + i));
		printk("\n");
	}
#endif

}

// FIXME: debug only
void display_async_list(struct ft313_hcd* ft313)
{
	struct ehci_qh *prev;
	u32 horizontal_ptr;
	u32 async_reg;

	printk("\n\nFT313 Async List as:\n\n");
	prev = ft313->async;
	ft313_mem_read(ft313, &horizontal_ptr, 4, prev->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));
	printk("From qh 0x%X (horizon ptr 0x%X) -> ", prev->qh_ft313, horizontal_ptr);

	while (prev->qh_next.qh != NULL) {
		prev = prev->qh_next.qh;
		ft313_mem_read(ft313, &horizontal_ptr, 4, prev->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));
		printk("qh 0x%X (horizon ptr 0x%X)-> ", prev->qh_ft313, horizontal_ptr);
	}
	printk(" NULL \n");

	async_reg = ft313_reg_read32(ft313, &ft313->regs->async_next);

	printk("Current Async List Address at 0x%X\n\n\n", async_reg);
}

static void
timer_action(struct ft313_hcd *ft313, enum ft313_timer_action action)
{
	FUN_ENTRY();
	/* Don't override timeouts which shrink or (later) disable
	 * the async ring; just the I/O watchdog.  Note that if a
	 * SHRINK were pending, OFF would never be requested.
	 */
	if (timer_pending(&ft313->watchdog)
			&& ((BIT(TIMER_ASYNC_SHRINK) | BIT(TIMER_ASYNC_OFF))
			    & ft313->actions)) {
		FUN_EXIT();
		return;
	}

	if (!test_and_set_bit(action, &ft313->actions)) {
		unsigned long t;

		switch (action) {
		case TIMER_IO_WATCHDOG:
			if (!ft313->need_io_watchdog) {
				FUN_EXIT();
				return;
			}
			t = EHCI_IO_JIFFIES;
			break;
		case TIMER_ASYNC_OFF:
			t = EHCI_ASYNC_JIFFIES;
			break;
		/* case TIMER_ASYNC_SHRINK: */
		default:
			t = EHCI_SHRINK_JIFFIES;
			break;
		}
		mod_timer(&ft313->watchdog, t + jiffies);
		DEBUG_MSG("Trigger FT313->watchdog timer with t = %u\n", (unsigned int)t);
	}
	FUN_EXIT();
}


/*-------------------------------------------------------------------------*/

/*
 * handshake - spin reading hc until handshake completes or fails
 * @ptr: address of hc register to be read
 * @mask: bits to look at in result of read
 * @done: value of those bits when handshake succeeds
 * @usec: timeout in microseconds
 *
 * Returns negative errno, or zero on success
 *
 * Success happens when the "mask" bits have the specified value (hardware
 * handshake done).  There are two failure modes:  "usec" have passed (major
 * hardware flakeout), or the register reads as all-ones (hardware removed).
 *
 * That last failure should_only happen in cases like physical cardbus eject
 * before driver shutdown. But it also seems to be caused by bugs in cardbus
 * bridge shutdown:  shutting down the bridge before the devices using it.
 */
static int handshake (struct ft313_hcd *ft313, void __iomem *ptr,
		      u32 mask, u32 done, int usec)
{
	u32	result;

	do {
		result = ft313_reg_read32(ft313, ptr);
		if (result == ~(u32)0)		/* card removed */
			return -ENODEV;
		result &= mask;
		if (result == done)
			return 0;
		udelay (1);
		usec--;
	} while (usec > 0);
	return -ETIMEDOUT;
}

/* force HC to halt state from unknown (EHCI spec section 2.3) */
static int ft313_halt (struct ft313_hcd *ft313)
{
	u32	temp = ft313_reg_read32(ft313, &ft313->regs->status);

	/* disable any irqs left enabled by previous code */
	ft313_reg_write32(ft313, 0, &ft313->regs->intr_enable);

/*	if (ehci_is_TDI(ft313) && tdi_in_host_mode(ft313) == 0) {
		return 0;
	}
*/
	if ((temp & HCHALTED) != 0)
		return 0;

	temp = ft313_reg_read32(ft313, &ft313->regs->command);
	temp &= ~RS;
	ft313_reg_write32(ft313, temp, &ft313->regs->command);
	return handshake (ft313, &ft313->regs->status,
			  HCHALTED, HCHALTED, 16 * 125);
}

static int handshake_on_error_set_halt(struct ft313_hcd *ft313, void __iomem *ptr,
				       u32 mask, u32 done, int usec)
{
	int error;

	error = handshake(ft313, ptr, mask, done, usec);
	if (error) {
		ft313_halt(ft313);
		DEBUG_MSG("Set HCD to Halt state\n");
		ft313_to_hcd(ft313)->state = HC_STATE_HALT;
		ERROR_MSG("force halt; handshake %p %08x %08x -> %d\n",
			ptr, mask, done, error);
	}

	return error;
}


/* reset a non-running (STS_HALT == 1) controller */
static int ft313_reset (struct ft313_hcd *ft313)
{
	int	retval;

	FUN_ENTRY();

	u32	command = ft313_reg_read32(ft313, &ft313->regs->command);

	/* If the EHCI debug controller is active, special care must be
	 * taken before and after a host controller reset */
//	if (ehci->debug && !dbgp_reset_prep())
//		ehci->debug = NULL;

	command |= HC_RESET;
	// dbg_cmd (ft313, "reset", command);
	DEBUG_MSG("Reset FT313\n");
	ft313_reg_write32(ft313, command, &ft313->regs->command);
	ft313_to_hcd(ft313)->state = HC_STATE_HALT;
	ft313->next_statechange = jiffies;
	retval = handshake (ft313, &ft313->regs->command,
			    HC_RESET, 0, 250 * 1000);

/*
	if (ehci->has_hostpc) {
		ft313_reg_write32(ehci, USBMODE_EX_HC | USBMODE_EX_VBPS,
			(u32 __iomem *)(((u8 *)ehci->regs) + USBMODE_EX));
		ft313_reg_write32(ehci, TXFIFO_DEFAULT,
			(u32 __iomem *)(((u8 *)ehci->regs) + TXFILLTUNING));
	}
	if (retval)
		return retval;

	if (ehci_is_TDI(ehci))
		tdi_reset (ehci);

	if (ehci->debug)
		dbgp_external_startup();
*/
	FUN_EXIT();

	return retval;
}

/* idle the controller (from running) */
static void ft313_quiesce (struct ft313_hcd *ft313)
{
	u32	temp;
	FUN_ENTRY();

#ifdef DEBUG
	if (!HC_IS_RUNNING (ehci_to_hcd(ehci)->state))
		BUG ();
#endif

	/* wait for any schedule enables/disables to take effect */
	temp = ft313_reg_read32(ft313, &ft313->regs->command) << 10;
	temp &= ASCH_STS | PSCH_STS;
	if (handshake_on_error_set_halt(ft313, &ft313->regs->status,
					ASCH_STS | PSCH_STS, temp, 16 * 125)) {
		FUN_EXIT();
		return;
	}

	/* then disable anything that's still active */
	temp = ft313_reg_read32(ft313, &ft313->regs->command);
	temp &= ~(ASCH_EN | INT_OAAD | PSCH_EN);
	ft313_reg_write32(ft313, temp, &ft313->regs->command);

	/* hardware can take 16 microframes to turn off ... */
	handshake_on_error_set_halt(ft313, &ft313->regs->status,
				    ASCH_STS | PSCH_STS, 0, 16 * 125);
	FUN_EXIT();
}


static void qh_link_async (struct ft313_hcd *ft313, struct ehci_qh *qh);
static void end_unlink_async (struct ft313_hcd *ft313);
static void ft313_work (struct ft313_hcd *ft313);
static int ft313_urb_enqueue_next (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	gfp_t			mem_flags
);
static void
timer_action(struct ft313_hcd *ft313, enum ft313_timer_action action);

static void free_cached_lists(struct ft313_hcd *ft313);

#include "ft313-mem.c"
#include "ft313-hub.c"
#include "ft313-q.c"
#include "ft313-sched.c"

/*-------------------------------------------------------------------------*/

static void ft313_iaa_watchdog(unsigned long param)
{
	FUN_ENTRY();

	struct ft313_hcd	*ft313 = (struct ft313_hcd *) param;
	unsigned long		flags;

	spin_lock_irqsave (&ft313->lock, flags);

	/* Lost IAA irqs wedge things badly; seen first with a vt8235.
	 * So we need this watchdog, but must protect it against both
	 * (a) SMP races against real IAA firing and retriggering, and
	 * (b) clean HC shutdown, when IAA watchdog was pending.
	 */
	if (ft313->reclaim
			&& !timer_pending(&ft313->iaa_watchdog)
			&& HC_IS_RUNNING(ft313_to_hcd(ft313)->state)) {
		u32 cmd, status;

		/* If we get here, IAA is *REALLY* late.  It's barely
		 * conceivable that the system is so busy that CMD_IAAD
		 * is still legitimately set, so let's be sure it's
		 * clear before we read STS_IAA.  (The HC should clear
		 * CMD_IAAD when it sets STS_IAA.)
		 */
		cmd = ft313_reg_read32(ft313, &ft313->regs->command);
		if (cmd & INT_OAAD)
			ft313_reg_write32(ft313, cmd & ~INT_OAAD,
					&ft313->regs->command);

		/* If IAA is set here it either legitimately triggered
		 * before we cleared IAAD above (but _way_ late, so we'll
		 * still count it as lost) ... or a silicon erratum:
		 * - VIA seems to set IAA without triggering the IRQ;
		 * - IAAD potentially cleared without setting IAA.
		 */
		status = ft313_reg_read32(ft313, &ft313->regs->status);
		if ((status & INT_OAA) || !(cmd & INT_OAAD)) {
			COUNT (ft313->stats.lost_iaa);
			ft313_reg_write32(ft313, INT_OAA, &ft313->regs->status);
		}

//		ehci_vdbg(ehci, "IAA watchdog: status %x cmd %x\n",
//				status, cmd);
		end_unlink_async(ft313);
	}

	spin_unlock_irqrestore(&ft313->lock, flags);

	FUN_EXIT();
}

static void ft313_watchdog(unsigned long param)
{
	FUN_ENTRY();

	struct ft313_hcd	*ft313 = (struct ft313_hcd *) param;
	unsigned long		flags;

	spin_lock_irqsave(&ft313->lock, flags);

	/* stop async processing after it's idled a bit */
	if (test_bit (TIMER_ASYNC_OFF, &ft313->actions)) {
		DEBUG_MSG("About to stop async scheduling\n");
		start_unlink_async (ft313, ft313->async);
	}

	/* ehci could run by timer, without IRQs ... */
	ft313_work (ft313);

	spin_unlock_irqrestore (&ft313->lock, flags);

	FUN_EXIT();
}



/* On some systems, leaving remote wakeup enabled prevents system shutdown.
 * The firmware seems to think that powering off is a wakeup event!
 * This routine turns off remote wakeup and everything else, on all ports.
 */
static void ft313_turn_off_all_ports(struct ft313_hcd *ft313)
{
	// FT313 has one port only!
	FUN_ENTRY();
	ft313_reg_write32(ft313, PORT_RWC_BITS,
				&ft313->regs->port_status[0]);
	FUN_EXIT();
}

/*
 * Halt HC, turn off all ports, and let the BIOS use the companion controllers.
 * Should be called with ehci->lock held.
 */
static void ft313_silence_controller(struct ft313_hcd *ft313)
{
	FUN_ENTRY();
	ft313_halt(ft313);
	ft313_turn_off_all_ports(ft313);

	/* make BIOS/etc use companion controller during reboot */
//	ft313_reg_write32(ft313, 0, &ft313->regs->configured_flag);

	/* unblock posted writes */
//	ehci_readl(ft313, &ft313->regs->configured_flag);
	FUN_EXIT();
}

/* ft313_shutdown kick in for silicon on any bus (not just pci, etc).
 * This forcibly disables dma and IRQs, helping kexec and other cases
 * where the next system software may expect clean state.
 */
static void ft313_shutdown(struct usb_hcd *hcd)
{
	struct ft313_hcd *ft313 = hcd_to_ft313(hcd);

	del_timer_sync(&ft313->watchdog);
	del_timer_sync(&ft313->iaa_watchdog);
#ifdef PORT_RESET_TIME_WORKAROUND
	del_timer_sync(&ft313->port_reset_timer);
#endif

	spin_lock_irq(&ft313->lock);
	ft313_silence_controller(ft313);
	spin_unlock_irq(&ft313->lock);
}

#if 0
static void ehci_port_power (struct ft313_hcd *ft313, int is_on)
{
	unsigned port;

	// FT313 doest not have port power control bit in hcs_params
	if (!HCS_PPC (ft313->hcs_params))
		return;

	ehci_dbg (ft313, "...power%s ports...\n", is_on ? "up" : "down");
	for (port = HCS_N_PORTS (ft313->hcs_params); port > 0; )
		(void) ehci_hub_control(ehci_to_hcd(ft313),
				is_on ? SetPortFeature : ClearPortFeature,
				USB_PORT_FEAT_POWER,
				port--, NULL, 0);
	/* Flush those writes */
	ft313_reg_read32(ft313, &ft313->regs->command);
	msleep(20);
}
#endif

/*-------------------------------------------------------------------------*/

/*
 * ehci_work is called from some interrupts, timers, and so on.
 * it calls driver completion functions, after dropping ft313->lock.
 */
static void ft313_work (struct ft313_hcd *ft313)
{
	FUN_ENTRY();

	timer_action_done (ft313, TIMER_IO_WATCHDOG);

	/* another CPU may drop ft313->lock during a schedule scan while
	 * it reports urb completions.  this flag guards against bogus
	 * attempts at re-entrant schedule scanning.
	 */
	if (ft313->scanning)
		return;
	ft313->scanning = 1;
	scan_async (ft313);
	if (ft313->next_uframe != -1)
		scan_periodic (ft313);

	ft313->scanning = 0;

	/* the IO watchdog guards against hardware or driver bugs that
	 * misplace IRQs, and should let us run completely without IRQs.
	 * such lossage has been observed on both VT6202 and VT8235.
	 */
	if (HC_IS_RUNNING (ft313_to_hcd(ft313)->state) &&
			(ft313->async->qh_next.ptr != NULL ||
			 ft313->periodic_sched != 0))
		timer_action (ft313, TIMER_IO_WATCHDOG);

	FUN_EXIT();
}

static struct file_operations ft313_fops;
#ifdef USE_UDEV
static DEVICE_ATTR(ftdi, S_IWUSR | S_IRUGO, NULL, NULL);
#endif
/*
 * Called when the ft313_hcd module is removed.
 */
static void ft313_stop (struct usb_hcd *hcd)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);

	//ehci_dbg (ft313, "stop\n");

#ifdef USE_UDEV
	device_remove_file(ftdi_device, &dev_attr_ftdi);
	device_destroy(ftdi_class, MKDEV(ft313->ft313_cdev_major, 0));
	class_unregister(ftdi_class);
	class_destroy(ftdi_class);
#endif
	// deregister char device
	cdev_del(&ft313->ft313_cdev);

	// return the major number allocated to system
	unregister_chrdev_region(ft313->ft313_cdev_major, ft313->ft313_cdev_count);

	// deallocate workqueue
	flush_workqueue(ft313->wakeup_wq);
	destroy_workqueue(ft313->wakeup_wq);

	/* no more interrupts ... */
	del_timer_sync(&ft313->watchdog);
	del_timer_sync(&ft313->iaa_watchdog);
#ifdef PORT_RESET_TIME_WORKAROUND
	del_timer_sync(&ft313->port_reset_timer);
#endif

	spin_lock_irq(&ft313->lock);
	if (HC_IS_RUNNING (hcd->state))
		ft313_quiesce (ft313);

	ft313_silence_controller(ft313);
	ft313_reset (ft313);
	spin_unlock_irq(&ft313->lock);

//	remove_companion_file(ehci);
//	remove_debug_files (ehci);

	/* root hub is shut down separately (first, when possible) */
	spin_lock_irq (&ft313->lock);
	if (ft313->async)
		ft313_work (ft313);
	spin_unlock_irq (&ft313->lock);
	ft313_mem_cleanup (ft313);

	// Shutdown V-Bus as well
	u16 tmp;
	tmp = ft313_reg_read16(ft313, &ft313->cfg->config);
	ft313_reg_write16(ft313, VBUS_OFF | tmp, &ft313->cfg->config);

//	if (ehci->amd_pll_fix == 1)
//		usb_amd_dev_put();

#ifdef	EHCI_STATS
	ehci_dbg (ehci, "irq normal %ld err %ld reclaim %ld (lost %ld)\n",
		ehci->stats.normal, ehci->stats.error, ehci->stats.reclaim,
		ehci->stats.lost_iaa);
	ehci_dbg (ehci, "complete %ld unlink %ld\n",
		ehci->stats.complete, ehci->stats.unlink);
#endif

//	dbg_status (ehci, "ehci_stop completed",
//		    ft313_reg_read32(ehci, &ehci->regs->status));
}

/* one-time init, only for memory state */
static int ft313_init(struct usb_hcd *hcd)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313(hcd);
	u32			temp;
	int			retval;
	u32			hcc_params;
	struct ehci_qh_hw	*hw;

	spin_lock_init(&ft313->lock);

	/*
	 * keep io watchdog by default, those good HCDs could turn off it later
	 */

	ft313->need_io_watchdog = 1;
	init_timer(&ft313->watchdog);
	ft313->watchdog.function = ft313_watchdog;
	ft313->watchdog.data = (unsigned long) ft313;

	init_timer(&ft313->iaa_watchdog);
	ft313->iaa_watchdog.function = ft313_iaa_watchdog;
	ft313->iaa_watchdog.data = (unsigned long) ft313;

#ifdef PORT_RESET_TIME_WORKAROUND
	init_timer(&ft313->port_reset_timer);
	ft313->port_reset_timer.function = ft313_clear_port_reset;
	ft313->port_reset_timer.data = (unsigned long) ft313;
#endif

	hcc_params = ft313_reg_read32(ft313, &ft313->caps->hcc_params);

	/*
	 * hw default: 1K periodic list heads, one per frame.
	 * periodic_size can shrink by USBCMD update if hcc_params allows.
	 */
	ft313->periodic_size = DEFAULT_I_TDPS;
	INIT_LIST_HEAD(&ft313->cached_itd_list);
	INIT_LIST_HEAD(&ft313->cached_sitd_list);

	if (PROG_FR_LIST_FLAG(hcc_params)) {
		/* periodic schedule size can be smaller than default */
		switch (EHCI_TUNE_FLS) {
		case 0: ft313->periodic_size = 1024; break;
		case 1: ft313->periodic_size = 512; break;
		case 2: ft313->periodic_size = 256; break;
		default:	BUG();
		}
	}
	if ((retval = ft313_mem_init(ft313, GFP_KERNEL)) < 0)
		return retval;

	/* controllers may cache some of the periodic schedule ... */
#if 0 // FixMe: Faraday IP does not mention this feature at all
	if (HCC_ISOC_CACHE(hcc_params))		// full frame cache
		ft313->i_thresh = 2 + 8;
	else					// N microframes cached
		ft313->i_thresh = 2 + HCC_ISOC_THRES(hcc_params);
#endif
	ft313->reclaim = NULL;
	ft313->next_uframe = -1;
	ft313->clock_frame = -1;
#if 1 // Disable only for irq test
	/*
	 * dedicate a qh for the async ring head, since we couldn't unlink
	 * a 'real' qh without stopping the async schedule [4.8].  use it
	 * as the 'reclamation list head' too.
	 * its dummy is used in hw_alt_next of many tds, to prevent the qh
	 * from automatically advancing to the next td after short reads.
	 */
	ft313->async->qh_next.qh = NULL;
	hw = ft313->async->hw;
//	hw->hw_next = QH_NEXT(ehci, ehci->async->qh_dma);
	hw->hw_next = QH_NEXT(ft313, ft313->async->qh_ft313);
	hw->hw_info1 = cpu_to_hc32(ft313, QH_HEAD);
	hw->hw_token = cpu_to_hc32(ft313, QTD_STS_HALT);
	hw->hw_qtd_next = EHCI_LIST_END(ft313);
	ft313->async->qh_state = QH_STATE_LINKED;
//	hw->hw_alt_next = QTD_NEXT(ehci, ehci->async->dummy->qtd_dma);
	hw->hw_alt_next = QTD_NEXT(ft313, ft313->async->dummy->qtd_ft313);
	// Write to FT313 on-chip memory
	ft313_mem_write(ft313, hw, sizeof(struct ehci_qh_hw), ft313->async->qh_ft313);
#endif

	/* clear interrupt enables, set irq latency */
	if (log2_irq_thresh < 0 || log2_irq_thresh > 6)
		log2_irq_thresh = 0;
	temp = 1 << (16 + log2_irq_thresh);
/*	if (HCC_PER_PORT_CHANGE_EVENT(hcc_params)) {
		ehci->has_ppcd = 1;
		ehci_dbg(ehci, "enable per-port change event\n");
		temp |= CMD_PPCEE;
	}
*/
	if (PROG_FR_LIST_FLAG(hcc_params)) {
		/* HW default park == 3, on hardware that supports it (like
		 * NVidia and ALI silicon), maximizes throughput on the async
		 * schedule by avoiding QH fetches between transfers.
		 *
		 * With fast usb storage devices and NForce2, "park" seems to
		 * make problems:  throughput reduction (!), data errors...
		 */
		if (park) {
			park = min(park, (unsigned) 3);
			temp |= ASYN_PK_EN;
			temp |= park << 8;
		}
		//ehci_dbg(ehci, "park %d\n", park);
	}
	if (PROG_FR_LIST_FLAG(hcc_params)) {
		/* periodic schedule size can be smaller than default */
		temp &= ~(3 << 2);
		temp |= (EHCI_TUNE_FLS << 2);
	}

//	if (HCC_LPM(hcc_params)) {
		/* support link power management EHCI 1.1 addendum */
//		ehci_dbg(ehci, "support lpm\n");
//		ehci->has_lpm = 1;
//		if (hird > 0xf) {
//			ehci_dbg(ehci, "hird %d invalid, use default 0",
//			hird);
//			hird = 0;
//		}
//		temp |= hird << 24;
//	}

	ft313->command = temp; // FixMe: quite a few fields not available in Faraday IP, check!

	/* Accept arbitrarily long scatter-gather lists */
	/* FixMe: HCD_LOCAL_MEM flag is questionable, check later! */
//	if (!(hcd->driver->flags & HCD_LOCAL_MEM))
//		hcd->self.sg_tablesize = ~0;
	return 0;
}

/* start HC running; it's halted, ft313_init() has been run (once) */
static int ft313_run (struct usb_hcd *hcd)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);
	int			retval;
	u32			temp;
	u32			hcc_params;

	FUN_ENTRY();

	hcd->uses_new_polling = 1;

	/* EHCI spec section 4.1 */
	/*
	 * TDI driver does the ehci_reset in their reset callback.
	 * Don't reset here, because configuration settings will
	 * vanish.
	 */
	if (!ehci_is_TDI(ft313) && (retval = ft313_reset(ft313)) != 0) {
		ft313_mem_cleanup(ft313);
		return retval;
	}

//	ehci_writel(ehci, ehci->periodic_dma, &ehci->regs->frame_list);
	ft313_reg_write32(ft313, ft313->periodic_ft313, &ft313->regs->frame_list);
	// FixMe: base of sync list is hard-coded as 0; it requies 4k aligment and 0 is best value
//	ehci_writel(ehci, (u32)ehci->async->qh_dma, &ehci->regs->async_next);
#if 1 // Disable only for irq test
	ft313_reg_write32(ft313, ft313->async->qh_ft313, &ft313->regs->async_next);
#endif
	/*
	 * hcc_params controls whether ehci->regs->segment must (!!!)
	 * be used; it constrains QH/ITD/SITD and QTD locations.
	 * pci_pool consistent memory always uses segment zero.
	 * streaming mappings for I/O buffers, like pci_map_single(),
	 * can return segments above 4GB, if the device allows.
	 *
	 * NOTE:  the dma mask is visible through dma_supported(), so
	 * drivers can pass this info along ... like NETIF_F_HIGHDMA,
	 * Scsi_Host.highmem_io, and so forth.  It's readonly to all
	 * host side drivers though.
	 */
	hcc_params = ft313_reg_read32(ft313, &ft313->caps->hcc_params);
/*	if (HCC_64BIT_ADDR(hcc_params)) {
		ft313_reg_write32(ehci, 0, &ehci->regs->segment);
#if 0
// this is deeply broken on almost all architectures
		if (!dma_set_mask(hcd->self.controller, DMA_BIT_MASK(64)))
			ehci_info(ehci, "enabled 64bit DMA\n");
#endif
	}
*/

	// Philips, Intel, and maybe others need CMD_RUN before the
	// root hub will detect new devices (why?); NEC doesn't
	ft313->command &= ~(INT_OAAD|PSCH_EN|ASCH_EN|HC_RESET);
	ft313->command |= ASYN_PK_EN | (3<<8); //From Faraday reference code!
	ft313->command |= RS;
	ft313_reg_write32(ft313, ft313->command, &ft313->regs->command);
//	dbg_cmd (ehci, "init", ehci->command);

	/*
	 * Start, enabling full USB 2.0 functionality ... usb 1.1 devices
	 * are explicitly handed to companion controller(s), so no TT is
	 * involved with the root hub.  (Except where one is integrated,
	 * and there's no companion controller unless maybe for USB OTG.)
	 *
	 * Turning on the CF flag will transfer ownership of all ports
	 * from the companions to the EHCI controller.  If any of the
	 * companions are in the middle of a port reset at the time, it
	 * could cause trouble.  Write-locking ehci_cf_port_reset_rwsem
	 * guarantees that no resets are in progress.  After we set CF,
	 * a short delay lets the hardware catch up; new resets shouldn't
	 * be started before the port switching actions could complete.
	 */
	hcd->state = HC_STATE_RUNNING;

	ft313_reg_read32(ft313, &ft313->regs->command);	/* unblock posted writes */
	msleep(5);

	ft313->last_periodic_enable = ktime_get_real();

	temp = HCIVERSION(ft313_reg_read32(ft313, &ft313->caps->hc_capbase));
/*	ehci_info (ehci,
		"USB %x.%x started, EHCI %x.%02x%s\n",
		((ehci->sbrn & 0xf0)>>4), (ehci->sbrn & 0x0f),
		temp >> 8, temp & 0xff,
		ignore_oc ? ", overcurrent ignored" : "");
*/
	ft313_reg_write32(ft313, INTR_MASK,
		    &ft313->regs->intr_enable); /* Turn On Interrupts */

#ifdef USB_SOF_INTR
	ft313_reg_write16(ft313, 0x0002, &ft313->cfg->hc_int_en); // Turn on SOF Interrupt
//	ft313_reg_read16(ft313, &ft313->cfg->hw_mode);
//	mdelay(20);
	//ft313_reg_read16(ft313, &ft313->cfg->hc_int_sts);	// Check SoF interrupt status
#else
	ft313_reg_write16(ft313, 0x0000, &ft313->cfg->hc_int_en); // FixMe: Explicitly disable FT313 interrupt
#endif
	/* GRR this is run-once init(), being done every time the HC starts.
	 * So long as they're part of class devices, we can't do it init()
	 * since the class device isn't created that early.
	 */
	//create_debug_files(ft313);
	//create_companion_file(ft313);

	// Register a charater device
	ft313->ft313_cdev_count = 1;
#ifdef USE_UDEV
	retval = alloc_chrdev_region(&ft313->ft313_cdev_major, 0, ft313->ft313_cdev_count, "ft313_hc");
#else
	ft313->ft313_cdev_major = FT313_MAJOR;
	retval = register_chrdev_region(ft313->ft313_cdev_major, ft313->ft313_cdev_count, "ft313_hc");
#endif
	if (retval) {
		FUN_EXIT();
		return retval;
	}
	int devno = MKDEV(ft313->ft313_cdev_major, 0);

	cdev_init(&ft313->ft313_cdev, &ft313_fops);
	ft313->ft313_cdev.owner = THIS_MODULE;
	retval = cdev_add(&ft313->ft313_cdev, devno, ft313->ft313_cdev_count);

	if (retval) {
		ALERT_MSG("Char device register fails\n");
		FUN_EXIT();
		return retval;
	}

#ifdef USE_UDEV
	// Create device file under "/dev"
	ftdi_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(ftdi_class)) {
		ALERT_MSG("failed to register device class '%s'\n", CLASS_NAME);
		retval = PTR_ERR(ftdi_class);
		goto failed_classreg;
	}

	/* With a class, the easiest way to instantiate a device is to call device_create() */
	ftdi_device = device_create(ftdi_class, NULL, devno, NULL, CLASS_NAME "_" DEVICE_NAME);
	if (IS_ERR(ftdi_device)) {
		ALERT_MSG("failed to create device '%s_%s'\n", CLASS_NAME, DEVICE_NAME);
		retval = PTR_ERR(ftdi_device);
		goto failed_devreg;
	}

	retval = device_create_file(ftdi_device, &dev_attr_ftdi);
	if (retval < 0) {
		ALERT_MSG("failed to create write /sys endpoint - continuing without\n");
	}
#endif
	FUN_EXIT();
	return 0;

#ifdef USE_UDEV
failed_devreg:
	class_unregister(ftdi_class);
	class_destroy(ftdi_class);
failed_classreg:
	cdev_del(&ft313->ft313_cdev);

	FUN_EXIT();
	return -1;
#endif
}

static irqreturn_t ft313_irq (struct usb_hcd *hcd)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);
	u32			status, masked_status, pcd_status = 0, cmd;
	int			bh;

	u32 temp = 0;
	u16 tmp = 0, hc_int_sts = 0, hc_int_en = 0;
	int ft313_spec_int = 0, count = 0;
	u32 static irq_count = 0;

	FUN_ENTRY();

	spin_lock (&ft313->lock);

	DEBUG_MSG("ft313 irq handler is called\n");

	hc_int_sts = ft313_reg_read16(ft313, &ft313->cfg->hc_int_sts);
	hc_int_en = ft313_reg_read16(ft313, &ft313->cfg->hc_int_en);

	if ((hc_int_sts & hc_int_en) != 0) { // FT313 HC Interrupt
		// Clean FT313 Interrupt
		DEBUG_MSG("FT313 Chip Interrupt is 0x%X, Enable bitmap is 0x%X\n", hc_int_sts, hc_int_en);
		ft313_reg_write16(ft313, hc_int_sts, &ft313->cfg->hc_int_sts);

		tmp = hc_int_sts & hc_int_en; // Mask out these disabled

		if (tmp & OCINT_EN) { // OC happens
			ALERT_MSG("Over current happened!\n");

			tmp = ft313_reg_read16(ft313, &ft313->cfg->config);
			//Turn VBUS off
			ft313_reg_write16(ft313, VBUS_OFF | tmp, &ft313->cfg->config);

			spin_unlock(&ft313->lock);
			FUN_EXIT();
			return IRQ_HANDLED;
		}

		if (tmp & (WAKEUPINT | REMOTEWKINT)) {
			DEBUG_MSG("Device connect/disconnect or remote wakeup happens during suspend\n");

			if (0 == (tmp & CLKREADY)) {
				ALERT_MSG("Wake up without clock ready set, strange?\n");
			}

			PREPARE_WORK(&ft313->wakeup_work, ft313_wakeup_wq_handler);
			if (0 == queue_work(ft313->wakeup_wq, &ft313->wakeup_work)) {
				ALERT_MSG("Work item is already in queue\n");
			}

			spin_unlock(&ft313->lock);
			FUN_EXIT();
			return IRQ_HANDLED;
		}

		if (tmp & CLKREADY) { // Chip just wakeup
			tmp = ft313_reg_read16(ft313, &ft313->cfg->config);
			ALERT_MSG("Current CONFIG register value is 0x%X\n", tmp);

			// Turn on EHCI core
			temp = ft313_reg_read32(ft313, &ft313->regs->eof_time);

			while (!(temp & U_SUSP_N) && count++ < 10) {
				temp |= U_SUSP_N; // turn on transeiver
				ft313_reg_write32(ft313, temp, &ft313->regs->eof_time);
				udelay(10);
				temp = ft313_reg_read32(ft313, &ft313->regs->eof_time);
			}

			tmp = hc_int_sts & hc_int_en;

			spin_unlock(&ft313->lock);
			FUN_EXIT();
			return IRQ_HANDLED;
		}

		ft313_spec_int = 1;
	}

#ifdef USB_SOF_INTR
//	temp = ft313_reg_read16(ft313, &ft313->cfg->hc_int_sts);
	temp = ioread16(&ft313->cfg->hc_int_sts);
	if ((temp & ft313->cfg->hc_int_en) != 0) {
		// Clean FT313 Interrupt
		//DEBUG_MSG("FT313 Chip Interrupt is 0x%X\n", temp);

//		ft313_reg_write16(ft313, temp, &ft313->cfg->hc_int_sts);
		iowrite16(temp, &ft313->cfg->hc_int_sts);
		ft313_spec_int = 1;
	}
	status = ioread32(&ft313->regs->status);
#else
	// When no SOF Interrupt, we can use reg read with log as interrupt
	// will not come so frequently
	status = ft313_reg_read32(ft313, &ft313->regs->status);
	if (status & HCHALTED) {
		DEBUG_MSG("HC is Halted!!!\n");
	}
#endif

	/* e.g. cardbus physical eject */
	if (status == ~(u32) 0) {
		ft313_dbg (ft313, "device removed\n");
		goto dead;
	}

	/* Shared IRQ? */
	masked_status = status & INTR_MASK;
	if (!masked_status || unlikely(hcd->state == HC_STATE_HALT)) {
		spin_unlock(&ft313->lock);
		if (ft313_spec_int == 0) {
			FUN_EXIT();
			return IRQ_NONE;
		} else {
			FUN_EXIT();
			return IRQ_HANDLED;
		}
	}

	DEBUG_MSG("ft313 EHCI interrupt happened for No. %d time\n", ++irq_count);

another_int_generated:

	/* clear (just) interrupts */
	do {
		ft313_reg_write32(ft313, masked_status, &ft313->regs->status);
		DEBUG_MSG("Try to clean interrupt use 0x%X\n", masked_status);
		temp = ft313_reg_read32(ft313, &ft313->regs->status);
		DEBUG_MSG("Status register now is 0x%X\n", temp);
	} while ((temp & masked_status) != 0);

	DEBUG_MSG("Interrupt cleaned\n");

	cmd = ft313_reg_read32(ft313, &ft313->regs->command);
	bh = 0;

#ifdef	VERBOSE_DEBUG
	/* unrequested/ignored: Frame List Rollover */
	dbg_status (ft313, "irq", status);
#endif

	/* INT, ERR, and IAA interrupt rates can be throttled */

	/* normal [4.15.1.2] or error [4.15.1.1] completion */
	if (likely ((status & (USB_INT | USBERR_INT)) != 0)) {
		if (likely ((status & USBERR_INT) == 0))
			COUNT (ft313->stats.normal);
		else
			COUNT (ft313->stats.error);
		bh = 1;
	}

	/* complete the unlinking of some qh [4.15.2.3] */
	if (status & INT_OAA) {
		DEBUG_MSG("Interrupt on Asynchronous Advance happened\n");
		/* guard against (alleged) silicon errata */
		if (cmd & INT_OAAD) {
			ft313_reg_write32(ft313, cmd & ~INT_OAAD,
					&ft313->regs->command);
			ALERT_MSG("IAA with IAAD still set?\n");
		}
		if (ft313->reclaim) {
			COUNT(ft313->stats.reclaim);
			end_unlink_async(ft313);
		} else
			ERROR_MSG("IAA with nothing to reclaim?\n");
	}

	/* remote wakeup [4.3.1] */
	if (status & PO_CHG_DET) {
		unsigned	i = HCS_N_PORTS (ft313->hcs_params);

		DEBUG_MSG("Got port status change interrupt\n");

		/* kick root hub later */
		pcd_status = status;

		/* resume root hub? */
		if (!(cmd & RS))
			usb_hcd_resume_root_hub(hcd);

		while (i--) {
			int pstatus;

			pstatus = ft313_reg_read32(ft313,
					 &ft313->regs->port_status[i]);

			ALERT_MSG("Port status reg is 0x%X\n", pstatus);

			if (pstatus & CONN_STS)
				ALERT_MSG("Device plugged in\n");
			else
				ALERT_MSG("Device removed \n");

//			if (pstatus & PORT_OWNER)
//				continue;
			if (!(test_bit(i, &ft313->suspended_ports)		&&
			      ((pstatus & F_PO_RESM) || !(pstatus & PO_SUSP))   &&
			      (pstatus & PO_EN) &&
			      ft313->reset_done[i] == 0))
				continue;

			/* start 20 msec resume signaling from this port,
			 * and make khubd collect PORT_STAT_C_SUSPEND to
			 * stop that signaling.  Use 5 ms extra for safety,
			 * like usb_port_resume() does.
			 */
			DEBUG_MSG("Set resume timer\n");
			ft313->reset_done[i] = jiffies + msecs_to_jiffies(25);
			DEBUG_MSG("port %d remote wakeup\n", i + 1);
			mod_timer(&hcd->rh_timer, ft313->reset_done[i]);
		}

	}

	/* PCI errors [4.15.2.4] */
	if (unlikely ((status & H_SYSERR) != 0)) {
		ALERT_MSG("fatal error\n");
		//dbg_cmd(ft313, "fatal", cmd);
		//dbg_status(ft313, "fatal", status);
		ft313_halt(ft313);
dead:
		ft313_reset(ft313);
//		ft313_reg_write32(ft313, 0, &ft313->regs->configured_flag);
		usb_hc_died(hcd);
		/* generic layer kills/unlinks all urbs, then
		 * uses ehci_stop to clean up the rest
		 */
		bh = 1;
	}

	if (bh)
		ft313_work (ft313);
//	spin_unlock (&ft313->lock);
//	if (pcd_status)
//		usb_hcd_poll_rh_status(hcd);

// Interrupt workaround start
#if 1
	status = ft313_reg_read32(ft313, &ft313->regs->status);
	masked_status = status & INTR_MASK;

	if (0 != masked_status) {
		DEBUG_MSG("Another interrupt come during processing with masked value 0x%X\n", masked_status);
//		spin_lock (&ft313->lock);
		goto another_int_generated;
	}
#endif
// Interrupt workaround end

	spin_unlock (&ft313->lock);
	if (pcd_status)
		usb_hcd_poll_rh_status(hcd);

	FUN_EXIT();

	return IRQ_HANDLED;
}

/*-------------------------------------------------------------------------*/

/*
 * non-error returns are a promise to giveback() the urb later
 * we drop ownership so next owner (or urb unlink) can get it
 *
 * urb + dev is in hcd.self.controller.urb_list
 * we're queueing TDs onto software and hardware lists
 *
 * hcd-specific init for hcpriv hasn't been done yet
 *
 * NOTE:  control, bulk, and interrupt share the same code to append TDs
 * to a (possibly active) QH, and the same QH scanning code.
 */
static int ft313_urb_enqueue (
	struct usb_hcd	*hcd,
	struct urb	*urb,
	gfp_t		mem_flags
) {
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);
	struct list_head	qtd_list;
	struct ehci_qh		*qh;
	struct ehci_iso_stream	*stream;

	int val;

	FUN_ENTRY();

	if (0 != in_interrupt()) {
		DEBUG_MSG("urb enqueue within interrupt context!\n");
	}

	DEBUG_MSG("urb 0x%X is issued with tranfer buffer length %d from 0x%X\n", urb, urb->transfer_buffer_length, urb->transfer_buffer);
	DEBUG_MSG("And targeted for EP 0x%X at Addr %d\n", urb->ep->desc.bEndpointAddress, usb_pipedevice(urb->pipe));

	if (usb_pipetype (urb->pipe) == PIPE_BULK) { // queue for bulk transfer
		/* if there is urb still under processing, we just queue the new urb */
		qh = (struct ehci_qh*)(urb->ep->hcpriv);

		if (qh != NULL) {
			struct qh_urb_queue_item *qh_urb_q_item;

			if (qh->urb != NULL) { // There is still urb pending
				DEBUG_MSG("Current pending urb for qH 0x%X is 0x%X, have to Q new urb 0x%X\n", qh->qh_ft313, qh->urb, urb);
				qh_urb_q_item = kmalloc(sizeof(struct qh_urb_queue_item), mem_flags);
				if (NULL == qh_urb_q_item) {
					FUN_EXIT();
					return -ENOMEM;
				}
				qh_urb_q_item->urb = urb;
				INIT_LIST_HEAD(&qh_urb_q_item->urb_list);
				list_add_tail(&qh_urb_q_item->urb_list, &qh->urb_list);
				DEBUG_MSG("urb 0x%X is saved for qH 0x%X (0x%X)\n", urb, qh->qh_ft313, qh);

				if (qh->urb == NULL) {
					ALERT_MSG("This should not happen, if it does, reverse queuing\n");
					list_del(&qh_urb_q_item->urb_list);
					kfree(qh_urb_q_item);
					ALERT_MSG("urb 0x%X restored for qH 0x%X (0x%X)\n", urb, qh->qh_ft313, qh);
				} else {
					FUN_EXIT();
					return 0;
				}
			}
		}

	} else if (usb_pipetype (urb->pipe) == PIPE_ISOCHRONOUS) { // queue for iso transfer
		unsigned		epnum;
		struct usb_host_endpoint *ep;

		epnum = usb_pipeendpoint (urb->pipe);
		if (usb_pipein(urb->pipe))
			ep = urb->dev->ep_in[epnum];
		else
			ep = urb->dev->ep_out[epnum];

		stream = ep->hcpriv;

		if (stream != NULL) {
			struct iso_urb_queue_item *iso_urb_q_item;

			if (stream->urb != NULL) { // There is urb under execution
				DEBUG_MSG("Current pending urb for stream 0x%X is 0x%X, have to Q new urb 0x%X\n", stream, stream->urb, urb);
				iso_urb_q_item = kmalloc(sizeof(struct iso_urb_queue_item), mem_flags);
				if (NULL == iso_urb_q_item) {
					FUN_EXIT();
					return -ENOMEM;
				}
				iso_urb_q_item->urb = urb;
				iso_urb_q_item->urb_buffer = 0;

				INIT_LIST_HEAD(&iso_urb_q_item->urb_list);
				list_add_tail(&iso_urb_q_item->urb_list, &stream->urb_list);
				DEBUG_MSG("urb 0x%X is saved for stream 0x%X\n", urb, stream);

				FUN_EXIT();
				return 0;
			}

		}
	}

	INIT_LIST_HEAD (&qtd_list);

	switch (usb_pipetype (urb->pipe)) {
	case PIPE_CONTROL:
		/* qh_completions() code doesn't handle all the fault cases
		 * in multi-TD control transfers.  Even 1KB is rare anyway.
		 */
		if (urb->transfer_buffer_length > (16 * 1024))
			return -EMSGSIZE;
		/* FALLTHROUGH */
	/* case PIPE_BULK: */
	default:
		if (!qh_urb_transaction (ft313, urb, &qtd_list, mem_flags)) {
			FUN_EXIT();
			return -ENOMEM;
		}
		val = submit_async(ft313, urb, &qtd_list, mem_flags);
		FUN_EXIT();
		return val;

	case PIPE_INTERRUPT:
		DEBUG_MSG("Interrupt transfer issued\n");
		if (!qh_urb_transaction (ft313, urb, &qtd_list, mem_flags))
			return -ENOMEM;
		return intr_submit(ft313, urb, &qtd_list, mem_flags);

	case PIPE_ISOCHRONOUS:
		DEBUG_MSG("Isochronous transfer issued\n");
		if (urb->dev->speed == USB_SPEED_HIGH)
			return itd_submit (ft313, urb, mem_flags);
		else
			return sitd_submit (ft313, urb, mem_flags);
	}
}

static int ft313_urb_enqueue_next (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	gfp_t			mem_flags
) {
	struct list_head	qtd_list;
	int val;

	FUN_ENTRY();

	DEBUG_MSG("urb 0x%X is continuing be served\n", urb);

	INIT_LIST_HEAD (&qtd_list);

	switch (usb_pipetype (urb->pipe)) {
		case PIPE_CONTROL:
			/* qh_completions() code doesn't handle all the fault cases
			 * in multi-TD control transfers.  Even 1KB is rare anyway.
			 */
			ERROR_MSG("Should not come here as Control transfer is small!\n");
			FUN_EXIT();
			return -1;

			if (urb->transfer_buffer_length > (16 * 1024))
				return -EMSGSIZE;
			/* FALLTHROUGH */
			/* case PIPE_BULK: */
		default:
			if (!qh_urb_transaction (ft313, urb, &qtd_list, mem_flags)) {
				FUN_EXIT();
				return -ENOMEM;
			}
			val = submit_async_next(ft313, urb, &qtd_list, mem_flags);
			FUN_EXIT();
			return val;

		case PIPE_INTERRUPT:
			ERROR_MSG("Should not come here as Interrupt transfer is small!\n");
			FUN_EXIT();
			return -1;

			if (!qh_urb_transaction (ft313, urb, &qtd_list, mem_flags))
				return -ENOMEM;
			return intr_submit(ft313, urb, &qtd_list, mem_flags);

		case PIPE_ISOCHRONOUS:
			if (urb->dev->speed == USB_SPEED_HIGH)
				return itd_submit (ft313, urb, mem_flags);
			else
				return sitd_submit (ft313, urb, mem_flags);

	}
}


static void unlink_async (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	FUN_ENTRY();

	/* failfast */
	if (!HC_IS_RUNNING(ft313_to_hcd(ft313)->state) && ft313->reclaim)
		end_unlink_async(ft313);

	/* If the QH isn't linked then there's nothing we can do
	 * unless we were called during a giveback, in which case
	 * qh_completions() has to deal with it.
	 */
	if (qh->qh_state != QH_STATE_LINKED) {
		if (qh->qh_state == QH_STATE_COMPLETING)
			qh->needs_rescan = 1;
		FUN_EXIT();
		return;
	}

	/* defer till later if busy */
	if (ft313->reclaim) {
		struct ehci_qh		*last;

		for (last = ft313->reclaim;
				last->reclaim;
				last = last->reclaim)
			continue;
		qh->qh_state = QH_STATE_UNLINK_WAIT;
		last->reclaim = qh;

	/* start IAA cycle */
	} else
		start_unlink_async (ft313, qh);

	FUN_EXIT();
}

/* remove from hardware lists
 * completions normally happen asynchronously
 */

static int ft313_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);
	struct ehci_qh		*qh;
	unsigned long		flags;
	int			rc;

	FUN_ENTRY();
	DEBUG_MSG("urb to cancel is 0x%X\n", urb);

	spin_lock_irqsave (&ft313->lock, flags);
	rc = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (rc) {
		DEBUG_MSG("Skip processing\n");
		goto done;
	}

	switch (usb_pipetype (urb->pipe)) {
	// case PIPE_CONTROL:
	// case PIPE_BULK:
	default:
		qh = (struct ehci_qh *) urb->hcpriv;
		if (!qh)
			break;
		DEBUG_MSG("qH state is %d\n", qh->qh_state);
		switch (qh->qh_state) {
		case QH_STATE_LINKED:
		case QH_STATE_COMPLETING:
			unlink_async(ft313, qh);
			break;
		case QH_STATE_UNLINK:
		case QH_STATE_UNLINK_WAIT:
			/* already started */
			break;
		case QH_STATE_IDLE:
			/* QH might be waiting for a Clear-TT-Buffer */
			qh_completions(ft313, qh);
			break;
		}
		break;

	case PIPE_INTERRUPT:
		qh = (struct ehci_qh *) urb->hcpriv;
		if (!qh)
			break;
		switch (qh->qh_state) {
		case QH_STATE_LINKED:
		case QH_STATE_COMPLETING:
			intr_deschedule (ft313, qh);
			break;
		case QH_STATE_IDLE:
			qh_completions (ft313, qh);
			break;
		default:
			ERROR_MSG("bogus qh %p state %d\n",
					qh, qh->qh_state);
			goto done;
		}
		break;

	case PIPE_ISOCHRONOUS:
		// itd or sitd ...

		// wait till next completion, do it then.
		// completion irqs can wait up to 1024 msec,
		break;

	}
done:
	spin_unlock_irqrestore (&ft313->lock, flags);

	FUN_EXIT();
	return rc;
}

/*-------------------------------------------------------------------------*/

// bulk qh holds the data toggle

static void
ft313_endpoint_disable (struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);
	unsigned long		flags;
	struct ehci_qh		*qh, *tmp;

	/* ASSERT:  any requests/urbs are being unlinked */
	/* ASSERT:  nobody can be submitting urbs for this any more */
	FUN_ENTRY();
	DEBUG_MSG("And targeted for EP 0x%X \n", ep->desc.bEndpointAddress);
rescan:
	spin_lock_irqsave (&ft313->lock, flags);
	qh = ep->hcpriv;
	if (!qh) {
		DEBUG_MSG("qh is NULL\n");
		goto done;
	}

	/* endpoints can be iso streams.  for now, we don't
	 * accelerate iso completions ... so spin a while.
	 */
	if (qh->hw == NULL) {
		//ehci_vdbg (ft313, "iso delay\n");
		goto idle_timeout;
	}

	if (!HC_IS_RUNNING (hcd->state))
		qh->qh_state = QH_STATE_IDLE;
	switch (qh->qh_state) {
	case QH_STATE_LINKED:
	case QH_STATE_COMPLETING:
		for (tmp = ft313->async->qh_next.qh;
				tmp && tmp != qh;
				tmp = tmp->qh_next.qh)
			continue;
		/* periodic qh self-unlinks on empty, and a COMPLETING qh
		 * may already be unlinked.
		 */
		if (tmp)
			unlink_async(ft313, qh);
		/* FALL THROUGH */
	case QH_STATE_UNLINK:		/* wait for hw to finish? */
	case QH_STATE_UNLINK_WAIT:
idle_timeout:
		DEBUG_MSG("Wait for iso to finish\n");
		spin_unlock_irqrestore (&ft313->lock, flags);
		schedule_timeout_uninterruptible(1);
		goto rescan;
	case QH_STATE_IDLE:		/* fully unlinked */
		if (qh->clearing_tt)
			goto idle_timeout;
		if (list_empty (&qh->qtd_list)) {
			qh_put (qh);
			break;
		}
		/* else FALL THROUGH */
	default:
		/* caller was supposed to have unlinked any requests;
		 * that's not our job.  just leak this memory.
		 */
		ERROR_MSG("qh %p (#%02x) state %d%s\n",
			qh, ep->desc.bEndpointAddress, qh->qh_state,
			list_empty (&qh->qtd_list) ? "" : "(has tds)");
		break;
	}
	ep->hcpriv = NULL;
done:
	spin_unlock_irqrestore (&ft313->lock, flags);
	FUN_EXIT();
}

static void
ft313_endpoint_reset(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313(hcd);
	struct ehci_qh		*qh;
	int			eptype = usb_endpoint_type(&ep->desc);
	int			epnum = usb_endpoint_num(&ep->desc);
	int			is_out = usb_endpoint_dir_out(&ep->desc);
	unsigned long		flags;

	FUN_ENTRY();

	if (eptype != USB_ENDPOINT_XFER_BULK && eptype != USB_ENDPOINT_XFER_INT) {
		FUN_EXIT();
		return;
	}

	spin_lock_irqsave(&ft313->lock, flags);
	qh = ep->hcpriv;

	/* For Bulk and Interrupt endpoints we maintain the toggle state
	 * in the hardware; the toggle bits in udev aren't used at all.
	 * When an endpoint is reset by usb_clear_halt() we must reset
	 * the toggle bit in the QH.
	 */
	if (qh) {
		usb_settoggle(qh->dev, epnum, is_out, 0);
		if (!list_empty(&qh->qtd_list)) {
			WARN_ONCE(1, "clear_halt for a busy endpoint\n");
		} else if (qh->qh_state == QH_STATE_LINKED ||
				qh->qh_state == QH_STATE_COMPLETING) {

			/* The toggle value in the QH can't be updated
			 * while the QH is active.  Unlink it now;
			 * re-linking will call qh_refresh().
			 */
			if (eptype == USB_ENDPOINT_XFER_BULK)
				unlink_async(ft313, qh);
			else
				intr_deschedule(ft313, qh);
		}
	}
	spin_unlock_irqrestore(&ft313->lock, flags);

	FUN_EXIT();
}


static int ft313_get_frame (struct usb_hcd *hcd)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313 (hcd);
	return (ft313_reg_read32(ft313, &ft313->regs->frame_index) >> 3) %
		ft313->periodic_size;
}

/*-------------------------------------------------------------------------*/


MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR (DRIVER_AUTHOR);
MODULE_LICENSE ("GPL");

int ft313_open(struct inode *inode, struct file *fp)
{
	struct ft313_hcd *ft313;

	ft313 = container_of(inode->i_cdev, struct ft313_hcd, ft313_cdev);
	fp->private_data = ft313; /* for other methods */

	printk("FT313 device file opened with ft313 is %X\n", ft313);

	return 0;
}

int ft313_close(struct inode *inode, struct file *fp)
{
	printk("FT313 device file closed\n");
	return 0;
}

int ft313_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct ft313_hcd *ft313;
	struct usb_hcd *hcd;
	int retval = -1;

//	if (_IOC_TYPE(cmd) != FT313_IOC_MAGIC) return -ENOTTY;
//	if (_IOC_NR(cmd) > FT313_IOC_MAXNR) return -ENOTTY;

	ft313 = fp->private_data;

	hcd = ft313_to_hcd(ft313);


	switch (cmd) {
		case FT313_IOC_SUSPEND:
			ALERT_MSG("FT313: SUSPEND chip\n");

			if (hcd->state == HC_STATE_SUSPENDED) {
				ALERT_MSG("FT313 is already in suspend state\n");
				retval = 0;
				break;
			}

			if(hcd->driver->bus_suspend)
				retval = hcd->driver->bus_suspend(hcd);
			break;

		case FT313_IOC_RESUME:
			ALERT_MSG("FT313: RESUME chip\n");

			if (HC_STATE_RUNNING == hcd->state) {
				ALERT_MSG("FT313 is not in suspend state! \n");
				retval = 0;
				break;
			}
			if(hcd->driver->bus_resume)
				retval = hcd->driver->bus_resume(hcd);
			break;

		case FT313_IOC_RESET:
			ALERT_MSG("FT313: RESET chip\n");
			ft313_quiesce(ft313);
			ft313_halt(ft313);
			ft313_reset(ft313);
			ssleep(1);
			// Rerun FT313
			ft313_run(hcd);
			retval = 0;
			break;

		default:
			printk("Wrong IOCTL cmd\n");
			break;
	}

	return retval;
}


/* HCD file operations */
static struct file_operations ft313_fops = {
	.owner =		THIS_MODULE,
	.read =			NULL,
	.write = 		NULL,
	.poll =			NULL,
	.unlocked_ioctl =	ft313_ioctl,
	.open =			ft313_open,
	.release =		ft313_close,
};

#ifdef CONFIG_PCI
#include "ft313-pci.c"
#define	PCI_DRIVER		ft313_pci_driver
#endif

#ifdef CONFIG_MACH_AM335XEVM
#include "ft313-am335x.c"
#define PLATFORM_DRIVER         ft313_am335x_evm_driver
#endif

#if !defined(PCI_DRIVER) && !defined(PLATFORM_DRIVER)
#error "missing bus glue for ft313-hcd"
#endif

static int __init ft313_hcd_init(void)
{
	if (usb_disabled())
		return -ENODEV;
#ifdef PLATFORM_DRIVER
	return platform_driver_register(&PLATFORM_DRIVER);
#endif

#ifdef PCI_DRIVER
	return pci_register_driver(&PCI_DRIVER);
#endif
}
module_init(ft313_hcd_init);

static void __exit ft313_hcd_cleanup(void)
{
#ifdef PLATFORM_DRIVER
	platform_driver_unregister(&PLATFORM_DRIVER);
#endif

#ifdef PCI_DRIVER
	pci_unregister_driver(&PCI_DRIVER);
#endif
}
module_exit(ft313_hcd_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chang Yang");
