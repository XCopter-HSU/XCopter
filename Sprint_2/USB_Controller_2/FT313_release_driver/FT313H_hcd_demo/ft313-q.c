/*
 * FT313 HCD qTD, qHead, iTD, siTD management.
 *
 * Copyright (C) 2011 Chang Yang <chang.yang@ftdichip.com>
 *
 * This code is *strongly* based on EHCI-HCD code by David Brownell since
 * the chip is a quasi-EHCI compatible.
 *
 * Licensed under GPL version 2 only.
 */
/* this file is part of ft313-hcd.c */

/* fill a qtd, returning how much of the buffer we were able to queue up */

static int
qtd_fill(struct ft313_hcd *ft313, struct ehci_qtd *qtd, void* buf,
		  size_t len, int token, int maxpacket)
{
	int	i, count;
	u32	addr;
	size_t	actual_len;
	struct ft313_mem_blk *mem_blk_ptr;

	FUN_ENTRY();

	DEBUG_MSG("qTD buffer ptr is at 0x%X with token as 0x%08X\n", buf, token);

	if (len != 0) {
		DEBUG_MSG("Try to allocate %d bytes buffer.\n", len);
		mem_blk_ptr = allocate_mem_blk(ft313, BUFFER, len); // Allocate comm buffer
		if (mem_blk_ptr == NULL) {
			printk("No more memory block available \n");
			return -1;
		}
		DEBUG_MSG("Got a buffer with size %d at 0x%X.\n", mem_blk_ptr->size, mem_blk_ptr->offset);

		actual_len = min(len, mem_blk_ptr->size);

		qtd->buffer_ft313 = mem_blk_ptr->offset;
		addr = qtd->buffer_ft313;
		qtd->hw_buf[0] = addr;

		if (mem_blk_ptr->size < len) { // Get a smaller buffer than needed
			// Check whether buffer got is too small or not
			if (mem_blk_ptr->size < maxpacket) {
				free_mem_blk(ft313, mem_blk_ptr->offset);
				ERROR_MSG("Cannot allocate minimal comm buffer for this urb, only %d bytes buffer found", mem_blk_ptr->size);
				return -1;
			}

			if (0 != (actual_len % maxpacket)) {
				// Adjust to be multiply of max packet size
				actual_len = (actual_len / maxpacket) * maxpacket;
			}
		}
	} else { // When no memory allocation is needed, set all as zero
		actual_len = 0;
		addr = 0;
		qtd->hw_buf[0] = 0;
	}

	if ((0 != buf) && // 0 means qtd for status phase of control tranfer
	    (0 != actual_len) &&
	    (QTD_PID(token) != 1)) { // Not IN transfer
		if ((unsigned int)buf <= 0x10000) {
			ERROR_MSG("buf 0x%X pointer is not valid \n", buf);
			return -1;
		}
		ft313_mem_write(ft313, buf, actual_len, mem_blk_ptr->offset); // Write payload
	}

//	qtd->hw_buf_hi[0] = cpu_to_hc32(ehci, (u32)(addr >> 32));
	count = 0x1000 - (addr & 0x0fff);	/* rest of that page */
	if (likely (actual_len < count))		/* ... iff needed */
		count = actual_len;
	else {
		addr +=  0x1000;
		addr &= ~0x0fff;

		/* per-qtd limit: from 16K to 20K (best alignment) */
		for (i = 1; count < actual_len && i < 5; i++) {
//			addr = buf;
			qtd->hw_buf[i] = cpu_to_hc32(ft313, (u32)addr);
//			qtd->hw_buf_hi[i] = cpu_to_hc32(ehci,
//					(u32)(addr >> 32));
			addr += 0x1000;
			if ((count + 0x1000) < actual_len)
				count += 0x1000;
			else
				count = actual_len;
		}

		/* short packets may only terminate transfers */
		if (count != actual_len)
			count -= (count % maxpacket);
	}
	qtd->hw_token = cpu_to_hc32(ft313, (count << 16) | token);
	qtd->length = count;

	DEBUG_MSG("qTD length is %d\n", count);

	FUN_EXIT();

	return count;
}

/*-------------------------------------------------------------------------*/

static inline void
qh_update (struct ft313_hcd *ft313, struct ehci_qh *qh, struct ehci_qtd *qtd)
{
	struct ehci_qh_hw *hw = qh->hw;

	DEBUG_MSG("Enter++\n");

	/* writes to an active overlay are unsafe */
	BUG_ON(qh->qh_state != QH_STATE_IDLE);

	// Do not update from memory as content just updated by caller
	// and caller is solo!

	hw->hw_qtd_next = QTD_NEXT(ft313, qtd->qtd_ft313);
	ft313_mem_write(ft313,
			&(hw->hw_qtd_next),
			sizeof(hw->hw_qtd_next),
			qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_qtd_next));
	DEBUG_MSG("Updated Next qTD Pointer for qH 0x%X\n", qh->qh_ft313);

	hw->hw_alt_next = EHCI_LIST_END(ft313);
	ft313_mem_write(ft313,
			&(hw->hw_alt_next),
			sizeof(hw->hw_alt_next),
			qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_alt_next));
	DEBUG_MSG("Updated Alternate Next qTD Pointer for qH 0x%X\n", qh->qh_ft313);

	/* Except for control endpoints, we make hardware maintain data
	 * toggle (like OHCI) ... here (re)initialize the toggle in the QH,
	 * and set the pseudo-toggle in udev. Only usb_clear_halt() will
	 * ever clear it.
	 */
	if (!(hw->hw_info1 & cpu_to_hc32(ft313, 1 << 14))) {
		unsigned	is_out, epnum;

		is_out = qh->is_out;
		epnum = (hc32_to_cpup(ft313, &hw->hw_info1) >> 8) & 0x0f;
		if (unlikely (!usb_gettoggle (qh->dev, epnum, is_out))) {
			hw->hw_token &= ~cpu_to_hc32(ft313, QTD_TOGGLE);
			DEBUG_MSG("Clear DT toggle bit for qH 0x%X\n", qh->ft313);
			ft313_mem_write(ft313,
					&(hw->hw_token),
					sizeof(hw->hw_token),
					qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_token));
			DEBUG_MSG("Updated Token DWord for qH 0x%X\n", qh->qh_ft313);
			usb_settoggle (qh->dev, epnum, is_out, 1);
		}
	}

	/* HC must see latest qtd and qh data before we clear ACTIVE+HALT */
	wmb ();
	hw->hw_token &= cpu_to_hc32(ft313, QTD_TOGGLE | QTD_STS_PING);
	ft313_mem_write(ft313,
			&(hw->hw_token),
			sizeof(hw->hw_token),
			qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_token));
	DEBUG_MSG("Updated Token DWord for qH 0x%X\n", qh->qh_ft313);

	DEBUG_MSG("Exit--\n");
}

/* if it weren't for a common silicon quirk (writing the dummy into the qh
 * overlay, so qh->hw_token wrongly becomes inactive/halted), only fault
 * recovery (including urb dequeue) would need software changes to a QH...
 */
static void
qh_refresh (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	FUN_ENTRY();

	struct ehci_qtd *qtd;

	if (list_empty (&qh->qtd_list)) {
		qtd = qh->dummy;
		DEBUG_MSG("qH 0x%X has an empty qTD list\n", qh->qh_ft313);
	}
	else {
		qtd = list_entry (qh->qtd_list.next,
				struct ehci_qtd, qtd_list);
		/* first qtd may already be partially processed */
		ft313_mem_read(ft313, qtd, sizeof(struct ehci_qtd_hw), qtd->qtd_ft313);
		ft313_mem_read(ft313, qh->hw, sizeof(struct ehci_qh_hw), qh->qh_ft313);
		if (cpu_to_hc32(ft313, qtd->qtd_ft313) == qh->hw->hw_current) {
			DEBUG_MSG("qTD 0x%X is the same as qH 0x%X's current qTD Pointer\n",
				  qtd->qtd_ft313, qh->qh_ft313);
			DEBUG_MSG("No qh_update\n");
			qtd = NULL;
		}
	}

	if (qtd)
		qh_update (ft313, qh, qtd);

	FUN_EXIT();
}

static void ft313_clear_tt_buffer_complete(struct usb_hcd *hcd,
		struct usb_host_endpoint *ep)
{
	struct ft313_hcd	*ft313 = hcd_to_ft313(hcd);
	struct ehci_qh		*qh = ep->hcpriv;
	unsigned long		flags;

	spin_lock_irqsave(&ft313->lock, flags);
	qh->clearing_tt = 0;
	if (qh->qh_state == QH_STATE_IDLE && !list_empty(&qh->qtd_list)
			&& HC_IS_RUNNING(hcd->state))
		qh_link_async(ft313, qh);
	spin_unlock_irqrestore(&ft313->lock, flags);
}

static void ft313_clear_tt_buffer(struct ft313_hcd *ft313, struct ehci_qh *qh,
				 struct urb *urb, u32 token)
{
	FUN_ENTRY();
	/* If an async split transaction gets an error or is unlinked,
	 * the TT buffer may be left in an indeterminate state.  We
	 * have to clear the TT buffer.
	 *
	 * Note: this routine is never called for Isochronous transfers.
	 */
	if (urb->dev->tt && !usb_pipeint(urb->pipe) && !qh->clearing_tt) {
#ifdef DEBUG
		struct usb_device *tt = urb->dev->tt->hub;
		dev_dbg(&tt->dev,
			"clear tt buffer port %d, a%d ep%d t%08x\n",
			urb->dev->ttport, urb->dev->devnum,
			usb_pipeendpoint(urb->pipe), token);
#endif /* DEBUG */
		if (!ehci_is_TDI(ft313)
				|| urb->dev->tt->hub !=
				ft313_to_hcd(ft313)->self.root_hub) {
			if (usb_hub_clear_tt_buffer(urb) == 0)
				qh->clearing_tt = 1;
		} else {

			/* REVISIT ARC-derived cores don't clear the root
			 * hub TT buffer in this way...
			 */
		}
	}
	FUN_EXIT();
}

static int qtd_copy_status (
	struct ft313_hcd *ft313,
	struct urb *urb,
	size_t length,
	u32 token,
	u32 qtd_buffer_offset
)
{
	int	status = -EINPROGRESS;
	u32	actual_rx_data_length = 0;
	void	*buf = NULL;

	DEBUG_MSG("Enter++ with token as 0x%X\n", token);

	/* count IN/OUT bytes, not SETUP (even short packets) */
	if (likely (QTD_PID (token) != 2)) {
		// for IN tranfer, need to copy data here!
		if (QTD_PID(token) == 1) {
			actual_rx_data_length = length - QTD_LENGTH(token);
			if (0 != actual_rx_data_length) {// 0 means qTD for status phase for ctrl without data phase
				DEBUG_MSG("Receive %d bytes for urb 0x%X\n", actual_rx_data_length, urb);

				if (urb->transfer_buffer == NULL) {
					ERROR_MSG("Only DMA address is provided\n");
					buf = phys_to_virt(urb->transfer_dma) + urb->actual_length;
					ERROR_MSG("buffer ptr used for copy data IN will be 0x%X\n", buf);
				}
				else
					buf = urb->transfer_buffer + urb->actual_length;

				ft313_mem_read(ft313,
					       buf,
					       actual_rx_data_length,
					       qtd_buffer_offset);
			}
		}
		urb->actual_length += (length - QTD_LENGTH (token));
		DEBUG_MSG("urb 0x%X actual_length field become %d\n", urb, urb->actual_length);
	}

	/* don't modify error codes */
	if (unlikely(urb->unlinked)) {
		DEBUG_MSG("Exit-- due to urb unlinked with status is %d\n", status);
		return status;
	}

	/* force cleanup after short read; not always an error */
	if (unlikely (IS_SHORT_READ (token)))
		status = -EREMOTEIO;

	/* serious "can't proceed" faults reported by the hardware */
	if (token & QTD_STS_HALT) {
		if (token & QTD_STS_BABBLE) {
			/* FIXME "must" disable babbling device's port too */
			ALERT_MSG("\n\n\n FT313 got USB babbing error!!! \n\n\n");
			status = -EOVERFLOW;
		/* CERR nonzero + halt --> stall */
		} else if (QTD_CERR(token)) {
			DEBUG_MSG("urb 0x%X is stalled\n", urb);
			status = -EPIPE;

		/* In theory, more than one of the following bits can be set
		 * since they are sticky and the transaction is retried.
		 * Which to test first is rather arbitrary.
		 */
		} else if (token & QTD_STS_MMF) {
			/* fs/ls interrupt xfer missed the complete-split */
			status = -EPROTO;
		} else if (token & QTD_STS_DBE) {
			status = (QTD_PID (token) == 1) /* IN ? */
				? -ENOSR  /* hc couldn't read data */
				: -ECOMM; /* hc couldn't write data */
		} else if (token & QTD_STS_XACT) {
			/* timeout, bad CRC, wrong PID, etc */
			ft313_dbg(ft313, "devpath %s ep%d%s 3strikes\n",
				urb->dev->devpath,
				usb_pipeendpoint(urb->pipe),
				usb_pipein(urb->pipe) ? "in" : "out");
			status = -EPROTO;
		} else {	/* unknown */
			status = -EPROTO;
		}
/*
		ehci_vdbg (ehci,
			"dev%d ep%d%s qtd token %08x --> status %d\n",
			usb_pipedevice (urb->pipe),
			usb_pipeendpoint (urb->pipe),
			usb_pipein (urb->pipe) ? "in" : "out",
			token, status); */
	}

	DEBUG_MSG("Exit-- with status as %d\n", status);
	return status;
}

static void
ft313_urb_done(struct ft313_hcd *ft313, struct urb *urb, int status)
__releases(ehci->lock)
__acquires(ehci->lock)
{
	DEBUG_MSG("Enter++\n");
	if (likely (urb->hcpriv != NULL)) {
		struct ehci_qh	*qh = (struct ehci_qh *) urb->hcpriv;

		/* S-mask in a QH means it's an interrupt urb */
		// FixMe: Is hw_info2 need update from FT313?
		if ((qh->hw->hw_info2 & cpu_to_hc32(ft313, QH_SMASK)) != 0) {

			/* ... update hc-wide periodic stats (for usbfs) */
			ft313_to_hcd(ft313)->self.bandwidth_int_reqs--;
		}
		qh_put (qh);
	}

	if (unlikely(urb->unlinked)) {
		COUNT(ft313->stats.unlink);
	} else {
		/* report non-error and short read status as zero */
		if (status == -EINPROGRESS || status == -EREMOTEIO)
			status = 0;
		COUNT(ft313->stats.complete);
	}

#ifdef EHCI_URB_TRACE
	ehci_dbg (ehci,
		"%s %s urb %p ep%d%s status %d len %d/%d\n",
		__func__, urb->dev->devpath, urb,
		usb_pipeendpoint (urb->pipe),
		usb_pipein (urb->pipe) ? "in" : "out",
		status,
		urb->actual_length, urb->transfer_buffer_length);
#endif

	/* complete() can reenter this HCD */
	usb_hcd_unlink_urb_from_ep(ft313_to_hcd(ft313), urb);
	spin_unlock (&ft313->lock);
	usb_hcd_giveback_urb(ft313_to_hcd(ft313), urb, status);
	spin_lock (&ft313->lock);
	DEBUG_MSG("Exit--\n");
}

static void start_unlink_async (struct ft313_hcd *ft313, struct ehci_qh *qh);
static void unlink_async (struct ft313_hcd *ft313, struct ehci_qh *qh);

static int qh_schedule (struct ft313_hcd *ft313, struct ehci_qh *qh);

/*
 * Process and free completed qtds for a qh, returning URBs to drivers.
 * Chases up to qh->hw_current.  Returns number of completions called,
 * indicating how much "real" work we did.
 */
static unsigned
qh_completions (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	FUN_ENTRY();

	struct ehci_qtd		*last, *end = qh->dummy;
	struct list_head	*entry, *tmp;
	int			last_status;
	int			stopped;
	unsigned		count = 0;
	u8			state;
	struct ehci_qh_hw	*hw = qh->hw;
	struct urb		*urb = NULL;
	struct ehci_qtd		*qtd = NULL;
	int			last_seg_still_active = 0;
	int			got_short_packet = 0;
	int			qh_in_unlink = 0;

	if (NULL == hw) {
		BUG_ON("hw ptr is NULL!\n");
		return count;
	}

	// Update qH from FT313 memory
	DEBUG_MSG("Update qH 0x%X from FT313 memory\n", qh->qh_ft313);
	ft313_mem_read(ft313, hw, sizeof(*hw), qh->qh_ft313);

	if (unlikely (list_empty (&qh->qtd_list))) {
		DEBUG_MSG("qH 0x%X has empty qTD list\n", qh->qh_ft313);
		FUN_EXIT();
		return count;
	}

	/* completions (or tasks on other cpus) must never clobber HALT
	 * till we've gone through and cleaned everything up, even when
	 * they add urbs to this qh's queue or mark them for unlinking.
	 *
	 * NOTE:  unlinking expects to be done in queue order.
	 *
	 * It's a bug for qh->qh_state to be anything other than
	 * QH_STATE_IDLE, unless our caller is scan_async() or
	 * scan_periodic().
	 */
	state = qh->qh_state;
	qh->qh_state = QH_STATE_COMPLETING;
	stopped = (state == QH_STATE_IDLE);
	DEBUG_MSG("Value of stopped is %d as qH 0x%X status is %d\n", stopped, qh->qh_ft313, state);

 rescan:
	last = NULL;
	last_status = -EINPROGRESS;
	qh->needs_rescan = 0;

	/* remove de-activated QTDs from front of queue.
	 * after faults (including short reads), cleanup this urb
	 * then let the queue advance.
	 * if queue is stopped, handles unlinks.
	 */
	list_for_each_safe (entry, tmp, &qh->qtd_list) {
		//struct ehci_qtd	*qtd;
		//struct urb	*urb;
		u32		token = 0;

		qtd = list_entry (entry, struct ehci_qtd, qtd_list);
		// Update qtd from ft313
		if (NULL == qtd) {
			BUG_ON("qtd ptr is NULL\n");
			return count;
		}

		urb = qtd->urb;
		DEBUG_MSG("urb associated with qTD 0x%X is 0x%X\n", qtd->qtd_ft313, urb);

		/* clean up any state from previous QTD ...*/
		if (last) {
			if (likely (last->urb != urb)) {
				DEBUG_MSG("urb is done in loop\n");
				ft313_urb_done(ft313, last->urb, last_status);
				count++;
				last_status = -EINPROGRESS;
			}
			ft313_qtd_free (ft313, last);
			last = NULL;
		}

		/* ignore urbs submitted during completions we reported */
		if (qtd == end) {
			DEBUG_MSG("Reach last qTD\n");
			break;
		}
		/* hardware copies qtd out of qh overlay */
		rmb ();
		DEBUG_MSG("Update qTD 0x%X hw token from FT313 \n", qtd->qtd_ft313);
		ft313_mem_read(ft313,
			       &(qtd->hw_token),
			       sizeof(qtd->hw_token),
			       qtd->qtd_ft313 + offsetof(struct ehci_qtd_hw, hw_token));
		token = hc32_to_cpu(ft313, qtd->hw_token);

		/* always clean up qtds the hc de-activated */
 retry_xacterr:
		if ((token & QTD_STS_ACTIVE) == 0) {
			DEBUG_MSG("qTD active bit is off\n");
			/* on STALL, error, and short reads this urb must
			 * complete and all its qtds must be recycled.
			 */
			if ((token & QTD_STS_HALT) != 0) {
				DEBUG_MSG("Halt bit is set for qTD 0x%X\n", qtd->qtd_ft313);
				/* retry transaction errors until we
				 * reach the software xacterr limit
				 */
				if ((token & QTD_STS_XACT) &&
						QTD_CERR(token) == 0 &&
						++qh->xacterrs < QH_XACTERR_MAX &&
						!urb->unlinked) {
					DEBUG_MSG(
	"detected XactErr len %zu/%zu retry %d\n",
	qtd->length - QTD_LENGTH(token), qtd->length, qh->xacterrs);

					/* reset the token in the qtd and the
					 * qh overlay (which still contains
					 * the qtd) so that we pick up from
					 * where we left off
					 */
					token &= ~QTD_STS_HALT;
					token |= QTD_STS_ACTIVE |
							(EHCI_TUNE_CERR << 10);
					qtd->hw_token = cpu_to_hc32(ft313,
							token);
					ft313_mem_write(ft313, &token, sizeof(token),
							qtd->qtd_ft313 + offsetof(struct ehci_qtd_hw, hw_token));
					wmb();
					hw->hw_token = cpu_to_hc32(ft313,
							token);
					ft313_mem_write(ft313, &token, sizeof(token),
							qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_token));
					goto retry_xacterr;
				}
				stopped = 1;

			/* magic dummy for some short reads; qh won't advance.
			 * that silicon quirk can kick in with this dummy too.
			 *
			 * other short reads won't stop the queue, including
			 * control transfers (status stage handles that) or
			 * most other single-qtd reads ... the queue stops if
			 * URB_SHORT_NOT_OK was set so the driver submitting
			 * the urbs could clean it up.
			 */
			} else if (IS_SHORT_READ (token)
					&& !(qtd->hw_alt_next
						& EHCI_LIST_END(ft313))) {
				DEBUG_MSG("qTD meet short packet\n");
				got_short_packet = 1;
				stopped = 1;
			}

		/* stop scanning when we reach qtds the hc is using */
		} else if (likely (!stopped
				&& HC_IS_RUNNING (ft313_to_hcd(ft313)->state))) {
			if ((usb_pipetype (urb->pipe) != PIPE_INTERRUPT)) {
				DEBUG_MSG("qTD Active bit in status is still on!\n");
				DEBUG_MSG("Stop process and jump out of loop\n");
			}
			last_seg_still_active = 1;
			break;

		/* scan the whole queue for unlinks whenever it stops */
		} else {
			stopped = 1;
			qh_in_unlink = 1;

			/* cancel everything if we halt, suspend, etc */
			if (!HC_IS_RUNNING(ft313_to_hcd(ft313)->state))
				last_status = -ESHUTDOWN;

			/* this qtd is active; skip it unless a previous qtd
			 * for its urb faulted, or its urb was canceled.
			 */
			else if (last_status == -EINPROGRESS && !urb->unlinked)
				continue;

			/* qh unlinked; token in overlay may be most current */
			if (state == QH_STATE_IDLE
					&& cpu_to_hc32(ft313, qtd->qtd_ft313)
						== hw->hw_current) {
				token = hc32_to_cpu(ft313, hw->hw_token);

				/* An unlink may leave an incomplete
				 * async transaction in the TT buffer.
				 * We have to clear it.
				 */
				ft313_clear_tt_buffer(ft313, qh, urb, token);
			}
		}

		/* unless we already know the urb's status, collect qtd status
		 * and update count of bytes transferred.  in common short read
		 * cases with only one data qtd (including control transfers),
		 * queue processing won't halt.  but with two or more qtds (for
		 * example, with a 32 KB transfer), when the first qtd gets a
		 * short read the second must be removed by hand.
		 */
		if (last_status == -EINPROGRESS) {
			last_status = qtd_copy_status(ft313, urb,
					qtd->length, token, qtd->hw_buf[0]);
			if (last_status == -EREMOTEIO
					&& (qtd->hw_alt_next
						& EHCI_LIST_END(ft313)))
				last_status = -EINPROGRESS;

			/* As part of low/full-speed endpoint-halt processing
			 * we must clear the TT buffer (11.17.5).
			 */
			if (unlikely(last_status != -EINPROGRESS &&
					last_status != -EREMOTEIO)) {
				/* The TT's in some hubs malfunction when they
				 * receive this request following a STALL (they
				 * stop sending isochronous packets).  Since a
				 * STALL can't leave the TT buffer in a busy
				 * state (if you believe Figures 11-48 - 11-51
				 * in the USB 2.0 spec), we won't clear the TT
				 * buffer in this case.  Strictly speaking this
				 * is a violation of the spec.
				 */
				if (last_status != -EPIPE);
					ft313_clear_tt_buffer(ft313, qh, urb,
							token);
			}
		}

		/* if we're removing something not at the queue head,
		 * patch the hardware queue pointer.
		 */
		if (stopped && qtd->qtd_list.prev != &qh->qtd_list) {
			last = list_entry (qtd->qtd_list.prev,
					struct ehci_qtd, qtd_list);
			last->hw_next = qtd->hw_next;
			DEBUG_MSG("Update qTD 0x%X's hw_next ptr to 0x%X\n", last->qtd_ft313, qtd->hw_next);
			ft313_mem_write(ft313,
					&(qtd->hw_next),
					sizeof(qtd->hw_next),
					last->qtd_ft313 + offsetof(struct ehci_qtd_hw, hw_next));
		}

		/* remove qtd; it's recycled after possible urb completion */
		list_del (&qtd->qtd_list);
		last = qtd;

		/* reinit the xacterr counter for the next qtd */
		qh->xacterrs = 0;
	} // End of qtd list loop

	/* last urb's completion might still need calling */
	if (likely (last != NULL)) {
		if ((usb_pipetype (urb->pipe) == PIPE_CONTROL) || // Control transfer
		    (usb_pipetype (urb->pipe) == PIPE_INTERRUPT) || // Interrupt transfer
		    (1 == got_short_packet) || // Got short packet case
		    (last_status != -EINPROGRESS) || // Error happens from copy_urb_status()
		    (last->urb->actual_length == last->urb->transfer_buffer_length)) { // Really complete
			DEBUG_MSG("urb 0x%X is done out of loop by qH 0x%X\n", last->urb, qh->qh_ft313);

			// Set lock here!
			unsigned long flags;

			if (!list_empty(&qh->urb_list)) {
				// There is urb waiting
				struct qh_urb_queue_item *qh_urb_q_item;
				qh_urb_q_item = list_entry(qh->urb_list.next,
							   struct qh_urb_queue_item,
							   urb_list);
				qh->urb = qh_urb_q_item->urb;
				qh->urb_pending = 1;
				qh->mem_flags = last->mem_flags;
				DEBUG_MSG("urb 0x%X is waiting already\n", qh->urb);
				list_del(&qh_urb_q_item->urb_list);
				kfree(qh_urb_q_item);
			} else {
				// Nothing to do
				qh->urb = NULL;
				qh->urb_pending = 0;
				DEBUG_MSG("qh 0x%X (0x%X) is idle already\n", qh->qh_ft313, qh);
			}

			ft313_urb_done(ft313, last->urb, last_status);
			count++;
			ft313_qtd_free (ft313, last);
		} else {
			if (0 == qh_in_unlink) { // qH is not in unlink process
				// Program next segment for the same urb
				if (0 == last_seg_still_active) { // This is used to prevent programing next segment
								  // if this function is called too early
					qh->urb_pending = 1;
					qh->urb = urb;
					qh->mem_flags = last->mem_flags;
					ft313_qtd_free(ft313, last);
					DEBUG_MSG("Program next segment of urb 0x%X\n", urb);
				} else {
					ALERT_MSG("Last segment is still Active, error case!\n");
					qh->urb_pending = 0;
				}
			} else {
				qh->urb_pending = 0;
				ft313_qtd_free(ft313, last);
			}
		}
	} else if ((1 == last_seg_still_active) &&
		   (usb_pipetype (urb->pipe) != PIPE_INTERRUPT)) {
		//DEBUG_MSG("Last segment is still Active when complete irq generated, may due to irq is from other qH\n");
		qh->urb_pending = 0;
	}

	/* Do we need to rescan for URBs dequeued during a giveback? */
	if (unlikely(qh->needs_rescan)) {
		DEBUG_MSG("qH 0x%X need rescan\n", qh->qh_ft313);
		/* If the QH is already unlinked, do the rescan now. */
		if (state == QH_STATE_IDLE)
			goto rescan;

		/* Otherwise we have to wait until the QH is fully unlinked.
		 * Our caller will start an unlink if qh->needs_rescan is
		 * set.  But if an unlink has already started, nothing needs
		 * to be done.
		 */
		if (state != QH_STATE_LINKED)
			qh->needs_rescan = 0;
	}

	/* restore original state; caller must unlink or relink */
	qh->qh_state = state;

	/* be sure the hardware's done with the qh before refreshing
	 * it after fault cleanup, or recovering from silicon wrongly
	 * overlaying the dummy qtd (which reduces DMA chatter).
	 */
	if (stopped != 0 || hw->hw_qtd_next == EHCI_LIST_END(ft313)) {
		switch (state) {
		case QH_STATE_IDLE:
			qh_refresh(ft313, qh);
			break;
		case QH_STATE_LINKED:
			/* We won't refresh a QH that's linked (after the HC
			 * stopped the queue).  That avoids a race:
			 *  - HC reads first part of QH;
			 *  - CPU updates that first part and the token;
			 *  - HC reads rest of that QH, including token
			 * Result:  HC gets an inconsistent image, and then
			 * DMAs to/from the wrong memory (corrupting it).
			 *
			 * That should be rare for interrupt transfers,
			 * except maybe high bandwidth ...
			 */

			/* Tell the caller to start an unlink */
			qh->needs_rescan = 1;
			DEBUG_MSG("qH 0x%X needs rescan due to stopped flag set\n", qh->qh_ft313);
			break;
		/* otherwise, unlink already started */
		}
	}

	FUN_EXIT();

	return count;
}

/*-------------------------------------------------------------------------*/

// high bandwidth multiplier, as encoded in highspeed endpoint descriptors
#define hb_mult(wMaxPacketSize) (1 + (((wMaxPacketSize) >> 11) & 0x03))
// ... and packet size, for any kind of endpoint descriptor
#define max_packet(wMaxPacketSize) ((wMaxPacketSize) & 0x07ff)

/*
 * reverse of qh_urb_transaction:  free a list of TDs.
 * used for cleanup after errors, before HC sees an URB's TDs.
 */
static void qtd_list_free (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct list_head	*qtd_list
) {
	struct list_head	*entry, *temp;
	FUN_ENTRY();

	list_for_each_safe (entry, temp, qtd_list) {
		struct ehci_qtd	*qtd;

		qtd = list_entry (entry, struct ehci_qtd, qtd_list);
		list_del (&qtd->qtd_list);
		ft313_qtd_free (ft313, qtd);
	}

	FUN_EXIT();
}


/*
 * create a list of filled qtds for this URB; won't link into qh.
 */
static struct list_head *
qh_urb_transaction (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct list_head	*head,
	gfp_t			flags
) {
	struct ehci_qtd		*qtd, *qtd_prev;
	void			*buf;
	int			len, this_sg_len, maxpacket;
	int			is_input;
	u32			token;
	int			i;
	struct scatterlist	*sg;

	FUN_ENTRY();

	/*
	 * URBs map to sequences of QTDs:  one logical transaction
	 */
	qtd = ft313_qtd_alloc (ft313, flags);
	if (unlikely (!qtd))
		return NULL;
	list_add_tail (&qtd->qtd_list, head);
	qtd->urb = urb;
	qtd->mem_flags = flags;

	token = QTD_STS_ACTIVE;
	token |= (EHCI_TUNE_CERR << 10);
	/* for split transactions, SplitXState initialized to zero */

	len = urb->transfer_buffer_length - urb->actual_length;
	DEBUG_MSG("urb 0x%X reminding payload size is %d\n", urb, len);

	if (usb_pipebulk(urb->pipe) &&
	    (len <= 0)) {
		ALERT_MSG("\n\n\nTry to programming %d length bulk packet!!!\n\n\n", len);
		goto cleanup;
	}

	is_input = usb_pipein (urb->pipe);
	if (usb_pipecontrol (urb->pipe)) {
		DEBUG_MSG("Create qTD for SETUP token\n");
		/* SETUP pid */
		if (0 > qtd_fill(ft313, qtd, urb->setup_packet,
				sizeof (struct usb_ctrlrequest),
				token | (2 /* "setup" */ << 8), 8)) {
			ERROR_MSG("qTD filling for SETUP token failed\n");
			goto cleanup;
		}

		/* ... and always at least one more pid */
		token ^= QTD_TOGGLE;
		qtd_prev = qtd;
		qtd = ft313_qtd_alloc (ft313, flags);
		if (unlikely (!qtd)) {
			ALERT_MSG("qTD allocation fail for ctrl transfer !!!\n");
			goto cleanup;
		}
		qtd->urb = urb;
//		qtd_prev->hw_next = QTD_NEXT(ft313, qtd->qtd_dma);
		qtd_prev->hw_next = QTD_NEXT(ft313, qtd->qtd_ft313);

		list_add_tail (&qtd->qtd_list, head);

		ft313_mem_write(ft313, qtd_prev, sizeof (struct ehci_qtd_hw), qtd_prev->qtd_ft313);

		/* for zero length DATA stages, STATUS is always IN */
		if (len == 0) {
			token |= (1 /* "in" */ << 8);
		}
		DEBUG_MSG("Data phase length is %d\n", len);
	}

	/*
	 * data transfer stage:  buffer setup
	 */
	i = urb->num_sgs;
	if (len > 0 && i > 0) {
		ALERT_MSG("FT313 HCD does not support scatter list\n");
		goto cleanup;

		sg = urb->sg;
		buf = sg_dma_address(sg);

		/* urb->transfer_buffer_length may be smaller than the
		 * size of the scatterlist (or vice versa)
		 */
		this_sg_len = min_t(int, sg_dma_len(sg), len);
	} else {
		sg = NULL;
		//buf = urb->transfer_dma;
		if ((!usb_pipecontrol(urb->pipe)) &&
		    (urb->transfer_buffer == NULL)) {
			ALERT_MSG("Only DMA address is provided\n");
			goto cleanup;
		}
		else
			buf = urb->transfer_buffer + urb->actual_length;

		this_sg_len = len;
	}

	if (is_input)
		token |= (1 /* "in" */ << 8);
	/* else it's already initted to "out" pid (0 << 8) */

	maxpacket = max_packet(usb_maxpacket(urb->dev, urb->pipe, !is_input));
	qtd->maxpacket = maxpacket;

	/*
	 * buffer gets wrapped in one or more qtds;
	 * last one may be "short" (including zero len)
	 * and may serve as a control status ack
	 */
	//for (;;) FixMe: One qTD only for FT313
	{
		int this_qtd_len;

		this_qtd_len = qtd_fill(ft313, qtd, buf, this_sg_len, token,
				maxpacket);
		if (this_qtd_len < 0) {
			ERROR_MSG("qTD 0x%X filling error\n", qtd->qtd_ft313);
			goto cleanup;
		}
		this_sg_len -= this_qtd_len;
		len -= this_qtd_len;
		buf += this_qtd_len;

		/*
		 * short reads advance to a "magic" dummy instead of the next
		 * qtd ... that forces the queue to stop, for manual cleanup.
		 * (this will usually be overridden later.)
		 */
		if (is_input)
			qtd->hw_alt_next = ft313->async->hw->hw_alt_next;


		/* qh makes control packets use qtd toggle; maybe switch it */
		if ((maxpacket & (this_qtd_len + (maxpacket - 1))) == 0) {
			token ^= QTD_TOGGLE;
		}
#if 0
		if (likely(this_sg_len <= 0)) {
			if (--i <= 0 || len <= 0)
				break;
			sg = sg_next(sg);
			buf = sg_dma_address(sg);
			this_sg_len = min_t(int, sg_dma_len(sg), len);
		}

		qtd_prev = qtd;
		qtd = ft313_qtd_alloc (ft313, flags);
		if (unlikely (!qtd))
			goto cleanup;
		qtd->urb = urb;
		qtd_prev->hw_next = QTD_NEXT(ft313, qtd->qtd_dma);
		list_add_tail (&qtd->qtd_list, head);
#endif

	}

	/*
	 * unless the caller requires manual cleanup after short reads,
	 * have the alt_next mechanism keep the queue running after the
	 * last data qtd (the only one, for control and most other cases).
	 */
	if (likely ((urb->transfer_flags & URB_SHORT_NOT_OK) == 0
				|| usb_pipecontrol (urb->pipe)))
		qtd->hw_alt_next = EHCI_LIST_END(ft313);

	/*
	 * control requests may need a terminating data "status" ack;
	 * bulk ones may need a terminating short packet (zero length).
	 */
	if (likely (urb->transfer_buffer_length != 0)) {
		int	one_more = 0;

		if (usb_pipecontrol (urb->pipe)) {
			one_more = 1;
			token ^= 0x0100;	/* "in" <--> "out"  */
			token |= QTD_TOGGLE;	/* force DATA1 */
		} else if (usb_pipebulk (urb->pipe)
				&& (urb->transfer_flags & URB_ZERO_PACKET)
				&& !(urb->transfer_buffer_length % maxpacket)) {
			ERROR_MSG("This urb requires additional Zero Length Packet\n");
			one_more = 1;
		}
		if (one_more) {
			qtd_prev = qtd;
			qtd = ft313_qtd_alloc (ft313, flags);
			if (unlikely (!qtd)) {
				ALERT_MSG("qTD allocation fail!\n");
				goto cleanup;
			}
			qtd->urb = urb;
			qtd_prev->hw_next = QTD_NEXT(ft313, qtd->qtd_ft313);
			ft313_mem_write(ft313, qtd_prev, sizeof (struct ehci_qtd_hw), qtd_prev->qtd_ft313);
			list_add_tail (&qtd->qtd_list, head);

			/* never any data in such packets */
			qtd_fill(ft313, qtd, 0, 0, token, 0);
		}
	}

	/* by default, enable interrupt on urb completion */
	if (likely (!(urb->transfer_flags & URB_NO_INTERRUPT))) {
		DEBUG_MSG("Set IOC bit to token dword\n");
		qtd->hw_token |= cpu_to_hc32(ft313, QTD_IOC);
	} else {
		DEBUG_MSG("URB_NO_INTERRUPT flag is set, force set IOC\n");
		qtd->hw_token |= cpu_to_hc32(ft313, QTD_IOC);
	}

	ft313_mem_write(ft313, qtd, sizeof (struct ehci_qtd_hw), qtd->qtd_ft313);

	FUN_EXIT();

	return head;

cleanup:
	qtd_list_free (ft313, urb, head);
	ALERT_MSG("qTD list creation failed!\n");
	FUN_EXIT();
	return NULL;
}

/*
 * Each QH holds a qtd list; a QH is used for everything except iso.
 *
 * For interrupt urbs, the scheduler must set the microframe scheduling
 * mask(s) each time the QH gets scheduled.  For highspeed, that's
 * just one microframe in the s-mask.  For split interrupt transactions
 * there are additional complications: c-mask, maybe FSTNs.
 */
static struct ehci_qh *
qh_make (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	gfp_t			flags
) {
	FUN_ENTRY();

	struct ehci_qh		*qh = ft313_qh_alloc (ft313, flags);
	u32			info1 = 0, info2 = 0;
	int			is_input, type;
	int			maxp = 0;
	struct usb_tt		*tt = urb->dev->tt;
	struct ehci_qh_hw	*hw;

	if (!qh)
		return qh;

	/*
	 * init endpoint/device data for this QH
	 */
	info1 |= usb_pipeendpoint (urb->pipe) << 8;
	info1 |= usb_pipedevice (urb->pipe) << 0;

	is_input = usb_pipein (urb->pipe);
	type = usb_pipetype (urb->pipe);
	maxp = usb_maxpacket (urb->dev, urb->pipe, !is_input);

	/* 1024 byte maxpacket is a hardware ceiling.  High bandwidth
	 * acts like up to 3KB, but is built from smaller packets.
	 */
	if (max_packet(maxp) > 1024) {
		ft313_dbg(ft313, "bogus qh maxpacket %d\n", max_packet(maxp));
		goto done;
	}
	DEBUG_MSG("Max packet size is %d\n", max_packet(maxp));

	/* Compute interrupt scheduling parameters just once, and save.
	 * - allowing for high bandwidth, how many nsec/uframe are used?
	 * - split transactions need a second CSPLIT uframe; same question
	 * - splits also need a schedule gap (for full/low speed I/O)
	 * - qh has a polling interval
	 *
	 * For control/bulk requests, the HC or TT handles these.
	 */
	if (type == PIPE_INTERRUPT) {
		qh->usecs = NS_TO_US(usb_calc_bus_time(USB_SPEED_HIGH,
				is_input, 0,
				hb_mult(maxp) * max_packet(maxp)));
		qh->start = NO_FRAME;
		qh->stamp = ft313->periodic_stamp;

		if (urb->dev->speed == USB_SPEED_HIGH) {
			DEBUG_MSG("High speed Interrupt transfer\n");
			qh->c_usecs = 0;
			qh->gap_uf = 0;

			qh->period = urb->interval >> 3;
			if (qh->period == 0 && urb->interval != 1) {
				/* NOTE interval 2 or 4 uframes could work.
				 * But interval 1 scheduling is simpler, and
				 * includes high bandwidth.
				 */
				urb->interval = 1;
			} else if (qh->period > ft313->periodic_size) {
				qh->period = ft313->periodic_size;
				urb->interval = qh->period << 3;
			}
		} else {
			DEBUG_MSG("Full/low speed Interrupt tranfer\n");
			int		think_time;

			/* gap is f(FS/LS transfer times) */
			qh->gap_uf = 1 + usb_calc_bus_time (urb->dev->speed,
					is_input, 0, maxp) / (125 * 1000);

			/* FIXME this just approximates SPLIT/CSPLIT times */
			if (is_input) {		// SPLIT, gap, CSPLIT+DATA
				qh->c_usecs = qh->usecs + HS_USECS (0);
				qh->usecs = HS_USECS (1);
			} else {		// SPLIT+DATA, gap, CSPLIT
				qh->usecs += HS_USECS (1);
				qh->c_usecs = HS_USECS (0);
			}

			think_time = tt ? tt->think_time : 0;
			qh->tt_usecs = NS_TO_US (think_time +
					usb_calc_bus_time (urb->dev->speed,
					is_input, 0, max_packet (maxp)));
			qh->period = urb->interval;
			if (qh->period > ft313->periodic_size) {
				qh->period = ft313->periodic_size;
				urb->interval = qh->period;
			}
		}
	}

	/* support for tt scheduling, and access to toggles */
	qh->dev = urb->dev;

	/* using TT? */
	switch (urb->dev->speed) {
	case USB_SPEED_LOW:
		info1 |= (1 << 12);	/* EPS "low" */
		/* FALL THROUGH */

	case USB_SPEED_FULL:
		/* EPS 0 means "full" */
		if (type != PIPE_INTERRUPT)
			info1 |= (EHCI_TUNE_RL_TT << 28);
		if (type == PIPE_CONTROL) {
			info1 |= (1 << 27);	/* for TT */
			info1 |= 1 << 14;	/* toggle from qtd */
		}
		info1 |= maxp << 16;

		info2 |= (EHCI_TUNE_MULT_TT << 30);

		/* Some Freescale processors have an erratum in which the
		 * port number in the queue head was 0..N-1 instead of 1..N.
		 */
		//if (ehci_has_fsl_portno_bug(ehci))
		//	info2 |= (urb->dev->ttport-1) << 23;
		//else
			info2 |= urb->dev->ttport << 23;

		/* set the address of the TT; for TDI's integrated
		 * root hub tt, leave it zeroed.
		 */
		if (tt && tt->hub != ft313_to_hcd(ft313)->self.root_hub)
			info2 |= tt->hub->devnum << 16;

		/* NOTE:  if (PIPE_INTERRUPT) { scheduler sets c-mask } */

		break;

	case USB_SPEED_HIGH:		/* no TT involved */
		info1 |= (2 << 12);	/* EPS "high" */
		if (type == PIPE_CONTROL) {
			info1 |= (EHCI_TUNE_RL_HS << 28);
			info1 |= 64 << 16;	/* usb2 fixed maxpacket */
			info1 |= 1 << 14;	/* toggle from qtd */
			info2 |= (EHCI_TUNE_MULT_HS << 30);
		} else if (type == PIPE_BULK) {
			info1 |= (EHCI_TUNE_RL_HS << 28);
			/* The USB spec says that high speed bulk endpoints
			 * always use 512 byte maxpacket.  But some device
			 * vendors decided to ignore that, and MSFT is happy
			 * to help them do so.  So now people expect to use
			 * such nonconformant devices with Linux too; sigh.
			 */
			info1 |= max_packet(maxp) << 16;
			info2 |= (EHCI_TUNE_MULT_HS << 30);
		} else {		/* PIPE_INTERRUPT */
			info1 |= max_packet (maxp) << 16;
			info2 |= hb_mult (maxp) << 30;
		}
		break;
	default:
		dbg ("bogus dev %p speed %d", urb->dev, urb->dev->speed);
done:
		qh_put (qh);
		FUN_EXIT();
		return NULL;
	}

	/* NOTE:  if (PIPE_INTERRUPT) { scheduler sets s-mask } */

	/* init as live, toggle clear, advance to dummy */
	qh->qh_state = QH_STATE_IDLE;
	hw = qh->hw;
	hw->hw_info1 = cpu_to_hc32(ft313, info1);
	hw->hw_info2 = cpu_to_hc32(ft313, info2);
	qh->is_out = !is_input;
	usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe), !is_input, 1);
	ft313_mem_write(ft313, hw, sizeof(struct ehci_qh_hw), qh->qh_ft313);
	qh_refresh (ft313, qh);

	FUN_EXIT();

	return qh;
}

/*-------------------------------------------------------------------------*/

/* move qh (and its qtds) onto async queue; maybe enable queue.  */

static void qh_link_async (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	__hc32		dma = QH_NEXT(ft313, qh->qh_dma);
	__hc32		dma_ft313 = QH_NEXT(ft313, qh->qh_ft313);
	struct ehci_qh	*head;

	FUN_ENTRY();

	/* Don't link a QH if there's a Clear-TT-Buffer pending */
	if (unlikely(qh->clearing_tt))
		return;

	WARN_ON(qh->qh_state != QH_STATE_IDLE);

	/* (re)start the async schedule? */
	head = ft313->async;
	timer_action_done (ft313, TIMER_ASYNC_OFF);
	if (!head->qh_next.qh) {
		u32	cmd = ft313_reg_read32(ft313, &ft313->regs->command);

		if (!(cmd & ASCH_EN)) {
			/* in case a clear of CMD_ASE didn't take yet */
			(void)handshake(ft313, &ft313->regs->status,
					ASCH_STS, 0, 150);
			cmd |= ASCH_EN | RS;
			DEBUG_MSG("Start Async Schedule for qH 0x%X\n", qh->qh_ft313);
			ft313_reg_write32(ft313, cmd, &ft313->regs->command);
			ft313_to_hcd(ft313)->state = HC_STATE_RUNNING;
			/* posted write need not be known to HC yet ... */
		}
	}

	/* clear halt and/or toggle; and maybe recover from silicon quirk */
	qh_refresh(ft313, qh);

	/* splice right after start */
	qh->qh_next = head->qh_next;
	qh->hw->hw_next = head->hw->hw_next;
	ft313_mem_write(ft313, &(head->hw->hw_next), sizeof(qh->hw->hw_next),
			qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));
	wmb ();

	head->qh_next.qh = qh;
	head->hw->hw_next = dma_ft313;
	ft313_mem_write(ft313, &(head->hw->hw_next), sizeof(head->hw->hw_next),
			head->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));

	qh_get(qh);
	qh->xacterrs = 0;
	qh->qh_state = QH_STATE_LINKED;
	/* qtd completions reported later by interrupt */

	FUN_EXIT();
}


/*-------------------------------------------------------------------------*/

/*
 * For control/bulk/interrupt, return QH with these TDs appended.
 * Allocates and initializes the QH if necessary.
 * Returns null if it can't allocate a QH it needs to.
 * If the QH has TDs (urbs) already, that's great.
 */
static struct ehci_qh *qh_append_tds (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct list_head	*qtd_list,
	int			epnum,
	void			**ptr
)
{
	struct ehci_qh		*qh = NULL;
	__hc32			qh_addr_mask = cpu_to_hc32(ft313, 0x7f);

	FUN_ENTRY();

	qh = (struct ehci_qh *) *ptr;
	if (unlikely (qh == NULL)) {
		/* can't sleep here, we have ehci->lock... */
		DEBUG_MSG("qH not availiable yet, create one\n");
		qh = qh_make (ft313, urb, GFP_ATOMIC);
		*ptr = qh;
	}

	DEBUG_MSG("qH 0x%X is used to serve EP 0x%X at Addr %d\n", qh->qh_ft313, epnum, usb_pipedevice(urb->pipe));

	if (likely (qh != NULL)) {
		struct ehci_qtd	*qtd;

		if (unlikely (list_empty (qtd_list)))
			qtd = NULL;
		else
			qtd = list_entry (qtd_list->next, struct ehci_qtd,
					qtd_list);

		/* control qh may need patching ... */
		if (unlikely (epnum == 0)) {

                        /* usb_reset_device() briefly reverts to address 0 */
                        if (usb_pipedevice (urb->pipe) == 0) {
				qh->hw->hw_info1 &= ~qh_addr_mask;
				ft313_mem_write(ft313,
						&(qh->hw->hw_info1),
						sizeof(qh->hw->hw_info1),
						qh->qh_ft313 + offsetof(struct ehci_qh_hw, hw_info1));
			}
		}

		/* just one way to queue requests: swap with the dummy qtd.
		 * only hc or qh_refresh() ever modify the overlay.
		 */
		if (likely (qtd != NULL)) {
			struct ehci_qtd		*dummy;
			dma_addr_t		dma;
			u32			dma_ft313;
			__hc32			token;

			/* to avoid racing the HC, use the dummy td instead of
			 * the first td of our list (becomes new dummy).  both
			 * tds stay deactivated until we're done, when the
			 * HC is allowed to fetch the old dummy (4.10.2).
			 */
			token = qtd->hw_token;
			qtd->hw_token = HALT_BIT(ft313);
			ft313_mem_write(ft313, &(qtd->hw_token), sizeof(qtd->hw_token),
					qtd->qtd_ft313 + offsetof(struct ehci_qtd_hw, hw_token));
			wmb ();
			dummy = qh->dummy;

			dma = dummy->qtd_dma;
			dma_ft313 = dummy->qtd_ft313;
			*dummy = *qtd;
			DEBUG_MSG("Dummy qTD for qH 0x%X is at 0x%X originally\n", qh->qh_ft313, dma_ft313);
			ft313_mem_write(ft313, qtd, sizeof(struct ehci_qtd_hw), dma_ft313); //Copy in ft313 memory also
			dummy->qtd_dma = dma;
			dummy->qtd_ft313 = dma_ft313;

			list_del (&qtd->qtd_list);
			list_add (&dummy->qtd_list, qtd_list);
			list_splice_tail(qtd_list, &qh->qtd_list);

			ft313_qtd_init(ft313, qtd, qtd->qtd_dma);
			DEBUG_MSG("qTD 0x%X become new dummy for qH 0x%X\n", qtd->qtd_ft313, qh->qh_ft313);
			qh->dummy = qtd;

			/* hc must see the new dummy at list end */
			dma = qtd->qtd_dma;
			dma_ft313 = qtd->qtd_ft313;
			qtd = list_entry (qh->qtd_list.prev,
					struct ehci_qtd, qtd_list);
			//qtd->hw_next = QTD_NEXT(ehci, dma);
			qtd->hw_next = QTD_NEXT(ft313, dma_ft313);
			ft313_mem_write(ft313, &(qtd->hw_next), sizeof(qtd->hw_next),
					qtd->qtd_ft313 + offsetof(struct ehci_qtd_hw, hw_next));


			/* let the hc process these next qtds */
			wmb ();
			dummy->hw_token = token;
			ft313_mem_write(ft313, &(dummy->hw_token), sizeof(dummy->hw_token),
					dummy->qtd_ft313 + offsetof(struct ehci_qtd_hw, hw_token));

			urb->hcpriv = qh_get (qh); // FIXME: maybe only needed for first segment programming
			qh->urb = urb;
		}
	}

#if 0 // Debug only
	{
		struct list_head	*entry, *temp;

		DEBUG_MSG("Dummy qTD for qH 0x%X is 0x%X\n", qh->qh_ft313, qh->dummy->qtd_ft313);

		list_for_each_safe (entry, temp, &qh->qtd_list) {
			struct ehci_qtd	*qtd;

			qtd = list_entry (entry, struct ehci_qtd, qtd_list);
			//list_del (&qtd->qtd_list);
			//ft313_qtd_free (ft313, qtd);
			DEBUG_MSG("qTD 0x%X is here\n", qtd->qtd_ft313);
		}
		ft313_reg_read32(ft313, &ft313->regs->intr_enable);
		ft313_reg_read32(ft313, &ft313->regs->status);
		ft313_reg_read32(ft313, &ft313->regs->async_next);
	}
#endif


	FUN_EXIT();

	return qh;
}

/*-------------------------------------------------------------------------*/

static int
submit_async (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct list_head	*qtd_list,
	gfp_t			mem_flags
) {
	FUN_ENTRY();

	int			epnum;
	unsigned long		flags;
	struct ehci_qh		*qh = NULL;
	int			rc;

	DEBUG_MSG("mem flags for ctrl or bulk is 0x%X\n", mem_flags);

	epnum = urb->ep->desc.bEndpointAddress;

#ifdef EHCI_URB_TRACE
	{
		struct ehci_qtd *qtd;
		qtd = list_entry(qtd_list->next, struct ehci_qtd, qtd_list);
		ehci_dbg(ehci,
			 "%s %s urb %p ep%d%s len %d, qtd %p [qh %p]\n",
			 __func__, urb->dev->devpath, urb,
			 epnum & 0x0f, (epnum & USB_DIR_IN) ? "in" : "out",
			 urb->transfer_buffer_length,
			 qtd, urb->ep->hcpriv);
	}
#endif

	spin_lock_irqsave (&ft313->lock, flags);
	if (unlikely(!HCD_HW_ACCESSIBLE(ft313_to_hcd(ft313)))) {
		rc = -ESHUTDOWN;
		goto done;
	}

	if (urb->actual_length == 0) { // Only first segment need this!
		DEBUG_MSG("Link urb to ep \n");
		rc = usb_hcd_link_urb_to_ep(ft313_to_hcd(ft313), urb);
		if (unlikely(rc))
			goto done;
	} else {
		DEBUG_MSG("Program next segment!\n");
	}

	qh = qh_append_tds(ft313, urb, qtd_list, epnum, &urb->ep->hcpriv);

	if (unlikely(qh == NULL)) {
		usb_hcd_unlink_urb_from_ep(ft313_to_hcd(ft313), urb);
		rc = -ENOMEM;
		goto done;
	}

	/* Control/bulk operations through TTs don't need scheduling,
	 * the HC and TT handle it when the TT has a buffer ready.
	 */
	if (likely (qh->qh_state == QH_STATE_IDLE))
		qh_link_async(ft313, qh);
	else {
		DEBUG_MSG("qh 0x%X is not idle but %d \n", qh->qh_ft313, qh->qh_state);
	}

#if 0 // Debug only
	display_async_list(ft313);
#endif

 done:
	spin_unlock_irqrestore (&ft313->lock, flags);
	if (unlikely (qh == NULL)) {
		qtd_list_free (ft313, urb, qtd_list);
	}

	DEBUG_MSG("return value %d\n", rc);

	FUN_EXIT();

	return rc;
}

static int
submit_async_next (
	struct ft313_hcd	*ft313,
	struct urb		*urb,
	struct list_head	*qtd_list,
	gfp_t			mem_flags
) {
	FUN_ENTRY();

	int			epnum;
	unsigned long		flags;
	struct ehci_qh		*qh = NULL;
	int			rc;

	epnum = urb->ep->desc.bEndpointAddress;

#ifdef EHCI_URB_TRACE
	{
		struct ehci_qtd *qtd;
		qtd = list_entry(qtd_list->next, struct ehci_qtd, qtd_list);
		ehci_dbg(ehci,
			 "%s %s urb %p ep%d%s len %d, qtd %p [qh %p]\n",
			 __func__, urb->dev->devpath, urb,
			 epnum & 0x0f, (epnum & USB_DIR_IN) ? "in" : "out",
			 urb->transfer_buffer_length,
			 qtd, urb->ep->hcpriv);
	}
#endif

//	spin_lock_irqsave (&ft313->lock, flags);
	if (unlikely(!HCD_HW_ACCESSIBLE(ft313_to_hcd(ft313)))) {
		rc = -ESHUTDOWN;
		goto done;
	}

	if (urb->actual_length == 0) { // Only first segment need this!
		DEBUG_MSG("Link urb to ep \n");
		rc = usb_hcd_link_urb_to_ep(ft313_to_hcd(ft313), urb);
		if (unlikely(rc))
			goto done;
	} else {
		DEBUG_MSG("Program next segment!\n");
	}

	qh = qh_append_tds(ft313, urb, qtd_list, epnum, &urb->ep->hcpriv);

	if (unlikely(qh == NULL)) {
		usb_hcd_unlink_urb_from_ep(ft313_to_hcd(ft313), urb);
		rc = -ENOMEM;
		goto done;
	}

	/* Control/bulk operations through TTs don't need scheduling,
	 * the HC and TT handle it when the TT has a buffer ready.
	 */
	if (likely (qh->qh_state == QH_STATE_IDLE))
		qh_link_async(ft313, qh);
	else {
		DEBUG_MSG("qh 0x%X is not idle but %d \n", qh->qh_ft313, qh->qh_state);
	}

#if 0 // Debug only
	display_async_list(ft313);
#endif

done:
//	spin_unlock_irqrestore (&ft313->lock, flags);
	if (unlikely (qh == NULL)) {
		qtd_list_free (ft313, urb, qtd_list);
	}

	FUN_EXIT();

	return rc;
}


/*-------------------------------------------------------------------------*/

/* the async qh for the qtds being reclaimed are now unlinked from the HC */

static void end_unlink_async (struct ft313_hcd *ft313)
{
	FUN_ENTRY();

	struct ehci_qh		*qh = ft313->reclaim;
	struct ehci_qh		*next;

	iaa_watchdog_done(ft313);

// Debug only
//	display_async_list(ft313);

	//Adjust the asynchronous list register value in case it is the same as qH for unlink
	u32 current_async_addr = 0;
	current_async_addr = ft313_reg_read32(ft313, &ft313->regs->async_next);
	if (qh->qh_ft313 == current_async_addr) {
		ERROR_MSG("Current async list register point to an unlinked qH, have to set manually!\n");

		u32 cmd, status;
		// FIXME: Check whether asynchronous schedule bit is still on before modify async list addr register
		// This fix is not fully tested yet! Yang Chang on August 31, 2012
		status = ft313_reg_read32(ft313, &ft313->regs->status);
		cmd = ft313_reg_read32(ft313, &ft313->regs->command);
		if (0 != (ASCH_STS & status)) { //Asychronous schedule still on
			ALERT_MSG("Hack Async List Addr register on the fly!!!\n");
			ALERT_MSG("Stop Async scheduling first\n");
			ft313_reg_write32(ft313, cmd & ~ASCH_EN, &ft313->regs->command);

			// Make sure asychrous schedule stopped
			handshake(ft313, &ft313->regs->status, ASCH_STS, 0, 150);
		}

		if (qh->qh_next.qh != NULL) {
			struct ehci_qh *next_qh;
			next_qh = qh->qh_next.qh;
			ft313_reg_write32(ft313, next_qh->qh_ft313, &ft313->regs->async_next);
			DEBUG_MSG("Set async list reg as 0x%X\n", next_qh->qh_ft313);
		} else {
			ft313_reg_write32(ft313, ft313->async->qh_ft313, &ft313->regs->async_next);
			ERROR_MSG("Set async list reg as 0x%X\n", ft313->async->qh_ft313);
		}

		//FIXME: Restore Asyncronous schedule bit if stopped above
		if (0 != (ASCH_STS & status)) {
			ft313_reg_write32(ft313, cmd | ASCH_EN, &ft313->regs->command);
			handshake(ft313, &ft313->regs->status, ASCH_STS, ASCH_STS, 150);
			ALERT_MSG("Restore Async scheduling\n");
		}
	}

	// qh->hw_next = cpu_to_hc32(qh->qh_dma);
	qh->qh_state = QH_STATE_IDLE;
	DEBUG_MSG("qH 0x%X is made state QH_STATE_IDLE\n", qh->qh_ft313);
	qh->qh_next.qh = NULL;
	qh_put (qh);			// refcount from reclaim

	/* other unlink(s) may be pending (in QH_STATE_UNLINK_WAIT) */
	next = qh->reclaim;
	ft313->reclaim = next;
	qh->reclaim = NULL;

	qh_completions (ft313, qh);

	if (!list_empty (&qh->qtd_list)
			&& HC_IS_RUNNING (ft313_to_hcd(ft313)->state))
		qh_link_async (ft313, qh);
	else {
		/* it's not free to turn the async schedule on/off; leave it
		 * active but idle for a while once it empties.
		 */
		if (HC_IS_RUNNING (ft313_to_hcd(ft313)->state)
				&& ft313->async->qh_next.qh == NULL)
			timer_action (ft313, TIMER_ASYNC_OFF);
	}
	qh_put(qh);			/* refcount from async list */

	if (next) {
		ft313->reclaim = NULL;
		start_unlink_async (ft313, next);
	}

	FUN_EXIT();
}

/* makes sure the async qh will become idle */
/* caller must own ehci->lock */

static void start_unlink_async (struct ft313_hcd *ft313, struct ehci_qh *qh)
{
	FUN_ENTRY();

	int		cmd = ft313_reg_read32(ft313, &ft313->regs->command);
	struct ehci_qh	*prev;

#ifdef DEBUG
	assert_spin_locked(&ft313->lock);
	if (ft313->reclaim
			|| (qh->qh_state != QH_STATE_LINKED
				&& qh->qh_state != QH_STATE_UNLINK_WAIT)
			)
		BUG ();
#endif

	/* stop async schedule right now? */
	if (unlikely (qh == ft313->async)) {
		/* can't get here without STS_ASS set */
		if (ft313_to_hcd(ft313)->state != HC_STATE_HALT
				&& !ft313->reclaim) {
			/* ... and CMD_IAAD clear */
			ft313_reg_write32(ft313, cmd & ~ASCH_EN,
				    &ft313->regs->command);
			wmb ();
			DEBUG_MSG("Stop Async Scheduling\n");

			// Restore async reg to original value if not!
			u32 async_reg = 0;
			async_reg = ft313_reg_read32(ft313, &ft313->regs->async_next);
			if (async_reg != ft313->async->qh_ft313)
				ft313_reg_write32(ft313, ft313->async->qh_ft313, &ft313->regs->async_next);

			// handshake later, if we need to
			timer_action_done (ft313, TIMER_ASYNC_OFF);
		}
		FUN_EXIT();
		return;
	}

	qh->qh_state = QH_STATE_UNLINK;
	DEBUG_MSG("qH 0x%X is set as UNLINK state\n", qh->qh_ft313);
	ft313->reclaim = qh = qh_get (qh);
	DEBUG_MSG("Set qh 0x%X (%p) as qH for reclaim\n", qh->qh_ft313, qh);

	prev = ft313->async;
//	DEBUG_MSG("From qh 0x%X -> ", prev->qh_ft313);
	while (prev->qh_next.qh != qh) {
		prev = prev->qh_next.qh;
//		printk("qh 0x%X -> ", prev->qh_ft313);
	}
//	printk(" qh 0x%X\n", qh->qh_ft313);

//	DEBUG_MSG("Before remove qH 0x%X\n", qh->qh_ft313);
//	display_async_list(ft313);

	prev->hw->hw_next = qh->hw->hw_next;
	ft313_mem_write(ft313,
			&(qh->hw->hw_next),
			sizeof(qh->hw->hw_next),
			prev->qh_ft313 + offsetof(struct ehci_qh_hw, hw_next));

	prev->qh_next = qh->qh_next;
	if (ft313->qh_scan_next == qh)
		ft313->qh_scan_next = qh->qh_next.qh;
	wmb ();

//	DEBUG_MSG("After remove qH 0x%X\n", qh->qh_ft313);
//	display_async_list(ft313);

#if 0
	//Adjust the asynchronous list register value in case it is the same as qH for unlink
	u32 current_async_addr = 0;
	current_async_addr = ft313_reg_read32(ft313, &ft313->regs->async_next);
	if (qh->qh_ft313 == current_async_addr) {
		if (qh->qh_next.qh != NULL) {
			struct ehci_qh *next_qh;
			next_qh = qh->qh_next.qh;
			ft313_reg_write32(ft313, next_qh->qh_ft313, &ft313->regs->async_next);
		} else {
			ft313_reg_write32(ft313, ft313->async->qh_ft313, &ft313->regs->async_next);
		}
	}
#endif

	/* If the controller isn't running, we don't have to wait for it */
	if (unlikely(!HC_IS_RUNNING(ft313_to_hcd(ft313)->state))) {
		/* if (unlikely (qh->reclaim != 0))
		 *	this will recurse, probably not much
		 */
		DEBUG_MSG("HC is not running!\n");
		end_unlink_async (ft313);
		FUN_EXIT();
		return;
	}

	cmd |= INT_OAAD;
	DEBUG_MSG("Set Interrupt on Asynch Advance Doorbell\n");
	ft313_reg_write32(ft313, cmd, &ft313->regs->command);
	(void)ft313_reg_read32(ft313, &ft313->regs->command);
	iaa_watchdog_start(ft313);

	FUN_EXIT();
}


/*-------------------------------------------------------------------------*/

static void scan_async (struct ft313_hcd *ft313)
{
	FUN_ENTRY();

	bool			stopped;
	struct ehci_qh		*qh;
	enum ft313_timer_action	action = TIMER_IO_WATCHDOG;
	int			qh_count = 0;

	timer_action_done (ft313, TIMER_ASYNC_SHRINK);
	stopped = !HC_IS_RUNNING(ft313_to_hcd(ft313)->state);

	ft313->qh_scan_next = ft313->async->qh_next.qh;
	while (ft313->qh_scan_next) {
		qh = ft313->qh_scan_next;
		ft313->qh_scan_next = qh->qh_next.qh;
 rescan:
		/* clean any finished work for this qh */
		if (!list_empty(&qh->qtd_list)) {
			DEBUG_MSG("qH 0x%X qTD list is not empty\n", qh->qh_ft313);
			int temp;

			/*
			 * Unlinks could happen here; completion reporting
			 * drops the lock.  That's why ehci->qh_scan_next
			 * always holds the next qh to scan; if the next qh
			 * gets unlinked then ehci->qh_scan_next is adjusted
			 * in start_unlink_async().
			 */
			qh = qh_get(qh);
			temp = qh_completions(ft313, qh);
			if (qh->needs_rescan)
				unlink_async(ft313, qh);
			qh->unlink_time = jiffies + EHCI_SHRINK_JIFFIES;
			qh_put(qh);

			qh_count++;

			if (temp != 0)
				goto rescan;
		}

		if (0 != in_interrupt()) { //Assume called from ft313_work by ft313_irq()
			if (qh->urb_pending == 1) {
				if (qh->urb != NULL) {
					//spin_unlock(&ft313->lock);
					if (0 > ft313_urb_enqueue_next(ft313, qh->urb, GFP_ATOMIC)) {
						ALERT_MSG("Program next segment failed!\n");
						qh->urb_pending = 0;
						//spin_lock(&ft313->lock); // ft313_urb_done will release lock first!
						ft313_urb_done(ft313, qh->urb, -EPROTO); // report protocol error!
					} else {
						//qh->urb = NULL;
						qh->urb_pending = 0;
					//	spin_lock(&ft313->lock);
					}
				}
			}
		}

		/* unlink idle entries, reducing DMA usage as well
		 * as HCD schedule-scanning costs.  delay for any qh
		 * we just scanned, there's a not-unusual case that it
		 * doesn't stay idle for long.
		 * (plus, avoids some kind of re-activation race.)
		 */
#ifdef ENABLE_DYN_UNLINK
		if (list_empty(&qh->qtd_list)
				&& qh->qh_state == QH_STATE_LINKED) {
			if (!ft313->reclaim && (stopped ||
					time_after_eq(jiffies, qh->unlink_time)))
				start_unlink_async(ft313, qh);
			else
				action = TIMER_ASYNC_SHRINK;
		}
#endif
	}

	if (action == TIMER_ASYNC_SHRINK)
		timer_action (ft313, TIMER_ASYNC_SHRINK);

	DEBUG_MSG("Totally there are %d qh actually processed \n", qh_count);

	FUN_EXIT();
}

