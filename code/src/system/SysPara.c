#include <string.h>
#include "Posapi.h"
#include "base.h"
#include "..\Cfgmanage\cfgmanage.h"
#include "..\lcd\LcdDrv.h"
#include "..\lcd\Lcdkd.h"

#undef NULL
#define NULL ((void *)0)
static uchar s_serial_num[33];
static uchar s_ex_sn[50];

/*GetValue,PutValue用于获取，创建缓冲区里面的记录
 *缓冲区记录的格式为"name=value\n"，name为记录的名称，value为记录的值，
 *每个记录必须以'\n'字符作为结尾,第一个'='字符作为name跟value之间的分隔符,
 *有'='与'\n'的记录被视为有效的一条记录.记录的value可以为空,即"name=\n'.
 */
/*valid string is name=value\n,note:value can be omit*/
int GetValue(unsigned char *buffer, int buffer_size,char *name,char *value)
{
    char *record_name;
    char *record_value;
    int name_len;
    int value_len;
    int left_len;
    int record_len;
    char *left_buffer;
    char *p_ch;
    int temp_len;
    int i;

    left_len = buffer_size;
    left_buffer = buffer;
    p_ch = left_buffer;
    while(1)
    {
        temp_len = left_len;
        p_ch = left_buffer;
        record_value = NULL;
        /*check if the rest of buffer containing a '\n'*/
        for(i=0; i<temp_len; i++,p_ch++) {
            if(*p_ch == '\n')
                break;
            /*the first '=' is consider to be a seperator between record's name and value*/
            if(*p_ch == '=' && record_value == NULL)
                record_value = p_ch+1;
        }

        /*not a '\n' found,so the rest of buffer containing no record*/
        if(i == temp_len) {
            *value = '\0';
            return -1;
        }

        record_len = i + 1;
        /*value is the content between the first '=' and the '\n'
         *if no '=' found,consider it's an invalid record,skip it.
         */
        if(record_value != NULL) {
            record_name = left_buffer;
            name_len = record_value - record_name - 1;
            value_len = record_len - name_len - 2;

            if(name_len == strlen(name) && !memcmp(name,record_name,name_len)) {
                memcpy(value,record_value,value_len);
                *(value + value_len) = '\0';
                return 0;
            }
        }

        left_len -= record_len;
        left_buffer += record_len;
    }

    /*we will never get here,just prevent complier's warning of function has no return value*/
    *value = 0;
    return -1;
}

/*if old record exist,will be updated*/
void PutValue(unsigned char *buffer, int *p_buffer_size,char *name,char *value)
{
    char *record_name;
    char *record_value;
    int name_len;
    int value_len;
    int record_len;
    char *left_buffer;
    int left_len;
    int i;
    int record_found = 0;
    int buffer_size;
    char *p_ch;
    int temp_len;

    left_len = *p_buffer_size;
    left_buffer = buffer;
    p_ch = left_buffer;
    while(1)
    {
        temp_len = left_len;
        p_ch = left_buffer;
        record_value = NULL;
        /*check if the rest of buffer containing a '\n'*/
        for(i=0; i<temp_len; i++,p_ch++) {
            if(*p_ch == '\n')
                break;
            /*the first '=' is considered to be a seperator between record's name and value*/
            if(*p_ch == '=' && record_value == NULL)
                record_value = p_ch+1;
        }

        /*not a '\n' found,so the rest of buffer containing no record*/
        if(i == temp_len)
            break;

        record_len = i + 1;

        /*value is the content between the first '=' and the '\n'
         *if no '=' found,consider it's an invalid record,skip it.
         */
        if(record_value != NULL) {
            record_name = left_buffer;
            name_len = record_value - record_name - 1;

            if(name_len == strlen(name) && !memcmp(name,record_name,name_len)) {
                record_found = 1;
                break;
            }
        }

        left_len -= record_len;
        left_buffer += record_len;
    }

    buffer_size = *p_buffer_size;

    /*last sentence is not valid record(doesn't end with '\n'),we need to cut it off*/
    if(!record_found && i != 0)
    {
        memset(left_buffer, 0, temp_len);
        buffer_size -= temp_len;
    }

    /*delete current record*/
    if(record_found)
    {
        memmove(record_name,record_name+record_len,((unsigned int)(buffer + buffer_size) - (unsigned int)(record_name + record_len)));
        buffer_size -= record_len;
        memset(buffer+buffer_size,0,record_len);
    }

    /*add record to the end of buffer*/
    memcpy(buffer+buffer_size,name,strlen(name));
    buffer_size += strlen(name);
    memcpy(buffer+buffer_size,"=",1);
    buffer_size += 1;
    memcpy(buffer+buffer_size,value,strlen(value));
    buffer_size += strlen(value);
    memcpy(buffer+buffer_size,"\n",1);
    buffer_size += 1;

    *p_buffer_size = buffer_size;
}

/*return -1:open SysPara file error
 *name为记录的名称,value为记录的值.
 *name为输入字符串;value均为输出,输出为一字符串
 */
int SysParaRead(const char *name,char *value)
{
	unsigned char file_buffer[MAX_SYS_PARA_FILE_SIZE];
	int file_len;
	int fd;
	int ret;

	fd = s_open(SYS_PARA_FILE,O_RDWR,(unsigned char *)"\xff\x05");
	if(fd < 0)	return -1;

	memset(file_buffer,0,sizeof(file_buffer));
	file_len = read(fd,file_buffer,MAX_SYS_PARA_FILE_SIZE);
	ret =GetValue(file_buffer,file_len,(char*)name,value)	;
	if(ret != 0) ret = -2;

	close(fd);
#if 0
    ScrRestore(0);
    ScrCls();
    Lcdprintf("get:%s",file_buffer);
    getkey();
    ScrRestore(1);
#endif
    return ret;
}

/*return -1:open SysPara file error
 *name为记录的名称,value为记录的值.
 *如果name对应的记录已存在，则更新其value，否则创建新的记录
 */
int SysParaWrite(const char *name,const char *value)
{
    unsigned char file_buffer[MAX_SYS_PARA_FILE_SIZE];
    int file_len;
    int fd;
    int ret;

    fd = s_open(SYS_PARA_FILE,O_RDWR | O_CREATE,(unsigned char *)"\xff\x05");
    if(fd < 0)
        return -1;

    memset(file_buffer,0,sizeof(file_buffer));
    file_len = read(fd,file_buffer,MAX_SYS_PARA_FILE_SIZE);
    close(fd);

    PutValue(file_buffer,&file_len,(char*)name,(char*)value);
    s_remove(SYS_PARA_FILE,(unsigned char *)"\xff\x05");
    fd = s_open(SYS_PARA_FILE,O_RDWR | O_CREATE,(unsigned char *)"\xff\x05");
    if(fd < 0 || write(fd,file_buffer,file_len) != file_len)
        ret = -2;
    else
        ret = 0;

    close(fd);
#if 0
    fd = s_open(SYS_PARA_FILE,O_RDWR,(unsigned char *)"\xff\x05");
    memset(file_buffer,0,sizeof(file_buffer));
    if(fd >= 0)
        read(fd,file_buffer,sizeof(file_buffer));
    ScrRestore(0);
    ScrCls();
    Lcdprintf("put read ret:%d*%s",ret,file_buffer);
    getkey();
    ScrRestore(1);
    close(fd);
#endif
    return ret;
}

int SerialCheck(const uchar *s,int chkl,int fixmode)
{
	int ii;
	uchar tch;
	uchar *p;
	p=s;
	for(ii=0;ii<chkl;ii++)
	{
		tch=*p++;
		if((tch>=0x21)&&(tch<=0x7e))
		{

		}
		else
		{
			break;
		}
	}
	if(ii==0)
	{
		if((s[0]==0x00)||(s[0]==0xff))
		{
			
		}
		else ii=-2;
	}
	if(fixmode)
	{
		if(ii==chkl) return ii;
		else		 return -1;
	}
	return ii;
}

void s_ReadSN(void)
{
	uchar ucTempSN[50],ucSN[16], ucAesKey[16];
	int iRet;
    int iLen;

	memset(s_serial_num,0,sizeof(s_serial_num));
	memset(ucTempSN,0,sizeof(ucTempSN));
	iRet = ReadTerminalInfo(INDEX_SN, ucTempSN,  sizeof(s_serial_num)-1);
	if(iRet >= 0)
	{
		if(s_GetBootSecLevel() >= POS_SECLEVEL_L2)
		{
			s_GetBootAesKey(ucAesKey);
			AES(ucTempSN, ucSN, 16, ucAesKey, AES_DECRYPT);
			memcpy(ucTempSN, ucSN, 16);
		}
		iLen = SerialCheck(ucTempSN,strlen(ucTempSN),1);
		if(iLen > 0) strcpy(s_serial_num, ucTempSN);
	}

	memset(s_ex_sn,0,32);
	iRet = ReadTerminalInfo(INDEX_EXSN,ucTempSN, sizeof(ucTempSN));  
	if(iRet >= 0)
	{
		iLen = SerialCheck(ucTempSN,22,0);
		if ( iLen > 0 ) memcpy(s_ex_sn, ucTempSN, iLen);
	}
}

/*********************************************************************
* 函数名称:
*               void ReadSN(unsigned char *SN)
* 功能描述:
*               读取POS机序列号
* 被以下函数调用:
*               无
* 调用以下函数:
*              s_FlashRead    FLASH读操作
* 输入参数:
*             无
* 输出参数:
*            SN       POS机序列号，最多8字节
* 返回值:
*            无
****************************************************************************/
void ReadSN(uchar *SN)
{
	if (SN == NULL)  return;
	strcpy(SN, s_serial_num);
}

/*********************************************************************
* 函数名称:
*               void EXReadSN(unsigned char *SN)
* 功能描述:
*               读取POS机扩展序列号
* 被以下函数调用:
*               无
* 调用以下函数:
*              s_FlashRead    FLASH读操作
* 输入参数:
*             无
* 输出参数:
*            SN       POS机扩展序列号，最多32字节
* 返回值:
*            无
****************************************************************************/
void EXReadSN(uchar *SN)
{
	if (SN == NULL) return;
	memcpy(SN, s_ex_sn, 22);
}

/*********************************************************************
* 函数名称:
*               void ReadMacAddr(unsigned char *MacAddr)
* 功能描述:
*               读取S80 MAC地址 
* 被以下函数调用:
*               无
* 调用以下函数:
*              s_FlashRead    FLASH读操作
* 输入参数:
*             无
* 输出参数:
*            MacAddr       6字节
* 返回值:
*            无
****************************************************************************/
void ReadMacAddr(unsigned char *MacAddr)
{
	uchar ucTempAddr[20];
    uchar SN[8],TimeBuf[9];
	int iRet;
	memset(ucTempAddr,0,sizeof(ucTempAddr));
	memset(SN,0,sizeof(SN));
	memset(TimeBuf,0,sizeof(TimeBuf));	
	if (MacAddr == NULL)
	{
		return;
	}
	  
	iRet=ReadTerminalInfo(INDEX_MACADDR,ucTempAddr, sizeof(ucTempAddr));
	if(iRet < 0)
	{
		//使用随机数生成 MAC
		GetRandom(ucTempAddr);
		ucTempAddr[0] &= 0xfe;
		ucTempAddr[0] |= 0x02;
		WriteMacAddr(ucTempAddr);
		memcpy(MacAddr, ucTempAddr, 6);
	}
	else
		memcpy(MacAddr,ucTempAddr,6);
}

uchar ReadLCDGray(void)
{
	uchar ucTemp[4];
	int iret;

	iret=ReadTerminalInfo(INDEX_LCDGRAY,ucTemp, sizeof(ucTemp));
	if (iret < 0) return GetDefaultGray();
	if((ucTemp[1]==LCD_CONST_FLAG)&&(ucTemp[0]<=MAX_GRAY)) return ucTemp[0];
	else return GetDefaultGray();
}
int  WriteLCDGray(unsigned char LcdGrayVal)
{
	uchar buf[2];

	buf[0]=LcdGrayVal;
	buf[1]=LCD_CONST_FLAG;

	return WriteTerminalInfo(INDEX_LCDGRAY, buf, 2);
}


int WriteMacAddr(uchar * MacAddr)
{
	int iret;
	iret =WriteTerminalInfo(INDEX_MACADDR,MacAddr,0x06);
	return iret;
}

// add by yangrb 射频卡参数
int WriteRFParam(int iInLen,unsigned char* pucParamIn)
{
    return 0;
}

int ReadRFParam(unsigned char* pucParamOut)
{
	int iret;
	char context[33];
	char buff[33];
	int i;
	if(pucParamOut	==NULL)
		return -1;
	iret=ReadCfgInfo("RF_PARA", context);
	if(iret<0)
		return iret;

	for(i=0;i<strlen(context);i++)
	{
		buff[i]=(char)toupper(context[i]);
		if(buff[i]>='A' && buff[i]<='F')
			buff[i]=buff[i]-'A'+0x0A;
		else if(buff[i]>='0' && buff[i]<='9')
			buff[i]=buff[i]-'0';
	}
	
	for(i=0;i<strlen(context)/2;i++)
	{
		pucParamOut[i]=buff[i*2]*16+buff[i*2+1];
	}
	
	return (strlen(context))/2;
}

/*TUSN: 终端唯一序列号（厂商编号+终端类型+SN*/
#define TUSN_FLAG "00000202+PAXSN"
int ReadTUSN(uchar* buff, uint len)
{
	uchar tmpbuff[64],tusn_flag[16], ucAesKey[16], type;
    int iLen = 0;
    static uchar TUSN[64]={0}, first = 0;

    if (first == 0) {
    	memset(tmpbuff,0,sizeof(tmpbuff));
    	iLen = ReadTerminalInfo(INDEX_TUSN, tmpbuff,  sizeof(tmpbuff)-1);
    	if (iLen<=0) return 0;

    	s_GetBootAesKey(ucAesKey);
    	AES(tmpbuff, tusn_flag, 16, ucAesKey, AES_DECRYPT);
        if (memcmp(tusn_flag, TUSN_FLAG, strlen(TUSN_FLAG))) return 0;
        if (0==strlen(s_serial_num)) return 0;
        //0x01:ATM, 0x02:传统POS, 0x03:MPOS(D180、D150), 0x04:智能POS, 0x05: II型电话POS
        type = 0x02;
        
        memset(TUSN,0,sizeof(TUSN));    
        sprintf(TUSN, "000002%02x%s", type, s_serial_num);
        first = 1;
    }

    iLen = strlen(TUSN);
    if (iLen>len) iLen = len;
    memcpy(buff, TUSN, iLen);
    return iLen;
}

int WriteTUSN()
{
	char buf[16],en_buf[16], key[16], tmpbuff[64];
	int ret;

    memset(tmpbuff,0,sizeof(tmpbuff));
    ret = ReadTerminalInfo(INDEX_TUSN, tmpbuff,  sizeof(tmpbuff)-1);
    if (ret<=0){//只要存在INDEX_TUSN，即不再允许写了
        memset(buf,0,sizeof(buf));    
        strcpy(buf, TUSN_FLAG);

        s_GetBootAesKey(key);
        AES(buf, en_buf, 16, key, AES_ENCRYPT);
        ret = WriteTerminalInfo(INDEX_TUSN, en_buf, 16);
        if (ret) return -1;
    }
    
	return 0;
}

int ReadCSN(uchar BuferLen, unsigned char *CSN)
{
	uchar ucTempCSN[129];
	int iRet;
    int iLen;
	if (CSN == NULL)
	{
		  return -1;
	}
	if(BuferLen==0)
		return -1;
	if(BuferLen>128)
		BuferLen = 128;

	CSN[0]=0x00;
	iRet = ReadTerminalInfo(INDEX_CSN,ucTempCSN, BuferLen+1);  
	if(iRet >= 0)
	{
		iLen = SerialCheck(ucTempCSN,iRet,0);
		if((iLen >=0) && (iLen <= BuferLen))
		{
		   	memcpy(CSN, ucTempCSN, iLen);
		   	CSN[iLen]=0;
		}
		else
			return -1;
	}

	return 0;
}
int WriteCSN(unsigned char *CSN)
{
	int iret,len;
	
	if(CSN == NULL)
	{
		return -1;
	}
	
	len=SerialCheck(CSN,128,0);
	if((len < 0) || (len < strlen(CSN)))
	{
		return 1;
	}

	if(len ==0) 
	{
		iret =WriteTerminalInfo(INDEX_CSN,"\xff",1);
	}
	else 
	{
		iret =WriteTerminalInfo(INDEX_CSN,CSN,len);
	}

	return iret;
}
