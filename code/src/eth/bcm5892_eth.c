//2016.9.21:Add an eth_init call in eth_rx()
//2016.9.21:Modified the is_linkup() for error link report following eth_init()
//2016.9.21:Enabled MAC address filter in bcm5892mac_reset()
//2016.9.26:Modified MAC_CFG register in bcm5892mac_reset() to be consistent with that in eth_init()
//2016.10.17,Added mac address reset in eth_init()
//2016.3.29,Added 3s debouncing process in is_linkup()
 
#include "bcm5892_eth.h"

static volatile uint gtx_idx=0, grx_idx=0;
static int flag_maintain = 0;
static void bcm5892mac_reset();
static uchar last_mac_addr[6]={0,0,0,0,0,0};//2016.10.16

static void mdio_write(uint reg_addr, ushort wdata, uint phy_addr)
{
    uint data =0;
    disable_softirq_timer();
    while(readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK);
    data=((1<<30)|(1<<28)|(2<<16)| 	/* Start & command (write) & Bus turn around */
		(phy_addr<<23)|(reg_addr<<18)|(wdata));
    writel(data,MMI_R_mmi_cmd_MEMADDR);
    while(readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK);
	
    data=(1<<30)|(1<<29)|(2<<16)| /* Start & command (read) & Bus turn around*/
		(phy_addr<<23)|(reg_addr<<18);
    writel(data,MMI_R_mmi_cmd_MEMADDR);
    while(readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK);
    data = readl(MMI_R_mmi_cmd_MEMADDR);
    enable_softirq_timer();
}
static uint mdio_read(uint reg_addr, uint phy_addr)
{
    uint data=0;
    disable_softirq_timer();
    while(readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK);
    data=(1<<30)|(1<<29)|(2<<16)| /* Start & command (read) & Bus turn around*/
		(phy_addr<<23)|(reg_addr<<18);
    writel(data,MMI_R_mmi_cmd_MEMADDR);
    while(readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK);
    data = readl(MMI_R_mmi_cmd_MEMADDR);
    enable_softirq_timer();
    return data;
}

void bcm5892_init_10xmb(uchar is_100mb, uchar is_full)
{
	if(is_100mb) writel_or((1<<2),BCM5892_MAC_CFG);
    else {
		mdio_write(0x0 ,0x0000,MDIO_INTERNAL_PHY);
		writel_and(~(1<<2),BCM5892_MAC_CFG);
		is_full =0; /*current,10M only support half */
    }

    if(is_full) writel_and(~(1<<10),BCM5892_MAC_CFG);
    if(!is_full) {
		writel_or((1<<10),BCM5892_MAC_CFG);
		writel_or(0x2b, BCM5892_MAC_BP);
		writel(0x18, BCM5892_MIN_TX_IPG);
	}
}

void EthSetRateDuplexMode_std(int mode)
{
    uint val,status;
	int i;
	disable_softirq_timer();
	/* issue soft reset */
	val = readl(BCM5892_MAC_CFG) | BCM5892_MAC_CFG_SRST;
	writel(val, BCM5892_MAC_CFG);

    writel_and(0xFFFFFFFC,BCM5892_MAC_CFG); /*disable rx/tx DMA*/

	//printk("mode=%d\r\n",mode);
	/* -- Configuration Initialization -- */
	writel(0x00, BCM5892_PHY_CTRL); 
	/* -- Internal Phy -- */
	/* Soft reset for Internal Phy */
	mdio_write(0x0, 0x8000,MDIO_INTERNAL_PHY); 

    switch(mode)
    {
        case ETH_AUTO:
        	/* Auto Neg,Restart auto neg */
        	mdio_write(0x0 ,0x3300,MDIO_INTERNAL_PHY); 
            break;
        case ETH_10M_HALF:
            bcm5892_init_10xmb(0,0);
            break;
        case ETH_100M_HALF:
            bcm5892_init_10xmb(1,0);
            break;
        case ETH_10M_FULL:
            bcm5892_init_10xmb(0,1);
            break;
        case ETH_100M_FULL:
            bcm5892_init_10xmb(1,1);
            break;
    }

	DelayMs(1);
	/* release soft reset */
    val = readl(BCM5892_MAC_CFG) & ~BCM5892_MAC_CFG_SRST;
	writel(val, BCM5892_MAC_CFG); 
	
	for(i=0;i<50;i++)
	{
		if(is_linkup()) break;
		DelayMs(100);
	}

	gtx_idx=0;
    grx_idx=0;
	/* initial the RX DMA */
	writel((uint)prbds, BCM5892_RBASE);
    writel(DMA_RXBD_MAX,BCM5892_RBCFG);
    writel(DEFAULT_RX_BUF_SIZE, BCM5892_RBUFFSZ);
    writel((uint)prbds, BCM5892_RBDPTR);
    writel((uint)&(prbds->bds[DMA_RXBD_MAX-1]), BCM5892_RSWPTR);
    writel_or(BCM5892_MAC_CFG_RXEN,BCM5892_MAC_CFG); /*enable rx DMA*/
	
	/* initial the TX DMA */
    writel((uint)ptbds, BCM5892_TBASE);        
    writel((DEFAULT_TX_WATERMARK << 16)|DMA_TXBD_MAX,BCM5892_TBCFG); //Ring size.
	writel((uint)ptbds, BCM5892_TBDPTR);
	writel((uint)ptbds, BCM5892_TSWPTR);
    writel_or(BCM5892_MAC_CFG_TXEN,BCM5892_MAC_CFG); /*enable tx DMA*/
	writel_and(0xFFFFFFFC,BCM5892_ETH_CTRL); /*clear GRS&GTS*/
	enable_softirq_timer();	

 }

int eth_init()
{
	uint status;
	int x=0; 

	writel_and(~(1<<8),DMU_R_dmu_pwd_blk1_MEMADDR);
	writel_or((1<<8),DMU_R_dmu_rst_blk1_MEMADDR);
	writel_and(~(1<<8),DMU_R_dmu_rst_blk1_MEMADDR);
	DelayUs(10);

	for(x=0;x<DMA_TXBD_MAX;x++) { 
		ptbds->bds[x].flag=0; 
		ptbds->bds[x].pdata=ptbds->dma_buf[x] ;
	}
	for(x=0;x<DMA_RXBD_MAX;x++) { 
		prbds->bds[x].flag=0; 
		prbds->bds[x].pdata=prbds->dma_buf[x] ;
	}

    /* -- Configuration Initialization -- */
	/* Setting the Internal phy */
	writel(0x00, BCM5892_PHY_CTRL); 
	/* Setting the clock value for Internal PHY. 0xcd */
	writel(0xcd,MMI_R_mmi_ctrl_MEMADDR);  
	/* -- Internal Phy -- */
	/* Soft reset for Internal Phy */
	mdio_write(0x0, 0x8000,MDIO_INTERNAL_PHY); 
	/* Enable Shadow registers (Earlier version.) */
	mdio_write(0x1f,0x0093,MDIO_INTERNAL_PHY); 
	/* Shad2. Misc Control - use SW control for Autoneg */
	mdio_write(0x10,0x1000,MDIO_INTERNAL_PHY); 
	/* TEST REG - disable shadow regs Grp1(Earlier version) */
	mdio_write(0x1f,0x0013,MDIO_INTERNAL_PHY); 
	/* Auto negotiation code. Need to open when the file is fixed.*/
    /* Auto neg Adv register Pause oper,100 FD,100 HD,10 FD, 10 HD, 802.3. */ 
	mdio_write(0x4 ,0x05E1,MDIO_INTERNAL_PHY); 
	/* MIICtrl - 100Mbit, Auto Neg,Restart auto neg, FD */
	mdio_write(0x0 ,0x3300,MDIO_INTERNAL_PHY); 
	
	/*Put the ethernet MAC into reset state.*/
    writel(BCM5892_MAC_CFG_SRST,BCM5892_MAC_CFG);
    //writel((BCM5892_MAC_CFG_CFWD|BCM5892_MAC_CFG_PROM|BCM5892_MAC_CFG_SPD_10),BCM5892_MAC_CFG);
    writel((BCM5892_MAC_CFG_CFWD|BCM5892_MAC_CFG_OFEN|BCM5892_MAC_CFG_SPD_100),BCM5892_MAC_CFG);//enable overflow control and the MAC-address filtering,2016.9.26
    writel(0x06,BCM5892_TX_FIFO_MACTXFE);
    writel(0x04,BCM5892_TX_FIFO_MACTXFF);
	writel_or(BCM5892_ETH_CTRL_MEN,BCM5892_ETH_CTRL); /* Init_Mib */
	/* Initialize mac config register to Ingnore transmit pause frame. */
	writel_or(BCM5892_MAC_CFG_TPD,BCM5892_MAC_CFG);	
	
	set_mac(last_mac_addr);//set the MAC address,2016.10.17
	
	gtx_idx=0;
    grx_idx=0;
	
	/* initial the RX DMA */
	writel((uint)prbds, BCM5892_RBASE);
    writel(DMA_RXBD_MAX,BCM5892_RBCFG);
    writel(DEFAULT_RX_BUF_SIZE, BCM5892_RBUFFSZ);
    writel((uint)prbds, BCM5892_RBDPTR);
    writel((uint)&(prbds->bds[DMA_RXBD_MAX-1]), BCM5892_RSWPTR);
    writel_or(BCM5892_MAC_CFG_RXEN,BCM5892_MAC_CFG); /*enable rx DMA*/
	
	/* initial the TX DMA */
    writel((uint)ptbds, BCM5892_TBASE);        
    writel((DEFAULT_TX_WATERMARK << 16)|DMA_TXBD_MAX,BCM5892_TBCFG); //Ring size.
	writel((uint)ptbds, BCM5892_TBDPTR);
	writel((uint)ptbds, BCM5892_TSWPTR);
    writel_or(BCM5892_MAC_CFG_TXEN,BCM5892_MAC_CFG); /*enable tx DMA*/
    writel_or(BCM5892_MAC_CFG_RXEN, BCM5892_MAC_CFG);
    /*Enable the RX DMA (clear ETH_CTRL[GRS])*/
    writel_and(~BCM5892_ETH_CTRL_GRS,BCM5892_ETH_CTRL);
	//flag_maintain = 1;
	return 0;
}

int is_linkup()
{
	int status;
	static uint t0=0,t_0,last_status=0,debounce_step=0;

	status = mdio_read(0x01,MDIO_INTERNAL_PHY) & (1<<2);
	if(flag_maintain == 0) 
	{
		//--to debounce the wire's plugging in and off at 3 seconds,added on 2017.3.29
		switch(debounce_step)
		{
		case 0:
			if(!status && status!=last_status)
			{
				t_0=GetMs();
				debounce_step=1;
				return last_status;
			}
			last_status=status;
			return status;
		case 1:
			//--if it restores,go to the initial step
			if(status==last_status)
			{
				debounce_step=0;
				return last_status;
			}

			if( (int)(GetMs()-t_0) >=3000 )
			{
				debounce_step=0;
				last_status=status;
				return status;
			}
			return last_status;
		default:
			debounce_step=0;
			break;
		}//siwtch(debounce_step)
		return status;
	}

	if (flag_maintain == 2)  {t0 = GetMs(); flag_maintain=1;}//GetTimerCount(),on 2017.3.29

	if(status != 0 || (GetMs() - t0 >5000))//GetTimerCount(),on 2017.3.29
	{  
		flag_maintain =0; 
		//t0 = 0;
		return status;
	}
	
	return 4;
}

int eth_tx(uchar *data, int len)
{
	bd_t *pSWbuf_desc,*pHWbuf_desc;
	uint temp_idx;
	int i;
	pHWbuf_desc = (bd_t *)readl(BCM5892_TBDPTR);  
	temp_idx=tbd_idx(pHWbuf_desc);
	pSWbuf_desc = addr_tbdesc(gtx_idx);
	if(len > ETH_PACKET_SIZE) len = ETH_PACKET_SIZE;
	memcpy(pSWbuf_desc->pdata, data, len);
	
	/* setup descriptor: sop, eop,since no append size =64B, crc appended by hardware, etc */
	pSWbuf_desc->flag= ( BCM5892_TX_BD_SOP|BCM5892_TX_BD_EOP|BCM5892_TX_BD_CAP|len);
	writel((uint)pSWbuf_desc, BCM5892_TSWPTR);
    gtx_idx =(gtx_idx+1)% DMA_TXBD_MAX;
	writel_and(~BCM5892_ETH_CTRL_GTS,BCM5892_ETH_CTRL);
	for(i=0;i<10000;i++)
	{
		if(gtx_idx != temp_idx) break;
		pHWbuf_desc = (bd_t *)readl(BCM5892_TBDPTR);  
	    temp_idx=tbd_idx(pHWbuf_desc);
	}
    if(i >= 10000) {
        eth_init();
        flag_maintain = 2;//1
        return -1;
    }
	return 0;
}

uint eth_rx(uint (*f_rx)(uchar*,uint))
{
    uint rx_cnt=0;
	bd_t * psw_rbd, *phw_wbd;
    uint flag,hw_idx;
	
	//rRXFIFO_STAT--D1:overrun,D0:underrun
	//rTXFIFO_STAT--D1:overrun,D0:underrun
    if((readl(BCM5892_ETH_CTRL+BCM5892_RX_FIFO_STAT) & 0x03) || (readl(BCM5892_ETH_CTRL+BCM5892_TX_FIFO_STAT) & 0x03)) 
    {        
          eth_init();
		  flag_maintain = 2;
		  return -2;
    }

	if(readl(BCM5892_INTR_RAW) & (BCM5892_INTR_RAW_RHLT|BCM5892_INTR_RAW_ROV)) 
	{    	
        bcm5892mac_reset();
	}

	phw_wbd	=(bd_t *) readl(BCM5892_RBDPTR);
	hw_idx = rbd_idx(phw_wbd);

    for(;hw_idx != grx_idx;grx_idx = (grx_idx+1)%DMA_RXBD_MAX) /* has package */
    {  
        psw_rbd = addr_rbdesc(grx_idx);
        if(is_desc_er_rx(psw_rbd->flag)) /* pass out */
        {
		     bcm5892mac_reset();
            //eth_init();
            return -1;
        }
        flag = psw_rbd->flag;
        if(is_desc_sop_rx(flag) && is_desc_eop_rx(flag))
        {
            rx_cnt = flag & 0xffff;
            if(rx_cnt > ETH_PACKET_SIZE) continue;
						f_rx(psw_rbd->pdata, rx_cnt);
        }
        writel((uint)addr_rbdesc(grx_idx), BCM5892_RSWPTR);
		writel_and(~BCM5892_ETH_CTRL_GRS, BCM5892_ETH_CTRL);
	}

	return 0;
}

static void bcm5892mac_reset()
{
	uint val;
    int i;
	int x;

	//printk("rx over\r\n");
	if(!(readl(BCM5892_ETH_CTRL) & BCM5892_ETH_CTRL_GRS)){
		val = readl(BCM5892_ETH_CTRL);
		val |= BCM5892_ETH_CTRL_GRS;
		writel(val,BCM5892_ETH_CTRL);
		for(i=0;i<100000;i++){
			if(readl(BCM5892_INTR_RAW) & BCM5892_INTR_RAW_GRSC)
				break;
		}
	}
	if (!(readl(BCM5892_ETH_CTRL) & BCM5892_ETH_CTRL_GTS)) {
        
		/* Still transmitting, wait for THLT */
		for(i=0;i<100000;i++) {
            if(readl(BCM5892_INTR_RAW) & BCM5892_INTR_RAW_THLT) break;
        }
	}
	
	writel_and(~BCM5892_MAC_CFG_RXEN,BCM5892_MAC_CFG); /*enable rx DMA*/
	writel_and(~BCM5892_MAC_CFG_TXEN,BCM5892_MAC_CFG); /*disable MAC tx path*/
	/* RX and TX DMA both halted */
	/* issue soft reset */
	val = readl(BCM5892_MAC_CFG) | BCM5892_MAC_CFG_SRST;
	writel(val, BCM5892_MAC_CFG);

	grx_idx = 0;
	gtx_idx = 0;
	for(x=0;x<DMA_TXBD_MAX;x++) { 
		ptbds->bds[x].flag=0; 
		ptbds->bds[x].pdata=ptbds->dma_buf[x] ;
	}
	for(x=0;x<DMA_RXBD_MAX;x++) { 
		prbds->bds[x].flag=0; 
		prbds->bds[x].pdata=prbds->dma_buf[x] ;
	}
	/* initial the RX DMA */
	writel((uint)prbds, BCM5892_RBDPTR);
	writel((uint)&(prbds->bds[DMA_RXBD_MAX-1]), BCM5892_RSWPTR);
	/* initial the TX DMA */
	writel((uint)ptbds, BCM5892_TBDPTR);
	writel((uint)ptbds, BCM5892_TSWPTR);
	/* clear GRS and GTS in eth_ctrl */
	val = readl(BCM5892_ETH_CTRL);
	val &= ~(BCM5892_ETH_CTRL_GRS | BCM5892_ETH_CTRL_GTS);
	writel(val, BCM5892_ETH_CTRL);
	writel_or(BCM5892_MAC_CFG_RXEN,BCM5892_MAC_CFG); /*enable MAC rx path*/
	writel_or(BCM5892_MAC_CFG_TXEN,BCM5892_MAC_CFG); /*enable MAC tx path*/
	writel_and(~BCM5892_MAC_CFG_PROM,BCM5892_MAC_CFG);//disable the ALL-ADDR-ACCEPTABLE mode,2016.9.21
	writel_and(~0x0c,BCM5892_MAC_CFG);//clear the D3D2 speed bits,2016.9.26
	writel_or(BCM5892_MAC_CFG_SPD_100|BCM5892_MAC_CFG_TPD,BCM5892_MAC_CFG);//D28:1-disable PAUSE frame transmit,D3D2:speed,01-100Mbps,2016.9.26
	writel_and(~BCM5892_MAC_CFG_NOLC,BCM5892_MAC_CFG);//D24:1-disable frame length check,0-enable it,2016.9.26
	writel_and(~BCM5892_MAC_CFG_PFWD,BCM5892_MAC_CFG);//D7:1-PAUSE frame forward,0-discarded,2016.9.26

	DelayMs(1);
	/* release soft reset */
    val = readl(BCM5892_MAC_CFG) & ~BCM5892_MAC_CFG_SRST;
	writel(val, BCM5892_MAC_CFG);

	/* Clear GRSC and THLT [INTR_RAW] bits */
	val = BCM5892_INTR_CLR_GRSC | BCM5892_INTR_CLR_RHLT | BCM5892_INTR_CLR_GTSC;
	val |= BCM5892_INTR_CLR_THLT | BCM5892_INTR_CLR_ROV;
	writel(val, BCM5892_INTR_CLR);
	writel(0,BCM5892_INTR_CLR);
}

int set_mac(uchar *mac_addr)
{
    uint mac, mac_cfg;
    mac=mac_addr[3]|(mac_addr[2]<<8)|(mac_addr[1]<<16)|(mac_addr[0]<<24);
    writel(mac,BCM5892_MAC_ADDR0);  
	mac=mac_addr[5]|(mac_addr[4]<<8);
	writel(mac,BCM5892_MAC_ADDR1);
	memcpy(last_mac_addr,mac_addr,6);//2016.10.17

	mac_cfg = readl(BCM5892_MAC_CFG);
	writel_or(1<<9,BCM5892_MAC_CFG);
	//mac_cfg = readl(BCM5892_MAC_CFG);
	//printk("enable mac_cfg:0x%08X\r\n", mac_cfg);
	return 0;
}

int mac_get(unsigned char mac[6])
{
	uint mac_addr;
	mac_addr= readl(BCM5892_MAC_ADDR0);
	mac[0] = (mac_addr>>24)&0xff;
	mac[1] = (mac_addr>>16)&0xff;
	mac[2] = (mac_addr>>8)&0xff;
	mac[3] = (mac_addr>>0)&0xff;
	mac_addr= readl(BCM5892_MAC_ADDR1);	
	mac[4] = (mac_addr>>8)&0xff;
	mac[5] = (mac_addr>>0)&0xff;
	//printk("--%s--: %02x-%02x-%02x-%02x-%02x-%02x\r\n", 
	//	__func__, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return 0;
}

#ifdef DEBUG_ETH_MAC
/* dump interrupt register, debug */
#define dump_intr_info() do { \
	uint intr_mask =readl(BCM5892_INTR_MASK); \
	uint intr_raw = readl(BCM5892_INTR_RAW); \
	uint intr = readl(BCM5892_INTR); \
	uint intr_clr = readl(BCM5892_INTR_CLR); \
	printk("INTR_MASK..........................:0x%.8X\n\r",intr_mask); \
	printk("INTR_RAW...........................:0x%.8X\n\r",intr_raw); \
	printk("INTR...............................:0x%.8X\n\r",intr); \
	printk("INTR_CLR...........................:0x%.8X\n\r",intr_clr); \
} while (0)

static void hex_dump(const uchar *buf, uint size)
{
	uint i;
	printk("-----------------------------------\n");
	for(i=0; i<size; i++)
		printk("%02x,%s", buf[i], (i+1)%16?"":"\n");
	printk("\n-----------------------------------\n");
}

void debug_rx(uchar* buf,uint len)
{
	printk("rx:%d!\r\n",len);
}

void test_eth(void)
{
	uchar buf[1024],key,key1;
	int i;
	static int count=0;

	for(i=0;i<sizeof(buf);i++) buf[i]=i;
	key=getkey();
	if(key==0x31)ScrPrint(0,2,0,"eth tx");
	if(key==0x32)ScrPrint(0,2,0,"eth rx");
	while(1)
	{
		ScrPrint(0,3,0,"count=%d",count++);
		//eth_tx(buf, sizeof(buf));
		//eth_tx(buf, sizeof(buf));
		if(key==0x31)eth_tx(buf, sizeof(buf));	
		if(key==0x33){eth_tx(buf, sizeof(buf));DelayMs(100);}
		if(key==0x34){eth_tx(buf, sizeof(buf));DelayMs(1000);}
		if(key==0x32)eth_rx(debug_rx);
		if(!kbhit())
		{
			getkey();
			key1=getkey();
			if(key1==0x35) EthSetRateDuplexMode(0);
			else if(key1==0x36) EthSetRateDuplexMode(1);
			else if(key1==0x37) EthSetRateDuplexMode(2);
			else if(key1==0x38) EthSetRateDuplexMode(3);
			else if(key1==0x39) EthSetRateDuplexMode(4);		
		} 
		//DelayMs(1000);
	}
}
#endif

