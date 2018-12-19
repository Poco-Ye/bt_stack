#ifndef _FAT_H_
#define _FAT_H_

#include "../usb/usb_sys.h"
#include "fsapi.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define FAT_IS_ERROR(ret) (((int)ret)<0&&((int)ret)>FAT_ERROR_MAX)

#define FAT_UDISK_DIR "udisk"
#define FAT_FLASH_DIR "flash"
#define FAT_SDCARD_DIR "sd_card"
#define FAT_UDISK_A_DIR "udisk_a"
#define FAT_DISK_NUM FS_MAX_NUMS

struct _drive_info;
typedef struct fat_driver_ops
{
	int (*read)(struct _drive_info *drv, 
			int start_sector, 
			int sector_num, 
			char *buf, int len);
	int (*write)(struct _drive_info *drv, 
			int start_sector, 
			int sector_num, 
			char *buf, int len);
	int (*w_submit)(struct _drive_info *drv);
}fat_driver_op;

/*
**  FAT Region:
**      Reserved Region
**      FAT Region
**      Root Dir Region (0 in FAT32)
**      Data Region(file and dir)
**/
typedef struct _drive_info 
{
	u8  bDevice;/* device num: FS_UDISK, or FS_FLASH, or FS_SDCARD */
	u8  bIsFAT32;
	u8  bCacheEnable;/* 是否打开cache功能*/
	u32 FATSectorOff;/* 上次分配FAT表的偏移*/
	//u8  bFlags;

	//Sector Info
	u32 dwSectorTotal;/* Sector 总数*/
	u16 wSectorSize;/* in bytes, 512, 1024, 2048 or 4096*/
	u8  bSectorSizePower;/* sector size: 2的n次方,合法值为9,10 ,11,12*/
	u16 wRsvSecCnt;/* Number of reserved sectors */
	u32 dwHiddSecCnt;/* Number of Hidden sectors */

	//Cluster Info
	u8  bSectorsPerCluster;/* 合法值为 1, 2, 4, 8, 16, 32, 64*/
	u8  bClusterPower;/* A Cluster's sector num: 2的n次方,
	                                  合法值为 0,1,2,3,4,5,6*/
	u32 dwClusterTotal;/* 整个disk的cluster数目*/
	u32 dwClusterSize;/* in bytes */

	//FAT Info
	u32 dwFAT1StartSector;/* fat1 start sector no */
	u32 dwFAT2StartSector;/* fat2 start sector no */
	u32 dwFATSize;/* in sectors */
	
	//Root Dir Info
	u16 wRootEntCnt;/* the count of 32-byte directory entries in the root directory*/
	u32 dwFirstRootDirSector;/* 根目录开始的sector号*/
	u32 dwRootCluster;/* 根目录开始的cluster号*/
	u32 dwRootDirSectors;/* 根目录占用的sector数*/

	//Data Info
	u32 dwCluster2StartSector;
	u32 dwFirstDataSector;
	u32 dwDataSectors;
	u32 dwDataClusters;

	fat_driver_op   *ops;
} DRIVE_INFO;

int fat_mount(int driver, DRIVE_INFO *info);
int fat_unmount(int driver);


//
// Define for correct return values Nut/OS
//
#define NUTDEV_OK                       0
#define NUTDEV_ERROR                    -1


#define FAT_MAX_DRIVE                   FS_MAX_NUMS

//
// Some defines for the FAT structures
//
#define ZIP_DRIVE_BR_SECTOR             32

#define BPB_RsvdSecCnt                  32
#define BPB_NumFATs                     2
#define BPB_HiddSec                     63

#define FAT32_MEDIA                     0xf8

#define FAT32_OFFSET_FSINFO             1
#define FAT32_OFFSET_BACKUP_BOOT        6

#define FAT16_CLUSTER_EOF               0x0000FFFF
#define FAT16_CLUSTER_ERROR             0x0000FFF7
#define FAT16_CLUSTER_MASK              0x0000FFFF

#define FAT32_CLUSTER_EOF               0x0FFFFFFF
#define FAT32_CLUSTER_ERROR             0x0FFFFFF7
#define FAT32_CLUSTER_MASK              0x0FFFFFFF

#define FAT_SIGNATURE                   0xAA55

#define MBR_SIGNATURE                   FAT_SIGNATURE
#define MBR_FAT32                       0x0C

#define FSINFO_FIRSTSIGNATURE           0x41615252
#define FSINFO_FSINFOSIGNATURE          0x61417272
#define FSINFO_SIGNATURE                FAT_SIGNATURE

#define DIRECTORY_ATTRIBUTE_READ_ONLY   0x01
#define DIRECTORY_ATTRIBUTE_HIDDEN      0x02
#define DIRECTORY_ATTRIBUTE_SYSTEM_FILE 0x04
#define DIRECTORY_ATTRIBUTE_VOLUME_ID   0x08
#define DIRECTORY_ATTRIBUTE_DIRECTORY   0x10
#define DIRECTORY_ATTRIBUTE_ARCHIVE     0x20

//
// DIRECTORY_ATTRIBUTE_READ_ONLY   |
// DIRECTORY_ATTRIBUTE_HIDDEN      |
// DIRECTORY_ATTRIBUTE_SYSTEM_FILE |
// DIRECTORY_ATTRIBUTE_VOLUME_ID
//
#define DIRECTORY_ATTRIBUTE_LONG_NAME   0x0F


//
// DIRECTORY_ATTRIBUTE_READ_ONLY   |
// DIRECTORY_ATTRIBUTE_HIDDEN      |
// DIRECTORY_ATTRIBUTE_SYSTEM_FILE |
// DIRECTORY_ATTRIBUTE_VOLUME_ID   |
// DIRECTORY_ATTRIBUTE_DIRECTORY   |
// DIRECTORY_ATTRIBUTE_ARCHIVE
// 
#define DIRECTORY_ATTRIBUTE_LONG_NAME_MASK  0x3F

#define FAT_NAME_LEN                    8
#define FAT_EXT_LEN                     3

//
// FAT_SHORT_NAME_LEN name len = 
// name + ext + 1 for the point
//
#define FAT_SHORT_NAME_LEN              (FAT_NAME_LEN+FAT_EXT_LEN+1)
#define FAT_LONG_NAME_LEN               64


//
// Some stuff for HD and CD, DRIVE_INFO Flags
// 
//
#define FLAG_FAT_IS_CDROM               0x0001
#define FLAG_FAT_IS_ZIP                 0x0002

//
//  DiskSize to SectorPerCluster table
//
typedef struct 
{
	u32 DiskSize;
	u8  SecPerClusVal;
} DSKSZTOSECPERCLUS;


#define CASE_LOWER_BASE	8	/* base is lower case */
#define CASE_LOWER_EXT	16	/* extension is lower case */

struct _FAT32FileDataTime 
{
	u32 Seconds:5;
	u32 Minute:6;
	u32 Hour:5;
	u32 Day:5;
	u32 Month:4;
	u32 Year:7;
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32FileDataTime FAT32_FILEDATETIME, *PFAT32_FILEDATETIME;

struct _FAT32DirectoryEntry 
{
	u8  Name[FAT_NAME_LEN];
	u8  Extension[FAT_EXT_LEN];
	u8  Attribute;
	u8  lcase;/* Case for base and extension */
	u8  CrtTimeTenth;
	FAT32_FILEDATETIME crDate;/* Create Date */
	u8  Reserved[2];
	u16 HighCluster;
	FAT32_FILEDATETIME Date;/* write Date */
	u16 LowCluster;
	u32 FileSize;
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32DirectoryEntry  FAT32_DIRECTORY_ENTRY;

struct _FAT32DirectoryEntryLong 
{
	u8  Order;
	u16 Name1[5];
	u8  Attribute;
	u8  Type;
	u8  Chksum;
	u16 Name2[6];
	u16 LowCluster;
	u16 Name3[2];
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32DirectoryEntryLong  FAT32_DIRECTORY_ENTRY_LONG;


struct _FAT32FileSystemInformation 
{
    u32 FirstSignature;
    u8 Reserved1[480];
    u32 FSInfoSignature;
    u32 NumberOfFreeClusters;
    u32 MostRecentlyAllocatedCluster;
    u8 Reserved2[12];
    u8 Reserved3[2];
    u16  Signature;
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32FileSystemInformation FAT32_FSINFO;

struct _FAT32PartitionEntry {
    u8 BootInd;
    u8 FirstHead;
    u8 FirstSector;
    u8 FirstTrack;
    u8 FileSystem;
    u8 LastHead;
    u8 LastSector;
    u8 LastTrack;
    u32 StartSectors;
    u32 NumSectors;
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32PartitionEntry FAT32_PARTITION_ENTRY;

struct _FAT32PartionTable 
{
    u8 LoadInstruction[446];
    FAT32_PARTITION_ENTRY Partition[4];
    u16  Signature;             /* AA55 */
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32PartionTable FAT32_PARTITION_TABLE;

struct _bpbfat16 
{
    u8 DrvNum;
    u8 Reserved1;
    u8 BootSig;
    u32 VollID;
    u8 VolLab[11];
    u8 FilSysType[8];
    u8 Reserved2[28];
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _bpbfat16 BPBFAT16;

struct _bpbfat32 
{
    u32 FATSz32;              // xxx
    u16  ExtFlags;              // 0
    u16  FSVer;                 // must 0
    u32 RootClus;             // 
    u16  FSInfo;                // typically 1
    u16  BkBootSec;             // typically 6
    u8 Reserved[12];          // set all to zero
    u8 DrvNum;                // must 0x80
    u8 Reserved1;             // set all to zero
    u8 BootSig;               // must 0x29 
    u32 VollID;               // xxx
    u8 VolLab[11];            // "abcdefghijk"
    u8 FilSysType[8];         // "FAT32   "
} 
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _bpbfat32 BPBFAT32;

typedef union _bpboffset36 
{
    BPBFAT16 FAT16;
    BPBFAT32 FAT32;
} BPBOFFSET36;

struct _FAT32BootRecord 
{
    u8 JumpBoot[3];           // 0xeb, 0x58, 0x90
    u8 OEMName[8];            // "MSWIN4.1"
    u16  BytsPerSec;            // must 512
    u8 SecPerClus;            // 8 for 4K cluster
    u16  RsvdSecCnt;            // typically 32 for FAT32
    u8 NumFATs;               // always 2
    u16  RootEntCnt;            // must 0 for FAT32
    u16  TotSec16;              // must 0 for FAT32
    u8 Media;                 // must 0xf8

    u16  FATSz16;               // 0   for FAT32
    u16  SecPerTrk;             // 63  for FAT32
    u16  NumHeads;              // 255 for FAT32
    u32 HiddSec;              // 63  for FAT32

    u32 TotSec32;             // xxx

    BPBOFFSET36 Off36;

    u8 Reserved[420];
    u16  Signature;             // must 0xAA55
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#else
	__attribute__((__packed__))
#endif
;
typedef struct _FAT32BootRecord FAT32_BOOT_RECORD, *PFAT32_BOOT_RECORD;

typedef struct _fat_entry_table16 {
    u16  aEntry[256];
} FAT_ENTRY_TABLE16;

typedef struct _fat_entry_table32 {
    u32 aEntry[128];
}  FAT_ENTRY_TABLE32;

typedef union _fat_dir_table {
    FAT32_DIRECTORY_ENTRY aShort[16];
    FAT32_DIRECTORY_ENTRY_LONG aLong[16];
} FAT_DIR_TABLE;

#define FAT_DIR_SIZE 32

/*
** short name 字符合法性检查
**/
#define SHORT_NAME_CHAR_GOOD(c) ((c)!='+'&&(c)!=','&&\
	(c)!=';'&&c!='='&&c!='['&&c!=']'&&(c)!='/'&&(c)!='\\'&&\
	(c)!=':'&&c!='*'&&c!='|'&&c!='?'&&(c)!='\''&&(c)!='\"'&&\
	(c)!='<'&&c!='>'\
	)

#define INODE_DIR(inode) (((inode)->attr&ATTR_DIR)==ATTR_DIR)

typedef struct _offset
{
	u32 bytes;/* in bytes */
	u32 cluster;
	u32 sector;
	u32 eof;
}_offset_t;


#define INODE_INV(inode) ((inode)->flag&0x1)/* 无效标志*/
#define INODE_SET_INV(inode) ((inode)->flag)|=0x1

typedef struct inode_s
{
	u32 flag;/* 是否有效标志位*/
	int long_dir_num;
	u8  long_name[LONG_NAME_MAX+2];
	u16 long_name_size;
	u8  short_name[SHORT_NAME_MAX+1];
	u8  short_name_size;
	u8  attr;
	FAT32_FILEDATETIME wrtime;/* lastly write time */
	FAT32_FILEDATETIME crtime;/* create time */
	u32 first_cluster;
	u32 first_sector;
	u32 parent_sector;/* parent first sector */
	u32 size;/* file size */

	/* 下面信息为dir_ent，对更新file size有用*/
	u32 long_dir_start_cluster;
	u32 long_dir_start_sector;/* long dir 开始扇区*/
	u32 long_dir_start_sector_offset;/* 扇区偏移*/
	u32 long_dir_start_byte;/* long dir 字节偏移*/
	/* 当long_dir_num==0上面域无效*/
	
	u32 short_dir_start_sector;
	u32 short_dir_start_sector_offset;
	
	_offset_t   offset;

	DRIVE_INFO *drv;
}inode_t;

void fat_read_seek(inode_t *inode, u32 offset, u8 flag);
void fat_write_seek(inode_t *inode, u32 offset, u8 flag);
#define FAT_SEEK_CUR 0x1
#define FAT_SEEK_START 0x2
void fat_seek(inode_t *inode, _offset_t *offset, u32 byte_offset, u8 flag);

int fat_sig_ok(int drv_num,u8 *buf);
u8 get_power(u32 value);

#define FAT_BAD_NO ((u32)-1)

#define INODE_DEL  TRUE
#define INODE_NODEL FALSE

//void (*udisk_hook)(void);

void fat_tbl_fix(int drv_num, int bFix);
void fs_fmt_rate(int rate);/* 格式化进度*/
#endif/* _FAT_H_ */
