/*
 *  Copyright (C) 2013 Realtek Semiconductor Corp.
 *
 *  This program is bt_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  Module Name:
 *     hciattach_rtk.c
 *
 *  Description:
 *     H4/H5 specific initialization
 *
 *  Revision History:
 *      Date         Version      Author             Comment
 *     ----------   ---------    ---------------    -----------------------
 *     2013-06-06    1.0.0        gordon_yang        Create
 *     2013-06-18    1.0.1        lory_xu            add support for multi fw
 *     2013-06-21    1.0.2        gordon_yang        add timeout for get version cmd
 *     2013-07-01    1.0.3        lory_xu            close file handle
 *     2013-07-01    2.0          champion_chen      add IC check
 *     2013-12-16    2.1          champion_chen      fix bug in Additional packet number
 *     2013-12-25    2.2          champion_chen      open host flow control after send last fw packet
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "hciattach.h"
#include "btstack_config.h"
#include "btstack_chipset_rtl8761a.h"
#include "btstack_debug.h"

// #ifdef ENABLE_UART_H4
// #include "rtl8761atf_fw.h"
// #include "rtl8761atf_config.h"
// #elif defined ENABLE_UART_H5
#include "rtl8761atv_fw.h"
#include "rtl8761atv_config.h"
// #else
// #error "Need to ENABLE_UART_H4 or ENABLE_UART_H5"
// #endif

#define RTK_VERSION "3.1.20180725.143433"

/* #define RTL_8703A_SUPPORT */


#define BAUDRATE_4BYTES

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(d)  (d)
#define cpu_to_le32(d)  (d)
#define le16_to_cpu(d)  (d)
#define le32_to_cpu(d)  (d)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(d)  bswap_16(d)
#define cpu_to_le32(d)  bswap_32(d)
#define le16_to_cpu(d)  bswap_16(d)
#define le32_to_cpu(d)  bswap_32(d)
#else
#error "Unknown byte order"
#endif

typedef uint8_t RT_U8, *PRT_U8;
typedef int8_t RT_S8, *PRT_S8;
typedef uint16_t RT_U16, *PRT_U16;
typedef int32_t RT_S32, *PRT_S32;
typedef uint32_t RT_U32, *PRT_U32;

RT_U8 DBG_ON = 1;
#define LOG_STR     "Realtek Bluetooth"
#define RS_DBG(fmt, arg...) \
    do{ \
        if (DBG_ON) \
            log_debug("%s :" fmt "\n" , LOG_STR, ##arg); \
    }while(0)

#define RS_INFO(fmt, arg...) \
    do{ \
        log_info("%s :" fmt "\n", LOG_STR, ##arg); \
    }while(0)

#define RS_ERR(fmt, arg...) \
    do{ \
        log_error("%s ERROR: " fmt "\n", LOG_STR, ##arg); \
    }while(0)

#define HCI_COMMAND_HDR_SIZE        3
#define HCI_EVENT_HDR_SIZE          2
/* #define RTK_PATCH_LENGTH_MAX        24576	*/ //24*1024
#define RTK_PATCH_LENGTH_MAX        (40 * 1024)
#define PATCH_DATA_FIELD_MAX_SIZE   252
#define READ_DATA_SIZE              16
#define H5_MAX_RETRY_COUNT          40

#define HCI_CMD_READ_BD_ADDR                0x1009
#define HCI_VENDOR_CHANGE_BDRATE            0xfc17
#define HCI_VENDOR_READ_RTK_ROM_VERISION    0xfc6d
#define HCI_CMD_READ_LOCAL_VERISION         0x1001
#define HCI_VENDOR_READ_CHIP_TYPE           0xfc61

#define ROM_LMP_NONE            0x0000
#define ROM_LMP_8723a           0x1200
#define ROM_LMP_8723b           0x8723
#define ROM_LMP_8821a           0x8821
#define ROM_LMP_8761a           0x8761
#define ROM_LMP_8761btc		0x8763

#define ROM_LMP_8703a           0x87b3
#define ROM_LMP_8763a           0x8763
#define ROM_LMP_8703b           0x8703
#define ROM_LMP_8723c           0x87c3 /* ??????? */
#define ROM_LMP_8822b           0x8822
#define ROM_LMP_8723cs_xx       0x8704
#define ROM_LMP_8723cs_cg       0x8705
#define ROM_LMP_8723cs_vf       0x8706

/* Chip type */
#define CHIP_8703AS    1
#define CHIP_8723CS_CG 3
#define CHIP_8723CS_VF 4
#define CHIP_RTL8723CS_XX 5
#define CHIP_8703BS   7

/* software id */
#define CHIP_UNKNOWN	0x00
#define CHIP_8761AT	0x1F
#define CHIP_8761ATF	0x2F
#define CHIP_8761BTC	0x3F
#define CHIP_8761B	0x4F
#define CHIP_8723BS	0x5F
#define CHIP_BEFORE	0x6F
#define CHIP_8822BS	0x70
#define CHIP_8723DS	0x71
#define CHIP_8821CS	0x72

/* HCI data types */
#define H5_ACK_PKT              0x00
#define HCI_COMMAND_PKT         0x01
#define HCI_ACLDATA_PKT         0x02
#define HCI_SCODATA_PKT         0x03
#define HCI_EVENT_PKT           0x04
#define H5_VDRSPEC_PKT          0x0E
#define H5_LINK_CTL_PKT         0x0F

#define H5_HDR_SEQ(hdr)         ((hdr)[0] & 0x07)
#define H5_HDR_ACK(hdr)         (((hdr)[0] >> 3) & 0x07)
#define H5_HDR_CRC(hdr)         (((hdr)[0] >> 6) & 0x01)
#define H5_HDR_RELIABLE(hdr)    (((hdr)[0] >> 7) & 0x01)
#define H5_HDR_PKT_TYPE(hdr)    ((hdr)[1] & 0x0f)
#define H5_HDR_LEN(hdr)         ((((hdr)[1] >> 4) & 0xff) + ((hdr)[2] << 4))
#define H5_HDR_SIZE             4

struct sk_buff {
	RT_U32 max_len;
	RT_U32 data_len;
	RT_U8 data[0];
};

/* Skb helpers */
struct bt_skb_cb {
	RT_U8 pkt_type;
	RT_U8 incoming;
	RT_U16 expect;
	RT_U8 tx_seq;
	RT_U8 retries;
	RT_U8 sar;
	unsigned short channel;
};

typedef struct {
	uint8_t index;
	uint8_t data[252];
} __attribute__ ((packed)) download_vendor_patch_cp;

struct hci_command_hdr {
	RT_U16 opcode;
	RT_U8 plen;
} __attribute__ ((packed));

struct hci_event_hdr {
	RT_U8 evt;
	RT_U8 plen;
} __attribute__ ((packed));

struct hci_ev_cmd_complete {
	RT_U8 ncmd;
	RT_U16 opcode;
} __attribute__ ((packed));

struct rtk_bt_vendor_config_entry {
	RT_U16 offset;
	RT_U8 entry_len;
	RT_U8 entry_data[0];
} __attribute__ ((packed));

struct rtk_bt_vendor_config {
	RT_U32 signature;
	RT_U16 data_len;
	struct rtk_bt_vendor_config_entry entry[0];
} __attribute__ ((packed));

struct rtk_epatch_entry {
	RT_U16 chipID;
	RT_U16 patch_length;
	RT_U32 start_offset;
	RT_U32 svn_ver;
	RT_U32 coex_ver;
} __attribute__ ((packed));

struct rtk_epatch {
	RT_U8 signature[8];
	RT_U32 fw_version;
	RT_U16 number_of_patch;
	struct rtk_epatch_entry entry[0];
} __attribute__ ((packed));

struct rtk_extension_entry {
	uint8_t opcode;
	uint8_t length;
	uint8_t* data;
} __attribute__ ((packed));

typedef enum _RTK_ROM_VERSION_CMD_STATE {
	cmd_not_send,
	cmd_has_sent,
	event_received
} RTK_ROM_VERSION_CMD_STATE;

typedef enum _H5_RX_STATE {
	H5_W4_PKT_DELIMITER,
	H5_W4_PKT_START,
	H5_W4_HDR,
	H5_W4_DATA,
	H5_W4_CRC
} H5_RX_STATE;

typedef enum _H5_RX_ESC_STATE {
	H5_ESCSTATE_NOESC,
	H5_ESCSTATE_ESC
} H5_RX_ESC_STATE;

typedef enum _H5_LINK_STATE {
	H5_SYNC,
	H5_CONFIG,
	H5_INIT,
	H5_PATCH,
	H5_ACTIVE
} H5_LINK_STATE;

uint16_t project_id[]= {
	ROM_LMP_8723a,
	ROM_LMP_8723b, /* RTL8723BS */
	ROM_LMP_8821a, /* RTL8821AS */
	ROM_LMP_8761a, /* RTL8761ATV */

	ROM_LMP_8703a,
	ROM_LMP_8763a,
	ROM_LMP_8703b,
	ROM_LMP_8723c, /* index 7 for 8723CS. What is for other 8723CS  */
	ROM_LMP_8822b, /* RTL8822BS */
	ROM_LMP_8723b, /* RTL8723DS */
	ROM_LMP_8821a, /* id 10 for RTL8821CS, lmp subver 0x8821 */
	ROM_LMP_NONE
};

#define RTL_FW_MATCH_CHIP_TYPE  (1 << 0)
#define RTL_FW_MATCH_HCI_VER    (1 << 1)
#define RTL_FW_MATCH_HCI_REV    (1 << 2)
struct patch_info {
	uint32_t    match_flags;
	uint8_t     chip_type;
	uint16_t    lmp_subver;
	uint16_t    proj_id;
	uint8_t     hci_ver;
	uint16_t    hci_rev;
	char*        patch_file;
	char*        config_file;
	char*        ic_name;
};

static struct patch_info h4_patch_table[] = {
	/* match flags, chip type, lmp subver, proj id(unused), hci_ver,
	 * hci_rev, ...
	 */

	/* RTL8761AT */
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8761AT,
		0x8761, 0xffff, 0, 0x000a,
		"rtl8761at_fw", "rtl8761at_config", "RTL8761AT"
	},
	/* RTL8761ATF */
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8761ATF,
		0x8761, 0xffff, 0, 0x000a,
		"rtl8761atf_fw", "rtl8761atf_config", "RTL8761ATF"
	},
	/* RTL8761B TC
	 * FW/Config is not used.
	 */
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8761BTC,
		0x8763, 0xffff, 0, 0x000b,
		"rtl8761btc_fw", "rtl8761btc_config", "RTL8761BTC"
	},
	/* RTL8761B */
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8761B,
		0x8761, 0xffff, 0, 0x000b,
		"rtl8761b_fw", "rtl8761b_config", "RTL8761B"
	},

	/* RTL8723DS */
	{
		RTL_FW_MATCH_HCI_VER | RTL_FW_MATCH_HCI_REV, CHIP_8723DS,
		ROM_LMP_8723b, ROM_LMP_8723b, 8, 0x000d,
		"rtl8723dsh4_fw", "rtl8723dsh4_config", "RTL8723DSH4"
	},

	{ 0, 0, 0, ROM_LMP_NONE, 0, 0, "rtl_none_fw", "rtl_none_config", "NONE"}
};

static struct patch_info patch_table[] = {
	/* match flags, chip type, lmp subver, proj id(unused), hci_ver,
	 * hci_rev, ...
	 */

	/* RTL8723AS */
	{
		0, 0, ROM_LMP_8723a, ROM_LMP_8723a, 0, 0,
		"rtl8723a_fw", "rtl8723a_config", "RTL8723AS"
	},
	/* RTL8821CS */
	{
		RTL_FW_MATCH_HCI_REV, CHIP_8821CS,
		ROM_LMP_8821a, ROM_LMP_8821a, 0, 0x000c,
		"rtl8821c_fw", "rtl8821c_config", "RTL8821CS"
	},
	/* RTL8821AS */
	{
		0, 0, ROM_LMP_8821a, ROM_LMP_8821a, 0, 0,
		"rtl8821a_fw", "rtl8821a_config", "RTL8821AS"
	},
	/* RTL8761ATV */
	{
		0, 0, ROM_LMP_8761a, ROM_LMP_8761a, 0, 0,
		"rtl8761a_fw", "rtl8761a_config", "RTL8761ATV"
	},

	/* RTL8703AS
	 * RTL8822BS
	 * */
#ifdef RTL_8703A_SUPPORT
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8703AS,
		ROM_LMP_8723b, ROM_LMP_8723b, 0, 0,
		"rtl8703a_fw", "rtl8703a_config", "RTL8703AS"
	},
#endif
	{
		0, CHIP_8822BS, ROM_LMP_8822b, ROM_LMP_8822b, 0, 0,
		"rtl8822b_fw", "rtl8822b_config", "RTL8822BS"
	},

	/* RTL8703BS
	 * RTL8723CS_XX
	 * RTL8723CS_CG
	 * RTL8723CS_VF
	 * */
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8703BS,
		ROM_LMP_8703b, ROM_LMP_8703b, 0, 0,
		"rtl8703b_fw", "rtl8703b_config", "RTL8703BS"
	},
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_RTL8723CS_XX,
		ROM_LMP_8703b, ROM_LMP_8723cs_xx, 0, 0,
		"rtl8723cs_xx_fw", "rtl8723cs_xx_config", "RTL8723CS_XX"
	},
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8723CS_CG,
		ROM_LMP_8703b, ROM_LMP_8723cs_cg, 0, 0,
		"rtl8723cs_cg_fw", "rtl8723cs_cg_config", "RTL8723CS_CG"
	},
	{
		RTL_FW_MATCH_CHIP_TYPE, CHIP_8723CS_VF,
		ROM_LMP_8703b, ROM_LMP_8723cs_vf, 0, 0,
		"rtl8723cs_vf_fw", "rtl8723cs_vf_config", "RTL8723CS_VF"
	},

	/* RTL8723BS */
	{
		RTL_FW_MATCH_HCI_VER | RTL_FW_MATCH_HCI_REV, 0,
		ROM_LMP_8723b, ROM_LMP_8723b, 6, 0x000b,
		"rtl8723b_fw", "rtl8723b_config", "RTL8723BS"
	},
	/* RTL8723DS */
	{
		RTL_FW_MATCH_HCI_VER | RTL_FW_MATCH_HCI_REV, CHIP_8723DS,
		ROM_LMP_8723b, ROM_LMP_8723b, 8, 0x000d,
		"rtl8723d_fw", "rtl8723d_config", "RTL8723DS"
	},
	/* add entries here*/

	{ 0, 0, 0, ROM_LMP_NONE, 0, 0, "rtl_none_fw", "rtl_none_config", "NONE"}
};

typedef struct btrtl_info {
	/**********************h5 releated*************************/
	RT_U8 rxseq_txack;	/* expected rx seq number */
	RT_U8 rxack;	/* last packet sent by us that the peer ack'ed */
	RT_U8 use_crc;
	RT_U8 is_txack_req;	/* txack required */
	RT_U8 msgq_txseq;	/* next pkt seq */
	RT_U16 message_crc;
	RT_U32 rx_count;	/* expected pkts to recv */

	H5_RX_STATE rx_state;
	H5_RX_ESC_STATE rx_esc_state;
	H5_LINK_STATE link_estab_state;

	struct sk_buff* rx_skb;
	struct sk_buff* host_last_cmd;

	uint16_t num_of_cmd_sent;
	RT_U16 lmp_subver;
	uint16_t hci_rev;
	uint8_t hci_ver;
	RT_U8 eversion;
	int h5_max_retries;
	RT_U8 chip_type;

	/**********************patch related************************/
	uint32_t baudrate;
	uint8_t dl_fw_flag;
	int serial_fd;
	int hw_flow_control;
	uint32_t parity_en:   16;
	uint32_t parity_even: 16;
	int final_speed;
	int total_num;	/* total pkt number */
	int tx_index;	/* current sending pkt number */
	int rx_index;	/* ack index from board */
	int fw_len;	/* fw patch file len */
	int config_len;	/* config patch file len */
	int total_len;	/* fw & config extracted buf len */
	uint8_t* fw_buf;	/* fw patch file buf */
	uint8_t* config_buf;	/* config patch file buf */
	uint8_t* total_buf;	/* fw & config extracted buf */
	RTK_ROM_VERSION_CMD_STATE rom_version_cmd_state;
	RTK_ROM_VERSION_CMD_STATE hci_version_cmd_state;
	RTK_ROM_VERSION_CMD_STATE chip_type_cmd_state;

	struct patch_info* patch_ent;

	int proto;
} rtk_hw_cfg_t;

static rtk_hw_cfg_t rtk_hw_cfg;

void* bt_malloc(unsigned int size) {
	void* ret;
	// RS_ERR("bt_malloc size:%d", size);
	ret = malloc(size);
	// RS_ERR("bt_malloc ret:%p", ret);

	if(ret == NULL)
		while(1);

	return ret;
}

void bt_free(void* loc) {
	// RS_ERR("bt_free size:%p", loc);
	free(loc);
}



void util_hexdump(const uint8_t* buf, size_t len) {
	static const char hexdigits[] = "0123456789abcdef";
	char str[16 * 3];
	size_t i;

	if (!buf || !len) {
		return;
	}

	for (i = 0; i < len; i++) {
		str[((i % 16) * 3)] = hexdigits[buf[i] >> 4];
		str[((i % 16) * 3) + 1] = hexdigits[buf[i] & 0xf];
		str[((i % 16) * 3) + 2] = ' ';

		if ((i + 1) % 16 == 0) {
			str[16 * 3 - 1] = '\0';
			RS_INFO("%s", str);
		}
	}

	if (i % 16 > 0) {
		str[(i % 16) * 3 - 1] = '\0';
		RS_INFO("%s", str);
	}
}

#ifdef EXTRA_CONFIG_OPTION

static inline int xtalset_supported(void) {
	struct patch_info* pent = rtk_hw_cfg.patch_ent;

	/* Only support rtl8723ds, rtl8822bs and rtl8821cs xtal config */
	if (pent->chip_type != CHIP_8723DS &&
	        pent->chip_type != CHIP_8822BS &&
	        pent->chip_type != CHIP_8821CS) {
		return 0;
	}

	return 1;
}

static void line_proc(char* buff, int len) {
	char* argv[32];
	int nargs = 0;
	RS_INFO("%s", buff);

	while (nargs < 32) {
		/* skip any white space */
		while ((*buff == ' ') || (*buff == '\t') ||
		        *buff == ',') {
			++buff;
		}

		if (*buff == '\0') {
			/* end of line, no more args */
			argv[nargs] = NULL;
			break;
		}

		/* begin of argument string */
		argv[nargs++] = buff;

		/* find end of string */
		while (*buff && (*buff != ' ') && (*buff != '\t')) {
			++buff;
		}

		if (*buff == '\r' || *buff == '\n') {
			++buff;
		}

		if (*buff == '\0') {
			/* end of line, no more args */
			argv[nargs] = NULL;
			break;
		}

		*buff++ = '\0';	/* terminate current arg */
	}

	/* valid config */
	if (nargs >= 4) {
		unsigned long int offset;
		uint8_t l;
		uint8_t i = 0;
		offset = strtoul(argv[0], NULL, 16);
		offset = offset | (strtoul(argv[1], NULL, 16) << 8);
		RS_INFO("extra config offset %04lx", offset);
		l = strtoul(argv[2], NULL, 16);

		if (l != (uint8_t)(nargs - 3)) {
			RS_ERR("invalid len %u", l);
			return;
		}

		if (offset == 0x015b && l <= 4) {
			/* Tx power */
			for (i = 0; i < l; i++) {
				txpower_cfg[i] = (uint8_t)strtoul(argv[3 + i], NULL, 16);
			}

			txpower_len = l;
			config_flags |= CONFIG_TXPOWER;
		} else if (offset == 0x01e6) {
			/* XTAL for 8822B, 8821C 8723D */
			xtal_cfg = (uint8_t)strtoul(argv[3], NULL, 16);
			config_flags |= CONFIG_XTAL;
		} else {
			RS_ERR("extra config %04lx is not supported", offset);
		}
	}
}

static void config_proc(uint8_t* buff, int len) {
	uint8_t* head = buff;
	uint8_t* ptr = buff;
	int l;

	while (len > 0) {
		ptr = strchr(head, '\n');

		if (!ptr) {
			break;
		}

		*ptr++ = '\0';

		while (*head == ' ' || *head == '\t') {
			head++;
		}

		l = ptr - head;

		if (l > 0 && *head != '#') {
			line_proc(head, l);
		}

		head = ptr;
		len -= l;
	}
}

#endif

/* Get the entry from patch_table according to LMP subversion */
struct patch_info* get_patch_entry(struct btrtl_info* btrtl) {
	struct patch_info* n = NULL;

	if (btrtl->proto == HCI_UART_3WIRE) {
		n = patch_table;
	} else {
		n = h4_patch_table;
	}

	for (; n->lmp_subver; n++) {
		if ((n->match_flags & RTL_FW_MATCH_CHIP_TYPE) &&
		        n->chip_type != btrtl->chip_type) {
			continue;
		}

		if ((n->match_flags & RTL_FW_MATCH_HCI_VER) &&
		        n->hci_ver != btrtl->hci_ver) {
			continue;
		}

		if ((n->match_flags & RTL_FW_MATCH_HCI_REV) &&
		        n->hci_rev != btrtl->hci_rev) {
			continue;
		}

		if (n->lmp_subver != btrtl->lmp_subver) {
			continue;
		}

		break;
	}

	return n;
}

// bite reverse in bytes
// 00000001 -> 10000000
// 00000100 -> 00100000
const RT_U8 byte_rev_table[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static __inline RT_U8 bit_rev8(RT_U8 byte) {
	return byte_rev_table[byte];
}

static __inline RT_U16 bit_rev16(RT_U16 x) {
	return (bit_rev8(x & 0xff) << 8) | bit_rev8(x >> 8);
}

static const RT_U16 crc_table[] = {
	0x0000, 0x1081, 0x2102, 0x3183,
	0x4204, 0x5285, 0x6306, 0x7387,
	0x8408, 0x9489, 0xa50a, 0xb58b,
	0xc60c, 0xd68d, 0xe70e, 0xf78f
};

// Initialise the crc calculator
#define H5_CRC_INIT(x) x = 0xffff

/**
* Malloc the socket buffer
*
* @param skb socket buffer
* @return the point to the bt_malloc buffer
*/
static __inline struct sk_buff* skb_alloc(unsigned int len) {
	struct sk_buff* skb = NULL;

	if ((skb = bt_malloc(len + 8))) {
		skb->max_len = len;
		skb->data_len = 0;
	} else {
		//RS_ERR("Allocate skb fails!!!");
		skb = NULL;
	}

	memset(skb->data, 0, len);
	return skb;
}

/**
* Free the socket buffer
*
* @param skb socket buffer
*/
static __inline void skb_free(struct sk_buff* skb) {
	bt_free(skb);
	return;
}

/**
* Increase the date length in sk_buffer by len,
* and return the increased header pointer
*
* @param skb socket buffer
* @param len length want to increase
* @return the pointer to increased header
*/
static RT_U8* skb_put(struct sk_buff* skb, RT_U32 len) {
	RT_U32 old_len = skb->data_len;

	if ((skb->data_len + len) > (skb->max_len)) {
		RS_ERR("Buffer too small");
		return NULL;
	}

	skb->data_len += len;
	return (skb->data + old_len);
}

/**
* decrease data length in sk_buffer by to len by cut the tail
*
* @warning len should be less than skb->len
*
* @param skb socket buffer
* @param len length want to be changed
*/
static void skb_trim(struct sk_buff* skb, unsigned int len) {
	if (skb->data_len > len) {
		skb->data_len = len;
	} else {
		RS_ERR("Error: skb->data_len(%d) < len(%d)", skb->data_len,
		       len);
	}
}

/**
* Decrease the data length in sk_buffer by len,
* and move the content forward to the header.
* the data in header will be removed.
*
* @param skb socket buffer
* @param len length of data
* @return new data
*/
static RT_U8* skb_pull(struct sk_buff* skb, RT_U32 len) {
	skb->data_len -= len;
	unsigned char* buf;

	if (!(buf = bt_malloc(skb->data_len))) {
		RS_ERR("Unable to allocate file buffer");
		exit(1);
	}

	memcpy(buf, skb->data + len, skb->data_len);
	memcpy(skb->data, buf, skb->data_len);
	bt_free(buf);
	return (skb->data);
}

/**
* Add "d" into crc scope, caculate the new crc value
*
* @param crc crc data
* @param d one byte data
*/
static void h5_crc_update(RT_U16* crc, RT_U8 d) {
	RT_U16 reg = *crc;
	reg = (reg >> 4) ^ crc_table[(reg ^ d) & 0x000f];
	reg = (reg >> 4) ^ crc_table[(reg ^ (d >> 4)) & 0x000f];
	*crc = reg;
}

struct __una_u16 {
	RT_U16 x;
};
static __inline RT_U16 __get_unaligned_cpu16(const void* p) {
	const struct __una_u16* ptr = (const struct __una_u16*)p;
	return ptr->x;
}

static __inline RT_U16 get_unaligned_be16(const void* p) {
	return __get_unaligned_cpu16((const RT_U8*)p);
}

static __inline RT_U16 get_unaligned_le16(RT_U8* p) {
	return (RT_U16) (*p) + ((RT_U16) (*(p + 1)) << 8);
}

static __inline RT_U32 get_unaligned_le32(RT_U8* p) {
	return (RT_U32) (*p) + ((RT_U32) (*(p + 1)) << 8) +
	       ((RT_U32) (*(p + 2)) << 16) + ((RT_U32) (*(p + 3)) << 24);
}

/**
* Get crc data.
*
* @param h5 realtek h5 struct
* @return crc data
*/
static RT_U16 h5_get_crc(rtk_hw_cfg_t* h5) {
	RT_U16 crc = 0;
	RT_U8* data = h5->rx_skb->data + h5->rx_skb->data_len - 2;
	crc = data[1] + (data[0] << 8);
	return crc;
//    return get_unaligned_be16(&h5->rx_skb->data[h5->rx_skb->data_len - 2]);
}

/**
* Just add 0xc0 at the end of skb,
* we can also use this to add 0xc0 at start while there is no data in skb
*
* @param skb socket buffer
*/
static void h5_slip_msgdelim(struct sk_buff* skb) {
	const char pkt_delim = 0xc0;
	memcpy(skb_put(skb, 1), &pkt_delim, 1);
}

/**
* Slip ecode one byte in h5 proto, as follows:
* 0xc0 -> 0xdb, 0xdc
* 0xdb -> 0xdb, 0xdd
* 0x11 -> 0xdb, 0xde
* 0x13 -> 0xdb, 0xdf
* others will not change
*
* @param skb socket buffer
* @c pure data in the one byte
*/
static void h5_slip_one_byte(struct sk_buff* skb, RT_U8 c) {
	const RT_S8 esc_c0[2] = { 0xdb, 0xdc };
	const RT_S8 esc_db[2] = { 0xdb, 0xdd };
	const RT_S8 esc_11[2] = { 0xdb, 0xde };
	const RT_S8 esc_13[2] = { 0xdb, 0xdf };

	switch (c) {
	case 0xc0:
		memcpy(skb_put(skb, 2), &esc_c0, 2);
		break;

	case 0xdb:
		memcpy(skb_put(skb, 2), &esc_db, 2);
		break;

	case 0x11:
		memcpy(skb_put(skb, 2), &esc_11, 2);
		break;

	case 0x13:
		memcpy(skb_put(skb, 2), &esc_13, 2);
		break;

	default:
		memcpy(skb_put(skb, 1), &c, 1);
		break;
	}
}

/**
* Decode one byte in h5 proto, as follows:
* 0xdb, 0xdc -> 0xc0
* 0xdb, 0xdd -> 0xdb
* 0xdb, 0xde -> 0x11
* 0xdb, 0xdf -> 0x13
* others will not change
*
* @param h5 realtek h5 struct
* @byte pure data in the one byte
*/
static void h5_unslip_one_byte(rtk_hw_cfg_t* h5, unsigned char byte) {
	const RT_U8 c0 = 0xc0, db = 0xdb;
	const RT_U8 oof1 = 0x11, oof2 = 0x13;

	if (H5_ESCSTATE_NOESC == h5->rx_esc_state) {
		if (0xdb == byte) {
			h5->rx_esc_state = H5_ESCSTATE_ESC;
		} else {
			memcpy(skb_put(h5->rx_skb, 1), &byte, 1);

			//Check Pkt Header's CRC enable bit
			if ((h5->rx_skb->data[0] & 0x40) != 0
			        && h5->rx_state != H5_W4_CRC) {
				h5_crc_update(&h5->message_crc, byte);
			}

			h5->rx_count--;
		}
	} else if (H5_ESCSTATE_ESC == h5->rx_esc_state) {
		switch (byte) {
		case 0xdc:
			memcpy(skb_put(h5->rx_skb, 1), &c0, 1);

			if ((h5->rx_skb->data[0] & 0x40) != 0
			        && h5->rx_state != H5_W4_CRC) {
				h5_crc_update(&h5->message_crc, 0xc0);
			}

			h5->rx_esc_state = H5_ESCSTATE_NOESC;
			h5->rx_count--;
			break;

		case 0xdd:
			memcpy(skb_put(h5->rx_skb, 1), &db, 1);

			if ((h5->rx_skb->data[0] & 0x40) != 0
			        && h5->rx_state != H5_W4_CRC) {
				h5_crc_update(&h5->message_crc, 0xdb);
			}

			h5->rx_esc_state = H5_ESCSTATE_NOESC;
			h5->rx_count--;
			break;

		case 0xde:
			memcpy(skb_put(h5->rx_skb, 1), &oof1, 1);

			if ((h5->rx_skb->data[0] & 0x40) != 0
			        && h5->rx_state != H5_W4_CRC) {
				h5_crc_update(&h5->message_crc, oof1);
			}

			h5->rx_esc_state = H5_ESCSTATE_NOESC;
			h5->rx_count--;
			break;

		case 0xdf:
			memcpy(skb_put(h5->rx_skb, 1), &oof2, 1);

			if ((h5->rx_skb->data[0] & 0x40) != 0
			        && h5->rx_state != H5_W4_CRC) {
				h5_crc_update(&h5->message_crc, oof2);
			}

			h5->rx_esc_state = H5_ESCSTATE_NOESC;
			h5->rx_count--;
			break;

		default:
			RS_ERR("Error: Invalid byte %02x after esc byte", byte);
			skb_free(h5->rx_skb);
			h5->rx_skb = NULL;
			h5->rx_state = H5_W4_PKT_DELIMITER;
			h5->rx_count = 0;
			break;
		}
	}
}

/**
* Prepare h5 packet, packet format as follow:
*  | LSB 4 octets  | 0 ~4095| 2 MSB
*  |packet header | payload | data integrity check |
*
* pakcket header fromat is show below:
*  | LSB 3 bits         | 3 bits				   | 1 bits   				     | 1 bits  		  |
*  | 4 bits 		   | 12 bits     | 8 bits MSB
*  |sequence number | acknowledgement number | data integrity check present | reliable packet |
*  |packet type | payload length | header checksum
*
* @param h5 realtek h5 struct
* @param data pure data
* @param len the length of data
* @param pkt_type packet type
* @return socket buff after prepare in h5 proto
*/
static struct sk_buff* h5_prepare_pkt(rtk_hw_cfg_t* h5, RT_U8* data,
                                      RT_S32 len, RT_S32 pkt_type) {
	struct sk_buff* nskb;
	RT_U8 hdr[4];
	RT_U16 H5_CRC_INIT(h5_txmsg_crc);
	int rel, i;

	switch (pkt_type) {
	case HCI_ACLDATA_PKT:
	case HCI_COMMAND_PKT:
	case HCI_EVENT_PKT:
		rel = 1;	// reliable
		break;

	case H5_ACK_PKT:
	case H5_VDRSPEC_PKT:
	case H5_LINK_CTL_PKT:
		rel = 0;	// unreliable
		break;

	default:
		RS_ERR("Unknown packet type");
		return NULL;
	}

	// Max len of packet: (original len +4(h5 hdr) +2(crc))*2
	//   (because bytes 0xc0 and 0xdb are escaped, worst case is
	//   when the packet is all made of 0xc0 and 0xdb :) )
	//   + 2 (0xc0 delimiters at start and end).
	//RS_ERR("#### allocate %d to h5_prepare_pkt", (len + 6) * 2 + 2 + 8);
	nskb = skb_alloc((len + 6) * 2 + 2);

	if (!nskb) {
		return NULL;
	}

	//Add SLIP start byte: 0xc0
	h5_slip_msgdelim(nskb);
	// set AckNumber in SlipHeader
	hdr[0] = h5->rxseq_txack << 3;
	h5->is_txack_req = 0;

	//RS_DBG("We request packet no(%u) to card", h5->rxseq_txack);
	//RS_DBG("Sending packet with seqno %u and wait %u", h5->msgq_txseq, h5->rxseq_txack);
	if (rel) {
		// set reliable pkt bit and SeqNumber
		hdr[0] |= 0x80 + h5->msgq_txseq;
		//RS_DBG("Sending packet with seqno(%u)", h5->msgq_txseq);
		++(h5->msgq_txseq);
		h5->msgq_txseq = (h5->msgq_txseq) & 0x07;
	}

	// set DicPresent bit
	if (h5->use_crc) {
		hdr[0] |= 0x40;
	}

	// set packet type and payload length
	hdr[1] = ((len << 4) & 0xff) | pkt_type;
	hdr[2] = (RT_U8) (len >> 4);
	// set checksum
	hdr[3] = ~(hdr[0] + hdr[1] + hdr[2]);

	// Put h5 header */
	for (i = 0; i < 4; i++) {
		h5_slip_one_byte(nskb, hdr[i]);

		if (h5->use_crc) {
			h5_crc_update(&h5_txmsg_crc, hdr[i]);
		}
	}

	// Put payload */
	for (i = 0; i < len; i++) {
		h5_slip_one_byte(nskb, data[i]);

		if (h5->use_crc) {
			h5_crc_update(&h5_txmsg_crc, data[i]);
		}
	}

	// Put CRC */
	if (h5->use_crc) {
		h5_txmsg_crc = bit_rev16(h5_txmsg_crc);
		h5_slip_one_byte(nskb, (RT_U8) ((h5_txmsg_crc >> 8) & 0x00ff));
		h5_slip_one_byte(nskb, (RT_U8) (h5_txmsg_crc & 0x00ff));
	}

	// Add SLIP end byte: 0xc0
	h5_slip_msgdelim(nskb);
	return nskb;
}

/**
* Removed controller acked packet from Host's unacked lists
*
* @param h5 realtek h5 struct
*/
static void h5_remove_acked_pkt(rtk_hw_cfg_t* h5) {
	int pkts_to_be_removed = 0;
	int seqno = 0;
	int i = 0;
	seqno = h5->msgq_txseq;
	//pkts_to_be_removed = GetListLength(h5->unacked);

	while (pkts_to_be_removed) {
		if (h5->rxack == seqno) {
			break;
		}

		pkts_to_be_removed--;
		seqno = (seqno - 1) & 0x07;
	}

	if (h5->rxack != seqno) {
		RS_DBG("Peer acked invalid packet");
	}

	//skb_queue_walk_safe(&h5->unack, skb, tmp) // remove ack'ed packet from h5->unack queue
	for (i = 0; i < 5; ++i) {
		if (i >= pkts_to_be_removed) {
			break;
		}

		i++;
		//__skb_unlink(skb, &h5->unack);
		//skb_free(skb);
	}

	//  if (skb_queue_empty(&h5->unack))
	//          del_timer(&h5->th5);
	//  spin_unlock_irqrestore(&h5->unack.lock, flags);

	if (i != pkts_to_be_removed)
		RS_DBG("Removed only (%u) out of (%u) pkts", i,
		       pkts_to_be_removed);
}

/**
* Realtek send pure ack, send a packet only with an ack
*
* @param fd uart file descriptor
*
*/
static void rtk_send_pure_ack_down(int fd) {
	struct sk_buff* nskb = h5_prepare_pkt(&rtk_hw_cfg, NULL, 0, H5_ACK_PKT);
	PortSends(fd, nskb->data, nskb->data_len);
	skb_free(nskb);
	return;
}

/**
* Parse hci event command complete, pull the cmd complete event header
*
* @param skb socket buffer
*
*/
static void hci_event_cmd_complete(struct sk_buff* skb) {
	struct hci_event_hdr* hdr = (struct hci_event_hdr*)skb->data;
	struct hci_ev_cmd_complete* ev = NULL;
	RT_U16 opcode = 0;
	RT_U8 status = 0;
	//pull hdr
	skb_pull(skb, HCI_EVENT_HDR_SIZE);
	ev = (struct hci_ev_cmd_complete*)skb->data;
	opcode = le16_to_cpu(ev->opcode);
	RS_DBG("receive hci command complete event with command:%x\n", opcode);
	//pull command complete event header
	skb_pull(skb, sizeof(struct hci_ev_cmd_complete));

	switch (opcode) {
	case HCI_VENDOR_CHANGE_BDRATE:
		status = skb->data[0];
		RS_DBG("Change BD Rate with status:%x", status);
		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
		rtk_hw_cfg.link_estab_state = H5_PATCH;
		break;

	case HCI_CMD_READ_BD_ADDR:
		status = skb->data[0];
		RS_DBG("Read BD Address with Status:%x", status);

		if (!status) {
			RS_DBG("BD Address: %8x%8x", *(int*)&skb->data[1],
			       *(int*)&skb->data[5]);
		}

		break;

	case HCI_CMD_READ_LOCAL_VERISION:
		rtk_hw_cfg.hci_version_cmd_state = event_received;
		status = skb->data[0];
		RS_DBG("Read Local Version Information with Status:%x", status);

		if (0 == status) {
			rtk_hw_cfg.hci_ver = skb->data[1];
			rtk_hw_cfg.hci_rev = (skb->data[2] | skb->data[3] << 8);
			rtk_hw_cfg.lmp_subver =
			    (skb->data[7] | (skb->data[8] << 8));
			RS_DBG("HCI Version 0x%02x", rtk_hw_cfg.hci_ver);
			RS_DBG("HCI Revision 0x%04x", rtk_hw_cfg.hci_rev);
			RS_DBG("LMP Subversion 0x%04x", rtk_hw_cfg.lmp_subver);
		} else {
			RS_ERR("Read Local Version Info status error!");
			//Need to do more
		}

		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
		break;

	case HCI_VENDOR_READ_RTK_ROM_VERISION:
		rtk_hw_cfg.rom_version_cmd_state = event_received;
		status = skb->data[0];
		RS_DBG("Read RTK rom version with Status:%x", status);

		if (0 == status) {
			rtk_hw_cfg.eversion = skb->data[1];
		} else if (1 == status) {
			rtk_hw_cfg.eversion = 0;
		} else {
			RS_ERR("READ_RTK_ROM_VERISION return status error!");
			//Need to do more
		}

		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
		break;

	case HCI_VENDOR_READ_CHIP_TYPE:
		rtk_hw_cfg.chip_type_cmd_state = event_received;
		status = skb->data[0];
		RS_DBG("Read RTK chip type with Status:%x", status);

		if (0 == status) {
			rtk_hw_cfg.chip_type= (skb->data[1] & 0x0f);
		} else {
			RS_ERR("READ_RTK_CHIP_TYPE return status error!");
		}

		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
		break;

	default:
		return;
	}

	rtk_hw_cfg.num_of_cmd_sent++;
}

/**
* Check if it's a hci frame, if it is, complete it with response or parse the cmd complete event
*
* @param skb socket buffer
*
*/
static void hci_recv_frame(struct sk_buff* skb) {
	int len;
	unsigned char h5sync[2] = { 0x01, 0x7E }, h5syncresp[2] = {
		0x02, 0x7D
	}, h5_sync_resp_pkt[0x8] = {
		0xc0, 0x00, 0x2F, 0x00, 0xD0, 0x02, 0x7D, 0xc0
	},
	h5_conf_resp_pkt_to_Ctrl[0x8] = {
		0xc0, 0x00, 0x2F, 0x00, 0xD0, 0x04, 0x7B, 0xc0
	}, h5conf[3] = {
		0x03, 0xFC, 0x10
	}, h5confresp[3] = {
		0x04, 0x7B, 0x10
	}, cmd_complete_evt_code = 0xe;

	if (rtk_hw_cfg.link_estab_state == H5_SYNC) {
		if (!memcmp(skb->data, h5sync, 2)) {
			RS_DBG("Get SYNC Pkt\n");
			len =
			    PortSends(rtk_hw_cfg.serial_fd, &h5_sync_resp_pkt, 0x8);
		} else if (!memcmp(skb->data, h5syncresp, 2)) {
			RS_DBG("Get SYNC Resp Pkt\n");
			rtk_hw_cfg.link_estab_state = H5_CONFIG;
		}

		skb_free(skb);
	} else if (rtk_hw_cfg.link_estab_state == H5_CONFIG) {
		if (!memcmp(skb->data, h5sync, 0x2)) {
			len =
			    PortSends(rtk_hw_cfg.serial_fd, &h5_sync_resp_pkt, 0x8);
			RS_DBG("Get SYNC pkt-active mode\n");
		} else if (!memcmp(skb->data, h5conf, 0x2)) {
			len =
			    PortSends(rtk_hw_cfg.serial_fd,
			              &h5_conf_resp_pkt_to_Ctrl, 0x8);
			RS_DBG("Get CONFG pkt-active mode\n");
		} else if (!memcmp(skb->data, h5confresp, 0x2)) {
			RS_DBG("Get CONFG resp pkt-active mode\n");
			rtk_hw_cfg.link_estab_state = H5_INIT;
		} else {
			RS_DBG("H5_CONFIG receive event\n");
			rtk_send_pure_ack_down(rtk_hw_cfg.serial_fd);
		}

		skb_free(skb);
	} else if (rtk_hw_cfg.link_estab_state == H5_INIT) {
		if (skb->data[0] == cmd_complete_evt_code) {
			hci_event_cmd_complete(skb);
		}

		rtk_send_pure_ack_down(rtk_hw_cfg.serial_fd);
		DelayMs(10);
		rtk_send_pure_ack_down(rtk_hw_cfg.serial_fd);
		DelayMs(10);
		rtk_send_pure_ack_down(rtk_hw_cfg.serial_fd);
		skb_free(skb);
	} else if (rtk_hw_cfg.link_estab_state == H5_PATCH) {
		if (skb->data[0] != 0x0e) {
			RS_DBG("Received event 0x%x\n", skb->data[0]);
			skb_free(skb);
			rtk_send_pure_ack_down(rtk_hw_cfg.serial_fd);
			return;
		}

		rtk_hw_cfg.rx_index = skb->data[6];
		RS_DBG("rtk_hw_cfg.rx_index %d\n", rtk_hw_cfg.rx_index);

		/* Download fw/config done */
		if (rtk_hw_cfg.rx_index & 0x80) {
			rtk_hw_cfg.rx_index &= ~0x80;
			rtk_hw_cfg.link_estab_state = H5_ACTIVE;
		}

		skb_free(skb);
	} else {
		RS_ERR("receive packets in active state");
		skb_free(skb);
	}
}

/**
* after rx data is parsed, and we got a rx frame saved in h5->rx_skb,
* this routinue is called.
* things todo in this function:
* 1. check if it's a hci frame, if it is, complete it with response or ack
* 2. see the ack number, bt_free acked frame in queue
* 3. reset h5->rx_state, set rx_skb to null.
*
* @param h5 realtek h5 struct
*
*/
static void h5_complete_rx_pkt(rtk_hw_cfg_t* h5) {
	int pass_up = 1;
	uint8_t* h5_hdr = NULL;
	h5_hdr = (uint8_t*) (h5->rx_skb->data);

	if (H5_HDR_RELIABLE(h5_hdr)) {
		RS_ERR("Received reliable seqno %u from card", h5->rxseq_txack);
		h5->rxseq_txack = H5_HDR_SEQ(h5_hdr) + 1;
		h5->rxseq_txack %= 8;
		h5->is_txack_req = 1;
	}

	h5->rxack = H5_HDR_ACK(h5_hdr);

	switch (H5_HDR_PKT_TYPE(h5_hdr)) {
	case HCI_ACLDATA_PKT:
	case HCI_EVENT_PKT:
	case HCI_SCODATA_PKT:
	case HCI_COMMAND_PKT:
	case H5_LINK_CTL_PKT:
		pass_up = 1;
		break;

	default:
		pass_up = 0;
		break;
	}

	h5_remove_acked_pkt(h5);

	if (pass_up) {
		skb_pull(h5->rx_skb, H5_HDR_SIZE);
		hci_recv_frame(h5->rx_skb);
	} else {
		skb_free(h5->rx_skb);
	}

	h5->rx_state = H5_W4_PKT_DELIMITER;
	h5->rx_skb = NULL;
}

/**
* Parse the receive data in h5 proto.
*
* @param h5 realtek h5 struct
* @param data point to data received before parse
* @param count num of data
* @return reserved count
*/
static int h5_recv(rtk_hw_cfg_t* h5, void* data, int count) {
	unsigned char* ptr;
	RS_DBG("zf:count %d rx_state %d rx_count %ld", count, h5->rx_state, h5->rx_count);
	ptr = (unsigned char*)data;

	while (count) {
		if (h5->rx_count) {
			if (*ptr == 0xc0) {
				RS_ERR("short h5 packet");
				skb_free(h5->rx_skb);
				h5->rx_state = H5_W4_PKT_START;
				h5->rx_count = 0;
			} else {
				h5_unslip_one_byte(h5, *ptr);
			}

			ptr++;
			count--;
			continue;
		}

		//RS_DBG("h5_recv h5->rx_state:%d count:%d ptr:%x", h5->rx_state,count,*ptr);

		switch (h5->rx_state) {
		case H5_W4_HDR:

			/* check header checksum. see Core Spec V4 "3-wire uart" page 67 */
			if ((0xff & (RT_U8) ~
			        (h5->rx_skb->data[0] + h5->rx_skb->data[1] +
			         h5->rx_skb->data[2])) != h5->rx_skb->data[3]) {
				RS_ERR("h5 hdr checksum error!!!");
				skb_free(h5->rx_skb);
				h5->rx_state = H5_W4_PKT_DELIMITER;
				h5->rx_count = 0;
				continue;
			}

			/* reliable pkt  & h5->hdr->SeqNumber != h5->Rxseq_txack */
			if (h5->rx_skb->data[0] & 0x80
			        && (h5->rx_skb->data[0] & 0x07) !=
			        h5->rxseq_txack) {
				RS_ERR("Out-of-order packet arrived, got(%u)expected(%u)",
				       h5->rx_skb->data[0] & 0x07,
				       h5->rxseq_txack);
				h5->is_txack_req = 1;
				skb_free(h5->rx_skb);
				h5->rx_state = H5_W4_PKT_DELIMITER;
				h5->rx_count = 0;

				/* depend on weather remote will reset ack numb or not!!!!!!special */
				if (rtk_hw_cfg.tx_index == rtk_hw_cfg.total_num) {
					rtk_hw_cfg.rxseq_txack =
					    h5->rx_skb->data[0] & 0x07;
				}

				continue;
			}

			h5->rx_state = H5_W4_DATA;
			h5->rx_count =
			    (h5->rx_skb->data[1] >> 4) +
			    (h5->rx_skb->data[2] << 4);
			continue;

		case H5_W4_DATA:

			/* pkt with crc */
			if (h5->rx_skb->data[0] & 0x40) {
				h5->rx_state = H5_W4_CRC;
				h5->rx_count = 2;
			} else {
				DelayMs(10);
				h5_complete_rx_pkt(h5);
			}

			continue;

		case H5_W4_CRC:
			if (bit_rev16(h5->message_crc) != h5_get_crc(h5)) {
				RS_ERR
				("Checksum failed, computed(%04x)received(%04x)",
				 bit_rev16(h5->message_crc),
				 h5_get_crc(h5));
				skb_free(h5->rx_skb);
				h5->rx_state = H5_W4_PKT_DELIMITER;
				h5->rx_count = 0;
				continue;
			}

			skb_trim(h5->rx_skb, h5->rx_skb->data_len - 2);
			h5_complete_rx_pkt(h5);
			continue;

		case H5_W4_PKT_DELIMITER:
			switch (*ptr) {
			case 0xc0:
				h5->rx_state = H5_W4_PKT_START;
				break;

			default:
				break;
			}

			ptr++;
			count--;
			break;

		case H5_W4_PKT_START:
			switch (*ptr) {
			case 0xc0:
				ptr++;
				count--;
				break;

			default:
				h5->rx_state = H5_W4_HDR;
				h5->rx_count = 4;
				h5->rx_esc_state = H5_ESCSTATE_NOESC;
				H5_CRC_INIT(h5->message_crc);
				// Do not increment ptr or decrement count
				// Allocate packet. Max len of a H5 pkt=
				// 0xFFF (payload) +4 (header) +2 (crc)
				h5->rx_skb = skb_alloc(0x1005);

				if (!h5->rx_skb) {
					RS_DBG("h5->rx_skb:%p and return 0",h5->rx_skb);
					h5->rx_state = H5_W4_PKT_DELIMITER;
					h5->rx_count = 0;
					return 0;
				}

				break;
			}

			break;

		default:
			break;
		}
	}

	return count;
}

/**
* Read data to buf from uart.
*
* @param fd uart file descriptor
* @param buf point to the addr where read data stored
* @param count num of data want to read
* @return num of data successfully read
*/
static int read_check_rtk(int fd, void* buf, int count,  struct sk_buff* nskb, int timeout_ms) {
	int res;
	unsigned char* ptr;
	ptr = (unsigned char*)buf;

	do {
		//res = PortRecvs(fd, buf, count, 5000);
		res = PortRecvs_std(fd, buf, count, 250);

		//RS_DBG("PortRecvs: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8],ptr[9],ptr[10],ptr[11],ptr[12],ptr[13],ptr[14],ptr[15]);
		//RS_DBG("res:%d %x",res,res);
		if (res != -1) {
			buf = (RT_U8*) buf + res;
			count -= res;
			return res;
		}

		DelayMs(timeout_ms);
		PortSends(rtk_hw_cfg.serial_fd, nskb->data, nskb->data_len);
	} while (count);

	return res;
}





/**
* Download patch using hci. For h5 proto, not recv reply for 2s will timeout.
* Call h5_tpatch_sig_alarm for retry.
*
* @param dd uart file descriptor
* @param index current index
* @param data point to the config file
* @param len current buf length
* @return #0 on success
*
*/
static int hci_download_patch(int dd, int index, uint8_t* data, int len) {
	unsigned char hcipatch[256] = { 0x20, 0xfc, 00 };
	unsigned char bytes[READ_DATA_SIZE];
	int retlen;
	download_vendor_patch_cp cp;
	memset(&cp, 0, sizeof(cp));
	cp.index = index;

	if (data != NULL) {
		memcpy(cp.data, data, len);
	}

	if (index & 0x80) {
		rtk_hw_cfg.tx_index = index & 0x7f;
	} else {
		rtk_hw_cfg.tx_index = index;
	}

	hcipatch[2] = len + 1;
	memcpy(hcipatch + 3, &cp, len + 1);
	struct sk_buff* nskb = h5_prepare_pkt(&rtk_hw_cfg, hcipatch, len + 4, HCI_COMMAND_PKT);	//data:len+head:4

	if (rtk_hw_cfg.host_last_cmd) {
		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
	}

	rtk_hw_cfg.host_last_cmd = nskb;
	len = PortSends(dd, nskb->data, nskb->data_len);

	if(len == 0) {
		RS_DBG("PortSends success len:%d", len);
	}

	while (rtk_hw_cfg.rx_index != rtk_hw_cfg.tx_index) {	//receive data and wait last pkt
		if ((retlen = read_check_rtk(dd, &bytes, READ_DATA_SIZE,
		                             rtk_hw_cfg.host_last_cmd, 3000)) == -1) {
			RS_ERR("read fail\n");
			return -1;
		}
		h5_recv(&rtk_hw_cfg, &bytes, retlen);
	}

	return 0;
}

#define READ_TRY_MAX	6
int os_read(int fd, uint8_t* buff, int len) {
	int n;
	int i;

	int try = 0;

	i = 0;
	n = 0;

	while (n < len) {
		i = PortRecvs(fd, buff + n, len - n, 250);

		if (i > 0) {
			n += i;
		} else if (i == 0) {
			RS_DBG("read nothing.");
			continue;
		} else {
			try++;

			if (try > READ_TRY_MAX) {
					RS_ERR("read reaches max try number.\n");
					return -1;
				}

			continue;
		}
	}

	return n;
}

#define DUMP_HCI_EVT
#ifdef DUMP_HCI_EVT
#define HCI_DUMP_BUF_LEN 128
static char hci_dump_buf[HCI_DUMP_BUF_LEN];
void hci_dump_evt(uint8_t* buf, uint16_t len) {
	int n;
	int i;

	if (!buf || !len) {
		RS_ERR("Invalid parameters %p, %u.\n", buf, len);
		return;
	}

	n = 0;

	for (i = 0; i < len; i++) {
		n += sprintf(hci_dump_buf + n, "%02x ", buf[i]);

		if ((i + 1) % 16 == 0) {
			RS_DBG(" %s\n", hci_dump_buf);
			n = 0;
		}
	}

	if (i % 16) {
		RS_DBG(" %s\n", hci_dump_buf);
	}
}
#endif

int read_hci_evt(int fd, uint8_t* buff, uint8_t evt_code,
                 void* cmd, size_t size, int timeout_ms) {
	uint8_t* evt_buff = buff;
	int ret;
	int try_type = 0;
	uint8_t vendor_evt = 0xff;
start_read:

	do {
		if (cmd != NULL && size != 0) {
			PortSends(fd, cmd, size);
		}

		DelayMs(timeout_ms);
		ret = os_read(fd, evt_buff, 1);

		if (ret == 1 && evt_buff[0] == 0x04) {
			break;
		} else {
			RS_DBG("no pkt type, continue.");
			try_type++;
			continue;
		}
	} while (try_type < 6);

	if (try_type >= 6) {
		return -1;
	}

	ret = os_read(fd, evt_buff + 1, 1);

	if (ret < 0) {
		RS_ERR("%s: failed to read event code\n", __func__);
		return -1;
	}

	ret = os_read(fd, evt_buff + 2, 1);

	if (ret < 0) {
		RS_ERR("%s: failed to read parameter total len.\n", __func__);
		return -1;
	}

	ret = os_read(fd, evt_buff + 3, evt_buff[2]);

	if (ret < 0) {
		RS_ERR("%s: failed to read payload of event.\n", __func__);
		return -1;
	}

#ifdef DUMP_HCI_EVT
	hci_dump_evt(evt_buff, ret + 3);
#endif

	/* This event to wake up host. */
	if (evt_buff[1] == vendor_evt) {
		try_type = 0;
		RS_DBG("%s: found vendor evt, continue reading.\n", __func__);
		goto start_read;
	}

	if (evt_buff[1] != evt_code) {
		RS_ERR("%s: event code mismatches, %x, expect %x.\n",
		       __func__, evt_buff[1], evt_code);
		return -1;
	}

	return (ret + 3);
}

/**
* Download h4 patch
*
* @param dd uart file descriptor
* @param index current index
* @param data point to the config file
* @param len current buf length
* @return ret_index
*
*/
static int hci_download_patch_h4(int dd, int index, uint8_t* data, int len) {
	unsigned char bytes[257] = { 0 };
	unsigned char buf[257] = { 0x01, 0x20, 0xfc, 00 };
	uint16_t readbytes = 0;
	int cur_index = index;
	int ret_Index = -1;
	uint16_t res = 0;
	int i = 0;
	size_t total_len;
	uint16_t w_len;
	uint8_t rstatus;
	int ret;
	uint8_t opcode[2] = {
		0x20, 0xfc,
	};
	RS_DBG("dd:%d, index:%d, len:%d", dd, index, len);

	if (NULL != data) {
		memcpy(&buf[5], data, len);
	}

	buf[3] = len + 1;
	buf[4] = cur_index;
	total_len = len + 5;
	w_len = PortSends(dd, buf, total_len);
	RS_DBG("h4 write success with len: %d\n", w_len);
	ret = read_hci_evt(dd, bytes, 0x0e, NULL, 0, 1);

	if (ret < 0) {
		RS_ERR("%s: read hci evt error.\n", __func__);
		return -1;
	}

	/* RS_DBG("%s: bytes: %x %x %x %x %x %x.\n",
	 *      __func__, bytes[0], bytes[1], bytes[2],
	 *      bytes[3], bytes[4], bytes[5]); */

	if ((0x04 == bytes[0]) && (opcode[0] == bytes[4])
	        && (opcode[1] == bytes[5])) {
		ret_Index = bytes[7];
		rstatus = bytes[6];
		RS_DBG("---->ret_Index:%d, ----->rstatus:%d\n", ret_Index,
		       rstatus);

		if (0x00 != rstatus) {
			RS_ERR("---->read event status is wrong\n");
			return -1;
		}
	} else {
		RS_ERR("==========>Didn't read curret data\n");
		return -1;
	}

	return ret_Index;
}





#ifdef USE_CUSTOMER_ADDRESS
int bachk(const char* str) {
	if (!str) {
		return -1;
	}

	if (strlen(str) != 17) {
		return -1;
	}

	while (*str) {
		if (!isxdigit(*str++)) {
			return -1;
		}

		if (!isxdigit(*str++)) {
			return -1;
		}

		if (*str == 0) {
			break;
		}

		if (*str++ != ':') {
			return -1;
		}
	}

	return 0;
}
/**
* get random realtek Bluetooth addr.
*
* @param bt_addr where bt addr is stored
*
*/
/* static void rtk_get_ram_addr(char bt_addr[0])
 * {
 * 	srand(time(NULL) + getpid() + getpid() * 987654 + rand());
 *
 * 	RT_U32 addr = rand();
 * 	memcpy(bt_addr, &addr, sizeof(RT_U8));
 * }
 */

/**
* Write the random bt addr to the file /data/misc/bluetoothd/bt_mac/btmac.txt.
*
* @param bt_addr where bt addr is stored
*
*/
/* static void rtk_write_btmac2file(char bt_addr[6])
 * {
 * 	int fd;
 * 	fd = open(BT_ADDR_FILE, O_CREAT | O_RDWR | O_TRUNC);
 *
 * 	if (fd > 0) {
 * 		chmod(BT_ADDR_FILE, 0666);
 * 		char addr[18] = { 0 };
 * 		addr[17] = '\0';
 * 		sprintf(addr, "%2x:%2x:%2x:%2x:%2x:%2x", bt_addr[0], bt_addr[1],
 * 			bt_addr[2], bt_addr[3], bt_addr[4], bt_addr[5]);
 * 		write(fd, addr, strlen(addr));
 * 		close(fd);
 * 	} else {
 * 		RS_ERR("open file error:%s\n", BT_ADDR_FILE);
 * 	}
 * }
 */
#endif





/**
* Init realtek Bluetooth h5 proto. h5 proto is added by realtek in the right kernel.
* Generally there are two steps: h5 sync and h5 config
*
* @param fd uart file descriptor
* @param ti termios struct
*
*/
int rtk_init_h5(int fd) {
	unsigned char bytes[READ_DATA_SIZE];
	int retlen;
	int ret;
	unsigned char h5sync[2] = { 0x01, 0x7E };
	rtk_hw_cfg.h5_max_retries = H5_MAX_RETRY_COUNT;
	struct sk_buff* nskb =
	    h5_prepare_pkt(&rtk_hw_cfg, h5sync, sizeof(h5sync),
	                   H5_LINK_CTL_PKT);
	/* h5 sync */
	rtk_hw_cfg.link_estab_state = H5_SYNC;
	
	//ret = PortSends(rtk_hw_cfg.serial_fd, nskb->data, nskb->data_len);
	log_debug("nskb->data:%s nskb->data_len:%d READ_DATA_SIZE:%d", nskb->data, nskb->data_len, READ_DATA_SIZE);
	ret = PortSends_std(rtk_hw_cfg.serial_fd, nskb->data, nskb->data_len);
	if(0 == ret) {
		log_debug("PortSends success ret:%d", ret);
	}

	while (rtk_hw_cfg.link_estab_state == H5_SYNC) {
		if ((retlen = read_check_rtk(fd, &bytes, READ_DATA_SIZE,
		                             nskb, 250)) == -1) {
			RS_ERR("H5 Read Sync Response Failed");
			return -1;
		}
		
		log_debug("rtk_init_h5 1111");
		h5_recv(&rtk_hw_cfg, &bytes, retlen);
	}

	skb_free(nskb);
	nskb = NULL;
	unsigned char h5conf[3] = { 0x03, 0xFC, 0x14 };
	nskb =
	    h5_prepare_pkt(&rtk_hw_cfg, h5conf, 3, H5_LINK_CTL_PKT);
	/* h5 config */
	PortSends(rtk_hw_cfg.serial_fd, nskb->data, nskb->data_len);

	while (rtk_hw_cfg.link_estab_state == H5_CONFIG) {
		if ((retlen = read_check_rtk(fd, &bytes, READ_DATA_SIZE,
		                             nskb, 250)) == -1) {
			RS_ERR("H5 Read Config Response Failed");
			return -1;
		}
		log_debug("rtk_init_h5 2222");
		h5_recv(&rtk_hw_cfg, &bytes, retlen);
	}

	rtk_send_pure_ack_down(fd);
	RS_DBG("H5 init finished\n");
	rtk_hw_cfg.rom_version_cmd_state = cmd_not_send;
	rtk_hw_cfg.hci_version_cmd_state = cmd_not_send;
	return 0;
}

/**
* Download realtek firmware and config file from uart with the proto.
* Parse the content to serval packets follow the proto and then write the packets from uart
*
* @param fd uart file descriptor
* @param buf addr where stor the content of firmware and config file
* @param filesize length of buf
* @param is_sent_changerate if baudrate need to be changed
* @param proto realtek Bluetooth protocol, shall be either HCI_UART_H4 or HCI_UART_3WIRE
*
*/
static int rtk_download_fw_config(int fd, RT_U8* buf, size_t filesize,
                                  int is_sent_changerate, int proto) {
	uint8_t iCurIndex = 0;
	uint8_t iCurLen = 0;
	uint8_t iEndIndex = 0;
	uint8_t iLastPacketLen = 0;
	uint8_t iAdditionPkt = 0;
	uint8_t iTotalIndex = 0;
	uint8_t iCmdSentNum = 0;
	unsigned char* bufpatch;
	uint8_t i, j;
	iEndIndex = (uint8_t) ((filesize - 1) / PATCH_DATA_FIELD_MAX_SIZE);
	iLastPacketLen = (filesize) % PATCH_DATA_FIELD_MAX_SIZE;
	/* if (is_sent_changerate)
	 * 	iCmdSentNum++;
	 * if (rtk_hw_cfg.rom_version_cmd_state >= cmd_has_sent)
	 * 	iCmdSentNum++;
	 * if (rtk_hw_cfg.hci_version_cmd_state >= cmd_has_sent)
	 * 	iCmdSentNum++; */
	iCmdSentNum += rtk_hw_cfg.num_of_cmd_sent;
	iAdditionPkt =
	    (iEndIndex + 1 + iCmdSentNum) % 8 ? (8 -
	            (iEndIndex + 1 +
	             iCmdSentNum) % 8) : 0;
	iTotalIndex = iAdditionPkt + iEndIndex;
	rtk_hw_cfg.total_num = iTotalIndex;	//init TotalIndex
	RS_DBG("iEndIndex:%d  iLastPacketLen:%d iAdditionpkt:%d\n", iEndIndex,
	       iLastPacketLen, iAdditionPkt);

	if (iLastPacketLen == 0) {
		iLastPacketLen = PATCH_DATA_FIELD_MAX_SIZE;
	}

	bufpatch = buf;

	for (i = 0; i <= iTotalIndex; i++) {
		/* Index will roll over when it reaches 0x80. */
		if (i > 0x7f) {
			j = (i & 0x7f) + 1;
		} else {
			j = i;
		}

		if (i < iEndIndex) {
			iCurIndex = j;
			iCurLen = PATCH_DATA_FIELD_MAX_SIZE;
		} else if (i == iEndIndex) {
			/* Send last data packets */
			if (i == iTotalIndex) {
				iCurIndex = j | 0x80;
			} else {
				iCurIndex = j;
			}

			iCurLen = iLastPacketLen;
		} else if (i < iTotalIndex) {
			/* Send additional packets */
			iCurIndex = j;
			bufpatch = NULL;
			iCurLen = 0;
			RS_DBG("Send additional packet %u", iCurIndex);
		} else {
			/* Send end packet */
			iCurIndex = j | 0x80;
			bufpatch = NULL;
			iCurLen = 0;
			RS_DBG("Send end packet %u", iCurIndex);
		}

		if (iCurIndex & 0x80) {
			RS_DBG("Send FW last command");
		}

		if (proto == HCI_UART_H4) {
			iCurIndex = hci_download_patch_h4(fd, iCurIndex,
			                                  bufpatch, iCurLen);

			if ((iCurIndex != j) && (i != rtk_hw_cfg.total_num)) {
				RS_DBG(
				    "index mismatch j %d, iCurIndex:%d, fail\n",
				    j, iCurIndex);
				return -1;
			}
		} else if (proto == HCI_UART_3WIRE) {
			if (hci_download_patch(fd, iCurIndex, bufpatch, iCurLen) < 0) {
				return -1;
			}
		}

		if (iCurIndex < iEndIndex) {
			bufpatch += PATCH_DATA_FIELD_MAX_SIZE;
		}
	}

	/* Set last ack packet down */
	if (proto == HCI_UART_3WIRE) {
		rtk_send_pure_ack_down(fd);
	}

	return 0;
}


//These two function(rtk<-->uart speed transfer) need check Host uart speed at first!!!! IMPORTANT
//add more speed if neccessary
typedef struct _baudrate_ex {
	RT_U32 rtk_speed;
	int uart_speed;
} baudrate_ex;

#ifdef BAUDRATE_4BYTES
baudrate_ex baudrates[] = {
#ifdef RTL_8703A_SUPPORT
	{0x00004003, 1500000}, /* for rtl8703as */
#endif
	{0x0252C014, 115200},
	{0x0252C00A, 230400},
	{0x05F75004, 921600},
	{0x00005004, 1000000},
	{0x04928002, 1500000},
	{0x01128002, 1500000},	//8761AT
	{0x00005002, 2000000},
	{0x0000B001, 2500000},
	{0x04928001, 3000000},
	{0x052A6001, 3500000},
	{0x00005001, 4000000},
};
#else
baudrate_ex baudrates[] = {
	{0x701d, 115200}
	{0x6004, 921600},
	{0x4003, 1500000},
	{0x5002, 2000000},
	{0x8001, 3000000},
	{0x9001, 3000000},
	{0x7001, 3500000},
	{0x5001, 4000000},
};
#endif

/**
* Change uart speed to realtek Bluetooth speed. It is matching in the struct baudrates:
*
* @code
* baudrate_ex baudrates[] =
* {
*  	{0x7001, 3500000},
*	{0x6004, 921600},
*	{0x4003, 1500000},
*	{0x5001, 4000000},
*	{0x5002, 2000000},
*	{0x8001, 3000000},
*	{0x701d, 115200}
* };
* @endcode
*
* If there is no match in baudrates, realtek Bluetooth speed will be set as #0x701D.
*
* @param uart_speed uart speed
* @param rtk_speed realtek Bluetooth speed
*
*/
static inline void uart_speed_to_rtk_speed(int uart_speed, RT_U32* rtk_speed) {
	*rtk_speed = 0x701D;
	unsigned int i;

	for (i = 0; i < sizeof(baudrates) / sizeof(baudrate_ex); i++) {
		if (baudrates[i].uart_speed == uart_speed) {
			*rtk_speed = baudrates[i].rtk_speed;
			return;
		}
	}

	return;
}



/**
* Send vendor cmd to get eversion: 0xfc6d
* If Rom code does not support this cmd, use default.
*/
void rtk_get_eversion(int dd) {
	unsigned char bytes[READ_DATA_SIZE];
	int retlen;
	unsigned char read_rom_patch_cmd[3] = { 0x6d, 0xfc, 00 };
	struct sk_buff* nskb =
	    h5_prepare_pkt(&rtk_hw_cfg, read_rom_patch_cmd, 3, HCI_COMMAND_PKT);

	if (rtk_hw_cfg.host_last_cmd) {
		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
	}

	rtk_hw_cfg.host_last_cmd = nskb;
	PortSends(dd, nskb->data, nskb->data_len);
	rtk_hw_cfg.rom_version_cmd_state = cmd_has_sent;
	RS_DBG("RTK send HCI_Vendor_Read_RTK_ROM_Verision command\n");

	while (rtk_hw_cfg.rom_version_cmd_state != event_received) {
		if ((retlen = read_check_rtk(dd, &bytes, READ_DATA_SIZE,
		                             rtk_hw_cfg.host_last_cmd, 3000)) == -1) {
			RS_DBG("rtk get eversion: read fail");
			return;
		}
		log_debug("rtk_get_eversion zf");
		h5_recv(&rtk_hw_cfg, &bytes, retlen);
	}

	return;
}


void rtk_get_lmp_version(int dd) {
	unsigned char bytes[READ_DATA_SIZE];
	int retlen;
	unsigned char read_rom_patch_cmd[3] = { 0x01, 0x10, 00 };
	struct sk_buff* nskb = h5_prepare_pkt(&rtk_hw_cfg, read_rom_patch_cmd, 3, HCI_COMMAND_PKT);	//data:len+head:4

	if (rtk_hw_cfg.host_last_cmd) {
		skb_free(rtk_hw_cfg.host_last_cmd);
		rtk_hw_cfg.host_last_cmd = NULL;
	}

	rtk_hw_cfg.host_last_cmd = nskb;
	PortSends(dd, nskb->data, nskb->data_len);
	rtk_hw_cfg.hci_version_cmd_state = cmd_has_sent;
	RS_DBG("RTK send HCI_Read_Local_Version_Information command\n");

	while (rtk_hw_cfg.hci_version_cmd_state != event_received) {
		if ((retlen = read_check_rtk(dd, &bytes, READ_DATA_SIZE,
		                             rtk_hw_cfg.host_last_cmd, 3000)) == -1) {
			RS_ERR("read fail");
			return;
		}
		
		log_debug("rtk_get_lmp_version zf");
		RS_ERR("[%s]", __func__);
		h5_recv(&rtk_hw_cfg, &bytes, retlen);
		RS_ERR("[%s]", __func__);
	}

	return;
}

static int rtk_max_retries = 5;


static void rtk_hci_local_ver(int fd) {
	uint8_t result[258];
	int ret;
	uint8_t cmd[4] = { 0x01, 0x01, 0x10, 0x00 };

	while (1) {
		ret = read_hci_evt(fd, result, 0x0e, cmd, sizeof(cmd),
		                   1000);

		/* break down is not needed if read fails, because the program
		 * will exit when alarm timeout
		 */
		if (ret < 0) {
			RS_ERR("%s: Read HCI event error.", __func__);
		} else {
			break;
		}
	}

	if (ret != 15) {
		RS_ERR("%s: incorrect complete event, len %u", __func__, ret);
		return;
	}

	if (result[6]) {
		RS_ERR("%s: status is %u", __func__, result[6]);
		return;
	}

	rtk_hw_cfg.hci_ver = result[7];
	rtk_hw_cfg.hci_rev = (uint32_t)result[9] << 8 | result[8];
	rtk_hw_cfg.lmp_subver = (uint32_t)result[14] << 8 | result[13];
}


static void rtk_hci_rom_ver(int fd) {
	uint8_t result[256];
	int ret;
	uint8_t cmd[4] = { 0x01, 0x6d, 0xfc, 0x00 };

	while (1) {
		ret = read_hci_evt(fd, result, 0x0e, cmd, sizeof(cmd), 1000);

		/* break down is not needed if read fails, because the program
		 * will exit when alarm timeout
		 */
		if (ret < 0) {
			RS_ERR("%s: Read HCI event error.", __func__);
		} else {
			break;
		}
	}

	if (ret != 8) {
		RS_ERR("%s: incorrect complete event, len %u", __func__, ret);
		return;
	}

	if (result[6]) {
		RS_ERR("%s: status is %u", __func__, result[6]);
		rtk_hw_cfg.eversion = 0;
	} else {
		rtk_hw_cfg.eversion = result[7];
	}
}

uint8_t rtk_get_fw_project_id(uint8_t* p_buf) {
	uint8_t opcode;
	uint8_t len;
	uint8_t data = 0;

	do {
		opcode = *p_buf;
		len = *(p_buf - 1);

		if (opcode == 0x00) {
			if (len == 1) {
				data = *(p_buf - 2);
				RS_DBG
				("rtk_get_fw_project_id: opcode %d, len %d, data %d",
				 opcode, len, data);
				break;
			} else {
				RS_ERR("rtk_get_fw_project_id: invalid len %d",
				       len);
			}
		}

		p_buf -= len + 2;
	} while (*p_buf != 0xFF);

	return data;
}



struct rtk_epatch_entry* rtk_get_patch_entry(void) {
	uint16_t i;
	struct rtk_epatch* patch;
	struct rtk_epatch_entry* entry;
	uint8_t* p;
	uint16_t chip_id;
	uint32_t tmp;
	patch = (struct rtk_epatch*)rtk_hw_cfg.fw_buf;
	entry = (struct rtk_epatch_entry*)bt_malloc(sizeof(*entry));

	if (!entry) {
		RS_ERR("failed to allocate mem for patch entry");
		return NULL;
	}

	//patch->number_of_patch = le16_to_cpu(patch->number_of_patch);
	//RS_DBG("fw_ver 0x%08x, patch_num %d",
	//le32_to_cpu(patch->fw_version), patch->number_of_patch);

	for (i = 0; i < patch->number_of_patch; i++) {
		RS_DBG("chip id 0x%04x",
		       get_unaligned_le16(rtk_hw_cfg.fw_buf + 14 + 2 * i));

		if (get_unaligned_le16(rtk_hw_cfg.fw_buf + 14 + 2 * i) ==
		        rtk_hw_cfg.eversion + 1) {
			entry->chipID = rtk_hw_cfg.eversion + 1;
			entry->patch_length =
			    get_unaligned_le16(rtk_hw_cfg.fw_buf + 14 +
			                       2 * patch->number_of_patch +
			                       2 * i);
			entry->start_offset =
			    get_unaligned_le32(rtk_hw_cfg.fw_buf + 14 +
			                       4 * patch->number_of_patch +
			                       4 * i);
			RS_DBG("patch length is 0x%x", entry->patch_length);
			RS_DBG("start offset is 0x%x", entry->start_offset);
			entry->svn_ver = get_unaligned_le32(rtk_hw_cfg.fw_buf +
			                                    entry->start_offset +
			                                    entry->patch_length - 8);
			entry->coex_ver = get_unaligned_le32(rtk_hw_cfg.fw_buf +
			                                     entry->start_offset +
			                                     entry->patch_length - 12);
			RS_INFO("Svn version: %8d\n", entry->svn_ver);
			tmp = ((entry->coex_ver >> 16) & 0x7ff) +
			      (entry->coex_ver >> 27) * 10000;
			RS_INFO("Coexistence: BTCOEX_20%06d-%04x\n", tmp,
			        (entry->coex_ver & 0xffff));
			break;
		}
	}

	if (i == patch->number_of_patch) {
		RS_ERR("failed to get entry");
		bt_free(entry);
		entry = NULL;
	}

	return entry;
}

void rtk_get_final_patch(int fd, int proto) {
	(void)(fd);
	struct btrtl_info* rtl = &rtk_hw_cfg;
	uint8_t proj_id = 0;
	struct rtk_epatch_entry* entry = NULL, _entry;
	struct rtk_epatch* patch = (struct rtk_epatch*)rtl->fw_buf;
	uint32_t svn_ver, coex_ver, tmp;
	rtl->total_len = rtk_hw_cfg.fw_len + rtl->config_len;
	log_debug("rtk_hw_cfg.fw_len:%d rtl->config_len:%d rtl->total_len:%d", rtk_hw_cfg.fw_len, rtl->config_len, rtl->total_len);

	if (!(rtl->total_buf = bt_malloc(rtl->total_len))) {
		RS_ERR("Can't alloc memory for multi fw&config");
		rtl->dl_fw_flag = 0;
		rtl->total_len = 0;
		goto free_buf;
	} else {
		memcpy(rtl->total_buf,
		       rtl->fw_buf,
		       rtk_hw_cfg.fw_len);
		memcpy(rtl->total_buf + rtk_hw_cfg.fw_len - 4,
		       &patch->fw_version, 4);

		if (rtl->config_len)
			memcpy(rtl->total_buf + rtk_hw_cfg.fw_len,
			       rtl->config_buf, rtl->config_len);

		rtl->dl_fw_flag = 1;
	}

	RS_DBG("fw:%s exists, config file:%s exists",
	       (rtl->fw_len > 0) ? "" : "not",
	       (rtl->config_len > 0) ? "" : "not");
free_buf:

	if (rtl->fw_len > 0) {
		rtl->fw_buf = NULL;
		rtl->fw_len = 0;
	}

	if (rtl->config_len > 0) {
		rtl->config_buf = NULL;
		rtl->config_len = 0;
	}

	if (entry) {
		bt_free(entry);
	}
}

/**
* Config realtek Bluetooth. The configuration parameter is get from config file and fw.
* Config file is rtk8723_bt_config. which is set in rtk_get_bt_config.
* fw file is "rlt8723a_chip_b_cut_bt40_fw", which is set in get_firmware_name.
*
* @warning maybe only one of config file and fw file exists. The bt_addr arg is stored in "/data/btmac.txt"
* or "/data/misc/bluetoothd/bt_mac/btmac.txt",
*
* @param fd uart file descriptor
* @param proto realtek Bluetooth protocol, shall be either HCI_UART_H4 or HCI_UART_3WIRE
* @param speed init_speed in uart struct
* @param ti termios struct
* @returns #0 on success
*/
static int rtk_config(int fd, int proto, int speed) {
	(void)(speed);
	int ret = 0;
	struct btrtl_info* btrtl = &rtk_hw_cfg;
	btrtl->proto = proto;

	/* Get Local Version Information and RTK ROM version */
	if (proto == HCI_UART_3WIRE) {
		RS_INFO("H5 IC");
		rtk_get_lmp_version(fd);
		rtk_get_eversion(fd);
	} else {
		RS_INFO("H4 IC");
		/* The following set is for special requirement that enables
		 * flow control before initializing */
		/* ti->c_cflag &= ~PARENB;
		 * ti->c_cflag |= CRTSCTS;
		 * if (tcsetattr(fd, TCSANOW, ti) < 0) {
		 * 	RS_ERR("H4 Can't enable RTSCTS");
		 * 	return -1;
		 * }
		 * usleep(20 * 1000);
		 */
		rtk_hci_local_ver(fd);
		rtk_hci_rom_ver(fd);

		if (rtk_hw_cfg.lmp_subver == ROM_LMP_8761btc) {
			rtk_hw_cfg.chip_type = CHIP_8761BTC;
			rtk_hw_cfg.hw_flow_control = 1;
			/* TODO: Change to different uart baud */
			uart_speed_to_rtk_speed(1500000, &rtk_hw_cfg.baudrate);
			goto change_baud;
		} else if (rtk_hw_cfg.lmp_subver == ROM_LMP_8761a) {
			if (rtk_hw_cfg.hci_rev == 0x000b) {
				rtk_hw_cfg.chip_type = CHIP_8761B;
				rtk_hw_cfg.hw_flow_control = 1;
				/* TODO: Change to different uart baud */
				uart_speed_to_rtk_speed(1500000, &rtk_hw_cfg.baudrate);
				goto change_baud;
			} else if (rtk_hw_cfg.hci_rev == 0x000a) {
				if (rtk_hw_cfg.eversion == 3) {
					rtk_hw_cfg.chip_type = CHIP_8761ATF;
				} else if (rtk_hw_cfg.eversion == 2) {
					rtk_hw_cfg.chip_type = CHIP_8761AT;
				} else {
					rtk_hw_cfg.chip_type = CHIP_UNKNOWN;
				}
			}
		} else if (rtk_hw_cfg.lmp_subver == ROM_LMP_8723b) {
			if (rtk_hw_cfg.hci_ver == 0x08 &&
			        rtk_hw_cfg.hci_rev == 0x000d) {
				rtk_hw_cfg.chip_type = CHIP_8723DS;
			} else if (rtk_hw_cfg.hci_ver == 0x06 &&
			           rtk_hw_cfg.hci_rev == 0x000b) {
				rtk_hw_cfg.chip_type = CHIP_8723BS;
			} else {
				RS_ERR("H4: unknown chip");
				return -1;
			}
		}
	}

	RS_INFO("LMP Subversion 0x%04x", btrtl->lmp_subver);
	RS_INFO("EVersion %d", btrtl->eversion);
	btrtl->patch_ent = get_patch_entry(btrtl);

	if (btrtl->patch_ent) {
		RS_INFO("IC: %s\n", btrtl->patch_ent->ic_name);
		RS_INFO("Firmware/config: %s, %s\n",
		        btrtl->patch_ent->patch_file,
		        btrtl->patch_ent->config_file);
	} else {
		RS_ERR("Can not find firmware/config entry\n");
		return -1;
	}

	btrtl->config_buf = rtl8761a_config;
	rtk_hw_cfg.config_len = sizeof(rtl8761a_config);
	btrtl->baudrate = 115200;

	if (rtk_hw_cfg.config_len < 0) {
		RS_ERR("Get Config file error, just use efuse settings");
		rtk_hw_cfg.config_len = 0;
	}

	btrtl->fw_buf = rtl8761a_fw;
	rtk_hw_cfg.fw_len = sizeof(rtl8761a_fw) - sizeof(rtl8761a_config); //rtk_get_bt_firmware(btrtl, &btrtl->fw_buf);
	log_debug("rtl8761a_fw size:%d rtl8761a_config size:%d", sizeof(rtl8761a_fw), sizeof(rtl8761a_config));

	if (rtk_hw_cfg.fw_len < 0) {
		RS_ERR("Get BT firmware error");
		rtk_hw_cfg.fw_len = 0;
		return -1;
	} else {
		rtk_get_final_patch(fd, proto);
	}

	if (rtk_hw_cfg.total_len > RTK_PATCH_LENGTH_MAX) {
		RS_ERR("total length of fw&config larger than allowed");
		return -1;
	}

	RS_INFO("Total len %d for fw/config", rtk_hw_cfg.total_len);
change_baud:
	RS_DBG("unused");
	/* wait for while for controller to setup */
	DelayMs(10);

	/* For 8761B Test chip, no patch to download */
	if (rtk_hw_cfg.chip_type == CHIP_8761BTC ||
	        rtk_hw_cfg.chip_type == CHIP_8761B) {
		goto done;
	}

	if ((rtk_hw_cfg.total_len > 0) && (rtk_hw_cfg.dl_fw_flag)) {
		rtk_hw_cfg.link_estab_state = H5_PATCH;
		rtk_hw_cfg.rx_index = -1;
		ret =
		    rtk_download_fw_config(fd, rtk_hw_cfg.total_buf,
		                           rtk_hw_cfg.total_len,
		                           rtk_hw_cfg.baudrate, proto);
		memset(rtk_hw_cfg.total_buf, 0, rtk_hw_cfg.total_len);
		bt_free(rtk_hw_cfg.total_buf);

		if (ret < 0) {
			return ret;
		}
	}

done:
	RS_DBG("Init Process finished");
	return 0;
}


/**
* Init uart by realtek Bluetooth.
*
* @param fd uart file descriptor
* @param proto realtek Bluetooth protocol, shall be either HCI_UART_H4 or HCI_UART_3WIRE
* @param speed init_speed in uart struct
* @param ti termios struct
* @returns #0 on success, depend on rtk_config
*/
int rtk_init(int fd, int proto, int speed) {
	RS_DBG("Realtek hciattach version %s \n", RTK_VERSION);
	memset(&rtk_hw_cfg, 0, sizeof(rtk_hw_cfg));
	rtk_hw_cfg.serial_fd = fd;
	rtk_hw_cfg.dl_fw_flag = 1;

	/* h4 will do nothing for init */
	if (proto == HCI_UART_3WIRE) {

		bt_disable_rts_cts();
		
		if (rtk_init_h5(fd) < 0) {
			return -1;
		};
	}

	return rtk_config(fd, proto, speed);
}

