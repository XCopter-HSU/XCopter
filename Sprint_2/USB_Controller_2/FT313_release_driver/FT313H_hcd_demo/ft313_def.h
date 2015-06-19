#ifndef __FT313_DEF_H__
#define __FT313_DEF_H__

#define FT313_CAP_OFFSET	0x0000
#define FT313_CONFIG_OFFSET	0x80

#define FT313_CHIP_MEM_AMT	(24 * 1024)
#define FT313_IN_8_BIT_MODE
//#undef  FT313_IN_8_BIT_MODE

//#define PORT_RESET_TIME_WORKAROUND
//#define USB_SOF_INTR
#ifndef USB_SOF_INTR
#ifdef CONFIG_PCI
#define PCI_ENABLE_MSI
#endif
#endif

//#define ENABLE_DYN_UNLINK

#undef CONFIG_USB_EHCI_TT_NEWSCHED


/* Ft313 chip configuration registers */
struct ft313_cfg {
	u32		chip_id;
	u32		hw_mode;
#define HOST_SPD_TYP		(3 << 6)		/* host speed type */
#define DACK_POL		(1 << 5)		/* DACK Polarity */
#define DREQ_POL		(1 << 4)		/* DACK Polarity */
#define INTF_LOCK		(1 << 3)		/* Interface lock */
#define INTR_POL		(1 << 2)		/* Interrupt Polarity */
#define INTR_EDGE		(1 << 1)		/* 1 - Edge, 0 - Level */
#define GLOBAL_INTR_EN		(1 << 0)		/* Global Interrupt enable */
	u32		edge_int_ctrl;
	u32		sw_reset;
#define IS_8_BIT_MODE(p)	((p) & (1 << 4))	/* true: 8 bit mode */
#define DATA_BUS_WIDTH		(1 << 4)
#define RESET_ATX		(1 << 2)
#define RESET_HC		(1 << 1)
#define RESET_ALL		(1 << 0)
	u16		mem_addr;
	u16		data_port;
	u16		data_session_len;
	u16		config;
#define BCD_MODE_CTRL		(1 << 15)
#define BCD_MODE_SDP		(0 << 13)
#define BCD_MODE_DCP		(1 << 13)
#define BCD_MODE_CDP1		(2 << 13)
#define BCD_MODE_CDP2		(3 << 13)
#define OSC_EN			(1 << 11)
#define PLL_EN			(1 << 10)
#define REG_PWR			(1 << 9)
#define HC_CLK_EN		(1 << 8)
#define VBUS_OFF		(1 << 7)
#define PORT_OC_EN		(1 << 6)
#define BCD_EN			(1 << 5)
#define BURST_LEN_1		(0 << 2)
#define BURST_LEN_4		(1 << 2)
#define BURST_LEN_8		(2 << 2)
#define BURST_LEN_16		(3 << 2)
#define ENABLE_DMA		(1 << 1)
#define DMA_ABORT		(1 << 0)
	u16		aux_mem_addr;
	u16		aux_data_port;
	u32		sleep_timer;
	u32		hc_int_sts;
#define WAKEUPINT		(1 << 7)
#define OCINT			(1 << 6)
#define CLKREADY		(1 << 5)
#define BUSINACTIVE		(1 << 4)
#define REMOTEWKINT		(1 << 3)
#define DMAEOTINT		(1 << 2)
#define SOFINT			(1 << 1)
#define MSOFINT			(1 << 0)
	u32		hc_int_en;
#define WAKEUPINT_EN		(1 << 7)
#define OCINT_EN		(1 << 6)
#define CLKREADY_EN		(1 << 5)
#define BUSINACTIVE_EN		(1 << 4)
#define REMOTEWKINT_EN		(1 << 3)
#define DMAEOTINT_EN		(1 << 2)
#define SOFINT_EN		(1 << 1)
#define MSOFINT_EN		(1 << 0)
};

/* Section 4.1.1.1 Host Controller Capability Registers */
struct ft313_caps {
	/* these fields are specified as 8 and 16 bit registers,
	 * but some hosts can't perform 8 or 16 bit PCI accesses.
	 */
	u32		hc_capbase;
#define CAPLENGTH(p)		(((p)>>00)&0x00ff)	/* bits 7:0 */
#define HCIVERSION(p)		(((p)>>16)&0xffff)	/* bits 31:16 */
	u32		hcs_params;     /* HCSPARAMS - offset 0x4 */
#define HCS_N_PORTS(p)		(((p)>>0)&0xf)	/* bits 3:0, ports on HC */

	u32		hcc_params;      /* HCCPARAMS - offset 0x8 */
#define ASYN_SCH_PARK_CAP(p)	((p)&(1 << 2))  /* true: can park on async qh */
#define PROG_FR_LIST_FLAG(p) 	((p)&(1 << 1))  /* true: periodic_size changes*/
} __attribute__ ((packed));


/* Section 4.1.1.4 USBCMD - HC USB Command Register */
struct ft313_regs {
	/* USBCMD: offset 0x10 */
	u32		command;
/* 23:16 is r/w intr rate, in microframes; default "8" == 1/msec */
#define INT_THRC	(((p)>>16) & 0xFF)
#define ASYN_PK_EN	(1<<11)		/* enable "park" on async qh */
#define ASYN_PK_CNT(c)	(((c)>>8)&3)	/* how many transfers to park for */
#define INT_OAAD	(1<<6)		/* "doorbell" interrupt async advance */
#define ASCH_EN		(1<<5)		/* async schedule enable */
#define PSCH_EN		(1<<4)		/* periodic schedule enable */
#define FRL_SIZE(c)	(((c)>>2)&3)	/* 3:2 is periodic frame list size */
#define HC_RESET	(1<<1)		/* reset HC not bus */
#define RS		(1<<0)		/* start/stop HC */

	/* USBSTS: offset 0x14 */
	u32		status;
#define ASCH_STS	(1<<15)		/* Async Schedule Status */
#define PSCH_STS	(1<<14)		/* Periodic Schedule Status */
#define RECLAMATION	(1<<13)		/* Reclamation */
#define HCHALTED	(1<<12)		/* Not running (any reason) */
/* some bits reserved */
#define INT_OAA		(1<<5)		/* Interrupted on async advance */
#define H_SYSERR	(1<<4)		/* such as some PCI access errors */
#define FRL_ROL		(1<<3)		/* frame list rolled over */
#define PO_CHG_DET	(1<<2)		/* port change detect */
#define USBERR_INT	(1<<1)		/* "error" completion (overflow, ...) */
#define USB_INT		(1<<0)		/* "normal" completion (short, ...) */

	/* USBINTR: offset 0x18 */
	u32		intr_enable;
#define INT_OAA_EN	(1<<5)		/* Interrupted on async advance */
#define H_SYSERR_EN	(1<<4)		/* such as some PCI access errors */
#define FRL_ROL_EN	(1<<3)		/* frame list rolled over */
#define PO_CHG_DET_EN	(1<<2)		/* port change detect */
#define USBERR_INT_EN	(1<<1)		/* "error" completion (overflow, ...) */
#define USB_INT_EN	(1<<0)		/* "normal" completion (short, ...) */

	/* FRINDEX: offset 0x1C */
	u32		frame_index;	/* current microframe number */
	/* CTRLDSSEGMENT: offset 0x20 */
	u32		segment;	/* address bits 63:32 if needed: not used! */
	/* PERIODICLISTBASE: offset 0x24 */
	u32		frame_list;	/* points to periodic list */
#define PERI_BASADR	(0xFFFFF<<12)
	/* ASYNCLISTADDR: offset 0x28 */
	u32		async_next;	/* address of next async queue head */

	/* CONFIGFLAG: offset 0x2C */
//	u32		configured_flag;
//#define FLAG_CF		(1<<0)		/* true: we'll support "high speed" */
	u32	placeholder_config_flag;
	/* PORTSC: offset 0x30 */
	u32		port_status[1];	/* one port only */
#define TST_FORCE_EN	(1<<16)		/* Test Force Enable */
/* 15:12 reserved */
#define LINE_STS	(3<<10)		/* Line Satus */
/* 9 reserved */
#define PO_RESET	(1<<8)		/* reset port */
#define PO_SUSP		(1<<7)		/* suspend port */
#define F_PO_RESM	(1<<6)		/* Force Port Resume */
/* 5:4 reserved */
#define PO_EN_CHG	(1<<3)		/* port enable/disable change */
#define PO_EN		(1<<2)		/* port enable/disable */
#define CONN_CHG	(1<<1)		/* connect status change */
#define CONN_STS	(1<<0)		/* Current Connect Status */
#define PORT_RWC_BITS   (PO_EN_CHG | CONN_CHG)


	/* EOF Time Register 0x34 */
	u32		eof_time;
#define U_SUSP_N	(1<<6)		/* Transceiver Suspend Mode */
#define EOF2_TIME	(3<<4)		/* EOF 2 Timing Points */
#define EOF1_TIME	(3<<2)		/* EOF 1 Timing Points */
#define ASYN_SCH_SLPT	(3<<0)		/* Asynchronous Schedule Sleep Timer */

	/* placeholder for 0x38 to 0x4C */
	u32		reserved2[6];

	/* Test Mode Register 0x50 */
	u32		test_mode;
#define TST_LOOPBK	(1<<4)		/* FIFO Loop Back Mode */
#define TST_PKT		(1<<2)		/* Test Packet */
#define TST_KSTA	(1<<1)		/* 1: D+/D- are set to the high-speed K state. */
#define TST_JSTA	(1<<0)		/* 1: D+/D- are set to the high-speed J state. */

	/* placeholder for 0x54 to 0x6C */
	u32		reserved3[7];

	/* Test parameter Setting 1 register 0x70 */
	u32		testpmset1;
#define DMA_LEN(x)	((x) << 8)	/* DMA Length */
#define DMA_TYPE	(1 << 1)	/* DMA Type, 0: FIFO to Memory, 1: Memory to FIFO */
#define DMA_START	(1 << 0)	/* DMA Start */

	/* Test parameter setting 2 register 0x74 */
	u32		testpmset2;
} __attribute__ ((packed));

#endif /* __FT313_DEF_H__ */
