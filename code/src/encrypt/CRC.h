#ifndef _CRC_H_
#define _CRC_H_

unsigned short Cal_CRC16_PRE(unsigned char *pucInBuff, unsigned int uiLen,unsigned short usPreCrc);

#define Cal_CRC16(_A_,_B_)	Cal_CRC16_PRE(_A_,_B_,0x0000)

#endif

