/*
修改历史：
20080605
1.ppp_md5_digest目前只能在s80上用，其他平台目前不能用
20080917 sunJH
去掉s80限制,避免其他平台不能运行
*/
#include "md5.h"

void ppp_md5_digest(unsigned char *digest, const unsigned char *data, unsigned int len)
{
	MD5_CTX ctx;
    MD5Init(&ctx);
    MD5Update(&ctx, (unsigned char *)data, len);
    MD5Final(digest, &ctx);
}
