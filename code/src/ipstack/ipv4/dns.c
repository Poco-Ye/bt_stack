/*
 * Domain Name System
 * Author: sunJH
 * Date:2007-08-07
 * Version:1.0
  修改历史
1.修正处理流程错误：当收到reply code为错误代码时，不应该重试而应该退出
2.inet_delay采用INET_TIMER_SCALE而不是1
080624 sunJH:
1.DnsResolve增加了字符串长度检查；
2.dns_request中取消IP地址解析成域名的功能；
3.dns_request查找可用route时，由只查找modem，
变为也查找cdma和gprs等；
4.dns_request发现如果dns->err存在，则返回相应错误码；
5.dns_input把header->flags中的状态码转换成错误码保存在dns->err；
081106 sunJH:
1. add DnsResolveExt function, which can set TIMEOUT Time
100827 sunJH
dns_request没有对局部变量dns_filter进行初始化，
在IP2K上面暴露出来，现象为成功率不高(80%左右)；
而S80(以太网)为100%;
通过查找,发现dns_filter没有初始化从而导致
udp_filter_register没有执行成功,
造成不能正常收到dns报文;

2016.11.22--Incremented the buffer size of dns name,Revised the pointer and border errors in dns_pkt_to_name()
*/

#include <string.h>
#include <inet/inet.h>
#include <inet/skbuff.h>
#include <inet/udp.h>
#include <inet/inet_softirq.h>

#define dns_debug //printk

#define MAXDELAY 3000      /* maximum delay in retry loop */

#define uchar unsigned char
struct dns_header /* DNS Header structure */
{             
	u16 id;          /* identifier */
	u16 flags;       /* flags */
	u16 no_q;        /* number of questions */
	u16 no_ans;      /* number of answer RRs */
	u16 no_auth;     /* number of authority RRs */
	u16 no_add;      /* number of additional RRs */
	//u8  data[1];     /* variable-format data */ not supported by P60-s1
};

#define DNS_NAME_SIZE 255/* the max limit */

struct dns_response
{
	u16 id;
	int type;/* DNS type */
	int err;
	u32 tm;/* tiemstamp receive this packet */
	u32 ttl;/* Time To Live */
	u8  name[DNS_NAME_SIZE+1];/* request,incremented on 2016.11.21 */
	u8  bRsp;/* Rsp Flag: 1 has rsp, 0 no rsp */
	u8  resp[DNS_NAME_SIZE+1];/* response,incremented on 2016.11.21 */
	u8 iplen;
};

static struct dns_response dns_resp={0,};

#define DNS_HEADER_SIZE 12            /* header size */
enum
{
	DNS_TYPE_A=1,/*host address*/
	DNS_TYPE_PTR=12,/* a domain name pointer */
	DNS_TYPE_MX=15,/* mail exchange */
};
#define DNS_CLASS_IN 0x1 /* Internet */

static int dns_response_flags(u16 flags)
{
	if((flags&0x8000)!=0x8000||/* is response? */
		(flags&0x7800)!=0 ||/* opcode is standard query?*/
		0//(flags&0xf)!=0/* Reply code is no error ?*/
		)
		return -1;
	return NET_OK;
}

/* DNS NAME in pkt is such as
** len1+string1+len2+string2+0
** here, len1 is the len of string1
**/
static uchar *dns_pkt_to_name(uchar *start,uchar *buf,  		
	int *buf_size, /*	IN */
    
 uchar *name, /*IN */
		
	int name_size	/* IN	*/)
{
	int count, seg_size;
	uchar *offset = NULL;
	char compress_found=0;
	
	count = 0;
	name[0] = 0;
	while (*buf_size > 0) {//revised on 2016.11.22
	        if((buf[0] & 0xC0) == 0xC0){	
			compress_found=1;
			offset = start + buf[1];
			(*buf_size) -= 2;
			buf += 2;
			while(name_size > 0){
				seg_size = offset[0];
				if(seg_size == 0)
					goto OUT;
				if(count > 0){
					*name ++ = '.';
					name_size--;
				}
				 if(seg_size > name_size){//revised on 2016.11.22
					return NULL;
				}
				memcpy(name,offset+1,seg_size);
				name +=	seg_size;
				name_size -= seg_size;
				offset +=	seg_size + 1;
				count++;
			}
		}else{
			seg_size = buf[0];
			if (seg_size ==	0) {
				name[0] = 0;
				(*buf_size)--;//revised on 2016.11.22
				if(!compress_found)buf++;
				break;
			}
			 if (count	>	0) {				
				*name++ = '.';				
				name_size--;			
			}						
			if ((seg_size + 1) > *buf_size || seg_size > name_size)//revised on 2016.11.21
				return NULL;	/* buf error */			
			memcpy(name, buf + 1,	seg_size);			
			name +=	seg_size;						
			name_size -= seg_size;			
			*buf_size	-= (seg_size + 1);			
			buf += seg_size + 1;			
			count++;		
		}	
	}
OUT:	
	return buf;
}

/* scan a short value from packet */
#define DNS_PKT_SHORT(buf, len, value) \
do{ \
	if(len < 2) \
		goto drop; \
	INET_MEMCPY(&value, buf, 2);\
	value = ntohs(value); \
	buf += 2; \
	len -= 2; \
}while(0)

/* scan a long value from packet */
#define DNS_PKT_LONG(buf, len, value) \
do{ \
	if(len < 4) \
		goto drop; \
	INET_MEMCPY(&value, buf, 4); \
	value = ntohl(value); \
	buf += 4; \
	len -= 4; \
}while(0)

static void dns_input(u32 arg, struct sk_buff *skb)
{
	int count;
	struct dns_response *dns = (struct dns_response*)arg;
	struct dns_header  *header = (struct dns_header*)skb->data;
	uchar   *buf;
	uchar *respstr;
	static uchar   name[DNS_NAME_SIZE+1];//incremented on 2016.11.21
	int i, len, j;
	u16 value, type, clss;
	u32 ttl;
	u8 ipnum;
	if(dns->id==0){
		dns_debug("dns->id==0\r\n");
		goto drop;
	}
	if(skb->len <= DNS_HEADER_SIZE)
	{
		dns_debug("skb->len <= DNS_HEADER_SIZE\r\n");
		goto drop;
	}
	dns_debug("head: \r\n");
	
	header->id     = ntohs(header->id);
	header->flags  = ntohs(header->flags);
	header->no_q   = ntohs(header->no_q);
	header->no_ans = ntohs(header->no_ans);
	
	dns_debug("0x%x, ", header->id);
	dns_debug("0x%x, ", header->flags);
	dns_debug("0x%x, ", header->no_q);
	dns_debug("0x%x, ", header->no_ans);
	dns_debug("head end. \r\n");

	dns_debug("DNS skb begin:\r\n");

	for(i=0;i<skb->len;i++)
		dns_debug("0x%x, ", *(skb->data+i));
	
	dns_debug("\r\nDNS skb end.\r\n");
	
	if(dns_response_flags(header->flags)!=NET_OK)
	{
		dns_debug("header->flags !=NET_OK\r\n");
		goto drop;
	}
	if(header->id != dns->id)
	{
		dns_debug("header->id != dns->id\r\n");
		goto drop;
	}
	dns->bRsp = 1;
	if(header->id == dns->id &&
		(header->flags&0xf)!= 0)
	{
		switch(header->flags&0x0f)
		{
		case 3:
		default:
			dns->err = NET_ERR_DNS;
		}
		dns_debug("!header->flags&0x0f\r\n");
		goto drop;
	}
	if(header->no_ans == 0)
	{
		dns_debug("header->no_ans == 0\r\n");
		goto drop;
	}
	if(dns->iplen < 20)
		ipnum = 1;
	else
		ipnum = dns->iplen/20;
	buf = (uchar*)(header+1);
	len = skb->len-DNS_HEADER_SIZE;	
	memset(name,0,sizeof(name));
	for(i=0; i<header->no_q; i++)
	{	
		dns_debug("header->no_q:%d\r\n",i);
		buf = dns_pkt_to_name((uchar *)(skb->data), buf, &len, name, DNS_NAME_SIZE);
		if(!buf){
			dns_debug("dns->0\r\n");
			goto drop;
			}
		dns_debug("name1:%s, name2:%s\r\n",name,dns->name);
		if(strcmp(name, (char*)dns->name)!=0)
		{
			dns_debug("dns->1\r\n");
			goto drop;
		}
		dns_debug("type:0x%x, ", dns->type);
		DNS_PKT_SHORT(buf, len, type);/* TYPE */
		dns_debug("-> 0x%x. \r\n",type);
		if(type != dns->type){
			dns_debug("dns->2 \r\n");
			goto drop;
			}
		dns_debug("clss:  ");
		DNS_PKT_SHORT(buf, len, clss);/* CLASS */
		dns_debug(" ->0x%x. \r\n",clss);
		if(clss != DNS_CLASS_IN){
			dns_debug("dns->3\r\n");
			goto drop;
			}
	}
	respstr = dns->resp;
	for(i=0; i<header->no_ans; i++)
	{
		if((buf[0]&0x000000c0) == 0xc0){
			buf += 2;/* skip 2 chars, such as 0xc0,0x0c */
			len -= 2;
		}
		else{//name pointer
			buf = dns_pkt_to_name((uchar *)skb->data,buf, &len, name, DNS_NAME_SIZE);
			if(NULL == buf){
				dns_debug("name pointer: DNS server return name error\r\n");
				goto drop;
			}		
		}
		dns_debug("< len:0x%x >.\r\n", len);
		
		dns_debug("<TYPE: ");
		DNS_PKT_SHORT(buf, len, type);/* TYPE */
		dns_debug("0x%x > \r\n",type);

		dns_debug("<CLASS: ");
		DNS_PKT_SHORT(buf, len, clss);/* CLASS */
		dns_debug("0x%x > \r\n",clss);

		dns_debug("<TTL: ");
		DNS_PKT_LONG(buf, len, ttl);/* TTL */
		dns_debug("0x%x > \r\n",ttl);

		dns_debug("<Data Length: ");
		DNS_PKT_SHORT(buf, len, value);/* Data Length */
		dns_debug("0x%x > \r\n",len);
		
		if(type == dns->type && clss == DNS_CLASS_IN)
		{
			switch(type)
			{
			case DNS_TYPE_A:
				if(value != 4)
				{
					dns_debug("dns->4\r\n");
					goto drop;
				}
				
				sprintf((char*)respstr, "%d.%d.%d.%d",
					buf[0],
					buf[1],
					buf[2],
					buf[3]
					);
				dns_debug("DNS_TYPE_A :%d.%d.%d.%d. \r\n",buf[0],
					buf[1],
					buf[2],
					buf[3]);
				ipnum--;
				if(ipnum > 0)
					respstr = respstr+20;
				break;
			case DNS_TYPE_PTR:
				if(value > DNS_NAME_SIZE-1)
				{
				    dns_debug("dns->5\r\n");
					goto drop;
				}
				/*
				** 把信息转换成形如a.b.c个数,
				** 信息采用Len(1byte)+value(len bytes)格式
				**/
				j = 0;
				while(value>0)
				{
					if(buf[0]+1>value)
					{
						//buf[0]islen,and 1 is for buf[0]
						//bad packet!!!!!!
						dns_debug("dns->6\r\n");
						goto drop;
					}
					if(buf[0]==0)
						break;
					if(j>0)
					{
						dns->resp[j++]='.';
					}
					INET_MEMCPY(respstr+j, buf+1, buf[0]);
					j += buf[0];
					value -= buf[0]+1;
					buf += buf[0]+1;
				}
				respstr[j] = 0;
				ipnum = 0;
				dns_debug("DNS_TYPE_PTR\r\n");
				break;
			}
			dns->ttl = ttl;
			dns->tm = inet_jiffier;
			dns_debug("dns->7\r\n");
			if(ipnum == 0)
				goto drop;
		}
		buf += value;
		len -= value;
		dns_debug("i->%d:: len->0x%0x\r\n", i, len);
	}
	dns_debug("\r\n++dns++\r\n");
drop:
	//skb_free(skb);
	dns_debug("\r\n--dns--\r\n");
}

extern INET_DEV_T *g_default_dev;
static int dns_request(char *fullname/* IN */,u8 iplen)
{
	int i1, i2,i3,i4,len, reqtyp;
	int inidelay, delay;
	u8  cc, *cp, *cp2, *cp3;
	u16 xid;
	struct dns_header *DNSstr;
	struct udp_pcb *pcb; 
	struct dns_response *dns = &dns_resp;
	INET_DEV_T   *dev;
	UDP_FILTER_T dns_filter;
	u8  buf[512];/* for dns packet */

	dev = g_default_dev;
	/*
	** Assert the Net Link is OK
	**
	**/
	if(dev == NULL)
		dev = dev_lookup(ETH_IFINDEX_BASE);
	if(dev==NULL||!DEV_FLAGS(dev, FLAGS_UP))
	{
		int i=PPP_IFINDEX_BASE;
		for(;i<MAX_PPP_IFINDEX; i++)
		{
			dev = dev_lookup(i);
			if(dev == NULL)
				continue;
			if(DEV_FLAGS(dev, FLAGS_UP))
				break;
			dev = NULL;
		}
		if(dev == NULL)
			return NET_ERR_IF;
	}

	if(dev->dns==0)/* no dns server ? */
		return NET_ERR_RTE;
    
	/*  
	**  If "name" is an IP address, resolve IP address to name
	**  If name contains alphabet characters, resolve name to IP address
	*/
	reqtyp = DNS_TYPE_A;
	if (sscanf(fullname, "%d.%d.%d.%d", &i1, &i2, &i3, &i4) == 4)     
	{
		#if 1
		i1 = inet_addr(fullname);
        if(i1>0)
            return NET_ERR_VAL;//取消IP地址映射到域名的功能
		#else
		reqtyp = DNS_TYPE_PTR;
		if(i1>255||i2>255||i3>255||i4>255)
			return NET_ERR_VAL;
		sprintf(tbuf,"%d.%d.%d.%d.in-addr.arpa", i4, i3, i2, i1);
		fullname = tbuf;
		#endif
	}
	else if ((cp = (unsigned char*)strchr(fullname, '@')) != 0)
	{
		reqtyp = DNS_TYPE_MX;
		fullname = (char*)cp + 1;
	}
	if(strcmp(fullname, (char*)dns->name)==0&&
		reqtyp == dns->type &&
		dns->tm+dns->ttl >= inet_jiffier)
	{
		return NET_OK;
	}

	/* create a temp udp pcb */
	inet_softirq_disable();
	pcb = udp_new();
	inet_softirq_enable();
	if(!pcb)
	{
		return NET_ERR_MEM;
	}
	pcb->out_dev = dev;/* let pcb bind the dev we select */
	
	xid = (u16)inet_jiffier;
	DNSstr = (struct dns_header *)buf;

	inidelay = (inet_jiffier & 0xff) + 500;

	memset(dns, 0, sizeof(struct dns_response));
	dns->id = xid;
	dns->type = reqtyp;
	dns->iplen = iplen;
	strcpy((char*)dns->name, fullname);

	memset(&dns_filter, 0, sizeof(dns_filter));//Init
	dns_filter.arg = (u32)dns;
	dns_filter.source = DNS_PORT;
	dns_filter.dest = 0;
	dns_filter.fun = dns_input;
	inet_softirq_disable();
	udp_filter_register(&dns_filter);
	inet_softirq_enable();
    
	/* Create and write message, wait answer, several times */
	/*
	** 重试次数:3
	** TimeOut时间: 4sec
	**/
	for (delay=inidelay; delay<MAXDELAY*2; delay<<=1)
	{
		memset(DNSstr, 0, sizeof(struct dns_header));
		DNSstr->id = htons(xid);
		DNSstr->flags = htons(0x0100);
		DNSstr->no_q = htons(1);
		/* Full name is encoded as length,part,length,part,,,0 */
		cp = (unsigned char*)fullname;
		cp3 = (unsigned char*)(DNSstr+1);
		cp2 = cp3 + 1;
		for (;;)
		{
			cc = *cp++;
			if (cc == '.' || cc == 0) 
			{
				*cp3 = cp2 - cp3 -1;
				cp3 = cp2;
			}
			*cp2++ = cc;
			if (cc == 0)
				break;
		}
		*cp2++ = 0, *cp2++ = reqtyp;	/* query type */
		*cp2++ = 0, *cp2++ = 1; 		/* query class */
		len =  (long)cp2 - (long)(char *)DNSstr;
		for(;;)
		{
			int ret;
			inet_softirq_disable();
			ret = udp_output(pcb, buf, len, dev->dns, DNS_PORT);
			inet_softirq_enable();
			if(ret<0)
				goto out;
			if(ret==len)
			{
				break;
			}
			inet_delay(100);
		}
		for(i1=0; i1<delay; i1+=INET_TIMER_SCALE)
		{
			inet_delay(INET_TIMER_SCALE*3);
			if(dns->ttl>0||dns->err)
			{
				goto out;
			}
		}
	}

out:
	inet_softirq_disable();
	udp_free(pcb);
	udp_filter_unregister(&dns_filter);
	inet_softirq_enable();
	if(dns->ttl > 0)
	{
		return NET_OK;
	}
	if(dns->err)
		return dns->err;
	return NET_ERR_TIMEOUT;
}

int DnsResolve_std(char *name/*IN*/, char *result/*OUT*/, int len)
{
	int err;
	int i;
	if(name == NULL || len < 16 ||
		name[0] == 0)
		return NET_ERR_VAL;
	if(result == NULL )
		return NET_ERR_VAL;
	if(str_check_max(name, DNS_NAME_SIZE)!=NET_OK)
		return NET_ERR_STR;

    len = 16;       //only return "one ipv4 address"
	result[0] = 0;
	dns_debug("req name:%s\r\n", name);
	err = dns_request(name,len);
	dns_debug("dns err:%d\r\n", err);
	
	if(err != NET_OK)
		return err;
	if((u32)len < strlen((char*)dns_resp.resp)+1)
		return NET_ERR_BUF;
	
	strcpy(result,(char *)dns_resp.resp);
	return NET_OK;
}

u32 get_jiffier(void)
{
    return inet_jiffier;
}

static int dns_request_ext(char *fullname/* IN */,u8 iplen,int timeout)
{
	int i1, i2,i3,i4,len, reqtyp;
	int inidelay, timer_v,timer_end_v;
	u8  cc, *cp, *cp2, *cp3;
	u16 xid;
	struct dns_header *DNSstr;
	struct udp_pcb *pcb; 
	struct dns_response *dns = &dns_resp;
	INET_DEV_T   *dev;
    UDP_FILTER_T dns_filter;	
	u8  buf[512];/* for dns packet */
    int retry_cnt = 0;
    int ret;
    
	dev = g_default_dev;
	/*
	** Assert the Net Link is OK
	**
	**/
	if(dev == NULL)
		dev = dev_lookup(ETH_IFINDEX_BASE);
	if(dev==NULL||!DEV_FLAGS(dev, FLAGS_UP))
	{
		int i=PPP_IFINDEX_BASE;
		for(;i<MAX_PPP_IFINDEX; i++)
		{
			dev = dev_lookup(i);
			if(dev == NULL)
				continue;
			if(DEV_FLAGS(dev, FLAGS_UP))
				break;
			dev = NULL;
		}
		if(dev == NULL)
			return NET_ERR_IF;
	}

	if(dev->dns==0)/* no dns server ? */
		return NET_ERR_RTE;
    
	/*  
	**  If "name" is an IP address, resolve IP address to name
	**  If name contains alphabet characters, resolve name to IP address
	*/
	reqtyp = DNS_TYPE_A;
	if (sscanf(fullname, "%d.%d.%d.%d", &i1, &i2, &i3, &i4) == 4)     
	{
        i1 = inet_addr(fullname);
        if(i1>0)
            return NET_ERR_VAL;//取消IP地址映射到域名的功能
	}
    else if ((cp = (unsigned char*)strchr(fullname, '@')) != 0)
	{
		reqtyp = DNS_TYPE_MX;
		fullname = (char*)cp + 1;
	}

	if(strcmp(fullname, (char*)dns->name)==0&&
		reqtyp == dns->type &&
		dns->tm+dns->ttl >= inet_jiffier)
	{
		return NET_OK;
	}    

	/* create a temp udp pcb */
	inet_softirq_disable();
	pcb = udp_new();
	inet_softirq_enable();
	if(!pcb)
	{
		return NET_ERR_MEM;
	}
	pcb->out_dev = dev;/* let pcb bind the dev we select */
	
	xid = (u16)inet_jiffier;
	DNSstr = (struct dns_header *)buf;

	inidelay = (inet_jiffier & 0xff) + 500;

	memset(dns, 0, sizeof(struct dns_response));
	dns->id = xid;
	dns->type = reqtyp;
	dns->iplen = iplen;
	strcpy((char*)dns->name, fullname);
    memset(&dns_filter, 0, sizeof(dns_filter));//Init
	dns_filter.arg = (u32)dns;
	dns_filter.source = DNS_PORT;
	dns_filter.dest = 0;
	dns_filter.fun = dns_input;
    inet_softirq_disable();
	udp_filter_register(&dns_filter);
	inet_softirq_enable();

    timer_end_v = get_jiffier() + timeout; 
    while( get_jiffier() < timer_end_v)
    {
    	/* Create and write message */
    	memset(DNSstr, 0, sizeof(struct dns_header));
    	DNSstr->id = htons(xid);
    	DNSstr->flags = htons(0x0100);
    	DNSstr->no_q = htons(1);
    	/* Full name is encoded as length,part,length,part,,,0 */
    	cp = (unsigned char*)fullname;
    	cp3 = (unsigned char*)(DNSstr+1);
    	cp2 = cp3 + 1;
    	for (;;)
    	{
    		cc = *cp++;
    		if (cc == '.' || cc == 0) 
    		{
    			*cp3 = cp2 - cp3 -1;
    			cp3 = cp2;
    		}
    		*cp2++ = cc;
    		if (cc == 0)
    			break;
    	}
    	*cp2++ = 0, *cp2++ = reqtyp;	/* query type */
    	*cp2++ = 0, *cp2++ = 1; 		/* query class */
    	len =  (long)cp2 - (long)(char *)DNSstr;
		for(;;)
		{
			inet_softirq_disable();
			ret = udp_output(pcb, buf, len, dev->dns, DNS_PORT);
			inet_softirq_enable();
			if(ret<0)
				goto out;
			if(ret==len)
			{
				break;
			}
			inet_delay(100);
		}       
        if(++retry_cnt>=4)retry_cnt=4;
        timer_v = get_jiffier() + retry_cnt*1500;
        if(timer_v > timer_end_v)
            timer_v = timer_end_v;
        while( get_jiffier()<timer_v)
        {
            if(dns->ttl>0||dns->err)
			{
                dns_debug("dns->ttl:%d,%d\r\n",dns->ttl,dns->err);
				goto out;
			}
        }
    }

out:    
	inet_softirq_disable();
	udp_free(pcb);
    udp_filter_unregister(&dns_filter);
	inet_softirq_enable();

    if(dns->ttl > 0)
		return NET_OK;
	
	if(dns->err)
		return dns->err;
    
	return NET_ERR_TIMEOUT;
}

/*
Time out support
*/
int DnsResolveExt_std(char *name/*IN*/, char *result/*OUT*/, int len, int timeout)
{
	int err, i;
	if(name == NULL || len<20 || 
		name[0] == 0||result==NULL)
		return NET_ERR_VAL;
	if(str_check_max(name, DNS_NAME_SIZE)!=NET_OK)
		return NET_ERR_STR;
    if(timeout<1500)
        timeout = 1500;
    if(timeout>3600000)
        timeout = 3600000;
    if(len>200)
        len = 200;

	memset(result,0,len);
    
	err = dns_request_ext(name,len,timeout);
	if(err != NET_OK)
		return err;
	if((u32)len < strlen((char*)dns_resp.resp)+1)
		return NET_ERR_BUF;
	
	len = len > sizeof(dns_resp.resp) ? sizeof(dns_resp.resp) : len;

	memcpy(result,(char *)dns_resp.resp,len);
	return NET_OK;
}

int dns_resolve_ip(char *name/*IN*/, long *p_ipaddr/*OUT*/,int len)
{
	int err;
	
	if(name == NULL || p_ipaddr == NULL ||
		name[0] == 0)
		return NET_ERR_VAL;
	*p_ipaddr = 0;
	err = dns_request(name,len);
	if(err != NET_OK)
		return err;
	if(dns_resp.type != DNS_TYPE_A)
	{
		return NET_ERR_VAL;
	}
	*p_ipaddr = ntohl(inet_addr((char*)dns_resp.resp));
	return NET_OK;
}

