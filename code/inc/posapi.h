/*****************************************************/
/* Pax.h                                             */
/* Define the Application Program Interface          */
/* for PAX POS terminals S80                         */
/*****************************************************/

#ifndef  _PAX_POS_API_H
#define  _PAX_POS_API_H

//Standard header files
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <ctype.h>
//TODO:  Add other header files

#ifndef     uchar
#define     uchar               unsigned char
#endif
#ifndef     uint
#define     uint                unsigned int
#endif
#ifndef     ulong
#define     ulong               unsigned long
#endif
#ifndef     ushort
#define     ushort              unsigned short
#endif

typedef int COLORREF;
#define NO_FILL_COLOR    1

//============================================
//      Defined for input key values
//============================================

//  P90具有的键值
#define     KEY1                0x31
#define     KEY2                0x32
#define     KEY3                0x33
#define     KEY4                0x34
#define     KEY5                0x35
#define     KEY6                0x36
#define     KEY7                0x37
#define     KEY8                0x38
#define     KEY9                0x39
#define     KEY0                0x30

#define     KEYCLEAR            0x08
#define     KEYALPHA            0x07
#define     KEYUP               0x05
#define     KEYDOWN             0x06
#define     KEYFN               0x15
#define     KEYMENU             0x14
#define     KEYENTER            0x0d
#define     KEYCANCEL           0x1b
#define     NOKEY               0xff

//  S80没有的键值
#define     KEYF1               0x01
#define     KEYF2               0x02
#define     KEYF3               0x03
#define     KEYF4               0x04
#define     KEYF5               0x09
#define     KEYF6               0x0a
#define     KEY00               0x3a
#define     KEYSETTLE           0x16
#define     KEYVOID             0x17
#define     KEYREPRINT          0x18
#define     KEYPRNDOWN          0x1a
#define     KEYPRNUP            0x19

// S80增加的ATM键
#define     KEYATM1             0x3B
#define     KEYATM2             0x3C
#define     KEYATM3             0x3D
#define     KEYATM4             0x3E
// S90 S80 增加的组合键
#define     FKEYFN               0x15
#define     FNKEY1                0x91
#define     FNKEY2                0x92
#define     FNKEY3                0x93
#define     FNKEY4                0x94
#define     FNKEY5                0x95
#define     FNKEY6                0x96
#define     FNKEY7                0x97
#define     FNKEY8                0x98
#define     FNKEY9                0x99
#define     FNKEY0                0x90
#define     FNKEYCLEAR            0x9a
#define     FNKEYALPHA            0x9b
#define     FNKEYUP               0x9c
#define     FNKEYDOWN             0x9d
#define     FNKEYMENU             0x9e
#define     FNKEYENTER            0x9f
#define     FNKEYCANCEL           0xa0
#define     FNKEYATM1             0xa1
#define     FNKEYATM2             0xa2
#define     FNKEYATM3             0xa3
#define     FNKEYATM4             0xa4
#define     FNKEYF2               0xa5 

// ENTER组合键
#define     ENKEY0                0xc0
#define     ENKEY1                0xc1
#define     ENKEY2                0xc2
#define     ENKEY3                0xc3
#define     ENKEY4                0xc4
#define     ENKEY5                0xc5
#define     ENKEY6                0xc6
#define     ENKEY7                0xc7
#define     ENKEY8                0xc8
#define     ENKEY9                0xc9
#define     ENKEYCLEAR            0xca
#define     ENKEYALPHA            0xcb
#define     ENKEYUP               0xcc
#define     ENKEYDOWN             0xcd
#define     ENKEYMENU             0xce
//#define     ENKEYENTER            0xcf//不会出现此按键
#define     ENKEYCANCEL           0xd0
#define     ENKEYATM1             0xd1
#define     ENKEYATM2             0xd2
#define     ENKEYATM3             0xd3
#define     ENKEYATM4             0xd4
#define     ENKEYF2               0xd5 
//================================================
//               System functions
//================================================
uchar   SystemInit(void);
int     GetLastError(void);

int SysParaRead(const char *name,char *value);
int SysParaWrite(const char *name,const char *value);

//for speak
void    Beep(void);
void    Beepf(uchar mode,ushort DlyTime);

//for real time
uchar   SetTime(uchar *time);
void    GetTime(uchar *time);

//for timer event
void    TimerSet(uchar TimerNo, ushort Cnts);
ushort  TimerCheck(uchar TimerNo);
int     SetTimerEvent(ushort uElapse, void (*TimerFunc)(void));
void    KillTimerEvent(int handle);

//for all hardware and software version
void    ReadSN(uchar *SerialNo);
void    EXReadSN(uchar *SN);
uchar   ReadVerInfo(uchar *VerInfo);
int     GetTermInfo(uchar *out_info);


// file change interface
int FileToApp(uchar *strFileName);
int FileToParam(uchar *strSrcFileName, uchar *strAppName, int mode);

//================================================
//                 Keyboard functions
//================================================
uchar   kbhit(void);
uchar   _getkey(void);
uchar   getkey(void);   //same as "_getkey()";
void    kbflush(void);
void    kbmute(uchar flag);
void    kbsound(uchar mode,ushort dlytime);

uchar   GetString(uchar *str,uchar mode,uchar minlen,uchar maxlen);
uchar   GetHzString(uchar *outstr, uchar max, ushort TimeOut);
void    kblight(uchar mode);

//================================================
//      LCD functions
//================================================
#define     ASCII               0x00
#define     CFONT               0x01
#define     REVER               0x80
#define     ICON_PHONE          1       //  phone 电话
#define     ICON_SIGNAL         2       //  wireless signal 信号
#define     ICON_PRINTER        3       //  printer 打印机
#define     ICON_ICCARD         4       //  smart card IC卡
#define     ICON_LOCK           5       //  lock 锁
#define     ICON_BATTERY        6       //  电池图标(P90)
#define     ICON_UP             7       //  up 向上
#define     ICON_DOWN           8       //  down 向下
//#define     ICON_SPEAKER        9       //  speeker 扬声器(P80 & P78)
#define 	ICON_BT				11
#define 	ICON_WIFI			12
#define		ICON_ETH			13

#define     ICON_NUMBER         13       //  图标的个数

#define     CLOSEICON           0       //  关闭图标[针对所有图标]
#define     OPENICON            1       //  显示图标[针对打印机、IC卡、锁、电池、向上、向下]

void    ScrCls(void);
void    ScrClrLine(uchar startline, uchar endline);
void    ScrGray(uchar mode);
void    ScrBackLight(uchar mode);
void    ScrGotoxy (uchar x,uchar y);
uchar   ScrFontSet(uchar fontsize);
uchar   ScrAttrSet(uchar Attr);
void    ScrPlot(uchar X, uchar Y, uchar Color);
void    ScrPrint(uchar col,uchar row,uchar mode, char *str,...);
void    ScrDrLogo(uchar *logo);
void    ScrSetIcon(uchar icon_no,uchar mode);
void    ScrDrLogoxy(int x,int y,uchar *logo);
int     Lcdprintf(uchar *fmt,...);
uchar   ScrRestore(uchar mode);
void    ScrDrawBox(uchar y1,uchar x1,uchar y2,uchar x2);
int     printf(const char *fmt,...);
void    SetLightTime(ushort LightTime);


//only for p60-a
//uchar IccAttribute(uchar slot);
void    Mc_Clk_Enable(uchar slot,uchar mode);
void    Mc_Io_Dir(uchar slot,uchar mode);
uchar   Mc_Io_Read(uchar slot);
void    Mc_Io_Write(uchar slot,uchar mode);
void    Mc_Vcc(uchar slot,uchar mode);
void    Mc_Reset(uchar slot,uchar mode);
void    Mc_Clk(uchar slot,uchar mode);
void    Mc_C4_Write(uchar slot,uchar mode);
void    Mc_C8_Write(uchar slot,uchar mode);
uchar   Mc_C4_Read(uchar slot);
uchar   Mc_C8_Read(uchar slot);
uchar   Read_CardSlotInfo(uchar slot);
void    Write_CardSlotInfo(uchar slot,uchar slotopen);

//define myfare card api
/*typedef struct
{
	uchar drv_ver[5];  //e.g. "1.01A", read only
	uchar drv_date[12]; //e.g. "2006.08.25",read only

	uchar a_conduct_w;  //Type A conduct write enable: 1--enable,else disable
	uchar a_conduct_val;//Type A output conduct value,0~63

	uchar m_conduct_w;  //M1 conduct write enable: 1--enable,else disable
	uchar m_conduct_val;//M1 output conduct value,0~63

	uchar b_modulate_w;  //Type B modulate index write enable,1--enable,else disable
	uchar b_modulate_val;//Type B modulate index value

	uchar card_buffer_w;//added in V1.00C,20061204
	ushort card_buffer_val;//max_frame_size of PICC

	uchar wait_retry_limit_w;//added in V1.00F,20071212
	ushort wait_retry_limit_val;//max retry count for WTX block requests,default 3

	uchar reserved[94]; //must be zero if write
}PICC_PARA;

typedef struct
{
	uchar drv_ver[10];  //e.g. "1.01A", read only
	uchar drv_date[12]; //e.g. "2006.08.25",read only

	uchar A_CWGsP_w;
	uchar A_CWGsP_Val; // CWGsPREG ADD:28H,DEF VAL:0x3F

	uchar A_CWGsNOn_w;
	uchar A_CWGsNOn_Val; // GsNOnREG bit7~4 ADD:27H,DEF VAL:0xF0 低4位数据将被清除

	uchar B_ModGsP_w;
	uchar B_ModGsP_Val; // ModGsPREG bit5~0 add 29h, DEF val:0x11 

	uchar B_ModGsNOn_w;
	uchar B_ModGsNOn_Val; // ModGsNOnREG bit3~0 add 27h, DEF val:0x04h 高4位数据将被清除
	
	uchar  card_buffer_w;//added in V1.00C,20061204
	ushort card_buffer_Val;//max_frame_size of PICC

	uchar reserved[97]; //must be zero if write
}PICC_PARA_INFO;
*/

//define myfare card api
typedef struct
{
	unsigned char  drv_ver[5];  //驱动程序的版本信息，如：”1.01A”；只能读取，写入无效
	unsigned char drv_date[12];  // 驱动程序的日期信息，如：”2006.08.25”； 只能读取
	unsigned char a_conduct_w;  //A型卡输出电导写入允许：1--允许，其它值—不允许
	unsigned char a_conduct_val;  // A型卡输出电导控制变量，有效范围0~63,超出时视为63
	//a_conduct_w=1时才有效，否则该值在驱动内部不作改变//用于调节驱动A型卡的输出功率，由此能调节其最大感应距离
	unsigned char m_conduct_w;  //M1卡输出电导写入允许：1--允许，其它值—不允许
	unsigned char m_conduct_val;  // M1卡输出电导控制变量，有效范围0~63,超出时视为63
	//m_conduct_w=1时才有效，否则该值在驱动内部不作改变 //用于调节驱动M1卡的输出功率，由此能调节其最大感应距离
	unsigned char b_modulate_w;  // B型卡调制指数写入允许：1--允许，其它值—不允许
	unsigned char b_modulate_val;  // B型卡调制指数控制变量，有效范围0~63,超出时视为63
	// b_modulate_w=1时才有效，否则该值在驱动内部不作改变,用于调节驱动B型卡的调制指数，由此能调节其最大感应距离
	unsigned char  card_buffer_w;  //卡片接收缓冲区大小写入允许：1--允许，其它值—不允许
	unsigned short card_buffer_val; //卡片接收缓冲区大小参数（单位：字节），有效值1~256。
	//大于256时，将以256写入；设为0时，将不会写入。
	//卡片接收缓冲区大小直接决定了终端向卡片发送一个命令串时是否需启动分包发送、以及各个分包的最大包大小。若待发送的命令串大于卡片接收缓冲区大小，则需将它切割成小包后，连续逐次发送
	//在PiccDetect( )函数执行过程中，卡片接收缓冲区大小之参数由卡片报告给终端，一般无需更改此值。但对于非标准卡，可能需要重设此参数值，以保证传输有效进行
	unsigned char  wait_retry_limit_w;// S(WTX)响应发送次数写入允许：1--允许，其它值—不允许 (暂不适用)
	unsigned short wait_retry_limit_val;//S(WTX)响应最大重试次数, 默认值为3(暂不适用)
	// 20080617 
	unsigned char card_type_check_w; // 卡片类型检查写入允许：1--允许，其它值--不允许，主要用于避免因卡片不规范引起的问题
	unsigned char card_type_check_val; // 0-检查卡片类型，其他－不检查卡片类型(默认为检查卡片类型)
	//2009-10-30
	unsigned char card_RxThreshold_w; //接收灵敏度检查写入允许：1--允许，其它值--不允许，主要用于避免因卡片不规范引起的问题
	unsigned char card_RxThreshold_val;//天线参数为5个字节时，为A卡和B卡的接收灵敏度，天线参数为7个字节时为B卡接收灵敏度
	//2009-11-20
	unsigned char f_modulate_w; // felica调制指数写入允许
	unsigned char f_modulate_val;//felica调制指数

	//add by wls 2011.05.17
	unsigned char a_modulate_w; // A卡调制指数写入允许：1--允许，其它值—不允许
	unsigned char a_modulate_val; // A卡调制指数控制变量，有效范围0~63,超出时视为63
	
    //add by wls 2011.05.17
	unsigned char a_card_RxThreshold_w; //接收灵敏度检查写入允许：1--允许，其它值--不允许
	unsigned char a_card_RxThreshold_val;//A卡接收灵敏度
	
	//add by liubo 2011.10.25, 针对A,B和C卡的天线增益
	unsigned char a_card_antenna_gain_w;
	unsigned char a_card_antenna_gain_val;
	
	unsigned char b_card_antenna_gain_w;
	unsigned char b_card_antenna_gain_val;
	
	unsigned char f_card_antenna_gain_w;
	unsigned char f_card_antenna_gain_val;
	
	//added by liubo 2011.10.25，针对Felica的接收灵敏度
	unsigned char f_card_RxThreshold_w;
	unsigned char f_card_RxThreshold_val;

	/* add by wanls 2012.08.14*/
	unsigned char f_conduct_w;  
	unsigned char f_conduct_val; 
	
	/* add by nt for paypass 3.0 test 2013/03/11 */
	unsigned char user_control_w;
	unsigned char user_control_key_val;

	/* add by wanls 20141106*/
	unsigned char protocol_layer_fwt_set_w;
	unsigned int  protocol_layer_fwt_set_val;
	/* add end */

	/* add by wanls 20150921*/
	unsigned char picc_cmd_exchange_set_w;
	/*Bit0 = 0 Disable TX CRC, Bit0 = 1 Enable TX CRC*/
	/*Bit1 = 0 Disable RX CRC, Bit1 = 1 Enable RX CRC*/
	unsigned char picc_cmd_exchange_set_val;
	/* add end */
	
	unsigned char reserved[60]; //modify by nt 2013/03/11 original 74.保留字节，用于将来扩展；写入时应全清零


}PICC_PARA;/*size of PICC_PARA is 124 Bytes*/



//================================================
//          Magcard reader functions
//================================================
void    MagReset(void);
void    MagOpen(void);
void    MagClose(void);
uchar   MagSwiped(void);
uchar   MagRead(uchar *Track1, uchar *Track2, uchar *Track3);

//================================================
//         Defined for modem communication
//=================================================
typedef struct
{
    uchar       DP;
    uchar       CHDT;
    uchar       DT1;
    uchar       DT2;
    uchar       HT;
    uchar       WT;
    uchar       SSETUP;
    uchar       DTIMES;
    uchar       TimeOut;
    uchar       AsMode;
}COMM_PARA;
//Modem communication functions
uchar   ModemReset(void);
uchar   ModemDial(COMM_PARA *MPara, uchar *TelNo, uchar mode);
uchar   ModemCheck(void);
uchar   ModemTxd(uchar *SendData, ushort len);
uchar   ModemRxd(uchar *RecvData, ushort *len);
uchar   OnHook(void);
uchar   ModemAsyncGet(uchar *ch);
ushort  ModemExCommand(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms);

//================================================
//     Asynchronism communication functions
//================================================
#define     COM1                0
#define     COM2                1       //  P90 define COM2 as 2, but no use
#define     WNETPORT            1       //  only for P90 direct physical port

#define     RS485               2       //  for P80,P78's logistic port
#define     PINPAD              3       //  for P80,P78's logistic port
#define     IC_COMMPORT         4       //  for P80,P78's logistic port

#define     RS232A              0       //  for P80,P78's logistic port
#define     RS232B              1       //  for P80,P78's logistic port
#define     LANPORT             2       //  for P80,P78's logistic port
#define     MODEM               4       //  for P80,P78's logistic port

#define     BASECOM1            0
#define     BASECOM2            1
#define     BASERS485           2
#define     BASEPINPAD          3
#define     HANDSETCOM1         4

uchar   PortOpen(uchar channel, uchar *Attr);
uchar   PortClose(uchar channel);
uchar   PortSend(uchar channel, uchar ch);
uchar   PortRecv(uchar channel, uchar *ch, uint ms);
uchar   PortReset(uchar channel);
uchar   PortSends(uchar channel,uchar *str,ushort str_len);
uchar   PortTxPoolCheck(uchar channel);

//for p70 RS232 communication
uchar   CheckDSR(void);
void    SetDTR(uchar ready);
uchar   EnableSwitch(void);
uchar   RS485AttrSet(uchar mode);
void    SetOffBaseProc(uchar (*Handle)());

//================================================
//            Printer functions
//================================================
#define     PRN_OK              0x00
#define     PRN_BUSY            0x01
#define     PRN_PAPEROUT        0x02
#define     PRN_WRONG_PACKAGE   0x03
#define     PRN_FAULT           0x04
#define     PRN_TOOHEAT         0x08
#define     PRN_UNFINISHED      0xf0
#define     PRN_NOFONTLIB       0xfc
#define     PRN_OUTOFMEMORY     0xfe


uchar   PrnInit(void);
void    PrnFontSet(uchar Ascii, uchar CFont);
void    PrnSpaceSet(uchar x, uchar y);
void    PrnLeftIndent(ushort Indent);
//void  PrnStep(uchar pixel);
void    PrnStep(short pixel);
uchar   PrnStr(char *str,...);
uchar   PrnLogo(uchar *logo);
uchar   PrnStart(void);
uchar   PrnStatus(void);

int     PrnGetDotLine(void);
void    PrnSetGray(int Level);
int     PrnGetFontDot(int FontSize, uchar *str, uchar *OutDot);

//================================================
//     Encrypt and decrypt functions
//================================================
#define     ENCRYPT             1
#define     DECRYPT             0

void    des(uchar *input, uchar *output,uchar *deskey, int mode);

//================================================
//        Defined for file system
//================================================
#define     FILE_EXIST          1
#define     FILE_NOEXIST        2
#define     MEM_OVERFLOW        3
#define     TOO_MANY_FILES      4
#define     INVALID_HANDLE      5
#define     INVALID_MODE        6
#define     NO_FILESYS          7
#define     FILE_NOT_OPENED     8
#define     FILE_OPENED         9
#define     END_OVERFLOW        10
#define     TOP_OVERFLOW        11
#define     NO_PERMISSION       12
#define     FS_CORRUPT          13

#define     O_RDWR              0x01
#define     O_CREATE            0x02
#define     O_ENCRYPT           0x04


//for the <stdio.h> had define it
#undef SEEK_CUR
#undef SEEK_SET
#undef SEEK_END
#define     SEEK_CUR            0
#define     SEEK_SET            1
#define     SEEK_END            2

typedef struct
{
    uchar       fid;
    uchar       attr;
    uchar       type;
    char        name[17];
    ulong       length;
} FILE_INFO;

extern uint      errno;
extern int      _app_para;

//File operation functions
void    InitFileSys(void);
int     open(char *filename, uchar mode);
int     read(int fid, uchar *dat, int len);
int     write(int fid, uchar *dat, int len);
int     close(int fid);
int     seek(int fid, long offset, uchar fromwhere);
long    filesize(char *filename);
long    freesize(void);
int     truncate(int fid,long len);
int     fexist(char *filename);
int     GetFileInfo(FILE_INFO* finfo);
int     ex_open(char *filename, uchar mode,uchar* attr);

long    tell(int fd);


uchar   GetEnv(char *name, uchar *value);
uchar   PutEnv(char *name, uchar *value);

//================================================
//     MultiApplication functions,called by AppManager.
//================================================
typedef struct
{
    uchar       AppName[32];
    uchar       AID[16];
    uchar       AppVer[16];
    uchar       AppProvider[32];
    uchar       Descript[64];
    uchar       DownloadTime[14];
    ulong       security_level;
    ulong       EventAddress;
    uchar       AppNum;
    uchar       RFU[73];
}APPINFO;


#define     MAGCARD_MSG         0x01
#define     ICCARD_MSG          0x02
#define     KEYBOARD_MSG        0x03
#define     USER_MSG            0x04

typedef struct
{
    uchar       RetCode;
    uchar       track1[256];
    uchar       track2[256];
    uchar       track3[256];
}ST_MAGCARD;

typedef struct
{
    int         MsgType;            //  MAGCARD_MSG,ICCARD_MSG,KEYBOARD_MSG,USER_MSG
    ST_MAGCARD  MagMsg;             //  MAGCARD_MSG
    uchar       KeyValue;           //  ICCARD_MSG
    uchar       IccSlot;            //  KEYBOARD_MSG
    void        *UserMsg;           //  USER_MSG
}ST_EVENT_MSG;

int     ReadAppInfo(uchar AppNo, APPINFO* ai);
uchar   ReadAppState(uchar AppNo);
int     SetAppActive(uchar AppNo, uchar flag);
int     RunApp(uchar AppNo);
int     DoEvent(uchar AppNo, ST_EVENT_MSG *msg);
//int DoEvent(uchar AppNo, void* para);
//#ifndef _STATIC_LIB_
//int event_main(ST_EVENT_MSG *msg);
//#endif

void    uart_echo(char* str);
int     uart_echoch(int ch, char* com_p);
void    uart_printf(const char *fmt,...);
void    uart_echoin(char* ch);


//void port_echo(uchar channel, char* str);
//void port_echoin(uchar channel, char* ch);
//int  port_echoch(int channel, char *ch);
//void port_printf(uchar channel, const char *fmt,...);
//int  echoch(char ch);



/*
//define myfare card api
uchar MyfareReset(void);
uchar CLCRequest(uchar CardType,uchar ReqCode, uchar *SerialNo);
uchar CLCSelect(uchar *SerialNo);
uchar M1Authority(uchar Type,uchar Block,uchar *Pwd,uchar *snr);
uchar M1ReadBlock(uchar Block,uchar *BlockValue);
uchar M1WriteBlock(uchar Block,uchar *BlockValue);
uchar M1Operate(uchar Type,uchar Block ,uchar *Value,uchar TranBlock);
void M1_HALT(void);
*/



//Module status related macros
#define     WNET_UNKNOWN        0x00
#define     WNET_CDMA           0x01
#define     WNET_GPRS           0x02
//#define WNET_DETACHED         0x00
//#define WNET_ATTACHED         0x01
//Port related macros
#define     COMNUM              0x02        //  Serial port number, CPU to Module, P90 define as 1, P78,P80 define as 2,S80 define as 2

#define     PORT_CLOSED         0x00
#define     PORT_OPENED         0x01

#define     PSEND_OK            0x00        //  serial port send successfully
#define     PSEND_ERR           0x01        //  serial port send error
#define     PRECV_UNOPEN        -2
#define     PRECV_ERR           -1          //  serial port receive error
#define     PRECV_TIMEOUT       0           //  serial port receive timeout

//General macros
#define     RET_OK              0x00        //  return successfully
#define     RET_ERR             0x01        //  error occurred
#define     RET_NORSP           0x02        //  no response from the module
#define     RET_RSPERR          0x03        //  "ERROR" is return from the module
#define     RET_NOSIM           0x04        //  SIM/UIM card is not inserted or not detected
#define     RET_NEEDPIN         0x05        //  SIM PIN is required
#define     RET_NEEDPUK         0x06        //  SIM PUK is required
#define     RET_SIMBLOCKED      0x07        //  SIM Card is permanently blocked
#define     RET_SIMERR          0x08        //  SIM card does not exist or needs SIM PIN
#define     RET_PINERR          0x09        //  SIM PIN is incorrect
#define     RET_NOTDIALING      0x0A        //  the module is not in dialing status
#define     RET_PARAMERR        0x0B        //  parameter error
#define     RET_FORMATERR       0x0C        //  Format error
#define     RET_SNLTOOWEAK      0x0D        //  the signal is too weak, please check the antenna
#define     RET_LINKCLOSED      0x0E        //  the module is offline
#define     RET_LINKOPENED      0x0F        //  the module is online
#define     RET_LINKOPENING     0x10        //  the module is connecting to the network
#define     RET_TCPCLOSED       0x11        //  tcp socket is closed
#define     RET_TCPOPENED       0x12        //  tcp socket is opened
#define     RET_TCPOPENING      0x13        //  the module is trying to open a TCP socket
#define     RET_ATTACHED        0x14        //  Attached
#define     RET_DETTACHED       0x15        //  Dettached
#define     RET_ATTACHING       0x16        //  the module is looking for the base station.
#define     RET_NOBOARD         0x17        //  no GPRS or CDMA board exist
#define     RET_UNKNOWNTYPE     0x18        //  unknown type
#define     RET_EMERGENCY       0x19        //  SIM/UIM is in emergency status

#define     RET_RING            0x1A        //  detect ringing
#define     RET_BUSY            0x1B        //  detect call in progress
#define     RET_DIALING         0x1C        //  dialing

#define     RET_PORTERR         0xFD        //  serial port error
#define     RET_PORTINUSE       0xFE        //  serial port is in use by another program
#define     RET_ABNORMAL        0xFF        //  abnormal error


/**********************************************
            Functions Declaration
**********************************************/
uchar   WNetInit(void);

uchar   WNetCheckSignal(uchar *pSignalLevel);
uchar   WNetCheckSim(void);
uchar   WNetSimPin(uchar *pin);
uchar   WNetUidPwd(uchar *Uid, uchar *Pwd);

uchar   WNetDial(uchar *DialNum, uchar *reserved1, uchar *reserved2);
uchar   WNetCheck(void);      //检查拨号返回
uchar   WNetClose(void);
uchar   WNetLinkCheck(void);  //检查拨号连接状态: CONNECT/NO CARRIER

uchar   WNetTcpConnect(uchar *DestIP, uchar *DestPort);
uchar   WNetTcpClose(void);
uchar   WNetTcpCheck(void);   //检查TCP连接状态: OPEN/CLOSED

uchar   WNetTcpTxd(uchar *TxData, ushort txlen);
uchar   WNetTcpRxd(uchar *RxData, ushort *prxlen, ushort ms);

uchar   WNetReset(void);

uchar   WNetSendCmd(uchar *cmd, uchar *rsp, ushort rsplen, ushort ms);

//  语音功能
uchar   WPhoneCall(uchar *PhoneNum);
uchar   WPhoneHangUp(void);
uchar   WPhoneStatus(void);
uchar   WPhoneAnswer(void);
uchar   WPhoneMicGain(uchar Level);
uchar   WPhoneSpkGain(uchar Level);
uchar   WPhoneSendDTMF(uchar DTMFNum, ushort DurationMs);


typedef struct
{
    uchar       connected;
    uchar       remote_closed;
    uchar       has_data_in;
    uchar       outbuf_emptied;
    ushort      left_outbuf_count;
    uchar       remote_addr[4];
    uchar       reserved1[30];
    long        link_state;
    uchar       local_addr[4];
    uchar       dns1_addr[4];
    uchar       dns2_addr[4];
    uchar       gateway_addr[4];
    uchar       subnet_mask[4];
    uchar       reserved2[30];
}PPP_NET_INFO;




//--Point to Point Protocol operation interface
int     NpppLogin(uchar *user_name,uchar *user_password);
int     NpppOpen(uchar *remote_addr,ushort remote_port,ushort local_port,uchar mode);
int     NpppRead(int connect_handle,uchar *out_data,ushort len);
int     NpppWrite(int connect_handle,uchar *buf,ushort len);
void    NpppProcess(void);
void    NpppCheck(int connect_handle,PPP_NET_INFO *pi);
int     NpppClose(int connect_handle);

//-------only for the test

uint GetTimerCount(void);               //  仅提供测试用的计时功能

int PrnTemperature(void);               //  仅提供测试用的功能


//--------PCI PED

#endif
