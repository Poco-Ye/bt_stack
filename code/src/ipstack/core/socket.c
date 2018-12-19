/*
**    Socket API
** Author:sunJH
修改历史：
080624 sunJH
SockAddrSet增加了字符串长度检查；
SockAddrSet和SockAddrGet的第三个参数port由short改为unsigned short
080710 sunJH
inet_tcp_ioctl中，bind Interface时只能在CLOSED下,
也就是说listen和connect之前才能CMD_IF_SET
080820 sunJH
inet_tcp_connect等待完成连接，正确的判断
应为pcb->state == SYN_SENT，
pcb->state != ESTABLISHED是有问题的
101118 sunJH
1.struct sock增加wait_accept
对于id>0且wait_accept的sock禁止被close;
从而避免NetAccept死循环
2.sock_get分离出另外一个函数sock_find_accept给inet_tcp_accept调用
这是比较合理的，以前sock_get设计负责而且存在隐藏性Bug
3.inet_tcp_close不能调用NetCloseSocket
应该直接调用tcp_close

2017.3.28--Added the cable's presence in inet_tcp_connect(),inet_tcp_sendto() and inet_tcp_recvfrom

**/
#include <inet/inet.h>
#include <inet/inet_softirq.h>
#include <inet/tcp.h>
#include <inet/udp.h>
#include <inet/dev.h>
#include <inet/ip.h>
#include <socket.h>

#define SOCK_DEBUG //s80_printf

#define SOCK_LISTEN 0x10 /* sock is listen */
#define SOCK_ACCEPT 0x20 /* sock wait to accept */

#define SOCK_TIMEOUT 20000/*20 sec */

struct sock;
struct inet_ops
{
	int type;
	int   (*create)(struct sock*);
	int   (*connect)(struct sock*, u32 serv_ip, u16 port);
	int   (*bind)(struct sock*, u32 bind_ip, u16 port);
	int   (*listen)(struct sock*, int backlog);
	int   (*accept)(struct sock*, u32 *p_client_ip, u16 *p_client_port);
	int   (*sendto)(struct sock*, void *buf, int size, u32 remote_ip, u16 remote_port);
	int   (*recvfrom)(struct sock*, void *buf, int size, u32 *p_remote_ip, u16 *p_remote_port);
	int   (*close)(struct sock*);
	int   (*ioctl)(struct sock*, int cmd, int arg);
};
/*
struct sock说明
type: 表示连接为TCP或UDP
id:>0表示被分配, 0表示空闲
    当为incoming connect(被动创建), id为服务器的id
    当为outcoming connect(主动创建),id全局唯一
    当D31置1表示等待accept,这时候不能调用NetCloseSocket关闭
backlog用于服务器
timeout表示用户设置的超时时间
event,主要用于临时保存Connect Event
child_num:用于Listen服务器,记录有多少connect等待accept
*/
struct sock
{
	int  type;
	int  id;/* = parent id if waiting for accept*/
	int  wait_accept;
	int  backlog;
	int  timeout;/* in ms */
	int  event;
	volatile int child_num;
	u32  flag;
	struct inet_ops *ops;
	void *data;
};

#define SOCK_SET_ACCEPT(sk) ((sk)->wait_accept=1)
#define SOCK_IS_ACCEPT(sk)  ((sk)->wait_accept!=0)
#define SOCK_CLEAR_ACCEPT(sk) ((sk)->wait_accept=0)

#define SOCK_BLOCK(sk) !((sk)->flag&NOBLOCK)
#define WAIT_EVENT(var, cmp, value, timeout) \
do{ \
	if(timeout == 0) \
		break; \
	if(timeout>0) \
	{ \
		if(timeout > INET_TIMER_SCALE) \
			timeout -= INET_TIMER_SCALE; \
		else \
			timeout = 0; \
	} \
	inet_delay(INET_TIMER_SCALE); \
}while(!((var) cmp (value)))

static struct sock sock_arr[MAX_SOCK_COUNT]={0,};
static u32 sock_id = 1;

/*
sock_get使用者:
1.NetSocket/inet_tcp_accept_cb  分配空闲的struct sock slot
参数sock_id=0
2.sock_set_id分配ID
参数sock_id为待分配的ID, accept=0
通过sock_get判断ID是否已被使用
*/
static struct sock* sock_get(int sock_id)
{
	int i;
	for(i=0; i<MAX_SOCK_COUNT; i++)
		if(sock_arr[i].id == sock_id)
			return sock_arr+i;
	return NULL;
}

/*
sock_find_accept: 找到等待accept的struct sock slot
参数sock_id为Listen ID
因此这时候需要判断child_num=0(剔除Liste sock slot)
*/
static struct sock* sock_find_accept(int sock_id)
{
	int i;
	for(i=0; i<MAX_SOCK_COUNT; i++)
		if(sock_arr[i].id == sock_id&&
			SOCK_IS_ACCEPT(sock_arr+i))
			return sock_arr+i;
	return NULL;
}

static struct sock* index_to_sk(int s)
{
	if(s<0||s>=MAX_SOCK_COUNT)
		return NULL;
	if(sock_arr[s].id == 0)
		return NULL;
	return sock_arr+s;
}

static int sk_to_index(struct sock *sk)
{
	u32 end = (u32)sk;
	u32 start = (u32)sock_arr;
	return (end-start)/sizeof(struct sock);
}

static void sock_set_id(struct sock *sk)
{
	for(sock_id;;sock_id++)
	{
		if(sock_id == 0)
			continue;
		if(sock_get(sock_id)==NULL)
			break;
	}
	sk->id = sock_id;
}

/* callback by tcp */
static int inet_tcp_connected_cb(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct sock *sk = (struct sock*)arg;
	
	if(sk == NULL)
		return NET_OK;
	if(err!=NET_OK)
	{
		sk->event |= SOCK_EVENT_ERROR;
		return NET_OK;
	}
	sk->event |= SOCK_EVENT_CONN;
	return NET_OK;
}

static void inet_tcp_err_cb(void *arg, err_t err)
{
	struct sock *sk = (struct sock*)arg;
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;
	int i;
	
	if(sk == NULL)
		return ;
	sk->event |= SOCK_EVENT_ERROR;
	if(err != NET_ERR_RST)
		return;
	if(!(sk->flag&SOCK_ACCEPT))
		return;
	/* find listen NetSocket */
	for(i=0; i<MAX_SOCK_COUNT; i++)
	{
		if(sk==(sock_arr+i))
			continue;
		if(sk->id == sock_arr[i].id&&
			sock_arr[i].child_num>0)
			break;
	}
	assert(i<MAX_SOCK_COUNT);
	/* we should free this sock,because it do not accept by user */
	pcb->state = CLOSED;/* let tcp_input free it */
	sock_arr[i].child_num--;
	sk->id = 0;/*free the NetSocket */
}

static err_t inet_tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	struct sock *sk = (struct sock*)arg;
	struct sock *child;
	
	if(sk == NULL)
		return NET_OK;
	assert(sk->id > 0);
	assert(newpcb->state == ESTABLISHED);
	if(sk->child_num>=sk->backlog)
		return NET_ERR_MEM;
	child = sock_get(0);
	if(!child)
	{
		return NET_ERR_MEM;
	}
	memset(child, 0, sizeof(struct sock));
	child->type = sk->type;
	child->id = sk->id;
	child->flag = sk->flag;
	child->ops = sk->ops;
	child->data = newpcb;
	child->timeout = SOCK_TIMEOUT;
	child->flag |= SOCK_ACCEPT;
	tcp_arg(newpcb, child);
	tcp_err(newpcb, inet_tcp_err_cb);
	sk->child_num++;
	sk->event |= SOCK_EVENT_ACCEPT;
	SOCK_SET_ACCEPT(child);
	return NET_OK;
}

static int inet_tcp_get_event(struct sock *sk, int arg)
{
	int event;
	struct tcp_pcb *pcb = sk->data;

	if(pcb->state == LISTEN)
	{
		/* arg must be 0 or SOCK_EVENT_ACCEPT
		** when NetSocket is listen 
		**/
		if(arg == SOCK_EVENT_ACCEPT)
		{
			return sk->child_num;
		}else if(arg == 0)
		{
			/* return EVENT */
			if(sk->child_num>0)
				return SOCK_EVENT_ACCEPT;/* yes, have ACCEPT event */
			return 0;/* nothing */
		}
		return NET_ERR_ARG;/* request is bad */
	}

	/* connect NetSocket */
	if(arg == SOCK_EVENT_ERROR)
	{
		if(pcb->err)
			return pcb->err;/* Get Last Error Code */
		if(pcb->state==CLOSE_WAIT)
			return NET_ERR_CLSD;
		return 0;
	}
	else if(arg == SOCK_EVENT_WRITE)
	{
		int num = pcb->unacked.qlen+pcb->unsent.qlen;
		return TCP_SND_BUF-num*TCP_MSS;
	}else if(arg == SOCK_EVENT_READ)
	{
		/* Get Avail incoming Data */
		struct sk_buff *skb;
		int len = 0;
		inet_softirq_disable();
		for(skb=pcb->incoming.next; 
			skb!=(struct sk_buff*)&pcb->incoming;
			skb=skb->next)
		{
			len += skb->len;
		}
		inet_softirq_enable();
		return len;
	}else if(arg != 0)
	{
		return NET_ERR_ARG;
	}

	/* collect events, such as READ, WRITE or ERROR */
	event = sk->event;/* sk->event set event CONN maybe*/
	sk->event = 0;
	if(pcb->incoming.qlen>0)
	{
		event |= SOCK_EVENT_READ;
	}
	if(pcb->state == ESTABLISHED)
	{
		/* we can send only if connection is ESTABLISHED */
		if(pcb->snd_buf>0)
			event |= SOCK_EVENT_WRITE;
	}else if(pcb->state==CLOSE_WAIT||pcb->err < 0)
	{
		/*
		** 如果有数据可读，
		** 则等用户读完数据后再通知error!!!
		**/
		if(pcb->incoming.qlen<=0)
		{
			event |= SOCK_EVENT_ERROR;
		}
	}
	return event;
}

static int inet_tcp_create(struct sock* sk)
{
	struct tcp_pcb *pcb;
	int err = 0;
	
	inet_softirq_disable();
	pcb = tcp_new();
	if(!pcb)
	{
		err = NET_ERR_MEM;
	}else
	{
		sk->data = pcb; 
		tcp_arg(pcb, sk);
		tcp_err(pcb, inet_tcp_err_cb);
	}
	inet_softirq_enable();
	return err;
}

static int inet_tcp_connect(struct sock* sk, u32 serv_ip, u16 port)
{
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;
	struct ip_addr ip;
	int err, i;
	assert(pcb);

	ip.addr = serv_ip;

	for(i=0; i<2; i++)
	{
		inet_softirq_disable();
		err = tcp_connect(pcb, &ip, port, 
			inet_tcp_connected_cb);
		inet_softirq_enable();
		if(err == NET_OK)
			break;
		if(err != NET_ERR_MEM)
		{
			pcb->err = err;
			sk->event |= SOCK_EVENT_ERROR;
			return err;
		}
		if(i==1)
			return NET_ERR_MEM;
		inet_softirq_disable();
		err = tcp_reclaim_skb();
		inet_softirq_enable();
		if(err>0)
			continue;
	
		//--check if the cable is plugged off,added on 2017.3.28
		if(RouteGetDefault_std() == 0)
		{
			long state;
			state=0;
			EthGet_std(NULL,NULL,NULL,NULL,&state);
			if(!(state&0x01))return NET_ERR_LINKDOWN;
		}

		inet_delay(1000);
	}

	/* wait state change */
	if(SOCK_BLOCK(sk))
	{
		int timeout = sk->timeout;
		if(timeout==0)
			timeout = -1;
		while(pcb->err==NET_OK&&pcb->state == SYN_SENT)
		{
			if(timeout==0)
				break;
			if(timeout>0)
			{
				if(timeout > INET_TIMER_SCALE)
					timeout -= INET_TIMER_SCALE;
				else
					timeout = 0;
			}
		
			//--check if the cable is plugged off,added on 2017.3.28
		    if(RouteGetDefault_std() == 0)
			{
			   long state;
				
			   state=0;
				EthGet_std(NULL,NULL,NULL,NULL,&state);
				if(!(state&0x01))return NET_ERR_LINKDOWN;
			}

			inet_delay(INET_TIMER_SCALE);
		}
		if(pcb->state == ESTABLISHED)
			return NET_OK;
		if(pcb->err != NET_OK)
			return pcb->err;
		return NET_ERR_TIMEOUT;
	}else
	{
		//--check if the cable is plugged off,added on 2017.4.10
		if(RouteGetDefault_std() == 0)
		{
			long state;
			state=0;
			EthGet_std(NULL,NULL,NULL,NULL,&state);
			if(!(state&0x01))return NET_ERR_LINKDOWN;
		}

		return 0;
	}
	return NET_OK;
}

static int inet_tcp_bind(struct sock *sk, u32 bind_ip, u16 port)
{
	struct tcp_pcb *pcb; 
	struct ip_addr ip;
	int err, i;

	ip.addr = bind_ip;

	for(i=0; i<MAX_SOCK_COUNT; i++)
	{
		if(sock_arr[i].id==0||
			sock_arr[i].type != NET_SOCK_STREAM)
			continue;
		pcb = (struct tcp_pcb*)sock_arr[i].data;
		if(pcb->local_port == port)
			return NET_ERR_USE;
	}
	pcb = (struct tcp_pcb*)sk->data;
	assert(pcb);
	inet_softirq_disable();
	err = tcp_bind(pcb, &ip, port);
	inet_softirq_enable();
	return err;
}

static int inet_tcp_listen(struct sock* sk, int backlog)
{
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;

	if(backlog<=0||backlog>5)
		return NET_ERR_ARG;

	inet_softirq_disable();
	pcb = tcp_listen(pcb);
	if(pcb)
		tcp_accept(pcb, inet_tcp_accept_cb);
	inet_softirq_enable();
	if(!pcb)
		return NET_ERR_MEM;
	sk->backlog = backlog;
	sk->data = pcb;
	sk->flag |= SOCK_LISTEN;
	return 0;
}

static int inet_tcp_accept(struct sock *sk, u32 *p_client_ip, u16 *p_client_port)
{
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;
	struct sock *child;
	
	if(pcb->state != LISTEN)
		return NET_ERR_VAL;
retry:	
	assert(sk->child_num>=0);
	if(sk->child_num == 0)
	{
		if(SOCK_BLOCK(sk))
		{
			int timeout = sk->timeout;
			if(timeout == 0)
				timeout = -1;//wait forever
			WAIT_EVENT(sk->child_num, >= , 1, timeout);
		}else
		{
			return NET_ERR_AGAIN;
		}
		if(sk->child_num<=0)
			return NET_ERR_AGAIN;
	}
	
	inet_softirq_disable();
	child = NULL;
	if(sk->child_num>0)
		child = sock_find_accept(sk->id);
	if(child)
	{
		struct tcp_pcb *pcb = (struct tcp_pcb*)child->data;

		assert(child->flag&SOCK_ACCEPT);
		child->flag &= ~SOCK_ACCEPT;
		sk->child_num--;
		SOCK_CLEAR_ACCEPT(child);
		sock_set_id(child);
		if(pcb->state != ESTABLISHED)
		{
			tcp_close(pcb);
			child->id = 0;/* free this NetSocket */
			child = NULL;
		}
	}
	inet_softirq_enable();
	if(child == NULL)
		goto retry;
	
	/* get ip and port */
	pcb = (struct tcp_pcb*)child->data;
	*p_client_ip = pcb->remote_ip;
	*p_client_port = pcb->remote_port;
	return sk_to_index(child);
}

static int inet_tcp_sendto(struct sock *sk, void *buf, int size, u32 remote_ip, u16 remote_port)
{
	int err,retry;
	u8 *p_buf;
	u16 buf_size, true_size, total_size;
	struct tcp_pcb *pcb = (struct tcp_pcb *)sk->data;

	if(size > 0xffff)
	{
		return NET_ERR_BUF;
	}
	p_buf = (u8*)buf;
	buf_size = (u16)size;

	retry = (sk->timeout>0)?sk->timeout:-1;
	total_size = 0;
	while(buf_size > 0)
	{	
		SOCK_DEBUG("-------inet_tcp_sendto---------\n");
		inet_softirq_disable();
		err = tcp_write(pcb, p_buf, buf_size, 0);
		inet_softirq_enable();
		
		//--check if the cable is plugged off,added on 2017.3.28
		if(RouteGetDefault_std() == 0)
		{
			long state;

			state=0;
			EthGet_std(NULL,NULL,NULL,NULL,&state);
			if(!(state&0x01))return NET_ERR_LINKDOWN;
		}
		if(!SOCK_BLOCK(sk))
			return err;
		if(err < 0)
		{			
			if(err != NET_ERR_MEM)
			{
				return err;
			}
		}
		if(err<=0)
		{
			/* Sleep and retry */
			do
			{
				static int count = 0;
				//--check if the cable is plugged off,added on 2017.3.28
				if(RouteGetDefault_std() == 0)
				{
					long state;
					state=0;
					EthGet_std(NULL,NULL,NULL,NULL,&state);
					if(!(state&0x01))return NET_ERR_LINKDOWN;
				}
				err = tcp_check_event(pcb, TCP_CAN_SND);
				SOCK_DEBUG("TCP Check Event %d, retry %d\n", err, retry);
				if(err<0||//find error, end of waiting
					err == 1//find the event, end of waiting
					)
					break;
				if(retry == 0)
				{
					return NET_ERR_TIMEOUT;
				}
				if(retry > 0)/* <0 means waiting forever */
				{
					if(retry > INET_TIMER_SCALE)
						retry -= INET_TIMER_SCALE;
					else
						retry = 0;
				}
				SOCK_DEBUG("-----sendto Delay %d start\n", count);
				inet_delay(INET_TIMER_SCALE);
				SOCK_DEBUG("-----sendto Delay %d end\n", count);
				count++;
			}while(1);
			continue;
		}
		/* now err is the true_size sent */
		true_size = err;
		buf_size -= true_size;
		p_buf += true_size;
		total_size += true_size;
		retry = (sk->timeout>0)?sk->timeout:-1;
	}
	return total_size;
}

static int inet_tcp_recvfrom(struct sock *sk, void *buf, int size, u32 *p_remote_ip, u16 *p_remote_port)
{
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;
	int true_size;
	int retry;

	if(p_remote_port)
		*p_remote_port = pcb->remote_port;
	if(p_remote_ip)
		*p_remote_ip = pcb->remote_ip;

	retry = (sk->timeout>0)?sk->timeout:-1;
	do
	{
		inet_softirq_disable();
		SOCK_DEBUG("tcp recvfrom retry=%d\n", retry);
		true_size = tcp_recv_data(pcb, (u8*)buf, size);
		inet_softirq_enable();
		if(true_size < 0)
			return true_size;/* error */
		//--check if the cable is plugged off,added on 2017.3.28
		if(RouteGetDefault_std() == 0)
		{
			long state;

			state=0;
			EthGet_std(NULL,NULL,NULL,NULL,&state);
			if(!(state&0x01))return NET_ERR_LINKDOWN;
		}
		if(!SOCK_BLOCK(sk))
			return true_size;/* no block, return immediately */
		if(true_size > 0)
			return true_size;
		if(retry == 0)
			return NET_ERR_TIMEOUT;
		if(retry > 0)/* <0 means waiting forever */
		{
			if(retry > INET_TIMER_SCALE)
				retry -= INET_TIMER_SCALE;
			else
				retry = 0;
		}
		inet_delay(INET_TIMER_SCALE);
	}while(1);	

	return NET_ERR_VAL;
}

static int inet_tcp_close(struct sock *sk)
{
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;

	inet_softirq_disable();
	if(pcb->state == LISTEN&&sk->child_num>0)
	{
		/* close child conn */
		int i;
		for(i=0; i<MAX_SOCK_COUNT; i++)
		{
			if(	(sock_arr+i) != sk &&
				sock_arr[i].id == sk->id )
			{
				tcp_close((struct tcp_pcb*)sock_arr[i].data);
				sock_arr[i].id = 0;
				SOCK_CLEAR_ACCEPT(sock_arr+i);
				sk->child_num--;
			}
		}
		assert(sk->child_num==0);
	}
	tcp_close(pcb);
	inet_softirq_enable();
	return NET_OK;
}

static int inet_tcp_ioctl(struct sock *sk, int cmd, int arg)
{
	struct tcp_pcb *pcb = (struct tcp_pcb*)sk->data;
	switch(cmd)
	{
	case CMD_IF_GET:
		if(pcb->state == LISTEN)
			return NET_ERR_VAL;
		if(pcb->out_dev)
			return pcb->out_dev->ifIndex;
		return NET_ERR_VAL;
	case CMD_IF_SET:
		if(pcb->state != CLOSED)
			return NET_ERR_VAL;
		{
			INET_DEV_T *dev = dev_lookup(arg);
			if(!dev)
				return NET_ERR_ARG;
			pcb->out_dev = dev;
			return NET_OK;
		}
	case CMD_EVENT_GET:
		{
			return inet_tcp_get_event(sk, arg);
		}
		break;
	/* 为P60-S1增加: CMD_SEND_FIN, CMD_GET_PCB*/
	case CMD_SEND_FIN:
		if(pcb->unacked.qlen+pcb->unsent.qlen==0)
			return 1;
		return 0;
	case CMD_GET_PCB:
		INET_MEMCPY((void*)arg, pcb, 4);
		return 0;		
	case CMD_BUF_GET:
		{
			int num;
			switch(arg)
			{
			case TCP_SND_BUF_MAX:
				return TCP_SND_BUF;
			case TCP_RCV_BUF_MAX:
				return TCP_WND;
			case TCP_SND_BUF_AVAIL:
				num = pcb->unacked.qlen+pcb->unsent.qlen;
				return TCP_SND_BUF-num*TCP_MSS;
			default:
				return NET_ERR_VAL;
			}
		}
		break;
	case CMD_KEEPALIVE_SET:
		if(arg<=0)
		{
			pcb->so_options &= ~SOF_KEEPALIVE;
			pcb->keepalive = 0;
		}else
		{
			if(arg<5000)
			{
				arg = 5000;
			}
			pcb->so_options |= SOF_KEEPALIVE;
			pcb->keepalive = (u32)arg;
		}
		return NET_OK;
		break;
	case CMD_KEEPALIVE_GET:
		if(pcb->so_options&SOF_KEEPALIVE)
		{
			return (int)pcb->keepalive;
		}else
		{
			return 0;
		}
		break;
	default:
		break;
	}
	
	return NET_ERR_VAL;
}

static struct inet_ops inet_tcp_ops =
{
	NET_SOCK_STREAM,
	inet_tcp_create,
	inet_tcp_connect,
	inet_tcp_bind,
	inet_tcp_listen,
	inet_tcp_accept,
	inet_tcp_sendto,
	inet_tcp_recvfrom,
	inet_tcp_close,
	inet_tcp_ioctl,
};

static int inet_udp_create(struct sock* sk)
{
	struct udp_pcb *pcb;
	int err = 0;
	
	inet_softirq_disable();
	pcb = udp_new();
	if(!pcb)
	{
		err = NET_ERR_MEM;
	}else
	{
		sk->data = pcb; 
	}
	inet_softirq_enable();
	return err;
}

static int inet_udp_connect(struct sock* sk, u32 serv_ip, u16 port)
{
	return NET_ERR_SYS;
}

static int inet_udp_bind(struct sock *sk, u32 bind_ip, u16 port)
{
	struct udp_pcb *pcb = (struct udp_pcb*)sk->data;
	int err;
	
	assert(pcb);
	
	inet_softirq_disable();
	err = udp_bind(pcb, bind_ip, port);
	inet_softirq_enable();
	return err;
}

static int inet_udp_listen(struct sock* sk, int backlog)
{
	return NET_ERR_SYS;
}

static int inet_udp_accept(struct sock *sk, u32 *p_client_ip, u16 *p_client_port)
{
	return NET_ERR_SYS;
}

static int inet_udp_sendto(struct sock *sk, void *buf, int size, u32 remote_ip, u16 remote_port)
{
	int err,retry;
	u8 *p_buf;
	u16 buf_size, true_size, total_size;
	struct udp_pcb *pcb = (struct udp_pcb *)sk->data;

	if(remote_ip==0||remote_port==0)
		return NET_ERR_ARG;
	if(size > 1024)
	{
		return NET_ERR_BUF;
	}
	p_buf = (u8*)buf;
	buf_size = (u16)size;

	retry = (sk->timeout>0)?sk->timeout:-1;
	total_size = 0;
	while(buf_size > 0)
	{	
		SOCK_DEBUG("-------inet_udp_sendto---------\n");
		inet_softirq_disable();
		err = udp_output(pcb, p_buf, buf_size, remote_ip, remote_port);
		inet_softirq_enable();
		if(!SOCK_BLOCK(sk))
			return err;
		if(err < 0)
		{			
			if(err != NET_ERR_MEM)
			{
				return err;
			}
		}
		if(err<=0)
		{
			/* Sleep and retry */
			if(retry == 0)
			{
				return NET_ERR_TIMEOUT;
			}
			if(retry > 0)/* <0 means waiting forever */
			{
				if(retry > INET_TIMER_SCALE)
					retry -= INET_TIMER_SCALE;
				else
					retry = 0;
			}
			inet_delay(INET_TIMER_SCALE);
			continue;
		}
		/* now err is the true_size sent */
		true_size = err;
		buf_size -= true_size;
		p_buf += true_size;
		total_size += true_size;
		retry = (sk->timeout>0)?sk->timeout:-1;
	}
	return total_size;
}

static int inet_udp_recvfrom(struct sock *sk, void *buf, int size, u32 *p_remote_ip, u16 *p_remote_port)
{
	struct udp_pcb *pcb = (struct udp_pcb*)sk->data;
	int true_size;
	int retry;

	retry = (sk->timeout>0)?sk->timeout:-1;
	do
	{
		inet_softirq_disable();
		true_size = udp_recv_data(pcb, (u8*)buf, size, p_remote_ip, p_remote_port);
		inet_softirq_enable();
		if(!SOCK_BLOCK(sk))
			return true_size;
		if(true_size > 0)
			return true_size;
		if(retry == 0)
			return NET_ERR_TIMEOUT;
		if(retry > 0)/* <0 means waiting forever */
		{
			if(retry > INET_TIMER_SCALE)
				retry -= INET_TIMER_SCALE;
			else
				retry = 0;
		}
		inet_delay(INET_TIMER_SCALE);
	}while(1);	

	return NET_ERR_VAL;
}

static int inet_udp_close(struct sock *sk)
{
	struct udp_pcb *pcb = (struct udp_pcb*)sk->data;
	inet_softirq_disable();
	udp_free(pcb);
	inet_softirq_enable();
	return NET_OK;
}

static int inet_udp_ioctl(struct sock *sk, int cmd, int arg)
{
	struct udp_pcb *pcb = (struct udp_pcb*)sk->data;
	switch(cmd)
	{
	case CMD_IF_GET:
		if(pcb->out_dev)
			return pcb->out_dev->ifIndex;
		return NET_ERR_VAL;
	case CMD_IF_SET:
		{
			INET_DEV_T *dev = dev_lookup(arg);
			if(!dev)
				return NET_ERR_ARG;
			pcb->out_dev = dev;
			return NET_OK;
		}
	}
	return NET_ERR_VAL;
}

static struct inet_ops inet_udp_ops =
{
	NET_SOCK_STREAM,
	inet_udp_create,
	inet_udp_connect,
	inet_udp_bind,
	inet_udp_listen,
	inet_udp_accept,
	inet_udp_sendto,
	inet_udp_recvfrom,
	inet_udp_close,
	inet_udp_ioctl,
};

int NetSocket_std(int domain, int type, int protocol)
{
	struct sock *sk = sock_get(0);
	int err;

	if(domain != NET_AF_INET)
		return NET_ERR_ARG;
	if(type != NET_SOCK_STREAM && type != NET_SOCK_DGRAM)
		return NET_ERR_ARG;
	if(protocol != 0)
		return NET_ERR_ARG;	

	if(!sk)
		return NET_ERR_MEM;
	memset(sk, 0, sizeof(*sk));
	switch(type)
	{
	case NET_SOCK_STREAM:
		sk->ops = &inet_tcp_ops;
		break;
	case NET_SOCK_DGRAM:
		sk->ops = &inet_udp_ops;
		break;
	default:
		sk->ops = NULL;
		break;
	}
	if(!sk->ops)
	{
		return NET_ERR_ARG;
	}
	err = sk->ops->create(sk);
	if(err != NET_OK)
	{
		/* we donot need to free NetSocket,because 
		** NetSocket id is zero
		**/
		return err;
	}
	sock_set_id(sk);
	sk->type = type;
	sk->timeout = SOCK_TIMEOUT;
	return sk_to_index(sk);
}

int NetConnect_std(int s, struct net_sockaddr *addr, socklen_t addrlen)
{
	struct sock *sk = index_to_sk(s);
	int err;
	struct net_sockaddr_in *in=(struct net_sockaddr_in*)addr;
	
	if(addrlen != sizeof(struct net_sockaddr)) return NET_ERR_ARG;
	if(!sk||!in)
		return NET_ERR_VAL;
	err = sk->ops->connect(sk, ntohl(in->sin_addr.s_addr),
				ntohs(in->sin_port)
				);
	return err;
}

int NetBind_std(int s, struct net_sockaddr *addr, socklen_t addrlen)
{
	struct sock *sk = index_to_sk(s);
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	if(!sk||addr==NULL)
		return NET_ERR_VAL;
	return sk->ops->bind(sk, ntohl(in->sin_addr.s_addr),
			ntohs(in->sin_port)
			);
}


int NetListen_std(int s, int backlog)
{
	struct sock *sk = index_to_sk(s);
	if(!sk)
		return NET_ERR_VAL;
	return sk->ops->listen(sk, backlog);
}

int NetAccept_std(int s, struct net_sockaddr *addr, socklen_t *addrlen)
{
	struct sock *sk = index_to_sk(s);
	int new_s;
	u32 ip;
	u16 port;
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	if(!sk||!addr||!addrlen)
		return NET_ERR_VAL;
	new_s = sk->ops->accept(sk, &ip, &port);
	if(new_s <0)
		return new_s;
	memset(in, 0, sizeof(*in));
	in->sin_family=NET_AF_INET;
	in->sin_port=htons((unsigned short)port);
	in->sin_addr.s_addr = htonl(ip);
	*addrlen = sizeof(*in);
	return new_s;
}

int NetSend_std(int s, void *buf/* IN */, int size, int flags)
{
	struct sock *sk = index_to_sk(s);
	int err;
	
	if(size > 10240) return NET_ERR_ARG;
	if(!sk||!buf)
		return NET_ERR_VAL;
	if(sk->type != NET_SOCK_STREAM)
		return NET_ERR_VAL;
	SOCK_DEBUG("-----------Socket Send Start--------\n");
	err = sk->ops->sendto(sk, buf, size, 0, 0);
	SOCK_DEBUG("***********Socket Send End***********\n");
	return err;
}

int NetSendto_std(int s, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t addrlen)
{
	struct sock *sk = index_to_sk(s);
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	
	if(addrlen != sizeof(struct net_sockaddr) || size > 10240) return NET_ERR_ARG;
	if(!sk||!buf||!addr)
		return NET_ERR_VAL;
	if(sk->type == NET_SOCK_STREAM)
		return NET_ERR_VAL;
	return sk->ops->sendto(sk, buf, size, 
			ntohl(in->sin_addr.s_addr),
			ntohs(in->sin_port)
			);
}


int NetRecv_std(int s, void *buf/* OUT */, int size, int flags)
{
	struct sock *sk = index_to_sk(s);

	if(!sk||!buf)
		return NET_ERR_VAL;
	if(sk->type != NET_SOCK_STREAM)
		return NET_ERR_VAL;
	return sk->ops->recvfrom(sk, buf, size, NULL, NULL);
}

int NetRecvfrom_std(int s, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t *addrlen)
{
	struct sock *sk = index_to_sk(s);
	struct net_sockaddr_in *in=(struct net_sockaddr_in*)addr;
	u32 ip;
	u16 port;
	int err;

	if(!sk||!buf||!addr||!addrlen)
		return NET_ERR_VAL;
	err = sk->ops->recvfrom(sk, buf, size, &ip, &port);
	if(err <= 0)
		return err;
	memset(in, 0, sizeof(*in));
	in->sin_family=NET_AF_INET;
	in->sin_port=htons((unsigned short)port);
	in->sin_addr.s_addr = htonl(ip);
	*addrlen = sizeof(*in);
	return err;
}

int NetCloseSocket_std(int s)
{
	if(s < 0)
		return NET_ERR_ARG;
	
	struct sock *sk = index_to_sk(s);

	if(!sk||SOCK_IS_ACCEPT(sk))
		return NET_ERR_VAL;
	sk->ops->close(sk);
	sk->id = 0;
	return NET_OK;
}

int Netioctl_std(int s, int cmd, int arg)
{
	struct sock *sk = index_to_sk(s);
	if(!sk&&
		(cmd != CMD_FD_GET)
		)
		return NET_ERR_VAL;
	switch(cmd)
	{
	case CMD_IO_SET:
		if(arg&NOBLOCK)
		{
			sk->flag = NOBLOCK;
		}else
		{
			sk->flag = 0;
		}
		break;
	case CMD_IO_GET:
		return sk->flag;
	case CMD_TO_SET:
		if(arg<=0)
			arg = 0;
		sk->timeout = arg;
		break;
	case CMD_TO_GET:
		return sk->timeout;	
	case CMD_FD_GET:
	{
		int i, free_count;
		free_count = 0;
		for(i=0; i<MAX_SOCK_COUNT; i++)
			if(sock_arr[i].id == 0)
				free_count++;
		return free_count;
	}
	default:
		return sk->ops->ioctl(sk, cmd, arg);
	}
	return NET_OK;
}

int SockAddrSet_std(struct net_sockaddr *addr, char *ip_str, unsigned short port)
{
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	u32 ip = 0;
	if(in == NULL)
		return NET_ERR_ARG;
	if(str_check_max(ip_str, MAX_IPV4_ADDR_STR_LEN)!=NET_OK)
		return NET_ERR_STR;
	memset(in, 0, sizeof(*in));
	in->sin_family=NET_AF_INET;
	in->sin_port=htons((unsigned short)port);
	if(ip_str)
	{
		ip = inet_addr(ip_str);
		if(ip==(u32)-1)
			return NET_ERR_VAL;
	}
	in->sin_addr.s_addr = ip;
	return 0;
}

int SockAddrGet_std(struct net_sockaddr *addr, char *ip_str, unsigned short *port)
{
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	u32 ip;
	
	if(addr==NULL)
		return NET_ERR_ARG;
	ip = ntohl(in->sin_addr.s_addr);
	if(ip_str)
	{
		sprintf(ip_str, "%d.%d.%d.%d",
				(ip>>24)&0xff,
				(ip>>16)&0xff,
				(ip>>8)&0xff,
				(ip)&0xff
				);
	}
	if(port)
		*port = ntohs(in->sin_port);
	return 0;
}

int router_type(int s, struct net_sockaddr *addr, socklen_t addrlen)
{
	struct sock *sk;
	struct tcp_pcb *pcb;
	struct route *rt;
	struct net_sockaddr_in *in;
	struct ip_addr ip;
	
	
	sk = index_to_sk(s);
	if(sk ==NULL)
		return -1;
	pcb = (struct tcp_pcb *)sk->data;
	in=(struct net_sockaddr_in*)addr;
	ip.addr = ntohl(in->sin_addr.s_addr);
	
	pcb->remote_ip = ip.addr;
			
	rt = ip_route(pcb->remote_ip, pcb->out_dev);	
	if(rt != NULL)
	{
		return rt->out->ifIndex;
	}
	
	return -1;
}

int tcp_peer_opened(int s)
{
	struct sock *sk;
	struct tcp_pcb *pcb;
	int i;
	
	sk = index_to_sk(s);
	
	if(sk ==NULL)
		return 0;	
	
	pcb = (struct tcp_pcb *)sk->data;

	pcb->event_mask &= ~TCP_PEER_OPENED;
	
	tcp_keepalive(pcb);
	
	for(i=0; i<500; i++)
	{
		if((pcb->event_mask & TCP_PEER_OPENED) == TCP_PEER_OPENED)
			break;
		DelayMs(10);
	}
	
	if((pcb->event_mask & TCP_PEER_OPENED) == TCP_PEER_OPENED)
	{
		return 1;
	}
	else
	{
		return 0;
	}
	
}
int SockUsed()
{
    int i,count;
    count = 0;
    for(i=0; i<MAX_SOCK_COUNT; i++)
    {
        if(sock_arr[i].id) count++;
    }
    return count;
}
int is_tcp_socket(int s)
{
    struct sock *sk;
    sk = index_to_sk(s);
    if(sk ==NULL)
    	return 0;	
    if( sk->type == NET_SOCK_STREAM)    return 1;

    return 0;
}
