/*
**  FAT file system
**  1.支持FAT16和FAT32两种版本
**  2.支持文件创建、删除和修改名字
**  3.支持目录创建、修改名字，
**     以及对空目录的删除
//2010.4.12--Inhibit tail '/' in FsOpen()
//2010.9.02--Added loops in FsFileWrite()
//2010.12.23--Deleted power check for S80 APIs
//2011.1.26--Added partition process in fat_mount()
**/
#include "fat.h"
#include "fcach.h"
#include  "..\kb\kb.h"
#include  "..\comm\comm.h"
//#define UDISK_TEST_OPEN
#define USB_DRV_VER 0x21

#define s80_printf //s_printf

static DRIVE_INFO *sDriveInfo[FAT_MAX_DRIVE];
static u8 fat_str_tmp[LONG_NAME_MAX+2];
void (*fmt_cb)(int rate)=NULL;/* 格式化进度回调函数*/
int fmt_rate = 0;
extern int usb_host_ohci_state;
extern int base_usb_ohci_state;
void set_udisk_a_absent(void);

/*
** 用于记录空闲的目录项(在一个sector内连续的)
**/
typedef struct dir_ent_free
{
	u32 long_name_N;/* N<=10, 长目录所占用dir ent数 */
	u32 bytes;/* 字节偏移*/
	u32 sector;
	u32 sector_offset;
}DIR_ENT_FREE;

#define DIR_ENT_FREE_SET(dir_free, sector, offset, _bytes)do{ \
	if(dir_free&&(dir_free)->sector==FAT_BAD_NO) \
	{ \
		(dir_free)->sector = sector; \
		(dir_free)->sector_offset = offset; \
		(dir_free)->bytes = _bytes; \
	} \
}while(0)


#define fat_debug //s80_printf
#define fat_error //s_printf

void fat_buf_debug(u8 *buf, int size, char *descr)
{
	int line, j, i;
	
	fat_debug("\n%s:\n", descr);
	line = size>>4;
	for(j=0; j<line; j++)
	{
		fat_debug("[%04x] ", j);
		for(i=0; i<16; i++)
		{
			fat_debug("%02x ", buf[i]);
		}
		fat_debug(" ");
		for(i=0; i<16; i++)
		{
			if(buf[i]>=0x20&&buf[i]<=0x7e)
			{
				fat_debug("%c", buf[i]);
			}else
			{
				fat_debug(".");
			}
		}
		fat_debug("\n");
		buf += 16;
	}
	if((size&0xf)>0)
	{		
		fat_debug("[%04x] ", j);
	}
	for(i=0; i<(size&0xf); i++)
	{
		fat_debug("0x%02x,", buf[i]);
	}
	for(;i<16; i++)
	{
		fat_debug("     ");
	}
	fat_debug(" ");
	for(i=0; i<(size&0xf); i++)
	{
		if(buf[i]>=0x20&&buf[i]<=0x7e)
		{
			fat_debug("%c", buf[i]);
		}else
		{
			fat_debug(".");
		}
	}
	fat_debug("\n");
	fat_debug("-----------------------------------\n");
}

void fat_name_print(u8 *buf, int len)
{
	int i;
	fat_debug("'");
	for(i=0; i<len; i++)
	{
		if(buf[i]==0)
			continue;
		if(buf[i]<=0x7f)
		{
			//ascii
			fat_debug("%c",buf[i]);
		}else
		{
			fat_debug("?");
		}
	}
	fat_debug("'");
}

void fat_attr_print(u8 attr)
{
	if(attr&ATTR_RO)
	{
		fat_debug(" -Ro");
	}
	if(attr&ATTR_HID)
	{
		fat_debug(" -Hid");
	}
	if(attr&ATTR_SYS)
	{
		fat_debug(" -Sys");
	}
	if(attr&ATTR_VOL)
	{
		fat_debug(" -Vol");
	}
	if(attr&ATTR_DIR)
	{
		fat_debug(" -Dir");
	}
	if(attr&ATTR_ARC)
	{
		fat_debug(" -Arc");
	}
	fat_debug(" ");
}

int GetDriveInfo(DRIVE_INFO *drv)
{
	if(NULL == sDriveInfo[3]) return -1;
	memcpy(drv,sDriveInfo[3],sizeof(DRIVE_INFO));
	return 0;
}

u8 get_power(u32 value)
{
	u8 p;
	p=0;
	for(p=0;value>(u32)(1<<p);p++)
		;
	if(value != (u32)(1<<p))
		return 0;//bad 
	return p;
}

int fat_sig_ok(int drv_num,u8 *buf)
{
	int i;
	if (drv_num == FS_UDISK || drv_num==FS_UDISK_A)
	{
		if(buf[510]==0x55&&buf[511]==0xaa || buf[510]==0x00&&buf[511]==0x00 ||
			buf[510]==0xff&&buf[511]==0xff)
			return FS_OK;
	}
	else if (drv_num == FS_FLASH || drv_num == FS_SDCARD)
	{
	if(buf[510]==0x55&&buf[511]==0xaa)
		return FS_OK;
	}

	return -1;
}

#define DIR_FREE_FLAG 0xE5
#define DIR_ALL_FREE_FLAG 0x00

#define FAT_DIR_NOUSED(dir) (((u8*)(dir))[0] == DIR_FREE_FLAG)
#define FAT_DIR_ALL_FREE(dir) (((u8*)(dir))[0] == DIR_ALL_FREE_FLAG)
#define FAT_DIR_FREE(dir) (FAT_DIR_NOUSED(dir)||FAT_DIR_ALL_FREE(dir))
#define FAT_DIR_LONG(dir) (((u8*)(dir))[11] == ATTR_LONG_NAME)
#define FAT_LAST_LONG_ENTRY(dir) (((u8*)(dir))[0] &0x40)

#define GOOD_CLUSTER(drv, table_value) \
	((table_value)>=2&&(((u32)table_value)-2)<(drv)->dwDataClusters)

//-----------------------------------------------------------------------------
//	fat_shortname_chksum()
//	Returns an unsigned byte checksum computed on an unsigned byte
//	array.  The array must be 11 bytes long and is assumed to contain
//	a name stored in the format of a MS-DOS directory entry.
//	Passed:	 shortname    Pointer to an unsigned byte array assumed to be
//                          11 bytes long.
//	Returns: Sum         An 8-bit unsigned checksum of the array pointed
//                           to by pFcbName.
//------------------------------------------------------------------------------
u8 fat_shortname_chksum (u8 *shortname)
{
	short len;
	u8 Sum;

	Sum = 0;
	for (len=11; len!=0; len--)
	{
		// NOTE: The operation is an unsigned char rotate right
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *shortname++;
	}
	return (Sum);
}

/*
** 扇区转换成簇号
**
**/
static inline u32 sector_to_cluster(DRIVE_INFO *drv, u32 sector)
{
	return (u32)(sector>>(drv->bClusterPower));
}

/*
** 扇区在簇中的偏移量
**
**/
static inline u32 sector_in_cluster(DRIVE_INFO *drv, u32 sector)
{
	return (u32)(sector&((1<<(drv->bClusterPower))-1));
}

/*
** 字节转换成扇区号
**
**/
static inline u32 byte_to_sector(DRIVE_INFO *drv, u32 bytes)
{
	return bytes>>drv->bSectorSizePower;
}
/*
** 字节在扇区的偏移
**
**/
static inline u32 byte_in_sector(DRIVE_INFO *drv, u32 bytes)
{
	return bytes&((1<<drv->bSectorSizePower)-1);
}

/*
** 数据扇区是否合法
**
**/
static inline u8 data_sector_is_good(DRIVE_INFO *drv, u32 sector)
{
	if(drv->bIsFAT32)
	{
		if(sector<drv->dwFirstDataSector||sector>=drv->dwSectorTotal)
			return FALSE;
		return TRUE;
	}else
	{
		if(sector<drv->dwFirstRootDirSector||sector>=drv->dwSectorTotal)
			return FALSE;
		return TRUE;
	}
}

/*
** 收集long dir中的名字信息
** 注意:名字格式为unicode
**/
int fat_get_long_name(FAT32_DIRECTORY_ENTRY_LONG *dir_ent_long, u8 *buff, int size)
{
	int offset;
	memcpy(buff, dir_ent_long->Name1, 10);
	memcpy(buff+10, dir_ent_long->Name2, 12);
	memcpy(buff+22, dir_ent_long->Name3, 4);
	for(offset=0; offset<26; offset+=2)
		if(buff[offset]==0&&buff[offset+1]==0)
			break;
	return offset;
}

/*
** 收集short dir中的名字信息
** 字符串格式为ascii
**/
int fat_get_short_name(FAT32_DIRECTORY_ENTRY *dir_ent, u8 *buff)
{
	int offset, i, ext_idx;
	u8 *ext = dir_ent->Extension;

	/*
	** Copy Base Name
	**/
	memcpy(buff, dir_ent->Name, FAT_NAME_LEN+1);
	if(buff[0] == ' ')
		return 0;
	for(offset=FAT_NAME_LEN; offset>0; offset--)
	{
		if(buff[offset-1]!=' ')
			break;
	}
	buff[offset] = 0;
	if(dir_ent->lcase&CASE_LOWER_BASE)
	{
		for(i=0; i<offset; i++)
		{
			if(buff[i]>='A'&&buff[i]<='Z')
				buff[i] = buff[i]-'A'+'a';
		}
	}

	/*
	** Copy Ext Type
	**/
	for(i=FAT_EXT_LEN; i>0; i--)
	{
		if(ext[i-1]!=' ')
			break;			
	}
	if(i<=0)
		return offset;
	buff[offset++] = '.';
	memcpy(buff+offset, ext, i);
	ext_idx = offset;
	offset += i;

	if(dir_ent->lcase&CASE_LOWER_EXT)
	{		
		for(i=ext_idx; i<offset; i++)
		{
			if(buff[i]>='A'&&buff[i]<='Z')
				buff[i] = buff[i]-'A'+'a';
		}
	}
	
	return offset;
}

static u32 fat_get_table_entry(DRIVE_INFO *drv, u32 cluster)
{
	FAT_SECTOR_CACHE *fat_buf;
	int err;
	u32  fat_table_sector;
	u32  size, offset, new_cluster;
	if(drv->bIsFAT32)
	{
		size = cluster*4;
	}else
	{
		size = cluster*2;
	}
	fat_table_sector = drv->dwFAT1StartSector;
	fat_table_sector += size>>drv->bSectorSizePower;
	err = fat_read(drv, fat_table_sector, 1, &fat_buf);
	if(err != FS_OK)
	{
		return (u32)err;
	}
	offset = byte_in_sector(drv, size);
	#if 0
	fat_debug("cluster[%d], sector[%d], offset[%d]\n", 
		cluster,
		fat_table_sector,
		offset
		);
	#endif
	if(drv->bIsFAT32)
	{
		u32 *ptable_32 = (u32*)((u8*)FAT_BUF_P(fat_buf)+offset);
		new_cluster = ftohl(*ptable_32)&FAT32_CLUSTER_MASK;
		if(new_cluster == FAT32_CLUSTER_EOF||new_cluster==FAT32_CLUSTER_ERROR)
			return FAT_BAD_NO;
		if(!GOOD_CLUSTER(drv, new_cluster))
			return FAT_BAD_NO;
		return new_cluster;
	}else
	{
		u16 *ptable_16 = (u16*)((u8*)FAT_BUF_P(fat_buf)+offset);
		//fat_debug("FAT table %04x, offset=%02x\n", *ptable_16, offset);
		new_cluster = ftohs(*ptable_16)&FAT16_CLUSTER_MASK;
		if(new_cluster == FAT16_CLUSTER_EOF||new_cluster==FAT16_CLUSTER_ERROR)
			return FAT_BAD_NO;
		if(!GOOD_CLUSTER(drv, new_cluster))
			return FAT_BAD_NO;
		return new_cluster;
	}
}

/*
** 根据fat表获取下一个链接的cluster
**
**/
static u32 fat_get_next_cluster(DRIVE_INFO *drv, u32 cluster)
{
	u32 table_value;
	table_value = fat_get_table_entry(drv, cluster);
	if(GOOD_CLUSTER(drv, table_value))
	{
		fat_debug("fat_get_next_cluster: %d-->%d\n",
			cluster, table_value);
		return table_value;
	}else
	{
		fat_debug("fat_get_next_cluster: %d-->BAD\n",
			cluster);
		return FAT_BAD_NO;
	}
}

static u32 fat_get_cluster(DRIVE_INFO *drv, FAT32_DIRECTORY_ENTRY *dir)
{
	u32 cluster;

	if(drv->bIsFAT32)
	{
		cluster = (ftohs(dir->HighCluster)<<16)|ftohs(dir->LowCluster);
	}else
	{
		cluster = ftohs(dir->LowCluster);
	}
	if(cluster == 0)
	{
		if(dir->Attribute&ATTR_DIR)
			//this is root dir
			return drv->dwRootCluster;
		else
			return 0;//未分配空间
	}else if(!GOOD_CLUSTER(drv, cluster))
	{
		//bad cluster NO
		return FAT_BAD_NO;
	}
	return cluster;
}
/*
** 获取dir(file)数据的第一个sector区
**
**/
static u32 fat_get_first_sector(DRIVE_INFO *drv, u32 cluster)
{
	if(cluster==0||cluster==FAT_BAD_NO)
	{
		return FAT_BAD_NO;
	}
	if(cluster == 1)
	{
		//root dir
		assert(drv->bIsFAT32==FALSE);
		return drv->dwFirstRootDirSector;
	}
	cluster -= 2;
	return (cluster<<drv->bClusterPower)+drv->dwFirstDataSector;
}

static u32 fat_sector_to_cluster(DRIVE_INFO *drv, u32 sector)
{
	if(drv->bIsFAT32==FALSE&&
		sector>=drv->dwFirstRootDirSector&&
		sector<(drv->dwFirstRootDirSector+drv->dwRootDirSectors))
	{
		return 1;
	}
	if(sector<drv->dwFirstDataSector)
		return FAT_BAD_NO;
	return ((sector-drv->dwFirstDataSector)>>drv->bClusterPower)+2;
}

/*
** for file or director (NOT Root Dir)
** for RootDir, please use fat_dir_get_next_sector
**/
u32 fat_get_next_sector(DRIVE_INFO *drv, u32 sector)
{
	u32 cluster, first_sector;
	
	assert(sector>=drv->dwFirstDataSector);
	cluster = ((sector-drv->dwFirstDataSector)>>drv->bClusterPower)+2;
	first_sector = fat_get_first_sector(drv, cluster);
	sector++;
	if(sector<first_sector+drv->bSectorsPerCluster)
	{
		return sector;
	}else
	{
		cluster = fat_get_next_cluster(drv, cluster);
		return fat_get_first_sector(drv, cluster);
	}
}

u32 fat_dir_get_next_sector(DRIVE_INFO *drv, u32 sector)
{
	if(sector<(drv->dwFAT2StartSector+drv->dwFATSize)||
		sector>drv->dwSectorTotal)
	{
		fat_error("fat_dir_get_next_sector: sector[%d] is bad!\n",
			sector
			);
		return FAT_BAD_NO;
	}
	if(drv->bIsFAT32)
	{
		// FAT32
		return fat_get_next_sector(drv, sector);
	}else
	{
		//FAT16
		if(sector<drv->dwFirstRootDirSector)
		{
			return FAT_BAD_NO;
		}
		if(sector<(drv->dwFirstRootDirSector+drv->dwRootDirSectors))
		{
			//root dir
			if(sector == (drv->dwFirstRootDirSector+drv->dwRootDirSectors-1))
			{
				return FAT_BAD_NO;
			}
			return sector+1;
		}else
		{
			return fat_get_next_sector(drv, sector);
		}		
	}
}

/*
** ASCII字符串(以0结尾)转换成UNICODE长名字格式
** FAT长名字以2个字节作为一个字符
**/
u8 *fat_ascii_to_UNICODE(u8 *str, int len)
{
	int i;
	if(len>(LONG_NAME_MAX/2))
		return NULL;
	memset(fat_str_tmp, 0, sizeof(fat_str_tmp));
	for(i=0; i<len; i++)
		fat_str_tmp[i*2] = str[i];
	return fat_str_tmp;
}

/*
** UNICODE长名字格式转换成ASCII字符串
**
**/
static u8 *fat_UNICODE_to_ascii(u8 *str, int len)
{
	static u8 ascii_str[LONG_NAME_MAX];
	int i;

	if(len>LONG_NAME_MAX)
		return NULL;
	for(i=0; i<(len/2); i++)
	{
		ascii_str[i] = str[i*2];
	}
	ascii_str[i] = 0;
	return ascii_str;
}

static inline u8 fat_char_upper(u8 c)
{
	if(c>='a'&&c<'z')
		return c-'a'+'A';
	return c;
}

u8 fat_short_name_cmp(u8 *src1, int src1_len, u8 *src2, int src2_len)
{
	int i;
	if(src1_len != src2_len)
		return FALSE;
	for(i=0; i<src1_len; i++)
	{
		if(fat_char_upper(src1[i])!=fat_char_upper(src2[i]))
			return FALSE;
	}
	return TRUE;
}

u8 fat_long_name_cmp(u8 *user, int user_len, int fmt, u8 *fat, int fat_len)
{
	int i;
	fat_buf_debug(user, user_len, "user input name");
	fat_buf_debug(fat, fat_len, "fat input name");
	switch(fmt)
	{
	case NAME_FMT_UNICODE:
		fat_debug("FAT UNICODE!\n");
		if(user_len != fat_len)
			return FALSE;
		for(i=0; i<fat_len; i++)
			if(fat_char_upper(user[i])!=fat_char_upper(fat[i]))
				return FALSE;
		return TRUE;
	default:
		fat_debug("FAT ASCII!\n");
		if(user_len*2 != fat_len)
		{
			return FALSE;
		}
		for(i=0; i<user_len; i++)
		{
			if(fat[1] != 0)
				return FALSE;/* we can not support */
			if(fat[0]>0x7f)
			{
				//no ascii
				return FALSE;
			}
			if(fat_char_upper(fat[0])!=fat_char_upper(user[0]))
			{
				return FALSE;
			}
			fat += 2;
			user++;
		}
		return TRUE;
	}
}

void fat_show_driv_info(DRIVE_INFO *info)
{
	s80_printf("------------------------------------\r\n");
	s80_printf("%s Driv Info:\r\n", (info->bDevice==FS_UDISK)?"UDISK":"FLASH");
	s80_printf("\tFile System: %s\r\n"
		"\tSector: size=%d byte(2power(%d)), total %d , Reserved %d, Hidden %d\r\n"
		"\tCluster: size=%d sector(2power(%d)), total %d, unit=%d bytes\r\n"
		"\tFAT: size=%d sectors, FAT1 start %d sector, FAT2 start %d sector\r\n"
		"\tRoot Dir: entry count=%d, start from %d sector(%d cluster), total %d sectors\r\n"
		"\tData: start from %d sector, total %d sectors(%d clusters)\r\n"
		,
		info->bIsFAT32==1?"FAT32":"FAT16",
		//sector
		info->wSectorSize,
		info->bSectorSizePower,
		info->dwSectorTotal,
		info->wRsvSecCnt,
		info->dwHiddSecCnt,
		//cluster
		info->bSectorsPerCluster,
		info->bClusterPower,
		info->dwClusterTotal,
		info->dwClusterSize,
		//FAT
		info->dwFATSize,
		info->dwFAT1StartSector,
		info->dwFAT2StartSector,
		//root dir
		info->wRootEntCnt,
		info->dwFirstRootDirSector,
		info->dwRootCluster,
		info->dwRootDirSectors,
		//Data
		info->dwFirstDataSector,
		info->dwDataSectors,
		info->dwDataClusters
		);
	s80_printf("------------------------------------\n");
}

void FAT_print(DRIVE_INFO *info, int first)
{
	u32 sector;
	u32 end, i;
	char desc[10];
	FAT_SECTOR_CACHE *fat_buf;

	if(first==1)
	{
		sector = info->dwFAT1StartSector;
		end = info->dwFAT1StartSector+info->dwFATSize;
		fat_debug("FAT1 all info........\n");
	}
	else
	{
		sector = info->dwFAT2StartSector;
		end = info->dwFAT2StartSector+info->dwFATSize;
		fat_debug("FAT2 all info........\n");
	}

	i = 0;
	for(; sector<end; sector++)
	{
		fat_read(info, sector, 1, &fat_buf);
		if(fat_buf == NULL)
		{
			fat_error("fat_print: fail to read sector[%d]\n",
				sector
				);
			return ;
		}
		if(info->bIsFAT32)
		{
		}else
		{
			sprintf(desc, "FAT[%d]", i++);
			fat_buf_debug(FAT_BUF_P(fat_buf), info->wSectorSize, desc);
		}
		DelayMs(300);
	}
}

/*
** 读取文件内容(bit流)
**/
static int fat_read_file(inode_t *inode, /* IN */
				u8 *buffer, /* OUT */
				int buffer_len)
{
	DRIVE_INFO *drv = inode->drv;
	FAT_SECTOR_CACHE *fat_buf;
	int err;
	u32 sector, sector_offset, total;
	int len;
	
	assert(!INODE_DIR(inode));
	if(inode->offset.bytes>=inode->size||inode->offset.eof)
	{
		inode->offset.eof = TRUE;
		fat_debug("fat_read_file: file eof, size=%d, offset=%d\n",
			inode->size, inode->offset.bytes);
		return FS_ERR_EOF;
	}
	total = 0;
	while(buffer_len>0)
	{
		sector = inode->offset.sector;
		if(inode->offset.eof)
		{
			fat_debug("fat_read_file: EOF!\n");
			break;
		}
		if(sector<drv->dwFirstDataSector||sector>=drv->dwSectorTotal)
		{
			fat_debug("fat_read_file: sector[%d] is illegal!\n",
				sector);
			break;
		}
		if(data_sector_is_good(drv, sector)==FALSE)
		{
			break;
		}
		sector_offset = byte_in_sector(drv, inode->offset.bytes);
		fat_debug("fat_read_file: sector[%d], offset[%d]!\n",
			sector, sector_offset);
		
		/* 有效数据长度*/
		len = inode->size-inode->offset.bytes;
		if((u32)len > (drv->wSectorSize-sector_offset))
		{
			/* 扇区剩余长度*/
			len = drv->wSectorSize-sector_offset;
		}
		if(len > buffer_len)
		{
			/* 缓存长度*/
			len = buffer_len;
		}
		err = fat_read(drv, sector, 1, &fat_buf);
		if(err != FS_OK)
		{
			fat_error("fat_read_file: fail to read sector[%d]\n",
				sector);
			return err;
		}
		memcpy(buffer, (u8*)FAT_BUF_P(fat_buf)+sector_offset, len);
		buffer += len;
		buffer_len -= len;
		total += len;
		fat_read_seek(inode, len, FAT_SEEK_CUR);
		fat_debug("fat_read_file: seek sector=%d, cluster=%d\n",
			inode->offset.sector,
			inode->offset.cluster
			);
		if(inode->offset.bytes == inode->size)
			inode->offset.eof = TRUE;
	}
	return total;
}

/*
**  读取目录内容,直到找到对应的节点或结束
**  name:  节点名称，null表示读取一个立刻返回
**  child: inode信息
**  dir_ent_free:收集到空闲dir ent信息
**  注意: fat_read_dir从parent的offset开始读取
**/
static int fat_read_dir(inode_t *parent,/* IN */
			char *name,/* IN */
			int name_size,/* IN */
			u8  name_fmt,/* IN */
			inode_t *child,/* OUT */
			DIR_ENT_FREE *dir_ent_free/* OUT */
			)
{
	int err;
	u8 long_name[LONG_NAME_MAX+2];
	u8 short_name[SHORT_NAME_MAX+1];
	u16 long_name_size = 0;
	u8  short_name_size = 0;
	u8  bIngoreLong=TRUE, bWrite;	
	FAT32_DIRECTORY_ENTRY *dir_ent;
	FAT32_DIRECTORY_ENTRY_LONG *dir_ent_long;
	DRIVE_INFO *drv = parent->drv;
	FAT_SECTOR_CACHE *fat_buf;
	u8  *buf;
	u32 sector;/* sector no */
	u32 sector_offset;/* in bytes */
	u32 dir_free_start, dir_free_end, dir_bytes_offset;/* 连续一段空闲的目录项*/
	u8  inode_found;
	u32 dir_index, i, dir_num;
	u8  long_dir_chksum;
	u32 long_dir_start_sector;/* long dir 开始结束扇区*/
	u32 long_dir_start_sector_offset;/* 扇区偏移*/
	u32 long_dir_start_byte;/* long dir 字节偏移*/
	u32 long_dir_start_cluster;
	int long_dir_num, max_long_dir_num;
	u32 bytes_offset;

	assert(parent != NULL);
	if((name == NULL || name_size <= 0)&&(name_fmt!=0))
	{
		s80_printf("name = %08x, name_size=%d\n",
			name, name_size);
		assert(name != NULL && name_size>0);
	}
	
	if(!INODE_DIR(parent))
	{
		fat_error("fat_read_dir: parent attr=%02x is not dir!\n",
			parent->attr
			);
		return FS_ERR_ARG;
	}
	inode_found = FALSE;
	if(parent->offset.eof)
	{
		fat_debug("fat_read_dir: read EOF!\n");
		return FS_ERR_EOF;
	}
	memset(child, 0, sizeof(child));
	child->long_dir_num = 0;
	fat_debug("fat_read_dir: cluster=%d,sector=%d\n",
		parent->offset.cluster,
		parent->offset.sector
		);
	for(i=0;!parent->offset.eof;i++)
	{
		sector = parent->offset.sector;
		sector_offset = byte_in_sector(drv, parent->offset.bytes);
		fat_debug("fat_read_dir: Start sector[%d], sector_offset[%d]\n", sector, sector_offset);
		if((sector_offset&(FAT_DIR_SIZE-1))!=0)
		{
			fat_error("fat_read_dir: parent offset %d(%d) is illegal!\n",
				sector,
				sector_offset
				);
			assert(0);
		}
		if(!data_sector_is_good(drv, sector))
		{
			fat_debug("fat_read_dir: sector[%d] is illegal!\n",
				sector);
			return -1;
		}
		err = fat_read(drv, sector, 1, &fat_buf);
		if(err != FS_OK)
		{
			fat_error("fat_read_dir: fail to read sector[%d]!\n",
				sector);
			return err;
		} 
		buf = FAT_BUF_P(fat_buf);
		dir_ent = (void*)(buf+sector_offset);
		dir_free_start = FAT_BAD_NO;
		dir_free_end = FAT_BAD_NO;
		dir_bytes_offset = FAT_BAD_NO;
		dir_index = sector_offset/FAT_DIR_SIZE;
		dir_num = drv->wSectorSize/FAT_DIR_SIZE;
		fat_debug("fat_read_dir: start dir_index=%d\n",dir_index);
		bytes_offset = parent->offset.bytes;
		bWrite = FALSE;
		for(;dir_index<dir_num; 
			dir_index++, dir_ent++, bytes_offset+=FAT_DIR_SIZE)
		{
			if(FAT_DIR_ALL_FREE(dir_ent))
			{
				fat_debug("fat_read_dir: start from sector[%d]offset[%d] all free!\n", 
					sector, dir_index*FAT_DIR_SIZE);
				//已经找到所需的空闲目录表项
				//则我们不需要继续扫描后面的
				if(dir_ent_free==NULL||dir_ent_free->sector != FAT_BAD_NO)
				{
					return FS_ERR_EOF;
				}
				if(dir_ent_free)
				{
					if(dir_free_start!=FAT_BAD_NO&&
						(dir_num-dir_free_start)>dir_ent_free->long_name_N)
					{
						DIR_ENT_FREE_SET(dir_ent_free, sector, dir_free_start*FAT_DIR_SIZE, bytes_offset);
					}
					if(dir_free_start==FAT_BAD_NO&&
						(dir_num-dir_index)>dir_ent_free->long_name_N)
					{
						DIR_ENT_FREE_SET(dir_ent_free, sector, dir_index*FAT_DIR_SIZE, bytes_offset);
					}
				}
				//由于我们分配inode是存放在同一个sector内的
				//因此如果这里不够的话,则我们就不会利用
				//为了让文件系统能找到后面的inode,必须标志为
				//DIR_FREE_FLAG
				*((u8*)dir_ent) = DIR_FREE_FLAG;
				bWrite = TRUE;
				continue;
			}
			if(FAT_DIR_FREE(dir_ent))
			{
				//记录空闲表项的开始位置和结束位置
				if(dir_free_start == FAT_BAD_NO)
					dir_free_start = dir_index;
				if(dir_bytes_offset == FAT_BAD_NO)
					dir_bytes_offset = bytes_offset;
				dir_free_end = dir_index;
				
				if(dir_ent_free&&
					(dir_free_end-dir_free_start+1)
						>=(dir_ent_free->long_name_N+1))
				{
					DIR_ENT_FREE_SET(dir_ent_free, sector, 
						dir_free_start*FAT_DIR_SIZE, dir_bytes_offset);
				}
				continue;
			}else if(dir_ent_free)
			{
				if(dir_free_start != FAT_BAD_NO)
				{
					assert(dir_free_end != FAT_BAD_NO);
					if((dir_free_end-dir_free_start+1)
						>=(dir_ent_free->long_name_N+1))
					{
						DIR_ENT_FREE_SET(dir_ent_free, sector, 
							dir_free_start*FAT_DIR_SIZE, dir_bytes_offset);
					}
				}
			}
			
			/*
			** LONG dir:
			**     nth....
			**     (n-1)th....
			**     .........
			**     1th
			**  SHORT dir
			**/
			if(FAT_DIR_LONG(dir_ent))/* long name */
			{
				int n, offset;
				dir_ent_long = (void*)dir_ent;
				n = dir_ent_long->Order&~0xc0;
				if(FAT_LAST_LONG_ENTRY(dir_ent))
				{
					max_long_dir_num = n;
					long_dir_num = n;
					if(n>((LONG_NAME_MAX-2)/26))
					{
						fat_debug("fat_read_dir: long name is too long!\n");
						continue;
					}
					bIngoreLong = FALSE;
					memset(long_name, 0, LONG_NAME_MAX+2);
					long_name_size = (n-1)*13*2;
					offset = long_name_size;
					long_name_size += fat_get_long_name(dir_ent_long,
						long_name+long_name_size, 26);
					long_dir_chksum = dir_ent_long->Chksum;
					/* 记住所在sector信息*/
					long_dir_start_cluster = parent->offset.cluster;
					long_dir_start_sector = sector;
					long_dir_start_sector_offset = dir_index*FAT_DIR_SIZE;
					long_dir_start_byte = parent->offset.bytes+
						dir_index*FAT_DIR_SIZE-sector_offset;
					
				}else if(!bIngoreLong)
				{
					long_dir_num--;
					if(long_dir_num!=n)
					{
						fat_debug("long dir N is error, %d v.s. %d\n",
							n, long_dir_num);
						bIngoreLong = TRUE;
						continue;
					}
					if(dir_ent_long->Chksum != long_dir_chksum)
					{
						bIngoreLong = TRUE;
						long_name_size = 0;
						fat_debug("Long dir chksum error %02x v.s. %02x\n",
							dir_ent_long->Chksum, long_dir_chksum
							);
						assert(0);
						continue;
					}
					offset = (n-1)*13*2;
					fat_get_long_name(dir_ent_long, long_name+offset, 26);

				}else
				{
					continue;
				}
				//force_urb_printf(long_name+offset, 26, "long name");
			}else//short name
			{
			
				//force_urb_printf(dir_ent->Name, 11, "short name");
				if(long_name_size>0)
				{
					if(long_dir_num!=1)
					{
						fat_error("Long dir ent %d, but only find idx[%d]\n",
							max_long_dir_num, long_dir_num);
						long_name_size = 0;
					}
					if(fat_shortname_chksum(dir_ent->Name)!=long_dir_chksum)
					{
						fat_error("Long dir chksum is bad, ignore it!!!!\n");
						long_name_size = 0;
					}
				}
				bIngoreLong = TRUE;
				inode_found = FALSE;
				short_name_size = fat_get_short_name(dir_ent, short_name);
				if(name_fmt>0)
				{
					/*
					** 首先比较long name,
					** 如果不相同则继续比较short name
					**/
					if(long_name_size>0&&
						fat_long_name_cmp(name, name_size,name_fmt, 
							long_name, long_name_size)==TRUE)
					{
						inode_found = TRUE;
					}
					if(!inode_found&&
						short_name_size>0)
					{
						u8 *t = name;
						int size = name_size;
						if(name_fmt==NAME_FMT_UNICODE)
						{
							name = fat_UNICODE_to_ascii(name, name_size);
							name_size = strlen(name);
						}
						if(name&&fat_short_name_cmp(name, name_size, 
								short_name, short_name_size)==TRUE)
						{
							inode_found = TRUE;
						}
						name = t;
						name_size = size;
					}
				}else
				{
					inode_found = TRUE;
				}
#if 1
				{
					fat_debug("Short name: ");
					fat_name_print(short_name, short_name_size);
					if(long_name_size>0)
					{
						fat_debug(" Long name:");
						fat_name_print(long_name, long_name_size);
					}
					fat_debug(" FirstCluster:%d", 
						fat_get_cluster(drv, dir_ent));
					fat_attr_print(dir_ent->Attribute);
					fat_debug(" size:%d", dir_ent->FileSize);
					if(dir_ent->lcase&CASE_LOWER_BASE)
					{
						fat_debug(" case lower base");
					}
					if(dir_ent->lcase&CASE_LOWER_EXT)
					{
						fat_debug(" case lower ext");
					}
					if(inode_found == TRUE)
					{
						fat_debug(" FOUND!\n");
					}else
					{
						fat_debug(" INGORE!\n");
					}
				}
#endif
				if(inode_found)
				{
					memcpy(child->long_name, long_name, long_name_size);
					child->long_name[long_name_size] = 0;
					child->long_name[long_name_size+1] = 0;
					child->long_name_size = long_name_size;
					memcpy(child->short_name, short_name, short_name_size);
					child->short_name[short_name_size] = 0;
					child->short_name_size = short_name_size;
					child->attr = dir_ent->Attribute;
					child->wrtime = dir_ent->Date;
					child->crtime = dir_ent->crDate;
					child->size = ftohl(dir_ent->FileSize);
					child->first_cluster = fat_get_cluster(drv, dir_ent);
					child->first_sector = fat_get_first_sector(drv, 
						child->first_cluster);
					child->parent_sector = parent->first_sector;
					fat_read_seek(child, 0, FAT_SEEK_START);
					fat_write_seek(child,0,FAT_SEEK_START);
					child->drv = drv;

					if(long_name_size>0)
					{
						child->long_dir_num = max_long_dir_num;
						child->long_dir_start_cluster = long_dir_start_cluster;
						child->long_dir_start_sector = long_dir_start_sector;
						child->long_dir_start_sector_offset = long_dir_start_sector_offset;
						child->long_dir_start_byte = long_dir_start_byte;
					}else
					{
						child->long_dir_num = 0;
						child->long_dir_start_cluster = FAT_BAD_NO;
						child->long_dir_start_sector = FAT_BAD_NO;
						child->long_dir_start_sector_offset = FAT_BAD_NO;
						child->long_dir_start_byte = FAT_BAD_NO;
					}
					child->short_dir_start_sector = sector;
					child->short_dir_start_sector_offset = dir_index*FAT_DIR_SIZE;


					dir_index++;
					break;
				}//inode_found

				long_name_size = 0;
			}//end long name and short name

		}
		if(bWrite)
		{		
			err = fat_write(drv, sector, 1, fat_buf);
			if(err!=FS_OK)
				return err;
		}
		fat_debug("fat_read_dir: end dir_index=%d\n",dir_index);
		fat_read_seek(parent, 
			dir_index*FAT_DIR_SIZE-sector_offset, 
			FAT_SEEK_CUR);
		sector_offset = 0;
		if(inode_found)
		{
			return FS_OK;
		}
	}

	return FS_ERR_EOF;
}

static int fat_check_name(char **p_name, int *p_name_size, u8 name_fmt)
{
	char *name = *p_name;
	int name_size = *p_name_size;
	int i, char_len=(name_fmt==NAME_FMT_UNICODE)?2:1;

	if(char_len==2&&(name_size%2)!=0)
	{
		return FS_ERR_NAME_SIZE;
	}
	/* 清除头部的空格*/
	for(i=0; i<name_size; i+=char_len)
	{
		if(name_fmt==NAME_FMT_UNICODE&&
			name[i+1]!=0)/* 两个字节的字符不检查*/
		{
			break;
		}
		if(name[i]!=' ')
			break;
	}
	if(i==name_size)
		return FS_ERR_CHAR;
	name += i;
	name_size -= i;

	/* 清除尾部的空格*/
	for(i=name_size-1; i>=0; i-=char_len)
	{
		if(name_fmt==NAME_FMT_UNICODE&&
			name[i+1]!=0)/* 两个字节的字符不检查*/
		{
			break;
		}
		if(name[i]!=' ')
			break;
	}
	if(i<0)
		return FS_ERR_CHAR;
	name_size = i+1;
	for(i=0; i<name_size; i+=char_len)
	{
		if(name_fmt==NAME_FMT_UNICODE&&
			name[i+1]!=0)/* 两个字节的字符不检查*/
		{
			continue;
		}
		if(!SHORT_NAME_CHAR_GOOD(name[i]))
			return FS_ERR_CHAR;
	}
	*p_name = name;
	*p_name_size = name_size;
	return FS_OK;
}

/*
** 根据用户输入的名字创建short name
**
**/
static int fat_create_short_name(char *name, /* 用户输入的名字*/
				int name_size, /* 名字的有效长度*/
				u8 name_fmt,/* 名字的格式*/
				u8 *short_name, /* 用于存放short name, such as aaaa.ext*/
				int *p_main_name_size,/* aaaa的长度*/
				u8 *p_lossy, /* 名字是否被截止*/
				u8 *p_lcase
				)
{
	u8 c;
	int i, ext_index, j, ext_len;
	int scan_char_flag=0;/* 0x1 find lower char, 0x2 find upper char */
	
	*p_lossy = FALSE;
	*p_lcase = CASE_LOWER_BASE|CASE_LOWER_EXT;
	if(name_fmt == NAME_FMT_UNICODE)
	{
		name = fat_UNICODE_to_ascii(name, name_size);
		if(name == NULL)
			return FS_ERR_NAME_SIZE;
		name_size = strlen(name);
	}
	
	/* 去掉尾部的空格*/
	for(;name_size>0;name_size--)
		if(name[name_size-1]!=' ')
			break;
	if(name_size == 0)
	{
		return FS_ERR_NAME_SIZE;
	}
	/* 第一个字符不能为空格*/
	if(name[0] == ' ')
		return FS_ERR_CHAR;
	if(name[0]=='.')
	{
		/* 只允许出现: '.' or '..'*/
		if(name_size>2||(name_size==2&&name[1]!='.'))
			return FS_ERR_CHAR;
		memcpy(short_name, name, name_size);
		short_name[name_size]=0;
		*p_main_name_size = name_size;
		*p_lossy = FALSE;
		return FS_OK;
	}
	ext_index = -1;
	for(i=0;i<name_size; i++)
	{
		c = name[i];
		if(!SHORT_NAME_CHAR_GOOD(c))
			return FS_ERR_CHAR;
		if(name[i] == '.')
			ext_index = i;
		if(name[i] == ' ')
			*p_lossy = TRUE;
	}
	/* 分析扩展名*/
	if(ext_index<0||ext_index==(name_size-1))
	{
		//不存在扩展名
		ext_len = 0;
	}else
	{
		ext_len = name_size-1-ext_index;
		if(ext_len>FAT_EXT_LEN)
		{
			//扩展名过长
			ext_len = FAT_EXT_LEN;
			*p_lossy = TRUE;
		}
	}
	/* 分析主干名字*/
	for(i=0, j=0; (i<name_size)&&(j<FAT_NAME_LEN); i++)
	{
		if(name[i] == '.')
		{
			if(i==ext_index)
			{
				//到达扩展名的位置
				break;
			}
			//忽略该字符
			*p_lossy = TRUE;
			continue;
		}
		if(name[i]<' '||name[i]>'~')
		{
			//非法字符
			//return FS_ERR_CHAR;
		}
		if(name[i]>='A'&&name[i]<='Z')
		{
			*p_lcase &= ~CASE_LOWER_BASE;
			scan_char_flag |= 0x2;//find Upper char
		}else if(name[i]>='a'&&name[i]<='z')
		{
			scan_char_flag |= 0x1;//find lowwer char
		}
		short_name[j++] = fat_char_upper(name[i]);
	}
	*p_main_name_size = j;
	if((ext_index>0&&i<ext_index)||
		(ext_index<0&&i<name_size))
	{
		*p_lossy = TRUE;
	}
	
	/* 复制扩展名*/
	if(ext_index>0)
	{
		short_name[j++] = '.';
		name += ext_index+1;
		for(;ext_len>0;ext_len--)
		{
			if(name[0]>='A'&&name[0]<='Z')
			{
				*p_lcase &= ~CASE_LOWER_EXT;
				scan_char_flag |= 0x2;//find Upper char
			}else if(name[0]>='a'&&name[0]<='z')
			{
				scan_char_flag |= 0x1;//find lowwer char
			}
			short_name[j++] = fat_char_upper(*name++);
		}
	}
	short_name[j] = 0;
	if(scan_char_flag == 0x3)
	{
		*p_lossy = TRUE;
	}
	return FS_OK;
}

/*
**  short name: such as******~N, or *****~MN
**  N,M is 0~9
**/
#define FAT_NUM_MAX 100000
typedef struct FAT_NUM_BITMAP_s
{
	u32 num;
	u32 bit[(FAT_NUM_MAX+31)/32];
}FAT_NUM_BITMAP;
static inline void fat_set_num_bitmap(FAT_NUM_BITMAP *fat_bmp, u32 num)
{
	if(num>=FAT_NUM_MAX)
		return;
	fat_bmp->bit[num/32]|=(1<<(num%32));
}
static inline void fat_init_num_bitmap(FAT_NUM_BITMAP *fat_bmp)
{
	fat_bmp->num = FAT_NUM_MAX;
	memset(fat_bmp->bit, 0, sizeof(fat_bmp->bit));
}

static inline u32 fat_get_num(FAT_NUM_BITMAP *fat_bmp)
{
	u32 num;
	for(num=0; num<fat_bmp->num; num++)
	{
		if((fat_bmp->bit[num/32]&(1<<(num%32)))==0)
			return num;
	}
	return (u32)FAT_BAD_NO;
}

/*
** 对给定的cluster计算出其所在的FAT1 sector
**/
static u32 fat_clusterTofat_sector(DRIVE_INFO *drv, u32 idx)
{
	if(drv->bIsFAT32)
	{
		idx = idx*4;
	}else
	{
		idx = idx*2;
	}
	return drv->dwFAT1StartSector+byte_to_sector(drv, idx);
}

/*
** 对给定的cluster计算出其在FAT1 sector的偏移量
**/
static u32 fat_clusterTofat_offset(DRIVE_INFO *drv, u32 idx)
{
	if(drv->bIsFAT32)
	{
		idx = idx*4;
	}else
	{
		idx = idx*2;
	}
	return byte_in_sector(drv, idx);

}

/*
** 设置fat32表项的值
**/
static inline void fat32_set(u32 *ptbl, u32 value)
{
	value = htofl(value);
	memcpy(ptbl, &value, 4);
}

/*
** 设置fat16表项的值
**
**/
static inline void fat16_set(u16 *ptbl, u16 value)
{
	value = htofs(value);
	memcpy(ptbl, &value, 2);
}


/*
** 为inode 分配一个新的cluster
**
**/
static int fat_new_cluster(DRIVE_INFO *drv, 
			u8 bDir, /* inode是否为目录*/
			u32 FATsector, /* FAT1开始的扇区号*/
			u32 first_cluster/* inode所拥有的cluster号*/
			)
{
	FAT_SECTOR_CACHE *fat_buf;
	u8	*buf;
	int i, err,edge;
	u8 has_free;/* 是否找到空闲cluster */
	u32 start_sector, cluster;/* cluster 编号*/
	u32 sector_edge[2][2];/* 用于search的sector边界[*][0]开始,[*][1]结束*/

	if(FATsector<drv->dwFAT1StartSector)
	{
		fat_error("fat_new_cluster: FATsector[%d] is bad!\n",
			FATsector);
		assert(0);
	}
	memset(sector_edge, 0, sizeof(sector_edge));
	start_sector = drv->FATSectorOff;
	if(start_sector>=drv->dwFAT2StartSector)
	{
		start_sector = drv->dwFAT1StartSector;
		drv->FATSectorOff = start_sector;
	}
	sector_edge[0][0] = start_sector;
	sector_edge[0][1] = drv->dwFAT2StartSector;
	if(start_sector>drv->dwFAT1StartSector)
	{
		sector_edge[1][0] = drv->dwFAT1StartSector;
		sector_edge[1][1] = start_sector;
	}
	has_free = FALSE;
	for(edge=0; edge<2; edge++)
	{
		if(sector_edge[edge][0]==0)
			break;//edge is bad!!!!!!
		start_sector = sector_edge[edge][0];
		cluster = (start_sector-drv->dwFAT1StartSector)*
			(drv->bIsFAT32?(drv->wSectorSize/4):(drv->wSectorSize/2));
		while(1)
		{
			if(start_sector>=sector_edge[edge][1])
			{
				break;
			}
			err = fat_read(drv, start_sector, 1, &fat_buf);
			if(err)
			{
				return err;
			}
			buf = FAT_BUF_P(fat_buf);
			if(drv->bIsFAT32)
			{
				u32 value;
				for(i=0;i<drv->wSectorSize/4;i++)
				{
					memcpy(&value, buf+i*4, 4);
					value = ftohl(value);
					if((value&FAT32_CLUSTER_MASK)==0)
					{
						has_free = TRUE;
						fat32_set((u32*)(buf+i*4), FAT32_CLUSTER_EOF);
						break;
					}
					cluster++;
				}
			}else
			{
				u16 value;
				for(i=0;i<drv->wSectorSize/2;i++)
				{
					memcpy(&value, buf+i*2, 2);
					value = ftohs(value);
					if(value==0)
					{
						has_free = TRUE;
						fat16_set((u16*)(buf+i*2), FAT16_CLUSTER_EOF);
						break;
					}
					cluster++;
				}
			}

			if(fat_get_first_sector(drv, cluster)>=drv->dwSectorTotal)
			{
				has_free = FALSE;
				break;
			}
		
			if(has_free)
			{
				goto out;
			}
			start_sector++;
		}
	}

out:	
	if(has_free == FALSE||
		/*
		** FAT1表项所对应的cluster可能会大于真实的
		**/
		fat_get_first_sector(drv, cluster)>=drv->dwSectorTotal)
	{
		fat_debug("fat_new_cluster: disk full!\n");
		return FS_ERR_DISK_FULL;
	}
	fat_debug("fat_new_cluster: new cluster=%d\n", cluster);

	//更新cluster对应的fat表项
	err = fat_write(drv, start_sector, 1, fat_buf);
	if(err)
	{
		return err;
	}
	drv->FATSectorOff = start_sector;

	//更新first_cluster对应的fat表项
	if(GOOD_CLUSTER(drv, first_cluster))
	{
		start_sector = fat_clusterTofat_sector(drv, first_cluster);
		err = fat_read(drv, start_sector, 1, &fat_buf);
		if(err)
			return err;
		buf = FAT_BUF_P(fat_buf);
		if(drv->bIsFAT32)
		{
			u32 *pvalue = (u32*)(buf+fat_clusterTofat_offset(drv, first_cluster));
			fat32_set(pvalue, cluster);
		}else
		{
			u16 *pvalue = (u16*)(buf+fat_clusterTofat_offset(drv, first_cluster));
			fat16_set(pvalue, (u16)cluster);
		}
		err = fat_write(drv, start_sector, 1, fat_buf);
		if(err)
			return err;
	}

	//初始化目录表项
	if(bDir)
	{
		start_sector = fat_get_first_sector(drv, cluster);
		for(i=0; i<drv->bSectorsPerCluster; i++)
		{
			fat_debug("new_cluster: dir ent init in sector[%d]\n",
				start_sector+i);
			err = fat_read(drv, start_sector+i, 1, &fat_buf);
			if(fat_buf == NULL)
				break;
			buf = FAT_BUF_P(fat_buf);
			memset(buf, 0, drv->wSectorSize);
			err = fat_write(drv, start_sector+i, 1, fat_buf);
			if(err)
				return err;
		}
	}
	return (int)cluster;
}

/*
** 设置fattable
**/
static int _fat_set_fat(DRIVE_INFO *drv, u32 cluster, u32 value)
{
	u32 sector, sector_offset;
	FAT_SECTOR_CACHE *fat_buf;
	u8 *buf;
	int err;
	
	sector = fat_clusterTofat_sector(drv, cluster);
	sector_offset = fat_clusterTofat_offset(drv, cluster);
	err = fat_read(drv, sector, 1, &fat_buf);
	if(err)
		return err;
	buf = FAT_BUF_P(fat_buf);
	if(drv->bIsFAT32)
	{
		u32 *pvalue = (u32*)(buf+sector_offset);
		fat32_set(pvalue, value);
	}else
	{
		u16 *pvalue = (u16*)(buf+sector_offset);
		fat16_set(pvalue, (u16)(value&0xffff));
	}
	err = fat_write(drv, sector, 1, fat_buf);
	return err;
}

/*
** 从prev_cluster开始的第num开始释放到尾部
** 包含prev_cluster
** 
**/
static int fat_free_cluster(DRIVE_INFO *drv, u32 prev_cluster, int num)
{
	u32 cluster;
	int err;

	assert(num>=0);
	if(!GOOD_CLUSTER(drv, prev_cluster))
	{
		return FS_OK;
	}
	cluster = prev_cluster;
	prev_cluster = FAT_BAD_NO;
	for(;num>0;num--)
	{
		prev_cluster = cluster;
		cluster = fat_get_next_cluster(drv, cluster);
		if(!GOOD_CLUSTER(drv, cluster))
			return FS_OK;
	}
	//更新prev_cluster对应的fat表项
	if(GOOD_CLUSTER(drv, prev_cluster))
	{
		err = _fat_set_fat(drv, prev_cluster, FAT32_CLUSTER_EOF);
		if(err== FS_ERR_CACHE_FULL)
			assert(0);
		if(err)
			return err;
	}
	
	//更新从cluster开始对应的fat表项
	prev_cluster = cluster;
	while(GOOD_CLUSTER(drv, prev_cluster))
	{		
		cluster = fat_get_next_cluster(drv, prev_cluster);
		err = _fat_set_fat(drv, prev_cluster, 0);
		if(err)
		{
			if(err != FS_ERR_CACHE_FULL)
				return err;
			err = fat_write_submit(drv, 0);
			err = _fat_set_fat(drv, prev_cluster, 0);
			if(err)
				return err;
		}
		//fat_write_submit(drv, 0);
		prev_cluster = cluster;
	}
	return FS_OK;
}

/*
** 分配多个cluster
**/
static int fat_new_clusters(inode_t *inode, u32 first_cluster, int num)
{
	DRIVE_INFO *drv = inode->drv;
	int i, err;
	int new_cluster;
	u32 start_sector; 
	FAT_SECTOR_CACHE *fat_buf;
	u8 *buf;

	err = fat_write_submit(drv, 0);
	if(err)
		return err;

	//找到链表的尾巴
	while(1)
	{
		start_sector = fat_clusterTofat_sector(drv, first_cluster);
		if(start_sector >= drv->dwSectorTotal)
			return -1;
		err = fat_read(drv,
			start_sector,
			1,
			&fat_buf);
		if(err)
			return err;
		buf = FAT_BUF_P(fat_buf);
		buf += fat_clusterTofat_offset(drv, first_cluster);
		if(drv->bIsFAT32)
		{
			u32 value;
			memcpy(&value, buf, 4);
			if(value != htofl(FAT32_CLUSTER_EOF))
			{
				first_cluster = ftohl(value);
			}
		}else
		{
			u16 value;
			memcpy(&value, buf, 2);
			if(value != htofs(FAT16_CLUSTER_EOF))
			{
				first_cluster = ftohs(value);
			}
		}
	}

	/*
	** 开始分配cluster
	**/
	start_sector = drv->dwFAT1StartSector;
	for(i=0; i<num; i++)
	{
		new_cluster = fat_new_cluster(inode->drv, 
			INODE_DIR(inode), start_sector, first_cluster);
		if(new_cluster<0)
		{
			fat_write_submit(drv, -1);
			break;
		}else
		{
			err = fat_write_submit(drv, FS_OK);
			if(err)
				break;
		}
		//cluster 在fat1中的位置
		start_sector = fat_clusterTofat_sector(drv, (u32)new_cluster);
	}
	return i;
}

static void fat_set_time(FAT32_FILEDATETIME *dtime)
{
	#define BCD2I(v) ((((v)>>4)&0xf)*10+((v)&0xf))
	u8 t[7];
	GetTime(t);
	dtime->Year = 2000-1980+BCD2I(t[0]);
	dtime->Month = BCD2I(t[1]);
	dtime->Day = BCD2I(t[2]);
	dtime->Hour = BCD2I(t[3]);
	dtime->Minute = BCD2I(t[4]);
	dtime->Seconds = BCD2I(t[5]);
}

/*
** 对给定的short_name,
** 判断inode是否使用对应的num
** for example:1. inode name is abcdef~2.txt(num is 2)
**                     short name is abcdefgh.txt
**                   2. inode name is abcdef~2.txt(num is NULL)
**                      short name is abcdeghi.txt
**/
static void fat_scan_num(inode_t *inode, 
			u8 *short_name,
			int short_name_size,
			int main_name_size,/* short_name 主干长度*/
			FAT_NUM_BITMAP *fat_bmp)
{
	int i;
	u32 num, base10/* 10的倍数*/;
	int ext_index1,/* inodex 扩展名启始位置(包含.) ,
				-1表示没有*/
		ext_index2;/* short_name  扩展名启始位置(包含.) ,
				-1表示没有*/
	char c;
	
	if(inode->short_name_size != short_name_size)
	{	
		fat_debug("fat_scan_num: inode size=%d not equal to %d\n",
			inode->short_name_size, short_name_size);
		return;
	}
	for(i=inode->short_name_size-1; i>=0; i--)
		if(inode->short_name[i]=='.')
			break;
	if(i==0)
	{
		fat_debug("fat_scan_num: inode short name='%s' is bad\n",
			inode->short_name);
		return;//bad name
	}
	ext_index1 = i;
	
	if(short_name_size-main_name_size>1)
	{
		ext_index2 = main_name_size;
	}else
	{
		ext_index2 = -1;/* 没有扩展名*/
	}
	if(ext_index2>0)
		assert(short_name[ext_index2]=='.');
	if(ext_index1 != ext_index2)
	{
		fat_debug("fat_scan_num: base or ext is not same!\n");
		return;//扩展名或base name 不一样
	}
	
	//比较扩展名，如果存在的话
	if(ext_index1>0&&
	fat_short_name_cmp(inode->short_name+ext_index1,
		short_name_size-ext_index1,
		short_name+ext_index2, short_name_size-ext_index2)!=TRUE
		)
	{
		//ext type is not same
		fat_debug("fat_scan_num: ext is not same!\n");
		return;
	}
	
	//扫描主干名字的号码
	num = 0;
	base10 = 1;
	if(ext_index1>0)
	{
		 i = ext_index1-1;
	}else
	{
		 i = inode->short_name_size-1;
	}
	for(; i>=2; i--)
	{
		c = inode->short_name[i];
		if(c>='0'&&c<='9')
		{
			num = (c-'0')*base10+num;
			base10*=10;
			continue;
		}
		else
		{
			if(c=='~')
			{
				/* '~' 是第一个字符?*/
				c = inode->short_name[i+1];
				if(c>='0'&&c<='9')
					break;
			}
			return;
		}
	}
	if(inode->short_name[i] != '~')
	{
		fat_debug("fat_scan_num: inode short_name[%d]=%c, is not '~'\n",
			i,
			inode->short_name[i]
			);
		return;
	}
	if(fat_short_name_cmp(inode->short_name, i, short_name, i)!=TRUE)
	{
		//base name is not same
		fat_debug("fat_scan_num: '%s', '%s', i=%d\n", inode->short_name, short_name, i);
		return;
	}
	fat_debug("fat_scan_num: num = %d\n", num);
	fat_set_num_bitmap(fat_bmp, num);
}

/*
** 创建inode节点
** 1.首先扫描parent包含的目录或文件,
**    如果存在同名则创建失败
** 2.如果parent没有空间且为fat16的根目录则创建失败,
**   其他则可以为parent分配新的cluster
** 3.在parent中创建相应的dir ent
**/
static int fat_open_inode(inode_t *parent,/* IN */
			char *name,/* IN */
			int name_size,/* IN */
			u8	name_fmt,/* IN */
			u8  bCreate,/* IN */
			inode_t *child/* OUT */)
{
	DRIVE_INFO *drv = parent->drv;
	u8 long_name[LONG_NAME_MAX+2];
	u8 short_name[SHORT_NAME_MAX+1];
	int long_name_size, short_name_size;
	inode_t t, *inode=&t;
	int main_short_name_size;/* the size of base short name*/
	u8  short_lossy;/* 是否从long name变成short name丢失信息*/
	u8  lcase;/* base or ext case lower*/
	int err;
	FAT_NUM_BITMAP bmp;
	DIR_ENT_FREE  dir_free;/* 空闲的目录项信息*/
	u32 fat_sector = drv->dwFAT1StartSector;/* FAT table sector */

	fat_debug("fat_open_inode: parent ");
	fat_name_print(parent->long_name, parent->long_name_size);
	fat_debug("\n");

	fat_read_seek(parent, 0, FAT_SEEK_START);

	if(fat_check_name(&name, &name_size, name_fmt)!=FS_OK)
	{
		fat_debug("fat_open_inode: check name fail!\n");
		return FS_ERR_CHAR;
	}
	
	memset(long_name, 0, sizeof(long_name));
	memset(short_name, 0, sizeof(short_name));
	if(name_fmt==NAME_FMT_UNICODE)
	{
		if((name_size%2)!=0)
		{
			return FS_ERR_NAME_SIZE;
		}
		if(name_size>195*2||name_size>LONG_NAME_MAX)
		{
			//不能超过一个setor
			return FS_ERR_NAME_SIZE;
		}
		memcpy(long_name, name, name_size);
		long_name_size = name_size;
	}else
	{
		if(name_size>195||name_size>(LONG_NAME_MAX/2))
		{
			//不能超过一个sector
			return FS_ERR_NAME_SIZE;
		}
		memcpy(long_name, 
			fat_ascii_to_UNICODE(name, name_size), 
			name_size*2);
		long_name_size = name_size*2;
	}
	err = fat_create_short_name(name, name_size, name_fmt, 
		short_name, &main_short_name_size, &short_lossy, &lcase);
	if(err != FS_OK)
	{
		fat_debug("fat_open_inode: create short name fail err=%d\n",
			err);
		return err;
	}
	short_name_size = strlen(short_name);

	fat_debug("fat_open_inode: short_name='%s', base len=%d,%s\n",
		short_name, main_short_name_size,
		short_lossy?"Lossy":"Not Lossy"
		);

	fat_set_time(&child->wrtime);
	fat_set_time(&child->crtime);
	memset(child->long_name, 0, LONG_NAME_MAX+2);
	memset(child->short_name, 0, SHORT_NAME_MAX+1);
	memcpy(child->long_name, long_name, long_name_size);
	memcpy(child->short_name, short_name, short_name_size);
	child->long_name_size = long_name_size;
	child->short_name_size = short_name_size;
	child->first_sector = fat_get_first_sector(drv, child->first_cluster);
	child->parent_sector = parent->first_sector;
	child->drv = drv;

	fat_init_num_bitmap(&bmp);
	if(name_fmt==NAME_FMT_UNICODE||short_lossy)
	{
		dir_free.long_name_N = (long_name_size+25)/26;
	}else
	{
		dir_free.long_name_N = 0;
	}
	dir_free.sector = FAT_BAD_NO;
	dir_free.sector_offset = FAT_BAD_NO;
	//扫描整个目录表项
	while(1)
	{
		if(fat_read_dir(parent, NULL, 0, 0, inode, &dir_free)!=FS_OK)
		{
			break;
		}
		if(inode->long_name_size>0)			
		{
			if(fat_long_name_cmp(name, name_size, name_fmt, 
				inode->long_name,  inode->long_name_size)==TRUE)
			{
				//the name exist
				*child = *inode;
				return FS_ERR_NAME_EXIST;
			}
			if(!short_lossy&&
				fat_short_name_cmp(short_name, strlen(short_name),
					inode->short_name, inode->short_name_size)==TRUE
				)
			{
				/*
				** 这里要处理情况如下:
				** 假设已存在inode, short_name='aaaaaa~0',
				**        long_name='aaaaaaaa1';
				** 新创建的节点名字为'aaaaaa~0'(short_lossy=FALSE),
				** 这时候我们应该强制新的inode short_lossy=TRUE
				** 并重新开始扫描.
				**/
				short_lossy = TRUE;
				dir_free.long_name_N = (long_name_size+25)/26;
				dir_free.sector = FAT_BAD_NO;
				dir_free.sector = FAT_BAD_NO;
				fat_read_seek(parent, 0, FAT_SEEK_START);
			}
		}else if(inode->short_name_size>0)
		{
			if((name_fmt == NAME_FMT_ASCII&&
				fat_short_name_cmp(name, name_size,
					inode->short_name, inode->short_name_size)==TRUE)||
				(name_fmt == NAME_FMT_UNICODE&&
				fat_long_name_cmp(inode->short_name, inode->short_name_size,
					NAME_FMT_ASCII, name, name_size)==TRUE))
			{
				//the name exist
				*child = *inode;
				return FS_ERR_NAME_EXIST;
			}
		}else
		{
			//illegal inode
			assert(0);
		}
		if(short_lossy)
		{
			//扫描已使用的尾号
			fat_debug("fat_scan_num........\n");
			fat_scan_num(inode, short_name, short_name_size, 
				main_short_name_size, &bmp);
		}
	}

	if(bCreate == FALSE)
	{
		return FS_ERR_NOENT;
	}

	if(short_lossy)
	{
		/*
		** 产生带有尾号的short name
		**/
		u32 num = fat_get_num(&bmp);
		u8 num_str[10];
		int num_str_size;
		
		if(num == FAT_BAD_NO)
			return FS_ERR_NAME_SPACE;
		fat_debug("Short name Num trail = %d\n", num);
		assert(num<bmp.num);
		num_str_size = sprintf(num_str, "~%d", num);
		memcpy(short_name+main_short_name_size-num_str_size, num_str,
			num_str_size);
		fat_debug("Short Name: %s\n", short_name);

		lcase = 0;
	}

	fat_debug("***Free Dir Ent Slot: sector[%d], offset[%d]\n",
		dir_free.sector, dir_free.sector_offset);
	if(dir_free.sector == FAT_BAD_NO)
	{
		int cluster;
		//extend parent dir space
		if(parent->first_cluster == 1)
		{
			//FAT16 root dir
			return FS_ERR_ROOT_FULL;
		}
		cluster = fat_new_cluster(parent->drv, 
				TRUE,
				drv->dwFAT1StartSector,
				parent->offset.cluster);
		if(cluster < 0)
			return cluster;/* cluster is error no */
		dir_free.sector = fat_get_first_sector(drv, (u32)cluster);
		dir_free.sector_offset = 0;
	}

	child->long_dir_num = dir_free.long_name_N;
	if(dir_free.long_name_N>0)
	{
		child->long_dir_start_cluster = 
			fat_sector_to_cluster(drv, dir_free.sector);
		child->long_dir_start_sector = dir_free.sector;
		child->long_dir_start_sector_offset = dir_free.sector_offset;
		child->long_dir_start_byte = dir_free.bytes;
	}
	child->short_dir_start_sector = dir_free.sector;
	child->short_dir_start_sector_offset = dir_free.sector_offset+
		(dir_free.long_name_N)*FAT_DIR_SIZE;

	/*
	** 分配cluster空间
	**/
	if(child->first_cluster==FAT_BAD_NO&&(child->attr==ATTR_DIR))
	{
		err = fat_new_cluster(parent->drv,
			TRUE,
			parent->drv->dwFAT1StartSector, 
			FAT_BAD_NO);
		if(err<0)
		{
			fat_debug("fat_open_inode_: fail to new cluster err=%d\n",
				err
				);
			return err;
		}
		child->first_cluster = err;
		child->first_sector = fat_get_first_sector(drv, err);
		fat_debug("fat_open_inode_: first_cluster=%d\n",
			child->first_cluster);
	}
	//把inode信息写入dir_ent中
	{
		FAT_SECTOR_CACHE *fat_buf;
		u8 *buf, *name = long_name;
		u32 i;
		int size;
		u8 fat_chksum;
		FAT32_DIRECTORY_ENTRY *dir_ent;
		FAT32_DIRECTORY_ENTRY_LONG *dir_ent_long;
		
		err = fat_read(drv, dir_free.sector, 1, &fat_buf);
		if(err)
		{
			return err;
		}
		buf = FAT_BUF_P(fat_buf);
		dir_ent = (void*)(buf+dir_free.sector_offset);
		dir_ent += dir_free.long_name_N;
		memset(dir_ent, 0, sizeof(*dir_ent));
		memset(dir_ent->Name, ' ', FAT_NAME_LEN);
		memset(dir_ent->Extension, ' ', FAT_EXT_LEN);
		memcpy(dir_ent->Name, short_name, 
			main_short_name_size);
		if(short_name_size-main_short_name_size>1)
			memcpy(dir_ent->Extension, 
				short_name+main_short_name_size+1,
				short_name_size-main_short_name_size-1
				);
		dir_ent->Attribute = child->attr;
		dir_ent->HighCluster = (u16)((child->first_cluster>>16)&0xffff);
		dir_ent->HighCluster = htofs(dir_ent->HighCluster);
		dir_ent->LowCluster = (u16)((child->first_cluster)&0xffff);
		dir_ent->LowCluster = htofs(dir_ent->LowCluster);
		dir_ent->Date = child->wrtime;
		dir_ent->crDate = child->crtime;
		dir_ent->FileSize = child->size;
		dir_ent->lcase = lcase;

		fat_chksum = fat_shortname_chksum(dir_ent->Name);
		dir_ent_long = (void*)(dir_ent-1);
		for(i=1; i<=dir_free.long_name_N; i++, dir_ent_long--)
		{
			memset(dir_ent_long, 0, sizeof(*dir_ent_long));
			dir_ent_long->Order = (u8)i;
			dir_ent_long->Attribute = ATTR_LONG_NAME;
			dir_ent_long->Chksum = fat_chksum;
			if(i!=dir_free.long_name_N)
			{
				memcpy(dir_ent_long->Name1, name, 10);
				name += 10;
				memcpy(dir_ent_long->Name2, name, 12);
				name += 12;
				memcpy(dir_ent_long->Name3, name, 4);
				name += 4;
				long_name_size -= 26;
			}else
			{
				dir_ent_long->Order |= 0x40;//Last Long Dir
				memset(dir_ent_long->Name1, 0xff, 10);
				memset(dir_ent_long->Name2, 0xff, 12);
				memset(dir_ent_long->Name3, 0xff, 4);
				size = 10;
				if(size>long_name_size)
				{
					size = long_name_size;
				}
				memcpy(dir_ent_long->Name1, name, size);
				name += size;
				long_name_size -= size;
				if(long_name_size == 0)
				{
					if(size==10)
						dir_ent_long->Name2[0] = 0x0;
					else
						dir_ent_long->Name1[size/2] = 0x0;
					break;//finished!!!
				}
				size = 12;
				if(size>long_name_size)
					size = long_name_size;
				memcpy(dir_ent_long->Name2, name, size);
				name += size;
				long_name_size -= size;
				if(long_name_size == 0)
				{
					if(size==12)
						dir_ent_long->Name3[0] = 0x0;
					else
						dir_ent_long->Name2[size/2] = 0x0;
					break;//finished!!!
				}

				size = 4;
				if(size>long_name_size)
					size = long_name_size;
				memcpy(dir_ent_long->Name3, name, size);
				name += size;
				long_name_size -= size;
				if(long_name_size == 0)
				{
					if(size!=4)
						dir_ent_long->Name3[size/2] = 0x0;
					break;//finished!!!
				}
			}
		}
		//force_urb_printf(FAT_BUF_P(fat_buf), 512, "inode dir ent");
		err = fat_write(drv, dir_free.sector, 1, fat_buf);
		if(err != FS_OK)
		{
			fat_debug("fat_open_inode: fat_write sector[%d] error=%d\n", 
				dir_free.sector, err);
			return err;
		}
	}
	return FS_OK;
}

static int fat_open_dir(inode_t *parent,/* IN */
			char *name,/* IN */
			int name_size,/* IN */
			u8  name_fmt,/* IN */
			inode_t *child,
			u32  attr
			)
{
	int err, new_cluster = FAT_BAD_NO;
	inode_t dot_dir, dotdot_dir;
	u8 bCreate = (u8)(attr&FS_ATTR_C);

	fat_debug("#####################################################\n");
	fat_debug("### fat_open_dir: start %s dir ", bCreate?"create":"open");
	fat_name_print(name, name_size);
	fat_debug("  ###\n#####################################################\n");

	if(bCreate==TRUE)
	{
		/*
		** 为了减少写次数,
		** 分配cluster在fat_open_inode内部进行
		**/
		new_cluster = FAT_BAD_NO;
		child->first_cluster = FAT_BAD_NO;
		#if 0
		err = fat_new_cluster(parent->drv,
			TRUE,
			parent->drv->dwFAT1StartSector, 
			FAT_BAD_NO);
		if(err<0)
		{
			fat_debug("fat_open_dir: fail to new cluster err=%d\n",
				err
				);
			goto out;
		}
		new_cluster = err;
		child->first_cluster = err;
		fat_debug("fat_open_dir: first_cluster=%d\n",
			child->first_cluster);
		#endif
		child->attr = ATTR_DIR;
	}
	err = fat_open_inode(parent, name, name_size, name_fmt, bCreate,
		child);
	if(err == 0)
	{
		if(!(attr&FS_ATTR_C))
		{
			goto out;
		}
	}else
	{
		fat_free_cluster(parent->drv, new_cluster, 0);
		if(err == FS_ERR_NAME_EXIST)
		{
			if(INODE_DIR(child))
			{
				if((attr&(FS_ATTR_C|FS_ATTR_E))!=(FS_ATTR_C|FS_ATTR_E))
				{
					err = FS_OK;
				}
			}else
			{
				err = FS_ERR_NOTDIR;
			}
		}
		goto out;
	}
	if(err!=FS_OK)
	{
		fat_debug("fat_open_dir: %s dir fail, err=%d\n",
			bCreate?"create":"open",
			err);
		goto out;
	}
	memset(&dot_dir, 0, sizeof(dot_dir));
	dot_dir.attr = ATTR_DIR;	
	dot_dir.first_cluster = child->first_cluster;
	err = fat_open_inode(child, ".", 1, NAME_FMT_ASCII, TRUE, &dot_dir);
	if(err != FS_OK)
	{
		fat_debug("fat_open_dir: create dir '.' fail, err=%d\n",
			err);
		goto out;
	}
	memset(&dotdot_dir, 0, sizeof(dot_dir));
	dotdot_dir.attr = ATTR_DIR;	
	if(parent->parent_sector == 0)
	{
		//this is root dir
		dotdot_dir.first_cluster = 0;
	}else
	{
		dotdot_dir.first_cluster = parent->first_cluster;
	}
	err = fat_open_inode(child, "..", 2, NAME_FMT_ASCII, TRUE, &dotdot_dir);
	if(err != FS_OK)
	{
		fat_debug("fat_open_dir: create dir '..' fail, err=%d\n",
			err);
		goto out;
	}
	//OK!!!!!!!!!
out:		
	fat_write_submit(parent->drv, err);
	fat_debug("#####################################################\n");
	fat_debug("### fat_open_dir: end %s dir ", bCreate?"create":"open");
	fat_name_print(name, name_size);
	fat_debug("  ###\n#####################################################\n");
	return err;
}

/*
** 更新inode对应的dir ent信息
** 这里主要是更新file size或first_cluster
**/
static int fat_update_inode(inode_t *inode, u8 bDel)
{
	int err, i, dir_num;
	DRIVE_INFO *drv = inode->drv;
	FAT_SECTOR_CACHE *fat_buf;
	u8 *buf;
	u32 sector, sector_offset;
	FAT32_DIRECTORY_ENTRY *dir_ent;
	FAT32_DIRECTORY_ENTRY_LONG *long_dir_ent;

	err = FS_OK;
	if(bDel)
	{
		/*
		** 清除dir entry
		**/
		_offset_t offset;
		inode_t parent;

		memset(&parent, 0, sizeof(inode_t));
		parent.drv = inode->drv;
		parent.first_sector = inode->parent_sector;
		parent.first_cluster = fat_sector_to_cluster(inode->drv, 
					inode->parent_sector);
		offset.cluster = inode->long_dir_start_cluster;
		offset.bytes = inode->long_dir_start_byte;
		offset.sector = inode->long_dir_start_sector;
		offset.eof = FALSE;
		dir_num = inode->long_dir_num;
		while(dir_num>0)
		{
			sector = offset.sector;
			err = fat_read(drv, sector, 1, &fat_buf);
			if(err)
				goto out;
			buf = FAT_BUF_P(fat_buf);
			sector_offset = byte_in_sector(drv, offset.bytes);
			long_dir_ent = (void*)(buf+sector_offset);
			i = (drv->wSectorSize-sector_offset)/FAT_DIR_SIZE;
			if(i>dir_num)
			{
				i = dir_num;
			}
			dir_num -= i;
			fat_seek(&parent, &offset, i*FAT_DIR_SIZE, FAT_SEEK_CUR);
			for(;i>0;i--,long_dir_ent++)
			{
				*((u8*)long_dir_ent) = DIR_FREE_FLAG;
			}
			err = fat_write(drv, sector, 1, fat_buf);
			if(err)
				return err;
		}
	}

	sector = inode->short_dir_start_sector;
	err = fat_read(inode->drv, sector, 1, &fat_buf);
	if(err)
	{
		goto out;
	}
	buf = FAT_BUF_P(fat_buf);
	dir_ent = (void*)(buf+inode->short_dir_start_sector_offset);
	if(bDel)
	{
		*((u8*)dir_ent) = DIR_FREE_FLAG;
	}else
	{
		fat_set_time(&inode->wrtime);
		dir_ent->Date = inode->wrtime;
		dir_ent->FileSize = inode->size;
		dir_ent->LowCluster = (u16)(inode->first_cluster&0xffff);
		dir_ent->LowCluster = htofs(dir_ent->LowCluster);
		dir_ent->HighCluster = (u16)((inode->first_cluster>>16)&0xffff);
		dir_ent->HighCluster = htofs(dir_ent->HighCluster);
	}
	fat_write(inode->drv, sector, 1, fat_buf);
out:
	return err;
}

static int fat_open_file(inode_t *parent,/* IN */
			char *name,/* IN */
			int name_size,/* IN */
			u8  name_fmt,/* IN */
			inode_t *child,/* OUT */
			u32  attr
			)
{
	int err;
	u8 bCreate = (u8)(attr&FS_ATTR_C);

	fat_debug("#####################################################\n");
	fat_debug("### fat_open_file: start %s file ", bCreate?"create":"open");
	fat_name_print(name, name_size);
	fat_debug("  ###\n#####################################################\n");

	err = fat_open_inode(parent, name,
		name_size,
		name_fmt,
		bCreate,
		child);
	if(err==FS_ERR_NAME_EXIST)
	{
		if(!INODE_DIR(child))
		{
			if((attr&(FS_ATTR_C|FS_ATTR_E))!=(FS_ATTR_C|FS_ATTR_E))
			{
				err = FS_OK;
			}
		}else
		{
			err = FS_ERR_ISDIR;
		}
	}
	if(err!=FS_OK)
	{
		fat_debug("fat_open_file: %s file fail, err=%d\n",
			bCreate?"create":"open",
			err);
	}
	fat_write_submit(parent->drv, err);
	fat_debug("#####################################################\n");
	fat_debug("### fat_open_file: end create file ");
	fat_name_print(name, name_size);
	fat_debug("  ###\n#####################################################\n");
	return err;
}

static int fat_write_file(inode_t *inode, char *in_buf, int len)
{
	int err;
	DRIVE_INFO *drv = inode->drv;
	u32 sector_offset, sector, cluster_offset,bytes, request_len;
	u32 first_cluster_free_bytes;
	u32 first_cluster, file_size;
	u32 cluster, prev_cluster;
	u32 fat_sector;/* fat table sector NO */
	_offset_t old_offset;
	FAT_SECTOR_CACHE *fat_buf;
	u8 *buf;
	int write_len;

	file_size = inode->size;
	first_cluster = inode->first_cluster;
	old_offset = inode->offset;
	assert(!INODE_DIR(inode));
	if(inode->first_cluster==0)
	{
		err = fat_new_cluster(drv,FALSE,inode->drv->dwFAT1StartSector,FAT_BAD_NO);
		if(err<0)
		{
			fat_debug("fat_write_file: fail to new cluster err=%d\n",err);
			goto out;
		}
		inode->first_cluster = (u32)err;
		inode->first_sector = fat_get_first_sector(drv, inode->first_cluster);
		fat_write_seek(inode, 0, FAT_SEEK_START);
		inode->offset.eof = FALSE;
	}
	if(!GOOD_CLUSTER(drv, inode->offset.cluster))
	{
		fat_debug("fat_write_file: cluster %d is bad!\n", inode->offset.cluster);
		return FS_ERR_CLUSTER;//file 被破坏了
	}

	/*
	** 计算对应的簇空闲空间(从指定的偏移开始)
	**/
	if(inode->offset.eof&&inode->offset.sector == FAT_BAD_NO)
	{
		bytes = 0;
	}else
	{
		sector_offset = byte_in_sector(drv, inode->offset.bytes);
		sector = inode->offset.sector;
		cluster_offset = sector_in_cluster(drv, sector);
		bytes = (drv->bSectorsPerCluster-cluster_offset)<<drv->bSectorSizePower;
		bytes -= sector_offset;
	}
	first_cluster_free_bytes = bytes;
	
	/*
	** 分配cluster空间，如果需要的话
	**/
	request_len = len;
	prev_cluster = inode->offset.cluster;
	fat_sector = drv->dwFAT1StartSector;
	write_len = bytes;
	while(bytes<request_len)
	{
		request_len -= bytes;
		cluster = fat_get_next_cluster(drv, prev_cluster);
		if(cluster == FAT_BAD_NO)
		{
			cluster = fat_new_cluster(drv, FALSE, fat_sector, 
				prev_cluster);
			fat_write_submit(drv, (int)cluster<0?-1:0);
			if( (int)cluster < 0)
			{
				//error
				if((int)cluster != FS_ERR_DISK_FULL)
					return (int)cluster;
				if(write_len==0)
					return FS_ERR_DISK_FULL;
				break;
			}
			fat_sector = fat_clusterTofat_sector(drv, cluster);
			fat_debug("newcluster %d\n", fat_get_next_cluster(drv, prev_cluster));
			if(inode->offset.eof&&inode->offset.sector == FAT_BAD_NO)
			{
				inode->offset.cluster = cluster;
				inode->offset.sector = fat_get_first_sector(drv, cluster);
				bytes = drv->dwClusterSize;
				first_cluster_free_bytes = bytes;	
				sector_offset = 0;
			}
		}
		prev_cluster = cluster;
		bytes = drv->dwClusterSize;
		write_len += bytes;
	}

	/*
	** 写入cluster中
	**/
	request_len = len;
	err = 0;
	inode->offset.eof = FALSE;
	write_len = 0;
	while(request_len>0)
	{
		if(inode->offset.sector == FAT_BAD_NO)
		{
//x_debug("ERROR,req_len:%d,wlen:%d\n",request_len,write_len);
			break;
		}

		bytes = drv->wSectorSize-sector_offset;
		if(bytes>request_len)
			bytes = request_len;
		if(sector_offset>0||bytes<drv->wSectorSize)
		{
			err = fat_read(drv, inode->offset.sector, 1, &fat_buf);
			if(err != FS_OK)
				break;
		}else
		{
			fat_buf = fat_buf_get(drv);
			memset(FAT_BUF_P(fat_buf), 0, drv->wSectorSize);
		}
		buf = FAT_BUF_P(fat_buf);
		memcpy(buf+sector_offset, in_buf, bytes);
		//fat_buf_debug(buf+sector_offset, bytes, "write data");
		err = fat_write(drv, inode->offset.sector, 1, fat_buf);
		if(err<0)
		{
			if(err == FS_ERR_CACHE_FULL)
			{
				fat_write_submit(drv, 0);
				err = fat_write(drv, inode->offset.sector, 1, fat_buf);
				if(err)
					break;
			}else
				break;
		}
		request_len -= bytes;
		in_buf += bytes;
		write_len += bytes;
		fat_seek(inode, &inode->offset, bytes, FAT_SEEK_CUR);
		bytes = drv->wSectorSize;
		sector_offset = 0;
	}
	if(err==0)
	{
		if(inode->offset.bytes > inode->size)
		{
			inode->size = inode->offset.bytes;
		}
		//if(inode->offset.bytes>inode->size)
		{
			err = fat_update_inode(inode, INODE_NODEL);
			if(err < 0)
			{
				//undo
				inode->size = file_size;
				inode->first_cluster = first_cluster;
				inode->offset = old_offset;
			}
		}
	}
	fat_write_submit(drv, err);
out:
	if(err<0)
		return err;
	return write_len;
}

static int fat_del_inode(inode_t *inode)
{
	int err;
	inode_t child;
	DRIVE_INFO *drv = inode->drv;
	u32 cluster;

	if(INODE_DIR(inode))
	{
		if(strcmp(inode->short_name, "/")==0||
			strcmp(inode->short_name, ".")==0||
			strcmp(inode->short_name, "..")==0
		)
		{
			return FS_ERR_NOENT;
		}
		while(1)
		{
			memset(&child, 0, sizeof(inode_t));
			err = fat_read_dir(inode, NULL, 0, 0,
				&child, NULL);
			if(err)
				break;
			if(strcmp(child.short_name, ".")==0||
				strcmp(child.short_name, "..")==0)
				continue;
			else
				return FS_ERR_NOTNULL;			
		}
	}	
	/*
	** 释放占用的cluster
	**/
	cluster = inode->first_cluster;
	err = fat_free_cluster(drv, cluster, 0);
	if(err)
		goto out;
	/* free dir ent */
	err = fat_update_inode(inode, INODE_DEL);

out:	
	fat_write_submit(drv, err);
	return err;
}

/*
** 打开虚拟的根目录,
** 实际上应该为对应的disk
**/
static int fat_open_root(DRIVE_INFO *drv, inode_t *root)
{
	memset(root->long_name, 0, LONG_NAME_MAX);
	root->long_name[0]='/';
	root->long_name_size = 2;
	strcpy(root->short_name, "/");
	root->short_name_size = 1;
	root->attr = ATTR_DIR;
	root->first_cluster = drv->dwRootCluster;
	root->first_sector = drv->dwFirstRootDirSector;
	root->parent_sector = 0;
	root->size = 0;
	fat_read_seek(root, 0, FAT_SEEK_START);
	fat_write_seek(root, 0, FAT_SEEK_START);
	root->drv = drv;
	return FS_OK;
}
static char     wd_str[PATH_NAME_MAX];
static FS_W_STR wd_name;/* 绝对工作目录name */

/*
** fat_seek: 计算出新的sector和cluster
**/
void fat_seek(inode_t *inode, _offset_t *offset, u32 byte_offset, u8 flag)
{
	DRIVE_INFO *drv = inode->drv;
	u32 sectors, /* 从当前cluster开始到读取字节后的总sector数目*/
		sector_offset, /* 字节在sector偏移量*/
		start_sector;/* 开始sector号*/
	u32 clusters, /* 总cluster数目*/
		cluster_offset, /* sector在cluster中的偏移量*/
		start_cluster,/* 开始cluster号*/
		next_cluster;
	u32 bytes;

	//fat_debug("fat_seek: start bytes=%d, offset=%d\n", offset->bytes, byte_offset);
	if(byte_offset == 0)
	{
		if(flag == FAT_SEEK_START)
		{
			offset->bytes = 0;
			offset->sector = inode->first_sector;
			offset->cluster = inode->first_cluster;
			offset->eof = FALSE;
			if(!INODE_DIR(inode)&&(inode->size==offset->bytes))
			{
				offset->eof = TRUE;
			}
		}
		#if 0
		fat_debug("fat_seek: bytes=0, sector=%d, cluster=%d!\n",
			offset->sector,
			offset->cluster
		);
		#endif
		return ;
	}
	if(flag == FAT_SEEK_START)
	{
		//reset
		fat_seek(inode, offset, 0, FAT_SEEK_START);
	}

	if(offset->eof)
	{
		fat_debug("fat_seek: EOF!\n");
		return;
	}
	
	start_sector = offset->sector;
	start_cluster = offset->cluster;
	sector_offset = byte_in_sector(drv, offset->bytes);
	bytes = sector_offset + byte_offset;
	sectors = byte_to_sector(drv, bytes);
	if(inode->first_cluster == 1)
	{
		//FAT16 root dir inode
		assert(drv->bIsFAT32==FALSE);
		offset->bytes += byte_offset;
		offset->sector += sectors;
		if(offset->sector >= 
			(drv->dwFirstRootDirSector+drv->dwRootDirSectors))
		{
			offset->sector = FAT_BAD_NO;
			offset->eof = TRUE;
		}
		#if 0
		fat_debug("fat_seek: ROOT dir bytes=%d, sector=%d, cluster=%d!\n",
			offset->bytes,
			offset->sector,
			offset->cluster			
			);
		#endif
		return;
	}
	if(start_cluster<=1||start_cluster>=drv->dwClusterTotal)
	{
		assert(0);
		return;
	}
	cluster_offset = start_sector-fat_get_first_sector(drv, start_cluster);
	assert(cluster_offset>=0&&cluster_offset<(u32)(1<<drv->bClusterPower));
	sectors += cluster_offset;
	
	//update cluster
	clusters = sector_to_cluster(drv, sectors);
	next_cluster = start_cluster;
	for(;clusters>0;clusters--)
	{
		next_cluster = fat_get_next_cluster(drv, start_cluster);
		if(next_cluster == FAT_BAD_NO)
		{
			break;
		}
		start_cluster = next_cluster;
	}
	if(next_cluster == FAT_BAD_NO)
	{
		if(clusters>1||
			sector_in_cluster(drv, sectors)!=0||
			byte_in_sector(drv, bytes)!=0)
		{
			/*
			** 这时候必然应该还有新的cluster
			** 如果读取失败可能文件系统破坏
			**/
			offset->sector = FAT_BAD_NO;
			offset->cluster = FAT_BAD_NO;
			offset->eof = TRUE;
			offset->bytes += byte_offset;
		}else
		{
			/*
			** now 我们已经到有效边界
			** 为了能够让写继续,
			** 我们不能设置成FAT_BAD_NO,
			** 而应该设置成最后一个cluster的最后一个sector
			**/
			offset->cluster = start_cluster;
			offset->sector = FAT_BAD_NO;
			offset->eof = TRUE;
			offset->bytes += byte_offset;
		}
		fat_debug("cluster=%d,sector=%d\n",offset->cluster, offset->sector);
		fat_debug("fat_seek: EOF!\n");
		return;
	}
	//update sector
	cluster_offset = sector_in_cluster(drv, sectors);
	start_sector = fat_get_first_sector(drv, start_cluster)+cluster_offset;

	//
	offset->bytes += byte_offset;
	offset->sector = start_sector;
	offset->cluster = start_cluster;	

	fat_debug("fat_seek: cluster=%d, sector=%d\n", 
		offset->cluster,
		offset->sector);
}

void fat_read_seek(inode_t *inode, u32 offset, u8 flag)
{
	fat_seek(inode, &inode->offset, offset, flag);
}

void fat_write_seek(inode_t *inode, u32 offset, u8 flag)
{
	fat_seek(inode, &inode->offset, offset, flag);
}

/*
** 格式化成FAT16
**
**/
#define SECTOR_SIZE_MAX 512
#define BOOT_SECTOR 2
int fat_format_16(DRIVE_INFO *drv)
{
	FAT_SECTOR_CACHE *fat_buf;
	u8   *buffer;
	u32 sector, offset;
	u32 inv_fat_sector, inv_fat_sector_offset;/* 无效表项启始位置*/
	int err;
	u32 sector_size;

	if(0)//if(drv->dwSectorTotal<=0||drv->dwClusterTotal<2048||drv->dwClusterTotal>64*1024)
	{
		return -1;
	}
	if(drv->wSectorSize<=0||drv->wSectorSize > SECTOR_SIZE_MAX)
	{
		return -1;
	}
	drv->bSectorSizePower = get_power(drv->wSectorSize);

	if(drv->bSectorsPerCluster<=0||drv->bSectorsPerCluster>128)
	{
		return -1;
	}
	drv->bClusterPower = get_power(drv->bSectorsPerCluster);
	
	
	fat_buf = fat_buf_get(drv);
	if(fat_buf == NULL)
	{
		return -1;
	}
	buffer = FAT_BUF_P(fat_buf);
	sector_size = drv->wSectorSize;
	/*
	** Disk Raw Format
	**/
	if(drv->bDevice == FS_FLASH)
	{
		FAT32_PARTITION_TABLE *pPartitionTable=(void*)buffer;
		FAT32_BOOT_RECORD *pBootRecord=(void*)buffer;

		//format partition
		memset(buffer, 0, sector_size);
		buffer[510]=0x55;
		buffer[511]=0xaa;
		pPartitionTable->Partition[0].NumSectors = htofl(drv->dwSectorTotal);
		pPartitionTable->Partition[0].StartSectors = htofl(BOOT_SECTOR);
		fat_write(drv, 0, 1, fat_buf);
		
		//format boot sector
		memset(buffer, 0, sector_size);
		buffer[0] = 0xEB;
		buffer[1] = 0x58;
		buffer[2] = 0x90;
		memcpy(pBootRecord->OEMName, "MSWIN4.1", 8);
		pBootRecord->BytsPerSec = 512;
		pBootRecord->SecPerClus = drv->bSectorsPerCluster;
		pBootRecord->RsvdSecCnt = htofs(drv->wRsvSecCnt);
		pBootRecord->HiddSec = htofl(drv->dwHiddSecCnt);
		pBootRecord->NumFATs = 2;
		pBootRecord->TotSec32 = htofl(drv->dwSectorTotal);
		pBootRecord->FATSz16 = (u16)((drv->dwFATSize)&0xffff);
		pBootRecord->FATSz16 = htofs(pBootRecord->FATSz16);
		pBootRecord->RootEntCnt = htofs(drv->wRootEntCnt);
		buffer[510]=0x55;
		buffer[511]=0xaa;
		fat_write(drv, BOOT_SECTOR, 1, fat_buf);
		
		drv->dwFAT1StartSector = drv->wRsvSecCnt+drv->dwHiddSecCnt;
		drv->dwFAT2StartSector = drv->dwFAT1StartSector+drv->dwFATSize;
		drv->dwFirstRootDirSector = drv->dwFAT2StartSector+drv->dwFATSize;

		fs_fmt_rate(8);
	}


	//format FAT1 table 
	fat_debug("format FAT1 table\n");
	memset(buffer, 0, sector_size);
	buffer[0] = 0xf8;
	buffer[1] = 0xff;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	sector = drv->dwFAT1StartSector;
	{
		u32 cluster;
		cluster = (drv->dwSectorTotal-drv->dwFirstDataSector)/drv->bSectorsPerCluster;
		inv_fat_sector = sector+(cluster+2)*2/drv->wSectorSize;
		inv_fat_sector_offset = ((cluster+2)*2)%drv->wSectorSize;
	}
	fat_write(drv, sector++, 1, fat_buf);
	memset(buffer, 0, 4);
	offset = inv_fat_sector_offset;
	while(sector<(drv->dwFAT1StartSector+drv->dwFATSize))
	{
		if(sector>=inv_fat_sector)
		{
			memset(buffer+offset, 0xff, drv->wSectorSize-offset);
			offset = 0;
		}
		err = fat_write(drv, sector, 1, fat_buf);
		if(err==FS_ERR_CACHE_FULL)
		{
			fat_write_submit(drv, 0);
			continue;
		}else if(err)
		{
			fat_debug("write sector[%d] fail, err=%d\n",
				sector, err);
			return err;
		}
		sector++;
		
	}
	fs_fmt_rate(8);
	
	//format FAT2 table
	#if 0
	fat_debug("format FAT2 table\n");
	sector = drv->dwFAT2StartSector;
	buffer[0] = 0xf8;
	buffer[1] = 0xff;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	fat_write(drv, sector++, 1, fat_buf);
	memset(buffer, 0, 4);
	offset = inv_fat_sector_offset;
	while(sector<(drv->dwFAT2StartSector+drv->dwFATSize))
	{
		if(sector>=inv_fat_sector)
		{
			memset(buffer+offset, 0xff, drv->wSectorSize-offset);
			offset = 0;
		}
		err = fat_write(drv, sector, 1, fat_buf);
		if(err==FS_ERR_CACHE_FULL)
		{
			fat_write_submit(drv, 0);
			continue;
		}else if(err)
		{
			fat_debug("write sector[%d] fail, err=%d\n",
				sector, err);
			return err;
		}
		sector++;
	}
	#endif

	//format root dir
	fat_debug("format root dir\n");
	sector = drv->dwFirstRootDirSector;
	memset(buffer, 0, drv->wSectorSize);
	while(sector<(drv->dwFirstRootDirSector+drv->dwRootDirSectors))
	{
		err = fat_write(drv, sector, 1, fat_buf);
		if(err==FS_ERR_CACHE_FULL)
		{
			fat_write_submit(drv, 0);
			continue;
		}else if(err)
		{
			fat_debug("write sector[%d] fail, err=%d\n",
				sector, err);
			return err;
		}
		sector++;
	}
	fat_write_submit(drv, 0);

	fs_fmt_rate(8);
	return FS_OK;
}

int mbr_check(u8 *sbuf, DRIVE_INFO *pDrive, u32 oldsec)
{
	FAT32_PARTITION_TABLE *pPartitionTable;
	FAT32_BOOT_RECORD *pBootRecord;
	u32 dwSector;
	FAT_SECTOR_CACHE *fat_buf;
	int i;
	u8 *buf;
	int err;

	pPartitionTable = (FAT32_PARTITION_TABLE*)sbuf;
	for(i = 0; i < 4; i++)
	{
		if(pPartitionTable->Partition[i].BootInd == 0x80 && pPartitionTable->Partition[i].NumSectors)
		{
			dwSector = pPartitionTable->Partition[i].StartSectors;
			dwSector = ftohl(dwSector);

			err = fat_read(pDrive, dwSector, 1, &fat_buf);
			if(err != FS_OK)
			{
				continue;
			}
			buf = FAT_BUF_P(fat_buf);
			if(buf[0x1fe] == 0x55 && buf[0x1ff] == 0xaa)
			{
				if(buf[0]==0xeb||buf[0]==0xe9)
				{
					fat_read(pDrive, oldsec, 1, &fat_buf);
					return 1;
				}
			}
		}
	}
	fat_read(pDrive, oldsec, 1, &fat_buf);
	return 0;
}

int fat_mount(int driver, DRIVE_INFO *info)
{
	int i, err;
	u32 dwSector;
	u32 dwFATSz;
	u32 dwRootDirSectors, dwFirstDataSector, dwDataSectors;
	u32 dwDataClusters;
	FAT32_PARTITION_TABLE *pPartitionTable;
	FAT32_BOOT_RECORD *pBootRecord;
	DRIVE_INFO *pDrive;
	fat_driver_op *op = info->ops;
	FAT_SECTOR_CACHE *fat_buf;
	u8 *buf,j;

	if(sDriveInfo[driver])
		fat_unmount(driver);
	pDrive = info;
	info->bDevice = driver;

	if(pDrive->wSectorSize==0||pDrive->dwSectorTotal==0)
	{
		assert(0);
		return -1;
	}
	pDrive->bSectorSizePower = get_power(pDrive->wSectorSize);
	dwSector = 0;
	for(i=0; i<10; i++)
	{
		err = fat_read(pDrive, dwSector, 1, &fat_buf);
		if(err != FS_OK)
		{
			fat_error("fat_mount: fail in Read sector[0]\n");
			return err;
		}
		buf = FAT_BUF_P(fat_buf);
		if(fat_sig_ok(driver,buf)!=FS_OK)
		{
			fat_error("fat_mount: sector[%d] sig is bad %02x%02x\n",
				dwSector,buf[510],buf[511]);
			fat_buf_debug(buf, 512, "sector");
			if(driver == FS_FLASH)
			{
				fat_debug("fat_mount: it is nand flash, format it.....\n");
				err = fat_format_16(info);
				if(err!=FS_OK)
				{
					fat_debug("fat_mount: format fail err=%d\n", err);
					return -1;
				}
				fat_debug("fat_mount: format OK!\n");
				continue;
			}else
			{
				return -1;
			}
		}
		//////////////////////////////////
		if(buf[0]==0xeb||buf[0]==0xe9)
		{
			/* this is boot sector */
			if(!mbr_check(buf, pDrive, dwSector))
				break;
		}

		// Try to find a PartitionTable.
		pPartitionTable = (FAT32_PARTITION_TABLE *) buf;
		for(j=0;j<4;j++)
		{
			if(	pPartitionTable->Partition[j].NumSectors)
			{
				// We found a PartitionTable, read BootRecord.
				dwSector = pPartitionTable->Partition[j].StartSectors;
				dwSector = ftohl(dwSector);
				fat_debug("fat_mount: Boot Sector May Be in %d\n", dwSector);
				break;
			}
		}//for(j)
		if(j>=4)return -1;
	}//for(i)

	if(i>=10)
	{
		fat_error("fat_mount: fail to find boot sector!\n");
		return -1;
	}	
	pBootRecord = (FAT32_BOOT_RECORD *) buf;

	//Get sector Info
	#if 0
	//不从这里读取，因此该信息会比实际小
	pDrive->dwSectorTotal = ftohs(pBootRecord->TotSec32);
	if(pDrive->dwSectorTotal == 0)
	{
		pDrive->dwSectorTotal = ftohs(pBootRecord->TotSec16);
	}
	#endif
	pDrive->wSectorSize = ftohs(pBootRecord->BytsPerSec);
	pDrive->bSectorSizePower = get_power(pDrive->wSectorSize);
	if(pDrive->bSectorSizePower<9||pDrive->bSectorSizePower>12)
	{
		fat_error("fat_mount: Sector Size %d is bad\n",
			pDrive->wSectorSize
			);
		return -1;
	}
	pDrive->wRsvSecCnt = ftohs(pBootRecord->RsvdSecCnt);
	pDrive->dwHiddSecCnt = ftohl(pBootRecord->HiddSec);

	//Get cluster Info
	pDrive->bSectorsPerCluster = pBootRecord->SecPerClus;
	pDrive->dwClusterTotal = pDrive->dwSectorTotal/pDrive->bSectorsPerCluster;
	pDrive->bClusterPower = get_power(pDrive->bSectorsPerCluster);
	if(pDrive->bClusterPower>7)
	{
		fat_error("fat_mount: SecPerClus %d is bad\n",
			pDrive->bSectorsPerCluster
			);
		return -1;
	}
	pDrive->dwClusterSize = pBootRecord->SecPerClus * 
		pDrive->wSectorSize;

	//Get FAT Info
	if(pBootRecord->NumFATs != 2)
	{
		fat_error("fat_mount: FAT num %d is bad, MUST be 2\n",
			pBootRecord->NumFATs
			);
		return -1;
	}
	if (pBootRecord->FATSz16 != 0) 
	{
		dwFATSz = ftohs(pBootRecord->FATSz16);
		//pDrive->bIsFAT32 = FALSE;
		//pDrive->dwRootCluster = 1;		/* special value, see */
	} else 
	{
		dwFATSz = ftohl(pBootRecord->Off36.FAT32.FATSz32);
		//pDrive->bIsFAT32 = TRUE;
		//pDrive->dwRootCluster = ftohl(pBootRecord->Off36.FAT32.RootClus);
	}
	if(dwFATSz == 0||dwFATSz>pDrive->dwSectorTotal)
	{
		fat_error("fat_mount: FAT Size %d sectors is bad!\n",
			dwFATSz
			);
		return -1;
	}
	pDrive->dwFATSize = dwFATSz;
	pDrive->dwFAT1StartSector = pDrive->wRsvSecCnt+pDrive->dwHiddSecCnt;
	pDrive->dwFAT2StartSector = pDrive->dwFAT1StartSector+ dwFATSz;

	//Get Root Dir Info
	pDrive->wRootEntCnt = ftohs(pBootRecord->RootEntCnt);
	dwRootDirSectors = 
	  ((pDrive->wRootEntCnt*32)+(pDrive->wSectorSize-1))/pDrive->wSectorSize;
	pDrive->dwRootDirSectors = dwRootDirSectors;
	dwFirstDataSector = pDrive->dwFAT2StartSector+dwFATSz+dwRootDirSectors;
	dwDataSectors = pDrive->dwSectorTotal-dwFirstDataSector;
	dwDataClusters = dwDataSectors>>pDrive->bClusterPower;
	#if 1
	if(dwDataClusters<4085)
	{
		fat_debug("This is FAT12!\n");		
		fat_debug("We NOT SUPPORT now!\n");
		return -1;
	}else
	#endif
	if(dwDataClusters<65525)
	{
		fat_debug("This is FAT16!\n");
		pDrive->bIsFAT32 = FALSE;
		pDrive->dwFirstRootDirSector = pDrive->dwFAT2StartSector+dwFATSz;
		pDrive->dwRootCluster = 1;//特定值，没有实际意义
	}else 
	{
		fat_debug("This is FAT32!\n");
		pDrive->bIsFAT32 = TRUE;
		pDrive->dwRootCluster = ftohl(pBootRecord->Off36.FAT32.RootClus);
		pDrive->dwFirstDataSector = dwFirstDataSector;
		pDrive->dwFirstRootDirSector = fat_get_first_sector(pDrive, pDrive->dwRootCluster);
	}
	
	//Get Data Sector Info
	pDrive->dwFirstDataSector = dwFirstDataSector;
	pDrive->dwDataSectors = dwDataSectors;
	pDrive->dwDataClusters = dwDataClusters;
	pDrive->dwCluster2StartSector = ftohl(pBootRecord->HiddSec) + 
		ftohs(pBootRecord->RsvdSecCnt) + 
		(pBootRecord->NumFATs * dwFATSz) + 
		dwRootDirSectors;
	pDrive->bCacheEnable = 1;
	pDrive->FATSectorOff = pDrive->dwFAT1StartSector;

	fat_show_driv_info(info);

	sDriveInfo[driver] = info;

	//fat_format_16(pDrive);
	return FS_OK;
}

int fat_format_32(DRIVE_INFO *drv, u8 bRaw)
{
	FAT_SECTOR_CACHE *fat_buf;
	u8  fat1[]={
		0xf8,0xff,0xff,0x0f,
		0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0x0f,
	};
	u32 i;

	assert(drv->dwRootCluster==2);

	//FAT1 table
	fat_buf = fat_buf_get(drv);
	memset(FAT_BUF_P(fat_buf), 0, drv->wSectorSize);
	memcpy(FAT_BUF_P(fat_buf), fat1, 4*3);
	fat_write(drv, drv->dwFAT1StartSector, 1, fat_buf);

	//Root Dir
	memset(FAT_BUF_P(fat_buf), 0, drv->wSectorSize);
	for(i=0; i<drv->bSectorsPerCluster; i++)
	{
		fat_write(drv, drv->dwFirstRootDirSector+i, 1, fat_buf);
	}

	fat_write_submit(drv, 0);
	return FS_OK;
}

int fat_check_fat(int disk)
{
	DRIVE_INFO *drv;
	int err;
	u32 sector;
	FAT_SECTOR_CACHE *cache;
	u8 *buf;
	int fat_size, value, cluster, i, free_num;
	
	if(disk<FS_UDISK||disk>=FS_MAX_NUMS)
		return 0;
	drv = sDriveInfo[disk];
	if(drv==NULL)
	{
		s80_printf("No Such Device!\n");
		return 0;
	}
	fat_size = drv->bIsFAT32?4:2;
	sector = drv->dwFAT1StartSector;
	cluster = 2;
	free_num = 0;
	
	while(sector<drv->dwFAT1StartSector+drv->dwFATSize)
	{
		err = fat_read(drv, sector, 1, &cache);
		if(err)
		{
			s80_printf("Read Sector[%d] fail, err=%d\n",
				sector, err);
			return 0;
		}
		buf = FAT_BUF_P(cache);
		if(sector == drv->dwFAT1StartSector)
		{
			i = 2;
			buf += 2*fat_size;
		}else
		{
			i = 0;
		}
		for(;i<drv->wSectorSize/fat_size;i++)
		{
			value = 0;
			memcpy(&value, buf, fat_size);
			if(value!=0)
			{
			}else
			{
				free_num++;
			}
			cluster++;
			buf+=fat_size;
		}
		sector++;
	}
	return free_num;
}

typedef struct fs_file
{
	int        id;
	inode_t    inode;
	int        flag;
}FS_FILE;
#define MAX_FILE_NUM 10
/*
** 文件句柄数组
**/
static FS_FILE fs_file_arr[MAX_FILE_NUM];

/*
** 用于临时存放路径信息
**/
#define MAX_PATH_LEVEL 50
static FS_W_STR parent_path[MAX_PATH_LEVEL];
static inode_t  parent_inode[MAX_PATH_LEVEL];
static int parent_level;

static FS_FILE *fs_get_file(int id)
{
	int i;

	for(i=0; i<MAX_FILE_NUM; i++)
	{
		if(fs_file_arr[i].id == id)
			return fs_file_arr+i;
	}
	return NULL;
}

static int fs_file2index(FS_FILE *file)
{
	if(file<&fs_file_arr[0]||file>&fs_file_arr[MAX_FILE_NUM-1])
		return -1;
	return (file-fs_file_arr);
}

static FS_FILE* fs_index2file(int index)
{
	if(index<0||index>=MAX_FILE_NUM)
		return NULL;
	if(fs_file_arr[index].id == 0)
		return NULL;
	return fs_file_arr+index;
}

static int fs_get_id(void)
{
	static int id = 1;
	int i;
	while(1)
	{
		for(i=0; i<MAX_FILE_NUM; i++)
		{
			if(fs_file_arr[i].id == id)
				break;
		}
		if(i>=MAX_FILE_NUM)
			return id++;
		id++;
	}
}

#define FS_PATH_CHAR '/'

#define FS_CHAR_WIDTH(fmt) ((fmt)==NAME_FMT_ASCII?1:2)

#define FS_DISABLE_UNICODE/* 不支持unicode编码 */

/*
**是否是相对路径
**/
static int fs_relat_path(FS_W_STR *name)
{
	if(name->str[0]==FS_PATH_CHAR)
		return FALSE;
	return TRUE;
}

/*
** 检查是否合法ASCII char
**/
static int fs_check_ascii_char(char *str, int len)
{
	int i;
	u8 c;

	for(i=0; i<len; i++)
	{
		c = (u8)str[i];
		if(c<0x20||c>0x7e)
			return FS_ERR_CHAR;
	}
	return FS_OK;
}

static int fs_check_name(FS_W_STR *name_in)
{
	if(name_in==NULL||name_in->str==NULL||
		name_in->size<=0||
		name_in->fmt<=0||name_in->fmt>NAME_FMT_ASCII||
		(name_in->fmt==NAME_FMT_UNICODE&&name_in->size%2!=0)
		)
	{
		return FS_ERR_ARG;
	}
	if((name_in->fmt==NAME_FMT_UNICODE&&name_in->size>PATH_NAME_MAX)||
		(name_in->fmt==NAME_FMT_ASCII&&name_in->size>(PATH_NAME_MAX/2)))
	{
		return FS_ERR_NAME_SIZE;
	}
	#ifdef FS_DISABLE_UNICODE
	if(name_in->fmt == NAME_FMT_UNICODE)
	{
		return FS_ERR_ARG;
	}
	if(fs_check_ascii_char(name_in->str, name_in->size))
	{
		return FS_ERR_CHAR;
	}
	#endif
	return FS_OK;
}

int fat_unmount(int driver)
{
	int i;
	DRIVE_INFO *drv;
	assert(driver>=FS_UDISK&&driver<FS_MAX_NUMS);
	drv = sDriveInfo[driver];
	if(drv == NULL)
		return FS_ERR_NODEV;
	fat_debug("fat_unmount..........\n");
	fat_cache_destroy(drv);
	sDriveInfo[driver] = NULL;
	for(i=0; i<MAX_FILE_NUM; i++)
	{
		if(fs_file_arr[i].id == 0)
			continue;
		if(fs_file_arr[i].inode.drv==drv)
		{
			INODE_SET_INV(&fs_file_arr[i].inode);
		}
	}
	fat_debug("fat_umount OK!\n");
	return FS_OK;
}

void s_fs_init(void)
{
	int err, disk_num = FS_FLASH;//FS_FLASH or FS_UDISK;
	inode_t inode;

	s80_printf("s_fs_init........\n");
	memset(fs_file_arr, 0, sizeof(fs_file_arr));
	memset(&wd_name, 0, sizeof(wd_name));
	strcpy(wd_str, "/");
	wd_name.str = wd_str;
	wd_name.fmt = NAME_FMT_ASCII;
	wd_name.size = strlen(wd_str);
	if(sDriveInfo[FS_FLASH]==NULL)return;

	err = fat_open_root(sDriveInfo[disk_num], &inode);
	if(err)return;

    switch(disk_num)
    {
    case FS_UDISK:
        strcpy(wd_str,"/"FAT_UDISK_DIR);
        break;

    case FS_FLASH:
        strcpy(wd_str,"/"FAT_FLASH_DIR);
        break;

    case FS_SDCARD:
        strcpy(wd_str,"/"FAT_SDCARD_DIR);
        break;

    case FS_UDISK_A:
        strcpy(wd_str,"/"FAT_UDISK_A_DIR);
        break;

    default:
        break;
    }
	wd_name.str = wd_str;
	wd_name.fmt = NAME_FMT_ASCII;
	wd_name.size = strlen(wd_str);
	return;
}

static int fs_get_path(FS_W_STR *name, u8 bRemove, char **ppath, int *psize)
{
	char *str = name->str, *path;
	int char_width = FS_CHAR_WIDTH(name->fmt);
	int size, str_size = name->size;
	
	while(str_size>0&&str[0]==FS_PATH_CHAR)
	{
		str += char_width;
		str_size -= char_width;
	}
	if(str_size == 0)
	{
		return -1;
	}
	path = str;
	size = 0;
	while(str_size>0&&str[0] != FS_PATH_CHAR)
	{
		size += char_width;
		str += char_width;
		str_size -= char_width;
	}
	if(bRemove)
	{
		name->str = str;
		name->size = str_size;
	}
	*ppath = path;
	*psize = size;
	return FS_OK;
}

static int fs_init_parent_path(FS_W_STR *name)
{
	char *path;
	int size, err;
	
	while(parent_level>=0&&parent_level<MAX_PATH_LEVEL)
	{
		err = fs_get_path(name, TRUE, &path, &size);
		if(err)
			break;
		if(fat_long_name_cmp(path, size, name->fmt, 
					".\0", 2)==TRUE)
		{
			continue;
		}
		if(fat_long_name_cmp(path, size, name->fmt,
					".\0.\0",4)==TRUE)
		{
			parent_level--;
			continue;
		}
		parent_path[parent_level].str = path;
		parent_path[parent_level].fmt = name->fmt;
		parent_path[parent_level].size = size;
		parent_level++;
	}
	if(parent_level<=0)
		return FS_ERR_NODEV;
	if(parent_level>=MAX_PATH_LEVEL)
		return FS_ERR_PATH;
	return FS_OK;
}

static void fs_print_parent(void)
{
	int i;
	fat_debug("-------------path level=%d---------\n",
		parent_level);
	for(i=0; i<parent_level; i++)
	{
		fat_name_print(parent_path[i].str, parent_path[i].size);
		fat_debug("\n");
	}
	fat_debug("--------------------------------\n");
}

static int fs_init_parent(FS_W_STR *name)
{
	char *path;
	int size, err;
	u8  drv_num;

	err = FS_OK;
	parent_level = 0;
	if(fs_relat_path(name))
	{
		FS_W_STR wd;
		//相对路径
		if(wd_name.size==0)
		{
			return -1;
		}
		wd = wd_name;
		err = fs_init_parent_path(&wd);
		if(err)
			return err;
	}
	err = fs_init_parent_path(name);
	if(err)
		return err;

	fs_print_parent();
	/*
	** 判断对应的磁盘是否存在
	**/
	path = parent_path[0].str;
	size = parent_path[0].size;
	drv_num = 0xff;
	/*
	** 先比较flash
	**/
	if(fat_long_name_cmp(path, size, parent_path[0].fmt,
		fat_ascii_to_UNICODE(FAT_FLASH_DIR, strlen(FAT_FLASH_DIR)),
		2*(strlen(FAT_FLASH_DIR)))==TRUE)
	{
		drv_num = FS_FLASH;
	}		
	/*
	** 接着比较U 盘
	**/
	else if(fat_long_name_cmp(path, size, parent_path[0].fmt,
		fat_ascii_to_UNICODE(FAT_UDISK_DIR, strlen(FAT_UDISK_DIR)),
		2*(strlen(FAT_UDISK_DIR)))==TRUE)
	{
		drv_num = FS_UDISK;
		fs_mount_udisk();
	}
	/*
	** 接着比较UDISK A 盘
	**/
	else if(fat_long_name_cmp(path, size, parent_path[0].fmt,
		fat_ascii_to_UNICODE(FAT_UDISK_A_DIR, strlen(FAT_UDISK_A_DIR)),
		2*(strlen(FAT_UDISK_A_DIR)))==TRUE)
	{
		drv_num = FS_UDISK_A;
		if(GetHWBranch() == D210HW_V2x)
		{
			err = OhciMountUdisk_proxy();
			if(FS_ERR_DEV_REMOVED == err)
			{
				set_udisk_a_absent();
			}
			else if(err)			// 如果是链路错误的话直接返回
			{
				return err;
			}
		}
		else
		{
			ohci_fs_mount_udisk();
		}
	}
	/*
	** 接着比较SD卡
	**/
	else if(fat_long_name_cmp(path, size, parent_path[0].fmt,
		fat_ascii_to_UNICODE(FAT_SDCARD_DIR, strlen(FAT_SDCARD_DIR)),
		2*(strlen(FAT_SDCARD_DIR)))==TRUE)
	{
		drv_num = FS_SDCARD;
	}
	
	if(drv_num>=FS_MAX_NUMS||sDriveInfo[drv_num]==NULL)
	{
		return FS_ERR_NODEV;
	}
	err = fat_open_root(sDriveInfo[drv_num], &parent_inode[0]);
	return err;
}

static int fs_create_inode(inode_t *parent, 
				inode_t *inode, int attr,
				char *name,
				int name_size,
				u8   fmt
				)
{
	int err;
	if(!(attr&FS_ATTR_C))
	{
		/*
		** Open Only
		**/
		err = fat_open_inode(parent,
				name,
				name_size,
				fmt,
				FALSE,
				inode
				);
		if(err==FS_ERR_NAME_EXIST)
		{
			if(attr&FS_ATTR_D&&!INODE_DIR(inode))
			{
				return FS_ERR_NOTDIR;
			}
			return FS_OK;
		}
		if(err == FS_OK)
			return -1;
		return err;
	}else
	{
		/*
		** Create if need
		**/
		if(attr&FS_ATTR_D)
		{
			err = fat_open_dir(parent,
				name,
				name_size,
				fmt,
				inode,
				attr);
			if(err)
				return err;
		}else
		{
			err = fat_open_file(parent,
				name,
				name_size,
				fmt,
				inode,
				attr);
			if(err)
				return err;
		}	
	}
	return FS_OK;
}

static int fs_path_open(FS_W_STR *name_in, int attr)
{
	FS_W_STR name_t, *name=&name_t;
	inode_t t, *inode=&t;
	int err, level;

	err = fs_check_name(name_in);
	if(err)
		return err;
	memset(parent_inode, 0, sizeof(parent_inode));
	*name = *name_in;
	err = fs_init_parent(name);
	if(err)
		return err;
	for(level=1; level<parent_level-1; level++)
	{
		
		fat_read_seek(parent_inode+level-1, 0, FAT_SEEK_START);		
		err = fat_open_dir(parent_inode+level-1,
					parent_path[level].str,
					parent_path[level].size,
					parent_path[level].fmt,
					parent_inode+level,
					0);
				
		if(err)
		{
			return err;
		}
		if(!INODE_DIR(parent_inode+level))
		{
			return FS_ERR_NOTDIR;
		}
	}
	if(level==parent_level-1)
	{
		err = fs_create_inode(parent_inode+level-1,
				parent_inode+level, attr,
				parent_path[level].str,
				parent_path[level].size,
				parent_path[level].fmt
			);
		if(err)
			return err;
	}else
	{
		level = 0;
	}
	return level;
}

static int fs_inode_used(inode_t *inode2)
{
	int i;
	inode_t *inode1;
	for(i=0; i<MAX_FILE_NUM; i++)
	{
		if(fs_file_arr[i].id == 0)
			continue;
		inode1 = &fs_file_arr[i].inode;
		if(inode1->parent_sector == inode2->parent_sector&&
			inode1->short_dir_start_sector == 
				inode2->short_dir_start_sector&&
			inode1->short_dir_start_sector_offset == 
				inode2->short_dir_start_sector_offset)
		{
			return FS_ERR_BUSY;
		}
	}
	return 0;
}

/*
** 检查cluster链表完整性
**/
static int fs_check_clusters(inode_t *inode)
{
	u32 cluster = inode->first_cluster;
	int num;
	num = 0;
	while(GOOD_CLUSTER(inode->drv, cluster))
	{
		if((u32)num>inode->drv->dwDataClusters)
		{
			fat_debug("fs_check_clusters_: num %d > data clusters %d\n",
				num, inode->drv->dwDataClusters);
			return FS_ERR_CLUSTER;
		}
		num++;
		cluster = fat_get_next_cluster(inode->drv, cluster);
	}
	if(!INODE_DIR(inode))
	{
		if(inode->size>0)
		{
			if(inode->size>(num*inode->drv->dwClusterSize))
			{
				fat_debug("fs_check_clusters_: size %d > num*clustersize %d\n",
					inode->size, num*inode->drv->dwClusterSize);
				return FS_ERR_CLUSTER;
			}
		}
	}
	return num;
}

int FsOpen(FS_W_STR *name_in, int attr)
{
	FS_FILE  *file;
	int level; 

	if( !(attr&FS_ATTR_W) && !(attr&FS_ATTR_R) )return FS_ERR_ARG;

	//--inhibit tail '/',2010.4.12
	if(strlen(name_in->str) && memcmp(name_in->str,"./",2) &&
		name_in->str[strlen(name_in->str)-1]=='/')return FS_ERR_ARG;

	file = fs_get_file(0);
	if(file == NULL)return FS_ERR_NOMEM;
		
	level = fs_path_open(name_in, attr);
	if(level<0)return level;

	if(!strcmp(parent_path[0].str,FAT_UDISK_DIR) && s_UsbHostState())return FS_ERR_NOINIT;
	if(!strcmp(parent_path[0].str,FAT_UDISK_A_DIR))
	{
		if(GetHWBranch() == D210HW_V2x && !base_usb_ohci_state) return FS_ERR_NOINIT;
		else if(!usb_host_ohci_state)return FS_ERR_NOINIT;
	}
    if(!strcmp(parent_path[0].str,FAT_SDCARD_DIR) && !get_sdcard_state())return FS_ERR_NOINIT;
	//if(GPIO1_IN_REG&BIT4)return FS_ERR_DEVPORT_OCCUPIED;//DEVICE port is attached to a host

	if(fs_check_clusters(parent_inode+level)<0)
		return FS_ERR_CLUSTER;

	/*
	** 检查节点是否已打开
	** fs不允许打开2次
	**/
	if(fs_inode_used(parent_inode+level))
		return FS_ERR_BUSY;

	memset(file, 0, sizeof(FS_FILE));
	file->id = fs_get_id();
	file->inode = parent_inode[level];
	file->flag = attr;
	fat_read_seek(&file->inode, 0, FAT_SEEK_START);
	return fs_file2index(file);
}

int FsSetAttr(int fd, int attr)
{
	FS_FILE *file = fs_index2file(fd);
		
	if(file == NULL)
		return FS_ERR_BADFD;
	attr &= (FS_ATTR_R|FS_ATTR_W);
	if((attr&(FS_ATTR_R|FS_ATTR_W))==0)
	{
		return FS_ERR_ARG;
	}
	file->flag = attr;
	return FS_OK;
}

int FsClose(int fd)
{
	FS_FILE *file = fs_index2file(fd);
	if(file == NULL)
		return FS_ERR_BADFD;
	file->id = 0;
	return FS_OK;
}

int FsDelete(FS_W_STR *name_in)
{
	FS_W_STR name_t, *name=&name_t;
	inode_t t, *inode=&t;
	int err, level;

	err = fs_check_name(name_in);
	if(err)
		return err;
	memset(parent_inode, 0, sizeof(parent_inode));
	*name = *name_in;
	err = fs_init_parent(name);
	if(err)return err;

	if(!strcmp(parent_path[0].str,FAT_UDISK_DIR) && s_UsbHostState())return FS_ERR_NOINIT;
   	if(!strcmp(parent_path[0].str,FAT_UDISK_A_DIR))
	{
		if(GetHWBranch() == D210HW_V2x && !base_usb_ohci_state) return FS_ERR_NOINIT;
		else if(!usb_host_ohci_state)return FS_ERR_NOINIT;
	}
    if(!strcmp(parent_path[0].str,FAT_SDCARD_DIR) && !get_sdcard_state())return FS_ERR_NOINIT;
	//if(GPIO1_IN_REG&BIT4)return FS_ERR_DEVPORT_OCCUPIED;//DEVICE port is attached to a host

	for(level=1; level<parent_level-1; level++)
	{
		
		fat_read_seek(parent_inode+level-1, 0, FAT_SEEK_START);		
		err = fat_open_dir(parent_inode+level-1,
					parent_path[level].str,
					parent_path[level].size,
					parent_path[level].fmt,
					parent_inode+level,
					0);
				
		if(err)
		{
			return err;
		}
		if(!INODE_DIR(parent_inode+level))
		{
			return FS_ERR_NOTDIR;
		}
	}
	if(level==parent_level-1)
	{
		err = fat_read_dir(parent_inode+level-1,
				parent_path[level].str,
				parent_path[level].size,
				parent_path[level].fmt,
				parent_inode+level,
				NULL);
		if(err)
		{
			if(err == FS_ERR_EOF)
				return FS_ERR_NOENT;
			return err;
		}
		if(fs_inode_used(parent_inode+level))
			return FS_ERR_BUSY;
		err = fat_del_inode(parent_inode+level);
	}else
	{
		return FS_ERR_PATH;
	}
	return err;
}

static void fs_cpy_time(FS_DATE_TIME *fs_fmt, FAT32_FILEDATETIME *fat_fmt)
{
	fs_fmt->seconds = fat_fmt->Seconds;
	fs_fmt->minute = fat_fmt->Minute;
	fs_fmt->hour = fat_fmt->Hour;
	fs_fmt->day = fat_fmt->Day;
	fs_fmt->month = fat_fmt->Month;
	fs_fmt->year = 1980+fat_fmt->Year;
}
static int fs_set_inode(inode_t *inode,/* IN */
				FS_INODE_INFO *fs_inode/* OUT */
				)
{
	memset(fs_inode, 0, sizeof(FS_INODE_INFO));
	#ifdef FS_DISABLE_UNICODE
	if(inode->long_name_size>0)
	{
		int i;
		u8 *p = inode->long_name;
		if((inode->long_name_size%2)!=0)
		{
			return FS_ERR_NAME_SIZE;
		}
		fs_inode->name_size = 0;
		for(i=0; i<inode->long_name_size; i+=2, p+=2)
		{
			if(p[1]!=0||
				p[0]<0x20||p[0]>0x7d)
			{
				return FS_ERR_CHAR;
			}
			fs_inode->name[fs_inode->name_size++] = p[0];
		}
		fs_inode->name_fmt = NAME_FMT_ASCII;
	}
	#else
	if(inode->long_name_size>0)
	{
		fs_inode->name = NAME_FMT_UNICODE;
		memcpy(fs_inode->name, inode->long_name, inode->long_name_size);
		fs_inode->name_size = inode->long_name_size;
	}
	#endif

	if(inode->long_name_size==0)
	{
		fs_inode->name_fmt = NAME_FMT_ASCII;
		fs_inode->name_size = inode->short_name_size;
		memcpy(fs_inode->name, inode->short_name, inode->short_name_size);		
	}
	
	fs_inode->attr = inode->attr;
	fs_cpy_time(&fs_inode->wrtime, &inode->wrtime);
	fs_cpy_time(&fs_inode->crtime, &inode->crtime);
	fs_inode->size = inode->size;
	fs_inode->space = -1;
	return FS_OK;
}

int FsGetInfo(int fd, FS_INODE_INFO *fs_inode)
{
	FS_FILE *file = fs_index2file(fd);
	DRIVE_INFO *drv;
	int num;

	if(file == NULL)
		return FS_ERR_BADFD;
	if(fs_inode == NULL)
		return FS_ERR_ARG;
	if(INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	drv = file->inode.drv;
	fs_set_inode(&file->inode, fs_inode);
	num = 0;
	num = fs_check_clusters(&file->inode);
	if(num<0)
		return FS_ERR_CLUSTER;
	fat_debug("FsGetInfo: cluster %d\n", num);
	fs_inode->space = num*drv->wSectorSize*drv->bSectorsPerCluster;
	return FS_OK;
}

int FsDirRead(int fd, FS_INODE_INFO *fs_inode, int num)
{
	FS_FILE *file = fs_index2file(fd);
	inode_t inode, *parent;
	int i, err, cluster_num;
	DRIVE_INFO *drv;

	if(fs_inode == NULL||num<=0)
		return FS_ERR_ARG;
	if(file == NULL)
		return FS_ERR_BADFD;
	parent = &file->inode;
	if(!INODE_DIR(parent))
	{
		return FS_ERR_NOTDIR;
	}
	if(INODE_INV(parent))
	{
		return FS_ERR_NODEV;
	}
	if(!(file->flag&FS_ATTR_R))
	{
		return FS_ERR_WO;
	}
	for(i=0; i<num; )
	{
		err = fat_read_dir(parent, NULL, 0, 0, &inode, NULL);
		if(err)
		{
			if(err==FS_ERR_EOF)
				break;
			return err;
		}
		drv = inode.drv;
		if(fs_set_inode(&inode, fs_inode)!=FS_OK)
		{
			continue;
		}
		cluster_num = 0;
		cluster_num = fs_check_clusters(&inode);
		if(cluster_num>=0)
		{
			fs_inode->space = cluster_num*drv->dwClusterSize;
		}
		fs_inode++;
		i++;
	}
	return i;
}

int FsDirSeek(int fd, int num, int flag)
{
	int i, err;
	FS_FILE *file = fs_index2file(fd);
	inode_t inode;
	FS_INODE_INFO fs_inode;
	
	if(file == NULL)
		return FS_ERR_BADFD;
	if(!INODE_DIR(&file->inode))
		return FS_ERR_NOTDIR;
	if(INODE_INV(&file->inode))
	{
		return FS_ERR_NODEV;
	}
	if(num<0)
		return FS_ERR_ARG;
	switch(flag)
	{
	case FS_SEEK_SET:
		fat_read_seek(&file->inode, 0, FAT_SEEK_START);
		break;
	case FS_SEEK_CUR:		
		break;
	case FS_SEEK_END:
	default:
		return FS_ERR_ARG;
	}
	for(i=0; i<num; )
	{
		err = fat_read_dir(&file->inode, NULL, 0, 0, &inode, NULL);
		if(err)
		{
			break;
		}
		if(fs_set_inode(&inode, &fs_inode)!=FS_OK)
		{
			continue;
		}
		i++;
	}
	return FS_OK;
}

int FsDirOpen(int fd, FS_W_STR *name, int attr)
{
	FS_FILE *parent = fs_index2file(fd);
	FS_FILE *child;
	int i, char_width; 
	_offset_t old_offset;
	int err;

	if(parent==NULL)
		return FS_ERR_BADFD;
	if(!INODE_DIR(&parent->inode))
		return FS_ERR_NOTDIR;	
	if(((attr&FS_ATTR_W)==0)&&
		((attr&FS_ATTR_R)==0))
	{
		return FS_ERR_ARG;
	}
	if((parent->flag&FS_ATTR_W)==0)
	{
		return FS_ERR_RO;
	}
	if(INODE_INV(&parent->inode))
	{
		return FS_ERR_NODEV;
	}
	err = fs_check_name(name);
	if(err)
		return err;
	char_width=FS_CHAR_WIDTH(name->fmt);
	for(i=0; i<name->size; i+=char_width)
	{
		if(name->str[i]==FS_PATH_CHAR)
			return FS_ERR_CHAR;
	}
	child = fs_get_file(0);
	if(child == NULL)
		return FS_ERR_NOMEM;
	old_offset = parent->inode.offset;
	memset(child, 0, sizeof(FS_FILE));
	err = fs_create_inode(&parent->inode, &child->inode, attr,
				name->str, name->size, name->fmt);
	parent->inode.offset = old_offset;
	if(err)
		return err;
	child->id = fs_get_id();
	child->flag = attr;
	return fs_file2index(child);
}

int FsDirDelete(int fd, FS_W_STR *name)
{
	int i, err;
	int char_width;
	inode_t inode;
	_offset_t old_offset;
	FS_FILE *file = fs_index2file(fd);
	
	if(file==NULL)
		return FS_ERR_BADFD;
	if(!INODE_DIR(&file->inode))
		return FS_ERR_NOTDIR;
	if(INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	err = fs_check_name(name);
	if(err)
		return err;
	char_width=FS_CHAR_WIDTH(name->fmt);
	for(i=0; i<name->size; i+=char_width)
	{
		if(name->str[i]==FS_PATH_CHAR)
			return FS_ERR_CHAR;
	}
	if((file->flag&FS_ATTR_W)==0)
	{
		return FS_ERR_RO;
	}
	old_offset = file->inode.offset;
	fat_read_seek(&file->inode, 0, FAT_SEEK_START);
	err = fat_read_dir(&file->inode, name->str, name->size, name->fmt, 
				&inode, NULL);
	file->inode.offset = old_offset;
	if(err)
	{
		if(err == FS_ERR_EOF)
			return FS_ERR_NOENT;
		return err;
	}
	if(fs_inode_used(&inode))
		return FS_ERR_BUSY;
	err = fat_del_inode(&inode);
	if(err)
		return err;
	return err;
}

int fs_dir_optimize(int fd, int free_num)
{
	int err;
	FS_FILE *file = fs_index2file(fd);
	inode_t *inode, child;
	_offset_t  offset;
	
	if(file==NULL)
		return FS_ERR_BADFD;
	if(!INODE_DIR(&file->inode))
		return FS_ERR_NOTDIR;
	if(!INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	inode = &file->inode;
	fat_read_seek(inode, 0, FAT_SEEK_START);
	while(0)
	{
		offset = inode->offset;
		err = fat_read_dir(inode, NULL, 0, 0, &child, NULL);
		if(err<0)
			break;
		
	}
	return FS_OK;
}

int FsFileRead(int fd, char *buf, int len)
{
	int err;
	FS_FILE *file = fs_index2file(fd);

	if(file==NULL)
		return FS_ERR_BADFD;
	if(INODE_DIR(&file->inode))
		return FS_ERR_ISDIR;
	if(INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	if(buf==NULL||len<=0)
		return FS_ERR_ARG;
	if((file->flag&FS_ATTR_R)==0)
	{
		return FS_ERR_WO;
	}
	err = fat_read_file(&file->inode, buf, len);
	return err;
}

int FsFileWrite(int fd, char *buf, int len)
{
	int tmpd,i;
	FS_FILE *file = fs_index2file(fd);

	if(file==NULL)return FS_ERR_BADFD;

	if(INODE_DIR(&file->inode))return FS_ERR_ISDIR;

	if(INODE_INV(&file->inode))return FS_ERR_NODEV;

	if(buf==NULL||len<=0)return FS_ERR_ARG;

	if((file->flag&FS_ATTR_W)==0)return FS_ERR_RO;

	i=0;
	while(i<len)
	{
		tmpd=fat_write_file(&file->inode,buf+i,len-i);
		if(tmpd<=0)return tmpd;

		i+=tmpd;
	}

	return i;
}

int FsFileTell(int fd)
{
	FS_FILE *file = fs_index2file(fd);
	if(file == NULL)
		return FS_ERR_BADFD;
	if(INODE_DIR(&file->inode))
		return FS_ERR_ISDIR;
	if(INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	return file->inode.offset.bytes;
}

int FsFileTruncate(int fd, int size, int reserve_space)
{
	int err;
	FS_FILE *file = fs_index2file(fd);
	inode_t *inode;
	int free_num;
	
	if(file==NULL)
		return FS_ERR_BADFD;
	if(INODE_DIR(&file->inode))
		return FS_ERR_ISDIR;
	if(INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	inode = &file->inode;
	if(size<0||(u32)size>inode->size)
	{
		return FS_ERR_ARG;
	}
	inode->size = size;
	err = fat_update_inode(inode, INODE_NODEL);
	if(err)
		goto out;
	fat_seek(inode, &inode->offset, size, FAT_SEEK_START);
	if(reserve_space>0)
	{
		u32 cluster_size = inode->drv->dwClusterSize;
		free_num = (reserve_space+cluster_size-1)/cluster_size;
	}
	else
		free_num = 1;
	err = fat_free_cluster(inode->drv, inode->offset.cluster, free_num);
out:
	fat_write_submit(inode->drv, err);
	return err;
}

int FsFileSeek(int fd, int offset, int flag)
{
	FS_FILE *file = fs_index2file(fd);
	inode_t *inode;
	int size;
	
	if(file == NULL)
		return FS_ERR_BADFD;
	if(INODE_DIR(&file->inode))
		return FS_ERR_ISDIR;
	if(INODE_INV(&file->inode))
		return FS_ERR_NODEV;
	inode = &file->inode;
	switch(flag)
	{
	case FS_SEEK_SET:
		if(offset<0)
			return FS_ERR_ARG;
		if((u32)offset>inode->size)
			offset = inode->size;
		fat_read_seek(inode, offset, FAT_SEEK_START);
		break;
	case FS_SEEK_CUR:
		size = inode->offset.bytes+offset;
		if(offset<0)
		{
			if(size<0)
			{
				return FS_ERR_ARG;
			}
			fat_read_seek(inode, size, FAT_SEEK_START);
		}else
		{
			if(size>(int)inode->size)
			{
				offset = inode->size-inode->offset.bytes;
			}
			fat_read_seek(inode, offset, FAT_SEEK_CUR);
		}
		break;
	case FS_SEEK_END:
		if(offset>0)
		{
			return FS_ERR_ARG;
		}
		offset = ((int)inode->size)+offset;
		if(offset<0)
			offset = 0;
		if((u32)offset<inode->offset.bytes)
		{
			fat_read_seek(inode, offset, FAT_SEEK_START);			
		}else
		{
			fat_read_seek(inode, offset-inode->offset.bytes, FAT_SEEK_CUR);
		}
		break;
	default:
		return FS_ERR_ARG;
	}
	return FS_OK;
}

int FsRename(FS_W_STR *old_name, FS_W_STR *new_name)
{
	int i, err, level;
	int char_width;
	inode_t inode;	

	/* check old_name */
	err = fs_check_name(old_name);
	if(err)
		return err;
	/* check new_name */
	err = fs_check_name(new_name);
	if(err)
		return err;
	char_width=FS_CHAR_WIDTH(new_name->fmt);
	for(i=0; i<new_name->size; i+=char_width)
	{
		if(new_name->str[i]==FS_PATH_CHAR)
			return FS_ERR_CHAR;
	}
	/* open inode */
	level = fs_path_open(old_name, FS_ATTR_RW);
	if(level<0)
	{
		return level;
	}
	if(level == 0)
	{
		//root inode can not rename
		return FS_ERR_NODEV;
	}

	if(!strcmp(parent_path[0].str,FAT_UDISK_DIR) && s_UsbHostState())return FS_ERR_NOINIT;
    if(!strcmp(parent_path[0].str,FAT_UDISK_A_DIR))
	{
		if(GetHWBranch() == D210HW_V2x && !base_usb_ohci_state) return FS_ERR_NOINIT;
		else if(!usb_host_ohci_state)return FS_ERR_NOINIT;
	}
    if(!strcmp(parent_path[0].str,FAT_SDCARD_DIR) && !get_sdcard_state())return FS_ERR_NOINIT;
	//if(GPIO1_IN_REG&BIT4)return FS_ERR_DEVPORT_OCCUPIED;//DEVICE port is attached to a host

	/*
	** Create New Inode
	**/
	memset(&inode, 0, sizeof(inode_t));
	inode.attr = parent_inode[level].attr;
	inode.size = parent_inode[level].size;
	inode.first_cluster = parent_inode[level].first_cluster;
	inode.first_sector = parent_inode[level].first_sector;
	err = fat_open_inode(&parent_inode[level-1], 
				new_name->str, new_name->size, new_name->fmt,
				TRUE, &inode);	
	if(err)
		goto out;
	/*
	** Del Old Inode
	**/
	err = fat_update_inode(&parent_inode[level], TRUE);
	if(err)
		goto out;

out:
	fat_write_submit(parent_inode[level].drv, err);
	return err;
}

int FsSetWorkDir(FS_W_STR *name)
{
	int i;
	char *str;
	u8   fmt;
	int size;
	int level;

	level = fs_path_open(name, FS_ATTR_D);
	if(level < 0)
	{
///ScrPrint(0,0,0,"setdir:%d",level);
		return level;
	}

	if(!strcmp(parent_path[0].str,FAT_UDISK_DIR) && s_UsbHostState())return FS_ERR_NOINIT;
   	if(!strcmp(parent_path[0].str,FAT_UDISK_A_DIR))
	{
		if(GetHWBranch() == D210HW_V2x && !base_usb_ohci_state) return FS_ERR_NOINIT;
		else if(!usb_host_ohci_state)return FS_ERR_NOINIT;
	}
    if(!strcmp(parent_path[0].str,FAT_SDCARD_DIR) && !get_sdcard_state())return FS_ERR_NOINIT;
	//if(GPIO1_IN_REG&BIT4)return FS_ERR_DEVPORT_OCCUPIED;//DEVICE port is attached to a host

	fmt = name->fmt;
	if(fs_relat_path(name))
	{
		fmt = name->fmt;
		if(name->fmt != wd_name.fmt)
		{
			//强制设置成Unicode
			fmt = NAME_FMT_UNICODE;
		}
	}else
	{
		fmt = name->fmt;
	}
	size = 0;
	for(i=0;i<=level; i++)
	{		
		if(parent_path[i].fmt == fmt)
		{
			if(parent_path[i].size+1>LONG_NAME_MAX)
				return FS_ERR_NAME_SIZE;
			size += parent_path[i].size+
				FS_CHAR_WIDTH(fmt);//this is for '/'
		}else
		{
			//fmt is unicode, but parent_path is ascii
			assert(fmt==NAME_FMT_UNICODE);
			assert(parent_path[i].fmt == NAME_FMT_ASCII);
			if(parent_path[i].size+1>LONG_NAME_MAX/2)
				return FS_ERR_NAME_SIZE;
			size += parent_path[i].size*2+
				2;//this is for '/'			
		}
	}
	if((fmt==NAME_FMT_UNICODE&&size>PATH_NAME_MAX)||
		(fmt==NAME_FMT_ASCII&&size>(PATH_NAME_MAX/2)))
	{
		return FS_ERR_NAME_SIZE;
	}
	str = wd_name.str+size;
	for(i=level; i>=0; i--)
	{
		if(parent_path[i].fmt == fmt)
		{
			char *buf = fat_str_tmp;
			str -= parent_path[i].size+
					FS_CHAR_WIDTH(fmt);//this is for '/'
			assert(str>=wd_name.str);
			memcpy(buf, "/", 2);
			memcpy(buf+FS_CHAR_WIDTH(fmt), 
				parent_path[i].str,
				parent_path[i].size);
			memcpy(str, buf, parent_path[i].size+
			FS_CHAR_WIDTH(fmt));
		}else
		{
			//fmt is unicode, but parent_path is ascii
			char *p=fat_ascii_to_UNICODE(parent_path[i].str, 
						parent_path[i].size);
			assert(fmt==NAME_FMT_UNICODE);
			assert(parent_path[i].fmt == NAME_FMT_ASCII);
			str -= parent_path[i].size*2+
				2;//this is for '/'	
			assert(str>=wd_name.str);
			memcpy(str, "/", 2);
			memcpy(str+2, p, parent_path[i].size*2);
		}
		
	}
	assert(str == wd_name.str);
	wd_name.str = str;
	wd_name.size = size;
	wd_name.fmt = fmt;
	return FS_OK;
}

int FsGetWorkDir(FS_W_STR *name)
{
	if(!strcmp(parent_path[0].str,FAT_UDISK_DIR) && s_UsbHostState())return FS_ERR_NOINIT;
    if(!strcmp(parent_path[0].str,FAT_UDISK_A_DIR))
	{
		if(GetHWBranch() == D210HW_V2x && !base_usb_ohci_state) return FS_ERR_NOINIT;
		else if(!usb_host_ohci_state)return FS_ERR_NOINIT;
	}
    if(!strcmp(parent_path[0].str,FAT_SDCARD_DIR) && !get_sdcard_state())return FS_ERR_NOINIT;
	//if(GPIO1_IN_REG&BIT4)return FS_ERR_DEVPORT_OCCUPIED;//DEVICE port is attached to a host

	if(name->size<wd_name.size+1)return wd_name.size;

	memcpy(name->str, wd_name.str, wd_name.size);
	name->size = wd_name.size;
	name->str[name->size] = 0;
	name->fmt = wd_name.fmt;
	return FS_OK;
}

void fs_fmt_rate(int rate)
{
	fmt_rate += rate;
	if(fmt_rate >= 100)
		fmt_rate = 100;
	if(fmt_cb)
	{
		fmt_cb(fmt_rate);
	}
}

int FsFormat(int disk_num, void (*cb)(int rate))
{
	DRIVE_INFO *drv;
	u32 jiffier = s_Get10MsTimerCount();//inet_jiffier;
	int err;

	if(disk_num<0||disk_num>=FS_MAX_NUMS)return FS_ERR_ARG;

	if((disk_num == FS_FLASH)||(disk_num==FS_SDCARD))
	{
		fmt_rate = 0;
		fmt_cb = cb;
		//err = nand_mount(1);
		fmt_cb = NULL;
		return FS_ERR_SYS;
	}

	if(s_UsbHostState())return FS_ERR_NOINIT;
	//if(GPIO1_IN_REG&BIT4)return FS_ERR_DEVPORT_OCCUPIED;//DEVICE port is attached to a host

	fs_mount_udisk();
	drv = sDriveInfo[disk_num];
	if(drv==NULL)return FS_ERR_NODEV;
	if(drv->bIsFAT32)
	{
		err = fat_format_32(drv, 1);
	}else
	{
		err = fat_format_16(drv);
	}
	fs_fmt_rate(100);
	return err;
}

int FsUdiskIn(void)
{
	int tmpd;

    if(s_UsbHostState())return FS_ERR_NOINIT;

	tmpd=fs_mount_udisk();
	if(tmpd)return FS_ERR_PORT_NOTOPEN;

	if(sDriveInfo[FS_UDISK]==NULL)
		return FS_ERR_NODEV;
    
	return FS_OK;
}

int FsGetDiskInfo_std(int disk_num, FS_DISK_INFO *disk_info)
{
	DRIVE_INFO *drv;
	FAT_SECTOR_CACHE *cache;
	u8 *buf;
	u32 cluster, sector, value;
	int free_num, err, i, dir_ent_size;
    u16 *buf_u16;
    u32 *buf_u32;

    switch(disk_num)
    {
    case FS_FLASH:
    case FS_UDISK:
    case FS_SDCARD:
    case FS_UDISK_A:
        break;
    default:return FS_ERR_ARG;
    }
	if(disk_info == NULL)return FS_ERR_ARG;

	memset(disk_info,0x00,sizeof(FS_DISK_INFO));

	if(disk_num == FS_UDISK)
	{
		if(s_UsbHostState())return FS_ERR_NOINIT;

		err = fs_mount_udisk();
        if(err)return FS_ERR_NODEV;
	}
    else if(disk_num == FS_UDISK_A)
	{
		if(!usb_host_ohci_state)return FS_ERR_NOINIT;

		err = ohci_fs_mount_udisk();
        if(err)return FS_ERR_NODEV;
	}    
    else if(disk_num==FS_SDCARD)
    {
        if(!get_sdcard_state())return FS_ERR_NOINIT;
    }
    else
    {
        return FS_ERR_NODEV;
    }

	drv = sDriveInfo[disk_num];
	if(drv == NULL)
		return FS_ERR_NODEV;
	free_num = 0;
	cluster = 0;    

    if(drv->bIsFAT32)
    {
        dir_ent_size = 4;
    	for(sector=drv->dwFAT1StartSector;
    		sector<(drv->dwFAT1StartSector+drv->dwFATSize)&&
    		cluster<(drv->dwDataClusters+2);
    		sector++)
    	{	    
    		err = fat_read(drv, sector, 1, &cache);
    		if(err<0)
    		{
    			if(sDriveInfo[disk_num]==NULL)
    				return FS_ERR_NODEV;
    			return err;
    		}
    		buf = FAT_BUF_P(cache);
    		for(i=0; 
    			i<drv->wSectorSize&&cluster<(drv->dwDataClusters+2); 
    			i+=dir_ent_size,
    			cluster++
    			)
    		{
                buf_u32 = (u32 *)(buf+i);
                if(((*buf_u32)&FAT32_CLUSTER_MASK)==0)free_num++;
    		}
    	}
        disk_info->ver = FS_VER_FAT32;
    }
    else
    {
        dir_ent_size = 2;
    	for(sector=drv->dwFAT1StartSector;
    		sector<(drv->dwFAT1StartSector+drv->dwFATSize)&&
    		cluster<(drv->dwDataClusters+2);
    		sector++)
    	{	    
    		err = fat_read(drv, sector, 1, &cache);
    		if(err<0)
    		{
    			if(sDriveInfo[disk_num]==NULL)
    				return FS_ERR_NODEV;
    			return err;
    		}
    		buf = FAT_BUF_P(cache);
    		for(i=0; 
    			i<drv->wSectorSize&&cluster<(drv->dwDataClusters+2); 
    			i+=dir_ent_size,
    			cluster++
    			)
    		{
                buf_u16 = (u16 *)(buf+i);
                if((*buf_u16)==0)free_num++;
    		}
    	}

		disk_info->ver = FS_VER_FAT16;
    }
    	
	disk_info->free_space = free_num*drv->dwClusterSize;
	disk_info->used_space = (drv->dwDataClusters-free_num)*drv->dwClusterSize;
	return FS_OK;
}

void fat_tbl_fix(int drv_num, int bFix)
{
	DRIVE_INFO *drv; 
	u32 sector;
	FAT_SECTOR_CACHE *cache;
	int err;
	char descr[50];
	
	fs_mount_udisk();
	drv = sDriveInfo[drv_num];
	if(drv==NULL)
	{
		s80_printf("No Such disk %d\n", drv_num);
		return;
	}
	sector = drv->dwFAT1StartSector;
	err = fat_read(drv, sector, 1, &cache);
	if(err)
	{
		fat_debug("fail to read sector[%d]\n", sector);
		return;
	}
	sprintf(descr, "FAT1 Sector[%d]", sector);
	fat_buf_debug(FAT_BUF_P(cache), drv->wSectorSize, descr);
	if(bFix==32)
	{
		DelayMs(200);
		fat_format_32(drv, 0);
		err = fat_read(drv, sector, 1, &cache);
		if(err)
		{
			fat_debug("fail to read sector[%d]\n", sector);
			return;
		}
		sprintf(descr, "FAT1 Sector[%d]", sector);
		fat_buf_debug(FAT_BUF_P(cache), drv->wSectorSize, descr);
	}
	return;
}

void fs_drv_show(int drv_num)
{
	DRIVE_INFO *drv; 
	
	fs_mount_udisk();
	drv = sDriveInfo[drv_num];
	if(drv==NULL)
	{
		s80_printf("No Such disk %d\n", drv_num);
		return;
	}
	fat_show_driv_info(drv);
}

char *fs_raw_read(int drv_num, int sector_num, int *p_size)
{
	FAT_SECTOR_CACHE *cache;
	DRIVE_INFO *drv;
	int err, i, j;
	u8 *buf;

	assert(drv_num>=0&&drv_num<=1);
	drv = sDriveInfo[drv_num];
	if(drv==NULL)
	{
		s80_printf("No such disk %d\n", drv_num);
		return NULL;
	}
	if(sector_num<0||(u32)sector_num>=drv->dwSectorTotal)
	{
		s80_printf("sector[%d] is illege!\n", sector_num);
		return NULL;
	}
	err = fat_read(drv, (u32)sector_num, 1, &cache);
	if(err)
	{
		s80_printf("read sector[%d],err=%d\n", 
			sector_num, err);
		return NULL;
	}
	s80_printf("Sector[%d]:\n", sector_num);
	buf = (u8*)FAT_BUF_P(cache);
	for(i=0; i<drv->wSectorSize; i+=16, buf+=16)
	{
		for(j=0; j<16; j++)
		{
			s80_printf("%02x,", buf[j]);
		}
		#ifndef WIN32
		for(j=0; j<16; j++)
		{
			if(buf[j]<' '||buf[j]>'~')
			{
				s80_printf(".");
				continue;
			}
			s80_printf("%c", buf[j]);
		}
		#endif
		s80_printf("\n");
	}
	s80_printf("\n");
	if(p_size)
		*p_size = drv->wSectorSize;
	return (u8*)FAT_BUF_P(cache);
}

int fs_raw_write(int drv_num, int sector_num, char *buf)
{
	FAT_SECTOR_CACHE *cache;
	DRIVE_INFO *drv;
	int err, i, j;

	assert(drv_num>=0&&drv_num<=1);
	drv = sDriveInfo[drv_num];
	if(drv==NULL)
	{
		s80_printf("No such disk %d\n", drv_num);
		return (int)NULL;
	}
	if(sector_num<0||(u32)sector_num>=drv->dwSectorTotal)
	{
		s80_printf("sector[%d] is illege!\n", sector_num);
		return (int)NULL;
	}
	cache = fat_buf_get(drv);
	assert(cache);
	memcpy(cache->cache, buf, drv->wSectorSize);
	err = fat_write(drv, (u32)sector_num, 1, cache);
	fat_write_submit(drv, err);
	return err;
}

void set_udisk_absent(void)
{
	sDriveInfo[FS_UDISK]=NULL;
}

void set_udisk_a_absent(void)
{
	sDriveInfo[FS_UDISK_A]=NULL;
}


//0xff means none usb
char fs_get_version(void)
{
	return USB_DRV_VER;
}

#if 0
void UdiskTest(void)
{
    unsigned char tmpc,fn,tmps[2100],xstr[2100];
	int tmpd,fd,rn,dn,wn,i,t0;
	FS_DISK_INFO udisk_info;
	FS_W_STR w_str,new_w_str;
	FS_INODE_INFO f_info;
    
    printk("\rUdiskTest 20120507_01\n");

    //while(UsbHostOpen(/*T_UDISK*/2));

    while(1)
    {
        tmpd = PortOpen(P_USB_HOST, "UDISK");
        DelayMs(1000);
        if(tmpd) printk("\rUsbHostOpen:%d\n",tmpd);
        else break;
    }
    printk("\rDetect a Udisk\n");

    tmpd = FsUdiskIn();
    if(tmpd) printk("\rFsUdiskIn failed!-%d\n",tmpd);
    
    tmpd=FsGetDiskInfo(0,&udisk_info);
	printk("\rFsGetDiskInfo RESULT:%d \n",tmpd);
	printk("\rVer:%d \n",udisk_info.ver);
	printk("\rFree:%d \n",udisk_info.free_space);
	printk("\rUsed:%d \n",udisk_info.used_space);


    //write test
    DelayMs(1000);
    
    strcpy(tmps,"/udisk/tmp.txt");

	w_str.str=tmps;
	w_str.size=strlen(w_str.str);
	w_str.fmt=2;
	fd=FsOpen(&w_str,FS_ATTR_CRW);
	printk("\rOPEN %s RESULT:%d \n",tmps,fd);

	for(i=0;i<40;i++)
	{
		memcpy(tmps+i*50,"000000123456789012345678901234567890ABCDEFABCDEF\r\n",50);
		sprintf(tmps+i*50,"%06d",i);
		tmps[i*50+6]='1';
	}
	printk("\rWRITING ... \n");
	t0=s_Get10MsTimerCount();
	for(i=0,wn=0;i<200;i++)
	{
		dn=FsFileWrite(fd,tmps,2000);
		printk("\r%d/200,WRITE RESULT:%d ",i,dn);
		if(dn!=2000)
		{
            printk("\n\rWrite dn!=2000\n");
            while(1);
			if(dn<=0)break;
		}
		if(dn>0)wn+=dn;
	}

	tmpd=s_Get10MsTimerCount()-t0;
	printk("\n\rFINISHED,TIME:%d \n",tmpd);
	if(tmpd)
	{
	tmpd=wn*100/tmpd;
	printk("\rSPEED:%d \n",tmpd);
	}
	FsClose(fd);

    //read test
    strcpy(tmps,"/udisk/tmp.txt");
	w_str.str=tmps;
	w_str.size=strlen(w_str.str);
	w_str.fmt=2;
	tmpd=FsOpen(&w_str,FS_ATTR_R);
	printk("\rOPEN RESULT:%d \n",tmpd);
	if(tmpd>=0)fd=tmpd;

	tmpd=FsGetInfo(fd,&f_info);
	printk("\rF_INFO:%d,flen:%d \n",tmpd,f_info.size);

	tmpd=FsFileSeek(fd,0,FS_SEEK_SET);
	printk("\rF_SEEK:%d \n",tmpd);
	printk("\rREADING ... \n");
	dn=0;
	t0=s_Get10MsTimerCount();
	while(1)
	{
		rn=FsFileRead(fd,tmps,2000);//1000
		if(rn<0)break;

		dn+=rn;
	}
	tmpd=s_Get10MsTimerCount()-t0;
	printk("\rFINISHED,TIME:%d \n",tmpd);

	if(tmpd)
	{
    	tmpd=dn*100/tmpd;
    	printk("\rSPEED:%d \n",tmpd);
	}
	FsClose(fd);
    while(1);
}
#endif

#ifdef UDISK_TEST_OPEN
void format_progress(int rate)
{
	ScrPrint(0,1,0," \%%d...",rate);
}

void UdiskTest(void)
{
	u8 tmpc,fn,tmps[2100],xstr[2100];
	int tmpd,fd,rn,dn,wn,i;
	FS_DISK_INFO udisk_info;
	FS_W_STR w_str,new_w_str;
	FS_INODE_INFO f_info;

	//sUsbHostInit();

	while(1)
	{
	ScrCls();
	ScrPrint(0,0*2,0,"UDISK TEST 110126A");
	ScrPrint(0,1*2,0,"0-IN     2-INFO");
	ScrPrint(0,2*2,0,"3-GETDIR  4-OPEN");
	ScrPrint(0,3*2,0,"5-READ    6-WRITE");
	ScrPrint(0,4*2,0,"7-CLOSE   8-SETDIR");
	ScrPrint(0,5*2,0,"9-DETECT  10-DELETE");
	ScrPrint(0,6*2,0,"11-RENAME 12-FORMAT");

	fn=getkey();
	if(fn=='1')fn=10+getkey()-'0';
	if(fn==KEYCANCEL)break;
	switch(fn)
	{
	case '9':
        /*
#define rUSB_STATUS         0x0d
		ScrCls();
		tmpc=r_reg(rUSB_STATUS);
		w_reg(rUSB_STATUS,tmpc);//clear status
		ScrPrint(0,7,0,"STATUS:%02X ",tmpc);
		getkey();
		*/
		break;
	case 12:
		ScrCls();
		ScrPrint(0,1*2,0,"FORMAT");
		tmpd=FsFormat(FS_UDISK,format_progress);
		ScrPrint(0,7*2,0,"RESULT:%d ",tmpd);
		getkey();
		break;
	case 11:
		ScrCls();

		ScrPrint(0,1*2,0,"RENAME:");
		ScrPrint(0,2*2,0,"1-tmp.txt->tmp.tmp");
		ScrPrint(0,3*2,0,"2-tmp.txt<-tmp.tmp");
		fn=getkey();
		ScrCls();

		if(fn=='1')w_str.str="/udisk/tmp.txt";
		else w_str.str="/udisk/tmp.tmp";
		w_str.size=strlen(w_str.str);
		w_str.fmt=2;

		if(fn=='1')new_w_str.str="tmp.tmp";
		else new_w_str.str="tmp.txt";
		new_w_str.size=strlen(new_w_str.str);
		new_w_str.fmt=2;
		tmpd=FsRename(&w_str,&new_w_str);
		ScrPrint(0,5*2,0,"RESULT:%d ",tmpd);
		getkey();
		break;
	case '0':
		ScrCls();
		tmpd=FsUdiskIn();
		ScrPrint(0,7*2,0,"RESULT:%d ",tmpd);
		getkey();
		break;
	case '2':
		ScrCls();
		tmpd=FsGetDiskInfo(0,&udisk_info);
		ScrPrint(0,1*2,0,"RESULT:%d ",tmpd);
		ScrPrint(0,2*2,0,"Ver:%d ",udisk_info.ver);
		ScrPrint(0,3*2,0,"Free:%d ",udisk_info.free_space);
		ScrPrint(0,4*2,0,"Used:%d ",udisk_info.used_space);
		getkey();
		break;
	case '3':
		ScrCls();
		memset(&w_str,0x00,sizeof(w_str));
		w_str.str=tmps;
		w_str.size=sizeof(tmps);
		tmpd=FsGetWorkDir(&w_str);
		ScrPrint(0,1*2,0,"RESULT:%d ",tmpd);
		ScrPrint(0,2*2,0,"Str:[%s]",w_str.str);
		ScrPrint(0,4*2,0,"Size:%d ",w_str.size);
		ScrPrint(0,5*2,0,"Fmt:%d ",w_str.fmt);
		getkey();
		break;
	case '8':
		ScrCls();
		memset(&w_str,0x00,sizeof(w_str));
		w_str.fmt=2;
		w_str.str=(char*)"/udisk/";
		w_str.size=7;
		tmpd=FsSetWorkDir(&w_str);
		ScrPrint(0,1*2,0,"RESULT:%d ",tmpd);
		getkey();
		break;
	case '4':
		ScrCls();

		//strcpy(tmps,"/udisk/1.mp3");
		//strcpy(tmps,"1.mp3");
		strcpy(tmps,"/udisk/1.bin");
		w_str.str=tmps;
		w_str.size=strlen(w_str.str);
		w_str.fmt=2;
		tmpd=FsOpen(&w_str,FS_ATTR_R);
		ScrPrint(0,1*2,0,"RESULT:%d ",tmpd);
		if(tmpd>=0)fd=tmpd;

		//ScrPrint(0,5,0,"device_state:%d ",device_state);
		getkey();
		break;
	case 10:
		ScrCls();

		strcpy(tmps,"/udisk/tmp.txt");
		w_str.str=tmps;
		w_str.size=strlen(w_str.str);
		w_str.fmt=2;
		tmpd=FsDelete(&w_str);
		ScrPrint(0,1*2,0,"RESULT:%d ",tmpd);

		//ScrPrint(0,5,0,"device_state:%d ",device_state);
		getkey();
		break;
	case '7':
		ScrCls();
		tmpd=FsClose(fd);
		ScrPrint(0,1*2,0,"CLOSE:%d ",tmpd);
		getkey();
		break;
	case '5':
		ScrCls();
		tmpd=FsUdiskIn();
		ScrPrint(0,0*2,0,"IN RESULT:%d ",tmpd);

		ScrPrint(0,1*2,0,"SELECT FILE:");
		ScrPrint(0,2*2,0,"1-1.mp3   2-1.bin");
		ScrPrint(0,3*2,0,"3-/1.mp3  4-/1.bin");
		ScrPrint(0,4*2,0,"5-/tmp.txt");
		ScrPrint(0,5*2,0,"6-/2222.pck");
		ScrPrint(0,6*2,0,"7-/tmp.txt/");
		fn=getkey();

		ScrCls();
		if(fn=='1')strcpy(tmps,"1.mp3");
		else if(fn=='2')strcpy(tmps,"1.bin");
		else if(fn=='3')strcpy(tmps,"/udisk/1.mp3");
		else if(fn=='5')strcpy(tmps,"/udisk/tmp.txt");
		else if(fn=='6')strcpy(tmps,"/udisk/2222.pck");
		else if(fn=='7')strcpy(tmps,"/udisk/tmp.txt/");
		else strcpy(tmps,"/udisk/1.bin");
		w_str.str=tmps;
		w_str.size=strlen(w_str.str);
		w_str.fmt=2;
		tmpd=FsOpen(&w_str,FS_ATTR_R);
		ScrPrint(0,1*2,0,"OPEN RESULT:%d ",tmpd);
		if(tmpd>=0)fd=tmpd;

		tmpd=FsGetInfo(fd,&f_info);
		ScrPrint(0,2*2,0,"F_INFO:%d,flen:%d ",tmpd,f_info.size);

		tmpd=FsFileSeek(fd,0,FS_SEEK_SET);
		ScrPrint(0,3*2,0,"F_SEEK:%d ",tmpd);
		ScrPrint(0,5*2,0,"READING ... ");
		PortOpen(0,"115200,8,n,1");
		dn=0;
		TimerSet(0,60000);
		while(1)
		{
			rn=FsFileRead(fd,tmps,2000);//1000
			//ScrPrint(0,5,0,"RESULT:%d,TN:%d ",rn,dn);
			//sprintf(tmps,"READ RESULT:%d.",rn);
			//PortSends(0,tmps,strlen(tmps));
			if(rn<0)break;

			//while(PortSends(0,tmps,rn));
			dn+=rn;
			//ScrPrint(0,5,0,"RESULT:%d,TN:%d ",rn,dn);
		}
		tmpd=60000-TimerCheck(0);
		ScrPrint(0,5*2,0,"FINISHED,TIME:%d ",tmpd);
		//sprintf(tmps,"T:%d.",tmpd);
		//PortSends(0,tmps,strlen(tmps));

		if(tmpd)
		{
		tmpd=dn*10/tmpd;
		ScrPrint(0,6*2,0,"SPEED:%d ",tmpd);
		}
		ScrPrint(0,7*2,0,"ANY KEY ... ");
		FsClose(fd);
		getkey();
		break;
	case '6':
		ScrCls();
		tmpd=FsUdiskIn();
		ScrPrint(0,0*2,0,"IN RESULT:%d ",tmpd);

		ScrPrint(0,2*2,0,"SELECT FILE:");
		ScrPrint(0,3*2,0,"1-/tmp.txt");
		ScrPrint(0,4*2,0,"2-/tmp.txt/");
		fn=getkey();
		ScrCls();
		if(fn=='2')strcpy(tmps,"/udisk/tmp.txt/");
		else strcpy(tmps,"/udisk/tmp.txt");

		w_str.str=tmps;
		w_str.size=strlen(w_str.str);
		w_str.fmt=2;
		fd=FsOpen(&w_str,FS_ATTR_CRW);
		ScrPrint(0,1*2,0,"OPEN %s RESULT:%d ",tmps,fd);

		for(i=0;i<40;i++)
		{
			memcpy(tmps+i*50,"000000123456789012345678901234567890ABCDEFABCDEF\r\n",50);
			sprintf(tmps+i*50,"%06d",i);
			tmps[i*50+6]='1';
		}
		ScrPrint(0,3*2,0,"WRITING ... ");
		TimerSet(0,60000);
		for(i=0,wn=0;i<200;i++)
		{
			dn=FsFileWrite(fd,tmps,2000);
			ScrPrint(0,4*2,0,"%d/200,WRITE RESULT:%d ",i,dn);
			if(dn!=2000)
			{
				Beep();
				getkey();
				if(dn<=0)break;
			}
			if(dn>0)wn+=dn;
		}

		tmpd=60000-TimerCheck(0);
		ScrPrint(0,6*2,0,"FINISHED,TIME:%d ",tmpd);
		if(tmpd)
		{
		tmpd=wn*10/tmpd;
		ScrPrint(0,7*2,0,"SPEED:%d ",tmpd);
		}
		FsClose(fd);
		getkey();
		break;
	case KEYCLEAR:
		ScrCls();
		tmpc=PortOpen(12,"Udisk");//P_USB_HOST
		ScrPrint(0,0*2,0,"OPEN RESULT:%d ",tmpd);

		tmpd=FsUdiskIn();
		ScrPrint(0,1*2,0,"IN RESULT:%d ",tmpd);

		strcpy(tmps,"/udisk/tmp.txt");
		w_str.str=tmps;
		w_str.size=strlen(w_str.str);
		w_str.fmt=2;
		tmpd=FsOpen(&w_str,FS_ATTR_R);
		ScrPrint(0,2*2,0,"OPEN RESULT:%d ",tmpd);
		if(tmpd>=0)fd=tmpd;

		tmpd=FsGetInfo(fd,&f_info);
		ScrPrint(0,3*2,0,"F_INFO:%d,flen:%d ",tmpd,f_info.size);

		while(11)
		{
		tmpd=FsFileSeek(fd,0,FS_SEEK_SET);
		ScrPrint(0,4*2,0,"F_SEEK:%d ",tmpd);
		ScrPrint(0,5*2,0,"READING ... ");
		PortOpen(0,"115200,8,n,1");
		dn=0;
		TimerSet(0,60000);
		while(111)
		{
			rn=FsFileRead(fd,tmps,2000);//1000
			ScrPrint(0,5*2,0,"RESULT:%d,TN:%d ",rn,dn);
			//sprintf(tmps,"READ RESULT:%d.",rn);
			//PortSends(0,tmps,strlen(tmps));
			if(rn<0)break;

			//while(PortSends(0,tmps,rn));
			dn+=rn;
			//ScrPrint(0,5,0,"RESULT:%d,TN:%d ",rn,dn);
		}//while(111)
		tmpd=60000-TimerCheck(0);
		ScrPrint(0,5*2,0,"FINISHED,TIME:%d ",tmpd);
		//sprintf(tmps,"T:%d.",tmpd);
		//PortSends(0,tmps,strlen(tmps));

		if(tmpd)
		{
		tmpd=dn*10/tmpd;
		ScrPrint(0,6*2,0,"SPEED:%d ",tmpd);
		}
		DelayMs(1000);
		}//while(11)
		ScrPrint(0,7*2,0,"ANY KEY ... ");
		FsClose(fd);
		getkey();
		break;
	}//switch(fn)

	}//while(1)
}
#endif
