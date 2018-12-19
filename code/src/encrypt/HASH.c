#include "base.h"
#include <string.h>
#include "hash.h"
void sha1_starts( STRUCT_SHA *ctx );
void sha1_update( STRUCT_SHA *ctx, uchar *input, uint length );
void sha1_finish( STRUCT_SHA *ctx, uchar digest[20] );

void Sha1Reset(STRUCT_SHA *pSha)
{
	sha1_starts(pSha);
}

// 输入数据长度不能大于64字节
void SHA_Update(STRUCT_SHA *pSha, uchar *pData, uint Length)
{
	sha1_update(pSha,pData,Length);
}
// 获取运算结果
void SHA_Final(STRUCT_SHA *pSha, uchar *pOutput, uint Length)
{
	sha1_finish(pSha,pOutput);
}

void SHA_Init(void)
{
}

void SHA_Close(void)
{
}

void Hash(uchar* DataIn,uint DataInLen,uchar* DataOut)
{
	sha1(DataIn,DataInLen,DataOut);
}

