/*****************************************
**    See 'Universal Serial Bus Mass Storage Class'
**          'UFI Command Specification'
*******************************************/

#ifndef _UFICMD_H_
#define _UFICMD_H_

/*
// RBC commands
*/
#define RBC_CMD_FORMAT					0x04
#define RBC_CMD_READ10					0x28
#define RBC_CMD_READCAPACITY				0x25
#define RBC_CMD_STARTSTOPUNIT				0x1B
#define RBC_CMD_SYNCCACHE				0x35
#define RBC_CMD_VERIFY10				0x2F
#define RBC_CMD_WRITE10					0x2A

/*
// SPC-2 commands
*/
#define SPC_CMD_INQUIRY					0x12
#define SPC_CMD_MODESELECT6				0x15
#define SPC_CMD_MODESENSE6				0x1A
#define SPC_CMD_PERSISTANTRESERVIN			0x5E
#define SPC_CMD_PERSISTANTRESERVOUT			0x5F
#define SPC_CMD_PRVENTALLOWMEDIUMREMOVAL		0x1E
#define SPC_CMD_RELEASE6				0x17
#define SPC_CMD_REQUESTSENSE				0x03
#define SPC_CMD_RESERVE6				0x16
#define SPC_CMD_TESTUNITREADY				0x00
#define SPC_CMD_WRITEBUFFER				0x3B
#define SPC_CMD_READLONG				0x23


/*
// ATAPI Command Descriptor Block
*/

typedef struct _READ_10 
{
	u8 OperationCode;
	u8 Reserved1;
	u8 LBA_3;
	u8 LBA_2;
	u8 LBA_1;
	u8 LBA_0;
	u8 Reserved2;
	u8 XferLen_1;
	u8 XferLen_0;
	u8 Reserved3[3];
} __attribute__((__packed__)) READ_10, * PREAD_10;

typedef struct _WRITE_10 
{
	u8 OperationCode;
	u8 Reserved1;
	u8 LBA_3;
	u8 LBA_2;
	u8 LBA_1;
	u8 LBA_0;
	u8 Reserved2;
	u8 XferLen_1;
	u8 XferLen_0;
	u8 Reserved3[3];
} __attribute__((__packed__)) WRITE_10, *PWRITE_10;

typedef struct _MODE_SENSE_10 
{
	u8 OperationCode;
	u8 Reserved1;
	u8 PageCode : 6;
	u8 Pc : 2;
	u8 Reserved2[4];
	u8 ParameterListLengthMsb;
	u8 ParameterListLengthLsb;
	u8 Reserved3[3];
} __attribute__((__packed__)) MODE_SENSE_10, *PMODE_SENSE_10;

typedef struct _MODE_SELECT_10 
{
	u8 OperationCode;
	u8 Reserved1 : 4;
	u8 PFBit : 1;
	u8 Reserved2 : 3;
	u8 Reserved3[5];
	u8 ParameterListLengthMsb;
	u8 ParameterListLengthLsb;
	u8 Reserved4[3];
} __attribute__((__packed__)) MODE_SELECT_10, *PMODE_SELECT_10;
/*
////////////////////////////////////////////////////////////////////////////////////
// Command Descriptor Block
//      _RBC : Reduced Block Command
//      _SPC : SPC-2 SCSI primary Command - 2
////////////////////////////////////////////////////////////////////////////////////
*/

/*
// Generic
*/
/*
// Generic
*/
typedef struct _GENERIC_CDB 
{
	u8 OperationCode;
	u8 Reserved[15];
} __attribute__((__packed__)) GENERIC_CDB,*PGENERIC_CDB;

typedef struct _GENERIC_RBC
{
	u8 OperationCode;
	u8 Reserved[8];
	u8 Control;
} __attribute__((__packed__)) GENERIC_RBC,*PGENERIC_RBC;

/*
// format unit
*/
typedef struct _FORMAT_RBC 
{
	u8 OperationCode;	/* 04H */
	u8 VendorSpecific;
	u8 Increment : 1;
	u8 PercentorTime : 1;
	u8 Progress : 1;
	u8 Immediate : 1;
	u8 VendorSpecific1 : 4;
	u8 Reserved2[2];
	u8 Control;
}__attribute__((__packed__)) FORMAT_RBC, *PFORMAT_RBC;


/*
// Read Command
*/
typedef struct _READ_RBC 
{
	u8 OperationCode;	/* 10H */
	u8 VendorSpecific;
	union
	{
		struct
		{
			u8 LBA_3;
			u8 LBA_2;
			u8 LBA_1;
			u8 LBA_0;
		} LBA_W8 ;

		u32 LBA_W32;
	}LBA;
	u8 Reserved;
	//u8 XferLength_1;
	//u8 XferLength_0;
	u16 XferLength;
	u8 Control;
	//u8 Reserved1[3];
} __attribute__((__packed__)) READ_RBC, *PREAD_RBC;


/*
// Read Capacity Data - returned in Big Endian format
*/
typedef struct _READ_CAPACITY_DATA 
{
	u8 LBA_3;
	u8 LBA_2;
	u8 LBA_1;
	u8 LBA_0;
	u8 BlockLen_3;
	u8 BlockLen_2;
	u8 BlockLen_1;
	u8 BlockLen_0;
} __attribute__((__packed__)) READ_CAPACITY_DATA, *PREAD_CAPACITY_DATA;
//////////////////////////////////////////////////
typedef struct _READ_LONG_CMD
{
	u8 OperationCode;
	u8 LogicalUnitNum	:3;
	u8 RES_1		:5;
	u8 RES_2;
	u8 RES_3;
	u8 RES_4;
	u8 RES_5;
	u8 RES_6;
	u16 AllocationLen;
	u8 RES_7;
	u8 RES_8;
	u8 RES_9;
}__attribute__((__packed__)) READ_LONG_CMD, *PREAD_LONG_CMD;
typedef struct _READ_LONG 
{
	u8 RES_1;
	u8 RES_2;
	u8 RES_3;
	u8 CAP_LIST_LEN;
    
	u8 LBA_3;
	u8 LBA_2;
	u8 LBA_1;
	u8 LBA_0;

	u8 Descripter;
	u8 BlockLen_2;
	u8 BlockLen_1;
	u8 BlockLen_0;
}__attribute__((__packed__)) READ_LONG, *PREAD_LONG;
/*
// Read Capacity command
*/
typedef struct _READ_CAPACITY_RBC 
{
	u8                OperationCode;	/* 10H */
	union  
	{
		u32               l0[2];
		u32               l[2];
		READ_CAPACITY_DATA  CapData;       /* Reserved area, here is used as temp*/
	} tmpVar;

	u8                Control;
} __attribute__((__packed__))READ_CAPACITY_RBC, *PREAD_CAPACITY_RBC;

typedef struct _READ_CAPACITY_RSP 
{
	u32 LastLBA;
	u32 BlockSize;
} __attribute__((__packed__)) READ_CAPACITY_RSP, *PREAD_CAPACITY_RSP;
/*
// START_STOP_UNIT
*/
typedef struct _START_STOP_RBC {
    u8 OperationCode;    /*1BH*/
    u8 Immediate: 1;
    u8 Reserved1 : 7;
    u8 Reserved2[2];
	union _START_STOP_FLAGS
    {
        struct
        {
            u8 Start          : 1;
            u8 LoadEject      : 1;
            u8 Reserved3      : 2;
            u8 PowerConditions: 4;
        } bits0;

        struct
        {
            u8 MediumState    : 2;
            u8 Reserved3      : 2;
            u8 PowerConditions: 4;
        } bits1;
    } Flags;
    u8 Control;
}__attribute__((__packed__))  START_STOP_RBC, *PSTART_STOP_RBC;

/*
// Synchronize Cache
*/
typedef struct _SYNCHRONIZE_CACHE_RBC {

	u8 OperationCode;    /* 0x35 */
	u8 Reserved[8];
	u8 Control;

} __attribute__((__packed__)) SYNCHRONIZE_CACHE_RBC, *PSYNCHRONIZE_CACHE_RBC;

/*
// Write Command
*/
typedef struct _WRITE_RBC {
    u8 OperationCode;	/* 2AH      */
    //u8 Reserved0 : 3;
    //u8 FUA : 1;
    //u8 Reserved1 : 4;
    u8 VendorSpecific;
    union{
	 struct
         {
	        u8 LBA_3;
	        u8 LBA_2;
	        u8 LBA_1;
	        u8 LBA_0;
          } LBA_W8 ;

	 u32 LBA_W32;
	}   LBA;
    u8 Reserved2;
    u16 XferLength;
    u8 Control;
} __attribute__((__packed__)) WRITE_RBC, *PWRITE_RBC;

/*
// VERIFY Command
*/
typedef struct _VERIFY_RBC {
    u8 OperationCode;	/* 2FH */
    u8 Reserved0;
	u8 LBA_3;			/* Big Endian */
	u8 LBA_2;
	u8 LBA_1;
	u8 LBA_0;
	u8 Reserved1;
    u8 VerifyLength_1;		/* Big Endian */
	u8 VerifyLength_0;
	u8 Control;
} __attribute__((__packed__)) VERIFY_RBC, *PVERIFY_RBC;


/*
//***********************************************************************************
// SPC-2 of SCSI-3 commands
//***********************************************************************************
*/

/*
// INQUIRY Command
*/
typedef struct _INQUIRY_SPC 
{
	u8 OperationCode;	/* 12H */
	u8 EnableVPD:1 ;
	u8 CmdSupportData:1 ;
	u8 Reserved0:6 ;
	u8 PageCode;
	u8 Reserved1;
	u8 AllocationLen;
	u8 Control;
} __attribute__((__packed__)) INQUIRY_SPC, *PINQUIRY_SPC;


typedef struct _STD_INQUIRYDATA
{
    u8 DeviceType : 5;
    u8 Reserved0 : 3;

    u8 Reserved1 : 7;
    u8 RemovableMedia : 1;

    u8 Reserved2;

    u8 Reserved3 : 5;
    u8 NormACA : 1;
    u8 Obsolete0 : 1;
    u8 AERC : 1;

    u8 Reserved4[3];

    u8 SoftReset : 1;
    u8 CommandQueue : 1;
	u8 Reserved5 : 1;
	u8 LinkedCommands : 1;
	u8 Synchronous : 1;
	u8 Wide16Bit : 1;
	u8 Wide32Bit : 1;
	u8 RelativeAddressing : 1;

	u8 VendorId[8];

	u8 ProductId[16];

	u8 ProductRevisionLevel[4];

/*
//  Above is 36 bytes
//  can be tranmitted by Bulk
*/

    u8 VendorSpecific[20];
    u8 InfoUnitSupport : 1;
    u8 QuickArbitSupport : 1;
    u8 Clocking : 2;
    u8 Reserved6 : 4;

    u8  Reserved7 ;
    u16 VersionDescriptor[8] ;

    u8 Reserved8[22];
    
} __attribute__((__packed__)) STD_INQUIRYDATA, *PSTD_INQUIRYDATA;

typedef struct _SERIALNUMBER_PAGE {
    u8 DeviceType : 5;
    u8 DeviceTypeQualifier : 3;

    u8 PageCode ;
    u8 Reserved0 ;

    u8 PageLength ;
    u8 SerialNumber[24] ;

}__attribute__((__packed__)) VPD_SERIAL_PAGE,* PVPD_SERIAL_PAGE;

#define ASCII_ID_STRING 32
typedef struct _ID_DESCRIPTOR {
	u8   CodeSet : 4;
	u8   Reserved0 : 4;

	u8   IDType : 4;
    u8   Association : 2;
    u8   Reserved1 : 2;

    u8   Reserved2;

	u8   IDLength ;
	u8   AsciiID[ASCII_ID_STRING];
} __attribute__((__packed__)) ASCII_ID_DESCRIPTOR,* PASCII_ID_DESCRIPTOR;

typedef struct _DEVICE_ID_PAGE
{
    u8 DeviceType : 5;
    u8 DeviceTypeQualifier : 3;

    u8 PageCode ;
    u8 Reserved0 ;

    u8 PageLength ;

    ASCII_ID_DESCRIPTOR   AsciiIdDescriptor[1];
} __attribute__((__packed__)) VPD_DEVICE_ID_PAGE, * PVPD_DEVICE_ID_PAGE;



/*
// Mode Select
*/
typedef struct _MODE_SELECT_SPC {
	u8 OperationCode;	/* 15H */
	u8 SavePage : 1 ;
	u8 Reseved0 : 3 ;
	u8 PageFormat : 1 ;
	u8 Reserved1 : 3 ;
	u8 Reserved2[2];
	u8 ParameterLen;
	u8 Control;
} __attribute__((__packed__)) MODE_SELECT_SPC, * PMODE_SELECT_SPC;

typedef struct _MBR_BLOCK {
  u8 Res[454];
  unsigned long StartSector;
  unsigned long TotalSector;
  u8 Res1[50];
} __attribute__((__packed__)) MBR_BLOCK,* PMBR_BLOCK;

typedef struct _BPB_BLOCK {
  u8 BS_jmpBoo[3];
  u8 BS_OEMName[8];
  u16 BPB_BytesPerSec;
  u8 BPB_SecPerClus;
  u8 BPB_RsvdSecCn[2];
  u8 BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8 BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  unsigned long BPB_HiddSec;
  unsigned long BPB_TotSec32;
  u8 BS_DrvNum;
  u8 BS_Reserved1;
  u8 BS_BootSig;
  u8 BS_VolID[4];
  u8 BS_VolLab[11];
  u8 BS_FilSysType[8];
  u8 ExecutableCode[448];
  u8 Marker[2];
}__attribute__((__packed__))  BPB_BLOCK,* PBPB_BLOCK;

typedef struct _SYS_INFO_BLOCK{
  unsigned long StartSector;
  unsigned long TotalSector;
  //u8 BS_jmpBoo[3];
  //u8 BS_OEMName[8];
  u16 BPB_BytesPerSec;
  u8 BPB_SecPerClus;
  //u8 BPB_RsvdSecCn[2];
  u8 BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8 BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  unsigned long BPB_HiddSec;
  unsigned long BPB_TotSec32;
  u8 BS_DrvNum;
  //u8 BS_Reserved1;
  u8 BS_BootSig;
  u8 BS_VolID[4];
  u8 BS_VolLab[11];
  u8 BS_FilSysType[8];
  //u8 ExecutableCode[448];
  //u8 Marker[2];
  ///////////////////////////////
  unsigned long FatStartSector;
  unsigned long RootStartSector;
  //unsigned long DataStartSector;
  unsigned long FirstDataSector;
  //unsigned long FirstSectorofCluster;
}__attribute__((__packed__))  SYS_INFO_BLOCK,* PSYS_INFO_BLOCK;
#if 0
typedef struct _FILE_INFO{
  unsigned char bFileOpen;
  unsigned int StartCluster;
  unsigned long LengthInByte;
  unsigned int ClusterPointer;
  unsigned long SectorPointer;
  unsigned int OffsetofSector;
  unsigned char SectorofCluster;
  unsigned long pointer;
  unsigned int	FatSectorPointer;
  

} FILE_INFO, * PFILE_INFO;

typedef struct _FREE_FAT_INFO{
  unsigned long SectorNum;
  unsigned int OffsetofSector;
  unsigned long OldSectorNum;
  

} FREE_FAT_INFO, * PFREE_FAT_INFO;


typedef struct _DIR_INFO{
	unsigned char name[8];
	unsigned char extension[3];
	unsigned char attribute;
	unsigned char Reserved[10];
	unsigned int lastUpdateDate;
	unsigned int lastUpdateTime;
	unsigned int startCluster;
	unsigned long length;
} DIR_INFO,* PDIR_INFO;
#endif
/*
// Mode Sense
*/
typedef struct _MODE_SENSE_SPC {
    u8 OperationCode;	/* 1AH */
    u8 Reseved0 : 3 ;
    u8 DisableBlockDescriptor : 1 ;
    u8 Reserved0 : 4 ;
    u8 PageCode:6 ;
    u8 PageControl : 2 ;
    u8 Reserved1;
    u8 ParameterLen;
    u8 Control;
} __attribute__((__packed__)) MODE_SENSE_SPC, * PMODE_SENSE_SPC;

typedef struct _MODE_PARAMETER_HEAD {
    u8 DataLen;
    u8 MediumType;
    u8 DeviceParameter;
    u8 BlockDescriptorLen;
} __attribute__((__packed__)) MODE_PARAMETER_HEAD, * PMODE_PARAMETER_HEAD;

/*
// Define Device Capabilities page.
*/
typedef struct _MODE_RBC_DEVICE_PARAMETERS_PAGE {
    u8 PageCode : 6;
	u8 Reserved : 1;
    u8 PageSavable : 1;
    u8 PageLength;
    u8 WriteCacheDisable : 1;
    u8 Reserved1 : 7;
    u8 LogicalBlockSize[2];
    u8 NumberOfLogicalBlocks[5];
    u8 PowerPerformance;
    u8 Lockable : 1;
    u8 Formattable : 1;
    u8 Writable : 1;
    u8 Readable : 1;
    u8 Reserved2 : 4;
    u8 Reserved3;
}__attribute__((__packed__)) MODE_RBC_DEVICE_PARAMETERS_PAGE, *PMODE_RBC_DEVICE_PARAMETERS_PAGE;

/*
// prevent/allow medium removal
*/
typedef struct _MEDIA_REMOVAL_SPC {
	u8 OperationCode;    /* 1EH */
	u8 Reserved0[3];
	u8 Prevent;
	//u8 Reserved1:6 ;
	//u8 Control;
} __attribute__((__packed__)) MEDIA_REMOVAL_SPC, *PMEDIA_REMOVAL_SPC;

/*
// Request Sense
*/
typedef struct _REQUEST_SENSE_SPC {
    u8 OperationCode;    /* 03H */
    u8 Reserved[3];
    u8 AllocationLen;
    u8 Control;
} __attribute__((__packed__)) REQUEST_SENSE_SPC, *PREQUEST_SENSE_SPC;

typedef struct _REQUEST_SENSE_DATA {
    u8 ResponseCode : 7;
    u8 Valid : 1;

    u8 SegmentNum;

    u8 SenseKey : 4;
    u8 Reserved0 : 1;
    u8 WrongLenIndicator : 1;
    u8 EndofMedium : 1;
    u8 FileMark : 1;

    u8 Info_0;
    u8 Info_1;
    u8 Info_2;
    u8 Info_3;

    u8 AdditionalSenseLen;

    u8 CommandSpecInfo_0;
    u8 CommandSpecInfo_1;
    u8 CommandSpecInfo_2;
    u8 CommandSpecInfo_3;

    u8 ASC;
    u8 ASCQ;
    u8 FieldReplacableUnitCode;
    u8 SenseKeySpec_0 : 7;
    u8 SenseKeySpecValid : 1;
    u8 SenseKeySpec_1;
    u8 SenseKeySpec_2;

} __attribute__((__packed__)) REQUEST_SENSE_DATA, *PREQUEST_SENSE_DATA;

/*
// Test Unit Ready
*/
typedef struct _TEST_UNIT_SPC {
	u8 OperationCode;    /* 00H */
	u8 Reserved[4];
	u8 Control;
} __attribute__((__packed__)) TEST_UNIT_SPC, *PTEST_UNIT_SPC;

/*
// Write Buffer
*/
typedef struct _WRITE_BUFFER_SPC {
    u8 OperationCode;    /* 3BH */
    u8 Mode:4 ;
    u8 Reserved0:4 ;
	u8 BufferID;
    u8 BufferOff_2;
    u8 BufferOff_1;
    u8 BufferOff_0;
    u8 ParameterLen_2;
    u8 ParameterLen_1;
	u8 ParameterLen_0;
    u8 Control;
} __attribute__((__packed__)) WRITE_BUFFER_SPC, *PWRITE_BUFFER_SPC;

typedef union _CDB_RBC
{
    GENERIC_CDB             Cdb_Generic;
  
     // RBC commands
    GENERIC_RBC             RbcCdb_Generic;

    FORMAT_RBC              RbcCdb_Format;
    READ_RBC                RbcCdb_Read;
    READ_CAPACITY_RBC       RbcCdb_ReadCapacity;
    START_STOP_RBC          RbcCdb_OnOffUnit;
    SYNCHRONIZE_CACHE_RBC   RbcCdb_SyncCache;
    VERIFY_RBC              RbcCdb_Verify;
    WRITE_RBC               RbcCdb_Write;

   
    // SPC-2 commands
   
    INQUIRY_SPC             SpcCdb_Inquiry;
    MODE_SELECT_SPC         SpcCdb_ModeSelect;
    MODE_SENSE_SPC          SpcCdb_ModeSense;
    MEDIA_REMOVAL_SPC       SpcCdb_Remove;
    REQUEST_SENSE_SPC       SpcCdb_RequestSense;
    TEST_UNIT_SPC           SpcCdb_TestUnit;
    WRITE_BUFFER_SPC        SpcCdb_WriteBuffer;

    // ATAPI Commands
    READ_10         CmdRead10;
    WRITE_10        CmdWrite10;
    MODE_SELECT_10  CmdModeSel10;
    MODE_SENSE_10   CmdModeSen10;
    //////////////////////////////////////
    READ_LONG_CMD	SpcCdb_ReadLong;

} CDB_RBC, *PCDB_RBC;


#endif/* _UFICMD_H_ */
