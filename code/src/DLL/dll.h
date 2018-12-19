#ifndef _S80_DLL_H
#define _S80_DLL_H

#define DLL_PARAM_ADDR      0x285F0000

//Define function types
#define DLL_SYS_FUN             0x01
#define DLL_IC_FUN              0x02
#define DLL_MAG_FUN             0x03
#define DLL_LCD_FUN             0x04
#define DLL_KEYBOARD_FUN        0x05
#define DLL_COMM_FUN            0x06
#define DLL_PRINT_FUN           0x07
#define DLL_FILE_FUN            0x08
#define DLL_PINPAD_FUN          0x09
#define DLL_PED_FUN             0x0d
#define DLL_IRDA_FUN            0x0a
#define DLL_EXPORT_FUN          0x0b
#define DLL_OTHER_FUN           0x0c
#define DLL_PICC_FUN          0x0e
#define DLL_WNET_FUN            0x0f
#define DLL_REMOTE_FUN          0x10
#define DLL_PPP_FUN             0x11
#define DLL_PCI_FUN		        0x12

#define DLL_LNET_FUN			0x13 //IP模块,PPP,以太网
#define DLL_FAT_FUN				0x14
#define DLL_EXWNET_FUN			0x15
#define DLL_SSL_FUN             0x16
#define DLL_TILTSENSOR_FUN      0x17
#define DLL_E2EE_FUN		    0x18
#define DLL_WAVE_FUN            0x19
#define DLL_SCAN_FUN            0x1a
#define DLL_TOUCHSCREEN_FUN		0x1b
#define DLL_BT_FUN				0x1C
#define DLL_GPS_FUN				0x1D

//REMOTE Download function
#define DLL_REMOTELOADAPP       0x01
#define DLL_GETLOADEDAPPSTATUS  0x02
#define DLL_QUERYLOADTASK       0x03
#define DLL_REMOTELOADMANAGE    0x04
#define DLL_PROTIMSENTRY         0x05

//Point to Point Protocol operation interface
#define DLL_PPPOPEN             0x01
#define DLL_PPPREAD             0x02
#define DLL_PPPWRITE            0x03
#define DLL_PPPPROCESS          0x04
#define DLL_PPPCHECK            0x05
#define DLL_PPPCLOSE            0x06
#define DLL_PPPLOGIN            0x07

//Define SYS_FUN function
#define DLL_BEEP                0x01
#define DLL_BEEF                0x02
#define DLL_GETTIME             0x03
#define DLL_DELAYMS             0x04
#define DLL_TIMERSET            0x05
#define DLL_READSN              0x06
#define DLL_EXREADSN            0x07
#define DLL_GETLASTERROR        0x08
#define DLL_SETTIMEREVENT       0x09
#define DLL_KILLTIMEREVENT      0x0a
#define DLL_SETTIME             0x0b
#define DLL_TIMERCHECK          0x0c
#define DLL_READVERINFO         0x0d
#define DLL_CHECKFIRST          0x0e
#define DLL_POWEROFF            0x0f
#define DLL_FILE_TO_APP         0x17
#define DLL_FILE_TO_PARAM       0x18
#define DLL_READ_FONTLIB		0x19
#define DLL_ENUM_FONT			0x1a
#define DLL_SEL_SCR_FONT		0x1b
#define DLL_SEL_PRN_FONT		0x1c
#define DLL_REBOOT				0x1d
#define DLL_SYS_PCIGETRANDOM    0x1f
#define DLL_SYS_SLEEP           0x20
#define DLL_GET_EXTINFO			0x21
#define DLL_ENUM_BASE_FONT	    0x22
#define DLL_SYS_CONFIG			0x23
#define DLL_GET_HWCFG			0x24
#define DLL_LED_DISPLAY         0x25
#define DLL_SYS_IDLE			0x26
#define DLL_GETCURSCRFONT       0x27
#define DLL_READCSN             0x28
#define DLL_SYS_WAKEUP_CHANNEL  0x29
#define DLL_READ_TUSN 			0x2a

//for p66
#define DLL_AUTOSHUTDOWN        0x10
#define DLL_AUTOSHUTDOWNSEC     0x11
#define DLL_OFFBASE             0x12
#define DLL_ONBASE              0x13
#define DLL_BASERESET           0x14
#define DLL_BATTERYCHECK        0x15

//for P90
#define DLL_POWERSAVE           0x31
#define DLL_AUTOPOWERSAVE       0x32
#define DLL_GETTERMINFO         0x40

#define DLL_GETTIMERCOUNT_P90   0x80
#define DLL_GETTIMERCOUNT_P80   0x16
#define DLL_GETVOLT             0x81

//Define KEYBOARD_FUN function
#define DLL_KBHIT               0x01
#define DLL_KBFLUSH             0x02
#define DLL__GETKEY             0x03
#define DLL_GETKEY              0x04
#define DLL_KBMUTE              0x05
#define DLL_KBBEEF              0x06
#define DLL_KBSOUND             0x07
#define DLL_GETSTRING           0x08

//for p66
#define DLL_GETHZSTRING         0x09
#define DLL_GETSTRINGFORMAT     0x0a
#define DLL_GETSTRINGRESULT     0x0b

//for D200
#define DLL_KBLOCK		0x0c
#define DLL_KBCHECK	0x0d

//for p90
#define DLL_KBLIGHT             0x20
#define DLL_SETSLIPFW           0x21

//Define LCD_FUN function
#define DLL_SCRCLS              0x01
#define DLL_SCRCLRLINE          0x02
#define DLL_SCRGRAY             0x03
#define DLL_SCRBACKLIGHT        0x04
#define DLL_SCRGOTOXY           0x05
#define DLL_SCRFONTSET          0x06
#define DLL_SCRATTRSET          0x07
#define DLL_SCRPLOT             0x08
#define DLL_SCRPRINT            0x09
#define DLL_SCRDRLOGO           0x0a
#define DLL_SCRSETICON          0x0b
#define DLL_FPUTC               0x0c
//#define DLL_PRINTF            0x0c
#define DLL_SCRDRLOGOXY         0x0d
#define DLL_SCRRESTORE          0x0e
#define DLL_SCRDRAWBOX          0x0f
#define DLL_LCDPRINTF           0x10
#define DLL_SETLIGHTTIME        0x20
#define DLL_SCRGOTOXY_EX		0x21
#define DLL_SCRGETXY_EX			0x22
#define DLL_SCRDRAWRECT			0x23
#define DLL_SCRCLRRECT			0x24
#define DLL_SCRSPACESET			0x25
#define DLL_SCRGETLCDSIZE		0x26
#define DLL_SCRTEXTOUT			0x27
#define DLL_SCRSETOUTPUT		0x28
#define DLL_SCRSETECHO			0x29
//for color lcd
#define DLL_CLCDSETFGCOLOR		0x2a
#define DLL_CLCDSETBGCOLOR		0x2b
#define DLL_CLCDDRAWPIXEL		0x2c
#define DLL_CLCDGETPIXEL		0x2d
#define DLL_CLCDCLRLINE			0x2e
#define DLL_CLCDTEXTOUT			0x2f
#define DLL_CLCDPRINT			0x30
#define DLL_CLCDDRAWRECT		0x31
#define DLL_CLCDCLRRECT			0x32
#define DLL_CLCDGETINFO			0x33
#define DLL_CLCDBGDRAWBOX		0x35

//Define IC_FUN function
#define DLL_ICCINIT             0x01
#define DLL_ICCCLOSE            0x02
#define DLL_ICCAUTORESP         0x03
#define DLL_ICCISOCOMMAND       0x04
#define DLL_ICCDETECT           0x05
#define DLL_MC_CLK_ENABLE       0x06
#define DLL_MC_IO_DIR           0x07
#define DLL_MC_IO_READ          0x08
#define DLL_MC_IO_WRITE         0x09
#define DLL_MC_VCC              0x0a
#define DLL_MC_RESET            0x0b
#define DLL_MC_CLK              0x0c
#define DLL_MC_C4_WRITE         0x0d
#define DLL_MC_C8_WRITE         0x0e
#define DLL_MC_C4_READ          0x0f
#define DLL_MC_C8_READ          0x10
#define DLL_READ_CARDSLOTINFO   0x11
#define DLL_WRITE_CARDSLOTINFO  0x12

//Define MAG_FUN function
#define DLL_MAGRESET            0x01
#define DLL_MAGOPEN             0x02
#define DLL_MAGCLOSE            0x03
#define DLL_MAGSWIPED           0x04
#define DLL_MAGREAD             0x05

//Define WNET_FUN function#define DLL_WNETINIT           0x01
#define DLL_WNETCHECKSIGNAL    0x02
#define DLL_WNETCHECKSIM       0x03
#define DLL_WNETSIMPIN         0x04
#define DLL_WNETUIDPWD         0x05
#define DLL_WNETDIAL           0x06
#define DLL_WNETCHECK          0x07
#define DLL_WNETCLOSE          0x08
#define DLL_WNETLINKCHECK      0x09
#define DLL_WNETTCPCONNECT     0x0a
#define DLL_WNETTCPCLOSE       0x0b
#define DLL_WNETTCPCHECK       0x0c
#define DLL_WNETTCPTXD         0x0d
#define DLL_WNETTCPRXD         0x0e
#define DLL_WNETRESET          0x0f
#define DLL_WNETSENDCMD        0x10
//  语音功能部分
#define DLL_WPHONECALL         0x30
#define DLL_WPHONEHANGUP       0x31
#define DLL_WPHONESTATUS       0x32
#define DLL_WPHONEANSWER       0x33
#define DLL_WPHONEMICGAIN      0x34
#define DLL_WPHONESPKGAIN      0x35
#define DLL_WPHONESENDDTMF     0x36

//Define MYFARE_FUN function
#define DLL_MYFARERESET        0x01
#define DLL_CLCREQUEST         0x02
#define DLL_CLCSELECT          0x03
#define DLL_M1AUTHORITY        0x04
#define DLL_M1READBLOCK        0x05
#define DLL_M1WRITEBLOCK       0x06
#define DLL_M1OPERATE          0x07
#define DLL_M1HALT             0x08

//Define PICC function
#define DLL_PICC_OPEN          0x01
#define DLL_PICC_SETUP         0x02
#define DLL_PICC_DETECT        0x03
#define DLL_M1AUTHORITY        0x04
#define DLL_M1READBLOCK        0x05
#define DLL_M1WRITEBLOCK       0x06
#define DLL_M1OPERATE          0x07
#define DLL_PICC_ISO_CMD       0x08
#define DLL_PICC_REMOVE        0x09
#define DLL_PICC_CLOSE         0x0A
#define	DLL_PICC_INFO		   0x0B
#define DLL_PICC_LED_CON		0x0C
#define	DLL_PICCCMDEXCHANGE		0x0D
#define	DLL_PICCINITFELICA		0x0E // liuxl 20091118
#define	DLL_PICCMANAGEREG      0x0F

//Define COMM_FUN function
#define DLL_MODEMDIAL           0x01
#define DLL_MODEMCHECK          0x02
#define DLL_MODEMTXD            0x03
#define DLL_MODEMRXD            0x04
#define DLL_ONHOOK              0x05
#define DLL_ASMODEMRXD          0x06
#define DLL_MODEMRESET          0x07
#define DLL_SMODEMINFO          0x41
#define DLL_MODEMEXCMD          0x50

//#define DLL_ISAINTHERE			0x49

#define DLL_PORTOPEN            0x08
#define DLL_PORTCLOSE           0x09
#define DLL_PORTSEND            0x0a
#define DLL_PORTRECV            0x0b
#define DLL_PORTRESET           0x0c
#define DLL_CHECKDSR            0x0d
#define DLL_SETDTR              0x0e
#define DLL_RS485ATTRSET        0x0f
#define DLL_ENABLESWITCH        0x10
#define DLL_SETOFFBASEPROC      0x11
#define DLL_PORTTXPOOLCHECK     0x12
#define DLL_PORTSENDS           0x13
#define DLL_PORTPEEP			0x14
#define DLL_PORTRECVS			0x15

#define DLL_HEARTBEAT			0x31

#define DLL_SPORTOPEN           0x20
#define DLL_SPORTCLOSE          0x21
#define DLL_SPORTSEND           0x22
#define DLL_SPORTRECV           0x23
#define DLL_SPORTRESET          0x24


//Define EXPORT_FUN function
#define DLL_EXPORTOPEN          0x01
#define DLL_EXPORTCLOSE         0x02
#define DLL_EXPORTSEND          0x03
#define DLL_EXPORTRECV          0x04

//Define PRINT_FUN function
#define DLL_PRNFONTSET          0x01
#define DLL_PRNSPACESET         0x02
#define DLL_PRNLEFTINDENT       0x03
#define DLL_PRNSTEP             0x04
#define DLL_PRNSTR              0x05
#define DLL_PRNLOGO             0x06
#define DLL_PRNINIT             0x07
#define DLL_PRNSTART            0x08
#define DLL_PRNSTATUS           0x09

#define DLL_PRNGETDOTLINE       0x0c
#define DLL_PRNSETGRAY          0x0d
#define DLL_PRNGETFONTDOT       0x0e
//wqh add 2006/05/16
#define DLL_PRNOPENPINTIME      0x10
#define DLL_PRNCLOSEPINTIME     0x11
#define DLL_PRNGETPINTIME       0x12
#define DLL_SPRNSTART           0x13
#define DLL_PRNTEMPERATURE      0x20

#define DLL_PRNDOUBLEWIDTH		0x21	
#define DLL_PRNDOUBLEHEIGHT		0x22
#define DLL_PRNATTRSET			0x23
#define DLL_PRNPREFEEDSET		0x24

//Define OTHER_FUN function
#define DLL_GETENV              0x01
#define DLL_PUTENV              0x02
#define DLL_DES                 0x03
#define DLL_RSARECOVER          0x04
#define DLL_HASH                0x05
#define DLL_READAPPINFO         0x06
#define DLL_WRITEAPPINFO        0x07
#define DLL_READAPPSTATE        0x08
#define DLL_SETAPPACTIVE        0x09
#define DLL_RUNAPP              0x0a
#define DLL_DOEVENT             0x0b
#define DLL_GETMATRIXDOT        0x0c
#define DLL_SSRAMTEST           0x0d
#define DLL_ERASESN             0x0e

#define DLL_CHECKMANAGE			0x0f

#define DLL_UARTECHO            0xe0
#define DLL_UARTECHOCH          0xe1
#define DLL_UARTPRINTF          0xe2
#define DLL_UARTECHOIN          0xe3



//Define FILE_FUN function
#define DLL_OPEN                0x01
#define DLL_READ                0x02
#define DLL_WRITE               0x03
#define DLL_CLOSE               0x04
#define DLL_SEEK                0x05
#define DLL_REMOVE              0x06
#define DLL_FILESIZE            0x07
#define DLL_FREESIZE            0x08
#define DLL_TRUNCATE            0x09
#define DLL_FEXIST              0x0a
#define DLL_GETFILEINFO         0x0b
#define DLL_EX_OPEN             0x0c

#define DLL_FILETOAPP			0x0d
#define DLL_FILETOPARAM			0x0e
#define DLL_FILETOFONT			0x0f
#define DLL_FILETOMONITOR		0x10
#define DLL_DELAPP				0x11
#define DLL_DELFONT				0x12
#define DLL_SSLSAVECERPRIVKEY	0x13
#define DLL_SSLDELCERPRIVKEY	0x14

#define DLL_FILETOPUK			0X16  
#define DLL_FSRECYCLE			0X17  
#define DLL_TELL                0x18
#define DLL_FILETOSO            0x19
#define DLL_FILETODRIVER		0x1a


//define IRDA_FUN function
#define DLL_IR_INTERFACE_OPEN                  0x01
#define DLL_IR_INTERFACE_CLOSE                 0x02
#define DLL_IR_INTERFACE_SEARCH_DEVICE         0x03
#define DLL_IR_INTERFACE_CONNECT_DEVICE        0x04
#define DLL_IR_INTERFACE_DISCONNECT_DEVICE     0x05
#define DLL_IR_INTERFACE_READ                  0x06
#define DLL_IR_INTERFACE_WRITE                 0x07
#define DLL_IR_INTERFACE_STATUS                0x08
#define DLL_IR_INTERFACE_SIZE                  0x09
#define DLL_IR_INTERFACE_BAUD                  0x0a
#define DLL_IR_INTERFACE_MESSAGE               0x0b
#define DLL_IR_INTERFACE_ADD_CLASS             0x0c
#define DLL_IR_INTERFACE_DEL_CLASS             0x0d
#define DLL_IR_INTERFACE_CIRCLE                0x0e
#define DLL_IR_INTERFACE_SET_HANDLE            0x0f


//Define PINPAD_FUN function
#define DLL_PPBEEP              0x01
#define DLL_PPLIGHT             0x02
#define DLL_PPINVOICE           0x03
#define DLL_PPKBMUTE            0x04
#define DLL_PPBACKLIGHT         0x05
#define DLL_PPINPUT             0x06
#define DLL_PPSCRPRINT          0x07
#define DLL_PPSCRCLS            0x08
#define DLL_PPSCRCLRLINE        0x09
#define DLL_PPSCRWRDATA         0x0a
#define DLL_PPGETPWD            0x0b
#define DLL_PPWRITEMKEY         0x0c
#define DLL_PPWRITEWKEY         0x0d
#define DLL_PPWRITEDKEY         0x0e
#define DLL_PPDERIVEKEY         0x0f
#define DLL_PPDES               0x10
#define DLL_PPMAC               0x11
#define DLL_PPCANCEL            0x12
#define DLL_PPVERINFO           0x13
#define DLL_PPUPDLOGO           0x14



#define DLL_EPSPPDISPMENU       0x20
#define DLL_EPSPPAMOUNT         0x21
#define DLL_EPSPPLOADTMK        0x22
#define DLL_EPSPPLOADKEY        0x23
#define DLL_EPSPPGETMAC1        0x24
#define DLL_EPSPPUPDATETPK      0x25
#define DLL_EPSPPSETMAC2        0x26
#define DLL_EPSPPGETPWD         0x27
#define DLL_EPSPPVERIFYTMK      0x28

#define DLL_SYLPPGETPWD         0x40
#define DLL_SYLPPVERIFYDERIVE   0x41
#define DLL_SYLPPCALCPINBLOCK   0x42



//Define PED_FUN function
#define DLL_PEDBEEP             0x01
#define DLL_PEDLIGHT            0x02
#define DLL_PEDINVOICE          0x03
#define DLL_PEDKBMUTE           0x04
#define DLL_PEDBACKLIGHT        0x05
#define DLL_PEDINPUT            0x06
#define DLL_PEDSCRPRINT         0x07
#define DLL_PEDSCRCLS           0x08
#define DLL_PEDSCRCLRLINE       0x09
#define DLL_PEDSCRWRDATA        0x0a
#define DLL_PEDGETPWD           0x0b
#define DLL_PEDWRITEMKEY        0x0c
#define DLL_PEDWRITEWKEY        0x0d
#define DLL_PEDWRITEDKEY        0x0e
#define DLL_PEDDERIVEKEY        0x0f
#define DLL_PEDDES              0x10
#define DLL_PEDMAC              0x11
#define DLL_PEDCANCEL           0x12
#define DLL_PEDVERINFO          0x13
#define DLL_PEDUPDLOGO          0x14
#define DLL_PEDINPUTTIMEOUT     0x15
#define DLL_PEDMANAGE           0x16
#define DLL_PEDGETPWD3DES       0x17
#define DLL_PEDINPUTNEWPINTIP   0x18
#define DLL_PEDCLEARKEY         0x19
#define DLL_WRITESRAM           0x1a
#define DLL_READSRAM            0x1b


#define DLL_EPSPEDDISPMENU      0x20
#define DLL_EPSPEDAMOUNT        0x21
#define DLL_EPSPEDLOADTMK       0x22
#define DLL_EPSPEDLOADKEY       0x23
#define DLL_EPSPEDGETMAC1       0x24
#define DLL_EPSPEDUPDATETPK     0x25
#define DLL_EPSPEDSETMAC2       0x26
#define DLL_EPSPEDGETPWD        0x27
#define DLL_EPSPEDVERIFYTMK     0x28
#define DLL_PEDPASSWORDENCRYPT  0x29
#define DLL_PEDPASSWORDINPUT    0x2a

#define DLL_SYLPEDGETPWD        0x40
#define DLL_SYLPEDVERIFYDERIVE  0x41
#define DLL_SYLPEDCALCPINBLOCK  0x42
#define DLL_SYLPEDLOADTMK       0x43

//#define DLL_PEDGETRANDOM         0x50
//#define DLL_PEDEXTERNALAUTH      0x51
//#define DLL_PEDINITAUTHKEY       0x52
//#define DLL_PEDPINENCRYPTX392    0x53
//#define DLL_PEDPINENCRYPTX98     0x54
//#define DLL_PEDUPDATEAUTHKEY     0x55
//#define DLL_PEDERASEPINKEY       0x56
//#define DLL_PEDWRITEPINKEY       0x57
//#define DLL_PEDSUPEREXTERNALAUTH 0x58
//#define DLL_PEDWRITEMASTERKEY    0x59
#define DLL_PCIGETRANDOM        0x50
#define DLL_PCIINITAUTHKEY      0x51
#define DLL_PCIEXTERNALAUTH     0x52
#define DLL_PCIUPDATEAUTHKEY    0x53
#define DLL_PCIWRITEMASTERKEY   0x54
#define DLL_PCIWRITEPINKEY      0x55
#define DLL_PCIWRITEMACKEY      0x56
#define DLL_PCIDERIVEKEY        0x57
#define DLL_PCIERASEKEY         0x58
#define DLL_PCIMAC              0x59
#define DLL_PCIPINENCRYPTX98    0x5a
#define DLL_PCIPINENCRYPTX392   0x5b
#define DLL_PCIOFFLINEENCPIN    0x5c
#define DLL_PCIOFFLINEPLAINPIN  0x5d
#define DLL_PCISUPERERASEKEY    0x5e

//Define PCI PED_FUN function
#define PCISETPEDTYPE                   0X00
#define PCIGETSTATUS                    0X01
#define PCIGETRANDOM                    0x02
#define PCIWRITEPINKEY                  0x03
#define PCIWRITEMACKEY                  0x04
#define PCIGETPIN                       0x05
#define PCIGETMAC                       0X06
#define PCIGETPINDUKPT                  0X07
#define PCIGETMACDUKPT                  0X08
#define PCIGETVERIFYPLAINPINOFFLINE     0X09
#define PCIGETVERIFYCIPHERPINOFFLINE    0X0A
#define PCIINQUIREKEYUSEDINFO           0X0B
#define PCICANCEL                       0X0C
#define PCISETBAUD                      0x0D
#define PCISETKBMUTE                    0X0E
#define PCIBEEP                         0X0F
#define PCISCRCLS                       0X10
#define PCISCRCLRLINE                   0X11
#define PCISCRPRINT                     0X12
#define PCISCRDRLOGO                    0x13
#define PCISETTIMEOUT				0x14

#define PCIWRITEMASTERKEY				0x15
#define PCIDERIVEPINKEY					0x16
#define PCIDERIVEMACKEY					0x17
#define PCIWRITEDUKPTINITKEY			0x18
#define PCIWRITEDESKEY		0X19
#define PCIDESKEYTDES		0x1A



//#define PCILOADAPPNAME					0x19
// #define PCILOADMASTERKEY				0x20

/* define SCANNER API */
#define DLL_SCAN_OPEN                 0x01
#define DLL_SCAN_CLOSE                0x02
#define DLL_SCAN_READ                 0x03


//define NET_COMM API
enum {
	DLL_NET_SOCKET = 0x01,
	DLL_NET_BIND,
	DLL_NET_CONNECT,
	DLL_NET_LISTEN,
	DLL_NET_ACCEPT,
	DLL_NET_SEND,
	DLL_NET_SENDTO,
	DLL_NET_RECV,
	DLL_NET_RECVFROM,
	DLL_NET_CLOSESOCKET,
	DLL_NET_IOCTL,
	DLL_SOCKADDR_SET,
	DLL_SOCKADDR_GET,
	DLL_DHCP_START,
	DLL_DHCP_STOP,
	DLL_DHCP_CHECK,
	DLL_PPP_LOGIN,
	DLL_PPP_LOGOUT,
	DLL_PPP_CHECK,
	DLL_ETH_MAC_SET,
	DLL_ETH_MAC_GET,
	DLL_ETH_SET,
	DLL_ETH_GET,
	DLL_DNS_RESOLVE,
	DLL_DNS_RESOLVE_IP,
	DLL_NET_PING,
	DLL_ROUTE_SET_DEFAULT,
	DLL_ROUTE_GET_DEFAULT,
	DLL_NET_DEV_GET,
	DLL_NET_ADD_STATICARP,
	DLL_NET_SETICMP,
	DLL_PPPOE_LOGIN,
	DLL_PPPOE_LOGOUT,
	DLL_PPPOE_CHECK,
	DLL_ETH_SET_RATE_DUPLEX_MODE,
	DLL_ETH_GET_FIRST_ROUTE_MAC,

	//for wifi
	DLL_WIFI_OPEN,
	DLL_WIFI_CLOSE,
	DLL_WIFI_SCAN,
	DLL_WIFI_CONNECT,
	DLL_WIFI_DISCON,
	DLL_WIFI_CHECK,
	DLL_WIFI_IOCTRL,
    DLL_WIFI_SCAN_EXT,
    
    DLL_DNS_RESOLVE_EXT,
    DLL_NET_SET_DHCP_DNS,
};

//define FAT API
enum {
	DLL_FAT_OPEN = 0x01,
	DLL_FAT_CLOSE,
	DLL_FAT_DELETE,
	DLL_FAT_GETINFO,
	DLL_FAT_RENAME,
	DLL_FAT_DIRSEEK,
	DLL_FAT_DIRREAD,
	DLL_FAT_FILEREAD,
	DLL_FAT_FILEWRITE,
	DLL_FAT_FILESEEK,
	DLL_FAT_TELL,
	DLL_FAT_TRUNCATE,
	DLL_FAT_SET_DIR,
	DLL_FAT_GET_DIR,
	DLL_FAT_UDISKIN,
	DLL_FAT_GETDISKINFO,
};

enum {
	DLL_EXWNET_INIT = 0x01,
	DLL_EXWNET_OPEN,
	DLL_EXWNET_CHECKSIGNAL,
	DLL_EXWNET_PPPDIAL,
	DLL_EXWNET_PPPCHECK,
	DLL_EXWNET_PPPCLOSE,
	DLL_EXWNET_TCPCONNECT,
	DLL_EXWNET_TCPCHECK,
	DLL_EXWNET_TCPCLOSE,
	DLL_EXWNET_TCPTXD,
	DLL_EXWNET_TCPRXD,
	DLL_EXWNET_CLOSE,
	DLL_WXNETINIT,
	DLL_WXNETGETSIGNAL,
	DLL_WXNETPPPLOGIN,
	DLL_WXNETPPPLOGOUT,
	DLL_WXNETPPPCHECK,
	DLL_WXPORTOPEN,
	DLL_WXPORTCLOSE,
	DLL_WXNETSENDCMD,
	DLL_WXNETPOWERSWITCH,	
	DLL_WXNETSELSIM,
	DLL_WXNETAUTOSTART,
	DLL_WXNETAUTOCHECK,
	DLL_WXNETPPPLOGIN_EX,
	DLL_WXTCPRETRYNUM,
	DLL_WXSETTCPDETECT,
	DLL_WXSETDNS,
};


enum{
	DLL_SSL_CERTREAD = 0x01,
	DLL_SSL_VERIFYCERT,
	DLL_SSL_COPYCERT,
	DLL_SSL_SIGN,
	DLL_SSL_VER,
	DLL_SSL_PKEYENCRYPT,
};

//TouchScreen
enum{
 	DLL_TOUCHSCREEN_OPEN=0x01,
	DLL_TOUCHSCREEN_READ,
	DLL_TOUCHSCREEN_CLOSE,
	DLL_TOUCHSCREEN_FLUSH,
	DLL_TOUCHSCREEN_ATTRSET,
	DLL_TOUCHSCREEN_CALIBRATION,
};
// Tilt Sensor Fun
enum {
    DLL_TILTSENSOR_GETLEANANGLE = 0,
}; 

//WAVE_FUN Fun
#define DLL_SOUNDPLAY           0x01

enum 
{
	DLL_BTOPEN=0x00,
	DLL_BTCLOSE,
	DLL_BTGETCONFIG,
	DLL_BTSETCONFIG,
	DLL_BTSCAN,
	DLL_BTCONNECT,
	DLL_BTDISCONNECT,
	DLL_BTGETSTATUS,
	DLL_BTIOCTRL,
	DLL_MODULEUPDATE=0xFF,
};

enum
{
	DLL_GPSOPEN = 0x00,
	DLL_GPSREAD,
	DLL_GPSCLOSE,
};

typedef struct{
    int    FuncType;
    int    FuncNo;
    int    int1;
    int    int2;
    void   *Addr1;
    void   *Addr2;
    long   long1;

    unsigned short  u_short1;
    unsigned short  u_short2;

    unsigned char   u_char1;
    unsigned char   u_char2;
    unsigned char   u_char3;
    unsigned char   u_char4;
    unsigned char   *u_str1;
    unsigned char   *u_str2;
    unsigned char   *u_str3;
    unsigned char   *u_str4;
    unsigned char   AppNum;
    char            char1;
    char            char2;
    unsigned long   u_long1;
    unsigned int    u_int1;
    short           short1;
    unsigned short  *up_short1;

}PARAM_STRUCT;


//Define process functions
void  FuncProc(void);
void  SysFun(void);
void  IcFun(void);
void  MagFun(void);
void  LcdFun(void);
void  KeyboardFun(void);
void  CommFun(void);
void  IrdaFun(void);
void  PrintFun(void);
void  OtherFun(void);
//void  PinpadFun(void);
void  PedFun(PARAM_STRUCT *glParam);
void  FileFun(void);
void  ExPortFun(void);
void  LoadDll(void);
void BtFun(void);

#endif





