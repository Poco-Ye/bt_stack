/*
修改历史:
20080605:
1.#define __LITTLE_ENDIAN_BITFIELD放入与cpu相关的文件(arch_s80.h)中
2.增加了对系统外部接口使用的声明，避免无定义警告
3.去掉了无用的声明constant_htons、constant_htonl
4.增加了sp30的宏定义
5.强制串口打印统一采用s_uart0_printf
080624 sunJH
1.增加MAX_IPV4_ADDR_STR_LEN宏定义
2.把TCP_SND_QUEUELEN、TCP_RCV_QUEUELEN放入inet_config.h
3.增加str_check_max声明
080820 sunJH
1.去掉了PortTx、PortOpen声明，避免不同平台冲突
2.S3C2410平台的memcpy优化问题的宏条件由
P80改为S3C2410，这样更合理
081108 sunJH
ScrPrint在NO_LED情况下为空函数
090507 sunJH
TCP_MAXRTX 最大重试次数改由12为120，
加强GPRS的TCP稳定性
*/
#ifndef _INET_H_
#define _INET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inet/inet_config.h>
#include <netapi.h>
#include <inet/inet_type.h>
#include <inet/ip_addr.h>
#include <inet/inet_irq.h>


#if defined(__HAS_NETWORK_OP)
//DON'T define htonl and htons
#elif defined(__LITTLE_ENDIAN_BITFIELD)
static inline u32 htonl(u32 l)
{
	return ((((l)&0xff)<<24)|(((l)&0xff00)<<8)|
					(((l)&0xff0000)>>8)|(((l)&0xff000000)>>24));
}
static inline u16 htons(u16 s)
{
	return (u16)((((s)&0xff)<<8)|(((s)&0xff00)>>8));
}
#define ntohl(l) htonl(l)
#define ntohs(s) htons(s)
#elif defined(__BIG_ENDIAN_BITFIELD)
#define htonl(l) (l)
#define htons(s) (s)
#define ntohl(l) (l)
#define ntohs(s) (s)
#else
#error	"Adjust your byte orders defines"
#endif


typedef int err_t;

#ifdef NET_DEBUG
#define TCP_DEBUG  1
#define UDP_DEBUG  2
#define ARP_DEBUG  3
#define IP_DEBUG   4
#define IPSTACK_DEBUG ipstack_print
int ipstack_print(int module_id, char *fmt, ...);
#else
#define TCP_DEBUG  0
#define UDP_DEBUG  0
#define ARP_DEBUG  0
#define IP_DEBUG   0
#define IPSTACK_DEBUG 
#endif





/* ---------- TCP options ---------- */
#define IP_DEFAULT_TTL 64
#ifndef TCP_TTL
#define TCP_TTL                         (IP_DEFAULT_TTL)
#endif

#define MAX_IPV4_ADDR_STR_LEN            15

/* TCP Maximum segment size. */
#ifndef TCP_MSS
#define TCP_MSS                         1400 /* A *very* conservative default. */
#endif

#ifndef TCP_WND
#define TCP_WND                         TCP_RCV_QUEUELEN*TCP_MSS
#endif 

#ifndef TCP_MAXRTX
#define TCP_MAXRTX                      120
#endif

#ifndef TCP_SYNMAXRTX
#define TCP_SYNMAXRTX                   100
#endif

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ                 0
#endif


/* TCP sender buffer space (bytes). */
#ifndef TCP_SND_BUF
#define TCP_SND_BUF                     TCP_MSS*TCP_SND_QUEUELEN
#endif

#define IPSTACK_MAX(x , y)  (x) > (y) ? (x) : (y)
#define IPSTACK_MIN(x , y)  (x) < (y) ? (x) : (y)

#ifndef NULL
#define NULL ((void *)0)
#endif

#define U16_F "uh"
#define U32_F "ul"
#define S16_F "d"
#define S32_F "d"

u16 inet_chksum(void *dataptr, int len);

struct net_stats
{
	u32 recv_pkt;
	u32 recv_err_pkt;
	u32 recv_err_proto;
	u32 recv_err_check;
	u32 recv_null;
	u32 drop;
	u32 snd_pkt;

};

#define NET_STATS_DROP(stats) ((stats)->drop++)
#define NET_STATS_RCV(stats) ((stats)->recv_pkt++)
#define NET_STATS_SND(stats) ((stats)->snd_pkt++)

void ipstack_init(void);

#ifdef NET_DEBUG
#define s80_printf printk
#define err_printf printk
#else
#define s80_printf
#define err_printf 
#endif
char* s80_gets(void);
void inet_delay(u32 ms);

#ifdef NET_DEBUG
#define assert(expr) \
do{	\
	if ((!(expr))) \
	{	\
		char *str = __FILE__; \
		int i; \
		for(i=strlen(str);i>=0;i--) \
			if(str[i]=='\\'||str[i]=='/') \
				break; \
		printk("ASSERTION FAILED (%s) at: %s:%d\n", #expr,	\
			str+i+1, __LINE__);				\
		ScrPrint(0, 2, 0, "ASSERTION FAILED (%s) at: %s:%d\n", #expr,	\
					str+i+1, __LINE__); 			\
		asm("mov r2, #0\n bx r2");\
	}									\
} while (0)
#else/* NET_DEBUG */
#define assert(expr) do{ \
}while(0)
#endif/* NET_DEBUG */



#define INET_MEMCMP memcmp
#define INET_MEMCPY memcpy


void show_time(void);/* time for debug */

extern u8 bc_mac[6];/* broadcast mac */
extern u8 zero_mac[6];/* 全0 mac address */
extern volatile u32  inet_jiffier;

#define IPSTACK_CALLBACK_API 1
#define IPSTACK_IN_CHECK 1

#define DHCPS_PORT 67
#define DHCPC_PORT 68
#define DHCPC 1

#define DNS_PORT 53


int net_has_init(void);
void s_InetSoftirqInit(void);
void s_DevInit(void);
void s_initEth(void);
void s_initPPP(void);
void s_InitWxNetPPP(void);
void s_initPPPoe(void);

extern int toupper(int c);

#endif/* _INET_H_ */
