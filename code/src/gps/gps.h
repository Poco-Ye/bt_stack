#ifndef __GPS_H__
#define __GPS_H__

#include "minmea.h"

#define GPS_RET_OK					(0)
#define GPS_RET_NAVIGATING			(-1)
#define GPS_RET_ERROR_NODEV			(-2)
#define GPS_RET_ERROR_NORESPONSE	(-3)
#define GPS_RET_ERROR_NOTOPEN		(-4)
#define GPS_RET_ERROR_NULL			(-5)
#define GPS_RET_ERROR_PARAMERROR	(-6)
#define GPS_RET_ERROR_NAVIGATE_FAIL	(-7)

typedef struct {
    double latitude;
    double longitude;
    double altitude;
} GpsLocation;

typedef struct {
	struct minmea_sentence_rmc rmc;
	struct minmea_sentence_gga gga;
	struct minmea_sentence_gll gll;
	struct minmea_sentence_gst gst;
	struct minmea_sentence_gsa gsa;
	struct minmea_sentence_gsv gsv;
	struct minmea_sentence_vtg vtg;
	struct minmea_sentence_txt txt;
}GpsInfoOpt;

int GpsOpen(void);
int GpsRead(GpsLocation *outGpsLoc);
void GpsClose(void);

#endif

