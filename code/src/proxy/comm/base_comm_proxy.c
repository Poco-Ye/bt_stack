/******************************************************************************
*  文件名称:
*               Comm.c
*  功能描述:
*               串口驱动
*  包含以下函数:
*               PortOpen             打开指定串口，并初始化相关通信参数
*               PortClose            关闭指定串口，如果串口还有数据未发送完毕，则
*                                        等待发送完毕后再关闭
*               PortSend            发送一个字符
*               PortSends           发送字符串
*               PortRecv            接收一个字符
*               PortTxPoolCheck    等待串口发送完毕
*               PortReset           串口复位，复位接收发送缓冲区相关参数
****************************************************************************/
//2013.1.7--First edition on BCM5982 CPU

#include  <stdio.h>
#include  <string.h>
#include<stdarg.h>
#include "Base.h"
#include "..\..\..\comm\comm.h"
#include "base_comm_proxy.h"


#define WAIT_TIME_MS 8000
#define TM_COMM_PROXY	3
#define HEARTBEAT_PAYLOAD 256

#define MAX_SIO_SIZE (10241)

extern volatile uint io_start;

uchar base_xtab[P_PORT_MAX];
static  volatile uchar comm_answer_eventFlag[20]={0};  //应答是否产生的标志
static  uchar comm_answer_databuff[600]={0};	//应答数据buff
static  uchar comm_answer_para[5][200];  //存放应答回来被解析后的参数
int base_usb_ohci_state = 0;
extern volatile uchar g_linktype;
static uchar AnswerCommParse(void);

void s_CommInit_Proxy(void)
{
	if(GetHWBranch() != D210HW_V2x) return;
	memset(base_xtab,0xff,sizeof(base_xtab));
	memset((void*)comm_answer_eventFlag,0x00,sizeof(comm_answer_eventFlag));
	memset(comm_answer_databuff,0x00,sizeof(comm_answer_databuff));
	memset(comm_answer_para,0x00,sizeof(comm_answer_para));
}

uchar PortOpen_Proxy(uchar channel, uchar *Attr)
{
	uchar tmpc,tmps[300];
	int tmpd;
	uint AttrLen,tx_len;
	uint t0,t1;

	if(channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)))
	{
		return 2;
	}
	if(channel==P_MODEM && base_modem_occupied())
	{
		return 0xf0;//modem is running
	}
	if(1 == g_linktype && channel == P_BASE_HOST)
	{
		if((!strcmp(Attr,"UDISK") || !strcmp(Attr,"UDISKFAST")))
			return ERR_IRDA_SYS;		// bt link not support udisk
	}
	AttrLen=strlen(Attr);
	if(AttrLen>255)return 0xfe;//invalid parameter

	memset(tmps,0x00,sizeof(tmps));
	tmps[0]=2;//parameter numbers
	
	//fill parameter 1
	tmps[1]=1;
	tmps[2]=0;
	tmps[3]=channel;
	
	//fill parameter 2
	tmps[4]=AttrLen;
	tmps[5]=0;
	memcpy(tmps+6,Attr,AttrLen);
	tx_len=6+AttrLen;
	
	comm_answer_eventFlag[F_PORTOPEN]=0; //清除标志
	tmpc=link_send(COMM_TYPE,F_PORTOPEN,channel,tx_len,tmps);
	if(tmpc)return ERR_IRDA_SEND;

	io_start=GetTimerCount();
	while(1)
	{
		if(comm_answer_eventFlag[F_PORTOPEN])
		{
			tmpc=AnswerCommParse();
			if((P_BASE_HOST == channel) && (!strcmp(Attr,"PAXDEV") || !strcmp(Attr,"UDISK") || !strcmp(Attr,"UDISKFAST")))
			{				
				if(tmpc) return tmpc;			
			}	
			else
				if(!tmpc) base_xtab[channel]=0;

			break;
		}

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;

		if(GetTimerCount()-io_start > WAIT_TIME_MS)
		{
			return 0xff;
		}
	}//while(1)	

	//BASE USB HOST PAXDEV waiting for result
	if(P_BASE_HOST == channel && (!strcmp(Attr,"PAXDEV") || !strcmp(Attr,"UDISK") || !strcmp(Attr,"UDISKFAST")))	
	{		
		base_xtab[channel]=0xff;
		io_start=GetMs();
		t0=GetMs();
		while(1)
		{			
			if((GetMs()-t0) < 500) continue;
			t0=GetMs();					
				
			comm_answer_eventFlag[F_OPEN_CHECK]=0; //清除标志
			tmpc=link_send(COMM_TYPE,F_OPEN_CHECK,P_BASE_HOST,0,NULL);			
			if(tmpc)return ERR_IRDA_SEND;
			
			tmpc = 0xff;
			t1=GetMs();
			while(!comm_answer_eventFlag[F_OPEN_CHECK]) 
			{
				if((GetMs()-t1) > 300) break;							
			}
			if(comm_answer_eventFlag[F_OPEN_CHECK])
			{
				tmpc=AnswerCommParse();	
				if(tmpc==0x55) tmpc=0xff;
				if(!tmpc) base_xtab[channel]=0;
				
			}	
			//--set open flag			
			if(tmpc!=0xff) break;
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)return ERR_IRDA_RECV;			
			if(GetMs()-io_start > WAIT_TIME_MS) return 0xff;		
		}
		if(!strcmp(Attr,"PAXDEV"))
			return tmpc;
	}
	if(tmpc) return tmpc;
	if(P_BASE_HOST == channel)
	{
		memcpy(tmps,Attr,9);
		tmps[9] = 0;
		strupr(tmps);
		if(!strcmp(tmps,"UDISK") || !strcmp(tmps,"UDISKFAST"))
		{
			tmpd = usb_enum_massdev_udiskA();		// 做文件系统相关的初始化
		//	printk("usb_enum_massdev_udiskA ret = %d\r\n",tmpd);
			if(tmpd)
			{
				return USB_ERR_DEV_QUERY;
			}
			base_usb_ohci_state = 1;
			tmpc = 0;
		}
		else
		{
			return 0xfe;
		}
	}
	return tmpc;
}

uchar PortClose_Proxy(uchar channel)
{
	uchar a,tmpc,resp,tmps[20];
	uint tx_len,tt;
	int tmpd;
	
	resp=0;

	if(channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)))
	{
		return 2;
	}

	if(channel!=P_MODEM && base_xtab[channel]>=P_PORT_MAX ||
		channel==P_MODEM && !base_modem_occupied() && base_xtab[channel]>=P_PORT_MAX)
	{
		return 3;
	}

	if(channel==P_MODEM && base_modem_occupied())
	{
		return 0xf0;//modem is running
	}

	//--wait for handset/baseset's tx_pool empty
	if(channel!=P_MODEM || !base_modem_occupied())
	{
		tt=port_max_close_time(channel);
		io_start=GetTimerCount();
		while(tt)
		{
			tmpc=link_send(COMM_TYPE,F_TXPOOLCHECK,channel,0,&a);
			if(tmpc){resp=ERR_IRDA_SEND;goto C_TAIL;}
			if(!a)break;//tx finished

			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0){resp=ERR_IRDA_SEND;goto C_TAIL;}
		
			if(GetTimerCount()-io_start > tt)
			{resp=0xf9;goto C_TAIL;}
		}//while(1)
	}

	tmps[0]=1;//parameter number
	//fill para 1
	tmps[1]=1;
	tmps[2]=0;
	tmps[3]=channel;
	
	//send data 
	tx_len=4;
	comm_answer_eventFlag[F_PORTCLOSE]=0; //清除标志
	tmpc=link_send(COMM_TYPE,F_PORTCLOSE,channel,tx_len,tmps);
	if(tmpc){resp=ERR_IRDA_SEND;goto C_TAIL;}

	io_start=GetTimerCount();
	while(2)
	{
		if(comm_answer_eventFlag[F_PORTCLOSE])
		{
			resp=AnswerCommParse();
			
			//--clear open flag
			base_xtab[channel]=0xff;
			return resp;
		}			
	
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0){resp=ERR_IRDA_RECV;goto C_TAIL;}
		
		if(GetTimerCount()-io_start > WAIT_TIME_MS)
		{
			resp=0xff;
			goto C_TAIL;
		}
	}//while(1)

C_TAIL:
	//--clear open flag
	base_xtab[channel]=0xff;
	if(resp)
	{
//ScrPrint(0,0,0,"CLOSE %d FAIL:%02X,%d.",channel,resp,tt);
//getkey();
		force_close_com(channel);
		if(resp) resp = 0;
	}
	if(channel == P_BASE_HOST)
	{
		usb_deinit_massdev_udiskA();
		base_usb_ohci_state = 0;
	}
    return resp;  
}

uchar PortSends_Proxy(uchar channel,uchar *str,ushort str_len)
{
	uchar tmpc;

	if( channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)) )
	{
		return 2;
	}
	
	if( (channel!=P_MODEM && base_xtab[channel]>=P_PORT_MAX) ||
		(channel==P_MODEM && !base_modem_occupied() && base_xtab[channel]>=P_PORT_MAX) )
	{
		if(channel==P_BASE_DEV)  return 0x0E;
		return 3;//channel not open
	}

	if(channel == P_MODEM && base_modem_occupied())
	{
		return 0xf0;//modem is running
	}	
	
	if(channel == P_MODEM || channel==P_BASE_A || channel==P_BASE_B)
	if(str_len >= SIO_SIZE)
	{
		return 4;//overflowed for too long input
	}

	if(channel==P_BASE_DEV || channel==P_BASE_HOST)
	if(str_len >= MAX_SIO_SIZE)
	{
		return 4;//overflowed for too long input
	}

	tmpc=link_send(COMM_TYPE,F_PORTSENDS,channel,str_len,str);
	if(tmpc)
	{
		if(tmpc==1)return 4;//data overflow
		return ERR_IRDA_SEND;
	}
	return 0;
}

uchar  PortSend_Proxy(uchar channel, uchar ch)
{
	return PortSends_Proxy(channel,&ch,1);
}

int PortRecvs_Proxy(uchar channel,uchar *pszBuf,ushort usRcvLen,ushort usTimeOutMs)
{
	uchar tmpc;
	ushort cur_port,i;
	int tmpd;

	if(channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)) )
	{
		return -2;
	}	
	
	if(channel!=P_MODEM && base_xtab[channel]>=P_PORT_MAX ||
		channel==P_MODEM && !base_modem_occupied() && base_xtab[channel]>=P_PORT_MAX)
	{
		if(channel==P_BASE_DEV)  return -0x0E;
		return -3;//channel not open
	}
	
	if(channel==P_MODEM && base_modem_occupied())
	{
		return -0xf0;//modem is running
	}	
	
	io_start=GetTimerCount();
	if( (usTimeOutMs+9)>65535 )
		usTimeOutMs = 65535;
	else
		usTimeOutMs=(usTimeOutMs+9)/10*10;//for compatibility
	i=0;
	while(i<usRcvLen)
	{
		tmpc=link_send(COMM_TYPE,F_PORTRECV,channel,1,pszBuf+i);
		if(tmpc)
		{
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{
				if(i)break;
				return -ERR_IRDA_SEND;
			}

			if(!usTimeOutMs)break;	
			if(GetTimerCount()-io_start<=usTimeOutMs)continue;

			if(i)break;
			return -0xff;//timeout
		}
		
		i++;
	}//while(rx)
	
	return i;
}

uchar PortRecv_Proxy(uchar channel, uchar *ch, uint ms)
{
	uchar cur_port,tmpc;
	int tmpd;

	tmpd=PortRecvs_Proxy(channel,ch,1,ms);
	if(tmpd>0)tmpd=0;
	else if(!tmpd)tmpd=0xff;
	else tmpd=-tmpd;
    
	tmpc=tmpd&0xff;
	
	return tmpc;
}

int PortPeep_Proxy(uchar channel,uchar *buf,ushort want_len)
{
	uchar cur_port,tmps[MAX_SIO_SIZE+2];
	int i;
	
	if(buf == NULL)
	{
		return -1;//parameter error
	}
	if(channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)) )	
	{
		return -2;//invalid channel no
	}
	
	if(channel!=P_MODEM && base_xtab[channel]>=P_PORT_MAX ||
		channel==P_MODEM && !base_modem_occupied() && base_xtab[channel]>=P_PORT_MAX)
	{
		if(channel==P_BASE_DEV)  return -0x0E;
		return -3;//channel not open
	}
	
	if(channel==P_MODEM && base_modem_occupied())
	{
		return -0xf0;//modem is running
	}	
	
	link_send(COMM_TYPE,F_PORTPEEP,channel,want_len,tmps);
	i=(tmps[0]<<8)+tmps[1];
	memcpy(buf,tmps+2,i);

	return i;
}

uchar PortReset_Proxy(uchar channel)
{
	uchar cur_port,tmpc;

	if(channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)) )	
	{
		return 2;
	}
	
	if(channel!=P_MODEM && base_xtab[channel]>=P_PORT_MAX ||
		channel==P_MODEM && !base_modem_occupied() && base_xtab[channel]>=P_PORT_MAX)
	{
		if(channel==P_BASE_DEV)  return 0x0E;
		return 3;//channel not open
	}
	if(channel==P_MODEM && base_modem_occupied())
	{
		return 0xf0;//modem is running
	}

	tmpc=link_send(COMM_TYPE,F_PORTRESET,channel,0,NULL);
	return tmpc;
}

uchar PortTxPoolCheck_Proxy(uchar channel)//--check if it's finished
{
	uchar tmpc,a;

	if(channel!=P_MODEM && ((channel < P_BASE_DEV) || (channel > P_BASE_B)) )
	{
		return 2;
	}
	
	if(channel!=P_MODEM && base_xtab[channel]>=P_PORT_MAX ||
		channel==P_MODEM && !base_modem_occupied() && base_xtab[channel]>=P_PORT_MAX)
	{
		if(channel==P_BASE_DEV)  return 0x0E;
		return 3;//channel not open
	}
	
	tmpc=link_send(COMM_TYPE,F_TXPOOLCHECK,channel,0,&a);	
	if(tmpc)	return ERR_IRDA_SEND;	
	
	return a;//1-tx not finished yet,0-tx finished
}

/*
SetHeartBeat(enable/disable, MsgLen, Msg, interval,COMn)
SwOn: 開始 / 停止 發送 Heart Beat
MsgLen: 資料包的長度
Msg: 整理好的資料包
interval100Ms: 發送資料包的週期，单位为100毫秒
COMn:     选择的串口，可以为COM1 COM2 或PINPAD口，端口参数由PortOpen来设定
在调用这个函数前要先使用PortOpen打开串口，否则返回失败

  ret:	0	成功
		1	已设置过
		-1 参数错误
		-2	端口没有打开
		-3 红外通讯失败
		-4 设置失败
		
*/
int SetHeartBeat(int SwOn, int MsgLen, const unsigned char *Msg, ushort interval100Ms,int COMn)
{
	uchar tmpc,buff[HEARTBEAT_PAYLOAD+20];
	uint t0;
	int tmpd;
	ushort offset;
	static int LastMlen=0,LastCOMn=0xff;
	static ushort LastInterval=0;
	static uchar LastMsg[HEARTBEAT_PAYLOAD];

	//if(COMn!=P_RS232A && COMn!=P_RS232B && COMn!=P_PINPAD)
	if(COMn!=P_BASE_A && COMn!=P_BASE_B )	
	{
		return -1;//非法端口 参数错误
	}
	if(base_xtab[COMn]>=P_PORT_MAX) //3)
	{
		return -2;//端口没有打开
	}
	
	if(SwOn && SwOn!=1) return -1;

	//--check if it's enabled repeatedly
	if(SwOn && MsgLen==LastMlen &&	COMn==LastCOMn && interval100Ms==LastInterval &&
		!memcmp(LastMsg,Msg,MsgLen))return 1;
	
	memset(buff,0,sizeof(buff));
	offset=0;
	buff[offset++]=SwOn;
	buff[offset++]=COMn;
	if(SwOn)
	{
		if(Msg==NULL || MsgLen<=0 || !interval100Ms ) return -1;
		if(MsgLen > HEARTBEAT_PAYLOAD) return -1;

		buff[offset++]=interval100Ms&0xff;
		buff[offset++]=interval100Ms>>8;

		buff[offset++]=(uchar)(MsgLen&0xff);
		buff[offset++]=(uchar)(MsgLen>>8);

		memcpy(buff+offset,Msg,MsgLen);
		offset+=MsgLen;
	}

	comm_answer_eventFlag[F_SET_HEARTBEAT]=0; //清除标志
	tmpc=link_send(COMM_TYPE,F_SET_HEARTBEAT,COMn,offset,buff);
	if(tmpc)return -3;

	t0=GetTimerCount();//reset timeout start
	while(1)
	{
		if(comm_answer_eventFlag[F_SET_HEARTBEAT])
		{
			tmpd=AnswerCommParse();
			break;
		}			

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return -ERR_IRDA_RECV;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > WAIT_TIME_MS)
		{
			return -4;
		}
	}//while(1)

	if(!tmpd)
	{
		if(!SwOn)
		{
			//--if close,reset the backup variables
			LastMlen=0;
			LastCOMn=0xff;
			LastInterval=0;
			memset(LastMsg,0x00,sizeof(LastMsg));
		}
		else
		{
			//--if open,store the backup variables
			LastMlen=MsgLen;
			LastCOMn=COMn;
			LastInterval=interval100Ms;
			memcpy(LastMsg,Msg,MsgLen);
		}
	}
	if(tmpd>=0xf0)tmpd=-(tmpd&0x0f);//translate the return value above 0xf0
	return tmpd;	 
}

void ComProcess(uchar ucIndex, ushort uiDataLen,uchar  *pData)
{
		memcpy(comm_answer_databuff,pData,uiDataLen);
		if(ucIndex < sizeof(comm_answer_eventFlag))
		{
			comm_answer_eventFlag[ucIndex]=1;  //置事件标志
		}
}

//对收到的数据包进行解析,返回值为待解析包中所含的返回值
uchar  AnswerCommParse(void)
{
     uchar uRet;
     uchar paranum,i;
     ushort len;
     ushort offset=0;
     
	 //--first byte is the return result
	 uRet=comm_answer_databuff[offset++];

      // fetch parameter from IRDA buff
      memset(comm_answer_para,0x00,sizeof(comm_answer_para));	  
      paranum=comm_answer_databuff[offset++];
	  
      for(i=0;i<paranum;i++)
      {
       //len=comm_answer_databuff[offset]+comm_answer_databuff[offset+1]*256;
       len=comm_answer_databuff[offset];
		offset+=2;
		//if(len>200){ScrCls();ScrPrint(0,0,0,"para parse error !");while(1);}
		memcpy(comm_answer_para[i],comm_answer_databuff+offset,len);
		offset+=len;
      }

     return uRet;
}

/*
extern int PackPiccLedSync(uchar *p);
void GetComSychroData(uchar*buff,int*offset)
{
	int   AttrLen;
	int len;
	uchar *ptrlen;

	AttrLen =(CHANNEL_LIMIT)*sizeof(ComPara); 
    //clear 0
	len=0;
	memset(buff,0x00,sizeof(buff));

    // 3 组信息分别为 串口同步信息 心跳包同步信息 PICCLED控制同步信息
    buff[len++]=3;    

	//fill para 1
	buff[len++]=AttrLen;
	buff[len++]=0;

	memcpy(buff+len,ComPara_Table,AttrLen);
	len+=AttrLen;

	//Pack HeartBeat Sych Data 心跳包同步信息下传
	ptrlen=buff+len;
	len+=2;
	
	if(LenHeartBeat)
	{
		buff[len++]=1;
		
		buff[len++]=0;
		buff[len++]=0;
		buff[len++]=ComHeartBeat>>8;
		buff[len++]=ComHeartBeat;
		
		buff[len++]=InterHeartBeat>>8;
		buff[len++]=InterHeartBeat;

		buff[len++]=LenHeartBeat>>24;
		buff[len++]=LenHeartBeat>>16;
		buff[len++]=LenHeartBeat>>8;
		buff[len++]=LenHeartBeat;

		memcpy(buff+len,BufHeartBeat,LenHeartBeat);
		len+=LenHeartBeat;

	}
	else
	{
		buff[len++]=0;
		
		buff[len++]=0;
		buff[len++]=0;
		buff[len++]=ComHeartBeat>>8;
		buff[len++]=ComHeartBeat;
	}

	AttrLen=&buff[len]-ptrlen-2;
	*ptrlen++ = AttrLen;
	*ptrlen   = AttrLen>>8;

	ptrlen=&buff[len];
	AttrLen=PackPiccLedSync(&buff[len]);
	*ptrlen++ = AttrLen;
	*ptrlen   = AttrLen>>8;
	len += AttrLen;

	
	*offset=len;
}
*/

