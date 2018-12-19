#ifndef SM_SOFT__H
#define SM_SOFT__H
extern int SM2GenKeyPair_sw(ST_SM2_KEY  *key );
extern int SM2Sign_sw (ST_SM2_INFO *info, unsigned char *data, unsigned int datalen,
		unsigned char * sign , unsigned int *signlen);
extern int SM2Verify_sw(ST_SM2_INFO * info,unsigned char *data,unsigned int datalen,
		unsigned char *sign, unsigned int signlen);
extern int SM2Recover_sw(ST_SM2_KEY *key ,unsigned char *datain,unsigned int datainlen, 
		unsigned char *dataout, unsigned int *dataoutlen, unsigned int mode );
extern int SM3_sw(unsigned char *key, unsigned int keylen ,unsigned char *datain,
		unsigned int datainlen,unsigned char *dataout );
extern int SM4_sw (unsigned char *datain,unsigned int datainlen,unsigned char iv[16],
		unsigned char key[16],unsigned char *dataout,unsigned int mode );

extern unsigned char *GetSmLibVer(void);
//-----------------------------------------------------------------------------
#endif

