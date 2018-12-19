#include "base.h"
#include "nand.h"
#include "..\file\filedef.h"


#define  PHY_WRITE_FLAG  "WRFG"
uint* cal_ecc(uint *datain);
int ecc_correct(uint *datain,uchar *dataout);
void s_CreateBlockMapTable(void);
void s_CheckFlash(void);




#define NAND_RET_OK		  		0   /*完全没有错误*/

#define NAND_WRITE_MAP_ERR  		-1001
#define NAND_CREATE_MAP1_ERR 	 	-1002
#define NAND_CREATE_MAP2_ERR  	-1003
#define NAND_WRITE_READ_ERR  		-1004
#define NAND_DATA_BLOCK_OVER  	-1005
#define NAND_PARA_ERROR  			-1006

/*映射块A,B使用的块的状态一直为RES_BLOCK_FREE*/
#define RES_BLOCK_FREE 0x1F00
#define RES_BLOCK_FREE_USED 0x1FA5
#define RES_BLOCK_USED 0x20F0
#define RES_BLOCK_BAD  0x300F
#define RES_BLOCK_MAP  0x45A5

static ushort block_map[DATA_START_NUM+DATA_BLOCK_NUM+FS_BLOCK_NUM]; 
#define  MAP_BLOCK_MAX  (FS_BLOCK_NUM+DATA_START_NUM+4) /*两个映射块中的最大块号*/
static uint g_FlashType;

#define  s_NandFail(){ScrCls();Lcdprintf("Nand Fail:%d!\n",__LINE__);while(1);}

uint nand_init(void)
{
	uint val;

	/*memif config*/
	val = 	( WIDTH_8 << 12 )	|  ( CHIPS_2  << 10 ) |
			( TYPE_NAND  <<  8 )	|   ( WIDTH_16 <<  4 ) |
			( CHIPS_3  <<  2 )		| ( TYPE_SRAM_NONMUXED  <<  0 ) ;
	writel(val,UMC_R_umc_memif_cfg_MEMADDR);

	umc_config_cs_type();
	/*config cs*/
	writel(0,UMC_R_umc_set_opmode_MEMADDR);
	// Direct Command
	// 25:23  :: 4 : CS0 on interface1 
	// 22:21  :: 2 : Update Regs
	// 20  :: 0 : Set cre 0
	// 19:00  :: 0 : dont care
	val = (0x04 <<  23) | (2 << 21);  // Update Reg Command
	writel(val,UMC_R_umc_direct_cmd_MEMADDR);
	
	umc_ecc_config(ECC_BYPASS);
	NAND_umc_set_cycles();
	umc_nand_init();
	s_CheckFlash();
 	return 0;
}

void nand_phy_erase(uint block_num)
{
	umc_nand_blk_erase(block_num);
	DelayMs(5);
	while(!((*(volatile uint *)UMC_R_NVSRAM_MEMC_STATUS_MEMADDR)&(1<<6)));
}
int s_nand_erase(uint block_num)
{
	if(block_num < DATA_START_NUM) return 0;
    if((GetNandType()!= NAND_ID_HYNIX) && IsBadBlock(block_num))return -1;
    nand_phy_erase(block_num);
	return 0;
}
 
int nand_phy_page_read(uint page_addr, uchar* dst_addr)
{
	uint i,j;
	uchar rbuf[(PAGE_SIZE+64)];
	int ret;
	uint log_addr;
	
	memset(dst_addr,0x00,PAGE_SIZE);
	if(page_addr/2 <DATA_START_NUM *BLOCK_SIZE) return NAND_PARA_ERROR;
	log_addr =block_map[LOG_ADDR/BLOCK_SIZE]*BLOCK_SIZE;
	if(((page_addr/2)>=log_addr) &&((page_addr/2) <(log_addr +BLOCK_SIZE)))
	{
		umc_nand_page_rd(page_addr,dst_addr,PAGE_SIZE);
		return NAND_RET_OK;
	}

	umc_nand_page_rd(page_addr, rbuf,PAGE_SIZE +64);
	for(i=PAGE_SIZE;i<(PAGE_SIZE+64);i++) 
	{
		if(rbuf[i] !=0xFF) break;
	}
	
	if(i ==PAGE_SIZE+64) /*该块未写入数据，刚擦除不进行ECC纠错*/
	{
		memcpy(dst_addr,rbuf,PAGE_SIZE);
		return 0;
	}
	ret = ecc_correct((uint *)rbuf, dst_addr);
	return ret;
}

uint nand_phy_page_write(uint page_addr, uchar* src_addr)
{
	uint i,j;
	uchar hash_buf[32];
	uint *ecc_buf;
	uint log_addr;

	if(page_addr/2 <DATA_START_NUM *BLOCK_SIZE) return NAND_PARA_ERROR;

	log_addr =block_map[LOG_ADDR/BLOCK_SIZE]*BLOCK_SIZE;
	if(((page_addr/2)>=log_addr) &&((page_addr/2) <(log_addr +BLOCK_SIZE)))
	{
		umc_nand_page_prg(page_addr,src_addr, PAGE_SIZE);
		return NAND_RET_OK;
	}
	ecc_buf = cal_ecc((uint *)src_addr);
	sha1(src_addr,PAGE_SIZE,hash_buf);
	memcpy(src_addr+PAGE_SIZE,ecc_buf,32);
	memcpy(&src_addr[(PAGE_SIZE+32)],hash_buf,HASH_SIZE);
	memcpy(&src_addr[(PAGE_SIZE+32+HASH_SIZE)],hash_buf,HASH_SIZE);

	umc_nand_page_prg( page_addr, src_addr, PAGE_SIZE + 64);
	return NAND_RET_OK;
}

static int puklevel_nand_phy_page_read(uint page_addr, uchar* dst_addr)
{
    uint i,j;
    uchar rbuf[(PAGE_SIZE+64)];
    int ret;

    if(page_addr/2 <(PUKLEVEL_START_NUM*BLOCK_SIZE)) return NAND_PARA_ERROR;
    umc_nand_page_rd(page_addr, rbuf,PAGE_SIZE +64);
    return ecc_correct((uint *)rbuf, dst_addr);
}

int s_puklevel_nand_read(uint offset, uchar *data, uint data_len)
{
    volatile uint i;
    uint start_sector, start_offset, end_offset,end_sector,blk_num,blk_info;
    uint number_of_sectors, leave_bytes, page_addr;
    uchar rbuf[PAGE_SIZE];
    int ret;

    if(offset+data_len >(PUKLEVEL_END*BLOCK_SIZE)) s_NandFail();

    start_sector = offset/PAGE_SIZE;
    start_offset = offset%PAGE_SIZE;
    if((start_offset + data_len)<=PAGE_SIZE) leave_bytes =data_len;
    else leave_bytes =PAGE_SIZE - start_offset;

    end_sector = (offset + data_len)/PAGE_SIZE;
    end_offset = (offset + data_len)%PAGE_SIZE;

    if (end_offset )  number_of_sectors = end_sector - start_sector + 1;
    else			   number_of_sectors = end_sector - start_sector;

    page_addr = start_sector * 2 * PAGE_SIZE;

    for( i = 0 ; i < number_of_sectors ; i++ )
    {
    if(i == 0)
    {
    /* offset is not the begining of the first page to read */
    //umc_nand_page_rd( cs, page_addr, dwidth, address_cycles,rbuf,PAGE_SIZE);
    ret = puklevel_nand_phy_page_read(page_addr, rbuf);
    if(ret < 0) return ret;
    memcpy(data, rbuf + start_offset,leave_bytes);
    data += leave_bytes;
    data_len -= leave_bytes;
    }
    else if(i == (number_of_sectors-1))
    {
    /* last page to read */
    ret = puklevel_nand_phy_page_read(page_addr, rbuf);
    if(ret <0) return ret;
    memcpy(data,rbuf,data_len);
    return 0;
    }
    else
    {
    /* pages between first and last pages to read */
    ret = puklevel_nand_phy_page_read(page_addr, data);
    if(ret < 0) return ret;
    //umc_nand_page_rd( cs, page_addr, dwidth, address_cycles, data,PAGE_SIZE);
    data += PAGE_SIZE;
    data_len -= PAGE_SIZE;
    }
    page_addr += 2 *PAGE_SIZE;
    }
    return 0;
}

static uint puklevel_phy_page_write(uint page_addr, uchar* src_addr)
{
	uint i,j;
	uchar hash_buf[32];
	uint *ecc_buf;

    if(page_addr/2 <(PUKLEVEL_START_NUM*BLOCK_SIZE)) return NAND_PARA_ERROR;

	ecc_buf = cal_ecc((uint *)src_addr);
	sha1(src_addr,PAGE_SIZE,hash_buf);
	memcpy(src_addr+PAGE_SIZE,ecc_buf,32);
	memcpy(&src_addr[(PAGE_SIZE+32)],hash_buf,HASH_SIZE);
	memcpy(&src_addr[(PAGE_SIZE+32+HASH_SIZE)],hash_buf,HASH_SIZE);

	umc_nand_page_prg( page_addr, src_addr, PAGE_SIZE + 64);
	return 0;
}
 int s_puklevel_nand_write(uint offset,uchar *data, uint data_len)
{
    volatile uint i;
    uint start_sector, start_offset, end_sector, end_offset;
    uint number_of_sectors, leave_bytes, page_addr;
    uchar rbuf[PAGE_SIZE + 64];
    int ret;

    if(offset+data_len >(PUKLEVEL_END *BLOCK_SIZE)) s_NandFail(); 
    start_sector = offset/PAGE_SIZE;
    start_offset = offset%PAGE_SIZE;
    if((start_offset + data_len)<=PAGE_SIZE) leave_bytes =data_len;
    else leave_bytes =PAGE_SIZE - start_offset;

    end_sector = (offset + data_len)/PAGE_SIZE;
    end_offset = (offset + data_len)%PAGE_SIZE;

    if (end_offset )  number_of_sectors = end_sector - start_sector + 1;
    else			   number_of_sectors = end_sector - start_sector;

    page_addr = start_sector * 2 * PAGE_SIZE;

    for( i = 0 ; i < number_of_sectors ; i++ )
    {
        memset(rbuf,0xff,sizeof(rbuf));
        if(i == 0) {
            /* offset is not the begining of the first page to write */
            memcpy(rbuf + start_offset, data,leave_bytes);
            puklevel_phy_page_write(page_addr, rbuf);

            data += leave_bytes;
            data_len -= leave_bytes;
        }
        else if(i == (number_of_sectors-1)) {
            /* last page to write */
            memcpy((void *)rbuf, data, data_len);
            puklevel_phy_page_write(page_addr, rbuf);
            return 0;
        }
        else {
            /* pages between first and last pages to write */
            memcpy(rbuf, data, PAGE_SIZE);
            ret=puklevel_phy_page_write(page_addr, rbuf);
            if(ret !=NAND_RET_OK) return ret;
            data += PAGE_SIZE;
            data_len -= PAGE_SIZE;
        }
        page_addr += PAGE_SIZE*2;
    }

    return 0;
}

 int s_nand_read(uint offset, uchar *data, uint data_len)
{
	volatile uint i;
	uint start_sector, start_offset, end_offset,end_sector,blk_num,blk_info;
	uint number_of_sectors, leave_bytes, page_addr;
	uchar rbuf[PAGE_SIZE];
	int ret,ret1=NAND_RET_OK;

	start_sector = offset/PAGE_SIZE;
	start_offset = offset%PAGE_SIZE;
	if((start_offset + data_len)<=PAGE_SIZE) leave_bytes =data_len;
	else leave_bytes =PAGE_SIZE - start_offset;
	
	end_sector = (offset + data_len)/PAGE_SIZE;
	end_offset = (offset + data_len)%PAGE_SIZE;

	if (end_offset )  number_of_sectors = end_sector - start_sector + 1;
	else		      number_of_sectors = end_sector - start_sector;

	page_addr = start_sector * 2 * PAGE_SIZE;
	
	for( i = 0 ; i < number_of_sectors ; i++ )
	{
		if(i == 0)
		{
			/* offset is not the begining of the first page to read */
			ret = nand_phy_page_read(page_addr, rbuf);
			if(ret <0) ret1=ret;
			memcpy(data, rbuf + start_offset,leave_bytes);
			data += leave_bytes;
			data_len -= leave_bytes;
		}
		else if(i == (number_of_sectors-1))
		{
			/* last page to read */
			ret = nand_phy_page_read(page_addr, rbuf);
			if(ret <0) ret1=ret;
			memcpy(data,rbuf,data_len);
            return ret1;
		}
		else
		{
			/* pages between first and last pages to read */
			ret = nand_phy_page_read(page_addr, data);
			if(ret <0) ret1=ret;
			data += PAGE_SIZE;
			data_len -= PAGE_SIZE;
		}
		page_addr += 2 *PAGE_SIZE;
	}

    return ret1;
}

 int s_nand_write(uint offset,uchar *data, uint data_len)
{
	volatile uint i;
	uint start_sector, start_offset, end_sector, end_offset;
	uint number_of_sectors, leave_bytes, page_addr;
	uchar rbuf[PAGE_SIZE + 64];
	int ret,ret1=NAND_RET_OK;
	

	start_sector = offset/PAGE_SIZE;
	start_offset = offset%PAGE_SIZE;
	if((start_offset + data_len)<=PAGE_SIZE) leave_bytes =data_len;
	else leave_bytes =PAGE_SIZE - start_offset;
		
	end_sector = (offset + data_len)/PAGE_SIZE;
	end_offset = (offset + data_len)%PAGE_SIZE;

	if (end_offset )  number_of_sectors = end_sector - start_sector + 1;
	else		      number_of_sectors = end_sector - start_sector;

	page_addr = start_sector * 2 * PAGE_SIZE;
	//umc_ecc_config(ECC_NOJUMP, ECC_RD_END, ECC_ENABLE, ECC_PAGE_2048); 

	for( i = 0 ; i < number_of_sectors ; i++ )
	{
		memset(rbuf,0xff,sizeof(rbuf));
		if(i == 0) {
			/* offset is not the begining of the first page to write */
			memcpy(rbuf + start_offset, data,leave_bytes);
			ret=nand_phy_page_write(page_addr, rbuf);
			if(ret !=NAND_RET_OK) ret1=ret;
			data += leave_bytes;
			data_len -= leave_bytes;
		}
		else if(i == (number_of_sectors-1)) {
			/* last page to write */
			memcpy((void *)rbuf, data, data_len);
			ret=nand_phy_page_write(page_addr, rbuf);
            if(ret !=NAND_RET_OK) ret1=ret;
			return ret1;
		}
		else {
			/* pages between first and last pages to write */
			memcpy(rbuf, data, PAGE_SIZE);
			ret=nand_phy_page_write(page_addr, rbuf);
			if(ret !=NAND_RET_OK) ret1=ret;
			
			data += PAGE_SIZE;
			data_len -= PAGE_SIZE;
		}
		page_addr += PAGE_SIZE*2;
	}
    return ret1;
}

/*file log list part*/
static uint g_LogBlock[LOG_INFO_END-LOG_INFO_START+1],g_LogBlockNum; // 文件日志所在块的编号
static int g_LogIndex;
#define RESERVE_LOG_NUM   20  /*供SaveFat时写日志用*/  
static int s_OtherForceWrite;    /*供SaveFat时写日志用，使得其可以将必需的日子写入LOG记录区*/  

void OtherForceWrite()
{
    s_OtherForceWrite =1;
}

static void OtherLogInit()
{
    uint block,i;
    /*find good blocks to write bad block information*/
    g_LogBlockNum=0;
    for(block=LOG_INFO_START,i=0; block<=LOG_INFO_END; block++){
        g_LogBlock[i++] = LOG_INFO_START;
        if(IsBadBlock(block)) continue;
        g_LogBlock[g_LogBlockNum++]=block;
    }
    g_LogIndex = 0;
    s_OtherForceWrite=0;
}

int OtherWriteLog(uint des, uchar *src, int len)
{
    uint block,page,i,addr,maxIndex;
    uchar buff[PAGE_SIZE];
    DATALOG log;

    memset(buff,0xFF,sizeof(buff));
    memset(log.buf,0xFF,sizeof(log.buf));
    maxIndex = ITEMS_PER_BLOCK*g_LogBlockNum-RESERVE_LOG_NUM;
    if(!s_OtherForceWrite && g_LogIndex > maxIndex) return -1;
    log.flag = LOG_TAG;
    log.offset = des;
    log.len = len;
    memcpy(log.buf,src,len);
    for(i=0;i<REWRITE_COPIES;i++){
        memcpy(buff+i*DATA_LOG_SIZE,(uchar*)&log,sizeof(log)); 
    }

    addr = g_LogBlock[g_LogIndex/ITEMS_PER_BLOCK] * BLOCK_SIZE;
    addr = (addr+ (g_LogIndex%ITEMS_PER_BLOCK) * PAGE_SIZE) * 2;
    umc_nand_page_prg(addr, buff,PAGE_SIZE);
    g_LogIndex++;

    return 0;
}

static  int GetPageItems(uchar *output,uchar *inbuf)
{
    uint addr,j,k;
    uchar *logptr;
    DATALOG log;
    logptr = (uchar*)&log;

    memset(logptr,0,sizeof(log));
    for(j=0;j<REWRITE_COPIES;j++){/*将所有备份数据对应bit进行或操作*/
        addr = j*DATA_LOG_SIZE; 
        for(k=0;k<DATA_LOG_SIZE;k++){
            logptr[k] |= inbuf[addr+k];
        }    
    }
    if(log.flag == LOG_TAG){ 
        memcpy(output+log.offset,log.buf,log.len);
    }
    if(log.flag == 0xFFFFFFFF) return -1; /*Read End*/
    else    g_LogIndex++;
    return 0; /*Success*/
}

int OtherReadLog(uint des, uchar *output, int len) /*该函数只能被文件系统初始化调用，否则会出现错误*/
{
    uint block,page,addr;
    uchar buf[PAGE_SIZE],*logptr;
    /*find good block to write bad block information*/
    g_LogIndex = 0; /*读取日志时总是从0日志索引开始*/
    memset(output,0xFF,len);
    for(block=0;block<g_LogBlockNum;block++){
        for(page=0;page<(BLOCK_SIZE/PAGE_SIZE);page++){
            addr = (g_LogBlock[block] * BLOCK_SIZE + page * PAGE_SIZE) * 2;
            umc_nand_page_rd(addr, buf, sizeof(buf));
            if(GetPageItems(output,buf)) return 0;            
        }
    }            
    return 0;
}
void OtherEraseLog(uint Addr)
{
    int i;
    g_LogIndex = 0;
    for(i=0;i<g_LogBlockNum;i++){
        if(g_LogBlock[i]<LOG_INFO_START || g_LogBlock[i]>LOG_INFO_END) continue;
        nand_phy_erase(g_LogBlock[i]);
    }
    s_OtherForceWrite=0;
}

/*--------------------------Api Part---------------------------------*/

void s_LoadBlockMapTable(void)
{
	int i,j,map_blockA,map_blockB;
	uchar page_bufA[0x1000],page_bufB[0x1000];
	uchar hash_buf[32];

	map_blockB=0xAA55;
	
	for(j=0,i=(DATA_BLOCK_NUM+DATA_START_NUM-1);i>=(FS_BLOCK_NUM+DATA_START_NUM);i--) //search from reserve block
	{
		s_nand_read(i*BLOCK_SIZE + DATA_MAGIC_OFFSET,page_bufA,sizeof(DATA_BLOCK_MAGIC));
		if(memcmp(page_bufA,DATA_BLOCK_MAGIC,sizeof(DATA_BLOCK_MAGIC))) continue;
		
		if(!j)	map_blockA =i;
		else 	map_blockB =i;	
		j++;
		if(j==2)break;
	}

	if(j==0)	
	{
		s_CreateBlockMapTable();  	/* 找不到两块映射块 */
		return ;
	}
	else if(j==1)	
	{
		s_nand_read(map_blockA*BLOCK_SIZE,page_bufA,sizeof(page_bufA));
		memset(page_bufB,0x12,sizeof(page_bufA));  /*填充垃圾数据，防止hash正确*/
		map_blockB =0xFFFFFF; /*标示B块为坏块*/
	}
	else
	{	
		s_nand_read(map_blockA*BLOCK_SIZE,page_bufA,sizeof(page_bufA));
		s_nand_read(map_blockB*BLOCK_SIZE,page_bufB,sizeof(page_bufB));
	}
	
	if(!memcmp(page_bufA,page_bufB,sizeof(block_map)+20)) /*value +hash ，A与B块内容相同 */
	{
		sha1(page_bufA,sizeof(block_map),hash_buf);
		if(memcmp(hash_buf,&page_bufA[sizeof(block_map)],20)) 
		{	
			s_CreateBlockMapTable(); /*两块内容均出现错误，挂死*/
			return;
		}
		memcpy(block_map,page_bufA,sizeof(block_map)); /*内容正确，直接填入块映射表*/
		return;
	}
	else /*最多只有一块有正确数据*/
	{
		sha1(page_bufA,sizeof(block_map),hash_buf);
		if(memcmp(hash_buf,&page_bufA[sizeof(block_map)],20)) /*A块数据内容不正确*/
		{
			if(map_blockB ==0xFFFFFF)
			{
				s_CreateBlockMapTable(); 
				return;
			}
			sha1(page_bufB,sizeof(block_map),hash_buf);
			if(memcmp(hash_buf,&page_bufB[sizeof(block_map)],20)) 
			{	
				s_CreateBlockMapTable(); 
				return;
			}
			memcpy(block_map,page_bufB,sizeof(block_map));	/*B块内容正确，直接填入块映射表*/
			
			return;
		}
		else	/*只有A块数据内容正确,A优先使用*/
		{
			memcpy(block_map,page_bufA,sizeof(block_map));/*A块内容正确，直接填入块映射表*/
			return;
		}

	}	
	
}

int s_WriteMapBlock(uint map_block)
{
	uchar page_buf_wr[RES_BLOCK_SIZE];
	uchar page_buf_rd[RES_BLOCK_SIZE];
	uchar hash_buf[32];
	
	memset(page_buf_wr,0,RES_BLOCK_SIZE);	
	/*生成sha1值*/
	sha1((uchar*)block_map,sizeof(block_map),hash_buf);
	memcpy(page_buf_wr,block_map,sizeof(block_map));
	memcpy(page_buf_wr+sizeof(block_map),hash_buf,sizeof(hash_buf));
	memcpy(page_buf_wr+DATA_MAGIC_OFFSET,DATA_BLOCK_MAGIC,sizeof(DATA_BLOCK_MAGIC));	

	s_nand_write(map_block*BLOCK_SIZE, page_buf_wr,RES_BLOCK_SIZE);
	s_nand_read(map_block*BLOCK_SIZE, page_buf_rd, RES_BLOCK_SIZE);

	if(memcmp(page_buf_wr,page_buf_rd,RES_BLOCK_SIZE)) return NAND_WRITE_MAP_ERR;
	return NAND_RET_OK;
}

int s_ReMapBlock(void)
{
	int i,j,ret,blockA_step,blockB_step,step2;
	uchar page_buf[RES_BLOCK_SIZE];
	//search from reserve block
	for(j=0,i=MAP_BLOCK_MAX-1;i>=(FS_BLOCK_NUM+DATA_START_NUM);i--) 
	{
		s_nand_read(i*BLOCK_SIZE + DATA_MAGIC_OFFSET,page_buf,sizeof(DATA_BLOCK_MAGIC));
		if(memcmp(page_buf,DATA_BLOCK_MAGIC,sizeof(DATA_BLOCK_MAGIC))) continue;
		
		if(!j)	blockA_step =i;
		else 	blockB_step =i;	
		j++;
		if(j==2)break;
	}

	if(j==0)
	{
		s_NandFail();
	}
	else if(j==1)
	{	
		/*先找后面一个新的Free块来写入映射信息，并将该块作为B块*/
		for(i=(blockA_step+1);i<MAP_BLOCK_MAX;i++) 
		{
			for(j=0;j<3;j++)
			{
				s_nand_erase(i);
				ret=s_WriteMapBlock(i);
				if(ret ==NAND_RET_OK) break;			
			}
			if(j<3) break;
			if(j>=3) s_nand_erase(i);
		}		
		if(j>=3) s_NandFail();
		step2 = i; /*B块使用的块号*/
		
		/*擦除有标志的A块，并写入映射信息*/
		for(j=0;j<3;j++)
		{
			s_nand_erase(blockA_step);
			ret=s_WriteMapBlock(blockA_step);
			if(ret ==NAND_RET_OK) return NAND_RET_OK;			
		}
		if(j>=3) s_nand_erase(blockA_step);

		/*从B块后，再找后面一块新的Free块来写A块的映射信息*/
		for(i=(step2+1);i<MAP_BLOCK_MAX;i++) 
		{
			for(j=0;j<3;j++)
			{
				s_nand_erase(i);
				ret=s_WriteMapBlock(i);
				if(ret ==NAND_RET_OK) break;			
			}
			if(j<3) break;
			if(j>=3) s_nand_erase(i);
		}		
		if(j>=3) s_NandFail();
	}
	else
	{
		/*擦除有标志的A块，并写入映射信息,不成功再找后面Free块来写映射信息*/
		for(i=blockA_step;i<MAP_BLOCK_MAX;i++) 
		{
			for(j=0;j<3;j++)
			{
				s_nand_erase(i);
				ret=s_WriteMapBlock(i);
				if(ret ==NAND_RET_OK) break;			
			}
			if(j<3) break;
			if(j>=3) s_nand_erase(i);
		}		
		if(j>=3) s_NandFail();
		step2 = i;
		
		/*擦除有标志的B块，并写入映射信息*/
		for(j=0;j<3;j++)
		{
			s_nand_erase(blockB_step);
			ret=s_WriteMapBlock(blockB_step);
			if(ret ==NAND_RET_OK) return NAND_RET_OK;			
		}
		if(j>=3) s_nand_erase(blockB_step);

		/*找后面Free块来写映射信息*/
		for(i=(step2+1);i<MAP_BLOCK_MAX;i++) 
		{
			for(j=0;j<3;j++)
			{
				s_nand_erase(i);
				ret=s_WriteMapBlock(i);
				if(ret ==NAND_RET_OK) break;			
			}
			if(j<3) break;
			if(j>=3) s_nand_erase(i);
		}		
		if(j>=3) s_NandFail();
	}
	return NAND_RET_OK;
}

void s_CreateBlockMapTable(void) /*被系统重建文件系统时调用*/
{
	int i,j,k,ret;

	/*建立预留域的信息*/
	for(j=0,i=FS_BLOCK_NUM+DATA_START_NUM;i<DATA_BLOCK_NUM+DATA_START_NUM;i++)
    {	 
		s_nand_erase(i);
		if(i<MAP_BLOCK_MAX) block_map[i] = RES_BLOCK_MAP;
		else block_map[i] = RES_BLOCK_FREE;	/*其余作为空闲块，可用于交换坏块*/		
	}
	/*建立数据域的信息*/
	for(i=0;i<DATA_START_NUM+FS_BLOCK_NUM;i++)
	{
		block_map[i] =i;  
	}
	/*建立数据域的状态映射表*/
	for(i=DATA_BLOCK_NUM+DATA_START_NUM;i<DATA_BLOCK_NUM+DATA_START_NUM+FS_BLOCK_NUM;i++)
	{
		block_map[i] =RES_BLOCK_USED;  
	}
	
	for(i=FS_BLOCK_NUM+DATA_START_NUM;i<MAP_BLOCK_MAX;i++)
    	{	 
		ret =s_WriteMapBlock(i);
		if(!ret) break;
		s_nand_erase(i);
		ret =s_WriteMapBlock(i);
		if(!ret) break;
		s_nand_erase(i);
		ret =s_WriteMapBlock(i);
		if(!ret) break;
		s_nand_erase(i);
	}
	if(i>=(MAP_BLOCK_MAX-1)) s_NandFail();	

	for(i=i+1;i<MAP_BLOCK_MAX;i++)
    	{	 
		ret =s_WriteMapBlock(i);
		if(!ret) break;
		s_nand_erase(i);
		ret =s_WriteMapBlock(i);
		if(!ret) break;
		s_nand_erase(i);
		ret =s_WriteMapBlock(i);
		if(!ret) break;
		s_nand_erase(i); /*写入不成功时将该MAP块擦除*/
	}
	if(i >=MAP_BLOCK_MAX) s_NandFail();

}


int nand_read(uint offset, uchar *data, uint data_len)
{
	uint start_block_num,end_block_num;
	uint block_phy_num;
	uint i,block_offset;
	int ret;

	if(offset+data_len > (DATA_START_NUM+FS_BLOCK_NUM)*BLOCK_SIZE) s_NandFail();
	
	start_block_num = offset/BLOCK_SIZE;
	end_block_num	= (offset+data_len)/BLOCK_SIZE + ((offset+data_len)%BLOCK_SIZE ?1:0);
	block_offset =offset % BLOCK_SIZE;
	
	for(i=start_block_num;i<end_block_num;i++)
	{
		block_phy_num=block_map[i];
		if(i==start_block_num)
		{
			if((data_len+block_offset) <=BLOCK_SIZE)
			{
				ret=s_nand_read(block_phy_num*BLOCK_SIZE +block_offset,data,data_len);
				return ret;
			}
			else
			{
				ret=s_nand_read(block_phy_num*BLOCK_SIZE +block_offset,data,BLOCK_SIZE-block_offset);
				if(ret <0) return ret;
				data_len -=BLOCK_SIZE-block_offset;
				data +=BLOCK_SIZE-block_offset;
				continue;				
			}
		}
		
		if(data_len >BLOCK_SIZE)
		{
			ret=s_nand_read(block_phy_num*BLOCK_SIZE,data,BLOCK_SIZE);
			if(ret <0) return ret;
			data_len -=BLOCK_SIZE;
			data +=BLOCK_SIZE;
			continue;				
		}
		else
		{
			ret=s_nand_read(block_phy_num*BLOCK_SIZE,data,data_len);
			return ret;
		}
	}
	return NAND_RET_OK;
	
}

void s_nandExchangeBlock(uint logic_block,uchar* block_buf)
{
	uint new_block_num;
	int i,j;
	
RE_EXCHANGE_BLOCK:
	/*原始的物理块已无法写入数据，需要标示成坏块*/
	for(i=(DATA_START_NUM+DATA_BLOCK_NUM-1);i>=(DATA_START_NUM+FS_BLOCK_NUM);i--)
	{
		if(i< MAP_BLOCK_MAX)
		{	
			if(s_Recycle_Free_block()>0) goto RE_EXCHANGE_BLOCK;
			s_NandFail(); /*可替换块已耗尽*/
		}
		if(block_map[i] != RES_BLOCK_FREE) continue;

		/*开始进行物理块交换*/
		new_block_num = i;
		block_map[logic_block] =i;
		block_map[i] =RES_BLOCK_USED;

		s_nand_erase(new_block_num);
		s_nand_read(new_block_num*BLOCK_SIZE,block_buf,BLOCK_SIZE);
		for(j=0;j<BLOCK_SIZE;j++) 
		{
			if(block_buf[j] !=0xFF) break;
		}
		if(j==BLOCK_SIZE)break;
		else block_map[i] =RES_BLOCK_BAD;
	}
	
	s_ReMapBlock(); /*将最新的映射信息写入到映射块中*/

}

int nand_erase(uint offset, uint size)
{
	uint start_block_num,end_block_num;
	uint block_phy_num;
	uint i,j;
	uchar block_buf[0x20000],ret;

	if(offset+size > (DATA_START_NUM+FS_BLOCK_NUM)*BLOCK_SIZE) s_NandFail();
	
	start_block_num = offset/BLOCK_SIZE;
	end_block_num	= (offset+size)/BLOCK_SIZE + ((offset+size)%BLOCK_SIZE ?1:0);

	for(i=start_block_num;i<end_block_num;i++)
	{
		block_phy_num=block_map[i];
        
		if(block_phy_num >(DATA_START_NUM +FS_BLOCK_NUM))
		{
			if(block_phy_num < MAP_BLOCK_MAX) 
			{
				s_NandFail(); /*可替换块已耗尽*/
			}
		}
		ret=s_nand_erase(block_phy_num); /*擦除对应的物理块*/
        if(ret) goto RECORD_BAD_BLOCK;
		s_nand_read(block_phy_num*BLOCK_SIZE,block_buf,BLOCK_SIZE);
		for(j=0;j<BLOCK_SIZE;j++) 
		{
			if(block_buf[j] !=0xFF) break;
		}
		if(j==BLOCK_SIZE)break;
		s_nand_erase(block_phy_num); /*擦除对应的物理块*/
		s_nand_read(block_phy_num*BLOCK_SIZE,block_buf,BLOCK_SIZE);
		for(j=0;j<BLOCK_SIZE;j++) 
		{
			if(block_buf[j] !=0xFF) break;
		}
		if(j==BLOCK_SIZE)break;
		s_nand_erase(block_phy_num); /*擦除对应的物理块*/
		s_nand_read(block_phy_num*BLOCK_SIZE,block_buf,BLOCK_SIZE);
		for(j=0;j<BLOCK_SIZE;j++) 
		{
			if(block_buf[j] !=0xFF) break;
		}
		if(j==BLOCK_SIZE)break;
RECORD_BAD_BLOCK:        
		/*擦三次都不成功，认为该块为坏块*/
		if(block_phy_num >(DATA_START_NUM+FS_BLOCK_NUM))block_map[block_phy_num] =RES_BLOCK_BAD;
		else block_map[block_phy_num+DATA_BLOCK_NUM] =RES_BLOCK_BAD;
		s_nandExchangeBlock(i,block_buf);
		
	}
	return NAND_RET_OK;
}

/*数据块与预留块的交换，仅由该函数完成*/
static int s_write_read(uint logic_block,uint offset,uchar *data, uint data_len)
{
	uchar block_buff[BLOCK_SIZE],page_buff[PAGE_SIZE+64],idle_page[BLOCK_SIZE/PAGE_SIZE];
	uint phy_block_num,new_block_num;
	int i,j,k,ret;
	
	if(data_len>BLOCK_SIZE) s_NandFail();

	phy_block_num =block_map[logic_block];
	s_nand_write(phy_block_num*BLOCK_SIZE+offset,data,data_len);
	ret=s_nand_read(phy_block_num*BLOCK_SIZE+offset,block_buff,data_len);
	if((ret ==NAND_RET_OK) || (ret ==0x11))
	{
		if(!memcmp(data,block_buff,data_len)) return NAND_RET_OK;
	}
    memset(idle_page,0,sizeof(idle_page));
    memcpy(block_buff+offset,data,data_len);
    
    for(i=0;i<BLOCK_SIZE;i+=PAGE_SIZE)
    {
        if(i>= offset && i< (offset+data_len)){
           idle_page[i/PAGE_SIZE] =1;
           continue;
        }
        ret=s_nand_read(phy_block_num*BLOCK_SIZE + i,block_buff+i,PAGE_SIZE);
        for(j=0;j<PAGE_SIZE;j++){
            if(block_buff[i+j] != 0xFF) break;
        }
        
        if(j!= PAGE_SIZE)  idle_page[i/PAGE_SIZE] =1;
        else{
            umc_nand_page_rd((phy_block_num*BLOCK_SIZE + i)*2, page_buff,PAGE_SIZE +64);
            for(k=PAGE_SIZE;k<(PAGE_SIZE+64);k++) 
            {
                if(page_buff[k] !=0xFF) {
                    idle_page[i/PAGE_SIZE] =1;
                    break;
                }
            }
        }
    }

RE_EXCHANGE_BLOCK:
	ret=s_reuse_data_block(logic_block,block_buff,idle_page);
	if(ret ==NAND_RET_OK) goto RESULT;

	/*原始的物理块已无法写入数据，需要标示成坏块*/
	for(i=(DATA_START_NUM+DATA_BLOCK_NUM-1);i>=(DATA_START_NUM+FS_BLOCK_NUM);i--)
	{
		if(i< MAP_BLOCK_MAX)
		{
			if(s_Recycle_Free_block() >0) goto RE_EXCHANGE_BLOCK;
			s_NandFail(); /*可替换块已耗尽*/
		}
		if(block_map[i] != RES_BLOCK_FREE) continue;

		/*开始进行物理块交换*/
		new_block_num = i;
		block_map[logic_block] =i;
		block_map[i] =RES_BLOCK_USED;
		for(j=0;j<3;j++)  /*对已分配的某块写三次*/
		{
			s_nand_erase(new_block_num);
            for(k=0;k<BLOCK_SIZE;k+=PAGE_SIZE){
                if(idle_page[k/PAGE_SIZE]==0) continue;
    			s_nand_write(new_block_num*BLOCK_SIZE+k,block_buff+k,PAGE_SIZE);
    			s_nand_read(new_block_num*BLOCK_SIZE+k,page_buff,PAGE_SIZE);
    			if(memcmp(page_buff,block_buff+k,PAGE_SIZE)) break;
            }
            if(k== BLOCK_SIZE) break;
		}
		if(j<3) break;
		if(j>=3) block_map[i] =RES_BLOCK_BAD;
	}
	
RESULT:	
	if(phy_block_num >(DATA_START_NUM+FS_BLOCK_NUM))
	{
		block_map[phy_block_num] =RES_BLOCK_FREE_USED; /*释放该逻辑块的前一次的替换块*/
	}
	else
	{
		block_map[DATA_BLOCK_NUM+phy_block_num] =RES_BLOCK_FREE_USED; /*释放该逻辑块的前一次的替换块*/
	}
	s_ReMapBlock(); /*将最新的映射信息写入到映射块中*/
	return NAND_RET_OK;
}

int s_Recycle_Free_block(void)
{
	int i,j,i_end;

	i_end =DATA_START_NUM+DATA_BLOCK_NUM+FS_BLOCK_NUM;
	for(j=0,i=DATA_START_NUM+FS_BLOCK_NUM;i<i_end;i++)
	{
		if(block_map[i] ==RES_BLOCK_FREE_USED)
		{	
			block_map[i] =RES_BLOCK_FREE;
			j++;
		}
	}
	return j;
}

int s_reuse_data_block(uint logic_block, uchar *data,uchar *idle_page)
{
	int i,j,k,new_block_num;
    uchar page_buff[PAGE_SIZE];
	for(i=(DATA_START_NUM+DATA_BLOCK_NUM);i<(DATA_START_NUM+DATA_BLOCK_NUM+FS_BLOCK_NUM);i++)
	{
		if(block_map[i] != RES_BLOCK_FREE) continue;

		/*开始进行物理块交换*/
		new_block_num = i-DATA_BLOCK_NUM;
		block_map[logic_block] =i-DATA_BLOCK_NUM;
		block_map[i] =RES_BLOCK_USED;
		for(j=0;j<3;j++)  /*对已分配的某块写三次*/
		{
			s_nand_erase(new_block_num);
            
            for(k=0;k<BLOCK_SIZE;k+=PAGE_SIZE){
                if(idle_page[k/PAGE_SIZE]==0) continue;
    			s_nand_write(new_block_num*BLOCK_SIZE+k,data+k,PAGE_SIZE);
    			s_nand_read(new_block_num*BLOCK_SIZE+k,page_buff,PAGE_SIZE);
    			if(memcmp(page_buff,data+k,PAGE_SIZE)) break;
            }
            if(k== BLOCK_SIZE) break;
		}
		if(j<3) return NAND_RET_OK;
		if(j>=3) block_map[i] =RES_BLOCK_BAD;
	}
	return NAND_DATA_BLOCK_OVER;
}

int nand_write(uint offset,uchar *data, uint data_len)
{
	uint start_block_num,end_block_num;
	uint logic_block,block_offset;
	int ret;

	if(offset+data_len > (DATA_START_NUM+FS_BLOCK_NUM)*BLOCK_SIZE) s_NandFail();
	
	start_block_num = offset/BLOCK_SIZE;
	end_block_num	= (offset+data_len)/BLOCK_SIZE + ((offset+data_len)%BLOCK_SIZE ?1:0);
	block_offset =offset % BLOCK_SIZE;
	
	for(logic_block=start_block_num;logic_block<end_block_num;logic_block++)
	{
		if(logic_block==start_block_num)
		{
			if((data_len+block_offset) <=BLOCK_SIZE)
			{
				ret=s_write_read(logic_block,block_offset,data,data_len);
				return ret;
			}
			else
			{
				ret=s_write_read(logic_block,block_offset,data,BLOCK_SIZE-block_offset);
				if(NAND_RET_OK != ret) return ret;
				data_len -=BLOCK_SIZE-block_offset;
				data +=BLOCK_SIZE-block_offset;
				continue;				
			}
		}
		
		if(data_len >BLOCK_SIZE)
		{
			ret=s_write_read(logic_block,0,data,BLOCK_SIZE);
			if(NAND_RET_OK != ret) return ret;
			data_len -=BLOCK_SIZE;
			data +=BLOCK_SIZE;
			continue;				
		}
		else
		{
			ret = s_write_read(logic_block,0,data,data_len);
			return ret;				
		}
	}
	return NAND_RET_OK;
	
}


void s_CheckFlash(void)
{
    g_FlashType=umc_nand_get_type();
    if(NAND_ID_HYNIX != GetNandType())BadBlockInit();
    if(NAND_ID_HYNIX != GetNandType())OtherLogInit(); 

	DATA_SECTORS = FS_BLOCK_NUM-3;  // 112 *1024/128K 

	BLOCK_PAGES=       BLOCK_SIZE/PAGE_SIZE; //每个数据扇区包含的数据块数
	LOG_SIZE  =       0x6000; //日志大小
	LOG_SECTOR_SIZE = BLOCK_SIZE;     //日志表所在扇区的大小

	FAT_SECTOR_SIZE=  BLOCK_SIZE;			      //FAT表所在扇区的大小
	DATA_SECTOR_SIZE =BLOCK_SIZE;               //数据扇区的大小

	FAT1_ADDR=	(DATA_START_NUM *BLOCK_SIZE);	  //FAT1表的地址
	FAT2_ADDR=	FAT1_ADDR + BLOCK_SIZE;	  //FAT2表的地址
	LOG_ADDR=  FAT2_ADDR+ BLOCK_SIZE;     //日志表的地址
	DATA_ADDR=LOG_ADDR +BLOCK_SIZE;     //数据扇区的起始地址
	s_LoadBlockMapTable();
}

uint GetNandType()
{
	return g_FlashType;
}


static int  is_bitodd(uint dt)
{
         dt=(dt>>16)^(dt);
         dt=(dt>>8)^(dt);
         dt=(dt>>4)^(dt);
         dt=(dt>>2)^(dt);
         dt=((dt>>1)^(dt))&1;

  return dt;
}

static int bit1_num(uint dt)
{
         int rsl=0;
         do {
                  if(dt & 1) rsl++;
                  dt>>=1;
         } while(dt);
         return rsl;
}

#define MATRIX_NUM  (128)  
uint* cal_ecc(uint *datain)
{
	int col,row;
	static uint ecc_buf[(MATRIX_NUM/32)*2];
	uint *ecc_col,*ecc_row;
	uint row_temp;

	memset(ecc_buf,0,sizeof(ecc_buf));
	ecc_col =ecc_buf;
	ecc_row =&ecc_buf[MATRIX_NUM/32];

	for(row=0;row<MATRIX_NUM;row++)
	{
		for(col=0,row_temp=0;col<MATRIX_NUM/(8*sizeof(uint));col++)
		{
			row_temp ^= *datain;
			ecc_col[col] ^= *datain++;
		}
		ecc_row[row/32] |= is_bitodd(row_temp)<<(row%32);
	}
	return ecc_buf;
}

/*PAGE DATA +OOB*/
/*OOB --32Bytes ECC,12Bytes sha1,same 12Bytes sha1,8Bytes reserve */
int ecc_correct(uint *datain,uchar *dataout)
{
	uchar hash_buf[32],*pt,*pch;
	int i,j;
	uint *ecc_buf;
	uint *ecc_col,*ecc_row;
	uint col_err_num,row_err_num,temp,temp2;
	int col_bitmap1,col_bitmap2,row_bitmap1,row_bitmap2;
	uint col_ecc_buf[4],row_ecc_buf[4];
	uint *pdatain[MATRIX_NUM];

	pt =(uchar*)&datain[(PAGE_SIZE+32)/4];

	for(i=0;i<HASH_SIZE;i++)       /*提取SHA1值*/
	{
	   if(pt[i] ==pt[i+HASH_SIZE]) continue;
	   for(j=0;j<8;j++)          /*出现某个字节不相同时，开始分析位*/
	   {
			if((pt[i] &(1<<j)) >(pt[i+HASH_SIZE]&(1<<j))) continue;
			pt[i] |=(1<<j); /*出现两个位不相同时，该位取1*/
	   }
	}
	
	sha1(datain,0x800,hash_buf);
	if(!memcmp(hash_buf,pt,HASH_SIZE)) 
	{
		memcpy(dataout,datain,PAGE_SIZE);
		return NAND_RET_OK;
	}

	ecc_buf =cal_ecc(datain);
	ecc_col =ecc_buf;
	ecc_row =&ecc_buf[MATRIX_NUM/32];

	col_err_num=0;
	row_err_num=0;

	for(i=0;i<4;i++) 
	{
		col_ecc_buf[i] = ecc_col[i] ^ datain[PAGE_SIZE/4 +i];
		row_ecc_buf[i] = ecc_row[i] ^ datain[(PAGE_SIZE+16)/4 +i];
		col_err_num += bit1_num(col_ecc_buf[i]);
		row_err_num += bit1_num(row_ecc_buf[i]);
	}

	if(col_err_num >2) return -0x30;
	if(row_err_num >2) return -0x03;
	

	col_bitmap1 =-1;
	col_bitmap2 =-1;
	row_bitmap1 =-1;
	row_bitmap2 =-1;
	pch =(uchar*)col_ecc_buf;
	for(i=0;i<MATRIX_NUM/8;i++) 
	{

		if(col_bitmap2 >0) break;
		if(!pch[i])continue;
		for(j=0;j<8;j++)
		{
			if((pch[i] & (1<<j)) == 0) continue;
			if(col_bitmap1 <0)	col_bitmap1=i*8+j;
			else
			{
				col_bitmap2=i*8+j;
				break;
			}
		}
	}
	pch =(uchar*)row_ecc_buf;
	for(i=0;i<MATRIX_NUM/8;i++) 
	{

		if(row_bitmap2 >0) break;
		if(!pch[i])continue;
		for(j=0;j<8;j++)
		{
			if((pch[i] & (1<<j)) == 0) continue;
			if(row_bitmap1 <0)	row_bitmap1=i*8+j;
			else
			{
				row_bitmap2=i*8+j;
				break;
			}
		}
	}

	if(col_err_num ==0 && row_err_num ==1) /*col-row: 0-1组合*/
	{
		for(i=0;i<MATRIX_NUM;i++)
		{	
			temp =datain[row_bitmap1*MATRIX_NUM/32 + i/32];
			datain[row_bitmap1*MATRIX_NUM/32 + i/32] ^= 1<<(i%32); 
			sha1(datain,PAGE_SIZE,hash_buf);
			if(!memcmp(hash_buf,pt,HASH_SIZE))
			{
				memcpy(dataout,datain,PAGE_SIZE);
				return 0x101;
			}
			datain[row_bitmap1*MATRIX_NUM/32 + i/32]=temp;
		}
		return -0x01;
	}

	if(col_err_num==1 && row_err_num ==0)  /*col-row: 1-0组合*/
	{

		for(i=0;i<MATRIX_NUM;i++)
		{	
			temp =datain[i*MATRIX_NUM/32 + col_bitmap1/32];
			datain[i*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
			sha1(datain,PAGE_SIZE,hash_buf);
			if(!memcmp(hash_buf,pt,HASH_SIZE))
			{
				memcpy(dataout,datain,PAGE_SIZE);
				return 0x110;
			}
			datain[i*MATRIX_NUM/32 + col_bitmap1/32]=temp;
		}
		return -0x10;
	}
	

	if(col_err_num==1 && row_err_num ==1) 	 /*col-row: 1-1组合*/
	{
		datain[row_bitmap1*MATRIX_NUM/32 +col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(memcmp(hash_buf,pt,HASH_SIZE)) return -0x11 ;
		memcpy(dataout,datain,PAGE_SIZE);
		return 0x11;
	}

	if(col_err_num==0 && row_err_num ==2)  /*col-row: 0-2 组合*/
	{

		for(i=0;i<MATRIX_NUM;i++)
		{	
			temp =datain[row_bitmap1*MATRIX_NUM/32 + i/32];
			temp2 =datain[row_bitmap2*MATRIX_NUM/32 + i/32];
			datain[row_bitmap1*MATRIX_NUM/32 + i/32] ^= 1<<(i%32); 
			datain[row_bitmap2*MATRIX_NUM/32 + i/32] ^= 1<<(i%32); 
			sha1(datain,PAGE_SIZE,hash_buf);
			if(!memcmp(hash_buf,pt,HASH_SIZE))
			{
				memcpy(dataout,datain,PAGE_SIZE);
				return 0x102;
			}
			datain[row_bitmap1*MATRIX_NUM/32 + i/32]=temp;
			datain[row_bitmap2*MATRIX_NUM/32 + i/32]=temp2;

		}
		return -0x02;
	}

	if(col_err_num==1 && row_err_num ==2)  /*col-row: 1-2 组合*/
	{
		temp =datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32];
		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(!memcmp(hash_buf,pt,HASH_SIZE))
		{
			memcpy(dataout,datain,PAGE_SIZE);
			return 0x112;
		}

		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32] =temp;
		temp =datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap1/32];
		datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(!memcmp(hash_buf,pt,HASH_SIZE))
		{
			memcpy(dataout,datain,PAGE_SIZE);
			return 0x112;
		}

		datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap1/32] =temp;

		for(i=0;i<MATRIX_NUM;i++)
		{	
			temp=datain[row_bitmap1*MATRIX_NUM/32 + i/32];
			temp2=datain[row_bitmap2*MATRIX_NUM/32 + i/32];
			datain[row_bitmap1*MATRIX_NUM/32 + i/32] ^= 1<<(i%32); 
			datain[row_bitmap2*MATRIX_NUM/32 + i/32] ^= 1<<(i%32); 
			sha1(datain,PAGE_SIZE,hash_buf);
			if(!memcmp(hash_buf,pt,HASH_SIZE))
			{
				memcpy(dataout,datain,PAGE_SIZE);
				return 0x112;
			}
			datain[row_bitmap1*MATRIX_NUM/32 + i/32]=temp;
			datain[row_bitmap2*MATRIX_NUM/32 + i/32]=temp2;

		}
		return -0x12;
	}


	if(col_err_num==2 && row_err_num ==0)  /*col-row: 2-0组合*/
	{

		for(i=0;i<MATRIX_NUM;i++)
		{	
			temp =datain[i*MATRIX_NUM/32 + col_bitmap1/32];
			temp2 =datain[i*MATRIX_NUM/32 + col_bitmap2/32];
			datain[i*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
			datain[i*MATRIX_NUM/32 + col_bitmap2/32] ^= 1<<(col_bitmap2%32); 
			sha1(datain,PAGE_SIZE,hash_buf);
			if(!memcmp(hash_buf,pt,HASH_SIZE))
			{
				memcpy(dataout,datain,PAGE_SIZE);
				return 0x120;
			}
			datain[i*MATRIX_NUM/32 + col_bitmap1/32]=temp;
			datain[i*MATRIX_NUM/32 + col_bitmap2/32]=temp2;

		}
		return -0x20;
	}
	
	if(col_err_num==2 && row_err_num ==1)  /*col-row: 2-1组合*/
	{
		temp =datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32];
		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(!memcmp(hash_buf,pt,HASH_SIZE))
		{
			memcpy(dataout,datain,PAGE_SIZE);
			return 0x121;
		}
		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32] =temp;

		temp =datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap2/32];
		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap2/32] ^= 1<<(col_bitmap2%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(!memcmp(hash_buf,pt,HASH_SIZE))
		{
			memcpy(dataout,datain,PAGE_SIZE);
			return 0x121;
		}

		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap2/32] =temp;

		for(i=0;i<MATRIX_NUM;i++)
		{	
			temp =datain[i*MATRIX_NUM/32 + col_bitmap1/32];
			temp2 =datain[i*MATRIX_NUM/32 + col_bitmap2/32];
			datain[i*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
			datain[i*MATRIX_NUM/32 + col_bitmap2/32] ^= 1<<(col_bitmap2%32); 
			sha1(datain,PAGE_SIZE,hash_buf);
			if(!memcmp(hash_buf,pt,HASH_SIZE))
			{
				memcpy(dataout,datain,PAGE_SIZE);
				return 0x121;
			}
			datain[i*MATRIX_NUM/32 + col_bitmap1/32]=temp;
			datain[i*MATRIX_NUM/32 + col_bitmap2/32]=temp2;

		}
		return -0x21;
	}

	if(col_err_num==2 || row_err_num ==2)  /*col-row: 2-2 组合*/
	{
		temp =datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32];
		temp2 =datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap2/32];
		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
		datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap2/32] ^= 1<<(col_bitmap2%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(!memcmp(hash_buf,pt,HASH_SIZE))
		{
			memcpy(dataout,datain,PAGE_SIZE);
			return 0x122;
		}

		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap1/32]=temp;
		datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap2/32]=temp2;

		datain[row_bitmap1*MATRIX_NUM/32 + col_bitmap2/32] ^= 1<<(col_bitmap2%32); 
		datain[row_bitmap2*MATRIX_NUM/32 + col_bitmap1/32] ^= 1<<(col_bitmap1%32); 
		sha1(datain,PAGE_SIZE,hash_buf);
		if(!memcmp(hash_buf,pt,HASH_SIZE))
		{
			memcpy(dataout,datain,PAGE_SIZE);
			return 0x122;
		}
		return -0x22;
	}

	return -0x1000;

}


#ifdef DEBUG_NAND
void test_nandflash()
{
	int blocks,j;
	uchar pages;
	uchar buff1[0x20000],buff2[0x20000];
	uchar ch;

	for(pages=0;pages<64;pages++)
	{
		for(j=0;j<PAGE_SIZE;j++) buff2[j+pages*PAGE_SIZE]=pages;
	}
	
//	DebugRecv();
	printk("input 0 erase all blocks, input 1 read all blocks \r\n");
	printk("input 2 write all blocks, input 3 write and read all blocks \r\n");
ch=0x31;
while(1)
{

	if(!DebugTstc()){	ch =DebugRecv(); printk("key value:%02x!\r\n",ch);}
	switch(ch)
	{
		case 0x30:
			for(blocks=DATA_START_NUM;blocks<DATA_START_NUM+DATA_BLOCK_NUM;blocks++)
			{
				printk("erase block num:%x!\r\n",blocks);
				s_nand_erase(blocks);
			}
			ch=0x32;
		break;
		case 0x32:
			for(blocks=DATA_START_NUM;blocks<DATA_START_NUM+DATA_BLOCK_NUM;blocks++)
			{
				printk("write block num:%x!\r\n",blocks);

				for(pages=0;pages<64;pages++)
				{
					//printk("write block num:%x,pages:%d,value:%02x!\r\n",blocks,pages,buff2[pages*PAGE_SIZE]);
					umc_nand_page_prg((blocks*BLOCK_SIZE+pages*PAGE_SIZE)*2,
						buff2+pages*PAGE_SIZE,PAGE_SIZE);
				}
			}
			ch=0x31;
			break;
		case 0x31:
			for(blocks=DATA_START_NUM;blocks<DATA_START_NUM+DATA_BLOCK_NUM;blocks++)
			{
				printk("read block num:%x!\r\n",blocks);
				for(pages=0;pages<64;pages++)
				{
					umc_nand_page_rd((blocks*BLOCK_SIZE+pages*PAGE_SIZE)*2,buff1,PAGE_SIZE);
					for(j=0;j<PAGE_SIZE;j++)
					{
						if(buff1[j] !=pages) 
						{
						printk("blocks:%0x,pages:%d,j:%02x,ch;%02x!\r\n",blocks,pages,j,buff1[j]);
						DebugRecv();
						}
					}
				}
			}
			ch=0x32;
			break;
			
	}
}	
}


void output_map()
{
	int i;
	s_LoadBlockMapTable();
	for(i=DATA_START_NUM;i<(sizeof(block_map)/2);i++)
	{
		if(i<(DATA_START_NUM+FS_BLOCK_NUM) ) 
		{
			if(block_map[i]!=i)
				printk("FS_BLOCK EXCHANGE %0x<-->%0x!\r\n",i,block_map[i]);
			continue;
		}
		if(i>=(DATA_START_NUM+DATA_BLOCK_NUM))
		{
			if(block_map[i]!=RES_BLOCK_USED)
				printk("FS_BLOCK %0x -->status:%x!\r\n",i-DATA_BLOCK_NUM,block_map[i]);
			continue;
		}
		printk("RES:%X!\r\n",block_map[i]);
			
	}
	DebugRecv();	
}
#endif



