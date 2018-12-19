
#include "../../protocol/iso14443hw_hal.h"

#include "pn512_regs.h"

#include "pn512_regs_conf.h"
#include <stdlib.h>
#include <string.h>

/**
 * =============================================================================
 * Typical setting for using the PN512 for ISO/IEC14443 iType A
 * These Values may differ dependent on the design of the Antenna and used interface
 */
static struct ST_Pn512RegVal PN512_ISO14443TYPEA_DEF[] = 
{

};

static struct ST_Pn512RegVal PN512_ISO14443TYPEA_INI[] = 
{
	{ PN512_TXMODE_REG,      0x80 }, /* ISO14443A, 106kBits/s, TxCRCEn */
	{ PN512_RXMODE_REG,      0x80 }, /* ISO14443A, 106KBits/s, RxCRCEn*/
	{ PN512_TXAUTO_REG,      0x40 }, /* Enable Force100%ASK */
	{ PN512_RXSEL_REG,       0x88 }, /* RxWait = 8, Modulation signal from the internal analog part */
	{ PN512_RXTHRESHOLD_REG, 0x7F }, /* RxThresholdReg register */
	{ PN512_CWGSP_REG,       0x3F }, /* Defines the conductance of the P-driver during times of no modulation */
	{ PN512_MODE_REG,        0x39 }, /* 6363 CRC Preset */
	{ PN512_MODWIDTH_REG,    0x26 }, /* #clocksLOW  = (ModWidth % 8)+1.#clocksHIGH = 16 - #clocksLOW. */
	{ PN512_TYPEB_REG,       0x00 }, 
	{ PN512_MODGSP_REG,      0x06 }, /* Defines the driver P-output conductance during modulation */
    { PN512_RFCFG_REG,       0x48 }, /* Configures the receiver gain and RF level detector sensitivity. */
    { PN512_COLL_REG,        0x80 },
	{ PN512_WATERLEVEL_REG,  0x10 },  /* Defines the level for FIFO under- and overflow warning is 16 Bytes */
    { PN512_TXCONTROL_REG,   0xA3 } /* Open Carrier */
};

void Pn512Iso14443TypeAInit( struct ST_PCDINFO* ptPcdInfo )
{
    int           i        = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( PN512_ISO14443TYPEA_INI ) / sizeof( struct ST_Pn512RegVal ) ); i++ )
    {
        ucRegVal = PN512_ISO14443TYPEA_INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_ISO14443TYPEA_INI[i].index,
                            &ucRegVal, 
                            1 ); 
    }    
    
    return;
}

/**
 * Typical setting for speed
 */
static struct ST_Pn512RegVal PN512_ISO14443TYPEA_106KDEF[] = 
{
   
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEA_106K[] = 
{
	//{ PN512_TXMODE_REG,      0x80 }, /* ISO14443A, 106kBits/s, TxCRCEn */
	//{ PN512_RXMODE_REG,      0x80 }  /* ISO14443A, 106KBits/s, RxCRCEn, RxNoErr */
   
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEA_212K[] = 
{
   
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEA_424K[] = 
{
   
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEA_848K[] = 
{

};

void Pn512Iso14443TypeAConf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_Pn512RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_106000:
        pst = PN512_ISO14443TYPEA_106K;
        iCount = sizeof( PN512_ISO14443TYPEA_106K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_212000:
        pst = PN512_ISO14443TYPEA_212K;
        iCount = sizeof( PN512_ISO14443TYPEA_212K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_424000:
        pst = PN512_ISO14443TYPEA_424K;
        iCount = sizeof( PN512_ISO14443TYPEA_424K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_848000:
        pst = PN512_ISO14443TYPEA_848K;  
        iCount = sizeof( PN512_ISO14443TYPEA_848K )/sizeof( struct ST_Pn512RegVal );
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

/**
 * =============================================================================
 * Typical setting for using the CLPN512 for ISO/IEC14443 iType B
 * These Values may differ dependent on the design of the Antenna and used interface
 */
static struct ST_Pn512RegVal PN512_ISO14443TYPEB_DEF[] = 
{

};

static struct ST_Pn512RegVal PN512_ISO14443TYPEB_INI[] = 
{
	{ PN512_TXMODE_REG,      0x83 }, /* ISO14443B, 106kBits/s, TxCRCEn */
	{ PN512_RXMODE_REG,      0x83 }, /* ISO14443B, 106KBits/s, RxCRCEn, RxNoErr */
	{ PN512_TXAUTO_REG,      0x00 }, /* Disable  Force100%ASK */
	{ PN512_RXSEL_REG,       0x88 }, /* RxWait = 8*/
	{ PN512_RXTHRESHOLD_REG, 0x7F }, /* RxThresholdReg register */
	{ PN512_CWGSP_REG,       0x3F }, /* Defines the conductance of the P-driver during times of no modulation */
	{ PN512_MODE_REG,        0x3B }, /* CRC "FFFF" Preset */
	{ PN512_TYPEB_REG,       0x01 }, /* TYPEB: RxSOF, RxEOF, TxSOF, TxEOF, TxEGT = 1(2ETU), SOF Width = 11ETU */
	{ PN512_MODGSP_REG,      0x06 }, /* Defines the driver P-output conductance during modulation */
	{ PN512_MODWIDTH_REG,    0x26 }, /* #clocksLOW  = (ModWidth % 8)+1.#clocksHIGH = 16 - #clocksLOW.*/
	{ PN512_BITFRAMING_REG,  0x00 }, /* set TxLastBits to 0 */
	{ PN512_COLL_REG,        0x80 }, /* Defines the first bit collision detected on the RF interface. */
	{ PN512_RFCFG_REG,       0x48 },  /* Configures the receiver gain and RF level detector sensitivity. */
	{ PN512_WATERLEVEL_REG,  0x10 },  /* Defines the level for FIFO under- and overflow warning is 16 Bytes */
	{ PN512_TXCONTROL_REG,   0xA3 } /* Open Carrier */

};

void Pn512Iso14443TypeBInit( struct ST_PCDINFO* ptPcdInfo )
{
    int           i      = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( PN512_ISO14443TYPEB_INI ) / sizeof( struct ST_Pn512RegVal ) ); i++ )
    {
        ucRegVal = PN512_ISO14443TYPEB_INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_ISO14443TYPEB_INI[i].index,
                            &ucRegVal, 
                            1 ); 
    } 
    
    return;   
}

static struct ST_Pn512RegVal PN512_ISO14443TYPEB_106KDEF[] = 
{
 
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEB_106K[] = 
{
   	//{ PN512_TXMODE_REG,      0x83 }, /* ISO14443B, 106kBits/s, TxCRCEn */
	//{ PN512_RXMODE_REG,      0x83 }, /* ISO14443B, 106KBits/s, RxCRCEn, RxNoErr */
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEB_212K[] = 
{
   
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEB_424K[] = 
{
 
};

static struct ST_Pn512RegVal PN512_ISO14443TYPEB_848K[] = 
{
   
};

void Pn512Iso14443TypeBConf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_Pn512RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_106000:
        pst = PN512_ISO14443TYPEB_106K;
        iCount = sizeof( PN512_ISO14443TYPEB_106K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_212000:
        pst = PN512_ISO14443TYPEB_212K;
        iCount = sizeof( PN512_ISO14443TYPEB_212K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_424000:
        pst = PN512_ISO14443TYPEB_424K;
        iCount = sizeof( PN512_ISO14443TYPEB_424K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_848000:
        pst = PN512_ISO14443TYPEB_848K;  
        iCount = sizeof( PN512_ISO14443TYPEB_848K )/sizeof( struct ST_Pn512RegVal );
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

/**
 * =============================================================================
 * Typical setting for using the CLPN512 for JIS X 6319-4
 * These Values may differ dependent on the design of the Antenna and used interface
 */
static struct ST_Pn512RegVal PN512_JISX6319_4DEF[] = 
{
    /*transmitter*/
  
    /*receiver*/
 
};

static struct ST_Pn512RegVal PN512_JISX6319_4INI[] = 
{
	{ PN512_BITFRAMING_REG, 0x00 }, /* set TxLastBits to 0 */
	{ PN512_MODE_REG,       0xB8 }, /* MSBFirst */
	{ PN512_TXAUTO_REG,     0x00 }, /* Disable  Force100%ASK */
	{ PN512_RXSEL_REG,      0x88 }, /* RxWait = 8, Modulation signal from the internal analog part */
	{ PN512_RXTHRESHOLD_REG,0x7F }, /* RxThresholdReg register */
	{ PN512_CWGSP_REG,      0x3F }, /* Defines the conductance of the P-driver during times of no modulation */
	{ PN512_DEMOD_REG,      0x4D }, /* Defines demodulator settings. Set Default value */
	{ PN512_FELNFC1_REG,    0x00 }, /* Defines FeliCa Sync bytes and the minimum length of the receivedpacket.*/
	{ PN512_FELNFC2_REG,    0x00 }, /* Defines the maximum length of the received packet.Set data length is 256 bytes*/
	{ PN512_RFCFG_REG,      0x7F }, /* Configures the receiver gain and RF level detector sensitivity. */
	{ PN512_MODGSP_REG,     0x08 }, /* Defines the driver P-output conductance during modulation */
	{ PN512_TYPEB_REG,      0x00 },
	{ PN512_MODWIDTH_REG,   0x26 },
	{ PN512_WATERLEVEL_REG, 0x10 }, /* Defines the level for FIFO under- and overflow warning is 16 Bytes */
	{ PN512_TXCONTROL_REG,  0xA3 }  /* Open Carrier */
};

void Pn512Jisx6319_4Init( struct ST_PCDINFO* ptPcdInfo )
{
    int           i      = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( PN512_JISX6319_4INI ) / sizeof( struct ST_Pn512RegVal ) ); i++ )
    {
        ucRegVal = PN512_JISX6319_4INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_JISX6319_4INI[i].index,
                            &ucRegVal, 
                            1 ); 
    }
    
    return;
}

static struct ST_Pn512RegVal PN512_JISX6319_4_212K[] = 
{
	{ PN512_TXMODE_REG,     0x92 }, /* FeliCa, 212kBits/s, TxCRCEn */
	{ PN512_RXMODE_REG,     0x9A }, /* FeliCa, 212KBits/s, RxCRCEn, RxNoErr */
   
};

static struct ST_Pn512RegVal PN512_JISX6319_4_424K[] = 
{
	{ PN512_TXMODE_REG,     0xA2 }, /* FeliCa, 424kBits/s, TxCRCEn */
	{ PN512_RXMODE_REG,     0xAA }, /* FeliCa, 424KBits/s, RxCRCEn, RxNoErr */
  
};

void Pn512Jisx6319_4Conf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_Pn512RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_212000:
        pst = PN512_JISX6319_4_212K;
        iCount = sizeof( PN512_JISX6319_4_212K )/sizeof( struct ST_Pn512RegVal );
    break;
    case BAUDRATE_424000:
        pst = PN512_JISX6319_4_424K;
        iCount = sizeof( PN512_JISX6319_4_424K )/sizeof( struct ST_Pn512RegVal );
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

/**
 * setting default parameters
 */
int Pn512SetDefaultConfigVal( int iType )
{
    int                    iCount;
    struct ST_Pn512RegVal* pst1;
    struct ST_Pn512RegVal* pst2;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEA_INI );
        pst1 = PN512_ISO14443TYPEA_INI;
        pst2 = PN512_ISO14443TYPEA_DEF;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEB_INI );
        pst1 = PN512_ISO14443TYPEB_INI;
        pst2 = PN512_ISO14443TYPEB_DEF;
    }
	else
	{
		return -2;
	}
    memcpy( pst1, pst2, iCount );

    /*setting 106Kbps*/
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEA_106K );
        pst1 = PN512_ISO14443TYPEA_106K;
        pst2 = PN512_ISO14443TYPEA_106KDEF;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEB_106K );
        pst1 = PN512_ISO14443TYPEB_106K;
        pst2 = PN512_ISO14443TYPEB_106KDEF;
    }
    else 
    {
        return -2;
    }
    memcpy( pst1, pst2, iCount );
    
    return;
}

/**
 * set register value in test mode
 */
int Pn512SetRegisterConfigVal( int iType, unsigned char ucReg, unsigned char ucVal )
{
    int                    i = 0;
    int                    iCount;
    struct ST_Pn512RegVal* pst;
    int                    Ret = -1;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEA_INI ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEA_INI;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEB_INI ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEB_INI;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( PN512_JISX6319_4INI ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_JISX6319_4INI;   
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
        iCount = sizeof ( PN512_ISO14443TYPEA_106K ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEA_106K;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEB_106K ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEB_106K;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( PN512_JISX6319_4_212K ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_JISX6319_4_212K;
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

int Pn512GetRegisterConfigVal( int iType, unsigned char ucReg )
{
    int                    i = 0;
    int                    iCount;
    struct ST_Pn512RegVal* pst;
    int                    Ret = -1;
    unsigned char          ucVal;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEA_INI ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEA_INI;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEB_INI ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEB_INI;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( PN512_JISX6319_4INI ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_JISX6319_4INI;   
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
        iCount = sizeof ( PN512_ISO14443TYPEA_106K ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEA_106K;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( PN512_ISO14443TYPEB_106K ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_ISO14443TYPEB_106K;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( PN512_JISX6319_4_212K ) / sizeof( struct ST_Pn512RegVal );
        pst = PN512_JISX6319_4_212K;
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
