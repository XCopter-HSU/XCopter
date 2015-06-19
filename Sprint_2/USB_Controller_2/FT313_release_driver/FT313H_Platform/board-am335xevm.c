/*
 * Code for AM335X EVM.
 *
 * Copyright (C) 2011 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/input/matrix_keypad.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wl12xx.h>
#include <linux/ethtool.h>
#include <linux/mfd/tps65910.h>
#include <linux/pwm_backlight.h>
#include <linux/pwm/pwm.h>

/* LCD controller is similar to DA850 */
#include <video/da8xx-fb.h>

#include <mach/hardware.h>
#include <mach/board-am335xevm.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/asp.h>

#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/lcdc.h>
#include <plat/usb.h>
#include <plat/mmc.h>

#include "board-flash.h"
#include "cpuidle33xx.h"
#include "mux.h"
#include "devices.h"
#include "hsmmc.h"

/* TLK PHY IDs */
#define TLK110_PHY_ID		0x2000A201
#define TLK110_PHY_MASK		0xfffffff0

/* BBB PHY IDs */
#define BBB_PHY_ID		0x7c0f1
#define BBB_PHY_MASK		0xfffffffe

/* TLK110 PHY register offsets */
#define TLK110_COARSEGAIN_REG	0x00A3
#define TLK110_LPFHPF_REG	0x00AC
#define TLK110_SPAREANALOG_REG	0x00B9
#define TLK110_VRCR_REG		0x00D0
#define TLK110_SETFFE_REG	0x0107
#define TLK110_FTSP_REG		0x0154
#define TLK110_ALFATPIDL_REG	0x002A
#define TLK110_PSCOEF21_REG	0x0096
#define TLK110_PSCOEF3_REG	0x0097
#define TLK110_ALFAFACTOR1_REG	0x002C
#define TLK110_ALFAFACTOR2_REG	0x0023
#define TLK110_CFGPS_REG	0x0095
#define TLK110_FTSPTXGAIN_REG	0x0150
#define TLK110_SWSCR3_REG	0x000B
#define TLK110_SCFALLBACK_REG	0x0040
#define TLK110_PHYRCR_REG	0x001F

/* TLK110 register writes values */
#define TLK110_COARSEGAIN_VAL	0x0000
#define TLK110_LPFHPF_VAL	0x8000
#define TLK110_SPANALOG_VAL	0x0000
#define TLK110_VRCR_VAL		0x0008
#define TLK110_SETFFE_VAL	0x0605
#define TLK110_FTSP_VAL		0x0255
#define TLK110_ALFATPIDL_VAL	0x7998
#define TLK110_PSCOEF21_VAL	0x3A20
#define TLK110_PSCOEF3_VAL	0x003F
#define TLK110_ALFACTOR1_VAL	0xFF80
#define TLK110_ALFACTOR2_VAL	0x021C
#define TLK110_CFGPS_VAL	0x0000
#define TLK110_FTSPTXGAIN_VAL	0x6A88
#define TLK110_SWSCR3_VAL	0x0000
#define TLK110_SCFALLBACK_VAL	0xC11D
#define TLK110_PHYRCR_VAL	0x4000

#ifdef CONFIG_TLK110_WORKAROUND
#define am335x_tlk110_phy_init()\
	do {	\
		phy_register_fixup_for_uid(TLK110_PHY_ID,\
					TLK110_PHY_MASK,\
					am335x_tlk110_phy_fixup);\
	} while (0);
#else
#define am335x_tlk110_phy_init() do { } while (0);
#endif

#define DBG        0

#if DBG
#define pr_dbg(x...)\
        do {    \
                 printk(x);\
         } while (0);
#else
#define pr_dbg(x...)   do { } while(0);
#endif

/* Convert GPIO signal to GPIO pin number */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

/* LCD backlight platform Data */
#define AM335X_BACKLIGHT_MAX_BRIGHTNESS        100
#define AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS    50
#define AM335X_PWM_PERIOD_NANO_SECONDS        (1000000 * 5)

/*#define LCD_PWM_DEVICE_ID   "ehrpwm.1:0"*/
#define LCD_PWM_DEVICE_ID   "ecap.2"

static struct platform_pwm_backlight_data lcd_backlight_data = {
        .pwm_id         = LCD_PWM_DEVICE_ID,
        .ch             = -1,
        .max_brightness = AM335X_BACKLIGHT_MAX_BRIGHTNESS,
        .dft_brightness = AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS,
        .pwm_period_ns  = AM335X_PWM_PERIOD_NANO_SECONDS,
};

static const struct display_panel disp_panel = {
	WVGA,
	32,
	16,
	COLOR_ACTIVE,
};

static struct lcd_ctrl_config lcd_cfg = {
	&disp_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data devkit8600_lcd_pdata = {
	.manu_name		= "InnoLux",
	.controller_data	= &lcd_cfg,
#if defined(CONFIG_LCD_43inch)
	.type			= "AT043TN24",
#elif defined(CONFIG_LCD_7inch)
	.type			= "AT070TN83",
#elif defined(CONFIG_VGA)
	.type			= "VGA",
#elif defined(CONFIG_LVDS)
        .type                   = "LVDS",
#endif
};

/* TSc controller */
#include <linux/input/ti_tscadc.h>
#include <linux/lis3lv02d.h>

static struct resource tsc_resources[]  = {
	[0] = {
		.start  = AM33XX_TSC_BASE,
		.end    = AM33XX_TSC_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = AM33XX_IRQ_ADC_GEN,
		.end    = AM33XX_IRQ_ADC_GEN,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct tsc_data am335x_touchscreen_data  = {
	.wires  = 4,
	.x_plate_resistance = 200,
};

static struct platform_device tsc_device = {
	.name   = "tsc",
	.id     = -1,
	.dev    = {
			.platform_data  = &am335x_touchscreen_data,
	},
	.num_resources  = ARRAY_SIZE(tsc_resources),
	.resource       = tsc_resources,
};

static u8 am335x_iis_serializer_direction0[] = {
	RX_MODE,	TX_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static u8 am335x_iis_serializer_direction1[] = {
	INACTIVE_MODE,	INACTIVE_MODE,	TX_MODE,	RX_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static struct snd_platform_data am335x_evm_snd_data0 = {
	.tx_dma_offset	= 0x46000000,	/* McASP0 */
	.rx_dma_offset	= 0x46000000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction0),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction0,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

static struct snd_platform_data am335x_evm_snd_data1 = {
	.tx_dma_offset	= 0x46400000,	/* McASP1 */
	.rx_dma_offset	= 0x46400000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction1),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction1,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

static struct omap2_hsmmc_info am335x_mmc[] __initdata = {
	{
		.mmc            = 1,
		.caps           = MMC_CAP_4_BIT_DATA,
		.gpio_cd        = GPIO_TO_PIN(3, 19),
		.gpio_wp        = /*GPIO_TO_PIN(3, 18)*/-EINVAL,
		.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34, /* 3V3 */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{}      /* Terminator */
};


#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	AM33XX_MUX(I2C0_SDA, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	AM33XX_MUX(I2C0_SCL, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define	board_mux	NULL
#endif

/* module pin mux structure */
struct pinmux_config {
	const char *string_name; /* signal name format */
	int val; /* Options for the mux register value */
};

struct evm_dev_cfg {
	void (*device_init)(int evm_id, int profile);

/*
* If the device is required on both baseboard & daughter board (ex i2c),
* specify DEV_ON_BASEBOARD
*/
#define DEV_ON_BASEBOARD	0
#define DEV_ON_DGHTR_BRD	1
	u32 device_on;

	u32 profile;	/* Profiles (0-7) in which the module is present */
};

/* AM335X - CPLD Register Offsets */
#define	CPLD_DEVICE_HDR	0x00 /* CPLD Header */
#define	CPLD_DEVICE_ID	0x04 /* CPLD identification */
#define	CPLD_DEVICE_REV	0x0C /* Revision of the CPLD code */
#define	CPLD_CFG_REG	0x10 /* Configuration Register */

static struct i2c_client *cpld_client;

static struct omap_board_config_kernel am335x_evm_config[] __initdata = {
};

/*
* EVM Config held in On-Board eeprom device.
*
* Header Format
*
*  Name			Size	Contents
*			(Bytes)
*-------------------------------------------------------------
*  Header		4	0xAA, 0x55, 0x33, 0xEE
*
*  Board Name		8	Name for board in ASCII.
*				example "A33515BB" = "AM335X
				Low Cost EVM board"
*
*  Version		4	Hardware version code for board in
*				in ASCII. "1.0A" = rev.01.0A
*
*  Serial Number	12	Serial number of the board. This is a 12
*				character string which is WWYY4P16nnnn, where
*				WW = 2 digit week of the year of production
*				YY = 2 digit year of production
*				nnnn = incrementing board number
*
*  Configuration option	32	Codes(TBD) to show the configuration
*				setup on this board.
*
*  Available		32720	Available space for other non-volatile
*				data.
*/
struct am335x_evm_eeprom_config {
	u32	header;
	u8	name[8];
	char	version[4];
	u8	serial[12];
	u8	opt[32];
};

static struct am335x_evm_eeprom_config config;
static bool daughter_brd_detected;

#define GP_EVM_REV_IS_1_0		0x1
#define GP_EVM_REV_IS_1_1A		0x2
#define GP_EVM_REV_IS_UNKNOWN		0xFF
static unsigned int gp_evm_revision = GP_EVM_REV_IS_UNKNOWN;
unsigned int gigabit_enable = 1;

#define EEPROM_MAC_ADDRESS_OFFSET	60 /* 4+8+4+12+32 */
#define EEPROM_NO_OF_MAC_ADDR		3
static char am335x_mac_addr[EEPROM_NO_OF_MAC_ADDR][ETH_ALEN];

#define AM335X_EEPROM_HEADER		0xEE3355AA

/* current profile if exists else PROFILE_0 on error */
static u32 am335x_get_profile_selection(void)
{
	int val = 0;

	if (!cpld_client)
		/* error checking is not done in func's calling this routine.
		so return profile 0 on error */
		return 0;

	val = i2c_smbus_read_word_data(cpld_client, CPLD_CFG_REG);
	if (val < 0)
		return 0;	/* default to Profile 0 on Error */
	else
		return val & 0x7;
}

/* Module pin mux for LCDC */
static struct pinmux_config lcdc_pin_mux[] = {
	{"lcd_data0.lcd_data0",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data1.lcd_data1",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data2.lcd_data2",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data3.lcd_data3",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data4.lcd_data4",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data5.lcd_data5",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data6.lcd_data6",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data7.lcd_data7",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data8.lcd_data8",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data9.lcd_data9",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data10.lcd_data10",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data11.lcd_data11",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data12.lcd_data12",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data13.lcd_data13",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data14.lcd_data14",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data15.lcd_data15",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
#if 0
	{"gpmc_ad8.lcd_data16",		OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad9.lcd_data17",		OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad10.lcd_data18",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad11.lcd_data19",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad12.lcd_data20",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad13.lcd_data21",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad14.lcd_data22",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad15.lcd_data23",	OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
#endif
	{"lcd_vsync.lcd_vsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_hsync.lcd_hsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_pclk.lcd_pclk",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_ac_bias_en.lcd_ac_bias_en", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static struct pinmux_config tsc_pin_mux[] = {
	{"ain0.ain0",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain1.ain1",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain2.ain2",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain3.ain3",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefp.vrefp",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefn.vrefn",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Pin mux for nand flash module */
static struct pinmux_config nand_pin_mux[] = {
	{"gpmc_ad0.gpmc_ad0",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.gpmc_ad1",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.gpmc_ad2",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.gpmc_ad3",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.gpmc_ad4",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.gpmc_ad5",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.gpmc_ad6",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad7.gpmc_ad7",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wait0.gpmc_wait0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wpn.gpmc_wpn",	  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn0.gpmc_csn0",	  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_advn_ale.gpmc_advn_ale",  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_oen_ren.gpmc_oen_ren",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_wen.gpmc_wen",     OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_ben0_cle.gpmc_ben0_cle",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{NULL, 0},
};

/* Module pin mux for SPI fash */
static struct pinmux_config spi0_pin_mux[] = {
	{"spi0_sclk.spi0_sclk", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_d0.spi0_d0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{"spi0_d1.spi0_d1", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_cs0.spi0_cs0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{NULL, 0},
};

#if 0
/* Module pin mux for SPI flash */
static struct pinmux_config spi1_pin_mux[] = {
	{"mcasp0_aclkx.spi1_sclk", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_fsx.spi1_d0", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_PULL_UP | AM33XX_INPUT_EN},
	{"mcasp0_axr0.spi1_d1", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_ahclkr.spi1_cs0", OMAP_MUX_MODE3 | AM33XX_PULL_ENBL
		| AM33XX_PULL_UP | AM33XX_INPUT_EN},
	{NULL, 0},
};
#endif

/* Module pin mux for rgmii1 */
static struct pinmux_config rgmii1_pin_mux[] = {
	{"mii1_txen.rgmii1_tctl", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.rgmii1_rctl", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.rgmii1_td3", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd2.rgmii1_td2", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.rgmii1_td1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.rgmii1_td0", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_txclk.rgmii1_tclk", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"mii1_rxclk.rgmii1_rclk", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.rgmii1_rd3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.rgmii1_rd2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.rgmii1_rd1", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.rgmii1_rd0", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for rgmii2 */
static struct pinmux_config rgmii2_pin_mux[] = {
	{"gpmc_a0.rgmii2_tctl", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a1.rgmii2_rctl", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a2.rgmii2_td3", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a3.rgmii2_td2", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a4.rgmii2_td1", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a5.rgmii2_td0", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a6.rgmii2_tclk", OMAP_MUX_MODE2 | AM33XX_PIN_OUTPUT},
	{"gpmc_a7.rgmii2_rclk", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a8.rgmii2_rd3", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a9.rgmii2_rd2", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a10.rgmii2_rd1", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"gpmc_a11.rgmii2_rd0", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for mii1 */
static struct pinmux_config mii1_pin_mux[] = {
	{"mii1_rxerr.mii1_rxerr", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.mii1_txen", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.mii1_rxdv", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.mii1_txd3", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd2.mii1_txd2", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.mii1_txd1", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.mii1_txd0", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txclk.mii1_txclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxclk.mii1_rxclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.mii1_rxd3", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.mii1_rxd2", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.mii1_rxd1", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.mii1_rxd0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for rmii1 */
static struct pinmux_config rmii1_pin_mux[] = {
	{"mii1_crs.rmii1_crs_dv", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxerr.mii1_rxerr", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.mii1_txen", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.mii1_txd1", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.mii1_txd0", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_rxd1.mii1_rxd1", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.mii1_rxd0", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"rmii1_refclk.rmii1_refclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config i2c0_pin_mux[] = {
        {"i2c0_sda.i2c0_sda",    OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
                                        AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
        {"i2c0_scl.i2c0_scl",   OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
                                        AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
        {NULL, 0},
};

static struct pinmux_config i2c1_pin_mux[] = {
	{"spi0_d1.i2c1_sda",    OMAP_MUX_MODE2 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
	{"spi0_cs0.i2c1_scl",   OMAP_MUX_MODE2 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Module pin mux for mcasp0 */
static struct pinmux_config mcasp0_pin_mux[] = {
	{"mcasp0_aclkx.mcasp0_aclkx", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_fsx.mcasp0_fsx", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_axr0.mcasp0_axr0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_axr1.mcasp0_axr1", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mcasp0_ahclkx.mcasp0_ahclkx", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* Module pin mux for mcasp1 */
static struct pinmux_config mcasp1_pin_mux[] = {
	{"mii1_crs.mcasp1_aclkx", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxerr.mcasp1_fsx", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_col.mcasp1_axr2", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{"rmii1_refclk.mcasp1_axr3", OMAP_MUX_MODE4 |
						AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};


/* Module pin mux for mmc0 */
static struct pinmux_config mmc0_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config mmc0_no_cd_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
//	{"mcasp0_aclkr.mmc0_sdwp", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};

/* Module pin mux for mmc1 */
static struct pinmux_config mmc1_pin_mux[] = {
	{"gpmc_ad7.mmc1_dat7",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.mmc1_dat6",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.mmc1_dat5",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.mmc1_dat4",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.mmc1_dat3",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.mmc1_dat2",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.mmc1_dat1",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad0.mmc1_dat0",	OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn1.mmc1_clk",	OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn2.mmc1_cmd",	OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn0.mmc1_sdwp",	OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_advn_ale.mmc1_sdcd", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

#if 0
/* Module pin mux for uart3 */
static struct pinmux_config uart3_pin_mux[] = {
	{"spi0_cs1.uart3_rxd", AM33XX_PIN_INPUT_PULLUP},
	{"ecap0_in_pwm0_out.uart3_txd", AM33XX_PULL_ENBL},
	{NULL, 0},
};

static struct pinmux_config d_can_gp_pin_mux[] = {
	{"uart0_ctsn.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
	{"uart0_rtsn.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};


static struct pinmux_config d_can_ia_pin_mux[] = {
	{"uart0_rxd.d_can0_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
	{"uart0_txd.d_can0_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};
#else
static struct pinmux_config d_can_pin_mux[] = {
        {"uart0_ctsn.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
        {"uart0_rtsn.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
        {NULL, 0},
};
#endif

/*
* @pin_mux - single module pin-mux structure which defines pin-mux
*			details for all its pins.
*/
static void setup_pin_mux(struct pinmux_config *pin_mux)
{
	int i;

	for (i = 0; pin_mux->string_name != NULL; pin_mux++) {
		omap_mux_init_signal(pin_mux->string_name, pin_mux->val);

	}

}

/* Matrix GPIO Keypad Support for profile-0 only: TODO */

#if 0
/* pinmux for keypad device */
static struct pinmux_config matrix_keypad_pin_mux[] = {
	{"gpmc_a5.gpio1_21",  OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"gpmc_a8.gpio1_22",  OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"gpmc_a9.gpio1_25",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_a10.gpio1_26", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_a11.gpio1_27", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};

/* Keys mapping */
static const uint32_t am335x_evm_matrix_keys[] = {
	KEY(0, 0, KEY_MENU),
	KEY(1, 0, KEY_BACK),
	KEY(2, 0, KEY_LEFT),

	KEY(0, 1, KEY_RIGHT),
	KEY(1, 1, KEY_ENTER),
	KEY(2, 1, KEY_DOWN),
};

const struct matrix_keymap_data am335x_evm_keymap_data = {
	.keymap      = am335x_evm_matrix_keys,
	.keymap_size = ARRAY_SIZE(am335x_evm_matrix_keys),
};

static const unsigned int am335x_evm_keypad_row_gpios[] = {
	GPIO_TO_PIN(1, 25), GPIO_TO_PIN(1, 26), GPIO_TO_PIN(1, 27)
};

static const unsigned int am335x_evm_keypad_col_gpios[] = {
	GPIO_TO_PIN(1, 21), GPIO_TO_PIN(1, 22)
};

static struct matrix_keypad_platform_data am335x_evm_keypad_platform_data = {
	.keymap_data       = &am335x_evm_keymap_data,
	.row_gpios         = am335x_evm_keypad_row_gpios,
	.num_row_gpios     = ARRAY_SIZE(am335x_evm_keypad_row_gpios),
	.col_gpios         = am335x_evm_keypad_col_gpios,
	.num_col_gpios     = ARRAY_SIZE(am335x_evm_keypad_col_gpios),
	.active_low        = false,
	.debounce_ms       = 5,
	.col_scan_delay_us = 2,
};

static struct platform_device am335x_evm_keyboard = {
	.name  = "matrix-keypad",
	.id    = -1,
	.dev   = {
		.platform_data = &am335x_evm_keypad_platform_data,
	},
};

static void matrix_keypad_init(int evm_id, int profile)
{
	int err;

	setup_pin_mux(matrix_keypad_pin_mux);
	err = platform_device_register(&am335x_evm_keyboard);
	if (err) {
		pr_err("failed to register matrix keypad (2x3) device\n");
	}
}
#endif

/* pinmux for led device */
static struct pinmux_config gpio_led_pin_mux[] = {
        {"gpmc_a10.gpio1_26",  OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
        {"gpmc_a11.gpio1_27",    OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
        {NULL, 0},
};

/* pinmux for keypad device */
static struct pinmux_config gpio_keys_pin_mux[] = {
	{"gpmc_csn1.gpio1_30",  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_csn2.gpio1_31",    OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_ad8.gpio0_22",	OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
};

static struct gpio_led gpio_leds[] = {
        {
                .name                   = "sys_led",
                .default_trigger        = "heartbeat",
                .gpio                   = GPIO_TO_PIN(1, 26),
        },
        {
                .name                   = "user_led",
                .gpio                   = GPIO_TO_PIN(1, 27),
        },
};

static struct gpio_led_platform_data gpio_led_info = {
        .leds           = gpio_leds,
        .num_leds       = ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
        .name   = "leds-gpio",
        .id     = -1,
        .dev    = {
                .platform_data  = &gpio_led_info,
        },
};

static void gpio_led_init(int evm_id, int profile)
{
        int err;

	pr_dbg("--------gpio_led_init\n");

        setup_pin_mux(gpio_led_pin_mux);
        err = platform_device_register(&leds_gpio);
        if (err)
                pr_err("failed to register gpio led device\n");
}

/* Configure GPIOs for Volume Keys */
static struct gpio_keys_button gpio_key_buttons[] = {
	{
		.code                   = KEY_F1,
		.gpio                   = GPIO_TO_PIN(1, 30),
		.active_low             = true,
		.desc                   = "menu",
		.type                   = EV_KEY,
//		.wakeup                 = 1,
	},
	{
		.code                   = KEY_ESC,
		.gpio                   = GPIO_TO_PIN(1, 31),
		.active_low             = true,
		.desc                   = "back",
		.type                   = EV_KEY,
//		.wakeup                 = 1,
	},
	{
		.code                   = KEY_HOME,
		.gpio                   = GPIO_TO_PIN(0, 22),
		.active_low             = true,
		.desc                   = "home",
		.type                   = EV_KEY,
//		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data gpio_key_info = {
	.buttons        = gpio_key_buttons,
	.nbuttons       = ARRAY_SIZE(gpio_key_buttons),
};

static struct platform_device gpio_keys = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &gpio_key_info,
	},
};

static void gpio_keys_init(int evm_id, int profile)
{
	int err;

	pr_dbg("--------gpio_keys_init\n");

	setup_pin_mux(gpio_keys_pin_mux);
	err = platform_device_register(&gpio_keys);
	if (err)
		pr_err("failed to register gpio key device\n");
}

#ifdef CONFIG_USB_FT313H_HCD
// START of FT313H Platform Device Code
#include <plat/gpmc.h>
/* Pin mux for ft313 hc module through GPMC, CS 1 */
static struct pinmux_config ft313_pin_mux[] = {
#if 0
	{"gpmc_ad0.gpmc_ad0",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.gpmc_ad1",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.gpmc_ad2",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.gpmc_ad3",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.gpmc_ad4",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.gpmc_ad5",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.gpmc_ad6",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad7.gpmc_ad7",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
#endif
	{"gpmc_a0.gpmc_a0",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a1.gpmc_a1",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a2.gpmc_a2",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a3.gpmc_a3",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a4.gpmc_a4",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a5.gpmc_a5",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a6.gpmc_a6",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{"gpmc_a7.gpmc_a7",	  OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},

	{"gpmc_csn1.gpmc_csn1",	  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},		// CS

	{"gpmc_ben1.gpmc_ben1",	  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},	// Interrupt

	{NULL, 0},
};

//#define FT313_USE_IRQ
//#undef  FT313_USE_IRQ

static struct resource ft313_hc_resources[] = {
	[0] = {
		.flags		= IORESOURCE_MEM,
	},
#ifdef FT313_USE_IRQ
	[1] = {
		.flags		= IORESOURCE_IRQ,
	},
#endif
};

struct ft313_hc_platform_data {
	int	id;
	struct	gpmc_timings *gpmc_t;
	int	cs;
	int	gpio_irq;
	int	irq;
//	int	gpio_reset;
	u32	flags;
};

static struct gpmc_timings ft313_gpmc_timing = {
	.sync_clk = 0,

	.cs_on = 0,
	.cs_rd_off = 100,
	.cs_wr_off = 100,

	.adv_on = 6,
	.adv_rd_off = 80,
	.adv_wr_off = 80,

	.we_off = 80,
	.oe_off = 80,

	.access = 80,
	.rd_cycle = 160,
	.wr_cycle = 160,

	.wr_access = 80,
	.wr_data_mux_bus = 20,
};

static struct ft313_hc_platform_data ft313_hc_info = {
	.cs             = 1,
	.gpmc_t		= &ft313_gpmc_timing,
};

static struct platform_device ft313_hc_device = {
	.name   = "ft313",
	.id     = -1,
	.dev    = {
		.dma_mask = ~0,
		.coherent_dma_mask = ~0,
		.platform_data  = &ft313_hc_info,
	},
	.num_resources	= ARRAY_SIZE(ft313_hc_resources),
	.resource	= ft313_hc_resources,
};

static int ft313_gpmc_retime(struct ft313_hc_platform_data *gpmc_cfg_data)
{
	struct gpmc_timings t;
	int err;

	if (!gpmc_cfg_data->gpmc_t)
		return 0;

	memset(&t, 0, sizeof(t));
	t.sync_clk = gpmc_cfg_data->gpmc_t->sync_clk;
	t.cs_on = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->cs_on);
	t.adv_on = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->adv_on);

	/* Read */
	t.adv_rd_off = gpmc_round_ns_to_ticks(
			       gpmc_cfg_data->gpmc_t->adv_rd_off);
	t.oe_on  = t.adv_on;
	t.access = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->access);
	t.oe_off = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->oe_off);
	t.cs_rd_off = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->cs_rd_off);
	t.rd_cycle  = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->rd_cycle);

	/* Write */
	t.adv_wr_off = gpmc_round_ns_to_ticks(
			       gpmc_cfg_data->gpmc_t->adv_wr_off);
	t.we_on  = t.oe_on;
	if (cpu_is_omap34xx()) {
		t.wr_data_mux_bus =	gpmc_round_ns_to_ticks(
						gpmc_cfg_data->gpmc_t->wr_data_mux_bus);
		t.wr_access = gpmc_round_ns_to_ticks(
				      gpmc_cfg_data->gpmc_t->wr_access);
	}
	t.we_off = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->we_off);
	t.cs_wr_off = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->cs_wr_off);
	t.wr_cycle  = gpmc_round_ns_to_ticks(gpmc_cfg_data->gpmc_t->wr_cycle);

	/* Configure GPMC */
	// Configure CONFIG1
	gpmc_cs_write_reg(gpmc_cfg_data->cs, GPMC_CS_CONFIG1,
			    GPMC_CONFIG1_READTYPE_ASYNC
			  | GPMC_CONFIG1_WRITETYPE_ASYNC
			  | GPMC_CONFIG1_FCLK_DIV2
			  | GPMC_CONFIG1_DEVICETYPE_NOR);


	// Set as 8 bit mode
//	gpmc_cs_configure(gpmc_cfg_data->cs, GPMC_CONFIG_DEV_SIZE, 0);

	// set as NOR mode
//	gpmc_cs_configure(gpmc_cfg_data->cs,
//			  GPMC_CONFIG_DEV_TYPE, GPMC_DEVICETYPE_NOR);
	err = gpmc_cs_set_timings(gpmc_cfg_data->cs, &t);
	if (err)
		return err;

	return 0;
}

int __init gpmc_ft313_init(struct ft313_hc_platform_data *board_data)
{
	int err = 0;
	unsigned long cs_mem_base;
	struct ft313_hc_platform_data *gpmc_cfg;
	u32 reg7;

	gpmc_cfg = board_data;

	pr_err("FT313 request CS %d \n", gpmc_cfg->cs);
	err = gpmc_cs_request(gpmc_cfg->cs, 0x100,
			      &cs_mem_base);
	if (err < 0) {
		printk("Cannot request GPMC CS\n");
		return err;
	}

	pr_err("FT313H base address is 0x%X\n", cs_mem_base);

	reg7 = gpmc_cs_read_reg(gpmc_cfg->cs, GPMC_CS_CONFIG7);

	pr_err("GPMC cs %d's Config7 Reg is 0x%X\n", gpmc_cfg->cs, reg7);

	ft313_hc_resources[0].start = cs_mem_base + 0x0;
	ft313_hc_resources[0].end = cs_mem_base + 0xff;

#ifdef FT313_USE_IRQ
	gpmc_cfg->gpio_irq = GPIO_TO_PIN(1, 28);
	if (err = gpio_request_one(gpmc_cfg->gpio_irq, GPIOF_IN, "ft313 irq")) {
		pr_err("Failed to request IRQ GPIO%d\n", gpmc_cfg->gpio_irq);
		goto out_free_cs;
	}

	ft313_hc_resources[1].start = gpio_to_irq(gpmc_cfg->gpio_irq);
#endif
	/* Set timings in GPMC */
	err = ft313_gpmc_retime(gpmc_cfg);
	if (err < 0) {
		printk("Unable to set gpmc timings: %d\n", err);
		goto out_free_cs;
	}

	// Register platform device
	err = platform_device_register(&ft313_hc_device);
	if (err < 0) {
		printk("Unable to register FT313 device\n");
		goto out_free_cs;
	}

#ifndef FT313_USE_IRQ
	gpmc_cfg->gpio_irq = GPIO_TO_PIN(1, 28);
	if (err = gpio_request_one(gpmc_cfg->gpio_irq, GPIOF_IN, "ft313 irq")) {
		pr_err("Failed to request IRQ GPIO%d\n", gpmc_cfg->gpio_irq);
		goto out_free_cs;
	}

	gpmc_cfg->irq = gpio_to_irq(gpmc_cfg->gpio_irq);
	pr_err("FT313 use irq %d \n", gpmc_cfg->irq);
#endif

#if 0
	int i;
	pr_err("Print all GPMC Config 7 registers\n");
	for (i = 0; i < 8; i++) {
		reg7 = gpmc_cs_read_reg(i, GPMC_CS_CONFIG7);
		pr_err("GPMC cs %d's Config7 Reg is 0x%X\n", i, reg7);
	}
#endif
	return 0;

out_free_cs:
	gpmc_cs_free(gpmc_cfg->cs);
	return err;
}

static void ft313_hc_init(int evm_id, int profile)
{
	int err;

	pr_err("Start FT313H init from board file\n");

	setup_pin_mux(ft313_pin_mux);
	err = gpmc_ft313_init(&ft313_hc_info);

	if (err < 0) {
		pr_err("FT313 Device Init failed!\n");
	} else {
		pr_err("FT313 Device Init successful\n");
	}
}

#endif // CONFIG_USB_FT313H_HCD
// END of FT313H Platform Device Code

/*
* @evm_id - evm id which needs to be configured
* @dev_cfg - single evm structure which includes
*				all module inits, pin-mux defines
* @profile - if present, else PROFILE_NONE
* @dghtr_brd_flg - Whether Daughter board is present or not
*/
static void _configure_device(int evm_id, struct evm_dev_cfg *dev_cfg,
	int profile)
{
	int i;

	/*
	* Only General Purpose & Industrial Auto Motro Control
	* EVM has profiles. So check if this evm has profile.
	* If not, ignore the profile comparison
	*/

	/*
	* If the device is on baseboard, directly configure it. Else (device on
	* Daughter board), check if the daughter card is detected.
	*/
	if (profile == PROFILE_NONE) {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->device_on == DEV_ON_BASEBOARD)
				dev_cfg->device_init(evm_id, profile);
			else if (daughter_brd_detected == true)
				dev_cfg->device_init(evm_id, profile);
		}
	} else {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->profile & profile) {
				if (dev_cfg->device_on == DEV_ON_BASEBOARD)
					dev_cfg->device_init(evm_id, profile);
				else if (daughter_brd_detected == true)
					dev_cfg->device_init(evm_id, profile);
			}
		}
	}
}

/* pinmux for usb0 drvvbus */
static struct pinmux_config usb0_pin_mux[] = {
	{"usb0_drvvbus.usb0_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* pinmux for usb1 drvvbus */
static struct pinmux_config usb1_pin_mux[] = {
	{"usb1_drvvbus.usb1_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

#if 0
/* pinmux for profibus */
static struct pinmux_config profibus_pin_mux[] = {
	{"uart1_rxd.pr1_uart0_rxd_mux1", OMAP_MUX_MODE5 | AM33XX_PIN_INPUT},
	{"uart1_txd.pr1_uart0_txd_mux1", OMAP_MUX_MODE5 | AM33XX_PIN_OUTPUT},
	{"mcasp0_fsr.pr1_pru0_pru_r30_5", OMAP_MUX_MODE5 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};
#endif

/* Module pin mux for eCAP2 */
static struct pinmux_config ecap2_pin_mux[] = {
	{"mcasp0_ahclkr.ecap2_in_pwm2_out", OMAP_MUX_MODE4 | AM33XX_PIN_OUTPUT}, /* Backlight */
	{NULL, 0},
};


/* Module pin mux for EHRPWM1 */
static struct pinmux_config ehrpwm1_pin_mux[] = {
	{"gpmc_a2.ehrpwm1A", OMAP_MUX_MODE6 | AM33XX_PIN_OUTPUT}, // Backlight
        {NULL, 0},
};

#if 0
/* Module pin mux for lvds */
static struct pinmux_config lvds_pin_mux[] = {
	{"gpmc_a11.gpio1_27", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
        {NULL, 0},
};
#endif

/* Module pin mux for uart1 */
static struct pinmux_config uart1_pin_mux[] = {
        {"uart1_rxd.uart1_rxd", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
        {"uart1_txd.uart1_txd", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL},
        {"uart1_ctsn.uart1_ctsn", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT},
//        {"uart1_rtsn.uart1_rtsn", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"uart1_rtsn.gpio0_13", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
        {NULL, 0},
};
/* Module pin mux for uart2 */
static struct pinmux_config uart2_pin_mux[] = {
        {"mii1_crs.uart2_rxd", OMAP_MUX_MODE6 | AM33XX_PIN_INPUT_PULLUP},
        {"mii1_rxerr.uart2_txd", OMAP_MUX_MODE6 | AM33XX_PULL_ENBL},
        {NULL, 0},
};

#if 1
#define AM335XEVM_WLAN_IRQ_GPIO		GPIO_TO_PIN(0, 27)

struct wl12xx_platform_data am335xevm_wlan_data = {
	.irq = OMAP_GPIO_IRQ(AM335XEVM_WLAN_IRQ_GPIO),
	.board_ref_clock = WL12XX_REFCLOCK_38_XTAL, /* 38.4Mhz */
};

/* Module pin mux for wlan and bluetooth */
static struct pinmux_config mmc2_wl12xx_pin_mux[] = {
	{"gpmc_ad12.mmc2_dat0", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad13.mmc2_dat1", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad14.mmc2_dat2", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad15.mmc2_dat3", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn3.mmc2_cmd", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_clk.mmc2_clk", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config uart3_wl12xx_pin_mux[] = {
        {"spi0_cs1.uart3_rxd", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLUP},
        {"ecap0_in_pwm0_out.uart3_txd", OMAP_MUX_MODE1 | AM33XX_PULL_ENBL},
	{NULL, 0},
};

static struct pinmux_config wl12xx_pin_mux_evm_rev1_1a[] = {
	{"gpmc_a0.gpio1_16", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"mcasp0_ahclkr.gpio3_17", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"mcasp0_ahclkx.gpio3_21", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
 };

static struct pinmux_config wl12xx_pin_mux_evm_rev1_0[] = {
	{"gpmc_csn1.gpio1_30", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"mcasp0_ahclkr.gpio3_17", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_csn2.gpio1_31", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
 };
static struct pinmux_config wl12xx_pin_mux_evm[] = {
	{"gpmc_ad9.gpio0_23", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad10.gpio0_26", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"gpmc_ad11.gpio0_27", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{NULL, 0},
 };
#endif

#if 1
static int backlight_enable = false;

static void enable_ecap2(int evm_id, int profile)
{
	backlight_enable = true;
}

static struct platform_device lcd_backlight = {
        .name           = "pwm-backlight",
        .id             = -1,
        .dev            = {
                .platform_data  = &lcd_backlight_data,
        }
};

static int __init ecap2_init(void)
{
	int status = 0;

	if (backlight_enable) {
		setup_pin_mux(ecap2_pin_mux);
		platform_device_register(&lcd_backlight);
	}
	return status;
}
late_initcall(ecap2_init);
#else
static int ehrpwm_backlight_enable = false;

static void enable_ehrpwm1(int evm_id, int profile)
{
        ehrpwm_backlight_enable = true;
}

static struct platform_device lcd_backlight = {
        .name           = "pwm-backlight",
        .id             = -1,
        .dev            = {
                .platform_data  = &lcd_backlight_data,
        }
};

static int __init ehrpwm1_init(void)
{
        int status = 0;
        if (ehrpwm_backlight_enable) {
		setup_pin_mux(ehrpwm1_pin_mux);
                platform_device_register(&lcd_backlight);
        }
        return status;
}
late_initcall(ehrpwm1_init);
#endif

#if 0
/* lvds */
#define AM335X_LVDS_EN_PIN       GPIO_TO_PIN(1, 27)

static int __init lvds_init(void)
{
	int status = 0;

	pr_dbg("--------lvds_init\n");

	setup_pin_mux(lvds_pin_mux);

	status = gpio_request(AM335X_LVDS_EN_PIN, "lvds enable\n");
	if (status < 0)
		pr_warn("Failed to request gpio for lvds enable\n");

	gpio_direction_output(AM335X_LVDS_EN_PIN, 1);

	return status;
}
late_initcall(lvds_init);
#endif

static int __init conf_disp_pll(int rate)
{
	struct clk *disp_pll;
	int ret = -EINVAL;

	disp_pll = clk_get(NULL, "dpll_disp_ck");
	if (IS_ERR(disp_pll)) {
		pr_err("Cannot clk_get disp_pll\n");
		goto out;
	}

	ret = clk_set_rate(disp_pll, rate);
	clk_put(disp_pll);
out:
	return ret;
}

static void lcdc_init(int evm_id, int profile)
{
	setup_pin_mux(lcdc_pin_mux);

	if (conf_disp_pll(300000000)) {
		pr_info("Failed configure display PLL, not attempting to"
				"register LCDC\n");
		return;
	}

	if (am33xx_register_lcdc(&devkit8600_lcd_pdata))
		pr_info("Failed to register LCDC device\n");
	return;
}

static void tsc_init(int evm_id, int profile)
{
	int err;

	am335x_touchscreen_data.analog_input = 1;

	setup_pin_mux(tsc_pin_mux);

	err = platform_device_register(&tsc_device);
	if (err)
		pr_err("failed to register touchscreen device\n");
}

static void rgmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rgmii1_pin_mux);
	return;
}

static void rgmii2_init(int evm_id, int profile)
{
	setup_pin_mux(rgmii2_pin_mux);
	return;
}

static void mii1_init(int evm_id, int profile)
{
	setup_pin_mux(mii1_pin_mux);
	return;
}

static void rmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rmii1_pin_mux);
	return;
}

static void usb0_init(int evm_id, int profile)
{
	pr_dbg("--------usb0_init\n");

	setup_pin_mux(usb0_pin_mux);
	return;
}

static void usb1_init(int evm_id, int profile)
{
	pr_dbg("--------usb1_init\n");

	setup_pin_mux(usb1_pin_mux);
	return;
}
#if 0
/* setup uart3 */
static void uart3_init(int evm_id, int profile)
{
	setup_pin_mux(uart3_pin_mux);
	return;
}
#endif

/* NAND partition information */
static struct mtd_partition am335x_nand_partitions[] = {
/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "SPL",
		.offset         = 0,			/* Offset = 0x0 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup1",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x20000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup2",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x40000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup3",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x60000 */
		.size           = SZ_128K,
	},
	{
		.name           = "U-Boot",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x80000 */
		.size           = 15 * SZ_128K,
	},
	{
		.name           = "U-Boot Env",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x260000 */
		.size           = 1 * SZ_128K,
	},
	{
		.name           = "Kernel",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x280000 */
		.size           = 40 * SZ_128K,
	},
	{
		.name           = "File System",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x780000 */
		.size           = MTDPART_SIZ_FULL,
	},
};

/* SPI 0/1 Platform Data */
/* SPI flash information */
static struct mtd_partition am335x_spi_partitions[] = {
	/* All the partition sizes are listed in terms of erase size */
	{
		.name       = "U-Boot-min",
		.offset     = 0,
		.size       = SZ_128K,
		.mask_flags = MTD_WRITEABLE,    /* force read-only */
	},
	{
		.name       = "U-Boot",
		.offset     = MTDPART_OFS_APPEND,
		.size       = 2 * SZ_128K,
		.mask_flags = MTD_WRITEABLE,    /* force read-only */
	},
	{
		.name       = "U-Boot Env",
		.offset     = MTDPART_OFS_APPEND,
		.size       = 2 * SZ_4K,
	},
	{
		.name       = "Kernel",
		.offset     = MTDPART_OFS_APPEND,
		.size       = 28 * SZ_128K,
	},
	{
		.name       = "File System",
		.offset     = MTDPART_OFS_APPEND,
		.size       = MTDPART_SIZ_FULL,     /* size ~= 1.1 MiB */
	}
};

static const struct flash_platform_data am335x_spi_flash = {
	.type      = "w25q64",
	.name      = "spi_flash",
	.parts     = am335x_spi_partitions,
	.nr_parts  = ARRAY_SIZE(am335x_spi_partitions),
};

/*
 * SPI Flash works at 80Mhz however SPI Controller works at 48MHz.
 * So setup Max speed to be less than that of Controller speed
 */
static struct spi_board_info am335x_spi0_slave_info[] = {
	{
		.modalias      = "m25p80",
		.platform_data = &am335x_spi_flash,
		.irq           = -1,
		.max_speed_hz  = 24000000,
		.bus_num       = 1,
		.chip_select   = 0,
	},
};

static struct spi_board_info am335x_spi1_slave_info[] = {
	{
		.modalias      = "m25p80",
		.platform_data = &am335x_spi_flash,
		.irq           = -1,
		.max_speed_hz  = 12000000,
		.bus_num       = 2,
		.chip_select   = 0,
	},
};

static void evm_nand_init(int evm_id, int profile)
{
	setup_pin_mux(nand_pin_mux);
	board_nand_init(am335x_nand_partitions,
		ARRAY_SIZE(am335x_nand_partitions), 0, 0);
}

static struct lis3lv02d_platform_data lis331dlh_pdata = {
	.click_flags = LIS3_CLICK_SINGLE_X |
			LIS3_CLICK_SINGLE_Y |
			LIS3_CLICK_SINGLE_Z,
	.wakeup_flags = LIS3_WAKEUP_X_LO | LIS3_WAKEUP_X_HI |
			LIS3_WAKEUP_Y_LO | LIS3_WAKEUP_Y_HI |
			LIS3_WAKEUP_Z_LO | LIS3_WAKEUP_Z_HI,
	.irq_cfg = LIS3_IRQ1_CLICK | LIS3_IRQ2_CLICK,
	.wakeup_thresh	= 10,
	.click_thresh_x = 10,
	.click_thresh_y = 10,
	.click_thresh_z = 10,
	.g_range	= 2,
	.st_min_limits[0] = 120,
	.st_min_limits[1] = 120,
	.st_min_limits[2] = 140,
	.st_max_limits[0] = 550,
	.st_max_limits[1] = 550,
	.st_max_limits[2] = 750,
};

static struct i2c_board_info am335x_i2c_boardinfo1[] = {
#if 0
	{
		I2C_BOARD_INFO("tlv320aic3x", 0x1b),
	},
#endif
	{
		I2C_BOARD_INFO("lis331dlh", 0x18),
		.platform_data = &lis331dlh_pdata,
	},
	{
		I2C_BOARD_INFO("tsl2550", 0x39),
	},
	{
		I2C_BOARD_INFO("tmp275", 0x48),
	},
};

static void i2c1_init(int evm_id, int profile)
{
	setup_pin_mux(i2c1_pin_mux);
	omap_register_i2c_bus(2, 100, am335x_i2c_boardinfo1,
			ARRAY_SIZE(am335x_i2c_boardinfo1));
	return;
}

static void mcasp0_init(int evm_id, int profile)
{
	pr_dbg("--------mcasp0_init\n");

	/* Configure McASP */
	setup_pin_mux(mcasp0_pin_mux);
	am335x_register_mcasp0(&am335x_evm_snd_data0);
	return;
}

/* Setup McASP 1 */
static void mcasp1_init(int evm_id, int profile)
{
	pr_dbg("--------mcasp1_init\n");

	/* Configure McASP */
	setup_pin_mux(mcasp1_pin_mux);
	am335x_register_mcasp1(&am335x_evm_snd_data1);
	return;
}

static void uart1_init(int evm_id, int profile)
{
        pr_dbg("--------uart1_init\n");

        /* Configure Uart1*/
        setup_pin_mux(uart1_pin_mux);
}

static void uart2_init(int evm_id, int profile)
{
        pr_dbg("--------uart2_init\n");

        /* Configure Uart2*/
        setup_pin_mux(uart2_pin_mux);
        return;
}

#if 1
static void mmc1_init(int evm_id, int profile)
{
	setup_pin_mux(mmc1_pin_mux);

	am335x_mmc[1].mmc = 2;
	am335x_mmc[1].caps = MMC_CAP_4_BIT_DATA;
	am335x_mmc[1].gpio_cd = GPIO_TO_PIN(2, 2);
	am335x_mmc[1].gpio_wp = GPIO_TO_PIN(1, 29);
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */

	/* mmc will be initialized when mmc0_init is called */
	return;
}

static void mmc2_wl12xx_init(int evm_id, int profile)
{
	setup_pin_mux(mmc2_wl12xx_pin_mux);

	am335x_mmc[1].mmc = 3;
	am335x_mmc[1].name = "wl1271";
	am335x_mmc[1].caps = MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD
				| MMC_PM_KEEP_POWER;
	am335x_mmc[1].nonremovable = true;
	am335x_mmc[1].gpio_cd = -EINVAL;
	am335x_mmc[1].gpio_wp = -EINVAL;
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */

	/* mmc will be initialized when mmc0_init is called */
	return;
}

static void uart3_wl12xx_init(int evm_id, int profile)
{
	setup_pin_mux(uart3_wl12xx_pin_mux);
}

static void wl12xx_bluetooth_enable(void)
{
	int status = gpio_request(am335xevm_wlan_data.bt_enable_gpio,
		"bt_en\n");
	if (status < 0)
		pr_err("Failed to request gpio for bt_enable");

	pr_info("Configure Bluetooth Enable pin...\n");
	gpio_direction_output(am335xevm_wlan_data.bt_enable_gpio, 0);
}

static int wl12xx_set_power(struct device *dev, int slot, int on, int vdd)
{
	if (on) {
		gpio_set_value(am335xevm_wlan_data.wlan_enable_gpio, 1);
		mdelay(70);
	}
	else
		gpio_set_value(am335xevm_wlan_data.wlan_enable_gpio, 0);

	return 0;
}

static void wl12xx_init(int evm_id, int profile)
{
	struct device *dev;
	struct omap_mmc_platform_data *pdata;
	int ret;

	/* Register WLAN and BT enable pins based on the evm board revision */
#if 0
	if (gp_evm_revision == GP_EVM_REV_IS_1_1A) {
		am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(1, 16);
		am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(3, 21);
	}
	else {
		am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(1, 30);
		am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(1, 31);
	}
#endif

	setup_pin_mux(wl12xx_pin_mux_evm);

        am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(0, 26);
        am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(0, 23);

	wl12xx_bluetooth_enable();

	if (wl12xx_set_platform_data(&am335xevm_wlan_data))
		pr_err("error setting wl12xx data\n");

	dev = am335x_mmc[1].dev;
	if (!dev) {
		pr_err("wl12xx mmc device initialization failed\n");
		goto out;
	}

	pdata = dev->platform_data;
	if (!pdata) {
		pr_err("Platfrom data of wl12xx device not set\n");
		goto out;
	}

	ret = gpio_request_one(am335xevm_wlan_data.wlan_enable_gpio,
		GPIOF_OUT_INIT_LOW, "wlan_en");
	if (ret) {
		pr_err("Error requesting wlan enable gpio: %d\n", ret);
		goto out;
	}

#if 0
	if (gp_evm_revision == GP_EVM_REV_IS_1_1A)
		setup_pin_mux(wl12xx_pin_mux_evm_rev1_1a);
	else
		setup_pin_mux(wl12xx_pin_mux_evm_rev1_0);
#endif

	pdata->slots[0].set_power = wl12xx_set_power;
out:
	return;
}
#endif

static void d_can_init(int evm_id, int profile)
{
	pr_dbg("--------d_can_init\n");

	setup_pin_mux(d_can_pin_mux);
	am33xx_d_can_init(1);
}

static void mmc0_init(int evm_id, int profile)
{
	setup_pin_mux(mmc0_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}

static void mmc0_no_cd_init(int evm_id, int profile)
{
	setup_pin_mux(mmc0_no_cd_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}


/* setup spi0 */
static void spi0_init(int evm_id, int profile)
{
	setup_pin_mux(spi0_pin_mux);
	spi_register_board_info(am335x_spi0_slave_info,
			ARRAY_SIZE(am335x_spi0_slave_info));
	return;
}

#if 0
/* setup spi1 */
static void spi1_init(int evm_id, int profile)
{
	setup_pin_mux(spi1_pin_mux);
	spi_register_board_info(am335x_spi1_slave_info,
			ARRAY_SIZE(am335x_spi1_slave_info));
	return;
}
#endif

static int beaglebone_phy_fixup(struct phy_device *phydev)
{
	phydev->supported &= ~(SUPPORTED_100baseT_Half |
				SUPPORTED_100baseT_Full);

	return 0;
}

#ifdef CONFIG_TLK110_WORKAROUND
static int am335x_tlk110_phy_fixup(struct phy_device *phydev)
{
	unsigned int val;

	/* This is done as a workaround to support TLK110 rev1.0 phy */
	val = phy_read(phydev, TLK110_COARSEGAIN_REG);
	phy_write(phydev, TLK110_COARSEGAIN_REG, (val | TLK110_COARSEGAIN_VAL));

	val = phy_read(phydev, TLK110_LPFHPF_REG);
	phy_write(phydev, TLK110_LPFHPF_REG, (val | TLK110_LPFHPF_VAL));

	val = phy_read(phydev, TLK110_SPAREANALOG_REG);
	phy_write(phydev, TLK110_SPAREANALOG_REG, (val | TLK110_SPANALOG_VAL));

	val = phy_read(phydev, TLK110_VRCR_REG);
	phy_write(phydev, TLK110_VRCR_REG, (val | TLK110_VRCR_VAL));

	val = phy_read(phydev, TLK110_SETFFE_REG);
	phy_write(phydev, TLK110_SETFFE_REG, (val | TLK110_SETFFE_VAL));

	val = phy_read(phydev, TLK110_FTSP_REG);
	phy_write(phydev, TLK110_FTSP_REG, (val | TLK110_FTSP_VAL));

	val = phy_read(phydev, TLK110_ALFATPIDL_REG);
	phy_write(phydev, TLK110_ALFATPIDL_REG, (val | TLK110_ALFATPIDL_VAL));

	val = phy_read(phydev, TLK110_PSCOEF21_REG);
	phy_write(phydev, TLK110_PSCOEF21_REG, (val | TLK110_PSCOEF21_VAL));

	val = phy_read(phydev, TLK110_PSCOEF3_REG);
	phy_write(phydev, TLK110_PSCOEF3_REG, (val | TLK110_PSCOEF3_VAL));

	val = phy_read(phydev, TLK110_ALFAFACTOR1_REG);
	phy_write(phydev, TLK110_ALFAFACTOR1_REG, (val | TLK110_ALFACTOR1_VAL));

	val = phy_read(phydev, TLK110_ALFAFACTOR2_REG);
	phy_write(phydev, TLK110_ALFAFACTOR2_REG, (val | TLK110_ALFACTOR2_VAL));

	val = phy_read(phydev, TLK110_CFGPS_REG);
	phy_write(phydev, TLK110_CFGPS_REG, (val | TLK110_CFGPS_VAL));

	val = phy_read(phydev, TLK110_FTSPTXGAIN_REG);
	phy_write(phydev, TLK110_FTSPTXGAIN_REG, (val | TLK110_FTSPTXGAIN_VAL));

	val = phy_read(phydev, TLK110_SWSCR3_REG);
	phy_write(phydev, TLK110_SWSCR3_REG, (val | TLK110_SWSCR3_VAL));

	val = phy_read(phydev, TLK110_SCFALLBACK_REG);
	phy_write(phydev, TLK110_SCFALLBACK_REG, (val | TLK110_SCFALLBACK_VAL));

	val = phy_read(phydev, TLK110_PHYRCR_REG);
	phy_write(phydev, TLK110_PHYRCR_REG, (val | TLK110_PHYRCR_VAL));

	return 0;
}
#endif

#if 0
static void profibus_init(int evm_id, int profile)
{
	setup_pin_mux(profibus_pin_mux);
	return;
}
#endif

/* Low-Cost EVM */
static struct evm_dev_cfg low_cost_evm_dev_cfg[] = {
	{rgmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{evm_nand_init, DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

/* General Purpose EVM */
static struct evm_dev_cfg gen_purp_evm_dev_cfg[] = {
	{enable_ecap2,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_1 | PROFILE_2 | PROFILE_7) },
//	{enable_ehrpwm1, DEV_ON_BASEBOARD, PROFILE_0},
	{lcdc_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_1 | PROFILE_2 | PROFILE_7) },
	{tsc_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_1 | PROFILE_2 | PROFILE_7) },
	{rgmii1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{rgmii2_init,	DEV_ON_DGHTR_BRD, (PROFILE_1 | PROFILE_2 | PROFILE_4 | PROFILE_6) },
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
        {uart1_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
        {uart2_init,    DEV_ON_BASEBOARD, PROFILE_ALL},
	{evm_nand_init, DEV_ON_BASEBOARD, (PROFILE_ALL & ~PROFILE_2 & ~PROFILE_3)},
	{i2c1_init,	DEV_ON_DGHTR_BRD, (PROFILE_ALL & ~PROFILE_2)},
	{mcasp0_init,   DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 | PROFILE_7) },
	{mcasp1_init,	DEV_ON_DGHTR_BRD, (PROFILE_0 | PROFILE_3 | PROFILE_7) },
//	{mmc1_init,	DEV_ON_DGHTR_BRD, PROFILE_2},
	{mmc2_wl12xx_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 | PROFILE_5)},
	{mmc0_init,	DEV_ON_BASEBOARD, (PROFILE_ALL & ~PROFILE_5)},
	{mmc0_no_cd_init,	DEV_ON_BASEBOARD, PROFILE_5},
	{spi0_init,	DEV_ON_DGHTR_BRD, PROFILE_2},
	{uart3_wl12xx_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 | PROFILE_5)},
	{wl12xx_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_3 | PROFILE_5)},
	{d_can_init,	DEV_ON_BASEBOARD, (PROFILE_0 | PROFILE_1)},
//	{matrix_keypad_init, DEV_ON_DGHTR_BRD, PROFILE_0},
	{gpio_led_init, DEV_ON_BASEBOARD, PROFILE_0},
#ifdef CONFIG_USB_FT313H_HCD
	{ft313_hc_init, DEV_ON_BASEBOARD, PROFILE_0},
#else
	{gpio_keys_init, DEV_ON_BASEBOARD, PROFILE_0}, //Removed because GPMC CS1 conflict with FT313
#endif
	{NULL, 0, 0},
};

/* Industrial Auto Motor Control EVM */
static struct evm_dev_cfg ind_auto_mtrl_evm_dev_cfg[] = {
	{mii1_init,	DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
//	{profibus_init, DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{evm_nand_init, DEV_ON_DGHTR_BRD, PROFILE_ALL},
//	{spi1_init,	DEV_ON_DGHTR_BRD, PROFILE_ALL},
//	{uart3_init,	DEV_ON_DGHTR_BRD, PROFILE_ALL},
	{i2c1_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{mmc0_no_cd_init,	DEV_ON_BASEBOARD, PROFILE_ALL},
	{NULL, 0, 0},
};

/* IP-Phone EVM */
static struct evm_dev_cfg ip_phn_evm_dev_cfg[] = {
//	{enable_ecap0,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{lcdc_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{tsc_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{rgmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{rgmii2_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{evm_nand_init, DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{i2c1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mcasp1_init,	DEV_ON_DGHTR_BRD, PROFILE_NONE},
	{mmc0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

/* Beaglebone < Rev A3 */
static struct evm_dev_cfg beaglebone_old_dev_cfg[] = {
	{rmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mmc0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

/* Beaglebone Rev A3 and after */
static struct evm_dev_cfg beaglebone_dev_cfg[] = {
	{mii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mmc0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};

static void setup_low_cost_evm(void)
{
	pr_info("The board is a AM335x Low Cost EVM.\n");

	_configure_device(LOW_COST_EVM, low_cost_evm_dev_cfg, PROFILE_NONE);
}

static void setup_general_purpose_evm(void)
{
	pr_dbg("--------setup_general_purpose_evm\n");

	u32 prof_sel = am335x_get_profile_selection();
	pr_info("The board is general purpose EVM in profile %d\n", prof_sel);

        daughter_brd_detected = false;
        gigabit_enable = 0;

	_configure_device(GEN_PURP_EVM, gen_purp_evm_dev_cfg, (1L << prof_sel));
}

static void setup_ind_auto_motor_ctrl_evm(void)
{
	u32 prof_sel = am335x_get_profile_selection();

	pr_info("The board is an industrial automation EVM in profile %d\n",
		prof_sel);

	/* Only Profile 0 is supported */
	if ((1L << prof_sel) != PROFILE_0) {
		pr_err("AM335X: Only Profile 0 is supported\n");
		pr_err("Assuming profile 0 & continuing\n");
		prof_sel = PROFILE_0;
	}

	_configure_device(IND_AUT_MTR_EVM, ind_auto_mtrl_evm_dev_cfg,
		PROFILE_0);

	/* Fillup global evmid */
	am33xx_evmid_fillup(IND_AUT_MTR_EVM);

	/* Initialize TLK110 PHY registers for phy version 1.0 */
	am335x_tlk110_phy_init();


}

static void setup_ip_phone_evm(void)
{
	pr_info("The board is an IP phone EVM\n");

	_configure_device(IP_PHN_EVM, ip_phn_evm_dev_cfg, PROFILE_NONE);
}

/* BeagleBone < Rev A3 */
static void setup_beaglebone_old(void)
{
	pr_info("The board is a AM335x Beaglebone < Rev A3.\n");

	/* Beagle Bone has Micro-SD slot which doesn't have Write Protect pin */
	am335x_mmc[0].gpio_wp = -EINVAL;

	_configure_device(LOW_COST_EVM, beaglebone_old_dev_cfg, PROFILE_NONE);

	phy_register_fixup_for_uid(BBB_PHY_ID, BBB_PHY_MASK,
					beaglebone_phy_fixup);
}

/* BeagleBone after Rev A3 */
static void setup_beaglebone(void)
{
	pr_info("The board is a AM335x Beaglebone.\n");

	/* Beagle Bone has Micro-SD slot which doesn't have Write Protect pin */
	am335x_mmc[0].gpio_wp = -EINVAL;

	_configure_device(LOW_COST_EVM, beaglebone_dev_cfg, PROFILE_NONE);
}


static void am335x_setup_daughter_board(struct memory_accessor *m, void *c)
{
	u8 tmp;
	int ret;

	/*
	 * try reading a byte from the EEPROM to see if it is
	 * present. We could read a lot more, but that would
	 * just slow the boot process and we have all the information
	 * we need from the EEPROM on the base board anyway.
	 */
	ret = m->read(m, &tmp, 0, sizeof(u8));
	if (ret == sizeof(u8)) {
		pr_info("Detected a daughter card on AM335x EVM..");
		daughter_brd_detected = true;
	} else {
		pr_info("No daughter card found\n");
		daughter_brd_detected = false;
	}
}

//static void am335x_evm_setup(struct memory_accessor *mem_acc, void *context)
static void am335x_evm_setup(void)
{
	int ret;
	char tmp[10];

	pr_dbg("--------am335x_evm_setup\n");

	setup_general_purpose_evm();

	/* Initialize cpsw after board detection is completed as board
	 * information is required for configuring phy address and hence
	 * should be call only after board detection
	 */
	am33xx_cpsw_init(gigabit_enable);

	return;
}

#if 0
static struct at24_platform_data am335x_daughter_board_eeprom_info = {
	.byte_len       = (256*1024) / 8,
	.page_size      = 64,
	.flags          = AT24_FLAG_ADDR16,
	.setup          = am335x_setup_daughter_board,
	.context        = (void *)NULL,
};

static struct at24_platform_data am335x_baseboard_eeprom_info = {
	.byte_len       = (256*1024) / 8,
	.page_size      = 64,
	.flags          = AT24_FLAG_ADDR16,
	.setup          = am335x_evm_setup,
	.context        = (void *)NULL,
};
#endif

static struct regulator_init_data am335x_dummy;

static struct regulator_consumer_supply am335x_vdd1_supply[] = {
	REGULATOR_SUPPLY("mpu", "mpu.0"),
};

static struct regulator_init_data am335x_vdd1 = {
	.constraints = {
		.min_uV			= 600000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE,
		.always_on		= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(am335x_vdd1_supply),
	.consumer_supplies	= am335x_vdd1_supply,
};

static struct tps65910_board am335x_tps65910_info = {
	.tps65910_pmic_init_data[TPS65910_REG_VRTC]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VIO]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDD1]	= &am335x_vdd1,
	.tps65910_pmic_init_data[TPS65910_REG_VDD2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDD3]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDIG1]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDIG2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VPLL]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDAC]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX1]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX33]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VMMC]	= &am335x_dummy,
};

/*
* Daughter board Detection.
* Every board has a ID memory (EEPROM) on board. We probe these devices at
* machine init, starting from daughter board and ending with baseboard.
* Assumptions :
*	1. probe for i2c devices are called in the order they are included in
*	   the below struct. Daughter boards eeprom are probed 1st. Baseboard
*	   eeprom probe is called last.
*/
static struct i2c_board_info __initdata am335x_i2c_boardinfo[] = {
#if 0
	{
		/* Daughter Board EEPROM */
		I2C_BOARD_INFO("24c256", DAUG_BOARD_I2C_ADDR),
		.platform_data  = &am335x_daughter_board_eeprom_info,
	},
	{
		/* Baseboard board EEPROM */
		I2C_BOARD_INFO("24c256", BASEBOARD_I2C_ADDR),
		.platform_data  = &am335x_baseboard_eeprom_info,
	},
#endif
	{
		I2C_BOARD_INFO("cpld_reg", 0x35),
	},
	{
		I2C_BOARD_INFO("tlc59108", 0x40),
	},
	{
		I2C_BOARD_INFO("tps65910", TPS65910_I2C_ID1),
		.platform_data  = &am335x_tps65910_info,
	},
	{
		I2C_BOARD_INFO("sgtl5000", 0x0A),
	},
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	.mode           = MUSB_OTG,
	.power		= 500,
	.instances	= 1,
};

#if 0
static int cpld_reg_probe(struct i2c_client *client,
	    const struct i2c_device_id *id)
{
	cpld_client = client;
	return 0;
}

static int __devexit cpld_reg_remove(struct i2c_client *client)
{
	cpld_client = NULL;
	return 0;
}

static const struct i2c_device_id cpld_reg_id[] = {
	{ "cpld_reg", 0 },
	{ }
};

static struct i2c_driver cpld_reg_driver = {
	.driver = {
		.name	= "cpld_reg",
	},
	.probe		= cpld_reg_probe,
	.remove		= cpld_reg_remove,
	.id_table	= cpld_reg_id,
};

static void evm_init_cpld(void)
{
	i2c_add_driver(&cpld_reg_driver);
}
#endif

static void __init am335x_evm_i2c_init(void)
{
//	evm_init_cpld();

	pr_dbg("--------am335x_evm_i2c_init\n");

	setup_pin_mux(i2c0_pin_mux);
	omap_register_i2c_bus(1, 100, am335x_i2c_boardinfo,
				ARRAY_SIZE(am335x_i2c_boardinfo));
}

static struct resource am335x_rtc_resources[] = {
	{
		.start		= AM33XX_RTC_BASE,
		.end		= AM33XX_RTC_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{ /* timer irq */
		.start		= AM33XX_IRQ_RTC_TIMER,
		.end		= AM33XX_IRQ_RTC_TIMER,
		.flags		= IORESOURCE_IRQ,
	},
	{ /* alarm irq */
		.start		= AM33XX_IRQ_RTC_ALARM,
		.end		= AM33XX_IRQ_RTC_ALARM,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device am335x_rtc_device = {
	.name           = "omap_rtc",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(am335x_rtc_resources),
	.resource	= am335x_rtc_resources,
};

static int am335x_rtc_init(void)
{
	void __iomem *base;
	struct clk *clk;

	clk = clk_get(NULL, "rtc_fck");
	if (IS_ERR(clk)) {
		pr_err("rtc : Failed to get RTC clock\n");
		return -1;
	}

	if (clk_enable(clk)) {
		pr_err("rtc: Clock Enable Failed\n");
		return -1;
	}

	base = ioremap(AM33XX_RTC_BASE, SZ_4K);

	if (WARN_ON(!base))
		return -ENOMEM;

	/* Unlock the rtc's registers */
	__raw_writel(0x83e70b13, base + 0x6c);
	__raw_writel(0x95a4f1e0, base + 0x70);

	/*
	 * Enable the 32K OSc
	 * TODO: Need a better way to handle this
	 * Since we want the clock to be running before mmc init
	 * we need to do it before the rtc probe happens
	 */
	__raw_writel(0x48, base + 0x54);

	iounmap(base);

	return  platform_device_register(&am335x_rtc_device);
}

/* Enable clkout2 */
static struct pinmux_config clkout2_pin_mux[] = {
	{"xdma_event_intr1.clkout2", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static void __init clkout2_enable(void)
{
	struct clk *ck_32;

	ck_32 = clk_get(NULL, "clkout2_ck");
	if (IS_ERR(ck_32)) {
		pr_err("Cannot clk_get ck_32\n");
		return;
	}

	clk_enable(ck_32);

	setup_pin_mux(clkout2_pin_mux);
}

void __iomem * __init am33xx_get_mem_ctlr(void)
{
	void __iomem *am33xx_emif_base;

	am33xx_emif_base = ioremap(AM33XX_EMIF0_BASE, SZ_32K);

	if (!am33xx_emif_base)
		pr_warning("%s: Unable to map DDR2 controller",	__func__);

	return am33xx_emif_base;
}

static struct resource am33xx_cpuidle_resources[] = {
	{
		.start		= AM33XX_EMIF0_BASE,
		.end		= AM33XX_EMIF0_BASE + SZ_32K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

/* AM33XX devices support DDR2 power down */
static struct am33xx_cpuidle_config am33xx_cpuidle_pdata = {
	.ddr2_pdown	= 1,
};

static struct platform_device am33xx_cpuidle_device = {
	.name			= "cpuidle-am33xx",
	.num_resources		= ARRAY_SIZE(am33xx_cpuidle_resources),
	.resource		= am33xx_cpuidle_resources,
	.dev = {
		.platform_data	= &am33xx_cpuidle_pdata,
	},
};

static void __init am33xx_cpuidle_init(void)
{
	int ret;

	am33xx_cpuidle_pdata.emif_base = am33xx_get_mem_ctlr();

	ret = platform_device_register(&am33xx_cpuidle_device);

	if (ret)
		pr_warning("AM33XX cpuidle registration failed\n");

}

static void __init am335x_evm_init(void)
{
	am33xx_cpuidle_init();
	am33xx_mux_init(board_mux);
	omap_serial_init();
	am335x_rtc_init();
	clkout2_enable();
	am335x_evm_i2c_init();
	am335x_evm_setup();
	omap_sdrc_init(NULL, NULL);
	usb_musb_init(&musb_board_data);
	omap_board_config = am335x_evm_config;
	omap_board_config_size = ARRAY_SIZE(am335x_evm_config);
	/* Create an alias for icss clock */
	if (clk_add_alias("pruss", NULL, "icss_uart_gclk", NULL))
		pr_err("failed to create an alias: icss_uart_gclk --> pruss\n");
	/* Create an alias for gfx/sgx clock */
	if (clk_add_alias("sgx_ck", NULL, "gfx_fclk", NULL))
		pr_err("failed to create an alias: gfx_fclk --> sgx_ck\n");
}

static void __init am335x_evm_map_io(void)
{
	omap2_set_globals_am33xx();
	omapam33xx_map_common_io();
}

MACHINE_START(AM335XEVM, "am335xevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_irq	= ti816x_init_irq,
	.init_early	= am335x_init_early,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END

MACHINE_START(AM335XIAEVM, "am335xiaevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_irq	= ti816x_init_irq,
	.init_early	= am335x_init_early,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END
