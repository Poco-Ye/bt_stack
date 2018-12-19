#include <string.h>
#include "Base.h"
#include "..\encrypt\Rsa.h"
#include "..\encrypt\HASH.h"
#include "..\encrypt\CRC.h"
#include "Puk.h"
#include "posapi.h"
#include "..\cfgmanage\cfgmanage.h"
#include "..\flash\nand.h"
#define PUKLEVEL_NAND_OFFSET (PUKLEVEL_START_NUM*BLOCK_SIZE)
#define PUK_LEVEL_FLAG                          "PUKLEVEL="

R_RSA_PUBLIC_KEY US_PubKey =
{
   1024,
  {

   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x9c,0xd1,0x26,0x43,0x7f,0xe7,0xe3,0x04,0x23,0xd2,0x26,0x94,0x68,0xa7,0xe7,0x30,
   0x2a,0x14,0xd0,0x5c,0x4f,0x46,0xfe,0xa0,0x2a,0x40,0xd6,0xfd,0x0c,0x35,0x11,0x3b,
   0xdc,0x48,0x31,0x29,0xc7,0xa1,0xd5,0x49,0x00,0xb1,0x12,0xfd,0x4f,0x14,0xaf,0xd1,
   0x5b,0x45,0x40,0xc3,0x97,0xeb,0x73,0xbe,0x7b,0xee,0x61,0x96,0x85,0x98,0xb4,0x7b,
   0x65,0x60,0xce,0xd5,0x64,0xb0,0xa2,0xc9,0xf3,0x6e,0x19,0x5e,0xd6,0xe4,0xdb,0x51,
   0xa0,0x7a,0xa7,0xb5,0x8e,0x5d,0xa9,0x69,0xbb,0x6e,0x21,0xfa,0x6f,0xc8,0xf9,0xa1,
   0xe1,0x45,0xed,0x65,0x9f,0x31,0xb0,0xb7,0x6c,0xe7,0xe9,0xc4,0x67,0x98,0x06,0xf9,
   0xb9,0x62,0x4a,0xe3,0xdd,0x4b,0x3b,0x5a,0xad,0x25,0x55,0x39,0xe0,0x5a,0xbc,0x53
  },
  {
    0x00,0x00,0x00,0x03
  }
};

R_RSA_PUBLIC_KEY MK_PubKey =
{
   2048,
  {

   0xCC,0xF1,0xBA,0xA7,0x88,0xA3,0xB8,0x56,0x36,0x08,0x39,0x46,0xE8,0x8B,0x8E,0x3E,
   0xFC,0x10,0xEE,0x9F,0x89,0x74,0x84,0xF4,0x79,0xC7,0x66,0x8C,0x77,0xEC,0xB8,0x3A,
   0x13,0x50,0xE5,0x83,0xA4,0x43,0xBE,0x80,0x5C,0xC5,0xB4,0x2F,0x12,0x93,0xCA,0xC8,
   0x8A,0x04,0x18,0x3D,0xB7,0x7D,0x65,0x02,0x59,0x2E,0xA5,0xC7,0x1F,0x80,0xAB,0x74,
   0xFB,0x55,0xB7,0xB2,0x5B,0x12,0xA3,0xB1,0x03,0xD4,0x88,0x9A,0xDA,0x5A,0x30,0x9D,
   0xB1,0xCB,0x92,0x55,0xC8,0x16,0x19,0x5E,0x79,0x20,0xCF,0x71,0x62,0xCD,0x26,0xAD,
   0x0E,0xCE,0x38,0xA2,0xD7,0x30,0x84,0xD4,0xDE,0x2E,0xC1,0x30,0x0C,0xE6,0x11,0x91,
   0x6E,0xA3,0xEC,0x26,0x62,0x5B,0x03,0x8A,0x74,0xA1,0x3C,0x06,0x07,0x1A,0x77,0x8F,
   0xC6,0xCD,0x82,0x73,0xEB,0x7E,0x07,0xB9,0xD3,0x08,0xDB,0x68,0x01,0x11,0x28,0xCF,
   0x7C,0x7B,0x3A,0x9D,0x3B,0x7C,0x80,0xA3,0x14,0x2F,0x49,0x6F,0x3A,0x24,0xBF,0xAF,
   0x68,0x87,0x8B,0xC4,0xC8,0x65,0xFB,0x06,0xF9,0x36,0x12,0x9D,0x20,0xA0,0x8C,0x30,
   0x57,0x76,0xF4,0xEA,0x22,0x0B,0x8F,0x36,0xF8,0x4B,0xA4,0x32,0x69,0x41,0x49,0xD9,
   0x64,0x60,0x29,0x36,0x33,0x39,0x6B,0xA8,0x51,0x8F,0x6D,0x1D,0x4E,0x35,0xE5,0xE0,
   0xE6,0x22,0x7A,0x50,0x86,0xA0,0x9C,0x5F,0x30,0x77,0xB4,0x84,0xDA,0xD5,0x2B,0x56,
   0xEC,0x4F,0x8D,0x84,0x52,0xBE,0x4D,0x80,0xCB,0xD4,0xF1,0x31,0xC3,0xE7,0x94,0x0A,
   0x61,0xD6,0xB7,0x9E,0xFD,0x61,0x96,0xC6,0x15,0x50,0x49,0xE8,0x9D,0x1C,0xF1,0x1D
  },
  {
    0x00,0x00,0x00,0x03
  }
};

R_RSA_PUBLIC_KEY MF_PubKey =
{
	2048, 
	{
	0xE7, 0xC5, 0xEA, 0xB8, 0xDA, 0xA5, 0xA8, 0x3B, 0x14, 0x3A, 0xC2, 0x07,
	0xA6, 0x15, 0x11, 0xF2, 0x59, 0xF6, 0x05, 0x88, 0x3D, 0x4D, 0xA1, 0x83, 0x05, 0x34, 0x61, 0xFB,
	0xB0, 0xBB, 0xF9, 0xEA, 0x4E, 0x7A, 0xFE, 0xEF, 0xC5, 0xA9, 0x16, 0x68, 0x13, 0xE0, 0xB4, 0x99,
	0xAD, 0x23, 0xC5, 0xA3, 0xBC, 0x26, 0x00, 0x9A, 0xED, 0x1A, 0xFD, 0x12, 0x6A, 0x80, 0xF5, 0xB7,
	0x95, 0xBF, 0x9F, 0x9F, 0x47, 0x01, 0x5F, 0x14, 0x2F, 0x26, 0xFB, 0x55, 0x7E, 0x98, 0xA6, 0x2C,
	0x5F, 0x32, 0xEE, 0x0D, 0x23, 0x2C, 0xC7, 0x5D, 0x0C, 0xAE, 0xEA, 0x82, 0x05, 0x94, 0x53, 0x4F,
	0xB6, 0x3D, 0xEE, 0x46, 0x35, 0x6F, 0xE3, 0x1F, 0x87, 0xA1, 0x09, 0x53, 0x0B, 0x98, 0x38, 0xBD,
	0xFF, 0x5D, 0xF1, 0x28, 0x39, 0xCF, 0xAE, 0x58, 0x59, 0x4F, 0x32, 0xB0, 0xCD, 0x92, 0x3F, 0xB0,
	0xE6, 0x43, 0xE1, 0xA3, 0x96, 0x3E, 0x52, 0xF8, 0xE4, 0xA5, 0x12, 0xE8, 0xD7, 0xCF, 0xC0, 0x18,
	0x9E, 0x87, 0x9D, 0xEC, 0x13, 0x57, 0xE8, 0x87, 0x38, 0x98, 0xC2, 0xA0, 0x61, 0x44, 0xE5, 0x67,
	0xDA, 0x00, 0xD4, 0x9E, 0x6A, 0xB5, 0x8B, 0xE9, 0xCE, 0x64, 0xD7, 0xED, 0x67, 0x6A, 0x00, 0x1F,
	0x7F, 0x78, 0x71, 0x31, 0xF3, 0x2F, 0x9F, 0xF5, 0xE5, 0x91, 0x88, 0xA3, 0xAD, 0x8D, 0xED, 0x79,
	0x5C, 0x35, 0xA0, 0xC8, 0x47, 0x3E, 0x56, 0x41, 0xEF, 0x11, 0x15, 0x53, 0xA7, 0x13, 0x35, 0x87,
	0xAB, 0xA3, 0x0B, 0x42, 0xDE, 0x83, 0xC9, 0xCF, 0xAC, 0x67, 0xC5, 0x68, 0x05, 0x54, 0x2A, 0x0F,
	0xEF, 0xAC, 0x80, 0x6B, 0xCF, 0xF3, 0xD7, 0x80, 0xFE, 0x72, 0x37, 0x0C, 0x11, 0xE3, 0xCE, 0x46,
	0xC0, 0x3B, 0x8A, 0x35, 0xF5, 0x4A, 0xA4, 0x70, 0x73, 0xA2, 0x2E, 0x1D, 0x4A, 0x96, 0xA8, 0x6A,
	0x12, 0x32, 0x73, 0x57
	},
	{
	0x00, 0x01, 0x00, 0x01
	}
};

static tPukInfo s_pukinfo[MAX_PUKID];
static uint s_CurPukLevel;

extern uint s_GetUsPukLevel();
extern int s_GetSecMode(void);
static int s_WritePukToCfg(uchar PukID, uint len, const uchar *pucKeyData);
static int s_GetPukFromCfg(int id,uchar *sig,uint *siglen,uchar *data);
static int s_VerifySig(uint type,uchar *data,uint datalen, 
                    R_RSA_PUBLIC_KEY *deppuk,uchar *sig,uchar *outdata);

static int s_SysLoadPuk()
{
    int i,ret;
    uint siglen;
    uchar sig[512],outdata[512],tmpbuf[USER_PUK_LEN],buff[16]; 
    for(i=0;i<MAX_PUKID;i++)
    {
        memset((void*)&s_pukinfo[i],0,sizeof(tPukInfo));
    }
    s_CurPukLevel = 0;

    /*load UA_PUK*/
    ret=s_GetPukFromCfg(INDEX_UAPUK,sig,&siglen,tmpbuf);
    if(ret== 0)
    { 
        ret = s_VerifySig(SIGN_FILE_UAPUK,tmpbuf,
                            sizeof(R_RSA_PUBLIC_KEY),&MK_PubKey,sig,outdata);
        if(ret==0) 
        {
            s_pukinfo[UA_PUKID].valid = 1;
            memcpy((uchar*)&(s_pukinfo[UA_PUKID].sig),outdata,sizeof(tSignFileInfo)); 
            s_pukinfo[UA_PUKID].puk.uiBitLen = tmpbuf[3]+(tmpbuf[2]<<8)
                                                +(tmpbuf[1]<<16)+(tmpbuf[0]<<24);
            memcpy(s_pukinfo[UA_PUKID].puk.aucModulus, tmpbuf+4, MAX_RSA_MODULUS_BITS/8+4);
        }
    }
    /*Load C1 PUK*/
    ret=s_GetPukFromCfg(INDEX_USPUK,sig,&siglen,tmpbuf);
    if(ret== 0)
    { 
        s_pukinfo[C1_PUKID].puk.uiBitLen=tmpbuf[3]+(tmpbuf[2]<<8)
                                        +(tmpbuf[1]<<16)+(tmpbuf[0]<<24);
        memcpy(s_pukinfo[C1_PUKID].puk.aucModulus, tmpbuf+4, MAX_RSA_MODULUS_BITS/8+4);        
        if(!memcmp((void*)&(s_pukinfo[C1_PUKID].puk), (void*)&US_PubKey, sizeof(R_RSA_PUBLIC_KEY)))
        {
            return PUK_RET_OK;
        }
        ret = s_VerifySig(SIGN_FILE_USPUK,tmpbuf,
                            sizeof(R_RSA_PUBLIC_KEY),&MK_PubKey,sig,outdata);
        if(ret == 0)
        { 
            memcpy((uchar*)&(s_pukinfo[C1_PUKID].sig),outdata,sizeof(tSignFileInfo)); 
        }
        else
        {
            if(!memcmp(sig+siglen-sizeof(tSignFileInfo)-strlen(SIGN_INFO_FLAG), SIGN_INFO_FLAG, strlen(SIGN_INFO_FLAG)))
            {
                memcpy((uchar*)&(s_pukinfo[C1_PUKID].sig),sig+siglen-sizeof(tSignFileInfo),sizeof(tSignFileInfo));            
            }
        }
        s_pukinfo[C1_PUKID].valid = 1;
        s_CurPukLevel = 1;        
    }
    else return PUK_RET_OK;
    /*Load C2 PUK*/
    ret=s_GetPukFromCfg(INDEX_USPUK1,sig,&siglen,tmpbuf);
    if(ret== 0)
    {
        ret = s_VerifySig(SIGN_FILE_USPUK,tmpbuf,sizeof(R_RSA_PUBLIC_KEY),
                           &(s_pukinfo[C1_PUKID].puk),sig,outdata);
        if(ret == 0)
        { 
            memcpy((void*)&(s_pukinfo[C2_PUKID].sig),outdata,sizeof(tSignFileInfo)); 
            s_pukinfo[C2_PUKID].puk.uiBitLen = tmpbuf[3]+(tmpbuf[2]<<8)
                                            + (tmpbuf[1]<<16) + (tmpbuf[0]<<24);
            memcpy(s_pukinfo[C2_PUKID].puk.aucModulus, tmpbuf+4, MAX_RSA_MODULUS_BITS/8+4);
            s_pukinfo[C2_PUKID].valid = 1;
            s_CurPukLevel = 2;
            
        }
        else return PUK_RET_OK;
    }
    else return PUK_RET_OK;
    /*Load C3 PUK*/
    ret=s_GetPukFromCfg(INDEX_USPUK2,sig,&siglen,tmpbuf);
    if(ret== 0)
    {
        ret = s_VerifySig(SIGN_FILE_USPUK,tmpbuf,sizeof(R_RSA_PUBLIC_KEY),
                           &(s_pukinfo[C2_PUKID].puk),sig,outdata);
        if(ret == 0)
        { 
            memcpy((void*)&(s_pukinfo[C3_PUKID].sig),outdata,sizeof(tSignFileInfo)); 
            s_pukinfo[C3_PUKID].puk.uiBitLen = tmpbuf[3] + (tmpbuf[2]<<8)
                                            + (tmpbuf[1]<<16) + (tmpbuf[0]<<24);
            memcpy(s_pukinfo[C3_PUKID].puk.aucModulus, tmpbuf+4, MAX_RSA_MODULUS_BITS/8+4);
            s_pukinfo[C3_PUKID].valid = 1;
            s_CurPukLevel = 3;
        }
        else return PUK_RET_OK;
    }
    else return PUK_RET_OK;  

    return PUK_RET_OK;
}
void s_LoadPuk()
{
    int ret, puk_level;
	ret = s_SysLoadPuk();
	if (!ret) 
	{
		if(s_GetBootSecLevel() < POS_SECLEVEL_L2) return;
		if(s_GetSecMode() == POS_SEC_L0) return;
        ret = ReadPukLevel(&puk_level);
        if((0==ret) && (puk_level==s_CurPukLevel)) return;        

        ScrCls();
        SCR_PRINT(0, 1, 0x01, "\n ÇëÖØÆô£¬²¢ÊÚÈ¨", "PUK INVALID,\npls reboot and authority.");
        getkey();
        Soft_Reset(); 
        while(1);
	}
	switch(ret)
	{
        case PUK_RET_PUK1_EXPIRATION_ERR:
            ScrPrint(0,3,0,"PUK1 is not within its validity period!");
            getkey();
        break;
        case PUK_RET_PUK2_EXPIRATION_ERR:
            ScrPrint(0,3,0,"PUK2 is not within its validity period!");
            getkey();
        break;
        case PUK_RET_PUK3_EXPIRATION_ERR:
            ScrPrint(0,3,0,"PUK3 is not within its validity period!");
            getkey();        
        break;	
	}
}

static int s_VerifySig(uint type,uchar *data,uint datalen, 
                    R_RSA_PUBLIC_KEY *deppuk,uchar *sig,uchar *outdata)
{
     uchar hash[64];
	 uint outlen,hashlen,ret,offset;
     tSignFileInfo SigInfo;

    ret= RSAPublicDecrypt(outdata,&outlen,sig,deppuk->uiBitLen/8,deppuk);
    if(ret) return PUK_RET_SIG_ERR;

    memcpy(&SigInfo,outdata,sizeof(tSignFileInfo));
    switch(type)
    {
        case SIGN_FILE_MON:
            sha256(data,datalen,hash);
            hashlen = 32;
            offset = deppuk->uiBitLen/8 - 1 - hashlen;
            break;
        case SIGN_FILE_UAPUK:
        case SIGN_FILE_USPUK:
        case SIGN_FILE_APP:
            if(SigInfo.ucFileType!=type) return PUK_RET_SIG_TYPE_ERR;
            if(SigInfo.ucHeader==SIGNFORMAT1 && SigInfo.ShaType==SHA256)
            {
                sha256(data,datalen,hash);
                hashlen = 32; 
                offset = deppuk->uiBitLen/8 - 1 - 32 - hashlen;
            }
            else
            {
                sha1(data,datalen,hash);
                hashlen = 20; 
                offset = deppuk->uiBitLen/8 - 1 - hashlen;
            }
            break;
    }
    
    if(memcmp(hash,outdata + offset,hashlen))return PUK_RET_SIG_ERR;

    return PUK_RET_OK;
}

/******************************
*      write puk to config area
*  0           Success
* <0          error
*******************************/
static int s_WritePukToCfg(uchar PukID, uint len, const uchar *pucKeyData)
{
    uchar tmpbuf[USER_PUK_LEN],en_data[USER_PUK_LEN],key[16];   
    int i = 0, Ret = 0;
    switch(PukID){
        case INDEX_UAPUK:
        case INDEX_USPUK:
        case INDEX_USPUK1:
        case INDEX_USPUK2:
            break;
        default:
            return PUK_RET_PARAM_ERR;
    }

    tmpbuf[3] = (uchar)len;
    tmpbuf[2] = (uchar)(len >> 8);
    tmpbuf[1] = (uchar)(len >> 16);
    tmpbuf[0] = (uchar)(len >> 24);
    for(i = 0; i < len; i++)
    {
    	tmpbuf[4 + i] = pucKeyData[i];
    }

	if(s_GetBootSecLevel() >= POS_SECLEVEL_L2)
	{
		s_GetBootAesKey(key);
		AES(tmpbuf, en_data, len+4, key, AES_ENCRYPT);
		memcpy(tmpbuf, en_data, len+4);
	}

	Ret=WriteTerminalInfo(PukID, tmpbuf, len+4);
	if (Ret) return PUK_RET_WRITE_ERR;/*write fial*/

    return PUK_RET_OK;
}

/****************************************************************
Puk Format

  DataLen     BitLen     mod     Exp     sig    siglen     sigindex    keyLen    Flag
|     4      |     4      | 256   |  4    |  256 |   4     |    4         |   4       |   16 |  2048bit puk size:548
  DataLen     BitLen     mod     Exp     sig    siglen     sigindex    keyLen    Flag
|     4      |     4      | 256   |  4    |  128 |   4     |    4         |   4       |   16 |  1024bit puk size:420 

Get puk from config area
* return:
*0         success
*-1	      no puk
*-2       invalid puk
******************************************************************/
static int s_GetPukFromCfg(int id,uchar *sig,uint *siglen,uchar *data)
{
    uint DataLen,SigLen;      
    int ret;
    uchar buff[USER_PUK_LEN+1],tmpbuf[USER_PUK_LEN+1],key[16]; 
    memset(buff, 0x00, sizeof(buff));
	ret=ReadTerminalInfo(id,buff,USER_PUK_LEN);
    if(ret <= 0) return -1; // can't find puk   	
	if(s_GetBootSecLevel() >= POS_SECLEVEL_L2)
	{
		s_GetBootAesKey(key);
		AES(buff, tmpbuf, ret, key, AES_DECRYPT);
		memcpy(buff, tmpbuf, ret);
	}

    DataLen =  buff[3] + (buff[2]<<8) + (buff[1]<<16) + (buff[0]<<24);   
    DataLen+=4; // add 4 bytes of len
  
    if(DataLen<256 || DataLen>USER_PUK_LEN) return -2;
    if(memcmp(buff+DataLen-16, SIGN_FLAG, 16)) return -2; /* invalid puk*/

    SigLen =  buff[DataLen-28] + (buff[DataLen-27]<<8)
              + (buff[DataLen-26]<<16) + (buff[DataLen-25]<<24);

    if((296+SigLen)>DataLen) return -2;/*296:4+264+28*/

    *siglen = SigLen;
    memcpy(data,buff+4, sizeof(R_RSA_PUBLIC_KEY));
    memcpy(sig,buff+268,SigLen);

    return 0;
}

uint GetCurPukLevel(void)
{
    return s_CurPukLevel;
}

int s_GetPuK(uchar ucKeyID, R_RSA_PUBLIC_KEY *ptPubKey,tSignFileInfo *siginfo)
{
    switch(ucKeyID)
    {
        case ID_UA_PUK:
            if(s_pukinfo[UA_PUKID].valid == 0) return PUK_RET_NULL_ERR;
            memcpy((void*)ptPubKey,(void*)&(s_pukinfo[UA_PUKID].puk),sizeof(R_RSA_PUBLIC_KEY));
            memcpy((void*)siginfo,(void*)&(s_pukinfo[UA_PUKID].sig),sizeof(tSignFileInfo));
            return PUK_RET_OK;
            break;
         case ID_US_PUK:
            if(s_CurPukLevel == 0)
            {
                memcpy((void*)ptPubKey,(void*)&US_PubKey,sizeof(R_RSA_PUBLIC_KEY));
                memset((void*)siginfo,0,sizeof(tSignFileInfo));
                siginfo->ucHeader = SIGNFORMAT1;
                strcpy(siginfo->owner, "PAX");
                memcpy(siginfo->aucDigestTime, "\x14\x08\x18", 3);
                memcpy(siginfo->validdate, "\x99\x12\x31", 3);
                siginfo->SecLevel = 1;
                return PUK_RET_OK;
            }
            if(s_pukinfo[s_CurPukLevel-1].valid == 0) return PUK_RET_NULL_ERR;            
            memcpy((void*)ptPubKey,(void*)&(s_pukinfo[s_CurPukLevel-1].puk),sizeof(R_RSA_PUBLIC_KEY));
            memcpy((void*)siginfo,(void*)&(s_pukinfo[s_CurPukLevel-1].sig),sizeof(tSignFileInfo));
            return PUK_RET_OK;
            break;

        default:
            return PUK_RET_PARAM_ERR;
    }

    return PUK_RET_OK;
}

int WritePukLevel(int level)
{
	char buf[16],en_buf[16], key[16];
	int block_num, i, j;
	
	memset(buf, 0x00, sizeof(buf));
	s_GetBootAesKey(key);
    strcpy(buf, PUK_LEVEL_FLAG);
    buf[strlen(PUK_LEVEL_FLAG)] = (level+0x30);
    AES(buf, en_buf, 16, key, AES_ENCRYPT);

    block_num = PUKLEVEL_END-PUKLEVEL_START_NUM;
	for(i=0;i<block_num;i++)
	{
		nand_phy_erase(PUKLEVEL_START_NUM + i);
	}

	for(i=0;i<block_num;i++)
	{
		for(j=0;j<(BLOCK_SIZE/PAGE_SIZE);j++)
		{
			s_puklevel_nand_write(PUKLEVEL_NAND_OFFSET + i*BLOCK_SIZE + j*PAGE_SIZE, en_buf, 16);
		}
	}

	return 0;
}

int ReadPukLevel(int *level)
{
	char buf[PAGE_SIZE],de_buf[16], key[16];
	int  block_num, i, j, ret;

	block_num = PUKLEVEL_END-PUKLEVEL_START_NUM;
	for(i=0;i<block_num;i++)
	{
		for(j=0;j<(BLOCK_SIZE/PAGE_SIZE);j++)
		{
			ret = s_puklevel_nand_read(PUKLEVEL_NAND_OFFSET + i*BLOCK_SIZE + j*PAGE_SIZE, buf, PAGE_SIZE);
			if(ret) continue;
			s_GetBootAesKey(key);
			AES(buf, de_buf, 16, key, AES_DECRYPT);
			if(!memcmp(de_buf, PUK_LEVEL_FLAG, strlen(PUK_LEVEL_FLAG)))
			{
				*level = atoi(de_buf+strlen(PUK_LEVEL_FLAG));
				return 0;
			}
		}
	}

	return 1;
}


/*
*   0       success
* <0       error
*/
int s_WritePuk(uchar PukID, const uchar *data, uint len)
{
    int ret,i,id;
    uchar sig[512],outdata[512],tmpbuf[USER_PUK_LEN];
    R_RSA_PUBLIC_KEY tmppuk, *pt;

    if (len<=264 || len>USER_PUK_LEN) return PUK_RET_PARAM_ERR;
    tmppuk.uiBitLen = data[3] + (data[2]<<8) + (data[1]<<16) + (data[0]<<24);
    memcpy(tmppuk.aucModulus, data+4, MAX_RSA_MODULUS_BITS/8);
    memcpy(tmppuk.aucExponent,data+4+MAX_RSA_MODULUS_BITS/8, 4);    
    switch(PukID)
    {
        case ID_UA_PUK:
            ret = s_VerifySig(SIGN_FILE_UAPUK,(uchar*)data,sizeof(R_RSA_PUBLIC_KEY),
                    &MK_PubKey,(uchar*)(data+sizeof(R_RSA_PUBLIC_KEY)),outdata);
            if (ret) return ret;
            ret = s_WritePukToCfg(INDEX_UAPUK, len, data);
            if (ret) return PUK_RET_WRITE_ERR;
        break;
        case ID_US_PUK:  
            memcpy(sig,data+264,len-264);
            if (s_CurPukLevel==0) pt = &MK_PubKey;
            else pt = &(s_pukinfo[s_CurPukLevel-1].puk);
            ret = s_VerifySig(SIGN_FILE_USPUK,(uchar*)data,sizeof(R_RSA_PUBLIC_KEY),
                               pt,sig,outdata);
            if(ret != 0)
            {
                if(s_CurPukLevel < 1 || s_CurPukLevel > 3) return ret;
                if(memcmp((void*)&tmppuk, (void*)&(s_pukinfo[s_CurPukLevel-1].puk), sizeof(R_RSA_PUBLIC_KEY))) return ret;
                if (s_CurPukLevel==1) pt = &MK_PubKey;
                else pt = &(s_pukinfo[s_CurPukLevel-2].puk);
                ret = s_VerifySig(SIGN_FILE_USPUK,(uchar*)data,sizeof(R_RSA_PUBLIC_KEY),
                		pt,sig,outdata);
                if (ret) return ret;
                if (!memcmp(outdata, (void*)&(s_pukinfo[s_CurPukLevel-1].sig), sizeof(tSignFileInfo)))
                {
                    return PUK_RET_OK;/*context and sig exactly the same*/
                }
                if(s_CurPukLevel==1) /*allow the old format  c1_puk(c1_pvk/mk_pvk) is covered by the new format c1_puk(mk_pvk)*/
                {
                    if(s_pukinfo[s_CurPukLevel-1].sig.ucHeader==SIGNFORMAT1 && outdata[0] != SIGNFORMAT1)
                    {
                    	return PUK_RET_SIG_ERR;
                    }
                	if(0==s_GetBootSecLevel())
                	{
                        i = len-28-sizeof(tSignFileInfo)-strlen(SIGN_INFO_FLAG);
                        memcpy((uchar*)(data+i), SIGN_INFO_FLAG, strlen(SIGN_INFO_FLAG));
                        i = len-28-sizeof(tSignFileInfo);
                        memcpy((uchar*)(data+i), outdata, sizeof(tSignFileInfo));
                	}
                	ret = s_WritePukToCfg(INDEX_USPUK, len, data);
                	if(ret) return ret;
                	memcpy((void*)&(s_pukinfo[0].sig),outdata,sizeof(tSignFileInfo));
                	memcpy((void*)&(s_pukinfo[0].puk),(void*)&tmppuk,sizeof(R_RSA_PUBLIC_KEY));
                	s_pukinfo[0].valid = 1;
                	return ret;
                }
                return PUK_RET_SIG_ERR;
            }    

            if(!memcmp((void*)&tmppuk, (void*)&US_PubKey, sizeof(R_RSA_PUBLIC_KEY))) return PUK_RET_OK;
            for(i=C1_PUKID;i<=C3_PUKID;i++)
            {
                if(!memcmp((void*)&tmppuk, (void*)&(s_pukinfo[i].puk), sizeof(R_RSA_PUBLIC_KEY)))                
                {                    
                    if((i+1)==s_CurPukLevel) return PUK_RET_OK;/*(Input_puk=Cn_puk(Cn_pvk), n=1,2,3)==Cur_puk*/
                    memset(tmpbuf,0,sizeof(tmpbuf));
                    if(i== C1_PUKID)/*Input_puk==c1_puk(cur_pvk),erase c2_puk,c3_puk*/ 
                    {
                        s_CurPukLevel = 1;
                        s_WritePukToCfg(INDEX_USPUK1,USER_PUK_LEN,tmpbuf);  /*erase c2_puk*/
                        memset((void*)&s_pukinfo[C2_PUKID],0,sizeof(tPukInfo));
                        
						if(s_GetBootSecLevel() >= POS_SECLEVEL_L2)
						{
							WritePukLevel(s_CurPukLevel);
						}
                    }

                    if(i == C2_PUKID)/*Input_puk==c2_puk(c3_pvk),erase c3_puk*/
                    {
                        s_CurPukLevel =2;
						if(s_GetBootSecLevel() >= POS_SECLEVEL_L2)
						{
							WritePukLevel(s_CurPukLevel);
						}                        
                    }    

                    s_WritePukToCfg(INDEX_USPUK2,USER_PUK_LEN,tmpbuf);  /*erase c2_puk*/
                    memset((void*)&s_pukinfo[C3_PUKID],0,sizeof(tPukInfo));

                    return PUK_RET_OK;
                }
            }

            if((s_GetUsPukLevel()-1) < s_CurPukLevel)
            {   
                if (0!=s_GetBootSecLevel()) return PUK_RET_LEVEL_ERR;
                /*for compatibility :old boot + new monitor*/
                s_CurPukLevel=0;            
            }

            switch(s_CurPukLevel){
                case 0:
                    id = INDEX_USPUK;
                    if (0==s_GetBootSecLevel())/*for compatibility :old boot + new monitor*/                        
                    {
                        i = len-28-sizeof(tSignFileInfo)-strlen(SIGN_INFO_FLAG);
                        memcpy((uchar*)(data+i), SIGN_INFO_FLAG, strlen(SIGN_INFO_FLAG));
                        i = len-28-sizeof(tSignFileInfo);
                        memcpy((uchar*)(data+i), outdata, sizeof(tSignFileInfo));
                    }
                    break;
                case 1: 
                    id = INDEX_USPUK1;
                    break;
                case 2:
                    id = INDEX_USPUK2;
                    break;
                default:
                    return PUK_RET_NO_SUPPORT_ERR;
            }
    
            ret = s_WritePukToCfg(id, len, data);
            if(ret) return ret;
            
            memcpy((void*)&(s_pukinfo[s_CurPukLevel].sig),outdata,sizeof(tSignFileInfo)); 
            memcpy((void*)&(s_pukinfo[s_CurPukLevel].puk),(void*)&tmppuk,sizeof(R_RSA_PUBLIC_KEY));
            s_pukinfo[s_CurPukLevel].valid = 1;
            s_CurPukLevel ++;
            if(s_GetBootSecLevel() >= POS_SECLEVEL_L2)
            {
                WritePukLevel(s_CurPukLevel);
            }
        break;
        default:
            return PUK_RET_PARAM_ERR;
    }

    return PUK_RET_OK;
}

int s_iVerifySignature(int iType,uchar * pucAddr,long lFileLen,tSignFileInfo *pstSigInfo)
{
    int ret, datalen, signlen;
    uchar buff[32], outdata[512];
    R_RSA_PUBLIC_KEY  PukKey;
    tSignFileInfo siginfo;

	if(pstSigInfo!=NULL) memset((uchar*)pstSigInfo,0x00,sizeof(tSignFileInfo));
    if(CheckIfDevelopPos())
    {
        if(0==s_GetBootSecLevel())/*for compatibility :old boot + new monitor*/
        {
            /*debug version && manufacturer's puk && not have sign flag, not need to verify*/
            if((0==s_CurPukLevel) && (0!=memcmp(pucAddr+lFileLen-16,SIGN_FLAG,16))) return 0;
        }
        else{
             /*debug setting && not tampered, not need to verify*/
            if(!s_GetTamperStatus()) return 0;
        }
    }   
	if(memcmp(pucAddr+lFileLen-16,SIGN_FLAG,16)!=0) return PUK_RET_SIG_ERR;
	
	memcpy(buff,pucAddr+lFileLen-16-12,12);	
	signlen = buff[0] + (buff[1]<<8) + (buff[2]<<16) + (buff[3]<<24);	
	datalen = buff[8] + (buff[9]<<8) + (buff[10]<<16) + (buff[11]<<24);	
			
	if((datalen<=0)||(signlen<=0)) return PUK_RET_SIG_ERR;

    switch(iType)
    {
        case SIGN_FILE_APP:
            if(!s_GetTamperStatus() || s_GetBootSecLevel()==0)
            {  
                if (POS_SEC_L0==s_GetSecMode() && s_GetBootSecLevel()>0) 
                    goto USE_FIRM_VERIFY;
                /*use application mode verify app*/
                ret = s_GetPuK(ID_US_PUK, &PukKey, &siginfo);
                if (ret < 0) goto USE_FIRM_VERIFY;
                ret = s_VerifySig(SIGN_FILE_APP,pucAddr,datalen,&PukKey, pucAddr+lFileLen-16-12-signlen,outdata);
                if (0==ret) break;
            }
USE_FIRM_VERIFY:            
            /*use firmware mode verify app*/
            ret = s_VerifySig(SIGN_FILE_MON,pucAddr,datalen,&MF_PubKey, 
                            pucAddr+lFileLen-16-12-signlen,outdata);
            if (ret != 0) return ret;                            
        break;
        case SIGN_FILE_UAPUK:
        case SIGN_FILE_USPUK:
            return 0;
        break;
        case SIGN_FILE_MON:
            ret = s_VerifySig(SIGN_FILE_MON,pucAddr,datalen,&MF_PubKey, 
                            pucAddr+lFileLen-16-12-signlen,outdata);
            if (ret != 0) return ret;                            
        break;
    }

	if(pstSigInfo != NULL){
        memcpy(pstSigInfo,outdata,sizeof(tSignFileInfo));
	}
	return 0;
}

#include "../file/filedef.h"
typedef struct{
int isValid;
tSignFileInfo siginfo;
}tAppSigInfo;
static tAppSigInfo AppSigInfo[APP_MAX_NUM];

int s_SelfVerify(void)
{
    FILE_INFO finfo[256];
    int i, sum, ret, fd, iType, len;
    uchar *ptrAppData=(uchar *)SAPP_RUN_ADDR;
    uchar AppName[17], attr[2];
    tSignFileInfo signinfo;

    memset(AppSigInfo, 0x00, sizeof(AppSigInfo));
    sum=GetFileInfo(finfo);

    for (i = 0; i < APP_MAX_NUM; i++)
    {
        sprintf(AppName,"%s%d",APP_NAME,i);
        attr[0]=0xff;
        attr[1]=0x01;        
        if(i==0) attr[1]=0x00;
        fd = s_open(AppName,O_RDWR,attr);
        if(fd < 0) continue;
        len = filesize(AppName);
        if (len < 0) continue;
        read(fd,ptrAppData,len);
        close(fd);
        if(memcmp((uchar *)(ptrAppData+len-strlen(DATA_SAVE_FLAG)), DATA_SAVE_FLAG, strlen(DATA_SAVE_FLAG))) continue;    
        ret=s_iVerifySignature(SIGN_FILE_APP,(uchar *)ptrAppData,len-16,&signinfo);            
        if(ret == 0)
        {
            AppSigInfo[i].isValid = 1;
            memcpy((uchar*)&(AppSigInfo[i].siginfo), (uchar*)&signinfo, sizeof(tSignFileInfo));
        }
    }

    for(i=0; i<sum; i++)
    {
        if((finfo[i].type == FILE_TYPE_SYSTEMSO) || (finfo[i].type == FILE_TYPE_USERSO))  //include pubso and privso
        {
            if(finfo[i].type == FILE_TYPE_SYSTEMSO) iType=SIGN_FILE_MON;    //system so
            else if(finfo[i].type == FILE_TYPE_USERSO) iType=SIGN_FILE_APP; //user so
            
            fd = s_open(finfo[i].name,O_RDWR,&finfo[i].attr);
            if(fd < 0)        
            {
                ScrCls();
                ScrPrint(0, 2, 0x01, "NAME:%s", finfo[i].name);
                ScrPrint(0, 0, 0x01, "OPEN SO ERROR");
                while(1);
            }

            read(fd,ptrAppData, finfo[i].length);
            ret = s_iVerifySignature(iType, (uchar *)ptrAppData, finfo[i].length, NULL);	
            if(ret != 0)
            {
                ScrCls();
                ScrPrint(0, 2, 0x01, "NAME:%s", finfo[i].name);
                ScrPrint(0, 0, 0x01, "SO SIGN ERROR");
                if (finfo[i].type == FILE_TYPE_SYSTEMSO) while(1);
                return -1;
            }   
            close(fd);
        }
        
        //mpatch
        if(finfo[i].attr==FILE_ATTR_PUBSO && finfo[i].type==FILE_TYPE_SYSTEMSO &&
            strstr(finfo[i].name,MPATCH_EXTNAME))
        {
            if(GetMpatchSysPara(finfo[i].name) >= MPATCH_MAX_PARA)
            {
                ScrCls();
                ScrPrint(0,0,1,"MPATCH ERROR!");
                while(1);
            }
        }       
    }
    return 0;
}

int s_SelfVerifyApp(void)
{
    int i, sum, ret, fd, iType, len;
    uchar *ptrAppData=(uchar *)SAPP_RUN_ADDR;
    uchar AppName[17], attr[2];
    tSignFileInfo signinfo;

    memset(AppSigInfo, 0x00, sizeof(AppSigInfo));
 
    for (i = 0; i < APP_MAX_NUM; i++)
    {
        sprintf(AppName,"%s%d",APP_NAME,i);
        attr[0]=0xff;
        attr[1]=0x01;        
        if(i==0) attr[1]=0x00;
        fd = s_open(AppName,O_RDWR,attr);
        if(fd < 0) continue;
        len = filesize(AppName);
        if (len < 0) continue;
        read(fd,ptrAppData,len);
        close(fd);
        if(memcmp((uchar *)(ptrAppData+len-strlen(DATA_SAVE_FLAG)), DATA_SAVE_FLAG, strlen(DATA_SAVE_FLAG))) continue;    
        ret=s_iVerifySignature(SIGN_FILE_APP,(uchar *)ptrAppData,len-16,&signinfo);            
        if(ret == 0)
        {
            AppSigInfo[i].isValid = 1;
            memcpy((uchar*)&(AppSigInfo[i].siginfo), (uchar*)&signinfo, sizeof(tSignFileInfo));
        }
    }
         
    return 0;
}

int GetAppSigInfo(int AppNo, tSignFileInfo *pt)
{
    if (!AppSigInfo[AppNo].isValid) return -1;
    if (pt) pt[0] = AppSigInfo[AppNo].siginfo;
    return 0;
}

void SetAppSigInfo(int AppNo, tSignFileInfo info)
{
    AppSigInfo[AppNo].siginfo = info;
    AppSigInfo[AppNo].isValid = 1;
}

