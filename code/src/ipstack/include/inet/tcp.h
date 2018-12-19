#ifndef _TCP_H_
#define _TCP_H_
#include <inet/inet.h>
#include <inet/skbuff.h>
#define CHECKSUM_GEN_TCP 1
struct tcp_pcb;

struct tcphdr {
	u16	source;
	u16	dest;
	u32	seq;
	u32	ack_seq;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u16	res1:4,
		doff:4,
		fin:1,
		syn:1,
		rst:1,
		psh:1,
		ack:1,
		urg:1,
		ece:1,
		cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u16	doff:4,
		res1:4,
		cwr:1,
		ece:1,
		urg:1,
		ack:1,
		psh:1,
		rst:1,
		syn:1,
		fin:1;
#else
#error	"Adjust your byte orders defines"
#endif	
	u16	window;
	u16	check;
	u16	urg_ptr;
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;

/*
 * TCP Module Init
 * Must be called first to initialize TCP.
 */
void tcp_init(void);/* Must be called first to initialize TCP. */

/*
 * TCP Timer Process
 * Must be called every TCP_TMR_INTERVAL ms. (Typically 250 ms).
 * 
 */
void tcp_tmr(unsigned long flags);

struct tcp_pcb * tcp_new     (void);

/*
 * Set Arg for Callback
 */
void tcp_arg(struct tcp_pcb *pcb, void *arg);

/*
 * Set Accept Callback Function
 */
void tcp_accept  (struct tcp_pcb *pcb,
     err_t (* accept)(void *arg, struct tcp_pcb *newpcb, err_t err)
     );
     
void tcp_recv    (struct tcp_pcb *pcb,
     err_t (* recv)(void *arg, struct tcp_pcb *tpcb,struct sk_buff *p, err_t err)
     );
void tcp_sent    (struct tcp_pcb *pcb,
     err_t (* sent)(void *arg, struct tcp_pcb *tpcb, u16 len)
     );
void tcp_poll    (struct tcp_pcb *pcb,
     err_t (* poll)(void *arg, struct tcp_pcb *tpcb), 
     u8 interval);
void tcp_err     (struct tcp_pcb *pcb,
     void (* err)(void *arg, err_t err)
     );


int tcp_recv_data(struct tcp_pcb *pcb, u8 *buf, int size);
void tcp_recved(struct tcp_pcb *pcb, u16 len);
err_t tcp_bind(struct tcp_pcb *pcb, struct ip_addr *ipaddr, u16 port);
err_t tcp_connect (struct tcp_pcb *pcb, struct ip_addr *ipaddr,
            u16 port, err_t (* connected)(void *arg,
                    struct tcp_pcb *tpcb,
                    err_t err));
struct tcp_pcb * tcp_listen  (struct tcp_pcb *pcb);
void             tcp_abort   (struct tcp_pcb *pcb);
err_t            tcp_close   (struct tcp_pcb *pcb);
err_t            tcp_write   (struct tcp_pcb *pcb, const void *dataptr, u16 len,
            u8 copy);

void             tcp_setprio (struct tcp_pcb *pcb, u8 prio);

#define TCP_PRIO_MIN    1
#define TCP_PRIO_NORMAL 64
#define TCP_PRIO_MAX    127

/* It is also possible to call these two functions at the right
   intervals (instead of calling tcp_tmr()). */
void             tcp_slowtmr (void);
void             tcp_fasttmr (void);


/* Only used by IP to pass a TCP segment to TCP: */
void             tcp_input   (struct sk_buff *skb);
/* Used within the TCP code only: */
err_t            tcp_output  (struct tcp_pcb *pcb);
void             tcp_rexmit  (struct tcp_pcb *pcb);
void             tcp_rexmit_rto  (struct tcp_pcb *pcb);



#define TCP_SEQ_LT(a,b)     ((s32)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)    ((s32)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b)     ((s32)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)    ((s32)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c) (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))
#define TCP_FIN 0x01U
#define TCP_SYN 0x02U
#define TCP_RST 0x04U
#define TCP_PSH 0x08U
#define TCP_ACK 0x10U
#define TCP_URG 0x20U
#define TCP_ECE 0x40U
#define TCP_CWR 0x80U

#define TCP_FLAGS 0x3fU

/* Length of the TCP header, excluding options. */
#define TCP_HLEN 20

#ifndef TCP_TMR_INTERVAL
#define TCP_TMR_INTERVAL 250  /* The TCP timer interval in milliseconds. */
#endif /* TCP_TMR_INTERVAL */

#ifndef TCP_FAST_INTERVAL
#define TCP_FAST_INTERVAL      TCP_TMR_INTERVAL /* the fine grained timeout in
                                       milliseconds */
#endif /* TCP_FAST_INTERVAL */

#ifndef TCP_SLOW_INTERVAL
#define TCP_SLOW_INTERVAL      (2*TCP_TMR_INTERVAL)  /* the coarse grained timeout in
                                       milliseconds */
#endif /* TCP_SLOW_INTERVAL */

#define TCP_FIN_WAIT_TIMEOUT 10000 /* milliseconds */
#define TCP_SYN_RCVD_TIMEOUT 10000  /* milliseconds */

#define TCP_OOSEQ_TIMEOUT        6U /* x RTO */

#define TCP_MSL 1000  /* The maximum segment lifetime in microseconds */

/*
 * User-settable options (used with setsockopt).
 */
#define TCP_NODELAY    0x01    /* don't delay send to coalesce packets */
#define TCP_KEEPALIVE  0x02    /* send KEEPALIVE probes when idle for pcb->keepalive miliseconds */

/* Keepalive values */
#define  TCP_KEEPDEFAULT   50000        /* KEEPALIVE timer in miliseconds */
#define  TCP_KEEPINTVL     TCP_SLOW_INTERVAL*2          /* Time between KEEPALIVE probes in miliseconds */
#define  TCP_KEEPCNT       10                           /* Counter for KEEPALIVE probes */
#define  TCP_MAXIDLE       TCP_KEEPCNT * TCP_KEEPINTVL   /* Maximum KEEPALIVE probe time */


#define TCPH_FLAGS(phdr)  (((u8*)phdr)[13]&TCP_FLAGS)

/*
 *  You must pull tcphdr already
 */
static inline int TCP_TCPLEN(struct sk_buff *skb)
{
	return skb->tcp_data_len;
}


static inline void TCP_TCPLEN_SET(struct sk_buff *skb, int len)
{
	skb->tcp_data_len = len+skb->h.th->fin+skb->h.th->syn;
}

enum tcp_state 
{
	CLOSED      = 0,
	LISTEN      = 1,
	SYN_SENT    = 2,
	SYN_RCVD    = 3,
	ESTABLISHED = 4,
	FIN_WAIT_1  = 5,
	FIN_WAIT_2  = 6,
	CLOSE_WAIT  = 7,
	CLOSING     = 8,
	LAST_ACK    = 9,
	TIME_WAIT   = 10
};
void tcp_pcb_free(struct tcp_pcb *pcb);

/*
 * TCP EVENTS
 */
#define TCP_CAN_SND 0x1
#define TCP_CAN_RCV 0X2
#define TCP_PEER_OPENED 0X4

#define TF_ACK_DELAY (u8)0x01U   /* Delayed ACK. */
#define TF_ACK_NOW   (u8)0x02U   /* Immediate ACK. */
#define TF_INFR      (u8)0x04U   /* In fast recovery. */
#define TF_RESET     (u8)0x08U   /* Connection was reset. */
#define TF_CLOSED    (u8)0x10U   /* Connection was sucessfully closed. */
#define TF_GOT_FIN   (u8)0x20U   /* Connection was closed by the remote end. */
#define TF_NODELAY   (u8)0x40U   /* Disable Nagle algorithm */

/* the TCP protocol control block */
struct tcp_pcb 
{
	/* Common members of all PCB types */
	u32 local_ip; 
	u32 remote_ip; 
	/* Socket options */  
	u16 so_options;
	/* Type Of Service */ 
	u8 tos;
	/* Time To Live */	
	u8 ttl;
	/** protocol specific PCB members */
  	struct tcp_pcb *next; /* for the linked list */
	volatile enum tcp_state state; /* TCP state */
	u8 prio;
	void *callback_arg;

	u16 local_port;
	u16 remote_port;
  
	u8 flags;

	volatile err_t  err;/* NET_ERR_RST, ERR_CLS, */

	INET_DEV_T  *out_dev;/*bind interface */
	
	/* receiver variables */
	u32 rcv_nxt;   /* next seqno expected */
	u16 rcv_wnd;   /* receiver window , the local available buffer */
  
	/* Timers */
	u32 tmr;
	u8 polltmr, pollinterval;
  
	/* Retransmission timer. */
	u16 rtime;
  
	u16 mss;   /* maximum segment size */
  
	/* RTT (round trip time) estimation variables */
	u32 rttest; /* RTT estimate in 500ms ticks */
	u32 rtseq;  /* sequence number being timed */
	s16 sa, sv; /* @todo document this */

	u16 rto;    /* retransmission time-out */
	u8 nrtx;    /* number of retransmissions */

	/* fast retransmit/recovery */
	u32 lastack; /* Highest acknowledged seqno. */
	u8 dupacks;
  
	/* congestion avoidance/control variables */
	u32 cwnd;  
	u32 ssthresh;

	/* variables for local sending */
	u32 snd_nxt;       /* next seqno to be sent */	
	u32 snd_max;       /* Highest seqno sent. */
	u32 snd_wnd;       /* peer's window for receiving, update by peer */
	u32 snd_wl1, snd_wl2; /* Sequence and acknowledgement numbers of last
							window update. */
	u32 snd_lbb;       /* Sequence number of next byte to be buffered. */

	u32 acked;      /* the size of data acked by peer */
  
	u32 snd_buf;   /* Available buffer space for sending (in bytes). */
	u8 snd_queuelen; /* Available buffer space for sending (in tcp_segs). */

  	u32 event_mask;
  
	/* These are ordered by sequence number: */
	struct sk_buff_head incoming; /* rcv */
	struct sk_buff_head unsent;   /* Unsent (queued) segments. */
	struct sk_buff_head unacked;  /* Sent but unacknowledged segments. */
	#if TCP_QUEUE_OOSEQ  
	struct sk_buff_head ooseq;    /* Received out of sequence segments. */
	#endif /* TCP_QUEUE_OOSEQ */

	#if IPSTACK_CALLBACK_API
	/* Function to be called when more send buffer space is available. */
	err_t (* sent)(void *arg, struct tcp_pcb *pcb, u16 space);
  
	/* Function to be called when (in-sequence) data has arrived. */
	err_t (* recv)(void *arg, struct tcp_pcb *pcb, struct sk_buff *p, err_t err);

	/* Function to be called when a connection has been set up. */
	err_t (* connected)(void *arg, struct tcp_pcb *pcb, err_t err);

	/* Function to call when a listener has been connected. */
	err_t (* accept)(void *arg, struct tcp_pcb *newpcb, err_t err);

	/* Function which is called periodically. */
	err_t (* poll)(void *arg, struct tcp_pcb *pcb);

	/* Function to be called whenever a fatal error occurs. */
	void (* errf)(void *arg, err_t err);
	#endif /* IPSTACK_CALLBACK_API */

	/* idle time before KEEPALIVE is sent */
	u32 keepalive;
  
	/* KEEPALIVE counter */
	u8 keep_cnt;

	/* 
	当没有完成一次数据接收时,
	如果要重传数据的话，
	则强制先发送第三次握手；
	因为第三次握手可能被丢掉,
	而一些防火墙必须完整完成三次握手,
	才能处理后面的数据
	*/
	u8 send_data_ok;/* 成功完成数据发送*/

};

struct tcp_pcb_listen {  
	/* Common members of all PCB types */
	u32 local_ip; 
	u32 remote_ip; 
	/* Socket options */  
	u16 so_options;
	/* Type Of Service */ 
	u8 tos;
	/* Time To Live */	
	u8 ttl;

	/* Protocol specific PCB members */
	struct tcp_pcb_listen *next;   /* for the linked list */
  
	/* Even if state is obviously LISTEN this is here for
	 * field compatibility with tpc_pcb to which it is cast sometimes
	 * Until a cleaner solution emerges this is here.FIXME
	 */ 
	enum tcp_state state;   /* TCP state */

	u8 prio;
	void *callback_arg;
  
	u16 local_port; 

	#if IPSTACK_CALLBACK_API
	/* Function to call when a listener has been connected. */
	err_t (* accept)(void *arg, struct tcp_pcb *newpcb, err_t err);
	#endif /* IPSTACK_CALLBACK_API */
};


#define TCP_EVENT_ACCEPT(pcb,err,ret)     \
                        if((pcb)->accept != NULL) \
                        (ret = (pcb)->accept((pcb)->callback_arg,(pcb),(err)))
#define TCP_EVENT_SENT(pcb,space,ret) \
                        if((pcb)->sent != NULL) \
                        (ret = (pcb)->sent((pcb)->callback_arg,(pcb),(u16)(space)))
#define TCP_EVENT_RECV(pcb,p,err,ret) \
                        if((pcb)->recv != NULL) \
                        { ret = (pcb)->recv((pcb)->callback_arg,(pcb),(p),(err)); } else { \
                          if (p) skb_free(p); }
#define TCP_EVENT_CONNECTED(pcb,err,ret) \
                        if((pcb)->connected != NULL) \
                        (ret = (pcb)->connected((pcb)->callback_arg,(pcb),(err)))
#define TCP_EVENT_POLL(pcb,ret) \
                        if((pcb)->poll != NULL) \
                        (ret = (pcb)->poll((pcb)->callback_arg,(pcb)))
#define TCP_EVENT_ERR(errf,arg,err) \
                        if((errf) != NULL) \
                        (errf)((arg),(err))


/* Internal functions and global variables: */
struct tcp_pcb *tcp_pcb_copy(struct tcp_pcb *pcb);
void tcp_pcb_purge(struct tcp_pcb *pcb);
void tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb);
struct tcp_pcb *tcp_alloc(u8 prio);
int tcp_reclaim_skb(void);

static inline void tcp_ack(struct tcp_pcb *pcb)
{
	if(1/*pcb->flags & TF_ACK_DELAY*/) 
	{	
		pcb->flags &= ~TF_ACK_DELAY;
		pcb->flags |= TF_ACK_NOW;
		tcp_output(pcb);
	} else 
	{
		pcb->flags |= TF_ACK_DELAY; 
	}
}

#define tcp_ack_now(pcb) (pcb)->flags |= TF_ACK_NOW; \
                         tcp_output(pcb)
int tcp_avail_snd_buf(struct tcp_pcb *pcb);

err_t tcp_send_ctrl(struct tcp_pcb *pcb, u8 flags);
err_t tcp_enqueue(struct tcp_pcb *pcb, void *dataptr, u16 len,
    u8 flags, u8 copy,
                u8 *optdata, u8 optlen);

void tcp_rst(u32 seqno, u32 ackno,
       u32 local_ip, u32 remote_ip,
       u16 local_port, u16 remote_port);

void tcp_reset(struct tcp_pcb *pcb);

u32 tcp_next_iss(void);

void tcp_keepalive(struct tcp_pcb *pcb);

int tcp_check_event(struct tcp_pcb *pcb, u32 mask);

void tcp_link_change_notify(INET_DEV_T *dev, u32 old_ip, int err_code);

extern struct tcp_pcb *tcp_input_pcb;
extern u32 tcp_ticks;

#if TCP_DEBUG 
void tcp_debug_print(struct tcphdr *tcphdr);
void tcp_debug_print_flags(u16 flags);
void tcp_debug_print_state(enum tcp_state s);
void tcp_debug_print_pcbs(void);
s16 tcp_pcbs_sane(void);
#else
#  define tcp_debug_print(tcphdr)
#  define tcp_debug_print_flags(flags)
#  define tcp_debug_print_state(s)
#  define tcp_debug_print_pcbs()
#  define tcp_pcbs_sane() 1
#endif /* TCP_DEBUG */

#if 1
#define tcp_timer_needed()
#else
void tcp_timer_needed(void);
#endif

/* The TCP PCB lists. */
union tcp_listen_pcbs_t 
{ 
	/* List of all TCP PCBs in LISTEN state. */
	struct tcp_pcb_listen *listen_pcbs; 
	struct tcp_pcb *pcbs;
};
extern union tcp_listen_pcbs_t tcp_listen_pcbs;
extern struct tcp_pcb *tcp_active_pcbs;  /* List of all TCP PCBs that are in a
              state in which they accept or send
              data. */
extern struct tcp_pcb *tcp_tw_pcbs;      /* List of all TCP PCBs in TIME-WAIT. */

extern struct tcp_pcb *tcp_tmp_pcb;      /* Only used for temporary storage. */
extern struct net_stats g_tcp_stats;

#define TCP_REG(pcbs, npcb) do { \
                            npcb->next = *pcbs; \
                            *(pcbs) = npcb; \
              tcp_timer_needed(); \
                            } while(0)
#define TCP_RMV(pcbs, npcb) do { \
                            if(*(pcbs) == npcb) { \
                               (*(pcbs)) = (*pcbs)->next; \
                            } else for(tcp_tmp_pcb = *pcbs; tcp_tmp_pcb != NULL; tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                               if(tcp_tmp_pcb->next != NULL && tcp_tmp_pcb->next == npcb) { \
                                  tcp_tmp_pcb->next = npcb->next; \
                                  break; \
                               } \
                            } \
                            npcb->next = NULL; \
                            } while(0)
#endif/* _TCP_H_ */
