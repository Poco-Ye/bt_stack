#ifndef _RPWIFI_H
#define _RPWIFI_H
#include <base.h>
#include <socket.h>
#include "../apis/wlanapp.h"

#define		STRING_LEN 64
#define		RP_WLAN_SEC_UNSEC (0)
#define		RP_WLAN_SEC_WPA_WPA2	(1)
#define		RP_WLAN_SEC_WPAPSK_WPA2PSK	(2)
#define		RP_WLAN_SEC_WEP	(3)

#define 	RP_RSSI_LEVEL3 (-65)
#define		RP_RSSI_LEVEL2 (-75)

#define     ERRNO_LEN       7 //"ERROR\r\n"
#define     MIN_RSP_LEN     4 //"OK\r\n"
#define     OK_LEN          2 //OK    
#define     ERR_LEN         5 //ERROR  
#define     END_LEN         2 //"\r\n"
#define     RP_AP_INFO_LEN      (34) // 32(ssid) + 1(sec) + 1(rssi)
#define     RP_BSSID_INFO_LEN   (38) // 32(ssid) + 6(bssid)
#define     RP_BSSTYPE_INFO_LEN (33) // 32(ssid) + 1(bsstype)
#define     MAX_AP_COUNT        40

enum
{
	WIFI_STATUS_CLOSE=0,
	WIFI_STATUS_NOTCONN,
	WIFI_STATUS_RESET,
	WIFI_STATUS_SLEEP,
	WIFI_STATUS_SCAN,
    WIFI_STATUS_SETAUTHMODE,
	WIFI_STATUS_CONNECTING,
	WIFI_STATUS_SETDNS,
	WIFI_STATUS_IPCONF,
	WIFI_STATUS_NWPARAMS,
	WIFI_STATUS_PWMODE,
	WIFI_STATUS_DISCON,
	WIFI_STATUS_CONNECTED,
};

typedef struct
{
	int iUsed;
	int id;
	long domaint;
    long type;
    long protocol;
	int role;
	uchar remoteIp[4];
	ushort remotePort;
	uchar localIp[4];
	ushort localPort;
	ushort backlog;//for listen socket
	uchar reserve[2];
}ST_SOCKET_INFO;

typedef struct
{
	uchar HostAddr[4];
	volatile int WState;
}ST_WIFI_STATUS;



int RedpineWifiOpen(void);
int RedpineWifiClose(void);
int RedpineWifiSendCmd(const uchar * ATstrIn,int ATstrLen, uchar *RspOut,ushort want_len,ushort TimeOut);
int RedpineWifiScan(ST_WIFI_AP *outAps, uint ApCount);
int RedpineWifiScanExt(ST_WIFI_AP_EX *pstWifiApsInfo, uint MaxApCnt);
int RedpineWifiConnect(ST_WIFI_AP *Ap,ST_WIFI_PARAM *WifiParam);
int RedpineWifiDisconnect(void);
int RedpineWifiCheck(ST_WIFI_AP * Ap);
int RedpineWifiCtrl(uint iCmd, void *pArgIn, uint iSizeIn,void *pArgOut, uint iSizeOut);


int s_RpWifiCheckBaudRate(void);
int s_RpWifiSoftReset(void);
void s_RpWifiGetStatus(ST_WIFI_STATUS *wifiStatus);
int sRedpineInit(void);
void s_RpGetLocalIp(uchar *ip);
void s_RpWifiPowerSwitch(int OnOff);
int s_RpWifiReadFromFifo(uchar *buf, int want_len);
void s_RpWifiWriteToFifo(uchar *buf, int len);
void s_RpWifiClearFifo(void);
int RpWifiUpdateOpen(void);
void s_RpWifiInSleep(void);
void s_RpWifiOutSleep(void);
int s_RpWifiInitForBack(void);
char * paxstrnstr(char *str1, char *str2, int str1len,int str2len);
unsigned int TryLock(unsigned int *lock);
void UnLock(unsigned int *lock);
void s_RedPineWifiDriverInit(void);
int RedPineWifiGetHVer(uchar * WifiFwVer);


#endif

