/************************************************************
 *tslib/tests/ts_calibrate.c
 * Copyright (C) 2001 Russell King.
 * This file is placed under the GPL.  
 * Please see the fileCOPYING for more details.
 * $Id: ts_calibrate.c,v 1.8 2004/10/19 22:01:27 dlowder Exp $
 * Basic test program for touchscreen library.
************************************************************ */
#include <string.h>
#include "base.h"
#include "platform.h"
#include "tsc2046.h"
#include "ts_sample.h"
#include "ts_hal.h"
#include "..\lcd\lcdapi.h"
#include "..\lcd\LcdDrv.h"

#define MAX_SAMPLES	128
#define TS_NR_STEPS	2
#define SAMPLE_PRESS_LOWER_LIMIT 	100
#define SAMPLE_PRESS_UPPER_LIMIT 	5000

TS_SAMPLE_T tLastSample = {0, 0, 0}; /* record the last sample info in Function "variance" */
static TS_SAMPLE_T tHistorySamples[23]; /* for calc the average in Function "dejitter" */
extern int lcd_logic_max_x;
extern int lcd_logic_max_y;
extern void ts_disable_irq();
void ts_timer_stop();
void ts_enable_irq();
void ts_disable_irq();
void draw_line_reset();

typedef struct 
{
	int x[5], xfb[5];
	int y[5], yfb[5];
	int a[7];
} tCALIBRATION;
struct tslib_linear tslin = {0,};
struct tslib_linear tslin_S300 = 
{
	.p_offset = 0,
	.p_mult = 1,
	.p_div = 1,
	.a[0] = 48,
	.a[1] = 8755,
	.a[2] = -1428704,
	.a[3] = -11241,
	.a[4] = 68,
	.a[5] = 21737780,
	.a[6] = 65536
};

struct tslib_linear tslin_D210 =
{
	.p_offset = 0,
	.p_mult = 1,
	.p_div = 1,
	.a[0] = -97,
	.a[1] = -11308,
	.a[2] = 22036684,
	.a[3] = 8583,
	.a[4] = -15,
	.a[5] = -564472,
	.a[6] = 65536
};

struct tslib_linear tslin_S500 = 
{
	.p_offset = 0,
	.p_mult = 1,
	.p_div = 1,
	.a[0] = 157,
	.a[1] = -12037,
	.a[2] = 22143576,
	.a[3] = 8517,
	.a[4] = -2,
	.a[5] = -530072,
	.a[6] = 65536
};

/* button check purpose sample queue */
QUEUE_SAMPLE_T gtBtnPpSample = {
		.head = 0,
		.nbr = 0
};

/* App needed sample queue */
QUEUE_SAMPLE_T gtAppPpSample = {
		.head = 0,
		.nbr = 0
};

/* sys raw sample queue */
QUEUE_SAMPLE_T gtSysRawSample = {
		.head = 0,
		.nbr = 0
};

static int sqr(int x)
{
	return x * x;
}

unsigned long distance(TS_SAMPLE_T *s1, const TS_SAMPLE_T *s2)
{
	return sqr(s1->x - s2->x) + sqr(s1->y - s2->y);
}

int samp_valid(TS_SAMPLE_T *s)
{
	return ((s->pressure > SAMPLE_PRESS_LOWER_LIMIT) && 
			(s->pressure < SAMPLE_PRESS_UPPER_LIMIT));
}

/* put one sample data to queue */
int ts_put(TS_SAMPLE_T *s, QUEUE_SAMPLE_T *ptSampleQue)
{
	int idx;
	
	if(ptSampleQue->nbr == QUE_SIZE) /* full */
	   	ptSampleQue->head = (ptSampleQue->head + 1) & QUE_WRAP;
	else
		ptSampleQue->nbr++;

	idx = (ptSampleQue->head + ptSampleQue->nbr) & QUE_WRAP;
    ptSampleQue->s[idx] = *s;
	return 1;
}

/* get one sample data from queue */
int ts_get(TS_SAMPLE_T *s, QUEUE_SAMPLE_T *ptSampleQue)
{
	int ret = 0;
	int flag;
	
	irq_save(flag);
	if(ptSampleQue->nbr <= 0) /* empty */
	{
		ret = 0; 
	}
	else
	{
		ptSampleQue->head = (ptSampleQue->head + 1) & QUE_WRAP;
        *s = ptSampleQue->s[ptSampleQue->head];
		ptSampleQue->nbr--;
		ret = 1;
	}
	irq_restore(flag);
	return ret;
}

/* just view rather than get sample data from queue */
int ts_view(TS_SAMPLE_T *s, int i, QUEUE_SAMPLE_T *ptSampleQue)
{
	int idx, ret = 0;
	int flag;
	
	irq_save(flag);
	if(ptSampleQue->nbr == 0)
	{
		ret = 0; /* empty */
	}
	else
	{
		idx = (ptSampleQue->head + i) & QUE_WRAP;
        *s = ptSampleQue->s[idx];
		ret = 1;
	}
	irq_restore(flag);
	return ret;
}

void ts_clear_buf()
{
	memset(&gtSysRawSample, 0, sizeof(QUEUE_SAMPLE_T));

	memset(&tLastSample, 0, sizeof(TS_SAMPLE_T));
	memset(tHistorySamples, 0, sizeof(tHistorySamples));
}

/* Interrupt: touch interrupt and timer interrupt */
int read_raw(TS_SAMPLE_T *s)
{
	static TS_SAMPLE_T tTmpSamp;
	static int state=0;
	static uint z1, z2;

 	/* get sample info in several step avoiding affecting others' interrupts */
	switch(state)
	{
		case 0:
			tTmpSamp.x = ts_cmd(TS_CMD_X);
			state = 1;
			break;

		case 1:
			tTmpSamp.y = ts_cmd(TS_CMD_Y);
			state = 2;
			break;

		case 2:
			z1 = ts_cmd(TS_CMD_Z1);
			state = 3;
			break;

		case 3:
			z2 = ts_cmd(TS_CMD_Z2);
			state = 4;
			break;

		default:
			state = 0;
			break;
	}

	if(state == 4)
	{
		tTmpSamp.pressure = (((z2 - z1) * tTmpSamp.x) / z1);
		memcpy(s, &tTmpSamp, sizeof(TS_SAMPLE_T));
		state = 0;
		return 1;
	}

	return 0;
}

	
/* * * * * 
 *  ret : 
 *  0  - 	indicate no dot in SysRawSampleQueue 
 *  1  - 	indicate a valid dot read 
 *  2  - 	indicate that ts is release 
 *  -1 -    indicate a noise dot read 
 * * * *  */  


extern volatile int giIsPiccDetected;

int variance(TS_SAMPLE_T *s)
{
	#define FIRST_SAMPLE_NUM 	4
	#define AUTI_RF_AVERAGE  	10	
	static TS_SAMPLE_T averSample[AUTI_RF_AVERAGE + 1];
	static int  averSum=0;
	static TS_SAMPLE_T firstSample[FIRST_SAMPLE_NUM], sumSample;
	const int DELTA = 600; 
	TS_SAMPLE_T next, next1;
	static uchar isFirstSamp=0;
	static int pIdx=0;
	int ret;
	int i;
	
	ret = ts_get(s, &gtSysRawSample);
	if(ret == 0) return 0;
	if(s->pressure == 0 && s->x == 0 && s->y == 0) return 2;

	/* to tell the first sample */
	if(tLastSample.pressure == 0)
	{
		isFirstSamp = 1;
		pIdx = 0;
		memset(&sumSample, 0, sizeof(TS_SAMPLE_T));
	}

	/*
	新外观情况下，非接天线不在触摸屏下，不需要针对非接的干扰来调整触摸屏
	*/
	if(0 == GetTerminalRfStyle() && giIsPiccDetected)
	{
		averSample[averSum++] = *s;
		if(averSum >= AUTI_RF_AVERAGE)
		{
			memset(&sumSample, 0, sizeof(TS_SAMPLE_T));
			averSum = 0;
			for (i = 0; i < AUTI_RF_AVERAGE; i++)
			{
				sumSample.x += averSample[i].x;
				sumSample.y += averSample[i].y;
				sumSample.pressure += averSample[i].pressure;
			}

			tLastSample.x = sumSample.x / AUTI_RF_AVERAGE;
			tLastSample.y = sumSample.y / AUTI_RF_AVERAGE;
			tLastSample.pressure = sumSample.pressure / AUTI_RF_AVERAGE;
			*s = tLastSample;
			return 1;
		}
		else 
		{
			return -1;
		}
	}
	else
	{
		if(isFirstSamp == 1)
		{
			tLastSample = *s;
			firstSample[pIdx] = tLastSample;

			pIdx++;
			if(pIdx >= FIRST_SAMPLE_NUM)
			{
				for(i=0; i<FIRST_SAMPLE_NUM-1; i++)
				{
					if(distance(&firstSample[i], &firstSample[i+1]) >= DELTA)
					{
						memset(&tLastSample, 0, sizeof(TS_SAMPLE_T));
						return -1;
					}
				}

				for(i=0; i<FIRST_SAMPLE_NUM; i++)
				{
					sumSample.x += firstSample[i].x;
					sumSample.y += firstSample[i].y;
					sumSample.pressure += firstSample[i].pressure;

					if(i == FIRST_SAMPLE_NUM - 1)
					{
						tLastSample.x = sumSample.x / FIRST_SAMPLE_NUM;
						tLastSample.y = sumSample.y / FIRST_SAMPLE_NUM;
						tLastSample.pressure = sumSample.pressure / FIRST_SAMPLE_NUM;
						isFirstSamp = 0;
						goto valid_sample;
					}
				}
			}
			else return -1;
		}
	}

	if(distance(s, &tLastSample) <= DELTA)
		goto valid_sample;

	ret = ts_view(&next, 1, &gtSysRawSample);
	if(ret == 0) goto noise_sample;

	if(distance(&tLastSample, &next) < DELTA)
		goto noise_sample;

	if(distance(s, &next) < DELTA)
		goto valid_sample;

noise_sample:
	next1.x = (tslin.a[2] + tslin.a[0] *  s->x + tslin.a[1] * s->y) / tslin.a[6];
	next1.y = (tslin.a[5] + tslin.a[3] *  s->x + tslin.a[4] * s->y) / tslin.a[6];
	if(next1.y>=lcd_logic_max_y || next1.x>=lcd_logic_max_x) /* avoid RF interference */
	{
		return -1;
	}
	tLastSample = *s;
	return -1;

valid_sample:
	next1.x = (tslin.a[2] + tslin.a[0] *  s->x + tslin.a[1] * s->y) / tslin.a[6];
	next1.y = (tslin.a[5] + tslin.a[3] *  s->x + tslin.a[4] *  s->y) / tslin.a[6];
	if(next1.y>=lcd_logic_max_y || next1.x>=lcd_logic_max_x) /* avoid RF interference */
	{
		return -1;
	}		
    tLastSample = *s;
	return 1;
}

/* * * * * 
 *  ret : 
 *  0  - 	indicate no valid dot read 
 *  1  - 	indicate a valid dot read 
 *  2  - 	indicate that's the first dot 
 *  3  -    indicate ts release 
 * * * *  */  
int dejitter(TS_SAMPLE_T *s)
{
	int i,cnt,sumx=0,sumy=0,ret;

	ret = variance(s);
	if(ret == 2)
	{
		memset(tHistorySamples, 0, sizeof(tHistorySamples));
	 	return 3; 
	}
	if(ret <= 0) return 0; 
	
	if(tHistorySamples[0].pressure == 0)
	{
        tHistorySamples[0] = *s;
		return 2; /* first point */
	}

	cnt = sizeof(tHistorySamples) / sizeof(tHistorySamples[0]);
	for(i=cnt-1; i>0; i--)
	{
		tHistorySamples[i] = tHistorySamples[i-1];
	}
	tHistorySamples[0] = *s;

	for(i=0; i<cnt; i++)
	{
		if(tHistorySamples[i].pressure == 0) break;
		sumx += tHistorySamples[i].x;
		sumy += tHistorySamples[i].y;
	}

	s->x = sumx / i;
	s->y = sumy / i;
	return 1; /* pressing point */
}

/* * * * * 
 *  ret : 
 *  <=0 -   indicate no valid dot read 
 *  1   - 	indicate a valid dot read 
 *  2   - 	indicate that's the first dot 
 *  3   - 	indicate ts is release
 *  
 * * * *  */  
int ts_read(TS_SAMPLE_T *samp, int timeout)
{
	struct tslib_linear *lin = &tslin;
	int xtemp, ytemp, ret;
	uint done = GetTimerCount()+timeout;

	do {
		ret = dejitter(samp);
		if (ret <= 0) continue;  
		if (ret == 3) return 3;

		xtemp = samp->x;
		ytemp = samp->y;
		samp->x = (lin->a[2] + lin->a[0] * xtemp + lin->a[1] * ytemp) / lin->a[6];
		samp->y = (lin->a[5] + lin->a[3] * xtemp + lin->a[4] * ytemp) / lin->a[6];
		/* for discriminating the first release dot or no pression dot */
		samp->pressure = samp->pressure ? samp->pressure : 1;
		return ret;
	} while(done>GetTimerCount());

	return -1;
}

/* Touch screen calibration */
static int sort_by_x(const void* a, const void *b)
{
	return (((TS_SAMPLE_T *)a)->x - ((TS_SAMPLE_T *)b)->x);
}

static int sort_by_y(const void* a, const void *b)
{
	return (((TS_SAMPLE_T *)a)->y - ((TS_SAMPLE_T *)b)->y);
}

int getxy(int *x, int *y)
{
	TS_SAMPLE_T samp[MAX_SAMPLES];
	int middle, index=0;
	int iInvalidIdx=0, iValidIdx=0;
	const int INVALID_MAX=100, VALID_MAX=5;
	int ret;

	while(1)
	{
		do {
			ret = read_raw(&samp[0]);
			DelayUs(10);
			if(ret == 0) continue;
			if(0 == samp_valid(&samp[0]))
			{
				iValidIdx = 0;
				continue;
			}

			iValidIdx++;
			if(iValidIdx >= VALID_MAX) break;
		}while(1);

		/* Now collect up to MAX_SAMPLES touches into the samp array. */
		do {
			ret = read_raw(&samp[index]);
			DelayUs(10);
			if(ret == 0) continue;
			if(0 == samp_valid(&samp[index]))
			{	
				iInvalidIdx++;
				if(iInvalidIdx >= INVALID_MAX) break;
			}
			else
			{
				if(iInvalidIdx != 0)
					iInvalidIdx = 0;

				if(index < MAX_SAMPLES-1) index++;
			}

		}while(1);
		
		if(index > 50) break;
		else index = 0;
	}

	/*
	 * At this point, we have samples in indices zero to (index-1)
	 * which means that we have (index) number of samples.  We want
	 * to calculate the median of the samples so that wild outliers
	 * don't skew the result.  First off, let's assume that arrays
	 * are one-based instead of zero-based.  If this were the case
	 * and index was odd, we would need sample number ((index+1)/2)
	 * of a sorted array; if index was even, we would need the
	 * average of sample number (index/2) and sample number
	 * ((index/2)+1).  To turn this into something useful for the
	 * real world, we just need to subtract one off of the sample
	 * numbers.  So for when index is odd, we need sample number
	 * (((index+1)/2)-1).  Due to integer division truncation, we
	 * can simplify this to just (index/2).  When index is even, we
	 * need the average of sample number ((index/2)-1) and sample
	 * number (index/2).  Calculate (index/2) now and we'll handle
	 * the even odd stuff after we sort.
	 */
	middle = index/2;
	if(x) 
	{
		qsort(samp, index, sizeof(TS_SAMPLE_T), sort_by_x);
		if (index & 1)
			*x = samp[middle].x;
		else
			*x = (samp[middle-1].x + samp[middle].x) / 2;
	}
	if(y) 
	{
		qsort(samp, index, sizeof(TS_SAMPLE_T), sort_by_y);
		if (index & 1)
			*y = samp[middle].y;
		else
			*y = (samp[middle-1].y + samp[middle].y) / 2;
	}
	return 0;
	
}

int perform_calibration(tCALIBRATION *cal)
{
	int j;
	float n, x, y, x2, y2, xy, z, zx, zy;
	float det, a, b, c, e, f, i;
	float scaling = 65536.0;

	/* Get sums for matrix */
	n = x = y = x2 = y2 = xy = 0;
	for(j=0;j<5;j++) 
	{
		n += 1.0;
		x += (float)cal->x[j];
		y += (float)cal->y[j];
		x2 += (float)(cal->x[j]*cal->x[j]);
		y2 += (float)(cal->y[j]*cal->y[j]);
		xy += (float)(cal->x[j]*cal->y[j]);
	}

	/* Get determinant of matrix,check if determinant is too small */
	det = n*(x2*y2 - xy*xy) + x*(xy*y - x*y2) + y*(x*xy - y*x2);
	if(det < 0.1 && det > -0.1) 
		return 0;

	/* Get elements of inverse matrix */
	a = (x2*y2 - xy*xy)/det;
	b = (xy*y - x*y2)/det;
	c = (x*xy - y*x2)/det;
	e = (n*y2 - y*y)/det;
	f = (x*y - n*xy)/det;
	i = (n*x2 - x*x)/det;

	/* Get sums for x calibration */
	z = zx = zy = 0;
	for(j=0;j<5;j++) 
	{
		z += (float)cal->xfb[j];
		zx += (float)(cal->xfb[j]*cal->x[j]);
		zy += (float)(cal->xfb[j]*cal->y[j]);
	}

	/* Now multiply out to get the calibration for framebuffer x coord */
	cal->a[0] = (int)((a*z + b*zx + c*zy)*(scaling));
	cal->a[1] = (int)((b*z + e*zx + f*zy)*(scaling));
	cal->a[2] = (int)((c*z + f*zx + i*zy)*(scaling));

	/* Get sums for y calibration */
	z = zx = zy = 0;
	for(j=0;j<5;j++) 
	{
		z += (float)cal->yfb[j];
		zx += (float)(cal->yfb[j]*cal->x[j]);
		zy += (float)(cal->yfb[j]*cal->y[j]);
	}

	/* Now multiply out to get the calibration for framebuffer y coord */
	cal->a[3] = (int)((a*z + b*zx + c*zy)*(scaling));
	cal->a[4] = (int)((b*z + e*zx + f*zy)*(scaling));
	cal->a[5] = (int)((c*z + f*zx + i*zy)*(scaling));

	/* If we got here, we're OK, so assign scaling to a[6] and return */
	cal->a[6] = (int)scaling;
	return 1;
}

void put_cross(int x, int y, ushort rgb)
{
	if (WIDTH_FOR_S300_S900 == lcd_logic_max_x)
	{
		s_ScrRect(0, 40, lcd_logic_max_x-1, lcd_logic_max_y-1-60, COLOR_WHITE, 1);
	}
	else
	{
		s_ScrRect(0, 40, lcd_logic_max_x-1, lcd_logic_max_y-1, COLOR_WHITE, 1);
	}
	s_LcdDrawLine(x-8, y,   x+8, y,   rgb);
	s_LcdDrawLine(x,   y-8, x,   y+8, rgb);
}

static int get_sample(tCALIBRATION *cal, int index, int x, int y, char *name)
{
	static int last_x = -1, last_y;
	ushort cross_color = RGB(28, 91, 170);
	int dx=0,dy=0,i=0;

	if (last_x != -1) 
	{
		dx = ((x - last_x) << 16) / TS_NR_STEPS;
		dy = ((y - last_y) << 16) / TS_NR_STEPS;
		last_x <<= 16;
		last_y <<= 16;
		for (i = 0; i < TS_NR_STEPS; i++) 
		{
			put_cross (last_x >> 16, last_y >> 16, cross_color);
			put_cross (last_x >> 16, last_y >> 16, cross_color);
			last_x += dx;
			last_y += dy;
		}
	}
 
	put_cross(x, y, cross_color);
	if(getxy (&cal->x [index], &cal->y [index])) return 1;
	put_cross(x, y, cross_color);

	last_x = cal->xfb [index] = x;
	last_y = cal->yfb [index] = y;
	return 0;
}

#define ERROR_AREA 20
int calibration_check(tCALIBRATION *cal)
{
	int ret = 1;
	int tmpx, tmpy;

	tmpx = (cal->a[0] + cal->a[1]*cal->x[4] + cal->a[2] * cal->y[4])/cal->a[6];
	tmpy = (cal->a[3] + cal->a[4]*cal->x[4] + cal->a[5] * cal->y[4])/cal->a[6];
	if((sqr(tmpx - cal->xfb[4]) + sqr(tmpy - cal->yfb[4])) > ERROR_AREA) 
		ret = 0;

	tmpx = (cal->a[0] + cal->a[1]*cal->x[3] + cal->a[2] * cal->y[3])/cal->a[6];
	tmpy = (cal->a[3] + cal->a[4]*cal->x[3] + cal->a[5] * cal->y[3])/cal->a[6];
	if((sqr(tmpx - cal->xfb[3]) + sqr(tmpy - cal->yfb[3])) > ERROR_AREA)
		ret = 0;

	return ret;
}

static void IntegerToString(unsigned int integer, char *str)
{
	memset(str, '0', 10);
	do {
		if(integer == 0) break;
		*(str+9) = integer % 10 + '0';
		str--;
	} while(integer = integer / 10);
}

static void StringToInteger(char *str, int *integer)
{
	char num;
	int i;

	*integer = 0;
	
	for(i=0; i<10; i++)
	{
		num =  str[i] - '0';
		*integer = *integer*10 + num;
	}
}

int TsWriteSysPara(struct tslib_linear *pCal)
{
	char buff[71];
	int i;

	for(i=0; i<7; i++)
	{
		IntegerToString(pCal->a[i], &buff[i*10]);
	}

    buff[70] = '\0';

    if(SysParaWrite(SET_TS_CALIBRATION_PARA, buff) == 0) return 0;
	return -1;
}

int TsReadSysParam(struct tslib_linear *pCal)
{
	char buff[200];
	int i, ret;

	memset(buff, 0, sizeof(buff));

	ret = SysParaRead(SET_TS_CALIBRATION_PARA, buff);
	if(ret < 0) return -1;
	if(strlen(buff) != 70) return -2;

	for(i=0; i<strlen(buff); i++)
	{
		if(buff[i] < '0' || buff[i] > '9')
			return -3;
	}

	for(i=0; i<7; i++)
	{
		StringToInteger(&buff[i*10], &pCal->a[i]);
	}

	return 0;
}

void TsCalibrationParamInit()
{
	int a[7], ret;
	struct tslib_linear tTmpTsLib;
	if (GetTsType() == TS_TM035KBH08)
	{
		if(get_machine_type()== D210)
			memcpy(&tslin, &tslin_D210, sizeof(tslin));
		else
			memcpy(&tslin, &tslin_S300, sizeof(tslin));
	}
	else
	{
		memcpy(&tslin, &tslin_S500, sizeof(tslin));
	}
	ret = TsReadSysParam(&tTmpTsLib); 
	if(ret == 0) memcpy(tslin.a, tTmpTsLib.a, sizeof(tTmpTsLib.a));
}

extern uchar bIsChnFont;
extern TS_DEV_T gtTsDev;
int TSCalibration(void)
{
	tCALIBRATION cal;
	int i, xtart=0,ystart=0,xend=lcd_logic_max_x, yend=lcd_logic_max_y;
	const int DISTANCE=30;
	int ret;
	if (GetTsType() <= 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x61, "No Touch Screen!");
		getkey();
		return -10;
	}
	ts_timer_stop();
	ts_disable_irq();
	ts_clear_irq();
	ts_clear_buf();
    ts_spi_init();

	if (WIDTH_FOR_S300_S900 == lcd_logic_max_x)
	{
		xtart = 0;
		ystart = 40;
		xend = lcd_logic_max_x;
		yend = lcd_logic_max_y - 60;
	}
	else
	{
		xtart = 0;
		ystart = 40;
		xend = lcd_logic_max_x;
		yend = lcd_logic_max_y;
	}
	s_ScrRect(xtart, ystart, xend - 1, yend - 1, COLOR_WHITE, 1);

	get_sample (&cal, 0, xtart + DISTANCE,ystart + DISTANCE,  "Top left");
	get_sample (&cal, 1, xend - DISTANCE, ystart + DISTANCE,  "Top right");
	get_sample (&cal, 2, xend - DISTANCE, yend - DISTANCE,  "Bot right");
	get_sample (&cal, 3, xtart + DISTANCE,yend - DISTANCE,  "Bot left");
	get_sample (&cal, 4, (xtart+xend) / 2,(ystart+yend)/2,"Center");

	if(perform_calibration(&cal))
	{
		s_ScrRect(xtart, ystart, xend - 1, yend - 1, COLOR_WHITE, 1);
		if(calibration_check(&cal))
		{
			tslin.a[0] = cal.a[1];
			tslin.a[1] = cal.a[2];
			tslin.a[2] = cal.a[0];
			tslin.a[3] = cal.a[4];
			tslin.a[4] = cal.a[5];
			tslin.a[5] = cal.a[3];
			tslin.a[6] = cal.a[6];	

			TsWriteSysPara(&tslin);

    		ScrClrLine(2, 7);
            if(bIsChnFont)
            {
                ScrPrint(0,3,0x41,"校准成功");
            }
            else
            {
        		ScrPrint(0, 3, 0x41, " Calibration");
                ScrPrint(0, 5, 0x41, " Success!");
            }
			ret = 0;
		}
		else
		{
            ScrClrLine(2, 7);
            if(bIsChnFont)
            {
                ScrPrint(0,3,0x41,"校准失败");
            }
            else
            {
    			ScrPrint(0, 3, 0x41, " Calibration");
                ScrPrint(0, 5, 0x41, " Failed!");
            }
			ret = -1;
		}
	} 
    else
    {
        ScrClrLine(2, 7);
        if(bIsChnFont)
        {
            ScrPrint(0,3,0x41,"校准失败");
        }
        else
        {
            ScrPrint(0, 3, 0x41, " Calibration");
            ScrPrint(0, 5, 0x41, " Failed!");
        }
        ret = -2;
    }
    if(gtTsDev.isSKeySupported)ts_enable_irq();
    WaitKeyBExit(3,0);
	return ret;
}

