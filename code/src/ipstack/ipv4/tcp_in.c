/*
**  tcp receive packet processing
**  Author:sunJH
**  Date:2007-07-01
修改历史:
20080605
1.增加的可疑报文过滤
080624 sunJH
在更新发送窗口时，发现对方提供的窗口小于TCP_MSS，
则窗口采用TCP_MSS，避免不能发送报文
080716 sunJH
1.当收到数据后，在tcp_input中pcb->send_data_ok=1表示真正完成三次握手
2.tcp_process和tcp_receive中，
pcb->snd_wnd < TCP_MSS，应该设置为TCP_MSS；该方法有缺陷；
应该加强判断： pcb->snd_wnd < TCP_MSS&&pcb->snd_wnd>0。
090803 sunJH
tcp_input在查找pcb之前就丢弃可疑报文
110420 xuml
tcp_process处理中，处于SYN_SENT时不对收到的ACK包发送复位应答。
**/
#include <inet/inet.h>
#include <inet/skbuff.h>
#include <inet/ip.h>
#include <inet/tcp.h>
#include <inet/dev.h>

static u32 seqno, ackno;/* from tcphdr, in host byte order */
static u8 flags;/* from tcphdr, in host byte order */
static u16 tcplen;/* the tcp payload, include SYN,FIN */

static u8 recv_flags;

struct tcp_pcb *tcp_input_pcb;
unsigned long dup_packet_num;

/* Forward declarations. */
static err_t tcp_process(struct tcp_pcb *pcb, struct sk_buff *skb);
static u8 tcp_receive(struct tcp_pcb *pcb, struct sk_buff *skb);
static void tcp_parseopt(struct tcp_pcb *pcb, struct sk_buff *skb);

static err_t tcp_listen_input(struct tcp_pcb_listen *pcb, struct sk_buff *skb);
static err_t tcp_timewait_input(struct tcp_pcb *pcb, struct sk_buff *skb);

/* 
 * tcp_input:
 *
 * The initial input processing of TCP. It verifies the TCP header, demultiplexes
 * the segment between the PCBs and passes it on to tcp_process(), which implements
 * the TCP finite state machine. This function is called by the IP layer (in
 * ip_input()).
 */

void tcp_input(struct sk_buff *skb)
{
	struct tcp_pcb *pcb, *prev;
	struct tcp_pcb_listen *lpcb;
	struct tcphdr *th = skb->h.th;
	struct iphdr  *iph = skb->nh.iph;
	err_t err;

	g_tcp_stats.recv_pkt++;

	#if TCP_DEBUG
	tcp_debug_print(th);
	#endif

	if(skb->len<20||__skb_may_pull(skb, th->doff*4)==NULL)
	{
		skb_free(skb);
		g_tcp_stats.drop++;
		return;
	}

	/* Don't even process incoming broadcasts/multicasts. */
	if (ip_addr_isbroadcast(iph->daddr, skb->dev) ||
		ip_addr_ismulticast(iph->daddr))
	{
		g_tcp_stats.drop++;
		skb_free(skb);
		return;
	}

	#if IPSTACK_IN_CHECK==1
	if (inet_chksum_pseudo(th, iph->tot_len-iph->ihl*4,
			iph->saddr,
			iph->daddr,
			iph->protocol) != 0) 
	{
		skb_free(skb);
		g_tcp_stats.drop++;
		return;
	}
	#endif

	/* 
	 * Move the payload pointer in the pbuf so that it points to the
	 * TCP data instead of the TCP header. 
	 */

	/* Convert fields in TCP header to host byte order. */
	th->source = ntohs(th->source);
	th->dest = ntohs(th->dest);
	seqno = th->seq = ntohl(th->seq);
	ackno = th->ack_seq = ntohl(th->ack_seq);
	th->window = ntohs(th->window);
	flags = TCPH_FLAGS(th);

	tcplen = skb->len+th->fin+th->syn;

	IPSTACK_DEBUG(TCP_DEBUG, "data len %d\n", tcplen);

	if(((flags&TCP_SYN)&&(flags&TCP_FIN))||
	   ((flags&TCP_FIN)&&!(flags&TCP_ACK))
	   )
	{
		skb_free(skb);
		g_tcp_stats.drop++;
		return ;
	}
	/* 
	 * Demultiplex an incoming segment. First, we check if it is destined
	 * for an active connection. 
	 */
	prev = NULL;  
	for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) 
	{
		if(pcb->state == CLOSED)
		{
			continue;
		}
		assert(pcb->state != TIME_WAIT);
		assert(pcb->state != LISTEN);
		if (pcb->remote_port == th->source &&
			pcb->local_port == th->dest &&
			ip_addr_cmp(pcb->remote_ip, iph->saddr) &&
			ip_addr_cmp(pcb->local_ip, iph->daddr)) 
		{

			/* 
			 * Move this PCB to the front of the list so that subsequent
			* lookups will be faster (we exploit locality in TCP segment
			* arrivals). 
			*/
			assert(pcb->next != pcb);
			if (prev != NULL) 
			{
				prev->next = pcb->next;
				pcb->next = tcp_active_pcbs;
				tcp_active_pcbs = pcb;
			}
			assert(pcb->next != pcb);
			break;
		}
		prev = pcb;
	}

	if (pcb == NULL) 
	{
		/* 
		 * If it did not go to an active connection, we check the connections
		 * in the TIME-WAIT state. 
		 */
		for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next)
		{
			assert(pcb->state == TIME_WAIT);
			if (pcb->remote_port == th->source &&
				pcb->local_port == th->dest &&
				ip_addr_cmp(pcb->remote_ip, iph->saddr) &&
				ip_addr_cmp(pcb->local_ip, iph->daddr)) 
			{
				/* We don't really care enough to move this PCB to the front
				 * of the list since we are not very likely to receive that
				 * many segments for connections in TIME-WAIT. 
				 */
				IPSTACK_DEBUG(TCP_DEBUG, ("tcp_input: packed for TIME_WAITing connection.\n"));
				tcp_timewait_input(pcb, skb);
				skb_free(skb);
				return;
			}
		}

		/* 
		 * Finally, if we still did not get a match, we check all PCBs that
		 * are LISTENing for incoming connections. 
		 */
		prev = NULL;
		for(lpcb = tcp_listen_pcbs.listen_pcbs; lpcb != NULL; lpcb = lpcb->next) 
		{
			if ((ip_addr_isany(lpcb->local_ip) ||
				ip_addr_cmp(lpcb->local_ip, iph->daddr)) &&
				lpcb->local_port == th->dest) 
			{
      
				IPSTACK_DEBUG(TCP_DEBUG, ("tcp_input: packed for LISTENing connection.\n"));
				tcp_listen_input(lpcb, skb);
				skb_free(skb);
				return;
			}
			prev = (struct tcp_pcb *)lpcb;
		}
	}

	#if TCP_INPUT_DEBUG
	IPSTACK_DEBUG(TCP_DEBUG, ("+-+-+-+-+-+-+-+-+-+-+-+-+-+- tcp_input: flags "));
	tcp_debug_print_flags(TCPH_FLAGS(th));
	IPSTACK_DEBUG(TCP_DEBUG, ("-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"));
	#endif /* TCP_INPUT_DEBUG */


	if (pcb != NULL)
	{
		/* The incoming segment belongs to a connection. */
		#if TCP_DEBUG
		tcp_debug_print_state(pcb->state);
		#endif /* TCP_DEBUG */
		recv_flags = 0;
		tcp_input_pcb = pcb;
		err = tcp_process(pcb, skb);
		tcp_input_pcb = NULL;
		if (err != NET_ERR_ABRT) 
		{
			if (recv_flags == TF_RESET) 
			{
				TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, NET_ERR_RST);
				TCP_RMV(&tcp_active_pcbs, pcb);
				pcb->err = NET_ERR_RST;
				switch(pcb->state)
				{
				case ESTABLISHED:
				case SYN_SENT:
				case CLOSE_WAIT:
					pcb->state = CLOSED;
					break;
				default:
					tcp_pcb_free(pcb);
				}
				//tcp_pcb_free(pcb);
			} else if (recv_flags == TF_CLOSED) 
			{
				tcp_pcb_remove(&tcp_active_pcbs, pcb);
				tcp_pcb_free(pcb);
            } else 
            {
				err = NET_OK;
				pcb->event_mask |= TCP_PEER_OPENED;
				/* 
				 * If the application has registered a "sent" function to be
				* called when new send buffer space is available, we call it
				* now.
				*/
				if (pcb->acked > 0) 
				{
					pcb->send_data_ok = 1;
					TCP_EVENT_SENT(pcb, pcb->acked, err);
				}

				#if 1      
				if (pcb->incoming.qlen>0) 
				{
					/* Notify application that data has been received. */
					TCP_EVENT_RECV(pcb, NULL, NET_OK, err);
				}
				#endif

				#if 0
				/* 下面的代码已无效了 */
				/*
				 * If a FIN segment was received, we call the callback
				 * function with a NULL buffer to indicate EOF. 
				 */
				if (recv_flags & TF_GOT_FIN) 
				{
					TCP_EVENT_RECV(pcb, NULL, NET_OK, err);
					/*
					** 由于设置pcb->err会导致tcp_close直接
					** 关闭pcb而不会发送fin,因此不符合
					** rfc;但是pcb->err为0又不会产生错误通知
					** 用户!!!!!!!!!
					** 目前采用的方式:
					** 在socket中判断pcb状态,如果state=close_wait,
					** 则强制通知用户错误事件;
					**/
					//pcb->err = NET_ERR_CLSD;
				}
				#endif
				/* If there were no errors, we try to send something out. */
				if (err == NET_OK) 
				{
					tcp_output(pcb);
				}
			}
		}

		#if TCP_DEBUG
		tcp_debug_print_state(pcb->state);
		#endif /* TCP_DEBUG */
      
	} else 
	{

		/*
		 * If no matching PCB was found, send a TCP RST (reset) to the
		 * sender. 
		 */
		IPSTACK_DEBUG(TCP_DEBUG, ("tcp_input: no PCB match found, resetting.\n"));
		if (!(TCPH_FLAGS(th) & TCP_RST)) 
		{
			tcp_rst(ackno, seqno + tcplen,
				iph->daddr, iph->saddr,
				th->dest, th->source);
		}
		g_tcp_stats.drop++;
		skb_free(skb);
	}

	assert(tcp_pcbs_sane());
}

/* 
 * tcp_listen_input():
 *
 * Called by tcp_input() when a segment arrives for a listening
 * connection.
 */
static err_t tcp_listen_input(struct tcp_pcb_listen *pcb,struct sk_buff *skb)
{
	struct tcp_pcb *npcb;
	struct tcphdr *th = skb->h.th;
	struct iphdr *iph = skb->nh.iph;
	err_t err;
	u32 optdata;

	/* 
	 * In the LISTEN state, we check for incoming SYN segments,
	 * creates a new PCB, and responds with a SYN|ACK. 
	 */
	if (flags & (TCP_ACK|TCP_FIN)) 
	{
		/* For incoming segments with the ACK or FIN flag set, respond with a
		 * RST. 
		 */
		IPSTACK_DEBUG(TCP_DEBUG, ("tcp_listen_input: ACK in LISTEN, sending reset\n"));
		tcp_rst(ackno + 1, seqno + tcplen,
			iph->daddr, iph->saddr,
			th->dest, th->source);
	} else if (flags & TCP_SYN)
	{
		IPSTACK_DEBUG(TCP_DEBUG, "TCP connection request %d -> %d.\n", th->source, th->dest);
		npcb = tcp_alloc(pcb->prio);
		/* 
		 * If a new PCB could not be created (probably due to lack of memory),
		 * we don't do anything, but rely on the sender will retransmit the
		 * SYN at a time when we have more memory available. 
		 */
		if (npcb == NULL)
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_listen_input: could not allocate PCB\n");
			return NET_ERR_MEM;
		}
		/* Set up the new PCB. */
		npcb->local_ip = iph->daddr;
		npcb->local_port = pcb->local_port;
		npcb->remote_ip = iph->saddr;
		npcb->remote_port = th->source;
		npcb->state = SYN_RCVD;
		npcb->rcv_nxt = seqno + 1;
		npcb->snd_wnd = th->window;
		npcb->ssthresh = npcb->snd_wnd;
		npcb->snd_wl1 = seqno - 1;/* initialise to seqno-1 to force window update */
		npcb->callback_arg = pcb->callback_arg;
		#if IPSTACK_CALLBACK_API
		npcb->accept = pcb->accept;
		#endif /* IPSTACK_CALLBACK_API */
		/* inherit NetSocket options */
		npcb->so_options = pcb->so_options & (SOF_DEBUG|SOF_DONTROUTE|SOF_KEEPALIVE|SOF_OOBINLINE|SOF_LINGER);
		npcb->out_dev = skb->dev;//连接需要绑定dev,从而确保回去的报文路径一致

		/* Parse any options in the SYN. */
		tcp_parseopt(npcb, skb);

		/* Build an MSS option. */
		optdata = htonl(((u32)2 << 24) |
			((u32)4 << 16) |
			(((u32)npcb->mss / 256) << 8) |
			(npcb->mss & 255));
		/* Send a SYN|ACK together with the MSS option. */
		err = tcp_enqueue(npcb, NULL, 0, TCP_SYN | TCP_ACK, 0, 
				(u8 *)&optdata, 4);
		if(err != NET_OK)
		{
			tcp_pcb_free(npcb);
			return err;
		}
		/* 
		 * Register the new PCB so that we can begin receiving segments
		 * for it. 
		 */
		TCP_REG(&tcp_active_pcbs, npcb);
		return tcp_output(npcb);
	}
	return NET_OK;
}

/* 
 * tcp_timewait_input():
 *
 * Called by tcp_input() when a segment arrives for a connection in
 * TIME_WAIT.
 */
static err_t tcp_timewait_input(struct tcp_pcb *pcb, struct sk_buff *skb)
{
	if (TCP_SEQ_GT(seqno + tcplen, pcb->rcv_nxt)) 
	{
		pcb->rcv_nxt = seqno + tcplen;
	}
	if (tcplen > 0) 
	{
		tcp_ack_now(pcb);
	}
	return tcp_output(pcb);
}

/* 
 * tcp_process
 *
 * Implements the TCP state machine. Called by tcp_input. In some
 * states tcp_receive() is called to receive data. 
 */
static err_t tcp_process(struct tcp_pcb *pcb, struct sk_buff *skb)
{
	u8 acceptable = 0;
	err_t err;
	u8 accepted_inseq;
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *th = skb->h.th;

	err = NET_OK;

	/* Process suspicious packet*/
	if(((flags&TCP_SYN)&&(flags&TCP_FIN))||
	   ((flags&TCP_FIN)&&!(flags&TCP_ACK))
	   )
	{
		skb_free(skb);
		return err;
	}

	/* Process incoming RST segments. */
	if (flags & TCP_RST) 
	{
		/* First, determine if the reset is acceptable. */
		if (pcb->state == SYN_SENT) 
		{
			if (ackno == pcb->snd_nxt) 
			{
				acceptable = 1;
			}
		} else 
		{
			if (TCP_SEQ_BETWEEN(seqno, pcb->rcv_nxt, pcb->rcv_nxt+pcb->rcv_wnd)) 
			{
				acceptable = 1;
			}
		}

		if (acceptable) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_process: Connection RESET\n");
			recv_flags = TF_RESET;
			pcb->flags &= ~TF_ACK_DELAY;
			skb_free(skb);
			return NET_ERR_RST;
		} else 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_process: unacceptable reset seqno %d rcv_nxt %d\n",
				seqno, pcb->rcv_nxt);
			skb_free(skb);
			return NET_OK;
		}
	}

	/* Update the PCB (in)activity timer. */
	pcb->tmr = tcp_ticks;
	pcb->keep_cnt = 0;

	/* Do different things depending on the TCP state. */
	switch (pcb->state) 
	{
	case SYN_SENT:
		/* received SYN ACK with expected sequence number? */
		if ((flags & TCP_ACK) && (flags & TCP_SYN)
			&& ackno == pcb->snd_nxt) 
		{
			struct sk_buff *t_skb;
			pcb->snd_buf++;
			pcb->rcv_nxt = seqno + 1;
			pcb->lastack = ackno;
			pcb->snd_wnd = th->window;
			if(pcb->snd_wnd < TCP_MSS&&pcb->snd_wnd>0)
				pcb->snd_wnd = TCP_MSS;
			pcb->snd_wl1 = seqno - 1; /* initialise to seqno - 1 to force window update */
			pcb->state = ESTABLISHED;
			pcb->cwnd = pcb->mss;
			--pcb->snd_queuelen;
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_process: SYN-SENT --queuelen %d\n", (u16)pcb->snd_queuelen);
			t_skb = __skb_dequeue(&pcb->unacked);
			assert(t_skb);
			skb_free(t_skb);

			/* Parse any options in the SYNACK. */
			tcp_parseopt(pcb, skb);

			/* 
			 * Call the user specified function to call when sucessfully
			 * connected.
			 */
			TCP_EVENT_CONNECTED(pcb, NET_OK, err);
			tcp_ack(pcb);

			/* we now can send */
			pcb->event_mask |= TCP_CAN_SND;
		}
		/* received ACK? possibly a half-open connection */
		else if (flags & TCP_ACK) 
		{
			/* send a RST to bring the other side in a non-synchronized state. */
			//tcp_rst(ackno, seqno + tcplen, iph->daddr, iph->saddr,th->dest, th->source);
		}
		skb_free(skb);
		break;
	case SYN_RCVD:
		if (flags & TCP_ACK) 
		{
			/* expected ACK number? */
			if (TCP_SEQ_BETWEEN(ackno, pcb->lastack+1, pcb->snd_nxt)) 
			{
				pcb->state = ESTABLISHED;
				IPSTACK_DEBUG(TCP_DEBUG, "TCP connection established %d -> %d.\n", th->source, th->dest);
				/* Call the accept function. */
				TCP_EVENT_ACCEPT(pcb, NET_OK, err);
				if (err != NET_OK) 
				{
					/*
					 * If the accept function returns with an error, we abort
					 * the connection. 
					 */
					tcp_abort(pcb);
					skb_free(skb);
					return NET_ERR_ABRT;
				}
				/*
				 * If there was any data contained within this ACK,
				 * we'd better pass it on to the application as well. 
				 */
				tcp_receive(pcb, skb);
				pcb->cwnd = pcb->mss;

				/* we now can send */
				pcb->event_mask |= TCP_CAN_SND;
			}
			/* incorrect ACK number */
			else 
			{
				/* drop it! */
				skb_free(skb);
			}
		}	else
		{
			/* drop it */
			skb_free(skb);
		}
		break;
	case CLOSE_WAIT:
    /* FALLTHROUGH */
	case ESTABLISHED:
		#if 0
		{
			static int i=100;

			if(i>0&&i<3)
			{
				i++;
				skb_free(skb);
				break; 
			}else
			{
				i++;
			}
		}
		#endif
		accepted_inseq = tcp_receive(pcb, skb);
		if ((flags & TCP_FIN) && accepted_inseq) 
		{
			/* passive close */
			tcp_ack_now(pcb);
			pcb->state = CLOSE_WAIT;
			/* 设置错误代码,避免tcp_sendto死循环*/
			pcb->err = NET_ERR_CLSD;
		}
		break;
  case FIN_WAIT_1:
		tcp_receive(pcb, skb);
		if (flags & TCP_FIN) 
		{
			if (flags & TCP_ACK && ackno == pcb->snd_nxt) 
			{
				IPSTACK_DEBUG(TCP_DEBUG,
					"TCP connection closed %d -> %d.\n", th->source, th->dest);
				tcp_ack_now(pcb);
				tcp_pcb_purge(pcb);
				TCP_RMV(&tcp_active_pcbs, pcb);
				pcb->state = TIME_WAIT;
				TCP_REG(&tcp_tw_pcbs, pcb);
			} else 
			{
				tcp_ack_now(pcb);
				pcb->state = CLOSING;
			}
		} else if (flags & TCP_ACK && ackno == pcb->snd_nxt) 
		{
			pcb->state = FIN_WAIT_2;
		}
		break;
	case FIN_WAIT_2:
		tcp_receive(pcb, skb);
		if (flags & TCP_FIN) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "TCP connection closed %d -> %d.\n", th->source, th->dest);
			tcp_ack_now(pcb);
			tcp_pcb_purge(pcb);
			TCP_RMV(&tcp_active_pcbs, pcb);
			pcb->state = TIME_WAIT;
			TCP_REG(&tcp_tw_pcbs, pcb);
		}
		break;
	case CLOSING:
		tcp_receive(pcb, skb);
		if (flags & TCP_ACK && ackno == pcb->snd_nxt) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "TCP connection closed %d -> %d.\n", th->source, th->dest);
			tcp_ack_now(pcb);
			tcp_pcb_purge(pcb);
			TCP_RMV(&tcp_active_pcbs, pcb);
			pcb->state = TIME_WAIT;
			TCP_REG(&tcp_tw_pcbs, pcb);
		}
		break;
  case LAST_ACK:
		tcp_receive(pcb, skb);
		if (flags & TCP_ACK && ackno == pcb->snd_nxt) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "TCP connection closed %d -> %d.\n", th->source, th->dest);
			pcb->state = CLOSED;
			recv_flags = TF_CLOSED;
		}
		break;
	default:
		/* drop it */
		skb_free(skb);
		break;
	}
	return NET_OK;
}

/* 
 * tcp_receive:
 *
 * Called by tcp_process. Checks if the given segment is an ACK for outstanding
 * data, and if so frees the memory of the buffered data. Next, is places the
 * segment on any of the receive queues (pcb->recved or pcb->ooseq). If the segment
 * is buffered, the pbuf is referenced by pbuf_ref so that it will not be freed until
 * i it has been removed from the buffer.
 *
 * If the incoming segment constitutes an ACK for a segment that was used for RTT
 * estimation, the RTT is estimated here as well.
 *
 * @return 1 if 
 */

static u8 tcp_receive(struct tcp_pcb *pcb, struct sk_buff *skb)
{
	s16 m;
	u32 right_wnd_edge;
	u8 accepted_inseq = 0;
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *th = skb->h.th;

	if(!(flags&TCP_ACK))
	{
		/* NO ACK, DROP it ! */
		skb_free(skb);
		return 0;
	}

	if (1/*flags & TCP_ACK*/) 
	{
		right_wnd_edge = pcb->snd_wnd + pcb->snd_wl1;

		/* Update window. */
		if (TCP_SEQ_LT(pcb->snd_wl1, seqno) ||/* the packet is more new ? */
			/* or packet ack more data ?*/
			(pcb->snd_wl1 == seqno && TCP_SEQ_LT(pcb->snd_wl2, ackno)) ||
			/* or peer advertise new window ?*/
			(pcb->snd_wl2 == ackno && th->window > pcb->snd_wnd))
		{
			/* update win */
			pcb->snd_wnd = th->window;
			if(pcb->snd_wnd < TCP_MSS&&pcb->snd_wnd>0)
				pcb->snd_wnd = TCP_MSS;
			pcb->snd_wl1 = seqno;
			pcb->snd_wl2 = ackno;
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: window update %d\n", pcb->snd_wnd);
		#if TCP_WND_DEBUG
		} else 
		{
			if (pcb->snd_wnd != th->window) 
			{
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: no window update lastack %d snd_max %d ackno %d wl1 %d seqno %d wl2 %d\n",
							pcb->lastack, pcb->snd_max, ackno, pcb->snd_wl1, seqno, pcb->snd_wl2);
			}
		#endif /* TCP_WND_DEBUG */
		}

		if (pcb->lastack == ackno) 
		{
			pcb->acked = 0;/* no data received by peer */

			if (pcb->snd_wl1 + pcb->snd_wnd == right_wnd_edge)
			{
				++pcb->dupacks;
				if (pcb->dupacks >= 3 && !skb_queue_empty(&pcb->unacked)) 
				{
					if (!(pcb->flags & TF_INFR)) 
					{
						/* This is fast retransmit. Retransmit the first unacked segment. */
						IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: dupacks %d (%d), fast retransmit %d\n",
											(u16)pcb->dupacks, pcb->lastack,
											ntohl(skb_peek(&pcb->unacked)->h.th->seq));
						tcp_rexmit(pcb);
						/* Set ssthresh to half of the minimum of the currenct cwnd and the advertised window */
						if (pcb->cwnd > pcb->snd_wnd)
							pcb->ssthresh = pcb->snd_wnd / 2;
						else
							pcb->ssthresh = pcb->cwnd / 2;

						pcb->cwnd = pcb->ssthresh + 3 * pcb->mss;
						pcb->flags |= TF_INFR;
					} else 
					{
						/* 
						 * Inflate the congestion window, but not if it means that
						*  the value overflows. */
						if ((u16)(pcb->cwnd + pcb->mss) > pcb->cwnd) 
						{
							pcb->cwnd += pcb->mss;
						}
					}
				}
			} else 
			{
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: dupack averted %u %u\n",
								pcb->snd_wl1 + pcb->snd_wnd, right_wnd_edge);
			}
		} else 
		if (TCP_SEQ_BETWEEN(ackno, pcb->lastack+1, pcb->snd_max))
		{
			/* We come here when the ACK acknowledges new data. */
	      
			/*
			 * Reset the "IN Fast Retransmit" flag, since we are no longer
			 * in fast retransmit. Also reset the congestion window to the
			 * slow start threshold. 
			 */
			if (pcb->flags & TF_INFR) 
			{
				pcb->flags &= ~TF_INFR;
				pcb->cwnd = pcb->ssthresh;
			}

			/* Reset the number of retransmissions. */
			pcb->nrtx = 0;

			/* Reset the retransmission time-out. */
			pcb->rto = (pcb->sa >> 3) + pcb->sv;

			/* Update the send buffer space. */
			pcb->acked = ackno - pcb->lastack;

			pcb->snd_buf += pcb->acked;

			/* Reset the fast retransmit variables. */
			pcb->dupacks = 0;
			pcb->lastack = ackno;
      		pcb->snd_nxt = ackno; 

			/* 
			 * Update the congestion control variables (cwnd and
			 * ssthresh). 
			 */
			if (pcb->state >= ESTABLISHED) 
			{
				if (pcb->cwnd < pcb->ssthresh) 
				{
					if ((u16)(pcb->cwnd + pcb->mss) > pcb->cwnd) 
					{
						pcb->cwnd += pcb->mss;
					}
					IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: slow start cwnd %d\n", pcb->cwnd);
				} else 
				{
					u16 new_cwnd = (u16)(pcb->cwnd + pcb->mss * pcb->mss / pcb->cwnd);
					if (new_cwnd > pcb->cwnd) 
					{
						pcb->cwnd = new_cwnd;
					}
					IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: congestion avoidance cwnd %d\n", pcb->cwnd);
				}
			}
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: ACK for %d, unacked->seqno %d:%d\n",
							ackno,
							skb_peek(&pcb->unacked)?
							ntohl(skb_peek(&pcb->unacked)->h.th->seq): 0,
							skb_peek(&pcb->unacked)?
							ntohl(skb_peek(&pcb->unacked)->h.th->seq) + TCP_TCPLEN(skb_peek(&pcb->unacked)): 0
							);

			/* 
			 * Remove segment from the unacknowledged list if the incoming
			 * ACK acknowlegdes them. 
			 */
			while(1)
			{
				struct sk_buff *t_skb = skb_peek(&pcb->unacked);
				if(!t_skb)
				{
					break;
				}
				if(TCP_SEQ_GT(ntohl(t_skb->h.th->seq)+TCP_TCPLEN(t_skb),
					ackno))
				{
					break;
				}
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: removing %d:%d from pcb->unacked\n",
							ntohl(t_skb->h.th->seq),
							ntohl(t_skb->h.th->seq) +
							TCP_TCPLEN(t_skb));
				__skb_unlink(t_skb, &pcb->unacked);
				pcb->snd_queuelen--;
				skb_free(t_skb);
			}
			/* 
			 * Remove segment from the unsent list if the incoming
			 * ACK acknowlegdes them. 
			 */
			while(1)
			{
				struct sk_buff *t_skb = skb_peek(&pcb->unsent);
				if(!t_skb)
				{
					break;
				}
				if(TCP_SEQ_GT(ntohl(t_skb->h.th->seq)+TCP_TCPLEN(t_skb),
					ackno))
				{
					break;
				}
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: removing %d:%d from pcb->unacked\n",
							ntohl(t_skb->h.th->seq),
							ntohl(t_skb->h.th->seq) +
							TCP_TCPLEN(t_skb));
				__skb_unlink(t_skb, &pcb->unsent);
				pcb->snd_queuelen--;
				skb_free(t_skb);
			}
			pcb->polltmr = 0;
		}

	    /* End of ACK for new data processing. */

		IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: pcb->rttest %d rtseq %d ackno %d\n",
				pcb->rttest, pcb->rtseq, ackno);

		/* RTT estimation calculations. This is done by checking if the
	       incoming segment acknowledges the segment we use to take a
	       round-trip time measurement. */
		if (pcb->rttest && TCP_SEQ_LT(pcb->rtseq, ackno)) 
		{
			m = (u16)(tcp_ticks - pcb->rttest);

			IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: experienced rtt %d ticks (%d msec).\n",
					m, m * TCP_SLOW_INTERVAL);

			/* This is taken directly from VJs original code in his paper */
			m = m - (pcb->sa >> 3);
			pcb->sa += m;
			if (m < 0) 
			{
				m = -m;
			}
			m = m - (pcb->sv >> 2);
			pcb->sv += m;
			pcb->rto = (pcb->sa >> 3) + pcb->sv;

			IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: RTO %d (%d miliseconds)\n",
					pcb->rto, pcb->rto * TCP_SLOW_INTERVAL);

			pcb->rttest = 0;
		}
	}

	/* **************************/

	if(tcplen <= 0)
	{
		/* Segments with length 0 is taken care of here. Segments that
		   fall out of the window are ACKed. */
		if(pcb->rcv_wnd>2&&!TCP_SEQ_BETWEEN(seqno, pcb->rcv_nxt, pcb->rcv_nxt + pcb->rcv_wnd-1))
		{
			tcp_ack_now(pcb);//BUGGGGGG!!!!!!
		}
		skb_free(skb);
		return accepted_inseq;
	}

	if(pcb->rcv_wnd <= 0)
	{
		skb_free(skb);
		tcp_ack_now(pcb);
		return accepted_inseq;
	}

	/* If the incoming segment contains data, we must process it
     further. */
     
	/* This code basically does three things:

	+) If the incoming segment contains data that is the next
		in-sequence data, this data is passed to the application. This
		might involve trimming the first edge of the data. The rcv_nxt
		variable and the advertised window are adjusted.

	+) If the incoming segment has data that is above the next
		sequence number expected (->rcv_nxt), the segment is placed on
		the ->ooseq queue. This is done by finding the appropriate
		place in the ->ooseq queue (which is ordered by sequence
		number) and trim the segment in both ends if needed. An
		immediate ACK is sent to indicate that we received an
		out-of-sequence segment.

	+) Finally, we check if the first segment on the ->ooseq queue
		now is in sequence (i.e., if rcv_nxt >= ooseq->seqno). If
		rcv_nxt > ooseq->seqno, we must trim the first edge of the
		segment on ->ooseq before we adjust rcv_nxt. The data in the
		segments that are now on sequence are chained onto the
		incoming segment so that we only need to call the application
		once.
	*/

	/* First, we check if we must trim the first edge. We have to do
		this if the sequence number of the incoming segment is less
		than rcv_nxt, and the sequence number plus the length of the
		segment is larger than rcv_nxt. */
	if (TCP_SEQ_BETWEEN(pcb->rcv_nxt, seqno + 1, seqno + tcplen - 1))
	{
		__skb_pull(skb, pcb->rcv_nxt-seqno); 
		
		seqno = pcb->rcv_nxt;
	} else 
	{
		if (TCP_SEQ_LT(seqno, pcb->rcv_nxt))
		{
			/* the whole segment is < rcv_nxt */
			/* must be a duplicate of a packet that has already been correctly handled */
       
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: duplicate seqno %u, rcv_nxt %u\n", 
						seqno, pcb->rcv_nxt);		
						
			tcp_ack_now(pcb);
		}
	}

	if(!TCP_SEQ_BETWEEN(seqno, pcb->rcv_nxt, pcb->rcv_nxt + pcb->rcv_wnd-1))
	{
		skb_free(skb);
		return accepted_inseq;
	}
	
	TCP_TCPLEN_SET(skb, skb->len);
	tcplen = TCP_TCPLEN(skb);

	/* The sequence number must be within the window (above rcv_nxt
	and below rcv_nxt + rcv_wnd) in order to be further
	processed. */
	if (pcb->rcv_nxt == seqno)
	{
		accepted_inseq = 1; 
		/* The incoming segment is the next in sequence. We check if
		we have to trim the end of the segment and update rcv_nxt
		and pass the data to the application. */
		#if TCP_QUEUE_OOSEQ
		if (!skb_queue_empty(&pcb->ooseq))
		{
			struct sk_buff *t_skb = skb_peek(&pcb->ooseq);
			if(TCP_SEQ_LEQ(t_skb->h.th->seq, seqno+TCP_TCPLEN(t_skb)))
			{
				__skb_trim(t_skb, t_skb->h.th->seq-seqno);
				TCP_TCPLEN_SET(t_skb, t_skb->len);
			}
		}
		#endif /* TCP_QUEUE_OOSEQ */

		tcplen = TCP_TCPLEN(skb);

		/* First received FIN will be ACKed +1, on any successive (duplicate)
		 * FINs we are already in CLOSE_WAIT and have already done +1.
		 */
		if (pcb->state != CLOSE_WAIT) 
		{
			pcb->rcv_nxt += tcplen;
		}

		/* Update the receiver's (our) window. */
		if (pcb->rcv_wnd < tcplen) 
		{
			pcb->rcv_wnd = 0;
		} else 
		{
			pcb->rcv_wnd -= tcplen;
		}

		if (skb->h.th->fin) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, ("tcp_receive: received FIN.\n"));
			recv_flags = TF_GOT_FIN;
		}
		if(tcplen > 0)
		{
			if((tcplen-skb->h.th->fin-skb->h.th->syn) > 0)
				__skb_queue_tail(&pcb->incoming, skb);
			else
				skb_free(skb);
		}else
		{
			skb_free(skb);
			return 0;
		}

		#if TCP_QUEUE_OOSEQ
		/* We now check if we have segments on the ->ooseq queue that
		is now in sequence. */
		while(!skb_queue_empty(&pcb->ooseq))
		{
			struct sk_buff *t_skb = skb_peek(&pcb->ooseq);
			if(t_skb->h.th->seq != pcb->rcv_nxt)
				break;
			pcb->rcv_nxt += TCP_TCPLEN(t_skb);
			if (pcb->rcv_wnd < TCP_TCPLEN(t_skb))
			{
				pcb->rcv_wnd = 0;
			} else
			{
				pcb->rcv_wnd -= TCP_TCPLEN(t_skb);
			}
			if (t_skb->h.th->fin)
			{
				IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: dequeued FIN.\n");
				recv_flags = TF_GOT_FIN;
				if (pcb->state == ESTABLISHED)
				{ /* force passive close or we can move to active close */
					pcb->state = CLOSE_WAIT;
				} 
			}
			__skb_unlink(t_skb, &pcb->ooseq);
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_receive: %08x from OOSEQ to incoming\n",
					t_skb);
			__skb_queue_tail(&pcb->incoming, t_skb);
		}
		#endif /* TCP_QUEUE_OOSEQ */


		/* Acknowledge the segment(s). */
		tcp_ack(pcb);

    } else 
	{
		/* We get here if the incoming segment is out-of-sequence. */
		tcp_ack_now(pcb);
		#if TCP_QUEUE_OOSEQ
		{
			struct sk_buff *t_skb, *prev;
			prev = NULL;
			for(t_skb=pcb->ooseq.next; t_skb!=(struct sk_buff*)&pcb->ooseq; 
				t_skb= t_skb->next)
			{
				if(TCP_SEQ_LEQ(seqno+TCP_TCPLEN(skb), t_skb->h.th->seq))
				{
					break;
				}
				prev = t_skb;
			}
			if(prev)
			{
				if(TCP_SEQ_GT(prev->h.th->seq+TCP_TCPLEN(prev), seqno))
				{
					__skb_trim(prev, seqno-prev->h.th->seq);
					TCP_TCPLEN_SET(prev, prev->len);
					if(TCP_TCPLEN(prev)==0)
					{
						//free it
						__skb_unlink(prev, &pcb->ooseq);
						skb_free(prev);
					}
				}
			}

			if(t_skb == (struct sk_buff*)&pcb->ooseq)
			{
				/*this seq is the last */
				__skb_queue_tail(&pcb->ooseq, skb);
			}else
			{
				/* this seq is between prev and t_skb */
				if(TCP_SEQ_GT(skb->h.th->seq+TCP_TCPLEN(skb),
						t_skb->h.th->seq))
				{
					__skb_trim(skb, t_skb->h.th->seq-skb->h.th->seq);
					TCP_TCPLEN_SET(skb, skb->len);
				}			
				if(TCP_TCPLEN(skb)>0)
				{
					__skb_queue_after(&pcb->ooseq, t_skb->prev, skb);
				}else
				{
					skb_free(skb);
				}
			}
		}
		#else
		skb_free(skb);
		#endif /* TCP_QUEUE_OOSEQ */

	}
	return accepted_inseq;
}

/*
 * tcp_parseopt:
 *
 * Parses the options contained in the incoming segment. (Code taken
 * from uIP with only small changes.)
 *
 */
static void tcp_parseopt(struct tcp_pcb *pcb, struct sk_buff *skb)
{
	u8 c;
	u8 *opts, opt;
	u16 mss;
	struct tcphdr *th = skb->h.th;
	int total_len = th->doff*4;

	opts = (u8 *)th + TCP_HLEN;

	/* Parse the TCP MSS option, if present. */
	total_len -= 20;
	if(total_len <= 0)
		return;
	for(c = 0; c < total_len ;) 
	{
		opt = opts[c];
		if (opt == 0x00) 
		{
			/* End of options. */
			break;
		} else if (opt == 0x01) 
		{
			++c;
			/* NOP option. */
		} else if (opt == 0x02 && opts[c + 1] == 0x04) 
		{
			/* An MSS option with the right option length. */
			mss = (opts[c + 2] << 8) | opts[c + 3];
			pcb->mss = mss > TCP_MSS? TCP_MSS: mss;

			/* And we are done processing options. */
			break;
		} else 
		{
			if (opts[c + 1] == 0) 
			{
				/* If the length field is zero, the options are malformed
				and we don't process them further. */
				break;
			}
			/* All other options have a length field, so that we easily
			can skip past them. */
			c += opts[c + 1];
		}
	}
}

