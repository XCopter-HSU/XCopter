/*
 * FT313 HCD (Host Controller Driver) PCI Bus Glue.
 *
 * Copyright (C) 2011 Chang Yang <chang.yang@ftdichip.com>
 *
 * This code is *strongly* based on EHCI-HCD code by David Brownell since
 * the chip is a quasi-EHCI compatible.
 *
 * Licensed under GPL version 2 only.
 */

/* this file is part of ft313-hcd.c */

#ifndef CONFIG_PCI
#error "This file is PCI bus glue.  CONFIG_PCI must be defined."
#endif

#define PCI_VENDOR_ID_FTDI	0x1172
#define PCI_DEVICE_ID_FTDI_313	0xE001

#define FT313_CORE_OFFSET	0x200000

void* g_old_dma_mask = NULL;

/* Called during probe() after chip reset completes.
 */
static int ft313_pci_setup(struct usb_hcd *hcd)
{
	struct ft313_hcd *ft313 = hcd_to_ft313(hcd);
	int retval;

	ft313->cfg = hcd->regs + FT313_CONFIG_OFFSET;

	/* FixMe: */
	// Some HW init function like HW reset, set chip mode (16/8 bit)
	// Interrupt setting (edge trigger or level trigger etc.)
	// may needed here!
#ifdef FT313_IN_8_BIT_MODE
	// Dummy read to wakeup chip!
	ioread8(&ft313->cfg->sw_reset);
	mdelay(10);

	u8 temp;
	iowrite8(RESET_ALL, &ft313->cfg->sw_reset); // Reset FT313
	udelay(10);
	temp = ioread8(&ft313->cfg->sw_reset);
	temp |= DATA_BUS_WIDTH;
	temp &= ~(RESET_ATX | RESET_HC | RESET_ALL);
	iowrite8(temp, &ft313->cfg->sw_reset);

#else
	// Dummy read to wakeup chip!
	ft313_reg_read16(ft313, &ft313->cfg->sw_reset);
	mdelay(10);

	//iowrite16(RESET_ALL, &ft313->cfg->sw_reset);
	ft313_reg_write16(ft313, RESET_ALL, &ft313->cfg->sw_reset);
	mdelay(200);
	
	ft313_reg_read16(ft313, &ft313->cfg->sw_reset);
#endif
	u16 tmp;

	tmp = ft313_reg_read16(ft313, &ft313->cfg->hw_mode);
	ft313_reg_write16(ft313,
			  tmp | INTF_LOCK | INTR_EDGE | GLOBAL_INTR_EN,
			  &ft313->cfg->hw_mode); // Enable global interrupt
	ft313_reg_read16(ft313, &ft313->cfg->hw_mode);

	tmp = ft313_reg_read16(ft313, &ft313->cfg->config);
	//Turn VBUS on and set BCD mode
	
	DEBUG_MSG("bcd_mode is %s\n", bcd_mode);
	
	if (!strcmp(bcd_mode, "Disable")) {
		tmp &= ~BCD_EN; // Disable BCD
	} else if (!strcmp(bcd_mode, "Enable")) {
		tmp |= BCD_EN; // Enable BCD, actual mode setting by BCD Mode Pins
		tmp &= ~BCD_MODE_CTRL;
	} else if (!strcmp(bcd_mode, "SDP")) {
		tmp &= ~(3 << 13); // Clear bit [14:13]
		tmp |= (BCD_MODE_CTRL | BCD_MODE_SDP | BCD_EN);
	} else if (!strcmp(bcd_mode, "DCP")) {
		tmp &= ~(3 << 13); // Clear bit [14:13]
		tmp |= (BCD_MODE_CTRL | BCD_MODE_DCP | BCD_EN);
	} else if (!strcmp(bcd_mode, "CDP1")) {
		tmp &= ~(3 << 13); // Clear bit [14:13]
		tmp |= (BCD_MODE_CTRL | BCD_MODE_CDP1 | BCD_EN);
	} else if (!strcmp(bcd_mode, "CDP2")) {
		tmp &= ~(3 << 13); // Clear bit [14:13]
		tmp |= (BCD_MODE_CTRL | BCD_MODE_CDP2 | BCD_EN);
	}
	
	ft313_reg_write16(ft313, ~VBUS_OFF & tmp, &ft313->cfg->config);

	u32 temp32;
	temp32 = ft313_reg_read32(ft313, &ft313->cfg->chip_id);

	ft313->caps = hcd->regs + FT313_CAP_OFFSET;
	ft313->regs = hcd->regs + CAPLENGTH(ft313_reg_read32(ft313, &ft313->caps->hc_capbase));

	/* cache this readonly data; minimize chip reads */
	ft313->hcs_params = ft313_reg_read32(ft313, &ft313->caps->hcs_params);

	retval = ft313_halt(ft313);
	if (retval)
		return retval;

	/* data structure init */
	retval = ft313_init(hcd);
	hcd->has_tt = 1;  // host include transaction-translator, will change speed to FS/LS
	ft313->need_io_watchdog = 0;

	if (retval)
		return retval;

	retval = ft313_reset(ft313);

	if (retval)
		return retval;

	ft313->wakeup_wq_name = FT313_WK_NAME;
	ft313->wakeup_wq = create_singlethread_workqueue(FT313_WK_NAME);
	if (ft313->wakeup_wq == NULL) {
		ERROR_MSG("FT313 Wakeup Workqueue creation failed\n");
		return -ENOMEM;
	}
	INIT_WORK(&ft313->wakeup_work, ft313_wakeup_wq_handler);

#if 0 // Move to ft313_run to avoid possible racing condtion
	// Register a charater device
	ft313->ft313_cdev_count = 1;
	retval = alloc_chrdev_region(&ft313->ft313_cdev_major, 0, ft313->ft313_cdev_count, "ft313_hc");

	if (retval)
		return retval;
	int devno = MKDEV(ft313->ft313_cdev_major, 0);

	cdev_init(&ft313->ft313_cdev, &ft313_fops);
	ft313->ft313_cdev.owner = THIS_MODULE;
	retval = cdev_add(&ft313->ft313_cdev, devno, ft313->ft313_cdev_count);

	if (retval) {
		printk("Char device register fails\n");
		return retval;
	}
#endif
	return 0;
}

static int ft313_pci_probe(struct pci_dev *dev,
					const struct pci_device_id *id)
{
	struct hc_driver	*driver;
	struct usb_hcd		*hcd;
	void *pci_iobase;
	int retval;

	ALERT_MSG("ft313 driver init start\n");

	if (usb_disabled())
		return -ENODEV;

	if (!id)
		return -EINVAL;

	driver = (struct hc_driver *)id->driver_data;

	if (!driver)
		return -EINVAL;

	if (pci_enable_device(dev))
		return -ENODEV;
	dev->current_state = PCI_D0;

#ifdef PCI_ENABLE_MSI
	// Enable message signed interrupts
	if (pci_enable_msi(dev))
	{
		dev->msi_enabled = 0;
		printk("Could not enable MSI interrupt \n");
	}
	else
	{
		dev->msi_enabled = 1;
		printk("Enable MSI interrupt. \n");
	}
#endif

	if (!dev->irq) {
		dev_err(&dev->dev,
			"Found HC with no IRQ.  Check BIOS/PCI %s setup!\n",
			pci_name(dev));
		retval = -ENODEV;
		goto disable_pci;
	}

//	pci_set_dma_max_seg_size(dev, 0);
//	pci_clear_master(dev);
//	if (0 != dma_set_mask(&dev->dev, DMA_BIT_MASK(0))) {
//		printk(KERN_ERR "Disable DMA fail\n");
//		retval = -ENOMEM;
//		goto disable_pci;
//	}

	hcd = usb_create_hcd(driver, &dev->dev, pci_name(dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto disable_pci;
	}

	DEBUG_MSG("FT313 bus name is %s with HCD is 0x%X\n", pci_name(dev), hcd);

	DEBUG_MSG("Use DMA is %d\n", hcd->self.uses_dma);

	hcd->rsrc_start = pci_resource_start(dev, 0);
	hcd->rsrc_len = pci_resource_len(dev, 0);
	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
			driver->description)) {
		dev_dbg(&dev->dev, "controller already in use\n");
		retval = -EBUSY;
		goto out_disable;
	}

	pci_iobase = ioremap_nocache(hcd->rsrc_start, hcd->rsrc_len);
	if (pci_iobase == NULL) {
		dev_dbg(&dev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto release_mem_region;
	}

	hcd->regs = pci_iobase + FT313_CORE_OFFSET;
	hcd->irq = dev->irq;
	hcd->state = HC_STATE_HALT;

	g_regbase = pci_iobase + SRAM_MEM_OFFSET0;
	g_print_cnt = 0;

	pci_set_master(dev);

	// FixMe: Disable DMA
//	hcd->self.uses_dma = 0;
//	hcd->self.controller->dma_mask = NULL;
	retval = usb_add_hcd(hcd, dev->irq, IRQF_DISABLED | IRQF_SHARED); // Call reset() and start() here!
	if (retval != 0)
		goto unmap_registers;

	// FixMe: Disable DMA
	hcd->self.uses_dma = 0;
	g_old_dma_mask = hcd->self.controller->dma_mask;
	hcd->self.controller->dma_mask = NULL;

	//dev->dev.dma_mask = NULL;

	pci_set_drvdata(dev, hcd);
	DEBUG_MSG("ft313 driver init complete hcd->state is 0x%X\n", hcd->state);

	return retval;

unmap_registers:
	iounmap(pci_iobase);

#ifdef PCI_ENABLE_MSI
	pci_disable_msi(dev);
	dev->msi_enabled = 0;
#endif

release_mem_region:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
out_disable:
	usb_put_hcd(hcd);
disable_pci:
	ALERT_MSG("ft313 init failed, ft313 device disabled\n");
	pci_disable_device(dev);

	return retval;
}

static void ft313_pci_remove(struct pci_dev *dev)
{
	struct usb_hcd *hcd;

	hcd = pci_get_drvdata(dev);
	if (!hcd)
		return;

	hcd->self.controller->dma_mask = g_old_dma_mask;
	hcd->self.uses_dma = 1;

	usb_remove_hcd(hcd);

	iounmap(hcd->regs - FT313_CORE_OFFSET);

#ifdef PCI_ENABLE_MSI
	if (dev->msi_enabled)
	{
		pci_disable_msi(dev);
		dev->msi_enabled = 0;
	}
#endif
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

	usb_put_hcd(hcd);

	pci_disable_device(dev);
}

void ft313_pci_shutdown(struct pci_dev *dev)
{
	struct usb_hcd		*hcd;

	hcd = pci_get_drvdata(dev);
	if (!hcd)
		return;

	if (test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags) &&
			hcd->driver->shutdown) {
		hcd->driver->shutdown(hcd);
		pci_disable_device(dev);
	}
}

static int ft313_update_device(struct usb_hcd *hcd, struct usb_device *udev)
{
//	struct ft313_hcd *ft313 = hcd_to_ft313(hcd);
	int rc = 0;

	if (!udev->parent) /* udev is root hub itself, impossible */
		rc = -1;
	/* we only support lpm device connected to root hub yet */
//	if (ehci->has_lpm && !udev->parent->parent) {
//		rc = ehci_lpm_set_da(ehci, udev->devnum, udev->portnum);
//		if (!rc)
//			rc = ehci_lpm_check(ehci, udev->portnum);
//	}
	return rc;
}




static const struct hc_driver ft313_pci_hc_driver = {
	.description =		"ft313-hcd",
	.product_desc =		"FT313 SPH Controller",
	.hcd_priv_size =	sizeof(struct ft313_hcd),

	/*
	 * Generic hardware linkage
	 */
	.irq =			ft313_irq,
//	.flags =		HCD_MEMORY | HCD_LOCAL_MEM | HCD_USB2,
	.flags =		HCD_MEMORY | HCD_USB2,

	/*
	 * Basic lifecycle operations
	 */
	.reset =		ft313_pci_setup,
	.start =		ft313_run,
	.stop =			ft313_stop,
	.shutdown =		ft313_shutdown,

	/*
	 * Managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ft313_urb_enqueue,
	.urb_dequeue =		ft313_urb_dequeue,
	.endpoint_disable =	ft313_endpoint_disable,
	.endpoint_reset =	ft313_endpoint_reset,

	/*
	 * Scheduling support
	 */
	.get_frame_number =	ft313_get_frame,

	/*
	 * Root hub support
	 */
	.hub_status_data =	ft313_hub_status_data,
	.hub_control =		ft313_hub_control,
	.bus_suspend =		ft313_bus_suspend,
	.bus_resume =		ft313_bus_resume,
	.relinquish_port =      ft313_relinquish_port,
	.port_handed_over =     ft313_port_handed_over,

	/*
	 * call back when device connected and addressed
	 */
	.update_device =	ft313_update_device,

	.clear_tt_buffer_complete	= ft313_clear_tt_buffer_complete,
};

static struct pci_device_id ft313_pci_ids[] __devinitdata = {
	{
		.vendor =	PCI_VENDOR_ID_FTDI,
		.device =	PCI_DEVICE_ID_FTDI_313,
		.subvendor =	0xA106,
		.subdevice =	0x1140,
		.driver_data =	(unsigned long) &ft313_pci_hc_driver,
	},
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ft313_pci_ids);

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver ft313_pci_driver = {
	.name = "ft313-pci",
	.id_table = ft313_pci_ids,
	.probe = ft313_pci_probe,
	.remove = ft313_pci_remove,
	.shutdown = ft313_pci_shutdown
};

