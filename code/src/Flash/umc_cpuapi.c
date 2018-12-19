#include "base.h"
#include "umc_cpuapi.h"
#include "nand.h"
 
uint umc_memif_cfg()
{
	uint val = 0;
	val = 	( WIDTH_8 << 12 )	|  ( CHIPS_2  << 10 ) |
			( TYPE_NAND  <<  8 )	|   ( WIDTH_16 <<  4 ) |
			( CHIPS_3  <<  2 )		| ( TYPE_SRAM_NONMUXED  <<  0 ) ;
	writel(val,UMC_R_umc_memif_cfg_MEMADDR);
	return 0;
}

uint umc_config_cs_type() 
{
	uint val,addrmatch;
	uchar  typecs = 0;

	addrmatch = (START_NAND_CS0 >> 24 ) |(START_NOR_FLASH1 >> 16 ) | (START_NOR_FLASH2 >>  8 ) ;
	//    print_log("UMC : Address match = 0x%x\n",addrmatch);
	writel(addrmatch,UMC_R_umc_addr_match_MEMADDR);
	writel(0xffffffff,UMC_R_umc_addr_mask_MEMADDR);

	typecs = (CS_SRAM << 2) |  (CS_SRAM << 1) |CS_NAND;
	// 3:0 has to be changed to the input cs_type
	val = 0x10f04400;
	val = val | (typecs & 0xf);

	writel(val,UMC_R_umc_conf_MEMADDR);
	if ( val == readl(UMC_R_umc_conf_MEMADDR) )  return 0;

	return 1;

}

 uint umc_ecc_config(uchar ecc_mode) 
{
	uint value ;

	// Writing the ecc_addr register
	writel(ECC_COL_ADDR_MT,UMC_R_umc_ecc_addr0_MEMADDR);

	value = 	((ECC_PAGE_2048 << 11)) 		|((1 << 10))                 
			|((ECC_NOJUMP & 0x3) << 5) 	|((ECC_RD_END & 0x1) << 4) 
			|((ecc_mode & 0x3) << 2) 		|((ECC_PAGE_2048 & 0x3) ) ;

	writel(value,UMC_R_umc_ecc_memcfg_MEMADDR);
	return 0;
}

uint NAND_umc_set_cycles() 
{
	uchar t_RR, t_TR,t_PC,t_WP,t_CEOE,t_WC,t_RC;
	uint val;

	t_RR=2;
	t_TR=1;
	t_PC=1;
	t_WP=2;
	t_CEOE=2;
	t_WC=4;
	t_RC=5;

	val = ( ((t_RR&T_TR_MASK)<<20) |
	      ((t_TR&T_TR_MASK)<<UMC_F_t_tr_3_R) |
	      ((t_PC&T_PC_MASK)<<UMC_F_t_pc_3_R) |
	      ((t_WP&T_WP_MASK)<<UMC_F_t_wp_3_R) |
	      ((t_CEOE&T_CEOE_MASK)<<UMC_F_t_ceoe_3_R) |
	      ((t_WC&T_WC_MASK)<<UMC_F_t_wc_0_R) |
	      ((t_RC&T_RC_MASK)<<UMC_F_t_rc_1_R));

	// print_log("UMC set_cycle : 0x%x\n",val);
	writel(val,UMC_R_umc_set_cycles_MEMADDR);

	return 0;

}

#define  umc_nand_page_prg_end(data)	writel(data,(START_NAND_CS0 | 0x00388400))
#define umc_nand_page_rd_end() 	readl(START_NAND_CS0 | 0x00298400)
#define NAND_WORD_WR_ADDR (START_NAND_CS0 | ( 0x01 << 21) | ( 1 << 19))
#define  umc_nand_word_wr(data) writel(data,NAND_WORD_WR_ADDR)
#define NAND_WORD_RD_ADDR (NAND_WORD_WR_ADDR|(NAND_PAGE_RD_E_CMD << 11))
#define umc_nand_wrod_rd() readl(NAND_WORD_RD_ADDR)


static uint umc_nand_page_prg_start_128(uint address) 
{
	uint addr;
	uint device_addr1;

	writel((1<<4),UMC_R_umc_memc_clr_MEMADDR);
	addr = START_NAND_CS0;
	addr = addr | (NAND_PAGE_PRG_S_CMD << 3)
			| (NAND_PAGE_PRG_E_CMD << 11) |( 4 << 21);//no_addr_cycles=4;
	device_addr1 = ((address &0xffff000)<<4) | (address &0xfff);
	//    print_log("NAND Page Program Start Command Issue : 0x%x : 0x%x 0x%x\n",addr,device_addr1);
	writel(device_addr1,addr);
	return 0;
}

static uint umc_nand_page_prg_start_256(uint address) 
{
	uint addr;
	uint device_addr1;

	writel((1<<4),UMC_R_umc_memc_clr_MEMADDR);
	addr = START_NAND_CS0;
	addr = addr | (NAND_PAGE_PRG_S_CMD << 3)
			| (NAND_PAGE_PRG_E_CMD << 11) |( 5 << 21);//no_addr_cycles=5;
	device_addr1 = ((address &0xffff000)<<4) | (address &0xfff);
	writel(device_addr1,addr);
    writel((address>>28)&1,addr);
	return 0;
}

static uint (* umc_nand_page_prg_start)(uint address)=umc_nand_page_prg_start_128; 

static uint umc_nand_page_rd_cmd_128(uint address) 
{
    volatile int j;
    uint addr;
    uint device_addr1;

    writel(1<<4,UMC_R_umc_memc_clr_MEMADDR );
    addr = START_NAND_CS0;
    addr = addr | (NAND_PAGE_RD_S_CMD << 3) 
		| (NAND_PAGE_RD_E_CMD << 11) | ( 4 << 21) | ( 1 << 20); //no_addr_cycles=4; End Command Valid = 1
    device_addr1 = ((address &0xffff000)<<4) | (address &0xfff);
    writel(device_addr1, addr);
    
    j=0x8000;
    while(!(readl(UMC_R_umc_status_reg_MEMADDR)&(1<<6)) && j--){}
    return 0;
}

static uint umc_nand_page_rd_cmd_256(uint address) 
{
    volatile int j;
    uint addr;
    uint device_addr1;

    writel(1<<4,UMC_R_umc_memc_clr_MEMADDR );
    addr = START_NAND_CS0;
    addr = addr | (NAND_PAGE_RD_S_CMD << 3) 
		| (NAND_PAGE_RD_E_CMD << 11) | ( 5 << 21) | ( 1 << 20); //no_addr_cycles=5; End Command Valid = 1
    device_addr1 = ((address &0xffff000)<<4) | (address &0xfff);
    writel(device_addr1, addr);
    writel((address>>28)&1,addr);
    
    j=0x8000;
    while(!(readl(UMC_R_umc_status_reg_MEMADDR)&(1<<6)) && j--){}
    return 0;
}

static uint (*umc_nand_page_rd_cmd)(uint address)=umc_nand_page_rd_cmd_128; 

uint umc_nand_blk_erase(uint block_num) {
	uint addr;
    
    if(block_num==0) return 1; /*can't erase boot area*/
	addr = START_NAND_CS0 | (NAND_BLK_ERS_S_CMD << 3) 
		| (NAND_BLK_ERS_E_CMD << 11) | ( 0x03 << 21) | ( 1 << 20);  //no_addr_cycles=3; End Command Valid = 1
	   
	writel(block_num << 6,addr);
	return 0;
}

uint umc_nand_page_prg(uint page_addr,uchar* src_addr, uint bytesize)
{
	int i;
	uint *src_ptr;
    if((page_addr/2)/BLOCK_SIZE == 0) return 1;/*can't write to boot area*/
    
	src_ptr = (uint*)(src_addr);
	umc_nand_page_prg_start( page_addr);

	for ( i = 0 ; i <(bytesize/4 -1); i++) 
	{
		umc_nand_word_wr( src_ptr[i]);
	}
	umc_nand_page_prg_end(src_ptr[i]);
	DelayUs(700);
	return 0;
}

uint umc_nand_page_rd(uint page_addr,uchar* dst_addr, uint bytesize) 
{
	int i;
	uint *dst_ptr;
	dst_ptr = (uint*) dst_addr;
	umc_nand_page_rd_cmd(page_addr);

	for ( i = 0 ; i <  (bytesize /4 -1); i++) 
	{
		dst_ptr[i]= umc_nand_wrod_rd();
	}
	dst_ptr[i] = umc_nand_page_rd_end();
	return 0;
}

uint umc_nand_get_type()
{   
    uint addr,data1,data2;

    addr = START_NAND_CS0 | ( 0x90 << 3   ) | ( 0x01 << 21 );   
    writel(0,addr); 
    data1 = umc_nand_wrod_rd();
    data2 = umc_nand_wrod_rd(); 
	return data1;
} 

extern uint k_FlashSize;
uint umc_nand_init(void)
{
    if(umc_nand_get_type()!=NAND_ID_SLC)
    {
        umc_nand_page_prg_start = umc_nand_page_prg_start_128;
        umc_nand_page_rd_cmd = umc_nand_page_rd_cmd_128;
		k_FlashSize = 128;
    }
    else
    {
        umc_nand_page_prg_start = umc_nand_page_prg_start_256;
        umc_nand_page_rd_cmd = umc_nand_page_rd_cmd_256;
		k_FlashSize = 256;
    }
    return 0;
}
