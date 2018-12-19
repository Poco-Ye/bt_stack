#include "base.h"
#include "../comm/comm.h"
#include "gps.h"

//#define __GPS_DEBUG__
#ifdef __GPS_DEBUG__
#define GpsDebugPrintf	iDPrintf
#else
#define GpsDebugPrintf
#endif

#define GPS_READ_GGA
//#define GPS_READ_GLL
#define GPS_READ_RMC
//#define GPS_READ_GSA
//#define GPS_READ_GST
//#define GPS_READ_GSV

typedef struct{
	int pwr_port;
	int pwr_pin;
}T_GPSDrvCfg;

static GpsInfoOpt gtGpsInfoOpt;
static GpsInfoOpt *gpGpsInfoOpt = &gtGpsInfoOpt;

static int giGpsPowerState = 0;
//SXXX中现在仅S900支持gps
T_GPSDrvCfg SxxxGPSCfg = {
	.pwr_port = GPIOB,
	.pwr_pin = 7,
};
T_GPSDrvCfg S900V3GPSCfg = {
	.pwr_port = GPIOD,
	.pwr_pin = 16,
};
T_GPSDrvCfg *pGPSDrvCfg = NULL;
#define GPS_POWER_PORT		pGPSDrvCfg->pwr_port
#define GPS_POWER_PIN		pGPSDrvCfg->pwr_pin


static int get_gps_module_type(void)
{
	char context[64];
	static int type = -1;

	if (type >= 0) return type; 
	memset(context,0,sizeof(context));
	if(ReadCfgInfo("GPS",context)<=0) type = 0;
	else if(strstr(context, "01")) type = 1;
	else if(strstr(context, "02")) type = 2;
    else type =0;
    
	return type;
}

int isGpsPowerOn(void)
{
	return (giGpsPowerState == 1);
}

void s_GpsPowerOn(void)
{
	gpio_set_pin_type(GPS_POWER_PORT, GPS_POWER_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(GPS_POWER_PORT, GPS_POWER_PIN, 0);
	giGpsPowerState = 1;
}

void s_GpsPowerOff(void)
{
	gpio_set_pin_type(GPS_POWER_PORT, GPS_POWER_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(GPS_POWER_PORT, GPS_POWER_PIN, 1);
	giGpsPowerState = 0;
}

int s_GpsRead(GpsLocation *outGpsLoc)
{
	char line[MINMEA_MAX_LENGTH];
	uint state_RMC = 0,state_GGA = 0,state_GLL = 0; 
	GpsInfoOpt *pGpsLoc = gpGpsInfoOpt;
	GpsLocation GpsLoc;

    memset(&GpsLoc, 0x00, sizeof(GpsLocation));

#ifdef GPS_READ_RMC
	if(pGpsLoc->rmc.valid == 1)
	{
		GpsLoc.latitude = (double)minmea_tocoord(&pGpsLoc->rmc.latitude);
		GpsLoc.longitude = (double)minmea_tocoord(&pGpsLoc->rmc.longitude);
		state_RMC = 1;
	}
#endif

#ifdef GPS_READ_GGA
	if(pGpsLoc->gga.satellites_tracked >= 3 && pGpsLoc->gga.fix_quality != 0)
	{
	    if(state_RMC==0)
		{
		    GpsLoc.latitude = (double)minmea_tocoord(&pGpsLoc->gga.latitude);
		    GpsLoc.longitude = (double)minmea_tocoord(&pGpsLoc->gga.longitude);
	    }
	    
		GpsLoc.altitude = (double)minmea_tofloat(&pGpsLoc->gga.altitude);
		state_GGA = 1;
	}
#endif

#ifdef GPS_READ_GLL
	if(pGpsLoc->gll.status == 'A')
	{
		GpsLoc.latitude = (double)minmea_tocoord(&pGpsLoc->gll.latitude);
		GpsLoc.longitude = (double)minmea_tocoord(&pGpsLoc->gll.longitude);
		state_GLL = 1;
	}
#endif
	if(state_RMC == 1 || state_GGA == 1 || state_GLL == 1)
	{
	    memcpy(outGpsLoc, &GpsLoc, sizeof(GpsLocation));	    
	    return GPS_RET_OK;
	}    

	return GPS_RET_NAVIGATING;
}
/* s_softTimer handle */
void s_GpsTimerDone(void)
{
	uint flag;
	static int iCnt = 0;
	GpsInfoOpt *gpsOpt;
	int i, iRet, len;
	uchar tmpLine[MINMEA_MAX_LENGTH],xxxLine[MINMEA_MAX_LENGTH];

	if(!SystemInitOver()) return ;			
	if(get_gps_module_type() == 0) return ;
	if(!isGpsPowerOn())	return ;		
	if(iCnt++ < 10)	return ;

	iCnt = 0;
	memset(tmpLine, 0x00, sizeof(tmpLine));
	iRet = PortPeep(P_GPS, tmpLine, sizeof(tmpLine));
	for(i = 0; i < iRet-1; i++)
	{
		if(tmpLine[i] == 0x0D && tmpLine[i+1] == 0x0A) break;/* \r & \n */		
	}
	if(i >= iRet-1)
	{
		// FixMe: Error gps module info, reset com port
		PortReset(P_GPS);
		return ;
	}

	memset(xxxLine, 0x00, sizeof(xxxLine));
	iRet = PortRecvs(P_GPS, xxxLine, i+2, 0);
	if(iRet <= 0)
	{
		// FixMe: unlikely
		PortReset(P_GPS);
		return ;
	}

	gpsOpt = gpGpsInfoOpt;
	switch(minmea_sentence_id(xxxLine))
	{
		case MINMEA_SENTENCE_RMC:
			#ifdef GPS_READ_RMC
			minmea_parse_rmc(&gpsOpt->rmc, xxxLine);
			#endif
		break;

		case MINMEA_SENTENCE_GGA:
			#ifdef GPS_READ_GGA
			minmea_parse_gga(&gpsOpt->gga, xxxLine);
			#endif
		break;
		
		case MINMEA_SENTENCE_GLL:
			#ifdef GPS_READ_GLL
			minmea_parse_gll(&gpsOpt->gll, xxxLine);
			#endif
		break;

		case MINMEA_SENTENCE_GSA:
			#ifdef GPS_READ_GSA
			minmea_parse_gsa(&gpsOpt->gsa, xxxLine);
			#endif
		break;
			
		case MINMEA_SENTENCE_GST:
			#ifdef GPS_READ_GST
			minmea_parse_gst(&gpsOpt->gst, xxxLine);
			#endif
		break;
			
		case MINMEA_SENTENCE_GSV:
			#ifdef GPS_READ_GSV
			minmea_parse_gsv(&gpsOpt->gsv, xxxLine);
			#endif
		break;
			
		case MINMEA_INVALID:
			break;
		default:
			break;
	}
}

int GpsOpen(void)
{
	uchar ucRet;

	if(get_gps_module_type() == 0) return GPS_RET_ERROR_NODEV;
	
	if(get_machine_type() == S900 && GetHWBranch() == S900HW_V3x)
	{
		pGPSDrvCfg = &S900V3GPSCfg;
	}
	else
	{
		pGPSDrvCfg = &SxxxGPSCfg;
	}
	
	if(isGpsPowerOn()) return GPS_RET_OK;
	ucRet = PortOpen(P_GPS, "9600,8,n,1");
	if(ucRet != 0) return GPS_RET_ERROR_NODEV;
	
	memset(gpGpsInfoOpt, 0x00, sizeof(GpsInfoOpt));	
	s_GpsPowerOn();
	return GPS_RET_OK;
}

void GpsClose(void)
{
	if(get_gps_module_type() == 0) return;
	if(!isGpsPowerOn()) return;
	
	s_GpsPowerOff();
	PortClose(P_GPS);

	return;
}

int GpsRead(GpsLocation *outGpsLoc)
{
    int ret;
	if(get_gps_module_type() == 0) return GPS_RET_ERROR_NODEV;
	if(!isGpsPowerOn()) return GPS_RET_ERROR_NOTOPEN;
	if(outGpsLoc == NULL) return GPS_RET_ERROR_NULL;

	ret = s_GpsRead(outGpsLoc);

	return  ret;
}

#if 0
// TODO: for debug info
void gpsShowDebugInfoRMC(struct minmea_sentence_rmc *frame)
{
	//GpsDebugPrintf("**** RMC - Recommended Minimum Specific GNSS Data ****\r\n");
	GpsDebugPrintf("valid : %d\r\n", frame->valid);
	GpsDebugPrintf("Date: 20%d-%d-%d , UTC Position: %d:%d:%d:%d\r\n", 
		frame->date.year, 
		frame->date.month, 
		frame->date.day,
		frame->time.hours, 
		frame->time.minutes, 
		frame->time.seconds, 
		frame->time.microseconds);
	GpsDebugPrintf("latitude: %d/%d, longtitude: %d/%d\r\n", 
		frame->latitude.value, 
		frame->latitude.scale,
		frame->longitude.value, 
		frame->longitude.scale);
	GpsDebugPrintf("speed: %d/%d, course: %d/%d\r\n", 
		frame->speed.value, 
		frame->speed.scale,
		frame->course.value, 
		frame->course.scale);
	GpsDebugPrintf("Magnetic variation: %d/%d\r\n", 
		frame->variation.value, 
		frame->variation.scale);
}
void gpsShowDebugInfoGLL(struct minmea_sentence_gll *frame)
{
	//GpsDebugPrintf("**** GLL - Geographic Position - Latitude/Longitude ****\r\n");
	GpsDebugPrintf("status: %02x, mode: %02x\r\n",
		frame->status, frame->mode);
	GpsDebugPrintf("latitude: %d/%d, longtitude: %d/%d\r\n", 
		frame->latitude.value,
		frame->latitude.scale,
		frame->longitude.value,
		frame->longitude.scale);
	GpsDebugPrintf("UTC position: %d:%d:%d:%d\r\n",
		frame->time.hours,
		frame->time.minutes, 
		frame->time.seconds,
		frame->time.microseconds);
}
void gpsShowDebugInfoGGA(struct minmea_sentence_gga *frame)
{
	//GpsDebugPrintf("**** GGA - Global Positioning System Fixed Data ****\r\n");
	GpsDebugPrintf("UTC position: %d:%d:%d:%d\r\n",
		frame->time.hours,
		frame->time.minutes, 
		frame->time.seconds,
		frame->time.microseconds);
	GpsDebugPrintf("fix_quality: %d, satellites_tracked: %d\r\n",
		frame->fix_quality,
		frame->satellites_tracked);
	GpsDebugPrintf("latitude: %d/%d, longtitude: %d/%d\r\n", 
		frame->latitude.value,
		frame->latitude.scale,
		frame->longitude.value,
		frame->longitude.scale);
	GpsDebugPrintf("altitude(%02x): %d/%d, height(%02x): %d/%d\r\n", 
		frame->altitude_units,
		frame->altitude.value,
		frame->altitude.scale,
		frame->height_units,
		frame->height.value,
		frame->height.scale);
	GpsDebugPrintf("hdop: %d/%d, dgps_age: %d\r\n",
		frame->hdop.value,
		frame->hdop.scale,
		frame->dgps_age);
}
void gpsShowDebugInfoGSA(struct minmea_sentence_gsa *frame)
{
}
void gpsShowDebugInfoGST(struct minmea_sentence_gst *frame)
{
}
void gpsShowDebugInfoGSV(struct minmea_sentence_gsv *frame)
{
}
#endif
/* `@_@` */

