#include "base.h"
#include "bcm5892-bbl.h"
#include "../lcd/lcdapi.h"

//#define DEBUG_BBL
/* Function prototypes */
uint bbl_get_status(void);
static int bbl_write(uint offset, int len, uint *value);
static int bbl_read(uint offset, int len, uint *value);
static void bbl_init(void);
static void bbl_unlock(void);
static uint bbl_read_reg(uint regAddrSel);
extern POS_AUTH_INFO *GetAuthParam(void);
extern int snprintf(char *str, size_t size, const char *format, ...); 

#ifdef DEBUG_BBL
static char tamperSources[][22] = {
	"TAMPER_N", "undef", "undef", "undef", "undef",
	"TAMPER_P", "ExtGrid - P1", "ExtGrid - P2",
	"ExtGrid - P3", "ExtGrid - P4",
	"ExtGrid - Fault", "ExtGrid - Open",
	"IntGrid - Fault", "IntGrid - Open",
	"undef", "ROC Tamper", "Low Temp Tamper",
	"High Temp Tamper", "Low Frequency Tamper",
	"High Frequency Tamper", "Low Voltage Tamper",
	"High Voltage Tamper",
};
#endif

static int dmu_block_enable(uint enable)
{
	*((volatile uint *)(DMU_R_dmu_pwd_blk1_MEMADDR)) &= ~(0x1 << enable);
	return 0;
}

static void bbl_init()
{
	uint val, addr, status;
	dmu_block_enable(DMU_F_dmu_pwd_bbl_R);

	val = BBL_SOFT_RST_BBL;
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
	while (readl(addr) & BBL0_F_rdyn_go_MASK);
}

static void bbl_write_256b_mem(uint mem_addr, uint data)
{
	uint val, addr;
	val = data;				/* use (address+1) as data */
	addr = BBL0_R_BBL_ACCDATA_MEMADDR;
	writel(val,addr);

	val = (mem_addr << BBL0_F_indaddr_R) | (BBL_WRITE_256B_MEM);
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
    while (readl(addr) & BBL0_F_rdyn_go_MASK);
}

static uint bbl_read_256b_mem(uint mem_addr)
{
	uint val, addr;
	val = (mem_addr << BBL0_F_indaddr_R) | (BBL_READ_256B_MEM);
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
    while (readl(addr) & BBL0_F_rdyn_go_MASK);

	// Read the return data
	addr = BBL0_R_BBL_ACCDATA_MEMADDR;
	val = readl(addr);
	return val;
}

static void bbl_write_2kram(uint mem_addr, uint data)
{
	uint val, addr;
    //printk("write:%x,data:%x!\r\n",mem_addr,data);
	val = data;
	addr = BBL0_R_BBL_ACCDATA_MEMADDR;
	writel(val,addr);

	val = (mem_addr << BBL0_F_indaddr_R) | (BBL_WRITE_2KRAM);
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
    while (readl(addr) & BBL0_F_rdyn_go_MASK);
}

static uint bbl_read_2kram(uint mem_addr)
{
	uint val, addr;
	val = (mem_addr << BBL0_F_indaddr_R) | (BBL_READ_2KRAM);
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
    while (readl(addr) & BBL0_F_rdyn_go_MASK);

	// Read the return data
	addr = BBL0_R_BBL_ACCDATA_MEMADDR;
	val = readl(addr);
    //printk("read:%x,data:%x!\r\n",mem_addr,val);
	return val;
}

static int bbl_read(uint offset, int len, uint *value)
{
	int i;
	if(!bbl_read_reg(BBL1_R_BBL_EN_TAMPERIN_SEL)) return -11;
	if(offset > BPK_SIZE) return -12;
	
	for (i = 0; i < len / 4; i++) {
		if ((offset + i * 4) >= BPK_SIZE) break;
		value[i] = bbl_read_256b_mem(offset/4 + i);
	}
	return i * 4;
}

static int bbl_write(uint offset, int len, uint *value)
{
	int i;

	if(!bbl_read_reg(BBL1_R_BBL_EN_TAMPERIN_SEL)) return -1;
	if(offset > BPK_SIZE) return -2;
	for (i = 0; i < len / 4; i++) {
		if ((offset + i * 4) >= BPK_SIZE)	break;
		bbl_write_256b_mem(offset / 4 + i, value[i]);
	}
	return i * 4;
}

/************************************************************************
Method:	bbl_wr_indirect_reg
Description:	Helper function to do BBL indirect writes.
Arguments:
	Input: regAddrSel, wrData
	Output: bblStatus
Exceptions:
	None
************************************************************************/
static uint bbl_write_reg(uint regAddrSel, uint wrDat)
{
	uint val;
	uint addr;
	/* Load the indirect data register with a value */
	val = wrDat;
	addr = BBL0_R_BBL_ACCDATA_MEMADDR;
	writel(val,addr);

	/* Indirect Reg Write to Register (regAddrSel) */
	val = (regAddrSel << BBL0_F_indaddr_R) | (BBL_WRITE_INDREG);
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
    while (readl(addr) & BBL0_F_rdyn_go_MASK);
	//printk("bbl_wr_indirect_reg: regAddrSel=0x%x, wrDat=0x%x\r\n", regAddrSel, wrDat);
	return BBL_STATUS_OK;
}

/************************************************************************
Method:	bbl_read_reg
Description:	Helper function to do BBL indirect reads.
Arguments:
	Input: regAddrSel, *rdDat
	Output: bblStatus, *rdDat

Exceptions:
	None

************************************************************************/
static uint bbl_read_reg(uint regAddrSel)
{
	uint val;
	uint addr;
	/* Indirect Reg Read to Register (regAddrSel) */
	val = (regAddrSel << BBL0_F_indaddr_R) | (BBL_READ_INDREG);
	addr = BBL0_R_BBL_CMDSTS_MEMADDR;
	writel(val,addr);
    while (readl(addr) & BBL0_F_rdyn_go_MASK);
	/* Read the indirect data register getting a value */
	addr = BBL0_R_BBL_ACCDATA_MEMADDR;
	val = readl(addr);

	/* Return the result */
	//printk("bbl_rd_indirect_reg: regAddrSel=0x%x, rdDat=0x%x\r\n",regAddrSel, val);

	return val;
}

static int bbl_config(int SensorSW)
{
	uint i;
	uint val = 0;
	uint new_val;
	uint data;

	/* lfo_cal config -- required for proper frequency monitoring
	   and voltage monitoring.  Clear tamper status after setting. */
	val = bbl_read_reg(BBL1_R_BBL_AFE_CFG_SEL);
	val &= ~BBL1_F_i_lfo_cal_MASK;

	val |= 5 << BBL1_F_i_lfo_cal_R;
	bbl_write_reg(BBL1_R_BBL_AFE_CFG_SEL, val);

	bbl_write_reg(BBL1_R_BBL_TAMPER_CLEAR_SEL, 0x7FFFFF);
	for (i = 0; i < 10000; i++)
		continue;
	bbl_write_reg(BBL1_R_BBL_TAMPER_CLEAR_SEL, 0);

	if (SensorSW & TEMPER_SENSOR_BIT) {
		data = bbl_read_reg(BBL1_R_BBL_AFE_CFG_SEL);
		data &= ~BBL1_F_i_temp_pwrdn_MASK;
		bbl_write_reg(BBL1_R_BBL_AFE_CFG_SEL, data);

		/*********************
            Temper=-40,     TADC=434
            Temper=25,	     TADC=343
            Temper=75,	     TADC=274
            Temper=100,     TADC=240
            Temper=125,     TADC=204
		**********************/
		val = 204;  /*125--high temperature*/
		data = bbl_read_reg(BBL1_R_BBL_SENSOR_CFG_SEL);
		data |= val << 17;
		data += 0xc << 26;
		data += 0x3 << 30;

		bbl_write_reg(BBL1_R_BBL_SENSOR_CFG_SEL, data);
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val |= 0x00020000;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);

		val = 434; /*-40--low temperature*/
		data = bbl_read_reg(BBL1_R_BBL_SENSOR_CFG_SEL);
		data &= ~(0x1ff << 8);
		data |= val << 8;
		bbl_write_reg(BBL1_R_BBL_SENSOR_CFG_SEL, data);

		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val |= 0x00010000;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}
	else {
		data = bbl_read_reg(BBL1_R_BBL_AFE_CFG_SEL);
		data |= BBL1_F_i_temp_pwrdn_MASK;
		bbl_write_reg(BBL1_R_BBL_AFE_CFG_SEL, data);        
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val &= 0xFFFCFFFF;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}

	if (SensorSW & FREQ_SENSOR_BIT) {
		data = bbl_read_reg(BBL1_R_BBL_AFE_CFG_SEL);
		data &= ~BBL1_F_i_fmon_pwrdn_MASK;
		bbl_write_reg(BBL1_R_BBL_AFE_CFG_SEL, data);
        
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val |= 0x00080000;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val |= 0x00040000;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}
	else {
		data = bbl_read_reg(BBL1_R_BBL_AFE_CFG_SEL);
		data |= BBL1_F_i_fmon_pwrdn_MASK;
		bbl_write_reg(BBL1_R_BBL_AFE_CFG_SEL, data);        
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val &= 0xFFF3FFFF;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);        
	}

	/* Configure Voltage Monitor*/
	if (SensorSW & VOLTAGE_SENSOR_BIT) {
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val |= 0x00300000;	/* workaround for voltage leakage */
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}
	else {
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val &= 0xFFDFFFFF;	/* workaround for voltage leakage */
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}

	/*Configure static tamper inputs*/
	if (SensorSW & STATIC_SENSOR_BIT) {
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val &= 0xFFFFFC00;
		val |= 0x021;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
		val = 0x01;
		bbl_write_reg(BBL1_R_BBL_EN_TAMPERIN_SEL, val);
	}
	else {
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		val &= 0xFFFFFC00;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
		val = 0;
		bbl_write_reg(BBL1_R_BBL_EN_TAMPERIN_SEL, val);
	}

	if (SensorSW & EX_SENSOR_BIT) {
		/* Enable external open check (bit 1), enable dynamic
		 * LSFR mode (bit 0).
		 */
		val = 0x00;
		if(SensorSW & Dync_SENSOR1_BIT) val|=0x000101;
		if(SensorSW & Dync_SENSOR2_BIT) val|=0x040202;
		if(SensorSW & Dync_SENSOR3_BIT) val|=0x200404;
		if(SensorSW & Dync_SENSOR4_BIT) val|=0xc00808;
		//val = 0x00e40F0F;
		bbl_write_reg(BBL1_R_BBL_EXTMESH_CONFIG_SEL, val);
		/* initialize the external mesh LFSR reg */
		data = 0xA1B2C354;
		bbl_write_reg(BBL1_R_BBL_LFSR_SEED_SEL, data);

		data = bbl_read_reg(BBL1_R_BBL_SENSOR_CFG_SEL);
		data |= 0xF;
		bbl_write_reg(BBL1_R_BBL_SENSOR_CFG_SEL, data);
		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		/* val &= ~0xfff; */
		/* enable both open and fault checks for external grid */
		val |= 0xC00;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}
	else {
		val = 0;
		bbl_write_reg(BBL1_R_BBL_EXTMESH_CONFIG_SEL, val);
		/* initialize the external mesh LFSR reg */
		data = 0;
		bbl_write_reg(BBL1_R_BBL_LFSR_SEED_SEL, data);

		data = bbl_read_reg(BBL1_R_BBL_SENSOR_CFG_SEL);
		data &= 0xFFFFFFF0;
		bbl_write_reg(BBL1_R_BBL_SENSOR_CFG_SEL, data);

		val = bbl_read_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL);
		/* enable both open and fault checks for external grid */
		val &= 0xFFFFF3FF;
		bbl_write_reg(BBL1_R_BBL_TAMPER_SRC_EN_SEL, val);
	}

	bbl_write_reg(BBL1_R_BBL_TAMPER_CLEAR_SEL, 0x7FFFFF);
	for (i = 0; i < 10000; i++);
	bbl_write_reg(BBL1_R_BBL_TAMPER_CLEAR_SEL, 0);
	/* data = bbl_read_reg(BBL1_R_BBL_TAMPER_STATUS_SEL); */

	/* clear mesh logic fault detector latches */
    bbl_write_reg(BBL1_R_BBL_CONTROL_SEL,
    bbl_read_reg(BBL1_R_BBL_CONTROL_SEL) | 0x8);
    bbl_write_reg(BBL1_R_BBL_CONTROL_SEL,
    bbl_read_reg(BBL1_R_BBL_CONTROL_SEL) & ~0x8);
	return 0;
}

uint bbl_get_status()
{
	uint val,addr,i; 
 
    val = bbl_read_reg(BBL1_R_BBL_TAMPER_STATUS_SEL);

#ifdef DEBUG_BBL
	for (i = 0; i < 22; i++) {
		if (!(val & (1 << i))) continue;
		printk("================================\r\n");
		printk("%03x--%s\r\n",val, tamperSources[i]);
	}
#endif

	return val & 0x7FFFFF;
}

static void bbl_unlock()
{
	volatile int i;
	bbl_init();
	bbl_write_reg(BBL1_R_BBL_TAMPER_CLEAR_SEL, 0x7FFFFF);

	for (i = 0; i < 10000; i++);

	bbl_write_reg(BBL1_R_BBL_TAMPER_CLEAR_SEL, 0);

	/* clear mesh logic fault detector latches */
	bbl_write_reg(BBL1_R_BBL_CONTROL_SEL
		,bbl_read_reg(BBL1_R_BBL_CONTROL_SEL) | 0x8);
	bbl_write_reg(BBL1_R_BBL_CONTROL_SEL
		,bbl_read_reg(BBL1_R_BBL_CONTROL_SEL) & ~0x8);
}

/*
 * 增加对2KBBRAM的写支持，
 * boot_SecLevel<0x07只支持写前四字节2KBBRAM[0~3],即2KBBRAM第一个字，
 * boot_SecLevel=0x07只支持写2KBBRAM[12~16]的4字节,即2KBBRAM第四个字。
 * 暂不支持bpk和2KBBRAM跨区域写，offset从32开始。
 */
int WriteSecRam(uint uiOffset, uint len, uchar *pucData)
{
    int ret;
    uint buf[BPK_SIZE/4],tmp, Word_No;
    uchar *pt;

    if(uiOffset < BPK_SIZE)
    {
		if (uiOffset+len > BPK_SIZE) return -1;
		ret=bbl_read(0,BPK_SIZE,buf);
		if(ret <0) return ret;
		pt =(uchar *)buf;
		memcpy(pt+uiOffset,pucData,len);
		ret=bbl_write(0,BPK_SIZE,buf);
		return ret;
    }
    else
    {
		if(uiOffset+len>BPK_SIZE+4) return -5;  
		if(s_GetBootSecLevel()<POS_SECLEVEL_L3) Word_No = 0;    
		else Word_No = 3;

		tmp = bbl_read_2kram(Word_No);
		pt = (uchar*)&tmp;
		memcpy(pt+uiOffset-BPK_SIZE,pucData,len);
		bbl_write_2kram(Word_No,tmp);           
		return len;         
    }
}

/*
* 输入参数 uiOffset : 读取的起始地址
* len        :要读取的长度
* 输出参数 pucData: 读取的数据
* 返回值:
* 0: 读取成功
* -1: 地址超出范围
*/
int ReadSecRam(uint uiOffset, uint len, uchar *pucData)
{
	int ret;
    uint buf[BPK_SIZE/4];
    uchar *pt;

	if (uiOffset+len > BPK_SIZE) return -1;
    bbl_read(0,BPK_SIZE,buf);
 
    pt = (uchar *)buf;
    memcpy(pucData,pt+uiOffset,len);
    return 0;
}

void GetRandom(uchar *pucDataOut)
{
	uint value[2];
	int cnt = 0;
	static int flag = 0;

	if (!flag)
	{
		writel_or(0x01, RNG_R_RNG_CTRL_MEMADDR);
		flag = 1;
	}	
	while(((readl(RNG_R_RNG_STATUS_MEMADDR)>>24)<0x02) && (cnt++<1000));
	value[0] = readl(RNG_R_RNG_DATA0_MEMADDR);
	value[1] = readl(RNG_R_RNG_DATA0_MEMADDR);
	memcpy(pucDataOut, &value[0], 8);
}

void ShowSensorInfo(uint status)
{
	ScrFontSet(0);
	ScrCls();
	if(status & (1<<20)) Lcdprintf("under-voltage     \n");
	if(status & (1<<21)) Lcdprintf("over-voltage      \n");
	if(status & (1<<19)) Lcdprintf("high-frequency    \n");
	if(status & (1<<18)) Lcdprintf("low-frequency     \n");
	if(status & (1<<17)) Lcdprintf("over-temperature  \n");
	if(status & (1<<16)) Lcdprintf("under-temperature \n");
	if(status & (1<<0))  Lcdprintf("Switch N0         \n");
	if(status & (1<<5))  Lcdprintf("Switch P0         \n");
	if(status & (1<<6))  Lcdprintf("Switch N1-P1      \n");
	if(status & (1<<7))  Lcdprintf("Switch N2-P2      \n");
	if(status & (1<<8))  Lcdprintf("Switch N3-P3      \n");
	if(status & (1<<9))  Lcdprintf("Switch N4-P4      \n");
	getkey();	                                        
}

void vOutPutAttackInfo(uchar ucComPort)
{
#define ATTACK_LOG_FILE 	"ped_attack_log"//ped受攻击日志
#define ATTACK_LOG_LENGTH	2048 //BYTES

	int iRet,iLen,iLoop,iTmp;
	uchar aucTmp[ATTACK_LOG_LENGTH+1024],pszTmp[128],*pucTmp;
	uint uiTmp;

	iRet = s_open(ATTACK_LOG_FILE,O_RDWR,"\xff\x05");
	if(iRet<0) return;
	seek(iRet, 0, SEEK_SET);
	iLen=read(iRet,aucTmp,sizeof(aucTmp));
	close(iRet);
	PortClose(ucComPort);
	if(0!=PortOpen(ucComPort,"115200,8,N,1"))return;
	for(iLoop=0;iLoop<iLen/16;iLoop++)
	{
		pucTmp = aucTmp+iLoop*16;
		iTmp = (pucTmp[8]<<24)+(pucTmp[9]<<16)+(pucTmp[10]<<8)+pucTmp[11];
		uiTmp = (pucTmp[12]<<24)+(pucTmp[13]<<16)+(pucTmp[14]<<8)+pucTmp[15];
		iRet = snprintf(pszTmp,sizeof(pszTmp)-1,"%02x%02x%02x%02x%02x%02x|%08X|%08X|\n",
		pucTmp[0],pucTmp[1],pucTmp[2],pucTmp[3],pucTmp[4],pucTmp[5],iTmp,uiTmp);
		if(iRet>0) PortSends(ucComPort,pszTmp,iRet);
	}
	PortClose(ucComPort);	
}

uint GetSensorSW()
{
	char context[20];
	int ret;
	static uint sw_value=0xFFF00000;
	if(sw_value !=0xFFF00000) return sw_value;
	ret = ReadCfgInfo("SEN_PARA", context);
	if (ret <= 0)
	{
		sw_value = SENSOR_Switch_CONFIG;
		return SENSOR_Switch_CONFIG;
	}
	if((context[0] >=0x30) && (context[0]<=0x39)) context[0]-=0x30;
	if((context[0] >='A') && (context[0]<='F')) context[0]=context[0]-'A'+10;
	if((context[4] >=0x30) && (context[4]<=0x39)) context[4]-=0x30;
	if((context[4] >='A') && (context[4]<='F')) context[4]=context[4]-'A'+10;
	sw_value=((uint)(context[0]<<16) + context[4]) & SENSOR_Switch_CONFIG;
	return sw_value | STATIC_SENSOR_BIT;
}

uint s_GetBBLStatus()
{
    uint SensorSW;
    uint val;

    SensorSW = GetSensorSW();
    val = bbl_get_status();
    if (!(SensorSW & VOLTAGE_SENSOR_BIT)) val &= (~0x300000);/*BIT[21,20]*/
    val &= (~0x4000);/*Res:BIT14*/

    return val & 0x7FFFFF;
}

void s_DisplayTamperInfo(uint status)
{
	uchar sn[33];
	memset(sn,0,sizeof(sn));
	ReadSN(sn);
	ScrClrLine(0,6);
	ScrPrint(0,1,0,"POS TAMPERED!");
	ScrPrint(0,2,0,"ALL APPS ARE CLEARED!");
	ScrPrint(0,3,0,"ALL KEYS ARE CLEARED!");			
	ScrPrint(0,4,0,"TAMPER STATUS: %06X",status);
	ScrPrint(0,5,0,"SN: %s",sn);
}

uint s_CheckUpSensor_comp(void)/*for compatibility :old boot + new monitor*/
{
	uchar key, buff[10];
	uint status;	
	if (0==CheckIfDevelopPos())
	{
		if(bbl_read_reg(BBL1_R_BBL_LFSR_SEED_SEL)==0)
		{
			GetTime(buff);
			bbl_unlock();
			SetTime(buff);
			bbl_config(GetSensorSW()); 
			DelayMs(200);
		}

		status =s_GetBBLStatus();
		if(status)
		{
            vTraceAttackInfo(0, status);/*记录触发信息*/ 	
			CLcdSetFgColor(COLOR_BLACK);
			CLcdSetBgColor(COLOR_WHITE);

			s_DisplayTamperInfo(status);
			key = getkey();
			if(key==KEYFN || key==KEYF1)
			{
				ShowSensorInfo(status);
			}
			GetTime(buff);
			bbl_unlock();
			SetTime(buff);
			bbl_config(GetSensorSW()); 
		}
	}
	else
	{
		if(bbl_read_reg(BBL1_R_BBL_LFSR_SEED_SEL) ==0)
		{
			GetTime(buff);
			bbl_unlock();
			SetTime(buff);
			bbl_config(GetSensorSW());
		}
        return 0x7FFFFF;		
	}
	return 0;
}

uint CheckUpSensor(void)
{
	uint status, i;
	uchar ch, buff[64], sn[33]={0};
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
       
    if(s_GetBootSecLevel()==0)/*for compatibility :old boot + new monitor*/
    {
        return s_CheckUpSensor_comp();
    }
    status =s_GetBBLStatus();
    if(status)
    {    
        vTraceAttackInfo(0, status);/*记录触发信息*/    
        CLcdSetFgColor(COLOR_BLACK);
        CLcdSetBgColor(COLOR_WHITE);    
        while(1)
        {
            s_DisplayTamperInfo(status);
            ch = getkey();
            if(ch==KEYFN || ch==KEYF1)
            {
                ShowSensorInfo(status);
            }
            
            if(!kbhit()) ch = getkey();
            if((authinfo->SecMode==POS_SEC_L0) && (ch!=KEY5)) continue;
            else break;            
        }
    }
    else
    {
        if (authinfo->LastBblStatus)/*The recent value of tamper state reg*/
        {
            vTraceAttackInfo(0, authinfo->LastBblStatus);/*记录触发信息*/  
            s_DisplayTamperInfo(authinfo->LastBblStatus);
            ch = getkey();
            if(ch==KEYFN || ch==KEYF1)
            {
                ShowSensorInfo(authinfo->LastBblStatus);
            }                        
        }
    
        memset(buff, 0x00, sizeof(buff));
        ReadSecRam(0, BPK_SIZE, buff);        
        switch(authinfo->SecMode)
        {
            case POS_SEC_L0:
            break;
            case POS_SEC_L1:
                if (authinfo->TamperClear>0 && 
                    !memcmp(buff, buff+BPK_SIZE, BPK_SIZE))
                {
                    vOutPutAttackInfo(COM1);                            
                    memset(sn,0,sizeof(sn));
                    ReadSN(sn);
                    if(0==strlen(sn)) break;/*SN is Null*/                        
                    for (i = 0; i < APP_MAX_NUM; i++) s_iDeleteApp(i);/*del all app and its sub file*/        
                    ScrCls();
                    kbflush();
                    SCR_PRINT(0,0,0x61,"POS已触发","POS TAMPERED!");            
                    SCR_PRINT_EXT(0,2,0x61,"SN:%s","SN:%s",sn);
                    SCR_PRINT(0,4,0x61,"请输入解锁密码:","PLS INPUT PWD:");
                        
                    while(1)
                    {
                        ScrClrLine(6,7);
                        ScrGotoxy(64-16,6);             
                        ch = GetString(buff,0xe9,4,4);
                        if(ch!= 0) continue;
                        if(memcmp(buff + 1,"9876",4) == 0) break;
                        SCR_PRINT(0,6,0x61,"密码错误","PWD ERROR.");
                        DelayMs(2000);
                    }
                }            
            break;
            case POS_SEC_L2:                
                if (authinfo->TamperClear==0) vOutPutAttackInfo(COM1);            
                if (authinfo->TamperClear==1 && 
                    !memcmp(buff, buff+BPK_SIZE, BPK_SIZE))
                {
                    /*del all app and its sub file*/
                    for (i = 0; i < APP_MAX_NUM; i++) s_iDeleteApp(i);
                }
            break;
        }
    }
	return status;
}

void s_bbl_intr(int id)
{
    int intr_sta,intr_en;

    intr_sta = bbl_read_reg(BBL1_R_BBL_INT_STS_SEL); 
    intr_en = bbl_read_reg(BBL1_R_BBL_INT_EN_SEL);
	intr_sta = intr_en & intr_sta;

    if(intr_sta&0x02)
	{ 
		bbl_write_reg(BBL1_R_BBL_CLR_INT_SEL, 0x02);
		bbl_write_reg(BBL1_R_BBL_INT_EN_SEL, intr_en&(~0x02));//disable match intr	
	}    
	else
	{ 
		Soft_Reset();
	}
}

void s_BBL_init()
{
    s_SetIRQHandler(IRQ_BBL_RTC, s_bbl_intr);
    bbl_write_reg(BBL1_R_BBL_INT_EN_SEL,0x300); /*enable  Tamper_out_N*/
    bbl_write_reg(0X06,bbl_read_reg(BBL1_R_BBL_INT_STS_SEL));
    enable_irq(IRQ_BBL_RTC);    
}


void bbl_match_setting(uint seconds)
{
    uint val,tmp,n,m,sec;
    val = bbl_get_rtc_secs();
    sec = val&0x7f;//[BIT6~BIT0]
    val &=0x7fff80;//[BIT22~BIT7]
    val = val>>7;

    n = (seconds>>7);
    m = (seconds&0x7f);
    tmp = sec+m;
    if(n==0 && (tmp<64)) n=1;
    else if(tmp>64 && tmp<192) n+=1;
    else if(tmp>192 && tmp<256) n+=2;
    val += n;

    bbl_write_reg(BBL1_R_BBL_MATCH_SEL, val&0xffff);

    disable_irq(IRQ_BBL_RTC);
	s_SetIRQHandler(IRQ_BBL_RTC, s_bbl_intr);
    bbl_write_reg(BBL1_R_BBL_CLR_INT_SEL, 0x02);//clear match intr    
    val = bbl_read_reg(BBL1_R_BBL_INT_EN_SEL);
    bbl_write_reg(BBL1_R_BBL_INT_EN_SEL, val|0x02);//enable match intr
    enable_irq(IRQ_BBL_RTC);
//printk("*[%s-%d]**intr_en:%08x*\r\n", __FUNCTION__, __LINE__,bbl_read_reg(BBL1_R_BBL_INT_EN_SEL));    
}

void bbl_match_cls()
{
    uint val;

	disable_irq(IRQ_BBL_RTC);
    val = bbl_read_reg(BBL1_R_BBL_INT_EN_SEL);
    if(val & 0x02)
    {
    	bbl_write_reg(BBL1_R_BBL_INT_EN_SEL, val&(~0x02));//disable match intr
	}
	enable_irq(IRQ_BBL_RTC);
}

#if DEBUG_BBL
void bbl_test()
{
	uchar ch;
	uint buf[50],i;
	static uint sensorSW=0;
	int ret;
	while(1)
	{
		printk("1--read\r\n");
		printk("2--write\r\n");
		printk("3--unlock\r\n");
		printk("4--config\r\n");
		printk("5--get status\r\n");
		printk("6--get register value\r\n");
		ch=DebugRecv();
		switch(ch)
		{
			case 0x31:
				memset(buf,0,sizeof(buf));
				ret=bbl_read(0,8,buf);
				printk("read:ret=%d,%08x-%08x!\r\n",ret,buf[0],buf[1]);
				break;
			case 0x32:
				buf[0]=0x12345678;
				buf[1]=0xabcdef01;
				ret=bbl_write(0,8,buf);
				printk("write:ret=%d,%08x-%08x!\r\n",ret,buf[0],buf[1]);
				break;
			case 0x33:
				bbl_unlock();
				break;
			case 0x34:

				printk("1--enable dync sensor,      a--diable\r\n");
				printk("2--enbale static sensor,    b--disable\r\n");
				printk("3--enable temper sensor,    c--disable\r\n");
				printk("4--enable frequence sensor, d--disable \r\n");
				printk("5--enable voltgage sensor , e--disable\r\n");
				printk("6--enable all ,             f--disable all\r\n");
				ch=DebugRecv();
				if(ch>=0x31 && ch <=0x35) sensorSW |=1<<(ch-0x31);
				if(ch>='a' && ch <='e')   sensorSW &=~(1<<(ch-'a'));
				if(ch ==0x36) sensorSW=0xf001f;
				if(ch =='f')  sensorSW =0;

				if(ch==0x31)
				{
					printk("1--enable dync sensor1\r\n");
					printk("2--enable dync sensor2\r\n");
					printk("3--enable dync sensor3\r\n");
					printk("4--enable dync sensor4\r\n");
					printk("5--enable dync sensor all\r\n");
					ch=DebugRecv();
					if(ch<=0x34)sensorSW |=1<<((ch-0x31)+16);
					else sensorSW |=0xf0000;
				}

				bbl_config(sensorSW);
				printk("current sensorSW:%x!\r\n",sensorSW);
				break;
			case 0x35:
				bbl_get_status();
				break;
			case 0x36:
				for(i=0;i<0x18;i++)
				bbl_read_reg(i);
				break;

		}
	}

}
#endif

