/*
** 修改历史记录
080619:
增加字符串长度检查,主要用于API参数
081217 sunJH
调整了buf_printf打印格式
090415 sunJH
增加调用s_InetSoftirqInit和s_DevInit
**/
#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/skbuff.h>
#include <inet/arp.h>
#include <inet/ethernet.h>
#include <inet/tcp.h>
#include <inet/udp.h>
#include <inet/ip.h>
#include <inet/inet_softirq.h>
#include <netapi.h>

/*
字符串长度检查,主要用于API参数
*/
int str_check_max(const char *str, int max)
{
	int i;
	if(str == NULL)
		return NET_OK;
	for(i=0; i<max+1; i++)
	{
		if(str[i]==0)
			return NET_OK;
	}
	return NET_ERR_STR;
}

void buf_printf(const unsigned char *buf, int size, const char *descr)
{
	int i,count;
	if(descr)
		s80_printf("\n%s\n",descr);
	s80_printf("-----------------------------------\n");
	count = 0;
	for(i=0; i<size; i++)
	{
		#ifndef WIN32
		s80_printf("0x");
		#endif
		s80_printf("%02x,", buf[i]);
		count += 3;
		if((i&0xf)==0xf)
		{
			s80_printf("\n");
			count++;
		}
		#ifndef WIN32
		if(count>512)
		{
			DelayMs(100);
			count = 0;
		}
		#endif
	}
	if(((i-1)&0xf)!=0xf)
		s80_printf("\n");
	s80_printf("-----------------------------------\n");
}

static u16 ip_standard_chksum(void *dataptr, int len)
{
	u32 acc;
	u16 src;
	u8 *octetptr;

	acc = 0;
	/* dataptr may be at odd or even addresses */
	octetptr = (u8*)dataptr;
	while (len > 1)
	{
		/* declare first octet as most significant
		 * thus assume network order, ignoring host order 
		 */
		src = (*octetptr) << 8;
		octetptr++;
		/* declare second octet as least significant */
		src |= (*octetptr);
		octetptr++;
		acc += src;
		len -= 2;
	}
	if (len > 0)
	{
		/* accumulate remaining octet */
		src = (*octetptr) << 8;
		acc += src;
	}
	/* add deferred carry bits */
	acc = (acc >> 16) + (acc & 0x0000ffffUL);
	if ((acc & 0xffff0000) != 0)
	{
		acc = (acc >> 16) + (acc & 0x0000ffffUL);
	}
	/* This maybe a little confusing: reorder sum using htons()
	 * instead of ntohs() since it has a little less call overhead.
	 * The caller must invert bits for Internet sum ! 
	 */
	return htons((u16)acc);
}

/* inet_chksum_pseudo:
 *
 * Calculates the pseudo Internet checksum used by TCP and UDP for a pbuf chain.
 */

u16 inet_chksum_pseudo(void *payload, int len,
			u32 src, u32 dest,
			u8 proto)
{
	u32 acc;
	u8 swapped;

	acc = 0;
	swapped = 0;
	acc += ip_standard_chksum(payload, len);
	while (acc >> 16) 
	{
		acc = (acc & 0xffffUL) + (acc >> 16);
	}
	if (len % 2 != 0) 
	{
		swapped = 1 - swapped;
		acc = ((acc & 0xff) << 8) | ((acc & 0xff00UL) >> 8);
	}

	if (swapped) 
	{
		acc = ((acc & 0xff) << 8) | ((acc & 0xff00UL) >> 8);
	}
	src = htonl(src);
	dest = htonl(dest);
	acc += (src & 0xffffUL);
	acc += ((src >> 16) & 0xffffUL);
	acc += (dest & 0xffffUL);
	acc += ((dest >> 16) & 0xffffUL);
	acc += (u32)htons((u16)proto);
	acc += (u32)htons((u16)len);

	while (acc >> 16) 
	{
		acc = (acc & 0xffffUL) + (acc >> 16);
	}
	return (u16)~(acc & 0xffffUL);
}

u16 inet_chksum(void *dataptr, int len)
{
  u32 acc;

  acc = ip_standard_chksum(dataptr, len);
  while (acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }
  return (u16)~(acc & 0xffff);
}

#include <stdarg.h>

extern struct list_head g_dev_list;
void inet_rx(unsigned long flag)
{
	struct list_head *p;
	INET_DEV_T *dev;
	list_for_each(p, &g_dev_list)
	{
		dev = list_entry(p, INET_DEV_T, list);
		if(DEV_FLAGS(dev, FLAGS_RX))
		{
			dev->dev_ops->poll(dev, 20);
		}
	}
}
void inet_tx(unsigned long flag)
{
	struct list_head *p;
	INET_DEV_T *dev;
	list_for_each(p, &g_dev_list)
	{
		dev = list_entry(p, INET_DEV_T, list);
		if(DEV_FLAGS(dev, FLAGS_TX))
		{
			dev->dev_ops->xmit(dev, NULL, 0, 0);
		}
	}
}

struct inet_softirq_s inet_rx_softirq;
struct inet_softirq_s inet_tx_softirq;

void ipstack_init(void)
{	
	s_InetSoftirqInit();
	s_DevInit();
	skb_init();
	tcp_init();
	udp_init();
	ip_init();
	arp_init();
	inet_rx_softirq.mask = INET_SOFTIRQ_RX;
	inet_rx_softirq.h = inet_rx;
	inet_softirq_register(&inet_rx_softirq);
	inet_tx_softirq.mask = INET_SOFTIRQ_TX;
	inet_tx_softirq.h = inet_tx;
	inet_softirq_register(&inet_tx_softirq);
}

