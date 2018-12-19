/**
 * Transmission Control Protocol, outgoing traffic
 *
 * The output functions of TCP.
 *
修改历史
080716 sunJH
1.tcp_write and tcp_rexmit_rto增加如下功能：
发送数据或重传时，如果目前还未收到对方的任何数据应答，
则重传第三次握手；该方法保证存在防火墙情况下，可以真正建立连接。
因为没有真正完成三次握手的话，防火墙是不允许发送数据的。
080820 sunJH
在没有完成一次数据传输之前，重传会增加ACK
重传ACK由tcp_ack改为tcp_rexmit_ack实现，
这样增加了可读性和可扩展性
 */
 
#include <inet/inet.h>
#include <inet/skbuff.h>
#include <inet/ip.h>
#include <inet/tcp.h>

err_t tcp_send_ctrl(struct tcp_pcb *pcb, u8 flags)
{
	/* no data, no length, flags, copy=1, no optdata, no optdatalen */
	return tcp_enqueue(pcb, NULL, 0, flags, 1, NULL, 0);
}

void tcp_rexmit_ack(struct tcp_pcb *pcb)
{
	tcp_ack(pcb);
}

/*
 * Write data for sending (but does not send it immediately).
 *
 * It waits in the expectation of more data being sent soon (as
 * it can send them more efficiently by combining them together).
 * To prompt the system to send data now, call tcp_output() after
 * calling tcp_write().
 * 
 * @arg pcb Protocol control block of the TCP connection to enqueue data for. 
 * 
 * @see tcp_write()
 */

err_t tcp_write(struct tcp_pcb *pcb, const void *arg, u16 len, u8 copy)
{
	err_t err;
	IPSTACK_DEBUG(TCP_DEBUG, "tcp_write(pcb=%p, arg=%p, len=%d, copy=%d)\n", (void *)pcb,
				arg, len, (u16)copy);
	/* connection is in valid state for data transmission? */
	if (pcb->state == ESTABLISHED ||
		pcb->state == CLOSE_WAIT 
		) 
	{
		if(pcb->state == CLOSE_WAIT&&
			pcb->err<0)
			return pcb->err;/* error! */
		
		if (len > 0) 
		{
			#ifndef REXMIT_ACK_DISABLE
			u8 send_ack_first;
			send_ack_first = (pcb->state==ESTABLISHED&&pcb->send_data_ok==0)?1:0;
			if(send_ack_first)
				tcp_rexmit_ack(pcb);
			#endif/* REXMIT_ACK_DISABLE */
			err = tcp_enqueue(pcb, (void *)arg, len, TCP_ACK, copy, NULL, 0);
			if(err >= 0)			 	
				tcp_output(pcb);
			else
			{
				pcb->event_mask &= ~TCP_CAN_SND;
			}
			return err;
		}
		return 0;
	} else 
	{
		IPSTACK_DEBUG(TCP_DEBUG,  "tcp_write() called in invalid state\n");
		if(pcb->err)
			return pcb->err;
		return NET_ERR_CONN;
	}
}

/*
 * Enqueue either data or TCP options  for tranmission
 * 
 * @arg pcb Protocol control block for the TCP connection to enqueue data for.
 * @arg arg Pointer to the data to be enqueued for sending.
 * @arg len Data length in bytes
 * @arg flags
 * @arg copy 1 if data must be copied, 0 if data is non-volatile and can be
 * referenced.
 * @arg optdata
 * @arg optlen
 */
err_t tcp_enqueue(struct tcp_pcb *pcb, void *arg, u16 len,
					u8 flags, u8 copy,
					u8 *optdata, u8 optlen)
{
	struct sk_buff *skb;
	struct tcphdr *th;
	INET_DEV_T *dev;
	struct route *rt;
	struct sk_buff_head  t_head;
	u32 left, seqno;
	u16 seglen;
	u8 queuelen;

	rt = ip_route(pcb->remote_ip, pcb->out_dev);

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_enqueue(pcb=%p, arg=%p, len=%d, flags=%x\n",
				(void *)pcb, arg, len, (u32)flags);
	assert(len == 0 || optlen == 0);
	assert(arg == NULL || optdata == NULL);

	if(!rt)
	{
		return NET_ERR_RTE;
	}
	dev = rt->out;
	/* If total number of pbufs on the unsent/unacked queues exceeds the
	 * configured maximum, return an error 
	 */
	queuelen = pcb->unacked.qlen+pcb->unsent.qlen;
	if (len>0&&queuelen >= TCP_SND_QUEUELEN) 
	{
		IPSTACK_DEBUG(TCP_DEBUG , "tcp_enqueue: too long queue %d (max %d)\n", queuelen, TCP_SND_QUEUELEN);
		return NET_ERR_MEM;
	}

	if (len > pcb->snd_buf) 
	{
		/*
		 * TCP_MSS is 1024, may be mini mss in PC Ethernet;
		 * But We can be sure it 100%!
		 */
		len = (u16)(pcb->snd_buf);
	}
	left = len;

	/* 
	 * seqno will be the sequence number of the first segment enqueued
	 * by the call to this function. 
	 */
	seqno = pcb->snd_lbb;

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_enqueue: queuelen: %d\n", (u16)pcb->snd_queuelen);

	/* 
	 * First, break up the data into segments and tuck them together in
	 * the local "queue" variable. 
	 */
	skb_queue_head_init(&t_head);
	while((left>0)||(flags&(TCP_SYN|TCP_FIN)))
	{
		int size;/* pkt size */
		if (queuelen >= TCP_SND_QUEUELEN) 
		{
			break;
		}
		/* 
		 * The segment length should be the MSS if the data to be enqueued
		 * is larger than the MSS. 
		 */
		seglen = (u16)(left > pcb->mss? pcb->mss: left);
		size = dev->header_len+sizeof(struct iphdr)+sizeof(struct tcphdr)+optlen+seglen;
		skb = skb_new(size);
		if (skb == NULL) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_queue: could not allocate memory for pbuf\n");
			break;
		}
		skb->dev = dev;
		skb_reserve(skb, size);
		skb->h.raw = __skb_push(skb, sizeof(struct tcphdr)+optlen+seglen);
		th = skb->h.th;
		th->source = htons(pcb->local_port);
		th->dest = htons(pcb->remote_port);
		th->seq = htonl(seqno);
		th->ack_seq = htonl(pcb->rcv_nxt);
		if(flags&TCP_SYN)
			th->syn = 1;
		if(flags&TCP_FIN)
			th->fin = 1;
		if(flags&TCP_ACK)
			th->ack = 1;
		if((flags&(TCP_SYN|TCP_FIN|TCP_RST))==0)
			th->psh = 1;//??????
		flags &= ~(TCP_SYN|TCP_FIN);
		th->window = 0;
		th->urg_ptr = 0;
		th->doff = (sizeof(struct tcphdr)+optlen)>>2;
		if(optlen>0)
			INET_MEMCPY(th+1, optdata, optlen);	
		if(seglen > 0)
			INET_MEMCPY(((u8*)th)+sizeof(struct tcphdr)+optlen, arg, seglen);
		th->check = 0;
		TCP_TCPLEN_SET(skb, seglen);
		__skb_queue_tail(&t_head, skb);
		++queuelen;
		left -= seglen;
		arg = (u8*)arg+seglen;
		seqno += seglen+th->fin+th->syn;
	}

	if(skb_queue_empty(&t_head))
		return NET_ERR_MEM;
	pcb->snd_buf -= (seqno - pcb->snd_lbb);
	pcb->snd_lbb = seqno;

	while(!skb_queue_empty(&t_head))
	{
		skb = __skb_dequeue(&t_head);
		__skb_queue_tail(&pcb->unsent, skb);
	}

	return len-left;
}

/*
 * Actually send a TCP segment over IP
 */
static void tcp_output_segment(struct tcp_pcb *pcb, struct sk_buff *skb, struct route *rt)
{
	struct tcphdr *th = skb->h.th;
	INET_DEV_T *dev = NULL;

	if(!rt)
	{
		rt = ip_route(pcb->remote_ip, pcb->out_dev);
		if(!rt)
		{
			return;
		}
	}
	dev = rt->out;

	/** @bug Exclude retransmitted segments from this count. */

	/* The TCP header has already been constructed, but the ackno and
	 * wnd fields remain. 
	 */
	th->ack_seq = htonl(pcb->rcv_nxt);

	/* silly window avoidance */
	if (pcb->rcv_wnd < pcb->mss) 
	{
		th->window = 0;
	} else 
	{
		/* advertise our receive window size in this TCP segment */
		th->window = htons(pcb->rcv_wnd);
	}

	if (ip_addr_isany(&(pcb->local_ip))) 
	{
		pcb->local_ip = dev->addr;
		skb->dev = dev;
	}

	pcb->rtime = 0;

	if (pcb->rttest == 0) 
	{
		pcb->rttest = tcp_ticks;
		pcb->rtseq = ntohl(th->seq);

		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output_segment: rtseq %d\n", pcb->rtseq);
	}
	IPSTACK_DEBUG(TCP_DEBUG, "tcp_output_segment: %d:%d\n",
          htonl(th->seq), htonl(th->seq)+skb->len-th->doff*4
          );

	th->check= 0;
	#if CHECKSUM_GEN_TCP
	th->check = inet_chksum_pseudo(th, skb->len,
             pcb->local_ip,
             pcb->remote_ip,
             IP_PROTO_TCP);
	#endif

	ip_output(skb, pcb->local_ip, pcb->remote_ip, pcb->ttl, pcb->tos,
      IP_PROTO_TCP, rt);
}

/* find out what we can send and send it */
err_t tcp_output(struct tcp_pcb *pcb)
{
	struct sk_buff *skb, *t_skb;
	struct tcphdr *th;
	struct route *rt = ip_route(pcb->remote_ip, pcb->out_dev);
	INET_DEV_T     *dev;
	u32 wnd;
	s16 i = 0;

	/* First, check if we are invoked by the TCP input processing
	code. If so, we do not output anything. Instead, we rely on the
	input processing code to call us when input processing is done
	with. */
	if (tcp_input_pcb == pcb)
	{
		return NET_OK;
	}

	if(!rt)
	{
		return NET_ERR_RTE;
	}
	dev = rt->out;

	if(!DEV_FLAGS(dev, FLAGS_TX))
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output dev NOT TX\n");
		return NET_OK;
	}

	IPSTACK_DEBUG(TCP_DEBUG, "tcp output..........\n");

	wnd = IPSTACK_MIN(pcb->snd_wnd, pcb->cwnd);

	skb = skb_peek(&pcb->unsent);
   
	/* If the TF_ACK_NOW flag is set and no data will be sent (either
	* because the ->unsent queue is empty or because the window does
	* not allow it), construct an empty ACK segment and send it.
	*
	* If data is to be sent, we will just piggyback the ACK (see below).
	*/
	if ((pcb->flags & TF_ACK_NOW) &&
		(skb==NULL||
		ntohl(skb->h.th->seq) - pcb->lastack + TCP_TCPLEN(skb) > wnd)) 
	{
		int size = dev->header_len+sizeof(struct iphdr)+sizeof(struct tcphdr);
		skb = skb_new(size);
		if (skb == NULL) 
		{
			IPSTACK_DEBUG(TCP_DEBUG, "tcp_output: (ACK) could not allocate pbuf\n");
			return NET_ERR_BUF;
		}
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output: sending ACK for %u\n", pcb->rcv_nxt);
		/* remove ACK flags from the PCB, as we send an empty ACK now */
		pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);

		skb_reserve(skb, size);
		skb->h.raw = __skb_push(skb, sizeof(struct tcphdr));
		th = skb->h.th;
		th->source = htons(pcb->local_port);
		th->dest = htons(pcb->remote_port);
		th->seq = htonl(pcb->snd_nxt);
		th->ack_seq = htonl(pcb->rcv_nxt);
		th->ack = 1;
		th->window = htons(pcb->rcv_wnd);
		th->urg_ptr = 0;
		th->doff = 5;
		th->check = 0;
		#if CHECKSUM_GEN_TCP
		th->check = inet_chksum_pseudo(th, skb->len, pcb->local_ip, pcb->remote_ip,
				IP_PROTO_TCP);
		#endif
		ip_output(skb, pcb->local_ip, pcb->remote_ip, pcb->ttl, pcb->tos,
			IP_PROTO_TCP, rt);
		skb_free(skb);

		return NET_OK;
	}

	#if TCP_OUTPUT_DEBUG
	if (skb == NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output: nothing to send \n");
	}
	#endif /* TCP_OUTPUT_DEBUG */
	#if TCP_CWND_DEBUG
	if (skb == NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output: snd_wnd %d, cwnd %d, wnd %d, seg == NULL, ack %u\n",
					pcb->snd_wnd, pcb->cwnd, wnd,
					pcb->lastack);
	} else 
	{
		th = skb->h.th;
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output: snd_wnd %d, cwnd %ud, wnd %u, effwnd %u, seq %u, ack %u\n",
					pcb->snd_wnd, pcb->cwnd, wnd,
					ntohl(th->seq) - pcb->lastack + TCP_TCPLEN(skb),
					ntohl(th->seq), pcb->lastack);
	}
	#endif /* TCP_CWND_DEBUG */
	
	/* data available and window allows it to be sent? */
	while (((skb=skb_peek(&pcb->unsent))!= NULL)) 
	{
		u32 size = ntohl(skb->h.th->seq) - pcb->lastack + TCP_TCPLEN(skb);
		if(!DEV_FLAGS(dev, FLAGS_TX))
		{
			IPSTACK_DEBUG(TCP_DEBUG,"tcp output ERROR dev %s NOT TX\n",
				dev->name
				);
			break;
		}
		if( size> wnd)
		{
			IPSTACK_DEBUG(TCP_DEBUG, 
			"tcp output ERROR seq=%u lastack=%u len=%d, size=%u, win=%d\n",
					ntohl(skb->h.th->seq),
					pcb->lastack,
					TCP_TCPLEN(skb),
					ntohl(skb->h.th->seq) - pcb->lastack + TCP_TCPLEN(skb),
					wnd
					);
			//assert(size<10*1024);
			break;
		}
		__skb_unlink(skb, &pcb->unsent);
		th = skb->h.th;
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_output: snd_wnd %ul, cwnd %"U16_F", wnd %"U32_F", effwnd %"U32_F", seq %"U32_F", ack %"U32_F", i %"S16_F"\n",
                            pcb->snd_wnd, pcb->cwnd, wnd,
                            ntohl(th->seq) + TCP_TCPLEN(skb) -
                            pcb->lastack,
                            ntohl(th->seq), pcb->lastack, i);
		++i;
		if (pcb->state != SYN_SENT) 
		{
			th->ack = 1;
			pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
		}
		tcp_output_segment(pcb, skb, rt);
		pcb->snd_nxt = ntohl(th->seq) + TCP_TCPLEN(skb);
		if (TCP_SEQ_LT(pcb->snd_max, pcb->snd_nxt))
		{
			pcb->snd_max = pcb->snd_nxt;
		}
		/* do not queue empty segments on the unacked list */
		if(TCP_TCPLEN(skb) == 0)
		{
			skb_free(skb);
			continue;
		}
		pcb->snd_queuelen++;
		/* put segment on unacknowledged list if length > 0 */
		t_skb = skb_peek(&pcb->unacked);
		if(t_skb == NULL)
		{
			__skb_queue_head(&pcb->unacked, skb);
			continue;
		}
		/* In the case of fast retransmit, the packet should not go to the tail
		 * of the unacked queue, but rather at the head. We need to check for
		 * this case.
		 */
		if (TCP_SEQ_LT(ntohl(th->seq), ntohl(t_skb->h.th->seq)))
		{
			/* add segment to head of unacked list */
			__skb_queue_head(&pcb->unacked, skb);
        } else 
        {
			/* add segment to tail of unacked list */
			__skb_queue_tail(&pcb->unacked, skb);
		}
	}
	return NET_OK;
}

void tcp_rst(u32 seqno, u32 ackno, u32 local_ip, u32 remote_ip,
  u16 local_port, u16 remote_port)
{
	struct sk_buff *skb;
	struct tcphdr *th;
	struct route *rt = ip_route(remote_ip, NULL);
	INET_DEV_T *dev;
	int len;

	if(!rt)
	{
		return;
	}
	dev = rt->out;
	len = dev->header_len+sizeof(struct iphdr)+sizeof(struct tcphdr);

	skb = skb_new(len);
	if (skb == NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_rst: could not allocate memory for pbuf\n");
		return;
	}
	skb_reserve(skb, len);
	skb->h.raw = __skb_push(skb, sizeof(struct tcphdr));
	th = skb->h.th;
	th->source = htons(local_port);
	th->dest = htons(remote_port);
	th->seq = htonl(seqno);
	th->ack_seq = htonl(ackno);
	th->rst = 1;
	th->ack = 1;
	th->window = htons(TCP_WND);
	th->urg_ptr = 0;
	th->doff = 5;
	th->check = 0;
#if CHECKSUM_GEN_TCP
	th->check = inet_chksum_pseudo(th, skb->len, local_ip, remote_ip,
              IP_PROTO_TCP);
#endif
   /* Send output with hardcoded TTL since we have no access to the pcb */
	ip_output(skb, local_ip, remote_ip, TCP_TTL, 0, IP_PROTO_TCP, 
			rt);
	skb_free(skb);
	IPSTACK_DEBUG(TCP_DEBUG, "tcp_rst: seqno %u ackno %u.\n", seqno, ackno);
}

/* requeue all unacked segments for retransmission */
void tcp_rexmit_rto(struct tcp_pcb *pcb)
{
	struct sk_buff *skb;
	u8 send_ack_first;

	if (skb_queue_empty(&pcb->unacked)) 
	{
		return;
	}

	send_ack_first = (pcb->state==ESTABLISHED&&pcb->send_data_ok==0)?1:0;
	//send_ack_first = 0;

	/* Move all unacked segments to the head of the unsent queue */
	while(!skb_queue_empty(&pcb->unacked))
	{
		#ifndef REXMIT_ACK_DISABLE
		if(send_ack_first)
		{
			tcp_rexmit_ack(pcb);
			send_ack_first = 0;
		}
		#endif/* REXMIT_ACK_DISABLE */
		skb = skb_peek_tail(&pcb->unacked);
		__skb_unlink(skb, &pcb->unacked);
		__skb_queue_head(&pcb->unsent, skb);
	}
	skb = skb_peek(&pcb->unsent);
	if(!skb)
		return;
	pcb->snd_nxt = ntohl(skb->h.th->seq);
	/* increment number of retransmissions */
	++pcb->nrtx;

	/* Don't take any RTT measurements after retransmitting. */
	pcb->rttest = 0;

	/* Do the actual retransmission */
	tcp_output(pcb);
}

void tcp_rexmit(struct tcp_pcb *pcb)
{
	struct sk_buff *skb;

	/* Move the first unacked segment to the unsent queue */
	skb = __skb_dequeue(&pcb->unacked);
	if(!skb)
		return;
	__skb_queue_head(&pcb->unsent, skb);
	pcb->snd_nxt = ntohl(skb->h.th->seq);
	++pcb->nrtx;

	/* Don't take any rtt measurements after retransmitting. */
	pcb->rttest = 0;

	/* Do the actual retransmission. */
	tcp_output(pcb);
}

void tcp_keepalive(struct tcp_pcb *pcb)
{
	struct sk_buff *skb;
	struct tcphdr *th;
	struct route *rt = ip_route(pcb->remote_ip, pcb->out_dev);
	INET_DEV_T *dev;
	int len ;

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_keepalive: sending KEEPALIVE probe to %d.%d.%d.%d\n",
                           ip4_addr1(pcb->remote_ip), ip4_addr2(pcb->remote_ip),
                           ip4_addr3(pcb->remote_ip), ip4_addr4(pcb->remote_ip));

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_keepalive: tcp_ticks %d   pcb->tmr %d  pcb->keep_cnt %d\n", 
							tcp_ticks, pcb->tmr, pcb->keep_cnt);
   
	if(!rt)
	{
		return;
	}
	dev = rt->out;
	len = dev->header_len+sizeof(struct iphdr)+sizeof(struct tcphdr)+1;

	skb = skb_new(len);
	if (skb == NULL) 
	{
		IPSTACK_DEBUG(TCP_DEBUG, "tcp_keepalive: could not allocate memory for pbuf\n");
		return;
	}
	skb_reserve(skb, len);
	skb->h.raw = __skb_push(skb, sizeof(struct tcphdr)+1);
	th = skb->h.th;
	th->source = htons(pcb->local_port);
	th->dest = htons(pcb->remote_port);
	//th->seq = htonl(pcb->snd_nxt - 1);
	th->seq = htonl(pcb->lastack - 1);
	th->ack_seq = htonl(pcb->rcv_nxt);
	th->ack = 1;
	th->window = htons(pcb->rcv_wnd);
	th->urg_ptr = 0;
	th->doff = 5;
	th->check = 0;
#if CHECKSUM_GEN_TCP
	th->check = inet_chksum_pseudo(th, skb->len, pcb->local_ip, pcb->remote_ip,
              IP_PROTO_TCP);
#endif
	/* Send output with hardcoded TTL since we have no access to the pcb */
	ip_output(skb, pcb->local_ip, pcb->remote_ip, 
		TCP_TTL, 0, IP_PROTO_TCP, rt);
	skb_free(skb);

	IPSTACK_DEBUG(TCP_DEBUG, "tcp_keepalive: seqno %d ackno %d.\n", pcb->snd_nxt - 1, pcb->rcv_nxt);
}

