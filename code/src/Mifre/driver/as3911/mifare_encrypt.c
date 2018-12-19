#include "mifare_encrypt.h"
#include <string.h>
#include <stdlib.h>


#define SWAPENDIAN(x) (x = (x >> 8 & 0xff00ff) | (x & 0xff00ff) << 8, x = x >> 16 | x << 16)
#define RET_INVALID_PARAM   (0x01)

struct Crypto1State gtPcs;

static const unsigned char OddParity[256] = 
{
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};


unsigned char ODD_PARITY(const unsigned char bt)
{
	return OddParity[bt];
}


void NumToBytes(unsigned long long int n, unsigned int len, unsigned char* dest) 
{
	while (len--) 
	{
        dest[len] = (unsigned char) n;
        n >>= 8;
    }
}

unsigned long long int BytesToNum(unsigned char* src, unsigned int len) 
{
    unsigned long long int num = 0;
    while (len--)
    {
        num = (num << 8) | (*src);
		src++;
    }
    return num;
}

unsigned short UpdateCrc(unsigned char ch, unsigned short *lpwCrc)
{
    ch = (ch^(unsigned char)((*lpwCrc) & 0x00FF));
    ch = (ch^(ch<<4));
    *lpwCrc = (*lpwCrc >> 8)^((unsigned short)ch << 8)^((unsigned short)ch<<3)
^((unsigned short)ch>>4);
    return(*lpwCrc);
}


unsigned char ComputeCrc(char *Data, int Length, unsigned char *TransmitFirst, unsigned char *TransmitSecond)
{
    unsigned char chBlock;
    unsigned short wCrc;
	
    /*add by wanls 2014.06.24*/
	if(Length <= 0)return RET_INVALID_PARAM;
	/*add end */

    wCrc = 0x6363;

    do 
	{
		chBlock = *Data++;
		UpdateCrc(chBlock, &wCrc);
	} while (--Length);
    
    *TransmitFirst = (unsigned char) (wCrc & 0xFF);
    *TransmitSecond = (unsigned char) ((wCrc >> 8) & 0xFF);

    return 0;
}

unsigned int SwapEndian32(unsigned char *pt)
{
	int i = 0;
	unsigned int idata = 0;
	for(i = 0; i < 4; i++)
	{
		idata |= (pt[i] << (i * 8));
	}
	return idata;
}


void AddOddParity(unsigned char *data, int InByteLength, int* OutByteLength, int* OutBitLength)
{
    unsigned char temp[256];
	unsigned char ucOddBitLength = 0;
	int k = 0;
	int i = 0;
	int iDataLenght = 0;
	int iAllBitLength = 0;

	
	memcpy(temp, data, InByteLength);
	memset(data, 0x00, ((InByteLength+7)/8 + InByteLength));

	
	for(i = 0; i < InByteLength; i++)
	{
	    ucOddBitLength = 0;
		
	    for(k = 0; k < 8; k++)
	    {
	        if(temp[i] & (0x1 << k))
	        {
	            ucOddBitLength++;
				data[iAllBitLength / 8] |= (0x01 << (iAllBitLength % 8));
	        }
			iAllBitLength++;
	    }
		
		if(!(ucOddBitLength & 0x01))
		{
		    data[iAllBitLength / 8] |= (0x01 << (iAllBitLength % 8));
		}
		iAllBitLength++;
	}

	*OutByteLength = (iAllBitLength + 7) / 8;
	*OutBitLength = iAllBitLength % 8;
}


void AddParity(unsigned char *data, int InByteLength, unsigned char* Parity, int* OutByteLength, int* OutBitLength)
{
    unsigned char temp[256];
	unsigned char ucOddBitLength = 0;
	int k = 0;
	int i = 0;
	int iDataLenght = 0;
	int iAllBitLength = 0;

	
	memcpy(temp, data, InByteLength);
	memset(data, 0x00, ((InByteLength+7)/8 + InByteLength));

	for(i = 0; i < InByteLength; i++)
	{
	    ucOddBitLength = 0;
		
	    for(k = 0; k < 8; k++)
	    {
	        if(temp[i] & (0x1 << k))
	        {
	            ucOddBitLength++;
				data[iAllBitLength / 8] |= (0x01 << (iAllBitLength % 8));
	        }
			iAllBitLength++;
	    }

        /*EVEN*/
		if (OddParity[temp[i]] != Parity[i])
		{
		    if(ucOddBitLength & 0x01)
		    {
		        data[iAllBitLength / 8] |= (0x01 << (iAllBitLength % 8));
		    }
		}
		/*ODD*/
		else
		{
		    if(!(ucOddBitLength & 0x01))
		    {
		        data[iAllBitLength / 8] |= (0x01 << (iAllBitLength % 8));
		    }
		}
		iAllBitLength++;
	}
	*OutByteLength = (iAllBitLength + 7) / 8;
	*OutBitLength = iAllBitLength % 8;
}


void RemoveParity(unsigned char *data, int InByteLength, int InBitStart, int* OutByteLength)
{
    unsigned char temp[256];
	int k = 0;
	int i = 0;
	int iAllBitLength = 0;
	int iUseBitLength = 0;
	int BitStart = InBitStart;
	
	memcpy(temp, data, InByteLength);
	memset(data, 0x00, InByteLength);
	
	for(i = 0; i < InByteLength; i++)
	{
	 for(k = BitStart; k < 8; k++)
	 {
		 iAllBitLength++;
	     if(iAllBitLength % 9)
	     {
			 if(temp[i] & (0x1 << k)) 
			 {
				 data[iUseBitLength / 8] |= (0x01 << (iUseBitLength % 8));
			 } 
			 iUseBitLength++;
		 }  
		 
	 }
	 BitStart = 0;
	}
	*OutByteLength = iUseBitLength / 8;
	/*deal with one byte length */
	if((iUseBitLength > 0) && (iUseBitLength < 8))
	{
	    *OutByteLength = 1;
	}
}


struct Crypto1State * crypto1_create(unsigned long long int key)
{
    memset(&gtPcs, 0x00, sizeof(struct Crypto1State));
	
	struct Crypto1State *s = &gtPcs;
	int i;

	for(i = 47;s && i > 0; i -= 2) 
	{
		s->odd  = s->odd  << 1 | BIT(key, (i - 1) ^ 7);
		s->even = s->even << 1 | BIT(key, i ^ 7);
	}
	return s;
}

unsigned char crypto1_bit(struct Crypto1State *s, unsigned char in, int is_encrypted)
{
	unsigned int feedin;
	unsigned char ret = filter(s->odd);

	feedin  = ret & !!is_encrypted;
	feedin ^= !!in;
	feedin ^= LF_POLY_ODD & s->odd;
	feedin ^= LF_POLY_EVEN & s->even;
	s->even = s->even << 1 | parity(feedin);

	s->odd ^= (s->odd ^= s->even, s->even ^= s->odd);

	return ret;
}

unsigned char crypto1_byte(struct Crypto1State *s, unsigned char in, int is_encrypted)
{
	unsigned char i, ret = 0;

	for (i = 0; i < 8; ++i)
		ret |= crypto1_bit(s, BIT(in, i), is_encrypted) << i;

	return ret;
}

unsigned int crypto1_word(struct Crypto1State *s, unsigned int in, int is_encrypted)
{
	unsigned int i, ret = 0;

	for (i = 0; i < 4; ++i, in <<= 8)
	{
		ret = ret << 8 | crypto1_byte(s, in >> 24, is_encrypted);
	}

	return ret;
}

/* prng_successor
 * helper used to obscure the keystream during authentication
 */
unsigned int prng_successor(unsigned int x, unsigned int n)
{
	SWAPENDIAN(x);
	while(n--)
		x = x >> 1 | (x >> 16 ^ x >> 18 ^ x >> 19 ^ x >> 21) << 31;

	return SWAPENDIAN(x);
}

int filter(unsigned int const x)
{
	unsigned int f;

	f  = 0xf22c0 >> (x       & 0xf) & 16;
	f |= 0x6c9c0 >> (x >>  4 & 0xf) &  8;
	f |= 0x3c8b0 >> (x >>  8 & 0xf) &  4;
	f |= 0x1e458 >> (x >> 12 & 0xf) &  2;
	f |= 0x0d938 >> (x >> 16 & 0xf) &  1;
	return BIT(0xEC57E80A, f);
}

int parity(unsigned int x)
{

	x ^= x >> 16;
	x ^= x >> 8;
	x ^= x >> 4;
	return BIT(0x6996, x & 0xf);
}

