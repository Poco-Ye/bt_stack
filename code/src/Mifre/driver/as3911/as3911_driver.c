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
#include "../../protocol/mifare.h"
#include "../../protocol/pcd_apis.h"
#include "phhalHw_As3911_Reg.h"
#include "as3911_regs_conf.h"
#include "as3911_driver.h"
#include "mifare_encrypt.h"
#include <string.h>
#include <stdlib.h>



typedef struct _AS3911ModulationLevelTable_
{
    unsigned char length; /*!< Length of the \a x and \a y point arrays. */
    unsigned char *x;     /*!< X data points of the modulation level table. */
    unsigned char *y;     /*!< Y data points of the modulation level table. */
} AS3911ModulationLevelTable_t;



AS3911ModulationLevelTable_t gtModulationLevelTable;
unsigned char gaucTypeBTableX[5] = {0x9E, 0x9E, 0x9B, 0x9A, 0x9A};
unsigned char gaucTypeBTableY[5] = {0xB4, 0xAE, 0xB2, 0xB6, 0xB4};

unsigned char gaucTypeCTableX[5] = {0x9E, 0x9E, 0x9B, 0x9A, 0x9A};
unsigned char gaucTypeCTableY[5] = {0xB4, 0xAE, 0xB2, 0xB6, 0xB4};


AS3911ModulationLevelTable_t *gptAs3911ModulationLevelTable;





static volatile unsigned int   guiRxLastBits; 
static volatile unsigned int   guiAs3911ComStatus;
static volatile unsigned int   guiPiccActivation;
static volatile unsigned int   guiDeltaT;
static struct ST_PCDINFO*      gptPcdChipInfo;
static volatile unsigned char  gaucIrqState[3];
static volatile unsigned char  gucTxRxTag = 0;
static volatile unsigned int   guiBytesTransmit = 0;
static volatile unsigned int   guiBytesNeedToSend = 0;
static volatile unsigned int   guiBytesReceive = 0;
struct Crypto1State* gptPcs;
extern PICC_PARA gt_c_para;

/**
 * the driver operations list
 */
const struct ST_PCDOPS gtAs3911Ops = 
{
    As3911Init,
   
    As3911CarrierOn,
    As3911CarrierOff,
    
    As3911Config,
    
    As3911Waiter,
    
    As3911Trans,
    As3911TransCeive,
    As3911MifareAuthenticate,
    
    As3911CommIsr,
    
    As3911GetParamTagValue,
	As3911SetParamValue
};



void ReturnModulateIndex(unsigned char ucInModulate, unsigned char *ucOutModulate)
{
    int iTemp = 0;
	int i = 0;
	int aiIndex[7] = {50000, 25000, 12500, 6250, 3125, 1562, 781};
	unsigned char ucTempModulate = 0;

	iTemp = 200000 * ucInModulate / (100 - ucInModulate);
	for(i = 0; i < 6; i++)
	{
	    if(iTemp > 0)
	    {
	        if((iTemp - aiIndex[i]) >= 0)
			{
			    iTemp -= aiIndex[i];
				ucTempModulate |= 0x01;
	        }
	    }
		ucTempModulate = ucTempModulate << 1;
	}
	ucTempModulate = ucTempModulate >> 1;
	if(iTemp > aiIndex[6])ucTempModulate |= 0x01;
	
	*ucOutModulate = ucTempModulate;
}


int As3911Init( struct ST_PCDINFO * ptPcdInfo)
{
	unsigned char ucRegVal = 0;
	unsigned char aucFiFo[12];
	unsigned char aucIrq[3];
	int i = 0;
	guiDeltaT = 16;

	/* Put the AS3911 in defalut state(same as after power-up) */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_SET_DEFAULT, &ucRegVal, 0);

	/* MCU_CLK and LF MCU_CLK off, 27MHz XTAL */
	ucRegVal = 0x0F;
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IO_CONF1, &ucRegVal, 1);

    /*add by wanls 20150921 初始化RF芯片之前关闭As3911内部所有中断，避免刚开机初始化As3911芯片
	时，触发oscillator stable 中断，引发死机 */
	/*disable all irq*/    
	memset(aucIrq, 0xFF, sizeof(aucIrq));	
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IRQ_MASK_MAIN, aucIrq, 3);
	/*add end*/

	/* Enable oscillator */
	ucRegVal = 0x80;
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_OP_CONTROL, &ucRegVal, 1);

	/* Delay 5 ms, Wait for oscillator frequency is stable */
	DelayMs(5);

	/* Clear FiFo buffer */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_FIFO, &ucRegVal, 0);
	
	/* Write and Read FiFo, Test spi is ok */
	memcpy(aucFiFo, "\x33\xCC\x66\x99\x55\xAA", 6);
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_FIFO_WRITE, aucFiFo, 6);
	
	memset(aucFiFo, 0x00, sizeof(aucFiFo));
	ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_READ, aucFiFo, 6);

	if( 0 != memcmp( aucFiFo, "\x33\xCC\x66\x99\x55\xAA", 6 ) )
	{
	    return 0xFF;
		
	}

	/* Clear FiFo buffer */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_FIFO, &ucRegVal, 0);

    /*Avoid amplitude variation when the 1th open carrier. Add by wanls 2013.6.25*/
    ucRegVal = gt_c_para.a_conduct_val;
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_RFO_AM_OFF_LEVEL, &ucRegVal, 1);
	/* add end */

	gptPcdChipInfo = ptPcdInfo;

	return 0;
	
}


int As3911SetDeltaT( int delta_t )
{
    guiDeltaT = delta_t;
    
    return 0;   
}


unsigned char As3911GetInterpolatedValue(unsigned char x1, unsigned char y1, 
	unsigned char x2, unsigned char y2, unsigned char xi)
{
    if (xi <= x1) 
	{
        return y1;
    } 
	else if(xi >= x2) 
	{
        return y2;
    } 
	else 
    {
        return y1 + (((long) y2 - y1) * (xi - x1)) / (x2 - x1);
    }
}


unsigned char GetModulationValue(struct ST_PCDINFO* ptPcdInfo, unsigned char *ptTableX, unsigned char *ptTableY)
{
	unsigned char ucAmplitudePhase = 0;
	unsigned char ucAntennaDriverStrength = 0;
	unsigned char ucRegVal = 0x00;
	int index = 0;

	gtModulationLevelTable.length = 5;
	gtModulationLevelTable.x = ptTableX;
	gtModulationLevelTable.y = ptTableY;
	gptAs3911ModulationLevelTable = &gtModulationLevelTable;


	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_MEASURE_AMPLITUDE, &ucAmplitudePhase, 0);
	DelayMs(1);

	for (index = 0; index < gptAs3911ModulationLevelTable->length; index++)
	{
	    if (ucAmplitudePhase <= gptAs3911ModulationLevelTable->x[index])
	        break;
	}
	if (index == gptAs3911ModulationLevelTable->length)
	    index--;

	if (index == 0)
	{
	    ucAntennaDriverStrength = As3911GetInterpolatedValue(
	                                gptAs3911ModulationLevelTable->x[index],
	                                gptAs3911ModulationLevelTable->y[index],
	                                gptAs3911ModulationLevelTable->x[index+1],
	                                gptAs3911ModulationLevelTable->y[index+1],
	                                ucAmplitudePhase);
	}
	else
	{
	    ucAntennaDriverStrength = As3911GetInterpolatedValue(
	                                gptAs3911ModulationLevelTable->x[index-1],
	                                gptAs3911ModulationLevelTable->y[index-1],
	                                gptAs3911ModulationLevelTable->x[index],
	                                gptAs3911ModulationLevelTable->y[index],
	                                ucAmplitudePhase);
	}

	return ucAntennaDriverStrength;
}




int As3911Config( struct ST_PCDINFO* ptPcdInfo )
{  
	unsigned char ucRegVal = 0;
	unsigned int  uiTimeOut= 0;
	int i = 0;
	
	if(ptPcdInfo->ucEmdEn)
	{
		guiPiccActivation = 1;
	}
	else
	{
		guiPiccActivation = 0;
	}

	/* Config protocol */
	switch(ptPcdInfo->ucProtocol)
	{
	case ISO14443_TYPEA_STANDARD:
	case MIFARE_STANDARD:
		As3911Iso14443TypeAInit(ptPcdInfo);
		break;
	case ISO14443_TYPEB_STANDARD:
		As3911Iso14443TypeBInit(ptPcdInfo);
		break;
	case JISX6319_4_STANDARD:
		As3911Jisx6319_4Init(ptPcdInfo);
		break;
	default:
		break;
	}

	/* Config baudrate */
	switch(ptPcdInfo->ucProtocol)
	{
	case ISO14443_TYPEA_STANDARD:
		if( ISO14443_TYPEA_SHORT_FRAME == ptPcdInfo->ucFrameMode )
        {
			/* Set ISO14443A bit oriented anticollision frame is sent */
            ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
			ucRegVal |= 0x01; 
            ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);

			/**/
			ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
			ucRegVal &= 0x3F;
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);
        }
        else if( ISO14443_TYPEA_STANDARD_FRAME == ptPcdInfo->ucFrameMode )
        {
            /*change by wanls 20140303 
            将原来的发送crc使能判断修改成接收crc使能判断 */
			/* Without CRC check */
			if(ptPcdInfo->ucRxCrcEnable == 0)
			/*change end */
			{
				/* Set ISO14443A bit oriented anticollision frame is sent */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
				ucRegVal |= 0x01; 
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);

				/*Receive without CRC */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
				ucRegVal = (ucRegVal & 0x3F) | 0x80;
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);
			}
			/* With CRC check */
			else
			{
				/* Clear ISO14443A bit oriented anticollision frame is sent */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
				ucRegVal &= 0xFE; 
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);

				/*Receive with CRC and put CRC bytes in FIFO */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
				ucRegVal = (ucRegVal & (~0x80)) | 0x40;
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);

			}
        }
		
		As3911Iso14443TypeAConf(ptPcdInfo);
		break;
		
	case ISO14443_TYPEB_STANDARD:
	    ucRegVal = 0x80;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AM_MOD_DEPTH_CONF, &ucRegVal, 1);
		/* Delete by wanls 2013.05.27 Disable Sampling */
		// ucRegVal = GetModulationValue(ptPcdInfo, gaucTypeBTableX, gaucTypeBTableY);
		// ucRegVal = gt_c_para.b_modulate_val;
		/* Delete end */
		ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, AS3911_REG_RFO_AM_ON_LEVEL );
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_RFO_AM_ON_LEVEL, &ucRegVal, 1);

		/* Clear ISO14443A bit oriented anticollision frame is sent */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
		ucRegVal &= 0xFE; 
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
		
		/*Receive with CRC and put CRC bytes in FIFO */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
		ucRegVal = (ucRegVal & (~0x80)) | 0x40;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);
		As3911Iso14443TypeBConf(ptPcdInfo);
		break;
		
	case JISX6319_4_STANDARD:
		ucRegVal = 0x80;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AM_MOD_DEPTH_CONF, &ucRegVal, 1);
		/* Delete by wanls 2013.05.27 Disable Sampling */
		//ucRegVal = GetModulationValue(ptPcdInfo, gaucTypeCTableX, gaucTypeCTableY);
		//ucRegVal = gt_c_para.f_modulate_val;
		/* Delete end */
		ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, AS3911_REG_RFO_AM_ON_LEVEL );
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_RFO_AM_ON_LEVEL, &ucRegVal, 1);

		/* Clear ISO14443A bit oriented anticollision frame is sent */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
		ucRegVal &= 0xFE; 
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
		
		/* Receive with CRC and put CRC bytes in FIFO */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
		ucRegVal = (ucRegVal & (~0x80)) | 0x40;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);
		
		As3911Jisx6319_4Conf(ptPcdInfo);
		break;
		
	case MIFARE_STANDARD:
		/*add by wanls 20140303 支持ultralight卡*/
        if(ptPcdInfo->ucPiccType == PHILIPS_MIFARE_ULTRALIGHT)
        {
            ptPcdInfo->ucM1CryptoEn = 0;
			ptPcdInfo->uiPcdTxRLastBits = 0;
            /* Without CRC check */
			if(ptPcdInfo->ucRxCrcEnable == 0)
			{
				/* Set ISO14443A bit oriented anticollision frame is sent */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
				ucRegVal |= 0x01; 
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);

				/*Receive without CRC */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
				ucRegVal = (ucRegVal & 0x3F) | 0x80;
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);
			}
			/* With CRC check */
			else
			{
				/* Clear ISO14443A bit oriented anticollision frame is sent */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
				ucRegVal &= 0xFE; 
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);

				/*Receive with CRC and put CRC bytes in FIFO */
				ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
				ucRegVal = (ucRegVal & (~0x80)) | 0x40;
				ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);

			}
            
        }
		/*add end*/
		else
		{
			/* Set ISO14443A bit oriented anticollision frame is sent */
		    ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
			ucRegVal |= 0x01; 
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
			
		    /* No Parity bit is generated during Tx And Receive without parity and CRC */
			ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
			ucRegVal |= 0xC0;
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_ISO14443A_NFC, &ucRegVal, 1);
			
			/*Receive without CRC */
			ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_AUX, &ucRegVal, 1);
			ucRegVal = (ucRegVal & 0x3F) | 0x80;
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_AUX, &ucRegVal, 1);
			
			/* Disable TxCRC and RxCRC */
			ptPcdInfo->ucTxCrcEnable = 0;
		    ptPcdInfo->ucRxCrcEnable = 0;
		}
		break;

	default:
		break;
	}

	/* Set the Receive Timeout */
	uiTimeOut = ptPcdInfo->uiFwt + guiDeltaT;

	/*configurating FWT*/
	uiTimeOut *= 2;
	if(uiTimeOut > 0xFFFF)
	{
		/*Selects the no-response timer step 4096/fc */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_GPT_CONF, &ucRegVal, 1);
		ucRegVal |= 0x01;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT_CONF, &ucRegVal, 1);
		uiTimeOut = uiTimeOut / 64; 
	}
	else
	{
		/* Selects the no-response timer step 64/fc */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_GPT_CONF, &ucRegVal, 1);
		ucRegVal &= (~0x01);
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT_CONF, &ucRegVal, 1);
	}

	/* Load the no-response timer MSB bits */
	ucRegVal = ((uiTimeOut >> 8) & 0xFF);
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_NO_RESPONSE_TIMER1, &ucRegVal, 1);
	/* Load the no-response timer LSB bits */
	ucRegVal = (uiTimeOut & 0xFF);
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_NO_RESPONSE_TIMER2, &ucRegVal, 1);
	
	gptPcdChipInfo = ptPcdInfo;

	return 0;
}

/*The Time out Rang form 590ns~38.7ms, Rang form 0 ETU ~ 4096 ETU*/
int As3911Waiter( struct ST_PCDINFO* ptPcdInfo, unsigned int uiEtuCount )
{
	unsigned int uiTimeOut = 0;
	unsigned char ucRegVal = 0;
	unsigned int uiRunCount = 0;
	unsigned char aucIrqEnable[3];
	unsigned int uiBeginTime = 0;
	int i = 0;
	

	if(!uiEtuCount)
	{
		return 0;
	}
	
	uiRunCount = uiEtuCount / 4096;
	uiTimeOut = uiEtuCount % 4096;
	
	/*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();
	
	for(i = 0; i <= uiRunCount; i++)
	{
	    memset((unsigned char*)gaucIrqState, 0x00, sizeof(gaucIrqState));
		
		/* Set the time start only with direct command (Start General  Purpose Timer) */
		ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_GPT_CONF, &ucRegVal, 1);
		ucRegVal &= 0x1F;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT_CONF, &ucRegVal, 1);
        
		/* load General Purpose Timer */
		if(i == uiRunCount)
		{
		    if(uiTimeOut == 0)
		    {
				return 0;
		    }
		    uiTimeOut = uiTimeOut << 4;
		    ucRegVal = ((uiTimeOut >> 8) & 0xFF);
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT1, &ucRegVal, 1);
			ucRegVal = uiTimeOut & 0xFF;
		    ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT2, &ucRegVal, 1);
			
		}
		else
		{
		    ucRegVal = 0xFF;
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT1, &ucRegVal, 1);
			ucRegVal = 0xFF;
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_GPT2, &ucRegVal, 1);
		}
		ptPcdInfo->ptBspOps->RfBspIntDisable();
		/* Configure interrrupts */
	    aucIrqEnable[AS3911_MASK_MAIN_INT] = ~0x00;
	    /* Enable GPE Interrupt */
	    aucIrqEnable[AS3911_MASK_TIME_NFC_INT] = ~AS3911_IRQ_MASK_GPE;
	    /* Close all interrupts */
	    aucIrqEnable[AS3911_MASK_ERROR_WAKE_INT] = ~0x00;
	    ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IRQ_MASK_MAIN, aucIrqEnable, 3);
	
	    /* Clear interrupts */
	    ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MAIN, aucIrqEnable, 3);

		/*enable interrupt*/
		ptPcdInfo->ptBspOps->RfBspIntEnable();

		/* start Timer */
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_START_GP_TIMER, &ucRegVal, 0);

		
		uiBeginTime = GetTimerCount();
        while( !(gaucIrqState[AS3911_MASK_TIME_NFC_INT] & AS3911_IRQ_MASK_GPE) )
        {
            if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 100)
            {
                break;
            }
        }
		
        /*disable interruption*/
        ptPcdInfo->ptBspOps->RfBspIntDisable();
	}
	return 0;
	
}

int As3911CarrierOn( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;

    ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_OP_CONTROL, &ucRegVal, 1);
	/* Open carrier and enable Tx operation */
	ucRegVal |= 0x08;
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_OP_CONTROL, &ucRegVal, 1);
    DelayMs( 6 );
   /* delete by nt for paypass 3.0 performance test. 2013/03/11 pay attention,must be delete.!!!*/
  //ptPcdInfo->uiPcdState = PCD_CARRIER_ON;
    
    return 0;
}


int As3911CarrierOff( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char ucRegVal = 0;
    
	ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_OP_CONTROL, &ucRegVal, 1);
	/* Close carrier and disable Tx operation */
    ucRegVal &= (~0x08);
    ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_OP_CONTROL, &ucRegVal, 1);
    DelayMs( 6 );
    
    /* delete by nt for paypass 3.0 performance test. 2013/03/11 pay attention,must be delete.!!!*/
    //ptPcdInfo->uiPcdState = PCD_CARRIER_OFF;

    /*add by wanls 2013.5.14 enter powerdown mode */
    if(ptPcdInfo->uiPcdState == PCD_BE_CLOSED)
    {
        /* Put the AS3911 in defalut state(same as after power-up) */
	    ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_SET_DEFAULT, &ucRegVal, 0);
		ucRegVal = 0x0F;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IO_CONF1, &ucRegVal, 1);
		/* Change supply from 3v  to 5v.Avoid amplitude variation when the 1th open carrier. Add by wanls 2013.6.25*/
		ucRegVal = 0x00;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IO_CONF2, &ucRegVal, 1);
		/* add end */
		ucRegVal = 0x00;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_OP_CONTROL, &ucRegVal, 1);
		ucRegVal = 0x08;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_MODE, &ucRegVal, 1);
    }
	/*add end*/
    
    return 0;
}



int As3911MifareAuthenticate( struct ST_PCDINFO* ptPcdInfo )
{
	unsigned char aucSendBuffer[15];
	int iRet = 0;
	int iBitNum = 0;
	int i = 0;
	unsigned int uiCardChallenge = 0;
    unsigned int uiReaderChallenge = 0x92494155;
	unsigned int uiCardID = 0;
	unsigned int uiNt = 0;
	unsigned long long int ullKey = 0;
	unsigned char aucNr[4]; 
    unsigned char aucArEnc[10];
    unsigned char aucArEncPar[10];
	unsigned char ucNestAuth = ptPcdInfo->ucM1CryptoEn;

	ptPcdInfo->ucTxCrcEnable = 0;
	ptPcdInfo->ucRxCrcEnable = 0;
	ptPcdInfo->ucM1CryptoEn = 0;
    

	/* Copy 60/61 + BlockNum + UID + key */
	memcpy(aucSendBuffer, ptPcdInfo->aucPcdTxRBuffer, 12);

	/* Get UID */
	uiCardID = BytesToNum(&aucSendBuffer[2], 4); 
	
    /* Get Key */
	ullKey = BytesToNum(&aucSendBuffer[6], 6); 

	
	/* ReaderChallenge to Bytes */
	NumToBytes(uiReaderChallenge, 4, aucNr);
	
	/* Send 60/60 + BlockNum + CRC */
	ComputeCrc((char*)ptPcdInfo->aucPcdTxRBuffer, 2, &ptPcdInfo->aucPcdTxRBuffer[2], &ptPcdInfo->aucPcdTxRBuffer[3]);

	if(ucNestAuth)
    {
    	for (i = 0; i < 4; i++) 
		{
			/* Load in, and encrypt the reader nonce (Nr) */
			aucArEnc[i] = crypto1_byte(gptPcs, 0x00, 0) ^ ptPcdInfo->aucPcdTxRBuffer[i];
			aucArEncPar[i] = filter(gptPcs->odd) ^ ODD_PARITY(ptPcdInfo->aucPcdTxRBuffer[i]);
		}

		memcpy(ptPcdInfo->aucPcdTxRBuffer, aucArEnc, 4);

		/* Add Parity */
		AddParity(ptPcdInfo->aucPcdTxRBuffer, 4, aucArEncPar, 
			&ptPcdInfo->uiPcdTxRNum, &ptPcdInfo->uiPcdTxRLastBits);
	   
    }
	else
	{
		AddOddParity(ptPcdInfo->aucPcdTxRBuffer, 4, &ptPcdInfo->uiPcdTxRNum, &ptPcdInfo->uiPcdTxRLastBits);
	}
	
	/* The number of the bits of the Last Byte */
	iBitNum = ptPcdInfo->uiPcdTxRLastBits;
    
	iRet = ptPcdInfo->ptPcdOps->PcdChipTransCeive( ptPcdInfo );
	
	if(iRet)
	{
	    ptPcdInfo->uiPcdTxRLastBits = 0;
	    return iRet;
	}
	

	/* Remove Parity */
	RemoveParity(ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, iBitNum, &ptPcdInfo->uiPcdTxRNum);

	/* Get Card Challenge */
	uiCardChallenge = (unsigned int)BytesToNum(ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum); 
	uiNt = uiCardChallenge;

	/* Init the cipher with key {0..47} bits */
	gptPcs = crypto1_create(ullKey);
    
	/* Load (plain) uiCardChallenge^uiCardId into the cipher {48..79} bits */
	if(ucNestAuth)
	{
	    uiNt = crypto1_word(gptPcs, uiNt ^ uiCardID, 1) ^ uiNt;
	}
	else
	{
        crypto1_word(gptPcs, uiNt ^ uiCardID, 0);
	}

	/* Generate (encrypted) nr+parity by loading it into the cipher */
    for (i = 0; i < 4; i++) 
	{
        /* Load in, and encrypt the reader nonce (Nr) */
        aucArEnc[i] = crypto1_byte(gptPcs, aucNr[i], 0) ^ aucNr[i];
        aucArEncPar[i] = filter(gptPcs->odd) ^ ODD_PARITY(aucNr[i]);
    }

	/* Skip 32 bits in the pseudo random generator */
    uiNt = prng_successor(uiNt, 32);
    /* Generate reader-answer from tag-nonce */
    for (i = 4; i < 8; i++) 
	{
        /* Get the next random byte */
        uiNt = prng_successor(uiNt, 8);

        /* Encrypt the reader-answer (Nt' = suc2(Nt)) */
        aucArEnc[i] = crypto1_byte(gptPcs, 0x00, 0) ^ (uiNt & 0xff);
        aucArEncPar[i] = filter(gptPcs->odd) ^ ODD_PARITY(uiNt);
    }
	
	memcpy(ptPcdInfo->aucPcdTxRBuffer, aucArEnc, 8);
	AddParity(ptPcdInfo->aucPcdTxRBuffer, 8, aucArEncPar, &ptPcdInfo->uiPcdTxRNum, &ptPcdInfo->uiPcdTxRLastBits);

	/* The number of bit of the last byte */
	iBitNum = ptPcdInfo->uiPcdTxRLastBits;

	/* Add Delay Time */
   
	/* Transmit */
	iRet = ptPcdInfo->ptPcdOps->PcdChipTransCeive( ptPcdInfo );

	if(iRet)
	{
	    ptPcdInfo->uiPcdTxRLastBits = 0;
	    return iRet;
	}
    
	/* Remove Parity */
	RemoveParity(ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, iBitNum, &ptPcdInfo->uiPcdTxRNum);

	/* Decrypt the tag answer and verify that suc3(Nt) is At */
	uiNt = prng_successor(uiNt, 32);
	
    if (!((crypto1_word(gptPcs, 0x00, 0) ^ BytesToNum(ptPcdInfo->aucPcdTxRBuffer, 4)) == (uiNt & 0xFFFFFFFF))) 
	{
		return PHILIPS_MIFARE_ERR_AUTHEN;
    }
	
	return 0;
}

int As3911Trans( struct ST_PCDINFO* ptPcdInfo )
{
	unsigned char ucRegVal = 0;
	unsigned int uiTxNum = 0;
	unsigned int  uiBeginTime = 0;

	unsigned char aucIrqEnable[3];
	unsigned char aucArEnc[30];
    unsigned char aucArEncPar[30];
	unsigned int uiTempTxNum = 0;
	int i = 0;
   
	/*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();

	/* Use for checking Transmit or Receive state */
	gucTxRxTag = AS3911_TX_TAG;
	guiBytesTransmit = 0;
	guiBytesReceive = 0;
	guiBytesNeedToSend = ptPcdInfo->uiPcdTxRNum;
	memset((unsigned char*)gaucIrqState, 0x00, sizeof(gaucIrqState));

    /* Configure interrrupts,enable Water Level and TXE */
	aucIrqEnable[AS3911_MASK_MAIN_INT] = ~(AS3911_IRQ_MASK_WL | AS3911_IRQ_MASK_TXE);
	/* Close all interrupts */
	aucIrqEnable[AS3911_MASK_TIME_NFC_INT] = ~0x00;
	/* Close all interrupts */
	aucIrqEnable[AS3911_MASK_ERROR_WAKE_INT] = ~0x00;
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IRQ_MASK_MAIN, aucIrqEnable, 3);
	
	/* Clear interrupts */
	ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MAIN, aucIrqEnable, 3);

	/*Stops all activities and clears FIFO */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_FIFO, &ucRegVal, 0);

	/* Clear the current squelch setting and loads the manual gain reduction form
	Receiver Configuration Register4 */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_SQUELCH, &ucRegVal, 0);

	if((ptPcdInfo->ucM1CryptoEn) && (ptPcdInfo->ucProtocol == MIFARE_STANDARD))
	{
		/* Add CRC */
		ComputeCrc((char*)ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, 
		&ptPcdInfo->aucPcdTxRBuffer[ptPcdInfo->uiPcdTxRNum], &ptPcdInfo->aucPcdTxRBuffer[ptPcdInfo->uiPcdTxRNum + 1]);
		
		ptPcdInfo->uiPcdTxRNum += 2;

		for (i = 0; i < ptPcdInfo->uiPcdTxRNum; i++) 
		{
			/* Load in, and encrypt the reader nonce (Nr) */
			aucArEnc[i] = crypto1_byte(gptPcs, 0x00, 0) ^ ptPcdInfo->aucPcdTxRBuffer[i];
			aucArEncPar[i] = filter(gptPcs->odd) ^ ODD_PARITY(ptPcdInfo->aucPcdTxRBuffer[i]);
		}
		
		memcpy(ptPcdInfo->aucPcdTxRBuffer, aucArEnc, ptPcdInfo->uiPcdTxRNum);

		/* Add Parity */
		AddParity(ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, aucArEncPar, 
			&ptPcdInfo->uiPcdTxRNum, &ptPcdInfo->uiPcdTxRLastBits);
    }


	/* If Type A short frame, direct transmits command */
	if((ptPcdInfo->ucProtocol == ISO14443_TYPEA_STANDARD) 
		&& (ptPcdInfo->ucFrameMode == ISO14443_TYPEA_SHORT_FRAME))
	{
		/*enable interrupt*/
		ptPcdInfo->ptBspOps->RfBspIntEnable();
		if(ptPcdInfo->aucPcdTxRBuffer[0] == TYPE_A_COMMAND_WUPA)
		{
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_WUPA, &ucRegVal, 0);
		}
		if(ptPcdInfo->aucPcdTxRBuffer[0] == TYPE_A_COMMAND_REQA)
		{
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_REQA, &ucRegVal, 0);
		}
	}
	/* If Type A standard frame or Type B frame */
	else
	{
	   /* Mifare_encrypt */
	    if((ptPcdInfo->uiPcdTxRLastBits & 0x07) && (ptPcdInfo->ucProtocol == MIFARE_STANDARD))
	    {
			uiTempTxNum = ptPcdInfo->uiPcdTxRNum - 1;
	    }
		else
		{
			uiTempTxNum = ptPcdInfo->uiPcdTxRNum;
		}
		/* Set number of transmitted bytes */
		ucRegVal = (uiTempTxNum >> 5) & 0xFF;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_NUM_TX_BYTES1, &ucRegVal, 1);
		/* Set the number of bytes */
		ucRegVal = (uiTempTxNum << 3) & 0xFF;
		/* Set the number of bits */
		ucRegVal |= (ptPcdInfo->uiPcdTxRLastBits & 0x07);
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_NUM_TX_BYTES2, &ucRegVal, 1);

		uiTxNum = ptPcdInfo->uiPcdTxRNum;
		if(uiTxNum > AS3911_FIFO_SIZE)
		{
			uiTxNum = AS3911_FIFO_SIZE;
		}

		/* Load transmitted bytes into FIFO */
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_FIFO_WRITE, &ptPcdInfo->aucPcdTxRBuffer[0], uiTxNum);

		guiBytesTransmit += uiTxNum;
		
		/*enable interrupt*/
		ptPcdInfo->ptBspOps->RfBspIntEnable();

		/* Without CRC Transmit */
		if(ptPcdInfo->ucTxCrcEnable == 0)
		{
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_WITHOUT_CRC, &ucRegVal, 0);
		}
		/* With CRC Transmit */
		else
		{
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_WITH_CRC, &ucRegVal, 0);
		}
	}
	
	/* Wait for interrupts */
	uiBeginTime = GetTimerCount();
    while( !(gaucIrqState[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_TXE) )
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > 7000)
        {
            break;
        }
    }
    /*disable interruption*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();

	return 0;
}


int As3911TransCeive( struct ST_PCDINFO* ptPcdInfo )
{
	unsigned char ucRegVal = 0;
	unsigned int uiTxNum = 0;
	unsigned int  uiBeginTime = 0;

	unsigned char aucIrqEnable[3];
	unsigned int uiTempTxNum = 0;
	unsigned char aucArEnc[30];
    unsigned char aucArEncPar[30];
	unsigned int uiLastByteBitNum = 0;
	unsigned char ucTempData = 0;
	unsigned char aucCRCByte[2];
	unsigned char ucRet = 0;
	int i = 0;
	

	/*disable all interrupts*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();

	/* Use for checking Transmit or Receive state */
	gucTxRxTag = AS3911_TX_TAG;
	guiBytesTransmit = 0;
	guiBytesReceive = 0;
	guiBytesNeedToSend = ptPcdInfo->uiPcdTxRNum;
	memset((unsigned char*)gaucIrqState, 0x00, sizeof(gaucIrqState));

    /* Configure interrrupts */
	aucIrqEnable[AS3911_MASK_MAIN_INT] = (unsigned char)~(AS3911_IRQ_MASK_WL | AS3911_IRQ_MASK_RXE | AS3911_IRQ_MASK_TXE | AS3911_IRQ_MASK_COL);
	aucIrqEnable[AS3911_MASK_TIME_NFC_INT] = (unsigned char)~AS3911_IRQ_MASK_NRE;
	aucIrqEnable[AS3911_MASK_ERROR_WAKE_INT] = (unsigned char)~(AS3911_IRQ_MASK_CRC | AS3911_IRQ_MASK_PAR | AS3911_IRQ_MASK_ERR2 | AS3911_IRQ_MASK_ERR1);
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IRQ_MASK_MAIN, aucIrqEnable, 3);
	
	/* Clear interrupts */
	ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MAIN, aucIrqEnable, 3);

	/*Stops all activities and clears FIFO */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_FIFO, &ucRegVal, 0);

	/* Clear the current squelch setting and loads the manual gain reduction form
	Receiver Configuration Register4 */
	ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_SQUELCH, &ucRegVal, 0);

	if((ptPcdInfo->ucM1CryptoEn) && (ptPcdInfo->ucProtocol == MIFARE_STANDARD))
	{
		/* Add CRC */
		ComputeCrc((char*)ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, &ptPcdInfo->aucPcdTxRBuffer[ptPcdInfo->uiPcdTxRNum], &ptPcdInfo->aucPcdTxRBuffer[ptPcdInfo->uiPcdTxRNum + 1]);
		
		ptPcdInfo->uiPcdTxRNum += 2;

		for (i = 0; i < ptPcdInfo->uiPcdTxRNum; i++) 
		{
			/* Load in, and encrypt the reader nonce (Nr) */
			aucArEnc[i] = crypto1_byte(gptPcs, 0x00, 0) ^ ptPcdInfo->aucPcdTxRBuffer[i];
			aucArEncPar[i] = filter(gptPcs->odd) ^ ODD_PARITY(ptPcdInfo->aucPcdTxRBuffer[i]);
		}
		
		memcpy(ptPcdInfo->aucPcdTxRBuffer, aucArEnc, ptPcdInfo->uiPcdTxRNum);

		/* Add Parity */
		AddParity(ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, aucArEncPar, 
			&ptPcdInfo->uiPcdTxRNum, &ptPcdInfo->uiPcdTxRLastBits);

		/* Last Byte bits num */
		uiLastByteBitNum = ptPcdInfo->uiPcdTxRLastBits;	
    }


	/* If Type A short frame, direct transmits command */
	if((ptPcdInfo->ucProtocol == ISO14443_TYPEA_STANDARD) 
		&& (ptPcdInfo->ucFrameMode == ISO14443_TYPEA_SHORT_FRAME))
	{
	    ptPcdInfo->uiPcdTxRLastBits = 0;
	    /* Close Water Level Interrupt */
	    ptPcdInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MAIN, &ucRegVal, 1);
		ucRegVal |= AS3911_IRQ_MASK_WL;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_IRQ_MAIN, &ucRegVal, 1);

        guiBytesTransmit = ptPcdInfo->uiPcdTxRNum;
		
		/*enable interrupt*/
		ptPcdInfo->ptBspOps->RfBspIntEnable();
		if(ptPcdInfo->aucPcdTxRBuffer[0] == TYPE_A_COMMAND_WUPA)
		{
		    /* add by wanls 2013.05.27 Deal with the amplitude is raised after open carrier wave*/
		    DelayMs(2);
			/* add end */
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_WUPA, &ucRegVal, 0);
		}
		if(ptPcdInfo->aucPcdTxRBuffer[0] == TYPE_A_COMMAND_REQA)
		{
		    /* add by wanls 2013.05.27 Deal with the amplitude is raised after open carrier wave*/
		    DelayMs(2);
			/* add end */
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_REQA, &ucRegVal, 0);
		}
	}
	/* If Type A standard frame or Type B frame */
	else
	{
	    /* Mifare_encrypt */
	    if((ptPcdInfo->uiPcdTxRLastBits & 0x07) && (ptPcdInfo->ucProtocol == MIFARE_STANDARD))
	    {
			uiTempTxNum = ptPcdInfo->uiPcdTxRNum - 1;
	    }
		else if(ptPcdInfo->ucProtocol == JISX6319_4_STANDARD)
		{
		    /* In felica frame mode, the length byte Automatic transmission*/
			uiTempTxNum = ptPcdInfo->uiPcdTxRNum - 1;
			ptPcdInfo->uiPcdTxRLastBits = 0;
		}
		else
		{
			uiTempTxNum = ptPcdInfo->uiPcdTxRNum;
		}
		/* Set number of transmitted bytes */
		ucRegVal = (uiTempTxNum >> 5) & 0xFF;
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_NUM_TX_BYTES1, &ucRegVal, 1);
		/* Set the number of bytes */
		ucRegVal = (uiTempTxNum << 3) & 0xFF;
		/* Set the number of bits */
		ucRegVal |= (ptPcdInfo->uiPcdTxRLastBits & 0x07);
		ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_NUM_TX_BYTES2, &ucRegVal, 1);

		uiTxNum = ptPcdInfo->uiPcdTxRNum;
		if(uiTxNum > AS3911_FIFO_SIZE)
		{
			uiTxNum = AS3911_FIFO_SIZE;
		}
		
	    /* Load transmitted bytes into FIFO */
		if(ptPcdInfo->ucProtocol == JISX6319_4_STANDARD)
		{
		    /* In felica frame mode, the length byte Automatic transmission,so the length byte
			not to fill in fifo */
		    ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_FIFO_WRITE, &ptPcdInfo->aucPcdTxRBuffer[1], (uiTxNum - 1));
			
		}
		else
		{
		    ptPcdInfo->ptBspOps->RfBspWrite(AS3911_REG_FIFO_WRITE, &ptPcdInfo->aucPcdTxRBuffer[0], uiTxNum);
		}
		guiBytesTransmit += uiTxNum;
		
		/*enable interrupt*/
		ptPcdInfo->ptBspOps->RfBspIntEnable();

		/* Without CRC Transmit */
		if(ptPcdInfo->ucTxCrcEnable == 0)
		{
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_WITHOUT_CRC, &ucRegVal, 0);
		}
		/* With CRC Transmit */
		else
		{
			ptPcdInfo->ptBspOps->RfBspWrite(AS3911_CMD_TRANSMIT_WITH_CRC, &ucRegVal, 0);
		}
	}
	
	/* Wait for interrupts */
	uiBeginTime = GetTimerCount();
    while( !( 
              ( (gaucIrqState[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_RXE) == AS3911_IRQ_MASK_RXE ) ||
              ( (gaucIrqState[AS3911_MASK_TIME_NFC_INT] & AS3911_IRQ_MASK_NRE) == AS3911_IRQ_MASK_NRE) 
            )
         )
    {
        if((unsigned int )((unsigned int )GetTimerCount() - uiBeginTime) > (ptPcdInfo->uiFwt / 100 + 100))
        {
            gaucIrqState[AS3911_MASK_TIME_NFC_INT] |= AS3911_IRQ_MASK_NRE;
            break;
        }
	
    }

    /*disable interruption*/
    ptPcdInfo->ptBspOps->RfBspIntDisable();

	/*timeout*/
    if(gaucIrqState[AS3911_MASK_TIME_NFC_INT] & AS3911_IRQ_MASK_NRE)return ISO14443_HW_ERR_COMM_TIMEOUT;
    /*fifo write error or fifo overflow*/
    if(gaucIrqState[AS3911_MASK_MAIN_INT] & AS3911_MASK_FIFO_OVERFLOW) return ISO14443_HW_ERR_COMM_FIFO;
    /* parity error*/
    if(gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & AS3911_IRQ_MASK_PAR)return ISO14443_HW_ERR_COMM_PARITY;
    /* crc error */
	if(gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & AS3911_IRQ_MASK_CRC)return ISO14443_HW_ERR_COMM_CRC;
    /*collision error*/
    if(gaucIrqState[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_COL)return ISO14443_HW_ERR_COMM_COLL;

    if(ptPcdInfo->ucProtocol != MIFARE_STANDARD)
    {
	    /*min frame error*/
        if(gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & (AS3911_IRQ_MASK_ERR1 | AS3911_IRQ_MASK_ERR2))
		return ISO14443_HW_ERR_COMM_FRAME; 
    }

    /*If Receive no error and Receive with CRC, Reveive Num subtract 2*/
	if((ptPcdInfo->ucRxCrcEnable) && (ptPcdInfo->uiPcdTxRNum >= 2))
	{
	    ptPcdInfo->uiPcdTxRNum -= 2;
	}

	
	if((ptPcdInfo->ucM1CryptoEn) && (ptPcdInfo->ucProtocol == MIFARE_STANDARD))
	{
		RemoveParity(ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum, 
			uiLastByteBitNum, &ptPcdInfo->uiPcdTxRNum);
		
		if(ptPcdInfo->uiPcdTxRNum == 1)
		{
			ucTempData = 0;
			for (i = 0; i < 4; i++)
			{
				ucTempData  |= (crypto1_bit(gptPcs, 0, 0) ^ BIT(ptPcdInfo->aucPcdTxRBuffer[0], i)) << i;
			}
			ptPcdInfo->aucPcdTxRBuffer[0] = ucTempData;
		} 
		
		else if(ptPcdInfo->uiPcdTxRNum > 1)
		{
			for (i = 0; i < ptPcdInfo->uiPcdTxRNum; i++)
			{
				ptPcdInfo->aucPcdTxRBuffer[i] = crypto1_byte(gptPcs, 0x00, 0) ^ ptPcdInfo->aucPcdTxRBuffer[i];
			}
            /*add by wanls 2014.06.24*/
			/*解决在做Mifare卡读写时，在临界点晃动,数据没接收完整，
			当刚好接收2个字节时,调用ComputeCrc进行CRC计算时出现死机问题*/
			ucRet = ComputeCrc((char*)ptPcdInfo->aucPcdTxRBuffer, ptPcdInfo->uiPcdTxRNum-2, &aucCRCByte[0], &aucCRCByte[1]);
            if(ucRet != 0)return ISO14443_HW_ERR_COMM_PARITY;
			/*add end*/
			
			if((aucCRCByte[0] != ptPcdInfo->aucPcdTxRBuffer[ptPcdInfo->uiPcdTxRNum - 2])
				|| (aucCRCByte[1] != ptPcdInfo->aucPcdTxRBuffer[ptPcdInfo->uiPcdTxRNum - 1]))
			{
				return ISO14443_HW_ERR_COMM_PARITY;
			}
			
			ptPcdInfo->uiPcdTxRNum -= 2;
		}
	}
	    
	return 0;
}


void As3911CommIsr()
{
	unsigned char ucRegVal = 0;


	unsigned char aucIrqTemp[3];
	unsigned char aucIrqEn[3];
	
	unsigned char aucFifoState[2];
    unsigned char ucEableReReceive = 0;
	
	unsigned int uiTxNum = 0;
	unsigned int uiRxNum = 0;
	unsigned int uiTempTxNum = 0;
	
	while(1)
    {   
		
		/* Read RF interrupts */
		gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MAIN, &aucIrqTemp[0], 1);
		gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_TIMER_NFC, &aucIrqTemp[1], 1);
		gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_ERROR_WUP, &aucIrqTemp[2], 1);
		
		gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MASK_MAIN, &aucIrqEn[0], 1);
		gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MASK_TIMER_NFC, &aucIrqEn[1], 1);
		gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_IRQ_MASK_ERROR_WUP, &aucIrqEn[2], 1);

		
		
		aucIrqTemp[AS3911_MASK_MAIN_INT] &= ~aucIrqEn[AS3911_MASK_MAIN_INT];
		aucIrqTemp[AS3911_MASK_TIME_NFC_INT] &= ~aucIrqEn[AS3911_MASK_TIME_NFC_INT];
		aucIrqTemp[AS3911_MASK_ERROR_WAKE_INT] &= ~aucIrqEn[AS3911_MASK_ERROR_WAKE_INT];
		
		gaucIrqState[AS3911_MASK_MAIN_INT] |= aucIrqTemp[AS3911_MASK_MAIN_INT];
		gaucIrqState[AS3911_MASK_TIME_NFC_INT] |= aucIrqTemp[AS3911_MASK_TIME_NFC_INT];
		gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] |= aucIrqTemp[AS3911_MASK_ERROR_WAKE_INT];


		/* IF no interrupts, return */
		if((aucIrqTemp[AS3911_MASK_MAIN_INT] | aucIrqTemp[AS3911_MASK_TIME_NFC_INT] | aucIrqTemp[AS3911_MASK_ERROR_WAKE_INT]) == 0)
		{
			return;
		}

		/* End of transmission */
		if(aucIrqTemp[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_TXE)
		{
			/* change to receive state */ 
			gucTxRxTag = AS3911_RX_TAG;
			gptPcdChipInfo->uiPcdTxRNum = 0;
			guiBytesReceive = 0;
		}
		
		/* FiFo water level */
		if(aucIrqTemp[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_WL)
		{
			/* Write FiFo*/
			if((gucTxRxTag == AS3911_TX_TAG) && (guiBytesNeedToSend > guiBytesTransmit))
			{
				gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_RX_STATUS1, aucFifoState, 2);
				uiTempTxNum = aucFifoState[0] & 0x7F;
				
				uiTxNum = guiBytesNeedToSend - guiBytesTransmit;
				if(uiTxNum > (AS3911_FIFO_SIZE - uiTempTxNum))
				{
					uiTxNum = AS3911_FIFO_SIZE - uiTempTxNum;
				}

				gptPcdChipInfo->ptBspOps->RfBspWrite(AS3911_REG_FIFO_WRITE, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesTransmit], uiTxNum);
				guiBytesTransmit += uiTxNum;
				
			}
			/* Read FiFo */
			if(gucTxRxTag == AS3911_RX_TAG)
			{
				gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_RX_STATUS1, aucFifoState, 2);
				uiRxNum = aucFifoState[0] & 0x7F;

				/* If overflow */
				if((guiBytesReceive + uiRxNum) > PCD_MAX_BUFFER_SIZE)
				{
					uiRxNum = PCD_MAX_BUFFER_SIZE - guiBytesReceive;
					gaucIrqState[AS3911_MASK_MAIN_INT] |= AS3911_MASK_FIFO_OVERFLOW;
				}

				gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_READ, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesReceive], uiRxNum);
				guiBytesReceive += uiRxNum;
				
			}
		}
		
		/* No response timer expire */
		if(aucIrqTemp[AS3911_MASK_TIME_NFC_INT] & AS3911_IRQ_MASK_NRE)
		{
			return;
		}

		/* End of receive */
		if(aucIrqTemp[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_RXE)
		{	
			gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_RX_STATUS1, &aucFifoState[0], 1);
			uiRxNum = (unsigned int)(aucFifoState[0] & 0x7F);
			gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_RX_STATUS2, &aucFifoState[1], 1);
			if(aucFifoState[1] & 0x20)
			{
				gaucIrqState[AS3911_MASK_MAIN_INT] |= AS3911_MASK_FIFO_OVERFLOW;
			}

			/* Residual bits are handled as hard framing error */
			if(aucFifoState[1] & 0x11)
			{
				gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] |= AS3911_IRQ_MASK_ERR2;
			}

			/* Collision error and frame error */
			if((gaucIrqState[AS3911_MASK_MAIN_INT] & AS3911_IRQ_MASK_COL)
				| (gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & AS3911_IRQ_MASK_ERR1)
				| (gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & AS3911_IRQ_MASK_ERR2))
			{
				ucEableReReceive = 1;
			}

			/* Parity error */
			if(gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & AS3911_IRQ_MASK_PAR)
			{
				if((guiBytesReceive + uiRxNum) < 4)ucEableReReceive = 1;
			}
			
			/* CRC error */
			if(gaucIrqState[AS3911_MASK_ERROR_WAKE_INT] & AS3911_IRQ_MASK_CRC)
			{
				if(((guiBytesReceive + uiRxNum) > 2) && ((guiBytesReceive + uiRxNum) < 4))ucEableReReceive = 1;
			}

			/* Receive again */
			if((ucEableReReceive == 1) && guiPiccActivation)
			{
				guiBytesReceive = 0;
				ucEableReReceive = 0;
				memset((unsigned char *)gaucIrqState, 0x00, sizeof(gaucIrqState));
				/* Stops all activities and clears FIFO */
				gptPcdChipInfo->ptBspOps->RfBspWrite(AS3911_CMD_CLEAR_FIFO, &ucRegVal, 0);
				/* Restart receive */
				gptPcdChipInfo->ptBspOps->RfBspWrite(AS3911_CMD_UNMASK_RECEIVE_DATA, &ucRegVal, 0);
				/* Start no response timer */
				gptPcdChipInfo->ptBspOps->RfBspWrite(AS3911_CMD_START_NO_RESPONSE_TIMER, &ucRegVal, 0);
				
			}
			else
			{
				/* If FiFo overflow */
				if((guiBytesReceive + uiRxNum) > PCD_MAX_BUFFER_SIZE)
				{
					uiRxNum = PCD_MAX_BUFFER_SIZE - guiBytesReceive;
					gaucIrqState[AS3911_MASK_MAIN_INT] |= AS3911_MASK_FIFO_OVERFLOW;
				}
				gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_READ, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesReceive], uiRxNum);
				guiBytesReceive += uiRxNum;
				gptPcdChipInfo->uiPcdTxRNum = guiBytesReceive;

				gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_RX_STATUS2, &ucRegVal, 1);
                /* If Last FIFO Byte is not complete */
				if((ucRegVal & 0x0E))
				{
				    gptPcdChipInfo->ptBspOps->RfBspRead(AS3911_REG_FIFO_READ, &gptPcdChipInfo->aucPcdTxRBuffer[guiBytesReceive], 1);
					guiBytesReceive += 1;
				    gptPcdChipInfo->uiPcdTxRNum = guiBytesReceive;
				}
			}
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

int As3911GetParamTagValue(PICC_PARA *ptParam, unsigned char *pucRfPara)
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
                
                As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                                      AS3911_REG_RFO_AM_OFF_LEVEL,
                                      ucRegVal );
                /*B*/          
                As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      AS3911_REG_RFO_AM_OFF_LEVEL,
                                      ucRegVal );
                /*C*/            
                As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      AS3911_REG_RFO_AM_OFF_LEVEL,
                                      ucRegVal );
            }
            /*Type m conduct value*/
            if( 1 == i )ptParam->m_conduct_val = pucPtr[i];
            /*Type B modulate value*/
            if( 2 == i )
            {
                ptParam->b_modulate_val = pucPtr[i];
				As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      AS3911_REG_RFO_AM_ON_LEVEL,
                                      ptParam->b_modulate_val );
            }
            /*Type B Receive Threshold value*/
            if( 3 == i )
            {
                ptParam->card_RxThreshold_val = pucPtr[i]; 
				/*add by wanls 2014.05.12*/
				As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      AS3911_REG_RX_CONF1,
                                      ptParam->card_RxThreshold_val );
				/*add end*/
            }
            /*Type C (Felica) modulate value*/
            if( 4 == i )
            {
                ptParam->f_modulate_val = pucPtr[i];
				As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      AS3911_REG_RFO_AM_ON_LEVEL,
                                      ptParam->f_modulate_val);
            }
            /*Type A modulate value*/
            if( 5 == i )ptParam->a_modulate_val = pucPtr[i];
            /*Type A Receive Threshold value*/
            if( 6 == i )
            {
                ptParam->a_card_RxThreshold_val = pucPtr[i];  
				/*add by wanls 2014.05.12*/
				As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                                      AS3911_REG_RX_CONF1,
                                      ptParam->a_card_RxThreshold_val );
				/*add end*/
				
			}

            /*Type C(Felica) Receive Threshold value*/
            if( 7 == i )
            {   
                ptParam->f_card_RxThreshold_val = pucPtr[i];
				/*add by wanls 2014.05.12*/
				As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      AS3911_REG_RX_CONF1,
                                      ptParam->f_card_RxThreshold_val);
				/*add end*/
            }
            /*Type A antenna gain value*/
            if( 8 == i )
            {   
                /*Enable Am Channel*/
                if((ptParam->a_card_RxThreshold_val & EANBLE_CHANNEL_MASR) == ENABLE_AM_CHANNEL)
                {
	                ptParam->a_card_antenna_gain_val = pucPtr[i]; 
					
					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                                 AS3911_REG_RX_CONF3 );
					ucRegVal &= 0x1F;
					ucRegVal |= (ptParam->a_card_antenna_gain_val & 0xE0);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                                      AS3911_REG_RX_CONF3,
	                                      ucRegVal );

					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                                 AS3911_REG_RX_CONF4 );
					ucRegVal &= 0x0F;
					ucRegVal |= ((ptParam->a_card_antenna_gain_val & 0x0F) << 4);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                                      AS3911_REG_RX_CONF4,
	                                      ucRegVal );
                }
				/*add by wanls 2014.05.12*/
				/*Enable Pm Channel*/
				else
				{
					ptParam->a_card_antenna_gain_val = pucPtr[i]; 
					
					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                                 AS3911_REG_RX_CONF3 );
					ucRegVal &= 0xE3;
					ucRegVal |= ((ptParam->a_card_antenna_gain_val & 0xE0) >> 3);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                                      AS3911_REG_RX_CONF3,
	                                      ucRegVal );

					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                                 AS3911_REG_RX_CONF4 );
					ucRegVal &= 0xF0;
					ucRegVal |= (ptParam->a_card_antenna_gain_val & 0x0F);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                                      AS3911_REG_RX_CONF4,
	                                      ucRegVal );
				}
				/*add end*/
            }
            /*Type B antenna gain value*/   
            if( 9 == i )
            {
                /*Enable Am Channel*/
                if((ptParam->card_RxThreshold_val & EANBLE_CHANNEL_MASR) == ENABLE_AM_CHANNEL)
                {
	                ptParam->b_card_antenna_gain_val = pucPtr[i];
					
					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                                 AS3911_REG_RX_CONF3 );
					ucRegVal &= 0x1F;
					ucRegVal |= (ptParam->b_card_antenna_gain_val & 0xE0);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                                      AS3911_REG_RX_CONF3,
	                                      ucRegVal );

					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                                 AS3911_REG_RX_CONF4 );
					ucRegVal &= 0x0F;
					ucRegVal |= ((ptParam->b_card_antenna_gain_val & 0x0F) << 4);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                                      AS3911_REG_RX_CONF4,
	                                      ucRegVal );
                }
				/*add by wanls 2014.05.12*/
				/*Enable Pm Channel*/
				else
				{
				    ptParam->b_card_antenna_gain_val = pucPtr[i];
					
					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                                 AS3911_REG_RX_CONF3 );
					ucRegVal &= 0xE3;
					ucRegVal |= ((ptParam->b_card_antenna_gain_val & 0xE0) >> 3);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                                      AS3911_REG_RX_CONF3,
	                                      ucRegVal );

					ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                                 AS3911_REG_RX_CONF4 );
					ucRegVal &= 0xF0;
					ucRegVal |= (ptParam->b_card_antenna_gain_val & 0x0F);
	  
	                As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                                      AS3911_REG_RX_CONF4,
	                                      ucRegVal );
				}
				/*add end*/

            }
            /*Felica antenna gain value*/
            if( 10 == i )
            {
                if((ptParam->f_card_RxThreshold_val & EANBLE_CHANNEL_MASR) == ENABLE_AM_CHANNEL)
                {
	                ptParam->f_card_antenna_gain_val = pucPtr[i]; 
					             
					ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                                 AS3911_REG_RX_CONF3 );
					ucRegVal &= 0x1F;
					ucRegVal |= (ptParam->f_card_antenna_gain_val & 0xE0);
	  
	                As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                                      AS3911_REG_RX_CONF3,
	                                      ucRegVal );

					ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                                 AS3911_REG_RX_CONF4 );
					ucRegVal &= 0x0F;
					ucRegVal |= ((ptParam->f_card_antenna_gain_val & 0x0F) << 4);
	  
	                As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                                      AS3911_REG_RX_CONF4,
	                                      ucRegVal );
                }
				/*add by wanls 2014.05.12*/
				/*Enable Pm Channel*/
				else
				{
				    ptParam->f_card_antenna_gain_val = pucPtr[i]; 
					             
					ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                                 AS3911_REG_RX_CONF3 );
					ucRegVal &= 0xE3;
					ucRegVal |= ((ptParam->f_card_antenna_gain_val & 0xE0) >> 3);
	  
	                As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                                      AS3911_REG_RX_CONF3,
	                                      ucRegVal );

					ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                                 AS3911_REG_RX_CONF4 );
					ucRegVal &= 0xF0;
					ucRegVal |= (ptParam->f_card_antenna_gain_val & 0x0F);
	  
	                As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                                      AS3911_REG_RX_CONF4,
	                                      ucRegVal );
				}
				/*add end*/
			}

			/* add by wanls 2012.08.14 */
			if( 11 == i)
			{
                ptParam->f_conduct_val = pucPtr[i];
				
			    ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                                 AS3911_REG_RFO_AM_OFF_LEVEL );
                ucRegVal = ptParam->f_conduct_val;
                As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      AS3911_REG_RFO_AM_OFF_LEVEL,
                                      ucRegVal );
			}
			/* add end */
        }
        
    }
    
    return 0;

}

int As3911SetParamValue(PICC_PARA *ptPiccPara )
{
    unsigned char ucRegVal;
    
    /*RF carrier amplifier*/
    if( 1 == ptPiccPara->a_conduct_w )
	{           
        ucRegVal = ptPiccPara->a_conduct_val;
		/*A*/
		As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
			AS3911_REG_RFO_AM_OFF_LEVEL, ucRegVal );
		/*B*/          
		As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
			AS3911_REG_RFO_AM_OFF_LEVEL, ucRegVal );
  
	}
	
	/*TypeB modulation deepth*/
	if( 1 == ptPiccPara->b_modulate_w )
	{
        ucRegVal = ptPiccPara->b_modulate_val;
		As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      AS3911_REG_RFO_AM_ON_LEVEL,
                                      ucRegVal);
	}
	
	/*Type B Receive Threshold value*/
    if( 1 == ptPiccPara->card_RxThreshold_w )
    {
        /*add by wanls 2014.05.12*/
        ucRegVal = ptPiccPara->card_RxThreshold_val;
		As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
                                      AS3911_REG_RX_CONF1,
                                      ucRegVal);
		/*add end*/
    }
    /*Type C (Felica) modulate value*/
    if( 1 == ptPiccPara->f_modulate_w )
    {      
        ucRegVal = ptPiccPara->f_modulate_val;
		As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      AS3911_REG_RFO_AM_ON_LEVEL,
                                      ucRegVal);
    }
   
    /*Type A Receive Threshold value*/
    if( 1 == ptPiccPara->a_card_RxThreshold_w )
    {
        /*add by wanls 2014.05.12*/
        ucRegVal = ptPiccPara->a_card_RxThreshold_val;
		As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
                                      AS3911_REG_RX_CONF1,
                                      ucRegVal);
		/*add end*/
    }

    /*Type C(Felica) Receive Threshold value*/
    if( 1 == ptPiccPara->f_card_RxThreshold_w )
    {
        /*add by wanls 2014.05.12*/
        ucRegVal = ptPiccPara->f_card_RxThreshold_val;
		As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
                                      AS3911_REG_RX_CONF1,
                                      ucRegVal);
		/*add end*/
    }
    /*Type A antenna gain value*/
    if( 1 == ptPiccPara->a_card_antenna_gain_w )
    {
        ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
                                         AS3911_REG_RX_CONF1 );
		/*Enable Am Channel*/
		if((ucRegVal & EANBLE_CHANNEL_MASR) == ENABLE_AM_CHANNEL)
		{
			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                         AS3911_REG_RX_CONF3 );
			ucRegVal &= 0x1F;
			ucRegVal |= (ptPiccPara->a_card_antenna_gain_val & 0xE0);

	        As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                              AS3911_REG_RX_CONF3,
	                              ucRegVal );

			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                         AS3911_REG_RX_CONF4 );
			ucRegVal &= 0x0F;
			ucRegVal |= ((ptPiccPara->a_card_antenna_gain_val & 0x0F) << 4);

	        As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                              AS3911_REG_RX_CONF4,
	                              ucRegVal );
		}
		/*add by wanls 2014.05.12*/
		/*Enable Pm Channel*/
		else
		{
			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                         AS3911_REG_RX_CONF3 );
			ucRegVal &= 0xE3;
			ucRegVal |= ((ptPiccPara->a_card_antenna_gain_val & 0xE0) >> 3);

	        As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                              AS3911_REG_RX_CONF3,
	                              ucRegVal );

			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEA_STANDARD, 
	                                         AS3911_REG_RX_CONF4 );
			ucRegVal &= 0xF0;
			ucRegVal |= (ptPiccPara->a_card_antenna_gain_val & 0x0F);

	        As3911SetRegisterConfigVal( ISO14443_TYPEA_STANDARD,
	                              AS3911_REG_RX_CONF4,
	                              ucRegVal );
		}
		/*add end*/

		
    }
    /*Type B antenna gain value*/   
    if( 1 == ptPiccPara->b_card_antenna_gain_w )
    {
        ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
                                         AS3911_REG_RX_CONF1 );
		/*Enable Am Channel*/
		if((ucRegVal & EANBLE_CHANNEL_MASR) == ENABLE_AM_CHANNEL)
		{
		
			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                         AS3911_REG_RX_CONF3 );
			ucRegVal &= 0x1F;
			ucRegVal |= (ptPiccPara->b_card_antenna_gain_val & 0xE0);

	        As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                              AS3911_REG_RX_CONF3,
	                              ucRegVal );

			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                         AS3911_REG_RX_CONF4 );
			ucRegVal &= 0x0F;
			ucRegVal |= ((ptPiccPara->b_card_antenna_gain_val & 0x0F) << 4);

	        As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                              AS3911_REG_RX_CONF4,
	                              ucRegVal );
		}
		/*add by wanls 2014.05.12*/
		/*Enable Pm Channel*/
		else
		{
		    ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                         AS3911_REG_RX_CONF3 );
			ucRegVal &= 0xE3;
			ucRegVal |= ((ptPiccPara->b_card_antenna_gain_val & 0xE0) >> 3);

	        As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                              AS3911_REG_RX_CONF3,
	                              ucRegVal );

			ucRegVal = As3911GetRegisterConfigVal( ISO14443_TYPEB_STANDARD, 
	                                         AS3911_REG_RX_CONF4 );
			ucRegVal &= 0xF0;
			ucRegVal |= (ptPiccPara->b_card_antenna_gain_val & 0x0F);

	        As3911SetRegisterConfigVal( ISO14443_TYPEB_STANDARD,
	                              AS3911_REG_RX_CONF4,
	                              ucRegVal );
		}
		/*add end*/
    }
    /*Felica antenna gain value*/
    if( 1 == ptPiccPara->f_card_antenna_gain_w )
    {
        ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
                                         AS3911_REG_RX_CONF1 );
		/*Enable Am Channel*/
		if((ucRegVal & EANBLE_CHANNEL_MASR) == ENABLE_AM_CHANNEL)
		{
    
			ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                         AS3911_REG_RX_CONF3 );
			ucRegVal &= 0x1F;
			ucRegVal |= (ptPiccPara->f_card_antenna_gain_val & 0xE0);

	        As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                              AS3911_REG_RX_CONF3,
	                              ucRegVal );

			ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                         AS3911_REG_RX_CONF4 );
			ucRegVal &= 0x0F;
			ucRegVal |= ((ptPiccPara->f_card_antenna_gain_val & 0x0F) << 4);

	        As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                              AS3911_REG_RX_CONF4,
	                              ucRegVal );
		}
		/*add by wanls 2014.05.12*/
		/*Enable Pm Channel*/
		else
		{
			ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                         AS3911_REG_RX_CONF3 );
			ucRegVal &= 0xE3;
			ucRegVal |= ((ptPiccPara->f_card_antenna_gain_val & 0xE0) >> 3);

	        As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                              AS3911_REG_RX_CONF3,
	                              ucRegVal );

			ucRegVal = As3911GetRegisterConfigVal( JISX6319_4_STANDARD, 
	                                         AS3911_REG_RX_CONF4 );
			ucRegVal &= 0xF0;
			ucRegVal |= (ptPiccPara->f_card_antenna_gain_val & 0x0F);

	        As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
	                              AS3911_REG_RX_CONF4,
	                              ucRegVal );
		}
		/*add end*/
    }

	if( 1 == ptPiccPara->f_conduct_w)
	{
        /*C*/
        ucRegVal = ptPiccPara->f_conduct_val;
		As3911SetRegisterConfigVal( JISX6319_4_STANDARD,
			AS3911_REG_RFO_AM_OFF_LEVEL, ucRegVal );
	}


    return 0;
}

struct ST_PCDOPS * GetAs3911OpsHandle()
{
    return (struct ST_PCDOPS *)&gtAs3911Ops;
}

