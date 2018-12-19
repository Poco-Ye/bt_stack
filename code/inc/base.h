#ifndef _BASE_H_
#define _BASE_H_
#include "bcm5892_reg.h"
#include "irqs.h"
#include "posapi.h"
#include "platform.h"
#include "link_info.h"
#include "..\src\multitask\paxos.h"
#include "mpatch.h"

#include "..\src\cfgmanage\cfgmanage.h"


#ifndef NULL
#define NULL 0
#endif


#define	BIT0    (1<<0)
#define	BIT1    (1<<1)
#define BIT2    (1<<2)
#define	BIT3    (1<<3)
#define BIT4    (1<<4)
#define	BIT5    (1<<5)
#define	BIT6    (1<<6)
#define BIT7    (1<<7)
#define BIT8    (1<<8)
#define BIT9    (1<<9)
#define	BIT10    (1<<10)
#define	BIT11    (1<<11)
#define BIT12    (1<<12)
#define	BIT13    (1<<13)
#define BIT14    (1<<14)
#define	BIT15    (1<<15)
#define	BIT16    (1<<16)
#define BIT17    (1<<17)
#define BIT18    (1<<18)
#define BIT19    (1<<19)
#define	BIT20    (1<<20)
#define	BIT21    (1<<21)
#define BIT22    (1<<22)
#define	BIT23    (1<<23)
#define BIT24    (1<<24)
#define	BIT25    (1<<25)
#define	BIT26    (1<<26)
#define BIT27    (1<<27)
#define BIT28    (1<<28)
#define BIT29    (1<<29)
#define BIT30    (1<<30)
#define BIT31    (1<<31)

#define P60S    1
#define P70     2
#define P60S1   3
#define P80     4
#define P78     5
#define P90     6
#define S80     7
#define SP30    8
#define S60     9
#define S90     10
#define S78     11
#define MT30    12
#define T52     13
#define S58     14
#define D200    15
#define D210    16
#define D300    17
#define T610    18
#define T620    19
#define S300    20
#define S800    21
#define S900    22
#define D180	23
#define S500    24
#define S980    25
#define S910    26

#define B210	28

#define R50     0x80
#define P50     0x81
#define P58     0x82
#define R30     0x83
#define R50M    0x84
#define T60     0x86
#define D100    0x87
#define T100    0x88
#define S200    0x89
#define ERR_MACHINE 0xFF

//MainBoardVer
#define D210HW_Vxx 0
#define D210HW_V2x 21

#define S800HW_Vxx 0
#define S800HW_V2x 81
#define S800HW_V4x 82

#define S900HW_Vxx 0
#define S900HW_V2x 91
#define S900HW_V3x 92


// 定义中断屏蔽号
#define IRQ_MASK	0x80
#define FIQ_MASK	0x40
#define INT_MASK	0xC0

// 定义中断优先级参数
#define INTC_PRIO_0			0x00
#define INTC_PRIO_1			0x01
#define INTC_PRIO_2			0x02
#define INTC_PRIO_3			0x03
#define INTC_PRIO_4			0x04
#define INTC_PRIO_5			0x05
#define INTC_PRIO_6			0x06
#define INTC_PRIO_7			0x07
#define INTC_PRIO_8			0x08
#define INTC_PRIO_9			0x09
#define INTC_PRIO_10		0x0A
#define INTC_PRIO_11		0x0B
#define INTC_PRIO_12		0x0C
#define INTC_PRIO_13		0x0D
#define INTC_PRIO_14		0x0E
#define INTC_PRIO_15		0x0F


// 定义中断类型
#define INTC_FIQ			0x01
#define INTC_IRQ    		0x00


#define DRIVER_L2_BASE 	0x401E0000
#define DRIVER_START 	0x21000000
#define DRIVER_END     	0x24000000

#define	ENABLE_DMR			0x01//使用屏幕注册输入
#define	ENABLE_APPFILE_SIG	0x02//应用件签名校验
#define ENABLE_TIMER_CHECK	0x04//Monitor定时固件自检
#define ENABLE_PED_LOCK		0x04//PED锁定
#define ENABLE_FREQUENCY_L	0x10//连续调用敏感服务频度限制
#define	ENABLE_SENSOR_CHECK 0x20//检查SENSOR
#define ENABLE_SSAK_CHECK	0x40//作为外置PED需要交易敏感授权
#define ENABLE_POST_PED		0x80//开机自检PED密钥
//总是打开的开关
#define ENABLE_MONITOR_SIG
#define ENABLE_SENSOR_CHECKED    // 允许外部检测开关

// 定义FLASH空间地址
#define EM_BOOT_MAX_ADDR    //0x1000C000
#define EM_FLASH_BASE_ADDR	//0x10000000

//定义配置扇区
#define CONFIG_SEC			//0x20000
#define CONFIG_ADDR			//(FLASH_BASE_ADDR+CONFIG_SEC)


/**每个公钥占用字节数*/
#define	USER_PUK_LEN	560//1024


/*系统配置文件*/
#define SYS_PARA_FILE "SysConfig"
#define MAX_SYS_PARA_FILE_SIZE 64*1024
#define SET_SYSTEM_TIME_PASSWORD "SetSystemTimePassword"
#define SET_PRN_HYSTERE_PARA     "SetPrnHysterePara"
#define SET_SIM_CHAN_PARA        "SetSimChannelPara"
#define SET_AUDIO_VOLUME_PARA     "SetAudioVolumePara"
#define SET_TS_CALIBRATION_PARA  "SetTSCalibrationPara"
#define MODEM_LEVEL_INFO         "ModemLevelInfo"
#define MODEM_ANSWER_TONE_INFO   "ModemAnswerToneInfo"
#define SET_NETWORK_MODE         "SetNetworkMode"
#define SET_RELAY_MODE           "SetRelayMode"
#define MODIFY_CFG_SETTING       "ModifyCfgSetting"
#define BARCODE_SETTING_INFO	 "BarcodeSettingInfo"
//  定义自检时间
#define CHECKUP_MAX_TIME    86400000    //  20000       //  86400000        //  24 hours
// 定义显示时间
#define SHOW_MAX_TIME		10000//10s,10*1000
//  定义最大支持的应用个数
#define APP_MAX_NUM         24
#define ADDIN_MAX_NUM		10
//定义BOOT信息存放地址
#define BOOTVER_STORE_ADDR	0x1BF800
//  定义应用程序运行的地址
#define FONTLIB_ADDR        0x20800000      //  字库入口地址colin modify
#define MONITOR_RUN_ADDR    0x20000000      //  monitor的运行地址
//#define BASESO_RUN_ADDR     0x22000000    使用变量baseso_text_sec_addr记录  
#define MAPP_RUN_ADDR       0x28000000      //  主控应用的运行地址
#define SAPP_RUN_ADDR       0x28200000      //  子应用的运行地址

#define FONTLIB_AREA_SIZE   0x00500000
#define MONITOR_AREA_SIZE   0x00800000
#define BASESO_AREA_SIZE    0x00600000
#define MAPP_AREA_SIZE      0x00200000
#define SAPP_AREA_SIZE      0x00200000
#define FILE_transfer_buff  FONTLIB_ADDR

#define SO_L2_BASE 	        0x401E0000
#define TTB_BASE            0x401FC000  
#define SO_START 	        0x2C000000
#define SO_END     	        0x2DF00000
#define SO_PHY_TOP          0x43C00000 /*top address of SO lib*/

#define CHINA_VER           0x01
#define HONGKONG_VER        0x02

#define FONT_NAME_S80       "S80FONTLIB"
#define APP_NAME            "AppFile"//"ADDIN"//
#define MAPP_FLAG           "PAX-S80-MAPP"
#define SAPP_FLAG           "PAX-S80-SAPP"
#define SIGN_FLAG           "SIGNED_VER:00001"
#define DATA_SAVE_FLAG      "SAVED OK"
#define ENV_FILE            "ENVFILE"//"EnvFile"
#define CONFIG_BAK_NAME		"PAXS80CFGBAK"

#define HANDSET_NOBASE 0
#define HANDSET_CHARGEONLY	1
#define HANDSET_UARTLINK    2
#define HANDSET_BTLINK		3

extern int sprintf(char *buffer, const char *format,...);
#define  SECTION_SRAM  __attribute__ ((section (".LowPower")))
#define  IO_ADDRESS(x)  (x)


#define POS_SEC_L0 0x00      /*pos making state*/
#define POS_SEC_L1 0x01      /*security level 1 state,if tamper , need input pwd*/     
#define POS_SEC_L2 0x03      /*security level 2 state, if tamper , need Authorization code*/

#define POS_SECLEVEL_L1	0x01
#define POS_SECLEVEL_L2	0x03
#define POS_SECLEVEL_L3 0x07

#define	AES_ENCRYPT	            0x01    // 加密
#define	AES_DECRYPT 	        0x00    // 解密

#define AUTH_RES_NUM                0x13
typedef struct _POS_AUTH_INFO
{
    uint   len;
    uint TamperClear;   /* 2 -unlock,1-del app,  0-tampered*/
    uint LastBblStatus; /*tamper status before bbl unlocked*/
    ushort SecMode;
    ushort security_level;
    uchar SnDownLoadSum;
    uchar AppDebugStatus;   /*monitor ver 1 -debug, 0 - release*/
    uchar FirmDebugStatus;  /* debug ver 1 -debug, 0 - release*/
    uchar UsPukLevel;
    uchar AuthDownSn;       /*SN update enable flag*/
    uchar MachineType;
    uchar AesKey[8];    
    ushort reserve[AUTH_RES_NUM];
}POS_AUTH_INFO;

// for SysSleep Control
enum {
	MODULE_LCD=0,
	MODULE_BT,
	MODULE_RTC,
};

typedef struct _SLEEP_CTRL
{
	uchar sleep_mode; //0表示保持；1表示下电；2表示进入低功耗待机模式；其它自定义。
	uchar wake_enable; //0表示系统不可以被该模块唤醒，1表示可以。
	int param;
}SLEEP_CTRL;

typedef struct
{
	uchar ver_hard;
	uchar ver_soft[9];
	uchar ver_exboard;
	uchar ver_boot;
	uchar ver_font;
	uchar ver_mdm_soft[4];
	uchar ver_mdm_hard[20];
	uchar ver_lan[30];
	uchar cnt_font;
	uchar flag_font;
	uchar prn_type[2];
	uchar BaseSN[8];
	uchar BaseEXSN[32];
	uint  font_size;
	uint  sdram_size;
	uint  flash_size;
    uchar bt_exist;
	uchar bt_mac[6];
	char  bt_pincode[16];
	uchar eth_mac[6];	
	S_HARD_INFO cfginfo[MAX_BASE_CFGITEM];
	uchar bt_name[20];
	uchar BaseSNLeft[4];
	uchar MachineType;
    uchar reserved[16];
} __attribute__((__packed__)) BASE_INFO;
extern BASE_INFO base_info;




#endif

