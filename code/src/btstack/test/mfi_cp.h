#ifndef _MFI_CP_H_
#define _MFI_CP_H_
#ifdef __cplusplus
extern "C" {
#endif

#define	MFIERR_BASE					0
#define MFIERR_INVALID_PARAM		MFIERR_BASE-1		//invalid parameter
#define	MFIERR_OPEN_COM_PORT		MFIERR_BASE-2		//open comport failed
#define	MFIERR_SEND_FAILED			MFIERR_BASE-3		//send data failed
#define	MFIERR_RECV_FAILED			MFIERR_BASE-4		//receive data failed
#define	MFIERR_SEND_OVERFLOW		MFIERR_BASE-5		//send fifo overflow
#define	MFIERR_RECV_TIMEOUT			MFIERR_BASE-6		//receive timeout
#define	MFIERR_FIFO_OVERFLOW		MFIERR_BASE-7		//receive fifo overflow
#define	MFIERR_CHECKSUM				MFIERR_BASE-8		//check sum error
#define	MFIERR_RES_FORMAT			MFIERR_BASE-9		//invalid response pakage format
#define	MFIERR_RES_ERROR			MFIERR_BASE-10		//ipad response error
#define	MFIERR_DEV_SUPPORT			MFIERR_BASE-11		//ipad not supported

#define	MFIERR_OPERATE_HW			MFIERR_BASE-12		//operate hardware failed
#define	MFIERR_UNDEF_STATUS			MFIERR_BASE-13		//undefined status

#define	MFIERR_IDPS					MFIERR_BASE-14		//IDPS failed
#define	MFIERR_ACC_AUTH				MFIERR_BASE-15		//Accessory authenticate failed

#define	MFIERR_INIT_FAILED			MFIERR_BASE-16		//intialize failed, must reboot the system
#define	MFIERR_DISCONNECT			MFIERR_BASE-17		//ipad disconnect
#define	MFIERR_BUSY					MFIERR_BASE-18		//the device is busy

void MfiCpInit(void);
void MfiCpReset(void);
//int MfiCpGetRegData(char addr, char *szData, int iLen);
int MfiCpGetRegData(unsigned char addr, unsigned char *szData, unsigned char iLen);
//int MfiCpSetRegData(char addr, char *szData, int iLen);
int MfiCpSetRegData(unsigned char addr, unsigned char *szData, unsigned char iLen);

void MfiCpTest(void);

#ifdef __cplusplus
}
#endif
#endif

