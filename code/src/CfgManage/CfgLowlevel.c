#include "cfgmanage.h"
#include "base.h"
#include "..\flash\nand.h"

static uchar config_store_buff[BLOCK_SIZE];
#define CONFIG_STORE_ADDR	config_store_buff

#define KEYWORD_LEN  16   //每个关键字的长度
const unsigned char KeywordTable[][KEYWORD_LEN+1]={
"$(CFG_SN)       ",
"$(EXSN)         ",
"$(MACADDR)      ",
"$(CFG_FILE)     ",
"$(LCDGRAY)      ",
"$(UA_PUK)       ",
"$(US_PUK)       ",
"$(BEEP_VOL)	 ",
"$(KEY_TONE)	 ",
"$(RF_PARA) 	 ",
"$(US_PUK1)      ",
"$(US_PUK2)      ",
"$(CUSTOMER_SN)  ",
"$(TUSN)         ",
};

extern uint* cal_ecc(uint *datain);
extern int ecc_correct(uint *datain,uchar *dataout);
extern void sha1(unsigned char *src,unsigned int len,unsigned char *out);

static int search(uint block_num, uint *index);
static int cfg_nand_read(uint page_addr, uchar* dst_addr);
static uint cfg_nand_write(uint page_addr, uchar* src_addr);
static uint cfg_nand_erase(uint block_num);

/************************************************************************
函数原型：int ReadTerminalInfo(int id,uchar*context, int len)
功能：将index对应的内容写入FLASH，并且会对写入满时进行重新处理。
参数：id(input)   关键字的索引哈，
	  context(input) 关键字输出的缓存区，其大小不能小于len+1(包含结束符\x00)
	  len            context的长度
返回值：
	   >0  读取的长度
	   -1 序列号错误
	   -2 没有该索引号
	   -3 索引号对应的内容为空
************************************************************************/
int ReadTerminalInfo(int id,uchar*context, int len)
{
	int len1=-1;
	int iret;
	int total_vaild_key=0;
	int Key_num;
	unsigned int index[MAX_INDEX];
	unsigned char tmp_buf[PAGE_SIZE];

	memset(index, 0xff, MAX_INDEX*4);	
	Key_num=sizeof(KeywordTable)/sizeof(KeywordTable[0]);  //待查找的Keyword的总数

	if((id>=Key_num) || (len <1)) return -1;  //不支持此INDEX查询		
	iret=Search_Keyword(index);
	if(!iret) return -2; //没有内容
	if(index[id] == 0xffffffff) return -3; //该配置项没内容

	memcpy(tmp_buf, (uchar*)(CONFIG_STORE_ADDR+index[id]*PAGE_SIZE), PAGE_SIZE);
	
	len1=tmp_buf[KEYWORD_LEN];
	len1+=tmp_buf[KEYWORD_LEN+1]*256;
	if (len1 > len) len1 = len;
	memcpy(context,tmp_buf+KEYWORD_LEN+2,len1);
	context[len1]=0;
	return len1;
}


/************************************************************************
函数原型：int Search_Keyword(unsigned int *index)   
功能：查找关键字，将有效关键字项的信息放到SDRAM
	          (有效关键字:该关键字在三个扇区中至少在两个扇区中是一样的)
参数：index(output)  存放关键字在SDRAM中的位置，
        		index应该为一个数组，数组下标为MAX_INDEX,
返回值：
	   0 表示没有有效的关键字
	   非0表示有效关键字的个数
************************************************************************/
int Search_Keyword(uint *index)
{
    int i,j,indexi,Key_num,vaild_key_num = 0;
    int len[CONFIG_BLOCK_NUM]={-1};
    uint offset, index_tmp[CONFIG_BLOCK_NUM][MAX_INDEX];
    uchar tmpbuf[CONFIG_BLOCK_NUM][PAGE_SIZE];

    memset(index_tmp, 0xff, CONFIG_BLOCK_NUM*MAX_INDEX*4);
    memset(tmpbuf, 0x00, CONFIG_BLOCK_NUM*PAGE_SIZE);
    
    Key_num=sizeof(KeywordTable)/sizeof(KeywordTable[0]);  //待查找的Keyword的总数

    //查找关键字项在各块中的页码
    for (i = 0; i < CONFIG_BLOCK_NUM; i++)
    {
        j = search((i + CONFIG_BASE_ADDR/BLOCK_SIZE), index_tmp[i]);
    }

    //将关键字KeywordTable[indexi]的值及内容拷贝到内存中
    for (indexi = 0; indexi < Key_num; indexi++)
    {
        j = 0;
        for (i = 0; i < CONFIG_BLOCK_NUM; i++)
        {           
            if(index_tmp[i][indexi] == 0xffffffff) continue;            
            offset = CONFIG_BASE_ADDR+i*BLOCK_SIZE+index_tmp[i][indexi]*PAGE_SIZE;
            if (cfg_nand_read(offset, tmpbuf[i])) continue;         
            j++;
        }
        if (j < 2) continue;//关键字KeywordTable[indexi]少于两份
        
        /*将两份一样的关键字信息写入DDR中*/
        for (i = 0; i < PAGE_SIZE; i++)
        {
            if (tmpbuf[0][i] == tmpbuf[1][i]) continue;
            if (tmpbuf[0][i] == tmpbuf[2][i]) continue;
            if (tmpbuf[1][i] == tmpbuf[2][i])
            {
                tmpbuf[0][i] = tmpbuf[1][i];
                continue;
            }
            tmpbuf[0][i] |= tmpbuf[1][i];
        }
        memcpy((uchar*)(CONFIG_STORE_ADDR+vaild_key_num*PAGE_SIZE), tmpbuf[0], PAGE_SIZE);
        index[indexi] = vaild_key_num;
        vaild_key_num++;        
    }

    return vaild_key_num; 
}


/************************************************************************
函数原型：int search(uint block_num, uint *index)   
功能：查找块block_num中各个关键字对应的有效内容的页号(0~63)
参数：index(output)  存放关键字在flash 块中的页码(0~63)，
        		index应该为一个数组，数组下标为MAX_INDEX,
        		存放KeywordTable中对应下标的关键字所存放的页号(0~63)
返回值：
	   0 表示没有有效的关键字
	   非0表示有效关键字的个数
************************************************************************/
static int search(uint block_num, uint *index)
{
	int i,j,len,indexi;
	unsigned char*pt;
	unsigned char lrc;
	int Key_num, ret;
	int total_vaild_key=0;
	unsigned char tmp_buf[PAGE_SIZE];
	unsigned int offset = 0;
	
	pt=tmp_buf;
	Key_num=sizeof(KeywordTable)/sizeof(KeywordTable[0]);  //待查找的Keyword的总数

	offset = block_num*BLOCK_SIZE;
	for(i=CONFIG_PAGE_NUM-1; i>=0; i--)
	{
		memset(pt, 0x00, PAGE_SIZE);
		ret = cfg_nand_read((unsigned int)(offset+i*PAGE_SIZE), pt);
		if (ret) continue;
		if('$'!=pt[0]) continue;
		for (indexi=0;indexi<Key_num;indexi++)
		{		
			if(index[indexi] != 0xffffffff) 
				continue;	 //已经找到了记录的，就不在寻找
			if(!memcmp((const unsigned char* )pt,KeywordTable[indexi],KEYWORD_LEN)) 
				break;	//找到了关键字，退出循环
		}
		if(indexi==Key_num) continue;
		
		len=pt[KEYWORD_LEN];
		len+=pt[KEYWORD_LEN+1]*256;
		if(len>PAGE_SIZE) continue;
		for(j=0,lrc=0;j<len;j++)
		{
			lrc^=pt[KEYWORD_LEN+2+j];
		}
		if(lrc==pt[KEYWORD_LEN+2+len])
		{
			index[indexi]=(unsigned int)(i);  //找到了对应的索引号，并将页码存放在index中
			total_vaild_key++;
		}		
	}

	return total_vaild_key;
}

/************************************************************************
函数原型：uint search_blank_page() 
功能：从当前配置块中找到空白页，仅被WriteToFlash函数调用
返回值：
			0~CONFIG_PAGE_NUM-1:FLASH的空白页的页码
			CONFIG_PAGE_NUM:	 当前配置块中没有空白页
************************************************************************/
static unsigned int search_blank_page(uint block_num)
{
	int i;
	int ret;
	unsigned char tmp_buf[PAGE_SIZE];
	uint offset;

	offset = BLOCK_SIZE*block_num;
	for(i=CONFIG_PAGE_NUM-1; i>=0; i--) //从FLASH的最后page 查找 
	{
		ret = cfg_nand_read((unsigned int)(offset+i*PAGE_SIZE), tmp_buf);
		if (ret) continue;

		if ('$'==tmp_buf[0]) return (unsigned int)(i+1); 
	}
	
	return 0; 
}

/************************************************************************
函数原型：int WriteToFlash(int index,unsigned char *context,int len)   
功能：将index对应的内容写入FLASH，仅被WriteTerminalInfo函数调用
参数：index(input)   关键字的索引号，
	  context(input) 关键字对应的内容
	  len            context的长度
返回值：
	   0  表示成功
	   -1 序列号错误
	   -2 长度超出有效长度
	   -3 FLASH没有足够的空间
************************************************************************/
int WriteToFlash(int index,unsigned char *context,int len)
{
	int i, j;
	int ret, Key_num, failed_flag = 0;
	unsigned int templen;
	unsigned int block_addr, blank_page;
	unsigned char temp_buf[PAGE_SIZE+64];
	unsigned char lrc;	
	unsigned char *puc_buf;
	
	puc_buf=temp_buf;
	Key_num=sizeof(KeywordTable)/sizeof(KeywordTable[0]);  //待查找的Keyword的总数

	if(index >= Key_num)
		return -1;				//序列号错误

	templen=KEYWORD_LEN+2+len+1;   //算出记录项需要的FLASH长度。
	if(templen>PAGE_SIZE)//每个单项都不能超过每页的大小。
		return -2;  //写入长度错误
			
	memcpy(puc_buf,KeywordTable[index],KEYWORD_LEN);
	memcpy(puc_buf+KEYWORD_LEN,&len,2);
	memcpy(puc_buf+KEYWORD_LEN+2,context,len);
	for(i=0,lrc=0;i<len;i++)
	{
		lrc^=context[i];
	}
	puc_buf[KEYWORD_LEN+2+len]=lrc;  

	for (i = 0; i < CONFIG_BLOCK_NUM; i++)
	{		
		block_addr = CONFIG_BASE_ADDR + i*BLOCK_SIZE;

		blank_page = search_blank_page(CONFIG_BASE_ADDR/BLOCK_SIZE + i);
//printk("*********%s__%d****index:%dblank:%d,page:%d******\r\n", __FUNCTION__, __LINE__, index,i, blank_page);//lanwq		
		if(blank_page == CONFIG_PAGE_NUM)	return -3;	//空间不足

		for (j = blank_page; j < CONFIG_PAGE_NUM; j++)
		{
			//当前页写不正确，则写块中下一页
			if (!cfg_nand_write((block_addr + PAGE_SIZE*j), puc_buf))
				break;
		}
		if (j == CONFIG_PAGE_NUM) failed_flag++;
//printk("*********%s__%d****index:%dblank:%d,page:%d,failed_flag:%d******\r\n", __FUNCTION__, __LINE__, index,i, blank_page,failed_flag);//lanwq		
	}	

	if (failed_flag > 1) return -3;
	
	return 0;
}

/************************************************************************
函数原型：int WriteTerminalInfo(int index,unsigned char *context,int len)  
功能：将index对应的内容写入FLASH，并且会对写入满时进行重新处理。
参数：index(input)   关键字的索引哈，
	  context(input) 关键字对应的内容
	  len            context的长度
返回值：
	   0  表示成功
	   -1 序列号错误
	   -2 长度超出有效长度
	   -4 写FLASH失败
************************************************************************/
int WriteTerminalInfo(int index,unsigned char *context,int len)
{
	int iret,i,j,z;
	int vaild_key_num = 0, failed_flag = 0;
	int block_addr;
	unsigned int index_buf[MAX_INDEX];
	unsigned char tmp_buf[PAGE_SIZE+64];
	unsigned char lrc;
	uchar *pt;

	memset(index_buf, 0xff, MAX_INDEX*4);
	memset(tmp_buf, 0x00, PAGE_SIZE+64);
	iret=Search_Keyword(index_buf);	
	if (!iret)
	{	
		//若无记录项，先擦除
		for (j = 0; j < CONFIG_BLOCK_NUM; j++)
		{
			cfg_nand_erase(CONFIG_BASE_ADDR/BLOCK_SIZE + j);
		}
	}
	
	//如果写入的与最新的记录相同，不再写入	
	if(index_buf[index]!= 0xffffffff)
	{
		memcpy(tmp_buf,(uchar*)(CONFIG_STORE_ADDR+index_buf[index]*PAGE_SIZE), PAGE_SIZE);
		if ((len==(tmp_buf[KEYWORD_LEN+1]*256+tmp_buf[KEYWORD_LEN])) 
			&& (!memcmp((unsigned char*)(tmp_buf+KEYWORD_LEN+2), context, len)))
			return 0;
	}
	

	iret=WriteToFlash(index,context,len);	
	if(iret!=-3) return iret; 	
	//printk("*********%s__%d**flash full********\r\n", __FUNCTION__, __LINE__);//lanwq
	//当写入时缓冲区满时，需要擦除扇区再写		
	memset(index_buf, 0xff, MAX_INDEX*4);
	iret=Search_Keyword(index_buf);

	for (z = 0; z < CONFIG_BLOCK_NUM; z++)
	{			
		cfg_nand_erase(CONFIG_BASE_ADDR/BLOCK_SIZE + z);
		
		j = 0;
		pt = (uchar*)(CONFIG_STORE_ADDR);
		block_addr = CONFIG_BASE_ADDR + z*BLOCK_SIZE;
		
		for(i=0;i<MAX_INDEX;i++)  
		{
			if((index_buf[i] == 0xffffffff) || (i==index)) continue; //不拷贝不存在的记录和本次需要更新的记录
		
			//从内存中拷贝KeywordTable[i] 至临时缓存
			memset(tmp_buf, 0x00, PAGE_SIZE);
			memcpy(tmp_buf, pt+index_buf[i]*PAGE_SIZE, PAGE_SIZE);
			
			while (j < CONFIG_PAGE_NUM)
			{
				//当前页写不正确，则写块中下一页
				iret = cfg_nand_write((block_addr+j*PAGE_SIZE), tmp_buf);
				j++;
				if (!iret) break;
			}		
		}
		
		
		//将新写入的数据放入缓存区
		memset(tmp_buf, 0x00, PAGE_SIZE);
		memcpy(tmp_buf,KeywordTable[index],KEYWORD_LEN);
		memcpy(tmp_buf+KEYWORD_LEN,&len,2);
		memcpy(tmp_buf+KEYWORD_LEN+2,context,len);		
		for(i=0,lrc=0;i<len;i++)
		{
			lrc^=context[i];
		}
		tmp_buf[KEYWORD_LEN+2+len]=lrc;
		
		//写入新的记录
		while (j < CONFIG_PAGE_NUM)
		{
			if (!cfg_nand_write((block_addr+j*PAGE_SIZE), tmp_buf))
				break;
			j++;
		}
		
		if (j == CONFIG_PAGE_NUM) failed_flag++;
	
	}
	
	if (failed_flag > 1)
	{
		return -4;//新记录写入FLASH失败			
	}

	return 0;	
}

static int cfg_nand_read(uint page_addr, uchar* dst_addr)
{
	uint i,j;
	uchar rbuf[(PAGE_SIZE+64)];
	int ret;
	if(page_addr<CONFIG_BASE_ADDR || page_addr > CONFIG_END_ADDR) return 0;

	umc_nand_page_rd(page_addr*2, rbuf,PAGE_SIZE +64);
	for(i=PAGE_SIZE;i<(PAGE_SIZE+64);i++) 
	{
		if(rbuf[i] !=0xFF) break;
	}

	if(i ==PAGE_SIZE+64) /*空块不进行ecc纠错*/
	{
		memcpy(dst_addr,rbuf,PAGE_SIZE);
		return 0;
	}
	
	return ecc_correct((uint *)rbuf, dst_addr);
}

static uint cfg_nand_write(uint page_addr, uchar* src_addr)
{
	uint i,j;
	uchar hash_buf[32];
	uint *ecc_buf;
	uchar rbuf[(PAGE_SIZE+64)];

	if(page_addr<CONFIG_BASE_ADDR || page_addr > CONFIG_END_ADDR) return 0;

	ecc_buf = cal_ecc((uint *)src_addr);
	sha1(src_addr,PAGE_SIZE,hash_buf);
	memcpy(src_addr+PAGE_SIZE,ecc_buf,32);
	memcpy(&src_addr[(PAGE_SIZE+32)],hash_buf,HASH_SIZE);
	memcpy(&src_addr[(PAGE_SIZE+32+HASH_SIZE)],hash_buf,HASH_SIZE);

	umc_nand_page_prg( page_addr*2, src_addr, PAGE_SIZE + 64);

	if (cfg_nand_read(page_addr, rbuf) || memcmp(src_addr, rbuf, PAGE_SIZE))
		return -1;
	
	return 0;
}

static uint cfg_nand_erase(uint block_num)
{
    if(block_num<(CONFIG_BASE_ADDR /BLOCK_SIZE) || 
        block_num >(CONFIG_END_ADDR /BLOCK_SIZE -1)) return 0;
    
    nand_phy_erase(block_num);
	return 0;
}

void s_GetCfgPageNo(int id, uint PageNo[])
{
    uint i,index_tmp[CONFIG_BLOCK_NUM][MAX_INDEX];

    memset(index_tmp, 0xff, CONFIG_BLOCK_NUM*MAX_INDEX*4);
    
    //查找关键字项在各块中的页码
    for (i = 0; i < CONFIG_BLOCK_NUM; i++)
    {
        search((i + CONFIG_BASE_ADDR/BLOCK_SIZE), index_tmp[i]);
        PageNo[i] = index_tmp[i][id];
    }
}
#if 0//lanwq
void test_cfg(void)
{
	int i,j,len;
	char tmp_buf[2048]= {0};
	uchar ch;
	static int cnt = 0;
	ScrCls();
	ScrPrint(0,1,0,"1--write\r\n");	
	ScrPrint(0,2,0,"2--read\r\n");	
	ScrPrint(0,3,0,"3--earse\r\n");		
	
	i = 0;
	while(1)
	{
		ch = 0x00;
		ch = getkey();
		printk("%02x ", ch);
		switch(ch)
		{
			case 0x31:
				for (i = 0; i < 20; i++)
				{
					for (j = 0; j < 2048; j++)
						tmp_buf[j] = i+cnt;
				
					len = WriteTerminalInfo(i%10, tmp_buf, 30);
					printk("%d, \r\n", len);			
				}
				cnt++;
			break;
			case 0x32:
				for (i = 0; i < 20; i++)
				{
					memset(tmp_buf, 0x00, 2048);
					len = ReadTerminalInfo(i, tmp_buf, 2048);
				
					printk("\r\nID:%d, LEN:%d\r\n", i, len);
					j = 0;
					while(j < len) 
					{
						printk("%d, ", tmp_buf[j++]);			
					}
				}
			break;
			case 0x33:
				for (i = 0; i < 3; i++)
				{
					cfg_nand_erase(i+1);
				}
			break;

			default:
			break;
		}
	}
}
#endif

