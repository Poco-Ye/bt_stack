/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : pn512_driver.c
 *
 * Author : WanliShan     
 * 
 * Date   : 2012-11-02
 *
 * Description:
 *
 *
 * History:
 *
 * =============================================================================
 */
#include "../../protocol/iso14443hw_hal.h"
#include "../../protocol/mifare.h"
#include "../../protocol/pcd_apis.h"
#include "pn512_driver.h"
#include "pn512_regs.h"
#include "pn512_regs_conf.h"
#include <stdlib.h>
#include <string.h>

/**
 * these are global variants in PN512 driver.
 */
static volatile unsigned int   guiRxLastBits; 
static volatile unsigned int   gucErrorState = 0;
static volatile unsigned int   guiPiccActivation;
static volatile unsigned int   guiDeltaT;
static volatile unsigned int   guiBytesTransmit = 0;
static volatile unsigned int   guiBytesReceive = 0;
static volatile unsigned char  gucIrqState = 0;

static volatile unsigned int   gucChipAbnormalTag = 0;


static struct ST_PCDINFO*      gptPcdChipInfo;
/**
 * the driver operations list
 */
const struct ST_PCDOPS gtPn512Ops = 
{
    Pn512Init,
   
    Pn512CarrierOn,
    Pn512CarrierOff,
    
    Pn512Config,
    
    Pn512Waiter,
    
    Pn512Trans,
    Pn512TransCeive,
    Pn512MifareAuthenticate,
    
    Pn512CommIsr,

	Pn512GetParamTagValue,
	Pn512SetParamValue
	
};


/**
 * initialising Pn512
 * 
 * parameters:
 *             no parameter
 * reval:
 *             no return value
 */
int Pn512Init( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    unsigned char aucFiFo[12];
    
    int           i;
    
    guiDeltaT = 16;

	/* Modulation signal (envelope) from the internal coder */
	ucRegVal = 0x10;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TXSEL_REG, &ucRegVal, 1 );

	/* Irq Inv */
	ucRegVal = 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_COMMIEN_REG, &ucRegVal, 1 );
	/* IRQPushPull=1 */
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_DIVIEN_REG, &ucRegVal, 1 );
	
    /* Set the PN512 acts as initiator*/
	ptPcdInfo->ptBspOps->RfBspRead(PN512_CONTROL_REG, &ucRegVal, 1 );
	ucRegVal |= 0x10;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_CONTROL_REG, &ucRegVal, 1 );

	/* Clear FiFo buffer */
	ucRegVal = 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_FIFOLEVEL_REG, &ucRegVal, 1);
	
	/* Write and Read FiFo, Test spi is ok */
	memcpy(aucFiFo, "\x33\xCC\x66\x99\x55\xAA", 6);
	for(i = 0; i < 6; i++)
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_FIFODATA_REG, &aucFiFo[i], 1);
	
	memset(aucFiFo, 0x00, sizeof(aucFiFo));
	for(i = 0; i < 6; i++)
	ptPcdInfo->ptBspOps->RfBspRead(PN512_FIFODATA_REG, &aucFiFo[i], 1);

	if( 0 != memcmp( aucFiFo, "\x33\xcc\x66\x99\x55\xaa", 6 ) )
	{
	    return 0xFF;
	}

	/* Clear FiFo buffer */
	ucRegVal = 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_FIFOLEVEL_REG, &ucRegVal, 1);

    /*provide these functions for ISR*/
    gptPcdChipInfo = ptPcdInfo;

    return 0; 
}

/**
 * Setting time out parameters
 */
int Pn512SetDeltaT( int delta_t )
{
    guiDeltaT = delta_t;
    
    return 0;   
}

/**
 * configurating Pn512
 * 
 * parameters:
 *            no paramters
 * reval:
 *            no return value
 */
int Pn512Config( struct ST_PCDINFO* ptPcdInfo )
{     
    unsigned char ucRegVal = 0;
    unsigned int  uiTimeOut= 0;
	unsigned int  uiTemp = 0;
	unsigned int uiTempClk = 0;
	unsigned int uiScaler = 0; 
    
    /*terminate current command*/
    ucRegVal = PN512_IDLE_CMD;
	/*change by wanls 20140303 修正寄存器地址*/
    ptPcdInfo->ptBspOps->RfBspWrite(PN512_COMMAND_REG, &ucRegVal, 1 );
	/*change end*/
    
    if( ptPcdInfo->ucEmdEn )guiPiccActivation = 1;
    else guiPiccActivation = 0;   
    
    /**
     * every type initialised configuration
     */
    switch( ptPcdInfo->ucProtocol )
    {
    case ISO14443_TYPEA_STANDARD:
	case MIFARE_STANDARD:
        Pn512Iso14443TypeAInit( ptPcdInfo );
    break;
    case ISO14443_TYPEB_STANDARD:
        Pn512Iso14443TypeBInit( ptPcdInfo );
    break;
    case JISX6319_4_STANDARD:/*Felica*/
        Pn512Jisx6319_4Init( ptPcdInfo );
    break;
    default:
    break;
    }

    /* Pn512 configuration */
    switch( ptPcdInfo->ucProtocol )
    {
    case ISO14443_TYPEA_STANDARD:
	case MIFARE_STANDARD:
    		//ScrPrint1("Type A");
        /*transmitter's digital uart configurations*/
        if( ISO14443_TYPEA_SHORT_FRAME == ptPcdInfo->ucFrameMode )
        {
            //ScrPrint1("Short Frame");
            ptPcdInfo->uiPcdTxRLastBits = 7;
            ucRegVal = 0x07;
        }
        else if( ISO14443_TYPEA_STANDARD_FRAME == ptPcdInfo->ucFrameMode )
        {
            ptPcdInfo->uiPcdTxRLastBits = 8;
            ucRegVal = 0x00;   
        }
		/*the bits number of the last bytes*/
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_BITFRAMING_REG, &ucRegVal, 1 );
        
        if( ptPcdInfo->ucTxCrcEnable )ucRegVal = 0x80;/*CRC16:0x6363*/
        else ucRegVal = 0x00;   
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_TXMODE_REG, &ucRegVal, 1 );
        
        if( ptPcdInfo->ucRxCrcEnable )ucRegVal = 0x80;/*crc will be only checked*/
        else ucRegVal = 0x00;
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_RXMODE_REG, &ucRegVal, 1 );
        
        /*the configuration in 106Kbps mode*/
        Pn512Iso14443TypeAConf( ptPcdInfo ); 
        
        if( !ptPcdInfo->ucM1CryptoEn )/*disable crypto1 with Mifare standard*/
        {
			ptPcdInfo->ptBspOps->RfBspRead( PN512_STATUS2_REG, &ucRegVal, 1 );
            ucRegVal &= (~PN512_BIT_CRYPTO1ON);    
            ptPcdInfo->ptBspOps->RfBspWrite( PN512_STATUS2_REG, &ucRegVal, 1 );  
        }
    break;
    case ISO14443_TYPEB_STANDARD:
        Pn512Iso14443TypeBConf( ptPcdInfo );
        
        /*disable crypto1 with mifare*/
		ptPcdInfo->ptBspOps->RfBspRead( PN512_STATUS2_REG, &ucRegVal, 1 );
        ucRegVal &= (~PN512_BIT_CRYPTO1ON);    
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_STATUS2_REG, &ucRegVal, 1 );  
    break;
    case JISX6319_4_STANDARD:
        Pn512Jisx6319_4Conf( ptPcdInfo );
        
        /*disable crypto1 with mifare*/
		ptPcdInfo->ptBspOps->RfBspRead( PN512_STATUS2_REG, &ucRegVal, 1 );
        ucRegVal &= (~PN512_BIT_CRYPTO1ON);    
        ptPcdInfo->ptBspOps->RfBspWrite( PN512_STATUS2_REG, &ucRegVal, 1 );  
    break;
    default:
    break;
    }
    
    uiTimeOut = ptPcdInfo->uiFwt + guiDeltaT;
    
    /*configurating FWT*/

	/*Stop Timer*/
	ptPcdInfo->ptBspOps->RfBspRead(PN512_CONTROL_REG, &ucRegVal, 1 );
	ucRegVal |= 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_CONTROL_REG, &ucRegVal, 1 );
	
    /*set TPrescalEven to logic 0 the following formula is used to calculate fTimer of the
    prescaler:fTimer = 13.56 MHz / (2 * TPreScaler + 1).*/
    ptPcdInfo->ptBspOps->RfBspRead(PN512_DEMOD_REG, &ucRegVal, 1 );
	ucRegVal &= ~0x10;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_DEMOD_REG, &ucRegVal, 1 );

	uiTemp = uiTimeOut / 512;
	uiScaler = (uiTemp + 1) / 2;

	/* Set Time Prescaler Register */
	ucRegVal = (unsigned char)(uiScaler >> 8) & 0x0F;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TMODE_REG, &ucRegVal, 1 );  
	
	ucRegVal = (unsigned char)(uiScaler & 0xFF);
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TPRESCALER_REG, &ucRegVal, 1 );  
	
	uiTempClk = (128 * uiTimeOut) / (2 * uiScaler + 1);

	/* Load time value */
	ucRegVal = (unsigned char)(uiTempClk >> 8) & 0xFF;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TRELOAD_H_REG, &ucRegVal, 1 ); 
	
	ucRegVal = (unsigned char)(uiTempClk & 0xFF);
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TRELOAD_L_REG, &ucRegVal, 1 ); 

    
    gptPcdChipInfo = ptPcdInfo;
    
    return 0;
}

/**
 * PN512 waiter
 */
int Pn512Waiter( struct ST_PCDINFO* ptPcdInfo, unsigned int uiEtuCount )
{

	unsigned char ucRegVal = 0;
	unsigned int  uiTemp = 0;
	unsigned int uiTempClk = 0;
	unsigned int uiScaler = 0; 
	
	if( !uiEtuCount )return 0;
	
	/*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();

    /*Stop Timer*/
	ptPcdInfo->ptBspOps->RfBspRead(PN512_CONTROL_REG, &ucRegVal, 1 );
	ucRegVal |= 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_CONTROL_REG, &ucRegVal, 1 );
	
    /*set TPrescalEven to logic 0 the following formula is used to calculate fTimer of the
    prescaler:fTimer = 13.56 MHz / (2 * TPreScaler + 1).*/
    ptPcdInfo->ptBspOps->RfBspRead(PN512_DEMOD_REG, &ucRegVal, 1 );
	ucRegVal &= ~0x10;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_DEMOD_REG, &ucRegVal, 1 );

	uiTemp = uiEtuCount / 512;
	uiScaler = (uiTemp + 1) / 2;

	/* Set Time Prescaler Register */
	ucRegVal = (unsigned char)(uiScaler >> 8) & 0x0F;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TMODE_REG, &ucRegVal, 1 );  
	
	ucRegVal = (unsigned char)(uiScaler & 0xFF);
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TPRESCALER_REG, &ucRegVal, 1 );  
	
	uiTempClk = (128 * uiEtuCount) / (2 * uiScaler + 1);

	/* Load time value */
	ucRegVal = (unsigned char)(uiTempClk >> 8) & 0xFF;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TRELOAD_H_REG, &ucRegVal, 1 ); 
	
	ucRegVal = (unsigned char)(uiTempClk & 0xFF);
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TRELOAD_L_REG, &ucRegVal, 1 ); 

	/* Start Timer */
	ptPcdInfo->ptBspOps->RfBspRead(PN512_CONTROL_REG, &ucRegVal, 1 );
	ucRegVal |= 0x40;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_CONTROL_REG, &ucRegVal, 1 );

	do
    {
		ptPcdInfo->ptBspOps->RfBspRead(PN512_STATUS1_REG, &ucRegVal, 1 );
		
  	}while(ucRegVal & RN512_BIT_TIMERUNNING);

    return 0;	
	
}

/**
 * Pn512 open carrier
 * 
 * parameter:
 *            no param
 * reval:
 *            ignore return value
 */
int Pn512CarrierOn( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
	ucRegVal = 0xA3;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_TXCONTROL_REG, &ucRegVal, 1 );
	/* delete by nt for paypass 3.0 performance test. 2013/03/11 pay attention,must be delete.!!!*/
//	ptPcdInfo->uiPcdState = PCD_CARRIER_ON;
	/* Delay 7ms */
	ptPcdInfo->ptPcdOps->PcdChipWaiter(ptPcdInfo, 740);
	
    return 0;
}

/**
 * Pn512 close carrier
 * 
 * parameter:
 *            no param
 * reval:
 *            ignore return value
 */
int Pn512CarrierOff( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
	ucRegVal = 0x30;
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_TXCONTROL_REG, &ucRegVal, 1 );
  /* delete by nt for paypass 3.0 performance test. 2013/03/11 pay attention,must be delete.!!!*/
  //  ptPcdInfo->uiPcdState = PCD_CARRIER_OFF;

	/* Delay 7ms */
	ptPcdInfo->ptPcdOps->PcdChipWaiter(ptPcdInfo, 740);

    
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
int Pn512MifareAuthenticate( struct ST_PCDINFO* ptPcdInfo )
{
	unsigned char  ucRegVal = 0;
    unsigned char  ucIrqEn  = 0;
    int            i        = 0;
	unsigned int   uiBeginTime = 0;
	unsigned char aucTransmitBuffer[15];

    guiBytesTransmit = 0;
	guiBytesReceive = 0;
	gucIrqState = 0;
	gucErrorState = 0;
	gucChipAbnormalTag = 0;
    
    /*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
	
    ucRegVal = PN512_BIT_IRQPUSHPULL;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_DIVIEN_REG, &ucRegVal, 1 );
    
	/* Clear Irq */
	ucRegVal = 0x7F;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMIRQ_REG, &ucRegVal, 1 );
	ucRegVal = 0x1F;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_DIVIRQ_REG, &ucRegVal, 1 );

	/* Configure interrrupts */
    ucIrqEn = PN512_BIT_IRQINV | PN512_BIT_IDLEIEN | PN512_BIT_TIMERIEN;
	
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_COMMIEN_REG, &ucIrqEn, 1 );

	/* Settings for the timer, Start after Tx, end after Rx */
	ptPcdInfo->ptBspOps->RfBspRead( PN512_TMODE_REG, &ucRegVal, 1 );
	ucRegVal |= 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_TMODE_REG, &ucRegVal, 1 );

	/* Clear FIFO */
	ucRegVal = PN512_BIT_FLUSHBUFFER;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_FIFOLEVEL_REG, &ucRegVal, 1 );

	/* Set the waterleve is 16 */
	ucRegVal = PN512_FIFO_WATERLEVEL;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_WATERLEVEL_REG, &ucRegVal, 1);

	/* fill data in FIFO */
	memset(aucTransmitBuffer, 0x00, sizeof(aucTransmitBuffer));
	/* write command into fifo, '60' or '61', blkno */
	memcpy(&aucTransmitBuffer[0], &ptPcdInfo->aucPcdTxRBuffer[0], 2);
	/* fill key */ 
	memcpy(&aucTransmitBuffer[2], &ptPcdInfo->aucPcdTxRBuffer[6], 6);
	/* fill UID  */ 
	memcpy(&aucTransmitBuffer[8], &ptPcdInfo->aucPcdTxRBuffer[2], 4);

	for(i = 0; i < 12; i++)
	{   
		ptPcdInfo->ptBspOps->RfBspWrite(PN512_FIFODATA_REG, &aucTransmitBuffer[i], 1);
	}
	guiBytesTransmit += 12;

    /*enable interrupt*/
    ptPcdInfo->ptBspOps->RfBspIntEnable();

    /*start tansmission*/
    ucRegVal = PN512_MFAUTHENT_CMD;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );

    /*waiting for reception completed*/    
    uiBeginTime = GetTimerCount();
    while(!((gucIrqState & PN512_BIT_IDLEIRQ ) || (gucIrqState & PN512_BIT_TIMERIRQ)))
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 7000)
        {
            gucErrorState |= PN512_ERROR_TIMEOUT;
            break;
        }
    }
	
    gptPcdChipInfo->ptBspOps->RfBspIntDisable();

	/* add by wanls 2013.10.08*/
	/*处理认证失败时，调用PiccOpen会失败的问题。 原因是认证不成功时，
	认证命令是不会被自动终止，即一直处于认证命令阶段，此阶段FIFO的访
	问是被阻止的。而PiccOpen中有读写FIFO数据判断芯片是否正常的操作，所
	以出现认证失败时，PiccOpen失败的问题*/
	ucRegVal = PN512_IDLE_CMD; 
	gptPcdChipInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );
	
	/* add end */

	

	ptPcdInfo->uiPcdTxRNum      = 0;
    ptPcdInfo->uiPcdTxRLastBits = 0;
	
    if(gucErrorState & PN512_ERROR_TIMEOUT)return ISO14443_HW_ERR_COMM_TIMEOUT;
	
    ptPcdInfo->ptBspOps->RfBspRead(PN512_STATUS2_REG, &ucRegVal, 1 );
	if(!(ucRegVal & PN512_BIT_MFCRYPTO1ON))return PHILIPS_MIFARE_ERR_AUTHEN; 
	
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
int Pn512Trans( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char  ucRegVal = 0;
    unsigned char  ucIrqEn  = 0;
    unsigned int   uiTxNum  = 0;
    int            i        = 0;
	unsigned int   uiBeginTime = 0;
	
	guiBytesTransmit = 0;
	guiBytesReceive = 0;
	gucIrqState = 0;
	gucErrorState = 0;
	gucChipAbnormalTag = 0;
    
    /*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
	
    ucRegVal = PN512_BIT_IRQPUSHPULL;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_DIVIEN_REG, &ucRegVal, 1 );
    
	/* Clear Irq */
	ucRegVal = 0x7F;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMIRQ_REG, &ucRegVal, 1 );
	ucRegVal = 0x1F;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_DIVIRQ_REG, &ucRegVal, 1 );

	/* Configure interrrupts */
    ucIrqEn = PN512_BIT_IRQINV | PN512_BIT_TXIEN 
    | PN512_BIT_IDLEIEN | PN512_BIT_LOALERTIEN;
	
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_COMMIEN_REG, &ucIrqEn, 1 );

	/* Clear FIFO */
	ucRegVal = PN512_BIT_FLUSHBUFFER;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_FIFOLEVEL_REG, &ucRegVal, 1 );

	/* Set the waterleve is 16 */
	ucRegVal = PN512_FIFO_WATERLEVEL;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_WATERLEVEL_REG, &ucRegVal, 1);

	/* fill data in FIFO */
	uiTxNum = ptPcdInfo->uiPcdTxRNum;
	if(uiTxNum > PN512_FIFO_SIZE)
	{
		uiTxNum = PN512_FIFO_SIZE;
	}
	
	for(i = 0; i < uiTxNum; i++)
	{
		ptPcdInfo->ptBspOps->RfBspWrite(PN512_FIFODATA_REG, &ptPcdInfo->aucPcdTxRBuffer[i], 1);
	}
	guiBytesTransmit += uiTxNum;

    /*enable interrupt*/
    ptPcdInfo->ptBspOps->RfBspIntEnable();

    /*start tansmission*/
    ucRegVal = PN512_TRANSMIT_CMD ;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );

    /*waiting for reception completed*/    
    uiBeginTime = GetTimerCount();
    while(!((gucIrqState & PN512_BIT_IDLEIRQ ) || (gucIrqState & PN512_BIT_TXIRQ)))
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 6000)
        {
            gucErrorState |= PN512_ERROR_TIMEOUT;
            break;
        }
    }

	/* Set Idle State */
	ucRegVal = PN512_IDLE_CMD; 
	gptPcdChipInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );

    gptPcdChipInfo->ptBspOps->RfBspIntDisable();
    
    return 0;
}

/**
 * Write datas into the FIFO of Pn512 and receive datas from picc
 * 
 * parameter:
 *            ptPcdInfo : the handle of device.
 * 
 * reval:
 *            0 - ok
 *            others value, error 
 */
int Pn512TransCeive( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    unsigned char ucIrqEn  = 0;
    unsigned int  uiTxNum  = 0;
    int           i;
	unsigned int  uiBeginTime = 0;
	guiBytesTransmit = 0;
	guiBytesReceive = 0;
	gucIrqState = 0;
	gucErrorState = 0;
	gucChipAbnormalTag = 0;
    
    /*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();

	/* Set IDLE State */
    ucRegVal = PN512_IDLE_CMD;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );
	
    ucRegVal = PN512_BIT_IRQPUSHPULL;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_DIVIEN_REG, &ucRegVal, 1 );
    
	/* Clear Irq */
	ucRegVal = 0x7F;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMIRQ_REG, &ucRegVal, 1 );
	ucRegVal = 0x1F;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_DIVIRQ_REG, &ucRegVal, 1 );

	/* Configure interrrupts */
    ucIrqEn = PN512_BIT_IRQINV | PN512_BIT_TXIEN | PN512_BIT_RXIEN 
		| PN512_BIT_IDLEIEN | PN512_BIT_LOALERTIEN | PN512_BIT_TIMERIEN ;
	
	ptPcdInfo->ptBspOps->RfBspWrite(PN512_COMMIEN_REG, &ucIrqEn, 1 );

	/* Settings for the timer, Start after Tx, end after Rx */
	ptPcdInfo->ptBspOps->RfBspRead( PN512_TMODE_REG, &ucRegVal, 1 );
	ucRegVal |= 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_TMODE_REG, &ucRegVal, 1 );

	/* Clear FIFO */
	ucRegVal = PN512_BIT_FLUSHBUFFER;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_FIFOLEVEL_REG, &ucRegVal, 1 );

	/* Set the waterleve is 16 */
	ucRegVal = PN512_FIFO_WATERLEVEL;
	ptPcdInfo->ptBspOps->RfBspWrite( PN512_WATERLEVEL_REG, &ucRegVal, 1);

	/* fill data in FIFO */
	uiTxNum = ptPcdInfo->uiPcdTxRNum;
	if(uiTxNum > PN512_FIFO_SIZE)
	{
		uiTxNum = PN512_FIFO_SIZE;
	}
	for(i = 0; i < uiTxNum; i++)
	{
		ptPcdInfo->ptBspOps->RfBspWrite(PN512_FIFODATA_REG, &ptPcdInfo->aucPcdTxRBuffer[i], 1);
	}
	guiBytesTransmit += uiTxNum;

    /*enable interrupt*/
    ptPcdInfo->ptBspOps->RfBspIntEnable();

    /*start tansmission*/
    ucRegVal = PN512_TRANSCEIVE_CMD;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );

    /*Start Tx,This bit is only valid in combination with the Transceive command*/
    ptPcdInfo->ptBspOps->RfBspRead( PN512_BITFRAMING_REG, &ucRegVal, 1 );
	ucRegVal |= PN512_BIT_START_SEND;
    ptPcdInfo->ptBspOps->RfBspWrite( PN512_BITFRAMING_REG, &ucRegVal, 1 );

    /*waiting for reception completed*/    
    uiBeginTime = GetTimerCount();
    while(!((gucIrqState & PN512_BIT_IDLEIRQ) || (gucIrqState & PN512_BIT_TIMERIRQ)))
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 6000)
        {
            gucErrorState |= PN512_ERROR_TIMEOUT;
            break;
        }
    }
    /* disable interruption*/
    gptPcdChipInfo->ptBspOps->RfBspIntDisable();

	/* Set Idle State */
	ucRegVal = PN512_IDLE_CMD; 
	gptPcdChipInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );

    /* timeout*/
    if(gucErrorState & PN512_ERROR_TIMEOUT)return ISO14443_HW_ERR_COMM_TIMEOUT;
    /* fifo overflow */
    if(gucErrorState & PN512_ERROR_BUFFEROVFL) return ISO14443_HW_ERR_COMM_FIFO;
    /* frame error */
    if(gucErrorState & PN512_ERROR_PROTOCOLERR)return ISO14443_HW_ERR_COMM_FRAME; 
    /* parity error */
    if(gucErrorState & PN512_ERROR_PARITYERR)return ISO14443_HW_ERR_COMM_PARITY;
	/* CRC error */
	if(gucErrorState & PN512_ERROR_CRCERR)return ISO14443_HW_ERR_COMM_CRC;
    /* collision error */
    if(gucErrorState & PN512_ERROR_COLLERR)return ISO14443_HW_ERR_COMM_COLL;
    
    
    return 0;
}

/**
 * Pn512 interrupt service routine
 * 
 * parameter:
 *            no parameter
 *
 * reval:
 *            no return value
 */
void Pn512CommIsr()
{
    unsigned int uiRemainNum = 0;
	unsigned int uiRxNum = 0;
	unsigned int uiTxNum = 0;
	unsigned int i = 0;
    unsigned char ucIrqStatus = 0;
    unsigned char ucIrqEn     = 0;
    unsigned char ucErrorFlag = 0;
    unsigned char ucRegVal    = 0;
	unsigned char ucEableReReceive = 0;
	unsigned char ucFIFOLength = 0;

    while(1)
	{	
	    gucChipAbnormalTag++;
	    /* if no interrupt, return */
		gptPcdChipInfo->ptBspOps->RfBspRead(PN512_STATUS1_REG, &ucRegVal, 1);
		if((ucRegVal & PN512_BIT_IRQ) == 0)
		{
		    if(gucChipAbnormalTag > 100)
		    {
				gptPcdChipInfo->ptBspOps->RfBspIntDisable();
                ucRegVal = 0x7F;
				gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);
				ucRegVal = 0x1F;
	            gptPcdChipInfo->ptBspOps->RfBspWrite( PN512_DIVIRQ_REG, &ucRegVal, 1 );
		
				gucIrqState |= PN512_BIT_IDLEIRQ;
		    }
			return;
		}
		gucChipAbnormalTag = 0;

		gptPcdChipInfo->ptBspOps->RfBspRead(PN512_COMMIEN_REG, &ucIrqEn, 1);
		gptPcdChipInfo->ptBspOps->RfBspRead(PN512_COMMIRQ_REG, &ucIrqStatus, 1);
		ucIrqStatus &= ucIrqEn;
		gucIrqState |= ucIrqStatus;
		//if(!ucIrqStatus)return;

		/* Deal with LoAlert interrupt */
		if(ucIrqStatus & PN512_BIT_LOALERTIRQ)
		{
			/* Read FIFO Length */
			gptPcdChipInfo->ptBspOps->RfBspRead(PN512_FIFOLEVEL_REG, &ucFIFOLength, 1);
			
			uiTxNum = gptPcdChipInfo->uiPcdTxRNum - guiBytesTransmit;
			if(uiTxNum > (PN512_FIFO_SIZE - ucFIFOLength))
			{
				uiTxNum = PN512_FIFO_SIZE - ucFIFOLength;
			}
			for(i = 0; i < uiTxNum; i++)
			{
				gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_FIFODATA_REG, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesTransmit + i], 1);
			}
			guiBytesTransmit += uiTxNum;

			/* Close LoAlert interrupt */
			if(guiBytesTransmit == gptPcdChipInfo->uiPcdTxRNum)
			{
				gptPcdChipInfo->ptBspOps->RfBspRead(PN512_COMMIEN_REG, &ucRegVal, 1);
				ucRegVal &= (~PN512_BIT_LOALERTIEN);
				gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIEN_REG, &ucRegVal, 1);
			}

			/* Clear LoAlert interrupt */
			ucRegVal = PN512_BIT_LOALERTIRQ;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);
		}

		/* Deal with HiAlert interrupt */
		if(ucIrqStatus & PN512_BIT_HIALERTIRQ)
		{
			/* Clear HiAlert interrupt */
			ucRegVal = PN512_BIT_HIALERTIRQ;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);

			/* Read FIFO Length */
			gptPcdChipInfo->ptBspOps->RfBspRead(PN512_FIFOLEVEL_REG, &ucRegVal, 1);
			uiRxNum = (unsigned int)ucRegVal;

			/* If overflow */
			if((guiBytesReceive + uiRxNum) > PCD_MAX_BUFFER_SIZE)
			{
				uiRxNum = PCD_MAX_BUFFER_SIZE - guiBytesReceive;
				gucErrorState |= PN512_ERROR_BUFFEROVFL;
			}
			/* Read FIFO */
			for(i = 0; i < uiRxNum; i++)
			{
				gptPcdChipInfo->ptBspOps->RfBspRead(PN512_FIFODATA_REG, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesReceive + i], 1);
			}
			guiBytesReceive += uiRxNum;
		}
		
		/* Deal with Tx interrupt */
		if(ucIrqStatus & PN512_BIT_TXIRQ)
		{
			/* Clear Tx interrupt */
			ucRegVal = PN512_BIT_TXIRQ;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);

			/* Open HiAlert interrupt*/
			gptPcdChipInfo->ptBspOps->RfBspRead(PN512_COMMIEN_REG, &ucRegVal, 1);
			ucRegVal |= PN512_BIT_HIALERTIEN;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIEN_REG, &ucRegVal, 1);
			
			/* change to receive state */ 
			gptPcdChipInfo->uiPcdTxRNum = 0;
			guiBytesReceive = 0;
		}
		
		/* Deal with TimeOut interrupt */
		if(ucIrqStatus & PN512_BIT_TIMERIRQ)
		{
			/* Clear TimeOut interrupt */
			ucRegVal = PN512_BIT_TIMERIRQ;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);
			gucErrorState |= PN512_ERROR_TIMEOUT;
			
			return;
		}

		/* Deal with Rx interrupt */
		if(ucIrqStatus & PN512_BIT_RXIRQ)
		{
			/* Clear Rx interrupt */
			ucRegVal = PN512_BIT_RXIRQ;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);

			/* Read FIFO Length */
			gptPcdChipInfo->ptBspOps->RfBspRead(PN512_FIFOLEVEL_REG, &ucRegVal, 1);
			uiRxNum = (unsigned int)ucRegVal;

			/* Read Error Register */
			gptPcdChipInfo->ptBspOps->RfBspRead(PN512_ERROR_REG, &ucErrorFlag, 1);
			ucErrorFlag &= 0x1F;
			gucErrorState |= ucErrorFlag;

		    /* Residual bits error */
			gptPcdChipInfo->ptBspOps->RfBspRead(PN512_CONTROL_REG , &ucRegVal, 1);
			if(ucRegVal & 0x07)ucEableReReceive = 1;

			/* Protocol error */
			if(ucErrorFlag & PN512_ERROR_PROTOCOLERR)ucEableReReceive = 1;
			
			/* Collision error */
			if(ucErrorFlag & PN512_ERROR_COLLERR)ucEableReReceive = 1;

			/* Parity error */
			if(ucErrorFlag & PN512_ERROR_PARITYERR)
			{
				if((guiBytesReceive + uiRxNum) < 4)ucEableReReceive = 1;
			}
			//((guiBytesReceive + uiRxNum) > 0) &&

			/* CRC error */
			if(ucErrorFlag & PN512_ERROR_CRCERR)
			{
				if( ((guiBytesReceive + uiRxNum) < 4))ucEableReReceive = 1;
			}

			/* Receive again */
			if((ucEableReReceive == 1) && guiPiccActivation)
			{
				guiBytesReceive = 0;
				ucEableReReceive = 0;
				gucErrorState = 0;

				/* Clear FIFO */
				ucRegVal = PN512_BIT_FLUSHBUFFER;
				gptPcdChipInfo->ptBspOps->RfBspWrite( PN512_FIFOLEVEL_REG, &ucRegVal, 1 );
				
				/* Restart receive */
				ucRegVal = PN512_RECEIVE_CMD; 
				gptPcdChipInfo->ptBspOps->RfBspWrite( PN512_COMMAND_REG, &ucRegVal, 1 );

				/* Restart Timer */
				gptPcdChipInfo->ptBspOps->RfBspRead(PN512_CONTROL_REG, &ucRegVal, 1 );
				ucRegVal |= 0x40;
				gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_CONTROL_REG, &ucRegVal, 1 );
			}
			else
			{
			    gucIrqState|= PN512_BIT_IDLEIRQ;
				/* If overflow */
				if((guiBytesReceive + uiRxNum) > PCD_MAX_BUFFER_SIZE)
				{
					uiRxNum = PCD_MAX_BUFFER_SIZE - guiBytesReceive;
					gucErrorState |= PN512_ERROR_BUFFEROVFL;
				}
				/* Read FIFO */
				for(i = 0; i < uiRxNum; i++)
				{
					gptPcdChipInfo->ptBspOps->RfBspRead(PN512_FIFODATA_REG, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesReceive + i], 1);
				}
				guiBytesReceive += uiRxNum;
				gptPcdChipInfo->uiPcdTxRNum = guiBytesReceive;
			}
		}

		/* Deal with Idle interrupt */
		if(ucIrqStatus & PN512_BIT_IDLEIRQ)
		{
			/* Clear Rx interrupt */
			ucRegVal = PN512_BIT_IDLEIRQ;
			gptPcdChipInfo->ptBspOps->RfBspWrite(PN512_COMMIRQ_REG, &ucRegVal, 1);
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
int Pn512GetParamTagValue(PICC_PARA *ptParam, unsigned char *pucRfPara)
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

    memcpy(&pucRfPara[1], aucRfCfg, (uiCount + 4));
	pucRfPara[0] = (char)(uiCount + 4);
	
    for( i = 0; i < uiCount; i++ )
    {
        if( ( 1 << i ) & uiFlag )
        {
            /*Type A conduct value*/
            if( 0 == i )
            {
                ptParam->a_conduct_val = pucPtr[i];  
				ucRegVal = ptParam->a_conduct_val;
                
                /*A*/         
                Pn512SetRegisterConfigVal(ISO14443_TYPEA_STANDARD,
                                      PN512_CWGSP_REG,
                                      ucRegVal);
                /*B*/
                Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      PN512_CWGSP_REG,
                                      ucRegVal );
                /*C*/
                Pn512SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PN512_CWGSP_REG,
                                      ucRegVal );
            }
            /*Type m conduct value*/
            if( 1 == i )ptParam->m_conduct_val = pucPtr[i];
            /*Type B modulate value*/
            if( 2 == i )
            {
                ptParam->b_modulate_val = pucPtr[i];        
                ucRegVal = ptParam->b_modulate_val;
				
                Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      PN512_MODGSP_REG,
                                      ucRegVal );
            }
            /*Type B Receive Threshold value*/
            if( 3 == i )
            {
                ptParam->card_RxThreshold_val = pucPtr[i]; 
                Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                      PN512_RXTHRESHOLD_REG, 
                                      ptParam->card_RxThreshold_val );
            }
            /*Type C (Felica) modulate value*/
            if( 4 == i )
            {
                ptParam->f_modulate_val = pucPtr[i];                   
                ucRegVal = ptParam->f_modulate_val;
                Pn512SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PN512_MODGSP_REG,
                                      ucRegVal );
            }
            /*Type A modulate value*/
            if( 5 == i )ptParam->a_modulate_val = pucPtr[i];
            /*Type A Receive Threshold value*/
            if( 6 == i )
            {
                ptParam->a_card_RxThreshold_val = pucPtr[i];    
                Pn512SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                      PN512_RXTHRESHOLD_REG, 
                                      ptParam->a_card_RxThreshold_val );
            }
            /*added by liubo for rc663*/
            /*Type C(Felica) Receive Threshold value*/
            if( 7 == i )
            {   
                ptParam->f_card_RxThreshold_val = pucPtr[i];
                Pn512SetRegisterConfigVal( JISX6319_4_STANDARD, 
                                      PN512_RXTHRESHOLD_REG, 
                                      ptParam->f_card_RxThreshold_val );
            }
            /*Type A antenna gain value*/
            if( 8 == i )
            {
                ptParam->a_card_antenna_gain_val = pucPtr[i]; 
                Pn512SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                      PN512_RFCFG_REG, 
                                      ptParam->a_card_antenna_gain_val );
            }
            /*Type B antenna gain value*/   
            if( 9 == i )
            {
                ptParam->b_card_antenna_gain_val = pucPtr[i];    
                Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                      PN512_RFCFG_REG, 
                                      ptParam->b_card_antenna_gain_val );
            }
            /*Felica antenna gain value*/
            if( 10 == i )
            {
                ptParam->f_card_antenna_gain_val = pucPtr[i];    
                Pn512SetRegisterConfigVal( JISX6319_4_STANDARD, 
                                      PN512_RFCFG_REG, 
                                      ptParam->f_card_antenna_gain_val );
            }

			/* add by wanls 2012.08.14 */
			if( 11 == i)
			{
                ptParam->f_conduct_val = pucPtr[i];       

                ucRegVal = ptParam->f_conduct_val;
                Pn512SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      PN512_CWGSP_REG,
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
int Pn512SetParamValue(PICC_PARA *ptPiccPara )
{
    unsigned char ucRegVal;

    /*RF carrier amplifier*/
    if( 1 == ptPiccPara->a_conduct_w )
	{
		/*A*/
        if(ptPiccPara->a_conduct_val > 0x3F)ptPiccPara->a_conduct_val = 0x3F;
		
        ucRegVal = ptPiccPara->a_conduct_val;
        Pn512SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                              PN512_CWGSP_REG,
                              ucRegVal );
        /*B*/
        Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                              PN512_CWGSP_REG,
                              ucRegVal );
		/*C*/
		ucRegVal = ptPiccPara->a_conduct_val;
        Pn512SetRegisterConfigVal( JISX6319_4_STANDARD,
                              PN512_CWGSP_REG,
                              ucRegVal );
	}
	
	/*TypeB modulation deepth*/
	if( 1 == ptPiccPara->b_modulate_w )
	{
	    if(ptPiccPara->b_modulate_val > 0x3F)ptPiccPara->b_modulate_val = 0x3F;
		
        ucRegVal = ptPiccPara->b_modulate_val;
        Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                              PN512_MODGSP_REG,
                              ucRegVal );
	}
	
	/*Type B Receive Threshold value*/
    if( 1 == ptPiccPara->card_RxThreshold_w )
    { 
        Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                              PN512_RXTHRESHOLD_REG, 
                              ptPiccPara->card_RxThreshold_val );
    }
    /*Type C (Felica) modulate value*/
    if( 1 == ptPiccPara->f_modulate_w )
    {      
        if(ptPiccPara->f_modulate_val > 0x3F)ptPiccPara->f_modulate_val = 0x3F;
		
        ucRegVal = ptPiccPara->f_modulate_val;
        Pn512SetRegisterConfigVal( JISX6319_4_STANDARD,
                              PN512_MODGSP_REG,
                              ucRegVal );
    }
   
    /*Type A Receive Threshold value*/
    if( 1 == ptPiccPara->a_card_RxThreshold_w )
    {    
        Pn512SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                             PN512_RXTHRESHOLD_REG, 
                             ptPiccPara->a_card_RxThreshold_val );
    }

    /*Type C(Felica) Receive Threshold value*/
    if( 1 == ptPiccPara->f_card_RxThreshold_w )
    {   
        Pn512SetRegisterConfigVal( JISX6319_4_STANDARD, 
                             PN512_RXTHRESHOLD_REG, 
                             ptPiccPara->f_card_RxThreshold_val );
    }
    /*Type A antenna gain value*/
    if( 1 == ptPiccPara->a_card_antenna_gain_w )
    {
        Pn512SetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                             PN512_RFCFG_REG, 
                             ptPiccPara->a_card_antenna_gain_val );
    }
    /*Type B antenna gain value*/   
    if( 1 == ptPiccPara->b_card_antenna_gain_w )
    {   
        Pn512SetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                             PN512_RFCFG_REG, 
                             ptPiccPara->b_card_antenna_gain_val );
    }
    /*Felica antenna gain value*/
    if( 1 == ptPiccPara->f_card_antenna_gain_w )
    {    
        Pn512SetRegisterConfigVal( JISX6319_4_STANDARD, 
                             PN512_RFCFG_REG, 
                             ptPiccPara->f_card_antenna_gain_val );
    }

    /* Felica carrier amplifier */
	if( 1 == ptPiccPara->f_conduct_w)
	{
        if(ptPiccPara->f_conduct_val > 0x3F)ptPiccPara->f_conduct_val = 0x3F;
		
        ucRegVal = ptPiccPara->f_conduct_val;
        Pn512SetRegisterConfigVal( JISX6319_4_STANDARD,
                              PN512_CWGSP_REG,
                              ucRegVal );
	}

    return 0;
}


struct ST_PCDOPS * GetPn512OpsHandle()
{
    return (struct ST_PCDOPS *)&gtPn512Ops;
}


