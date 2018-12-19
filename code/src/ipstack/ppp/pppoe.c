/*
   PPP over Ethernet
   Author: sunJH
   Create: 090824
history:
090911 pppoe_init_server内部要初始化ACName,ServiceName

*/
#include <inet/dev.h>
#include <inet/inet_softirq.h>
#include <inet/skbuff.h>
#include <inet/ethernet.h>
#include "pppoe.h"

#ifdef PPPOE_ENABLE

static PPPOE_DEV  pppoe_dev = {0};
static struct inet_softirq_s  pppoe_timer;
static struct inet_softirq_s  ppp_timer;

/* 检查PPPoE是否处于Session状态 */
#define PPPOE_SES_OK(dev) \
	((dev)->state == PPPOE_SES||(dev)->state==PPPOE_UP|| \
		(dev)->state==PPPOE_PRE_TERM)


/*
不区分大小写
*/
static int MemCmp_NoCase(void *lsh, void *rsh, u16 len)
{
	u16 i;
	u8 *c1 = (u8*)lsh, *c2 = (u8*)rsh;
	u8 t1, t2;
	for (i = 0; i < len; i++) 
	{
		t1 = *c1;
		t2 = *c2;
		if(t1>='a'&&t1<='z')
			t1 = t1-'a'+'A';
		if(t2>='a'&&t2<='z')
			t2 = t2-'a'+'A';
		if (t1 != t2) return 1;
		++c1;
		++c2;
	}
	return 0;   
}

/*---------------PPPOE function-------------*/
static err_t pppoe_build_header(struct sk_buff *skb,u8 code,u16 sessionid,u16 length)
{
	struct pppoe_hdr *pdr;
	skb->nh.raw = __skb_push(skb,sizeof (struct pppoe_hdr));
	pdr = (struct pppoe_hdr *)skb->nh.raw;
	pdr->code = code;
	pdr->length = htons(length);
	pdr->sid = sessionid;
	pdr->ver = PPPOE_VER;
	pdr->type = PPPOE_TYPE;
	return NET_OK;	
}

static err_t getFirstTag(struct sk_buff *skb,struct pppoe_taghdr* tag)
{
	struct pppoe_hdr * phr = (struct pppoe_hdr*)skb->nh.raw;//
	tag = (struct pppoe_taghdr*)(skb->nh.raw+sizeof(struct pppoe_hdr));
	if((phr->length = ntohs(phr->length))!=0)
	{
		tag->tag_type = ntohs(tag->tag_type);
		tag->tag_len = ntohs(tag->tag_len);
	}
	else 
	{
		ppp_printf("error length pdr->len:%d,",phr->length);
		return -1;
	}
	return 0;
}

static err_t getNextTag(struct sk_buff *skb,struct pppoe_taghdr* tag_cur,struct pppoe_taghdr* tag_next,u16 len)
{
	struct pppoe_hdr *pdr = (struct pppoe_hdr *)skb->nh.raw;
	tag_next = (struct pppoe_taghdr*)((u8*)tag_cur+sizeof(struct pppoe_taghdr)+tag_cur->tag_len);
	if(len-ntohs(tag_next->tag_len)<=0)
	{
		ppp_printf("parsing end\n");
		return -1;
	}
	if((u8*)tag_next-skb->nh.raw+sizeof(struct pppoe_hdr)<=ntohs(pdr->length))
	{
		tag_next->tag_type = ntohs(tag_next->tag_type);
		tag_next->tag_len = ntohs(tag_next->tag_len);
	}
	else
	{
		ppp_printf("error pppoe header length pdr->len:%d,",pdr->length);
		return-1;
	}
	return 0;
}

static err_t addTag(struct sk_buff *skb,u16 type,u16 len,void *data)
{
	struct pppoe_hdr *phr =(struct pppoe_hdr*)(skb->data-len-sizeof(struct pppoe_taghdr)-sizeof(struct pppoe_hdr));
	struct pppoe_tag *tag;
	int size = sizeof(struct pppoe_taghdr)+len;
	tag	= (struct pppoe_tag *)__skb_push(skb,size);
	if(phr->length+sizeof(struct pppoe_taghdr)+len >1478)
	{
	 	ppp_printf("Data length is too long to insert tag");
		return 1;
	}
	tag->type = htons(type);
	tag->len = htons(len);

	if(!( type == PTT_SRV_NAME&&len==0))
	{
        	tag->data = (u8*)((u8*)tag+sizeof(struct pppoe_taghdr));
		INET_MEMCPY(tag->data,data,len);
	}
	ppp_printf("tag type:0x%04x  tag len:0x%04x\n",tag->type,tag->len);
	return 0;	
}
static err_t pppoe_disc_send(PPPOE_DEV *dev);
static int pppoe_req_send(PPPOE_DEV *dev);

static char *pppoe_state_str(PPPOE_STATE state)
{
	char *descr[]=
	{
		"PPPOE_INIT",
		"PPPOE_PADI",
		"PPPOE_PADR",
		"PPPOE_SES",/* Session Stage */
		"PPPOE_UP",
		"PPPOE_PRE_TERM",
		"PPPOE_TERM",/* terminate Stage */
	};
	return descr[state-1];
}

/*
状态切换
*/
static void pppoe_switch_state(PPPOE_DEV *dev, PPPOE_STATE NextState)
{
	PPP_DEV_T *ppp = &dev->ppp_dev;
	ppp_printf("pppoe switch state (%s-->%s) delta[%d]\n",
		pppoe_state_str(dev->state),
		pppoe_state_str(NextState),
		inet_jiffier-dev->jiffier
		);
	switch(NextState)
	{
	case PPPOE_INIT:
		dev->user_event = 0;
		dev->retry = 0;	
		dev->session_id = 0;
		break;
	case PPPOE_PADI:
		dev->retry = 0;
		dev->timeout = inet_jiffier+PPPOE_TIMEOUT;
		/* disable TX */
		ppp->inet_dev->flags &= ~FLAGS_TX;
		pppoe_disc_send(dev);
		break;
	case PPPOE_PADR:
		dev->retry = 0;
		dev->timeout = inet_jiffier+PPPOE_TIMEOUT;
		/* disable TX */
		ppp->inet_dev->flags &= ~FLAGS_TX;
		pppoe_req_send(dev);
		break;
		break;
	case PPPOE_SES:
		break;
	case PPPOE_PRE_TERM:
		/* 等待PPP断线时间最长10s*/
		dev->timeout = inet_jiffier+10*1000;
		break;
	case PPPOE_TERM:
		dev->retry = 0;
		dev->timeout = inet_jiffier+PPPOE_TERM_TIMEOUT;
		/* disable TX */
		ppp->inet_dev->flags &= ~FLAGS_TX;
		break;
	default:
 		break;
	}
	dev->state = NextState;
	dev->jiffier = inet_jiffier;
}

/*-----initialize the server device------*/
static int  pppoe_init_server(PPPOE_DEV *dev,char* Data)
{
	u8* AcName = (u8*)Data;
	u8* SerName = NULL;
	u8  SerRead = 0;
	u16 len;	
	u16 SerLen = 0;
	u16 AcLen = 0;
	u16 i;

	dev->ACName[0] = 0;
	dev->ACNameLen = 0;
	dev->ServiceName[0] = 0;
	dev->ServiceNameLen = 0;
	if(Data == NULL||strlen(Data)==0)
	{
		return -1;
	}
	len = strlen(Data);
	
	i = 0;
	while(i<=len)
	{
		if(Data[i] != '@'&&SerRead == 0)
		{
			AcLen++;
		}
		else
		{
			SerRead = 1;
			if(Data[i]=='@')
			{
				i++;
				SerName = (u8*)&Data[i];
			}
		}
		if(SerRead == 1)
		{
			SerLen++;
		}
		i++;
	}

	if (SerLen == 1 && SerName[0] == ' ') SerLen = 0;
	
	if(AcLen == 0)
	{
		ppp_printf("there is no Acname has been set!\n");
		return -1;
	}
	else
	{
		INET_MEMCPY(dev->ACName,AcName,AcLen);
		ppp_printf("AC name :%s\n",dev->ACName);
		dev->ACNameLen = AcLen-1;
	}
	if(SerLen == 0)
	{
		ppp_printf("there is no sername has been set!\n");
		return -1;
	}
	else
	{
		INET_MEMCPY(dev->ServiceName,SerName,SerLen);
		ppp_printf("server name :%s\n",dev->ServiceName);
		dev->ServiceNameLen = SerLen-1;
	}
	return 0;	
}

/*
创建PADI报文并发送
*/
static err_t pppoe_disc_send(PPPOE_DEV *dev)
{
	int err = 0;
	INET_DEV_T *inet_dev = dev_lookup(ETH_IFINDEX_BASE);
	/*int pdrlen = (sizeof(PPPOE_SERNAME)+sizeof (PPPOE_HOST)+sizeof(struct pppoe_taghdr));*/
	struct sk_buff *skb = NULL;
	int pdrlen =sizeof(dev->HostUniq)+2*sizeof(struct pppoe_taghdr);
	int size = sizeof (struct ethhdr)+sizeof (struct pppoe_hdr)+pdrlen;
	u8 *h_src  = inet_dev->mac;

	skb = skb_new(size);
	if(skb==NULL)
	{
		ppp_printf("it can not be allocate memory\n");
		return err=1;
	}
	skb_reserve(skb, size);
	dev->session_id = 0;


	addTag(skb,PTT_HOST_UNIQ,sizeof(dev->HostUniq),(void*)dev->HostUniq);
	addTag(skb, PTT_SRV_NAME, 0,NULL);
	pppoe_build_header(skb, PADI_CODE, 0, pdrlen);
	eth_build_header(skb, bc_mac, inet_dev->mac, ETH_P_PPP_DISC);

	err = inet_dev->dev_ops->xmit(inet_dev, skb, 0, ETH_P_PPP_DISC);
	skb_free(skb);

	return err;
}
/*
处理PADO报文
处理流程:
1. 保存AC-Name TAG, AC-Name TAG
2. 保存mac addr
返回值:
处理成功返回0, 失败返回-1
*/
static int pppoe_offer_input(PPPOE_DEV *dev, struct sk_buff *skb)
{
	int ret = 0;
	u16 len = 0;
	int ServiceOk = 0;

	char *errType = NULL;
	u8 *errStr = NULL;
	u16 errStrLen = 0;

	u8 *acStr = NULL;
	u16 acStrLen = 0;

	u8 *cookieStr = NULL;
	u16 cookieStrLen = 0;

	int haveHostUniq = 0;
	u16 hostUniq = 0;
	u8 *TagData = NULL;
	struct ethhdr* eth = (struct ethhdr*)skb->mac.raw;
	struct pppoe_hdr *hdr = (struct pppoe_hdr *)skb->nh.raw;
	struct pppoe_taghdr *thr_1 =(struct pppoe_taghdr*)(skb->nh.raw+sizeof(struct pppoe_hdr));
	
	len = ntohs(hdr->length);
	//struct pppoe_taghdr *thr_2;
	if(getFirstTag(skb, thr_1)== -1)
	{
		ppp_printf("can not get the first tag\n");
		return -1;
	}
	len -= thr_1->tag_len+sizeof(struct pppoe_taghdr);
	while(1)
	{
		//ppp_printf("tag: 0x%x, len: %d\n", thr_1->tag_type, thr_1->tag_len);
		TagData = ((u8*)thr_1+sizeof(struct pppoe_taghdr));
		switch(thr_1->tag_type)
		{
		case PTT_SRV_NAME:
			if(ServiceOk==0)
			{
				    INET_MEMCPY(dev->ServiceName, TagData, thr_1->tag_len);
				    dev->ServiceNameLen = thr_1->tag_len;
				    ppp_printf("Service name is: %s\t serverlen is:%d",dev->ServiceName,dev->ServiceNameLen);
				    ServiceOk = 1;
			}
			break;
		case PTT_AC_NAME:
			acStr = TagData;
			acStrLen = thr_1->tag_len;
			break;
		case PTT_HOST_UNIQ:
			if (/*thr_1->tag_len != sizeof(u32) || */thr_1->tag_len!= 8) 
			{
				ppp_printf("Host-Uniq tag has wrong length: %d\n", thr_1->tag_len);
				return -1;
			}
			else if(INET_MEMCMP(&dev->HostUniq, TagData, thr_1->tag_len)!=0)
			{				
			    ppp_printf("receive host uniq ID is wrong\n",TagData);
			    buf_printf((u8*)&dev->HostUniq, 8, "HostUniq SRC");
			    buf_printf((u8*)TagData, 8, "HostUniq RCV");
				return -1;
			}
			haveHostUniq = 1;
			break;
		case PTT_AC_COOKIE:
			cookieStr = TagData;
			cookieStrLen = thr_1->tag_len;
			break;
		case PTT_SRV_ERR:
			errType = "Server name wrong\n";
			errStr  = TagData;
			errStrLen = thr_1->tag_len;
			break;
		case PTT_SYS_ERR:
			errType = "AC name error \n";
			errStr = TagData;
			errStrLen = thr_1->tag_len;
			break;
		case PTT_GEN_ERR:
			errType = "General error\n";
			errStr = TagData;
			errStrLen = thr_1->tag_len;
			break;		
		}
		if(getNextTag(skb, thr_1, thr_1,len)!=0)
			break;//搜索下一个标识
		thr_1 = (void*)((u8*)thr_1+thr_1->tag_len+sizeof(struct pppoe_taghdr));
		len -= thr_1->tag_len+sizeof(struct pppoe_taghdr);
	}
	//ppp_printf("leave test tag: 0x%x, len: %d\n", thr_1->tag_type, thr_1->tag_len);
	if(ServiceOk == 0)
	{
		ppp_printf("service not OK\n");
		return -1;
	}
	if(haveHostUniq == 0)
	{
		ppp_printf("There is no host uniq\n");
		return -1;
	}
	if(acStrLen >0)
	{
		ppp_printf("AC name is: %s\t aclen is:%d",acStr,acStrLen);
		INET_MEMCPY(dev->ACName, acStr, acStrLen);
		dev->ACNameLen = acStrLen;
	}
	if(errType != NULL)
	{
		ppp_printf("error exit: %s",errType);
		return -1;
	}
	if(cookieStr!=NULL)
	{
		INET_MEMCPY(dev->ACCookie, cookieStr, cookieStrLen);
		dev->ACCookieLen = cookieStrLen;
	}
	INET_MEMCPY(dev->serv_mac,eth->h_source, ETH_ALEN);
	return 0;
}

/*
创建PADR报文并发送
*/
static int pppoe_req_send(PPPOE_DEV *dev)
{
	int err = 0;
	INET_DEV_T *inet_dev = dev_lookup(ETH_IFINDEX_BASE);
	struct sk_buff *skb = NULL;
	int pdrlen = sizeof(dev->HostUniq)+2*sizeof(struct pppoe_taghdr)+dev->ServiceNameLen+dev->ACCookieLen;
	int size = sizeof (struct ethhdr)+sizeof (struct pppoe_hdr)+pdrlen;
	u8 *h_src  = inet_dev->mac;
	
	skb = skb_new(size);
	if(skb==NULL)
	{
		ppp_printf("it can not be allocate memory\n");
		return err=1;
	}
	skb_reserve(skb, size);
	if(dev->ACCookieLen>0)
	{
		addTag(skb, PTT_AC_COOKIE, dev->ACCookieLen, dev->ACCookie);
	}

	addTag(skb,PTT_HOST_UNIQ,sizeof(dev->HostUniq),(void*)dev->HostUniq);
	addTag(skb, PTT_SRV_NAME, dev->ServiceNameLen,(void*)dev->ServiceName);
	pppoe_build_header(skb, PADR_CODE, 0,pdrlen);
	eth_build_header(skb, dev->serv_mac, inet_dev->mac, ETH_P_PPP_DISC);
	
	err = inet_dev->dev_ops->xmit(inet_dev, skb, 0, ETH_P_PPP_DISC);
	skb_free(skb);
	return err;
}
/*
处理PADS报文
1. 保存session id
返回值:
处理成功返回0, 失败返回-1
*/
static err_t pppoe_ses_input(PPPOE_DEV *dev, struct sk_buff *skb)
{
	int ret = 0;
	struct pppoe_hdr *phr = (void*)skb->nh.raw;
	u16 len = 0;
	char* errType = NULL;
	u8* errStr = NULL;
	u16 errStrLen;

	int haveHostUniq = 0;
	int ServiceOk = 0;
	
	struct ethhdr* eth = (struct ethhdr*)skb->mac.raw;
	struct pppoe_hdr *hdr = (struct pppoe_hdr *)skb->nh.raw;
	struct pppoe_taghdr *thr_1 =(struct pppoe_taghdr*)(skb->nh.raw+sizeof(struct pppoe_hdr));
	u8 *TagData;

	len = ntohs(hdr->length);
	if(getFirstTag(skb, thr_1) == 0)
	{
		len -= thr_1->tag_len+sizeof(struct pppoe_taghdr);
		while(1)
		{
			//ppp_printf("tag: 0x%x, len: %d\n", thr_1->tag_type, thr_1->tag_len);
			TagData = ((u8*)thr_1+sizeof(struct pppoe_taghdr));
			switch(thr_1->tag_type)
			{
			#if 0
			case PTT_SRV_NAME:
				if(ServiceOk==0)
				{
				/*	if(dev->ServiceNameLen!= thr_1->tag_len)break;
					if(thr_1->tag_len!=0&&INET_MEMCMP(TagData,dev->ServiceName,thr_1->tag_len)!=0)
					break;*/
					ServiceOk = 1;
				}
				break;
			case PTT_HOST_UNIQ:
				if(thr_1->tag_len != sizeof(u32))
				{
					ppp_printf("the lengthen of host uniq is wrong\n");
					return ret=-1;
				}
				haveHostUniq = 1;
				dev->HostUniq = *TagData;
				break;
			#endif
			case PTT_SRV_ERR:
				errType = "Server name wrong\n";
				errStr = TagData;
				errStrLen = thr_1->tag_len;
				break;
			case PTT_SYS_ERR:
				errType = "AC name wrong\n";
				errStr = TagData;
				errStrLen = thr_1->tag_len;
				break;
			case PTT_GEN_ERR:
				errType = "General err\n";
				errStr = TagData;
				errStrLen = thr_1->tag_len;
				break;
			}
			if(getNextTag(skb, thr_1, thr_1,len)!= 0)break; //搜寻下一个标识的数据内容
			thr_1 = (void*)((u8*)thr_1+thr_1->tag_len+sizeof(struct pppoe_taghdr));
			len -= thr_1->tag_len+sizeof(struct pppoe_taghdr);
		}
		
	}
	//ppp_printf("PADS input parsing success....\n");
	if(errType != NULL)
	{
		ppp_printf("err exit:%s",errType);
		return ret = -1;
	}
	if(hdr->sid==0)
	{
		ppp_printf("receive NO session ID");
		return -1;
	}

	dev->session_id = hdr->sid; 
	return ret;
}
/*
创建PADT报文并发送
*/
static err_t pppoe_term_send(PPPOE_DEV *dev)
{
	struct sk_buff *skb = NULL;
	u16 pdrlen = 0;
	INET_DEV_T *inet_dev = dev_lookup(ETH_IFINDEX_BASE);
	u8 *h_src  = inet_dev->mac;
	int size = sizeof (struct ethhdr)+sizeof (struct pppoe_hdr);
	
	skb = skb_new(size);
	if(skb == NULL)
	{
		ppp_printf("allocate memory fail\n");
		return -1;
	}
	skb_reserve(skb, size);
	pppoe_build_header(skb,PADT_CODE, dev->session_id, pdrlen);
	eth_build_header(skb, dev->serv_mac, inet_dev->mac, ETH_P_PPP_DISC);
	inet_dev->dev_ops->xmit(inet_dev, skb, PPPOE_ARP_INDEX, ETH_P_PPP_DISC);
	skb_free(skb);
	return 0;
}
/*---PADT recive Parsing----
   if success return 0
   else return -1*/
static err_t pppoe_term_input(PPPOE_DEV *dev,struct sk_buff *skb)
{
	int err = 0;
	struct pppoe_hdr *phr =(struct pppoe_hdr *)skb->nh.raw;
	if(phr->sid != dev->session_id)
	{
		ppp_printf("PADT received session ID does not match");
		return 0;
	}
	pppoe_switch_state(dev, PPPOE_INIT);
	return 0;
}

/*------PPPOE discovery------*/
static err_t pppoe_discovery(PPPOE_DEV *dev,struct sk_buff *skb)
{
	int ret = 0;
	u16 len ;
	PPP_DEV_T *ppp = &dev->ppp_dev;
	struct pppoe_hdr* phr =(void*)skb->nh.raw;
	struct ethhdr *eth = (struct ethhdr *)skb->mac.raw;	

	len = ntohs(phr->length);
	switch(phr->code)
	{
	case PADO_CODE:
		ppp_printf("PPPoE input Discover offer!\n");
		if(phr->sid != 0)
		{
			ppp_printf("exit error session ID is not zero:%d\n",phr->sid);
			goto out;
		}
		if(dev->state != PPPOE_PADI)
		{
			ppp_printf("PADO in non-PADI state:%d\n",dev->state);
			goto out;
		}
		if(pppoe_offer_input(dev,skb)==0)
		{
			ppp_printf("PADR connecting....\n");
			pppoe_switch_state(dev, PPPOE_PADR);
		}
		break;
	case PADS_CODE:
		ppp_printf("PPPoE input Discover session!\n");
		if(phr->sid == 0)
		{
			ppp_printf("PADS session ID is zero\n");
			goto out;
		}
		if(dev->state != PPPOE_PADR)
		{
			ppp_printf("PADS in non-PADR state: %d\n", dev->state);
			goto out;
		}
		if(pppoe_ses_input(dev,skb)==0)
		{
			ppp_printf("SESSION connecting....\n");
			pppoe_switch_state(dev, PPPOE_SES); // switch to session state
			ppp_login(ppp, ppp->userid, ppp->passwd, dev->auth, 0);
		} 
		break;
	case PADT_CODE:
		ppp_printf("PPPoE input Discover Terminat!\n");
		if (INET_MEMCMP(bc_mac, eth->h_dest, ETH_ALEN) == 0)
		{
			ppp_printf("PADT to broadcast address\n");
			goto out;
        }
        if (phr->sid != dev->session_id) 
        {
			ppp_printf("PADT with wrong session id\n");
			goto out;
		}

		if (!PPPOE_SES_OK(dev)) 
		{
			ppp_printf("PADS in non-SESSION state: %s\n", 
				pppoe_state_str(dev->state));
			goto out;
		}
		pppoe_term_input(dev,skb);
        break;
	default:
        break;
	}
out:	
	skb_free(skb);
	return 0;
}

int ppp_input(struct sk_buff *skb, PPP_DEV_T *ppp);
static err_t pppoe_session(PPPOE_DEV *dev,struct sk_buff *skb)
{
	int err = 0;
	INET_DEV_T *inet_dev = dev_lookup(ETH_IFINDEX_BASE);
	struct pppoe_hdr *phr =(struct pppoe_hdr *)skb->nh.raw;
	struct ethhdr *eth = (struct ethhdr *)skb->mac.raw;

	__skb_pull(skb, sizeof(struct pppoe_hdr));
	
	if(phr->sid != dev->session_id)
	{
		if(phr->sid != 0)
		{
			if(INET_MEMCMP(eth->h_dest, inet_dev->mac, ETH_ALEN)==0)
			{
				u16 sid = dev->session_id;
				ppp_printf("This pppoe session packet is for me, but we should terminate it!\n");
				dev->session_id = phr->sid;
				pppoe_term_send(dev);
				dev->session_id = sid;
			}
		}
		ppp_printf("PPPOE_SES_REV received session ID does not match\n");
		goto out;
	}
	if(!PPPOE_SES_OK(dev))
	{
		ppp_printf("PPPOE_SES_REV received state not match (%s)\n",
			pppoe_state_str(dev->state)
		);
		goto out;
	}
	if(phr->code != 0)
	{
		ppp_printf("PPPOE_SES_REV received wrong code!\n ");
		goto out;
	}
	if((phr->ver|phr->type<<4)!= 0x11)
	{
		ppp_printf("the VerType is wrong\n");
		goto out;
	}
	if(INET_MEMCMP(eth->h_source, dev->serv_mac, ETH_ALEN) != 0)
	{
		ppp_printf("session packet not from server\n");
		goto out;
	}
	if (INET_MEMCMP(eth->h_dest, inet_dev->mac, ETH_ALEN) != 0) {
		ppp_printf("session packet not directly to us\n");
		goto out;
	}
	ppp_input(skb,&dev->ppp_dev);
	return 0;
out:	
	skb_free(skb);
	return -1;
}

/*
PPPoE定时器处理事件
*/
static void pppoe_timeout(unsigned long mask)
{
	PPPOE_DEV *dev = &pppoe_dev;
	PPP_DEV_T *ppp = &dev->ppp_dev;
	
	if(dev->state == 0)
		return;
	switch(dev->state)
	{
	case PPPOE_INIT:
		if(dev->user_event==USER_EVENT_LOGIN)
		{
			dev->jiffier = dev->login_jiffier;
			memset(dev->HostUniq, 0x00, sizeof(dev->HostUniq));
			INET_MEMCPY(dev->HostUniq,&dev->jiffier,sizeof(u32));
		//	dev->HostUniq++;
			dev->session_id = 0;
			pppoe_switch_state(dev, PPPOE_PADI);
		}
		dev->user_event = 0;
		break;
		//continue PPPOE_PADI
	case PPPOE_PADI:
		if(dev->user_event==USER_EVENT_LOGOUT)
		{
			dev->jiffier = dev->logout_jiffier;
			pppoe_switch_state(dev, PPPOE_INIT);
			dev->error_code = NET_ERR_LOGOUT;
			dev->user_event = 0;
			break;
		}
		dev->user_event = 0;
		if(dev->retry >=PPPOE_RETRY)
		{
			ppp_printf("PADI connect failed \n");
			pppoe_switch_state(dev, PPPOE_INIT);
			dev->error_code = NET_ERR_SERV;
		}
		else if(TM_LE(dev->timeout, inet_jiffier))
		{
			//隔一个时间戳发送一个数据包
			dev->retry++;
			dev->timeout = inet_jiffier+PPPOE_TIMEOUT;
			ppp_printf("PADI connecting retry......[%d]\n", inet_jiffier);
			pppoe_disc_send(dev);
		}
		break;
	case PPPOE_PADR:
		if(dev->user_event==USER_EVENT_LOGOUT)
		{
			dev->jiffier = dev->logout_jiffier;
			pppoe_switch_state(dev, PPPOE_INIT);
			dev->error_code = NET_ERR_LOGOUT;
			break;
		}
		dev->user_event = 0;
		if(dev->retry >=PPPOE_RETRY)
		{
			ppp_printf("PADI connect failed \n");
			dev->error_code = NET_ERR_SERV;
			pppoe_switch_state(dev, PPPOE_INIT);
			break;
		}
		else if(TM_LE(dev->timeout, inet_jiffier))
		{
			dev->retry++;
			dev->timeout = inet_jiffier+PPPOE_TIMEOUT;
			ppp_printf("PADR connecting retry......[%d]\n", inet_jiffier);
			pppoe_req_send(dev);
		}
		break;
	case PPPOE_SES: 
		if(dev->user_event == USER_EVENT_LOGOUT)
		{
			ppp_printf("User Logout........\n");
			dev->jiffier = dev->logout_jiffier;
			dev->error_code = NET_ERR_LOGOUT;
			ppp_logout(ppp, NET_ERR_LOGOUT);
			pppoe_switch_state(dev, PPPOE_PRE_TERM);
			dev->user_event = 0;
			break;
		}
		dev->user_event = 0;
		if(ppp->state == PPPclsd)
		{
			if(ppp->error_code)
				dev->error_code = ppp->error_code;
			else
				dev->error_code = NET_ERR_PPP;
			pppoe_switch_state(dev, PPPOE_TERM);
			break;
		}
		if(ppp->state==PPPopen)
		{
			ppp_printf("PPPoE OK delta[%d]\n",
				inet_jiffier-dev->jiffier);
			pppoe_switch_state(dev, PPPOE_UP);
		}
		break;
	case PPPOE_UP:
		if(dev->user_event == USER_EVENT_LOGOUT)
		{
			ppp_printf("User Logout........\n");
			dev->jiffier = dev->logout_jiffier;
			ppp->error_code = NET_ERR_LOGOUT;
			ppp_logout(ppp, NET_ERR_LOGOUT);
			pppoe_switch_state(dev, PPPOE_PRE_TERM);
			dev->user_event = 0;
			break;
		}
		dev->user_event = 0;
		if(ppp->state == PPPclsd)
		{
			if(ppp->error_code)
				dev->error_code = ppp->error_code;
			else
				dev->error_code = NET_ERR_PPP;
			pppoe_switch_state(dev, PPPOE_TERM);
			break;
		}
		break;
	case PPPOE_PRE_TERM:
		if(ppp->state == PPPclsd||TM_LE(dev->timeout, inet_jiffier))
		{
			pppoe_switch_state(dev, PPPOE_TERM);
			break;
		}
		break;
	case PPPOE_TERM:
		if(dev->retry >=PPPOE_TERM_NUM)
		{
			dev->error_code = ppp->error_code;
			pppoe_switch_state(dev, PPPOE_INIT);
		}
		else if(TM_LE(dev->timeout, inet_jiffier))
		{
			dev->retry++;
			dev->timeout = inet_jiffier+PPPOE_TERM_TIMEOUT;
			ppp_printf("PADR Term Send......\n");
			pppoe_term_send(dev);
		}
		break;
	}
}
static void pppoe_irq_disable(PPP_DEV_T *ppp)
{
}

static void pppoe_irq_enable(PPP_DEV_T *ppp)
{
}

static void pppoe_irq_tx(PPP_DEV_T *ppp)
{
}

static int  pppoe_ready(PPP_DEV_T *ppp)
{
	int ret = 0;
	return ret;
}
/* check: link设备是否已掉线
**	0: ok, <0 fail
*/
static int  pppoe_check(PPP_DEV_T *ppp, long ppp_state)
{
	int ret = -1;
	if(PPPOE_SES_OK(&pppoe_dev))
		ret = 0;
	return ret;
}

/* force_down: 强制link设备掉线*/
static void pppoe_force_down(PPP_DEV_T *ppp)
{
}

static PPP_LINK_OPS  pppoe_link_ops=
{
	pppoe_irq_disable,
	pppoe_irq_enable,
	pppoe_irq_tx,
	pppoe_ready,
	pppoe_check,
	pppoe_force_down,
	NULL,
};

static void pppoe_ppp_timeout(unsigned long mask)
{
	PPPOE_DEV *pppoe = &pppoe_dev;	
	ppp_timeout(&pppoe->ppp_dev, mask);
}

#endif/* PPPOE_ENABLE */

/*
PPPoE收到报文处理
*/
err_t pppoe_input(struct sk_buff *skb, INET_DEV_T *in, u16 proto)
{
#ifdef PPPOE_ENABLE

	if(proto == ETH_P_PPP_DISC)
	{
		pppoe_discovery(&pppoe_dev, skb);
	}
	else if(proto == ETH_P_PPP_SES)
	{
		pppoe_session(&pppoe_dev,skb);
	}
#else
	skb_free(skb);
#endif
	return 0;
}

int pppoe_output(PPP_DEV_T *ppp, void *data, int len, u16 proto)
{
#ifdef PPPOE_ENABLE
	struct sk_buff *skb;
	void *p;
	struct pppoe_hdr *hdr;
	int size = len+sizeof(struct ethhdr)+sizeof(struct pppoe_hdr)+sizeof(u16);
	PPPOE_DEV *pppoe = &pppoe_dev;
	INET_DEV_T *inet_dev = dev_lookup(ETH_IFINDEX_BASE);
	if(!PPPOE_SES_OK(pppoe))
	{
		return -1;
	}
	skb = skb_new(size);
	if(skb == NULL)
	{
		return -1;
	}
	skb_reserve(skb, size);
	
	/* Copy PPP Data */
	p = (void*)__skb_push(skb, len);
	memcpy(p, data, len);	
	/* Add PPP Proto */
	p = (void*)__skb_push(skb, sizeof(u16));
	proto = htons(proto);
	memcpy(p, &proto, 2);

	/* Add PPPoE header */
	hdr = (struct pppoe_hdr*)__skb_push(skb, sizeof(struct pppoe_hdr));
	hdr->code = 0;
	hdr->length = htons(len+sizeof(u16));
	hdr->sid = pppoe->session_id;
	hdr->ver = PPPOE_VER;
	hdr->type = PPPOE_TYPE;

	/* Add Ethernet header */
	eth_build_header(skb, pppoe->serv_mac, inet_dev->mac, ETH_P_PPP_SES);
	
	inet_dev->dev_ops->xmit(inet_dev, skb, 0, ETH_P_PPP_SES);
	skb_free(skb);
#endif
	return 0;
}


void s_initPPPoe(void)
{
#ifdef PPPOE_ENABLE
	PPPOE_DEV *pppoe = &pppoe_dev;
	PPP_DEV_T *ppp = &pppoe->ppp_dev;
	INET_DEV_T *dev = &pppoe->inet_dev;
	static u8 bInit=0;

	if(!is_eth_module()) return;
		
	if(bInit)
		return;
	bInit = 1;

	memset(pppoe, 0, sizeof(PPPOE_DEV));
	/*------PPPOE initialize----*/
	pppoe->state = PPPOE_INIT;
	pppoe->link_dev = dev_lookup(ETH_IFINDEX_BASE);
	
	
	ppp_dev_init(ppp); 
	ppp_inet_init(ppp, dev, "PPPOE", PPPOE_IFINDEX); //register the device 
	ppp->link_ops = &pppoe_link_ops;
	ppp->init = 1;
	ppp->link_type = PPP_LINK_ETH; 
	
	pppoe_timer.mask = INET_SOFTIRQ_TIMER;
	pppoe_timer.h = pppoe_timeout;
	inet_softirq_register(&pppoe_timer);  //register the timer for time task
	
	ppp_timer.mask = INET_SOFTIRQ_TIMER;
	ppp_timer.h = pppoe_ppp_timeout;
	inet_softirq_register(&ppp_timer);  //register the timer for time task
	ppp_printf("PPPoE module Init!\n");
#endif/* PPPOE_ENABLE */	
}

int PppoeLogin(char *name, char *passwd, int timeout)
{
#ifdef PPPOE_ENABLE
	int ret;
	PPPOE_DEV *dev = &pppoe_dev;
	PPP_DEV_T *ppp = &dev->ppp_dev;

	if(dev->link_dev == NULL)
		return NET_ERR_SYS;
	
	while(dev->state!=PPPOE_INIT)
	{
		//强制等待
	}

	if(dev->state == 0)
		return NET_ERR_INIT;
	if(dev->state == PPPOE_UP)
	{
		return 0;
	}else if(dev->state == PPPOE_PADI||dev->state==PPPOE_PADR||
		dev->state == PPPOE_SES)
	{
		return NET_ERR_AGAIN;// doing 
	}
	inet_softirq_disable();
	ppp->error_code = 0;
	ret = ppp_set_authinfo(ppp, name, passwd);
	if(ret == 0)
	{
		pppoe_init_server(dev,name);
		dev->user_event = USER_EVENT_LOGIN;
		dev->auth = 0xff;
		dev->login_jiffier = inet_jiffier;
	}
	inet_softirq_enable();
	if(ret)
		return ret;//error
	while(1)
	{
		if(dev->user_event == USER_EVENT_LOGIN)
			continue;
		ret = PppoeCheck();
		if(ret==0)
			return NET_OK;
		if(ret<0)
			return ret;
		if(timeout==0)
			return NET_ERR_TIMEOUT;
		inet_delay(INET_TIMER_SCALE);
		if(timeout > 0)
		{
			if(timeout > INET_TIMER_SCALE)
				timeout -= INET_TIMER_SCALE;
			else
				timeout = 0;
		}
	}
#else
	return NET_ERR_SYS;
#endif/* PPPOE_ENABLE */
}

int PppoeCheck(void)
{
#ifdef PPPOE_ENABLE
	PPPOE_DEV *dev = &pppoe_dev;
	PPP_DEV_T *ppp = &dev->ppp_dev;
	
	if(dev->link_dev == NULL)
		return NET_ERR_SYS;
	if(dev->state == 0)
		return NET_ERR_INIT;
	if(dev->user_event == USER_EVENT_LOGIN||
		dev->user_event == USER_EVENT_LOGOUT)
		return 1;// doing it!!!!!
	if(dev->state==PPPOE_INIT)
	{
		if(dev->error_code)
			return dev->error_code;
		return NET_ERR_LINKDOWN;
	}
	if(dev->state==PPPOE_UP)
	{
		/* Link OK*/
		return 0;
	}
	/* Login or Logout doing */
	return 1;
	
#else
	return NET_ERR_SYS;
#endif/* PPPOE_ENABLE */
}

void PppoeLogout(void)
{
#ifdef PPPOE_ENABLE
	PPPOE_DEV *dev = &pppoe_dev;
	
	if(dev->link_dev==NULL)
		return ;
	if(dev->state == 0)
		return ;
	if(dev->state==PPPOE_INIT)
		return ;
	inet_softirq_disable();
	dev->user_event = USER_EVENT_LOGOUT;
	inet_softirq_enable();
	dev->logout_jiffier = inet_jiffier;
	while(dev->user_event==USER_EVENT_LOGOUT)
	{
	}
	ppp_printf("PppoeLogout ok end!\n");	
#endif/* PPPOE_ENABLE */
}
