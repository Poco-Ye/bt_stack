#include "base.h"
#include "../comm/comm.h"
#include "barcode.h"

#define P_SCAN_PORT  P_BARCODE

/* * * * * * * * * * * * * *  * * * * * * * * * * * * * */ 
/*                                                      */
/*					SCAN1D SE655 MODULE    				*/
/*                                                      */
/* * * * * * * * * * * * * *  * * * * * * * * * * * * * */
typedef struct t_barcode_ctl_io{
	int power_port;
	int power_pin;
	int trige_port;
	int trige_pin;
	int tx_port;
	int tx_pin;
	int rx_port;
	int rx_pin;
	int power_on;
	int power_off;
}T_BarCodeDrvConfig;

static T_BarCodeDrvConfig *ptBarCodeDrvCfg = NULL;

#define CFG_BAR_PWR()		gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,GPIO_OUTPUT)	
#define ENABLE_BAR_PWR()		{gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,ptBarCodeDrvCfg->power_on);DelayMs(10);}
#define DISABLE_BAR_PWR()	{gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,ptBarCodeDrvCfg->power_off);DelayMs(30);}

#define CODE_MAX_SIZE (256)
typedef struct
{
	uchar data[CODE_MAX_SIZE];
	uchar datalen;
	uchar opcode;
	uchar status;
	uchar no_use;
}SCAN1D_TDATA;

typedef struct
{
    uchar param[6];
    uchar len;
}SCAN1D_PARAM;

static const SCAN1D_PARAM Scan1dParam[] ={
"\xFF\xee\x01",    3, //packeted decode data
"\xFF\x53\x01",    3, //enable Bookland EAN  
"\xFF\xf1\x40\x01",4, //Bookland ISBN-13
"\xFF\x10\x02",    3, //Autodiscriminate UPC/EAN Supplementals 
"\xFF\x4D\x00",    3, //UPC/EAN Security Level 0          
"\xFF\x55\x01",    3, //enable UCC Coupon Extended Code
"\xFF\xf1\x69\x01",4, //enable ISSN EAN
"\xFF\x09\x01",    3, //Enable Code 93
"\xFF\x0A\x01",    3, //Enable Code 11
"\xFF\x06\x01",    3, //Enable Interleaved 2 of 5
"\xFF\x16\x00",    3, //Set L1 Lengths for Interleaved 2 of 5
"\xFF\x17\x00",    3, //Set L2 Lengths for Interleaved 2 of 5 
"\xFF\x05\x01",    3, //Enable Discrete 2 of 5         
"\xFF\xF0\x98\x01",4, //Enable Chinese 2 of 5        
"\xFF\xF1\x6A\x01",4, //Enable Matrix 2 of 5
"\xFF\x0c\x01",    3, //enable UPC-E1, UPC-E1 is not a UCC (Uniform Code Council) approved symbology       
"\xFF\xF1\x4A\x02",4, //Inverse 1D, Autodetect both regular and inverse 1D bar codes        
"\xFF\x07\x01",    3, //Enable Codabar
"\xFF\x0B\x01",    3, //Enable MSI
"\xFF\x11\x01",    3, //enable Code 39 Full ASCII 
"\xFF\x0D\x00",    3, //disable Trioptic Code 39             
"\xFF\x80\x01",    3, //lower power mode
"\xFF\x4E\x01",    3, //security level 0    
"\xFF\x8A\x00",    3, //level trigger mode
"\xFF\xd1\x00",    3, //Set L1 Lengths for code128, any lengths
"\xFF\xd2\x00",    3, //Set L2 Lengths for code128, any lengths 
"\xFF\x12\x00",    3, //Set L1 Lengths for code39
"\xFF\x13\x00",    3, //Set L2 Lengths for code39 
"\xFF\x1a\x00",    3, //Set L1 Lengths for code93
"\xFF\x1b\x00",    3, //Set L2 Lengths for code93 
"\xFF\x1c\x00",    3, //Set L1 Lengths for code11
"\xFF\x1d\x00",    3, //Set L2 Lengths for code11 
"\xFF\x14\x00",    3, //Set L1 Lengths for Discrete 2 of 5
"\xFF\x15\x00",    3, //Set L2 Lengths for Discrete 2 of 5 
"\xFF\xf1\x6b\x00",    4, //Set L1 Lengths for Matrix 2 of 5
"\xFF\xf1\x6c\x00",    4, //Set L2 Lengths for Matrix 2 of 5 
"\xFF\x18\x00",    3, //Set L1 Lengths for codebar
"\xFF\x19\x00",    3, //Set L2 Lengths for codebar
"\xFF\x1e\x00",    3, //Set L1 Lengths for MSI
"\xFF\x1f\x00",    3, //Set L2 Lengths for MSI
};


static ushort Scan1DCalcCrc(uchar *buf,int size)
{
    /*checksum 为数据的字节的和，再取补码 */
    int i;
    ushort checksum;

    checksum = 0;
    for (i = 0; i < size; i++)
    {
        checksum += buf[i];
    }

    checksum = (ushort) (~ (checksum)) + 1;   /*取补码 */
    return checksum;
}

static int SCAN1D_RecvPacket(SCAN1D_TDATA * resp)
{
    uchar buf[CODE_MAX_SIZE];
    int ret, i, len;
    ushort checksum1, checksum2;

    if (resp == NULL)
        return -2;             /* invalid package */
    memset(buf, 0x00, sizeof(buf));
    ret = PortRecv(P_SCAN_PORT, &buf[0], 100);
    if (ret) return -1;
    if (buf[0] < 4) return -2; /* invalid package */

    len = buf[0] + 2;
    for (i = 1; i < len; i++)
    {
        ret = PortRecv(P_SCAN_PORT, &buf[i], 100);
        if (ret) return -1;          /*time out */
    }
    checksum1 = Scan1DCalcCrc(buf, len - 2);
    checksum2 = buf[len - 2] * 256 + buf[len - 1];
    if (checksum1 != checksum2) return -4;              /*verify error */

    /************************************************************************************
        *DECODE_DATA:
        *len(1B)+opcode(1B)+message source(1B)+status(1B)+Barcode Type(1B)+data+checksum(2B)
	************************************************************************************/
    resp->opcode = buf[1];
    resp->status = buf[3];
    resp->datalen = len - 7;    /*remove:len,opcode, */
    memcpy(resp->data, buf + 5, resp->datalen);
    return 0;
}

static int SCAN1D_SendCmd(uchar opcode,uchar *data,uchar datalen)  
{
    int i, ret;
    SCAN1D_TDATA rx_data;
    uchar txBuf[CODE_MAX_SIZE];
    ushort checksum = 0, txLen;

    if(datalen)
    {
        if(data==NULL)return -1;
        if((datalen+6)>sizeof(txBuf))return -2;
    }

    txLen = datalen + 4;
    txBuf[0] = txLen;
    txBuf[1] = opcode;
    txBuf[2] = 0x04;            // host
    txBuf[3] = 0;
    memcpy(txBuf + 4, data, datalen);

    checksum = Scan1DCalcCrc(txBuf, txLen);
    txBuf[txLen] = checksum >> 8;
    txBuf[txLen + 1] = checksum & 0x00FF;
    for (i = 0; i < 3; i++)     /*retry 3 times */
    {
        PortReset(P_SCAN_PORT);
        PortSends(P_SCAN_PORT, txBuf, (txLen + 2));
        ret = SCAN1D_RecvPacket(&rx_data);
        if (ret < 0) continue;

        if (rx_data.opcode == SCAN1D_CMD_NAK
            && rx_data.data[0] == SCAN1D_NAK_RESEND)
            continue;

        break;
    }

    if (i == 3) return -3;    /*time out */
    return 0;
}

static int Scan1DDecode(uchar *resp, uint size)
{
    int ret, len = 0;
    SCAN1D_TDATA rx_data;
    uchar AckBuf[6];
    ushort checksum;

    ret = SCAN1D_RecvPacket(&rx_data);
    if (ret) return -1;
    if (rx_data.opcode != SCAN1D_DECODE_DATA)
        return -2;              /*invalid package */

    /*send ack to module */
    SCAN1D_SendCmd(SCAN1D_CMD_ACK, NULL, 0);

    if (size > rx_data.datalen)
        len = rx_data.datalen;
    else
        len = size;

    memcpy(resp, rx_data.data, len);
    return len;
}

void SCAN1D_SE655_Init()
{
	DISABLE_BAR_PWR();
	CFG_BAR_PWR();
}

static int SCAN1D_SE655_Open(void)
{
    int ret, i;

    ENABLE_BAR_PWR();
    if (PortOpen(P_SCAN_PORT, "9600,8,n,1") != 0x00)
        return -1;              /*port used */

    ret = SCAN1D_SendCmd(SCAN1D_SE_DEF_PARAM, NULL, 0); //Set Factory Defaults
    if (ret != 0)
        return -3;              /*module failure */

    DelayMs(500);//等待条码复位完成，测量值是300到400毫秒
    for (i = 0; i < sizeof(Scan1dParam) / sizeof(SCAN1D_PARAM); i++)
    {
        SCAN1D_SendCmd(SCAN1D_SEND_PARAM, Scan1dParam[i].param, Scan1dParam[i].len);
    }
    return 0;
}

int SCAN1D_SE655_Read(uchar *buf, uint size)
{
    int ret = 0;
    T_SOFTTIMER scan_timer;

    SCAN1D_SendCmd(SCAN1D_START_DECODE, NULL, 0);
    s_TimerSetMs(&scan_timer, 3000);
    while (s_TimerCheckMs(&scan_timer))
    {
        ret = PortPeep(P_SCAN_PORT, buf, 1);
        if (ret > 0) break;
    }
    if (ret > 0)
        ret = Scan1DDecode(buf, size);

    SCAN1D_SendCmd(SCAN1D_STOP_DECODE, NULL, 0);
    if (ret < 0) return -3;              //无解码数据 
    return ret;                 //读取数据成功，返回字符长度
}

void SCAN1D_SE655_Close()
{
    SCAN1D_SendCmd(SCAN1D_STOP_DECODE, NULL, 0);
    SCAN1D_SendCmd(SCAN1D_AIM_OFF, NULL, 0);    // 关闭瞄准功能
    SCAN1D_SendCmd(SCAN1D_SLEEP, NULL, 0);  // sleep，省电模式
    PortClose(P_SCAN_PORT);
    DISABLE_BAR_PWR();
}



/*****************************************************************/
/*                                                               */
/*                       SCANNER 1D MT780 MODULE                     */
/*                                                               */
/*****************************************************************/                       

#define START_SCAN         "{ }"
//设置版本号
#define MT_Set_Version "MT_V01"//格式:AB_VXX,AB代表版本,Vxx代表版本号
typedef struct
{
    uchar param[63];
	uchar len;
}MT780_CMD;
static uint ScanSet_flag = 0;

//更改设置命令后务必更新设置版本号MT_Set_Version
static const MT780_CMD MT780Cmd[] = {
"{MDEFWT}", 		8,		            //set to default
"{MC01WT6}",        9,                  //set to Serial Trigger Mode
"{MC02WT0,1,0}",   13,                  //send data length before ouput data
"{MC03WT0,1,0}",   13,                  //intercharacter delay 500us
"{MC10WT0,,,,,#5}",               16,   //no-read timeout:500ms
"{MB02WT1,1,1,#255,#255}",        23,   //enable code 32
"{MB07WT1,0,1,1,99,#255,#255}",   28,   //enable Standard 2/5
"{MB08WT1,0,1,1,99,#255,#255}",   28,   //enable Matrix 2/5
"{MB09WT1,0,1,1,99,#255,#255}",   28,   //enable Industrial 2/5
"{MB10WT1,0,1,0,1,99,#255,#255}", 30,   //enable Code 11 2/5
"{MB11WT1,0,1,1,99,#255,#255}",   28,   //enable MSI Plessey
"{MB12WT1,0,#255,#255}",          21,   //enable UK Plessey 
"{MB13WT1,1,#255,#255}",          21,   //enable Telepen
"{MB18WT1,1,99,#255,#255}",       24,   //enable code 93
"{MB21WT1,0,0,1,#255,#255}",      25,   //enable GS1 Databar   
"{MB22WT1,0,0,#255,#255}",        23,   //enable GS1 DataBar-Limited
"{MB23WT1,1,1,99,#255,#255}",     26,   //enable GS1 DataBar-Expanded
"{MB01WT1,0,1,0,1,99,#255,#255,1,#255,#255}", 42,   //Code 39 Status
"{MB03WT1,0,1,1,0,0,1,99,#255,#255}",         34,   //Codabar Status
"{MB05WT1,0,1,0,1,99,#255,#255}",             30,   //Interleaved 2/5 Status
"{MB06WT1,0,1,1,99,#255,#255}",               28,   //Toshiba 2/5
"{MB19WT1,1,99,#255,#255}",                   24,   //Code 128 Status
"{MCMDWT1}",                  9,      //save settings
};


static int SCAN1D_MT780_RecvPacket(uchar *dataOut, uint size, ushort CHAR_TIMEOUT, uchar flag)
{
	uchar buff[CODE_MAX_SIZE];
	int ret=-1, i=0, len;
	T_SOFTTIMER timer;

	memset(buff,0,sizeof(buff));
	
	//read timeout

	if(flag)
	    s_TimerSetMs(&timer, 500);//receive barcode timeout
	else
		s_TimerSetMs(&timer, 1000);//receive cmd timeout
	
	while (s_TimerCheckMs(&timer))
    {
        ret = PortPeep(P_SCAN_PORT, buff, 1);
        if (ret > 0) 
			break;
    }	
	if (ret <= 0) //timeout
		return -3;
	while(1)
	{
		ret = PortRecv(P_SCAN_PORT, &buff[i], CHAR_TIMEOUT);
		if (ret != 0) 
			break;
        if(i >= sizeof(buff)) 
			return -2;
        i++;
	}	

	if(flag)  //receive barcode
	{
	    if(i<=4)
			return -3;	
		len = (buff[0]-'0')*10 + (buff[1]-'0') + 4;  //前两个字符为条码长度
		if( len != i)
			return -2;	
		if (buff[i-2] == 13 && buff[i-1] == 10)  //最后两个字符为CR+LF
		{	
		    if(size > (i-4)) 
			    len = i-4;
		    else 
			    len = size;
		    memcpy(dataOut, buff+2, len);
		    return len;
	    }
	    else
			return -2;
	}
	else    //receive command
	{
	    if(buff[0]=='{' && buff[i-1]=='}')
	    {
	        memcpy(dataOut, buff, i);
			return 0;
	    }
		else
			return -3;
	}

}


void SCAN1D_MT780_Init()
{	
    DISABLE_BAR_PWR();
	CFG_BAR_PWR();
}


int SCAN1D_MT780_Open (void)
{   
    int i, j=0, ret=-2;
	uchar rx_buf[CODE_MAX_SIZE],buf[20];
	memset(buf, 0, sizeof(buf));
	ret=SysParaRead(BARCODE_SETTING_INFO,buf);
	if(ret==0&&(!strncmp(buf,MT_Set_Version,6)))
	{
		ScanSet_flag = 1;
	}

	ENABLE_BAR_PWR();
	DelayMs(20);
	
    if (PortOpen(P_SCAN_PORT, "9600,8,n,1") != 0x00)
        return -1;	

    if(!ScanSet_flag)
    {
    	PortSends(P_SCAN_PORT, MT780Cmd[0].param, MT780Cmd[0].len);	//factory default

		for(i=1; i<(sizeof(MT780Cmd)/sizeof(MT780_CMD)-1); i++)
		{
	    	memset(rx_buf, 0, sizeof(rx_buf));
	    	PortSends(P_SCAN_PORT, MT780Cmd[i].param, MT780Cmd[i].len);
			ret = SCAN1D_MT780_RecvPacket(rx_buf, 100, 20, 0);
			if(ret)break;
		} 
		
	    PortSends(P_SCAN_PORT, MT780Cmd[i].param, MT780Cmd[i].len); //save settings
	    if(i == (sizeof(MT780Cmd)/sizeof(MT780_CMD)-1)) 
		{
			memset(buf, 0, sizeof(buf));
			memcpy(buf,MT_Set_Version,6);
			SysParaWrite(BARCODE_SETTING_INFO,buf);
			ScanSet_flag = 1;
	    }
	    return 0;
    }	
    return 0;
}


int SCAN1D_MT780_Read (uchar *buf, uint size)
{   

    int ret;

    PortSends(P_SCAN_PORT, START_SCAN, 3);        //enter barcode scanning 

	ret = SCAN1D_MT780_RecvPacket(buf, size, 60, 1);
	
    return ret;
	
} 

void SCAN1D_MT780_Close (void)
{
	PortClose(P_SCAN_PORT);
    DISABLE_BAR_PWR();
}

void SCAN1D_MT780_Reset(void)
{
	char buf[20];
	
	memset(buf, 0x00, sizeof(buf));
	memcpy(buf,"MT_V00",6);
	SysParaWrite(BARCODE_SETTING_INFO,buf);
	ScanSet_flag = 0;
}
/*****************************************************************/
/*                                                               */
/*                       SCANNER 2D ME5110 MODULE                */
/*                                                               */
/*****************************************************************/                       

typedef struct
{
    uchar param[20];
    uchar len;
}SCAN2D_PARAM;

static const SCAN2D_PARAM Scan2dParam[] = {
	"IMGVGA0",     	7,  /* image size(default 640*480) : 752*480   */
	"PRTWGT7",      7, 	/* print weight : level 7 */
	"DLYRRD1000",  	10, /* reread delay : 1000 */
	"DLYGRD1000", 	10, /* good read delay : 1000ms(CharTimeOut=700) */
	"TRGSTO3000", 	10, /* read time out : 3000 ms */
	"TRGMOD0",     	7,  /* work mode : serial trigger */
						/* telepen */
	"TELENA1",      7,  /* enable */
						/* MSI */	 
	"MSIENA1",		7,  /* enable */ 
						/* microPDF417 */
	"MPDENA1",		7,  /* enable */ 
						/* kix post */
	"KIXENA1",		7,  /* enable */
						/* plessey code */
	"PLSENA1", 		7,  /* enable */
						/* code 11 */
	"C11ENA1", 		7, 	/* enable */
	"C11CK20",      7,  /* one Check Digital */
	"C11MIN01",     8,  /* minimum 1 */
	"C11MAX80",     8,  /* maximum 84 */
						/* EAN/JAN-8 addenda */
	"EA8AD21", 		7,  /* 2 digital addenda enable */
	"EA8AD51", 		7,  /* 5 digital addenda enable 8 */
	"EA8ARQ0",      7,  /* required */
						/* EAN/JAN-13 2 digital addenda */
	"E13AD21",		7,  /* 2 digital addenda on */ 
	"E13AD51", 		7,  /* 5 digital addenda on */
	"E13ARQ0", 		7, 	/* required */
						/* EAN/JAN-8 addenda */
	"EA8AD21", 		7,  /* 2 digital addenda enable */
	"EA8AD51", 		7,  /* 5 digital addenda enable 8 */
	"EA8ARQ0", 		7,  /* required */
						/* UPC-E0 addenda required */
	"UPEAD21", 		7,  /* 2 digital addenda on */    
	"UPEAD51", 		7,  /* 5 digital addenda on */
	"UPEARQ0",		7,  /* required */ 
						/* interleaved 2 of 5 */	
	"I25CK20",     	7,  /* validate digital check */
	"I25MIN04",     8,  /* minimum : 4 */
	"I25MAX80",     8,  /* maximum : 80 */
					   	/* striaight 2 of 5 industrial */
	"R25ENA1",     	7,  /* enable */
						/* straight 2 of IATA */
	"A25ENA1", 	   	7,  /* enable */
						/* maxtrix 2 of 5 */
	"X25ENA1",     	7,  /* enable */
						/* TCIF Link code 39 */
	"T39ENA1", 		7,	/* enable */
						/* upc-a */
	"UPAAD21",      7,  /* upc-a 2 digital addenda */
	"UPAAD51",      7,  /* upc-a 5 digital addenda */
	"UPAARQ0", 		7,  /* required */ 
						/* trioptic code */	
	"TRIENA1",		7,  /* enable */ 
						/* UPC-E1 */
	"UPEEN11", 		7,  /* enable */
						/* codablock F */
	"CBFENA1", 		7,  /* enable */
	"CBFMIN0001",   10, /* minimum digital: 1 */  
	"CBFMAX2048",   10, /* maximum digital: 2048 */
						/* code 16K */
	"16KENA1", 		7,  /* enable */
	"16KMIN001",    9,  /* mininum digital: 1 */
	"16KMAX160",    9,  /* maximum digital: 160 */
						/* postnet */
	"NETENA1", 		7,  /* enable */
						/* planet code */
	"PLNENA1", 		7,  /* enable */
						/* british post */
	"BPOENA1", 		7,  /* enable */					
						/* candian post */	
	"CANENA1",		7,  /* enable */ 
						/* kix post */
	"KIXENA1",		7,  /* enable */
						/* Australian post */
	"AUSENA1", 		7,  /* enable */
						/* japanese post */
	"JAPENA1", 		7,  /* enable */
						/* china post */
	"CPCENA1", 		7,  /* enable */
						/* korea post */
	"KPCENA1", 		7,  /* enable */
};

enum TX_MODE
{
	TX_RAW,
	TX_ADD_ONS,
};

/* ME5110 Command macro define */
#define ME5110_START_DECODE		"\x16\x54\x0D"
#define ME5110_STOP_DECODE		"\x16\x55\x0D"

#define ACK						0x06
#define MAX_BUFF_SIZE			8*1024

static void FillScanBuf(uchar *pPool, int *idx, uchar *pDataIn, int fillLen)
{
	memcpy(pPool+ *idx, pDataIn, fillLen);
	*idx += fillLen;
}

static int SCAN2D_ME5110_SendCmd(uchar *dataIn, int mode)
{
	int ret, idx=0;
	const int MAX_ATTACH_LEN=4; /*3 bytes of headers, 1 byte of tail */
	uchar tmp[3], scanCmdBuf[MAX_BUFF_SIZE];

	if(strlen(dataIn)+MAX_ATTACH_LEN > sizeof(scanCmdBuf)) return -11;

	if(mode == TX_ADD_ONS)
	{
		memcpy(tmp, "\x16\x4D\x0D", 3);
		FillScanBuf(scanCmdBuf, &idx, tmp, 3); 			 /* fill prefix */
		FillScanBuf(scanCmdBuf, &idx, dataIn, strlen(dataIn));    /* fill param  */
		FillScanBuf(scanCmdBuf, &idx, "!", 1); 			 /* fill sufix  */
	}
	else
	{
		memcpy(scanCmdBuf, dataIn, strlen(dataIn));
		idx = strlen(dataIn);
	}
	PortSends(P_SCAN_PORT, scanCmdBuf, idx);
}

static int SCAN2D_ME5110_RecvCmd(uchar *dataOut, uint lenIn, uint timeout)
{
	const int CHAR_TIMEOUT=10;
	uchar buff[MAX_BUFF_SIZE];
	int ret=-1, i=0, lenOut;
    T_SOFTTIMER timer;

    s_TimerSetMs(&timer, timeout);
	while (s_TimerCheckMs(&timer))
    {
        ret = PortPeep(P_SCAN_PORT, buff, 1);
        if (ret > 0) break;
    }

	/* receive time out */
	if(ret < 0) return -1;

	while(1)
	{
		ret = PortRecv(P_SCAN_PORT, &buff[i], CHAR_TIMEOUT);
		if(ret == 0)
		{
			if(i >= sizeof(buff)) return -2;

		   	i++;
			continue;
		}
		else break;
	}

	if(lenIn > i) 
		lenOut = i;
	else 			
		lenOut = lenIn;

	if(!memcmp(&buff[lenOut-2], "\r\n", 2))
		lenOut -= 2;

	memcpy(dataOut, buff, lenOut);
	return lenOut;
}

static void SCAN2D_ME5110_Init(void)
{
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, 1); /* power on */
	gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, GPIO_OUTPUT);

	gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
	gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);

	gpio_set_pin_type(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, GPIO_OUTPUT);
	gpio_set_pin_type(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, GPIO_OUTPUT);
	gpio_set_pin_val(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, 0);
	gpio_set_pin_val(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, 0);
}

static int SCAN2D_ME5110_Open(void)
{
	const int PARAM_ITEM_CNT=sizeof(Scan2dParam)/sizeof(SCAN2D_PARAM);
	const int RETRY=5;
	int ret, cnt, i;
	uint idx=0;
	uchar uaRecvBuff[200];
	
	gpio_set_pin_type(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, GPIO_FUNC0);
	gpio_set_pin_type(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, GPIO_FUNC0);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_on); /* power on */
	DelayMs(1200); /* It costs this time for scannner fully power on! */

    if (PortOpen(P_SCAN_PORT, "115200,8,n,1")) return -1; /*port used*/

	memset(uaRecvBuff, 0, sizeof(uaRecvBuff));

	for(i=0; i<PARAM_ITEM_CNT; i++)
	{
		for(cnt=0; cnt<RETRY; cnt++) 
		{
			SCAN2D_ME5110_SendCmd(Scan2dParam[i].param, TX_ADD_ONS);
			ret = SCAN2D_ME5110_RecvCmd(uaRecvBuff, sizeof(uaRecvBuff), 200);
			if(ret <= Scan2dParam[i].len) 
			{
				DelayMs(50);
				PortReset(P_SCAN_PORT);
				continue;
			}	
			if(uaRecvBuff[Scan2dParam[i].len] == ACK) break;
		}

		if(cnt == RETRY) return -2;
	}

	return 0;
}

static int SCAN2D_ME5110_Read(uchar *buff, uint size)
{
    int ret = 0;
    T_SOFTTIMER scan_timer;

	PortReset(P_SCAN_PORT); /* clear the fifo */
    SCAN2D_ME5110_SendCmd(ME5110_START_DECODE, TX_RAW); /* send start decode command */
	ret = SCAN2D_ME5110_RecvCmd(buff, size, 3000); /* receive barcode data */
    SCAN2D_ME5110_SendCmd(ME5110_STOP_DECODE, TX_RAW);
    if (ret <= 0) return -3;         
    return ret;  /* decode successfully, return data length */          
}

static void SCAN2D_ME5110_Close(void)
{
    SCAN2D_ME5110_SendCmd(ME5110_STOP_DECODE, TX_RAW);
    PortClose(P_SCAN_PORT);

	gpio_set_pin_type(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, GPIO_OUTPUT);
	gpio_set_pin_type(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, GPIO_OUTPUT);
	gpio_set_pin_val(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, 0);
	gpio_set_pin_val(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, 0);
    gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_off);   /* power off */
}

static void SCAN2D_ME5110_Reset(void)
{
	uchar ucKey;

	gpio_set_pin_type(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, GPIO_FUNC0);
	gpio_set_pin_type(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, GPIO_FUNC0);
	gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, GPIO_OUTPUT);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_on); /* power on */
	DelayMs(850);

	while(1)
	{
		gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);	
		gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
		DelayMs(200);

		gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_OUTPUT);	
		gpio_set_pin_val(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, 0);
		ucKey = getkey();
		if(ucKey == KEYCANCEL) break;
	}

	gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);	
	gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_off); /* power off */
	gpio_set_pin_type(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, GPIO_OUTPUT);
	gpio_set_pin_type(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, GPIO_OUTPUT);
	gpio_set_pin_val(ptBarCodeDrvCfg->rx_port,ptBarCodeDrvCfg->rx_pin, 0);
	gpio_set_pin_val(ptBarCodeDrvCfg->tx_port,ptBarCodeDrvCfg->tx_pin, 0);
}

/*****************************************************************/
/*                                                               */
/*                       SCANNER 2D ZM1000 MODULE                */
/*                                                               */
/*****************************************************************/                       

const SCAN2D_PARAM Scan2dParam_ZM1000[] = {
#if 0
	"\x21\x10\x41\x03", 4,  /* single & multi code */
	"\x21\x10\x40\x01", 4,  /* enable QR */
	"\x21\x11\x41\x03", 4,  /* single & multi code */
	"\x21\x11\x40\x01", 4,  /* enable PD417 */
	"\x21\x12\x41\x03", 4,  /* single & multi code */
	"\x21\x12\x40\x01", 4,  /* enable hanxing */
	"\x21\x13\x41\x03", 4,  /* single & multi code */
	"\x21\x13\x40\x01", 4,  /* enable dataMatrix */
	"\x21\x21\x40\x01", 4,  /* enable code128 */
	"\x21\x22\x40\x01", 4,  /* enable code93 */
	"\x21\x23\x40\x01", 4,  /* enable code39 */	
	"\x21\x24\x41\x01", 4,  /* enable UPC-A */
	"\x21\x24\x42\x01", 4,  /* enable UPC-E */
	"\x21\x24\x43\x01", 4,  /* enable EAN-13 */
	"\x21\x24\x44\x01", 4,  /* enable EAN-8 */
	"\x21\x25\x40\x01", 4,  /* enable codabar */
	"\x21\x26\x40\x01", 4,  /* enable standard 2 of 5 */
	"\x21\x27\x40\x01", 4,  /* enable maxtrix 2 of 5 */
	"\x21\x28\x40\x01", 4,  /* enable interleaved 2 of 5 */

	"\x21\x42\x40\x00",	4,  /* enable vitual COM0 */ 
	"\x21\x63\x41\x00", 4,  /* beeper : short */
#endif
#if 0
	"\x21\x51\x43\x01", 4,  /* data out in protocol */
	"\x21\x51\x44\x00", 4,	/* disable translation, as: \n \f */ 
	"\x21\x61\x41\x04", 4,  /* trigger mode : wave */
	"\x21\x62\x41\x01", 4,  /* compensation light: flash frequently */
	"\x21\x62\x42\x01", 4,  /* aim light : flash frequently */
	"\x21\x62\x43\x01", 4,  /* success indication light : on */
	"\x21\x61\x82\x0B\xB8", 5, /* trigger time out : 3000 ms */
	"\x21\x64\x81\x00\x00", 5, /* diff result interval : 0 s */
	"\x21\x64\x82\x03\xe8", 5, /* same result interval : 1 s */
	"\x21\x71\x81\x00\x00", 5, /* auto explode */
	"\x32\x76\x42\x03", 4,  /* enable 1d & 2d */
#endif
	"\x32\x76\x43\x01", 4,
};

/* ME5110 Command macro define */
#define ZM1000_START_DECODE		"\x32\x75\x01"
#define ZM1000_STOP_DECODE		"\x32\x75\x02"
#define ZM1000_VERSION_READ		"\x43\x02\xC2"


#define ACK						0x06
#define MAX_BUFF_SIZE			8*1024

/* configuration commmands */
#define ZM1000_CONFIG_W_REQ_CMD		0x21
#define ZM1000_CONFIG_W_RSP_CMD		0x22
#define ZM1000_CONFIG_R_REQ_CMD		0x23
#define ZM1000_CONFIG_R_RSP_CMD		0x24	
/* control command */
#define ZM1000_CTRL_R_REQ_CMD		0x32
#define ZM1000_CTRL_R_RSP_CMD		0x33
/* status commands */
#define ZM1000_STATUS_R_REQ_CMD		0x43
#define ZM1000_STATUS_R_RSP_CMD		0x44
/* image commands */
#define ZM1000_IMG_R_REQ_CMD		0x60
#define ZM1000_IMG_R_RSP_CMD		0x61
/* barcode data stream commmand */
#define ZM1000_DATA_R_RSP_CMD		0x03 // 0x65

void SCAN2D_ZM1000_Reset(void)
{
	uchar ucKey;

	gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, GPIO_OUTPUT);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_on); /* power on */
	DelayMs(850);

	while(1)
	{
		gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);	
		gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
		DelayMs(200);

		gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_OUTPUT);	
		gpio_set_pin_val(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, 0);
		ucKey = getkey();
		if(ucKey == KEYCANCEL) break;
	}

	gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);	
	gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_off); /* power off */
}

int SCAN2D_ZM1000_SendCmd(uchar *dataIn, uint len)
{
	if(len == 0 || dataIn == NULL) return -11;

	PortSends(P_SCAN_PORT, dataIn, len);
	return 0;
}

int SCAN2D_ZM1000_RecvCmd(uchar *dataOut, uint lenIn, uint timeout)
{
	const int CHAR_TIMEOUT=10;
	uchar buff[MAX_BUFF_SIZE];
	int ret=-1, i=0, lenOut;
    T_SOFTTIMER timer;

    s_TimerSetMs(&timer, timeout);
	while (s_TimerCheckMs(&timer))
    {
        ret = PortPeep(P_SCAN_PORT, buff, 1);
        if (ret > 0) break;
    }

	/* receive time out */
	if(ret < 0) return -1;

	while(1)
	{
		ret = PortRecv(P_SCAN_PORT, &buff[i], CHAR_TIMEOUT);
		if(ret == 0)
		{
			if(i >= sizeof(buff)) return -2;
		   	i++;
			continue;
		}
		else break;
	}

	if(lenIn > i) 
		lenOut = i;
	else 			
		lenOut = lenIn;

	memcpy(dataOut, buff, lenOut);
	return lenOut;
}

int SCAN2D_ZM1000_ExchgCmd(uchar *dataIn, uint lenIn, uchar *dataOut, uint recvLen, int timeout)
{
	uchar uaRecvBuff[20*1024];
	const int RETRY=2;
	int tmpLen, cnt, ret=0;
	ulong done = GetTimerCount()+timeout;
	uchar ch;

	for(cnt=0; cnt<RETRY; cnt++) 
	{
		memset(uaRecvBuff, 0, sizeof(uaRecvBuff));
		SCAN2D_ZM1000_SendCmd(dataIn, lenIn);
		ret = SCAN2D_ZM1000_RecvCmd(uaRecvBuff, sizeof(uaRecvBuff), timeout);
		if(ret <= 0) 
		{
			if(GetTimerCount() >= done) return -101;
			continue ;
		}

		if(!memcmp(dataIn, ZM1000_START_DECODE, sizeof(ZM1000_START_DECODE)-1)) 
		{
			if(uaRecvBuff[0] != ZM1000_DATA_R_RSP_CMD) return -102;
			if(ret <= 3) return -1;
			tmpLen = uaRecvBuff[1] * 256 + uaRecvBuff[2];
			if(ret != tmpLen + 2 + 1) return -103;
			if(dataOut == NULL) return 0;
			
			memcpy(dataOut, &uaRecvBuff[3], tmpLen>=recvLen ? recvLen : tmpLen);
			return tmpLen;
		}

		if(!memcmp(dataIn, ZM1000_VERSION_READ, sizeof(ZM1000_VERSION_READ)-1))
		{
			if(uaRecvBuff[0] != ZM1000_STATUS_R_RSP_CMD)	return -301;
			if(ret <= 3)	return -302;
			tmpLen = uaRecvBuff[3] * 256 + uaRecvBuff[4];
			if(ret != tmpLen + 3 + 2)	return -303;
			if(dataOut == NULL)	return 0;
			memcpy(dataOut, &uaRecvBuff[5], tmpLen>=recvLen ? recvLen : tmpLen);
			return tmpLen;
		}

		if(dataIn[0] == ZM1000_CONFIG_W_REQ_CMD)
			ch = ZM1000_CONFIG_W_RSP_CMD;
		else if(dataIn[0] == ZM1000_CTRL_R_REQ_CMD)
			ch = ZM1000_CTRL_R_RSP_CMD;
		else 
			return -201;

		if(ret != 5) 
			return  -202;

		if(ch != uaRecvBuff[0]) 
			return -203;

		if(memcmp(&uaRecvBuff[1], &dataIn[1], 2) || uaRecvBuff[4] != '\0') 
			return -204;

		if(!memcmp(dataIn, ZM1000_STOP_DECODE, sizeof(ZM1000_STOP_DECODE)-1))
			return 0;

		if (((dataIn[2] & 0x80) && (uaRecvBuff[3] != '\0')) ||
			(!(dataIn[2] & 0x80) && uaRecvBuff[3] != dataIn[3])) 
		{
			return -205;
		}

		return 0;
	}

	if(cnt == RETRY) return -206;

	return 0;
}	

static void SCAN2D_ZM1000_Init(void)
{
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_off); /* power off */
	gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, GPIO_OUTPUT);

	gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
	gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);
}

static int SCAN2D_ZM1000_Open(void)
{
	const int PARAM_ITEM_CNT=sizeof(Scan2dParam_ZM1000)/sizeof(SCAN2D_PARAM);
	const int RETRY=2;
	int ret, i;
	uchar uaRecvBuff[200];

	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_on); /* power on */
	DelayMs(850);
	DelayMs(500);
    if (PortOpen(P_SCAN_PORT, "115200,8,n,1")) return -1; /*port used*/

	for(i=0; i<PARAM_ITEM_CNT; i++)
	{
		ret = SCAN2D_ZM1000_ExchgCmd(Scan2dParam_ZM1000[i].param, 
				Scan2dParam_ZM1000[i].len, uaRecvBuff, sizeof(uaRecvBuff), 3000);
		if(ret != 0) 
			return -3;
	}

	return 0;
}

int SCAN2D_ZM1000_Read(uchar *buff, uint size)
{
    int len;
    T_SOFTTIMER scan_timer;

    if (buff == NULL) return -2;

	/* clear the fifo */
	PortReset(P_SCAN_PORT);

	/* send start decode command */
    len = SCAN2D_ZM1000_ExchgCmd(ZM1000_START_DECODE, sizeof(ZM1000_START_DECODE)-1, buff, size, 3000);
    SCAN2D_ZM1000_ExchgCmd(ZM1000_STOP_DECODE, sizeof(ZM1000_STOP_DECODE)-1, NULL, 0, 3000);
    if (len < 0) return -3;         
    return len;  /* decode successfully, return data length */          
}

void SCAN2D_ZM1000_Close(void)
{
    SCAN2D_ZM1000_ExchgCmd(ZM1000_STOP_DECODE, sizeof(ZM1000_STOP_DECODE)-1, NULL, 0, 3000);
    PortClose(P_SCAN_PORT);
    gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_off);   /* power off */
	DelayMs(10);
}

void SCAN2D_ZM1000_Update(void)
{
	unsigned char ucRet;
	unsigned char ch;

	ScrCls();
	ScrPrint(0, 0, 0xE1, "Scan Update");
	
	PortOpen(0, "115200,8,n,1");
	PortClose(P_SCAN_PORT);
	PortOpen(P_SCAN_PORT, "115200,8,n,1");

	gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_OUTPUT);
	gpio_set_pin_val(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, 0);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_on); /* power on */
	DelayMs(1500);
	
	ScrPrint(0, 3, 0x60, "ComSwitch...");
	ScrPrint(0, 4, 0x60, "Press Cancel to quit!");

	while(1)
	{
		ucRet = PortRecv(0, &ch, 0);
		if (ucRet == 0)
		{
			PortSend(P_SCAN_PORT, ch);
		}
		ucRet = PortRecv(P_SCAN_PORT, &ch, 0);
		if (ucRet == 0)
		{
			PortSend(0, ch);
		}
		if (!kbhit()&&getkey()==KEYCANCEL)
		{
			break;
		}
	}
	gpio_set_pin_type(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin, GPIO_INPUT);	
	gpio_disable_pull(ptBarCodeDrvCfg->trige_port,ptBarCodeDrvCfg->trige_pin);
	gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin, ptBarCodeDrvCfg->power_off); /* power off */

	PortClose(0);
	PortClose(P_SCAN_PORT);
}
extern int snprintf(char *str, size_t size, const char *format, ...); 
int SCAN2D_ZM1000_Version(char *ver, int len)
{
	/* Version Format: "barcodeU.crc x.x.xxx.xx Build yymmdd SVN:xxx" */
	int isScanOpen;
	int i, revLen;
	int l, r;
	char buff[64];
	static char ver_info[16] = {0};

	if(ver_info[0] != 0)
	{
		snprintf(ver, len, "%s", ver_info);
		return 0;
	}
	if(!(isScanOpen = s_ScanIsOpen()) && ScanOpen())
		return -1;
	
    revLen = SCAN2D_ZM1000_ExchgCmd(ZM1000_VERSION_READ, sizeof(ZM1000_VERSION_READ)-1, buff, sizeof(buff), 5000);
	if(revLen < 0)
	{
		if(!isScanOpen)	ScanClose();
		return -2;
	}
	
	l = 0;
	while(buff[l] != ' ')
		l++;
	l++;
	r = l;
	while(buff[r] != ' ')
		r++;
	buff[r] = 0;

	snprintf(ver, len, "%s", buff+l);
	snprintf(ver_info, sizeof(ver_info), "%s", buff+l);
	if(!isScanOpen) ScanClose();

	return 0;
}

/*****************************************************************/
/*                                                               */
/*                       SCANNER 2D EM2039 MODULE                     */
/*                                                               */
/*****************************************************************/                       
#define cfg_barpwr()		gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,GPIO_OUTPUT)	
#define enable_barpwr()		gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,ptBarCodeDrvCfg->power_on)
#define disable_barpwr()	{gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,ptBarCodeDrvCfg->power_off);DelayMs(30);}
#define EM2039_ASK '?'
#define EM2039_REPLY '!'
#define EM2039_WAKE  "\x1b\x31"
#define EM2039_SLEEP "\x1b\x30"
#define EM2039_SET_SUCCESS 0x06
//设置版本号
#define EM_Set_Version "EM_V01"//格式:AB_VXX,AB代表版本,Vxx代表版本号

#define EM2039_MAX_BUFF_SIZE 8*1024
#define EM2039_SCAN_TIME_OUT 500//RecvCmd time out
#define EM2039_OPEN_TIME 800//waitting device power on
//更改设置命令后务必更新设置版本号EM_Set_Version
static const SCAN2D_PARAM Scan2dParam_EM2039[] ={
"NLS0006010;",	  		11, //set enable
"NLS0200010;",			11, //scan light
"NLS0201000;",			11, //scan read light
"NLS0302000;",			11, //trigger mode
//"NLS0313050=0;", 		13, //device sleep
"NLS0001020;",	  		11, //all code enable
"NLS0309000;",	  		11, //Don't send Prefix and suffix
"NLS0408080;",	  		11, //disable (Code39  send start or termination symbol) 
"NLS0409080;",	  		11, //disable (Codabar  send start or termination symbol)
"NLS0400040=127;",		15, //set code128 Max 127
"NLS0001021;",			11, //enable Inverse video  read 
"NLS0006000;",			11, //close set 
};


static int SCAN2D_EM2039_RecvCmd(uchar *dataOut, uint lenIn,ushort CHAR_TIMEOUT)
{
	uchar buff[EM2039_MAX_BUFF_SIZE];
	int ret=-1, i=0, lenOut;
	T_SOFTTIMER timer;
	memset(buff,0,EM2039_MAX_BUFF_SIZE);
	
	//read timeout
	s_TimerSetMs(&timer, EM2039_SCAN_TIME_OUT);
	while (s_TimerCheckMs(&timer))
    {
        ret = PortPeep(P_SCAN_PORT, buff, 1);
        if (ret > 0) break;
    }
	if(ret<=0) return -3;

	while(1)
	{
		ret = PortRecv(P_SCAN_PORT, &buff[i], CHAR_TIMEOUT);
		if(ret!=0) break;
        if(i >= sizeof(buff)) return -2;
        i++;
	}
	if(lenIn > i) lenOut = i;
	else lenOut = lenIn;
	memcpy(dataOut, buff, lenOut);
	return lenOut;
}

static void SCAN2D_EM2039_Config(void)
{	
	int i,iRet=0,num;
	uchar buf[20];
	
	//check and set
	memset(buf,0x00,sizeof(buf));
	iRet=SysParaRead(BARCODE_SETTING_INFO,buf);
	if(iRet==0&&(!strncmp(buf,EM_Set_Version,6)))
	{
		ScanSet_flag = 1;
	}
	else 
	{	
		num = sizeof(Scan2dParam_EM2039) / sizeof(SCAN2D_PARAM);
		for (i = 0; i < num; i++)
		{
			PortSends(P_SCAN_PORT, Scan2dParam_EM2039[i].param, Scan2dParam_EM2039[i].len);
		}
		memset(buf, 0x00, sizeof(buf));
		iRet = PortRecvs(P_SCAN_PORT, buf, num, 1000);
		if(iRet==num)
		{
			for (i = 0; i < num; i++) 
				if(buf[i]!=0x06)break;
				
		}
		if(i==num)
		{
			memset(buf, 0x00, sizeof(buf));
			memcpy(buf,EM_Set_Version,6);
			SysParaWrite(BARCODE_SETTING_INFO,buf);
			ScanSet_flag = 1;
		}
	}
}
void SCAN2D_EM2039_Init()
{	
	disable_barpwr();
    cfg_barpwr();
}

//RETURN:0x00-open success
//		  -1:Port is occupied,-3:Device fault

int SCAN2D_EM2039_Open (void)
{   
	int len,i;
	uchar ch,ret,buff[40],val=0xff;
	
	if(get_machine_type()==S300)
    {
        if(GetVolt()>=5500)
        {    
        	gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
        	gpio_set_pin_val(GPIOB,31, 0);//power off
            return -3; 
        }

        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
        gpio_set_pin_val(GPIOB,31, 1);//power open
    }
	enable_barpwr();
	DelayMs(EM2039_OPEN_TIME);

	ret = PortOpen(P_SCAN_PORT, "9600,8,n,1");
	if(ret!=0) return -1;
	if(ScanSet_flag == 0) SCAN2D_EM2039_Config();
	PortSend(P_SCAN_PORT, EM2039_ASK);// check the connection
	PortRecv(P_SCAN_PORT, &ch, 500);//500ms timeout 
	if(ch!=EM2039_REPLY) return -3;
	return 0x00;
}

//RETURN: i -the len of barcode data
//		  -1:Device not open,-2:invalid parameters,-3:Without barcode data

int SCAN2D_EM2039_Read (uchar *buf, uint size)
{   
	int ret;
	uchar ch=0,buff[MAX_BUFF_SIZE];
wake:
	PortSends(P_SCAN_PORT,EM2039_WAKE,2);//device wake
	PortRecv(P_SCAN_PORT,&ch,500);
	if(ch!=EM2039_SET_SUCCESS)
	{ 
		memset(buff,0,sizeof(buff));
		SCAN2D_EM2039_RecvCmd(buff,MAX_BUFF_SIZE,20);
		goto wake;
	 }


	//get the barcode data 
	ret= SCAN2D_EM2039_RecvCmd(buf,size,20);
	PortSends(P_SCAN_PORT,EM2039_SLEEP,2);//device sleep
	PortRecv(P_SCAN_PORT,&ch,500);
	return ret;
} 

void SCAN2D_EM2039_Close (void)
{
	PortClose(P_SCAN_PORT);
    disable_barpwr();
}
void SCAN2D_EM2039_Reset(void)
{
	char buf[20];
	memset(buf, 0x00, sizeof(buf));
	memcpy(buf,"EM_V00",6);
	SysParaWrite(BARCODE_SETTING_INFO,buf);
	ScanSet_flag = 0;
}

/*****************************************************************/
/*                                                               */
/*                       SCANNER 2D CH MODULE                     */
/*                                                               */
/*****************************************************************/ 
#define cfg_barpwr()		gpio_set_pin_type(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,GPIO_OUTPUT)	
#define enable_barpwr()		gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,ptBarCodeDrvCfg->power_on)
#define disable_barpwr()	{gpio_set_pin_val(ptBarCodeDrvCfg->power_port,ptBarCodeDrvCfg->power_pin,ptBarCodeDrvCfg->power_off);DelayMs(30);}

#define CH_WAKE  "\xff\x54\x0D"
#define CH_SLEEP "\xff\x55\x0D"
#define CH_QUERY "\xff\x56\x0D"
#define CH_REPLY '\x56'
//设置版本号
#define CH_Set_Version "CH_V02"//格式:AB_VXX,AB代表版本,Vxx代表版本号

#define CH_MAX_BUFF_SIZE 8*1024
#define CH_SCAN_TIME_OUT 500//RecvCmd time out
#define CH_OPEN_TIME 3000//waitting device power on
#define CH_UPDATE_TIME (3*60*1000)//waitting device update

#define CH_SET_CMD "\x02\x50\x36\x36\x36\x36\x36\x36\x03"//set start or end
#define CH_VERSION_CMD "\x02\x50\x36\x36\x36\x36\x46\x42\x03"//set start or end
static uint CH_t0 = 0;//CH_PowerOn_StartTime

//更改设置命令后务必更新设置版本号CH_Set_Version
static const SCAN2D_PARAM Scan2dParam_CH[] ={
	"\x02\x45\x39\x39\x33\x31\x31\x03",	  		8, //CMD:E99311_add"0x0d"
	"\x02\x41\x46\x46\x37\x31\x31\x03",			8, //CMD:AFF711_closebeep
	"\x02\x46\x46\x44\x37\x31\x31\x03", 		8, //CMD:FFD711_enable_CODE39
	"\x02\x46\x46\x45\x32\x31\x31\x03", 		8, //CMD:FFE211_enable_CODE93
	"\x02\x46\x46\x41\x32\x32\x32\x03", 		8, //CMD:FFA222_enable_DataMatrix
	"\x02\x46\x46\x39\x35\x31\x31\x03", 		8, //CMD:FF9511_enable_MicroQR
	"\x02\x46\x46\x42\x37\x31\x31\x03", 		8, //CMD:FFB711_enable_MicroPDF417
};
static int CH_Version_Flag =0;

static int SCAN2D_CH_RecvCmd(uchar *dataOut, uint lenIn,ushort CHAR_TIMEOUT)
{
	uchar buff[CH_MAX_BUFF_SIZE];
	int ret=-1, i=0, lenOut;
	T_SOFTTIMER timer;
	memset(buff,0,CH_MAX_BUFF_SIZE);
	
	//read timeout
	s_TimerSetMs(&timer, CH_SCAN_TIME_OUT);
	while (s_TimerCheckMs(&timer))
    {
        ret = PortPeep(P_SCAN_PORT, buff, 1);
        if (ret > 0) break;
    }
	if(ret<=0) return -3;

	while(1)
	{
		ret = PortRecv(P_SCAN_PORT, &buff[i], CHAR_TIMEOUT);
		if(ret!=0) break;
        if(i >= sizeof(buff)) return -2;
        i++;
	}
	i--;
	if(buff[i]!=0x0d) return -3;//0x0d作为后缀检查完整性
	if(lenIn > i) lenOut = i;
	else lenOut = lenIn;

	memcpy(dataOut, buff, lenOut);
	return lenOut;

}

void SCAN2D_CH6810_Init()
{	
    cfg_barpwr();
	enable_barpwr();
    CH_t0=GetTimerCount();

}
void SCAN2D_CH5891_Init()
{	
	disable_barpwr();
    cfg_barpwr();
}

//RETURN:0x00-open success
//		  -1:Port is occupied,-3:Device fault

int SCAN2D_CH6810_Open (void)
{   
	uchar ret,Recvbuff[50],buf[20];
	int iRet,num,i;
	static int power_on = 0;
	
	if(!power_on)
	{
    	while(GetTimerCount()-CH_t0<=CH_OPEN_TIME);//waitting for device power on
    	power_on = 1;
	}
	memset(buf, 0x00, sizeof(buf));
	iRet=SysParaRead(BARCODE_SETTING_INFO,buf);
	if(iRet==0&&(!strncmp(buf,CH_Set_Version,6)))
	{
		ScanSet_flag = 1;
	}
	ret = PortOpen(P_SCAN_PORT, "115200,8,n,1");
	if(ret!=0) return -1;
	
	PortSends(P_SCAN_PORT,CH_SET_CMD,9);// check the connection
	memset(Recvbuff, 0x00, sizeof(Recvbuff));
	PortRecvs(P_SCAN_PORT, Recvbuff, 50, 500);//500ms timeout
	if(!strncmp(Recvbuff,"\x0D\x0A\x23\x23\x23\x23",6))
	{
		if(ScanSet_flag==0)
		{
			num = sizeof(Scan2dParam_CH) / sizeof(SCAN2D_PARAM);
			for (i = 0; i < num; i++)
			{
				PortSends(P_SCAN_PORT, Scan2dParam_CH[i].param, Scan2dParam_CH[i].len);
				DelayMs(50);//确保模块有足够时间响应设置
			}
			memset(buf, 0x00, sizeof(buf));
			memcpy(buf,CH_Set_Version,6);
			SysParaWrite(BARCODE_SETTING_INFO,buf);
			ScanSet_flag = 1;
		}
		PortSends(P_SCAN_PORT,CH_SET_CMD,9);// finish check the connection
		DelayMs(1500);//确保模块有足够时间响应设置
	}
	else
		return -3;

	return 0x00;
}

//RETURN:0x00-open success
//		  -1:Port is occupied,-3:Device fault

int SCAN2D_CH5891_Open (void)
{   
	uchar ret,Recvbuff[50],buf[20],ch;
	int iRet,num,i;
	
	memset(buf, 0x00, sizeof(buf));
	iRet=SysParaRead(BARCODE_SETTING_INFO,buf);
	if(iRet==0&&(!strncmp(buf,CH_Set_Version,6)))
	{
		ScanSet_flag = 1;
	}
	enable_barpwr();
	DelayMs(1500);
	ret = PortOpen(P_SCAN_PORT, "115200,8,n,1");
	if(ret!=0) return -1;
	PortSends(P_SCAN_PORT,CH_QUERY,3);// check the connection
	PortRecv(P_SCAN_PORT, &ch, 500);//500ms timeout 
	//如果没有响应再重发一次应答命令
	if(ch!=CH_REPLY)
	{
		PortClose(P_SCAN_PORT);
		disable_barpwr();
		enable_barpwr();
		DelayMs(1500);
		ret = PortOpen(P_SCAN_PORT, "115200,8,n,1");
		if(ret!=0) return -1;
		PortSends(P_SCAN_PORT,CH_QUERY,3);// check the connection
		PortRecv(P_SCAN_PORT, &ch, 500);//500ms timeout 
	}
	if(ch==CH_REPLY)
	{
		if(ScanSet_flag==0)
		{
			DelayMs(1500);
			PortSends(P_SCAN_PORT,CH_SET_CMD,9);// start set cmd
			memset(Recvbuff, 0x00, sizeof(Recvbuff));
			PortRecvs(P_SCAN_PORT, Recvbuff, 50, 500);//500ms timeout
			if(!strncmp(Recvbuff,"\x0D\x0A\x23\x23\x23\x23",6))
			{
				num = sizeof(Scan2dParam_CH) / sizeof(SCAN2D_PARAM);
				for (i = 0; i < num; i++)
				{
					PortSends(P_SCAN_PORT, Scan2dParam_CH[i].param, Scan2dParam_CH[i].len);
					DelayMs(50);//确保模块有足够时间响应设置
				}
				memset(buf, 0x00, sizeof(buf));
				memcpy(buf,CH_Set_Version,6);
				SysParaWrite(BARCODE_SETTING_INFO,buf);
				ScanSet_flag = 1;
				PortSends(P_SCAN_PORT,CH_SET_CMD,9);// finish set cmd
			}
		}
	}
	else
		return -3;
	return 0x00;
}
int SCAN2D_CH_Read (uchar *buf, uint size)
{   
	int ret;

	PortReset(P_SCAN_PORT);
	PortSends(P_SCAN_PORT,CH_WAKE,3);//device wake
	
	//get the barcode data 
	ret= SCAN2D_CH_RecvCmd(buf,size,20);
	PortSends(P_SCAN_PORT,CH_SLEEP,3);//device sleep
	DelayMs(10);

	return ret;
} 

void SCAN2D_CH6810_Close (void)
{
	PortClose(P_SCAN_PORT);
	DelayMs(500);
}
void SCAN2D_CH5891_Close (void)
{
	PortClose(P_SCAN_PORT);
	disable_barpwr();
}

void SCAN2D_CH_Reset(void)
{
	char buf[20];
	memset(buf, 0x00, sizeof(buf));
	memcpy(buf,"CH_V00",6);
	SysParaWrite(BARCODE_SETTING_INFO,buf);
	ScanSet_flag = 0;
}
void SCAN2D_CH_Update(void)
{
	unsigned char ucRet;
	unsigned char ch;

	ScrCls();
	ScrPrint(0, 0, 0xE1, "Scan Update");

	enable_barpwr();

	ScrPrint(0, 4, 0x60, "Press Cancel to quit!");

	ScrPrint(0,7,0,"Waiting for update...");


	PortOpen(P_SCAN_PORT, "115200,8,n,1");
    PortOpen(RS232A, "115200,8,n,1");
    while(1)
    {			
    	if (PortRx(RS232A, &ch)==0)
    	{
			PortTx(P_SCAN_PORT, ch);
    	}
    	if (PortRx(P_SCAN_PORT, &ch)==0) PortTx(RS232A, ch);
    	if (!kbhit() && (getkey()==KEYCANCEL))break;
    }
	PortClose(RS232A);
	ScanClose();
	CH_Version_Flag=0;
}
int SCAN2D_CH_Version(char *ver, int len)
{
	// Version Format: "2XAA00000XXXX" 2XAA:硬件版本XXXX:模块软件版本
	int isScanOpen;
	int revLen;
	char buff[64];	
	static char ver_info[14] = {0};
	
	if(CH_Version_Flag)
	{	
		memcpy(ver, ver_info, len);
		return 0;
	}
	ScanOpen();
	DelayMs(1500);
	PortSends(P_SCAN_PORT,CH_SET_CMD,9);// check the connection
	memset(buff, 0x00, sizeof(buff));
	PortRecvs(P_SCAN_PORT, buff, 50, 500);//500ms timeout
	if(strncmp(buff,"\x0D\x0A\x23\x23\x23\x23",6))
	{
		ScanClose();
		return -2;
	}
	PortSends(P_SCAN_PORT,CH_VERSION_CMD,9);// check the version
	memset(buff, 0x00, sizeof(buff));
	revLen =PortRecvs(P_SCAN_PORT, buff, 50, 500);//500ms timeout
	if(revLen < 0)
	{
		ScanClose();
		return -2;
	}
	memcpy(ver, buff, len);	
	memcpy(ver_info, buff, sizeof(ver_info));
	
	ScanClose();
	CH_Version_Flag=1;
	return 0;
}

/*****************************************************************/
/*                                                               */
/*                       SCANNER 2D XL7130 MODULE                     */
/*                                                               */
/*****************************************************************/                       

#define XL7130_WAKE  "\x02\xF4\x03"
#define XL7130_SLEEP "\x02\xF5\x03"
//查询版本号:7100c.APS.xxx。xxx为具体版本号
#define XL7130_QUERY_CMD   "\x02\xF0\x03\x30\x44\x31\x33\x30\x32\x3F\x2E"
//检查在位或匹配时校验固定部分:7100c.
#define XL7130_VER "\x37\x31\x30\x30\x63\x2E"
#define XL_Set_Version "XL_V01"
#define XL7130_MAX_BUFF_SIZE 8*1024
#define XL7130_SCAN_TIME_OUT 500//RecvCmd time out
#define XL7130_OPEN_TIME 1000//waitting device power on
static const SCAN2D_PARAM Scan2dParam_XL7130[] ={
	"\x02\xF0\x03\x30\x39\x31\x41\x30\x30\x2E",				10, //CMD:"\x02\xF0\x03091A00."_TriggerMode
	"\x02\xF0\x03\x30\x32\x30\x44\x30\x31\x31\x2E", 		11, //CMD:"\x02\xF0\x03020D011."_enable_CODE93
};
static const SCAN2D_PARAM Scan2dReply_XL7130[] ={
	"\x30\x39\x31\x41\x30\x30\x06\x2E",				8, //Reply:"091A00-."_TriggerMode
	"\x30\x32\x30\x44\x30\x31\x31\x06\x2E", 		9, //Reply:"020D011-."_enable_CODE93
};

static int SCAN2D_XL7130_RecvCmd(uchar *dataOut, uint lenIn,ushort CHAR_TIMEOUT)
{
	uchar buff[XL7130_MAX_BUFF_SIZE];
	int ret=-1, i=0, lenOut;
	T_SOFTTIMER timer;
	memset(buff,0,XL7130_MAX_BUFF_SIZE);
	
	//read timeout
	s_TimerSetMs(&timer, XL7130_SCAN_TIME_OUT);
	while (s_TimerCheckMs(&timer))
    {
        ret = PortPeep(P_SCAN_PORT, buff, 1);
        if (ret > 0) break;
    }
	if(ret<=0) return -3;

	while(1)
	{
		ret = PortRecv(P_SCAN_PORT, &buff[i], CHAR_TIMEOUT);
		if(ret!=0) break;
        if(i >= sizeof(buff)) return -2;
        i++;
	}
	if(lenIn > i) lenOut = i;
	else lenOut = lenIn;
	memcpy(dataOut, buff, lenOut);
	return lenOut;
}

static void SCAN2D_XL7130_Config(void)
{	
	int iRet,num,i;
	uchar buf[20],Recvbuff[20];
		
	//check and set

	memset(buf, 0x00, sizeof(buf));
	iRet=SysParaRead(BARCODE_SETTING_INFO,buf);
	if(iRet==0&&(!strncmp(buf,XL_Set_Version,6)))
	{
		ScanSet_flag = 1;
	}
	else 
	{	
		num = sizeof(Scan2dParam_XL7130) / sizeof(SCAN2D_PARAM);
		for (i = 0; i < num; i++)
		{
			PortSends(P_SCAN_PORT, Scan2dParam_XL7130[i].param, Scan2dParam_XL7130[i].len);
			memset(Recvbuff, 0x00, sizeof(Recvbuff));
			PortRecvs(P_SCAN_PORT, Recvbuff, 20, 500);
			if(strncmp(Recvbuff,Scan2dReply_XL7130[i].param, Scan2dReply_XL7130[i].len))break;
		}
		if(i == num)
		{
			memset(buf, 0x00, sizeof(buf));
			memcpy(buf,XL_Set_Version,6);
			SysParaWrite(BARCODE_SETTING_INFO,buf);
			ScanSet_flag = 1;
		}
	}

}
void SCAN2D_XL7130_Init()
{	
	disable_barpwr();
    cfg_barpwr();
}

//RETURN:0x00-open success
//		  -1:Port is occupied,-3:Device fault

int SCAN2D_XL7130_Open (void)
{   
	int len;
	uchar ch,ret,buff[40];

	enable_barpwr();
	DelayMs(XL7130_OPEN_TIME);

	ret = PortOpen(P_SCAN_PORT, "115200,8,n,1");
	if(ret!=0) return -1;
	PortSends(P_SCAN_PORT,XL7130_QUERY_CMD,11);
	memset(buff, 0x00, sizeof(buff));
	PortRecvs(P_SCAN_PORT, buff, 40, 500);
	if(!strstr(buff,XL7130_VER))return -3;
	if(ScanSet_flag == 0) SCAN2D_XL7130_Config();
	return 0x00;
}

//RETURN: i -the len of barcode data
//		  -1:Device not open,-2:invalid parameters,-3:Without barcode data

int SCAN2D_XL7130_Read (uchar *buf, uint size)
{   
	int ret;
	uchar ch=0,buff[MAX_BUFF_SIZE];

	PortSends(P_SCAN_PORT,XL7130_WAKE,3);//device wake

	//get the barcode data 
	ret= SCAN2D_XL7130_RecvCmd(buf,size,20);
	PortSends(P_SCAN_PORT,XL7130_SLEEP,3);//device sleep
	return ret;
} 

void SCAN2D_XL7130_Close (void)
{
	PortClose(P_SCAN_PORT);
    disable_barpwr();
}
void SCAN2D_XL7130_Reset(void)
{
	char buf[20];
	memset(buf, 0x00, sizeof(buf));
	memcpy(buf,"XL_V00",6);
	SysParaWrite(BARCODE_SETTING_INFO,buf);
	ScanSet_flag = 0;
}

/*****************************************************************/
/*                                                               */
/*                       BARCODE API DEFINE                      */
/*                                                               */
/*****************************************************************/                       

static SCANNER_API_T tScannerAPI;

static int get_module_type(void)
{
    char context[64],buf[20];
    static int type = -1;

 
    if(get_machine_type()==S300)
    {
		memset(buf,0x00,sizeof(buf));
		SysParaRead(BARCODE_SETTING_INFO,buf);//查看BARCODE_SETTING_INFO中版本

		if(!strncmp(buf,"EM",2)) type = SCAN_EM2039;
		if(!strncmp(buf,"CH",2)) type = SCAN_CH6810;
		if(!strncmp(buf,"XL",2)) type = SCAN_XL7130;
    }
    if (type >= 0) return type;
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("BAR_CODE",context)>0){
        if(strstr(context, "MOTOROLA-SE655")) type = SCAN_SE655;
        if(strstr(context, "HONEYWELL-ME5100")) type = SCAN_ME5110;
		if(strstr(context, "ZM1000")) type = SCAN_ZM1000;
		if(strstr(context, "01")) type = SCAN_EM2039;
		if(strstr(context, "07")) type = SCAN_EM2096;
		if(strstr(context, "09")) type = SCAN_CH5891;
		if(strstr(context, "11")) type = SCAN_CH6810;
		if(strstr(context, "14")) type = SCAN_MT780;
		if(strstr(context, "05")) type = SCAN_XL7130;
    }
    else type  = 0;
    
    return type;
}
int is_barcode_module()
{
	return get_module_type()>0? 1:0;
}
extern uchar  bIsChnFont;
void ScannerReset(void)
{
	int type;
	uchar ucKey;

	ScrCls();
	if(bIsChnFont == 1)
	{
		ScrPrint(0, 0, 0xC1, "扫描头复位");
		ScrPrint(0, 2, 0x1,  "警告：此功能仅当");
		ScrPrint(0, 4, 0x1,  "扫描头出故障时使");
		ScrPrint(0, 6, 0x1,  "用!"); 
		ScrPrint(0, 8, 0x1,  "[确定] 启动");
		ScrPrint(0, 10, 0x1, "[取消] 退出");
	}
	else
	{
		ScrPrint(0, 0, 0xC1,  "SCANNER RESET");
		ScrPrint(0, 2, 0,    "WARNING: This only be");
		ScrPrint(0, 3, 0, 	 "done when scanner wi-");
		ScrPrint(0, 4, 0,    "th fault!");
		ScrPrint(0, 5, 0,    "[Enter] to action");
		ScrPrint(0, 6, 0,    "[Cannel] to exit");
	}

	ucKey = getkey();
	if(ucKey == KEYCANCEL) return ;

	type = get_module_type();
	switch(type)
	{
		case SCAN_ZM1000:
			SCAN2D_ZM1000_Reset();
			break;

		case SCAN_SE655:
			break;

		case SCAN_ME5110:
			SCAN2D_ME5110_Reset();
			break;
			
		case SCAN_EM2039:
		case SCAN_EM2096:
			SCAN2D_EM2039_Reset();
			break;
			
		case SCAN_CH6810:
		case SCAN_CH5891:
			SCAN2D_CH_Reset();
			break;
			
		case SCAN_MT780:
			SCAN1D_MT780_Reset();
			break;
		case SCAN_XL7130:
			SCAN2D_XL7130_Reset();
			break;
	}
}
int s_ScannerMatching()
{
	uint CH_t;
	uchar ch,buf[20],Recvbuff[40],context[64];

	memset(context,0,sizeof(context));
	if(ReadCfgInfo("BAR_CODE",context)<=0)return 0;

	//Power On
	cfg_barpwr();
	enable_barpwr();
   
	CH_t = GetTimerCount();
	
	//check for EM2039,time:800ms(DelayMs)+500ms(timeout)
	DelayMs(800);
	PortOpen(P_SCAN_PORT, "9600,8,n,1");
	PortSend(P_SCAN_PORT, EM2039_ASK);// check the connection
	PortRecv(P_SCAN_PORT, &ch, 500);//500ms timeout
	if(ch==EM2039_REPLY)
	{
		memset(buf, 0x00, sizeof(buf));
		memcpy(buf,"EM_V00",6);
		SysParaWrite(BARCODE_SETTING_INFO,buf);
		PortClose(P_SCAN_PORT);
		disable_barpwr();//Power Off
		return SCAN_EM2039;
	}
	PortClose(P_SCAN_PORT);
	//check for XL7130,time:1300ms(DelayMs)+500ms(timeout)
	PortOpen(P_SCAN_PORT, "115200,8,n,1");
	memset(Recvbuff, 0x00, sizeof(Recvbuff));
	PortSends(P_SCAN_PORT,XL7130_QUERY_CMD,11);// check the connection
	PortRecvs(P_SCAN_PORT, Recvbuff, 40, 500);//500ms timeout
	if(strstr(Recvbuff,XL7130_VER))
	{
		memset(buf, 0x00, sizeof(buf));
		memcpy(buf,"XL_V00",6);
		SysParaWrite(BARCODE_SETTING_INFO,buf);
		PortClose(P_SCAN_PORT);
		disable_barpwr();//Power Off
		return SCAN_XL7130;
	}
	//check for CH,time:3000ms(DelayMs)+500ms(timeout)+200ms(DelayMs)
	while(GetTimerCount()-CH_t<=CH_OPEN_TIME);//waitting for device power on
	PortOpen(P_SCAN_PORT, "115200,8,n,1");
	memset(Recvbuff, 0x00, sizeof(Recvbuff));
	PortSends(P_SCAN_PORT,CH_SET_CMD,9);// check the connection
	PortRecvs(P_SCAN_PORT, Recvbuff, 6, 500);//500ms timeout
	if(!strncmp(Recvbuff,"\x0D\x0A\x23\x23\x23\x23",6))
	{	
		memset(buf, 0x00, sizeof(buf));
		memcpy(buf,"CH_V00",6);
		SysParaWrite(BARCODE_SETTING_INFO,buf);
		PortClose(P_SCAN_PORT);
		disable_barpwr();//Power Off
		DelayMs(200);
		return SCAN_CH6810;
	}
	//no module,time:3500ms(DelayMs)
	memset(buf, 0x00, sizeof(buf));
	SysParaWrite(BARCODE_SETTING_INFO,buf);
	PortClose(P_SCAN_PORT);
	disable_barpwr();//Power Off
	return 0;
}
void ScannerMatching(void)
{
	uchar ucKey,type[10];
	int iret;
	ScrCls();
	if(bIsChnFont == 1)
	{
		ScrPrint(0, 0, 0xC1, "扫描头匹配");
		ScrPrint(0, 2, 0x1,  "警告：此功能仅当");
		ScrPrint(0, 4, 0x1,  "扫描头更换时使用");
		ScrPrint(0, 6, 0x1,  "!"); 
		ScrPrint(0, 8, 0x1,  "[确定] 启动");
		ScrPrint(0, 10, 0x1, "[取消] 退出");
	}
	else
	{
		ScrPrint(0, 0, 0xC1,  "SCANNER Matching");
		ScrPrint(0, 2, 0,    "WARNING: This only be");
		ScrPrint(0, 3, 0, 	 "done when scanner wi-");
		ScrPrint(0, 4, 0,    "th change!");
		ScrPrint(0, 5, 0,    "[Enter] to action");
		ScrPrint(0, 6, 0,    "[Cannel] to exit");
	}

	ucKey = getkey();
	if(ucKey == KEYCANCEL) return ;
	iret=s_ScannerMatching();

	memset(type,0,sizeof(type));
	if(iret == SCAN_EM2039)     strcpy(type, "01");
	if(iret == SCAN_CH6810)	    strcpy(type, "11");
	if(iret == SCAN_XL7130)	    strcpy(type, "05");
	ScrCls();
	if(iret)
	{
		if(bIsChnFont == 1)
		{
			ScrPrint(0, 0, 0xC1, "扫描头匹配");
			ScrPrint(0, 4, 0x1,  "匹配扫描头:%s",type);
			ScrPrint(0, 6, 0x1,  "重启中..."); 
		}
		else
		{
			ScrPrint(0, 0, 0xC1,  "SCANNER Matching");
			ScrPrint(0, 4, 0, 	  "Matching Scanner:%s",type);
			ScrPrint(0, 6, 0,     "Reboot...");
		}
		DelayMs(1000);
	}
	else
	{	
		if(bIsChnFont == 1)
		{
			ScrPrint(0, 0, 0xC1, "扫描头匹配");
			ScrPrint(0, 6, 0x1,  "匹配失败!"); 
		}
		else
		{
			ScrPrint(0, 0, 0xC1,  "SCANNER Matching");
			ScrPrint(0, 6, 0,     "Matching Fault!");
		}
		getkey();
		return;
	}
	Soft_Reset();
}

static int scan_func_nop(void)
{
	return RET_NO_MODULE;
	/* just a do-nothing function */
} 


static T_BarCodeDrvConfig S900Barcode_Ver1 = {
	.power_port = GPIOC,
	.power_pin = 6,
	.trige_port = GPIOB,
	.trige_pin = 9,
	.tx_port = GPIOA,
	.tx_pin = 20,
	.rx_port = GPIOA,
	.rx_pin = 19,
	.power_on = 0,
	.power_off = 1,
};
static T_BarCodeDrvConfig S900Barcode_Ver2 = {
	.power_port = GPIOB,
	.power_pin = 11,
	.trige_port = GPIOB,
	.trige_pin = 9,
	.tx_port = GPIOA,
	.tx_pin = 20,
	.rx_port = GPIOA,
	.rx_pin = 19,
	.power_on = 0,
	.power_off = 1,
};
static T_BarCodeDrvConfig S300Barcode_Ver = {
	.power_port = GPIOB,
	.power_pin = 29,
	.power_on = 1,
	.power_off = 0,
};

void s_ScanInit(void)
{
	int type;
	int mach;
	int isDevHW;

	mach = get_machine_type();
	if (mach == S300)
	{
		ptBarCodeDrvCfg = &S300Barcode_Ver;
	} 
	else if(mach == S900)
	{
		isDevHW = GetHWBranch();
		if(isDevHW == S900HW_V2x||isDevHW == S900HW_V3x)
		{
			ptBarCodeDrvCfg = &S900Barcode_Ver2;
		}
		else
		{
			ptBarCodeDrvCfg = &S900Barcode_Ver1;
		}
	}
	else
	{
		tScannerAPI.ScanOpen = (void *)scan_func_nop;
		tScannerAPI.ScanRead = (void *)scan_func_nop;
		tScannerAPI.ScanClose = (void *)scan_func_nop;
		tScannerAPI.ScanUpdate = (void *)scan_func_nop;
		tScannerAPI.ScanGetVersion = (void *)scan_func_nop;
	   	return; 
	}
	type = get_module_type();
	
	switch(type)
	{
		case SCAN_SE655:
			tScannerAPI.ScanOpen = SCAN1D_SE655_Open;
			tScannerAPI.ScanRead = SCAN1D_SE655_Read;
			tScannerAPI.ScanClose = SCAN1D_SE655_Close;
			tScannerAPI.ScanUpdate = (void *)scan_func_nop;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;

			SCAN1D_SE655_Init();
			break;

		case SCAN_ME5110:
			tScannerAPI.ScanOpen = SCAN2D_ME5110_Open;
			tScannerAPI.ScanRead = SCAN2D_ME5110_Read;
			tScannerAPI.ScanClose = SCAN2D_ME5110_Close;
			tScannerAPI.ScanUpdate = (void *)scan_func_nop;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;

			SCAN2D_ME5110_Init();
			break;

		case SCAN_ZM1000:
			tScannerAPI.ScanOpen = SCAN2D_ZM1000_Open;
			tScannerAPI.ScanRead = SCAN2D_ZM1000_Read;
			tScannerAPI.ScanClose = SCAN2D_ZM1000_Close;
			tScannerAPI.ScanUpdate = SCAN2D_ZM1000_Update;
			tScannerAPI.ScanGetVersion = SCAN2D_ZM1000_Version;

			SCAN2D_ZM1000_Init();
			break;
		case SCAN_EM2039:
		case SCAN_EM2096:
			tScannerAPI.ScanOpen = SCAN2D_EM2039_Open;
			tScannerAPI.ScanRead = SCAN2D_EM2039_Read;
			tScannerAPI.ScanClose = SCAN2D_EM2039_Close;
			tScannerAPI.ScanUpdate = (void *)scan_func_nop;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;

			SCAN2D_EM2039_Init();
			break;
		case SCAN_CH6810:
			tScannerAPI.ScanOpen = SCAN2D_CH6810_Open;
			tScannerAPI.ScanRead = SCAN2D_CH_Read;
			tScannerAPI.ScanClose = SCAN2D_CH6810_Close;
			tScannerAPI.ScanUpdate = SCAN2D_CH_Update;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;
		
			SCAN2D_CH6810_Init();
			break;
		case SCAN_CH5891:
			tScannerAPI.ScanOpen = SCAN2D_CH5891_Open;
			tScannerAPI.ScanRead = SCAN2D_CH_Read;
			tScannerAPI.ScanClose = SCAN2D_CH5891_Close;
			tScannerAPI.ScanUpdate = SCAN2D_CH_Update;
			tScannerAPI.ScanGetVersion = SCAN2D_CH_Version;
		
			SCAN2D_CH5891_Init();
			break;
	    case SCAN_MT780:
			tScannerAPI.ScanOpen = SCAN1D_MT780_Open;
			tScannerAPI.ScanRead = SCAN1D_MT780_Read;
			tScannerAPI.ScanClose = SCAN1D_MT780_Close;
			tScannerAPI.ScanUpdate = (void *)scan_func_nop;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;

			SCAN1D_MT780_Init();
			break;
		case SCAN_XL7130:
			tScannerAPI.ScanOpen = SCAN2D_XL7130_Open;
			tScannerAPI.ScanRead = SCAN2D_XL7130_Read;
			tScannerAPI.ScanClose = SCAN2D_XL7130_Close;
			tScannerAPI.ScanUpdate = (void *)scan_func_nop;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;
	
			SCAN2D_XL7130_Init();
			break;
		default: 
			tScannerAPI.ScanOpen = (void *)scan_func_nop;
			tScannerAPI.ScanRead = (void *)scan_func_nop;
			tScannerAPI.ScanClose = (void *)scan_func_nop;
			tScannerAPI.ScanUpdate = (void *)scan_func_nop;
			tScannerAPI.ScanGetVersion = (void *)scan_func_nop;
			break;
	}

}

static int igScanOpen=0;
int ScanOpen(void)
{
	int ret,MatchTimes=0;
	if(igScanOpen == 1) return 0;

SCANOPEN: 
	ret = tScannerAPI.ScanOpen();
	if(ret == RET_NO_MODULE)
	   	return -2;
	else if(get_machine_type() == S300 && ret == -3 && MatchTimes == 0)
	{
		MatchTimes++;
		if(s_ScannerMatching())s_ScanInit();//打开失败可能由于匹配错误，进行重新匹配一次
		goto SCANOPEN;
	}
	else if(ret != 0)  /* module go wrong */
		return ret;

	igScanOpen = 1;
	return 0;
}

int s_ScanIsOpen(void)
{
    return igScanOpen;
}

int ScanRead(uchar *buff, uint size)
{
	int ret;

	if(igScanOpen == 0) return -1;
	if(buff == NULL || size==0) return -2;

	ret = tScannerAPI.ScanRead(buff, size);
	if(ret <= 0) return -3;

	return ret;
}

void ScanClose()
{
	tScannerAPI.ScanClose();
	igScanOpen = 0;
}

void ScanUpdate(void)
{
	tScannerAPI.ScanUpdate();
}

int ScanGetVersion(char *ver, int len)
{
	return tScannerAPI.ScanGetVersion(ver, len);
}

/* end of barcode.c */
