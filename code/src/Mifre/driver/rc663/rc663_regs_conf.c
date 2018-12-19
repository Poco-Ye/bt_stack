/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : rc663_regs_conf.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-5-20
 *
 * Description:
 *
 *
 * History:
 *
 * =============================================================================
 */
#include "../../protocol/iso14443hw_hal.h"

#include "phhalHw_Rc663_Reg.h"

#include "rc663_regs_conf.h"
#include <stdlib.h>
#include <string.h>

/**
 * =============================================================================
 * Typical setting for using the CLRC663 for ISO/IEC14443 iType A
 * These Values may differ dependent on the design of the Antenna and used interface
 */
static struct ST_Rc663RegVal RC663_ISO14443TYPEA_DEF[] = 
{
    /*transmitter*/
    { PHHAL_HW_RC663_REG_TXSYM10LEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10MOD, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTCTRL, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x00 },//???
    { PHHAL_HW_RC663_REG_TXSYM1H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1L, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM0L, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM0H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM2, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM3, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32LEN, 0x00 },
    { PHHAL_HW_RC663_REG_FRAMECON, 0xCF },
    { PHHAL_HW_RC663_REG_TXBITMOD, 0x30 },
    { PHHAL_HW_RC663_REG_TXWAITCTRL, 0xC0 },
    { PHHAL_HW_RC663_REG_TXWAITLO, 0x12 },
    { PHHAL_HW_RC663_REG_TXDATAMOD, 0x50 },
    { PHHAL_HW_RC663_REG_TXDATACON, 0x04 },
    { PHHAL_HW_RC663_REG_TXSYM32MOD, 0x50 },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x40 },
    { PHHAL_HW_RC663_REG_TXCRCCON, 0x18 },
    { PHHAL_HW_RC663_REG_TXDATANUM, 0x08 },
    
    /*receiver*/
    { PHHAL_HW_RC663_REG_RCV, 0x12 },
    { PHHAL_HW_RC663_REG_LPO_TRIMM, 0x00 },
    { PHHAL_HW_RC663_REG_RXTHRESHOLD, 0x5B },
    { PHHAL_HW_RC663_REG_RXSVETTE, 0xB2 },
    { PHHAL_HW_RC663_REG_RXCORR, 0x80 },
    { PHHAL_HW_RC663_REG_RXSYNCMOD, 0x00 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x01 },
    { PHHAL_HW_RC663_REG_RXSYNCVALH, 0x00 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x04 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x03 },/*origanl : 0x03*/
    { PHHAL_HW_RC663_REG_RXWAIT, 0x90 },
    { PHHAL_HW_RC663_REG_RXBITCTRL, 0x08 },/*Orignal : 0x08*/
    { PHHAL_HW_RC663_REG_RXEOFSYM, 0x00 },
    { PHHAL_HW_RC663_REG_RXCOLL, 0x00 },/*Orignal : 0x00*/
    { PHHAL_HW_RC663_REG_RXCRCCON, 0x18 },/*Write CRC into FIFO*/
    { PHHAL_HW_RC663_REG_RXANA, 0x06 }, 
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEA_INI[] = 
{
    /*transmitter*/
    { PHHAL_HW_RC663_REG_TXSYM10LEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10MOD, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTCTRL, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x00 },//???
    { PHHAL_HW_RC663_REG_TXSYM1H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1L, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM0L, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM0H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM2, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM3, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32LEN, 0x00 },
    { PHHAL_HW_RC663_REG_FRAMECON, 0xCF },
    { PHHAL_HW_RC663_REG_TXBITMOD, 0x30 },
    { PHHAL_HW_RC663_REG_TXWAITCTRL, 0xC0 },
    { PHHAL_HW_RC663_REG_TXWAITLO, 0x12 },
    { PHHAL_HW_RC663_REG_TXDATAMOD, 0x50 },
    { PHHAL_HW_RC663_REG_TXDATACON, 0x04 },
    { PHHAL_HW_RC663_REG_TXSYM32MOD, 0x50 },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x40 },
    { PHHAL_HW_RC663_REG_TXCRCCON, 0x18 },
    { PHHAL_HW_RC663_REG_TXDATANUM, 0x08 },
    
    /*receiver*/
    { PHHAL_HW_RC663_REG_RCV, 0x12 },
    { PHHAL_HW_RC663_REG_LPO_TRIMM, 0x00 },
    { PHHAL_HW_RC663_REG_RXTHRESHOLD, 0x5B },
    { PHHAL_HW_RC663_REG_RXSVETTE, 0xB2 },
    { PHHAL_HW_RC663_REG_RXCORR, 0x80 },
    { PHHAL_HW_RC663_REG_RXSYNCMOD, 0x00 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x01 },
    { PHHAL_HW_RC663_REG_RXSYNCVALH, 0x00 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x04 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x03 },/*origanl : 0x03*/
    { PHHAL_HW_RC663_REG_RXWAIT, 0x8E },/*must be 7 etus 2011-07-26 AM, EMD test*/
    { PHHAL_HW_RC663_REG_RXBITCTRL, 0x00 },/*Orignal : 0x08*/
    { PHHAL_HW_RC663_REG_RXEOFSYM, 0x00 },
    { PHHAL_HW_RC663_REG_RXCOLL, 0x00 },/*Orignal : 0x00*/
    { PHHAL_HW_RC663_REG_RXCRCCON, 0x18 },/*Write CRC into FIFO*/
    { PHHAL_HW_RC663_REG_RXANA, 0x06 }, 
};

void Rc663Iso14443TypeAInit( struct ST_PCDINFO* ptPcdInfo )
{
    int           i        = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( RC663_ISO14443TYPEA_INI ) / sizeof( struct ST_Rc663RegVal ) ); i++ )
    {
        ucRegVal = RC663_ISO14443TYPEA_INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( RC663_ISO14443TYPEA_INI[i].index,
                            &ucRegVal, 
                            1 ); 
    }    
    
    return;
}

/**
 * Typical setting for speed
 */
static struct ST_Rc663RegVal RC663_ISO14443TYPEA_106KDEF[] = 
{
    { PHHAL_HW_RC663_REG_TXAMP, 0x15 },
    { PHHAL_HW_RC663_REG_TXI, 0x06 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x09 },/*2011-6-28 liubo*/
    { PHHAL_HW_RC663_REG_DRVMOD, 0x89 },/*2011-7-28 liubo*/
    
    { PHHAL_HW_RC663_REG_RXANA, 0x06 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x01 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x03 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x27 },
    
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x40 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x04 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x04 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x08 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEA_106K[] = 
{
    { PHHAL_HW_RC663_REG_TXAMP, 0x15 },
    { PHHAL_HW_RC663_REG_TXI, 0x06 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x09 },/*2011-6-28 liubo*/
    { PHHAL_HW_RC663_REG_DRVMOD, 0x89 },
    
    { PHHAL_HW_RC663_REG_RXANA, 0x06 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x01 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x02 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x27 },
    
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x40 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x04 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x04 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x08 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEA_212K[] = 
{
    { PHHAL_HW_RC663_REG_TXAMP, 0x12 },
    { PHHAL_HW_RC663_REG_TXI, 0x06 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x11 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8E },
    
    { PHHAL_HW_RC663_REG_RXANA, 0x02 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x00 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x23 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x10 },
    
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x50 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x05 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x05 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x0d },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEA_424K[] = 
{
    { PHHAL_HW_RC663_REG_TXAMP, 0x12 },
    { PHHAL_HW_RC663_REG_TXI, 0x06 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x11 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8E },
    
    { PHHAL_HW_RC663_REG_RXANA, 0x02 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x00 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x23 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x08 },
    
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x60 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x06 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x06 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x0d },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEA_848K[] = 
{
    { PHHAL_HW_RC663_REG_TXAMP, 0xDB },
    { PHHAL_HW_RC663_REG_TXI, 0x06 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x11 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8F },
    
    { PHHAL_HW_RC663_REG_RXANA, 0x02 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x00 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x23 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x02 },
    
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x70 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x07 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x07 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x0d },
};

void Rc663Iso14443TypeAConf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_Rc663RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_106000:
        pst = RC663_ISO14443TYPEA_106K;
        iCount = sizeof( RC663_ISO14443TYPEA_106K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_212000:
        pst = RC663_ISO14443TYPEA_212K;
        iCount = sizeof( RC663_ISO14443TYPEA_212K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_424000:
        pst = RC663_ISO14443TYPEA_424K;
        iCount = sizeof( RC663_ISO14443TYPEA_424K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_848000:
        pst = RC663_ISO14443TYPEA_848K;  
        iCount = sizeof( RC663_ISO14443TYPEA_848K )/sizeof( struct ST_Rc663RegVal );
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
 * Typical setting for using the CLRC663 for ISO/IEC14443 iType B
 * These Values may differ dependent on the design of the Antenna and used interface
 */
static struct ST_Rc663RegVal RC663_ISO14443TYPEB_DEF[] = 
{
    /*transmitter*/
    { PHHAL_HW_RC663_REG_TXAMP, 0xCF },
    { PHHAL_HW_RC663_REG_TXI, 0x05 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x09 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8F },
    { PHHAL_HW_RC663_REG_TXSYM10LEN, 0xAB },
    { PHHAL_HW_RC663_REG_TXSYM10MOD, 0x08 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTCTRL, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1L, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM0L, 0x03 },
    { PHHAL_HW_RC663_REG_TXSYM0H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM2, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM3, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32LEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32MOD, 0x00 },
    { PHHAL_HW_RC663_REG_FRAMECON, 0x05 },
    { PHHAL_HW_RC663_REG_TXDATANUM, 0x08 },
    { PHHAL_HW_RC663_REG_TXBITMOD, 0x09 },
    { PHHAL_HW_RC663_REG_TXWAITCTRL, 0x01 },
    { PHHAL_HW_RC663_REG_TXWAITLO, 0x00 },
    { PHHAL_HW_RC663_REG_TXDATAMOD, 0x08 },
    { PHHAL_HW_RC663_REG_TXCRCCON, 0x7B },
    
    /*receiver*/
    { PHHAL_HW_RC663_REG_RCV, 0x12 },
    { PHHAL_HW_RC663_REG_LPO_TRIMM, 0x00 },
    { PHHAL_HW_RC663_REG_RXTHRESHOLD, 0x5B },
    { PHHAL_HW_RC663_REG_RXSVETTE, 0xB2 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x1D },
    { PHHAL_HW_RC663_REG_RXCORR, 0x80 },
    { PHHAL_HW_RC663_REG_RXSYNCMOD, 0x02 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x00 },
    { PHHAL_HW_RC663_REG_RXSYNCVALH, 0x00 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x04 },
    { PHHAL_HW_RC663_REG_RXWAIT, 0x90 },
    { PHHAL_HW_RC663_REG_RXBITCTRL, 0x88 },
    { PHHAL_HW_RC663_REG_RXEOFSYM, 0x00 },
    { PHHAL_HW_RC663_REG_RXCOLL, 0x00 },
    { PHHAL_HW_RC663_REG_RXCRCCON, 0x7B },/*Write CRC into FIFO*/
    { PHHAL_HW_RC663_REG_RXANA, 0x06 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEB_INI[] = 
{
    /*transmitter*/
    { PHHAL_HW_RC663_REG_TXAMP, 0xCD },
    { PHHAL_HW_RC663_REG_TXI, 0x05 },
    { PHHAL_HW_RC663_REG_DRVCON, 0x09 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8F },
    { PHHAL_HW_RC663_REG_TXSYM10LEN, 0xAB },
    { PHHAL_HW_RC663_REG_TXSYM10MOD, 0x08 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTCTRL, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1L, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM0L, 0x03 },
    { PHHAL_HW_RC663_REG_TXSYM0H, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM2, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM3, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32LEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32MOD, 0x00 },
    { PHHAL_HW_RC663_REG_FRAMECON, 0x05 },
    { PHHAL_HW_RC663_REG_TXDATANUM, 0x08 },
    { PHHAL_HW_RC663_REG_TXBITMOD, 0x09 },
    { PHHAL_HW_RC663_REG_TXWAITCTRL, 0x01 },
    { PHHAL_HW_RC663_REG_TXWAITLO, 0x00 },
    { PHHAL_HW_RC663_REG_TXDATAMOD, 0x08 },
    { PHHAL_HW_RC663_REG_TXCRCCON, 0x7B },
    
    /*receiver*/
    { PHHAL_HW_RC663_REG_RCV, 0x12 },
    { PHHAL_HW_RC663_REG_LPO_TRIMM, 0x00 },
    { PHHAL_HW_RC663_REG_RXTHRESHOLD, 0x5B },
    { PHHAL_HW_RC663_REG_RXSVETTE, 0xB2 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x1D },
    { PHHAL_HW_RC663_REG_RXCORR, 0x80 },
    { PHHAL_HW_RC663_REG_RXSYNCMOD, 0x02 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x00 },
    { PHHAL_HW_RC663_REG_RXSYNCVALH, 0x00 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x04 },
    { PHHAL_HW_RC663_REG_RXWAIT, 0x8E },/*TB340.0 and TB435.0*/
    { PHHAL_HW_RC663_REG_RXBITCTRL, 0x00 },
    { PHHAL_HW_RC663_REG_RXEOFSYM, 0x00 },
    { PHHAL_HW_RC663_REG_RXCOLL, 0x00 },
    { PHHAL_HW_RC663_REG_RXCRCCON, 0x7B },
    { PHHAL_HW_RC663_REG_RXANA, 0x06 },
};

void Rc663Iso14443TypeBInit( struct ST_PCDINFO* ptPcdInfo )
{
    int           i      = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( RC663_ISO14443TYPEB_INI ) / sizeof( struct ST_Rc663RegVal ) ); i++ )
    {
        ucRegVal = RC663_ISO14443TYPEB_INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( RC663_ISO14443TYPEB_INI[i].index,
                            &ucRegVal, 
                            1 ); 
    } 
    
    return;   
}

static struct ST_Rc663RegVal RC663_ISO14443TYPEB_106KDEF[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x06 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x34 },/*0x34*/
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x0A },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x04 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x04 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEB_106K[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x06 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x34 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x0A },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x04 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x04 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEB_212K[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x02 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x35 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x0A },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x05 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x05 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEB_424K[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x02 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x36 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x0A },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x06 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x06 },
};

static struct ST_Rc663RegVal RC663_ISO14443TYPEB_848K[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x02 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x27 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x0A },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x07 },     
    { PHHAL_HW_RC663_REG_TXDATACON, 0x07 },
};

void Rc663Iso14443TypeBConf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_Rc663RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_106000:
        pst = RC663_ISO14443TYPEB_106K;
        iCount = sizeof( RC663_ISO14443TYPEB_106K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_212000:
        pst = RC663_ISO14443TYPEB_212K;
        iCount = sizeof( RC663_ISO14443TYPEB_212K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_424000:
        pst = RC663_ISO14443TYPEB_424K;
        iCount = sizeof( RC663_ISO14443TYPEB_424K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_848000:
        pst = RC663_ISO14443TYPEB_848K;  
        iCount = sizeof( RC663_ISO14443TYPEB_848K )/sizeof( struct ST_Rc663RegVal );
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
 * Typical setting for using the CLRC663 for JIS X 6319-4
 * These Values may differ dependent on the design of the Antenna and used interface
 */
static struct ST_Rc663RegVal RC663_JISX6319_4DEF[] = 
{
    /*transmitter*/
    { PHHAL_HW_RC663_REG_TXAMP, 0x04 },
    { PHHAL_HW_RC663_REG_TXI, 0x05 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8F },
    { PHHAL_HW_RC663_REG_DRVCON, 0x09 },
    { PHHAL_HW_RC663_REG_RXSOFD, 0x00 },
    { PHHAL_HW_RC663_REG_LPO_TRIMM, 0x00 },
    { PHHAL_HW_RC663_REG_FRAMECON, 0x01 },
    { PHHAL_HW_RC663_REG_TXDATAMOD, 0x01 },
    { PHHAL_HW_RC663_REG_TXBITMOD, 0x80 },
    
    { PHHAL_HW_RC663_REG_TXSYM0L, 0x4D },
    { PHHAL_HW_RC663_REG_TXSYM0H, 0xB2 },
    { PHHAL_HW_RC663_REG_TXSYM10LEN, 0x0F },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x03 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTCTRL, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM10MOD, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM2, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM3, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32LEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32MOD, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1L, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM1H, 0x00 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x00 },
    { PHHAL_HW_RC663_REG_TXWAITCTRL, 0xC0 },
    { PHHAL_HW_RC663_REG_TXWAITLO, 0x00 },
    { PHHAL_HW_RC663_REG_TXCRCCON, 0x09 },
    { PHHAL_HW_RC663_REG_TXDATANUM, 0x08 },
    
    /*receiver*/
    { PHHAL_HW_RC663_REG_RCV, 0x12 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x18 },
    { PHHAL_HW_RC663_REG_RXBITCTRL, 0x80 },
    { PHHAL_HW_RC663_REG_RXEOFSYM, 0x00 },
    { PHHAL_HW_RC663_REG_RXCOLL, 0x00 },
    { PHHAL_HW_RC663_REG_RXCRCCON, 0x09 },/*ucReg Original: 09h;Write CRC into FIFO*/
    { PHHAL_HW_RC663_REG_RXSYNCMOD, 0xF0 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x4D },
    { PHHAL_HW_RC663_REG_RXSYNCVALH, 0xB2 },
    { PHHAL_HW_RC663_REG_RXTHRESHOLD, 0x5C },
    { PHHAL_HW_RC663_REG_RXSVETTE, 0xF0 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x19 },
    { PHHAL_HW_RC663_REG_RXANA, 0x00 },
};

static struct ST_Rc663RegVal RC663_JISX6319_4INI[] = 
{
    /*transmitter*/
    { PHHAL_HW_RC663_REG_TXAMP, 0x04 },
    { PHHAL_HW_RC663_REG_TXI, 0x05 },
    { PHHAL_HW_RC663_REG_DRVMOD, 0x8F },
    { PHHAL_HW_RC663_REG_DRVCON, 0x09 },
    { PHHAL_HW_RC663_REG_RXSOFD, 0x00 },
    { PHHAL_HW_RC663_REG_LPO_TRIMM, 0x00 },
    { PHHAL_HW_RC663_REG_FRAMECON, 0x01 },
    { PHHAL_HW_RC663_REG_TXDATAMOD, 0x01 },
    { PHHAL_HW_RC663_REG_TXBITMOD, 0x80 },
    
    { PHHAL_HW_RC663_REG_TXSYM0L, 0x4D },
    { PHHAL_HW_RC663_REG_TXSYM0H, 0xB2 },
    { PHHAL_HW_RC663_REG_TXSYM10LEN, 0x0F },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTLEN, 0x03 },
    { PHHAL_HW_RC663_REG_TXSYM10BURSTCTRL, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM10MOD, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM2, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM3, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32LEN, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM32MOD, 0x00 },
    { PHHAL_HW_RC663_REG_TXSYM1L, 0x01 },
    { PHHAL_HW_RC663_REG_TXSYM1H, 0x00 },
    { PHHAL_HW_RC663_REG_TXMODWIDTH, 0x00 },
    { PHHAL_HW_RC663_REG_TXWAITCTRL, 0xC0 },
    { PHHAL_HW_RC663_REG_TXWAITLO, 0x00 },
    { PHHAL_HW_RC663_REG_TXCRCCON, 0x09 },
    { PHHAL_HW_RC663_REG_TXDATANUM, 0x08 },
    
    /*receiver*/
    { PHHAL_HW_RC663_REG_RCV, 0x12 },
    { PHHAL_HW_RC663_REG_RXBITMOD, 0x18 },
    { PHHAL_HW_RC663_REG_RXBITCTRL, 0x80 },
    { PHHAL_HW_RC663_REG_RXEOFSYM, 0x00 },
    { PHHAL_HW_RC663_REG_RXCOLL, 0x00 },
    { PHHAL_HW_RC663_REG_RXCRCCON, 0x09 },/*ucReg Original: 09h;Write CRC into FIFO*/
    { PHHAL_HW_RC663_REG_RXSYNCMOD, 0xF0 },
    { PHHAL_HW_RC663_REG_RXSYNCVALL, 0x4D },
    { PHHAL_HW_RC663_REG_RXSYNCVALH, 0xB2 },
    { PHHAL_HW_RC663_REG_RXTHRESHOLD, 0x5C },
    { PHHAL_HW_RC663_REG_RXSVETTE, 0xF0 },
    { PHHAL_HW_RC663_REG_RXMOD, 0x19 },
    { PHHAL_HW_RC663_REG_RXANA, 0x00 },
};

void Rc663Jisx6319_4Init( struct ST_PCDINFO* ptPcdInfo )
{
    int           i      = 0;
    unsigned char ucRegVal = 0;
    
    for( i = 0; i < ( sizeof( RC663_JISX6319_4INI ) / sizeof( struct ST_Rc663RegVal ) ); i++ )
    {
        ucRegVal = RC663_JISX6319_4INI[i].value;
        ptPcdInfo->ptBspOps->RfBspWrite( RC663_JISX6319_4INI[i].index,
                            &ucRegVal, 
                            1 ); 
    }
    
    return;
}

static struct ST_Rc663RegVal RC663_JISX6319_4_212K[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x03 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x05 },
    { PHHAL_HW_RC663_REG_RXWAIT, 0x86 },
    { PHHAL_HW_RC663_REG_TXDATACON, 0x05 },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x05 },
    { PHHAL_HW_RC663_REG_RXCORR, 0x20 },
};

static struct ST_Rc663RegVal RC663_JISX6319_4_424K[] = 
{
    { PHHAL_HW_RC663_REG_RXANA, 0x03 },
    { PHHAL_HW_RC663_REG_RXCTRL, 0x06 },
    { PHHAL_HW_RC663_REG_RXWAIT, 0x40 },
    { PHHAL_HW_RC663_REG_TXDATACON, 0x06 },
    { PHHAL_HW_RC663_REG_TXSYMFREQ, 0x06 },
    { PHHAL_HW_RC663_REG_RXCORR, 0x50 },
};

void Rc663Jisx6319_4Conf( struct ST_PCDINFO* ptPcdInfo )
{
    int                    i       = 0;
    unsigned char          ucRegVal  = 0;
    struct ST_Rc663RegVal* pst;
    int                    iCount   = 0;
    
    switch( ptPcdInfo->uiPcdBaudrate )
    {
    case BAUDRATE_212000:
        pst = RC663_JISX6319_4_212K;
        iCount = sizeof( RC663_JISX6319_4_212K )/sizeof( struct ST_Rc663RegVal );
    break;
    case BAUDRATE_424000:
        pst = RC663_JISX6319_4_424K;
        iCount = sizeof( RC663_JISX6319_4_424K )/sizeof( struct ST_Rc663RegVal );
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
int Rc663SetDefaultConfigVal( int iType )
{
    int                    iCount;
    struct ST_Rc663RegVal* pst1;
    struct ST_Rc663RegVal* pst2;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEA_INI );
        pst1 = RC663_ISO14443TYPEA_INI;
        pst2 = RC663_ISO14443TYPEA_DEF;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEB_INI );
        pst1 = RC663_ISO14443TYPEB_INI;
        pst2 = RC663_ISO14443TYPEB_DEF;
    }
	else
	{
		return -2;
	}
    memcpy( pst1, pst2, iCount );

    /*setting 106Kbps*/
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEA_106K );
        pst1 = RC663_ISO14443TYPEA_106K;
        pst2 = RC663_ISO14443TYPEA_106KDEF;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEB_106K );
        pst1 = RC663_ISO14443TYPEB_106K;
        pst2 = RC663_ISO14443TYPEB_106KDEF;
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
int Rc663SetRegisterConfigVal( int iType, unsigned char ucReg, unsigned char ucVal )
{
    int                    i = 0;
    int                    iCount;
    struct ST_Rc663RegVal* pst;
    int                    Ret = -1;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEA_INI ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEA_INI;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEB_INI ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEB_INI;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( RC663_JISX6319_4INI ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_JISX6319_4INI;   
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
        iCount = sizeof ( RC663_ISO14443TYPEA_106K ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEA_106K;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEB_106K ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEB_106K;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( RC663_JISX6319_4_212K ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_JISX6319_4_212K;
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

int Rc663GetRegisterConfigVal( int iType, unsigned char ucReg )
{
    int                    i = 0;
    int                    iCount;
    struct ST_Rc663RegVal* pst;
    int                    Ret = -1;
    unsigned char          ucVal;
    
    if( ISO14443_TYPEA_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEA_INI ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEA_INI;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEB_INI ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEB_INI;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( RC663_JISX6319_4INI ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_JISX6319_4INI;   
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
        iCount = sizeof ( RC663_ISO14443TYPEA_106K ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEA_106K;
    }
    else if( ISO14443_TYPEB_STANDARD == iType )
    {
        iCount = sizeof ( RC663_ISO14443TYPEB_106K ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_ISO14443TYPEB_106K;
    }
    else if( JISX6319_4_STANDARD == iType )
    {
        iCount = sizeof ( RC663_JISX6319_4_212K ) / sizeof( struct ST_Rc663RegVal );
        pst = RC663_JISX6319_4_212K;
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
