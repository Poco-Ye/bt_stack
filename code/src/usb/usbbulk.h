#ifndef _USB_BULK_H_
#define _USB_BULK_H_
#include "usb.h"

#define RBC_CMD_READ10					0x28
#define RBC_CMD_READCAPACITY				0x25
#define RBC_CMD_WRITE10					0x2A
// RBC commands
#define SPC_CMD_INQUIRY					0x12
#define SPC_CMD_PRVENTALLOWMEDIUMREMOVAL		0x1E
#define SPC_CMD_REQUESTSENSE				0x03
#define SPC_CMD_TESTUNITREADY				0x00


#define CBW_SIGNATURE   0x55534243
#define CSW_SIGNATURE   0x55534253

#define usb_debug //s80_printf
#define usb_error //s80_printf

#if 0
typedef struct _COMMAND_BLOCK_WRAPPER
{
    u32   dCBW_Signature;
    u32   dCBW_Tag;
    u32   dCBW_DataXferLen;
    u8    bCBW_Flag;
    u8    bCBW_LUN;
    u8    bCBW_CDBLen;
    CDB_RBC cdbRBC;
} __attribute__((__packed__)) CBW, *PCBW;

typedef struct _COMMAND_STATUS_WRAPPER{
    u32   dCSW_Signature;
    u32   dCSW_Tag;
    u32   dCSW_DataResidue;
    u8    bCSW_Status;
} CSW, *PCSW;

typedef union _TPBULK_STRUC {
    CBW     TPBulk_CommandBlock;
    CSW     TPBulk_CommandStatus;

}TPBLK_STRUC, * PTPBLK_STRUC;
#endif

#if 0
int usb_enum_massdev(void);
int SPC_Inquiry(void);
int SPC_RequestSense(void);
int SPC_TestUnit(void);
int SPC_LockMedia(void);
int RBC_ReadCapacity(void);
int RBC_Read(unsigned long lba,unsigned char len,unsigned char *pBuffer);
int RBC_Write(unsigned long lba,unsigned char len,unsigned char *pBuffer);

////////////////////////////////////////////////////////////////////////////////////
// Command Descriptor Block
//      _RBC : Reduced Block Command
//      _SPC : SPC-2 SCSI primary Command - 2
////////////////////////////////////////////////////////////////////////////////////
typedef struct _SYS_INFO_BLOCK
{
  u32 StartSector;
  u32 TotalSector;
  
  u16 BPB_BytesPerSec;
  u8  BPB_SecPerClus;
  
  u8  BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8  BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  u32 BPB_HiddSec;
  u32 BPB_TotSec32;
  u8  BS_DrvNum;
  u8  BS_BootSig;
  u8  BS_VolID[4];
  u8  BS_VolLab[11];
  u8  BS_FilSysType[8];
  
  ///////////////////////////////
  u32 FatStartSector;
  u32 RootStartSector;
  u32 FirstDataSector;
} __attribute__((__packed__)) SYS_INFO_BLOCK;

typedef struct _FILE_INFO
{
  u8 bFileOpen;
  unsigned int StartCluster;
  u32 LengthInByte;
  unsigned int ClusterPointer;
  u32 SectorPointer;
  unsigned int OffsetofSector;
  u8 SectorofCluster;
  u32 pointer;
  unsigned int	FatSectorPointer;
} FILE_INFO;

typedef struct _DIR_INFO
{
	u8 name[8];
	u8 extension[3];
	u8 attribute;
	u8 Reserved[10];
	unsigned int lastUpdateDate;
	unsigned int lastUpdateTime;
	unsigned int startCluster;
	u32 length;
} DIR_INFO;
#endif
#endif/* _USB_BULK_H_ */
