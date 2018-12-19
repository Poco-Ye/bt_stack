/*****************************************************************************
* Copyright 2008 - 2009 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/


/*
 *  Interrupt sources on BCM5892
 */

/* Secure MODE only - Secure VIC */
#define SVIC_IRQ_START	0
#define IRQ_PWRFAIL		0
#define IRQ_SEC_WDOG_TMOUT	1
#define IRQ_BBL_RTC		2
#define IRQ_DMU		3
#define IRQ_SEC_TIMER8	4
#define IRQ_SEC_TIMER9	5
#define IRQ_SMAU		6
#define IRQ_SEC_DMA		7
#define IRQ_PKA			8
#define IRQ_RNG		9
#define IRQ_EXT_RSTOUT	10
#define IRQ_PEND_OPEN		11
/* Secure/Open MODES*/
#define IRQ_SEC_TIMER0	12
#define IRQ_SEC_TIMER1	13
#define IRQ_SEC_ADC0		14
#define IRQ_SEC_ADC1		15
#define IRQ_EXT_GRP0		16
#define IRQ_EXT_GRP1		17
#define IRQ_EXT_GRP2		18
#define IRQ_EXT_GRP3		19
#define IRQ_EXT_GRP4		20
#define IRQ_EXT_GRP5       	21
#define IRQ_EXT_GRP6		22
#define IRQ_EXT_GRP7		23
#define IRQ_TDM_BUFCNTRL	24
#define IRQ_SMARTCARD0	25
#define IRQ_SMARTCARD1	26
#define IRQ_BSC0		27
#define IRQ_MSR		28
#define IRQ_SPI0		29
#define IRQ_SPI1		30
#define IRQ_SEC_PERF		31
#define SVIC_IRQ_END		31

/* Open MODE - Open VIC0 */
#define OVIC0_IRQ_START	32
#define IRQ_PEND_SECURE	(OVIC0_IRQ_START + 0)
#define IRQ_OTIMER0	       	(OVIC0_IRQ_START + 1)
#define IRQ_OTIMER1	   	(OVIC0_IRQ_START + 2)
#define IRQ_OADC0		(OVIC0_IRQ_START + 3)
#define IRQ_OADC1	      	(OVIC0_IRQ_START + 4)
#define IRQ_OEXTGRP0		(OVIC0_IRQ_START + 5)
#define IRQ_OEXTGRP1		(OVIC0_IRQ_START + 6)
#define IRQ_OEXTGRP2		(OVIC0_IRQ_START + 7)
#define IRQ_OEXTGRP3		(OVIC0_IRQ_START + 8)
#define IRQ_OEXTGRP4		(OVIC0_IRQ_START + 9)
#define IRQ_OEXTGRP5		(OVIC0_IRQ_START + 10)
#define IRQ_OEXTGRP6		(OVIC0_IRQ_START + 11)
#define IRQ_OEXTGRP7		(OVIC0_IRQ_START + 12)
#define IRQ_TDMBC		(OVIC0_IRQ_START + 13)
#define IRQ_OSMARTCARD0	(OVIC0_IRQ_START + 14)
#define IRQ_OSMARTCARD1	(OVIC0_IRQ_START + 15)
#define IRQ_OBSC0		(OVIC0_IRQ_START + 16)
#define IRQ_OMSR		(OVIC0_IRQ_START + 17)
#define IRQ_OSPI0		(OVIC0_IRQ_START + 18)
#define IRQ_OSPI1		(OVIC0_IRQ_START + 19)
#define IRQ_OPERF		(OVIC0_IRQ_START + 20)
#define IRQ_OTIMER2		(OVIC0_IRQ_START + 21)
#define IRQ_OTIMER3		(OVIC0_IRQ_START + 22)
#define IRQ_OWDOG_TIMOUT	(OVIC0_IRQ_START + 23)
#define IRQ_OLCDC		(OVIC0_IRQ_START + 24)
#define IRQ_OVIDDEC		(OVIC0_IRQ_START + 25)
#define IRQ_OUSB0		(OVIC0_IRQ_START + 26)
#define IRQ_OUSBH1		(OVIC0_IRQ_START + 27)
#define IRQ_OUSBH2		(OVIC0_IRQ_START + 28)
#define IRQ_OEMAC		(OVIC0_IRQ_START + 29)
#define IRQ_ONANDC		(OVIC0_IRQ_START + 30)
#define IRQ_OODMA		(OVIC0_IRQ_START + 31)
#define OVIC0_IRQ_END		63

/* Open VIC1 */
#define OVIC1_IRQ_START	64
#define IRQ_OSDIOMMCCEATA4	(OVIC1_IRQ_START + 0)	/* 4-b SD/SDIO/CEATA */
#define IRQ_OSDIOMMCCEATA8	(OVIC1_IRQ_START + 1)	/* 8-b SD/SDIO/CEATA */
#define IRQ_OI2S		(OVIC1_IRQ_START + 2)
#define IRQ_OTPBSPI4		(OVIC1_IRQ_START + 3)
/* 4 - Reserved */
#define IRQ_OSPI3		(OVIC1_IRQ_START + 5)
#define IRQ_OUART0		(OVIC1_IRQ_START + 6)
#define IRQ_OUART1		(OVIC1_IRQ_START + 7)
#define IRQ_OUART2		(OVIC1_IRQ_START + 8)
#define IRQ_OUART3		(OVIC1_IRQ_START + 9)
#define IRQ_OD1W		(OVIC1_IRQ_START + 10)
#define IRQ_OBSC1		(OVIC1_IRQ_START + 11)
#define IRQ_OTIMER4	       	(OVIC1_IRQ_START + 12)
#define IRQ_OTIMER5	       	(OVIC1_IRQ_START + 13)
#define IRQ_OTIMER6	       	(OVIC1_IRQ_START + 14)
#define IRQ_OTIMER7	       	(OVIC1_IRQ_START + 15)
#define IRQ_OTIMER8	       	(OVIC1_IRQ_START + 16)
#define IRQ_OTIMER9	       	(OVIC1_IRQ_START + 17)
/* 18-31 - Reserved */
#define OVIC1_IRQ_END		95

#define NR_IRQS		96
