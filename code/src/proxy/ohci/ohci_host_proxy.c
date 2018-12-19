#include "ohci_host_proxy.h"
#include "..\..\fat\fsapi.h"
#include "..\..\fat\fat.h"

#define OHCI_LINK_PAYLOAD 1024
#define M_OHCI     6
#ifndef TO_REPLY_OHCI
#define TO_REPLY_OHCI 10000
#endif
enum OHCI_PROXY_FUNC{
	OHCI_PROXY_ENUM = 14,OHCI_PROXY_WRITE,OHCI_PROXY_READ,FS_GETDISKONFO,FS_GETINFOCHECK,OHCI_MOUNT_UDISK,
};
static unsigned char ohciRspPool[OHCI_LINK_PAYLOAD];
static unsigned short ohciRspLen;
static void ohci_reply_reset(void)
{
	ohciRspLen = 0;
}

void OhciProcess(unsigned char func_no,unsigned short uiDataLen,unsigned char *pucData)
{
	memcpy(ohciRspPool,pucData,uiDataLen);
	ohciRspLen = uiDataLen;
}

static int ohci_reply_get(int *result,unsigned short *dlen,unsigned char *dout)
{
	if(!ohciRspLen)return 0;
	if(ohciRspLen<4)return OHCI_ERR_IRDA_BUF;
	*result=ohciRspPool[0]+(ohciRspPool[1]<<8)+(ohciRspPool[2]<<16)+(ohciRspPool[3]<<24);
	if(dlen)*dlen=ohciRspLen-4;
	if(dout)memcpy(dout,ohciRspPool+4,ohciRspLen-4);
	return 1;
}

int OhciHostEnum_Proxy(unsigned char *buf)
{
	unsigned char tmpc,datout[20];
	unsigned short datlen;
	unsigned int t0,t1;
	int tmpd,result,buflen;
	t1 = GetMs();
	while(1)
	{
		ohci_reply_reset();
		tmpc = link_send(M_OHCI,OHCI_PROXY_ENUM,0,0,NULL);
		if(tmpc)
		{
			return 0xf3;
		}
		memset(datout,0,sizeof(datout));
		t0 = GetMs();
		while(2)
		{
			tmpd = ohci_reply_get(&result,&datlen,datout);
			if(tmpd < 0) return tmpd;
			if(tmpd > 0) break;
			tmpd = OnbaseCheck();
			if(tmpd < 0) return 0xf4;
			if(tmpd > 0) t0 = GetTimerCount();
			if(GetMs() - t0 > TO_REPLY_OHCI)
			{
				return 0xff;
			}
		}
		if(result == 19 || result == 0) break;
		if(GetMs() - t1 > TO_REPLY_OHCI) 
		{
			return 0xff;
		}
		DelayMs(100);
	}
	if(!result)
	{
		memcpy((unsigned char *)&buflen,datout,sizeof(int));
		memcpy(buf,datout+4,buflen);
	}
	return result;
}

int OhciHostWrite_Proxy(int start_sector,int sector_num,int sector_byte,uchar *WriteBuf)
{
	int tmpd,result;
	unsigned char command[OHCI_LINK_PAYLOAD],tmpc;
	unsigned int index,i,arraycount,sendlen,txbufindex,writelen,t0;

	writelen = sector_byte*sector_num;
	arraycount = (writelen + 3*sizeof(int))/OHCI_LINK_PAYLOAD;
	if((writelen + 3*sizeof(int))%OHCI_LINK_PAYLOAD)	
	{
		arraycount ++;
	}
	index = 0;
	for(i=0;i<arraycount;i++)
	{
		if(i == arraycount - 1)
		{
			sendlen = writelen + 3*sizeof(int) - i*OHCI_LINK_PAYLOAD;
		}
		else
		{
			sendlen = OHCI_LINK_PAYLOAD;
		}

		if(0 == i)		// 第一包要带上长度
		{
			memset(command,0,sizeof(command));
			index = 0;
			memcpy(&command[index],(unsigned char *)&start_sector,sizeof(int));
			index += sizeof(int);
			memcpy(&command[index],(unsigned char *)&sector_num,sizeof(int));
			index += sizeof(int);
			memcpy(&command[index],(unsigned char *)&writelen,sizeof(int));
			index += sizeof(int);
			memcpy(&command[index],(unsigned char *)WriteBuf,sendlen - 3*sizeof(int));
			txbufindex = sendlen - 3*sizeof(int);
		}
		else
		{
			memset(command,0,sizeof(command));
			memcpy(command,(unsigned char *)&WriteBuf[txbufindex],sendlen);
			txbufindex += sendlen;
		}
		ohci_reply_reset();
		tmpc = link_send(M_OHCI,OHCI_PROXY_WRITE,0,sendlen,command);
		if(tmpc)
		{
			return OHCI_ERR_IRDA_SEND;
		}
		t0 = GetTimerCount();
		while(1)
		{
			tmpd = ohci_reply_get(&result,NULL,NULL);
			if(tmpd < 0) return tmpd;
			if(tmpd > 0) break;
			tmpd = OnbaseCheck();
			if(tmpd < 0) return OHCI_ERR_IRDA_OFFBASE;
			if(tmpd > 0) t0 = GetTimerCount();
			if(GetTimerCount() - t0 > TO_REPLY_OHCI)
			{
				return OHCI_ERR_IRDA_RECV;
			}
		}
		if(result < 0) return result;
	}
	return result;
}


int OhciHostRead_Proxy(int start_sector,int sector_num,int sector_byte,uchar *ReadBuf)
{
	int tmpd,result,resultbak,index,resultlen;
	unsigned char command[20],datout[OHCI_LINK_PAYLOAD],tmpc,firstpck;
	unsigned int  rxbufindex,t0;
	unsigned short datlen;

	index = 0;
	memset(command,0,sizeof(command));
	memcpy(&command[index],(unsigned char *)&start_sector,sizeof(int));
	index += sizeof(int);
	memcpy(&command[index],(unsigned char *)&sector_num,sizeof(int));
	index += sizeof(int);
	rxbufindex = 0;
	firstpck = 0;
	while(1)
	{
		ohci_reply_reset();
		tmpc = link_send(M_OHCI,OHCI_PROXY_READ,0,index,command);
		if(tmpc) return OHCI_ERR_IRDA_SEND;

		t0 = GetTimerCount();
		while(1)
		{
			tmpd = ohci_reply_get(&result,&datlen,datout);
			if(tmpd < 0) return tmpd;
			if(tmpd > 0) break;

			tmpd = OnbaseCheck();
			if(tmpd < 0) return OHCI_ERR_IRDA_OFFBASE;
			if(tmpd > 0) t0 = GetTimerCount();
			if(GetTimerCount() - t0 > TO_REPLY_OHCI)
			{
				return OHCI_ERR_IRDA_RECV;
			}
		}
		// 第一包需要取出读的结果以及长度
		if(0 == firstpck && 0 == result)
		{
			resultbak = result; 	// 取出返回码
			memcpy((unsigned char *)&resultlen,datout,sizeof(int)); 	// 取出读到的总长度
			if(resultlen > OHCI_LINK_PAYLOAD - 8) 	// 说明有多包
			{
				memcpy(ReadBuf,&datout[4],OHCI_LINK_PAYLOAD - 8);
				rxbufindex = OHCI_LINK_PAYLOAD - 8;
				firstpck = 1;						// 需要接收后续包
			}
			else									// 说明只有一包
			{
				memcpy(ReadBuf,&datout[4], resultlen);
				break;
			}
		}
		else if(result)
		{
			return result;
		}

		if(firstpck)		// 后续包
		{
			memcpy(&ReadBuf[rxbufindex],datout,datlen);
			rxbufindex += datlen;
			if(rxbufindex == resultlen)
				break;
		}
	}
	return resultbak;
}

int FsGetDiskInfo_proxy(int disk_num,FS_DISK_INFO * disk_info)
{
	int tmpd,result;
	unsigned char command[OHCI_LINK_PAYLOAD],datout[20],tmpc;
	unsigned int  t0,t1,index;
	unsigned short datlen;
	DRIVE_INFO drv;
	
	if(disk_info == NULL)return FS_ERR_ARG;
	memset(&drv,0,sizeof(DRIVE_INFO));
	tmpd = GetDriveInfo(&drv);
	if(tmpd)
	{
		return FS_ERR_NODEV;
	}
	memset(command,0,sizeof(command));
	index = 0;
	memcpy(&command[index],(unsigned char *)&(drv.bDevice),sizeof(DRIVE_INFO) - sizeof(fat_driver_op));
	index += sizeof(DRIVE_INFO) - sizeof(fat_driver_op);
	ohci_reply_reset();
	tmpc = link_send(M_OHCI,FS_GETDISKONFO,0,index,command);
	if(tmpc) return OHCI_ERR_IRDA_SEND;

	t0 = GetTimerCount();
	while(1)
	{
		tmpd = ohci_reply_get(&result,&datlen,datout);
		if(tmpd < 0) return tmpd;
		if(tmpd > 0) break;

		tmpd = OnbaseCheck();
		if(tmpd < 0) return OHCI_ERR_IRDA_OFFBASE;
		if(tmpd > 0) t0 = GetTimerCount();
		if(GetTimerCount() - t0 > TO_REPLY_OHCI)
		{
			return OHCI_ERR_IRDA_RECV;
		}
	}	
	if(FS_ERR_DEV_REMOVED == result || FS_ERR_NODEV == result)
	{
		set_udisk_a_absent();
	}
	if(result) return result;
	t0 = GetMs();
	while(1)
	{
		ohci_reply_reset();
		tmpc = link_send(M_OHCI,FS_GETINFOCHECK,0,0,NULL);
		if(tmpc) return OHCI_ERR_IRDA_SEND;

		t1 = GetMs();
		while(2)
		{
			tmpd = ohci_reply_get(&result,&datlen,datout);
			if(tmpd < 0) return tmpd;
			if(tmpd > 0) break;

			tmpd = OnbaseCheck();
			if(tmpd < 0) return OHCI_ERR_IRDA_OFFBASE;
			if(tmpd > 0) t1 = GetMs();
			if(GetMs() - t1 > TO_REPLY_OHCI)
			{
				return OHCI_ERR_IRDA_RECV;
			}
		}
		if(result <= 0) break;
		if(GetMs() - t1 > TO_REPLY_OHCI*2)
		{
			return OHCI_ERR_IRDA_RECV;
		}
		DelayMs(200);
	}
	if(FS_ERR_DEV_REMOVED == result || FS_ERR_NODEV == result)
	{
		set_udisk_a_absent();
	}
	if(result == 0)
	{
		memcpy((u8 *)disk_info, datout,sizeof(FS_DISK_INFO));//need debug 
	}
	return result;
}


int OhciMountUdisk_proxy()
{
	int tmpd,result;
	unsigned char datout[20],tmpc;
	unsigned int  t0,index;
	unsigned short datlen;
	
	ohci_reply_reset();
	tmpc = link_send(M_OHCI,OHCI_MOUNT_UDISK,0,0,NULL);
	if(tmpc) return OHCI_ERR_IRDA_SEND;

	t0 = GetTimerCount();
	while(1)
	{
		tmpd = ohci_reply_get(&result,NULL,NULL);
		if(tmpd < 0) return tmpd;
		if(tmpd > 0) break;

		tmpd = OnbaseCheck();
		if(tmpd < 0) return OHCI_ERR_IRDA_OFFBASE;
		if(tmpd > 0) t0 = GetTimerCount();
		if(GetTimerCount() - t0 > TO_REPLY_OHCI)
		{
			return OHCI_ERR_IRDA_RECV;
		}
	}	
	return result;
}

