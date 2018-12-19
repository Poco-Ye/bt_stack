/*
   PPP over Ethernet
   Author: sunJH
   Create: 090824
history:
090907 sunJH
PPPoE State增加PPPOE_UP,PPPOE_PRE_TERM状态
090911 sunJH
重试次数由50改为10次
*/
#ifndef _PPP_OE_H
#define _PPP_OE_H
#include <inet/inet.h>

#include "ppp.h"
/* Codes to identify message types */
#define PADI_CODE	0x09
#define PADO_CODE	0x07
#define PADR_CODE	0x19
#define PADS_CODE	0x65
#define PADT_CODE	0xa7

#define SES_CODE    0x00
/*------MTU discovery-----*/
#define PPPOE_MAX    1484
#define	PPPOE_DIS_SID 0x0000

#define PPPOE_ARP_INDEX 0x00000002
/*------Struct declare------------------*/
struct pppoe_taghdr 
{
	u16 tag_type;
	u16 tag_len;
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;
struct pppoe_tag  
{
	u16  type;
	u16  len; 
 	u8  *data;
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;
/* Tag identifiers */
#define PTT_EOL		    0x0000
#define PTT_SRV_NAME	0x0101
#define PTT_AC_NAME	    0x0102
#define PTT_HOST_UNIQ	0x0103
#define PTT_AC_COOKIE	0x0104
#define PTT_VENDOR 	    0x0105
#define PTT_RELAY_SID	0x0110
#define PTT_SRV_ERR     0x0201
#define PTT_SYS_ERR  	0x0202
#define PTT_GEN_ERR  	0x0203
/*------error --------*/
#define PPPOE_SUCC        0
#define PPPOE_FAIL        -1

#define PPPOE_VER    0x1
#define PPPOE_TYPE   0x1

#define PPPOE_TAG_MAX  2
struct pppoe_hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u8 ver : 4;
	u8 type : 4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u8 type : 4;
	u8 ver : 4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	u8 code;
	u16 sid;
	u16 length;
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;
/*
	state switch diagram
PPPOE_INIT  ---> PPPOE_PADI   (event = USER_EVENT_LOGIN)

PPPOE_PADI  ---> PPPOE_PADR   (event = Input Offer Packet)
  PPPOE_PADI  ---> PPPOE_INIT   (event = USER_EVENT_LOGOUT or Timeout)

PPPOE_PADR  ---> PPPOE_SES   (event = Input Session Packet)
  PPPOE_PADR  ---> PPPOE_INIT   (event = USER_EVENT_LOGOUT or Timeout)

PPPOE_SES  ---> PPPOE_UP (event = PPPopen)
  PPPOE_SES ---> PPPOE_PRE_TERM (event = PPPclsd)

PPPOE_UP  ---> PPPOE_PRE_TERM(event = USER_EVENT_LOGOUT)
  PPPOE_UP ---> PPPOE_TERM(event = PPPclsd)

PPPOE_PRE_TERM ---> PPPOE_TERM (event = PPPClsd or Timeout[10s])

PPPOE_TERM ---> PPPOE_INIT(event = Timeout[500ms])
    
*/
typedef enum 
{
	PPPOE_INIT=1,/* 初始化完成*/
	PPPOE_PADI,
	PPPOE_PADR,
	PPPOE_SES,/* Session Stage */
	PPPOE_UP,/* PPP Link Up Stage */
	PPPOE_PRE_TERM,/* waiting for PPP Down */
	PPPOE_TERM,/* terminate Stage */
}PPPOE_STATE;

typedef struct pppoe_dev
{
	volatile PPPOE_STATE  state;/* 状态*/
	int          error_code;
	volatile USER_EVENT   user_event;
	u32          timeout;/* 超时*/
	int          retry;/* 重试次数*/
	long         auth;/* 认证算法*/
	PPP_DEV_T    ppp_dev;/*PPP Dev*/
	INET_DEV_T   inet_dev;
	u16          session_id;/* in network order */
	u16          protocal;
	
	u8           serv_mac[ETH_ALEN];/* PPPoE Server MAC Addr*/
//	u32          HostUniq;
	u8		 HostUniq[8];

	u8  ServiceName[100];//保存地址
	u16 ServiceNameLen;
	
	u8  ACName[100];
	u16  ACNameLen;

	u8  ACCookie[1024];
	u16  ACCookieLen;
	INET_DEV_T   *link_dev;/* link layer Ethernet Dev */

	/*用于debug，查看每个阶段所耗时间 */
	u32 		 jiffier;  /*PPPOE 时间戳 */
	u32          login_jiffier;/* Login时间戳 */
	u32          logout_jiffier;/* Logout时间戳 */
}PPPOE_DEV;


#define PPPOE_TIMEOUT   1000
#define PPPOE_RETRY      30

#define PPPOE_TERM_TIMEOUT 1000
#define PPPOE_TERM_NUM     3

#define PPPOE_TAGHEAD_LENGTH   (sizeof(struct pppoe_taghdr)-sizeof(u8*))//it must be 4
err_t pppoe_input(struct sk_buff *skb, INET_DEV_T *in, u16 proto);

#endif/* _PPP_OE_H */
