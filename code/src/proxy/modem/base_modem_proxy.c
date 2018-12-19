//2013.1.6--First edition at BCM5892 platform
//2014.5.15-Added F_MDM_POLL_TX_POOL for FSK tx
//         -Modified ModemRxd() to enable SDLC data rx after line-broken
//         -Added Async check in ModemReset()
//2014.5.23-Revised F_MDM_CHECK output from 1 byte to 4 bytes
//2014.6.04-Revised modem_status transfer in ModemCheck()
//2015.11.30-Added tx_level/answer_tone_threshold adjust in ModemDial()
//          -Added idra_close_modem() call in ModemDial()
//          -Added support for FSK's ASYNC_RX fetching

#include <string.h>
#include "Base.h"
#include "..\..\..\modem\modem.h"
#include "..\..\..\comm\comm.h"

#define M_MODEM 0x02
#define ERR_IRDA_SEND 0xf3/*failed to send REQUEST*/
#define ERR_IRDA_RECV 0xf4/*failed to receive REPLY*/
#define TIMEOUT_REPLY 8000
#define LINK_PAYLOAD 1024

//#define MODEM_DEBUG  1

enum  MDM_FUNC{F_MDM_ONHOOK=0,F_MDM_DIAL,F_MDM_TXD,F_MDM_RXD,
		F_MDM_RESET,F_MDM_CHECK=6,F_MDM_ASYNC_GET,F_MDM_EXCOMMAND,F_MDM_INFO,
		F_MDM_GET_TX_RESULT,F_MDM_START_RX,F_MDM_GET_RX_RESULT,F_MDM_POLL_TX_POOL};

extern volatile uint io_start;
struct
{
   uchar sysmenu_tx_level;//菜单中手动调整发送电平,取值1~15.为0表示关闭手动调整
   uchar sysmenu_answer_tone;//菜单中应答音门限值，取值1~255.为0表示关闭手动调整	
}signal_cfg;

static uchar DRV_VER[20]; 
static uchar DRV_MAKE_DATE[20];
static uchar MODEM_NAME[20];
static uchar status_check_flag=0;

static volatile uchar is_sync,is_sync_callee,is_fsk;
static volatile uchar answer_eventFlag[20]={0};  //应答是否产生的标志
static uchar answer_data_buff[LINK_PAYLOAD];	//应答数据buff
static uchar answer_para[3][LINK_PAYLOAD];  //存放应答回来被解析后的参数
static ushort answer_data_len;

extern uchar s_GetScrEcho(void);

static uchar  AnswerModemParse(uchar is_fsk_rx);
ushort ModemExCommand_Proxy(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms);
uchar OnHook_Proxy(void);

uchar s_ModemInit_Proxy(uchar mode)
{
	uchar tmps[200];
	is_sync=1;
	is_sync_callee=0;
	is_fsk=0;
	memset((char *)answer_eventFlag,0x00,sizeof(answer_eventFlag));
	memset(answer_data_buff,0x00,sizeof(answer_data_buff));
	memset(answer_para,0x00,sizeof(answer_para));
	memset(&signal_cfg,0x00,sizeof(signal_cfg));

	 if(!SysParaRead(MODEM_LEVEL_INFO, tmps))
	 {
		  //为了和APP保持兼容，电平0dBm略去，只支持1~15范围调整
		  if(tmps[0])signal_cfg.sysmenu_tx_level = tmps[1]-'0';//如果菜单手动设置电平使能了则tmp[0]>0;如果禁止则tmp[1]==0
	 }

	 if(!SysParaRead(MODEM_ANSWER_TONE_INFO, tmps))
	 {	 	
		  //应答音门限制的取值范围:1~255
		  if(tmps[0])signal_cfg.sysmenu_answer_tone= atol(tmps+1);//如果菜单手动设置应答音门限值使能了则tmp[0]>0;如果禁止则tmp[1]==0
	 }
	 
	return 0;
}

void s_ModemInfo_Proxy(uchar *drv_ver,uchar *mdm_name,uchar *last_make_date,uchar *others)
{
    uchar uRet,mdm_info[100];
	ushort templen,len;
	int tmpd;
	static uchar flag=0;
	
	if(!flag)
	{
		memset(DRV_VER,0,sizeof(DRV_VER));
		memset(DRV_MAKE_DATE,0,sizeof(DRV_MAKE_DATE));
		memset(MODEM_NAME,0,sizeof(MODEM_NAME));
		memset(mdm_info,0x00,sizeof(mdm_info));
		
		answer_eventFlag[F_MDM_INFO]=0;
		uRet=link_send(M_MODEM,F_MDM_INFO,0,0,NULL);
		if(uRet)return;

		io_start=GetTimerCount();
		while(1)
		{
			if(answer_eventFlag[F_MDM_INFO])
			{
				uRet=AnswerModemParse(0);
				templen=answer_para[1][0]+answer_para[1][1]*256;
				memcpy(mdm_info,answer_para[0],templen);
				answer_eventFlag[F_MDM_INFO]=0; //清除标志
				break;
			}

			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)return;

			if(GetTimerCount()-io_start > TIMEOUT_REPLY)
			{
				return;
			}
		}//while(1)
		
		len=strlen(mdm_info)+1;
		memcpy(DRV_VER,mdm_info,len);
		
		templen=len;
		len=strlen(mdm_info+templen)+1;
		memcpy(DRV_MAKE_DATE,mdm_info+templen,len);
		
		templen+=len;	
		len=strlen(mdm_info+templen)+1;
		memcpy(MODEM_NAME,mdm_info+templen,len);

		if(strlen(DRV_VER)&&strlen(DRV_MAKE_DATE)&&strlen(MODEM_NAME))
			flag=1;
	}

	if(drv_ver)	strcpy(drv_ver,DRV_VER);
	if(last_make_date) strcpy(last_make_date,DRV_MAKE_DATE);
	if(mdm_name) strcpy(mdm_name,MODEM_NAME);
	if(others)sprintf(others,"%s %s %s",drv_ver,last_make_date,mdm_name);
	return;
}

uchar ModemDial_Proxy(COMM_PARA *Para,uchar *TelNo,uchar BlockMode)
{
	uchar channel_no,status,uRet,echo_bak,buff[300];
	ushort offset,len;
	MODEM_CONFIG modem_config,*MPara;
	int tmpd;

	//--close modem port task if it's opened already
	close_modem();//added on 2015.12.1
	//--set tx_level ranging from 1 to 15
	if(signal_cfg.sysmenu_tx_level && signal_cfg.sysmenu_tx_level<16)
	{
		sprintf(buff,"ATS91=%d\r",signal_cfg.sysmenu_tx_level);
		uRet=ModemExCommand_Proxy(buff,NULL,NULL,0);
		if(uRet)return uRet;
	}

	//--set answer_tone_threshold in 10ms
	if(signal_cfg.sysmenu_answer_tone)
	{
        sprintf(buff,"AT!4879=%02X\r",signal_cfg.sysmenu_answer_tone);
		uRet=ModemExCommand_Proxy(buff,NULL,NULL,0);
		if(uRet)return uRet;
	}
	
	offset=0;
	len=0;
	memset(buff,0x00,sizeof(buff));

    buff[offset++]=3;//parameter number
	//fill para 1
	len=sizeof(MODEM_CONFIG);
	buff[offset++]=len;
	buff[offset++]=0;

	is_sync_callee=0;
	is_fsk=0;
	MPara = (MODEM_CONFIG*)Para;
	if(MPara)
	{
		memcpy(buff+offset,MPara,len);
		if(!(MPara->pattern&0x80))is_sync=1;
		else is_sync=0;
		channel_no=is_sync;
		if(!is_sync && (MPara->async_format==8 || MPara->async_format==9))
		{
			channel_no=2;
			is_fsk=1;
		}
		if(MPara->ignore_dialtone&0x02)channel_no|=0x10;//bit d4 is callee flag

		if(is_sync && MPara->ignore_dialtone&0x02)is_sync_callee=1;
	}
	else//为空时使用缺省参数 
	{
		modem_config.mode=0;
		modem_config.ignore_dialtone=0;
		modem_config.dial_wait=0;
		modem_config.pbx_pause=10;
		modem_config.code_hold=70;
		modem_config.code_space=8;
		modem_config.pattern=0x02;
		modem_config.dial_times=1;
		modem_config.idle_timeout=6;//*10s
		modem_config.async_format=0;
		memcpy(buff+offset,&modem_config,len);

		is_sync=1;
		channel_no=1;
	}
	offset+=len;

	//fill para 2
	len=strlen(TelNo);
	buff[offset++]=len;
	buff[offset++]=0;
    memcpy(buff+offset,TelNo,len);
	offset+=len;

    //fill para 3
	buff[offset++]=1;
	buff[offset++]=0;
	buff[offset++]=0;//采用预拨号

	answer_eventFlag[F_MDM_DIAL]=0;
	//--channel_no:0-async,1-sync,2-FSK
	uRet=link_send(M_MODEM,F_MDM_DIAL,channel_no,offset,buff);
	if(uRet)return ERR_IRDA_SEND;

	io_start=GetTimerCount();
	while(1)
	{
		if(answer_eventFlag[F_MDM_DIAL])
		{
            uRet=AnswerModemParse(0);
			break;
		}			

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;

		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			return 0xff;
		}
	}//while(1)

	status_check_flag=1;//for ModemCheck() process

	if(!BlockMode || BlockMode&&uRet)return uRet;

	//--blocked dial,waiting for result
	io_start=GetTimerCount();
	while(1)
	{
		if(!kbhit() && (getkey()==KEYCANCEL)) 
		{
			echo_bak=s_GetScrEcho();
			ScrSetEcho(1);
			OnHook_Proxy();
			ScrSetEcho(echo_bak);
			return 0xfd;
		}

		link_send(M_MODEM,F_MDM_CHECK,0,0,buff);
		status=buff[0];
		if(status!=0x0a)break;
	
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;
		
		if(GetTimerCount()-io_start > 300000)
		{
			return 0xf9;//too long time
		}
	}
	if(status==0x08)status=0;
	return status;		
}

ushort ModemExCommand_Proxy(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms)
{
	uchar buff[255],uRet=-1;
	ushort offset,len;
	int tmpd;
	
  	offset=0;
	len=0;
	memset(buff,0x00,sizeof(buff));
    buff[offset++]=2;    //parameter number
	
	//fill para 1
	len=strlen(CmdStr);
	if(len>100)return 2;

	buff[offset++]=len;
	buff[offset++]=0;
    memcpy(buff+offset,CmdStr,len);
	offset+=len;
	
    //fill para 2
	buff[offset++]=2;
	buff[offset++]=0;
	buff[offset++]=Timeout10ms&0xff;
	buff[offset++]=(Timeout10ms>>8)&0xff;
	
    //send data 
	answer_eventFlag[F_MDM_EXCOMMAND]=0;
	uRet=link_send(M_MODEM,F_MDM_EXCOMMAND,0,offset,buff);
	if(uRet)return ERR_IRDA_SEND;
	
	io_start=GetTimerCount();
	while(1)
	{
		if(answer_eventFlag[F_MDM_EXCOMMAND])
		{
			uRet=AnswerModemParse(0);
			if(Dlen!=NULL)*Dlen=0;
			
			answer_eventFlag[F_MDM_EXCOMMAND]=0;
			break;
		}			

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;

		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			return 0xff;
		}
	}//while(1)
	return uRet;
}

uchar OnHook_Proxy(void)
{
	uchar uRet;
	int tmpd;
	
	answer_eventFlag[F_MDM_ONHOOK]=0;
	uRet=link_send(M_MODEM,F_MDM_ONHOOK,0xff,0,NULL);
	if(uRet)return ERR_IRDA_SEND;

	io_start=GetTimerCount();
	while(1)
	{
		if(answer_eventFlag[F_MDM_ONHOOK])
		{
            uRet=AnswerModemParse(0);
			break;
		}			

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;

		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			return 0xff;
		}
	}//while(1)
	
	return uRet;
}

uchar ModemCheck_Proxy(void)
{
	uchar cc,tmps[10];

	//--All 4 bytes of irda_link_send()'s data[] argument:
	//B0:modem status reported by baseset
	//B1:rx data existence reported by baseset
	//B2:rx data existence reported by handset
	//B3:SDLC's tx data occupied reported by handset
	link_send(M_MODEM,F_MDM_CHECK,0,0,tmps);
	cc=tmps[0];
	if(tmps[2] && (!cc || cc==0x01))cc|=0x08;
	if(tmps[3] && (!cc || cc==0x08))cc|=0x01;
	if(status_check_flag==1 && cc==0x08)
	{
		cc=0;
		status_check_flag=2;
	}

	return cc;
}

uchar ModemTxd_Proxy(uchar *SendData,ushort TxLen)
{
	uchar a,status,tmps[10];
	int tmpd;
	
	//1.1--check modem status
	a=link_send(M_MODEM,F_MDM_CHECK,0,0,tmps);
	if(a)return ERR_IRDA_SEND;

	status=tmps[0];
	//--skip over FSK tx occupied due to IO delay
	if(is_fsk && (status==0x09 || status==0x01))status=0;

	//B3:SDLC's tx data occupied reported by handset
	if(tmps[3] || status==0x09 || status==0x01)return 0x01;
	if(status && status!=0x08)return status;
	
	if(!is_sync_callee&&TxLen>DATA_SIZE || is_sync_callee&&TxLen>DATA_SIZE+5)return 0xfe;
	if(!TxLen)return 0xfe;

	//1.2--send data 
	a=link_send(M_MODEM,F_MDM_TXD,0,TxLen,SendData);
	if(a>10)return ERR_IRDA_SEND;
	else if(a)return 1;//tx space is busy
	
	if(!is_fsk)return 0;

	//2--check if handset's FSK tx is finished
	io_start=GetTimerCount();
	while(1)
	{
		a=link_send(M_MODEM,F_MDM_POLL_TX_POOL,2,0,NULL);
		if(!a)break;

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;
		
		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			return 0xff;
		}
	}//while(1)
	
	//2--get FSK tx result
GET_FSK_TX_RESULT:
	answer_eventFlag[F_MDM_GET_TX_RESULT]=0;
	a=link_send(M_MODEM,F_MDM_GET_TX_RESULT,2,0,NULL);
	if(a)return ERR_IRDA_SEND;

	io_start=GetTimerCount();
	while(2)
	{
		if(answer_eventFlag[F_MDM_GET_TX_RESULT])
		{
            a=AnswerModemParse(0);
			if(a==0x0a)goto GET_FSK_TX_RESULT;
			break;//0x0a means tx is doing and pending
		}			
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;

		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			return 0xff;
		}
	}//while(2)

	return a;
}

uchar MdmFskRxd(uchar *RecvData,ushort *len)
{
	uchar a,resp,status,tmps[10];
	ushort n_segments,rn,tn,tmpu;
	int tmpd;

	//1.1--start FSK rx
	answer_eventFlag[F_MDM_START_RX]=0;
	a=link_send(M_MODEM,F_MDM_START_RX,2,0,NULL);
	if(a)return ERR_IRDA_SEND;
	
	io_start=GetTimerCount();
	while(11)
	{
		if(answer_eventFlag[F_MDM_START_RX])
		{
			a=AnswerModemParse(0);
			break;
		}			
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return ERR_IRDA_RECV;

		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			return 0xff;
		}
	}//while(11)
	//if(a)return a;//need not check the result
	
	//1.2--get FSK rx result
	rn=0;
	n_segments=0;
	resp=0;
GET_FSK_RX_RESULT:		
	io_start=GetTimerCount();

    tmps[0]=1;//parameter number
	//fill para 1
	tmps[1]=1;
	tmps[2]=0;
	tmps[3]=n_segments;
	tn=4;
	
	answer_eventFlag[F_MDM_GET_RX_RESULT]=0;
	a=link_send(M_MODEM,F_MDM_GET_RX_RESULT,2,tn,tmps);
	if(a)return ERR_IRDA_SEND;
	
	while(12)
	{
		if(answer_eventFlag[F_MDM_GET_RX_RESULT])
		{
			resp=AnswerModemParse(1);
			if(resp==0x0a)goto GET_FSK_RX_RESULT;//0x0a means rx is doing and pending
			if(resp)break;
			
			if(answer_data_len<2){resp=200/*ERR_IRDA_RECV*/;break;}
			answer_data_len-=2;//delete the header 2 bytes:result,channel_no
			if(answer_data_len>LINK_PAYLOAD-2){resp=201/*ERR_IRDA_RECV*/;break;}
			tmpu=n_segments*(LINK_PAYLOAD-2)+answer_data_len;
			if(tmpu>DATA_SIZE)
			{
				answer_data_len=DATA_SIZE-n_segments*(LINK_PAYLOAD-2);
				resp=203/*ERR_IRDA_RECV*/;
				memcpy(RecvData+n_segments*(LINK_PAYLOAD-2),answer_para[0]+2,answer_data_len);
				break;
			}
			memcpy(RecvData+n_segments*(LINK_PAYLOAD-2),answer_para[0]+2,answer_data_len);
			rn+=answer_data_len;
			n_segments++;
			if(answer_data_len<LINK_PAYLOAD-2)break;
			if(rn>=DATA_SIZE)break;
			
			goto GET_FSK_RX_RESULT;//receive the following packets
		}			
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0){resp=ERR_IRDA_RECV;break;}

		if(GetTimerCount()-io_start > TIMEOUT_REPLY)
		{
			resp=0xff;
			break;
		}
	}//while(12)
	
	*len=rn;
	return resp;
}

uchar ModemRxd_Proxy(uchar *RecvData,ushort *len)
{
	uchar a,status,tmps[DATA_SIZE+10];
	ushort tmpu;

	*len=0;
	
	if(is_fsk)
	{
		a=MdmFskRxd(RecvData,len);
		return a;
	}

	//--check modem status,Modified on 2014.5.15
	link_send(M_MODEM,F_MDM_CHECK,0,0,tmps);
	status=tmps[0];	
	if(is_sync)
	{
		//B1:rx data existence reported by baseset
		a=tmps[1];//rx_pool state of the base

		//--if line broken and base occupied, report 0x0c
		if(a && status && status!=0x09 && status!=0x08 && status!=0x01)return 0x0c;
	}
	
	//--receive data 
	link_send(M_MODEM,F_MDM_RXD,0,0,tmps);
	tmpu=(tmps[0]<<8)+tmps[1];//first 2 bytes are data length
	
	if(!tmpu)
	{
		if(status && status!=0x09 && status!=0x08 && status!=0x01)return status;
		return 0x0c;//empty
	}

	*len=tmpu;
	memcpy(RecvData,tmps+2,tmpu);
	
	return 0;
}

uchar ModemReset_Proxy(void)
{
	int tmpd;

	if(!is_sync)//added on 2014.5.19
	{
	//--if off base,waiting or aborted according to ScrSetEcho()
	tmpd=OnbaseCheck();
	if(tmpd<0)return ERR_IRDA_SEND;

	//--clear rx data 
	link_send(M_MODEM,F_MDM_RESET,0,0,NULL);
	}

	return 0;
}

uchar ModemAsyncGet_Proxy(uchar *ch)
{
	uchar status,a,tmps[10];

	//--check modem status
	link_send(M_MODEM,F_MDM_CHECK,0,0,tmps);
	status=tmps[0];
	
	//--receive one byte 
	a=link_send(M_MODEM,F_MDM_ASYNC_GET,0,0,ch);
	if(a)//empty or SYNC
	{
		if(status && status!=8 && status!=9	&& status!=0x01)
				return status;

		return 0x0c;//data absent
	}

	return 0;
}

void ModemProcess(uchar ucIndex, ushort uiDataLen,uchar *pData)
{
     memcpy(answer_data_buff,pData,uiDataLen);
     answer_eventFlag[ucIndex]=1;//置事件标志
	 answer_data_len=uiDataLen;
}

uchar AnswerModemParse(uchar is_fsk_rx)
{
     uchar uRet;
     uchar paranum,i;
     ushort len;
     ushort offset=0;
  
	 uRet=answer_data_buff[offset++];
	 if(is_fsk_rx)
	 {
		memcpy(answer_para[0],answer_data_buff,answer_data_len);
		return uRet;
	 }

      //fetch parameter from IRDA buff
      memset(answer_para,0x00,sizeof(answer_para));	  
      paranum=answer_data_buff[offset++];
	  
      for(i=0;i<paranum;i++)
      {
        len=answer_data_buff[offset];
		offset+=2;
		memcpy(answer_para[i],answer_data_buff+offset,len);
		offset+=len;
      }

     return uRet;
}

#if MODEM_DEBUG
void one_two(uchar *in,int lc,uchar *out)
{
	uchar tmp;
	int i,len;

	len=lc;
	for(i=0;i<len;i++)
	{
		tmp=in[i]/16;
		if(tmp<10)
			out[2*i]=tmp+'0';
		else
			out[2*i]=toupper(tmp)+'A'-0x0A;

		tmp=in[i]%16;
		if(tmp<10)
			out[2*i+1]=tmp+'0';
		else
			out[2*i+1]=toupper(tmp)+'A'-0x0A;
	}
	out[2*i]=0;
}

/*
void two_one(uchar *in,int in_len,uchar *out)
{
	uchar tmpc;
	int i;

	for(i=0;i<in_len;i+=2)
	{
		tmpc=toupper(in[i]);
		if(tmpc>='A'&&tmpc<='F')tmpc=tmpc-('A'-0x0A);
		else      tmpc&=0x0f;
		tmpc<<=4;
		out[i/2]=tmpc;

		tmpc=toupper(in[i+1]);
		if(tmpc>='A'&&tmpc<='F')tmpc=tmpc-('A'-0x0A);
		else         tmpc&=0x0f;
		out[i/2]+=tmpc;
	}
}
*/

uchar rcv_sdlc_pack(uchar *str,ushort *dlen)
{
	  uchar tmpc,resp,tmps[10];
	  ushort i,j,pn;

	  *dlen=0;
	  j=0;
	  s_TimerSet(5,1000);
	  resp=0;
	  pn=100;
	  while(1)
	  {
		  if(!s_TimerCheck(5)){resp=1;goto R_END;}

		  if(!kbhit()&&getkey()==KEYCANCEL)
		  {
				ScrPrint(0,0,0,"MODEM_RXD QUIT!");
				resp=4;
				goto R_END;
		  }

		  tmpc=ModemAsyncGet(str+j);
		  if(tmpc==0x0c)
		  {
		  	ScrPrint(0,0,0,"AsyncGet:%0x,j:%d!",tmpc,j);
		  	//DelayMs(100);

		  	continue;
		  }
		  if(tmpc==0xfd)
		  {
				ScrPrint(0,0,0,"RXD RESULT:%d",tmpc);
				resp=2;
				goto R_END;
		  }
		  else if(tmpc)
		  {
				ScrPrint(0,0,0,"RXD RESULT:%d",tmpc);
				resp=3;
				goto R_END;
		  }

		  s_TimerSet(5,100);

		  if(str[0]==0x02)
			j++;

		  if(j==3)
		  {
			one_two(str+1,2,tmps);
			pn=atoi((char*)tmps);
		  }

		  if(j==pn+5)break;

		  if(j>=2048)//1030
		  {
				ScrPrint(0,0,0,"RXD OVERFLOW   ");
				resp=9;
				goto R_END;
		  }

	  }//while(1)

	  if(str[0]!=0x02)
	  {
		ScrPrint(0,0,0,"ERR RXD STX:%02X ",str[0]);
		resp=5;
		goto R_END;
	  }

	  if(str[j-2]!=0x03)
	  {
		ScrPrint(0,0,0,"ERR RXD ETX:%02X ",str[j-2]);
		resp=6;
		goto R_END;
	  }

	  for(tmpc=0,i=1;i<j;i++)tmpc^=str[i];
	  if(tmpc)
	  {
	    static uint lrcerr=0;
		ScrPrint(0,0,0,"ERR RXD LRC %d!",++lrcerr);
		getkey();
		resp=7;
		goto R_END;
	  }

	R_END:
	  *dlen=j;

	  return resp;
}

uchar fetch_nac_pack(uchar port_no,uchar *out_pack,ushort *out_dlen)
{
	static ushort pn,i=0,si_flag=0;
	static uchar d_pool[2500];
	ushort wn,tmpu_data;
	uchar a,b;
	if(PortRecv(port_no,d_pool+i,0))return 1;//no data found
	i++;
	switch(si_flag)
	{
	case 0:
		a=d_pool[0];
		// 1--detect if the header is right, else search the first header
		if(a!=0x02){i=0;return 2;}
		si_flag=1;
	case 1:
		// 3--detect if it's an entire pack
		
		if(i<5)return 4;//not an entire pack 1
		a=d_pool[1];
		b=d_pool[2];
		pn=(a>>4)*1000+(a&0x0f)*100+(b>>4)*10+(b&0x0f)+5;
		if(pn>2053)//invalid pack length,data may be lost
		{
			i=0;//throw away all the receiving data
			si_flag=0;
			return 5;

		}
    		si_flag=2;
	case 2:
		if(i<pn)return 6;//not an entire pack 2
		// 3--fetch a pack with header and tail from receive buffer
		memcpy(out_pack,d_pool,pn);

		for(a=0,wn=1;wn<pn;wn++)a^=out_pack[wn];

		// 4--reply for the request pack
		si_flag=0;
		if(a)
		{
			i=0;
			return 7;//LRC error
		}

		*out_dlen=pn;
		i=0;
		return 0;
	 default:
		si_flag=0;
		i=0;
		return 8;
	 }//switch(a)
}


void modem_test(void)
{
	  uchar a,tmpc,tel_no[100],tmps[3000],xstr[3000];
	  int i,sn,rn,tmp;
	  MODEM_CONFIG mdm;
	  int ctl;
	  int dial_count[256];
	  int t_count[256];
	  int r_count[300];
	  int total=0; //总执行次数
	  uchar print_buff[50];
	  int countss=0 ;
	  
	int j,k,l,dn,loops;
	uchar pool[2500];
	memset(dial_count,0x0,256);
	memset(t_count,0x0,256);
	memset(r_count,0x0,300);
	 while(1)
	 {
		 ScrCls();
		 ScrPrint(0,0,0, "1-PICKUP 3-AT_CMD02A");
		 ScrPrint(0,1,0, "4 - COM1 TEST");
		 ScrPrint(0,2,0, "5 - MODEM DIAL");
		 ScrPrint(0,3,0, "6 - MODEM DIAL2");
		 ScrPrint(0,4,0, "7 - ONHOOK");
		 ScrPrint(0,5,0, "8 - ASYNC TEST");
		 ScrPrint(0,6,0, "9 - TX LEVEL");
		 ScrPrint(0,7,0, "0-CALLEE  2-PULSE");
	 
		 tmpc = getkey();
		 //PortOpen(0,"115200,8,n,1");

		 //tmpc='6';
		 //delay11(1000);
		 if(tmpc==KEYCANCEL)return;

		  switch(tmpc)
		  {
             case '8':   //async_exchange
			     ScrCls();
				 memset(&mdm,0x00,sizeof(mdm));
				 memset(tel_no,0x00,sizeof(tel_no));
				 ScrPrint(0,1,0, "INPUT DIAL NO:");
				 for(i=0;1;i++)
				 {
				 	tmpc=getkey();
					if(tmpc==KEYENTER) break;
					tel_no[i]=tmpc;
				 	ScrPrint(20,2,0, "%s",tel_no);
				 }
				 if(!strcmp(tel_no,"983163166"))
				 {	
					 strcpy(tel_no,"983163166");
					 mdm.pbx_pause=10;
				 }

				 ScrCls();
				 ScrPrint(0,2,0, "SELECT SPEED:");
				 ScrPrint(0,3,0, "1.--1200");
				 ScrPrint(0,4,0, "2.--2400");
				 ScrPrint(0,5,0, "3.--9600");
				 ScrPrint(0,6,0, "4.--14400");
				 ScrPrint(0,7,0, "5.--336000");
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;

				 mdm.async_format=0x00;
				 //mdm.pattern=0x82;//1200bps
				 mdm.pattern=0x87;//1200bps
				 if(tmpc=='2')mdm.pattern|=0x20;
				 else if(tmpc=='3')mdm.pattern|=0x40;				 
				 else if(tmpc=='4') // 14400bps
				 {	
				 	 mdm.pattern=0x87;
				 	mdm.async_format=0x70;
				 }
				 else if(tmpc=='5') // 14400bps
				 {	
				 	 mdm.pattern=0x87;
				 	mdm.async_format=0xf0;
				 }

				 ScrCls();
				 ScrPrint(0,3,0, "set sn=key*1000!");
				 tmpc=getkey();				 
				 sn=(tmpc-'0')*1000;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key*100!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0')*100;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key*10!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0')*10;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0');
				 ScrCls();
				 
		ASYNC_DIAL_AGAIN:

					 
				 ScrCls();
				 ScrPrint(0,3,0, "DIALING %s...",tel_no);
				 tmpc=ModemDial(&mdm,tel_no,1);
				 dial_count[tmpc]++; //记录拨号结果值
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,3,0, "DIAL RESULT:%02X",tmpc);

				 }
				 else
				 {
				 	ScrCls();
				 	ScrPrint(0,3,0, "DIAL OK");
				 }

				 tmpc=rcv_sdlc_pack(xstr,&rn);// 取出上次剩余的包

				for(l=sn,loops=0;l<2045;loops++,l++)
				{	
				//	total++;
				   //if(total%10==0) goto ASYNC_END;
		   ABACK:
					dn=5+l;
					pool[0]=0x02;
					sprintf((char*)xstr,"%04d",dn);
					two_one(xstr,4,pool+1);
					memcpy(pool+3,"\x60\x00\x00\x00\x00",5);
					for(j=0;j<dn;j++)
					{
 						pool[8+j]=j%256;
					}
					pool[8]=l%256;
					pool[8+l]=0x03;
					for(tmpc=0,j=1;j<9+l;j++)tmpc^=pool[j];
					pool[9+l]=tmpc;
					dn=l+10;
					ScrPrint(0,2,0,"%03d,SND%d BYTES..",loops,dn);

					//ModemReset();

					tmpc=ModemTxd(pool,dn);
					t_count[tmpc]++; //记录发送结果
					
					if(tmpc)
					{
						ScrPrint(0,4,0,"F TO SND:%02X,%d,%d!",tmpc,dn,l);//FAILED TO SEND
					}
		//DelayMs(5000);
					ScrPrint(0,3,0,"%03d,RECV,%d..",loops,dn);
					tmpc=rcv_sdlc_pack(xstr,&rn);
					r_count[tmpc]++;

					if(!tmpc)
					{
						if(rn==dn&&!memcmp(pool,xstr,dn))
						{
							ScrPrint(0,1,0,"%03d,RCV OK,DN:%ld",loops,dn);
							r_count[258]++;	
							continue;
						}

						else
						{
							r_count[256]++;	
							continue;
						}

					}
					//r_count[257]++;
					if (tmpc==4)
					{
                        			OnHook();
						goto ASYNC_DIAL_AGAIN;
					}
								

				}//for(l)

ASYNC_END:        
               		 memset(print_buff,0,50);
				sprintf(print_buff,"\r\ntotal number: %d !\r\n",total-1);
                		PortSends(0,print_buff,strlen(print_buff));

				for(i=0;i<256;i++)
				{
					if(dial_count[i]==0) continue;
					DelayMs(50);
					memset(print_buff,0,50);
					sprintf(print_buff,"dial result[%0x] count: %d !\r\n",i,dial_count[i]);
					PortSends(0,print_buff,strlen(print_buff));
				}
               
				for(i=0;i<256;i++)
				{
					if(t_count[i]==0) continue;
					DelayMs(50);
					memset(print_buff,0,50);
					sprintf(print_buff,"send result[%0x] count: %d !\r\n",i,t_count[i]);
					PortSends(0,print_buff,strlen(print_buff));
				}

				for(i=0;i<300;i++)
				{
					if(r_count[i]==0) continue;
					DelayMs(50);
					memset(print_buff,0,50);
					sprintf(print_buff,"recv result[%0x] count: %d !\r\n",i,r_count[i]);
					
					PortSends(0,print_buff,strlen(print_buff));
				}

				goto ABACK;			
		
				OnHook();			
				break;
				
			 case KEYCLEAR:
				memset(&mdm,0x00,sizeof(mdm));
				mdm.mode=0;
				mdm.ignore_dialtone=0x02;
				mdm.pattern=0x47;

				ScrCls();
				ScrPrint(0,0,0,"SDLC SERVER");
				ScrPrint(0,6,0,"DIALLING ...");

				tmpc=ModemDial(&mdm,".",1);
				ScrPrint(0,6,0,"RESULT:%02X",tmpc);
				DelayMs(500);

				ScrClrLine(6,6);
				ScrPrint(0,6,0,"SERVING ...");
/*				tmpc=PortOpen(P_RS232A,"9600,8,N,1");
				if(tmpc)
				{
					ScrPrint(0,6,0,"PortOpen RESULT:%02X ",tmpc);
					Beep();
					getkey();
					return;
				}
*/
				while(1) 
				{
				  if(!kbhit())
				  {
					 tmpc=getkey();
					 if(tmpc==KEYCANCEL)break;
				  }

//				 tmpc=fetch_nac_pack(P_RS232A,xstr,&sn);
				 if(!tmpc)
				 {
					tmpc=ModemTxd(xstr,sn);
					ScrPrint(0,4,0,"Tx:%d,Sn:%d,...",tmpc,sn);
				 }

				// if(!ModemRxd(tmps,&rn))
//					PortSends(P_RS232A,tmps,rn);
			  }

				break;
			 case '1':
				ScrCls();
				memset(&mdm,0x00,sizeof(mdm));
				mdm.ignore_dialtone=0x41;//double timeout
				mdm.pattern=0x07;
				while(1)
				{
					ScrPrint(0,5,0, "PICKING UP...  ");
					tmpc=ModemDial(&mdm,tel_no,1);
					if(tmpc==0xfd)break;

					ScrPrint(0,5,0, "DIAL RESULT:%02X",tmpc);
					if(!kbhit() && getkey()==KEYCANCEL)break;
					Beep();

					DelayMs(1000);
				}
				break;
			 case '2':
				 ScrCls();

				 memset(&mdm,0x00,sizeof(mdm));
				 strcpy(tel_no,"1,2,3,4,5,6,7,8,9,*,0,#");

				 ScrPrint(0,2,0, "SELECT PULSE MODE:");
				 ScrPrint(0,3,0, "1.--61/39");
				 ScrPrint(0,4,0, "2.--67/33");

				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='2')mdm.mode=2;
				 else mdm.mode=1;
				 strcpy(tel_no,"1,2,3,4,5,6,7,8,9,*,0,#");
				 ScrCls();

				 ScrPrint(0,5,0, "DIALING %s...",tel_no);
				 //mdm.pattern=0x40;//9600bps
				 mdm.pattern=0x02;//1200bps
				 tmpc=ModemDial(&mdm,tel_no,1);
				 Beep();
				 ScrPrint(0,5,0, "DIAL RESULT:%02X",tmpc);
				 getkey();
				break;
			 case '3':
//				 PortOpen(P_MODEM,"115200,8,n,1");
//				 PortOpen(P_RS232A,"115200,8,n,1");
				 ScrCls();
				 ScrPrint(0,5,0, "DIRECT ACCESS");
				 while(1)
				 {
				  if(!kbhit())
				  {
					 a=getkey();
					 if(a==KEYCANCEL)break;
				  }

//				  if(!PortRecv(P_RS232A,&a,0))
//						PortSend(P_MODEM,a);
//				  if(!PortRecv(P_MODEM,&a,0))
//						PortSend(P_RS232A,a);
				}
				break;
			 case '4':
				 ScrCls();
/*				 tmpc=PortOpen(P_RS232A,"115200,8,n,1");
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "PORT_OPEN RESULT:%02X",tmpc);
					getkey();
					break;
				 }
*/				 
				 for(i=0;i<3000;i++)tmps[5+i]=i&0xff;
				 memcpy(tmps,"\x60\x00\x00\x00\x00",5);
				 sn=510;
				 while(sn<2060)
				 {

					ScrPrint(0,5,0, "%d,SENDING ...",sn);
					tmps[6]=sn&0xff;
//					tmpc=PortSends(P_RS232A,tmps,sn);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "PORTSENDS RESULT:%02X",tmpc);
					getkey();
					break;
				 }

				 ScrPrint(0,5,0, "%d,RECEIVING ...",sn);
				 rn=0;
				 while(11)
				 {
					if(!kbhit() && getkey()==KEYCANCEL){tmpc=0xfd;break;}

//					tmpc=PortRecv(P_RS232A,xstr+rn,0);
					if(tmpc)continue;
					rn++;
					ScrPrint(0,5,0, "%d,RECEIVING %d...",sn,rn);

					if(rn>=sn)break;
				  }//while(11)

				  if(tmpc)
				  {
					Beep();
					ScrPrint(0,5,0, "PORTRCV RESULT:%02X",tmpc);
					getkey();
					break;
				  }

				  if(rn!=sn)
				  {
					Beep();
					ScrPrint(0,5,0, "SN/RN mismatch:%d-%d",sn,rn);
					getkey();
					break;
				  }

				  if(memcmp(tmps,xstr,sn))
				  {
					Beep();
					ScrPrint(0,5,0, "data mismatch,SN:%d",sn);
					while(1)
					{
						ScrPrint(0,7,0, "ANY KEY ...");
						if(getkey()==KEYCANCEL)break;
//						tmpc=PortSends(P_RS232A,xstr,rn);
						ScrClrLine(6,7);
						for(i=0;i<sn;i++)
							if(tmps[i]!=xstr[i])
							{
								Beep();
								ScrPrint(0,6,0, "OFFSET:%d,%02X-%02X",i,tmps[i],xstr[i]);
								DelayMs(1000);
								getkey();
							}
					}
					break;
				  }
					ScrPrint(0,6,0, "%d,RECEIVED %d ",sn,rn);

					sn=(sn+1)%2060;
					if(!sn)sn=1;
				 }//while

//				 PortClose(P_RS232A);
				break;
			 case '5':
			ScrCls();

				 tmpc=PortOpen(P_MODEM,"115200,8,n,1");
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "PORT_OPEN RESULT:%02X",tmpc);
					getkey();
					break;
				 }

				 memset(&mdm,0x00,sizeof(mdm));

				 ScrPrint(0,1,0, "SELECT DIAL NO:");
				 ScrPrint(0,2,0, "1.--86");
				 ScrPrint(0,3,0, "2.--8740");
				 ScrPrint(0,4,0, "3.--1,2,3,...,0");
				 ScrPrint(0,5,0, "4.--8704");  //wu
				 ScrPrint(0,6,0, "5.--983163166");
				 ScrPrint(0,7,0, "X.--8335");

				 tmpc=getkey();
				
				 //delay11(1000);
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='1')strcpy(tel_no,"86");
				 else if(tmpc=='2')strcpy(tel_no,"8740");
				 else if(tmpc=='3')
				 {
					strcpy(tel_no,",1,2,3,4,5,6,7,8,9,*,0,#");
					mdm.code_hold=100;
				 }
				 else if(tmpc=='4')strcpy(tel_no,"8704");
				 else if(tmpc=='5')strcpy(tel_no,"983163166");
				 else 
				 {	
					 strcpy(tel_no,"8335");
					 mdm.pbx_pause=10;
					 //mdm.code_hold=100;
				 }
				 ScrCls();
				 ScrPrint(0,2,0, "SELECT SPEED:");
				 ScrPrint(0,3,0, "1.--1200");
				 ScrPrint(0,4,0, "2.--2400");
				 ScrPrint(0,5,0, "3.--9600");

				 tmpc=getkey();
				 //tmpc='1';
				 //delay11(1000);
				 if(tmpc==KEYCANCEL)break;

				 //mdm.pattern=0x02;//1200bps
				 mdm.pattern=0x07;//1200bps
				 if(tmpc=='2')mdm.pattern|=0x20;
				 else if(tmpc=='3')mdm.pattern|=0x40;

	AGAIN:
				 ScrCls();

				 ScrPrint(0,3,0, "DIALING %s...",tel_no);
				 tmpc=ModemDial(&mdm,tel_no,1);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,3,0, "DIAL RESULT:%02X",tmpc);
					getkey();
					break;
				 }
				 Beep();
				 ScrPrint(0,3,0, "DIAL OK");
				 //delay11(1000);
				 //ScrPrint(0,7,0, "5-REDIAL,OTHER-TX/RX");
				 tmpc=getkey();
				 if(tmpc=='5')
				 {
				 	OnHook();
					//s_ModemInit(2);
					goto AGAIN;
				  }
				 ScrCls();

				 for(i=0;i<3000;i++)tmps[i]=i&0xff;
				 memcpy(tmps,"\x60\x00\x00\x00\x00",5);
				/* 
                 		ScrPrint(0,3,0, "set sn=key*200+200!");
				 tmpc=getkey();
				 sn=(tmpc-'0')*200+200;
				 */
				 ScrCls();
				 ScrPrint(0,3,0, "set sn=key*1000!");
				 tmpc=getkey();				 
				 sn=(tmpc-'0')*1000;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key*100!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0')*100;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key*10!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0')*10;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0');
				 ScrCls();				 
				 
				// sn=300;//510,520,108,109
				 while(sn<2060)
				 {

					ScrPrint(0,2,0, "!%d,SENDING ...",sn);
					//tmps[6]=sn&0xff;
					tmpc=ModemTxd(tmps,sn);
					//tmpc=MdmTx(tmps,sn);
					 if(tmpc)
					 {
						Beep();
						ScrPrint(0,3,0, "MODE_TXD RESULT:%02X",tmpc);
						getkey();
						break;
					 }
				
					ScrPrint(0,3,0, "!%d,RECEIVING ...",sn);
					 while(11)
					 {
						if(!kbhit() && getkey()==KEYCANCEL){tmpc=0xfd;break;}

						tmpc=ModemRxd(xstr,&rn);
						if(tmpc!=0x0c)break;
					  }//while(11)

					  if(tmpc)
					  {
						Beep();
						ScrPrint(0,3,0, "MODE_RXD RESULT:%02X",tmpc);
						getkey();
						break;
					  }

					  if(rn!=sn)
					  {
						Beep();
						ScrPrint(0,3,0, "SN/RN mis:%d-%d!",sn,rn);
						getkey();
						break;
					  }

					  if(memcmp(tmps+5,xstr+5,sn-5))
					  {
						Beep();
						ScrPrint(0,3,0, "data mis,SN:%d!",sn);
						getkey();
						break;
					  }
					  ScrPrint(0,1,0, "!%d,RECEIVED %d !",sn,rn);
				
					sn++;
					//getkey();
					//OnHook();
					//goto AGAIN;

				 }//while
				 OnHook();
				 break;

		case '6':				 
BACK:  			ScrCls();
				memset(&mdm,0x00,sizeof(mdm));
				 memset(tel_no,0x00,sizeof(tel_no));
				 ScrPrint(0,1,0, "INPUT DIAL NO:");
				 for(i=0;1;i++)
				 {
				 	tmpc=getkey();
					if(tmpc==KEYENTER) break;
					tel_no[i]=tmpc;
				 	ScrPrint(20,2,0, "%s",tel_no);
				 }
				 if(!strcmp(tel_no,"983163166"))
				 {	
					 strcpy(tel_no,"983163166");
					 mdm.pbx_pause=10;
				 }

				 ScrCls();
				 ScrPrint(0,2,0, "SELECT SPEED:");
				 ScrPrint(0,3,0, "1.--1200");
				 ScrPrint(0,4,0, "2.--2400");
				 ScrPrint(0,5,0, "3.--9600");

				 tmpc=getkey();
				 //tmpc='1';
				 //delay11(1000);
				 if(tmpc==KEYCANCEL)break;

				 //mdm.pattern=0x02;//1200bps
				 mdm.pattern=0x07;//1200bps
				 if(tmpc=='2')mdm.pattern|=0x20;
				 else if(tmpc=='3')mdm.pattern|=0x40;
                         
                       
			     	 ScrCls();
				 ScrPrint(0,3,0, "set sn=key*1000!");
				 tmpc=getkey();				 
				 sn=(tmpc-'0')*1000;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key*100!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0')*100;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key*10!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0')*10;
				 ScrCls();
				 ScrPrint(0,3,0, "set sn+=key!");
				 tmpc=getkey();				 
				 sn+=(tmpc-'0');
				 ScrCls();
				 j=0;
				 
				 ScrPrint(0,0,0, "!mdm check:%02X!",ModemCheck());
DIAL_AGAIN:
				 //total++;
				 //if(total%10==0) goto SYNC_END;

		
				 
				ScrPrint(0,3,0, "DIALING %s...",tel_no);
				tmpc=ModemDial(&mdm,tel_no,1);
				/*ModemDial(&mdm,tel_no,0);
				while(1)
				{
					ScrClrLine(0,0);
						ScrPrint(0,0,0, "check:%d",total++);
					if(ModemCheck()!=0x0a) {tmpc=0;break;}
				}*/
				 dial_count[tmpc]++;

				 if(tmpc)
				 {
					ScrPrint(0,4,0, "DIAL fail:%02X!",tmpc);
				 }
				 else
				 {
 					//ScrCls();
 					ScrPrint(0,4,0, "DIAL OK");
					//DelayMs(300);
				 }
				 
				//getkey();
				 
				 for(i=0;i<3000;i++)tmps[i]=i&0xff;
				 //for(i=0;i<3000;i++)tmps[i]=0x12;
				 memcpy(tmps,"\x60\x00\x00\x00\x00",5);
				 memset(tmps+5,0x22,8);
				   
		SENDAGAIN:
		            //total++;
				 //if(total%10==0) goto SYNC_END;

				if(sn>2045)
					sn=100;
				if(sn==1200)
					sn=1201;
				if(sn==1800)
					sn=1801;
				
				ScrPrint(0,2,0, "!%d,SENDING ...",sn);
               
				tmpc=ModemTxd(tmps,sn);
				t_count[tmpc]++;
		
				ScrPrint(0,3,0, "!%d,RECEIVING ...",sn);
                             memset(xstr,0xff,3000);
	                    while(1)
	                    {
               		  tmpc=ModemRxd(xstr,&rn);
               		  if (tmpc != 0x0C)
               		    break;
               		  if (!kbhit() && (getkey() == KEYCANCEL))
               		    goto END1;
               		}
			         
				  
				  if(tmpc)
				  {
					r_count[tmpc]++;
					j++;
					ScrPrint(0,4,0, "M_R RESULT:%02X",tmpc);
					//getkey();
					/*for(i=0;i<rn;i++)
						com0_Print("%02X,",xstr[i]);
				  	getkey();*/
		                    
				  }

				  if(rn!=sn)
				  {
                         		r_count[256]++;  //SN/RN mis
                         		j++;
				       ScrPrint(0,4,0, "M_R rn:%d,sn:%d,%02X !",rn,sn,tmpc);
					//getkey();
					/*for(i=0;i<rn;i++)
						com0_Print("%02X,",xstr[i]);
                         		getkey();*/

				  }

				  if(memcmp(tmps,xstr,sn))
				  {
					   r_count[257]++;   //data mis
					   j++;
					   ScrPrint(0,4,0, "MODE_RXD cmp err!",tmpc);
					   //getkey();
					   /*for(i=0;i<rn;i++)
						com0_Print("%02X,",xstr[i]);
					   getkey();*/
				  }
				  r_count[258]++;        //COMM OK
                          
			 END1:
			 	ScrPrint(0,1,0, "!%d,REC %d,err %d!",sn,rn,j);
				ScrPrint(0,5,0, "!mdm check:%02X!",ModemCheck());
				
                             sn++;
			        OnHook();
			        //goto BACK;
					goto DIAL_AGAIN;
                            
			 	goto SENDAGAIN;
			 	
				
		SYNC_END:        
                		memset(print_buff,0,50);
				sprintf(print_buff,"\r\ntotal number: %d !\r\n",total-1);
                		PortSends(0,print_buff,strlen(print_buff));

				for(i=0;i<256;i++)
				{
					if(dial_count[i]==0) continue;
					DelayMs(50);
					memset(print_buff,0,50);
					sprintf(print_buff,"dial result[%0x] count: %d !\r\n",i,dial_count[i]);
					PortSends(0,print_buff,strlen(print_buff));
				}
               
				for(i=0;i<256;i++)
				{
					if(t_count[i]==0) continue;
					DelayMs(50);
					memset(print_buff,0,50);
					sprintf(print_buff,"send result[%0x] count: %d !\r\n",i,t_count[i]);
					PortSends(0,print_buff,strlen(print_buff));
				}

				for(i=0;i<300;i++)
				{
					if(r_count[i]==0) continue;
					DelayMs(50);
					memset(print_buff,0,50);
					if(i>255||i==0)
						sprintf(print_buff,"recv result[%0x] count: %d !\r\n",i,r_count[i]);
					else
						sprintf(print_buff,"recv result[%0x] count: %d !\r\n",i,r_count[i]/2);
					
					PortSends(0,print_buff,strlen(print_buff));
				}

				goto BACK;

				 OnHook();
				 break;
			 case '7':
				 ScrCls();
				 ScrPrint(0,5,0, "ONHOOK ...");
				 OnHook();
//				 PortClose(P_PINPAD);
				 DelayMs(1000);
				 break;
		/*
			 case '8':
				ScrCls();
//				tmpc=PortOpen(P_PINPAD,"9600,8,n,1");
//				ScrPrint(0,5,0, "PINPAD OPEN RESULT:%02X",tmpc);
				getkey();
				break;
		*/
			 case '9':
				 ScrCls();

				 ScrPrint(0,2,0, "SELECT TX LEVEL:");
				 ScrPrint(0,3,0, "0.--15");
				 ScrPrint(0,4,0, "ENTER.--11");
				 ScrPrint(0,5,0, "CLEAR.--12");
				 ScrPrint(0,6,0, "CANCEL.--EXIT");

				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc>'0' && tmpc<='9')tmpc-='0';
				 else if(tmpc==KEYENTER)tmpc=11;
				 else if(tmpc==KEYCLEAR)tmpc=12;
				 else tmpc=10;
				sprintf(tmps,"ATS91=%d",tmpc);
				ModemExCommand(tmps,NULL,NULL,0);
				break;
			 case '0':
				 ScrCls();
				 memset(&mdm,0x00,sizeof(mdm));
				 //mdm.pattern=0x40;//9600bps
				 mdm.ignore_dialtone=0x02;

				 ScrPrint(0,2,0, "SELECT ASYNC/SYNC:");
				 ScrPrint(0,3,0, "1.--ASYNC");
				 ScrPrint(0,4,0, "2.--SYNC");
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='1')mdm.pattern=0x87;
				 else mdm.pattern=0x07;

				 ScrCls();
				 ScrPrint(0,2,0, "SELECT SPEED:");
				 ScrPrint(0,3,0, "1.--1200");
				 ScrPrint(0,4,0, "2.--2400");
				 ScrPrint(0,5,0, "3.--9600");
				 ScrPrint(0,6,0, "4.--14400");

				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;

				 if(tmpc=='2')mdm.pattern|=0x20;
				 else if(tmpc=='3')mdm.pattern|=0x40;
				 else if(tmpc=='4')mdm.pattern|=0x60;
				 else if(tmpc=='9')mdm.async_format=0xf0;
CALLEE_RETRY:

				tmpc=ModemDial(&mdm,tel_no,1);
				Beep();
				ScrCls();
				ScrPrint(0,2,0, "PATTERN:%02X",mdm.pattern);
				ScrPrint(0,3,0, "CALLEE RESULT:%02X",tmpc);
				ScrPrint(0,4,0, "5-RETRY,6-ONHOOK");
				if(mdm.pattern&0x80)strcpy(tmps,"115200,8,n,1");
				else strcpy(tmps,"9600,8,n,1");
				PortOpen(RS232A,tmps);
				ScrPrint(0,7,0, "EXTERNAL:%s",tmps);
				

				while(1)
				{

					if(!kbhit())
					{
						tmpc=getkey();
						if(tmpc==KEYCANCEL)break;
						if(tmpc=='5')goto CALLEE_RETRY;
						if(tmpc=='6'){OnHook();getkey();break;}
					}
					ScrPrint(0,6,0, "STATUS:%02X ",ModemCheck());
/*					
					if(!ModemRxd(tmps,&rn))
					{
						ScrPrint(0,0,0,"RN:%d ",rn);
						ModemTxd(tmps,rn);
					}
*/
					//ScrPrint(0,0,0,"PortRecv:          ");
					//ScrPrint(0,1,0,"ModemRxd      ");
					sn=1;
//countss++;
//ScrPrint(0,0,0,"%d-PortRecv......",countss,tmpc,sn);
					if(mdm.pattern&0x80)
						tmpc=PortRecv(RS232A,xstr,0);
					else	tmpc=fetch_nac_pack(RS232A,xstr,&sn);
//ScrPrint(0,1,0,"%d-Recv:%d,SN:%d",countss,tmpc,sn);
//if(sn!=1)
//ScrPrint(0,2,0,"%d-tRecv:%d,SN:%d",countss,tmpc,sn);
					if(!tmpc)tmpc=ModemTxd(xstr,sn);
//ScrPrint(0,3,0,"%d-MdmRxd...",countss);
					if(!ModemRxd(tmps,&rn))
					{
//ScrPrint(0,4,0,"%d-MdmRxd---%d !",countss,rn);
						PortSends(RS232A,tmps,rn);
					}
					

					//if(!ModemAsyncGet(&a))
					//	PortSend(RS232A,a);
				}
				PortClose(RS232A);
				break;
				
		  }//switch()
	 }//while(1)
}
#endif
  
