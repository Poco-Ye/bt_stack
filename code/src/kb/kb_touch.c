#include "kb_touch.h"
#include "kbi2c.h"
#include <string.h>
#include "base.h"

int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

int s_CheckStringIsHex(unsigned char *string)
{
	int stringlen=0;
	int i;
	if ((strlen(string)%2) != 0)
	{
		return -1;
	}
	stringlen = strlen(string);
	for (i = 0;i < stringlen;i++)
	{
		if (string[i] < '0')
		{
			break;
		}
		else if (string[i] > '9' && string[i] < 'A')
		{
			break;
		}
		else if (string[i] > 'F' && string[i] < 'a')
		{
			break;
		}
		else if (string[i] > 'f')
		{
			break;
		}
	}
	if (i < stringlen)
	{
		return -1;
	}
	else 
	{
		return 0;
	}
}

int s_string2hex(unsigned char *string, unsigned char *result)
{
	int hexlen=0,stringlen=0;
	int i;
	int iRet;
	iRet = s_CheckStringIsHex(string);
	if (iRet < 0)
		return -1;
	stringlen = strlen(string);
	hexlen = stringlen / 2;
	for (i = 0;i < stringlen;i++)
	{
		if ((string[i] >= 'a') && (string[i] <= 'f'))
		{
			string[i] = string[i] - 'a' + 'A';
		}
	}
	for (i = 0;i < hexlen;i++)
	{
		if ((string[2*i] >= 'A') && (string[2*i] <= 'F'))
		{
			result[i] = (string[2*i]-'A'+10)*16;
		}
		else
		{
			result[i] = (string[2*i]-'0')*16;
		}
		if ((string[2*i+1] >= 'A') && (string[2*i+1] <= 'F'))
		{
			result[i] += string[2*i+1]-'A'+10;
		}
		else
		{
			result[i] += string[2*i+1]-'0';
		}
	}
	return hexlen;
}
int ReadTouchKeyParam(unsigned char *Param/*Out*/)
{
	unsigned char buf[256];
	unsigned char temp[30];
	int iRet;
	
	memset(temp, 0, sizeof(temp));
	memset(buf,0,sizeof(buf));
	if(ReadCfgInfo("TOUCH_KEY_PARA_1",buf) > 0)
	{
		if (strlen(buf) == 20)
		{
			if(ReadCfgInfo("TOUCH_KEY_PARA_2",buf+20) > 0)
			{
				if (strlen(buf) == 40)
				{
					ReadCfgInfo("TOUCH_KEY_PARA_3",buf+40);
				}
			}
		}
		iRet = s_string2hex(buf, temp);
		if (iRet > 0)
		{
			memcpy(Param, temp, iRet);
		}
		return iRet;
	}
	else
	{
		return -1;
	}
}


const unsigned char gucTouchKbBaseLineParam[15]
	={140,180,180,200,180,180,180,180,220,200,160,180,210,210,230};

void s_TouchKeySetBaseLine(void)
{
	unsigned char pBuf[32];
	unsigned char pBufRead[32];
	unsigned char Param[30];
	int i=0,j;
	int iRet;
	unsigned char version;
	int KeyNum;
	KeyNum = sizeof(gucTouchKbBaseLineParam);
	memset(Param, 0, sizeof(Param));
	iRet = ReadTouchKeyParam(Param);
	if (iRet>0)
	{
		version = s_TouchKeyVersion();
		if (version != Param[0])
		{
			//配置文件内的参数与触摸芯片版本不匹配，使用默认参数
			Param[0] = 0x04;
			memcpy(Param+1, gucTouchKbBaseLineParam, KeyNum);
		}
	}
	else
	{
		Param[0] = 0x04;
		memcpy(Param+1, gucTouchKbBaseLineParam, KeyNum);
	}
		
	for (i = 0;i < 3;i++)
	{
		memset(pBuf,0,sizeof(pBuf));
		memset(pBufRead,0,sizeof(pBufRead));
		pBuf[0] = KB_I2C_ADDR_BASELINE;
		memcpy(pBuf+1,Param+1,KeyNum);
		kb_i2c_write_str(KB_I2C_SLAVER_WRITE, pBuf, 1+KeyNum);
		for (j = 0;j < KeyNum;j++)
		{
			pBuf[0] = KB_I2C_ADDR_BASELINE+j;
			kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,1);
			kb_i2c_read_str(KB_I2C_SLAVER_READ, pBufRead+j, 1);
		#ifdef DEBUG_TOUCHKEY
			iDPrintf("pBufRead[%d]=%d\r\n",j,pBufRead[j]);
		#endif
		}
		if (memcmp(Param+1, pBufRead,KeyNum) == 0)
		{
			break;
		}
	}
}

void s_TouchKeyStart(void)
{
	unsigned char pBuf[2];
	unsigned char pBufRead[1];
	int i=0;
	for (i = 0;i < 3;i++)
	{
		pBuf[0] = KB_I2C_ADDR_TOUCH_ENABLE;
		pBuf[1] = KB_I2C_VALUE_TOUCH_ENABLE;
		kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,2);
		pBuf[0] = KB_I2C_ADDR_TOUCH_ENABLE;
		kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,1);
		kb_i2c_read_str(KB_I2C_SLAVER_READ, pBufRead, 1);
	#ifdef DEBUG_TOUCHKEY
		iDPrintf("[%d]touch=0x%02x\r\n",__LINE__,pBufRead[0]);
	#endif
		if (pBufRead[0] == pBuf[1])
		{
			break;
		}
	}
}

void s_TouchKeyStop(void)
{
	unsigned char pBuf[8];
	pBuf[0] = KB_I2C_ADDR_KBBL;
	pBuf[1] = KB_I2C_VALUE_KBBL_OFF;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,2);
	pBuf[0] = KB_I2C_ADDR_TOUCH_ENABLE;
	pBuf[1] = KB_I2C_VALUE_TOUCH_DISABLE;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,2);
		
	pBuf[0] = KB_I2C_ADDR_KBBL;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,1);
	kb_i2c_read_str(KB_I2C_SLAVER_READ,pBuf,1);
#ifdef DEBUG_TOUCHKEY
	iDPrintf("[%d]kbbl=0x%02x\r\n",__LINE__,pBuf[0]);
#endif
	pBuf[0] = KB_I2C_ADDR_TOUCH_ENABLE;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,1);
	kb_i2c_read_str(KB_I2C_SLAVER_READ,pBuf,1);
#ifdef DEBUG_TOUCHKEY
	iDPrintf("[%d]touch=0x%02x\r\n",__LINE__,pBuf[0]);
#endif
}

void s_TouchKeyInitBaseLine(void)
{
	unsigned char pBuf[2];
	pBuf[0] = KB_I2C_ADDR_INITBASELINE;
	pBuf[1] = 0x01;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,2);
}

unsigned char s_TouchKeyVersion(void)
{
	unsigned char pBuf[1] = {0};
	pBuf[0] = KB_I2C_ADDR_VERSION;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,1);
	kb_i2c_read_str(KB_I2C_SLAVER_READ,pBuf,1);
	return pBuf[0];
}

void s_TouchKeyBLightCtrl(unsigned char mode)
{
	unsigned char pBuf[2] = {0,0};
	if (mode == 0)
	{
		pBuf[0] = KB_I2C_ADDR_KBBL;
		pBuf[1] = KB_I2C_VALUE_KBBL_OFF;
		kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,2);
	}
	else if (mode == 1)
	{
		pBuf[0] = KB_I2C_ADDR_KBBL;
		pBuf[1] = KB_I2C_VALUE_KBBL_ON;
		kb_i2c_write_str(KB_I2C_SLAVER_WRITE,pBuf,2);
	}
}

int giTouchKeyLockFlag = 0;

void s_TouchKeyLockSwitch(int LockMode)
{	
	if (LockMode==0)
	{
		/* unlock kb */
		gpio_set_pin_interrupt(KB_INT_GPIO,1);
	}
	else
	{
		/* lock kb */
		gpio_set_pin_interrupt(KB_INT_GPIO,0);
	}
	giTouchKeyLockFlag = LockMode;
}


