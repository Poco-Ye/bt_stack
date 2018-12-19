#include  <string.h>
#include  <stdarg.h>
#include  "base.h"
#include  "..\comm\comm.h"
#include "..\lcd\lcddrv.h"

#define TM_SYS 5
#define LINK_PAYLOAD 1024
#define TO_REPLY 8000
#define F_GET_BASE_INFO 10
#define F_HANDSET_BASE_LINK_MUTEX	15
#define F_SEND_BIN 		32
#define F_SEND_UPDATE	33

BASE_INFO base_info;
static volatile uchar VerFlag=0;
static volatile uchar BinFlag=0;
static volatile int Binresult=0;
static volatile uchar LinkMutexFlag = 0;
static volatile int LinkMutexResult = 0;
volatile char g_GetBaseInfoFlag = 0;
// OnBase()返回0表示在座机上
uchar OnBase(void)
{
	uchar val;
    // 根据外电信号判断手机是否在座机上
    // 手机在座机上是指:座机电源打开,并且手机和座机物理接触	
	val = gpio_get_pin_val(GPIOB,7);
	return (val == 0) ? 0 : 0xff;
}

int SysProcess(uchar func_no,ushort uiDataLen,uchar *pucData)
{
	int index;
	//if(func_no!=F_GET_BASE_INFO )return 1;
	switch(func_no)
	{
	case F_GET_BASE_INFO:
		if(uiDataLen<sizeof(base_info))return 2;
		VerFlag=1;
		memset(&base_info,0,sizeof(base_info));
		memcpy(&base_info,pucData,sizeof(base_info));
		if(!g_GetBaseInfoFlag)
		{
			if(base_info.bt_exist)
			{
				SetBaseType(HANDSET_BTLINK);
				SetLinkType(1);
			}
			else
			{
				SetBaseType(HANDSET_UARTLINK);
				SetLinkType(0);
			}
		}
		return 0;	
	case F_HANDSET_BASE_LINK_MUTEX:
		if(uiDataLen<4) return 2;
		memcpy((uchar *)&LinkMutexResult,pucData,sizeof(int));
		LinkMutexFlag = 1;
		return 0;
	case F_SEND_BIN:		
	case F_SEND_UPDATE:
		if(uiDataLen<4) return 2;
		Binresult = *(int*)pucData;
		BinFlag=1;
		return 0;
	default:
		return 1;
	}	
}

int SendBaseDriver(uchar *PtrMonitor, long len,uchar flag)
{
	uchar tmpc;
	int tmpd;
	uint t0;	
	int size = len;
	uchar *pbuf=PtrMonitor;
	uchar packet[LINK_PAYLOAD];
	uint cur_bin_pos=0;
	int FRAME_LEN = LINK_PAYLOAD-32;
	
	if (PtrMonitor == NULL || len==0) return -12;

	LinkReset();
	if(flag)
	{
		ScrClrLine(6,6);
	}
	while(size>0)
	{	
		if(flag)
			ScrPrint(0, 6, 1, "Sending ...%02d%%",100*(len-size)/len);
		BinFlag = 0;
		Binresult =0;		
		if(size>=FRAME_LEN)
		{			
			memcpy(packet,&cur_bin_pos,4);
			memcpy(packet+4,&FRAME_LEN,4);
			memcpy(packet+8,pbuf+cur_bin_pos,FRAME_LEN);
			size-=FRAME_LEN;			
			cur_bin_pos+=FRAME_LEN;	
			tmpc=link_send(TM_SYS,F_SEND_BIN,0,FRAME_LEN+8,packet);	
		}
		else
		{
			memcpy(packet,&cur_bin_pos,4);
			memcpy(packet+4,&size,4);
			memcpy(packet+8,pbuf+cur_bin_pos,size);	
			tmpc=link_send(TM_SYS,F_SEND_BIN,0,size+8,packet);	
			size=0;			
		}
		if(tmpc) return -1;	
		t0=GetTimerCount();
		while(1)
		{
			if(BinFlag) 
			{
				if(Binresult) return Binresult;
				break;
			}			
			tmpd=OnbaseCheck();
			if(tmpd<0)	return -2;
			if(tmpd>0)  t0=GetTimerCount();		
			if(GetTimerCount()-t0 > TO_REPLY)return -3;
		}//while(1)					
	}

	BinFlag =0; 
	Binresult =0;
	if(flag)
	{
		ScrClrLine(6,6);
		ScrPrint(0, 6, 1, "Updating ...  ");
	}
	
	tmpc=link_send(TM_SYS,F_SEND_UPDATE,0,0,NULL);
	if(tmpc) return -1;
	t0=GetTimerCount();
	while(1)
	{
		if(BinFlag) 
		{
			if(Binresult) return Binresult;
			break;
		}	
		tmpd=OnbaseCheck();
		if(tmpd<0)	return -2;
		if(tmpd>0)  t0=GetTimerCount();		
		if(GetTimerCount()-t0 > TO_REPLY*2)	return -3;			
	}//while(1)
	if(flag)
	{	
		ScrClrLine(6,6);
		ScrPrint(0, 6, 1, "Update Finished ");
	}
	LinkReset();
	return 0;
}

// 获取座机版本信息的后台处理
int GetBaseVerBack(void)
{
	unsigned char tmpc;
	int tmpd,result;
	static uint t0;
	static uchar step;
	if(g_GetBaseInfoFlag) return 0;
	switch(step)
	{
		case 0: goto step0;
		case 1: goto step1;
	}
	
	step0:
	tmpc=link_send(TM_SYS,F_GET_BASE_INFO,0,0,NULL);
	if(tmpc)return -1;
	VerFlag=0;
	t0=GetTimerCount();
	step = 1;
	return 0;
	step1:
	if(VerFlag)
	{
		step = 2;
		goto step2;
	}
	//--if off base,waiting or aborted according to ScrSetEcho()
	tmpd=OnbaseCheck();
	if(tmpd<0)
	{
		step = 0;
		return -2;
	}
	if(tmpd>0)
	{
		t0=GetTimerCount();//reset timeout start
	}
	if(GetTimerCount()-t0 > TO_REPLY)
	{
		step = 0;
		return -3;
	}
	return 0;
	step2:
	g_GetBaseInfoFlag = 1;
	return 0;
}

int GetBaseVerExecute(void)
{
	unsigned char tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	unsigned int t0;
	
	VerFlag=0;
	tmpc=link_send(TM_SYS,F_GET_BASE_INFO,0,0,NULL);
	if(tmpc)return -1;
	
	t0=GetTimerCount();
	while(1)
	{
		if(VerFlag)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return -2;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)return -3;
	}//while(1)
	g_GetBaseInfoFlag = 1;
	return 0;
}

int GetBaseVer(void)
{
	uchar linktypebak,basetypebak,switchflag;
	int iRet;
	
	switchflag = 0;
	if(OnbaseCheck())			// 为蓝牙链路，但是蓝牙未连接，需要通过串口方式获取
	{
		if(OnBase()) return -1;
		linktypebak = GetLinkType();
		basetypebak = GetBaseType();
		SetLinkType(0);
		SetBaseType(HANDSET_UARTLINK);
		switchflag = 1;
	}
	iRet = GetBaseVerExecute();
	if(switchflag)
	{
		SetLinkType(linktypebak);
		SetBaseType(basetypebak);
	}
	return iRet;
}

int BtCtrl_GetBaseBt(uchar *pArgOut,uint iSizeOut)
{
	uint t0,index;
	uchar offbase;
	int iRet;
	// 蓝牙链路蓝牙连接着或者是串口链路手持机在位可以直接获取当前的座机bt信息
	offbase = 0;
	memset(pArgOut,0,iSizeOut);
	if(1 == GetLinkType())				// 蓝牙链路
	{
		if(OnbaseCheck())					// 蓝牙链路未成功建立
		{
			if(!OnBase())					// 座机在位
			{
				t0 = GetTimerCount();
				while(1)
				{
					if(GetTimerCount() > t0 + 1500)
					{
						return -1;						// 超时
					}
					if(0 == GetBaseBT())
					{
						break;
					}
					DelayMs(30);
				}
			}
			else							// 座机不在位
			{
				return -2;
			}
		}
		else
		{
			iRet = GetBaseVerExecute();
			if(iRet) return iRet;
		}
	}
	else if(0 == GetLinkType())			// 串口链路
	{
		if(!OnBase())					
		{
			iRet = GetBaseVerExecute();
			if(iRet) return iRet;
		}
		else
		{
			return -2;
		}
	}
	else								// 单手机使用
	{
		return -2;
	}

	if(!offbase)
	{
		index = 0;
		pArgOut[0] = base_info.bt_exist;
		index++;
		if(base_info.bt_exist)
		{
			memcpy(&pArgOut[index],base_info.bt_mac,22);
			index += 22;
			memcpy(&pArgOut[index],base_info.bt_name,20);
			index += 20;
		}
	}
	return 0;
}

//PICC Led????
static uchar PiccLedEnable=0,StatusBaseLed=0,PiccOpenSta=0;
//SysConfigμ÷ó?????
void s_PiccLedCtrl(int val)
{
	PiccLedEnable=val;
	GetBaseVer();
}
int s_SetPiccOpenSta(int st)
{
	PiccOpenSta=st;
}
#define TAG_PICC_LED 0x71
//PICC Ledí?2?°ü
int PackPiccLedSync(uchar *p)
{
	*p++=TAG_PICC_LED;
	*p++=2;
	if(PiccLedEnable && PiccOpenSta)
	{
		*p++=PiccLedEnable;
	}
	else
	{
		*p++=0;
	}
	*p++=StatusBaseLed;
	return 4;
}
void BasePiccLight(uchar ucIndex,uchar ucOnOff)
{
	if(ucOnOff)
	{
		StatusBaseLed |= ucIndex;
	}
	else
	{
		StatusBaseLed &= (~ucIndex);
	}
	GetBaseVer();
}

// count￡oμ￥??10ms
static T_SOFTTIMER s_softTM[20];
void s_TimerSet(unsigned char TimerNo, unsigned long count)
{
    if(TimerNo >20) return;
    s_TimerSetMs(&s_softTM[TimerNo],count*10);
}

unsigned long s_TimerCheck(unsigned char TimerNo)
{
    if(TimerNo >20) return 0;
    return s_TimerCheckMs(&s_softTM[TimerNo])/10;
}


int MutexHandsetBaseLink(uchar mutex)
{
	uchar tmpc,tmps[20];
	int tmpd,result;
	unsigned int t0;

	memset(tmps,0,sizeof(tmps));
	tmps[0] = mutex;
	LinkMutexFlag = 0;
	LinkMutexResult = 0;
	tmpc = link_send(TM_SYS,F_HANDSET_BASE_LINK_MUTEX,0,1,tmps);
	if(tmpc) return -1;

	t0 = GetTimerCount();
	while(1)
	{
		if(LinkMutexFlag) break;

		tmpd = OnbaseCheck();
		if(tmpd < 0) return -2;
		if(tmpd > 0) t0 = GetTimerCount();

		if(GetTimerCount() - t0 > TO_REPLY) return -3;
	}
	if(0 == LinkMutexResult) return 0;
	else return -4;
}

// 更新座机的driver  (int iType,uchar * pucAddr,long lFileLen,tSignFileInfo *pstSigInfo)
int WriteBaseDriver(uchar *PtrDriver, long len,uchar flag)//SendBaseDriver
{
	uchar linktypebak,basetypebak,linkstatusbak,linkenablebak,linkedflag,restoretype;
	int iRet;
	uchar echo_bak;

	restoretype = 0;
	linktypebak = GetLinkType();

	if(linktypebak == 1)				// 蓝牙链路
	{
		//linkstatusbak = GetBtLinkStatus();	//deleted by shics				
		//linkenablebak = GetBtLinkEnable();       //deleted by shics	
		linkedflag = GetLinkedFlag();
		// 蓝牙连接着或者是蓝牙断开着，但是使能开启着说明正在自动重连
		restoretype = 1;
		linktypebak = GetLinkType();
		basetypebak = GetBaseType();
		if(linkstatusbak || linkenablebak)
		{
			//BtDisconnect(); //deleted by shics	
		}
		SetLinkType(0);
		SetBaseType(HANDSET_UARTLINK);
	}
	else if(linktypebak == 0)			// 串口链路
	{
		// 可以直接给座机下载driver
	}
	else 								// 没有座机或仅充电的座机
	{
		return -1;
	}
	if(OnBase()) 
	{
		if(restoretype)
		{
			SetLinkType(linktypebak);
			SetBaseType(basetypebak);
			//SetBtLinkEnable(linkenablebak); //deleted by shics	
			SetLinkedFlag(linkedflag);
		}
		return -2;				// 座机不在位
	}
	LinkReset();
	iRet = 0;
	if(restoretype)
	{
		// 告知座机可以开始下载
		iRet = MutexHandsetBaseLink(1);
	}

	if(!iRet)
	{
		if(flag)
		{
			echo_bak = s_GetScrEcho();
			ScrSetEcho(1);
		}
		iRet = SendBaseDriver(PtrDriver, len,flag);
		if(flag)
		{
			ScrSetEcho(echo_bak);
		}
	}
	else
	{
		LinkReset();
		return -3;
	}

	if(iRet && restoretype)				// 下载失败
	{
		MutexHandsetBaseLink(0);
		LinkReset();
	}

	if(restoretype)
	{
		SetLinkType(linktypebak);
		SetBaseType(basetypebak);
		//SetBtLinkEnable(linkenablebak); //deleted by shics	
		SetLinkedFlag(linkedflag);
	}

	if(iRet) return -3;
	return 0;
}


