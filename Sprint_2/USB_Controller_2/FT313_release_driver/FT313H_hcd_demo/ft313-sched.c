/*
 * FT313 transaction schedue support.
 *
 * Copyright (C) 2011 Chang Yang <chang.yang@ftdichip.com>
 *
 * This code is *strongly* based on EHCI-HCD code by David Brownell since
 * the chip is a quasi-EHCI compatible.
 *
 * Licensed under GPL version 2 only.
 */

/* this file is part of ft313-hcd.c */

static int ft313_get_frame (struct usb_hcd *hcd);

/*-------------------------------------------------------------------------*/

/*
 * periodic_next_shadow - return "next" pointer on shadow list
 * @periodic: host pointer to qh/itd/sitd
 * @tag: hardware tag for type of this record
 */
static union ehci_shadow *
		periodic_next_shadow(struct ft313_hcd *ft313, union ehci_shadow *periodic,
				     __hc32 tag)
{
	switch (hc32_to_cpu(ft313, tag)) {
		case Q_TYPE_QH:
			return &periodic->qh->qh_next;
		case Q_TYPE_FSTN:
			return &periodic->fstn->fstn_next;
		case Q_TYPE_ITD:
			return &periodic->itd->itd_next;
			// case Q_TYPE_SITD:
		default:
			return &periodic->sitd->sitd_next;
	}
}

static __hc32 *
shadow_next_periodic(struct ft313_hcd *ft313, union ehci_shadow *periodic,
		     __hc32 tag)
{
	switch (hc32_to_cpu(ft313, tag)) {
			/* our ehci_shadow.qh is actually software part */
		case Q_TYPE_QH:
			return &periodic->qh->hw->hw_next;
			/* others are hw parts */
		default:
			return periodic->hw_next;
	}
}

/* caller must hold ft313->lock */
static void periodic_unlink (struct ft313_hcd *ft313, unsigned frame, void *ptr)
{
	union ehci_shadow	*prev_p = &ft313->pshadow[frame];
	__hc32			*hw_p = &ft313->periodic[frame];
	__hc32			hw_p_ft313 = frame * sizeof(__hc32);
	union ehci_shadow	here = *prev_p;

	/* find predecessor of "ptr"; hw and shadow lists are in sync */
	while (here.ptr && here.ptr != ptr) {
		prev_p = periodic_next_shadow(ft313, prev_p,
					      Q_NEXT_TYPE(ft313, *hw_p));
		hw_p = shadow_next_periodic(ft313, &here,
					    Q_NEXT_TYPE(ft313, *hw_p));
		hw_p_ft313 = here.qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next);
		here = *prev_p;
	}
	/* an interrupt entry (at list end) could have been shared */
	if (!here.ptr)
		return;

	/* update shadow and hardware lists ... the old "next" pointers
	 * from ptr may still be in use, the caller updates them.
	 */
	*prev_p = *periodic_next_shadow(ft313, &here,
					Q_NEXT_TYPE(ft313, *hw_p));
#if 0 // As use_dummy_qh is always 0 for our case!
	if (!ft313->use_dummy_qh ||
			*shadow_next_periodic(ft313, &here, Q_NEXT_TYPE(ft313, *hw_p))
			!= EHCI_LIST_END(ft313))
#endif
		*hw_p = *shadow_next_periodic(ft313, &here,
					      Q_NEXT_TYPE(ft313, *hw_p));
		//hw_p_ft313 = here.qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next);
		ft313_mem_write(ft313, hw_p, sizeof(*hw_p), hw_p_ft313);
#if 0
	else
	{
//		*hw_p = ft313->dummy->qh_dma;
		*hw_p = ft313->dummy->qh_ft313;
		ft313_mem_write(ft313,
				&(ft313->dummy->qh_ft313),
				sizeof(__hc32*),
				(unsigned int)hw_p - (unsigned int)(ft313->periodic));
	}
#endif
}

/* how many of the uframe's 125 usecs are allocated? */
static unsigned short
periodic_usecs (struct ft313_hcd *ft313, unsigned frame, unsigned uframe)
{
	__hc32			*hw_p = &ft313->periodic [frame];
	union ehci_shadow	*q = &ft313->pshadow [frame];
	unsigned		usecs = 0;
	struct ehci_qh_hw	*hw;

//	FUN_ENTRY();

//	DEBUG_MSG("Now process Frame number %d and uFrame number %d\n", frame, uframe);

	// Update from chip memory, is this really needed? FixMe
//	ft313_mem_read(ft313, &ft313->periodic[frame], sizeof(*hw_p), 0 + frame * sizeof(__le32));

	while (q->ptr) {
		switch (hc32_to_cpu(ft313, Q_NEXT_TYPE(ft313, *hw_p))) {
			case Q_TYPE_QH:
				DEBUG_MSG("Meet qH\n");
				hw = q->qh->hw;
				// Update from chip memory
				//ft313_mem_read(ft313, hw, sizeof(*hw), q->qh->qh_ft313);
				/* is it in the S-mask? */
				if (hw->hw_info2 & cpu_to_hc32(ft313, 1 << uframe))
					usecs += q->qh->usecs;
				/* ... or C-mask? */
				if (hw->hw_info2 & cpu_to_hc32(ft313,
							       1 << (8 + uframe)))
					usecs += q->qh->c_usecs;
				hw_p = &hw->hw_next;
				DEBUG_MSG("hw_p now pointer to 0x%X with value 0x%X\n", hw_p, *hw_p);
				q = &q->qh->qh_next;
				break;
				// case Q_TYPE_FSTN:
			default:
				ERROR_MSG("Wrong exeuction\n");
				/* for "save place" FSTNs, count the relevant INTR
				 * bandwidth from the previous frame
				 */
				if (q->fstn->hw_prev != EHCI_LIST_END(ft313)) {
					DEBUG_MSG("ignoring FSTN cost ...\n");
				}
				hw_p = &q->fstn->hw_next;
				q = &q->fstn->fstn_next;
				break;
			case Q_TYPE_ITD:
				DEBUG_MSG("Meet iTD\n");
				if (q->itd->hw_transaction[uframe])
					usecs += q->itd->stream->usecs;
				hw_p = &q->itd->hw_next;
				DEBUG_MSG("hw_p now pointer to %p with value 0x%X\n", hw_p, *hw_p);
				q = &q->itd->itd_next;
				break;
			case Q_TYPE_SITD:
				DEBUG_MSG("Meet siTD\n");
				/* is it in the S-mask?  (count SPLIT, DATA) */
				if (q->sitd->hw_uframe & cpu_to_hc32(ft313,
								     1 << uframe)) {
					if (q->sitd->hw_fullspeed_ep &
							cpu_to_hc32(ft313, 1<<31))
						usecs += q->sitd->stream->usecs;
					else	/* worst case for OUT start-split */
						usecs += HS_USECS_ISO (188);
				}

				/* ... C-mask?  (count CSPLIT, DATA) */
				if (q->sitd->hw_uframe &
						cpu_to_hc32(ft313, 1 << (8 + uframe))) {
					/* worst case for IN complete-split */
					usecs += q->sitd->stream->c_usecs;
				}

				hw_p = &q->sitd->hw_next;
				q = &q->sitd->sitd_next;
				break;
		}
	}
#ifdef	DEBUG
	if (usecs > 100)
		ehci_err (ft313, "uframe %d sched overrun: %d usecs\n",
			  frame * 8 + uframe, usecs);
#endif
//	FUN_EXIT();
	return usecs;
}

/*-------------------------------------------------------------------------*/

static int same_tt (struct usb_device *dev1, struct usb_device *dev2)
{
	if (!dev1->tt || !dev2->tt)
		return 0;
	if (dev1->tt != dev2->tt)
		return 0;
	if (dev1->tt->multi)
		return dev1->ttport == dev2->ttport;
	else
		return 1;
}

#ifdef CONFIG_USB_EHCI_TT_NEWSCHED

/* Which uframe does the low/fullspeed transfer start in?
 *
 * The parameter is the mask of ssplits in "H-frame" terms
 * and this returns the transfer start uframe in "B-frame" terms,
 * which allows both to match, e.g. a ssplit in "H-frame" uframe 0
 * will cause a transfer in "B-frame" uframe 0.  "B-frames" lag
 * "H-frames" by 1 uframe.  See the EHCI spec sec 4.5 and figure 4.7.
 */
static inline unsigned char tt_start_uframe(struct ft313_hcd *ft313, __hc32 mask)
{
	unsigned char smask = QH_SMASK & hc32_to_cpu(ft313, mask);
	if (!smask) {
		ehci_err(ft313, "invalid empty smask!\n");
		/* uframe 7 can't have bw so this will indicate failure */
		return 7;
	}
	return ffs(smask) - 1;
}

static const unsigned char
max_tt_usecs[] = { 125, 125, 125, 125, 125, 125, 30, 0 };

/* carryover low/fullspeed bandwidth that crosses uframe boundries */
static inline void carryover_tt_bandwidth(unsigned short tt_usecs[8])
{
	int i;
	for (i=0; i<7; i++) {
		if (max_tt_usecs[i] < tt_usecs[i]) {
			tt_usecs[i+1] += tt_usecs[i] - max_tt_usecs[i];
			tt_usecs[i] = max_tt_usecs[i];
		}
	}
}

/* How many of the tt's periodic downstream 1000 usecs are allocated?
 *
 * While this measures the bandwidth in terms of usecs/uframe,
 * the low/fullspeed bus has no notion of uframes, so any particular
 * low/fullspeed transfer can "carry over" from one uframe to the next,
 * since the TT just performs downstream transfers in sequence.
 *
 * For example two separate 100 usec transfers can start in the same uframe,
 * and the second one would "carry over" 75 usecs into the next uframe.
 */
static void
periodic_tt_usecs (
	struct ft313_hcd *ft313,
	struct usb_device *dev,
	unsigned frame,
	unsigned short tt_usecs[8]
)
{
	__hc32			*hw_p = &ft313->periodic [frame];
	union ehci_shadow	*q = &ft313->pshadow [frame];
	unsigned char		uf;

	memset(tt_usecs, 0, 16);

	while (q->ptr) {
		switch (hc32_to_cpu(ft313, Q_NEXT_TYPE(ft313, *hw_p))) {
			case Q_TYPE_ITD:
				hw_p = &q->itd->hw_next;
				q = &q->itd->itd_next;
				continue;
			case Q_TYPE_QH:
				if (same_tt(dev, q->qh->dev)) {
					uf = tt_start_uframe(ft313, q->qh->hw->hw_info2);
					tt_usecs[uf] += q->qh->tt_usecs;
				}
				hw_p = &q->qh->hw->hw_next;
				q = &q->qh->qh_next;
				continue;
			case Q_TYPE_SITD:
				if (same_tt(dev, q->sitd->urb->dev)) {
					uf = tt_start_uframe(ft313, q->sitd->hw_uframe);
					tt_usecs[uf] += q->sitd->stream->tt_usecs;
				}
				hw_p = &q->sitd->hw_next;
				q = &q->sitd->sitd_next;
				continue;
				// case Q_TYPE_FSTN:
			default:
				ehci_dbg(ft313, "ignoring periodic frame %d FSTN\n",
					 frame);
				hw_p = &q->fstn->hw_next;
				q = &q->fstn->fstn_next;
		}
	}

	carryover_tt_bandwidth(tt_usecs);

	if (max_tt_usecs[7] < tt_usecs[7])
		ehci_err(ft313, "frame %d tt sched overrun: %d usecs\n",
			 frame, tt_usecs[7] - max_tt_usecs[7]);
}

/*
 * Return true if the device's tt's downstream bus is available for a
 * periodic transfer of the specified length (usecs), starting at the
 * specified frame/uframe.  Note that (as summarized in section 11.19
 * of the usb 2.0 spec) TTs can buffer multiple transactions for each
 * uframe.
 *
 * The uframe parameter is when the fullspeed/lowspeed transfer
 * should be executed in "B-frame" terms, which is the same as the
 * highspeed ssplit's uframe (which is in "H-frame" terms).  For example
 * a ssplit in "H-frame" 0 causes a transfer in "B-frame" 0.
 * See the EHCI spec sec 4.5 and fig 4.7.
 *
 * This checks if the full/lowspeed bus, at the specified starting uframe,
 * has the specified bandwidth available, according to rules listed
 * in USB 2.0 spec section 11.18.1 fig 11-60.
 *
 * This does not check if the transfer would exceed the max ssplit
 * limit of 16, specified in USB 2.0 spec section 11.18.4 requirement #4,
 * since proper scheduling limits ssplits to less than 16 per uframe.
 */
static int tt_available (
	struct ft313_hcd		*ft313,
	unsigned		period,
	struct usb_device	*dev,
	unsigned		frame,
	unsigned		uframe,
	u16			usecs
)
{
	if ((period == 0) || (uframe >= 7))	/* error */
		return 0;

	for (; frame < ft313->periodic_size; frame += period) {
		unsigned short tt_usecs[8];

		periodic_tt_usecs (ft313, dev, frame, tt_usecs);

		ehci_vdbg(ft313, "tt frame %d check %d usecs start uframe %d in"
			  " schedule %d/%d/%d/%d/%d/%d/%d/%d\n",
			  frame, usecs, uframe,
			  tt_usecs[0], tt_usecs[1], tt_usecs[2], tt_usecs[3],
			  tt_usecs[4], tt_usecs[5], tt_usecs[6], tt_usecs[7]);

		if (max_tt_usecs[uframe] <= tt_usecs[uframe]) {
			ehci_vdbg(ft313, "frame %d uframe %d fully scheduled\n",
				  frame, uframe);
			return 0;
		}

		/* special case for isoc transfers larger than 125us:
		 * the first and each subsequent fully used uframe
		 * must be empty, so as to not illegally delay
		 * already scheduled transactions
		 */
		if (125 < usecs) {
			int ufs = (usecs / 125);
			int i;
			for (i = uframe; i < (uframe + ufs) && i < 8; i++)
				if (0 < tt_usecs[i]) {
					ehci_vdbg(ft313,
						  "multi-uframe xfer can't fit "
						  "in frame %d uframe %d\n",
						  frame, i);
					return 0;
				}
		}

		tt_usecs[uframe] += usecs;

		carryover_tt_bandwidth(tt_usecs);

		/* fail if the carryover pushed bw past the last uframe's limit */
		if (max_tt_usecs[7] < tt_usecs[7]) {
			ehci_vdbg(ft313,
				  "tt unavailable usecs %d frame %d uframe %d\n",
				  usecs, frame, uframe);
			return 0;
		}
	}

	return 1;
}

#else

/* return true iff the device's transaction translator is available
 * for a periodic transfer starting at the specified frame, using
 * all the uframes in the mask.
 */
static int tt_no_collision (
	struct ft313_hcd	*ft313,
	unsigned		period,
	struct usb_device	*dev,
	unsigned		frame,
	u32			uf_mask
)
{
	if (period == 0)	/* error */
		return 0;

	/* note bandwidth wastage:  split never follows csplit
	 * (different dev or endpoint) until the next uframe.
	 * calling convention doesn't make that distinction.
	 */
	for (; frame < ft313->periodic_size; frame += period) {
		union ehci_shadow	here;
		__hc32			type;
		struct ehci_qh_hw	*hw;

		here = ft313->pshadow [frame];
		type = Q_NEXT_TYPE(ft313, ft313->periodic [frame]);
		while (here.ptr) {
			switch (hc32_to_cpu(ft313, type)) {
				case Q_TYPE_ITD:
					DEBUG_MSG("iTD found\n");
					type = Q_NEXT_TYPE(ft313, here.itd->hw_next);
					here = here.itd->itd_next;
					continue;
				case Q_TYPE_QH:
					DEBUG_MSG("qH found\n");
					hw = here.qh->hw;
					if (same_tt (dev, here.qh->dev)) {
						u32		mask;

						mask = hc32_to_cpu(ft313,
								   hw->hw_info2);
						/* "knows" no gap is needed */
						mask |= mask >> 8;
						if (mask & uf_mask)
							break;
					}
					type = Q_NEXT_TYPE(ft313, hw->hw_next);
					here = here.qh->qh_next;
					continue;
				case Q_TYPE_SITD:
					DEBUG_MSG("siTD found\n");
					if (same_tt (dev, here.sitd->urb->dev)) {
						u16		mask;

						mask = hc32_to_cpu(ft313, here.sitd
								   ->hw_uframe);
						/* FIXME assumes no gap for IN! */
						mask |= mask >> 8;
						if (mask & uf_mask)
							break;
					}
					type = Q_NEXT_TYPE(ft313, here.sitd->hw_next);
					here = here.sitd->sitd_next;
					continue;
					// case Q_TYPE_FSTN:
				default:
					DEBUG_MSG("periodic frame %d bogus type %d\n",
						  frame, type);
			}

			/* collision or error */
			return 0;
		}
	}

	/* no collision */
	return 1;
}

#endif /* CONFIG_USB_EHCI_TT_NEWSCHED */

/*-------------------------------------------------------------------------*/

static int enable_periodic (struct ft313_hcd *ft313)
{
	u32	cmd;
	int	status;

	FUN_ENTRY();

	if (ft313->periodic_sched++) {
		FUN_EXIT();
		return 0;
	}

	/* did clearing PSE did take effect yet?
	 * takes effect only at frame boundaries...
	 */
	status = handshake_on_error_set_halt(ft313, &ft313->regs->status,
					     PSCH_STS, 0, 9 * 125);
	if (status) {
		ERROR_MSG("FT313 Host dead\n");
		usb_hc_died(ft313_to_hcd(ft313));
		return status;
	}

	cmd = ft313_reg_read32(ft313, &ft313->regs->command) | PSCH_EN;
	ft313_reg_write32(ft313, cmd, &ft313->regs->command);
	DEBUG_MSG("Start periodic scheduling\n");
	/* posted write ... PSS happens later */
	ft313_to_hcd(ft313)->state = HC_STATE_RUNNING;

	/* make sure ehci_work scans these */
	ft313->next_uframe = ft313_reg_read32(ft313, &ft313->regs->frame_index)
			     % (ft313->periodic_size << 3);
//	if (unlikely(ft313->broken_periodic))
//		ft313->last_periodic_enable = ktime_get_real();

	FUN_EXIT();

	return 0;
}

static int disable_periodic (struct ft313_hcd *ft313)
{
	u32	cmd;
	int	status;

	FUN_ENTRY();

	if (--ft313->periodic_sched) {
		FUN_EXIT();
		return 0;
	}
#if 0 // Faraday code does not have this
	if (unlikely(ft313->broken_periodic)) {
		/* delay experimentally determined */
		ktime_t safe = ktime_add_us(ft313->last_periodic_enable, 1000);
		ktime_t now = ktime_get_real();
		s64 delay = ktime_us_delta(safe, now);

		if (unlikely(delay > 0))
			udelay(delay);
	}
#endif

	/* did setting PSE not take effect yet?
	 * takes effect only at frame boundaries...
	 */
	status = handshake_on_error_set_halt(ft313, &ft313->regs->status,
					     PSCH_STS, PSCH_STS, 9 * 125);
	if (status) {
		usb_hc_died(ft313_to_hcd(ft313));
		return status;
	}

	cmd = ft313_reg_read32(ft313, &ft313->regs->command) & ~PSCH_EN;
	ft313_reg_write32(ft313, cmd, &ft313->regs->command);
	DEBUG_MSG("Stop periodic schedule\n");
	/* posted write ... */

	// Below related to iso transfer
	free_cached_lists(ft313);

	ft313->next_uframe = -1;

	FUN_EXIT();

	return 0;
}


/*-------------------------------------------------------------------------*/

/* periodic schedule slots have iso tds (normal or split) first, then a
 * sparse tree for active interrupt transfers.
 *
 * this just links in a qh; caller guarantees uframe masks are set right.
 * no FSTN support (yet; ehci 0.96+)
 */
static int qh_link_periodic (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	unsigned	i;
	unsigned	period = qh->period;
	int		hw_p_ft313;

	FUN_ENTRY();

#if 0
	dev_dbg (&qh->dev->dev,
		 "link qh%d-%04x/%p start %d [%d/%d us]\n",
		 period, hc32_to_cpup(ft313, &qh->hw->hw_info2)
		 & (QH_CMASK | QH_SMASK),
		 qh, qh->start, qh->usecs, qh->c_usecs);
#endif
	/* high bandwidth, or otherwise every microframe */
	if (period == 0)
		period = 1;

	for (i = qh->start; i < ft313->periodic_size; i += period) {
		union ehci_shadow	*prev = &ft313->pshadow[i];
		__hc32			*hw_p = &ft313->periodic[i];
					 hw_p_ft313 = i * sizeof(__le32);
		union ehci_shadow	here = *prev;
		__hc32			type = 0;

		/* skip the iso nodes at list head */
		while (here.ptr) {
			type = Q_NEXT_TYPE(ft313, *hw_p);
			if (type == cpu_to_hc32(ft313, Q_TYPE_QH))
				break;
			prev = periodic_next_shadow(ft313, prev, type);
			hw_p = shadow_next_periodic(ft313, &here, type);
			//FixMe: currently, only qH is supported!
			//hw_p_ft313 = prev->qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next);
			hw_p_ft313 = here.qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next);
			here = *prev;
		}

		/* sorting each branch by period (slow-->fast)
		 * enables sharing interior tree nodes
		 */
		while (here.ptr && qh != here.qh) {
			if (qh->period > here.qh->period)
				break;
			prev = &here.qh->qh_next;
			hw_p = &here.qh->hw->hw_next; // hw_p no longer pointer to periodic list item!
			hw_p_ft313 = &here.qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next);
			here = *prev;
		}
		/* link in this qh, unless some earlier pass did that */
		if (qh != here.qh) {
			qh->qh_next = here;
			if (here.qh) {
				qh->hw->hw_next = *hw_p;
				DEBUG_MSG("Update to HW\n");
				ft313_mem_write(ft313,
						hw_p,
						sizeof(qh->hw->hw_next),
						qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));
			}
			wmb ();
			prev->qh = qh;
//			*hw_p = QH_NEXT (ft313, qh->qh_dma);
			*hw_p = QH_NEXT (ft313, qh->qh_ft313);

			DEBUG_MSG("Update to HW\n");
			ft313_mem_write(ft313,
					hw_p,
					sizeof(*hw_p),
					hw_p_ft313);
		}
	}
	qh->qh_state = QH_STATE_LINKED;
	qh->xacterrs = 0;
	qh_get (qh);

	/* update per-qh bandwidth for usbfs */
	ft313_to_hcd(ft313)->self.bandwidth_allocated += qh->period
			? ((qh->usecs + qh->c_usecs) / qh->period)
			: (qh->usecs * 8);

	/* maybe enable periodic schedule processing */
	FUN_EXIT();
	return enable_periodic(ft313);
}

static int qh_unlink_periodic(struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	unsigned	i;
	unsigned	period;
	int		retval = 0;

	FUN_ENTRY();

	// FIXME:
	// IF this isn't high speed
	//   and this qh is active in the current uframe
	//   (and overlay token SplitXstate is false?)
	// THEN
	//   qh->hw_info1 |= cpu_to_hc32(1 << 7 /* "ignore" */);

	/* high bandwidth, or otherwise part of every microframe */
	if ((period = qh->period) == 0)
		period = 1;

	for (i = qh->start; i < ft313->periodic_size; i += period)
		periodic_unlink (ft313, i, qh);

	/* update per-qh bandwidth for usbfs */
	ft313_to_hcd(ft313)->self.bandwidth_allocated -= qh->period
			? ((qh->usecs + qh->c_usecs) / qh->period)
			: (qh->usecs * 8);

	dev_dbg (&qh->dev->dev,
		 "unlink qh%d-%04x/%p start %d [%d/%d us]\n",
		 qh->period,
		 hc32_to_cpup(ft313, &qh->hw->hw_info2) & (QH_CMASK | QH_SMASK),
		 qh, qh->start, qh->usecs, qh->c_usecs);

	/* qh->qh_next still "live" to HC */
	qh->qh_state = QH_STATE_UNLINK;
	qh->qh_next.ptr = NULL;
	qh_put (qh);

	/* maybe turn off periodic schedule */
	retval = disable_periodic(ft313);
	FUN_EXIT();
	return retval;
}

static void intr_deschedule (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	unsigned		wait;
	struct ehci_qh_hw	*hw = qh->hw;
	int			rc;

	FUN_ENTRY();

	DEBUG_MSG("Update qh 0x%X from HW\n", qh->qh_ft313);
	ft313_mem_read(ft313, hw, sizeof(*hw), qh->qh_ft313);

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

	qh_unlink_periodic (ft313, qh);

	/* simple/paranoid:  always delay, expecting the HC needs to read
	 * qh->hw_next or finish a writeback after SPLIT/CSPLIT ... and
	 * expect khubd to clean up after any CSPLITs we won't issue.
	 * active high speed queues may need bigger delays...
	 */
	if (list_empty (&qh->qtd_list)
			|| (cpu_to_hc32(ft313, QH_CMASK)
			    & hw->hw_info2) != 0)
		wait = 2;
	else
		wait = 55;	/* worst case: 3 * 1024 */

	udelay (wait);
	qh->qh_state = QH_STATE_IDLE;
	hw->hw_next = EHCI_LIST_END(ft313);
	ft313_mem_write(ft313, &(hw->hw_next), sizeof(hw->hw_next), qh->qh_ft313);

	wmb ();

	qh_completions(ft313, qh);

	/* reschedule QH iff another request is queued */
	if (!list_empty(&qh->qtd_list) &&
			HC_IS_RUNNING(ft313_to_hcd(ft313)->state)) {
		rc = qh_schedule(ft313, qh);

		/* An error here likely indicates handshake failure
		 * or no space left in the schedule.  Neither fault
		 * should happen often ...
		 *
		 * FIXME kill the now-dysfunctional queued urbs
		 */
		if (rc != 0)
			ERROR_MSG("can't reschedule qh %p, err %d\n",
				 qh, rc);
	}

	FUN_EXIT();
}

/*-------------------------------------------------------------------------*/

static int check_period (
	struct ft313_hcd *ft313,
	unsigned	frame,
	unsigned	uframe,
	unsigned	period,
	unsigned	usecs
) {
	int		claimed;

	FUN_ENTRY();

	DEBUG_MSG("frame = %d, uframe = %d, period = %d, usecs = %d\n", frame, uframe, period, usecs);

	/* complete split running into next frame?
	 * given FSTN support, we could sometimes check...
	 */
	if (uframe >= 8) {
		DEBUG_MSG("Error case\n");
		FUN_EXIT();
		return 0;
	}

	/*
	 * 80% periodic == 100 usec/uframe available
	 * convert "usecs we need" to "max already claimed"
	 */
	usecs = 100 - usecs;

	/* we "know" 2 and 4 uframe intervals were rejected; so
	 * for period 0, check _every_ microframe in the schedule.
	 */
	if (unlikely (period == 0)) {
		do {
			for (uframe = 0; uframe < 7; uframe++) {
				claimed = periodic_usecs (ft313, frame, uframe);
				if (claimed > usecs) {
					FUN_EXIT();
					return 0;
				}
			}
		} while ((frame += 1) < ft313->periodic_size);

		/* just check the specified uframe, at that period */
	} else {
		do {
			claimed = periodic_usecs (ft313, frame, uframe);
			if (claimed > usecs) {
				FUN_EXIT();
				return 0;
			}
		} while ((frame += period) < ft313->periodic_size);
	}

	// success!
	FUN_EXIT();
	return 1;
}

static int check_intr_schedule (
	struct ft313_hcd	*ft313,
	unsigned		frame,
	unsigned		uframe,
	const struct ehci_qh	*qh,
	__hc32			*c_maskp
)
{
	int		retval = -ENOSPC;
	u8		mask = 0;

	FUN_ENTRY();

	if (qh->c_usecs && uframe >= 6)		/* FSTN territory? */
		goto done;

	if (!check_period (ft313, frame, uframe, qh->period, qh->usecs))
		goto done;
	if (!qh->c_usecs) {
		retval = 0;
		*c_maskp = 0;
		goto done;
	}

#ifdef CONFIG_USB_EHCI_TT_NEWSCHED
	if (tt_available (ft313, qh->period, qh->dev, frame, uframe,
			  qh->tt_usecs)) {
		unsigned i;

		/* TODO : this may need FSTN for SSPLIT in uframe 5. */
		for (i=uframe+1; i<8 && i<uframe+4; i++)
			if (!check_period (ft313, frame, i,
					   qh->period, qh->c_usecs))
				goto done;
			else
				mask |= 1 << i;

		retval = 0;

		*c_maskp = cpu_to_hc32(ft313, mask << 8);
	}
#else
	/* Make sure this tt's buffer is also available for CSPLITs.
	 * We pessimize a bit; probably the typical full speed case
	 * doesn't need the second CSPLIT.
	 *
	 * NOTE:  both SPLIT and CSPLIT could be checked in just
	 * one smart pass...
	 */
	mask = 0x03 << (uframe + qh->gap_uf);
	*c_maskp = cpu_to_hc32(ft313, mask << 8);

	mask |= 1 << uframe;
	if (tt_no_collision (ft313, qh->period, qh->dev, frame, mask)) {
		if (!check_period (ft313, frame, uframe + qh->gap_uf + 1,
				   qh->period, qh->c_usecs))
			goto done;
		if (!check_period (ft313, frame, uframe + qh->gap_uf,
				   qh->period, qh->c_usecs))
			goto done;
		retval = 0;
	}
#endif
done:
	FUN_EXIT();
	return retval;
}

/* "first fit" scheduling policy used the first time through,
 * or when the previous schedule slot can't be re-used.
 */
static int qh_schedule(struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	int		status;
	unsigned	uframe;
	__hc32		c_mask;
	unsigned	frame;		/* 0..(qh->period - 1), or NO_FRAME */
	struct ehci_qh_hw	*hw = qh->hw;

	FUN_ENTRY();

	DEBUG_MSG("qH 0x%X being scheduled\n", qh->qh_ft313);

	qh_refresh(ft313, qh); //qh->hw will be refreshed in this function
	hw->hw_next = EHCI_LIST_END(ft313);
	ft313_mem_write(ft313,
			&(hw->hw_next),	sizeof(hw->hw_next),
			qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));

	DEBUG_MSG("Frame number is %d\n", qh->start);
	frame = qh->start;

	/* reuse the previous schedule slots, if we can */
	if (frame < qh->period) {
		uframe = ffs(hc32_to_cpup(ft313, &hw->hw_info2) & QH_SMASK);
		status = check_intr_schedule (ft313, frame, --uframe,
					      qh, &c_mask);
	} else {
		uframe = 0;
		c_mask = 0;
		status = -ENOSPC;
	}

	/* else scan the schedule to find a group of slots such that all
	 * uframes have enough periodic bandwidth available.
	 */
	if (status) {
		/* "normal" case, uframing flexible except with splits */
		if (qh->period) {
			int		i;

			for (i = qh->period; status && i > 0; --i) {
				frame = ++ft313->random_frame % qh->period;
				for (uframe = 0; uframe < 8; uframe++) {
					status = check_intr_schedule (ft313,
								      frame, uframe, qh,
								      &c_mask);
					if (status == 0)
						break;
				}
			}

			/* qh->period == 0 means every uframe */
		} else {
			frame = 0;
			status = check_intr_schedule (ft313, 0, 0, qh, &c_mask);
		}
		if (status)
			goto done;
		qh->start = frame;

		/* reset S-frame and (maybe) C-frame masks */
		hw->hw_info2 &= cpu_to_hc32(ft313, ~(QH_CMASK | QH_SMASK));
		hw->hw_info2 |= qh->period
				? cpu_to_hc32(ft313, 1 << uframe)
				: cpu_to_hc32(ft313, QH_SMASK);
		hw->hw_info2 |= c_mask;

		DEBUG_MSG("Update qH DWord2 (S and C Frame)\n");
		ft313_mem_write(ft313,
				&(hw->hw_info2),
				sizeof(hw->hw_info2),
				qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_info2));
	} else
		DEBUG_MSG("reused qh %p (0x%X)schedule\n", qh, qh->qh_ft313);

	/* stuff into the periodic schedule */
	status = qh_link_periodic (ft313, qh);
done:
	FUN_EXIT();
	return status;
}

static int intr_submit (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct list_head	*qtd_list,
	gfp_t			mem_flags
) {
	unsigned		epnum;
	unsigned long		flags;
	struct ehci_qh		*qh;
	int			status;
	struct list_head	empty;

	FUN_ENTRY();

	/* get endpoint and transfer/schedule data */
	epnum = urb->ep->desc.bEndpointAddress;

	spin_lock_irqsave (&ft313->lock, flags);

	if (unlikely(!HCD_HW_ACCESSIBLE(ft313_to_hcd(ft313)))) {
		status = -ESHUTDOWN;
		goto done_not_linked;
	}
	status = usb_hcd_link_urb_to_ep(ft313_to_hcd(ft313), urb);
	if (unlikely(status))
		goto done_not_linked;

	/* get qh and force any scheduling errors */
	INIT_LIST_HEAD (&empty);
	qh = qh_append_tds(ft313, urb, &empty, epnum, &urb->ep->hcpriv);
	if (qh == NULL) {
		status = -ENOMEM;
		goto done;
	}
	if (qh->qh_state == QH_STATE_IDLE) {
		if ((status = qh_schedule (ft313, qh)) != 0)
			goto done;
	}

	/* then queue the urb's tds to the qh */
	qh = qh_append_tds(ft313, urb, qtd_list, epnum, &urb->ep->hcpriv);
	BUG_ON (qh == NULL);

	/* ... update usbfs periodic stats */
	ft313_to_hcd(ft313)->self.bandwidth_int_reqs++;

done:
	if (unlikely(status))
		usb_hcd_unlink_urb_from_ep(ft313_to_hcd(ft313), urb);
done_not_linked:
	spin_unlock_irqrestore (&ft313->lock, flags);
	if (status)
		qtd_list_free (ft313, urb, qtd_list);

	FUN_EXIT();

	return status;
}

#if 1 // Iso related start

/*-------------------------------------------------------------------------*/

u32 get_iso_urb_size(struct urb *urb)
{
	u32 total_length = 0;
	int i, num_of_pkts = 0;

	if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
		num_of_pkts = urb->number_of_packets;

		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned length, offset;

			offset = urb->iso_frame_desc[i].offset;
			length = urb->iso_frame_desc[i].length;

			DEBUG_MSG("No. %d segment at 0x%X with length %d\n", i + 1, offset, length);
			//total_length += length;

		}

		total_length = urb->iso_frame_desc[num_of_pkts - 1].offset +
			       urb->iso_frame_desc[num_of_pkts - 1].length;



	} else {
		ALERT_MSG("urb 0x%X is not isochronous URB \n", urb);
	}

	DEBUG_MSG("Total payload size for iso urb 0x%X is %d with %d segments\n", \
		  urb, total_length, num_of_pkts);

	return total_length;
}


/* ehci_iso_stream ops work with both ITD and SITD */

static struct ehci_iso_stream *
iso_stream_alloc (gfp_t mem_flags)
{
	struct ehci_iso_stream *stream;

	DEBUG_MSG("Create a new stream!\n");

	stream = kzalloc(sizeof *stream, mem_flags);
	if (likely (stream != NULL)) {
		INIT_LIST_HEAD(&stream->td_list);
		INIT_LIST_HEAD(&stream->free_list);
		INIT_LIST_HEAD(&stream->urb_list);
		stream->next_uframe = -1;
		stream->refcount = 1;
	}
	return stream;
}

static void
iso_stream_init (
	struct ft313_hcd	*ft313,
	struct ehci_iso_stream	*stream,
	struct usb_device	*dev,
	int			pipe,
	unsigned		interval
)
{
	static const u8 smask_out [] = { 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f };

	u32			buf1;
	unsigned		epnum, maxp;
	int			is_input;
	long			bandwidth;

	FUN_ENTRY();

	/*
	 * this might be a "high bandwidth" highspeed endpoint,
	 * as encoded in the ep descriptor's wMaxPacket field
	 */
	epnum = usb_pipeendpoint (pipe);
	is_input = usb_pipein (pipe) ? USB_DIR_IN : 0;
	maxp = usb_maxpacket(dev, pipe, !is_input);
	if (is_input) {
		buf1 = (1 << 11);
	} else {
		buf1 = 0;
	}

	/* knows about ITD vs SITD */
	if (dev->speed == USB_SPEED_HIGH) {
		unsigned multi = hb_mult(maxp);

		stream->highspeed = 1;

		maxp = max_packet(maxp);
		buf1 |= maxp;
		maxp *= multi;

		stream->buf0 = cpu_to_hc32(ft313, (epnum << 8) | dev->devnum);
		stream->buf1 = cpu_to_hc32(ft313, buf1);
		stream->buf2 = cpu_to_hc32(ft313, multi);

		/* usbfs wants to report the average usecs per frame tied up
		 * when transfers on this endpoint are scheduled ...
		 */
		stream->usecs = HS_USECS_ISO (maxp);
		bandwidth = stream->usecs * 8;
		bandwidth /= interval;

	} else {
		u32		addr;
		int		think_time;
		int		hs_transfers;

		addr = dev->ttport << 24;
		if (!ehci_is_TDI(ft313)
				|| (dev->tt->hub !=
					ft313_to_hcd(ft313)->self.root_hub))
			addr |= dev->tt->hub->devnum << 16;
		addr |= epnum << 8;
		addr |= dev->devnum;
		stream->usecs = HS_USECS_ISO (maxp);
		think_time = dev->tt ? dev->tt->think_time : 0;
		stream->tt_usecs = NS_TO_US (think_time + usb_calc_bus_time (
				dev->speed, is_input, 1, maxp));
		hs_transfers = max (1u, (maxp + 187) / 188);
		if (is_input) {
			u32	tmp;

			addr |= 1 << 31;
			stream->c_usecs = stream->usecs;
			stream->usecs = HS_USECS_ISO (1);
			stream->raw_mask = 1;

			/* c-mask as specified in USB 2.0 11.18.4 3.c */
			tmp = (1 << (hs_transfers + 2)) - 1;
			stream->raw_mask |= tmp << (8 + 2);
		} else
			stream->raw_mask = smask_out [hs_transfers - 1];
		bandwidth = stream->usecs + stream->c_usecs;
		bandwidth /= interval << 3;

		/* stream->splits gets created from raw_mask later */
		stream->address = cpu_to_hc32(ft313, addr);
	}
	stream->bandwidth = bandwidth;

	stream->udev = dev;

	stream->bEndpointAddress = is_input | epnum;
	stream->interval = interval;
	stream->maxp = maxp;

	DEBUG_MSG("stream 0x%X's maxpacket with multi is %d, interval is %d\n", stream, maxp, interval);

	FUN_EXIT();
}

static void
iso_stream_put(struct ft313_hcd *ft313, struct ehci_iso_stream *stream)
{
	FUN_ENTRY();

	stream->refcount--;

	/* free whenever just a dev->ep reference remains.
	 * not like a QH -- no persistent state (toggle, halt)
	 */
	if ((stream->refcount == 1) &&
	    (stream->urb_waiting == 0)) { // No urb waiting in queue as operation below is clear stream ptr in ep->hcpriv
		// BUG_ON (!list_empty(&stream->td_list));
		DEBUG_MSG("Clean up stream 0x%X\n", stream);
		while (!list_empty (&stream->free_list)) {
			struct list_head	*entry;

			entry = stream->free_list.next;
			list_del (entry);

			// Free stream buffer memory in FT313
			if (stream->buffer_ft313 != 0) {
				free_mem_blk(ft313, stream->buffer_ft313);
				stream->buffer_ft313 = 0;
			}

			/* knows about ITD vs SITD */
			if (stream->highspeed) {
				struct ehci_itd		*itd;

				itd = list_entry (entry, struct ehci_itd,
						itd_list);

				if (0 != itd->itd_ft313)
					free_mem_blk(ft313, itd->itd_ft313);
#ifndef DMA_POOL_WORKAROUND
				dma_pool_free (ft313->itd_pool, itd,
						itd->itd_dma);
#else
				kfree(itd);
#endif
			} else {
				struct ehci_sitd	*sitd;

				sitd = list_entry (entry, struct ehci_sitd,
						sitd_list);

				if (0 != sitd->sitd_ft313)
					free_mem_blk(ft313, sitd->sitd_ft313);
#ifndef DMA_POOL_WORKAROUND
				dma_pool_free (ft313->sitd_pool, sitd,
						sitd->sitd_dma);
#else
				kfree(sitd);
#endif
			}
		}

		stream->bEndpointAddress &= 0x0f;
		if (stream->ep) {
			DEBUG_MSG("ep->hcpriv is cleared for EP 0x%X\n", stream->ep->desc.bEndpointAddress);
			stream->ep->hcpriv = NULL;
		}

		kfree(stream);
	}

	FUN_EXIT();
}

static inline struct ehci_iso_stream *
iso_stream_get (struct ehci_iso_stream *stream)
{
	if (likely (stream != NULL))
		stream->refcount++;
	return stream;
}

static struct ehci_iso_stream *
iso_stream_find (struct ft313_hcd *ft313, struct urb *urb)
{
	unsigned		epnum;
	struct ehci_iso_stream	*stream;
	struct usb_host_endpoint *ep;
	unsigned long		flags;

	epnum = usb_pipeendpoint (urb->pipe);
	if (usb_pipein(urb->pipe))
		ep = urb->dev->ep_in[epnum];
	else
		ep = urb->dev->ep_out[epnum];

	spin_lock_irqsave (&ft313->lock, flags);
	stream = ep->hcpriv;

	if (unlikely (stream == NULL)) {
		DEBUG_MSG("stream 0x%X is still NULL, need initialization\n", stream);
		stream = iso_stream_alloc(GFP_ATOMIC);
		if (likely (stream != NULL)) {
			/* dev->ep owns the initial refcount */
			ep->hcpriv = stream;
			stream->ep = ep;
			iso_stream_init(ft313, stream, urb->dev, urb->pipe,
					urb->interval);
		}

	/* if dev->ep [epnum] is a QH, hw is set */
	} else if (unlikely (stream->hw != NULL)) {
		DEBUG_MSG("dev %s ep%d%s, not iso??\n",
			urb->dev->devpath, epnum,
			usb_pipein(urb->pipe) ? "in" : "out");
		stream = NULL;
	}

	/* caller guarantees an eventual matching iso_stream_put */
	stream = iso_stream_get (stream);

	spin_unlock_irqrestore (&ft313->lock, flags);
	return stream;
}

/*-------------------------------------------------------------------------*/

/* ehci_iso_sched ops can be ITD-only or SITD-only */

static struct ehci_iso_sched *
iso_sched_alloc (unsigned packets, gfp_t mem_flags)
{
	struct ehci_iso_sched	*iso_sched;
	int			size = sizeof *iso_sched;

	size += packets * sizeof (struct ehci_iso_packet);
	iso_sched = kzalloc(size, mem_flags);
	if (likely (iso_sched != NULL)) {
		INIT_LIST_HEAD (&iso_sched->td_list);
	}
	return iso_sched;
}

static inline int
itd_sched_init(
	struct ft313_hcd	*ft313,
	struct ehci_iso_sched	*iso_sched,
	struct ehci_iso_stream	*stream,
	struct urb		*urb
)
{
	unsigned	i;
	u32		urb_total_length = 0; //, actual_len;
//	dma_addr_t	dma = urb->transfer_dma;
//	void		*urb_buffer = urb->transfer_buffer;
	u32		buffer_ft313;
	struct ft313_mem_blk *mem_blk_ptr;
	int is_input, maxpacket;

	FUN_ENTRY();

	is_input = usb_pipein (urb->pipe);
	maxpacket = max_packet(usb_maxpacket(urb->dev, urb->pipe, !is_input));

	urb_total_length = get_iso_urb_size(urb);

	mem_blk_ptr = allocate_mem_blk(ft313, BUFFER, urb_total_length); // Allocate comm buffer
	if (mem_blk_ptr == NULL) {
		ALERT_MSG("No more memory block available \n");
		FUN_EXIT();
		return -1;
	}
	DEBUG_MSG("Got a buffer with size %d at 0x%X.\n", mem_blk_ptr->size, mem_blk_ptr->offset);

	if (mem_blk_ptr->size < urb_total_length) { // Get a smaller buffer than needed
		if (mem_blk_ptr->size < maxpacket) {
			free_mem_blk(ft313, mem_blk_ptr->offset);
			ALERT_MSG("No enough buffer to serve this urb\n");
			FUN_EXIT();
			return -2;
		}
	}

	buffer_ft313 = mem_blk_ptr->offset;
	DEBUG_MSG("Payload buffer at 0x%X with size %d\n", buffer_ft313, mem_blk_ptr->size);
	if (stream->buffer_ft313 == 0) // No urb under execution
		stream->buffer_ft313 = buffer_ft313;
	else {
		struct iso_urb_queue_item *iso_urb_q_item;
		iso_urb_q_item = kmalloc(sizeof(struct iso_urb_queue_item), GFP_ATOMIC);
		if (NULL != iso_urb_q_item) {
			iso_urb_q_item->urb = urb;
			iso_urb_q_item->urb_buffer = buffer_ft313;

			INIT_LIST_HEAD(&iso_urb_q_item->urb_list);
			list_add_tail(&iso_urb_q_item->urb_list, &stream->urb_list);

			DEBUG_MSG("urb 0x%X's buffer offset 0x%X is queued\n", urb, buffer_ft313);

		}
		else {
			ERROR_MSG("Memory allocation for iso queue failed!\n");
			FUN_EXIT();
			return -3;
		}
	}

	stream->urb = urb;
	//actual_len = min(urb_total_length, mem_blk_ptr->size);

	/* how many uframes are needed for these transfers */
	iso_sched->span = urb->number_of_packets * stream->interval;

	// Assume we have enough memory first!
	iso_sched->actual_number_of_packets = urb->number_of_packets;

	/* figure out per-uframe itd fields that we'll need later
	 * when we fit new itds into the schedule.
	 */
	for (i = 0; i < urb->number_of_packets; i++) {
		struct ehci_iso_packet	*uframe = &iso_sched->packet [i];
		unsigned		length;
		dma_addr_t		buf;
		u32			trans;

		length = urb->iso_frame_desc[i].length;
		buf = buffer_ft313 + urb->iso_frame_desc[i].offset;

		trans = EHCI_ISOC_ACTIVE;
		trans |= buf & 0x0fff;
		if (unlikely (((i + 1) == urb->number_of_packets))
				&& !(urb->transfer_flags & URB_NO_INTERRUPT))
			trans |= EHCI_ITD_IOC;
		trans |= length << 16;
		uframe->transaction = cpu_to_hc32(ft313, trans);
		DEBUG_MSG("Transaction DW[%d] is 0x%X\n", i, trans);

		/* might need to cross a buffer page within a uframe */
		uframe->bufp = (buf & ~(u64)0x0fff);
		buf += length;
		if (unlikely ((uframe->bufp != (buf & ~(u64)0x0fff))))
			uframe->cross = 1;

		if (((i + 1) != urb->number_of_packets) && // Not last packet!
		    ((buf + urb->iso_frame_desc[i + 1].length) > (buffer_ft313 + mem_blk_ptr->size))) {
			DEBUG_MSG("Memory used up for this urb\n");
			iso_sched->actual_number_of_packets = i + 1;
			DEBUG_MSG("Only %d packets could be programmed\n", i + 1);
			trans |= EHCI_ITD_IOC;
			uframe->transaction = cpu_to_hc32(ft313, trans);
			break;
		}
	}

	if (iso_sched->actual_number_of_packets < urb->number_of_packets)
		stream->actual_number_of_packets = iso_sched->actual_number_of_packets;
	else
		stream->actual_number_of_packets = urb->number_of_packets;

	FUN_EXIT();

	return 0;
}

static void
iso_sched_free (
	struct ehci_iso_stream	*stream,
	struct ehci_iso_sched	*iso_sched
)
{
	if (!iso_sched)
		return;
	// caller must hold ft313->lock!
	if (!list_empty(&iso_sched->td_list)) {
		DEBUG_MSG("iso sched td list is not empty\n");
	} else {
		DEBUG_MSG("iso sched td list is empty\n");
	}
	list_splice (&iso_sched->td_list, &stream->free_list);
	kfree (iso_sched);
}

static int
itd_urb_transaction (
	struct ehci_iso_stream	*stream,
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	gfp_t			mem_flags
)
{
	struct ehci_itd		*itd;
	dma_addr_t		itd_dma;
	int			i;
	unsigned		num_itds;
	struct ehci_iso_sched	*sched;
	unsigned long		flags;
	u32			itd_ft313;

	FUN_ENTRY();

	sched = iso_sched_alloc (urb->number_of_packets, mem_flags);
	if (unlikely (sched == NULL))
		return -ENOMEM;

	if (0 > itd_sched_init(ft313, sched, stream, urb)) //FixMe: not consider big URB yet!!
		return -ENOMEM;

	if (urb->interval < 8)
		num_itds = 1 + (sched->span + 7) / 8;
	else
		num_itds = urb->number_of_packets;

	DEBUG_MSG("Num of iTDs needed is %d\n", num_itds);

	/* allocate/init ITDs */
	spin_lock_irqsave (&ft313->lock, flags);
	for (i = 0; i < num_itds; i++) {

		/* free_list.next might be cache-hot ... but maybe
		 * the HC caches it too. avoid that issue for now.
		 */

		/* prefer previously-allocated itds */
		if (likely (!list_empty(&stream->free_list))) {
			DEBUG_MSG("Reuse previous iTD\n");
			itd = list_entry (stream->free_list.prev,
					struct ehci_itd, itd_list);
			list_del (&itd->itd_list);
			itd_dma = itd->itd_dma;
			itd_ft313 = itd->itd_ft313;
		} else {
			struct ft313_mem_blk	*mem_blk_ptr = NULL;

			DEBUG_MSG("Allocate iTD from FT313\n");

			mem_blk_ptr = allocate_mem_blk(ft313, ITD, 0);
			if (mem_blk_ptr == NULL) {
				ALERT_MSG("Cannot allocate iTD \n");
				iso_sched_free(stream, sched);
				spin_unlock_irqrestore(&ft313->lock, flags);
				FUN_EXIT();
				return -ENOMEM;
			}
			else {
				itd_ft313 = mem_blk_ptr->offset;
				DEBUG_MSG("Get an iTD at 0x%X from FT313\n", itd_ft313);
			}

			spin_unlock_irqrestore (&ft313->lock, flags);

			//ALERT_MSG("itd_pool is at %p when allocate itd\n", ft313->itd_pool);

#ifndef DMA_POOL_WORKAROUND
			itd = dma_pool_alloc (ft313->itd_pool, mem_flags,
					&itd_dma);
#else
			itd = kmalloc(sizeof(struct ehci_itd), GFP_ATOMIC);
			itd_dma = virt_to_bus(itd);
#endif

			spin_lock_irqsave (&ft313->lock, flags);
			if (!itd) {
				ALERT_MSG("Cannot allocate DMA memory for iTD!\n");
				iso_sched_free(stream, sched);
				spin_unlock_irqrestore(&ft313->lock, flags);
				free_mem_blk(ft313, itd_ft313);
				FUN_EXIT();
				return -ENOMEM;
			}
		}

		memset (itd, 0, sizeof *itd);
		itd->itd_dma = itd_dma;
		itd->itd_ft313 = itd_ft313;
		list_add (&itd->itd_list, &sched->td_list);
	}
	spin_unlock_irqrestore (&ft313->lock, flags);

	/* temporarily store schedule info in hcpriv */
	urb->hcpriv = sched;
	urb->error_count = 0;

	FUN_EXIT();

	return 0;
}

/*-------------------------------------------------------------------------*/

static inline int
itd_slot_ok (
	struct ft313_hcd	*ft313,
	u32			mod,
	u32			uframe,
	u8			usecs,
	u32			period
)
{
	FUN_ENTRY();

	uframe %= period;
	do {
		/* can't commit more than 80% periodic == 100 usec */
		if (periodic_usecs (ft313, uframe >> 3, uframe & 0x7)
				> (100 - usecs)) {
			FUN_EXIT();
			return 0;
		}

		/* we know urb->interval is 2^N uframes */
		uframe += period;
	} while (uframe < mod);

	FUN_EXIT();
	return 1;
}

static inline int
sitd_slot_ok (
	struct ft313_hcd	*ft313,
	u32			mod,
	struct ehci_iso_stream	*stream,
	u32			uframe,
	struct ehci_iso_sched	*sched,
	u32			period_uframes
)
{
	u32			mask, tmp;
	u32			frame, uf;

	FUN_ENTRY();

	mask = stream->raw_mask << (uframe & 7);

	/* for IN, don't wrap CSPLIT into the next frame */
	if (mask & ~0xffff)
		return 0;

	/* this multi-pass logic is simple, but performance may
	 * suffer when the schedule data isn't cached.
	 */

	/* check bandwidth */
	uframe %= period_uframes;
	do {
		u32		max_used;

		frame = uframe >> 3;
		uf = uframe & 7;

#ifdef CONFIG_USB_EHCI_TT_NEWSCHED
		/* The tt's fullspeed bus bandwidth must be available.
		 * tt_available scheduling guarantees 10+% for control/bulk.
		 */
		if (!tt_available (ehci, period_uframes << 3,
				stream->udev, frame, uf, stream->tt_usecs))
			return 0;
#else
		/* tt must be idle for start(s), any gap, and csplit.
		 * assume scheduling slop leaves 10+% for control/bulk.
		 */
		if (!tt_no_collision (ft313, period_uframes << 3,
				stream->udev, frame, mask))
			return 0;
#endif

		/* check starts (OUT uses more than one) */
		max_used = 100 - stream->usecs;
		for (tmp = stream->raw_mask & 0xff; tmp; tmp >>= 1, uf++) {
			if (periodic_usecs (ft313, frame, uf) > max_used)
				return 0;
		}

		/* for IN, check CSPLIT */
		if (stream->c_usecs) {
			uf = uframe & 7;
			max_used = 100 - stream->c_usecs;
			do {
				tmp = 1 << uf;
				tmp <<= 8;
				if ((stream->raw_mask & tmp) == 0)
					continue;
				if (periodic_usecs (ft313, frame, uf)
						> max_used)
					return 0;
			} while (++uf < 8);
		}

		/* we know urb->interval is 2^N uframes */
		uframe += period_uframes;
	} while (uframe < mod);

	stream->splits = cpu_to_hc32(ft313, stream->raw_mask << (uframe & 7));

	FUN_EXIT();

	return 1;
}

/*
 * This scheduler plans almost as far into the future as it has actual
 * periodic schedule slots.  (Affected by TUNE_FLS, which defaults to
 * "as small as possible" to be cache-friendlier.)  That limits the size
 * transfers you can stream reliably; avoid more than 64 msec per urb.
 * Also avoid queue depths of less than ehci's worst irq latency (affected
 * by the per-urb URB_NO_INTERRUPT hint, the log2_irq_thresh module parameter,
 * and other factors); or more than about 230 msec total (for portability,
 * given EHCI_TUNE_FLS and the slop).  Or, write a smarter scheduler!
 */

#define SCHEDULE_SLOP	80	/* microframes */

static int
iso_stream_schedule (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct ehci_iso_stream	*stream
)
{
	u32			now, next, start, period, span;
	int			status;
	unsigned		mod = ft313->periodic_size << 3;
	struct ehci_iso_sched	*sched = urb->hcpriv;

	FUN_ENTRY();

	period = urb->interval;
	span = sched->span;
	if (!stream->highspeed) {
		period <<= 3;
		span <<= 3;
	}

	if (span > mod - SCHEDULE_SLOP) {
		DEBUG_MSG("iso request %p too long\n", urb);
		status = -EFBIG;
		goto fail;
	}

	now = ft313_reg_read32(ft313, &ft313->regs->frame_index) & (mod - 1);

	/* Typical case: reuse current schedule, stream is still active.
	 * Hopefully there are no gaps from the host falling behind
	 * (irq delays etc), but if there are we'll take the next
	 * slot in the schedule, implicitly assuming URB_ISO_ASAP.
	 */
	if (likely (!list_empty (&stream->td_list)) ||
		   (stream->urb_waiting == 1)) { // if urb is from urb queuing
		u32	excess;

		DEBUG_MSG("stream iTD list is not empty\n");

		/* For high speed devices, allow scheduling within the
		 * isochronous scheduling threshold.  For full speed devices
		 * and Intel PCI-based controllers, don't (work around for
		 * Intel ICH9 bug).
		 */
		if (!stream->highspeed) //FixMe: need revisit! original code is "&&", not "||" !
			next = now + ft313->i_thresh;
		else
			next = now;

		/* Fell behind (by up to twice the slop amount)?
		 * We decide based on the time of the last currently-scheduled
		 * slot, not the time of the next available slot.
		 */
		excess = (stream->next_uframe - period - next) & (mod - 1);
		if (excess >= mod - 2 * SCHEDULE_SLOP)
			start = next + excess - mod + period *
					DIV_ROUND_UP(mod - excess, period);
		else
			start = next + excess + period;
		if (start - now >= mod) {
			DEBUG_MSG("request %p would overflow (%d+%d >= %d)\n",
					urb, start - now - period, period,
					mod);
			status = -EFBIG;
			goto fail;
		}
	}

	/* need to schedule; when's the next (u)frame we could start?
	 * this is bigger than ehci->i_thresh allows; scheduling itself
	 * isn't free, the slop should handle reasonably slow cpus.  it
	 * can also help high bandwidth if the dma and irq loads don't
	 * jump until after the queue is primed.
	 */
	else {
		DEBUG_MSG("Start new scheduling\n");

		start = SCHEDULE_SLOP + (now & ~0x07);

		/* NOTE:  assumes URB_ISO_ASAP, to limit complexity/bugs */

		/* find a uframe slot with enough bandwidth */
		next = start + period;
		for (; start < next; start++) {

			/* check schedule: enough space? */
			if (stream->highspeed) {
				if (itd_slot_ok(ft313, mod, start,
						stream->usecs, period))
					break;
			} else {
				if ((start % 8) >= 6)
					continue;
				if (sitd_slot_ok(ft313, mod, stream,
						start, sched, period))
					break;
			}
		}

		/* no room in the schedule */
		if (start == next) {
			ERROR_MSG("iso resched full %p (now %d max %d)\n",
				urb, now, now + mod);
			status = -ENOSPC;
			goto fail;
		}
	}

	/* Tried to schedule too far into the future? */
	if (unlikely(start - now + span - period
				>= mod - 2 * SCHEDULE_SLOP)) {
		ERROR_MSG("request %p would overflow (%d+%d >= %d)\n",
				urb, start - now, span - period,
				mod - 2 * SCHEDULE_SLOP);
		status = -EFBIG;
		goto fail;
	}

	stream->next_uframe = start & (mod - 1);

	/* report high speed start in uframes; full speed, in frames */
	urb->start_frame = stream->next_uframe;
	if (!stream->highspeed)
		urb->start_frame >>= 3;

	FUN_EXIT();
	return 0;

 fail:
	iso_sched_free(stream, sched);
	urb->hcpriv = NULL;
	FUN_EXIT();
	return status;
}

/*-------------------------------------------------------------------------*/

static inline void
itd_init(struct ft313_hcd *ft313, struct ehci_iso_stream *stream,
		struct ehci_itd *itd)
{
	int i;

	FUN_ENTRY();

	/* it's been recently zeroed */
	itd->hw_next = EHCI_LIST_END(ft313);
	itd->hw_bufp [0] = stream->buf0;
	itd->hw_bufp [1] = stream->buf1;
	itd->hw_bufp [2] = stream->buf2;

	for (i = 0; i < 8; i++)
		itd->index[i] = -1;

	ft313_mem_write(ft313, itd, sizeof(struct ehci_itd_hw), itd->itd_ft313);

	FUN_EXIT();
	/* All other fields are filled when scheduling */
}

static inline void
itd_patch(
	struct ft313_hcd	*ft313,
	struct ehci_itd		*itd,
	struct ehci_iso_sched	*iso_sched,
	unsigned		index,
	u16			uframe
)
{
	struct ehci_iso_packet	*uf = &iso_sched->packet [index];
	unsigned		pg = itd->pg;

	// BUG_ON (pg == 6 && uf->cross);
	FUN_ENTRY();

	uframe &= 0x07;
	itd->index [uframe] = index;

	itd->hw_transaction[uframe] = uf->transaction;
	itd->hw_transaction[uframe] |= cpu_to_hc32(ft313, pg << 12);
	itd->hw_bufp[pg] |= cpu_to_hc32(ft313, uf->bufp & ~(u32)0);
//	itd->hw_bufp_hi[pg] |= cpu_to_hc32(ehci, (u32)(uf->bufp >> 32));

	/* iso_frame_desc[].offset must be strictly increasing */
	if (unlikely (uf->cross)) {
		u64	bufp = uf->bufp + 4096;

		itd->pg = ++pg;
		itd->hw_bufp[pg] |= cpu_to_hc32(ft313, bufp & ~(u32)0);
//		itd->hw_bufp_hi[pg] |= cpu_to_hc32(ehci, (u32)(bufp >> 32));
	}

	ft313_mem_write(ft313, itd, sizeof(struct ehci_itd_hw), itd->itd_ft313);

	FUN_EXIT();
}

static inline void
itd_link (struct ft313_hcd *ft313, unsigned frame, struct ehci_itd *itd)
{
	union ehci_shadow	*prev = &ft313->pshadow[frame];
	__hc32			*hw_p = &ft313->periodic[frame];
	u32			hw_p_ft313 = sizeof(__hc32) * frame;
	union ehci_shadow	here = *prev;
	__hc32			type = 0;

	/* skip any iso nodes which might belong to previous microframes */
	while (here.ptr) {
		DEBUG_MSG("here.ptr is 0x%X\n", here.ptr);

		type = Q_NEXT_TYPE(ft313, *hw_p);
		if (type == cpu_to_hc32(ft313, Q_TYPE_QH))
			break;
		prev = periodic_next_shadow(ft313, prev, type);
		hw_p = shadow_next_periodic(ft313, &here, type);

		switch (hc32_to_cpu(ft313, type)) {
			case Q_TYPE_QH:
				DEBUG_MSG("Meet qH\n");
				hw_p_ft313 = here.qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next);
				break;
			case Q_TYPE_FSTN:
				ALERT_MSG("FSTN not supported\n");
				break;
			case Q_TYPE_ITD:
				DEBUG_MSG("Meet iTD\n");
				hw_p_ft313 = here.itd->itd_ft313 + offsetof(struct ehci_itd_hw, hw_next);
				break;
				// case Q_TYPE_SITD:
			default:
				ALERT_MSG("SITD not supported yet\n");
				hw_p_ft313 = here.sitd->sitd_ft313 + offsetof(struct ehci_sitd_hw, hw_next);
				break;
		}

		here = *prev;
	}

	itd->itd_next = here;
	itd->hw_next = *hw_p;
	ft313_mem_write(ft313,
			&(itd->hw_next),
			sizeof(itd->hw_next),
			itd->itd_ft313 + offsetof(struct ehci_itd_hw, hw_next));
	prev->itd = itd;
	itd->frame = frame;
	wmb ();
//	*hw_p = cpu_to_hc32(ft313, itd->itd_dma | Q_TYPE_ITD);
	*hw_p = cpu_to_hc32(ft313, itd->itd_ft313 | Q_TYPE_ITD);
	ft313_mem_write(ft313, hw_p, sizeof(*hw_p), hw_p_ft313);
}

/* fit urb's itds into the selected schedule slot; activate as needed */
static int
itd_link_urb (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	unsigned		mod,
	struct ehci_iso_stream	*stream
)
{
	int			packet;
	unsigned		next_uframe, uframe, frame;
	struct ehci_iso_sched	*iso_sched = urb->hcpriv;
	struct ehci_itd		*itd;

	int			ret = 0;

	FUN_ENTRY();

	next_uframe = stream->next_uframe & (mod - 1);

	if (unlikely (list_empty(&stream->td_list))) {
		ft313_to_hcd(ft313)->self.bandwidth_allocated
				+= stream->bandwidth;
	/*	ehci_vdbg (ehci,
			"schedule devp %s ep%d%s-iso period %d start %d.%d\n",
			urb->dev->devpath, stream->bEndpointAddress & 0x0f,
			(stream->bEndpointAddress & USB_DIR_IN) ? "in" : "out",
			urb->interval,
			next_uframe >> 3, next_uframe & 0x7);*/
	}

#if 0
	if (ehci_to_hcd(ehci)->self.bandwidth_isoc_reqs == 0) {
		if (ehci->amd_pll_fix == 1)
			usb_amd_quirk_pll_disable();
	}
#endif
	ft313_to_hcd(ft313)->self.bandwidth_isoc_reqs++;

	/* fill iTDs uframe by uframe */
	for (packet = 0, itd = NULL; packet < urb->number_of_packets; ) {
		if (itd == NULL) {
			/* ASSERT:  we have all necessary itds */
			// BUG_ON (list_empty (&iso_sched->td_list));

			/* ASSERT:  no itds for this endpoint in this uframe */

			itd = list_entry (iso_sched->td_list.next,
					struct ehci_itd, itd_list);
			list_move_tail (&itd->itd_list, &stream->td_list);
			itd->stream = iso_stream_get (stream);
			itd->urb = urb;
			itd_init (ft313, stream, itd);
		}

		uframe = next_uframe & 0x07;
		frame = next_uframe >> 3;

		itd_patch(ft313, itd, iso_sched, packet, uframe);

		next_uframe += stream->interval;
		next_uframe &= mod - 1;
		packet++;

		/* link completed itds into the schedule */
		if (((next_uframe >> 3) != frame)
				|| packet == urb->number_of_packets
				|| packet == iso_sched->actual_number_of_packets) {
			itd_link(ft313, frame & (ft313->periodic_size - 1), itd);
			itd = NULL;
		}

		if (packet == iso_sched->actual_number_of_packets) {
			//itd = list_entry(iso_sched->td_list.next,
			//		struct ehci_itd, itd_list);
			//list_splice_tail(&itd->itd_list, &stream->free_list);
			DEBUG_MSG("Limit reached, stop program iTD\n");
			break;
		}
	}
	stream->next_uframe = next_uframe;

	/* don't need that schedule data any more */
	iso_sched_free (stream, iso_sched);
	urb->hcpriv = NULL;

	timer_action (ft313, TIMER_IO_WATCHDOG);
	ret = enable_periodic(ft313);

	FUN_EXIT();

	return ret;
}

#define	ISO_ERRS (EHCI_ISOC_BUF_ERR | EHCI_ISOC_BABBLE | EHCI_ISOC_XACTERR)

/* Process and recycle a completed ITD.  Return true iff its urb completed,
 * and hence its completion callback probably added things to the hardware
 * schedule.
 *
 * Note that we carefully avoid recycling this descriptor until after any
 * completion callback runs, so that it won't be reused quickly.  That is,
 * assuming (a) no more than two urbs per frame on this endpoint, and also
 * (b) only this endpoint's completions submit URBs.  It seems some silicon
 * corrupts things if you reuse completed descriptors very quickly...
 */
static unsigned
itd_complete (
	struct ft313_hcd	*ft313,
	struct ehci_itd		*itd
) {
	struct urb				*urb = itd->urb;
	struct usb_iso_packet_descriptor	*desc;
	u32					t;
	unsigned				uframe;
	int					urb_index = -1;
	struct ehci_iso_stream			*stream = itd->stream;
	struct usb_device			*dev;
	unsigned				retval = false;

	FUN_ENTRY();

	// Update itd from FT313 memory
	ft313_mem_read(ft313, itd, sizeof(struct ehci_itd_hw), itd->itd_ft313);

	/* for each uframe with a packet */
	for (uframe = 0; uframe < 8; uframe++) {
		if (likely (itd->index[uframe] == -1))
			continue;
		urb_index = itd->index[uframe];
		desc = &urb->iso_frame_desc [urb_index];

		t = hc32_to_cpup(ft313, &itd->hw_transaction [uframe]);
		itd->hw_transaction [uframe] = 0;

		/* report transfer status */
		if (unlikely (t & ISO_ERRS)) {
			urb->error_count++;
			if (t & EHCI_ISOC_BUF_ERR)
				desc->status = usb_pipein (urb->pipe)
					? -ENOSR  /* hc couldn't read */
					: -ECOMM; /* hc couldn't write */
			else if (t & EHCI_ISOC_BABBLE)
				desc->status = -EOVERFLOW;
			else /* (t & EHCI_ISOC_XACTERR) */ {
				ALERT_MSG("Got Iso Transaction error!!!\n");
				desc->status = -EPROTO;
			}

			/* HC need not update length with this error */
			if (!(t & EHCI_ISOC_BABBLE)) {
				// FT313 is not compitable with EHCI on iTD handling!
				// FixMe: Out transfer not support yet!
				desc->actual_length = desc->length - EHCI_ITD_LENGTH(t);
				urb->actual_length += desc->actual_length;
			}
		} else if (likely ((t & EHCI_ISOC_ACTIVE) == 0)) {
			desc->status = 0;
			// FT313 is not compitable with EHCI on iTD handling!
			// FixMe: Out transfer not support yet!
			desc->actual_length = desc->length - EHCI_ITD_LENGTH(t);
			urb->actual_length += desc->actual_length;

			// copy data for IN transfer
			DEBUG_MSG("Copy itd payload\n");
			ft313_mem_read(ft313,
				       urb->transfer_buffer + desc->offset,
				       desc->actual_length,
				       stream->buffer_ft313 + desc->offset);
		} else {
			/* URB was too late */
			ERROR_MSG("URB was too late\n");
			desc->status = -EXDEV;
		}
	}

	/* handle completion now? */
	if (likely ((urb_index + 1) != urb->number_of_packets &&
		    (urb_index + 1) != stream->actual_number_of_packets))
		goto done;

	/* ASSERT: it's really the last itd for this urb
	list_for_each_entry (itd, &stream->td_list, itd_list)
		BUG_ON (itd->urb == urb);
	 */

	// Free urb data buffer
	DEBUG_MSG("Free buffer mem blk for urb 0x%X which is at 0x%X\n", urb, stream->buffer_ft313);
	free_mem_blk(ft313, stream->buffer_ft313);
	if (!list_empty(&stream->urb_list)) { // there is urb in queue
		struct iso_urb_queue_item *iso_urb_q_item;
		iso_urb_q_item = list_entry(stream->urb_list.next,
					    struct iso_urb_queue_item,
					    urb_list);
		stream->urb = iso_urb_q_item->urb;
		stream->buffer_ft313 = iso_urb_q_item->urb_buffer;
		stream->urb_waiting = 1;

		DEBUG_MSG("urb 0x%X is moved out of queue\n", stream->urb);
		list_del(&iso_urb_q_item->urb_list);
		kfree(iso_urb_q_item);
	} else {
		DEBUG_MSG("urb queue for stream 0x%X is empty\n", stream);
		stream->urb = NULL;
		stream->urb_waiting = 0;
		stream->buffer_ft313 = 0;
	}

	/* give urb back to the driver; completion often (re)submits */
	dev = urb->dev;
	ft313_urb_done(ft313, urb, 0);
	retval = true;
	urb = NULL;
	(void) disable_periodic(ft313);
	ft313_to_hcd(ft313)->self.bandwidth_isoc_reqs--;

//	if (ehci_to_hcd(ehci)->self.bandwidth_isoc_reqs == 0) {
//		if (ehci->amd_pll_fix == 1)
//			usb_amd_quirk_pll_enable();
//	}

	if (unlikely(list_is_singular(&stream->td_list))) {
		ft313_to_hcd(ft313)->self.bandwidth_allocated
				-= stream->bandwidth;
//		ehci_vdbg (ehci,
//			"deschedule devp %s ep%d%s-iso\n",
//			dev->devpath, stream->bEndpointAddress & 0x0f,
//			(stream->bEndpointAddress & USB_DIR_IN) ? "in" : "out");
	}
	iso_stream_put (ft313, stream);

done:
	itd->urb = NULL;
	if (ft313->clock_frame != itd->frame || itd->index[7] != -1) {
		/* OK to recycle this ITD now. */
		itd->stream = NULL;
		list_move(&itd->itd_list, &stream->free_list);
		iso_stream_put(ft313, stream);
	} else {
		/* HW might remember this ITD, so we can't recycle it yet.
		 * Move it to a safe place until a new frame starts.
		 */
		list_move(&itd->itd_list, &ft313->cached_itd_list);
		if (stream->refcount == 2) {
			/* If iso_stream_put() were called here, stream
			 * would be freed.  Instead, just prevent reuse.
			 */
			stream->ep->hcpriv = NULL;
			stream->ep = NULL;
		}
	}

	FUN_EXIT();

	return retval;
}

/*-------------------------------------------------------------------------*/

static int itd_submit (struct ft313_hcd *ft313, struct urb *urb,
	gfp_t mem_flags)
{
	int			status = -EINVAL;
	unsigned long		flags;
	struct ehci_iso_stream	*stream;
	struct timeval 		tv1, tv2;
	unsigned long 		diff;

	FUN_ENTRY();

//	DEBUG_MSG("mem_flags for isochronous transfer is 0x%X\n", mem_flags);

	/* Get iso_stream head */
	stream = iso_stream_find (ft313, urb);
	if (unlikely (stream == NULL)) {
		DEBUG_MSG("can't get iso stream\n");
		return -ENOMEM;
	}
	if (unlikely (urb->interval != stream->interval)) {
		DEBUG_MSG("can't change iso interval %d --> %d\n",
			stream->interval, urb->interval);
		goto done;
	}

#ifdef EHCI_URB_TRACE
	ehci_dbg (ehci,
		"%s %s urb %p ep%d%s len %d, %d pkts %d uframes [%p]\n",
		__func__, urb->dev->devpath, urb,
		usb_pipeendpoint (urb->pipe),
		usb_pipein (urb->pipe) ? "in" : "out",
		urb->transfer_buffer_length,
		urb->number_of_packets, urb->interval,
		stream);
#endif

	/* allocate ITDs w/o locking anything */
	status = itd_urb_transaction (stream, ft313, urb, mem_flags);
	if (unlikely (status < 0)) {
		DEBUG_MSG("can't init itds\n");
		goto done;
	}

	/* schedule ... need to lock */
	spin_lock_irqsave (&ft313->lock, flags);
	if (unlikely(!HCD_HW_ACCESSIBLE(ft313_to_hcd(ft313)))) {
		status = -ESHUTDOWN;
		goto done_not_linked;
	}
	status = usb_hcd_link_urb_to_ep(ft313_to_hcd(ft313), urb);
	if (unlikely(status))
		goto done_not_linked;
	status = iso_stream_schedule(ft313, urb, stream);
	if (likely (status == 0))
		itd_link_urb (ft313, urb, ft313->periodic_size << 3, stream);
	else
		usb_hcd_unlink_urb_from_ep(ft313_to_hcd(ft313), urb);

done_not_linked:

	do_gettimeofday(&tv1);
	spin_unlock_irqrestore (&ft313->lock, flags);
	do_gettimeofday(&tv2);

	if (tv2.tv_usec < tv1.tv_usec) {
		diff = (tv2.tv_sec - 1 - tv1.tv_sec) * 1000 + (tv2.tv_usec + 1000 - tv1.tv_usec);
	} else {
		diff = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec);
	}

	if (diff > 1000) { // More than 1 millisecond
		ERROR_MSG("Gap bigger than 1 millisecond\n");
	}

done:
	if (unlikely (status < 0))
		iso_stream_put (ft313, stream);

	FUN_EXIT();
	return status;
}

/*-------------------------------------------------------------------------*/

/*
 * "Split ISO TDs" ... used for USB 1.1 devices going through the
 * TTs in USB 2.0 hubs.  These need microframe scheduling.
 */

static inline int
sitd_sched_init(
	struct ft313_hcd	*ft313,
	struct ehci_iso_sched	*iso_sched,
	struct ehci_iso_stream	*stream,
	struct urb		*urb
)
{
	unsigned	i;
//	dma_addr_t	dma = urb->transfer_dma;

	u32		buffer_ft313;
	u32		urb_total_length = 0;
	struct ft313_mem_blk *mem_blk_ptr;
	int is_input, maxpacket;

	FUN_ENTRY();

	is_input = usb_pipein (urb->pipe);
	maxpacket = max_packet(usb_maxpacket(urb->dev, urb->pipe, !is_input));

	urb_total_length = get_iso_urb_size(urb);

	mem_blk_ptr = allocate_mem_blk(ft313, BUFFER, urb_total_length); // Allocate comm buffer
	if (mem_blk_ptr == NULL) {
		ALERT_MSG("No more memory block available \n");
		FUN_EXIT();
		return -1;
	}
	DEBUG_MSG("Got a buffer with size %d at 0x%X.\n", mem_blk_ptr->size, mem_blk_ptr->offset);

	if (mem_blk_ptr->size < urb_total_length) { // Get a smaller buffer than needed
		if (mem_blk_ptr->size < maxpacket) {
			free_mem_blk(ft313, mem_blk_ptr->offset);
			ALERT_MSG("No enough buffer to serve this urb\n");
			FUN_EXIT();
			return -2;
		}
	}

	buffer_ft313 = mem_blk_ptr->offset;
	DEBUG_MSG("Payload buffer at 0x%X with size %d\n", buffer_ft313, mem_blk_ptr->size);
	if (stream->buffer_ft313 == 0) // No urb under execution
		stream->buffer_ft313 = buffer_ft313;
	else {
		struct iso_urb_queue_item *iso_urb_q_item;
		iso_urb_q_item = kmalloc(sizeof(struct iso_urb_queue_item), GFP_ATOMIC);
		if (NULL != iso_urb_q_item) {
			iso_urb_q_item->urb = urb;
			iso_urb_q_item->urb_buffer = buffer_ft313;

			INIT_LIST_HEAD(&iso_urb_q_item->urb_list);
			list_add_tail(&iso_urb_q_item->urb_list, &stream->urb_list);

			DEBUG_MSG("urb 0x%X's buffer offset 0x%X is queued\n", urb, buffer_ft313);

		}
		else {
			ERROR_MSG("Memory allocation for iso queue failed!\n");
			FUN_EXIT();
			return -3;
		}
	}

	stream->urb = urb;

	/* how many frames are needed for these transfers */
	iso_sched->span = urb->number_of_packets * stream->interval;

	// Assume we have enough memory first!
	iso_sched->actual_number_of_packets = urb->number_of_packets;

	/* figure out per-frame sitd fields that we'll need later
	 * when we fit new sitds into the schedule.
	 */
	for (i = 0; i < urb->number_of_packets; i++) {
		struct ehci_iso_packet	*packet = &iso_sched->packet [i];
		unsigned		length;
		dma_addr_t		buf;
		u32			trans;

		length = urb->iso_frame_desc [i].length & 0x03ff;
		buf = buffer_ft313 + urb->iso_frame_desc [i].offset;

		trans = SITD_STS_ACTIVE;
		if (((i + 1) == urb->number_of_packets)
				&& !(urb->transfer_flags & URB_NO_INTERRUPT))
			trans |= SITD_IOC;
		trans |= length << 16;
		packet->transaction = cpu_to_hc32(ft313, trans);

		/* might need to cross a buffer page within a td */
		packet->bufp = buf;
		packet->buf1 = (buf + length) & ~0x0fff;
		if (packet->buf1 != (buf & ~(u64)0x0fff))
			packet->cross = 1;

		if (((i + 1) != urb->number_of_packets) && // Not last packet!
				((buf + urb->iso_frame_desc[i + 1].length) > (buffer_ft313 + mem_blk_ptr->size))) {
			DEBUG_MSG("Memory used up for this urb\n");
			iso_sched->actual_number_of_packets = i + 1;
			DEBUG_MSG("Only %d packets could be programmed\n", i + 1);
			trans |= SITD_IOC;
			packet->transaction = cpu_to_hc32(ft313, trans);
			break;
		}

		/* OUT uses multiple start-splits */
		if (stream->bEndpointAddress & USB_DIR_IN)
			continue;
		length = (length + 187) / 188;
		if (length > 1) /* BEGIN vs ALL */
			length |= 1 << 3;
		packet->buf1 |= length;
	}

	if (iso_sched->actual_number_of_packets < urb->number_of_packets)
		stream->actual_number_of_packets = iso_sched->actual_number_of_packets;
	else
		stream->actual_number_of_packets = urb->number_of_packets;

	FUN_EXIT();

	return 0;
}

static int
sitd_urb_transaction (
	struct ehci_iso_stream	*stream,
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	gfp_t			mem_flags
)
{
	struct ehci_sitd	*sitd;
	dma_addr_t		sitd_dma;
	int			i;
	struct ehci_iso_sched	*iso_sched;
	unsigned long		flags;
	u32			sitd_ft313;

	FUN_ENTRY();

	iso_sched = iso_sched_alloc (urb->number_of_packets, mem_flags);
	if (iso_sched == NULL)
		return -ENOMEM;

	if (0 > sitd_sched_init(ft313, iso_sched, stream, urb))
		return -ENOMEM;

	/* allocate/init sITDs */
	spin_lock_irqsave (&ft313->lock, flags);
	for (i = 0; i < urb->number_of_packets; i++) {

		/* NOTE:  for now, we don't try to handle wraparound cases
		 * for IN (using sitd->hw_backpointer, like a FSTN), which
		 * means we never need two sitds for full speed packets.
		 */

		/* free_list.next might be cache-hot ... but maybe
		 * the HC caches it too. avoid that issue for now.
		 */

		/* prefer previously-allocated sitds */
		if (!list_empty(&stream->free_list)) {
			sitd = list_entry (stream->free_list.prev,
					 struct ehci_sitd, sitd_list);
			list_del (&sitd->sitd_list);
			sitd_dma = sitd->sitd_dma;
			sitd_ft313 = sitd->sitd_ft313;
		} else {
			struct ft313_mem_blk	*mem_blk_ptr = NULL;

			DEBUG_MSG("Allocate siTD from FT313\n");

			mem_blk_ptr = allocate_mem_blk(ft313, SITD, 0);
			if (mem_blk_ptr == NULL) {
				ALERT_MSG("Cannot allocate siTD \n");
				iso_sched_free(stream, iso_sched);
				spin_unlock_irqrestore(&ft313->lock, flags);
				FUN_EXIT();
				return -ENOMEM;
			}
			else {
				sitd_ft313 = mem_blk_ptr->offset;
				DEBUG_MSG("Get an siTD at 0x%X from FT313\n", sitd_ft313);
			}

			spin_unlock_irqrestore (&ft313->lock, flags);
#ifndef DMA_POOL_WORKAROUND
			sitd = dma_pool_alloc (ft313->sitd_pool, mem_flags,
					&sitd_dma);
#else
			sitd = kmalloc(sizeof(struct ehci_sitd), mem_flags);
			sitd_dma = virt_to_bus(sitd);
#endif
			spin_lock_irqsave (&ft313->lock, flags);
			if (!sitd) {
				iso_sched_free(stream, iso_sched);
				spin_unlock_irqrestore(&ft313->lock, flags);
				free_mem_blk(ft313, sitd_ft313);
				FUN_EXIT();
				return -ENOMEM;
			}
		}

		memset (sitd, 0, sizeof *sitd);
		sitd->sitd_dma = sitd_dma;
		sitd->sitd_ft313 = sitd_ft313;
		list_add (&sitd->sitd_list, &iso_sched->td_list);
	}

	/* temporarily store schedule info in hcpriv */
	urb->hcpriv = iso_sched;
	urb->error_count = 0;

	spin_unlock_irqrestore (&ft313->lock, flags);
	return 0;
}

/*-------------------------------------------------------------------------*/

static inline void
sitd_patch(
	struct ft313_hcd	*ft313,
	struct ehci_iso_stream	*stream,
	struct ehci_sitd	*sitd,
	struct ehci_iso_sched	*iso_sched,
	unsigned		index
)
{
	struct ehci_iso_packet	*uf = &iso_sched->packet [index];
	u64			bufp = uf->bufp;

	sitd->hw_next = EHCI_LIST_END(ft313);
	sitd->hw_fullspeed_ep = stream->address;
	sitd->hw_uframe = stream->splits;
	sitd->hw_results = uf->transaction;
	sitd->hw_backpointer = EHCI_LIST_END(ft313);

	bufp = uf->bufp;
	sitd->hw_buf[0] = cpu_to_hc32(ft313, bufp);
	sitd->hw_buf_hi[0] = cpu_to_hc32(ft313, bufp >> 32);

	sitd->hw_buf[1] = cpu_to_hc32(ft313, uf->buf1);
	if (uf->cross)
		bufp += 4096;
	sitd->hw_buf_hi[1] = cpu_to_hc32(ft313, bufp >> 32);
	sitd->index = index;

	ft313_mem_write(ft313, sitd, sizeof(struct ehci_sitd_hw), sitd->sitd_ft313);
}

static inline void
sitd_link (struct ft313_hcd *ft313, unsigned frame, struct ehci_sitd *sitd)
{
	/* note: sitd ordering could matter (CSPLIT then SSPLIT) */
	sitd->sitd_next = ft313->pshadow [frame];
	sitd->hw_next = ft313->periodic [frame];
	ft313->pshadow [frame].sitd = sitd;
	sitd->frame = frame;
	wmb ();
	ft313->periodic[frame] = cpu_to_hc32(ft313, sitd->sitd_ft313 | Q_TYPE_SITD);
	ft313_mem_write(ft313,
			&ft313->periodic[frame],
			sizeof(ft313->periodic[frame]),
			0 + frame * sizeof(ft313->periodic[frame]));
}

/* fit urb's sitds into the selected schedule slot; activate as needed */
static int
sitd_link_urb (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	unsigned		mod,
	struct ehci_iso_stream	*stream
)
{
	int			packet;
	unsigned		next_uframe;
	struct ehci_iso_sched	*sched = urb->hcpriv;
	struct ehci_sitd	*sitd;

	next_uframe = stream->next_uframe;

	if (list_empty(&stream->td_list)) {
		/* usbfs ignores TT bandwidth */
		ft313_to_hcd(ft313)->self.bandwidth_allocated
				+= stream->bandwidth;
		DEBUG_MSG (
			"sched devp %s ep%d%s-iso [%d] %dms/%04x\n",
			urb->dev->devpath, stream->bEndpointAddress & 0x0f,
			(stream->bEndpointAddress & USB_DIR_IN) ? "in" : "out",
			(next_uframe >> 3) & (ft313->periodic_size - 1),
			stream->interval, hc32_to_cpu(ft313, stream->splits));
	}
#if 0
	if (ft313_to_hcd(ft313)->self.bandwidth_isoc_reqs == 0) {
		if (ehci->amd_pll_fix == 1)
			usb_amd_quirk_pll_disable();
	}
#endif
	ft313_to_hcd(ft313)->self.bandwidth_isoc_reqs++;

	/* fill sITDs frame by frame */
	for (packet = 0, sitd = NULL;
			packet < urb->number_of_packets;
			packet++) {

		/* ASSERT:  we have all necessary sitds */
		BUG_ON (list_empty (&sched->td_list));

		/* ASSERT:  no itds for this endpoint in this frame */

		sitd = list_entry (sched->td_list.next,
				struct ehci_sitd, sitd_list);
		list_move_tail (&sitd->sitd_list, &stream->td_list);
		sitd->stream = iso_stream_get (stream);
		sitd->urb = urb;

		sitd_patch(ft313, stream, sitd, sched, packet);
		sitd_link(ft313, (next_uframe >> 3) & (ft313->periodic_size - 1),
				sitd);

		next_uframe += stream->interval << 3;

		if (packet + 1 == stream->actual_number_of_packets) {
			DEBUG_MSG("Reach limit, stop\n");
			break;
		}

	}
	stream->next_uframe = next_uframe & (mod - 1);

	/* don't need that schedule data any more */
	iso_sched_free (stream, sched);
	urb->hcpriv = NULL;

	timer_action (ft313, TIMER_IO_WATCHDOG);
	return enable_periodic(ft313);
}

/*-------------------------------------------------------------------------*/

#define	SITD_ERRS (SITD_STS_ERR | SITD_STS_DBE | SITD_STS_BABBLE \
				| SITD_STS_XACT | SITD_STS_MMF)

/* Process and recycle a completed SITD.  Return true iff its urb completed,
 * and hence its completion callback probably added things to the hardware
 * schedule.
 *
 * Note that we carefully avoid recycling this descriptor until after any
 * completion callback runs, so that it won't be reused quickly.  That is,
 * assuming (a) no more than two urbs per frame on this endpoint, and also
 * (b) only this endpoint's completions submit URBs.  It seems some silicon
 * corrupts things if you reuse completed descriptors very quickly...
 */
static unsigned
sitd_complete (
	struct ft313_hcd	*ft313,
	struct ehci_sitd	*sitd
) {
	struct urb				*urb = sitd->urb;
	struct usb_iso_packet_descriptor	*desc;
	u32					t;
	int					urb_index = -1;
	struct ehci_iso_stream			*stream = sitd->stream;
	struct usb_device			*dev;
	unsigned				retval = false;

	FUN_ENTRY();

	urb_index = sitd->index;
	desc = &urb->iso_frame_desc [urb_index];
	t = hc32_to_cpup(ft313, &sitd->hw_results);

	/* report transfer status */
	if (t & SITD_ERRS) {
		ERROR_MSG("Error occur in siTD handling\n");
		urb->error_count++;
		if (t & SITD_STS_DBE)
			desc->status = usb_pipein (urb->pipe)
				? -ENOSR  /* hc couldn't read */
				: -ECOMM; /* hc couldn't write */
		else if (t & SITD_STS_BABBLE)
			desc->status = -EOVERFLOW;
		else /* XACT, MMF, etc */
			desc->status = -EPROTO;
	} else {
		desc->status = 0;
		desc->actual_length = desc->length - SITD_LENGTH(t);
		urb->actual_length += desc->actual_length;

		// copy data for IN transfer
		ft313_mem_read(ft313,
			       urb->transfer_buffer + desc->offset,
			       desc->actual_length,
			       stream->buffer_ft313 + desc->offset);
	}

	/* handle completion now? */
	if ((urb_index + 1) != urb->number_of_packets &&
	    (urb_index + 1) != stream->actual_number_of_packets)
		goto done;

	/* ASSERT: it's really the last sitd for this urb
	list_for_each_entry (sitd, &stream->td_list, sitd_list)
		BUG_ON (sitd->urb == urb);
	 */

	// Free urb data buffer
	DEBUG_MSG("Free buffer mem blk for urb 0x%X which is at 0x%X\n", urb, stream->buffer_ft313);
	free_mem_blk(ft313, stream->buffer_ft313);
	if (!list_empty(&stream->urb_list)) { // there is urb in queue
		struct iso_urb_queue_item *iso_urb_q_item;
		iso_urb_q_item = list_entry(stream->urb_list.next,
					    struct iso_urb_queue_item,
					    urb_list);
		stream->urb = iso_urb_q_item->urb;
		stream->buffer_ft313 = iso_urb_q_item->urb_buffer;
		stream->urb_waiting = 1;

		DEBUG_MSG("urb 0x%X is moved out of queue\n", stream->urb);
		list_del(&iso_urb_q_item->urb_list);
		kfree(iso_urb_q_item);
	} else {
		DEBUG_MSG("urb queue for stream 0x%X is empty\n", stream);
		stream->urb = NULL;
		stream->urb_waiting = 0;
		stream->buffer_ft313 = 0;
	}

	/* give urb back to the driver; completion often (re)submits */
	dev = urb->dev;
	ft313_urb_done(ft313, urb, 0);
	retval = true;
	urb = NULL;
	(void) disable_periodic(ft313);
	ft313_to_hcd(ft313)->self.bandwidth_isoc_reqs--;

	if (list_is_singular(&stream->td_list)) {
		ft313_to_hcd(ft313)->self.bandwidth_allocated
				-= stream->bandwidth;
		DEBUG_MSG(
			"deschedule devp %s ep%d%s-iso\n",
			dev->devpath, stream->bEndpointAddress & 0x0f,
			(stream->bEndpointAddress & USB_DIR_IN) ? "in" : "out");
	}
	iso_stream_put (ft313, stream);

done:
	sitd->urb = NULL;
	if (ft313->clock_frame != sitd->frame) {
		/* OK to recycle this SITD now. */
		sitd->stream = NULL;
		list_move(&sitd->sitd_list, &stream->free_list);
		iso_stream_put(ft313, stream);
	} else {
		/* HW might remember this SITD, so we can't recycle it yet.
		 * Move it to a safe place until a new frame starts.
		 */
		list_move(&sitd->sitd_list, &ft313->cached_sitd_list);
		if (stream->refcount == 2) {
			/* If iso_stream_put() were called here, stream
			 * would be freed.  Instead, just prevent reuse.
			 */
			stream->ep->hcpriv = NULL;
			stream->ep = NULL;
		}
	}

	FUN_EXIT();

	return retval;
}


static int sitd_submit (struct ft313_hcd *ft313, struct urb *urb,
	gfp_t mem_flags)
{
	int			status = -EINVAL;
	unsigned long		flags;
	struct ehci_iso_stream	*stream;

	/* Get iso_stream head */
	stream = iso_stream_find (ft313, urb);
	if (stream == NULL) {
		DEBUG_MSG("can't get iso stream\n");
		return -ENOMEM;
	}
	if (urb->interval != stream->interval) {
		DEBUG_MSG ("can't change iso interval %d --> %d\n",
			stream->interval, urb->interval);
		goto done;
	}

#ifdef EHCI_URB_TRACE
	ehci_dbg (ehci,
		"submit %p dev%s ep%d%s-iso len %d\n",
		urb, urb->dev->devpath,
		usb_pipeendpoint (urb->pipe),
		usb_pipein (urb->pipe) ? "in" : "out",
		urb->transfer_buffer_length);
#endif

	/* allocate SITDs */
	status = sitd_urb_transaction (stream, ft313, urb, mem_flags);
	if (status < 0) {
		DEBUG_MSG ("can't init sitds\n");
		goto done;
	}

	/* schedule ... need to lock */
	spin_lock_irqsave (&ft313->lock, flags);
	if (unlikely(!HCD_HW_ACCESSIBLE(ft313_to_hcd(ft313)))) {
		status = -ESHUTDOWN;
		goto done_not_linked;
	}
	status = usb_hcd_link_urb_to_ep(ft313_to_hcd(ft313), urb);
	if (unlikely(status))
		goto done_not_linked;
	status = iso_stream_schedule(ft313, urb, stream);
	if (status == 0)
		sitd_link_urb (ft313, urb, ft313->periodic_size << 3, stream);
	else
		usb_hcd_unlink_urb_from_ep(ft313_to_hcd(ft313), urb);
done_not_linked:
	spin_unlock_irqrestore (&ft313->lock, flags);

done:
	if (status < 0)
		iso_stream_put (ft313, stream);
	return status;
}

/*-------------------------------------------------------------------------*/

static void free_cached_lists(struct ft313_hcd *ft313)
{
	struct ehci_itd *itd, *n;
	struct ehci_sitd *sitd, *sn;

	list_for_each_entry_safe(itd, n, &ft313->cached_itd_list, itd_list) {
		struct ehci_iso_stream	*stream = itd->stream;
		itd->stream = NULL;
		list_move(&itd->itd_list, &stream->free_list);
		iso_stream_put(ft313, stream);
	}

	list_for_each_entry_safe(sitd, sn, &ft313->cached_sitd_list, sitd_list) {
		struct ehci_iso_stream	*stream = sitd->stream;
		sitd->stream = NULL;
		list_move(&sitd->sitd_list, &stream->free_list);
		iso_stream_put(ft313, stream);
	}
}


#endif // Iso related end

/*-------------------------------------------------------------------------*/

static void
scan_periodic (struct ft313_hcd *ft313)
{
	unsigned	now_uframe, frame, clock, clock_frame, mod;
	unsigned	modified;

	FUN_ENTRY();

	mod = ft313->periodic_size << 3;

	/*
	 * When running, scan from last scan point up to "now"
	 * else clean up by scanning everything that's left.
	 * Touches as few pages as possible:  cache-friendly.
	 */
	now_uframe = ft313->next_uframe;
	if (HC_IS_RUNNING(ft313_to_hcd(ft313)->state)) {
		clock = ft313_reg_read32(ft313, &ft313->regs->frame_index);
		clock_frame = (clock >> 3) & (ft313->periodic_size - 1);
	} else  {
		clock = now_uframe + mod - 1;
		clock_frame = -1;
	}

	if (ft313->clock_frame != clock_frame) {
		free_cached_lists(ft313);
		ft313->clock_frame = clock_frame;
	}

	clock &= mod - 1;
	clock_frame = clock >> 3;
	++ft313->periodic_stamp;

	for (;;) {
		union ehci_shadow	q, *q_p;
		__hc32			type, *hw_p;
		u32			hw_p_ft313;
		unsigned		incomplete = false;

		frame = now_uframe >> 3;

restart:
		/* scan each element in frame's queue for completions */
//		DEBUG_MSG("Process frame No. %d\n", frame);
		q_p = &ft313->pshadow [frame];
		hw_p = &ft313->periodic [frame];
		hw_p_ft313 = 0 + sizeof(__hc32) * frame;
		q.ptr = q_p->ptr;
		type = Q_NEXT_TYPE(ft313, *hw_p);
		modified = 0;

		while (q.ptr != NULL) {
			unsigned		uf;
			union ehci_shadow	temp;
			int			live;
			struct ehci_iso_stream *stream = NULL;

			live = HC_IS_RUNNING (ft313_to_hcd(ft313)->state);
			switch (hc32_to_cpu(ft313, type)) {
				case Q_TYPE_QH: {
					/* handle any completions */
					DEBUG_MSG("Process qH\n");
					temp.qh = qh_get (q.qh);
					type = Q_NEXT_TYPE(ft313, q.qh->hw->hw_next);
					q = q.qh->qh_next;
					if (temp.qh->stamp != ft313->periodic_stamp) {
						modified = qh_completions(ft313, temp.qh);
						if (!modified)
							temp.qh->stamp = ft313->periodic_stamp;
#if 0 // Disable urb queue for interrupt transfer
						if (0 != in_interrupt()) { //Assume called from ft313_work by ft313_irq()
							if (temp.qh->urb_pending == 1) {
								if (temp.qh->urb != NULL) {
									spin_unlock(&ft313->lock);
									if (0 > ft313_urb_enqueue_next(ft313, temp.qh->urb, GFP_ATOMIC)) {
										ERROR_MSG("Program next segment failed!\n");
										temp.qh->urb_pending = 0;
										spin_lock(&ft313->lock); // ft313_urb_done will release lock first!
										ft313_urb_done(ft313, temp.qh->urb, -EPROTO); // report protocol error!
									} else {
										//qh->urb = NULL;
										//qh->urb_pending = 0;
										spin_lock(&ft313->lock);
									}
								}
							}
						}

						if ((temp.qh->urb_pending != 1) ||
						    (0 == in_interrupt()))
#endif
						{ // No urb queue processing
							if (unlikely(list_empty(&temp.qh->qtd_list) ||
									temp.qh->needs_rescan))
								intr_deschedule(ft313, temp.qh);
						}
					}
					qh_put (temp.qh);
					break;
				}
				case Q_TYPE_FSTN: {
					/* for "save place" FSTNs, look at QH entries
					 * in the previous frame for completions.
					 */
					ERROR_MSG("Wrong Execution, FSTN not supported\n");
					if (q.fstn->hw_prev != EHCI_LIST_END(ft313)) {
						dbg ("ignoring completions from FSTNs");
					}
					type = Q_NEXT_TYPE(ft313, q.fstn->hw_next);
					q = q.fstn->fstn_next;
					break;
				}
				case Q_TYPE_ITD: {
					DEBUG_MSG("Process iTD when frame is %d and clock_frame is %d\n", frame, clock_frame);
					/* If this ITD is still active, leave it for
					 * later processing ... check the next entry.
					 * No need to check for activity unless the
					 * frame is current.
					 */
					if (frame == clock_frame && live) {
						DEBUG_MSG("Update from FT313 memory\n");
						ft313_mem_read(ft313, q.itd, sizeof(struct ehci_itd_hw), q.itd->itd_ft313);
						rmb();
						for (uf = 0; uf < 8; uf++) {
							if (q.itd->hw_transaction[uf] &
									ITD_ACTIVE(ft313)) {
								DEBUG_MSG("uFrame %d is still active\n", uf);
								break;
							}
						}
						if (uf < 8) {
							incomplete = true;
							q_p = &q.itd->itd_next;
							hw_p = &q.itd->hw_next;
							hw_p_ft313 = q.itd->itd_ft313 + offsetof(struct ehci_itd_hw, hw_next);
							DEBUG_MSG("hw next offset is 0x%X\n", hw_p_ft313);
							type = Q_NEXT_TYPE(ft313,
									   q.itd->hw_next);
							q = *q_p;
							break;
						}
					}

					/* Take finished ITDs out of the schedule
					 * and process them:  recycle, maybe report
					 * URB completion.  HC won't cache the
					 * pointer for much longer, if at all.
					 */
					*q_p = q.itd->itd_next;
//					if (!ft313->use_dummy_qh ||
//							q.itd->hw_next != EHCI_LIST_END(ft313))
						*hw_p = q.itd->hw_next;
						DEBUG_MSG("Update itd 0x%X's hw_next field\n", q.itd->itd_ft313);
						ft313_mem_write(ft313, hw_p, sizeof(__hc32), hw_p_ft313);
//					else
//						*hw_p = ft313->dummy->qh_dma;
					type = Q_NEXT_TYPE(ft313, q.itd->hw_next);
					wmb();
					stream = q.itd->stream; //Save stream info.
					modified = itd_complete (ft313, q.itd);
					q = *q_p;
					break;
				}
				case Q_TYPE_SITD: {
					ERROR_MSG("Process siTD \n");
					/* If this SITD is still active, leave it for
					 * later processing ... check the next entry.
					 * No need to check for activity unless the
					 * frame is current.
					 */
					DEBUG_MSG("Update sitd from FT313H\n");
					ft313_mem_read(ft313, q.sitd, sizeof(struct ehci_sitd_hw), q.sitd->sitd_ft313);
					if (((frame == clock_frame) ||
							(((frame + 1) & (ft313->periodic_size - 1))
							 == clock_frame))
							&& live
							&& (q.sitd->hw_results &
							    SITD_ACTIVE(ft313))) {

						incomplete = true;
						q_p = &q.sitd->sitd_next;
						hw_p = &q.sitd->hw_next;
						hw_p_ft313 = q.sitd->sitd_ft313 + offsetof(struct ehci_sitd_hw, hw_next);
						type = Q_NEXT_TYPE(ft313,
								   q.sitd->hw_next);
						q = *q_p;
						break;
					}

					/* Take finished SITDs out of the schedule
					 * and process them:  recycle, maybe report
					 * URB completion.
					 */
					*q_p = q.sitd->sitd_next;

					// FixMe: revisit here, there is an "if" clause in original code
					*hw_p = q.sitd->hw_next;
					DEBUG_MSG("Update sitd 0x%X's hw_next \n", q.sitd->sitd_ft313);
					ft313_mem_write(ft313, hw_p, sizeof(__hc32), hw_p_ft313);

					type = Q_NEXT_TYPE(ft313, q.sitd->hw_next);
					wmb();
					stream = q.sitd->stream; //Save stream info.
					modified = sitd_complete (ft313, q.sitd);
					q = *q_p;
					break;
				}
				default: {
					ERROR_MSG("corrupt type %d frame %d shadow %p",
					     type, frame, q.ptr);
					// BUG ();
					q.ptr = NULL;
				}
			}

			/* assume completion callbacks modify the queue */
			if (unlikely (modified)) {
				// Check urb queue here as one urb is completed
				if (stream != NULL) {
					if (stream->urb_waiting == 1) {
						DEBUG_MSG("There is urb 0x%X from queue, submit it\n", stream->urb);
						spin_unlock(&ft313->lock); // Unlock so that next urb submit can continue
						if (0 > ft313_urb_enqueue_next(ft313, stream->urb, GFP_ATOMIC)) {
							ALERT_MSG("Iso urb submission from queue failed\n");
							spin_lock(&ft313->lock); // ft313_urb_done will unlock, then lock
							ft313_urb_done(ft313, stream->urb, -EPROTO);
						} else {
							stream->urb_waiting = 0;
							// Relock the spinlock
							spin_lock(&ft313->lock);
						}
					}
				}

				stream = NULL;

				if (likely(ft313->periodic_sched > 0))
					goto restart;
				/* short-circuit this scan */
				now_uframe = clock;
				break;
			}
		}

		/* If we can tell we caught up to the hardware, stop now.
		 * We can't advance our scan without collecting the ISO
		 * transfers that are still pending in this frame.
		 */
		if (incomplete && HC_IS_RUNNING(ft313_to_hcd(ft313)->state)) {
			ft313->next_uframe = now_uframe;
			break;
		}

		// FIXME:  this assumes we won't get lapped when
		// latencies climb; that should be rare, but...
		// detect it, and just go all the way around.
		// FLR might help detect this case, so long as latencies
		// don't exceed periodic_size msec (default 1.024 sec).

		// FIXME:  likewise assumes HC doesn't halt mid-scan

		if (now_uframe == clock) {
			unsigned	now;

			if (!HC_IS_RUNNING (ft313_to_hcd(ft313)->state)
					|| ft313->periodic_sched == 0)
				break;
			ft313->next_uframe = now_uframe;
			now = ft313_reg_read32(ft313, &ft313->regs->frame_index) &
			      (mod - 1);
			if (now_uframe == now)
				break;

			/* rescan the rest of this frame, then ... */
			DEBUG_MSG("Rescan the rest of frame\n");
			clock = now;
			clock_frame = clock >> 3;
			if (ft313->clock_frame != clock_frame) {
				free_cached_lists(ft313);
				ft313->clock_frame = clock_frame;
				++ft313->periodic_stamp;
			}
		} else {
			DEBUG_MSG("now_uframe != clock\n");
			now_uframe++;
			now_uframe &= mod - 1;
		}
	}

	FUN_EXIT();
}


