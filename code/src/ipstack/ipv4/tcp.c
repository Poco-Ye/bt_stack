/*
** 修改历史
1.2008-06-16
  tcp_link_change_notify中
  先调用NetCloseSocket再调用OnHook会导致内存泄漏，
  因为NetCloseSocket把pcb状态转换为FIN_WAIT_1,
  而OnHook后定时器调用tcp_link_change_notify
  把pcb转换成CLOSE_WAIT,而该状态却是等待NetCloseSocket,
  因此该pcb永远不会被释放，从而导致泄漏。
  解决方法：当pcb状态为ESTABLISHED或SYN_SEND才转换成CLOSE_WAIT，
  而其他状态则让它超时。
080716 sunJH
1.增加tcp_reset函数用于发送rst
2.tcp_recv_data取消了调试信息
3.tcp_connect中pcb->send_data_ok = 0来保证三次握手完整
4.tcp_alloc中pcb->send_data_ok = 1，从而可以根据需要打开该功能
5.tcp_next_iss修改了iss累加方式
6.增加tcp_conn_exist判断是否存在从指定接口出去的连接
7.tcp_link_change_notify中，当用户执行Logout时，强制发送RST断开连接
8.tcp_debug_print_pcbs中，增加mss调试信息
080729 sunJH
tcp_link_change_notify无条件RST连接
080820 sunJH
TCP数据传输重传时间改为固定的1秒

2017.3.27--revised return value NET_ERR_SYS in tcp_connect()

**/
#include <inet/inet.h>
#include <inet/mem_pool.h>
#include <inet/tcp.h>
#include <inet/skbuff.h>
#include <inet/inet_timer.h>
#include <inet/inet_softirq.h>
#include <inet/dev.h>
#include <inet/ip.h>
static MEM_POOL tcp_pcb_pool;
static MEM_POOL tcp_listen_pcb_pool;
static char tcp_pcb_static_buf[MEM_NODE_SIZE(sizeof(struct tcp_pcb))*
			MAX_TCP_PCB_COUNT+4];
static char tcp_listen_pcb_static_buf[MEM_NODE_SIZE(sizeof(struct tcp_pcb_listen))*
			MAX_TCP_LISTEN_PCB_COUNT+4];
struct net_stats g_tcp_stats={0,};

/* Incremented every coarse grained timer shot (typically every 500 ms). */
u32 tcp_ticks=0;
const u8 tcp_backoff[13] =
    { 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7};

/* The TCP PCB lists. */

/*
 * List of all TCP PCBs in LISTEN state 
 */
union tcp_listen_pcbs_t tcp_listen_pcbs;
/*
 * List of all TCP PCBs that are in a state in which
 * they accept or send data. 
 */
struct tcp_pcb *tcp_active_pcbs;  
/*
 * List of all TCP PCBs in TIME-WAIT state 
 */
struct tcp_pcb *tcp_tw_pcbs;

struct tcp_pcb *tcp_tmp_pcb;

static u8 tcp_timer;
static u16 tcp_new_port(void);

static struct inet_softirq_s tcp_timer_softirq;

/*
 * Initializes the TCP layer.
 */
void tcp_init(void)
{
	/* Clear globals. */
	tcp_listen_pcbs.listen_pcbs = NULL;
	tcp_active_pcbs = NULL;
	tcp_tw_pcbs = NULL;
	tcp_tmp_pcb = NULL;
  
	/* initialize timer */
	tcp_ticks = 0;
	tcp_timer = 0;  

	tcp_timer_softirq.mask = INET_SOFTIRQ_TIMER;
	tcp_timer_softirq.h = tcp_tmr;
	inet_softirq_register(&tcp_timer_softirq);

	mem_pool_init(&tcp_pcb_pool, sizeof(struct tcp_pcb), MAX_TCP_PCB_COUNT,
			tcp_pcb_static_buf, sizeof(tcp_pcb_static_buf),
			"tcp_pcb"
			);
	mem_pool_init(&tcp_listen_pcb_pool, sizeof(struct tcp_pcb_listen),
			MAX_TCP_LISTEN_PCB_COUNT, tcp_listen_pcb_static_buf,
			sizeof(tcp_listen_pcb_static_buf),
			"tcp_listen_pcb"
			);
}

/*
 * Called periodically to dispatch TCP timers.
 *
 */
void tcp_tmr(unsigned long flags)
{
	static int count = 250;
	if(count > 0)
	{
		count -= INET_TIMER_SCALE;
		return;
	}
	count = 250;
	/* Call tcp_fasttmr() every 250 ms */
	tcp_fasttmr();

	if (++tcp_timer & 1) 
	{
		/* Call tcp_tmr() every 500 ms, i.e., every other timer
		tcp_tmr() is called. */
		tcp_slowtmr();
		#if TCP_DEBUG
		{
			extern void force_ipstack_print(void);
			
				s80_printf("*****************************\n");
				s80_printf("   TCP	 TMR \n");
				s80_printf("*****************************\n");
				force_ipstack_print();
		}
		#endif
	}
}

struct tcp_pcb *tcp_pcb_malloc(void)
{
	return (struct tcp_pcb*)mem_malloc(&tcp_pcb_pool);
}

void inline _tcp_pcb_purge(struct tcp_pcb *pcb)
{
	__skb_queue_purge(&pcb->unsent);
	__skb_queue_purge(&pcb->unacked);
	__skb_queue_purge(&pcb->incoming);
	#if TCP_QUEUE_OOSEQ
	__skb_queue_purge(&pcb->ooseq);
	#endif
}

void tcp_pcb_free(struct tcp_pcb *pcb)
{
	_tcp_pcb_purge(pcb);
	mem_free(&tcp_pcb_pool, pcb);
}

struct tcp_pcb_listen *tcp_pcb_listen_malloc(void)
{
	return (struct tcp_pcb_listen*)mem_malloc(&tcp_listen_pcb_pool);
}

void tcp_pcb_listen_free(struct tcp_pcb_listen *pcb)
{
	mem_free(&tcp_listen_pcb_pool, pcb);
}

/*
 * Closes the connection held by the PCB.
 *
 */
err_t tcp_close(struct tcp_pcb *pcb)
{
	err_t err;

	#if TCP_DEBUG
	IPSTACK_DEBUG(TCP_DEBUG, ("tcp_close: closing in "));
	tcp_debug_print_state(pcb->state);
	#endif /* TCP_DEBUG */

	switch (pcb->state) 
	{
	case CLOSED:
		err = NET_OK;
		tcp_pcb_free(pcb);
		pcb = NULL;
		break;
	case LISTEN:
		err = NET_OK;
		tcp_pcb_remove((struct tcp_pcb **)&tcp_listen_pcbs.pcbs, pcb);
		tcp_pcb_listen_free((struct tcp_pcb_listen*)pcb);
		pcb = NULL;
		break;
	case SYN_SENT:
	case SYN_RCVD:
		err = NET_OK;
		tcp_pcb_remove(&tcp_active_pcbs, pcb);
		tcp_pcb_free(pcb);
		pcb = NULL;
		break;
	case ESTABLISHED:
		err = tcp_send_ctrl(pcb, TCP_FIN);
		pcb->state = FIN_WAIT_1;
		break;
	case CLOSE_WAIT:
		/* 收到fin后会设置err=NET_ERR_CLSD*/
		if(pcb->err != NET_ERR_CLSD)
		{
			/* Maybe Linkdown , we should force to close at once */
			err = NET_OK;
			tcp_pcb_remove(&tcp_active_pcbs, pcb);
			tcp_pcb_free(pcb);
			pcb = NULL;
		}else
		{
			err = tcp_send_ctrl(pcb, TCP_FIN);
			pcb->state = LAST_ACK;
		}
		break;
	default:
		/* Has already been closed, do nothing. */
		err = NET_OK;
		pcb = NULL;
		break;
	}

	if(pcb)
	{
		/* disable CALL BACK */
		tcp_arg(pcb, NULL);
		tcp_err(pcb, NULL);
	}

	if (pcb != NULL && err == NET_OK) 
	{
		err = tcp_output(pcb);
	}
	return err;
}

/*
 * Aborts a connection by sending a RST to the remote host and deletes
 * the local protocol control block. This is done when a connection is
 * killed because of shortage of memory.
 *
 */
void
tcp_abort(struct tcp_pcb *pcb)
{
	u32 seqno, ackno;
	u16 remote_port, local_port;
	u32 remote_ip, local_ip;
	#if IPSTACK_CALLBACK_API  
	void (* errf)(void *arg, err_t err);
	#endif /* IPSTACK_CALLBACK_API */
	void *errf_arg;

  
	/* Figure out on which TCP PCB list we are, and remove us. 
	 * If we are in an active state, call the receive function associated with
	 * the PCB with a NULL argument, and send an RST to the remote end. 
	 */
	if (pcb->state == TIME_WAIT)
	{
		tcp_pcb_remove(&tcp_tw_pcbs, pcb);
		tcp_pcb_free(pcb);
	} else 
	{
		seqno = pcb->snd_nxt;
		ackno = pcb->rcv_nxt;
		local_ip = pcb->local_ip;
		remote_ip = pcb->remote_ip;
		local_port = pcb->local_port;
		remote_port = pcb->remote_port;
		#if IPSTACK_CALLBACK_API
		errf = pcb->errf;
		#endif /* IPSTACK_CALLBACK_API */
		errf_arg = pcb->callback_arg;
		tcp_pcb_remove(&tcp_active_pcbs, pcb);
		_tcp_pcb_purge(pcb);
		tcp_pcb_free(pcb);
		TCP_EVENT_ERR(errf, errf_arg, NET_ERR_ABRT);
		IPSTACK_DEBUG(TCP_DEBUG, ("tcp_abort: sending RST\n"));
		tcp_rst(seqno, ackno, local_ip, remote_ip, local_port, remote_port);
	}
}

void tcp_reset(struct tcp_pcb *pcb)
{
	IPSTACK_DEBUG(TCP_DEBUG, ("tcp_reset: sending RST\n"));
	tcp_rst(pcb->snd_nxt, pcb->rcv_nxt, 
		pcb->local_ip, pcb->remote_ip, 
		pcb->local_port, pcb->remote_port);
}

/*
 * Binds the connection to a local portnumber and IP address. If the
 * IP address is not given (i.e., ipaddr == NULL), the IP address of
 * the outgoing network interface is used instead.
 *
 */
err_t tcp_bind(struct tcp_pcb *pcb, struct ip_addr *ipaddr, u16 port)
{
	struct tcp_pcb *cpcb;

	if(pcb->state != CLOSED)
		return NET_ERR_USE;
	if(pcb->local_port != 0)
		return NET_ERR_VAL;

	if (port == 0) 
	{
		port = tcp_new_port();
	}
	/* Check if the address already is in use. */
	for(cpcb = (struct tcp_pcb *)tcp_listen_pcbs.pcbs;
		cpcb != NULL; cpcb = cpcb->next) 
	{
		if (cpcb->local_port == port) 
		{
			if (ip_addr_isany(cpcb->local_ip) ||
				ip_addr_isany(ipaddr->addr) ||
				ip_addr_cmp(cpcb->local_ip, ipaddr->addr)) 
			{
				return NET_ERR_USE;
			}
		}
	}
	for(cpcb = tcp_active_pcbs;
		cpcb != NULL; cpcb = cpcb->next) 
	{
		if (cpcb->local_port == port) 
		{
			if (ip_addr_isany(&(cpcb->local_ip)) ||
				ip_addr_isany(ipaddr) ||
				ip_addr_cmp(cpcb->local_ip, ipaddr->addr)) 
			{
				return NET_ERR_USE;
			}
		}
	}

	if (!ip_addr_isany(ipaddr->addr)) 
	{
		pcb->local_ip = ipaddr->addr;
	}
	pcb->local_port = port;
	IPSTACK_DEBUG(TCP_DEBUG, "tcp_bind: bind to port %d\n", port);
	return NET_OK;
}

#if IPSTACK_CALLBACK_API
static err_t
tcp_accept_null(void *arg, struct tcp_pcb *pcb, err_t err)
{
	(void)arg;
	(void)pcb;
	(void)err;

	return NET_ERR_ABRT;
}
#endif /* IPSTACK_CALLBACK_API */

/**
 * Set the state of the connection to be LISTEN, which means that it
 * is able to accept incoming connections. The protocol control block
 * is reallocated in order to consume less memory. Setting the
 * connection to LISTEN is an irreversible process.
 *
 */
struct tcp_pcb *
tcp_listen(struct tcp_pcb *pcb)
{
	struct tcp_pcb_listen *lpcb;

	/* already listening? */
	if (pcb->state == LISTEN) 
	{
		return pcb;
	}
	if(pcb->local_port == 0)
	{
		return NULL;
	}
	lpcb = tcp_pcb_listen_malloc();
	if (lpcb == NULL) 
	{
		return NULL;
	}
	lpcb->callback_arg = pcb->callback_arg;
	lpcb->local_port = pcb->local_port;
	lpcb->state = LISTEN;
	lpcb->so_options = pcb->so_options;
	lpcb->so_options |= SOF_ACCEPTCONN;
	lpcb->ttl = pcb->ttl;
	lpcb->tos = pcb->tos;
	lpcb->local_ip = pcb->local_ip;
	tcp_pcb_free(pcb);
	#if IPSTACK_CALLBACK_API
	lpcb->accept = tcp_accept_null;
	#endif /* IPSTACK_CALLBACK_API */
  	TCP_REG(&tcp_listen_pcbs.listen_pcbs, lpcb);
	return (struct tcp_pcb *)lpcb;
}

int tcp_recv_data(struct tcp_pcb *pcb, u8 *buf, int size)
{
	int true_size, total_size;
	struct sk_buff *skb;
	total_size = 0;
	//IPSTACK_DEBUG(TCP_DEBUG, "tcp_recv_data %d, %d\n", size, pcb->incoming.qlen);
	while((skb=skb_peek(&pcb->incoming)))
	{
		true_size = (skb->len>size)?size:skb->len;
		INET_MEMCPY(buf, skb->data, true_size);
		buf += true_size;
		size -= true_size;
		total_size += true_size;
		if(true_size == skb->len)
		{
			__skb_unlink(skb, &pcb->incoming);
			skb_free(skb);
		}else
		{
			__skb_pull(skb, true_size);
		}
		if(size<=0)
			break;
	}
	if(pcb->incoming.qlen == 0)
		pcb->event_mask &= ~TCP_CAN_RCV;
	else
		pcb->event_mask |= TCP_CAN_RCV;
	if(total_size > 0)
	{
		tcp_recved(pcb, (u16)total_size);
	}else
	{
		if(pcb->state == CLOSED)
		{
			if(pcb->err != NET_OK)
			{
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_recv_data end err=%d\n",
					pcb->err);
				return pcb->err;
			}
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_recv_data end err=%d\n",
				NET_ERR_CONN);
			return NET_ERR_CONN;
		}
		if(pcb->state == CLOSE_WAIT)
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_recv_data end state=CLOSE_WAIT\n");
			if(pcb->err<0)
				return pcb->err;
			return NET_ERR_CLSD;
		}
	}
	return total_size;
}

/*
 * This function should be called by the application when it has
 * processed the data. The purpose is to advertise a larger window
 * when the data has been processed.
 *
 */
void tcp_recved(struct tcp_pcb *pcb, u16 len)
{
	if ((u32)pcb->rcv_wnd + len > TCP_WND) 
	{
		pcb->rcv_wnd = TCP_WND;
	}else 
	{
		pcb->rcv_wnd += len;
	}
	if (!(pcb->flags & TF_ACK_DELAY) &&
		!(pcb->flags & TF_ACK_NOW)) 
	{
		/*
		* We send an ACK here (if one is not already pending, hence
		* the above tests) as tcp_recved() implies that the application
		* has processed some data, and so we can open the receiver's
		* window to allow more to be transmitted.  This could result in
		* two ACKs being sent for each received packet in some limited cases
		* (where the application is only receiving data, and is slow to
		* process it) but it is necessary to guarantee that the sender can
		* continue to transmit.
		*/
		tcp_ack(pcb);
	} 
	else if (pcb->flags & TF_ACK_DELAY && pcb->rcv_wnd >= TCP_WND/2) 
	{
		/* If we can send a window update such that there is a full
		* segment available in the window, do so now.  This is sort of
		* nagle-like in its goals, and tries to hit a compromise between
		* sending acks each time the window is updated, and only sending
		* window updates when a timer expires.  The "threshold" used
		* above (currently TCP_WND/2) can be tuned to be more or less
		* aggressive  */
		tcp_ack_now(pcb);
	}

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_recved: recveived %d bytes, wnd %d (%d).\n",
			len, pcb->rcv_wnd, TCP_WND - pcb->rcv_wnd);
}

/*
 * A nastly hack featuring 'goto' statements that allocates a
 * new TCP local port.
 */
static volatile int s_TcpPortInit = 0; 
void tcp_port_reinit(void)
{
	s_TcpPortInit = 0;
}
static u16 tcp_new_port(void)
{
	struct tcp_pcb *pcb;
#ifndef TCP_LOCAL_PORT_RANGE_START
#define TCP_LOCAL_PORT_RANGE_START 4096
#define TCP_LOCAL_PORT_RANGE_END   61000
#define TCP_LOCAL_PORT_RANGE (61000 - 4096)
#endif
	static u16 port = TCP_LOCAL_PORT_RANGE_START;

	if(!s_TcpPortInit)
	{
		u32 num;
		#ifdef WIN32
		srand(time(NULL));
		num = rand() % (TCP_LOCAL_PORT_RANGE + 1);
		#else
		{
			extern void GetRandom(unsigned char *pucDataOut);
			u16 value[4];
			int i;
			GetRandom((u8*)value);
			num = value[0];
		}
		num = num % (TCP_LOCAL_PORT_RANGE + 1); 
		#endif
		port = TCP_LOCAL_PORT_RANGE_START+(u16)num;
		s_TcpPortInit = 1;
	}
  
 again:
	if (++port > TCP_LOCAL_PORT_RANGE_END) 
	{
		port = TCP_LOCAL_PORT_RANGE_START;
	}
  
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if (pcb->local_port == port) 
		{
			goto again;
		}
	}
	for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if (pcb->local_port == port) 
		{
			goto again;
		}
	}
	for(pcb = (struct tcp_pcb *)tcp_listen_pcbs.pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if (pcb->local_port == port) 
		{
			goto again;
		}
	}
	return port;
}

/**
 * Connects to another host. The function given as the "connected"
 * argument will be called when the connection has been established.
 *
 */
err_t tcp_connect(struct tcp_pcb *pcb, struct ip_addr *ipaddr, u16 port,
      err_t (* connected)(void *arg, struct tcp_pcb *tpcb, err_t err))
{
	u32 optdata;
	err_t ret;
	u32 iss;

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_connect to port %"U16_F"\n", port);
	if(pcb->state == LISTEN)
		return NET_ERR_SYS;
	else if(pcb->state == ESTABLISHED)//revised on 2017.3.27
		return NET_ERR_ISCONN;
	else if(pcb->state != CLOSED)
		return NET_ERR_EXIST;//revised on 2016.3.27
	if (ipaddr != NULL) 
	{
		pcb->remote_ip = ipaddr->addr;
	} else 
	{
		return NET_ERR_VAL;
	}
	pcb->remote_port = port;
	if(pcb->local_ip == 0)
	{
		struct route *rt = ip_route(pcb->remote_ip, pcb->out_dev);
		if(!rt)
		{
			return NET_ERR_RTE;	
		}
		pcb->local_ip = rt->out->addr;
		pcb->out_dev = rt->out;//bind the dev
	}else
	{
		struct route *rt = ip_route(pcb->remote_ip, pcb->out_dev);
		if(!rt)
		{
			return NET_ERR_RTE;
		}
		pcb->out_dev = rt->out;//bind the dev
	}
	if (pcb->local_port == 0) 
	{
		pcb->local_port = tcp_new_port();
	}
	
	pcb->send_data_ok = 0;/* 保证三次握手完整 */
	
	iss = tcp_next_iss();
	pcb->rcv_nxt = 0;
	pcb->snd_nxt = iss;
	pcb->lastack = iss - 1;
	pcb->snd_lbb = iss - 1;
	pcb->rcv_wnd = TCP_WND;
	pcb->snd_wnd = TCP_WND;
	pcb->mss = TCP_MSS;
	pcb->cwnd = 1;
	pcb->ssthresh = pcb->mss * 10;
	pcb->state = SYN_SENT;
	#if IPSTACK_CALLBACK_API  
	pcb->connected = connected;
	#endif /* IPSTACK_CALLBACK_API */  


  
	/* Build an MSS option */
	optdata = htonl(((u32)2 << 24) | 
			((u32)4 << 16) | 
			(((u32)pcb->mss / 256) << 8) |
			(pcb->mss & 255));

	ret = tcp_enqueue(pcb, NULL, 0, TCP_SYN, 0, (u8 *)&optdata, 4);
	if (ret == NET_OK) 
	{ 
		TCP_REG(&tcp_active_pcbs, pcb);
		tcp_output(pcb);
	}else
	{
		pcb->state = CLOSED;
	}
	return ret;
} 

/*
 * Called every 500 ms and implements the retransmission timer and the timer that
 * removes PCBs that have been in TIME-WAIT for enough time. It also increments
 * various timers such as the inactivity timer in each PCB.
 */
void
tcp_slowtmr(void)
{
	struct tcp_pcb *pcb, *pcb2, *prev;
	u32 eff_wnd;
	u8 pcb_remove;      /* flag if a PCB should be removed */
	u8 app_free;/* let application free PCB ? */
	err_t err;	

	++tcp_ticks;

	/* Steps through all of the active PCBs. */
	prev = NULL;
	pcb = tcp_active_pcbs;
	if (pcb == NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("tcp_slowtmr: no active pcbs\n"));
	}
	while (pcb != NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("tcp_slowtmr: processing active pcb\n"));
		assert(pcb->state != CLOSED);
		assert(pcb->state != LISTEN);
		assert(pcb->state != TIME_WAIT);

		pcb_remove = 0;
		app_free = 1;
		err = NET_OK;

		if (pcb->state == SYN_SENT && pcb->nrtx == TCP_SYNMAXRTX)
		{
			++pcb_remove;
			err = NET_ERR_TIMEOUT;
			IPSTACK_DEBUG(TCP_DEBUG, ("tcp_slowtmr: max SYN retries reached\n"));
		}
		else if (pcb->nrtx == TCP_MAXRTX) 
		{
			++pcb_remove;
			err = NET_ERR_ABRT;
			IPSTACK_DEBUG(TCP_DEBUG, ("tcp_slowtmr: max DATA retries reached\n"));
		} else 
		{
			++pcb->rtime;
			if (!skb_queue_empty(&pcb->unacked)&& pcb->rtime >= pcb->rto) 
			{

				/* Time for a retransmission. */
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: rtime %d pcb->rto %d\n", pcb->rtime, pcb->rto);

				/*
				 * Double retransmission time-out unless we are trying to
				 * connect to somebody (i.e., we are in SYN_SENT). 
				 */
				if (pcb->state != SYN_SENT) 
				{
					pcb->rto = 1000/TCP_SLOW_INTERVAL;//((pcb->sa >> 3) + pcb->sv) << tcp_backoff[pcb->nrtx];
				}
				/* Reduce congestion window and ssthresh. */
				eff_wnd = IPSTACK_MIN(pcb->cwnd, pcb->snd_wnd);
				pcb->ssthresh = eff_wnd >> 1;
				if (pcb->ssthresh < pcb->mss) 
				{
					pcb->ssthresh = pcb->mss * 2;
				}
				pcb->cwnd = pcb->mss;
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: cwnd %"U16_F" ssthresh %"U16_F"\n",
                                pcb->cwnd, pcb->ssthresh);
 
				/* The following needs to be called AFTER cwnd is set to one mss - STJ */
				tcp_rexmit_rto(pcb);
			}
		}
		/* Check if this PCB has stayed too long in FIN-WAIT-1,
		 *   FIN-WAIT-2,CLOSING,TIME_WAIT
		 */
		if (pcb->state == FIN_WAIT_1 ||
			pcb->state == FIN_WAIT_2 ||
			pcb->state == CLOSING    
		   )
		{
			if ((u32)(tcp_ticks - pcb->tmr) >
				TCP_FIN_WAIT_TIMEOUT / TCP_SLOW_INTERVAL)
			{
				++pcb_remove;
				app_free = 0;
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: removing pcb stuck in FIN-WAIT-2\n");
			}
		}

		/* Check if KEEPALIVE should be sent */
		if((pcb->so_options & SOF_KEEPALIVE) && 
		    ((pcb->state == ESTABLISHED) /*|| (pcb->state == CLOSE_WAIT)*/)
		    ) 
		{
			if(pcb->keep_cnt>TCP_KEEPCNT) 
			{
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: KEEPALIVE timeout. Aborting connection to %d.%d.%d.%d.\n",
							ip4_addr1(pcb->remote_ip), ip4_addr2(pcb->remote_ip),
							ip4_addr3(pcb->remote_ip), ip4_addr4(pcb->remote_ip));
				TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, NET_ERR_TIMEOUT);
				++pcb_remove;
				pcb->err = NET_ERR_TIMEOUT;
				/*tcp_abort(pcb);*/
			}
			else if((u32)(tcp_ticks - pcb->tmr) > (pcb->keepalive + pcb->keep_cnt * TCP_KEEPINTVL) / TCP_SLOW_INTERVAL)
			{
				tcp_keepalive(pcb);
				pcb->keep_cnt++;
			}
		}

		/* 
		 * If this PCB has queued out of sequence data, but has been
		 * inactive for too long, will drop the data (it will eventually
		 * be retransmitted). 
		 */
		#if TCP_QUEUE_OOSEQ    
		if (!skb_queue_empty(&pcb->ooseq)&&
			(u32)tcp_ticks - pcb->tmr >= pcb->rto * TCP_OOSEQ_TIMEOUT) 
		{
			__skb_queue_purge(&pcb->ooseq);
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: dropping OOSEQ queued data\n");
		}
		#endif /* TCP_QUEUE_OOSEQ */

		/* Check if this PCB has stayed too long in SYN-RCVD */
		if (pcb->state == SYN_RCVD) 
		{
			if ((u32)(tcp_ticks - pcb->tmr) >
				TCP_SYN_RCVD_TIMEOUT / TCP_SLOW_INTERVAL) 
			{
				++pcb_remove;
				app_free = 0;
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: removing pcb stuck in SYN-RCVD\n");
			}
		}

		/* Check if this PCB has stayed too long in LAST-ACK */
		if (pcb->state == LAST_ACK) 
		{
			if ((u32)(tcp_ticks - pcb->tmr) > 2 * TCP_MSL / TCP_SLOW_INTERVAL) 
			{
				++pcb_remove;
				app_free = 0;
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: removing pcb stuck in LAST-ACK\n");
			}
		}

		/* If the PCB should be removed, do it. */
		if (pcb_remove) 
		{
			tcp_pcb_purge(pcb);      
			/* Remove PCB from tcp_active_pcbs list. */
			if (prev != NULL)
			{
				assert(pcb != tcp_active_pcbs);
				prev->next = pcb->next;
			} else
			{
				/* This PCB was the first. */
				assert(tcp_active_pcbs == pcb);
				tcp_active_pcbs = pcb->next;
			}

			pcb->state = CLOSED;
			if(err != NET_OK)
			{
				pcb->err = err;
				TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, err);
			}

			pcb2 = pcb->next;
			if(!app_free)
			{
				tcp_pcb_free(pcb);
			}
			pcb = pcb2;
		} else 
		{

			/* We check if we should poll the connection. */
			++pcb->polltmr;
			if (pcb->polltmr >= pcb->pollinterval) 
			{
				pcb->polltmr = 0;
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: polling application\n");
				TCP_EVENT_POLL(pcb, err);
				if (err == NET_OK) 
				{
					tcp_output(pcb);
				}
			}
      
			prev = pcb;
			pcb = pcb->next;
		}
	}

  
	/* Steps through all of the TIME-WAIT PCBs. */
	prev = NULL;    
	pcb = tcp_tw_pcbs;
	if (pcb == NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_slowtmr: no TIME-WAIT pcbs\n");
	}
	while (pcb != NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "Process TIME-WAIT pcb\n");
		assert(pcb->state == TIME_WAIT);
		pcb_remove = 0;

		/* Check if this PCB has stayed long enough in TIME-WAIT */
		IPSTACK_DEBUG(TCP_DEBUG, "%d vs %d\n",
			tcp_ticks - pcb->tmr,
			2 * TCP_MSL / TCP_SLOW_INTERVAL
			);
		if ((u32)(tcp_ticks - pcb->tmr) > 2 * TCP_MSL / TCP_SLOW_INTERVAL) 
		{ 
			++pcb_remove;
		}

		/* If the PCB should be removed, do it. */
		if (pcb_remove) 
		{
			tcp_pcb_purge(pcb);      
			/* Remove PCB from tcp_tw_pcbs list. */
			if (prev != NULL) 
			{
				assert(pcb != tcp_tw_pcbs);
				prev->next = pcb->next;
			} else 
			{
				/* This PCB was the first. */
				assert(tcp_tw_pcbs == pcb);
				tcp_tw_pcbs = pcb->next;
			}
			pcb2 = pcb->next;
			tcp_pcb_free(pcb);
			pcb = pcb2;
		} else 
		{
			prev = pcb;
			pcb = pcb->next;
		}
	}
}

/*
 * Is called every TCP_FAST_INTERVAL (250 ms) and sends delayed ACKs.
 */
void
tcp_fasttmr(void)
{
	struct tcp_pcb *pcb;

	/* send delayed ACKs */  
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if ((pcb->flags & TF_ACK_DELAY)) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_fasttmr: delayed ACK\n");
			tcp_ack_now(pcb);
			pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
		}
	}
}

/*
 * Reclaim skb used by tcp conn
 */
int tcp_reclaim_skb(void)
{
	struct tcp_pcb *pcb;
	int num = 0;

	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		#if TCP_QUEUE_OOSEQ
		if(!skb_queue_empty(&pcb->ooseq))
		{
			num += pcb->ooseq.qlen;
			__skb_queue_purge(&pcb->ooseq);
		}
		#endif
	}
	return num;
}

/*
 * Sets the priority of a connection.
 *
 */
void
tcp_setprio(struct tcp_pcb *pcb, u8 prio)
{
	pcb->prio = prio;
}

int tcp_check_event(struct tcp_pcb *pcb, u32 mask)
{
	if(pcb->state != ESTABLISHED)
		return NET_ERR_CONN;
	if(pcb->event_mask & mask)
		return 1;
	return 0;
}


#if IPSTACK_CALLBACK_API
static err_t tcp_recv_null(void *arg, struct tcp_pcb *pcb, struct sk_buff *p, err_t err)
{
	pcb->event_mask |= TCP_CAN_RCV;
	return NET_OK;
}
err_t tcp_sent_null(void *arg, struct tcp_pcb *pcb, u16 space)
{
	pcb->event_mask |= TCP_CAN_SND;
	IPSTACK_DEBUG(TCP_DEBUG, "TCP Send EVENT %08x\n", pcb->event_mask);
	return NET_OK;
}

#endif /* IPSTACK_CALLBACK_API */

static void tcp_kill_prio(u8 prio)
{
	struct tcp_pcb *pcb, *inactive;
	u32 inactivity;
	u8 mprio;


	mprio = TCP_PRIO_MAX;
  
	/* 
	 * We kill the oldest active connection that has lower priority than prio. 
	 */
	inactivity = 0;
	inactive = NULL;
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if (pcb->prio <= prio &&
			pcb->prio <= mprio &&
			(u32)(tcp_ticks - pcb->tmr) >= inactivity) 
		{
			inactivity = tcp_ticks - pcb->tmr;
			inactive = pcb;
			mprio = pcb->prio;
		}
	}
	if (inactive != NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_kill_prio: killing oldest PCB %p (%d)\n",(void *)inactive, inactivity);
		tcp_abort(inactive);
	}      
}

static void tcp_kill_timewait(void)
{
	struct tcp_pcb *pcb, *inactive;
	u32 inactivity;

	inactivity = 0;
	inactive = NULL;
	for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if ((u32)(tcp_ticks - pcb->tmr) >= inactivity) 
		{
			inactivity = tcp_ticks - pcb->tmr;
			inactive = pcb;
		}
	}
	if (inactive != NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_kill_timewait: killing oldest TIME-WAIT PCB %p (%d)\n",(void *)inactive, inactivity);
		tcp_abort(inactive);
		return;
	}

	inactivity = 0;
	inactive = NULL;
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next)
	{
		if((pcb->state==FIN_WAIT_1||
			pcb->state == FIN_WAIT_2||
			pcb->state == CLOSING||
			pcb->state == LAST_ACK
			)&&
			(u32)(tcp_ticks - pcb->tmr) >= inactivity
			)
		{
			inactivity = tcp_ticks - pcb->tmr;
			inactive = pcb;
		}
	}
	if (inactive != NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_kill_timewait: killing oldest state=%d PCB %p (%d)\n",
					inactive->state,
					(void *)inactive, 
					inactivity);
		tcp_abort(inactive);
		return;
	}
}

extern int mem_assert;
struct tcp_pcb *tcp_alloc(u8 prio)
{
	struct tcp_pcb *pcb;
	u32 iss;
	int old_mem_assert = mem_assert;

  	mem_assert = 0;
  	
	pcb = tcp_pcb_malloc();
	if (pcb == NULL) 
	{
		/* Try killing oldest connection in TIME-WAIT. */
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_alloc: killing off oldest TIME-WAIT connection\n");
		tcp_kill_timewait();
		pcb = tcp_pcb_malloc();
		#if 0
		if (pcb == NULL) 
		{
			tcp_kill_prio(prio);    
			pcb = tcp_pcb_malloc();
		}
		#endif
	}
	if (pcb != NULL) 
	{
		memset(pcb, 0, sizeof(struct tcp_pcb));
		pcb->prio = TCP_PRIO_NORMAL;
		pcb->snd_buf = TCP_SND_BUF;
		pcb->snd_queuelen = 0;
		pcb->rcv_wnd = TCP_WND;
		pcb->tos = 0;
		pcb->ttl = TCP_TTL;
		pcb->mss = TCP_MSS;
		pcb->rto = 3000 / TCP_SLOW_INTERVAL;
		pcb->sa = 0;
		pcb->sv = 3000 / TCP_SLOW_INTERVAL;
		pcb->rtime = 0;
		pcb->cwnd = 1;
		iss = tcp_next_iss();
		pcb->snd_wl2 = iss;
		pcb->snd_nxt = iss;
		pcb->snd_max = iss;
		pcb->lastack = iss;
		pcb->snd_lbb = iss;   
		pcb->tmr = tcp_ticks;
		pcb->so_options = SOF_KEEPALIVE;

		skb_queue_head_init(&pcb->incoming);
		skb_queue_head_init(&pcb->unsent);
		skb_queue_head_init(&pcb->unacked);
		#if TCP_QUEUE_OOSEQ
		skb_queue_head_init(&pcb->ooseq);
		#endif

		pcb->polltmr = 0;

		#if IPSTACK_CALLBACK_API
		pcb->recv = tcp_recv_null;
		pcb->sent = tcp_sent_null;
		#endif /* IPSTACK_CALLBACK_API */  
    
		/* Init KEEPALIVE timer */
		pcb->keepalive = TCP_KEEPDEFAULT;
		pcb->keep_cnt = 0;

		pcb->send_data_ok = 1;
	}

	mem_assert = old_mem_assert;
	return pcb;
}

/*
 * Creates a new TCP protocol control block but doesn't place it on
 * any of the TCP PCB lists.
 *
 * @internal: Maybe there should be a idle TCP PCB list where these
 * PCBs are put on. We can then implement port reservation using
 * tcp_bind(). Currently, we lack this (BSD NetSocket type of) feature.
 */
struct tcp_pcb *tcp_new(void)
{
	return tcp_alloc(TCP_PRIO_NORMAL);
}

/*
 * tcp_arg():
 *
 * Used to specify the argument that should be passed callback
 * functions.
 *
 */ 
void tcp_arg(struct tcp_pcb *pcb, void *arg)
{  
	pcb->callback_arg = arg;
}
#if IPSTACK_CALLBACK_API

/*
 * Used to specify the function that should be called when a TCP
 * connection receives data.
 *
 */ 
void
tcp_recv(struct tcp_pcb *pcb,
   err_t (* recv)(void *arg, struct tcp_pcb *tpcb, struct sk_buff *p, err_t err))
{
	pcb->recv = recv;
}

/*
 * Used to specify the function that should be called when TCP data
 * has been successfully delivered to the remote host.
 *
 */ 

void
tcp_sent(struct tcp_pcb *pcb,
   err_t (* sent)(void *arg, struct tcp_pcb *tpcb, u16 len))
{
	pcb->sent = sent;
}

/*
 * Used to specify the function that should be called when a fatal error
 * has occured on the connection.
 *
 */ 
void
tcp_err(struct tcp_pcb *pcb,
   void (* errf)(void *arg, err_t err))
{
	pcb->errf = errf;
}

/**
 * Used for specifying the function that should be called when a
 * LISTENing connection has been connected to another host.
 *
 */ 
void
tcp_accept(struct tcp_pcb *pcb,
     err_t (* accept)(void *arg, struct tcp_pcb *newpcb, err_t err))
{
  ((struct tcp_pcb_listen *)pcb)->accept = accept;
}
#endif /* IPSTACK_CALLBACK_API */


/**
 * Used to specify the function that should be called periodically
 * from TCP. The interval is specified in terms of the TCP coarse
 * timer interval, which is called twice a second.
 *
 */ 
void
tcp_poll(struct tcp_pcb *pcb,
   err_t (* poll)(void *arg, struct tcp_pcb *tpcb), u8 interval)
{
#if IPSTACK_CALLBACK_API
  pcb->poll = poll;
#endif /* IPSTACK_CALLBACK_API */  
  pcb->pollinterval = interval;
}

/**
 * Purges a TCP PCB. Removes any buffered data and frees the buffer memory.
 *
 */
void
tcp_pcb_purge(struct tcp_pcb *pcb)
{
	if (pcb->state != CLOSED &&
		pcb->state != TIME_WAIT &&
		pcb->state != LISTEN)
	{

		IPSTACK_DEBUG(TCP_DEBUG, ("tcp_pcb_purge\n"));
		_tcp_pcb_purge(pcb);
	}
}

/*
 * Purges the PCB and removes it from a PCB list. Any delayed ACKs are sent first.
 *
 */
void
tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb)
{
	TCP_RMV(pcblist, pcb);

	tcp_pcb_purge(pcb);
  
	/* if there is an outstanding delayed ACKs, send it */
	if (pcb->state != TIME_WAIT &&
		pcb->state != LISTEN &&
		pcb->flags & TF_ACK_DELAY) 
	{
		pcb->flags |= TF_ACK_NOW;
		tcp_output(pcb);
	}  
	pcb->state = CLOSED;

	assert(tcp_pcbs_sane());
}

/*
 * Calculates a new initial sequence number for new connections.
 *
 */
u32
tcp_next_iss(void)
{
	static u32 iss = 6510;
  
	iss += inet_jiffier;//tcp_ticks;       /* XXX */
	return iss;
	//return 0x196f;//for debug
}

int tcp_avail_snd_buf(struct tcp_pcb *pcb)
{
	/* Get Avail Sending Buf */
	int size = pcb->snd_buf>0?pcb->snd_buf:0;
	if((pcb->unacked.qlen+pcb->unsent.qlen)>=TCP_SND_QUEUELEN)
		size = 0;
	return size;
}

/* 判断是否存在从指定接口出去的连接 */
int tcp_conn_exist(INET_DEV_T *dev)
{
	struct tcp_pcb *pcb = tcp_active_pcbs;
	while(pcb)
	{
		if(pcb->out_dev == dev)
			return 1;
		pcb = pcb->next;
	}
	return 0;
}

void tcp_link_change_notify(INET_DEV_T *dev, u32 old_ip, int err_code)
{
	struct tcp_pcb *pcb, *prev;
	s80_printf("%s Link Change err=%d\n", dev->name, err_code);
	prev = NULL;
	pcb = tcp_active_pcbs;
	while(pcb != NULL)
	{
		if(pcb->out_dev == dev ||
			pcb->local_ip == old_ip)
		{
			tcp_reset(pcb);
			if(pcb->state == ESTABLISHED||
				pcb->state == SYN_SENT
				)
			{
				pcb->state = CLOSE_WAIT;
				pcb->err = err_code;
			}
		}
		pcb = pcb->next;
	}
}

#if TCP_DEBUG
void
tcp_debug_print(struct tcphdr *th)
{
	IPSTACK_DEBUG(TCP_DEBUG, "TCP header:\n");
	IPSTACK_DEBUG(TCP_DEBUG, "+-------------------------------+\n");
	IPSTACK_DEBUG(TCP_DEBUG, "|    %5u      |    %5u      | (src port, dest port)\n",
			ntohs(th->source), ntohs(th->dest));
	IPSTACK_DEBUG(TCP_DEBUG, "+-------------------------------+\n");
	IPSTACK_DEBUG(TCP_DEBUG, "|           %010u          | (seq no)\n",
			ntohl(th->seq));
	IPSTACK_DEBUG(TCP_DEBUG, "+-------------------------------+\n");
	IPSTACK_DEBUG(TCP_DEBUG, "|           %010u          | (ack no)\n",
			ntohl(th->ack_seq));
	IPSTACK_DEBUG(TCP_DEBUG, "+-------------------------------+\n");
	IPSTACK_DEBUG(TCP_DEBUG, "| %2d |   |%d%d%d%d%d%d|     %5d     | (hdrlen, flags (",
			th->doff,
			th->urg,
			th->ack,
			th->psh,
			th->rst,
			th->syn,
			th->fin,
			ntohs(th->window));
	{
		if(th->ack)
			IPSTACK_DEBUG(TCP_DEBUG, "ACK ");
		if(th->rst)
			IPSTACK_DEBUG(TCP_DEBUG, "RST ");
		if(th->syn)
			IPSTACK_DEBUG(TCP_DEBUG, "SYN ");
		if(th->fin)
			IPSTACK_DEBUG(TCP_DEBUG, "FIN ");
	}
	
	IPSTACK_DEBUG(TCP_DEBUG, "), win)\n");
	IPSTACK_DEBUG(TCP_DEBUG, "+-------------------------------+\n");
	IPSTACK_DEBUG(TCP_DEBUG, "|    0x%04x     |     %5d     | (chksum, urgp)\n",
			ntohs(th->check), ntohs(th->urg_ptr));
	IPSTACK_DEBUG(TCP_DEBUG, "+-------------------------------+\n");
}

void
tcp_debug_print_state(enum tcp_state s)
{
	s80_printf("State: ");
	switch (s) 
	{
	case CLOSED:
		s80_printf("CLOSED\n");
		break;
	case LISTEN:
		s80_printf("LISTEN\n");
		break;
	case SYN_SENT:
		s80_printf("SYN_SENT\n");
		break;
	case SYN_RCVD:
		s80_printf("SYN_RCVD\n");
		break;
	case ESTABLISHED:
		s80_printf("ESTABLISHED\n");
		break;
	case FIN_WAIT_1:
		s80_printf("FIN_WAIT_1\n");
		break;
	case FIN_WAIT_2:
		s80_printf("FIN_WAIT_2\n");
		break;
	case CLOSE_WAIT:
		s80_printf("CLOSE_WAIT\n");
		break;
	case CLOSING:
		s80_printf("CLOSING\n");
		break;
	case LAST_ACK:
		s80_printf("LAST_ACK\n");
		break;
	case TIME_WAIT:
		s80_printf("TIME_WAIT\n");
		break;
	}
}

void tcp_debug_print_flags(u16 flags)
{
	if (flags & TCP_FIN)
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("FIN "));
	}
	if (flags & TCP_SYN) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("SYN "));
	}
	if (flags & TCP_RST) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("RST "));
	}
	if (flags & TCP_PSH) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("PSH "));
	}
	if (flags & TCP_ACK) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("ACK "));
	}
	if (flags & TCP_URG) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("URG "));
	}
	if (flags & TCP_ECE) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("ECE "));
	}
	if (flags & TCP_CWR) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, ("CWR "));
	}
}

void tcp_debug_print_pcbs(void)
{
	struct tcp_pcb *pcb;
	s80_printf("Active PCB states:\n");
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		s80_printf("%08x: Local port %d, foreign port %d \n\tsnd_nxt %u rcv_nxt %u mss %d ",
					pcb,
					(u32)pcb->local_port, (u32)pcb->remote_port,
					pcb->snd_nxt, pcb->rcv_nxt,
					pcb->mss
					);
		tcp_debug_print_state(pcb->state);
		s80_printf("\tsnd_win %d,%d\n",
					(u32)pcb->snd_wnd,
					(u32)pcb->cwnd
					);
		s80_printf("\trcv_win %u, %u, %u\n",
					(u32)pcb->rcv_wnd,
					(u32)pcb->snd_wnd,
					(u32)pcb->snd_wl1,
					(u32)pcb->snd_wl2
					);
		s80_printf("\tqlen in %d, unsent %d, unack %d, oos %d\n",
					pcb->incoming.qlen,
					pcb->unsent.qlen,
					pcb->unacked.qlen,
					#if TCP_QUEUE_OOSEQ
					pcb->ooseq.qlen
					#else
					0
					#endif
					);
	}    
	s80_printf("Listen PCB states:\n");
	for(pcb = (struct tcp_pcb *)tcp_listen_pcbs.pcbs; pcb != NULL; pcb = pcb->next) 
	{
		s80_printf("\t%08x Local port %d ",
					pcb,
					pcb->local_port
					);
		tcp_debug_print_state(pcb->state);
		s80_printf("\n");
	}    
	s80_printf("TIME-WAIT PCB states:\n");
	for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		s80_printf("\t%08x Local port %d, foreign port %d snd_nxt %d rcv_nxt %d ",
					pcb,
					pcb->local_port, pcb->remote_port,
					pcb->snd_nxt, pcb->rcv_nxt);
		tcp_debug_print_state(pcb->state);
		s80_printf(" incoming qlen %d, %d, %d\n",
					pcb->incoming.qlen,
					pcb->unsent.qlen,
					pcb->unacked.qlen
					);
	}    
}

s16 tcp_pcbs_sane(void)
{
	struct tcp_pcb *pcb;
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		assert(pcb->state != CLOSED);
		assert(pcb->state != LISTEN);
		assert(pcb->state != TIME_WAIT);
	}
	for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		assert(pcb->state == TIME_WAIT);
	}
	return 1;
}
#endif

/*
tcp_find_pcb: 在pcb list查找特定的值,
为P60-S1增加的特殊接口;
p: 用户关心的pcb pointer
返回值:0,找到;其他值,没找到;
*/
int tcp_find_pcb(void *p)
{
	struct tcp_pcb *pcb;
	int ret=NET_ERR_ARG;
	inet_softirq_disable();
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if(pcb == p)
		{
			ret = 0;
			goto out;
		}
		
	}
	for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if(pcb == p)
		{
			ret = 0;
			goto out;
		}
	}
out:
	inet_softirq_enable();
	return ret;
}

