#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/skbuff.h>
#include <inet/inet_softirq.h>
#include <inet/inet_timer.h>
#include <inet/ethernet.h>
#include <inet/tcp.h>
#include <inet/inet_list.h>
#include <inet/mem_pool.h>
#include <inet/ip.h>
#include "ppp.h"
#include "mschap.h"
#include <inet/icmp.h>

#define PPP_PAP_USED(ppp) ((ppp)->neg_authent&PPP_ALG_PAP)
#define PPP_CHAP_USED(ppp) ((ppp)->neg_authent&PPP_ALG_CHAP)
#define PPP_MSCHAP_USED(ppp) ((ppp)->neg_authent&PPP_ALG_MSCHAPV1)
#define PPP_MSCHAPV2_USED(ppp) ((ppp)->neg_authent&PPP_ALG_MSCHAPV2)
#define PPP_ALG_INIT(ppp) (ppp)->neg_authent = (ppp)->init_authent
#define PPP_ALG_SET(ppp, alg) (ppp)->neg_authent = alg

#define ppp_irq_disable(ppp) do{\
	if((ppp)->link_ops&&(ppp)->link_ops->irq_disable)\
		(ppp)->link_ops->irq_disable(ppp);\
}while(0)	
#define ppp_irq_enable(ppp) do{ \
	if((ppp)->link_ops&&(ppp)->link_ops->irq_enable) \
		(ppp)->link_ops->irq_enable(ppp); \
}while(0)
#define ppp_irq_tx(ppp)   do{ \
	if((ppp)->link_ops&&(ppp)->link_ops->irq_tx) \
		(ppp)->link_ops->irq_tx(ppp); \
}while(0)

#define ppp_check_modem(ppp)   \
	(((ppp)->link_ops&&(ppp)->link_ops->ready)? \
		(ppp)->link_ops->ready(ppp): \
		1)
#define ppp_linkup(ppp, state)      \
	(((ppp)->link_ops&&(ppp)->link_ops->check)?\
		(ppp)->link_ops->check(ppp, state): \
		1)
#define ppp_force_modem_hang_off(ppp) do{\
	if(((ppp)->link_ops&&(ppp)->link_ops->force_down)) \
		(ppp)->link_ops->force_down(ppp); \
}while(0)

PPP_COM_OPS *ppp_comm[MAX_COMM_NUM] ={ NULL,NULL,NULL,NULL,NULL,};/* this is for comm irq handler */
//static int ppp_ticks = 0;/*in ms */

volatile static int flag_login = 0;

static u8 t_buf[PPP_MSS+PPP_HEADER_SPACE+2];/* temp buffer */
/* table for fast checksum sequence calculation */
static const unsigned short fcstab[256] = 
{
	0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,
	0x8c48,0x9dc1,0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,
	0x1081,0x0108,0x3393,0x221a,0x56a5,0x472c,0x75b7,0x643e,
	0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,0xf9ff,0xe876,
	0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,
	0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,
	0x3183,0x200a,0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,
	0xbdcb,0xac42,0x9ed9,0x8f50,0xfbef,0xea66,0xd8fd,0xc974,
	0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,0x2732,0x36bb,
	0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
	0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,
	0xdecd,0xcf44,0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,
	0x6306,0x728f,0x4014,0x519d,0x2522,0x34ab,0x0630,0x17b9,
	0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,0x8a78,0x9bf1,
	0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,
	0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,
	0x8408,0x9581,0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,
	0x0840,0x19c9,0x2b52,0x3adb,0x4e64,0x5fed,0x6d76,0x7cff,
	0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,0xf1bf,0xe036,
	0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
	0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,
	0x2942,0x38cb,0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,
	0xb58b,0xa402,0x9699,0x8710,0xf3af,0xe226,0xd0bd,0xc134,
	0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,0x5cf5,0x4d7c,
	0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,
	0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,
	0xd68d,0xc704,0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,
	0x5ac5,0x4b4c,0x79d7,0x685e,0x1ce1,0x0d68,0x3ff3,0x2e7a,
	0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,0x8238,0x93b1,
	0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
	0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,
	0x7bc7,0x6a4e,0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78
};

void ppp_buf_check(PPP_DEV_T *ppp, char *where);

static int ppp_init_buf(PPP_BUF_T *base, int num)
{
	int i;

	for(i=0; i<num; i++)
	{
		INIT_LIST_HEAD(&base[i].list);
		base[i].status = 0;
	}
	return 0;
}

static void ppp_reset_buf(PPP_DEV_T *ppp)
{
	inet_irq_disable();
	ppp_init_buf(ppp->snd_bufs, PPP_SND_QLEN);
	ppp->snd_buf = NULL;
	ppp->bufout = NULL;
	ppp->snd_flags = 0;
	ppp->chsout = 0;
	ppp->nxtout = 0;
	
	ppp_init_buf(ppp->rcv_bufs, PPP_RCV_QLEN);
	ppp->rcv_buf = NULL;
	ppp->bufin = NULL;
	ppp->lastin = 0;
	inet_irq_enable();
}

static int ppp_get_used_buf(PPP_BUF_T *base, int num)
{
	int i;

	for(i=0; i<num; i++)
	{
		if(base[i].status == 1)
			return i;
	}
	return -1;
}

static PPP_BUF_T* ppp_find_buf_used(PPP_BUF_T *base, int num)
{
	int i;
	for(i=0; i<num; i++)
	{
		if(base[i].status == PPP_BUF_USED)
			return base+i;
	}
	return NULL;
}

static int ppp_get_buf(PPP_BUF_T *base, int num, int force)
{
	int i, select;
	u32 timestamp=0xffffffff;

	/* firstly search free slot */
	select = -1;
	for(i=0; i<num; i++)
	{
		if(base[i].status == PPP_BUF_FREE)
		{
			if(select < 0)
				select = i;
		}else
		{
			if(base[i].timestamp>0)
				base[i].timestamp--;/* older it */
		}		
	}
	if(select>=0)
		goto out;
	if(force == 0)
		return -1;
	/* secondly search the oldest  used slot and force to free it */
	for(i=0; i<num; i++)
	{
		if(base[i].timestamp<timestamp&&base[i].status!=PPP_BUF_DOING)
		{
			select = i;
			timestamp = base[i].timestamp;
		}
	}
	if(select<0)
		select = 0;
out:
	assert(select>=0&&select<num);
	base[select].status = PPP_BUF_FREE;
	base[select].timestamp = 0xffffffff;
	base[select].len = 0;
	return select;
}

static void ppp_inet_up(PPP_DEV_T *ppp);
static void ppp_inet_down(PPP_DEV_T *ppp);

static void ppp_build_header(PPP_DEV_T *ppp, u8 *data, int len, u16 proto, u8 *header)
{
	u16 us1;
	u8 *cp, *check;
	int i1;

	cp = header;
	check = cp;
	cp[0]=cp[1]=0;/* cheksum */
	cp[2] = 0xff;/* control */
	cp[3] = 0x03;/* address */
	cp[4] = (u8)(proto>>8);
	cp[5] = (u8)(proto&0xff);
	/* checksum */
	us1 = 0xffff;
	/* header checksum */
	i1 = 4;
	cp = check+2;
	while (i1--)
		us1 = (us1 >> 8) ^ fcstab[(us1 ^ *cp++) & 0xff];
	/* data checksum */
	i1  = len;
	cp = data;
	while (i1--)
		us1 = (us1 >> 8) ^ fcstab[(us1 ^ *cp++) & 0xff];
	us1 ^= 0xffff;
	
	check[0] = (u8)(us1&0xff);
	check[1] = (u8)(us1 >> 8);
}

/*
 * 确定报文是否重传 
 */
static int ppp_pkt_cmp(PPP_DEV_T *ppp, int recv_id, int recv_proto)
{
	if(ppp->recv_id>0&&
		ppp->recv_id==recv_id&&
		ppp->recv_prot>0&&
		ppp->recv_prot == recv_proto)
	{
		return 0;
	}
	return -1;
}

int pppoe_output(PPP_DEV_T *ppp, void *data, int len, u16 proto);
/*
**
** Put Packet Into PPP Send Bufs
**/
static inline int ppp_put_snd(PPP_DEV_T *ppp, u8 *data, int len, u16 proto)
{
	PPP_BUF_T *buf;
	int index, err;
	u8 header[PPP_HEADER_SPACE];

	ppp_printf("PPP Send len=%d,proto=%04x[%d]\n", len, proto, inet_jiffier);

	if(ppp->link_type==PPP_LINK_ETH)
	{
		//PPPoE		
		return pppoe_output(ppp, data, len, proto);
	}


	if(ppp->at_cmd_mode==1)
		return -1;
	if(len<=0||len>PPP_MSS)
	{
	
		assert(0);
		return 0;
	}

	ppp_build_header(ppp, data, len, proto, header);
	index = ppp_get_buf(ppp->snd_bufs, PPP_SND_QLEN, 0);
	if(index<0)
	{
		err = NET_ERR_BUF;
		goto out;
	}
	buf = ppp->snd_bufs+index;
	INET_MEMCPY(buf->data, header, PPP_HEADER_SPACE);
	INET_MEMCPY(buf->data+PPP_HEADER_SPACE, data, len);
	err = NET_OK;

	#if PPP_PKT_DEBUG
	ppp_printf("Send Packet[%d]\n", inet_jiffier);
	buf_printf(buf->data, PPP_HEADER_SPACE+len, NULL);
	#endif

	ppp_irq_disable(ppp);
	buf->status = PPP_BUF_USED;
	buf->len = len + PPP_HEADER_SPACE;
	ppp_irq_enable(ppp);
	
out:	
	ppp_irq_tx(ppp);
	return err;
}

static int ppp_xmit(PPP_DEV_T *ppp, struct sk_buff *skb, u32 nexthop, u16 proto)
{
	int err = NET_OK;
	INET_DEV_T *dev = ppp->inet_dev;

	if(ppp_put_snd(ppp, skb->data, skb->len, proto)!=NET_OK)
	{
		err = NET_ERR_MEM;
		dev->flags &= ~FLAGS_TX;
		dev->stats.drop++;
	}	
	skb_free(skb);
	return err;
}

static int ppp_write(PPP_DEV_T *ppp, void *buf, int size, u16 protocol)
{
	INET_DEV_T *dev = ppp->inet_dev;
	if(ppp_put_snd(ppp, (u8*)buf, size, protocol)!=NET_OK)
	{
		dev->flags &= ~FLAGS_TX;
		dev->stats.drop++;
		return NET_ERR_MEM;
    }
    return NET_OK;
}

int ppp_time(void)
{
	return inet_jiffier;
}

/*
KeepAlive : Send ICMP Ping Echo ;
*/
static int ppp_keepalive_req(PPP_DEV_T *ppp)
{
	struct sk_buff *skb;
	struct icmphdr *icmph;
	INET_DEV_T *out = ppp->inet_dev;
	int pkt_size;
	u32 dst_ip = htonl(inet_addr("58.63.236.45"));
	struct route rt; 

	rt.out = out;
	rt.next_hop = 0;

	if(out==NULL||!DEV_FLAGS(out, FLAGS_TX))
		return -1;
	pkt_size = out->header_len+sizeof(struct iphdr)+
			sizeof(struct icmphdr);
	skb = skb_new(pkt_size);
	if(!skb)
	{
		return NET_ERR_MEM;
	}
	ppp_printf("PPP ICMP Ping\n");
	skb_reserve(skb, pkt_size);
	skb->h.raw = __skb_push(skb, sizeof(struct icmphdr));
	icmph = skb->h.icmph;
	icmph->type = ICMP_ECHOREPLY;
	icmph->code = 0;
	icmph->un.echo.id = 100;
	icmph->un.echo.sequence = 1000;
	icmph->checksum = 0;
	icmph->checksum = inet_chksum(icmph, sizeof(struct icmphdr));
	ip_output(skb, out->addr, dst_ip,
		64, 0, IP_PROTO_ICMP, &rt);
	skb_free(skb);
	return 0;
}

/*
** * * * * * *
** ppp_negotiate()   Internal packet writing function.
**
** static void ppp_negotiate(struct sk_buff *skb);
**
** PARAMETERS:
**   (in/out) skb              A buffer to write with
**
** DESCRIPTION:
**   This function will check the state and create any necessary
**   packet to negotiate with.
**
** USAGE COMMENTS:
**   This is for use by ppp_timeout() and screen() only!  If the state is
**   open or the restart counter has expired, do not call this function.
**   It only looks at the current state (not other flags)!  PAP and CHAP
**   will not be done in a single turn as they are now (see comment in
**   code).
**
** * * * * * *
*/
static void ppp_negotiate(PPP_DEV_T *ppp)
{
	int i1;
	u8    *cp, *cp2;
	u16   protocol;
	u32   ip_addr;

	if (ppp->state != PPPclsd && ppp->state != PPPopen) 
	{    	
		cp = t_buf;
		cp2 = cp + 4;
		ppp_printf("PPP state %08x\n", ppp->state);
		if (ppp->state == PPPclsng) 
		{
			ppp_printf("LCP Term Req Send\n");
			protocol = PROTlcp;
			*cp = LCPterm_req;
			memset(cp2, 0, 6);
			cp2 += 6;
			goto write;
        }
        else if ((ppp->state & LCPopen) == LCPopen)
        {
			/* If peer requested PAP, we send Auth-req */
			if (PPP_PAP_USED(ppp)&&(ppp->opt4 & AUTHppap)) 
			{
				if(ppp->lcp_ack_retry>0)
					return;
				if(ppp->opt4 & AUTHpwat)
					return;
				ppp_printf("PAP Auth Req Send\n");
				ppp->opt4 |= AUTHpwat;
				protocol = PROTpap;
				*cp = PAPauth_req;
				cp[2] = 0;
				*cp2 = (unsigned char)strlen(ppp->userid);
				strcpy((char *)cp2+1, ppp->userid);
				cp2 += *cp2 + 1;
				*cp2 = (unsigned char)strlen(ppp->passwd);
				strcpy((char *)cp2+1, ppp->passwd);
				cp2 += *cp2 + 1;
				goto write;
			}
			else
			if ((ppp->auth_state==1)&&((ppp->state & IPCPtxREQ) == 0))
			{
				ppp_printf("IPCP Conf Req Send\n");
				ppp->state |= IPCPtxREQ;
				ppp->state &= ~IPCPrxACK;
				protocol = PROTipcp;
				*cp = IPCPconf_req;
				*cp2++ = IPCPopt_IPAD;
				*cp2++ = 2 + 4;
				ip_addr = htonl(ppp->local_addr);				
				INET_MEMCPY(cp2, &ip_addr, 4);
				cp2 += 4;
#if IPCP_DNS & 1
				/* Primary DNS server */
				if (ppp->ipopts & IPCP_DNSP) 
				{
					*cp2++ = IPCPopt_DNSP;
					*cp2++ = 6;
					ip_addr = htonl(ppp->primary_dns);
					INET_MEMCPY(cp2, &ip_addr, 4);
					cp2 += 4;
                }

				/* Secondary DNS server */
				if (ppp->ipopts & IPCP_DNSS) 
				{
					*cp2++ = IPCPopt_DNSS;
					*cp2++ = 6;
					ip_addr = htonl(ppp->second_dns);
					INET_MEMCPY(cp2, &ip_addr, 4);
					cp2 += 4;
				}
#endif
				goto write;
			}
        }
		else if ((ppp->state & LCPtxREQ) == 0)
		{
        	ppp_printf("LCP Conf Req Send\n");
			ppp->state |= LCPtxREQ;
			ppp->state &= ~LCPrxACK;
			protocol = PROTlcp;
			*cp = LCPconf_req;

			/* Loop though possible configure options (i1 = type) */
			for (i1=0; i1 < 16; i1++) 
			{
				/* if the option is selected, include it in conf-req */
				if (ppp->locopts & (1<<i1)) 
				{
					*cp2++ = i1;
					switch (i1) 
					{
					default:
						cp2 -= 1;   /* Not supported; Undo operation */
						break;
#if PPP_MRU
					case LCPopt_MRU:
						*cp2++ = 4;
						*cp2++ = ppp->mss >> 8;
						*cp2++ = ppp->mss;
						break;
#endif
					case LCPopt_ASYC:
						*cp2++ = 6;
						*cp2++ = ppp->raccm >> 24;
						*cp2++ = ppp->raccm >> 16;
						*cp2++ = ppp->raccm >> 8;
						*cp2++ = ppp->raccm;
						break;
#if COMPRESSION						
					case LCPopt_PCMP:
					case LCPopt_ACMP:
						*cp2++ = 2;
#endif						
						break;
                    }
                }
            }

write:
            cp[1] = ppp->id;                  /* Set ID field */
            i1 = cp2 - cp;                          /* Get length */
            cp[2] = i1 >> 8, cp[3] = i1;            /* Set length field */
            ppp->counter--;                        /* Restart counter-- */
            ppp->timems = ppp_time();
            ppp_write(ppp, t_buf, i1, protocol);    /* TX negotiation */
        }
    }
}

void ppp_prepare(PPP_DEV_T *ppp)
{
	ppp->prepare = 1;
	ppp_reset_buf(ppp);
	if(ppp->comm>=0&&ppp->comm<MAX_COMM_NUM)
	{
		ppp_com_register(ppp_comm+ppp->comm, &ppp->com_ops);
	}
}

static int ppp_up(PPP_DEV_T *ppp)
{
	ppp_printf("ppp_up state=%02x\n", ppp->state);
	if (ppp->state == PPPopen)
		return 1;   /* PPP is already up */

	if (ppp->state == PPPclsd || ppp->state == PPPclsng)
	{
		if(ppp->prepare==0)
		{
			ppp_reset_buf(ppp);
			if(ppp->comm>=0&&ppp->comm<MAX_COMM_NUM)
				ppp_com_register(ppp_comm+ppp->comm, &ppp->com_ops);
		}else
		{
			ppp->inet_dev->flags |= FLAGS_RX;
			INET_SOFTIRQ(INET_SOFTIRQ_RX);
		}
		flag_login = 1;
		ppp_start(ppp); /* Make PPP ready for starting actively */
		flag_login = 0;
		ppp_negotiate(ppp);
	}
	ppp->prepare = 0;
	return 0;   /* PPP is trying to get up */
}


/*
** * * * * * *
** ppp_down()   Bring the link down if it's up
**
** static int ppp_down(int netno);
**
** PARAMETERS:
**   (in) netno    The easyNET network number (the interface to bring up)
**
** RETURNS:
**   1             PPP is down
**   0             PPP is working to be down
**
** DESCRIPTION:
**   This function forces PPP to bring itself down regardless.
**
** * * * * * *
*/
static int ppp_down(PPP_DEV_T *ppp)
{
	ppp_inet_down(ppp);
	if (ppp->state == PPPclsng)
	{
		return 0;
	}

	if (ppp->state == PPPclsd) 
	{
		return 1;   /* PPP is already down or going down */
	}

	ppp->state = PPPclsng;
	ppp->counter = 10;
	ppp_negotiate(ppp);
	return 1;   /* PPP get down */
}
/*
处理echo_req功能
*/
void ppp_echo_req(PPP_DEV_T *ppp)
{
	static u32 timeout=0;
	char pkt[8]={0x09,0x01,0x00,0x08,0x4f,0x75,0x1a,0xfb};

	if(ppp->echo_state == ECHO_ENABLE)
	{
		/* 当每次收到对方报文时则更新echo信息 */
		if(!TM_LE(ppp->last_rx_timestamp+ECHO_START_TIME, inet_jiffier))
		{
			ppp->echo_retry = 0;
			ppp->echo_timeout = inet_jiffier;
			return ;
		}
		if(ppp->echo_retry >= ECHO_RETRY_MAX)
		{
			ppp_down(ppp);
			ppp->error_code = NET_ERR_LINKDOWN;
		}
	}else if(ppp->echo_state == ECHO_DETECT)
	{
		if(ppp->echo_retry>=ECHO_RETRY_MAX)
		{
			ppp->echo_state = ECHO_DISABLE;
			return;
		}
	}else
	{
		return;
	}
	if(TM_LE(ppp->echo_timeout, inet_jiffier))
	{
		ppp->echo_retry++;
		ppp->echo_timeout = inet_jiffier+ECHO_RETRY_TIME;
		ppp_printf("Send LCP Echo Req\n");
		ppp_write(ppp, pkt, sizeof(pkt), PROTlcp);			
	}
}

/*
** * * * * * *
** ppp_timeout()  State machine driver for PPP.
**
** int ppp_timeout(int netno);
**
** PARAMETERS:
**   (in) netno                Interface index into nets structure
**
** RETURNS:
**  -1                         PPP closed
**   0                         PPP working to be open/closed
**   1                         PPP open
**
** DESCRIPTION:
**   This function drives the state machine for PPP.
**
** USAGE/COMMENTS:
**   This function assumes it will be called periodically (ideally 10-20 and
**   at least 2 or 3 times a second.  It should not cause any harm
**   to call this much more frequently (100 -> 200 times per second),
**   but it's better not to waste CPU time if at all possible.
**
** * * * * * *
*/
void ppp_timeout(PPP_DEV_T *ppp, unsigned long mask)
{
	unsigned long ul1, ul2;
	int prevstate;

	prevstate = ppp->state;

	if(ppp_linkup(ppp,prevstate)<0&&
		prevstate!=PPPclsd)
	{
		ppp_printf("TCP Link Change Notify........\n");
		tcp_link_change_notify(ppp->inet_dev, 
			ppp->local_addr, PPP_MODEM);
		ppp_closed(ppp);
		ppp_printf("TCP Link Change Notify End..........\n");
		return;
	}

	if(ppp->user_request == PPP_USER_REQUEST_LOGOUT)
	{
		if(ppp->trigger_time <= inet_jiffier)
		{
			ppp->user_request = 0;
			ppp_down(ppp);
		}
	}

	/* If link is open, check for idle if necessary and return 1 */
	if (prevstate == PPPopen||prevstate==PPPclsd) 
	{
		if(prevstate == PPPopen&&ppp->at_cmd_mode==0)
			ppp_echo_req(ppp);
		if(prevstate == PPPopen&&
			ppp->keep_alive_interval>0&&
			ppp->next_keep_alive<ppp_time()&&
			DEV_FLAGS(ppp->inet_dev, FLAGS_TX)
			)
		{
			ppp_keepalive_req(ppp);
			ppp->next_keep_alive = ppp_time()+ppp->keep_alive_interval;
		}
		return ;
	}
	ul1 = ppp_time();

	if(ppp->timestamp!=0&&
		(s32)(ul1-ppp->timestamp)>=0)
	{
		//buf_printf((u8*)&ppp_dev, sizeof(ppp_dev), "PPP_DEV:");
		//buf_printf((u8*)&ppp_inet_dev, sizeof(ppp_inet_dev), "PPP Inet Dev:");
		
		ppp_closed(ppp);/* force to close */
		ppp_force_modem_hang_off(ppp);
		return;
	}

	if (ppp->lcp_ack_retry > 0&&(ul1-ppp->timems) > 100&&
		PPP_PAP_USED(ppp)&&(ppp->opt4 & AUTHppap)) 
	{
		ppp_printf("Retran LCP ACK\n");
		ppp_write(ppp, ppp->lcp_ack_pkt, ppp->lcp_ack_pkt_size, PROTlcp);
		ppp->timems = ppp_time();
		ppp->lcp_ack_retry--;
		return;
	}

	/* Check for a timeout. */
 	ul2= 1500; /*1000*//* ms */
 	//ppp_irq_disable();
	if ((ul1 - ppp->timems) > ul2)
	{
		ppp_printf("ppp Time out counter=%d, state=%08x [%d]\n", 
			ppp->counter, prevstate,
			inet_jiffier
			);
		if (prevstate == PPPclsng)
		{
			if (ppp->counter <= 0) 
			{
				//buf_printf((u8*)&ppp_dev, sizeof(ppp_dev), "PPP_DEV:");
				//buf_printf((u8*)&ppp_inet_dev, sizeof(ppp_inet_dev), "PPP Inet Dev:");
				
				ppp_closed(ppp);
				ppp_force_modem_hang_off(ppp);
				//ppp_irq_enable();
				return ;
			}
		}
		else if (ppp->counter <= 0) 
		{
			ppp_printf("Counter is <=0\n");
			ppp_down(ppp);
		}
		else if ((prevstate & (LCPrxACK)) != (LCPrxACK))
			ppp->state &= ~(LCPtxREQ | LCPrxACK);
		else if (ppp->opt4 & AUTHhini)
			ppp->opt4 &= ~AUTHwait;
		else if ((prevstate & IPCPopen) != IPCPopen)
			ppp->state &= ~(IPCPtxREQ | IPCPrxACK);

		ppp_negotiate(ppp);
		ppp->timems = ppp_time();
	}
	//ppp_irq_enable();

	return ;  
}


/*
 * 检查校验和
 */
static int ppp_checksum(int i1, u8 *cp)
{
	u16 us1, us2;
	
	us1 = 0xffff;	
	while (i1--)
		us1 = (us1 >> 8) ^ fcstab[(us1 ^ *cp++) & 0xff];
	us1 ^= 0xffff;
	us2 = ((unsigned short)cp[1] << 8) | cp[0];
	if (us1 != us2)
	{
		ppp_printf("Recv CheckSum is %04x, Our CheckSum is %04x\n",
			us2, us1);
		return -1;
	}
	return 0;
}

#define PPP_REJ -1
#define PPP_TERM -2
#define PPP_OK 0

static char* lcp_code_str(int code)
{
	static char *code_str[]=
	{
		"Config Req",
		"Config Ack",
		"Config Nack",
		"Config Reject",
		"Term   Req",
		"Term   Ack",
		"LCP Code   Reject",
		"LCP Proto  Reject",
		"LCP Echo   Req",
		"LCP Echo   Reply",
		"LCP N/A",
	};
	int size = sizeof(code_str)/sizeof(char*);
	code--;
	if(code<0||code>size)
		return code_str[size-1];
	return code_str[code];
}

int lcp_input(struct sk_buff *skb, PPP_DEV_T *ppp)
{
	int i1, i2, naklen, rejlen;
	unsigned short us1, us2;
	unsigned char prevstate, *cp, *cp2;
	u8 *nakbuf = t_buf;
	int naklimit = (PPP_MSS+PPP_HEADER_SPACE+2)/2;
	u8 *rejbuf = t_buf+naklimit;
	int rejlimit = naklimit;

	cp = (unsigned char *)(skb->data);  /* Point to PPP info. */
	us1 = cp[0];                                /* code */
	us2 = cp[1];                                /* identifier */
	i1 = ((int)cp[2] << 8) + cp[3] - 4;         /* length */
	cp2 = cp + 4;                               /* Point to data */
	naklen = rejlen = 0;                        /* No nak/rej yet */
	prevstate = ppp->state;        /* Previous state of PPP */
	if (prevstate == PPPclsng && us1 != LCPterm_req && us1 != LCPterm_ack)
	{
		skb_free(skb);
		return PPP_REJ;
	}

	ppp_printf("LCP Input code=%s,id=%02x\n",
				lcp_code_str(us1),
				us2
				);

	switch (us1) 
	{
	case LCPconf_req:
		if ((prevstate & LCPopen) == LCPopen || prevstate == PPPclsd)
		{
			ppp_inet_down(ppp);
			ppp_start(ppp);         /* tld, restart LCP */				
		}
		ppp->recv_id = us2;
		ppp->recv_prot = PROTlcp;
		*cp = LCPconf_ack;              /* set code to ACK (2) */
		i2 = 0;                         /* Check for authentication */
		for (; i1 > 0; cp2 += us2)     /* Cycle through options */
		{
			us1 = cp2[0];               /* point to option code */
			us2 = cp2[1];               /* length of option request */
			if (us2 < 2)                /* length must be >= 2 */
				break;
			i1 -= us2;
			switch (us1)
			{
			case LCPopt_RES0:
				goto rej3;
			case LCPopt_MRU:
				us1 = ((unsigned short)cp2[2] << 8) + cp2[3];
				if (us1<=ppp->mss)
					ppp->mss = us1;
				else 
				{
					cp2[2] = (u8)(ppp->mss >> 8);
					cp2[3] = (u8)(ppp->mss&0xff);
nak1:
					INET_MEMCPY(nakbuf+naklen, cp2, us2);
					naklen += us2;
				}
				break;
			case LCPopt_ASYC:
				ppp->ACCM = ((unsigned long)cp2[2] << 24) +
						((unsigned long)cp2[3] << 16) +
						((unsigned long)cp2[4] << 8) +
						cp2[5];
				ppp_printf("Remote ACCM %08x\n", ppp->ACCM);
				break;
			case LCPopt_AUTH:
				if(PPP_PAP_USED(ppp))
				{
					if (cp2[2] == 0xc0 && cp2[3] == 0x23)
					{
						ppp->opt4 |= AUTHppap;
						PPP_ALG_SET(ppp, PPP_ALG_PAP);
					}
				}
				if (cp2[2] == 0xc2 && cp2[3] == 0x23) 
				{
					if(PPP_MSCHAPV2_USED(ppp)&&cp2[4] == CHAPalg_MSCHAP2)
					{
						ppp->opt4 |= AUTHpmsc;
						PPP_ALG_SET(ppp, PPP_ALG_MSCHAPV2);
					}
					if (PPP_MSCHAP_USED(ppp)&&cp2[4] == CHAPalg_MD4)
					{
						ppp->opt4 |= AUTHpmsc;
						PPP_ALG_SET(ppp, PPP_ALG_MSCHAPV1);
					}
					if (PPP_CHAP_USED(ppp)&&cp2[4] == CHAPalg_MD5)
					{
						ppp->opt4 |= AUTHpchp;
						PPP_ALG_SET(ppp, PPP_ALG_CHAP);
					}
				}
				if ((ppp->opt4 & 7) == 0) 
				{
					if(PPP_MSCHAPV2_USED(ppp))
					{
						/* If authentication isn't supported, suggest MS_CHAPv2 */
						cp2[1] = 5, cp2[2] = 0xc2, cp2[3] = 0x23,
						cp2[4] = 0x81;
						us2 = 5;
						goto nak1;
					}
					if(PPP_MSCHAP_USED(ppp))
					{
						/* If authentication isn't supported, suggest MS_CHAPv1 */
						cp2[1] = 5, cp2[2] = 0xc2, cp2[3] = 0x23,
						cp2[4] = 0x80;
						us2 = 5;
						goto nak1;
					}
					if(PPP_CHAP_USED(ppp))
					{
						/* If authentication isn't supported, suggest CHAP */
						cp2[1] = 5, cp2[2] = 0xc2, cp2[3] = 0x23,
						cp2[4] = 0x5;
						us2 = 5;
						goto nak1;
					}
					if(PPP_PAP_USED(ppp))
					{
						/* If authentication isn't supported, suggest PAP */
						cp2[1] = 4, cp2[2] = 0xc0, cp2[3] = 0x23;
						us2 = 4;
						goto nak1;
					}
					goto rej3;
				}
				break;
			case LCPopt_MNUM:
				break;
			case LCPopt_RES6:
				goto rej3;
			case LCPopt_PCMP:
			case LCPopt_ACMP:
				break;
			default:
rej3:
				INET_MEMCPY(rejbuf+rejlen, cp2, us2);
				rejlen += us2;
				break;
			}
		}//for()

		if (naklen || rejlen)
		{
			ppp->remopts = 0;
			if (naklen) 
			{
				*cp = LCPconf_nak;      /* Nak with hint */
				INET_MEMCPY((char *)cp + 4, nakbuf, naklen);
				i2 = naklen + 4;
			}
			else if (rejlen) 
			{
				*cp = LCPconf_rej;      /* Reject others */
				INET_MEMCPY((char *)cp + 4, rejbuf, rejlen);
				i2 = rejlen + 4;
			}
			__skb_trim(skb, i2);
			cp[2] = i2 >> 8, cp[3] = i2;
			ppp->state &= ~(LCPrxREQ + LCPtxACK);
		}
		else
		{
			ppp->state |= (LCPrxREQ | LCPtxACK);
			ppp->opt4 &= ~AUTHpwat;
			if(ppp->inet_dev->ifIndex == WNET_IFINDEX)
			{
				INET_MEMCPY(ppp->lcp_ack_pkt, skb->data, skb->len);
				ppp->lcp_ack_pkt_size = skb->len;
				ppp->lcp_ack_retry = ppp->lcp_ack_retry_max;
			}
		}
		ppp_printf("Send LCP Reply %s\n", lcp_code_str(*cp));
		ppp_xmit(ppp, skb, 0, PROTlcp);
		return NET_OK;
	case LCPconf_ack:
		if (cp[1] != ppp->id || (prevstate & LCPtxREQ) == 0)
		{
			skb_free(skb);
			return PPP_REJ;
		}
		if ((prevstate & LCPrxACK)==0)     
		{
			//do nothing
			ppp->state |= LCPrxACK;        /* ack-rcvd */
			ppp->counter = MAXCONF;        /* Initialize restart counter */
			ppp->timems = ppp_time();     /* Initialize timer */
		}
		break;
	case LCPconf_nak:
	case LCPconf_rej:
		if (cp[1] != ppp->id)
		{
			skb_free(skb);
			return PPP_REJ;
		}
		ppp->id++;    /* Request options will change */

		/* Find and disable unaccepted options */
		for (; i1>0; cp2+=us2) 
		{
			us1 = cp2[0];                   /* type */
			us2 = cp2[1];                   /* length */
			i1 -= us2;                      /* length - 1 */
			if (us1 > 19 || us2 < 2)
				break;
			if (us1 == LCPopt_AUTH) 
			{
				return PPP_TERM;
			}
			if (us1 == LCPopt_MNUM) 
			{
				return PPP_TERM;
			}
			if (*cp == LCPconf_nak && us1 == LCPopt_ASYC) 
			{
				ppp->raccm |= ((unsigned long)cp2[2] << 24) +
							((unsigned long)cp2[3] << 16) +
							((unsigned long)cp2[4] << 8) +
							cp2[5];
			}
			else
			{
				/* disable option */
				ppp->locopts &= ~((unsigned short)1<<us1);
				if (us1 == LCPopt_ASYC)
					ppp->raccm = 0xffffffff;
			}
		}
		if ((prevstate & LCPopen) == LCPopen)
			ppp_start(ppp);             /* tld, restart LCP */
		else if (prevstate & LCPrxACK)
			ppp->state = 0;
		else 
		{
			ppp->counter = MAXCONF;        /* Initialize restart counter */
			ppp->timems = ppp_time();     /* Initialize timer */
			ppp->state &= ~(LCPtxREQ | LCPrxACK);
		}
		break;
	case LCPterm_req:
		if ((prevstate & LCPopen) == LCPopen) 
		{
			/* 收到LCPterm_req时也应该通知tcp断开连接 */
			ppp_printf("TCP Link Change Notify........\n");
			tcp_link_change_notify(ppp->inet_dev, 
				ppp->local_addr, PPP_MODEM);
			ppp_printf("TCP Link Change Notify End..........\n");
			ppp_down(ppp);
		}
		else if (prevstate != PPPclsd && prevstate != PPPclsng)
			ppp_start(ppp);
		*cp = LCPterm_ack;                      /* Set new type */
		ppp_printf("Send LCP Reply %s\n", lcp_code_str(*cp));
		ppp_xmit(ppp, skb, 0, PROTlcp);                         /* Send term-ack */
		return -100;
	case LCPterm_ack:
		if (cp[1] != ppp->id)             /* reject bad ID */
			return PPP_REJ;
		if (prevstate != PPPclsng)
		{
			if ((prevstate & LCPopen) == LCPopen)
				ppp->state = 0;                /* resend conf-req */
			else
				ppp->state &= ~(LCPrxACK);
		}
		else 
		{                                  /* If closing/stopping, */
			ppp_closed(ppp);               /* link layer closed */
			ppp_force_modem_hang_off(ppp);
			ppp->counter = 0;
		}
		break;
	case LCPcode_rej:
		break;
	case LCPprot_rej:
		us1 = (cp2[0] << 8) + cp2[1];
		if (us1 == PROTlqp)
			ppp->locopts &= ~(1<<LCPopt_LQPT);
		else if (us1 == PROTipcp) 
		{
			return PPP_TERM;
		}
		break;
	case LCPecho_req:
		if ((prevstate & LCPopen) == LCPopen)
		{
			*cp = LCPecho_rep;
			memset((char *)cp+4, 0, 4);
			ppp_printf("Send LCP Reply %s\n", lcp_code_str(*cp));
			ppp_xmit(ppp, skb, 0, PROTlcp);
			return PPP_OK;
		}
		break;
	case LCPecho_rep:
		if(ppp->echo_state == ECHO_DETECT)
			ppp->echo_state = ECHO_ENABLE;
		break;
	case LCPdisc_req:
		break;
	default:            /* Unknown code */
		i1 += 4;                                /* Length of packet */
		us1 = i1 + 4;                           /* Length of new packet */
		for (; i1 >= 0; i1--)                   /* Copy packet */
			cp2[i1] = cp[i1];
		*cp = LCPcode_rej;                      /* New code field */
		cp[2] = (u8)(us1 >> 8);                 /* New length field */
		cp[3] = (u8)us1;          
		__skb_trim(skb, us1);
		ppp_xmit(ppp, skb, 0, PROTlcp);
		return PPP_REJ;
	}
	skb_free(skb);
	return PPP_OK;
}

int ipcp_input(struct sk_buff *skb, PPP_DEV_T *ppp)
{
	int i1, naklen, rejlen;
	unsigned short us1, us2;
	unsigned char prevstate, *cp, *cp2;
	u8 *nakbuf = t_buf;
	int naklimit = (PPP_MSS+PPP_HEADER_SPACE+2)/2;
	u8 *rejbuf = t_buf+naklimit;
	int rejlimit = naklimit;
	u32  ip_addr;

	cp = (unsigned char *)(skb->data);  /* Point to PPP info. */
	us1 = cp[0];                                /* code */
	us2 = cp[1];                                /* identifier */
	i1 = ((int)cp[2] << 8) + cp[3] - 4;         /* length */
	cp2 = cp + 4;                               /* Point to data */
	naklen = rejlen = 0;                        /* No nak/rej yet */
	prevstate = ppp->state;        /* Previous state of PPP */
	ppp_printf("IPCP Input %s, id=%02x\n", lcp_code_str(us1), us2);
	switch (us1) 
	{
	case IPCPconf_req:
		#if 0
		/* 状态不退回LCPopen,因为可能会收到IPCP Req重传的
		从而导致死循环
		*/
		if ((prevstate & IPCPopen) == IPCPopen)
			ppp->state = LCPopen;
		#endif
		if(ppp->auth_state==0)
		{
			ppp_printf("Ignore Auth\n");
			ppp->auth_state=1;/* 不需要认证,忽略认证*/
		}
		/* scan through IPCP options */
		for (; i1>0; cp2+=us2) 
		{
			us1 = cp2[0];                   /* option type */
			us2 = cp2[1];                   /* option length */
			if (us2 < 2)
				break;
			i1 -= us2;
			switch (us1) 
			{
			case IPCPopt_IPAD:
				ip_addr = htonl(ppp->remote_addr);
				/* If the other end has no IP address, we provide it */
				if ((cp2[2]|cp2[3]|cp2[4]|cp2[5]) == 0)
				{
					if (ppp->remote_addr != 0) 
					{						
						INET_MEMCPY(cp2+2, &ip_addr, 4);
						INET_MEMCPY(nakbuf+naklen, cp2, us2);
						naklen += us2;
					}
					else 
					{  
						INET_MEMCPY(rejbuf+rejlen, cp2, us2);
						rejlen += us2;
					}
				}
				else if (INET_MEMCMP(&ip_addr, cp2+2, 4) != 0) 
				{
					INET_MEMCPY(&ip_addr, cp2+2, 4);
					ppp->remote_addr = ntohl(ip_addr);
				}
				break;
			case XIPCPopt_IPAD:
			default:
				INET_MEMCPY(rejbuf+rejlen, cp2, us2);
				rejlen += us2;
				break;
			}
		}
		if (rejlen > 0 || naklen > 0) 
		{
			if (rejlen > 0) 
			{
				*cp = IPCPconf_rej;     /* REJ's pulled from +400 */
				INET_MEMCPY((char *)cp+4, (char *)rejbuf, rejlen);
			}
			else 
			{
				*cp = IPCPconf_nak;     /* NAK's pulled from +200 */
				INET_MEMCPY((char *)cp+4, (char *)nakbuf, naklen);
				rejlen = naklen;
			}
			rejlen += 4;
			__skb_trim(skb, rejlen);
			cp[2] = rejlen >> 8, cp[3] = rejlen;
			if (ppp->state != PPPclsng)
				ppp->state &= ~(IPCPrxREQ + IPCPtxACK);
		}
		else 
		{
			*cp = IPCPconf_ack;         /* ACK's already in place */
			if (ppp->state != PPPclsng)
				ppp->state |= IPCPrxREQ + IPCPtxACK;
			if(ppp->state == PPPopen)
			{
				ppp_inet_up(ppp);
			}
		}
		/* Write message, ack, nak, or rej */
		ppp_printf("Send IPCP Reply %s\n", lcp_code_str(*cp));
		ppp_xmit(ppp, skb, 0, PROTipcp);
		return PPP_OK;
	case IPCPconf_ack:
		if (cp[1] != ppp->id || (prevstate & IPCPtxREQ) == 0)
			return PPP_REJ;
		if (prevstate & IPCPrxACK)
			ppp->state=LCPopen;
		else 
		{
			ppp->state |= IPCPrxACK;
			ppp->counter = MAXCONF+1;
			ppp->timems = ppp_time();
		}
		if(ppp->state == PPPopen)
		{			
			ppp_inet_up(ppp);
		}
		break;
	case IPCPconf_nak:
	case IPCPconf_rej:
		if (cp[1] != ppp->id)
		{
			skb_free(skb);
			return PPP_REJ;
		}
		ppp->id++;    /* Request options will change */
		for (; i1>0; cp2+=us2) 
		{
			us1 = cp2[0];
			us2 = cp2[1];
			if (us2 < 2)
				break;
			i1 -= us2;
			switch (us1) 
			{
			case IPCPopt_IPAD:
				if ((cp2[2]|cp2[3]|cp2[4]|cp2[5]) == 0) 
				{
					return PPP_TERM;
				}
				INET_MEMCPY(&ip_addr, cp2+2, 4);
				ppp->local_addr = ntohl(ip_addr);
				break;
			case IPCPopt_DNSP:
				us1 = cp2[2]|cp2[3]|cp2[4]|cp2[5];
				ip_addr = htonl(ppp->primary_dns);
				if (us1 == 0 || us1 == 1 || cp[0] == IPCPconf_rej)
				{
					ppp->ipopts &= ~(IPCP_DNSP);  /* Disable */
				}
				else
				{
					INET_MEMCPY(&ip_addr, cp2+2, 4);
					ppp->primary_dns = ntohl(ip_addr);
				}
				break;
			case IPCPopt_DNSS:
				us1 = cp2[2]|cp2[3]|cp2[4]|cp2[5];
				ip_addr = htonl(ppp->second_dns);
				if (us1 == 0 || us1 == 1 || cp[0] == IPCPconf_rej)
				{
					ppp->ipopts &= ~(IPCP_DNSS);  /* Disable */
				}
				else
				{
					INET_MEMCPY(&ip_addr, cp2+2, 4);
					ppp->second_dns = ntohl(ip_addr);
				}
				break;
			case XIPCPopt_IPAD:
			default:
				break;
			}
		}
		if (prevstate & IPCPrxACK)
			ppp->state=LCPopen;
		else 
		{
			ppp->state &= ~(IPCPtxREQ | IPCPrxACK);
			ppp->counter = MAXCONF;
			ppp->timems = ppp_time();
		}
		break;
	case IPCPterm_req:
		*cp = IPCPterm_ack;
		if ((prevstate & IPCPopen) == IPCPopen) 
		{
			ppp->counter = 0;
			ppp->state=LCPopen;
		}
		else
			ppp->state=LCPopen | IPCPtxREQ;
		ppp_printf("Send IPCP Reply %s\n", lcp_code_str(*cp));
		ppp_xmit(ppp, skb, 0, PROTipcp);
		return PPP_OK;
	case IPCPterm_ack:
		if (cp[1] != ppp->id)
		{
			skb_free(skb);
			return PPP_REJ;
		}
		if ((prevstate & IPCPopen) == IPCPopen)
			ppp->state=LCPopen;
		else
			ppp->state &= ~IPCPrxACK;
		skb_free(skb);
		return PPP_REJ;
	default:
		skb_free(skb);
		return PPP_REJ;
	}
	skb_free(skb);
	return PPP_OK;
}

static int pap_input(struct sk_buff *skb, PPP_DEV_T *ppp)
{
	int i1, naklen, rejlen;
	unsigned short us1, us2;
	unsigned char prevstate, *cp, *cp2;
	u8 *nakbuf = t_buf;
	int naklimit = (PPP_MSS+PPP_HEADER_SPACE+2)/2;
	u8 *rejbuf = t_buf+naklimit;
	int rejlimit = naklimit;

	ppp_printf("PAP Input: ");
	
	cp = (unsigned char *)(skb->data);  /* Point to PPP info. */
	us1 = cp[0];                                /* code */
	us2 = cp[1];                                /* identifier */
	i1 = ((int)cp[2] << 8) + cp[3] - 4;         /* length */
	cp2 = cp + 4;                               /* Point to data */
	naklen = rejlen = 0;                        /* No nak/rej yet */
	prevstate = ppp->state;        /* Previous state of PPP */

	switch (us1) 
	{
	case PAPauth_req:
		skb_free(skb);
		return PPP_TERM;
	case PAPauth_ack:
		ppp_printf(" AUTH_ACK, pap finished!\n");
		ppp->opt4 &= ~(AUTHppap|AUTHpwat);	/* Host accepted */
		ppp->auth_state = 1;
		ppp->counter = MAXCONF;
		break;
	case PAPauth_nak:
		ppp_printf(" AUTH_NAK,");
		if(ppp->opt4 & AUTHpwat)
		{
			ppp_printf(" PPP TERM!\n");
			ppp->id++;
			ppp->error_code = PPP_PASSWD;
			skb_free(skb);
			return PPP_TERM;
		}else
		{
			ppp_printf(" Ignore(LCP not complete)!\n");
		}
		break;
	}
	skb_free(skb);
	return PPP_OK;
}

int chap_input(struct sk_buff *skb, PPP_DEV_T *ppp)
{
	int i1, i2, naklen, rejlen;
	unsigned short us1, us2;
	unsigned char prevstate, *cp, *cp2, *cp3;

	ppp_printf("CHAP Input\n");
	
	cp = (unsigned char *)(skb->data);  /* Point to PPP info. */
	us1 = cp[0];                                /* code */
	us2 = cp[1];                                /* identifier */
	i1 = ((int)cp[2] << 8) + cp[3] - 4;         /* length */
	cp2 = cp + 4;                               /* Point to data */
	naklen = rejlen = 0;                        /* No nak/rej yet */
	prevstate = ppp->state;        /* Previous state of PPP */
	switch (us1) 
	{
	case CHAPchallenge:
		ppp_printf("\t code=Challenge,id=%d\n", us2);
		ppp_printf("---------------CHAP Challenge--------------\n");
		ppp->opt4 |= AUTHpwat;			/* Wait for success/fail */
		*cp = CHAPresponse; 				/* Signal a response (2) */
		us2 = *cp2; 						/* Value size */
		if (ppp->opt4 & AUTHpmsc) 
		{				 
			cp3 = cp2 + 50;				 /* End of data field */
			INET_MEMCPY(cp3, cp2+1, us2);		  /* Save challenge value */
			if(PPP_MSCHAP_USED(ppp))
			{
				chapms_make_response(cp2, 0, NULL, cp3, ppp->passwd, 
					strlen(ppp->passwd), NULL);
			}else if(PPP_MSCHAPV2_USED(ppp))
			{
				chapms2_make_response(cp2, 0, ppp->userid, cp3, ppp->passwd, 
					strlen(ppp->passwd), (unsigned char*)ppp->mschapv2_cache);
			}
			i2 = 50;								 /* place to add id */
			rejlen = 54;							 /* add name length */
		}
		else if (ppp->opt4 & AUTHpchp) 
		{
			/* CHAP response:
			** MD5(id + secret + challenge) ==> new value field
			*/
			cp3 = cp2 + i1;
			*cp3 = cp[1];//ID
			strcpy((char *)cp3+1, ppp->passwd); /* PassWd */
			i1 = strlen(ppp->passwd) + 1;		 /* PassWd len + ID */
			INET_MEMCPY((char *)cp3+i1, cp2+1, us2);	  /* Copy challenge */
			i1 += us2; 							 /* Length to hash */
			*cp2 = 16; 							 /* Value size is 16 */
			ppp_md5_digest(cp2+1, cp3, i1);
			i2 = 17;								 /* Place to add id */
			rejlen = 21;							 /* Add name length */
		}else
		{
			i2 = 0;
		}
		strcpy((char *)cp2 + i2, ppp->userid);	/* Add name */
		rejlen += strlen(ppp->userid);		/* Add length of name */
		cp[2] = rejlen >> 8, cp[3] = rejlen;	/* Length */
		__skb_trim(skb, rejlen);
		ppp_printf("Send CHAP Reply CHAPresponse\n");
		ppp_xmit(ppp, skb, 0, PROTchap);
		return PPP_OK;
	case CHAPsuccess:
		ppp_printf("\t code=success,id=%d\n", us2);
		if(PPP_MSCHAPV2_USED(ppp))
		{
			if(memcmp(cp+4, ppp->mschapv2_cache, 42)!=0)
			{
				ppp_printf("Fail in MS-CHAPV2\n");
				buf_printf((unsigned char*)ppp->mschapv2_cache, 42, "Local mschapv2:");
				buf_printf(cp+4, 42, "Peer mschapv2:");
				return PPP_TERM;
			}
		}
		ppp->opt4 &= ~(AUTHpchp|AUTHpmsc|AUTHpwat);  /* Accepted */
		ppp->auth_state = 1;
		break;
	
	case CHAPfailure:
		ppp_printf("\t code=failure,id=%d\n", us2);
		ppp->id++;
		ppp->error_code = PPP_PASSWD;
		break;
	}
	skb_free(skb);
	return PPP_OK;
}

int ppp_input(struct sk_buff *skb, PPP_DEV_T *ppp)
{
	int i1, i3, naklen, rejlen;
	unsigned short protocol, us1, us2;
	unsigned char prevstate, *cp, *cp2;
	int err =0;

#define GO_DROP() goto drop


	cp = skb->data;
	//buf_printf(cp, skb->len, "In Message:");

	/*
	** Check the async framing:
	** - address and control,
	** - protocol,
	*/
	if(ppp->link_type == PPP_LINK_ETH)
	{
		/*增加对PPPoE的处理*/
		cp2 = cp;
		protocol = *(short*)cp2;
		cp2 += 2;
		skb->nh.raw = __skb_pull(skb, 2);
	}else
	{
		cp2 = cp;
		i3 = 0;
		if(cp2[0]!=0xff&&cp2[1]!=0x03)
		{
			if(cp2[0]==0x21)
			{
				protocol = htons(PROTip);
				cp2 += 1;
				skb->nh.raw = __skb_pull(skb, 1);
			}
			else
			{
				/*
				在南京使用s90 cdma发现有一些卡
				chap阶段收到没有0xff,0x03
				*/
				protocol = *(short*)cp2;
				cp2 += 2;
				skb->nh.raw = __skb_pull(skb, 2);
			}
			ppp_printf("NO 0xFF,0x03, Ignore...\n");
		}else
		{
			cp2 += 2;
			protocol = *(short*)cp2;
			cp2 += 2;
			skb->nh.raw = __skb_pull(skb, 4);
		}
	}
	ppp->last_rx_timestamp = inet_jiffier;
	prevstate = ppp->state; 	   /* Previous state of PPP */
	if (prevstate == PPPopen) 
	{
		if (protocol == htons(PROTip)) 
		{
			/* ip packet */
			ppp_printf("PPP Input IP packet(size=%d)\n",
				skb->len
				);
			skb->dev = ppp->inet_dev;
			ip_input(skb, ppp->inet_dev);
			return 0;
		}
	}

	/* Handle PPP traffic next */
	ppp_printf("PPP Input packet(proto=%04x, size=%d) State %08x[%d]\n", 
		ntohs(protocol),
		skb->len,
		ppp->state,
		inet_jiffier
		);


	/* Set up helpful pointers and other values for LCP, Auth and IPCP */

	cp = (unsigned char *)(skb->data);  /* Point to PPP info. */
	us1 = cp[0];                                /* code */
	us2 = cp[1];                                /* identifier */
	i1 = ((int)cp[2] << 8) + cp[3] - 4;         /* length */
	cp2 = cp + 4;                               /* Point to data */
	naklen = rejlen = 0;                        /* No nak/rej yet */

	if (protocol == htons(PROTlcp)) 
	{
		err = lcp_input(skb, ppp);
	}
	else if ((prevstate & LCPopen) != LCPopen)
	{
		ppp_printf("Silence Drop\n");
		GO_DROP();                          /* Only LCP until LCP is open */
	}
	else if (protocol == htons(PROTpap)) 
	{
		err = pap_input(skb, ppp);
	}
	/* CHAP/MS-CHAP auth allowed */
	else if (protocol == htons(PROTchap))
	{
		err = chap_input(skb, ppp);
	}	 
	else if (ppp->opt4 != 0)
	{
		ppp_printf("Silence Drop\n");
		GO_DROP();                      /* No IPCP until authenticated */
	}
	else if (protocol == htons(PROTipcp)) 
	{
		err = ipcp_input(skb, ppp);
	}
	else 
	{
		/*  Received a Protocol that is not known */
    	/* drop in silence  */
		ppp_printf("Silence Drop\n");
		GO_DROP();
	}

	switch(err)
	{
	case PPP_REJ:
		return -1;
	case PPP_TERM:
		goto term;
	case PPP_OK:
		break;
	default:
		return err;
	}
    	


	/*
	** Handle outbound link negotiations if necessary.
	**
	** If the link is open but wasn't prior to reception of the screening
	**  packet, PPP has entered the open state and must pay a toll fee
	**  and have it's cargo checked for insect bearing fruit.
	*/
	if (ppp->state != PPPopen) 
	{
		/*
		** counter is used as the retry counter.
		**
		** Initially its value is set to MAXCONF/MAXTERM
		** For each Control Protocol (CP) packet sent it is decremented
		** If the PPP state transition diagram (RFC 1661) says to initialize
		**  the restart counter (irc) for a certain condition, we do so in
		**  the CP code.
		*/
		if (ppp->counter)
			ppp_negotiate(ppp);
	}
	else if (prevstate != PPPopen) 
	{
	}
	return -2;

drop:
	skb_free(skb);
	return -1;

/* here we start closing/stopping state */
term:
	ppp_down(ppp);
	//ppp_negotiate(ppp);
	return -1;
}


/* for debug */
static u8 hdlc_recv_frame[4096];
static int hdlc_recv_offset = 0;
static int hdlc_recv_count=0;
#define hdlc_recv_start() hdlc_recv_offset = 0
void hdlc_recv_add(u8 c) 
{
	if(hdlc_recv_offset<sizeof(hdlc_recv_frame)) 
		hdlc_recv_frame[hdlc_recv_offset++]=(u8)((c)&0xff);
	else
	{ 	
	} 
}

#define hdlc_recv_end() \
if(0){ \
	char desc[20]; \
	sprintf(desc, "HDLC Frame Recv %d", hdlc_recv_count++);\
	buf_printf(hdlc_recv_frame, hdlc_recv_offset, desc); \
}


void checksum_err_printf(u8 *buf, int size, 
		u8 *frame, int frame_size)
{
	int i, j, line;
	
	err_printf("\nCheckSum Error:\n");
	err_printf("ppp packet size=%d\n", size);
	line = size>>4;
	for(j=0; j<line; j++)
	{
		for(i=0; i<16; i++)
		{
			err_printf("0x%02x,", buf[i]);
		}
		err_printf(" ");
		for(i=0; i<16; i++)
		{
			if(buf[i]>=0x20&&buf[i]<=0x7e)
			{
				err_printf("%c ", buf[i]);
			}else
			{
				err_printf(". ");
			}
		}
		err_printf("\n");
		buf += 16;
	}
	for(i=0; i<(size&0xf); i++)
	{
		err_printf("0x%02x,", buf[i]);
	}
	err_printf(" ");
	for(i=0; i<(size&0xf); i++)
	{
		if(buf[i]>=0x20&&buf[i]<=0x7e)
		{
			err_printf("%c ", buf[i]);
		}else
		{
			err_printf(". ");
		}
	}
	err_printf("\n");
	err_printf("Frame Size=%d\n", frame_size);
	for(i=0; i<frame_size; i++)
	{
		err_printf("0x%02x,", frame[i]);
		if((i&0xf)==0xf)
		{
			err_printf("\n");
		}
	}
	if(((i-1)&0xf)!=0xf)
		err_printf("\n");
	err_printf("-----------------------------------\n");
}

void packet_printf(u8 *buf, int size, char *descr)
{
	int i, j, line;
	
	err_printf("\n%s\n", descr);
	err_printf("ppp packet size=%d\n", size);
	line = size>>4;
	for(j=0; j<line; j++)
	{
		err_printf("[%04x] ", j);
		for(i=0; i<16; i++)
		{
			err_printf("0x%02x,", buf[i]);
		}
		err_printf(" ");
		for(i=0; i<16; i++)
		{
			if(buf[i]>=0x20&&buf[i]<=0x7e)
			{
				err_printf("%c", buf[i]);
			}else
			{
				err_printf(".");
			}
		}
		err_printf("\n");
		buf += 16;
	}
	if((size&0xf)>0)
	{		
		err_printf("[%04x] ", j);
	}
	for(i=0; i<(size&0xf); i++)
	{
		err_printf("0x%02x,", buf[i]);
	}
	for(;i<16; i++)
	{
		err_printf("     ");
	}
	err_printf(" ");
	for(i=0; i<(size&0xf); i++)
	{
		if(buf[i]>=0x20&&buf[i]<=0x7e)
		{
			err_printf("%c", buf[i]);
		}else
		{
			err_printf(".");
		}
	}
	err_printf("\n");
	err_printf("-----------------------------------\n");
}

void ppp_buf_check(PPP_DEV_T *ppp, char *where)
{
	int i, bad_bufin = 0, bad_rcv_buf=0, 
			bad_bufout=0, bad_snd_buf = 0, bad_mg1=0, bad_mg2=0;
	PPP_BUF_T *rcv_buf, *snd_buf;
	u8 *bufin, *bufout;

	if(ppp->magic_id!=0xffeeccdd)
		bad_mg1 = 1;
	if(ppp->magic_id2 != 0xffeeccdd)
		bad_mg2 = 1;
	ppp_irq_disable(ppp);
	bufin = ppp->bufin;
	rcv_buf = ppp->rcv_buf;
	bufout = ppp->bufout;
	snd_buf = ppp->snd_buf;
	ppp_irq_enable(ppp);
	if(bufin)
	{
		//assert(ppp->rcv_buf);
		for(i=0; i<PPP_RCV_QLEN; i++)
		{
			if(rcv_buf == &(ppp->rcv_bufs[i]))
				break;
		}
		if(i>=PPP_RCV_QLEN)
		{
			err_printf("RcvBuf i=%d\n", i);
			bad_rcv_buf = 1;
			bad_bufin = 1;
		}
		else
		{
			if(bufin<rcv_buf->data||
				bufin>rcv_buf->data+PPP_MSS)
			{
				err_printf("Bufin %08x, data %08x\n",
					bufin, rcv_buf->data);
				bad_bufin = 1;
			}
		}
	}else
	{
		if(rcv_buf != NULL)
		{
			err_printf("Bufin %08x, buf rcv_buf %08x\n",
				bufin, rcv_buf);
			bad_rcv_buf = 1;
		}
	}
	
	if(bufout)
	{
		for(i=0; i<PPP_SND_QLEN; i++)
		{
			if(snd_buf == &(ppp->snd_bufs[i]))
				break;
		}
		if(i>=PPP_SND_QLEN)
		{
			bad_snd_buf = 1;
			bad_bufout = 1;
			err_printf("SndBuf i=%d\n", i);
		}
		else
		{
			if(bufout<snd_buf->data||bufout>snd_buf->data+PPP_MSS )
			{
				bad_bufout = 1;
				err_printf("Bufout %08x, data %08x\n",
					bufout, snd_buf->data);
			}
		}
	}else
	{
		if(snd_buf!=NULL)
		{
			bad_snd_buf = 1;
			err_printf("Bufout %08x, but SndBuf %08x\n",
				bufout, snd_buf);
		}
	}
	
	if(bad_mg1==0&&bad_mg2==0&&
		bad_bufin==0&&bad_rcv_buf==0&&
		bad_bufout==0&&bad_snd_buf==0)
		return;
	err_printf("[%08x]magic id %08x\n", &ppp->magic_id, ppp->magic_id);
	err_printf("[%08x]magic id2 %08x\n", &ppp->magic_id2, ppp->magic_id2);
	err_printf("Check Fail in %s\n", where);
	err_printf("bufin %08x is %s\n",
		bufin, bad_bufin==0?"OK":"Bad");
	err_printf("rcvbuf %08x is %s\n",
		rcv_buf, bad_rcv_buf==0?"OK":"Bad");
	err_printf("PPP Recv Queue:\n");
	for(i=0; i<PPP_RCV_QLEN; i++)
	{
		err_printf("[%d]%08x: %08x, %08x, status=%08x, data=%08x, len=%08x\n",
			i, 
			ppp->rcv_bufs+i,
			ppp->rcv_bufs[i].list.prev,
			ppp->rcv_bufs[i].list.next,
			ppp->rcv_bufs[i].status,
			ppp->rcv_bufs[i].data,
			ppp->rcv_bufs[i].len
			);
	}
	err_printf("bufout %08x is %s\n",
		bufout, bad_bufout==0?"OK":"Bad");
	err_printf("sndbuf %08x is %s\n",
		snd_buf, bad_snd_buf==0?"OK":"Bad");

	err_printf("PPP Send Queue:\n");
	for(i=0; i<PPP_SND_QLEN; i++)
	{
		err_printf("[%d]%08x: %08x, %08x, status=%08x, data=%08x, len=%08x\n",
			i, 
			ppp->snd_bufs+i,
			ppp->snd_bufs[i].list.prev,
			ppp->snd_bufs[i].list.next,
			ppp->snd_bufs[i].status,
			ppp->snd_bufs[i].data,
			ppp->snd_bufs[i].len
			);
	}
	packet_printf((u8*)ppp, sizeof(PPP_DEV_T), "ppp mem:");
	assert(0);

}

/*
** 由中断调用
** 从串口收取一个字符
** Note: 禁止通过串口打印信息
**         否则会导致死循环
**/
static void _ppp_recv_char(int cc,void *data)
{
	INET_DEV_T *dev;
	PPP_DEV_T *ppp = (PPP_DEV_T*)data;

	if(!ppp||!ppp->inet_dev)
		return;
	dev = ppp->inet_dev;
	cc &= 0xff; /* Force the data to be 8-bits, not 16 for some drivers */

	if(ppp->link_ops&&ppp->link_ops->irq_rx)
		ppp->link_ops->irq_rx(ppp, cc);

	//ppp_printf("ppp_recv_char %02x\n", cc);
	/* If first octet in */
	if (ppp->bufin == NULL) 
	{
		int index;
		/* FLAG (0x7e) should always begin and end a PPP HDLC frame */
		if (cc != FLAG && ppp->lastin != FLAG)
		{
			goto out;
		}
		hdlc_recv_start();
		index = ppp_get_buf(ppp->rcv_bufs, PPP_RCV_QLEN, 1);
		//ppp_printf("ppp_rcv_char index=%d\n", index);
		assert(index>=0&&index<PPP_RCV_QLEN);
		ppp->rcv_buf = ppp->rcv_bufs+index;
		assert(ppp->rcv_buf->status == PPP_BUF_FREE);
		ppp->bufin = ppp->rcv_buf->data;
		if (cc == FLAG)                 /* First character should be 0x7e */
		{
        		hdlc_recv_add(cc);
			goto lab4;
		}
		*ppp->bufin++ = FLAG;       /* lastin was flag so now use it */
		hdlc_recv_add(FLAG);
		hdlc_recv_add(cc);
	}else
	{
		hdlc_recv_add(cc);
	}

	if (ppp->lastin == CTL_ESC)  /* Current character is escaped */
	{  
		cc ^= 0x20;                     /* Decode character */
		ppp->lastin = 0;
		goto lab3;
	}

	ppp->lastin = cc;
	if (cc == CTL_ESC)/* Next character is data */
	{
		goto out;
	}

	if (cc == FLAG) /* End of frame flag found */
	{
		*ppp->bufin++ = cc;
		hdlc_recv_end();
		ppp->rcv_buf->len = (u32)(ppp->bufin - ppp->rcv_buf->data);
		if(ppp->rcv_buf->len<=4)
		{
			goto drop;
		}	

		ppp->rcv_buf->status = PPP_BUF_USED;
		dev->flags |= FLAGS_RX;
		INET_SOFTIRQ(INET_SOFTIRQ_RX);
drop:
		ppp->bufin = NULL;
		ppp->rcv_buf = NULL;
		goto out;
	}

lab3:
	/*
	 ** Check only the first three characters for addr/ctl and protocol.
	 ** If there are any problems, let screen() take care of them.
	 */
	#if 0
	if (ppp->opt3 < 4) 
	{
		ppp->opt3++;
	}
	#endif
lab4:
	if (ppp->bufin < (ppp->rcv_buf->data+PPP_MSS+PPP_HEADER_SPACE+1))
		*ppp->bufin++ = cc;
	else
	{
		//err_printf("Big toooooooo  larg!\n");		
	}
	
out:
	return;
}

void ppp_recv_char(int cc, PPP_COM_OPS *ops)
{
	if(ops==NULL||ops->recv_char==NULL)
		return;
	ops->recv_char(cc, ops->data);
}

/* for debug */
static u8 hdlc_snd_frame[4096];
static int hdlc_snd_offset = 0;
static int hdlc_snd_count=0;
#define hdlc_snd_start() hdlc_snd_offset = 0
#define hdlc_snd_add(c) hdlc_snd_frame[hdlc_snd_offset++]=(u8)((c)&0xff)
#define hdlc_snd_end() \
if(0){ \
	char desc[20]; \
	sprintf(desc, "HDLC Frame Send %d", hdlc_snd_count++);\
	buf_printf(hdlc_snd_frame, hdlc_snd_offset, desc); \
}

/*
** 由中断调用
** 通过串口发送一个字符
** Note: 禁止通过串口打印信息
**         否则会导致死循环
**/
static int _ppp_xmit_char(void *data)
{
	unsigned char cc;
	PPP_BUF_T *buf;
	PPP_DEV_T *ppp = (PPP_DEV_T*)data;

	if(!ppp)
		return -1;
	
	//ppp_printf("%s ppp_xmit_char\n", ppp->inet_dev->name);
	buf = ppp->snd_buf;
	
	if (ppp->snd_flags) 
	{
		assert(buf != NULL);
		if (ppp->snd_flags & PPP_SND_ESC)
		{
			ppp->snd_flags -= PPP_SND_ESC;
			hdlc_snd_add(ppp->nxtout);
			return ppp->nxtout;
		}
		if (ppp->snd_flags & PPP_SND_END) 
		{
			ppp->snd_flags = 0;
			hdlc_snd_add(FLAG);
			hdlc_snd_end();
			cc = FLAG;
			goto out;
		}
		if (ppp->snd_flags & PPP_SND_CHECK1)
		{
			cc = buf->data[0];
			ppp->snd_flags = PPP_SND_CHECK2;
		}else if (ppp->snd_flags & PPP_SND_CHECK2)
		{
			cc = buf->data[1];
			ppp->snd_flags = PPP_SND_END;
		}
	}else
	{
		if (ppp->chsout == 0) 
		{
			if(ppp->snd_buf)
			{
				ppp->snd_buf->status = PPP_BUF_FREE;/* free it */
				ppp->snd_buf = NULL;
				ppp->bufout = NULL;
			}
			ppp->snd_buf = ppp_find_buf_used(ppp->snd_bufs, PPP_SND_QLEN);
			if(ppp->snd_buf==NULL)
			{
				ppp->snd_flags = 0;
				if(DEV_FLAGS(ppp->inet_dev, FLAGS_UP))
				{
					ppp->inet_dev->flags |= FLAGS_TX;
				}
				return -1;
			}
			ppp->snd_flags = 0;
			ppp->snd_buf->status = PPP_BUF_DOING;
			hdlc_snd_start();
			ppp->inet_dev->stats.snd_pkt++;
			buf = ppp->snd_buf;
			/* Point to start of PPP frame */
			ppp->bufout = (u8*)(buf->data+2);/* data[0~1] is checksum */
			/* Message length (plus 2 for checksum) */
			assert(buf->len>4&&buf->len<PPP_MSS+PPP_HEADER_SPACE+2);

			ppp->chsout = buf->len-2;
			hdlc_snd_add(FLAG);
			cc = FLAG;
			/* we can transmit more data */
			if(DEV_FLAGS(ppp->inet_dev, FLAGS_UP))
			{
				ppp->inet_dev->flags |= FLAGS_TX;
			}
			goto out;
		}else
		{
			buf = ppp->snd_buf;
		}
		if (--ppp->chsout == 0)
			ppp->snd_flags = PPP_SND_CHECK1;
		cc = *ppp->bufout++;
	}


	/*
	 * For asynchronous links, ASCII control characters take form
	 * CTL_ESC, cc+0x20 unless the other host has turned this off.
	 * Bit n zero in ppp->hw.ul1 means that ASCII value n can be
	 * sent directly.  Direct binary values are only sent when the
	 * link is established.
	 */
	if (cc < 0x20)
	{
		if ((ppp->state & LCPopen) != LCPopen || 
			((ppp->ACCM >> cc) & 1) ||
			ppp->auth_state != 1)
			goto lab8;
	}
	if (cc == FLAG || cc == CTL_ESC)
	{
lab8:
		ppp->nxtout = cc ^ 0x20;
		ppp->snd_flags |= PPP_SND_ESC;
		cc = CTL_ESC;
	}
	hdlc_snd_add(cc);

out:
	ppp->last_tx_timestamp = inet_jiffier;
	return cc;
}

int ppp_xmit_char(PPP_COM_OPS *ops)
{
	if(ops==NULL||ops->xmit_char==NULL)
		return -1;
	return ops->xmit_char(ops->data);
}

static int ppp_inet_open(INET_DEV_T *dev, unsigned int flags)
{
	return -1;
}

static int ppp_inet_close(INET_DEV_T *dev)
{
	return -1;
}

/* called by tcp/ip stack */
static int ppp_inet_xmit(INET_DEV_T *dev, struct sk_buff *skb, 
			u32	 nexthop, u16 proto)
{
	PPP_DEV_T *ppp;
	int err;
	
	if(skb == NULL)
	{
		return NET_OK;/*nothing to do */
	}
	assert(dev);
	ppp = (PPP_DEV_T *)dev->private_data;
	if(proto != ETH_P_IP||!DEV_FLAGS(dev, FLAGS_UP))
	{
		return NET_ERR_ARG;
	}
	if(!DEV_FLAGS(dev, FLAGS_TX))
	{
		return NET_ERR_MEM;
	}
	err = ppp_write(ppp, skb->data, skb->len, PROTip);
	return err;
}

struct sk_buff * ppp_get_rcv(PPP_DEV_T *ppp, INET_DEV_T *dev)
{
	int size;
	PPP_BUF_T *buf = NULL;
	struct sk_buff *skb;

	ppp_irq_disable(ppp);
	buf = ppp_find_buf_used(ppp->rcv_bufs, PPP_RCV_QLEN);
	if(buf)
		buf->status = PPP_BUF_DOING;
	ppp_irq_enable(ppp);
	if(buf==NULL)
	{
		return NULL;
	}
	size = buf->len;
	if(size<6||size>PPP_MSS+PPP_HEADER_SPACE+2)
	{
		ppp_printf("Fail in ppp_get_rcv size=%d\n", size);
		
		//assert(size>6&&size<PPP_MSS);
	}
	/* 由于skb可利用作为发送,因此要预留一些空间*/
	skb = skb_new(size+8);
	if(!skb)
	{
		/* no buffer */
		NET_STATS_DROP(&dev->stats);

		/* free ppp buf */
		ppp_irq_disable(ppp);
		buf->status = PPP_BUF_FREE;
		ppp_irq_enable(ppp);
		return NULL;
	}
	skb_reserve(skb, 4);
	INET_MEMCPY(skb->data, buf->data+1, size-2);
	__skb_put(skb, size-4);

	/* free ppp buf */
	ppp_irq_disable(ppp);
	buf->status = PPP_BUF_FREE;
	ppp_irq_enable(ppp);
	return skb;
}

/* called by tcp/ip stack  softirq */
static int ppp_inet_poll(INET_DEV_T *dev, int count)
{
	PPP_DEV_T *ppp = (PPP_DEV_T *)dev->private_data;
	struct sk_buff *skb;
	if(ppp->prepare)
		return 0;
	//ppp_printf("ppp_inet_poll rcv_queue %d\n", ppp->rcv_queue.qlen);
	while(1)
	{
		skb = ppp_get_rcv(ppp, dev);
		if(!skb)
		{
			dev->flags &= ~FLAGS_RX;
			break;
		}
		if(ppp_checksum(skb->len, skb->data)!=0)
		{
			/* checksum error */
			ppp_printf("RECV checksum error!\n");
			#if PPP_PKT_DEBUG
			buf_printf(skb->data, skb->len+2,  "Checksum Error packet:");
			#endif
			dev->stats.recv_err_check++;
			skb_free(skb);
			ppp->chsum_err_count++;
			if(ppp->chsum_err_count > 10)
			{
				ppp_printf("Too Many checksum error, Down DataLink!\n");
				ppp->chsum_err_count = 0;
				tcp_link_change_notify(ppp->inet_dev, 
					ppp->local_addr, PPP_LINKDOWN);
				ppp_closed(ppp);
				break;
			}
		}else
		{
			ppp->chsum_err_count = 0;
			dev->stats.recv_pkt++;
			#if PPP_PKT_DEBUG
			buf_printf(skb->data, skb->len+2, "Recv packet:");
			#endif
			ppp_input(skb, ppp);
		}
		//ppp_printf("ppp_input end...........\n");
	}
	return 0;
}

static INET_DEV_OPS ppp_ops=
{
	ppp_inet_open,
	ppp_inet_close,
	ppp_inet_xmit,
	ppp_inet_poll,
};

static PPP_COM_OPS com_ops=
{
	10,/* prio */
	_ppp_recv_char,
	_ppp_xmit_char,
	NULL,/* data*/
	NULL,/* NEXT */
};

void ppp_inet_init(PPP_DEV_T *ppp, INET_DEV_T *dev, char *name, int if_index)
{
	int size;
	ppp->inet_dev = dev;
	memset(dev, 0, sizeof(INET_DEV_T));
	size = strlen(name);
	if(size>sizeof(dev->name)-1)
		size = sizeof(dev->name)-1;
	INET_MEMCPY(dev->name, name, size);
	dev->magic_id = DEV_MAGIC_ID;
	dev->mask = 0xffffffff;
	dev->broadcast_addr=0xffffffff;
	dev->private_data = ppp;
	dev->dev_ops = &ppp_ops;
	dev->ifIndex = if_index;
	dev->header_len = 0;/* we reserve buffer for the packet */
	dev->flags = FLAGS_NOARP|FLAGS_NODHCP;
	dev_register(dev, 1);
}

static void ppp_inet_up(PPP_DEV_T *ppp)
{
	INET_DEV_T *dev = ppp->inet_dev;	
	assert(ppp->state == PPPopen);
	
	dev->flags = FLAGS_UP|FLAGS_TX;
	dev->addr = ppp->local_addr;
	dev->next_hop = ppp->remote_addr;
	dev->dns = ppp->primary_dns;
	dev->mtu = ppp->mss;
	dev->mask = 0xffffffff;
	
	ppp->timestamp = 0;//Clear timer of Login
	ppp->echo_timeout = inet_jiffier;
	ppp->echo_state = ECHO_DETECT;
	ppp_printf("PPP Inet UP!\n");
}

static void ppp_inet_down(PPP_DEV_T *ppp)
{
	INET_DEV_T *dev = ppp->inet_dev;

	dev->flags &= ~(FLAGS_UP|FLAGS_TX|FLAGS_RX);
	/* 清空IP配置信息*/
	dev->addr = dev->next_hop = dev->dns = 0;	
	ppp->local_addr = 0;
	ppp->mask_addr = 0;
	ppp->remote_addr = 0;
	ppp->primary_dns = 0;
	ppp->second_dns = 0;
	ppp_printf("PPP Inet Down!\n");
}

int ppp_set_authinfo(PPP_DEV_T *ppp, char *name, char *passw)
{
	int size;

	if(name==NULL)
		name = "";
	if(passw==NULL)
		passw = "";
	size = strlen(name);	
	if(str_check_max(name, sizeof(ppp->userid)-1)!=NET_OK||
		str_check_max(passw, sizeof(ppp->passwd)-1)!=NET_OK)
		return NET_ERR_STR; 

	if(size+1 > sizeof(ppp->userid))
		size = sizeof(ppp->userid)-1;
	INET_MEMCPY(ppp->userid, name, size);
	ppp->userid[size] = 0;
	size = strlen(passw);
	if(size+1 > sizeof(ppp->passwd))
		size = sizeof(ppp->passwd)-1;
	INET_MEMCPY(ppp->passwd, passw, size);
	ppp->passwd[size] = 0;
	return 0;
}

int ppp_dev_init(PPP_DEV_T *ppp)
{
	memset(ppp, 0, sizeof(PPP_DEV_T));
	ppp->magic_id = 0xffeeccdd;
	ppp->magic_id2 = 0xffeeccdd;
	ppp->com_ops = com_ops;
	ppp->com_ops.data = (void*)ppp;
	ppp->ifType = 23;
	ppp->state = PPPclsd;
	ppp->error_code = PPP_LINKDOWN;
	ppp->mss = PPP_MSS;
	return 0;
}

/*
** * * * * * *
** ppp_start()  Internal function to make PPP ready for negotiations
**
**
** DESCRIPTION:
**   This function will set PPP's data to initial values for negotiations.
**   It neither checks nor changes the state.  The apflag is there because
**   active links usually do not request authentication, but passive ones
**   usually do.
**
** USAGE COMMENTS:
**   This is an internal function to PPP.
**
** * * * * * *
*/
void ppp_start(PPP_DEV_T *ppp)
{
	ppp_printf("ppp_start..........\n");
	ppp->state = 0;
	ppp->auth_state = 0;
	ppp->recv_id = -1;
	ppp->recv_prot = -1;

	PPP_ALG_INIT(ppp);

	/* Options used in first configure request for LCP */
	ppp->locopts = 0
#if PPP_MRU
		+ (1<<LCPopt_MRU)
#endif
#if ASYNC
		+ (1<<LCPopt_ASYC)
#endif
#if MAGICNUM
		+ (1<<LCPopt_MNUM)
#endif
#if COMPRESSION
		+ (1<<LCPopt_PCMP)
		+ (1<<LCPopt_ACMP)
#endif		
	;

	ppp->remopts = 0;

#if IPCP_DNS & 1
	ppp->ipopts |= IPCP_DNSP | IPCP_DNSS;
#endif

	ppp->opt4 = 0;

	/*
	** The host recieve ACCM can be configured through the
	** LCP ACCM option.  The 'RACCM' value specifies the
	** initial value to request.
	*/
#if ASYNC
	ppp->raccm = RACCM;
#else
	ppp->raccm = 0xffffffff;
#endif

	/* Transmit ACCM (configured by peer request) */
	ppp->ACCM = 0xffffffff;

	/* Default MRU/MTU */
	ppp->mss = PPP_MSS;

	if (flag_login == 1)
		ppp->id++;
	//ppp->id = 1;  /* Initial ID value */
	ppp->counter = MAXCONF;        /* Initialize restart counter */
}

void ppp_closed(PPP_DEV_T *ppp)
{
	if(!ppp)
		return;
	if(ppp->comm>=0&&ppp->comm<MAX_COMM_NUM)
		ppp_com_unregister(ppp_comm+ppp->comm, &ppp->com_ops);
	ppp_inet_down(ppp);
	ppp->state = PPPclsd;
}

int ppp_login(PPP_DEV_T *ppp, char *name, char *passwd, long auth, int timeout) 
{
	int retry = timeout;

	if(ppp->init == 0)
	{
		ppp_printf("PPP NO Init!\n");
		return NET_ERR_INIT;
	}
	if(str_check_max(name, sizeof(ppp->userid)-1)!=NET_OK||
		str_check_max(passwd, sizeof(ppp->passwd)-1)!=NET_OK)
		return NET_ERR_STR;
	if(ppp_check_modem(ppp)!=0)
	{
		ppp_printf("PPP: modem dial firstly!\n");
		return PPP_MODEM;/* modem dial firstly */
	}
	if(ppp->state == PPPopen)
	{
		ppp_printf("PPP: Login OK already!\n");
		return NET_OK;
	}
	if((auth&0xf)==0)
	{
		ppp_printf("PPP: auth is bad!\n");
		return NET_ERR_ARG;
	}
	ppp->magic_id = 0xffeeccdd;
	ppp->magic_id2 = 0xffeeccdd;
	ppp->init_authent = auth;
	ppp->at_cmd_mode = 0;

	/* half-bottom */
	inet_softirq_disable();
	ppp->user_request = 0;
	ppp_set_authinfo(ppp, name, passwd);
	ppp_up(ppp);
	if(retry<=0)
	{
		/* 2 minute is the max time for ppp completion */
		ppp->timestamp = ppp_time()+120*1000;
	}else
	{
		ppp->timestamp = ppp_time()+retry;
	}
	if(ppp->timestamp == 0)
		ppp->timestamp = 1;//
	inet_softirq_enable();
	
	if(retry == 0)
	{
		//return NET_OK;
		goto out;
	}
	ppp_printf("PPPLogin wait..............\n");
	
	while(!DEV_FLAGS(ppp->inet_dev, FLAGS_UP))
	{
		if(ppp->state == PPPclsd)
		{
			ppp_printf("PPPLogin fail err=%d\n", ppp->error_code);
			return ppp->error_code;
		}
		if(retry == 0)
		{
			ppp_printf("PPPLogin fail err=TIME_OUT\n");
			return NET_ERR_TIMEOUT;
		}
		if(retry>0)
			retry--;
		inet_delay(100);
	#if 0
		if(DEV_FLAGS(ppp->inet_dev, FLAGS_RX))
		{
			inet_softirq_disable();
			ppp->inet_dev->dev_ops->poll(ppp->inet_dev, 10);
			inet_softirq_enable();
		}
	#endif
	}
	ppp_printf("PPPLogin OK!\n");
	
out:	
	#if	S90_SOFT_MODEMPPP
	set_modem_ppp_flag();
	#endif
	return NET_OK;
}

void ppp_logout(PPP_DEV_T *ppp, int err)
{
#if	S90_SOFT_MODEMPPP
	clear_modem_ppp_flag();
#endif

	if(ppp->init == 0)
	{
		ppp_printf("PPP NO Init!\n");
		return ;
	}
	if(ppp->state == PPPclsd||
		ppp->state == PPPclsng||
		ppp->user_request == PPP_USER_REQUEST_LOGOUT)
		return;
	inet_softirq_disable();	
 
	tcp_link_change_notify(ppp->inet_dev, 
		ppp->local_addr, err);
	if(tcp_conn_exist(ppp->inet_dev))
	{
		/* 存在TCP连接,则等待一定时间后才断开PPP,
		这样有利于TCP连接断开
		*/
                ppp->trigger_time = inet_jiffier+2000;
                ppp_printf("WAIT FOR TCP CLOSE: 2S!\n");
	}else
	{
		ppp->trigger_time = inet_jiffier;
	}
	ppp->user_request = PPP_USER_REQUEST_LOGOUT;
	inet_softirq_enable();
}

void ppp_set_keepalive(PPP_DEV_T *ppp, int interval)
{
	ppp->keep_alive_interval = interval;
	ppp->next_keep_alive = ppp_time()+interval;
}

int ppp_check(PPP_DEV_T *ppp)
{
	if(ppp->init == 0)
	{
		ppp_printf("PPP NO Init!\n");
		return NET_ERR_INIT;
	}
	if(DEV_FLAGS(ppp->inet_dev, FLAGS_UP))
		return NET_OK;
	if(ppp->state!=PPPclsd&&ppp->state!=PPPclsng)
		return 1;/* in doing */
	if(ppp->error_code<0)
		return ppp->error_code;
	return PPP_LINKDOWN;
}

void ppp_start_atmode(PPP_DEV_T *ppp)
{
	ppp_irq_disable(ppp);
	ppp->at_cmd_mode = 1;
	ppp->inet_dev->flags &= ~FLAGS_TX;
	ppp_irq_enable(ppp);
}

void ppp_stop_atmode(PPP_DEV_T *ppp)
{
	ppp_irq_disable(ppp);
	ppp->at_cmd_mode = 0;
	ppp_irq_enable(ppp);
	ppp_irq_tx(ppp);
}

void ppp_com_register(PPP_COM_OPS **head, PPP_COM_OPS *node)
{
	int prio;
	PPP_COM_OPS *prev;
	node->next = NULL;
	if(*head==NULL)
	{
		*head = node;
	}else
	{
		prev = *head;
		if(prev->prio > node->prio)
		{
			node->next = prev;
			*head = node;
			return;
		}
		while(prev->next)
		{
			if(node->prio<prev->next->prio)
			{
				break;
			}
			prev = prev->next;
		}
		prev->next = node;
	}
}

void ppp_com_unregister(PPP_COM_OPS **head, PPP_COM_OPS *node)
{
	PPP_COM_OPS *prev;

	if(*head == NULL)
	{
		return;
	}
	prev = *head;
	if(*head == node)
	{
		*head = prev->next;
		node->next = NULL;
		return;
	}
	while(prev->next)
	{
		if(prev->next == node)
		{
			break;
		}
	}
	if(prev->next==node)
	{
		prev->next = prev->next->next;
		node->next = NULL;
	}
	return;
}

/* 
** For Win32 Simulator
**
**/
#ifdef WIN32
static u8   xmit_buf[]=
{
	0x0,
};
static u8 rcv_buf[]=
{
	0x0,
};
static u8  xmit_offset = 0;

void ppp_test_xmit(PPP_DEV_T *ppp)
{
	static char buf[2*1024];
	static int count = 0;
	char desc[20];
	while(ppp_get_used_buf(ppp->snd_bufs, PPP_SND_QLEN)>=0)
	{
		int i=0;
		int c;
		while((c=ppp_xmit_char(ppp))>=0)
		{
			buf[i++]=c&0xff;
		}
		sprintf(desc, "Send PPP Frame %d", count++);
		buf_printf(buf, i, desc);		
		if(xmit_offset+i<sizeof(xmit_buf)&&
		    memcmp(xmit_buf+xmit_offset, buf, i) != 0)
		{
			ppp_printf("ERROR!\n");
			assert(0);
		}
		xmit_offset += i;
	}
}

static int recv_offset = 0;
void ppp_recv(PPP_DEV_T *ppp)
{
	int c; 
	if(recv_offset >= sizeof(rcv_buf))
		return;
	ppp_recv_char(rcv_buf[recv_offset++], ppp);
	do
	{
		c = rcv_buf[recv_offset++];
		ppp->com_ops.recv_char(c, ppp);
		if(DEV_FLAGS(ppp->inet_dev, FLAGS_RX))
		{
			inet_softirq_disable();
			ppp->inet_dev->dev_ops->poll(ppp->inet_dev, 10);
			inet_softirq_enable();
		}
		if(recv_offset >= sizeof(rcv_buf))
			return;
	}while(c!=FLAG);
}

//extern PPP_DEV_T ppp_dev;

void chapms2_test(void)
{
	char response[100];
	char challenge[100]={ 
		0x5B,0x5D,0x7C,0x7D,0x7B,0x3F,0x2F,0x3E,0x3C,0x2C,0x60,0x21,0x32,0x26 ,0x26 ,0x28};
	char secret[]={0x63,0x6C ,0x69 ,0x65 ,0x6E ,0x74 ,0x50 ,0x61 ,0x73 ,0x73 };
	char name[]={0x55,0x73,0x65,0x72,0x0};
	char private[100];
	int secret_len = sizeof(secret);

	char peerchange[]={0x21,0x40,0x23,0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3A,0x33,0x7C,0x7E};
	//test1();
	//return;

	chapms2_make_response(response, 0x0, name, challenge, secret, secret_len, private);
	//buf_print(challenge, 8, "Challenge:");
	//buf_print(secret, secret_len, "Secret:");
	//buf_print(response, MS_CHAP_RESPONSE_LEN+1, "Response:");
}


int ppp_testcase(long auth, char *name, char *passwd, long timeout)
{
	INET_DEV_T *dev; 
	PPP_DEV_T *ppp; 
	//chapms2_test();
	PPPLogin(name, passwd, 0xff, 0);
	dev = dev_lookup(10);
	ppp = (PPP_DEV_T*)dev->private_data;
	while(ppp->state != PPPopen)
	{
		ppp_test_xmit(ppp);
		
		ppp_recv(ppp);
		//ppp_timeout(0);
	}
	dev_show();
	mem_debug_show();
	while(1)
	{
		ppp_test_xmit(ppp);
		ppp_recv(ppp);
	}
	return 0;
}
#endif
