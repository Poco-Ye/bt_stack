
/*****************************************************************************
* Copyright (c) 2008 - 2009 Broadcom Corporation.  All rights reserved.
*
* This program is the proprietary software of Broadcom Corporation and/or
* its licensors, and may only be used, duplicated, modified or distributed
* pursuant to the terms and conditions of a separate, written license
* agreement executed between you and Broadcom (an "Authorized License").
* Except as set forth in an Authorized License, Broadcom grants no license
* (express or implied), right to use, or waiver of any kind with respect to
* the Software, and Broadcom expressly reserves all rights in and to the
* Software and all intellectual property rights therein.  IF YOU HAVE NO
* AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
* WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
* THE SOFTWARE.
*****************************************************************************/
#ifndef _NAND_H_
#define _NAND_H_

#include "umc_cpuapi.h"
//#include <bcm5892_sw.h>
//#include "base.h"

#define PAGE_SIZE   (2048)
#define BLOCK_SIZE  (0x20000)   
#define HASH_SIZE   12

/*Boot Area*/
#define BOOT_BASE_ADDR      0
#define BOOT_END_ADDR      BLOCK_SIZE 

/*Config manage area macro*/
#define CONFIG_BASE_ADDR    0x20000
#define CONFIG_END_ADDR     0x80000
#define CONFIG_BLOCK_NUM    3
#define CONFIG_PAGE_NUM     (BLOCK_SIZE / PAGE_SIZE)

/*Monitor Area*/
#define MONITOR_BASE_ADDR   (0x80000) //0.5M START
#define MONITOR_END_ADDR    (0x400000) //4M END

/*PUKLEVEL Area*/
#define PUKLEVEL_START_NUM   (6*8-2) //6M-2*BLOCK_SIZE START
#define PUKLEVEL_END         (6*8) //6M END

/*File System Area*/
#define DATA_START_NUM         (8*8)   //8M START
#define	DATA_BLOCK_NUM			(120*8) 	//120M
#define	RESERVE_BLOCK_NUM		(8*8)  	// 8M 
#define	FS_BLOCK_NUM			(DATA_BLOCK_NUM-RESERVE_BLOCK_NUM)
#define DATA_BLOCK_MAGIC    "DATA_BLOCK_MAGIC"
#define RES_BLOCK_SIZE       0x1000         // 4KBytes  
#define DATA_MAGIC_OFFSET   (RES_BLOCK_SIZE-0x40)  //max 64Bytes

/*Bad block manage information area*/
#define BAD_INFO_START (6*8) //6M START
#define BAD_INFO_END (BAD_INFO_START+3)

/*Log record area*/
#define LOG_INFO_START (BAD_INFO_END +1)
#define LOG_INFO_END (DATA_START_NUM-1)
#define REWRITE_COPIES      4
#define ITEMS_PER_BLOCK     (BLOCK_SIZE/PAGE_SIZE)
#define DATA_LOG_SIZE       (PAGE_SIZE/REWRITE_COPIES)
#define LOG_TAG             0x30474F4C  /* "LOG0" */

typedef struct {
    uint flag ;
    ushort offset;
    ushort len;
    uchar buf[DATA_LOG_SIZE-8];
} __attribute__((__packed__)) DATALOG;


#define NAND_ID_HYNIX   0x1500a1ad
#define NAND_ID_ESMT    0x1580a1c8
#define NAND_ID_SLC     0x1590aa01
uint GetNandType();


#define SWAP_LONG(x) \
    ((unsigned int)( \
    (((unsigned int)(x) & (unsigned int)0x000000ff) << 24) | \
    (((unsigned int)(x) & (unsigned int)0x0000ff00) <<  8) | \
    (((unsigned int)(x) & (unsigned int)0x00ff0000) >>  8) | \
    (((unsigned int)(x) & (unsigned int)0xff000000) >> 24) ))

/* NAND device info staructure. */
struct nand_info {
	unsigned char  device_code;
	unsigned char  flag_offset;
	unsigned int dwidth;
	unsigned int cs;
	unsigned int addr_cycle;
	unsigned int pg_size;
	unsigned int block_size;
	unsigned int block_num;
	unsigned int spare_area;
	unsigned int boot_interface;
};
extern struct nand_info g_nand_device;

struct nand_flash_table {
	char	*partno;
	unsigned int id;
	unsigned int chip_size;
	unsigned int addr_cycle;
	unsigned int dwidth;
};

/* MkImage utility header */
typedef struct image_header {
	unsigned int        ih_magic;       /* Image Header Magic Number    */
	unsigned int        ih_hcrc;        /* Image Header CRC Checksum    */
	unsigned int        ih_time;        /* Image Creation Timestamp     */
	unsigned int        ih_size;        /* Image Data Size              */
	unsigned int        ih_load;        /* Data  Load  Address          */
	unsigned int        ih_ep;          /* Entry Point Address          */
	unsigned int        ih_dcrc;        /* Image Data CRC Checksum      */
	unsigned char         ih_os;          /* Operating System             */
	unsigned char         ih_arch;        /* CPU architecture             */
	unsigned char         ih_type;        /* Image Type                   */
	unsigned char         ih_comp;        /* Compression Type             */
	unsigned char         ih_name[32];    /* Image Name           */
} image_header_t;

#endif
