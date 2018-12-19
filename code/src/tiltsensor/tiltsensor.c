/**************************************

文件描述:tiltsensor.c

**************************************/
#include "i2c.h"
#include "tiltsensor.h"
#include <math.h>
#include "base.h"

//#define G_SENSOR_DEBUG
// 局部函数定义
void  Tiltsenor_Init(void);
void  Tiltsensor_Get_ID(uchar *pucData);
static void  ADXL345_Init(void);
static void  ADXL345_Read(uchar RegAddr,uchar *Buf);
static void  ADXL345_Write(uchar RegAddr,uchar Data);
static uchar Sin_Angle2Value(uchar ucAngleValue,float *pfValue);
static void  Sin_Value2Angle(float fValue,uchar *pucAngle);
static void  Tiltsensor_Get_Data(uchar *pucData);
void  TiltsensorTest(void);
static void  s_GetLeanAngle(int *piXLeanAngle,int *piYLeanAngle,int *piZLeanAngle);
static int g_AngleValue[3];

typedef struct
{
    uint  uiAngleValue;
    float fSinValue;
}SinTable;

// 正弦函数查询表
SinTable SinLookUpTable[] = 
{
    {0, 0.000000},{1, 0.017452},{2, 0.034899},{3, 0.052336},{4, 0.069756},
    {5, 0.087155},{6, 0.104528},{7, 0.121869},{8, 0.139173},{9, 0.156434},
    {10,0.173648},{11,0.190809},{12,0.207912},{13,0.224951},{14,0.241922},
    {15,0.258819},{16,0.275637},{17,0.292372},{18,0.309017},{19,0.325568},
    {20,0.342020},{21,0.358368},{22,0.374607},{23,0.390731},{24,0.406737},
    {25,0.422618},{26,0.438371},{27,0.453990},{28,0.469471},{29,0.484809},
    {30,0.500000},{31,0.515038},{32,0.529919},{33,0.544639},{34,0.559193},
    {35,0.573576},{36,0.587785},{37,0.601815},{38,0.615661},{39,0.629320},
    {40,0.642788},{41,0.656059},{42,0.669131},{43,0.681998},{44,0.694658},
    {45,0.707107},{46,0.719340},{47,0.731354},{48,0.743145},{49,0.754710},
    {50,0.766044},{51,0.777146},{52,0.788010},{53,0.798636},{54,0.809017},
    {55,0.819152},{56,0.829038},{57,0.838671},{58,0.848048},{59,0.857167},
    {60,0.866025},{61,0.874620},{62,0.882948},{63,0.891007},{64,0.898794},
    {65,0.906308},{66,0.913545},{67,0.920505},{68,0.927184},{69,0.933580},
    {70,0.939693},{71,0.945519},{72,0.951057},{73,0.956305},{74,0.961262},
    {75,0.965926},{76,0.970296},{77,0.974370},{78,0.978148},{79,0.981627},
    {80,0.984808},{81,0.987688},{82,0.990268},{83,0.992546},{84,0.994522},
    {85,0.996195},{86,0.997564},{87,0.998630},{88,0.999391},{89,0.999848},
    {90,1.000000}
};

volatile uchar gucTransPermitMaxAngle = 0;

typedef struct 
{
	int offset[3];
	int Gain[3];

}ADJustParam;

static ADJustParam g_ADjust;

// 初始化
#define DEFAULT_X_OFFSET	9
#define DEFAULT_Y_OFFSET	-7
#define DEFAULT_Z_OFFSET	-32

#define DEFAULT_X_GAIN		1000
#define DEFAULT_Y_GAIN		1000
#define DEFAULT_Z_GAIN		960

void Tiltsenor_Init(void)
{
	char context[32];
	int Angletemp[3];
	int x_angle,y_angle,z_angle,ret,i;

    if(ReadCfgInfo("G_SENSOR",context) < 0) return;//未配置G-sensor

	i2c_config();
	ADXL345_Init();	
	memset(g_AngleValue,0,sizeof(g_AngleValue));	
	
    /*暂时用于屏蔽重力传感器的硬件缺陷*/
    i = 10;
    while(i-- > 0)
    {
        i2c_config();
        ADXL345_Init(); 
        DelayMs(10);
        GetLeanAngle(&x_angle,&y_angle,&z_angle);
        if(x_angle || y_angle || z_angle) break;
    }
	ret=ReadCfgInfo("G_SENSOR_PARA",context);
	if(ret>0)
	{
		for(i=0;i<9;i++)	
		{
			context[i]= context[i]-'0';
		}
		Angletemp[0]=context[0]*100+context[1]*10+context[2];
		Angletemp[1]=context[3]*100+context[4]*10+context[5];
		Angletemp[2]=context[6]*100+context[7]*10+context[8];
		if(Angletemp[0]>180) Angletemp[0]=-(Angletemp[0]-180);
		if(Angletemp[1]>180) Angletemp[1]=-(Angletemp[1]-180);
		if(Angletemp[2]>180) Angletemp[2]=-(Angletemp[2]-180);
		g_AngleValue[0]=Angletemp[0];
		g_AngleValue[1]=Angletemp[1];
		g_AngleValue[2]=Angletemp[2];		
	}	
	g_ADjust.Gain[0] = DEFAULT_X_GAIN;
	g_ADjust.Gain[1] = DEFAULT_Y_GAIN;
	g_ADjust.Gain[2] = DEFAULT_Z_GAIN;

	g_ADjust.offset[0] = DEFAULT_X_OFFSET;
	g_ADjust.offset[1] = DEFAULT_Y_OFFSET;
	g_ADjust.offset[2] = DEFAULT_Z_OFFSET;
	g_ADjust.offset[0] += g_AngleValue[0];
	g_ADjust.offset[1] += g_AngleValue[1];
	g_ADjust.offset[2] += g_AngleValue[2];
}



static void ADXL345_Init(void)
{
    uchar ucTemp = 0;

    //Power CTL: Measure mode, activity and inactivity serial
    ucTemp = XL345_MEASURE;
	ADXL345_Write(XL345_POWER_CTL,ucTemp);

    //Output Data Rate: 100Hz
	ucTemp = XL345_RATE_100;			
	ADXL345_Write(XL345_BW_RATE,ucTemp);

    //Data Format: +/-2g range, right justified,  256->1g
	ucTemp = 0x00;		
	ADXL345_Write(XL345_DATA_FORMAT,ucTemp);

    // No Int Enable
	ucTemp = 0x00;
	ADXL345_Write(XL345_INT_ENABLE,ucTemp);

    //INT_Map: ACTIVITY & INACTIVITY interrupt to INT2 pin
	/*ucTemp = XL345_ACTIVITY | XL345_INACTIVITY;	
	i2c_write(1, XL345_INT_MAP,ucTemp);*/
}


static void ADXL345_Read(uchar RegAddr,uchar *Buf)
{
    uchar ucTemp = 0;
    ucTemp = RegAddr;
    i2c_write(TILTSENSOR_WRITE_ADDR,&ucTemp,1);
    i2c_read(TILTSENSOR_READ_ADDR,Buf,1);
}

static void ADXL345_Write(uchar RegAddr,uchar Data)
{
    uchar ucTemp[2];
    memset(ucTemp,0x00,sizeof(ucTemp));
    ucTemp[0] = RegAddr;
    ucTemp[1] = Data;
    i2c_write(TILTSENSOR_WRITE_ADDR,ucTemp,2);
}

// 获取ADXL345 ID
void Tiltsensor_Get_ID(uchar *pucData)
{
    ADXL345_Read(XL345_DEVID,pucData);
}


#define SAMPLE_COUNT 5
void Tiltsensor_Get_Data(uchar *pucData)
{
    unsigned char temp;
    int i,j;
    unsigned char sample_data[SAMPLE_COUNT][6];
    int x[SAMPLE_COUNT],y[SAMPLE_COUNT],z[SAMPLE_COUNT];
    int x_max = -1024,x_min = 1024,x_sum = 0;
    int y_max = -1024,y_min = 1024,y_sum = 0;
    int z_max = -1024,z_min = 1024,z_sum = 0;

    for(i=0; i<SAMPLE_COUNT; i++)
    {
        for(j=0; j<100; j++)
        {
            ADXL345_Read(XL345_INT_SOURCE,&temp);
            if((temp & 0x80)) /*等待数据就绪*/
                break;
        }
        
        // 获取X轴方向数据
        ADXL345_Read(XL345_DATAX0,&sample_data[i][0]);
        ADXL345_Read(XL345_DATAX1,&sample_data[i][1]);

        // 获取Y轴方向数据
        ADXL345_Read(XL345_DATAY0,&sample_data[i][2]);
        ADXL345_Read(XL345_DATAY1,&sample_data[i][3]);

        // 获取Z轴方向数据
        ADXL345_Read(XL345_DATAZ0,&sample_data[i][4]);
        ADXL345_Read(XL345_DATAZ1,&sample_data[i][5]);
		DelayMs(10);
    }

    for(i=0; i<SAMPLE_COUNT; i++)
    {
        x[i] = (short)(sample_data[i][1]<<8 | sample_data[i][0]);
        y[i] = (short)(sample_data[i][3]<<8 | sample_data[i][2]);
        z[i] = (short)(sample_data[i][5]<<8 | sample_data[i][4]);

        x_sum += x[i];
        if(x[i] < x_min)
            x_min = x[i];
        if(x[i] > x_max)
            x_max = x[i];

        y_sum += y[i];
        if(y[i] < y_min)
            y_min = y[i];
        if(y[i] > y_max)
            y_max = y[i];

        z_sum += z[i];
        if(z[i] < z_min)
            z_min = z[i];
        if(z[i] > z_max)
            z_max = z[i];
    }

    x_sum -= x_min + x_max;
    x_sum /= SAMPLE_COUNT - 2;
    y_sum -= y_min + y_max;
    y_sum /= SAMPLE_COUNT - 2;
    z_sum -= z_min + z_max;
    z_sum /= SAMPLE_COUNT - 2;
	
    *pucData++ = x_sum & 0xff;
    *pucData++ = x_sum >> 8;
    *pucData++ = y_sum & 0xff;
    *pucData++ = y_sum >> 8;
    *pucData++ = z_sum & 0xff;
    *pucData = z_sum >> 8;
}

void  GetLeanAngle(int *piXLeanAngle,int *piYLeanAngle,int *piZLeanAngle)
{
	s_GetLeanAngle(piXLeanAngle,piYLeanAngle,piZLeanAngle);		
}

static void  s_GetLeanAngle(int *piXLeanAngle,int *piYLeanAngle,int *piZLeanAngle)
{
    uchar ucData[6];
    int   iTemp = 0;
    float fValue = 0.00;
    uchar ucTemp = 0;
    int   iDegree = 0;
    uchar ucPolarity = 0;

    memset(ucData,0x00,sizeof(ucData));

    Tiltsensor_Get_Data(ucData);

    iTemp = (ucData[1] & 0x03) << 8 | ucData[0];
    if (iTemp > 512)
    {
    	fValue = (float)g_ADjust.Gain[0]/1000;
    	iTemp = iTemp -g_ADjust.offset[0];
        iTemp = 1024 - iTemp;
		iTemp =(int) ((float)iTemp /fValue);
        ucPolarity = 1;

    }else
	{
    	fValue = (float)g_ADjust.Gain[0]/1000;
    	iTemp =(int) ((float)(iTemp -g_ADjust.offset[0])/fValue);
	}
	if(iTemp<0)
	{
		iTemp = -iTemp;
		ucPolarity = 1;

	}
    fValue = (float)iTemp / 256;
    
    Sin_Value2Angle(fValue,&ucTemp);

    if (ucPolarity == 0)
    {
        *piXLeanAngle = (int)ucTemp;
    }
    else
    {
        *piXLeanAngle =  (int)-ucTemp;
    }

    ucPolarity = 0;
    iTemp = (ucData[3] & 0x03) << 8 | ucData[2];
    if (iTemp > 512)
    {
    	fValue = (float)g_ADjust.Gain[1]/1000;
    	iTemp = iTemp -g_ADjust.offset[1];
		iTemp = 1024 - iTemp;
		iTemp =(int) ((float)iTemp /fValue);
		ucPolarity = 1;

    }else
	{
    	fValue = (float)g_ADjust.Gain[1]/1000;
    	iTemp =(int) ((float)(iTemp -g_ADjust.offset[1])/fValue);
	}
	if(iTemp<0)
	{
		iTemp = -iTemp;
		ucPolarity = 1;
	}
    fValue = (float)iTemp / 256;
    
    Sin_Value2Angle(fValue,&ucTemp);

    if (ucPolarity == 0)
    {
        *piYLeanAngle = (int)ucTemp;
    }
    else
    {
        *piYLeanAngle = (int)-ucTemp;
    }


    ucPolarity = 1;
    iTemp = (ucData[5] & 0x03) << 8 | ucData[4];
    if (iTemp > 512)
    {      
        ucPolarity = 0;
        fValue = (float)g_ADjust.Gain[2]/1000;
        iTemp = iTemp -g_ADjust.offset[2];
		iTemp = 1024 - iTemp;
		iTemp =(int) ((float)iTemp /fValue);
    }
	else
	{
    	fValue = (float)g_ADjust.Gain[2]/1000;
    	iTemp =(int) ((float)(iTemp -g_ADjust.offset[2])/fValue);
	}
	if(iTemp<0)
	{
		iTemp = -iTemp;
		ucPolarity = 1;

	}
    fValue = (float)iTemp / 256;
    
    Sin_Value2Angle(fValue,&ucTemp);

    if (ucPolarity == 0)
    {
        *piZLeanAngle = (int)ucTemp;
    }
    else
    {
        *piZLeanAngle =  (int)-ucTemp;
    }
    
}

static uchar Sin_Angle2Value(uchar ucAngleValue,float *pfValue)
{
    uint i = 0;
    
    if (ucAngleValue > 90)
    {
        return 1;
    }

    for (i = 0; i < sizeof(SinLookUpTable) / sizeof(SinTable);i++)
    {
        if (ucAngleValue == SinLookUpTable[i].uiAngleValue)
        {
            *pfValue = SinLookUpTable[i].fSinValue;
            break;
        }
    }
    return 0;
}

static void Sin_Value2Angle(float fValue,uchar *pucAngle)
{
    uint i = 0;

    if (fValue >= 1.00)
    {
        *pucAngle = 90;
    }
    
    for (i = 0; i < sizeof(SinLookUpTable) / sizeof(SinTable) - 1; i++)
    {
        if ((fValue >= SinLookUpTable[i].fSinValue) && (fValue <= SinLookUpTable[i+1].fSinValue))
        {
            *pucAngle = SinLookUpTable[i].uiAngleValue;
            break;
        }
    }
}

#ifdef G_SENSOR_DEBUG
void TiltsensorTest(void)
{
    uchar ucKey = 0;
    uchar ucID = 0;
    int   iData[3];
    uchar ucTemp[2];
    int   iAngleValue = 0;
    float fValue = 0.00;
    uchar ucDegree = 0;
    uchar ucRet = 0;

    memset(iData,0x00,sizeof(iData));
    memset(ucTemp,0x00,sizeof(ucTemp));
    
    Tiltsenor_Init();
    while(1)
    {    
        ScrCls();
        ScrPrint(0,0,0,"Tilt Sensor");
        ScrPrint(0,1,0,"1-Get ID 2-Data");
        ScrPrint(0,2,0,"3-Set Max Angle");
        ScrPrint(0,3,0,"4-CheckIfPermitTrans");
        ScrPrint(0,4,0,"5-Get Current Angle");
        ucKey = getkey();
        switch(ucKey)
        {
            case '1':
            {
                Tiltsensor_Get_ID(&ucID);
                ScrCls();
                ScrPrint(0,1,0,"ID:%x",ucID);
                getkey();
                break;
            }
            case '2':
            {
                while(1)
                {
                    ADXL345_Read(XL345_DATAX0,&ucTemp[0]);
                    ADXL345_Read(XL345_DATAX1,&ucTemp[1]);
                    iData[0] = (ucTemp[1] & 0x03) << 8 | ucTemp[0];
                    
                    ADXL345_Read(XL345_DATAY0,&ucTemp[0]);
                    ADXL345_Read(XL345_DATAY1,&ucTemp[1]);
                    iData[1] = (ucTemp[1] & 0x03) << 8 | ucTemp[0];
                
                    ADXL345_Read(XL345_DATAZ0,&ucTemp[0]);
                    ADXL345_Read(XL345_DATAZ1,&ucTemp[1]);
                    iData[2] = (ucTemp[1] & 0x03) << 8 | ucTemp[0];

                    ScrCls();
                    ScrPrint(0,1,0,"X:%d",iData[0]);
                    ScrPrint(0,2,0,"Y:%d",iData[1]);
                    ScrPrint(0,3,0,"Z:%d",iData[2]);
                    getkey();

                    if (iData[0] > 512)
                    {
                        iData[0] = 1024 - iData[0];
                    }
                    ScrCls();
                    fValue = (float)iData[0] / 256;
                    Sin_Value2Angle(fValue,&ucDegree);
                    ScrPrint(0,1,0,"X Sin Value:%f",fValue);
                    ScrPrint(0,2,0,"X Degree:%d",ucDegree);
                    if (iData[1] > 512)
                    {
                        iData[1] = 1024 - iData[1];
                    }
                    fValue = (float)iData[1] / 256;
                    Sin_Value2Angle(fValue,&ucDegree);
                    ScrPrint(0,3,0,"Y Sin Value:%f",fValue);
                    ScrPrint(0,4,0,"Y Degree:%d",ucDegree);
                    if (iData[2] > 512)
                    {
                        iData[2] = 1024 - iData[2];
                    }
                    fValue = (float)iData[2] / 256;
                    Sin_Value2Angle(fValue,&ucDegree);
                    ScrPrint(0,5,0,"Z Sin Value:%f",fValue);
                    ScrPrint(0,6,0,"Z Degree:%d",ucDegree);
                    if (getkey() == 0x1B)
                    {
                        break;
                    }
                }
                break;
            }
            case '3':
            {
                uchar aucStr[4];
                memset(aucStr,0x00,sizeof(aucStr));
                ScrCls();
                ScrGotoxy(0,32);
                GetString(aucStr,0xB5,0,2);
                //ucRet = SetTransPermitMaxAngle((uchar)atoi(&aucStr[1]));
                if (ucRet == 0)
                {
                    ScrPrint(0,5,0,"Set Success");
                }
                else
                {
                    ScrPrint(0,5,0,"Set Failed");
                }
                getkey();
                break;
            }
            case '4':
            {   
                while(1)
                {
                    //ucRet = CheckIfPermitTrans();
                    ScrCls();
                    if (ucRet == 0)
                    {
                        ScrPrint(0,4,0,"Permit");
                    }
                    else
                    {
                        ScrPrint(0,4,0,"Forbid");
                    }
                    if (!kbhit() && getkey() == 0x1B)
                    {
                        break;
                    }
                }
                break;
            }

            case '5':
            {
                int iXAngle = 0,iYAngle = 0,iZAngle = 0;

                while(1)
                {
                    GetLeanAngle(&iXAngle,&iYAngle,&iZAngle);
                    ScrCls();
                    ScrPrint(0,1,0,"X:%d",iXAngle);
                    ScrPrint(0,2,0,"Y:%d",iYAngle);
                    ScrPrint(0,3,0,"Z:%d",iZAngle);
                    DelayMs(50);
                    if (!kbhit() && getkey() == 0x1B)
                    {
                        break;
                    }
                }
            }
            default:
                break;
        }
    }
}
#endif

