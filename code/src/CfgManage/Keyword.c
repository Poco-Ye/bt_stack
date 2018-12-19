#include "cfgmanage.h"

/*********************************************************************************************
						关键字及关键字对应的检查项--开始
说明:
1.下面结构数组中，其变量命名中有UnCheck的变量，表示该项不需要对其内容合法性进行检查
				其变量命名中有Check的变量，表示该项需要对其内容合法性进行检查
2.数据结构说明：
  2.1每项的第一个字符串，为关键字(keyword)，其他若干字符串为待合法性检查的内容(context)
					    如果是UnCheck的结构，就只有keyword，没有context。
  2.2示例：
    2.2.1：一个UnCheck的项,其关键字为"TERMINAL_NAME"。可编写为：
		S_ITEMS_VALUE item_001_UnCheck[]={"CONFIG_FILE_VER"};
	2.2.2：一个Check的项,其关键字为"MODEM"，待检查的内容有"CX20493","CX93011","ESF"。可编写为：
		S_ITEMS_VALUE item_010_Check[]={"MODEM","CX20493","CX93011","ESF"};

3.增加关键字及关键字对应项须知
  3.1只增加检查内容时，只要直接在所在结构数组中的尾部加上字符串即可。
	例如给"ETHERNET"关键字对应的结构数组增加一个"DM9001",可将
	S_ITEMS_VALUE item_009_Config[]={"ETHERNET","DM9000"};改为
	S_ITEMS_VALUE item_009_Config[]={"ETHERNET","DM9000","DM9001"};
  
  3.2需要增加关键字时：
	3.2.1:首先需要创建一个该关键字对应的结构数组。
	3.2.2:再将该结构数组相关信息加入到S_VERSION_CONFIG s_Versicon_Keyword[]中。
		  加入方法为：在s_Versicon_Keyword中添加一行：FILL_ITEM(关键字对应的结构数组名),
		  具体格式参考下面的代码。
***********************************************************************************************/
//config file type
S_ITEMS_VALUE item_001_Other[]={"PN"};
S_ITEMS_VALUE item_002_Other[]={"CONFIG_FILE_VER"};
S_ITEMS_VALUE item_003_Other[]={"TERMINAL_NAME"};
S_ITEMS_VALUE item_004_Other[]={"C_TERMINAL"};
S_ITEMS_VALUE item_005_Other[]={"C_PN"};
//用来表示终端非接外观，01表示S500-R；02表示S900-R
//S500-R使用物理非接灯，打印机加热头和切纸刀之间增加1.5mm
//S900-R使用物理非接灯，打印机加热头和切纸刀之间增加6mm
//默认情况下认为使用虚拟非接灯，然后加热头和切纸刀之间距离为0
S_ITEMS_VALUE item_006_Other[]={"TERMINAL_RF_STYLE"};


//Board type
S_ITEMS_VALUE item_001_Board[]={"MAIN_BOARD"};
S_ITEMS_VALUE item_002_Board[]={"PORT_BOARD"};
S_ITEMS_VALUE item_003_Board[]={"MODEM_BOARD"};
S_ITEMS_VALUE item_004_Board[]={"RF_BOARD"};
S_ITEMS_VALUE item_005_Board[]={"ANT_BOARD"};
S_ITEMS_VALUE item_006_Board[]={"GPRS_BOARD"};
S_ITEMS_VALUE item_007_Board[]={"CDMA_BOARD"};
S_ITEMS_VALUE item_008_Board[]={"WCDMA_BOARD"};
S_ITEMS_VALUE item_009_Board[]={"TD_BOARD"};
S_ITEMS_VALUE item_010_Board[]={"WIFI_BOARD"};
S_ITEMS_VALUE item_011_Board[]={"KEY_BOARD"};
S_ITEMS_VALUE item_012_Board[]={"IP_BOARD"};
S_ITEMS_VALUE item_013_Board[]={"PRINT_BOARD"};
S_ITEMS_VALUE item_014_Board[]={"POWER_BOARD"};
S_ITEMS_VALUE item_015_Board[]={"SAM_BOARD"};
S_ITEMS_VALUE item_016_Board[]={"ICCARD_BOARD"};
S_ITEMS_VALUE item_017_Board[]={"MESH_BOARD"};
S_ITEMS_VALUE item_018_Board[]={"TELEPHONE_BOARD"};
S_ITEMS_VALUE item_019_Board[]={"DISP_BOARD"};

//Parameter type
S_ITEMS_VALUE item_001_Para[]={"RF_PARA_1"};
S_ITEMS_VALUE item_002_Para[]={"RF_PARA_2"};
S_ITEMS_VALUE item_003_Para[]={"RF_PARA_3"};
S_ITEMS_VALUE item_004_Para[]={"G_SENSOR_PARA"};
S_ITEMS_VALUE item_005_Para[]={"SEN_PARA"};
S_ITEMS_VALUE item_006_Para[]={"TOUCH_KEY_PARA_1"};
S_ITEMS_VALUE item_007_Para[]={"TOUCH_KEY_PARA_2"};
S_ITEMS_VALUE item_008_Para[]={"TOUCH_KEY_PARA_3"};


//Config info type
S_ITEMS_VALUE item_001_Config[]={"PRINTER","MP130","PT486F","FTP628","SS205","M-T183","JT-TP208","PRT-48A","PRT-488A","LTP02-245","LTPJ245G","PRT-48F","01","02","03","04","05","06","07","08"};
S_ITEMS_VALUE item_002_Config[]={"TOUCH_SCREEN","TFT1N7070-V1-E","TM035KBH08","RTP_001","01","02","03","04","05","06","07","08","09","10","11","12","13"};
S_ITEMS_VALUE item_003_Config[]={"MAG_CARD","21006540","M3-2300LQ","AP-S012","IDHA-5112","21030058","ZA9L0","MH1601-V01","NULL"};
S_ITEMS_VALUE item_004_Config[]={"IC_CARD","00","01","02","03","04","05","06","07"};
S_ITEMS_VALUE item_005_Config[]={"RF_1356M","01","02","03","04","05","06"};
S_ITEMS_VALUE item_006_Config[]={"RF_24G","NATIONZ-1","NATIONZ-1-2","NATIONZ-2","SHNM100","unFang-1","EM350","SOSIM"};
S_ITEMS_VALUE item_007_Config[]={"G_SENSOR","ADXL345BCCZ"};
S_ITEMS_VALUE item_008_Config[]={"ETHERNET","DM9000","BCM5892","ON-CHIP"};
S_ITEMS_VALUE item_009_Config[]={"MODEM","CX93011","ZA9L0","Si2457D"};
S_ITEMS_VALUE item_010_Config[]={"CDMA","EM200","Q26Elite","MC509"};
S_ITEMS_VALUE item_011_Config[]={"GPRS","Q24C","Q24E","Q2687RD","GSM0306-70","G24","BGS2","MG323-B","G620-A50","G620-Q50","G610","01","02","03","04","09"};
S_ITEMS_VALUE item_012_Config[]={"WCDMA","EM701","MU509","01","05","06","07","08","10"};
S_ITEMS_VALUE item_013_Config[]={"TD","SIM4100D"};
S_ITEMS_VALUE item_014_Config[]={"WIFI_NET","CO2128","WG1300-00","RS9110-N-11-24","02","03","04","05","06","07","08"};
S_ITEMS_VALUE item_015_Config[]={"BAR_CODE","MOTOROLA-SE955","MOTOROLA-SE655","HONEYWELL-ME5100", "ZM1000","01","02","03","04","05","06","07","08","09","11","12","13","14"};
S_ITEMS_VALUE item_016_Config[]={"BLUE_TOOTH","WT12","BM57SPP","01","02","03","04","05","06","07","08","09"};
S_ITEMS_VALUE item_017_Config[]={"SD_READER","NULL","ON-CHIP"};
S_ITEMS_VALUE item_018_Config[]={"USB","SL811HS"};
S_ITEMS_VALUE item_019_Config[]={"DUAL_SIM","TRUE"};
S_ITEMS_VALUE item_020_Config[]={"LCD","TFT_H24C117-00N","LCD_001","LCD_002","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30"};
S_ITEMS_VALUE item_021_Config[]={"TOUCH_KEY","CY8C20434","CY8C20436A"};
S_ITEMS_VALUE item_022_Config[]={"AUDIO","CS4344-CZZ","01"};
S_ITEMS_VALUE item_023_Config[]={"COMM_TYPE", "RS232A", "USB","01"};
S_ITEMS_VALUE item_024_Config[]={"BAT_DECT","TRUE","FALSE"};
S_ITEMS_VALUE item_025_Config[]={"SAM_NUM","01","02","03","04"};
S_ITEMS_VALUE item_026_Config[]={"GPS","01","02","03"};
S_ITEMS_VALUE item_027_Config[]={"CIPHER_CHIP","NULL","01","02","03","04"};
S_ITEMS_VALUE item_028_Config[]={"KEY_LAYOUT","01","02"};
S_ITEMS_VALUE item_029_Config[]={"4G","01","13","14"};
S_ITEMS_VALUE item_030_Config[]={"CAMERA_NUMBER","NULL","01","02"};
S_ITEMS_VALUE item_031_Config[]={"COULOMB_COUNTER","01","02"};
S_ITEMS_VALUE item_032_Config[]={"CAMERA_FRONT","01"};
S_ITEMS_VALUE item_033_Config[]={"CAMERA_BACK","01"};
//用于进行硬件信息合法性检测的结构体

static const S_VERSION_CONFIG s_Versicon_Keyword[]=
{
//Other type	
	FILL_ITEM(OTHER_TYPE,item_001_Other),
	FILL_ITEM(OTHER_TYPE,item_002_Other),
	FILL_ITEM(OTHER_TYPE,item_003_Other),
	FILL_ITEM(OTHER_TYPE,item_004_Other),
	FILL_ITEM(OTHER_TYPE,item_005_Other),
	FILL_ITEM(OTHER_TYPE,item_006_Other),
//PCB Board type 
	FILL_ITEM(BOARD_TYPE,item_001_Board),
	FILL_ITEM(BOARD_TYPE,item_002_Board),
	FILL_ITEM(BOARD_TYPE,item_003_Board),
	FILL_ITEM(BOARD_TYPE,item_004_Board),
	FILL_ITEM(BOARD_TYPE,item_005_Board),
	FILL_ITEM(BOARD_TYPE,item_006_Board),
	FILL_ITEM(BOARD_TYPE,item_007_Board),
	FILL_ITEM(BOARD_TYPE,item_008_Board),
	FILL_ITEM(BOARD_TYPE,item_009_Board),
	FILL_ITEM(BOARD_TYPE,item_010_Board),
	FILL_ITEM(BOARD_TYPE,item_011_Board),
	FILL_ITEM(BOARD_TYPE,item_012_Board),
	FILL_ITEM(BOARD_TYPE,item_013_Board),
	FILL_ITEM(BOARD_TYPE,item_014_Board),
	FILL_ITEM(BOARD_TYPE,item_015_Board),
	FILL_ITEM(BOARD_TYPE,item_016_Board),
	FILL_ITEM(BOARD_TYPE,item_017_Board),
	FILL_ITEM(BOARD_TYPE,item_018_Board),
	FILL_ITEM(BOARD_TYPE,item_019_Board),

//parameter type
	FILL_ITEM(PARA_TYPE,item_001_Para),
	FILL_ITEM(PARA_TYPE,item_002_Para),
	FILL_ITEM(PARA_TYPE,item_003_Para),
	FILL_ITEM(PARA_TYPE,item_004_Para),	
	FILL_ITEM(PARA_TYPE,item_005_Para),	
	FILL_ITEM(PARA_TYPE,item_006_Para),
	FILL_ITEM(PARA_TYPE,item_007_Para),	
	FILL_ITEM(PARA_TYPE,item_008_Para),
//hardware config type
	FILL_ITEM(CFG_TYPE,item_001_Config),                               
	FILL_ITEM(CFG_TYPE,item_002_Config),                               
	FILL_ITEM(CFG_TYPE,item_003_Config),                               
	FILL_ITEM(CFG_TYPE,item_004_Config),                               
	FILL_ITEM(CFG_TYPE,item_005_Config),                               
	FILL_ITEM(CFG_TYPE,item_006_Config),                               
	FILL_ITEM(CFG_TYPE,item_007_Config),                               
	FILL_ITEM(CFG_TYPE,item_008_Config),                               
	FILL_ITEM(CFG_TYPE,item_009_Config),                               
	FILL_ITEM(CFG_TYPE,item_010_Config),                               
	FILL_ITEM(CFG_TYPE,item_011_Config),                               
	FILL_ITEM(CFG_TYPE,item_012_Config),                               
	FILL_ITEM(CFG_TYPE,item_013_Config),                               
	FILL_ITEM(CFG_TYPE,item_014_Config),  
	FILL_ITEM(CFG_TYPE,item_015_Config), 
	FILL_ITEM(CFG_TYPE,item_016_Config),                               
	FILL_ITEM(CFG_TYPE,item_017_Config),
	FILL_ITEM(CFG_TYPE,item_018_Config),
	FILL_ITEM(CFG_TYPE,item_019_Config),
	FILL_ITEM(CFG_TYPE,item_020_Config),
	FILL_ITEM(CFG_TYPE,item_021_Config),	
	FILL_ITEM(CFG_TYPE,item_022_Config),
	FILL_ITEM(CFG_TYPE,item_023_Config),
	FILL_ITEM(CFG_TYPE,item_024_Config),	
	FILL_ITEM(CFG_TYPE,item_025_Config),
    FILL_ITEM(CFG_TYPE,item_026_Config),
    FILL_ITEM(CFG_TYPE,item_027_Config),
    FILL_ITEM(CFG_TYPE,item_028_Config),
    FILL_ITEM(CFG_TYPE,item_029_Config),
    FILL_ITEM(CFG_TYPE,item_030_Config),
}; 



