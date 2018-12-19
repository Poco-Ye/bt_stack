/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : rc663_driver.c
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
#include "../../protocol/mifare.h"
#include "rc663_driver.h"
#include "../../protocol/pcd_apis.h"
#include <stdlib.h>
#include <string.h>

/**
 * these are global variants in Rc663 driver.
 */
static volatile unsigned int   guiRxLastBits; 
static volatile unsigned int   guiRc663ComStatus;
static volatile unsigned int   guiPiccActivation;
static volatile unsigned int   guiDeltaT;
static struct ST_PCDINFO*      gptPcdChipInfo;
static volatile unsigned int   gucChipAbnormalTag = 0;

/**
 * the driver operations list
 */
const struct ST_PCDOPS gtRc663Ops = 
{
    Rc663Init,
   
    Rc663CarrierOn,
    Rc663CarrierOff,
    
    Rc663Config,
    
    Rc663Waiter,
    
    Rc663Trans,
    Rc663TransCeive,
    Rc663MifareAuthenticate,
    
    Rc663CommIsr,

	Rc663GetParamTagValue,
	Rc663SetParamValue
	
};


/**
 * initialising Rc663
 * 
 * parameters:
 *             no parameter
 * reval:
 *             no return value
 */
int Rc663Init( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    unsigned char aucFiFo[12];
    
    int           i;
    
    guiDeltaT = 16;
    
    /*test spi communication if is ok?*/
    /* clearing fifo ready to filling with datas
     * set the deepth of fifo is 512, and the waterleve is 256
     */
    ucRegVal = 0x14;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    ucRegVal = 0x00;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_WATERLEVEL, &ucRegVal, 1 );
    
    /*write and read fixed serial number into FIFO*/
    ucRegVal = 0x33;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    ucRegVal = 0xCC;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    ucRegVal = 0x66;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    ucRegVal = 0x99;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    ucRegVal = 0x55;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    ucRegVal = 0xAA;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    for( i = 0; i < 6; i++ )
    {
        ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
        aucFiFo[i] = ucRegVal;
    }
    
    /*compare fixed serial number*/
    if( 0 != memcmp( aucFiFo, "\x33\xcc\x66\x99\x55\xaa", 6 ) )
    {
        return 0xFF;/*compliant with R50*/
    }
    ucRegVal = 0x00;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
    //s_UartPrint( "Rc663Init!\r\n" );
    
    /*provide these functions for ISR*/
    gptPcdChipInfo = ptPcdInfo;
    
    return 0; 
}

/**
 * Setting time out parameters
 */
static int Rc663SetDeltaT( int delta_t )
{
    guiDeltaT = delta_t;
    
    return 0;   
}

/**
 * configurating Rc663
 * 
 * parameters:
 *            no paramters
 * reval:
 *            no return value
 */
int Rc663Config( struct ST_PCDINFO* ptPcdInfo )
{     
    unsigned char ucRegVal = 0;
    unsigned int  uiTimeOut= 0;
    
    /*terminate current command*/
    ucRegVal = PHHAL_HW_RC663_CMD_IDLE;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
    
    if( ptPcdInfo->ucEmdEn )guiPiccActivation = 1;
    else guiPiccActivation = 0;   
    
    /**
     * every type initialised configuration
     */
    switch( ptPcdInfo->ucProtocol )
    {
    case ISO14443_TYPEA_STANDARD:
	case MIFARE_STANDARD:
        Rc663Iso14443TypeAInit( ptPcdInfo );
    break;
    case ISO14443_TYPEB_STANDARD:
        Rc663Iso14443TypeBInit( ptPcdInfo );
    break;
    case JISX6319_4_STANDARD:/*Felica*/
        Rc663Jisx6319_4Init( ptPcdInfo );
    break;
    default:
    break;
    }
    
    /*Rc663 configuration...*/
    switch( ptPcdInfo->ucProtocol )
    {
    case ISO14443_TYPEA_STANDARD:
	case MIFARE_STANDARD:
    		
        /*transmitter's digital uart configurations*/
        if( ISO14443_TYPEA_SHORT_FRAME == ptPcdInfo->ucFrameMode )
        {
            ptPcdInfo->uiPcdTxRLastBits = 7;
            ucRegVal = 0x4F;
        }
        else if( ISO14443_TYPEA_STANDARD_FRAME == ptPcdInfo->ucFrameMode )
        {
            ptPcdInfo->uiPcdTxRLastBits = 8;
            ucRegVal = 0xCF;   
        }
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FRAMECON, &ucRegVal, 1 );
        
        /*the bits number of the last bytes*/
        ucRegVal = 0x08 | ptPcdInfo->uiPcdTxRLastBits;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TXDATANUM, &ucRegVal, 1 ); 
        
        if( ptPcdInfo->ucTxCrcEnable )ucRegVal = 0x19;/*CRC16:0x6363*/
        else ucRegVal = 0x18;   
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TXCRCCON, &ucRegVal, 1 );
        
        if( ptPcdInfo->ucRxCrcEnable )ucRegVal = 0x19;/*crc will be only checked*/
        else ucRegVal = 0x18;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_RXCRCCON, &ucRegVal, 1 );
        
        /*the configuration in 106Kbps mode*/
        Rc663Iso14443TypeAConf( ptPcdInfo ); 
        
        if( !ptPcdInfo->ucM1CryptoEn )/*disable crypto1 with Mifare standard*/
        {
            ucRegVal = ( ~PHHAL_HW_RC663_BIT_CRYPTO1ON ) & 0x00;    
            ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_STATUS, &ucRegVal, 1 );  
        }
    break;
    case ISO14443_TYPEB_STANDARD:
        Rc663Iso14443TypeBConf( ptPcdInfo );
        
        /*disable crypto1 with mifare*/
        ucRegVal = ( ~PHHAL_HW_RC663_BIT_CRYPTO1ON ) & 0x00;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_STATUS, &ucRegVal, 1 );
    break;
    case JISX6319_4_STANDARD:
        Rc663Jisx6319_4Conf( ptPcdInfo );
        
        /*disable crypto1 with mifare*/
        ucRegVal = ( ~PHHAL_HW_RC663_BIT_CRYPTO1ON ) & 0x00;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_STATUS, &ucRegVal, 1 );
    break;
    default:
    break;
    }
    
    /*inverted modulation*/
    if( ptPcdInfo->ucModInvert )
    {
        ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_TXDATAMOD, &ucRegVal, 1 );
        ucRegVal |= 0x08;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TXDATAMOD, &ucRegVal, 1 );    
    }
    else
    {
        ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_TXDATAMOD, &ucRegVal, 1 );
        ucRegVal &= ~0x08;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TXDATAMOD, &ucRegVal, 1 );       
    }
    
    /*stop all timer*/
    ucRegVal = 0x00;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );
    
    /*wow god, it's terrible*/
    uiTimeOut = ptPcdInfo->uiFwt + guiDeltaT;
    /*configurating timer0 as base timer, per etu*/
    if( uiTimeOut > 65535 )
    {
        ucRegVal = 0;/*32etu*/
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADLO, &ucRegVal, 1 );
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALLO, &ucRegVal, 1 );
        ucRegVal = 16;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADHI, &ucRegVal, 1 );     
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALHI, &ucRegVal, 1 );
    }
    else
    {
        ucRegVal = 128;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADLO, &ucRegVal, 1 );
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALLO, &ucRegVal, 1 );
        ucRegVal = 0;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADHI, &ucRegVal, 1 );
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALHI, &ucRegVal, 1 );
    }
    ucRegVal = PHHAL_HW_RC663_VALUE_TCLK_1356_MHZ |
               PHHAL_HW_RC663_BIT_TAUTORESTARTED;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0CONTROL, &ucRegVal, 1 );
    
    /*start tomer0*/
    ucRegVal = PHHAL_HW_RC663_BIT_T0RUNNING |
               PHHAL_HW_RC663_BIT_T0STARTSTOPNOW;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );
    
    /*configurating FWT*/
    if( uiTimeOut > 65535 )
    {
        uiTimeOut = uiTimeOut / 32;
        if( uiTimeOut % 32 )uiTimeOut++;
    }
    ucRegVal = uiTimeOut % 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1RELOADLO, &ucRegVal, 1 ); 
    ucRegVal = uiTimeOut / 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1RELOADHI, &ucRegVal, 1 );
    
    ucRegVal = uiTimeOut % 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1COUNTERVALLO, &ucRegVal, 1 );
    ucRegVal = uiTimeOut / 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1COUNTERVALHI, &ucRegVal, 1 );
    
    ucRegVal = PHHAL_HW_RC663_VALUE_TCLK_T0 |
               PHHAL_HW_RC663_BIT_TSTOP_RX |
               PHHAL_HW_RC663_BIT_TSTART_TX;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1CONTROL, &ucRegVal, 1 );
    
    gptPcdChipInfo = ptPcdInfo;
    
    return 0;
}

/**
 * RC663 waiter
 */
int Rc663Waiter( struct ST_PCDINFO* ptPcdInfo, unsigned int uiEtuCount )
{
	unsigned int  uiTimeOut = 0;
	unsigned char ucRegVal  = 0;
	
	if( !uiEtuCount )return 0;
    if( uiEtuCount < ISO14443_PICC_FDT_MIN )uiEtuCount = ISO14443_PICC_FDT_MIN;	
	
	/*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
    
	/*stop all timer*/
	uiTimeOut = uiEtuCount;
    ucRegVal = PHHAL_HW_RC663_BIT_T1STARTSTOPNOW | 
               PHHAL_HW_RC663_BIT_T0STARTSTOPNOW;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );
    
    ucRegVal = PHHAL_HW_RC663_BIT_TIMER1IRQ;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
    
    /*configurating timer0 as base timer, per etu*/
    if( uiTimeOut > 65535 )
    {
        ucRegVal = 0;/*32etu*/
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADLO, &ucRegVal, 1 );
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALLO, &ucRegVal, 1 );
        ucRegVal = 16;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADHI, &ucRegVal, 1 );     
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALHI, &ucRegVal, 1 );
    }
    else
    {
        ucRegVal = 128;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADLO, &ucRegVal, 1 );
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALLO, &ucRegVal, 1 );
        ucRegVal = 0;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0RELOADHI, &ucRegVal, 1 );
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0COUNTERVALHI, &ucRegVal, 1 );
    }
    ucRegVal = PHHAL_HW_RC663_VALUE_TCLK_1356_MHZ |
               PHHAL_HW_RC663_BIT_TAUTORESTARTED;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T0CONTROL, &ucRegVal, 1 );
    
    /*start tomer0*/
    ucRegVal = PHHAL_HW_RC663_BIT_T0RUNNING |
               PHHAL_HW_RC663_BIT_T0STARTSTOPNOW;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );
    
    /*configurating FWT*/
    if( uiTimeOut > 65535 )
    {
        uiTimeOut = uiTimeOut / 32;
        if( uiTimeOut % 32 )uiTimeOut++;
    }

    ucRegVal = uiTimeOut % 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1RELOADLO, &ucRegVal, 1 ); 
    ucRegVal = uiTimeOut / 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1RELOADHI, &ucRegVal, 1 );
    
    ucRegVal = uiTimeOut % 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1COUNTERVALLO, &ucRegVal, 1 );
    ucRegVal = uiTimeOut / 256;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1COUNTERVALHI, &ucRegVal, 1 );
    
    ucRegVal = PHHAL_HW_RC663_VALUE_TCLK_T0;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_T1CONTROL, &ucRegVal, 1 );
    
    ucRegVal = PHHAL_HW_RC663_BIT_T1RUNNING |
               PHHAL_HW_RC663_BIT_T1STARTSTOPNOW;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );
    
    do
    {
		ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
  	}while( !( ucRegVal & PHHAL_HW_RC663_BIT_TIMER1IRQ ) );
  	
  	ucRegVal = PHHAL_HW_RC663_BIT_TIMER1IRQ;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
    
    ucRegVal = PHHAL_HW_RC663_BIT_T1STARTSTOPNOW | 
               PHHAL_HW_RC663_BIT_T0STARTSTOPNOW;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );

    return 0;	
	
}

/**
 * Rc663 open carrier
 * 
 * parameter:
 *            no param
 * reval:
 *            ignore return value
 */
int Rc663CarrierOn( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    
//    if( PCD_CARRIER_OFF == ptPcdInfo->uiPcdState )
//    {
	    ucRegVal = 0x88;
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_DRVMOD, &ucRegVal, 1 );
        PcdWaiter( ptPcdInfo, 540 );
        //DelayMs( 5 );
//    }
  /* delete by nt for paypass 3.0 performance test. 2013/03/11 pay attention,must be delete.!!!*/
 //   ptPcdInfo->uiPcdState = PCD_CARRIER_ON;
    
    return 0;
}

/**
 * Rc663 close carrier
 * 
 * parameter:
 *            no param
 * reval:
 *            ignore return value
 */
int Rc663CarrierOff( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    
    ucRegVal = 0x80;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_DRVMOD, &ucRegVal, 1 );
    PcdWaiter( ptPcdInfo, 740 );
   /* delete by nt for paypass 3.0 performance test. 2013/03/11 pay attention,must be delete.!!!*/
  //  ptPcdInfo->uiPcdState = PCD_CARRIER_OFF;
    
    return 0;
}

/**
 * process the mifare one standard authentication.
 * 
 * parameter:
 *            ptPcdInfo : the handle of device.
 * 
 * reval:
 *            0 - ok
 *            others value, error 
 */
int Rc663MifareAuthenticate( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    unsigned int  uiCNumTx = 0;
	unsigned int  uiBeginTime = 0;
    int           i;
	gucChipAbnormalTag = 0;
    
    /*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
    
    ucRegVal = PHHAL_HW_RC663_BIT_IRQINV;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucRegVal, 1 );
    ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 ); 
    
    /**
     * clearing fifo ready to filling with datas
     * set the deepth of fifo is 512, and the waterleve is 256 + 64
     */
    ucRegVal = 0x14;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    ucRegVal = 0x40;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_WATERLEVEL, &ucRegVal, 1 );
    
    /*load key to key buffer to initialize the crypto1*/
    for( i = 0; i < 6; i++ )
    {
        ucRegVal = ptPcdInfo->aucPcdTxRBuffer[ i + 6 ];
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    }
    ucRegVal = PHHAL_HW_RC663_CMD_LOADKEY;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
    
    /*check the load key command executing status*/
    do
    {
        ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOLENGTH, &ucRegVal, 1 );
        if( ucRegVal > 6 )return ISO14443_HW_ERR_RC663_SPI;
    }while( ucRegVal );
    
    /*write command into fifo, '60' or '61', blkno, {UID}*/
    guiRc663ComStatus = RC663_HW_TRANSFERRING;
    for( i = 0; i < 6; i++ )
    {
        ucRegVal = ptPcdInfo->aucPcdTxRBuffer[ i ];
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    }
    
    /**
     * read the fifo length at once to verify the spi operation be ok
     */
    ptPcdInfo->uiPcdTxRNum      = 0;
    ptPcdInfo->uiPcdTxRLastBits = 0;
    
    ucRegVal = 0; 
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    uiCNumTx = ( ucRegVal & 0x03 ) << 8;
    ucRegVal = 0; 
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOLENGTH, &ucRegVal, 1 );
    uiCNumTx += ucRegVal;
    if( 6 != uiCNumTx )return ISO14443_HW_ERR_RC663_SPI;
    
    /*clearing interrupts*/
    ucRegVal = 0x7F;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
    ucRegVal = 0x7F;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 ); 
    
    /*enable interrupts*/
    ucRegVal = PHHAL_HW_RC663_BIT_IRQINV | PHHAL_HW_RC663_BIT_IDLEIRQ;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucRegVal, 1 );
    ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL | 
               PHHAL_HW_RC663_BIT_TIMER1IRQ | 
               PHHAL_HW_RC663_BIT_GLOBALIRQ;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 );
    
    /*enable interrupt*/
    ptPcdInfo->ptBspOps->RfBspIntEnable();
    
    /*start mifare authenticate procedure*/
    ucRegVal = PHHAL_HW_RC663_CMD_MFAUTHENT;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );

    /* add by wanls 2012.05.03*/
	uiBeginTime = GetTimerCount();
    while( !( 
              ( RC663_HW_DEVICE_IDLE == ( guiRc663ComStatus & 0xFF ) ) ||
              ( guiRc663ComStatus & 0x10000 ) 
            )
         )
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 7000)
        {
            guiRc663ComStatus = 0x10000 | RC663_HW_ERROR_OCCURED;
            break;
        }
    }
    /* add by wanls 2013.10.08*/
	/*处理认证失败时，调用PiccOpen会失败的问题。 原因是认证不成功时，
	认证命令是不会被自动终止，即一直处于认证命令阶段，此阶段FIFO的访
	问是被阻止的。而PiccOpen中有读写FIFO数据判断芯片是否正常的操作，所
	以出现认证失败时，PiccOpen失败的问题*/
	ucRegVal = PHHAL_HW_RC663_CMD_IDLE;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
	/* add end */


	
    /*disable interruption*/
    gptPcdChipInfo->ptBspOps->RfBspIntDisable();
	
	if( guiRc663ComStatus & 0x10000 )return ISO14443_HW_ERR_COMM_TIMEOUT;
	/* add end */
    
    /*check error status register*/
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_ERROR, &ucRegVal, 1 ); /*view authenticate status*/
    if( ucRegVal )return ISO14443_HW_ERR_COMM_PROT;

    /*check the cypto1 on flag*/
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_STATUS, &ucRegVal, 1 ); /*view authenticate status*/
    if( PHHAL_HW_RC663_BIT_CRYPTO1ON != ucRegVal )return PHILIPS_MIFARE_ERR_AUTHEN; 

    return 0;
}

/**
 * transfer datas to the picc bypass fifo in rc663
 * 
 * parameter:
 *            ptPcdInfo : the handle of device.
 * 
 * reval:
 *            0 - ok
 *            others value, error 
 */
int Rc663Trans( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char  ucRegVal = 0;
    unsigned char  ucIrqEn  = 0;
    unsigned int   uiTxNum  = 0;
    int            i        = 0;
	unsigned int   uiBeginTime = 0;
	gucChipAbnormalTag = 0;
    
    /*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
    
    ucRegVal = PHHAL_HW_RC663_BIT_IRQINV;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucRegVal, 1 );
    ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 );
    
    ucIrqEn = PHHAL_HW_RC663_BIT_IRQINV|
              PHHAL_HW_RC663_BIT_TXIRQ;
            
    /**
     * clearing fifo ready to filling with datas
     * set the deepth of fifo is 512, and the waterleve is 256
     */
    ucRegVal = 0x14;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    ucRegVal = 0x00;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_WATERLEVEL, &ucRegVal, 1 );
    
    /*filling data into fifo*/
    guiRc663ComStatus = RC663_HW_TRANSFERRING;
    uiTxNum = ptPcdInfo->uiPcdTxRNum;
    for( i = 0; i < ptPcdInfo->uiPcdTxRNum; i++ )
    {
        ucRegVal = ptPcdInfo->aucPcdTxRBuffer[i];
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    }
    
    /**
     * read the fifo length at once to verify the spi operation be ok
     */
    ptPcdInfo->uiPcdTxRNum      = 0;
    ptPcdInfo->uiPcdTxRLastBits = 0;
    
    ucRegVal = 0; 
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    uiTxNum ^= ( ucRegVal & 0x03 ) << 8;
    ucRegVal = 0; 
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOLENGTH, &ucRegVal, 1 );
    uiTxNum ^= ucRegVal;
    if( uiTxNum )return ISO14443_HW_ERR_RC663_SPI;
    
    /*clearing interrupts*/
    ucRegVal = 0x7F;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
    ucRegVal = 0x7F;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );  
    
    /*configurating interrupts and enable global interrupt mask bit*/
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucIrqEn, 1 );
    ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL | 
               PHHAL_HW_RC663_BIT_TIMER1IRQ | 
               PHHAL_HW_RC663_BIT_GLOBALIRQ;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 );
    
    /*enable interrupt*/
    ptPcdInfo->ptBspOps->RfBspIntEnable();

    /*start tansmission*/
    ucRegVal = PHHAL_HW_RC663_CMD_TRANSMIT;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
    
    /*waiting for transfer completely*/
    uiBeginTime = GetTimerCount();
    while( RC663_HW_TRANSFERRING == guiRc663ComStatus )
    {
        if((unsigned int)((unsigned int )GetTimerCount() - uiBeginTime) > 7000)
        {
            guiRc663ComStatus = 0x10000 | RC663_HW_ERROR_OCCURED;
            break;
        }
    }
    gptPcdChipInfo->ptBspOps->RfBspIntDisable();
    
    return 0;
}

/**
 * Write datas into the FIFO of Rc663 and receive datas from picc
 * 
 * parameter:
 *            ptPcdInfo : the handle of device.
 * 
 * reval:
 *            0 - ok
 *            others value, error 
 */
int Rc663TransCeive( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    unsigned char ucIrqEn  = 0;
    unsigned int  uiTxNum  = 0;
    int           i;
	unsigned int  uiBeginTime = 0;
	gucChipAbnormalTag = 0;
    
    /*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
    
    ucRegVal = PHHAL_HW_RC663_BIT_IRQINV;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucRegVal, 1 );
    ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 );
    
    ucIrqEn = PHHAL_HW_RC663_BIT_IRQINV|
              PHHAL_HW_RC663_BIT_TXIRQ | 
              PHHAL_HW_RC663_BIT_RXIRQ;
    /**
     * clearing fifo ready to filling with datas
     * set the deepth of fifo is 512, and the waterleve is 256
     */
    ucRegVal = 0x14;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    ucRegVal = 0x00;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_WATERLEVEL, &ucRegVal, 1 );
    
    /*filling data into fifo*/
    guiRc663ComStatus = RC663_HW_TRANSFERRING;
    uiTxNum = ptPcdInfo->uiPcdTxRNum;
    /*filling data into fifo*/
    for( i = 0; i < ptPcdInfo->uiPcdTxRNum; i++ )
    {
        ucRegVal = ptPcdInfo->aucPcdTxRBuffer[i];
        ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
    }
    
    /**
     * read the fifo length at once to verify the spi operation be ok
     */
    ptPcdInfo->uiPcdTxRNum      = 0;
    ptPcdInfo->uiPcdTxRLastBits = 0;
    
    ucRegVal = 0; 
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    uiTxNum ^= ( ucRegVal & 0x03 ) << 8;
    ucRegVal = 0; 
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOLENGTH, &ucRegVal, 1 );
    uiTxNum ^= ucRegVal;
    if( uiTxNum )return ISO14443_HW_ERR_RC663_SPI;
    
    /**
     * clearing interrupts at this position is need, because after the trans command, 
     * write datas into fifo will generate the FiFoWrErr. but it is need by mifare.
     */
    ucRegVal = 0x7F;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
    ucRegVal = 0x7F;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 ); 
    
    /*configurating interrupts and enable global interrupt mask bit*/
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucIrqEn, 1 );
    ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL | 
               PHHAL_HW_RC663_BIT_TIMER1IRQ | 
               PHHAL_HW_RC663_BIT_GLOBALIRQ;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 );
    
    /*enable interrupt*/
    ptPcdInfo->ptBspOps->RfBspIntEnable();

    /*start tansmission*/
    ucRegVal = PHHAL_HW_RC663_CMD_TRANSCEIVE;
    ptPcdInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
    
    /*waiting for reception completed*/    
    uiBeginTime = GetTimerCount();
    while( !( 
              ( RC663_HW_RECEPTION_COMPLETED == ( guiRc663ComStatus & 0xFF ) ) ||
              ( guiRc663ComStatus & 0x10000 ) 
            )
         )
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 7000)
        {
            guiRc663ComStatus = 0x10000 | RC663_HW_ERROR_OCCURED;
            break;
        }
    }
    /*disable interruption*/
    gptPcdChipInfo->ptBspOps->RfBspIntDisable();
    
    /*timeout*/
    if( guiRc663ComStatus & 0x10000 )return ISO14443_HW_ERR_COMM_TIMEOUT;
    /*fifo write error or fifo overflow*/
    if( guiRc663ComStatus & 0x6800 ) return ISO14443_HW_ERR_COMM_FIFO;
    /*min frame error*/
    if( guiRc663ComStatus & 0x1000 )return ISO14443_HW_ERR_COMM_FRAME; 
    /*intergit error, crc or parity error*/
    if( guiRc663ComStatus & 0x0100 )return ISO14443_HW_ERR_COMM_PARITY;
    /*collision error*/
    if( guiRc663ComStatus & 0x0400 )return ISO14443_HW_ERR_COMM_COLL;
    /*protocol error, e.g. miss or a wrong EOF, SOF, stop bit, 
     *or a wrong number of received data bytes error
     */
    if( guiRc663ComStatus & 0x0200 )  return ISO14443_HW_ERR_COMM_PROT;
    
    /*read FIFO*/
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
    ptPcdInfo->uiPcdTxRNum  = ( ucRegVal & 0x03 ) << 8;
    ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOLENGTH, &ucRegVal, 1 );
    ptPcdInfo->uiPcdTxRNum += ucRegVal;
    if( ptPcdInfo->uiPcdTxRNum > PCD_MAX_BUFFER_SIZE )ptPcdInfo->uiPcdTxRNum = PCD_MAX_BUFFER_SIZE;
    for( i = 0; i < ptPcdInfo->uiPcdTxRNum; i++ )
    {
        ptPcdInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFODATA, &ucRegVal, 1 );
        ptPcdInfo->aucPcdTxRBuffer[i] = ucRegVal; 
    }
    ptPcdInfo->uiPcdTxRLastBits = guiRxLastBits;
    
    return 0;
}

/**
 * Rc663 interrupt service routine
 * 
 * parameter:
 *            no parameter
 *
 * reval:
 *            no return value
 */
void Rc663CommIsr()
{
    unsigned int  uiRemainNum = 0;
    unsigned char ucIrqStatus = 0;
    unsigned char ucIrqEn     = 0;
    
    unsigned char ucErrorFlag = 0;
    unsigned char ucRegVal    = 0;
    
    /*Clearing global interrupt*/
    while( 1 )
    {
        /* add by wanls 2013.12.10*/
        gucChipAbnormalTag++;
		/* add end */
        gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_IRQ1, &ucIrqStatus, 1 );
        gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_IRQ1EN, &ucIrqEn, 1 );
        ucIrqStatus &= ucIrqEn;
		/*quit interruption service*/
        if( 0x00 == ( ucIrqStatus & PHHAL_HW_RC663_BIT_GLOBALIRQ ) )
		{
			/* add by wanls 2013.01.24 用于处理芯片进入异常状态时，中断口线一直处于低电平状态
			如果设置为低电平触发中断，会频繁进出中断导致死机 */
		    if(gucChipAbnormalTag > 100)
		    {
				gptPcdChipInfo->ptBspOps->RfBspIntDisable();
                ucRegVal = 0x7F;
                gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
                ucRegVal = 0x3F;
                gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
		    }
			/* add end */
		    return;
        }
		gucChipAbnormalTag = 0;
        
        /*clearing interrupt*/
        ucRegVal = PHHAL_HW_RC663_BIT_GLOBALIRQ;
        gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
        
        /*clearing LPCDIRQ interrupt*/
        if( ucIrqStatus & PHHAL_HW_RC663_BIT_LPCDIRQ )
        {
            ucRegVal = PHHAL_HW_RC663_BIT_LPCDIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
        }
        
        /*Tov interruptions*/
        if( ucIrqStatus & PHHAL_HW_RC663_BIT_TIMER1IRQ )
        {
            guiRc663ComStatus |= ( 0x10000 | RC663_HW_ERROR_OCCURED );   
            /*clearing interrupt*/
            ucRegVal = PHHAL_HW_RC663_BIT_TIMER1IRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1, &ucRegVal, 1 );
        }
        
        /*read interrupt status*/
        gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_IRQ0, &ucIrqStatus, 1 );
        gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_IRQ0EN, &ucIrqEn, 1 );
        ucIrqStatus &= 0x7F;
        ucIrqStatus &= ucIrqEn; 
        /*during receiving, if there are more than 256 bytes to read.*/
        if( ucIrqStatus & PHHAL_HW_RC663_BIT_HIALERTIRQ )
        { 
            //__printf( "{HIALERTIRQ:%d}\r\n", guiRxNum );
            
            /*clearing interrupt*/
            ucRegVal = PHHAL_HW_RC663_BIT_HIALERTIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
        }
        
        /*during sending, if there are more than 512 bytes to fill*/
        if( ucIrqStatus & PHHAL_HW_RC663_BIT_LOALERTIRQ )
        { 
            //__printf( "{LOALERTIRQ:%d}\r\n", guiTxNum );
            
            /*clearing interrupt*/
            ucRegVal = PHHAL_HW_RC663_BIT_LOALERTIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
        }
        
        /*transmission finished*/
        if( ucIrqStatus & PHHAL_HW_RC663_BIT_TXIRQ )
        {
            /*clearing interrupt*/
            ucRegVal = PHHAL_HW_RC663_BIT_TXIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
            
            ucIrqEn = PHHAL_HW_RC663_BIT_IRQINV|
                      PHHAL_HW_RC663_BIT_RXIRQ | 
                      PHHAL_HW_RC663_BIT_HIALERTIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucIrqEn, 1 );
            
            guiRc663ComStatus |= RC663_HW_TRANSMISSION_COMPLETED;
            //__printf( "TXC!\r\n" );
        }
        
        /*reception finished*/
        if( ucIrqStatus & PHHAL_HW_RC663_BIT_RXIRQ )
        {
            /*clearing interrupt*/
            ucRegVal = PHHAL_HW_RC663_BIT_RXIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
            
            /*read fifo length*/
            gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
            uiRemainNum  = ( ucRegVal & 0x03 ) << 8; 
            gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_FIFOLENGTH, &ucRegVal, 1 );
            uiRemainNum += ucRegVal;
            
            /*process the error case*/
            gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_ERROR, &ucErrorFlag, 1 );      
            if( ucErrorFlag )/*Receiption completed with error*/
            {
                if( ucErrorFlag & 0x13 )/*frame, parity or crc error*/
                {
                    gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_RXBITCTRL, &ucRegVal, 1 );
                    if(   ( 
                             ( 0x12 & ucErrorFlag ) 
                          || ( 
                                 ( 0x01 & ucErrorFlag ) 
                              && ( ( uiRemainNum < 2 ) || ( ucRegVal & 0x07 ) ) 
                             )
                           )
                        && guiPiccActivation
                      )
                    {
                        /*continue receiving, t(recovery)=10etu, restart timer to wait for next receiving*/
                        ucRegVal = 0x14;
                        gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_FIFOCONTROL, &ucRegVal, 1 );
                        ucRegVal = 0x00;
                        gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_WATERLEVEL, &ucRegVal, 1 );

                        ucRegVal = PHHAL_HW_RC663_CMD_RECEIVE;
                        gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_COMMAND, &ucRegVal, 1 );
                        ucRegVal = PHHAL_HW_RC663_BIT_T1RUNNING |
                                   PHHAL_HW_RC663_BIT_T1STARTSTOPNOW;
                        gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_TCONTROL, &ucRegVal, 1 );
                        
                        /*EMV TB306_12 this case is special*/
                        if( ( 0 == uiRemainNum ) && ( ISO14443_TYPEB_STANDARD == gptPcdChipInfo->ucProtocol ) )
                        {
                            guiRc663ComStatus = 0x0100 | RC663_HW_RECEPTION_COMPLETED;   
                        }
                    }
                    else /*parity or crc error*/
                    {
                        guiRc663ComStatus = 0x0100 | RC663_HW_RECEPTION_COMPLETED;
                    }
                    //__printf( "{%X/%X/%X}\r\n", ucErrorFlag, uiRemainNum, ucRegVal );
                }
                else if( ucErrorFlag & 0x04 )/*Collission error*/
                {
                    guiRc663ComStatus = 0x0400 | RC663_HW_RECEPTION_COMPLETED;
                    
                    gptPcdChipInfo->ptBspOps->RfBspRead( PHHAL_HW_RC663_REG_RXCOLL, &ucRegVal, 1 );
                    if( ucRegVal & 0x80 )guiRxLastBits = ucRegVal & 0x7F;
                }
            }
            else /*Receiption completed without error*/
            { 
                guiRc663ComStatus = RC663_HW_RECEPTION_COMPLETED;
                //__printf( "{RxNum=%d}\r\n", guiRxNum );
                
                /*disable all interrupts*/
                ucRegVal = PHHAL_HW_RC663_BIT_IRQINV;
                gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0EN, &ucRegVal, 1 );
                ucRegVal = PHHAL_HW_RC663_BIT_IRQPUSHPULL;
                gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ1EN, &ucRegVal, 1 );
            }

        }
		
		/* add by wanls 2012.05.03 */
		/*IDlE interrupt*/
		if(ucIrqStatus & PHHAL_HW_RC663_BIT_IDLEIRQ)
		{
		    if(gptPcdChipInfo->uiPollTypeFlag & TYPEM_INDICATOR)
		    {
		        guiRc663ComStatus = RC663_HW_DEVICE_IDLE;
		    }
			/*clearing interrupt*/
            ucRegVal = PHHAL_HW_RC663_BIT_IDLEIRQ;
            gptPcdChipInfo->ptBspOps->RfBspWrite( PHHAL_HW_RC663_REG_IRQ0, &ucRegVal, 1 );
		}
    }
    
    return;
}




static int CStringToHexArray( char *pcStr, unsigned char *pucArr )
{
    unsigned char *pucHead = pucArr;
    unsigned char  ucTmp;
    
    while( '\0' != *pcStr )
    {
        if( *pcStr >= '0' && *pcStr <= '9' )ucTmp = *pcStr - '0';
        else if( *pcStr >= 'a' && *pcStr <= 'f' )ucTmp = 10 + ( *pcStr - 'a' );
        else if( *pcStr >= 'A' && *pcStr <= 'F' )ucTmp = 10 + ( *pcStr - 'A' );
        else return -1;
        
        pcStr++;
        if( '\0' != *pcStr )
        {
            ucTmp <<= 4;
            if( *pcStr >= '0' && *pcStr <= '9' )ucTmp |= *pcStr - '0';
            else if( *pcStr >= 'a' && *pcStr <= 'f' )ucTmp |= 10 + ( *pcStr - 'a' );
            else if( *pcStr >= 'A' && *pcStr <= 'F' )ucTmp |= 10 + ( *pcStr - 'A' );
            else return -1;
        }
        pcStr++;
        *pucArr++ = ucTmp;
    }
    
    return ( pucArr - pucHead );
}

/**
 * get parameters value from configuration file RF_PARA1,2,3 region
 * and set PCD parameters
 * 
 * parameter:
 *     ptParam  : [in]
 * 
 * return
 *    zero
 */
int Rc663GetParamTagValue(PICC_PARA *ptParam, unsigned char *pucRfPara)
{
    unsigned char  aucRfCfg[30];
    char           acRfCfgStr[33];
    unsigned char *pucPtr = aucRfCfg + 4;
    unsigned int   uiFlag;
    unsigned int   i;
    unsigned int   uiCount;
    unsigned char  ucRegVal;
    
    if( ReadCfgInfo( "RF_PARA_1", acRfCfgStr ) < 0 )return -1;
    uiCount = 6;
    CStringToHexArray( acRfCfgStr, aucRfCfg );
    uiFlag  = aucRfCfg[3];
    uiFlag |= aucRfCfg[2] << 8;
    uiFlag |= aucRfCfg[1] << 16;
    uiFlag |= aucRfCfg[0] << 24;
    for( i = 0; i < 10; i++ )
    {
        if( uiFlag & ( 1 << ( i + 6 ) ) )
        {
            if( ReadCfgInfo( "RF_PARA_2", acRfCfgStr ) < 0 )return -1;   
            uiCount = 16;
            CStringToHexArray( acRfCfgStr, aucRfCfg + 10 );
            break;
        }
    }
    
    for( i = 0; i < 10; i++ )
    {
        if( uiFlag & ( 1 << ( i + 16 ) ) )
        {
            if( ReadCfgInfo( "RF_PARA_3", acRfCfgStr ) < 0 )return -1;   
            uiCount = 26;
            CStringToHexArray( acRfCfgStr, aucRfCfg + 20 );
            break;
        }
    }

	/* add by wanls 2015.05.03*/
    memcpy(&pucRfPara[1], aucRfCfg, (uiCount + 4));
	pucRfPara[0] = (char)(uiCount + 4);
	/* add end */
	
    for( i = 0; i < uiCount; i++ )
    {
        if( ( 1 << i ) & uiFlag )
        {
            /*Type A conduct value*/
            if( 0 == i )
            {
                if( pucPtr[i] > 7 )pucPtr[i] = 0x04;
                ptParam->a_conduct_val = pucPtr[i];  
                
                /*A*/
                ucRegVal = Rc663GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
                ucRegVal &= 0x3F;
                ucRegVal |= ( ( ptParam->a_conduct_val & 0x03 ) << 6 );
                Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                                      PHHAL_HW_RC663_REG_TXAMP,
                                      ucRegVal );
                ucRegVal  = Rc663GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                                 PHHAL_HW_RC663_REG_DRVCON );             
                ucRegVal &= 0xF7;
                ucRegVal |= ( ( ptParam->a_conduct_val & 0x04 ) << 1 );
                Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                                      PHHAL_HW_RC663_REG_DRVCON,
                                      ucRegVal );
                /*B*/
                ucRegVal = Rc663GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
                ucRegVal &= 0x3F;
                ucRegVal |= ( ( ptParam->a_conduct_val & 0x03 ) << 6 );
                Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      PHHAL_HW_RC663_REG_TXAMP,
                                      ucRegVal );
                ucRegVal  = Rc663GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                                 PHHAL_HW_RC663_REG_DRVCON );             
                ucRegVal &= 0xF7;
                ucRegVal |= ( ( ptParam->a_conduct_val & 0x04 ) << 1 );
                Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      PHHAL_HW_RC663_REG_DRVCON,
                                      ucRegVal );
                /*C*/
                ucRegVal = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
                ucRegVal &= 0x3F;
                ucRegVal |= ( ( ptParam->a_conduct_val & 0x03 ) << 6 );
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PHHAL_HW_RC663_REG_TXAMP,
                                      ucRegVal );
                ucRegVal  = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                                 PHHAL_HW_RC663_REG_DRVCON );             
                ucRegVal &= 0xF7;
                ucRegVal |= ( ( ptParam->a_conduct_val & 0x04 ) << 1 );
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PHHAL_HW_RC663_REG_DRVCON,
                                      ucRegVal );
            }
            /*Type m conduct value*/
            if( 1 == i )ptParam->m_conduct_val = pucPtr[i];
            /*Type B modulate value*/
            if( 2 == i )
            {
                ptParam->b_modulate_val = pucPtr[i];        
                ucRegVal = Rc663GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
                ucRegVal &= 0xE0;
                ucRegVal |= ptParam->b_modulate_val & 0x1F;
                Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      PHHAL_HW_RC663_REG_TXAMP,
                                      ucRegVal );
            }
            /*Type B Receive Threshold value*/
            if( 3 == i )
            {
                ptParam->card_RxThreshold_val = pucPtr[i]; 
                Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                      PHHAL_HW_RC663_REG_RXTHRESHOLD, 
                                      ptParam->card_RxThreshold_val );
            }
            /*Type C (Felica) modulate value*/
            if( 4 == i )
            {
                ptParam->f_modulate_val = pucPtr[i];        
                ucRegVal = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
                ucRegVal &= 0xE0;
                ucRegVal |= ptParam->f_modulate_val & 0x1F;
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PHHAL_HW_RC663_REG_TXAMP,
                                      ucRegVal );
            }
            /*Type A modulate value*/
            if( 5 == i )ptParam->a_modulate_val = pucPtr[i];
            /*Type A Receive Threshold value*/
            if( 6 == i )
            {
                ptParam->a_card_RxThreshold_val = pucPtr[i];    
                Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                      PHHAL_HW_RC663_REG_RXTHRESHOLD, 
                                      ptParam->a_card_RxThreshold_val );
            }
            /*added by liubo for rc663*/
            /*Type C(Felica) Receive Threshold value*/
            if( 7 == i )
            {   
                ptParam->f_card_RxThreshold_val = pucPtr[i];
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD, 
                                      PHHAL_HW_RC663_REG_RXTHRESHOLD, 
                                      ptParam->f_card_RxThreshold_val );
            }
            /*Type A antenna gain value*/
            if( 8 == i )
            {
                ptParam->a_card_antenna_gain_val = pucPtr[i]; 
                Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                      PHHAL_HW_RC663_REG_RXANA, 
                                      ptParam->a_card_antenna_gain_val );
            }
            /*Type B antenna gain value*/   
            if( 9 == i )
            {
                ptParam->b_card_antenna_gain_val = pucPtr[i];    
                Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                      PHHAL_HW_RC663_REG_RXANA, 
                                      ptParam->b_card_antenna_gain_val );
            }
            /*Felica antenna gain value*/
            if( 10 == i )
            {
                ptParam->f_card_antenna_gain_val = pucPtr[i];    
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD, 
                                      PHHAL_HW_RC663_REG_RXANA, 
                                      ptParam->f_card_antenna_gain_val );
            }

			/* add by wanls 2012.08.14 */
			if( 11 == i)
			{
			    if( pucPtr[i] > 7 )pucPtr[i] = 0x04;
                ptParam->f_conduct_val = pucPtr[i];
				
			    ucRegVal = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
                ucRegVal &= 0x3F;
                ucRegVal |= ( ( ptParam->f_conduct_val & 0x03 ) << 6 );
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PHHAL_HW_RC663_REG_TXAMP,
                                      ucRegVal );
                ucRegVal  = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                                 PHHAL_HW_RC663_REG_DRVCON );             
                ucRegVal &= 0xF7;
                ucRegVal |= ( ( ptParam->f_conduct_val & 0x04 ) << 1 );
                Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PHHAL_HW_RC663_REG_DRVCON,
                                      ucRegVal );
			}
			/* add end */
        }
        
    }
    
    return 0;
}

/**
 * set PCD parameters base on PICC_PARA
 * 
 * parameter:
 *     ptPiccPara  : [in]
 * 
 * return
 *    zero
 */
int Rc663SetParamValue(PICC_PARA *ptPiccPara )
{
    unsigned char ucRegVal;
    
    /*RF carrier amplifier*/
    if( 1 == ptPiccPara->a_conduct_w )
	{
		if( ptPiccPara->a_conduct_val > 7 )ptPiccPara->a_conduct_val = 0x04;
		/*A*/
        ucRegVal = Rc663GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                         PHHAL_HW_RC663_REG_TXAMP );             
        ucRegVal &= 0x3F;
        ucRegVal |= ( ( ptPiccPara->a_conduct_val & 0x03 ) << 6 );
        Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                              PHHAL_HW_RC663_REG_TXAMP,
                              ucRegVal );
        ucRegVal  = Rc663GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                         PHHAL_HW_RC663_REG_DRVCON );             
        ucRegVal &= 0xF7;
        ucRegVal |= ( ( ptPiccPara->a_conduct_val & 0x04 ) << 1 );
        Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                              PHHAL_HW_RC663_REG_DRVCON,
                              ucRegVal );
        /*B*/
        ucRegVal = Rc663GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                         PHHAL_HW_RC663_REG_TXAMP );             
        ucRegVal &= 0x3F;
        ucRegVal |= ( ( ptPiccPara->a_conduct_val & 0x03 ) << 6 );
        Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                              PHHAL_HW_RC663_REG_TXAMP,
                              ucRegVal );
        ucRegVal  = Rc663GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                         PHHAL_HW_RC663_REG_DRVCON );             
        ucRegVal &= 0xF7;
        ucRegVal |= ( ( ptPiccPara->a_conduct_val & 0x04 ) << 1 );
        Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                              PHHAL_HW_RC663_REG_DRVCON,
                              ucRegVal );
	}
	
	/*TypeB modulation deepth*/
	if( 1 == ptPiccPara->b_modulate_w )
	{
	    if( ptPiccPara->b_modulate_val > 0x1F )ptPiccPara->b_modulate_val = 0x10;
        ucRegVal = Rc663GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                                 PHHAL_HW_RC663_REG_TXAMP );             
        ucRegVal &= 0xE0;
        ucRegVal |= ptPiccPara->b_modulate_val & 0x1F;
        Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                              PHHAL_HW_RC663_REG_TXAMP,
                              ucRegVal );
	}
	
	/*Type B Receive Threshold value*/
    if( 1 == ptPiccPara->card_RxThreshold_w )
    { 
        Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                              PHHAL_HW_RC663_REG_RXTHRESHOLD, 
                              ptPiccPara->card_RxThreshold_val );
    }
    /*Type C (Felica) modulate value*/
    if( 1 == ptPiccPara->f_modulate_w )
    {  
        if( ptPiccPara->f_modulate_val > 0x1F )ptPiccPara->f_modulate_val = 0x10;
        ucRegVal = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                         PHHAL_HW_RC663_REG_TXAMP );             
        ucRegVal &= 0xE0;
        ucRegVal |= ptPiccPara->f_modulate_val & 0x1F;
        Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                              PHHAL_HW_RC663_REG_TXAMP,
                              ucRegVal );
    }
   
    /*Type A Receive Threshold value*/
    if( 1 == ptPiccPara->a_card_RxThreshold_w )
    {    
        Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                             PHHAL_HW_RC663_REG_RXTHRESHOLD, 
                             ptPiccPara->a_card_RxThreshold_val );
    }

    /*Type C(Felica) Receive Threshold value*/
    if( 1 == ptPiccPara->f_card_RxThreshold_w )
    {   
        Rc663SetRegisterConfigVal( JISX6319_4_STANDARD, 
                             PHHAL_HW_RC663_REG_RXTHRESHOLD, 
                             ptPiccPara->f_card_RxThreshold_val );
    }
    /*Type A antenna gain value*/
    if( 1 == ptPiccPara->a_card_antenna_gain_w )
    {
        Rc663SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                             PHHAL_HW_RC663_REG_RXANA, 
                             ptPiccPara->a_card_antenna_gain_val );
    }
    /*Type B antenna gain value*/   
    if( 1 == ptPiccPara->b_card_antenna_gain_w )
    {   
        Rc663SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                             PHHAL_HW_RC663_REG_RXANA, 
                             ptPiccPara->b_card_antenna_gain_val );
    }
    /*Felica antenna gain value*/
    if( 1 == ptPiccPara->f_card_antenna_gain_w )
    {    
        Rc663SetRegisterConfigVal( JISX6319_4_STANDARD, 
                             PHHAL_HW_RC663_REG_RXANA, 
                             ptPiccPara->f_card_antenna_gain_val );
    }

	/* add by wanls 2012.08.14 */
	if( 1 == ptPiccPara->f_conduct_w)
	{
        /*C*/
		if( ptPiccPara->f_conduct_val > 7 )ptPiccPara->f_conduct_val = 0x04;
		
        ucRegVal = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                         PHHAL_HW_RC663_REG_TXAMP );             
        ucRegVal &= 0x3F;
        ucRegVal |= ( ( ptPiccPara->f_conduct_val & 0x03 ) << 6 );
        Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                              PHHAL_HW_RC663_REG_TXAMP,
                              ucRegVal );
        ucRegVal  = Rc663GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                         PHHAL_HW_RC663_REG_DRVCON );             
        ucRegVal &= 0xF7;
        ucRegVal |= ( ( ptPiccPara->f_conduct_val & 0x04 ) << 1 );
        Rc663SetRegisterConfigVal( JISX6319_4_STANDARD,
                              PHHAL_HW_RC663_REG_DRVCON,
                              ucRegVal );
	}
	/* add end */


	
    return 0;
}




/* add by wanls 2012.08.14 */
struct ST_PCDOPS * GetRc663OpsHandle()
{
    return (struct ST_PCDOPS *)&gtRc663Ops;
}
/* add end */

