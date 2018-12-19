/*****************************************************************
2007-11-13
修改人:杨瑞彬
修改日志:
修改MSR_TrackInfo结构体
*****************************************************************/
#ifndef _MAGCARD_H_
#define	_MAGCARD_H_



#ifndef     ON
#define     ON                  1
#endif

#ifndef     OFF
#define     OFF                 0
#endif

#ifndef     IO_OUT
#define     IO_OUT              0
#endif

#ifndef     IO_IN
#define     IO_IN               1
#endif

#ifndef     IO_INT_FALLING
#define     IO_INT_FALLING      2
#endif

#ifndef     IO_INT_RISING
#define     IO_INT_RISING       3
#endif


#define PARI_ERR        0x01
#define LRC_ERR         0x10

#define TRACK_LEN       256
#define TRACK_BIT_LEN   2100



// Modified by yangrb 2007-11-13
typedef struct
{
    uchar   buf[150];           //  解码后的数据缓冲,未译码,含起始符结束符和校验符
    uchar   start;              //  解码时起始符是否正确. 1=是；0=否
    uchar   stop;               //  解码时结束符是否正确. 1=是；0=否
    uchar   lrc;                //  解码时校验符是否正确. 1=是；0=否
    uchar   odd;                //  解码时某字节校验是否正确.1=是 0=否
    int     len;                //  解码得到的数据长度(不含起始符、结束符和校验符)
    int     error;              //  解码后的状态，0=成功，1=错误
    //int     OddErr;             //  n=奇校验有错误的个数
}MSR_TrackInfo;

typedef struct
{
    uchar   buf[750];           //  从磁头返回的位数据
    int     BitOneNum;          //  从磁头返回的位数据为1的个数
}MSR_BitInfo;

void   s_MagInit(void);

void   MagOpen(void);
void   MagClose(void);
void   MagReset(void);
uchar  MagSwiped(void);
uchar  MagRead(uchar *Track1, uchar *Track2, uchar *Track3);
void	s_SetMagBitBuf(MSR_BitInfo msr_bt[3]);
#endif
