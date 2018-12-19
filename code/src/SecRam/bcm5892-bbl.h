/* $Id: bcm5892-bbl.h 4560 2012-01-06 20:27:16Z danfe $ */

#ifndef _INCLUDE_BBL_CPUAPI
#define _INCLUDE_BBL_CPUAPI

#define BBL_START_SCRUB		0x3
#define BBL_CLEAR_2KRAM		0x5
#define BBL_READ_256B_MEM	0x9
#define BBL_WRITE_256B_MEM	0x49
#define BBL_READ_2KRAM		0x11
#define BBL_WRITE_2KRAM		0x51
#define BBL_READ_INDREG		0x21
#define BBL_WRITE_INDREG	0x61
#define BBL_SOFT_RST_BBL	0x81

#define BBL_STATUS_OK		0
/* clock period in nanoseconds (exact value is 30518) */
#define BBL_32KHZ_PERIOD	320


#define EX_SENSOR_BIT		(1 << 0)
#define STATIC_SENSOR_BIT	(1 << 1)
#define TEMPER_SENSOR_BIT	(1 << 2)
#define FREQ_SENSOR_BIT		(1 << 3)
#define VOLTAGE_SENSOR_BIT	(1 << 4)

#define Dync_SENSOR1_BIT	(1 << 16)
#define Dync_SENSOR2_BIT	(1 << 17)
#define Dync_SENSOR3_BIT	(1 << 18)
#define Dync_SENSOR4_BIT	(1 << 19)
#define Dync_SENSOR_ALL	    (0xF << 16)
#define SENSOR_Switch_CONFIG (Dync_SENSOR_ALL|EX_SENSOR_BIT|STATIC_SENSOR_BIT)

#endif /* _INCLUDE_BBL_CPUAPI */
