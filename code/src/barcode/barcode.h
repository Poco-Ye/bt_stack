#ifndef __BARCODE_H__
#define __BARCODE_H__

enum SCAN_MODULE_TYPE
{
	SCAN_SE655=1,
	SCAN_SE955,
	SCAN_UE966,
	SCAN_ME5110,
	SCAN_ZM1000,
	SCAN_EM2039,
	SCAN_EM2096,
	SCAN_CH6810,
	SCAN_CH5891,
	SCAN_MT780,
	SCAN_XL7130,
};

/* scan1d command macro define */
#define SCAN1D_AIM_ON               0xC5   // 启动瞄准，类似激光笔，平常只有一个红点 
#define SCAN1D_AIM_OFF              0xC4   // 关闭瞄准
#define SCAN1D_BEEP                 0xE6   // 蜂鸣
#define SCAN1D_CMD_ACK              0xD0   // 包有效应答
#define SCAN1D_CMD_NAK              0xD1   // 包无效应答
#define SCAN1D_DECODE_DATA          0xF3   // 解码数据
#define SCAN1D_EVENT                0xF6
#define SCAN1D_LED_ON               0xE7
#define SCAN1D_LED_OFF              0xE8
#define SCAN1D_SEND_PARAM           0xC6   // 设置SE系列某参数 
#define SCAN1D_REQUEST_PARAM        0xC7   // 读取SE系列某参数 
#define SCAN1D_REQUEST_REVISION     0xA3   // 读取版本信息 
#define SCAN1D_SCAN_ENABLE          0xE9   // 启用扫描功能 
#define SCAN1D_SCAN_DISABLE         0xEA   // 禁用扫描，也就不可以去读条码了 
#define SCAN1D_SLEEP                0xEB   // 手动进入省电模式 
#define SCAN1D_WAKEUP               0x00   // 直接发送0x00唤醒
#define SCAN1D_START_DECODE         0xE4   // 开始扫描 
#define SCAN1D_STOP_DECODE          0xE5   // 停止解码指令
#define SCAN1D_SE_DEF_PARAM         0xC8   // 恢复SE默认参数 
#define SCAN1D_UE_DEF_PARAM         0xD8   // 恢复uE默认参数 
#define SCAN1D_SE_CUSTOM_DEF_PARAM  0x12   // 恢复SE系列客户自定义默认参数值
#define SCAN1D_UE_CUSTOM_DEF_PARAM  0x22   // 恢复uE系列客户自定义默认参数值

#define SCAN1D_NAK_RESEND           1
#define SCAN1D_NAK_BAD_CONTEXT      2
#define SCAN1D_NAK_DENIED           6

#define RET_NO_MODULE				0x12345678
typedef struct 
{
	int (*ScanOpen)(void);
	int (*ScanRead)(unsigned char *buff, uint size);
	void (*ScanClose)(void);
	void (*ScanUpdate)(void);
	int (*ScanGetVersion)(char *ver, int len);
}SCANNER_API_T;

int ScanOpen(void);
int ScanRead(unsigned char *buff, uint size);
void ScanClose(void);
void ScanUpdate(void);
int ScanGetVersion(char *ver, int len);
extern int is_barcode_module(void);

#endif /* __BARCODE_H__ */
