#ifndef _PUK_H_
#define _PUK_H_

#include "..\encrypt\Rsa.h"

// 用户公钥号
enum {ID_MK_PUK=0,ID_US_PUK,ID_UA_PUK,ID_MF_PUK};

#define	SIGN_FILE_BOOT	0X01
#define	SIGN_FILE_MON	0X02
#define	SIGN_FILE_USPUK	0X11
#define	SIGN_FILE_UAPUK	0X12
#define	SIGN_FILE_APP	0X21

#define SIGN_INFO_FLAG "SIGN_FILE_INFO"
#define SIGN_INFO_LEN   284
/*sign format */
#define SIGNFORMAT0 0
#define SIGNFORMAT1 1 /*use new format,it include vailddate,SecLevel,owner*/
#define SHA1          0
#define SHA256        1
#define SHA384        2
#define SHA512        3

#define PUK_RET_OK                    0
#define PUK_RET_PARAM_ERR            -1
#define PUK_RET_NULL_ERR             -2
#define PUK_RET_SIG_ERR              -3
#define PUK_RET_WRITE_ERR            -4
#define PUK_RET_LEVEL_ERR            -5
#define PUK_RET_NO_SUPPORT_ERR       -6
#define PUK_RET_SIG_EXPIRATION_ERR   -7
#define PUK_RET_PUK1_EXPIRATION_ERR  -8
#define PUK_RET_PUK2_EXPIRATION_ERR  -9
#define PUK_RET_PUK3_EXPIRATION_ERR  -10
#define PUK_RET_SIG_TYPE_ERR         -11


typedef struct
{
    uchar ucHeader;   
	uchar ucDataFormat; 
	uchar ucFileType;  
	uchar aucDigestTime[3];
    uchar validdate[3];
    uchar SecLevel;
    uchar owner[16];
    uchar ShaType;
}tSignFileInfo;

enum PUK_INDEX{C1_PUKID=0,C2_PUKID,C3_PUKID,UA_PUKID,MAX_PUKID};
typedef struct {
    int valid;
    R_RSA_PUBLIC_KEY puk;
    tSignFileInfo   sig;
}tPukInfo;



// 函数定义
int s_WritePuk(uchar PukID, const uchar *pucKeyData, uint len);
int s_GetPuK(uchar ucKeyID, R_RSA_PUBLIC_KEY *ptPubKey,tSignFileInfo *siginfo);

#endif

