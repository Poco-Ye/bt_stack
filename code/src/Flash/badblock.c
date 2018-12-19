#include "base.h"
#include "nand.h"
#include "..\file\filedef.h"


#define LIST_SIZE (DATA_START_NUM + DATA_BLOCK_NUM)  /*max is 1000*/
static uchar BadBlocks[LIST_SIZE];
#define BAD_LIST_FLAG  "BAD_LIST" 

int IsBadBlock(int block)
{
	return BadBlocks[block];
}

static int ReadBadBlockInfo()
{
	uint block,page,i,len,addr;
	uchar data[PAGE_SIZE],data_back[PAGE_SIZE], sha[20];

    memset(data_back,0,PAGE_SIZE);
    memset(BadBlocks, 0, sizeof(BadBlocks));
	for(block=BAD_INFO_END; block>=BAD_INFO_START; block--)
	{
		for(page=0;page<(BLOCK_SIZE/PAGE_SIZE);page++)
		{
            addr = (block * BLOCK_SIZE + page * PAGE_SIZE) * 2;
            umc_nand_page_rd(addr, data, PAGE_SIZE);
			if(memcmp(data,BAD_LIST_FLAG,strlen(BAD_LIST_FLAG))) continue;
            
			len = sizeof(BadBlocks) + strlen(BAD_LIST_FLAG);
			sha1(data, len, sha);
			if(memcmp(data+len, sha, sizeof(sha)) == 0)
			{
				memcpy(BadBlocks, data+strlen(BAD_LIST_FLAG), sizeof(BadBlocks));
				return 0;
			}
            for(i=0;i<PAGE_SIZE;i++)  data_back[i] |= data[i];
 		}
	}
    
    if(memcmp(data_back,BAD_LIST_FLAG,strlen(BAD_LIST_FLAG))) return -1;
    len = sizeof(BadBlocks) + strlen(BAD_LIST_FLAG);
    sha1(data_back, len, sha);
    if(memcmp(data_back+len, sha, sizeof(sha)) == 0)
    {
        memcpy(BadBlocks, data_back+strlen(BAD_LIST_FLAG), sizeof(BadBlocks));
        return 0;
    }
    
	return -2;
}

static int ProbeBadBlock(int block)
{
	uchar buf[PAGE_SIZE+64],buf0[PAGE_SIZE+64];
	uint addr;

	memset(buf0,0xff,sizeof(buf0));
	addr = (block * BLOCK_SIZE) * 2;
	umc_nand_page_rd(addr, buf, sizeof(buf));
	if(memcmp(buf, buf0, sizeof(buf0))) return -1;
    
	addr = ((block+1) * BLOCK_SIZE - PAGE_SIZE) * 2;
	umc_nand_page_rd(addr, buf, sizeof(buf));
	if(memcmp(buf, buf0, sizeof(buf0))) return -2;

    return 0;
}

// initial bad block list 
void BadBlockInit()
{
    uint block,page,len,addr;
	uchar data[PAGE_SIZE], sha[20];

    if(NAND_ID_HYNIX == GetNandType()) return ;
	if(ReadBadBlockInfo()== 0) return;

    memset(BadBlocks, 0, sizeof(BadBlocks));
    /*chack physical bad block*/
    for(block=DATA_START_NUM-16; block<(DATA_START_NUM + DATA_BLOCK_NUM); block++)
    {
        if(ProbeBadBlock(block)) BadBlocks[block] = 1; 
    }
    /*find good block to write bad block information*/
	for(block=BAD_INFO_END; block>=BAD_INFO_START; block--){
		if(IsBadBlock(block)==0) break;
	}
    if(block < BAD_INFO_START) block = BAD_INFO_START;
	nand_phy_erase(block);

    /*save bad block information*/
    memcpy(data, BAD_LIST_FLAG, strlen(BAD_LIST_FLAG));
    memcpy(data+strlen(BAD_LIST_FLAG), BadBlocks, sizeof(BadBlocks));
    len = sizeof(BadBlocks) + strlen(BAD_LIST_FLAG);
    sha1(data, len, sha);
    memcpy(data+len,sha,sizeof(sha));
    /*write 64 copies of bad block information*/
    for(page=0; page<(BLOCK_SIZE/PAGE_SIZE); page++) {
        addr = (block * BLOCK_SIZE + page * PAGE_SIZE) * 2;
        umc_nand_page_prg(addr, data, PAGE_SIZE);
    }
}



