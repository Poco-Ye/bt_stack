#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "sm_chip_thk88.h"
#include "posapi.h"
#include "platform.h"
#include "..\encrypt\CRC.h"
#include "sm_soft.h"
#include "base.h"
//-----------------------------------------------------------------------------
//registers for spi
#define PL022_CR0   (0x00)
#define PL022_CR1   (0x04)
#define PL022_CPSR  (0x10)

static SM_Config *SMConfig = NULL;

SM_Config SxxxSM_Ver = {
	.sm_spi_cs_port = GPIOD,
	.sm_spi_cs_pin = 8,

};

SM_Config SxxxSM_Ver2 = {
	.sm_spi_cs_port = GPIOB,
	.sm_spi_cs_pin = 30,
};

// 用于保存SPI 当前的配置参数
static uint spiCR0;
static uint spiCR1;
static uint spiCRSR;		
struct gm_spi_opt gtGmGpiOpt=
{
	
};
struct gm_spi_opt *gptGmSpiOpt = &gtGmGpiOpt;
#define SM_SPI_CHANNEL		1

static uchar is_sm_module(void);

static void save_spiconf(int channel)
{
	uint base;
	
	if(channel ==0) base =START_SPI0;
	else if(channel ==1) base =START_SPI1;
	else if(channel ==2) base =START_SPI2;
	else if(channel ==3) base =START_SPI3_CFG;
	else base = SPI4_REG_BASE_ADDR;

	spiCR0 = readl(base+PL022_CR0);
	spiCR1 = readl(base+PL022_CR1);
	spiCRSR = readl(base+PL022_CPSR);
}

static void restore_spiconf(int channel)
{
	uint base;
	
	if(channel ==0) base =START_SPI0;
	else if(channel ==1) base =START_SPI1;
	else if(channel ==2) base =START_SPI2;
	else if(channel ==3) base =START_SPI3_CFG;
	else base = SPI4_REG_BASE_ADDR;

	/* Set CR0 params */
	writel(spiCR0, (base+PL022_CR0));

	writel(spiCR1, (base+PL022_CR1));

	/* Set prescale divisor */
	writel(spiCRSR, (base+PL022_CPSR));	
}

void thk888_spi_init(void)
{
	 /*初始化SPI，配置为1M速率*/
	 if(get_machine_type()==S500||get_machine_type()==S300)
	 {
		SMConfig = &SxxxSM_Ver2;
	 }
	 else
	 {
		SMConfig = &SxxxSM_Ver;
	 }
	gpio_set_pin_type(GPIOA,11,GPIO_OUTPUT); /*配置SPI_NCS口线*/
	gpio_set_pin_type(SM_SPI_CS,GPIO_OUTPUT);
	gpio_set_pin_val(GPIOA,11,1) ;//disable rf spi
	gpio_set_pin_val(SM_SPI_CS,1) ;//disable sm spi	
	
	gpio_set_pin_type(GPIOA,8,GPIO_FUNC0);   /*配置SPI_CLK口线*/
	gpio_set_pin_type(GPIOA,9,GPIO_FUNC0);   /*配置SPI_MISO口线*/	
	gpio_set_pin_type(GPIOA,10,GPIO_FUNC0);  /*配置SPI_MOSI口线*/

	writel(0, (START_SPI1+PL022_CR1));/*SPI Disable ,调试时发现在使能状态下修改模式不会立即生效*/
	DelayUs(1);
	/*Phase =1,clkpol = 1,配置SPI速率为1M*/
	spi_config(SM_SPI_CHANNEL, 1000000, 8, 3);
	DelayUs(1);
}
void thk888_spi_set_cs(int v)
{
	if(v==0)//enable cs
	{
		gpio_set_pin_val(SM_SPI_CS,0) ;
	}
	else
	{
		gpio_set_pin_val(SM_SPI_CS,1) ; 
	}
}
void thk888_spi_enable(void) 
{
	save_spiconf(SM_SPI_CHANNEL);		// 先保存使用的SPI 的当前参数
	thk888_spi_init();
	thk888_spi_set_cs(0);//low enable
/*关于THK88芯片的休眠唤醒，使能SPI接口的SSN唤醒信号后，
到系统唤醒正常工作，期间需要约582个系统时钟周期。
您这边是否可以验证一下，是否因为这个问题引起的系统休眠无法唤醒。	
*/

// 压力测试过程出错，修改参数
	DelayUs(50);				// >(582/30)
}
void thk888_spi_disable(void)
{
	thk888_spi_set_cs(1);
	// 压力测试过程出错，修改参数
	DelayUs(30);
	writel(0, (START_SPI1+PL022_CR1));	/*SPI Disable ,调试时发现在使能状态下修改模式不会立即生效*/
	DelayUs(1);	
	restore_spiconf(SM_SPI_CHANNEL);  // 恢复SPI 参数到调用前状态
	
	//if (GetTsType()>0) enable_ts_spi();
}
void thk888_spi_send(unsigned char ucData)
{
	unsigned short val;

	val = spi_txrx(SM_SPI_CHANNEL, ucData, 8);
	DelayUs(5);
}
void thk888_spi_recv(unsigned char *pData)
{
	unsigned short cc;
	
	cc = spi_txrx(SM_SPI_CHANNEL, 0xffff, 8);
	*pData = (unsigned char)cc;	
	DelayUs(5);
}
//-----------------------------------------------------------------------------
/*THK888 SPI 通讯代码*/
// 16 位累加和校验
static unsigned short LRC16(unsigned char *pb, unsigned short Length)
{
	unsigned short Lrc16 = 0;

	while(Length--)
		Lrc16 += *pb++;

	return Lrc16;
}
//-----------------------------------------------------------------------------
// 接收数据包封装有效性识别
static int CheckReceive(unsigned char *pbuf, unsigned short length)
{
	unsigned char status[2];
	unsigned short DataLen,lrc16 = 0;
	
	if (pbuf==NULL || length<4)
	{
		return 0;
	}

	if (pbuf[0]!=SPI_BEGIN_DATA)
	{
		return 0;
	}
	status[0] = pbuf[1];
	status[1] = pbuf[2];
	DataLen = pbuf[3];
	DataLen <<= 8;
	DataLen += pbuf[4];
	if ((DataLen+8)!=length)
	{
		return 0;
	}
	lrc16 = pbuf[length-3];
	lrc16 <<= 8;
	lrc16 += pbuf[length-2];
	if (lrc16 != LRC16(&pbuf[1], length-4))
	{
		return 0;
	}
	if (pbuf[length-1]!=SPI_END_DATA)
	{
		return 0;
	}

	return 1;
}
//-----------------------------------------------------------------------------
static int check_lead(unsigned char *buf, unsigned short expect_len)
{
	unsigned char status[2];
	unsigned short length;
	
	if (buf[0]!=SPI_BEGIN_DATA)
		return SM_RET_TIMEOUT;
	status[0] = buf[1];
	status[1] = buf[2];
	length = buf[3]*256+buf[4];
	if (status[0]!=0||length!=expect_len)
		return SM_RET_ERROR;
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
void thk888_send(struct gm_spi_opt *opt, unsigned char *sbuf, unsigned int slen)
{
	unsigned int i;
	unsigned char *p;
	
	if (opt==NULL || sbuf==NULL || slen==0) return;
	i = slen;
	p = sbuf;
	while(i--)
		opt->spi_send(*p++);
}
//-----------------------------------------------------------------------------
void thk888_recv(struct gm_spi_opt *opt, unsigned char *rbuf, unsigned int rlen)
{
	unsigned int i;
	unsigned char *p;
	
	if (opt==NULL || rbuf==NULL || rlen==0) return;
	i = rlen;
	p = rbuf;
	while(i--)
		opt->spi_recv(p++);
}
//-----------------------------------------------------------------------------
int thk888_recv_sync(struct gm_spi_opt *opt, unsigned char *rbuf, unsigned int timeoutMs)
{
	unsigned int times;
	
	if (opt==NULL || rbuf==NULL) return SM_RET_INVALID_PARAM;
	times = timeoutMs/2;
	do{
		DelayMs(2);
		opt->spi_recv(rbuf);
		if ((*rbuf)==SPI_BEGIN_DATA) return SM_RET_OK;
	}while(times--);
	return SM_RET_TIMEOUT;
}
//-----------------------------------------------------------------------------
// 读取THK888 固件版本信息
int Thk888_GetVersion(unsigned short *pVer) 
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[256];
	unsigned int i;
	unsigned char status[2];
	unsigned short Length,expect_len;
	unsigned short Version;
	int result;

	if (opt==NULL)
	{
			return SM_RET_ERROR;
	}
	if (pVer==NULL)
		return SM_RET_INVALID_PARAM;
		
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_BASE;
	//buf[i++] = GM_CMD_BASE_GET_VERSION; 
	buf[i++] = 0x07; 
	buf[i++] = 0x00;
	buf[i++] = 0x01;
	buf[i++] = 0x00;
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	DelayMs(15);
	thk888_recv(opt, buf, 5);	
	expect_len = 2;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	Version = buf[5];
	Version <<= 8;
	Version += buf[6];
	*pVer = Version;
	Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// 读取THK888 固件版本信息
int Thk888_Sleep(void) 
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[256];
	unsigned int i;
	unsigned char status[2];
	unsigned short expect_len;
	int result;
	
	if (opt==NULL)
	{
			return SM_RET_ERROR;
	}
		
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_BASE;
	//buf[i++] = GM_CMD_BASE_GET_VERSION; 
	buf[i++] = 0x08; 
	buf[i++] = 0x00;
	buf[i++] = 0x01;
	buf[i++] = 0x00;
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	DelayMs(15);
	opt->spi_disable();

	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// 生成SM2 密钥对
int SM2GenKeyPair_hw(ST_SM2_KEY  *key )
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
			return SM_RET_ERROR;
	}
	if (key==NULL)
		return SM_RET_INVALID_PARAM;
		
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM2;
	buf[i++] = GM_CMD_SM2_GEN_KEYPAIR;
	buf[i++] = 0;		// 无参数
	buf[i++] = 0;	
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);	
	DelayMs(50);
	thk888_recv(opt, buf, 5);
	expect_len = 96;					// 64字节公鉏，32字节私钥
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	k = 5;
	memcpy(key->pubkey, &buf[k], 64);		// 64字节公钥
	k += 64;
	memcpy(key->privkey, &buf[k], 32);

	Thk888_Sleep();
	
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM2 签名
int SM2Sign_hw(ST_SM2_INFO *info, unsigned char *data, unsigned int datalen, 
unsigned char *sign, unsigned int *signlen)
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[2048+512];
	unsigned int i, j, k;
	unsigned char status[2];
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}
	if (info==NULL || data==NULL || sign==NULL || signlen==NULL || info->idalen>USRID_MAX || datalen>MSG_MAX)
		return SM_RET_INVALID_PARAM;
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM2;
	buf[i++] = GM_CMD_SM2_SIGN;
	k = 64+32+2+info->idalen+2+datalen;
	buf[i++] = k>>8;
	buf[i++] = k;
	j = 64;
	k = 0;
	while(j--)
		buf[i++] = info->key.pubkey[k++];
	j = 32;
	k = 0;
	while(j--)
		buf[i++] = info->key.privkey[k++];	
	buf[i++] = info->idalen>>8;
	buf[i++] = info->idalen;
	j = info->idalen;
	k = 0;
	while(j--)
		buf[i++] = info->ida[k++];
	buf[i++] = datalen>>8;
	buf[i++] = datalen;
	j = datalen;
	k = 0;
	while(j--)
		buf[i++] = data[k++];
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	// 压力测试过程出错，修改参数
	// SM2Sign 执行时间不稳定，增大延时
	DelayMs(50+25);
	thk888_recv(opt, buf, 5);	
	expect_len = 64;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	memcpy(sign, &buf[5], expect_len);
	*signlen = expect_len;

	Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM2 验签
int SM2Verify_hw(ST_SM2_INFO *info, unsigned char *data, unsigned int datalen,
unsigned char *sign, unsigned int signlen)
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[2048+512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}
	if (info == NULL || data==NULL || info->idalen>USRID_MAX || datalen>MSG_MAX || signlen>64)
		return SM_RET_INVALID_PARAM;
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM2;
	buf[i++] = GM_CMD_SM2_VERIFY;
	// Pubkey[64]-IdLen[2]-Id[n]-MsgLen[2]-Msg[n]-SignData[64]
	k = 64+2+info->idalen+2+datalen+2+signlen; 		
	buf[i++] = k>>8;
	buf[i++] = k;
	j = 64;
	k = 0;
	while(j--)
		buf[i++] = info->key.pubkey[k++];
	buf[i++] = info->idalen>>8;
	buf[i++] = info->idalen;
	j = info->idalen;
	k = 0;
	while(j--)
		buf[i++] = info->ida[k++];
	buf[i++] = datalen>>8;
	buf[i++] = datalen;
	j = datalen;
	k = 0;
	while(j--)
		buf[i++] = data[k++];
	j = signlen;
	buf[i++] = signlen>>8;
	buf[i++] = signlen;
	k = 0;
	while(j--)
		buf[i++] = sign[k++];
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	DelayMs(90);
	thk888_recv(opt, buf, 5);
	expect_len = 0;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM2 加/ 解密
int SM2Recover_hw(ST_SM2_KEY *key , unsigned char *datain,
unsigned int datainlen, unsigned char *dataout, unsigned int *dataoutlen, unsigned int mode )
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[2048+512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}
	if (key == NULL || datain == NULL || dataout == NULL || dataoutlen == NULL || mode>1)
		return SM_RET_INVALID_PARAM;
	if (mode==0)
	{
		if (datainlen>SM2_DECRYPT_MAX || datainlen<96)
			return SM_RET_INVALID_PARAM;
	}
	else
	{
		if (datainlen>SM2_ENCRYPT_MAX)
			return SM_RET_INVALID_PARAM;
	}
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM2;
	buf[i++] = GM_CMD_SM2_RECOVER;
	k = 64+32+1+2+datainlen; 		
	buf[i++] = k>>8;
	buf[i++] = k;
	j = 64;
	k = 0;
	while(j--)
		buf[i++] = key->pubkey[k++];
	j = 32;
	k = 0;
	while(j--)
		buf[i++] = key->privkey[k++];	
	buf[i++] = mode;
	buf[i++] = datainlen>>8;
	buf[i++] = datainlen;
	j = datainlen;
	k = 0;
	while(j--)
		buf[i++] = datain[k++];
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);	
	DelayMs(100);
	thk888_recv(opt, buf, 5);	
	if (mode==0) expect_len = datainlen-96; 	// 解密返回数据长度= 密文长度- 96 字节
	else	expect_len = datainlen+96;			// 加密返回数据长度= 明文长度+ 96 字节
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	memcpy(dataout, &buf[5], expect_len);
	*dataoutlen = expect_len;
	Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM3 hash 
int SM3_hw(unsigned char *key, unsigned int keylen ,
unsigned char *datain, unsigned int datainlen, unsigned char *dataout )
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[SM3_BUF_MAX+512];
	unsigned int i, j, k;
	unsigned char status[2];
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}	
	if (key!=NULL || datain==NULL || dataout==NULL)
		return SM_RET_INVALID_PARAM;
	if (datainlen>((0)?(SM3_INPUT_MAX):(SM3_BUF_MAX)))
		return SM_RET_INVALID_PARAM;
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM3;
	buf[i++] = GM_CMD_SM3_HASH;
	k = 2+datainlen; 		
	buf[i++] = k>>8;
	buf[i++] = k;
	buf[i++] = datainlen>>8;
	buf[i++] = datainlen;
	j = datainlen;
	k = 0;
	while(j--)
		buf[i++] = datain[k++];	
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	DelayMs(60);
	thk888_recv(opt, buf, 5);	
	expect_len = 32;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	memcpy(dataout, &buf[5], expect_len);
	Thk888_Sleep();
	return SM_RET_OK;
}
// SM3 hash 开始计算(迭代方式)
int SM3Start_hw(void)
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[SM3_BUF_MAX+512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}	
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM3;
	buf[i++] = GM_CMD_SM3_START;		
	buf[i++] = 0x00;
	buf[i++] = 0x00;
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	DelayMs(25);
	thk888_recv(opt, buf, 5);
	expect_len = 0;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	//Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM3 hash 计算64 字节(迭代方式)
int SM3Update_hw(unsigned char *datain)
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[SM3_BUF_MAX+512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}	
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM3;
	buf[i++] = GM_CMD_SM3_UPDATE;		
	buf[i++] = 0x00;
	buf[i++] = 64;
	j = 64;
	k = 0;
	while(j--)
		buf[i++] = datain[k++];	
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	//DelayMs(50);
	DelayMs(3);
	thk888_recv(opt, buf, 5);
	expect_len = 0;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	//Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM3 hash 计算最后一次，并得到结果(迭代方式)
int SM3Final_hw(unsigned char *datain, unsigned int datainlen, unsigned char *dataout)
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[SM3_BUF_MAX+512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}	
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM3;
	buf[i++] = GM_CMD_SM3_FINAL;	
	k = datainlen+2;
	buf[i++] = k>>8;
	buf[i++] = k;
	buf[i++] = datainlen>>8;
	buf[i++] = datainlen;	
	j = datainlen;
	k = 0;
	while(j--)
		buf[i++] = datain[k++];	
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);	
	//DelayMs(50);
	DelayMs(25);
	thk888_recv(opt, buf, 5);
	expect_len = 32;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	memcpy(dataout, &buf[5], expect_len);
	//Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM3 hash 
int SM3Stack_hw(unsigned char *key, unsigned int keylen ,
unsigned char *datain, unsigned int datainlen, unsigned char *dataout )
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[SM3_BUF_MAX+512];
	unsigned int i, j, k;
	unsigned char status[2];
	unsigned short Length;
	int Result;
	unsigned int calclen,pos;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}	
	if (key!=NULL || datain==NULL || dataout==NULL)
		return SM_RET_INVALID_PARAM;
	//if (datainlen>0xffff || datainlen==0)
	if (datainlen==0)
		return SM_RET_INVALID_PARAM;

	Result = SM3Start_hw();
	if (Result!=SM_RET_OK)
		return Result;
	pos = 0;
	while(datainlen>0)
	{
		if (datainlen>64)
		{
			calclen = 64;
			Result = SM3Update_hw(&datain[pos]);
		}
		else
		{
			calclen = datainlen;
			Result = SM3Final_hw(&datain[pos], calclen, dataout);
		}
		if (Result!=SM_RET_OK)
			return Result;
		datainlen -= calclen;
		pos += calclen;
	}
	Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
// SM4
int SM4_hw(unsigned char *datain, unsigned int datainlen, unsigned char *iv,
			unsigned char *key, unsigned char *dataout, unsigned int mode )
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[SM4_BUF_MAX+512];
	unsigned int i, j, k;
	unsigned short expect_len;
	int result;

	if (opt==NULL)
	{
		return SM_RET_ERROR;
	}
	if (datain==NULL || key==NULL || dataout==NULL || mode>3)
		return SM_RET_INVALID_PARAM;
	if (mode>=2 && iv==NULL)
		return SM_RET_INVALID_PARAM;
	if (datainlen>SM4_BUF_MAX)
		return SM_RET_INVALID_PARAM;
	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA;
	buf[i++] = GM_CMD_TYPE_SM4;
	buf[i++] = GM_CMD_SM4_RECOVER;
	k = 2+datainlen+1+16+16; 		
	buf[i++] = k>>8;
	buf[i++] = k;
	buf[i++] = datainlen>>8;
	buf[i++] = datainlen;
	j = datainlen;
	k = 0;
	while(j--)
		buf[i++] = datain[k++];
	buf[i++] = mode;
	j = 16;
	k = 0;
	while(j--)
		buf[i++] = (mode>=2)?iv[k++]:0x00;	
	j = 16;
	k = 0;
	while(j--)
		buf[i++] = key[k++];	
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	thk888_send(opt, buf, i);
	DelayMs(100);
	thk888_recv(opt, buf, 5);	
	expect_len = datainlen;
	result = check_lead(buf, expect_len);
	if (result)
	{
		opt->spi_disable();
		return result;
	}		
	thk888_recv(opt, &buf[5], expect_len+3);   // expect_len+LRC16+END
	opt->spi_disable();
	if (!CheckReceive(buf, expect_len+8))
		return SM_RET_ERROR;
	memcpy(dataout, &buf[5], expect_len);
	Thk888_Sleep();
	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
/*-------国密算法API-------
一. 满足以下几个条件时，采用THK888 方式执行算法功能
	1. 配置有THK888 芯片
	2. 数据长度没有符合THK888 限制条件
	3. 通讯数据包不会超过BUFFER 限制条件
二. 其它情况下采用软件库国密算法
*/
//-----------------------------------------------------------------------------
int SM2GenKeyPair(ST_SM2_KEY  *key )
{
	if (!is_sm_module())
	{
		return SM2GenKeyPair_sw(key);
	}
	else
	{
		return SM2GenKeyPair_hw(key);
	}
}
//-----------------------------------------------------------------------------
int SM2Sign(ST_SM2_INFO *info, unsigned char *data, unsigned int datalen, 
unsigned char *sign, unsigned int *signlen)
{
	if (info==NULL || data==NULL || sign==NULL || signlen==NULL || info->idalen>USRID_MAX || datalen>MSG_MAX)
		return SM_RET_INVALID_PARAM;
		
	if (info->idalen>USRID_MAX || datalen>MSG_MAX || !is_sm_module())
		return SM2Sign_sw(info, data, datalen, sign, signlen);
	else
		return SM2Sign_hw(info, data, datalen, sign, signlen);
}
//-----------------------------------------------------------------------------
int SM2Verify(ST_SM2_INFO *info, unsigned char *data, unsigned int datalen,
unsigned char *sign, unsigned int signlen)
{
	if (info == NULL || data==NULL || info->idalen>USRID_MAX || datalen>MSG_MAX || signlen>64)
		return SM_RET_INVALID_PARAM;
		
	if ( info->idalen>USRID_MAX || datalen>MSG_MAX || !is_sm_module())
		return SM2Verify_sw(info, data, datalen, sign, signlen);
	else
		return SM2Verify_hw(info, data, datalen, sign, signlen);
}
//-----------------------------------------------------------------------------
int SM2Recover(ST_SM2_KEY *key , unsigned char *datain, unsigned int datainlen,
unsigned char *dataout, unsigned short *dataoutlen, unsigned int mode)
{
	unsigned int dataoutlen_t;    // PED接口和底层SM接口指针类型不一致，临时转换
	int result = SM_RET_ERROR;
	
	if (key == NULL || datain == NULL || dataout == NULL || dataoutlen == NULL || mode>1)
		return SM_RET_INVALID_PARAM;
		
	if (mode==0)
	{
		if (datainlen>1024)
			return SM_RET_INVALID_PARAM;
			
		if (datainlen>SM2_DECRYPT_MAX || !is_sm_module())
			result = SM2Recover_sw(key, datain, datainlen, dataout, &dataoutlen_t, mode);
		else
			result =  SM2Recover_hw(key, datain, datainlen, dataout, &dataoutlen_t, mode);
	}
	else
	{
		if (datainlen>(1024-96))
			return SM_RET_INVALID_PARAM;
			
		if (datainlen>SM2_ENCRYPT_MAX || !is_sm_module())
			result =  SM2Recover_sw(key, datain, datainlen, dataout, &dataoutlen_t, mode);
		else
			result =  SM2Recover_hw(key, datain, datainlen, dataout, &dataoutlen_t, mode);
	}
	if (result==SM_RET_OK) *dataoutlen = (unsigned short)dataoutlen_t;

	return result;
}
//-----------------------------------------------------------------------------
int SM3(unsigned char *key, unsigned int keylen ,
unsigned char *datain, unsigned int datainlen, unsigned char *dataout )
{
	if (key!=NULL || !is_sm_module())
		return SM3_sw(key, keylen, datain, datainlen, dataout);
	else
		return SM3Stack_hw(key, keylen, datain, datainlen, dataout);
}
//-----------------------------------------------------------------------------
int SM4(unsigned char *datain, unsigned int datainlen, unsigned char *iv,
			unsigned char *key, unsigned char *dataout, unsigned int mode )
{
	if (datainlen>8192)	return SM_RET_INVALID_PARAM;
	if ((datainlen%16)!=0) return SM_RET_INVALID_PARAM;
	
	if (datainlen>SM4_BUF_MAX || !is_sm_module())
		return SM4_sw(datain, datainlen, iv, key, dataout, mode);
	else
		return SM4_hw(datain, datainlen, iv, key, dataout, mode);
}
//-----------------------------------------------------------------------------
static int Thk888EnterBootMode(void)
{
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned short lrc16 = 0;
	unsigned char buf[512];
	unsigned short i, j, k;
	unsigned char status[2];
	unsigned short Length;

	i = 0;
	opt->spi_enable();
	buf[i++] = SPI_BEGIN_DATA; 
	buf[i++] = 0xff;
	buf[i++] = 0x01;
	buf[i++] = 0x00;
	buf[i++] = 0x00;
	lrc16 = LRC16(&buf[1], i-1);
	buf[i++] = lrc16>>8;
	buf[i++] = lrc16;
	buf[i++] = SPI_END_DATA;
	j = 0;
	while(i--)
		opt->spi_send(buf[j++]);		
		
	opt->spi_disable();

	DelayMs(2000);

	return SM_RET_OK;
}
//-----------------------------------------------------------------------------
int SM_thk88_writeFlash(unsigned char *dat, int len)
{
	unsigned char tx_buf[1024];
	T_SOFTTIMER timer;
	int i;
	unsigned short mod,cnt;
	unsigned long pageaddr;
	#define 	PAGE_SIZE	512
	unsigned char ch;
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned int s,j;	
	
	if(dat == NULL || len<=0) return -1;
	opt->spi_enable();
	mod = len%PAGE_SIZE;
	cnt = len/PAGE_SIZE;	
	pageaddr = 0x00000200;
	for(i=0;i<cnt;i++)
	{
		tx_buf[0]=0x02;
		tx_buf[1]=(unsigned char)(pageaddr>>24);
		tx_buf[2]=(unsigned char)(pageaddr>>16);
		tx_buf[3]=(unsigned char)(pageaddr>>8);
		tx_buf[4]=(unsigned char)(pageaddr);//page start addr
		tx_buf[5]=0x02;tx_buf[6]=0x00;//page size
		memcpy(&tx_buf[7], &dat[i*PAGE_SIZE], PAGE_SIZE);//data
		s = 7+PAGE_SIZE;
		j = 0;
		while(s--)
			opt->spi_send(tx_buf[j++]);
		DelayMs(5);
		s_TimerSetMs(&timer, 1000);
		while(1)
		{
			opt->spi_send(0x05);
			DelayMs(2);
			opt->spi_recv(&ch);
			if(ch == 0x01)//write finish
			{
				break;
			}
			if(s_TimerCheckMs(&timer) == 0)
			{
				opt->spi_disable();
				return -13;
			}
			DelayMs(10);
		}
		pageaddr += PAGE_SIZE;
	}
	if(mod>0)
	{
		tx_buf[0]=0x02;
		tx_buf[1]=(unsigned char)(pageaddr>>24);
		tx_buf[2]=(unsigned char)(pageaddr>>16);
		tx_buf[3]=(unsigned char)(pageaddr>>8);
		tx_buf[4]=(unsigned char)(pageaddr);//page start addr
		tx_buf[5]=(unsigned char)(mod>>8);tx_buf[6]=(unsigned char)(mod);//data size
		memcpy(&tx_buf[7], &dat[cnt*PAGE_SIZE], mod);//data
		s = 7+mod;
		j = 0;
		while(s--)
			opt->spi_send(tx_buf[j++]);
		DelayMs(5);
		s_TimerSetMs(&timer, 1000);
		while(1)
		{
			opt->spi_send(0x05);
			DelayMs(2);
			opt->spi_recv(&ch);
			if(ch == 0x01)//write finish
			{
				break;
			}
			if(s_TimerCheckMs(&timer) == 0)
			{
				opt->spi_disable();
				return -13;
			}
			DelayMs(10);
		}
	}
	opt->spi_disable();
	return 0;
}
//-----------------------------------------------------------------------------
int SM_thk88_eraseFlash( void )
{
	unsigned char tx_buf[1024],rx_buf[100];
	T_SOFTTIMER timer;
	unsigned char ch;
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned int i,j;

	opt->spi_enable();

	tx_buf[0]=0x20;
	tx_buf[1]=0x00;tx_buf[2]=0x00;tx_buf[3]=0x02;tx_buf[4]=0x00;
	tx_buf[5]=0x00;tx_buf[6]=0xc8;//erase page number 100k=200 page 4ms/page
	i = 7;
	j = 0;
	while(i--)
		opt->spi_send(tx_buf[j++]);
	DelayMs(800);///
	//read status
	s_TimerSetMs(&timer, 1000);
	while(1)
	{
		opt->spi_send(0x05);
		DelayMs(2);
		opt->spi_recv(&ch);
		if(ch == 0x02)//erase finish
		{
			break;
		}
		if(s_TimerCheckMs(&timer) == 0)
		{
			opt->spi_disable();
			return -2;
		}
	}
	opt->spi_disable();

	return 0;
}
//-----------------------------------------------------------------------------
int SM_thk88_writeVectorPage(unsigned char dat[512], int len)
{
	unsigned char tx_buf[1024];
	T_SOFTTIMER timer;
	int i;
	#define 	PAGE_SIZE	512
	unsigned char ch;
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned s,j;
	
	if(dat == NULL || len!=512) return -1;

	opt->spi_enable();
	
	tx_buf[0]=0x02;
	tx_buf[1]=0x03;
	tx_buf[2]=0x00;
	tx_buf[3]=0x00;
	tx_buf[4]=0x00;//page start addr
	tx_buf[5]=0x02;tx_buf[6]=0x00;//page size
	memcpy(&tx_buf[7], dat, PAGE_SIZE);//data
	s = 7+PAGE_SIZE; //5us*512
	j = 0;
	while(s--)
		opt->spi_send(tx_buf[j++]);
	DelayMs(5);
	s_TimerSetMs(&timer, 1000);
	while(1)
	{
		opt->spi_send(0x05);
		DelayMs(1);
		opt->spi_recv(&ch);
		if(ch == 0x01)//write finish
		{
			break;
		}
		if(s_TimerCheckMs(&timer) == 0)
		{
			opt->spi_disable();
			return -13;
		}
		DelayMs(10);
	}
	opt->spi_disable();
	return 0;
}
//-----------------------------------------------------------------------------
int SM_thk88_read_verifyFlash(int type,unsigned char *dat, int len)
{
	unsigned char buf[1024];
	struct gm_spi_opt *opt = gptGmSpiOpt;
	unsigned int datalen,offset,calclen,i,s;

	datalen = len;
	offset = (type==0)?(0):(512);
	opt->spi_enable();
	while(datalen>0)
	{
		if (datalen>512)
			calclen = 512;
		else
			calclen = datalen;
		buf[0]=0x03;
		buf[1]=offset>>24;
		buf[2]=offset>>16;
		buf[3]=offset>>8;
		buf[4]=offset;				//flash addr
		buf[5]=(unsigned char)(calclen>>8);
		buf[6]=(unsigned char)(calclen);//read len
		s = 7;
		i = 0;
		while(s--)
			opt->spi_send(buf[i++]);
		DelayMs(20);
		s = calclen;
		i = 0;
		while(s--)
			opt->spi_recv(&buf[i++]);
		if (0==memcmp(&dat[offset], buf, calclen))
		{
			;
		}
		else
		{
			opt->spi_disable();
			return -1;
		}
		offset += calclen;
		datalen -= calclen;
		DelayMs(5);
	}
	opt->spi_disable();
	
	return 0;
}
//-----------------------------------------------------------------------------
int thk88_update_firmware(unsigned char *dat, int len)
{
	if(Thk888EnterBootMode()) 
	{
		return -1;
	}
	//printk("THK88 erasing...");
	if(SM_thk88_eraseFlash()) 
	{
		return -1;
	}
	if(SM_thk88_writeFlash(&dat[512], len-512))
	{
		return -1;
	}
	if (SM_thk88_read_verifyFlash(1, dat, len-512))
	{
		return -1;
	}	
	if (SM_thk88_writeVectorPage(dat, 512))
	{
		return -1;
	}
	if (SM_thk88_read_verifyFlash(0, dat, 512))
	{
		return -1;
	}
	
	return 0;
}
//-----------------------------------------------------------------------------
static int thk888_opt_init(unsigned char *Version)
{
	int retv;
	unsigned short Ver;
	
	memset(gptGmSpiOpt, 0x00, sizeof(struct gm_spi_opt));
	gptGmSpiOpt->spi_init    = thk888_spi_init;
	gptGmSpiOpt->spi_cs      = thk888_spi_set_cs;
	gptGmSpiOpt->spi_enable  = thk888_spi_enable;
	gptGmSpiOpt->spi_disable = thk888_spi_disable;
	gptGmSpiOpt->spi_send    = thk888_spi_send;
	gptGmSpiOpt->spi_recv    = thk888_spi_recv;

	retv = Thk888_GetVersion(&Ver);
	if (retv==SM_RET_OK && Ver<=99)
	{
		*Version = Ver;
		return 1;
	}
	else
		return 0;
}


static uchar smVersion = 0;
uchar GetSMFirmwareVer(void)
{
	return smVersion;
}

/*INFO :chip_type.sm firmware ver.soft lib ver*/
int SMGetModuleInfo(uchar *info)
{
    int len;
	char context[64];
	uchar ver[64];
    memset(context,0,sizeof(context));
    memset(ver,0,sizeof(ver));    
    len = ReadCfgInfo("CIPHER_CHIP",context);
    if(len<0) sprintf(ver, "00.00.%s",GetSmLibVer());    
	else{
        context[len] = 0;
        sprintf(ver, "%s.%02d.%s",context, GetSMFirmwareVer(), GetSmLibVer());
	}
	strcpy(info,ver);
	return strlen(ver);
}

uchar GetSMType(void)
{
	char context[64];
	static int SMType =-1;
    if(SMType!=-1) return SMType;
	
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("CIPHER_CHIP",context)<0) SMType = 0;
	else if (strstr(context, "01")) SMType = 1;			// THK888
	else SMType = 0;
	
    return SMType;
}

static uchar is_sm_module(void)
{
	return GetSMType()>0?1:0;
}


void s_SMInit(void)
{
	if (1 == GetSMType()) //thk888 chip
	{
		if(!thk888_opt_init(&smVersion))
			smVersion = 0;
	}
}

//-----------------------------------------------------------------------------

