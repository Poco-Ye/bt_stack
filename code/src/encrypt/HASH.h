#ifndef _HASH_H_
#define _HASH_H_

#define SHA1_HASH_SIZE_ucharS    20
#define SHA1_BLOCK_SIZE_ucharS   64
#define SHA_MAX_MSG_LEN			0x10000000

typedef struct
{
    uint state[5];
    uint total[2];
    uchar buffer[64];
} STRUCT_SHA;

void SHA_Init(void);
void SHA_Close(void);
void Sha1Reset(STRUCT_SHA *pSha);
void SHA_Update(STRUCT_SHA *pSha, uchar *pData, uint Length);
void SHA_Final(STRUCT_SHA *pSha, uchar *pOutput, uint Length);
void Hash(uchar* DataIn,uint DataInLen,uchar* DataOut);

void Test_HASH(void);

#endif


