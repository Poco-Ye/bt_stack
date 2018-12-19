/*=============================================================================
* Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
*
* File	 : as3911_driver.c
*
* Author : WanLiShan	 
* 
* Date	 : 2012-10-10
*
* Description:
*
*
* History:
*
* =============================================================================
*/
#include "../../protocol/iso14443hw_hal.h"
#include "phhalHw_As3911_Reg.h"
#include "as3911_regs_conf.h"
#include <string.h>
#include <stdlib.h>


static unsigned char gucLastProtocol;
static unsigned int  guiLastSpeed;

static struct ST_As3911RegVal AS3911_ISO14443TYPEA_DEF[] = 
{
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEA_INI[] = 
{
	{ AS3911_REG_IO_CONF1, 0x0F },       /* Select 27.12 MHx Xtal */
	{ AS3911_REG_IO_CONF2, 0x00 },       /* 5V supply *//* change by wanls 2013.05.27 Deal with the amplitude is raised after open carrier wave*/
	{ AS3911_REG_OP_CONTROL, 0xF8 },     /* Enable oscillator and regulator, Enable Receive,Enable Tx,One channel enabled */
	{ AS3911_REG_MODE, 0x08 },           /* Initiator Operator modes 14443A */
	{ AS3911_REG_ISO14443A_NFC, 0x01 },  /* ISO14443A bit oriented anticollision frame is send */
	{ AS3911_REG_AUX, 0x00 },            /* Set OOK mode */
	{ AS3911_REG_RX_CONF1, 0x00 },       /* Enable AM channel */
	{ AS3911_REG_RX_CONF2, 0x12 },       /* AGC enable, Automatic squelch activation after end of Tx */
	{ AS3911_REG_RX_CONF3, 0xE0 },       /* Full gain */
	{ AS3911_REG_RX_CONF4, 0x80 },       /* Set gain reduction in second and third stage and digitizer */
	{ AS3911_REG_MASK_RX_TIMER, 0x0D },  /* Set mask receive time 6.5 etu */
	{ AS3911_REG_GPT_CONF, 0x00 },       /* No trigger source,start only with direct command (Start General Purpose Timer)*/
	{ AS3911_REG_NUM_TX_BYTES1, 0x00},
	{ AS3911_REG_NUM_TX_BYTES2, 0x00},


	{ AS3911_REG_ANT_CAL_CONF, 0x90 },   /* LC trim switch */

	//{ AS3911_REG_AM_MOD_DEPTH_CONF, 0x80 }, //0ld:0x10/* AM modulated level is defined by bits mod5 to mod0 */
	{ AS3911_REG_RFO_AM_ON_LEVEL, 0x00 },
	{ AS3911_REG_RFO_AM_OFF_LEVEL, 0x00 },
	{ AS3911_REG_FIELD_THRESHOLD, 0x33},

};

void As3911Iso14443TypeAInit( struct ST_PCDINFO* ptPcdInfo )
{
    int           i        = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( AS3911_ISO14443TYPEA_INI ) / sizeof( struct ST_As3911RegVal ) ); i++ )
    {
        ucRegVal = AS3911_ISO14443TYPEA_INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( AS3911_ISO14443TYPEA_INI[i].index, &ucRegVal, 1);
    }    
    
    return;
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEA_106KDEF[] = 
{
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEA_106K[] = 
{
	{ AS3911_REG_BIT_RATE, 0x00 },       /* 106kbps */
};
static struct ST_As3911RegVal AS3911_ISO14443TYPEA_212K[] = 
{
};
static struct ST_As3911RegVal AS3911_ISO14443TYPEA_424K[] = 
{
};
static struct ST_As3911RegVal AS3911_ISO14443TYPEA_848K[] = 
{
};

void As3911Iso14443TypeAConf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_As3911RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_106000:
        pst = AS3911_ISO14443TYPEA_106K;
        iCount = sizeof( AS3911_ISO14443TYPEA_106K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_212000:
        pst = AS3911_ISO14443TYPEA_212K;
        iCount = sizeof( AS3911_ISO14443TYPEA_212K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_424000:
        pst = AS3911_ISO14443TYPEA_424K;
        iCount = sizeof( AS3911_ISO14443TYPEA_424K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_848000:
        pst = AS3911_ISO14443TYPEA_848K;  
        iCount = sizeof( AS3911_ISO14443TYPEA_848K )/sizeof( struct ST_As3911RegVal );
    break;
    default:
        return;
    break;
    }
    for( i = 0; i < iCount; i++ )
    {
        ucRegVal = pst[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( pst[i].index, &ucRegVal, 1 ); 
    } 
    
    return;
}

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_DEF[] = 
{
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_INI[] = 
{
	{ AS3911_REG_IO_CONF1, 0x0F },          /* Select 27.12 MHx Xtal */
	{ AS3911_REG_IO_CONF2, 0x00},            /* 5V supply *//* change by wanls 2013.05.27 Deal with the amplitude is raised after open carrier wave*/
	{ AS3911_REG_OP_CONTROL, 0xF8 },        /* Enable oscillator and regulator,Enable Receive,Enable Tx,One channel enabled */
	{ AS3911_REG_MODE, 0x10 },              /* Initiator Operator modes 14443B */
	{ AS3911_REG_ISO14443A_NFC, 0x00 },     /* ISO14443A bit oriented anticollision frame is not send */
	{ AS3911_REG_ISO14443B_1, 0x00 },       /* SOF_0 is 10ETU, SOF_1 is 2ETU, EOF is 10 ETU*/
	{ AS3911_REG_AUX, 0x20 },               /* Set AM mode */
	{ AS3911_REG_RX_CONF1, 0x00 },          /* Enable AM channel */
	{ AS3911_REG_RX_CONF2, 0x12 },          /* AGC enable, Automatic squelch activation after end of Tx */
	{ AS3911_REG_RX_CONF3, 0xE0 },          /* Full gain */
	{ AS3911_REG_RX_CONF4, 0x80 },          /* Set gain reduction in second and third stage and digitizer */
	{ AS3911_REG_MASK_RX_TIMER, 0x0A },     /* Set mask receive time 7 etu */
	{ AS3911_REG_GPT_CONF, 0x00 },          /* No trigger source,start only with direct command (Start General Purpose Timer)*/

	{ AS3911_REG_NUM_TX_BYTES1, 0x00},
	{ AS3911_REG_NUM_TX_BYTES2, 0x00},


	{ AS3911_REG_ANT_CAL_CONF, 0x90 },      /* LC trim switch */

	//{ AS3911_REG_AM_MOD_DEPTH_CONF, 0x80},//0x80 }, //0ld:0x10/* AM modulated level is defined by bits mod5 to mod0 */
	{ AS3911_REG_RFO_AM_ON_LEVEL, 0x00 },
	{ AS3911_REG_RFO_AM_OFF_LEVEL, 0x00 },
	{ AS3911_REG_ISO14443B_2, 0x40},
	{ AS3911_REG_FIELD_THRESHOLD, 0x33},
};

void As3911Iso14443TypeBInit( struct ST_PCDINFO* ptPcdInfo )
{
    int           i      = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( AS3911_ISO14443TYPEB_INI ) / sizeof( struct ST_As3911RegVal ) ); i++ )
    {
        ucRegVal = AS3911_ISO14443TYPEB_INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( AS3911_ISO14443TYPEB_INI[i].index, &ucRegVal, 1); 
    } 
    
    return;   
}

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_106KDEF[] = 
{
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_106K[] = 
{
	{ AS3911_REG_BIT_RATE, 0x00 },       /* 106kbps */
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_212K[] = 
{
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_424K[] = 
{
};

static struct ST_As3911RegVal AS3911_ISO14443TYPEB_848K[] = 
{
};

void As3911Iso14443TypeBConf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_As3911RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_106000:
        pst = AS3911_ISO14443TYPEB_106K;
        iCount = sizeof( AS3911_ISO14443TYPEB_106K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_212000:
        pst = AS3911_ISO14443TYPEB_212K;
        iCount = sizeof( AS3911_ISO14443TYPEB_212K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_424000:
        pst = AS3911_ISO14443TYPEB_424K;
        iCount = sizeof( AS3911_ISO14443TYPEB_424K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_848000:
        pst = AS3911_ISO14443TYPEB_848K;  
        iCount = sizeof( AS3911_ISO14443TYPEB_848K )/sizeof( struct ST_As3911RegVal );
    break;
    default:
        return;
    break;
    }
    for( i = 0; i < iCount; i++ )
    {
        ucRegVal = pst[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( pst[i].index, &ucRegVal, 1 ); 
    } 
    
    return;
}

static struct ST_As3911RegVal AS3911_JISX6319_4DEF[] = 
{
};

static struct ST_As3911RegVal AS3911_JISX6319_4INI[] = 
{
	{ AS3911_REG_IO_CONF1, 0x0F },          /* Select 27.12 MHx Xtal */
	{ AS3911_REG_IO_CONF2, 0x00 },          /* 5V supply *//* change by wanls 2013.05.27 Deal with the amplitude is raised after open carrier wave*/
	{ AS3911_REG_OP_CONTROL, 0xF8 },        /* Enable oscillator and regulator,Enable Receive,Enable Tx,One channel enabled */
	{ AS3911_REG_MODE, 0x18 },              /* Initiator Operator modes FeliCa */
	{ AS3911_REG_ISO14443A_NFC, 0x80 },     /* ISO14443A bit oriented anticollision frame is not send */
	{ AS3911_REG_AUX, 0x60 },               /* Set AM mode, Make CRC check and put CRC bytes in fifo and add them to number of receive bytes */
	{ AS3911_REG_RX_CONF1, 0x00 },          /* Enable AM channel */
	{ AS3911_REG_RX_CONF2, 0x12 },          /* AGC enable, Automatic squelch activation after end of Tx */
	{ AS3911_REG_RX_CONF3, 0x00 },          /* Full gain */
	{ AS3911_REG_RX_CONF4, 0x00 },          /* Set gain reduction in second and third stage and digitizer */
	{ AS3911_REG_MASK_RX_TIMER, 0x0E },     /* Set mask receive time 7 etu */
	{ AS3911_REG_GPT_CONF, 0x00 },          /* No trigger source,start only with direct command (Start General Purpose Timer)*/

	{ AS3911_REG_NUM_TX_BYTES1, 0x00},
	{ AS3911_REG_NUM_TX_BYTES2, 0x00},


	{ AS3911_REG_ANT_CAL_CONF, 0x90 },      /* LC trim switch */

	//{ AS3911_REG_AM_MOD_DEPTH_CONF, 0x40 }, /* AM modulated level 20% */
	//{ AS3911_REG_RFO_AM_ON_LEVEL, 0x00 },
	{ AS3911_REG_AM_MOD_DEPTH_CONF, 0x80 }, //0ld:0x10/* AM modulated level is defined by bits mod5 to mod0 */
	{ AS3911_REG_RFO_AM_ON_LEVEL, 0x00 },
	{ AS3911_REG_RFO_AM_OFF_LEVEL, 0x00 },
	{ AS3911_REG_FIELD_THRESHOLD, 0x33},
};

void As3911Jisx6319_4Init( struct ST_PCDINFO* ptPcdInfo )
{
    int           i      = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( AS3911_JISX6319_4INI ) / sizeof( struct ST_As3911RegVal ) ); i++ )
    {
        ucRegVal = AS3911_JISX6319_4INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( AS3911_JISX6319_4INI[i].index, &ucRegVal, 1); 
    }
    
    return;
}

static struct ST_As3911RegVal AS3911_JISX6319_4_212K[] = 
{
	{ AS3911_REG_BIT_RATE, 0x11 },       /* 212kbps */
};

static struct ST_As3911RegVal AS3911_JISX6319_4_424K[] = 
{
	{ AS3911_REG_BIT_RATE, 0x22 },       /* 424kbps */
};

void As3911Jisx6319_4Conf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_As3911RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_212000:
        pst = AS3911_JISX6319_4_212K;
        iCount = sizeof( AS3911_JISX6319_4_212K )/sizeof( struct ST_As3911RegVal );
    break;
    case BAUDRATE_424000:
        pst = AS3911_JISX6319_4_424K;
        iCount = sizeof( AS3911_JISX6319_4_424K )/sizeof( struct ST_As3911RegVal );
    break;
    default:
        return;
    break;
    }
    for( i = 0; i < iCount; i++ )
    {
        ucRegVal = pst[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( pst[i].index, &ucRegVal, 1 ); 
    } 
    
    return;
}


int As3911SetDefaultConfigVal( int iType )
{
    int                    iCount;
    struct ST_As3911RegVal* pst1;
    struct ST_As3911RegVal* pst2;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEA_INI );
        pst1 = AS3911_ISO14443TYPEA_INI;
        pst2 = AS3911_ISO14443TYPEA_DEF;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEB_INI );
        pst1 = AS3911_ISO14443TYPEB_INI;
        pst2 = AS3911_ISO14443TYPEB_DEF;
    }
	else
	{
		return -2;
	}
    memcpy( pst1, pst2, iCount );

    /*setting 106Kbps*/
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEA_106K );
        pst1 = AS3911_ISO14443TYPEA_106K;
        pst2 = AS3911_ISO14443TYPEA_106KDEF;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEB_106K );
        pst1 = AS3911_ISO14443TYPEB_106K;
        pst2 = AS3911_ISO14443TYPEB_106KDEF;
    }
    else 
    {
        return -2;
    }
    memcpy( pst1, pst2, iCount );
    
    return;
}


int As3911SetRegisterConfigVal( int iType, unsigned char ucReg, unsigned char ucVal )
{
    int                    i = 0;
    int                    iCount;
    struct ST_As3911RegVal* pst;
    int                    Ret = -1;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEA_INI ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEA_INI;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEB_INI ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEB_INI;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_JISX6319_4INI ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_JISX6319_4INI;   
    }
    else 
    {
        return -2;
    }
    
    for( i = 0; i < iCount; i++ )
    {
        if( pst[i].index == ucReg )
        {
            pst[i].value = ucVal;
            Ret = 0;
        }
    }
    
    /*setting 106Kbps*/
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEA_106K ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEA_106K;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEB_106K ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEB_106K;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_JISX6319_4_212K ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_JISX6319_4_212K;
    }
    else 
    {
        return -2;
    }
    
    for( i = 0; i < iCount; i++ )
    {
        if( pst[i].index == ucReg )
        {
            pst[i].value = ucVal;
            Ret = 0;
        }
    }
    
    return Ret;
}

int As3911GetRegisterConfigVal( int iType, unsigned char ucReg )
{
    int                    i = 0;
    int                    iCount;
    struct ST_As3911RegVal* pst;
    int                    Ret = -1;
    unsigned char          ucVal;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEA_INI ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEA_INI;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEB_INI ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEB_INI;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_JISX6319_4INI ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_JISX6319_4INI;   
    }
    else 
    {
        return -2;
    }
    
    for( i = 0; i < iCount; i++ )
    {
        if( pst[i].index == ucReg )
        {
            ucVal = pst[i].value;
            Ret = 0;
        }
    }
    
    /*setting 106Kbps*/
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEA_106K ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEA_106K;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_ISO14443TYPEB_106K ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_ISO14443TYPEB_106K;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( AS3911_JISX6319_4_212K ) / sizeof( struct ST_As3911RegVal );
        pst = AS3911_JISX6319_4_212K;
    }
    else 
    {
        return -2;
    }
    
    for( i = 0; i < iCount; i++ )
    {
        if( pst[i].index == ucReg )
        {
            ucVal = pst[i].value;
            Ret = 0;
        }
    }
    
    return ( Ret == 0 ) ? ucVal : Ret;
}

