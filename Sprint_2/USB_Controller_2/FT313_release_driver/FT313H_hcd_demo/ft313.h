#ifndef __FT313_H__
#define __FT313_H__

//#define LOG_ON
//#define DEBUG_MSG_ON

#define DMA_POOL_WORKAROUND
//#define TI_PLAT_WORKAROUND
#define DISABLE_HCD_DMA


#define TRUE	1
#define FALSE	0

#define __hc32	__le32
#define __hc16	__le16

enum mem_blk_type {
	ITD = 1,
	QHEAD,
	SITD,
	FSTN,
	QTD,
	BUFFER
};

struct ft313_mem_map_t {
	enum mem_blk_type	type;
	u16			size;
	u16			num_of_items;
	u16			alignment;
};

struct ft313_mem_blk {
	unsigned		in_use;
	unsigned		type; // qTD, qHead, iTD, siTD, buffer
	unsigned		size;
	unsigned		offset;
};

#define	EHCI_MAX_ROOT_PORTS	15		/* see HCS_N_PORTS */
#define FT313_WK_NAME		"FT313 WQ"

struct ft313_hcd {
	/* FT313 register map */
	struct ft313_caps __iomem *caps;
	struct ft313_regs __iomem *regs;
	struct ft313_cfg __iomem *cfg;

	__u32			hcs_params;	/* cached register copy */
	spinlock_t		lock;

	/* FT313 on chip memory */
	struct ft313_mem_blk	*mem;		/* on chip memory block */
	unsigned		mem_blk_num;	/* number of memory blocks */
	spinlock_t		mem_lock;	/* lock for memory table access */
	spinlock_t		dataport_lock;	/* lock for data port register access */

	/* async schedule support */
	struct ehci_qh		*async;
//	struct ehci_qh		*dummy;		/* For AMD quirk use */
	struct ehci_qh		*reclaim;
	struct ehci_qh		*qh_scan_next;
	unsigned		scanning : 1;

	/* periodic schedule support */
#define	DEFAULT_I_TDPS		1024		/* some HCs can do less */
	unsigned		periodic_size;
	__hc32			*periodic;	/* hw periodic table */
	dma_addr_t		periodic_dma;
	unsigned		periodic_ft313; /* FT313 in chip memory offset */

	unsigned		i_thresh;	/* uframes HC might cache */

	union ehci_shadow	*pshadow;	/* mirror hw periodic table */
	int			next_uframe;	/* scan periodic, start here */
	unsigned		periodic_sched;	/* periodic activity count */

	/* list of itds & sitds completed while clock_frame was still active */
	struct list_head	cached_itd_list;
	struct list_head	cached_sitd_list;
	unsigned		clock_frame;

	/* per root hub port */
	unsigned long		reset_done [EHCI_MAX_ROOT_PORTS];

	/* bit vectors (one bit per port) */
	unsigned long		bus_suspended;		/* which ports were
			already suspended at the start of a bus suspend */
	unsigned long		owned_ports;		/* which ports are
			owned by the companion during a bus suspend */
	unsigned long		port_c_suspend;		/* which ports have
			the change-suspend feature turned on */
	unsigned long		suspended_ports;	/* which ports are
			suspended */

	/* per-HC memory pools */
	struct dma_pool		*qh_pool;	/* qh per active urb */
	struct dma_pool		*qtd_pool;	/* one or more per qh */
	struct dma_pool		*itd_pool;	/* itd per iso urb */
	struct dma_pool		*sitd_pool;	/* sitd per split iso urb */

	struct timer_list	iaa_watchdog;	// FixMe: This two watchdog is for silicon
	struct timer_list	watchdog;	// bugs of earlier controller, should be removed now.
	unsigned long		actions;
	unsigned		periodic_stamp;
	unsigned		random_frame;
	unsigned long		next_statechange;
	ktime_t			last_periodic_enable;
	u32			command;

	/* SILICON QUIRKS */
	unsigned		need_io_watchdog:1;

#ifdef	PORT_RESET_TIME_WORKAROUND
	struct timer_list	port_reset_timer;
#endif

	u8			sbrn;		/* packed release number */

	/* irq statistics */
#ifdef EHCI_STATS
	struct ehci_stats	stats;
#	define COUNT(x) do { (x)++; } while (0)
#else
#	define COUNT(x) do {} while (0)
#endif
	struct workqueue_struct	*wakeup_wq;
	const char		*wakeup_wq_name;
	struct work_struct	wakeup_work;

	dev_t			ft313_cdev_major;
	unsigned int		ft313_cdev_count;
	struct cdev		ft313_cdev;
};

static inline struct ft313_hcd *hcd_to_ft313 (struct usb_hcd *hcd)
{
	return (struct ft313_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *ft313_to_hcd (struct ft313_hcd *ft313)
{
	return container_of ((void *) ft313, struct usb_hcd, hcd_priv);
}


static inline void
iaa_watchdog_start(struct ft313_hcd *ft313)
{
	//printk("iaa watchdog timer triggered\n");
	WARN_ON(timer_pending(&ft313->iaa_watchdog));
	mod_timer(&ft313->iaa_watchdog,
			jiffies + msecs_to_jiffies(EHCI_IAA_MSECS));
}

static inline void iaa_watchdog_done(struct ft313_hcd *ft313)
{
	//printk("iaa watchdog timer canceled\n");
	del_timer(&ft313->iaa_watchdog);
}

enum ft313_timer_action {
	TIMER_IO_WATCHDOG,
	TIMER_ASYNC_SHRINK,
	TIMER_ASYNC_OFF,
};

static inline void
timer_action_done (struct ft313_hcd *ft313, enum ft313_timer_action action)
{
	clear_bit (action, &ft313->actions);
}

#define	QTD_NEXT(ehci, dma)	cpu_to_hc32(ehci, (u32)dma)

/*
 * EHCI Specification 0.95 Section 3.5
 * QTD: describe data transfer components (buffer, direction, ...)
 * See Fig 3-6 "Queue Element Transfer Descriptor Block Diagram".
 *
 * These are associated only with "QH" (Queue Head) structures,
 * used with control, bulk, and interrupt transfers.
 */
struct ehci_qtd_hw {
	__hc32			hw_next;	/* see EHCI 3.5.1 */
	__hc32			hw_alt_next;    /* see EHCI 3.5.2 */
	__hc32			hw_token;       /* see EHCI 3.5.3 */
	__hc32			hw_buf [5];        /* see EHCI 3.5.4 */
};

struct ehci_qtd {
	/* first part defined by EHCI spec */
	__hc32			hw_next;	/* see EHCI 3.5.1 */
	__hc32			hw_alt_next;    /* see EHCI 3.5.2 */
	__hc32			hw_token;       /* see EHCI 3.5.3 */
#define	QTD_TOGGLE	(1 << 31)	/* data toggle */
#define	QTD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define	QTD_IOC		(1 << 15)	/* interrupt on complete */
#define	QTD_CERR(tok)	(((tok)>>10) & 0x3)
#define	QTD_PID(tok)	(((tok)>>8) & 0x3)
#define	QTD_STS_ACTIVE	(1 << 7)	/* HC may execute this */
#define	QTD_STS_HALT	(1 << 6)	/* halted on error */
#define	QTD_STS_DBE	(1 << 5)	/* data buffer error (in HC) */
#define	QTD_STS_BABBLE	(1 << 4)	/* device was babbling (qtd halted) */
#define	QTD_STS_XACT	(1 << 3)	/* device gave illegal response */
#define	QTD_STS_MMF	(1 << 2)	/* incomplete split transaction */
#define	QTD_STS_STS	(1 << 1)	/* split transaction state */
#define	QTD_STS_PING	(1 << 0)	/* issue PING? */

#define ACTIVE_BIT(ehci)	cpu_to_hc32(ehci, QTD_STS_ACTIVE)
#define HALT_BIT(ehci)		cpu_to_hc32(ehci, QTD_STS_HALT)
#define STATUS_BIT(ehci)	cpu_to_hc32(ehci, QTD_STS_STS)

	__hc32			hw_buf [5];        /* see EHCI 3.5.4 */
//	__hc32			hw_buf_hi [5];        /* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t		qtd_dma;		/* qtd address */
	unsigned		qtd_ft313;		/* Offset in FT313 on chip memory */
	struct list_head	qtd_list;		/* sw qtd list */
	struct urb		*urb;			/* qtd's urb */

	gfp_t			mem_flags;		/* memory allocation flag from usbcore */
	u32			maxpacket;
	u32			buffer_ft313;		/* offset of mem buffer in ft313 */
	size_t			length;			/* length of buffer */
} __attribute__ ((aligned (32)));

/* mask NakCnt+T in qh->hw_alt_next */
#define QTD_MASK(ehci)	cpu_to_hc32 (ehci, ~0x1f)

#define IS_SHORT_READ(token) (QTD_LENGTH (token) != 0 && QTD_PID (token) == 1)

/*-------------------------------------------------------------------------*/

/* type tag from {qh,itd,sitd,fstn}->hw_next */
#define Q_NEXT_TYPE(ehci,dma)	((dma) & cpu_to_hc32(ehci, 3 << 1))

/*
 * Now the following defines are not converted using the
 * cpu_to_le32() macro anymore, since we have to support
 * "dynamic" switching between be and le support, so that the driver
 * can be used on one system with SoC EHCI controller using big-endian
 * descriptors as well as a normal little-endian PCI EHCI controller.
 */
/* values for that type tag */
#define Q_TYPE_ITD	(0 << 1)
#define Q_TYPE_QH	(1 << 1)
#define Q_TYPE_SITD	(2 << 1)
#define Q_TYPE_FSTN	(3 << 1)

/* next async queue entry, or pointer to interrupt/periodic QH */
#define QH_NEXT(ehci,dma)	(cpu_to_hc32(ehci, (((u32)dma)&~0x01f)|Q_TYPE_QH))

/* for periodic/async schedules and qtd lists, mark end of list */
#define EHCI_LIST_END(ehci)	cpu_to_hc32(ehci, 1) /* "null pointer" to hw */

/*
 * Entries in periodic shadow table are pointers to one of four kinds
 * of data structure.  That's dictated by the hardware; a type tag is
 * encoded in the low bits of the hardware's periodic schedule.  Use
 * Q_NEXT_TYPE to get the tag.
 *
 * For entries in the async schedule, the type tag always says "qh".
 */
union ehci_shadow {
	struct ehci_qh		*qh;		/* Q_TYPE_QH */
	struct ehci_itd		*itd;		/* Q_TYPE_ITD */
	struct ehci_sitd	*sitd;		/* Q_TYPE_SITD */
	struct ehci_fstn	*fstn;		/* Q_TYPE_FSTN */
	__hc32			*hw_next;	/* (all types) */
	void			*ptr;
};

/*-------------------------------------------------------------------------*/

/*
 * EHCI Specification 0.95 Section 3.6
 * QH: describes control/bulk/interrupt endpoints
 * See Fig 3-7 "Queue Head Structure Layout".
 *
 * These appear in both the async and (for interrupt) periodic schedules.
 */

/* first part defined by EHCI spec */
struct ehci_qh_hw {
	__hc32			hw_next;	/* see EHCI 3.6.1 */
	__hc32			hw_info1;       /* see EHCI 3.6.2 */
#define	QH_HEAD		0x00008000
	__hc32			hw_info2;        /* see EHCI 3.6.2 */
#define	QH_SMASK	0x000000ff
#define	QH_CMASK	0x0000ff00
#define	QH_HUBADDR	0x007f0000
#define	QH_HUBPORT	0x3f800000
#define	QH_MULT		0xc0000000
	__hc32			hw_current;	/* qtd list - see EHCI 3.6.4 */

	/* qtd overlay (hardware parts of a struct ehci_qtd) */
	__hc32			hw_qtd_next;
	__hc32			hw_alt_next;
	__hc32			hw_token;
	__hc32			hw_buf [5];
//	__hc32			hw_buf_hi [5]; // 64 bit mode is not needed!
} __attribute__ ((aligned(32)));

struct qh_urb_queue_item {
	struct urb		*urb;
	struct list_head	urb_list;
};

struct ehci_qh {
	struct ehci_qh_hw	*hw;
	/* the rest is HCD-private */
	dma_addr_t		qh_dma;		/* address of qh */
	unsigned		qh_ft313;	/* FT313 in chip memory offset */
	union ehci_shadow	qh_next;	/* ptr to qh; or periodic */
	struct list_head	qtd_list;	/* sw qtd list */
	struct ehci_qtd		*dummy;
	struct ehci_qh		*reclaim;	/* next to reclaim */

	struct ft313_hcd	*ft313;
	unsigned long		unlink_time;

	int			urb_pending;	/* this used to record whether current urb is completed or not */
	struct list_head	urb_list;	/* waiting urb list */
	struct urb		*urb;		/* current urb under execution */

	gfp_t			mem_flags;	/* memory allocation flag from usbcore */

	/*
	 * Do NOT use atomic operations for QH refcounting. On some CPUs
	 * (PPC7448 for example), atomic operations cannot be performed on
	 * memory that is cache-inhibited (i.e. being used for DMA).
	 * Spinlocks are used to protect all QH fields.
	 */
	u32			refcount;
	unsigned		stamp;

	u8			needs_rescan;	/* Dequeue during giveback */
	u8			qh_state;
#define	QH_STATE_LINKED		1		/* HC sees this */
#define	QH_STATE_UNLINK		2		/* HC may still see this */
#define	QH_STATE_IDLE		3		/* HC doesn't see this */
#define	QH_STATE_UNLINK_WAIT	4		/* LINKED and on reclaim q */
#define	QH_STATE_COMPLETING	5		/* don't touch token.HALT */

	u8			xacterrs;	/* XactErr retry counter */
#define	QH_XACTERR_MAX		32		/* XactErr retry limit */

	/* periodic schedule info */
	u8			usecs;		/* intr bandwidth */
	u8			gap_uf;		/* uframes split/csplit gap */
	u8			c_usecs;	/* ... split completion bw */
	u16			tt_usecs;	/* tt downstream bandwidth */
	unsigned short		period;		/* polling interval */
	unsigned short		start;		/* where polling starts */
#define NO_FRAME ((unsigned short)~0)			/* pick new start */

	struct usb_device	*dev;		/* access to TT */
	unsigned		is_out:1;	/* bulk or intr OUT */
	unsigned		clearing_tt:1;	/* Clear-TT-Buf in progress */
};

/*-------------------------------------------------------------------------*/

/* description of one iso transaction (up to 3 KB data if highspeed) */
struct ehci_iso_packet {
	/* These will be copied to iTD when scheduling */
	u64			bufp;		/* itd->hw_bufp{,_hi}[pg] |= */
	__hc32			transaction;	/* itd->hw_transaction[i] |= */
	u8			cross;		/* buf crosses pages */
	/* for full speed OUT splits */
	u32			buf1;
};

/* temporary schedule data for packets from iso urbs (both speeds)
 * each packet is one logical usb transaction to the device (not TT),
 * beginning at stream->next_uframe
 */
struct ehci_iso_sched {
	struct list_head	td_list;
	unsigned		span;
	unsigned		actual_number_of_packets;
	struct ehci_iso_packet	packet [0];
};


struct iso_urb_queue_item {
	struct urb		*urb;
	struct list_head	urb_list;
	unsigned		urb_buffer;
};

/*
 * ehci_iso_stream - groups all (s)itds for this endpoint.
 * acts like a qh would, if EHCI had them for ISO.
 */
struct ehci_iso_stream {
	/* first field matches ehci_hq, but is NULL */
	struct ehci_qh_hw	*hw;

	u32			refcount;
	u8			bEndpointAddress;
	u8			highspeed;
	struct list_head	td_list;	/* queued itds/sitds */
	struct list_head	free_list;	/* list of unused itds/sitds */
	struct usb_device	*udev;
	struct usb_host_endpoint *ep;

	/* output of (re)scheduling */
	int			next_uframe;
	__hc32			splits;

	/* the rest is derived from the endpoint descriptor,
	 * trusting urb->interval == f(epdesc->bInterval) and
	 * including the extra info for hw_bufp[0..2]
	 */
	u8			usecs, c_usecs;
	u16			interval;
	u16			tt_usecs;
	u16			maxp;
	u16			raw_mask;
	unsigned		bandwidth;

	/* This is used to initialize iTD's hw_bufp fields */
	__hc32			buf0;
	__hc32			buf1;
	__hc32			buf2;

	/* this is used to initialize sITD's tt info */
	__hc32			address;

	u32			buffer_ft313;
	unsigned		actual_number_of_packets;

	int			urb_waiting;	/* this used to record whether there is urb waiting for prcoess */
	struct list_head	urb_list;	/* waiting urb list */
	struct urb		*urb;		/* current urb under execution */

};

/*-------------------------------------------------------------------------*/

/*
 * EHCI Specification 0.95 Section 3.3
 * Fig 3-4 "Isochronous Transaction Descriptor (iTD)"
 *
 * Schedule records for high speed iso xfers
 */
struct ehci_itd_hw {
	__hc32			hw_next;           /* see EHCI 3.3.1 */
	__hc32			hw_transaction [8]; /* see EHCI 3.3.2 */
	__hc32			hw_bufp [7];	/* see EHCI 3.3.3 */
//	__hc32			hw_bufp_hi [7];	/* Appendix B */
};

struct ehci_itd {
	/* first part defined by EHCI spec */
	__hc32			hw_next;           /* see EHCI 3.3.1 */
	__hc32			hw_transaction [8]; /* see EHCI 3.3.2 */
#define EHCI_ISOC_ACTIVE        (1<<31)        /* activate transfer this slot */
#define EHCI_ISOC_BUF_ERR       (1<<30)        /* Data buffer error */
#define EHCI_ISOC_BABBLE        (1<<29)        /* babble detected */
#define EHCI_ISOC_XACTERR       (1<<28)        /* XactErr - transaction error */
#define	EHCI_ITD_LENGTH(tok)	(((tok)>>16) & 0x0fff)
#define	EHCI_ITD_IOC		(1 << 15)	/* interrupt on complete */

#define ITD_ACTIVE(ehci)	cpu_to_hc32(ehci, EHCI_ISOC_ACTIVE)

	__hc32			hw_bufp [7];	/* see EHCI 3.3.3 */
//	__hc32			hw_bufp_hi [7];	/* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t		itd_dma;	/* for this itd */
	unsigned		itd_ft313;	/* offset in ft313 for this itd */
	union ehci_shadow	itd_next;	/* ptr to periodic q entry */

	struct urb		*urb;
	struct ehci_iso_stream	*stream;	/* endpoint's queue */
	struct list_head	itd_list;	/* list of stream's itds */

	/* any/all hw_transactions here may be used by that urb */
	unsigned		frame;		/* where scheduled */
	unsigned		pg;
	unsigned		index[8];	/* in urb->iso_frame_desc */
} __attribute__ ((aligned (32)));

/*-------------------------------------------------------------------------*/

/*
 * EHCI Specification 0.95 Section 3.4
 * siTD, aka split-transaction isochronous Transfer Descriptor
 *       ... describe full speed iso xfers through TT in hubs
 * see Figure 3-5 "Split-transaction Isochronous Transaction Descriptor (siTD)
 */
struct ehci_sitd_hw {
	__hc32			hw_next;
	__hc32			hw_fullspeed_ep;	/* EHCI table 3-9 */
	__hc32			hw_uframe;		/* EHCI table 3-10 */
	__hc32			hw_results;		/* EHCI table 3-11 */
	__hc32			hw_buf [2];		/* EHCI table 3-12 */
	__hc32			hw_backpointer;		/* EHCI table 3-13 */
//	__hc32			hw_buf_hi [2];		/* Appendix B */
};

struct ehci_sitd {
	/* first part defined by EHCI spec */
	__hc32			hw_next;
/* uses bit field macros above - see EHCI 0.95 Table 3-8 */
	__hc32			hw_fullspeed_ep;	/* EHCI table 3-9 */
	__hc32			hw_uframe;		/* EHCI table 3-10 */
	__hc32			hw_results;		/* EHCI table 3-11 */
#define	SITD_IOC	(1 << 31)	/* interrupt on completion */
#define	SITD_PAGE	(1 << 30)	/* buffer 0/1 */
#define	SITD_LENGTH(x)	(0x3ff & ((x)>>16))
#define	SITD_STS_ACTIVE	(1 << 7)	/* HC may execute this */
#define	SITD_STS_ERR	(1 << 6)	/* error from TT */
#define	SITD_STS_DBE	(1 << 5)	/* data buffer error (in HC) */
#define	SITD_STS_BABBLE	(1 << 4)	/* device was babbling */
#define	SITD_STS_XACT	(1 << 3)	/* illegal IN response */
#define	SITD_STS_MMF	(1 << 2)	/* incomplete split transaction */
#define	SITD_STS_STS	(1 << 1)	/* split transaction state */

#define SITD_ACTIVE(ehci)	cpu_to_hc32(ehci, SITD_STS_ACTIVE)

	__hc32			hw_buf [2];		/* EHCI table 3-12 */
	__hc32			hw_backpointer;		/* EHCI table 3-13 */
	__hc32			hw_buf_hi [2];		/* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t		sitd_dma;
	u32			sitd_ft313;
	union ehci_shadow	sitd_next;	/* ptr to periodic q entry */

	struct urb		*urb;
	struct ehci_iso_stream	*stream;	/* endpoint's queue */
	struct list_head	sitd_list;	/* list of stream's sitds */
	unsigned		frame;
	unsigned		index;
} __attribute__ ((aligned (32)));

/*-------------------------------------------------------------------------*/

/*
 * EHCI Specification 0.96 Section 3.7
 * Periodic Frame Span Traversal Node (FSTN)
 *
 * Manages split interrupt transactions (using TT) that span frame boundaries
 * into uframes 0/1; see 4.12.2.2.  In those uframes, a "save place" FSTN
 * makes the HC jump (back) to a QH to scan for fs/ls QH completions until
 * it hits a "restore" FSTN; then it returns to finish other uframe 0/1 work.
 */
struct ehci_fstn {
	__hc32			hw_next;	/* any periodic q entry */
	__hc32			hw_prev;	/* qh or EHCI_LIST_END */

	/* the rest is HCD-private */
	dma_addr_t		fstn_dma;
	union ehci_shadow	fstn_next;	/* ptr to periodic q entry */
} __attribute__ ((aligned (32)));


/*
 * Some EHCI controllers have a Transaction Translator built into the
 * root hub. This is a non-standard feature.  Each controller will need
 * to add code to the following inline functions, and call them as
 * needed (mostly in root hub code).
 */

#define	ehci_is_TDI(e)			(ft313_to_hcd(e)->has_tt)
static inline u8 ft313_reg_read8(const struct ft313_hcd *ft313,
				  void __iomem * regs);
static inline u16 ft313_reg_read16(const struct ft313_hcd *ft313,
		void __iomem * regs);
/* Returns the speed of a device attached to a port on the root hub. */
static inline unsigned int
ft313_port_speed(struct ft313_hcd *ft313, unsigned int portsc)
{
	u32 tmp;

	tmp = ft313_reg_read16(ft313, &ft313->cfg->hw_mode);
	tmp = (tmp & HOST_SPD_TYP ) >> 6;

	if (tmp == 2) {
		printk("ft313 host controller detect a high USB speed device\n");
		return USB_PORT_STAT_HIGH_SPEED;
	}
	else if (tmp == 1) {
		printk("ft313 host controller detect a Low USB speed device\n");
		return USB_PORT_STAT_LOW_SPEED;
	}
	else if (tmp == 0) {
		printk("ft313 host controller detect a full USB speed device\n");
		return 0;
	}
	else {
		printk("ft313 host controller Don't know speed, set as High speed\n");
		printk("Wrong value read for Host Speed Type\n");
		return USB_PORT_STAT_HIGH_SPEED;
	}
}



/* cpu to ehci */
static inline __hc32 cpu_to_hc32 (const struct ft313_hcd *ehci, const u32 x)
{
	return cpu_to_le32(x);
}

/* ehci to cpu */
static inline u32 hc32_to_cpu (const struct ft313_hcd *ehci, const __hc32 x)
{
	return le32_to_cpu(x);
}

static inline u32 hc32_to_cpup (const struct ft313_hcd *ehci, const __hc32 *x)
{
	return le32_to_cpup(x);
}


#endif /* __FT313_H__ */
