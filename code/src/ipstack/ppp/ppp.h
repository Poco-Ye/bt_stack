/*
修改历史
080624 sunJH:
1.增加了对PPP Link设备的操作的定义
2.PPP_DEV_T中增加了应用选择的认证算法和协商使用的认证算法
3.声明了ppp_login、ppp_logout等接口供外部模块使用
080711 sunJH
1.注释采用英文,避免sourceinsight不能解析
2.增加了链路保持激活,主要用于cdma
3.增加了连续收到校验出错的报文统计,当达到一定数量后,
认定链路以及断开或转换到AT mode
4.增加了at mode的判断,当ppp处理at mode时,丢弃任何发送的报文
5.增加了多个供外部使用的接口
ppp_set_keepalive,ppp_set_authinfo,ppp_start_atmode,ppp_stop_atmode
6.引用外部接口tcp_conn_exist
080729 sunJH
1.在struct ppp_dev_s增加了保存用于重传应答的LCP ACK报文信息，
这样可以保证提高PPP协商的成功率，因为无线模块丢包率比较高
2.MAXCONF从10次改为20次，提高成功率
3.ppp_logout增加错误码参数，用于正确通知TCP连接断开的原因
080808 sunJH
为无线增加了PPP Prepare功能,
该功能为：没有启动PPP时可以接收报文，
避免无线拨号时丢掉报文;
ppp_login后关闭该功能;
080813 sunJH
PPP_DEV_T增加lcp_ack_retry_max域
080901 sunJH
1.去掉垃圾代码
2.PPP_DEBUG改为打开
3.去掉链表
4.对发送接收进行剥离
080917 sunJH
1. PPP与串口之间的结合为PPP_COM_OPS
该结构增加了优先级prio、
支持单向链表*next、
支持上下文信息data;
2. 增加了ppp_com_register和ppp_com_unregister接口
这两个接口是对PPP_COM_OPS单向链表操作的支持
090824 sunJH
1. 增加直接通过ethernet发送ppp报文(PPPoE)
2. 增加echo功能
3. User Event从wnet_ppp.h移入该文件
110411 xuml
1.调整MAXCONF时间，增加到40S
*/
#ifndef _PPP_H_
#define _PPP_H_

/* PPP Protocols */
#define PROTip     0x0021   /* Internet Protocol */
#define PROTcomp   0x002d   /* VJ compressed Internet Protocol */
#define PROTuncomp 0x002f   /* VJ uncompressed Internet Protocol */
#define PROTmp     0x003d   /* Multilink Protocol */
#define PROTipcp   0x8021   /* Internet Protocol Control Protocol */
#define PROTccp    0x80fd   /* Compression Control Protocol */
#define PROTlcp    0xc021   /* Link Control Protocol */
#define PROTpap    0xc023   /* Password Authentication Protocol */
#define PROTlqp    0xc025   /* Link Quality Protocol */
#define PROTchap   0xc223   /* Challenge Handshake Auth. Protocol */

/*  LCP codes */
#define LCPvend_ext 0   /* Reserved for Vendor extension */
#define LCPconf_req 1   /* Configure-Request */
#define LCPconf_ack 2   /* Configure-Acknowledge */
#define LCPconf_nak 3   /* Configure-NOT-Acknowledge */
#define LCPconf_rej 4   /* Configure-Reject */
#define LCPterm_req 5   /* Terminate-Request */
#define LCPterm_ack 6   /* Terminate-Acknowledge */
#define LCPcode_rej 7   /* Code-Reject */
#define LCPprot_rej 8   /* Protocol-Reject */
#define LCPecho_req 9   /* Echo-Request */
#define LCPecho_rep 10  /* Echo-Reply */
#define LCPdisc_req 11  /* Discard-Request */

/* LCP configuration options */
#define LCPopt_RES0 0   /* RESERVED for vemdor extensions */
#define LCPopt_MRU  1   /* Maximum-Receive-Unit */
#define LCPopt_ASYC 2   /* Async mapping (RFC 1548) */
#define LCPopt_AUTH 3   /* Authentication Protocol */
#define LCPopt_LQPT 4   /* Link Quality Protocol */
#define LCPopt_MNUM 5   /* Magic Number */
#define LCPopt_RES6 6   /* RESERVED (RFC 1548) */
#define LCPopt_PCMP 7   /* Protocol-Field-Compression */
#define LCPopt_ACMP 8   /* Address-and-Control-Field-Compression */
#define LCPopt_MRRU 17  /* MP Maximum Reconstructed Receive Unit */
#define LCPopt_SNHF 18  /* MP Short Number Header Format */
#define LCPopt_ENDD 19  /* MP Endpoint Descriminator */

/* LCP Option: Endpoint Discriminator--Class Field */
#define ENDDcf_NULL 0   /* Null class */
#define ENDDcf_LCAS 1   /* Locally Assigned Address */

/* IPCP oriented values */
/* Similar to LCP, but only codes 1 through 7 supported */
#define IPCPconf_req 1  /* Configure-Request */
#define IPCPconf_ack 2  /* Configure-Acknowledge */
#define IPCPconf_nak 3  /* Configure-NOT-Acknowledge */
#define IPCPconf_rej 4  /* Configure-Reject */
#define IPCPterm_req 5  /* Terminate-Request */
#define IPCPterm_ack 6  /* Terminate-Acknowledge */
#define IPCPcode_rej 7  /* Code-Reject */

/*  IP Control protocol codes */
#define XIPCPopt_IPAD 1 /* IP-Addresses (deprecated) */
#define IPCPopt_IPCP  2 /* IP-Compression-Protocol */
#define IPCPopt_IPAD  3 /* IP-Address */
#define IPCPopt_DNSP  129   /* Primary DNS address */
#define IPCPopt_DNSS  131   /* Secondary DNS address */

/* CHAP algorithms */
#define CHAPalg_MD5 0x05/* Algorithm for CHAP */
#define CHAPalg_MD4 0x80/* Algorithm for MS-CHAP */
#define CHAPalg_MSCHAP2 0x81 /* Algorithm for MS_CHAPv2 */

/* CHAP packet codes */
#define CHAPchallenge 1
#define CHAPresponse  2
#define CHAPsuccess   3
#define CHAPfailure   4

/* PAP packet codes */
#define PAPauth_req 1
#define PAPauth_ack 2
#define PAPauth_nak 3


/* PPP states */
#define LCPtxREQ  0x01  /* Sent a conf-req (6) */
#define LCPrxACK  0x02  /* Recieved a conf-ack (7) */
#define LCPrxREQ  0x04  /* Recieved a conf-req (-) */
#define LCPtxACK  0x08  /* Sent a conf-ack (8) */
#define LCPopen   0x0f  /* LCP Configuration complete (9) */

#define IPCPtxREQ 0x10  /* Sent a conf-req (6) */
#define IPCPrxACK 0x20  /* Recieved a conf-ack (7) */
#define IPCPrxREQ 0x40  /* Recieved a conf-req (-) */
#define IPCPtxACK 0x80  /* Sent a conf-ack (8) */
#define IPCPopen  0xf0  /* IPCP Configuration complete (9) */

#define PPPclsng  0xcc  /* PPP in closing/stopping state */
#define PPPclsd   0xee  /* PPP in closed/stopped state */
#define PPPopen   0xff  /* PPP is Open */

/* Authentication states */
#define AUTHppap  0x01  /* Peer requests PAP bit; host sends auth-req */
#define AUTHpchp  0x02  /* Peer requests CHAP bit; host gets challenge */
#define AUTHpmsc  0x04  /* Peer requests MS-CHAP bit; host gets challenge */
#define AUTHpwat  0x08  /* Peer requested authentication pending bit */
#define AUTHhpap  0x10  /* Host requests PAP bit; host gets auth-req */
#define AUTHhchp  0x20  /* Host requests CHAP bit; host sends challenge */
#define AUTHhmsc  0x40  /* Host requests MS-CHAP bit; host sends challenge */
#define AUTHhwat  0x80  /* Host requested authentication pending bit */
#define AUTHpini  0x16  /* Peer initiates authentication bits */
#define AUTHhini  0x61  /* Host initiates authentication bits */
#define AUTHwait  0x88  /* Authentication pending bits */
#define AUTHfail  0x77  /* Authentication failed value */

/* MP options */
#define MPmrru    0x02  /* MRRU and MP enable */
#define MPsnhf    0x04  /* Short header (not yet implemented) */
#define MPendd    0x08  /* Endpoint Descriminator */

/* IPCP DNS options */
#define IPCP_DNSP 0x10  /* Active Primary DNS option */
#define IPCP_DNSS 0x20  /* Active Secondary DNS option */


/*
** netp->locopts -- Options to be used in conf-req
**  Each option to be used in host's LCP configure request is encoded in
**  this by shifting the number 1 left by the code of an option for
**  each option (eg  1<<(LCPopt_MNUM) == 0x20)
*/

/* netp->remopts -- Compression options */
#define COMPpcmp  0x80  /* 1 << compress protocol field */
#define COMPacmp 0x100  /* 1 << compress address/control */

#define FLAG    0x7e    /* special flag character */
#define CTL_ESC 0x7d    /* escape character */

#define PPP_MSS  1500
#define PPP_HEADER_SPACE 6/* 2 check+1 control(0xff)+1 addr(0x03)+2 proto */

#define PPP_SND_QLEN  10
#define PPP_RCV_QLEN  10

/*
 * PPP Error Code
 *
 */
#define PPP_PASSWD    NET_ERR_PASSWD  
#define PPP_LINKDOWN  NET_ERR_LINKDOWN /* ppp link is down */
#define PPP_MODEM     NET_ERR_MODEM    /* modem is fail */

/* PPP Send flags */
#define PPP_SND_ESC 0x1
#define PPP_SND_CHECK1 0x2
#define PPP_SND_CHECK2 0x4
#define PPP_SND_END 0x8

#define PPP_BUF_FREE 0x0
#define PPP_BUF_USED 0x1
#define PPP_BUF_DOING 0x2
typedef struct ppp_buf_s
{
	struct list_head list;
	u32 status;/* 0 : free, 1: used , 2: in doing*/
	u32 timestamp;
	u32 len;
	u8  data[PPP_MSS+PPP_HEADER_SPACE+2];
}PPP_BUF_T;

#define PPP_LINK_ETH  1/* PPP Link type */

struct ppp_dev_s;
/*
** Operation Set for Data Link
** Data Link may be such as modem, GPRS, CDMA ,etc
**/
typedef struct ppp_link_ops
{
	void (*irq_disable)(struct ppp_dev_s*);
	void (*irq_enable)(struct ppp_dev_s*);
	/* irq_tx: Open TX IRQ */
	void (*irq_tx)(struct ppp_dev_s*);
	/* ready: DataLink is OK?
	**   0: ok, <0 fail
	*/
	int  (*ready)(struct ppp_dev_s*);
	/* check: DataLink is Down?
	**  0: ok, <0 fail
	*/
	int  (*check)(struct ppp_dev_s*, long ppp_state);
	/* force_down: force dataLink to down */
	void (*force_down)(struct ppp_dev_s*);
	/* irq_rx: recv a char */
	void (*irq_rx)(struct ppp_dev_s*, int cc);
}PPP_LINK_OPS;

/*
	recv_char : callback function for Recv Char
	xmit_char : callback function for Send Char
*/
typedef struct ppp_com_ops
{
	int     prio;
	void    (*recv_char)(int , void *);
	int     (*xmit_char)(void *);
	void*   data;
	struct ppp_com_ops *next;
}PPP_COM_OPS;

typedef struct ppp_dev_s
{
	int     init;/* 1: is init, 0: not init */
	u32     magic_id;
	int     state;
	int     auth_state;/* 1 ok */
	int     keep_alive_interval;
	int     next_keep_alive;
	int     chsum_err_count;/* the counter for chsum error */
	int     mss;
	int     counter;
	int     error_code;
	u32     local_addr;
	u32     mask_addr;
	u32     remote_addr;
	u32     primary_dns;
	u32     second_dns;
	u8      id;
	int     recv_id;/* the last remote id recv */
	int     recv_prot;/* the last protocol pkt recv */
	int     ipopts;/* ip configure options */
	int     opt4;/* auth state */
	int     locopts;/* local LCP options */
	int     remopts;/* remote LCP options */
	int     raccm;/* remote ACCM */
	int     ifType;/* always 23 */
	char    userid[100];
	char    passwd[100];
	int     timems;
	int     echo_time;
	int     comm;/* the physical COM Number */

	int      link_type; /*link_type */

	/* */
	PPP_COM_OPS com_ops;
	/* */
	PPP_LINK_OPS *link_ops; 

	/* cache for MSCHAPv2*/
	char    mschapv2_cache[50];

	/* IP Device */
	INET_DEV_T  *inet_dev;

	/* App 设置的认证算法*/
	unsigned long init_authent;
	/* negotiate auth */
	unsigned long neg_authent;

	/* send cache */
	u32       ACCM;
	u32       snd_flags;
	u8        *bufout;
	int       chsout;
	int       nxtout;
	PPP_BUF_T *snd_buf;

	u32       last_tx_timestamp;/* 上次发送的时间戳 */
	u32       last_rx_timestamp;/* 上次接收时间戳 */

	/*   recv cache */
	u8         *bufin;
	u8         lastin;
	PPP_BUF_T  *rcv_buf;

	/* timestamp for end */
	u32      timestamp;

	u32     magic_id2;
	PPP_BUF_T  rcv_bufs[PPP_RCV_QLEN];
	PPP_BUF_T snd_bufs[PPP_SND_QLEN];

	int    at_cmd_mode;

#define PPP_USER_REQUEST_LOGIN  0x1
#define PPP_USER_REQUEST_LOGOUT 0x2
	int user_request;/* 0: none, 1: login, 2: logout */
	u32 trigger_time;/* do request in the trigger time */

	/* LCP ACK packet for retran */
	u8  lcp_ack_pkt[2048];
	int lcp_ack_pkt_size;
	int lcp_ack_retry;

	int prepare;/* can RX, but not start PPP; 
                             this is only for Wireless
                           */
	int lcp_ack_retry_max;

	/* Link Echo Request */
	u32   echo_state;
	u32   echo_timeout;
	u32   echo_retry;
}PPP_DEV_T;

/* Echo 功能状态 
为了支持链路状态检查功能，因此增加echo request;
但是考虑到对方服务器可能不支持echo request，
因此login成功后作为echo_detect状态检查对方是否支持,
1. 当echo_request发送达到一定数量而对方没有回复则认为
   对方不支持echo(echo_disable);
2. 当对方回复echo_request,则对方支持(echo_enable);
   后续在一定时间没有收到对方任何报文,则启动echo_request；
*/
enum
{
	ECHO_DETECT=1,/* 检测对方是否支持echo功能 */
	ECHO_DISABLE,/* 对方不支持echo功能 */
	ECHO_ENABLE,/* 对方支持echo功能*/
};
#define ECHO_RETRY_MAX  10   /* 最大重传次数 */
#define ECHO_RETRY_TIME 500  /* 重试间隔 */
#define ECHO_START_TIME 30000/* 30s没有收到对方报文 */

/*
** PPP has a maximum receive unit.  It defaults to the value of MAXBUF in
** local.h - MESSH_SZ - HOFF.  Changing MAXBUF will allow you to control
** what PPP negotiates.  PPP assumes an MRU of 1500 (RFC 1661), which is the
** same as the easyNET default.  If the value of MAXBUF drops much below 1500
** it would be a good idea to enable this option to inform the peer that
** it ought not to send smaller packets..
**
** NOTE: The PPP protocol requires that a host always be able to receive
** a 1500 byte packet regardless of the MRU.  Because the easyNET MRU is
** fixed, easyNET PPP will simply ignore a 'nak' with a larger value than
** is possible.  This is probably incorrect but will prevent an immediate
** link failure.
**
** 1    negotiate with easyNET's MRU
** 0    disable negotiations (host/remote assume default)
*/
#define PPP_MRU 0 


/*
** The magic number is used to detect anomalies in the link.  Leave it
** on unless you have a good reason not to.
**
** 1    Use magic number
** 0    Don't use magic number
*/
#define MAGICNUM 0


/*
** RFC 1662 defines the HDLC framing PPP uses.  It allows a host to escape
** some control characters and not others.  If this option is enabled, easyNET
** will attempt to have the RACCM characters escaped.  Reducing the number
** of escaped characters increases through-put but may reduce reliability.
** Escaping no characters implies an ACCM of 00000000; escaping all
** characters implies an ACCM of ffffffff.  By default, PPP assumes that
** all characters are escaped (ffffffff).
**
** 0    Do not actively negotiate host receive character map
** 1    Actively negotiate host receive character map
*/
#define ASYNC 1

/*
** Define the host receive ACCM
**
** Each bit in this 32-bit field represents a character < 0x20 to
** escape.  If the bit corresponding to the character is true, the
** character must be escaped.
*/
//#define RACCM 0x00000000
#define RACCM 0x000a0000 /*For XON/XOFF escaped */

/*
** RFC 1877 includes extensions for PPP that allow configuration of DNS
** addresses during IPCP.  This is not recommended except for dedicated
** devices with minimal application functionality.  Note that DNS must
** be set to 1 or 2 in local.h.
**
** Typically the application will configure DNS addresses through
** netconf.c using the DNSVER flag.  If bit 0 is set, the host will make a
** request to the peer asking to use the configured primary and secondary
** DNS addresses.  If bit 1 is set and the peer makes a request for DNS
** addresses, we will send them the addresses already configured.  If no
** address is configured by the application, it does not make sense to
** set bit 1.
**
** bit 0    Actively request DNS information
** bit 1    Passively suggest DNS information
*/
#define IPCP_DNS 1 

/*
** Value in restart counter for TO+ until TO- for IPCP/LCP conf-req packets.
** See RFC 1661 for an explanation.
**
** Number of timeouts with resends allowed.
*/
#define MAXCONF 40 /*20*/

#define TOUTMS 0
#define ECHO_TOUTMS 0
#define IDLE_TOUT 0
/*
** Set this value to enable dynamic timeout changes.  This means that the
** timeout period will grow up to TOUTMS as more timeouts occur.
**
** For line speeds less than about 19200 bps, it is recommended that this
** be turned off in order to avoid race conditions.  As an alternative,
** TOUTMS may be increased.
**
** 1 = Shift TOUTMS right by restart counter/2
** 0 = TOUTMS is the only PPP timeout value
*/
#define TOUT_GROW 1/*0*/

/*
** Bits for compression code present.  Enabling either or both of these
** should increase the link throughput.
**
** bit 0    protocol, address/control fields
*/
#define COMPRESSION 0  


void ppp_start(PPP_DEV_T *ppp);
void ppp_closed(PPP_DEV_T *ppp);

err_t PPPLogin(char *name, char *passwd, long auth, int timeout);

void PPPLogout(void);

#define PPP_DEBUG 0
#if PPP_DEBUG
void buf_printf(const unsigned char *buf, int size, const char *descr);		      
#define ppp_printf s80_printf
#else
#define buf_printf
#define ppp_printf
#endif

#define PPP_PKT_DEBUG 0


int ppp_login(PPP_DEV_T *ppp, char *name, char *passwd, long auth, int timeout);
void ppp_logout(PPP_DEV_T *ppp, int err);
int ppp_check(PPP_DEV_T *ppp);
void ppp_inet_init(PPP_DEV_T *ppp, INET_DEV_T *dev, char *name, int if_index);
int ppp_dev_init(PPP_DEV_T *ppp);
void ppp_timeout(PPP_DEV_T *ppp, unsigned long mask);
void ppp_set_keepalive(PPP_DEV_T *ppp, int interval);
int ppp_set_authinfo(PPP_DEV_T *ppp, char *name, char *passw);
void ppp_start_atmode(PPP_DEV_T *ppp);
void ppp_stop_atmode(PPP_DEV_T *ppp);
void ppp_prepare(PPP_DEV_T *ppp);
void ppp_com_register(PPP_COM_OPS **head, PPP_COM_OPS *node);
void ppp_com_unregister(PPP_COM_OPS **head, PPP_COM_OPS *node);

extern int tcp_conn_exist(INET_DEV_T *dev);

#define MAX_COMM_NUM 5
extern PPP_COM_OPS *ppp_comm[MAX_COMM_NUM];

/* 用户请求事件*/
typedef enum
{
	USER_EVENT_NONE   = 0x00,
	USER_EVENT_LOGIN  = 0x01,
	USER_EVENT_LOGOUT = 0x02,
}USER_EVENT;

#endif/* _PPP_H_ */
