#include "usbbulk.h"
#include "uficmd.h"
#include "../fat/fat.h"

static u8  submit_buf[4096];
#define BULK_IN  0
#define BULK_OUT 1

#define rUSB_STATUS         0x0d

static int udisk_read(struct _drive_info *drv, 
		int start_sector, 
		int sector_num, 
		char *buf, int len);
static int udisk_write(struct _drive_info *drv, 
		int start_sector,
		int sector_num,
		char *buf, int len);

//////////////////////////////////
static unsigned char out[0x20];
fat_driver_op udisk_op={
	.read = udisk_read,
	.write = udisk_write
};

static DRIVE_INFO   udisk_info;
static int usb_bulk_submit(void *cbw,void *csw,u8 *buffer,int len,int direction)
{
	int i,err,rn;
	u8  tmpc;

	for(i=0;i<5;i++)//5
	{
		err=host_out_epx(31,(u8*)cbw);
//if(err)x_debug("%d,out:%d\n",i,err);
		if(err){err=-50-err;goto do_error;}

		if(len>0)
		{
			rn=len;
			if(direction == BULK_OUT)err=host_out_epx(len,buffer);
			else err=host_in_epx(buffer,&rn);
//if(err)x_debug("%d,io:%d,rn:%d\n",i,err,rn);

			if(err){err=-50-err;goto do_error;}
		}

		rn=13;
		err=host_in_epx((u8*)csw,&rn);
//if(err)x_debug("%d,csw:%d,rn:%d\n",i,err,rn);
		if(err){err=-50-err;goto do_error;}
		if(rn!=13)
		{
//x_debug("%d,CSW:%d,RN:%d\n",i,err,rn);
			err = FS_ERR_HW;
			goto do_error;
		}
/*
		usb_debug("usb_bulk_submit: csw status=%02x\n",buf[12]);
		switch(buf[12])//command status
		{
		case 0://passed
			return FS_OK;
		case 1://fail
			return 1;
		case 2://phase error
			err = FS_ERR_HW;
		default:
			err = FS_ERR_HW;
		}
*/
		return FS_OK;

do_error:
		if(s_UsbHostState())return FS_ERR_DEV_REMOVED;

		if(err==FS_ERR_NODEV||err==FS_ERR_HW)return err;

		usb_debug("usb_bulk_submit: retry=%d\n", i);
	}

	return FS_ERR_TIMEOUT;
}

static int SPC_Inquiry(void)
{
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;
	out[4]=0x60;out[5]=0xa6;out[6]=0x24;out[7]=0xde;
	out[8]=0x24;out[9]=0x00;out[10]=0x00;out[11]=0x00;
	out[12]=0x80;out[13]=0x00;out[14]=6;
	out[15]=SPC_CMD_INQUIRY;out[16]=0x00;out[17]=0x00;
	out[18]=0x00;out[19]=0x24;out[20]=0x00;	
	usb_debug("########## SPC_Inquiry............\n");
	return usb_bulk_submit(out, out, submit_buf, 36, BULK_IN);
}

static int SPC_TestUnit(void)
{
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;	
	out[4]=0x60;out[5]=0xa6;out[6]=0x24;out[7]=0xde;	
	out[8]=0x00;out[9]=0x00;out[10]=0x00;out[11]=0x00;
	out[12]=0x00;out[13]=0x00;out[14]=6;
	/////////////////////////////////////	
	out[15]=SPC_CMD_TESTUNITREADY;
	out[16]=0;out[17]=0;out[18]=0;out[19]=0;out[20]=0;
	//////////////////////////////////////
	usb_debug("########## SPC_TestUnit............\n");
	return usb_bulk_submit(out, out, NULL, 0, -1);
}

static int SPC_LockMedia(void)
{
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;
	out[4]=0x60;out[5]=0xa6;out[6]=0x24;out[7]=0xde;	
	out[8]=0x00;out[9]=0x00;out[10]=0x00;out[11]=0x00;
	out[12]=0x00;out[13]=0x00;out[14]=5;
	///////////////////////////////////////////
	out[15]=SPC_CMD_PRVENTALLOWMEDIUMREMOVAL;
	out[16]=0;out[17]=0;out[18]=0;out[19]=1;
	return usb_bulk_submit(out, out, NULL, 0, -1);	
}

static int SPC_RequestSense(void)
{
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;	
	out[4]=0x60;out[5]=0xa6;out[6]=0x24;out[7]=0xde;
	out[8]=0x0e;out[9]=0x00;out[10]=0x00;out[11]=0x00;
	out[12]=0x80;out[13]=0x00;out[14]=6;
	out[15]=SPC_CMD_REQUESTSENSE;out[16]=0x00;out[17]=0x00;
	out[18]=0x00;out[19]=14;out[20]=0x00;		
	//////////////////////////////////////
	usb_debug("########## SPC_RequestSense............\n");
	return usb_bulk_submit(out, out, out, 14, BULK_IN);	
}

static int RBC_ReadCapacity(void)
{
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;	
	out[4]=0x60;out[5]=0xa6;out[6]=0x24;out[7]=0xde;	
	out[8]=0x08;out[9]=0x00;out[10]=0x00;out[11]=0x00;	
	out[12]=0x80;out[13]=0x00;out[14]=10;
	/////////////////////////////////////
	out[15]=RBC_CMD_READCAPACITY;
	out[16]=0;out[17]=0;out[18]=0;out[19]=0;
	out[20]=0;out[21]=0;out[22]=0;out[23]=0;
	out[24]=0;
	//////////////////////////////////////
	usb_debug("########## RBC_ReadCapacity............\n");
	return usb_bulk_submit(out, out, submit_buf, 8, BULK_IN);
	
}
  
//read sectors
static int RBC_Read(unsigned long lba,unsigned char len,unsigned char *pBuffer)
{
	unsigned long lout;	
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;	
	out[4]=0x60;out[5]=0xa6;out[6]=0x24;out[7]=0xde;	
	lout=len*udisk_info.wSectorSize;
	out[8]=(unsigned char)(lout&0xff);
	out[9]=(unsigned char)((lout>>8)&0xff);
	out[10]=(unsigned char)((lout>>16)&0xff);
	out[11]=(unsigned char)((lout>>24)&0xff);	
	out[12]=0x80;out[13]=0x00;out[14]=10;
	/////////////////////////////////////
	out[15]=RBC_CMD_READ10;out[16]=0x00;
	out[17]=(unsigned char)((lba>>24)&0xff);
	out[18]=(unsigned char)((lba>>16)&0xff);
	out[19]=(unsigned char)((lba>>8)&0xff);
	out[20]=(unsigned char)(lba&0xff);	
	out[21]=0x00;
	out[22]=(unsigned char)((len>>8)&0xff);
	out[23]=(unsigned char)(len&0xff);	
	out[24]=0x00;
	usb_debug("########### RBC_Read #############\n");
	return usb_bulk_submit(out, out, pBuffer, lout, BULK_IN);	
	
}


//write sectors
static int RBC_Write(unsigned long lba,unsigned char len,unsigned char *pBuffer)
{
	unsigned long lout;
	out[0]=0x55;out[1]=0x53;out[2]=0x42;out[3]=0x43;
	out[4]=0xb4;out[5]=0xd9;out[6]=0x77;out[7]=0xc1;	
	lout=len*udisk_info.wSectorSize;
	out[8]=(unsigned char)(lout&0xff);
	out[9]=(unsigned char)((lout>>8)&0xff);
	out[10]=(unsigned char)((lout>>16)&0xff);
	out[11]=(unsigned char)((lout>>24)&0xff);	
	out[12]=0x00;out[13]=0x00;out[14]=10;
	/////////////////////////////////////
	out[15]=RBC_CMD_WRITE10;	
	out[16]=0x00;
	out[17]=(unsigned char)((lba>>24)&0xff);
	out[18]=(unsigned char)((lba>>16)&0xff);
	out[19]=(unsigned char)((lba>>8)&0xff);
	out[20]=(unsigned char)(lba&0xff);
	out[21]=0x00;
	out[22]=(unsigned char)((len>>8)&0xff);
	out[23]=(unsigned char)(len&0xff);	
	out[24]=0x00;
	usb_debug("########### RBC_Write #############\n");
	//force_urb_printf(pBuffer, lout, "RBC_Write");
	return usb_bulk_submit(out, out, pBuffer, lout, BULK_OUT);	
	
}

///////////////////////////////////////////////////////////////////////////
static int usb_remount(void)
{
	unsigned int ReservedSectorsNum;

	#if 1
	if(SPC_Inquiry()<0)
	{
		usb_error("usb_remount: Fail in SPC_Inquiry!\n");
		return -1;
	}
	if(SPC_TestUnit()<0)
	{
		usb_error("usb_remount: Fail in SPC_TestUnit!\n");
		return -1;
	}
	if(SPC_LockMedia()<0)
	{
		usb_error("usb_remount: Fail in SPC_LockMedia!\n");
		return -1;
	}
	if(SPC_RequestSense()<0)
	{
		usb_error("usb_remount: Fail in SPC_RequestSense 1!\n");
		return -1;
	}
	if(SPC_TestUnit()<0)
	{
		usb_error("usb_remount: Fail in SPC_TestUnit 1!\n");
		return FALSE;
	}
	if(RBC_ReadCapacity()<0)
	{
		usb_error("usb_remount: Fail in RBC_ReadCapacity 1!\n");
		return -1;
	}
	if(SPC_RequestSense()<0)
	{
		usb_error("usb_remount: Fail in SPC_RequestSense 2!\n");
		return -1;
	}
	if(SPC_TestUnit()<0)
	{
		usb_error("usb_remount: Fail in SPC_TestUnit 2!\n");
		return -1;
	}
	#endif
	if(RBC_ReadCapacity()!=FS_OK)
	{
		usb_error("usb_remount: Fail in RBC_ReadCapacity!\n");
		return -1;
	}
	return FS_OK;
}

///////////////////////////////////////////////////////////////////////////
int usb_enum_massdev(void)
{
	int tmpd;
	unsigned int ReservedSectorsNum;

	fat_unmount(FS_UDISK);

	memset(&udisk_info, 0, sizeof(udisk_info));

	#if 1
	if(SPC_Inquiry()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_Inquiry!\n");
		return -1;
	}
	if(SPC_TestUnit()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_TestUnit!\n");
		return -2;
	}
	if(SPC_LockMedia()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_LockMedia!\n");
		return -3;
	}
	if(SPC_RequestSense()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_RequestSense 1!\n");
		return -4;
	}
	if(SPC_TestUnit()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_TestUnit 1!\n");
		return -5;
	}
	if(RBC_ReadCapacity()<0)
	{
		usb_error("usb_enum_massdev: Fail in RBC_ReadCapacity 1!\n");
		return -6;
	}
	if(SPC_RequestSense()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_RequestSense 2!\n");
		return -7;
	}
	if(SPC_TestUnit()<0)
	{
		usb_error("usb_enum_massdev: Fail in SPC_TestUnit 2!\n");
		return -8;
	}
	#endif
	if(RBC_ReadCapacity()!=FS_OK)
	{
		usb_error("usb_enum_massdev: Fail in RBC_ReadCapacity!\n");
		return -9;
	}
	{
		PREAD_CAPACITY_RSP pCap=(void*)submit_buf;
		memset(&udisk_info, 0, sizeof(udisk_info));
		udisk_info.wSectorSize = ntohl(pCap->BlockSize);
		udisk_info.dwSectorTotal = ntohl(pCap->LastLBA)+1;
		usb_debug("Sector Size = %d bytes, Total = %d\n",
			udisk_info.wSectorSize,
			udisk_info.dwSectorTotal
			);
		udisk_info.ops = &udisk_op;
		tmpd=fat_mount(FS_UDISK, &udisk_info);
		if(tmpd<0)return -10;
	}
	return FS_OK;
}


static int udisk_read(struct _drive_info *drv, 
		int start_sector, 
		int sector_num, 
		char *buf, int len)
{
	//usb_debug("udisk_read........\n");
	if(sector_num > 0xff||
		start_sector<0||
		(start_sector+sector_num>drv->dwSectorTotal))
		return FS_ERR_ARG;
	assert(len >= (sector_num*drv->wSectorSize));
	return RBC_Read(start_sector, sector_num, buf);
}

static int udisk_write(struct _drive_info *drv,
		int start_sector,
		int sector_num,
		char *buf, int len)
{
	//static char r_buf[2049];
	//int err, retry;

	assert(sector_num == 1);
	if(sector_num > 0xff|| start_sector<0 ||
		(start_sector+sector_num>drv->dwSectorTotal))
	{
		return FS_ERR_ARG;
	}

	assert(len >= (sector_num*drv->wSectorSize));
	return RBC_Write(start_sector, sector_num, buf);
/*
	for(retry=0; retry<10; retry++)
	{
		err = RBC_Write(start_sector, sector_num, buf);
		if(err != FS_OK)
			return err;
		err = udisk_read(drv, start_sector, sector_num, r_buf, sizeof(r_buf));
		if(err != FS_OK)
			return err;
		if(memcmp(buf, r_buf, drv->wSectorSize)==0)
			return FS_OK;
	}
	return FS_ERR_HW;
*/
}

