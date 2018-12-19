/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : iso14443_4.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-5-26
 *
 * Description:
 *      implement the half duplex communication protocol with ISO14443-4
 *
 * History:
 *
 * =============================================================================
 */
#include <stdlib.h>
#include <string.h>

#include "iso14443hw_hal.h"
#include "iso14443_4.h"


extern PICC_PARA gt_c_para; /* add by nt for paypass 3.0 test 2013/03/11 */

//#include <stdarg.h>
//
//int ScomSends( int iPort, unsigned char *pucTxBuf, int iNum )
//{
//    while( iNum-- )
//    {
//        s_UartSend( *pucTxBuf++ );
//    }
//    
//    return 0;
//}
//
//
//int __printf( char *format, ... )
//{
//	va_list       varg;
//	int           retv;
//	char          sbuffer[1024];
//	char          *where;
//
//   	memset( sbuffer, 0, sizeof( sbuffer ) );
//	where = &( sbuffer[ 0 ] );
//	va_start( varg, format );
//	retv = vsprintf( sbuffer,  format,  varg );
//	va_end( varg );
//	PortOpen( 0, "115200,8,n,1" );
//	ScomSends( 0, sbuffer, strlen( sbuffer ) ); 
//
//	return;
//}

/**
 * implement the half duplex communication protocol with ISO14443-4
 * 
 * parameters:
 *             ptPcdInfo  : PCD information structure pointer
 *             pucSrc     : the datas information will be transmitted by ptPcdInfo
 *             iTxNum     : the number of transmitted datas by ptPcdInfo
 *             pucDes     : the datas information will be transmitted by PICC
 *             piRxN      : the number of transmitted datas by PICC.
 * retval:
 *            0 - successfully
 *            others, error.
 */
int ISO14443_4_HalfDuplexExchange( struct ST_PCDINFO *ptPcdInfo, 
                                   unsigned char *pucSrc, 
                                   int iTxN, 
                                   unsigned char *pucDes, 
                                   int *piRxN )
{
    int            iRev  = 0;
    
    int            iTxNum= 0; /*the length of transmitted datas*/
    int            iTxLmt= 0; /*the maxium I-block length*/
    int            iTxCn = 0; /*the last transmitted I block length*/
    unsigned char  aucTxBuf[256];/*{PCB} + [NAD] + [CID] + [INF] + {CRC16}*/
    unsigned char  aucRxBuf[256];/*{PCB} + [NAD] + [CID] + [INF] + {CRC16}*/
    unsigned char* pucTxBuf  = aucTxBuf;
    unsigned char* pucRxBuf  = aucRxBuf;
    
    int            iSRetry   = 0; /*continuous s block count*/
    int            iIRetry   = 0; /*i block re-transmission count*/
    int            iErrRetry = 0; /*error cause re-transmission count*/
    
    unsigned int   uiWtx     = 1;/*the extra wait time in communication*/
    
    int            i;
    int s_swt_limit_count; /*after the special counts(user control), pcd responds timeout,if the card always sends s-block
	                           add by nt for paywave test  2013/03/11 */
    
    enum EXC_STEP  eExStep; /*excuting stage*/

    /*the first step is ready for transmission I block <$10.3.4.1>*/
    eExStep   = ORG_IBLOCK;
    *piRxN    = 0;
    uiWtx     = 1;
    iSRetry   = 0;
    iIRetry   = 0;
    iErrRetry = 0;
    
    /*if supported the CID or NAD*/
    if( ptPcdInfo->uiFsc < 16 )ptPcdInfo->uiFsc = 32;
    iTxLmt = ptPcdInfo->uiFsc - 3;/*PCB and CRC16*/
    if( ptPcdInfo->ucNadEn )iTxLmt--;
    if( ptPcdInfo->ucCidEn )iTxLmt--;
    
    /*the number of transmitted data bytes*/
    iTxNum = iTxN;
    s_swt_limit_count=0;/* add by nt for paywave test 2013/03/11 */
    /*protocol process*/
    do
    {
        switch( eExStep )
        {
        case ORG_IBLOCK:/*I block*/
//            __printf( "ORG_IBLOCK\r\n" );
            /*point to block buffer header*/
            pucTxBuf  = aucTxBuf;
            
            /*ready for I block*/
            if( iTxNum > iTxLmt )/*chaninning block*/
            {
                aucTxBuf[0] = 0x12 | ( ptPcdInfo->ucPcdPcb & ISO14443_CL_PROTOCOL_ISN );/*serial number and chaninning flag*/
                if( ptPcdInfo->ucCidEn )aucTxBuf[0] |= ISO14443_CL_PROTOCOL_CID;/*support CID*/
                if( ptPcdInfo->ucNadEn )aucTxBuf[0] |= ISO14443_CL_PROTOCOL_NAD;/*support NAD*/
                pucTxBuf++;
                
                if( ptPcdInfo->ucCidEn )*pucTxBuf++ = ptPcdInfo->ucCid & 0x0F;  
                if( ptPcdInfo->ucNadEn )*pucTxBuf++ = ptPcdInfo->ucNad & 0x0F;
                
                iTxCn    = iTxLmt;
                iTxNum  -= iTxLmt;
            }
            else /*non-chaninning block*/
            {
                aucTxBuf[0] = 0x02 | ( ptPcdInfo->ucPcdPcb & ISO14443_CL_PROTOCOL_ISN );/*serial number and non-chaninning flag*/
                if( ptPcdInfo->ucCidEn )aucTxBuf[0] |= ISO14443_CL_PROTOCOL_CID;/*support CID*/
                if( ptPcdInfo->ucNadEn )aucTxBuf[0] |= ISO14443_CL_PROTOCOL_NAD;/*support NAD*/
                pucTxBuf++;
                
                if( ptPcdInfo->ucCidEn )*pucTxBuf++ = ( ptPcdInfo->ucCid & 0x0F );  
                if( ptPcdInfo->ucNadEn )*pucTxBuf++ = ( ptPcdInfo->ucNad & 0x0F );
                
                iTxCn   = iTxNum;
                iTxNum  = 0;
            }
            memcpy( pucTxBuf, pucSrc, iTxCn );/*the INF of I block*/
            pucTxBuf += iTxCn;
            pucSrc   += iTxCn;/*the pointer of datas*/
         
            ptPcdInfo->ucPcdPcb = aucTxBuf[0];
            
            eExStep = ORG_TRARCV;
        break;
        case ORG_ACKBLOCK:/*R(ACK) response*/
//            __printf( "ORG_ACKBLOCK\r\n" );
            pucTxBuf  = aucTxBuf;
            aucTxBuf[0] = 0xA2 | ( ptPcdInfo->ucPcdPcb & 1 );
            if( ptPcdInfo->ucCidEn )aucTxBuf[0] |= ISO14443_CL_PROTOCOL_CID;/*support CID*/
            pucTxBuf++;
            if( ptPcdInfo->ucCidEn )*pucTxBuf++ = ( ptPcdInfo->ucCid & 0x0F );  
            eExStep = ORG_TRARCV;
        break;
        case ORG_NACKBLOCK:/*R(NACK) reponse*/
//            __printf( "ORG_NACKBLOCK\r\n" );
            pucTxBuf  = aucTxBuf;
            aucTxBuf[0] = 0xB2 | ( ptPcdInfo->ucPcdPcb & 1 );
            if( ptPcdInfo->ucCidEn )aucTxBuf[0] |= ISO14443_CL_PROTOCOL_CID;/*support CID*/
            pucTxBuf++;
            if( ptPcdInfo->ucCidEn )*pucTxBuf++ = ( ptPcdInfo->ucCid & 0x0F );  
            eExStep = ORG_TRARCV;
        break;
        case ORG_SBLOCK:/*S(uiWtx) block response*/
//            __printf( "ORG_SBLOCK\r\n" );
            pucTxBuf  = aucTxBuf;
            *pucTxBuf++ = 0xF2;
            *pucTxBuf++ = uiWtx & 0x3F;  
            eExStep = ORG_TRARCV;    
        break;
        case ORG_TRARCV:
//            __printf( "ORG_TRARCV\r\n" );
            /*statistical transmission error count*/
            iErrRetry++;
            
            /*because if S-uiWtx requested by PICC, must be response with the same value
             *process the case with uiWtx > 59 here.
             */
            /* by tanbx 20140303 for case as TA404_10, TA408_8, TA411_10, 
            TA417_6, TB404_10, TB408_8, TB409_10, TB411_10, TB417_6：
            WTX>59 当协议错处理并复位载波*/
			if( uiWtx > 59 )
			{
			    iRev = ISO14443_4_ERR_PROTOCOL;
			    eExStep = RCV_INVBLOCK;
			    break;
		    }
			/*change end*/
            ptPcdInfo->uiFwt = uiWtx * ptPcdInfo->uiFwt;
            
            /*transmitted datas and configurating the timeout parameters*/
            iRev = PcdTransCeive( ptPcdInfo, aucTxBuf, ( pucTxBuf - aucTxBuf ), aucRxBuf, 256 );
			if(gt_c_para.user_control_w == 1){
					if(!(kbhit()) && (getkey()== gt_c_para.user_control_key_val)){	
						iRev = ISO14443_PCD_ERR_USER_CANCEL;							
						eExStep = RCV_INVBLOCK;							
						break;				 
					}	
					kbflush();				
			}
			/* change by wanls 2013.6.8 */
			if((gt_c_para.wait_retry_limit_w == 1) && (gt_c_para.wait_retry_limit_val > 0))
			{					
					if(s_swt_limit_count >= gt_c_para.wait_retry_limit_val){	
						iRev = ISO14443_HW_ERR_COMM_TIMEOUT;					
						eExStep = RCV_INVBLOCK;							
						break;					
					}				
			}
			/* add end */
			
//            __printf( "\r\npcd tranceive=%d\r\n PCD > ", iRev );
//            for( i = 0; i < ( pucTxBuf - aucTxBuf ); i++  )
//            {
//                 __printf( "%02X ", aucTxBuf[i] );
//                 if( 0 == ( ( i + 1 ) % 16 ) )__printf( "\r\n       " );	
//            }
//            if( 0 != ( ( i + 1 ) % 16 ) )__printf( "\r\n PICC < " );	
//            for( i = 0; i <  ptPcdInfo->uiPcdTxRNum; i++  )
//            {
//                 __printf( "%02X ", aucRxBuf[i] );
//                 if( 0 == ( ( i + 1 ) % 16 ) )__printf( "\r\n        " );	
//            }
//            if( 0 != ( ( i + 1 ) % 16 ) )__printf( "\r\n------------------------\r\n" );	
            
            if( ISO14443_HW_ERR_COMM_PARITY == iRev ||
                ISO14443_HW_ERR_COMM_CODE == iRev || 
                ISO14443_HW_ERR_COMM_TIMEOUT == iRev ||
                ISO14443_HW_ERR_COMM_CRC == iRev)/*transmission with CRC or parity error*/
            {
                if( iErrRetry > ISO14443_PROTOCOL_RETRANSMISSION_LIMITED ||
                    iSRetry > ISO14443_PROTOCOL_RETRANSMISSION_LIMITED )
                {
                    eExStep = RCV_INVBLOCK;/*consider as protocol error*/
                }
                else
                {
                    if( ptPcdInfo->ucPiccPcb & ISO14443_CL_PROTOCOL_CHAINED )/*<$10.3.5.6> & <$10.3.5.8>*/
                    {
                        eExStep = ORG_ACKBLOCK;
                    }
                    else /*<$10.3.5.3> & <$10.3.5.5>*/
                    {
                        eExStep = ORG_NACKBLOCK;  
                    }
                }
            }
            else
            {
                iErrRetry = 0;
                
                if( !iRev )/*transmission without error*/
                {   
                    /*<$10.3.5.4 & $10.3.5.7>*/
                    pucRxBuf = aucRxBuf;
                    
                    /*class the block type to process*/
                    if( 0x02 == ( aucRxBuf[0] & 0xE2 ) )
                    {
                        if( ( 0 == ( aucRxBuf[0] & 0x2 ) ) || ( ptPcdInfo->uiPcdTxRNum > 254 ) ||
                            ( ( ISO14443_CL_PROTOCOL_CID | ISO14443_CL_PROTOCOL_NAD ) & aucRxBuf[0] )
                          )
                        {/*EMV CL requirements <$10.3.5.4 & $10.3.5.7>,ptPcdInfo's ifsd=256*/
                            //__printf( "invblock1\r\n" );
                            eExStep = RCV_INVBLOCK;
                        }
                        else
                        {
                            iSRetry = 0;
                            eExStep  = RCV_IBLOCK;
                        }
                    }
                    else if( 0xA0 == ( aucRxBuf[0] & 0xE0 ) )
                    {
                        /*EMV CL requirements <$10.3.5.4 & $10.3.5.7>*/
                        if( ( ( ISO14443_CL_PROTOCOL_CID | ISO14443_CL_PROTOCOL_NAD ) & aucRxBuf[0] ) ||
                            ( ptPcdInfo->uiPcdTxRNum > 2 )
                          )/*EMV CL requirements <$10.3.5.4 & $10.3.5.7>*/
                        {    
                            //__printf( "invblock2\r\n" );
                            eExStep = RCV_INVBLOCK;
                        }
                        else
                        {
                            iSRetry = 0;
                            eExStep  = RCV_RBLOCK;
                        }
                    }
                    else if( 0xC0 == ( aucRxBuf[0] & 0xC0 ) )
                    {   
                        /*EMV CL requirements <$10.3.5.4 & $10.3.5.7>*/
                        if( ( ( ISO14443_CL_PROTOCOL_CID | ISO14443_CL_PROTOCOL_NAD ) & aucRxBuf[0] )||
                            ( ptPcdInfo->uiPcdTxRNum > 2 )
                          )/*EMV CL requirements <$10.3.5.4 & $10.3.5.7>*/
                        {    
                            //__printf( "invblock3\r\n" );
                            eExStep = RCV_INVBLOCK;
                        }
                        else
                        {
                            iSRetry++;
                            eExStep = RCV_SBLOCK;
                        }
                    }
                    else 
                    {
                    	//__printf( "invblock4\r\n" );
                        eExStep = RCV_INVBLOCK;/*protocol error*/
                    }
                }
                else
                {
                    /*PICC enter auto-recovery step*/   
                }
            }
            
            ptPcdInfo->uiFwt /= uiWtx;
            uiWtx       = 1;/*restore the FWT*/
        break;
        case RCV_IBLOCK:
//            __printf( "RCV_IBLOCK\r\n" );
            if( iTxNum )/*command datas has be not completed*/
            {
            	//__printf( "invblock5\r\n" );
                eExStep = RCV_INVBLOCK;
            }
            else
            {
                pucRxBuf++;/*jump over PCB*/
                
                /*the serail number equal to ptPcdInfo's current serial number*/
                if( ( ptPcdInfo->ucPcdPcb & ISO14443_CL_PROTOCOL_ISN ) == 
                    ( aucRxBuf[0] & ISO14443_CL_PROTOCOL_ISN ) )
                {   
                    ptPcdInfo->ucPiccPcb = aucRxBuf[0];/*the last receipt of a block*/
                    
                    if( aucRxBuf[0] & ISO14443_CL_PROTOCOL_CHAINED )/*chainning block*/
                    {
                        if( aucRxBuf[0] & ISO14443_CL_PROTOCOL_CID )pucRxBuf++;/*jump over CID*/
                        if( aucRxBuf[0] & ISO14443_CL_PROTOCOL_NAD )pucRxBuf++;/*jump over NAD*/
                         
                        /*R(ACK)*/
                        eExStep = ORG_ACKBLOCK;
                    }
                    else/*non-chaninning block*/
                    {
                        if( aucRxBuf[0] & ISO14443_CL_PROTOCOL_CID )pucRxBuf++;/*jump over CID*/
                        if( aucRxBuf[0] & ISO14443_CL_PROTOCOL_NAD )pucRxBuf++;/*jump over NAD*/
                        
                        eExStep = NON_EVENT;   
                    }
                    
                    /*process received datas informations*/
                    if( ptPcdInfo->uiPcdTxRNum >= ( pucRxBuf - aucRxBuf ) )
                    {
                        memcpy( pucDes, pucRxBuf, ( ptPcdInfo->uiPcdTxRNum - ( pucRxBuf - aucRxBuf ) ) );
                        pucDes  += ptPcdInfo->uiPcdTxRNum - ( pucRxBuf - aucRxBuf );
                        *piRxN  += ptPcdInfo->uiPcdTxRNum - ( pucRxBuf - aucRxBuf );
                    }
                    /*toggle serial number <$10.3.3.3>*/
                    ptPcdInfo->ucPcdPcb ^= ISO14443_CL_PROTOCOL_ISN;
                }
                else /*the serail number don't equal to ptPcdInfo's current serial number*/
                {
                    //__printf( "invblock6\r\n" );
                    eExStep = RCV_INVBLOCK;   
                }
            }
        break;
        case RCV_RBLOCK:
//            __printf( "RCV_RBLOCK\r\n" );
            if( aucRxBuf[0] & 0x10 )/*R(NAK) <$10.3.4.6>*/
            {
            	//__printf( "invblock7\r\n" );
                eExStep = RCV_INVBLOCK;
            }
            else
            {
                /*the serail number equal to ptPcdInfo's current serial number*/
                if( ( ptPcdInfo->ucPcdPcb & ISO14443_CL_PROTOCOL_ISN ) == 
                    ( aucRxBuf[0] & ISO14443_CL_PROTOCOL_ISN ) )
                {
                    /*receiving R(ACK), send the next i block <$10.3.4.5>*/
                    if( ptPcdInfo->ucPcdPcb & ISO14443_CL_PROTOCOL_CHAINED )
                    {   
                        /*toggle serial number <$10.3.3.3>*/
                        ptPcdInfo->ucPcdPcb ^= ISO14443_CL_PROTOCOL_ISN;
                        
                        iIRetry = 0;
                        eExStep = ORG_IBLOCK;
                    }
                    else
                    {
                    	//__printf( "invblock8\r\n" );
                        eExStep = RCV_INVBLOCK; 
                    }
                }
                else /*re-transmitted last i block <$10.3.4.4>*/
                {
                    iIRetry++;
                    
                    if( iIRetry > ISO14443_PROTOCOL_RETRANSMISSION_LIMITED )
                    {
                    	//__printf( "invblock9\r\n" );
                        eExStep = RCV_INVBLOCK;/*protocol error*/      
                    }
                    else
                    {
                        iTxNum += iTxCn;
                        pucSrc -= iTxCn;
                        eExStep = ORG_IBLOCK;
                    }
                }
            }
        break;
        case RCV_SBLOCK:/*S(uiWtx) request*/
//            __printf( "RCV_SBLOCK\r\n" );
            if( 0xF2 != ( aucRxBuf[0] & 0xF7 ) )/*receiving the S(uiWtx)*/
            {
            	//__printf( "invblock10\r\n" );
                eExStep = RCV_INVBLOCK;
            }
            else
            {
                pucRxBuf = aucRxBuf + 1;
                if( aucRxBuf[0] & ISO14443_CL_PROTOCOL_CID )pucRxBuf++;/*jump over CID*/
                if( 0 == ( *pucRxBuf & 0x3F ) )
                {
                    //__printf( "invblock11\r\n" );
                    eExStep = RCV_INVBLOCK;
                }
                else
                {
                    s_swt_limit_count++; /*add by nt 20121224 for zhoujie paypass 3.0*/
                    uiWtx = ( *pucRxBuf & 0x3F );/*response S(uiWtx) using the same value with PICC*/
                    eExStep = ORG_SBLOCK;
                }
            }
        break;
        case RCV_INVBLOCK:/*protocol error*/
//            __printf( "RCV_INVBLOCK\r\n" );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev && iRev != ISO14443_PCD_ERR_USER_CANCEL)/* modify by nt 2013/03/11 */
				iRev = ISO14443_4_ERR_PROTOCOL;
            eExStep = NON_EVENT;/*ready to return*/
            
            PcdCarrierOff( ptPcdInfo );/*Liubo 2011-11-30*/
            PcdCarrierOn( ptPcdInfo );
            
        break;
        default:
        break;
        }
    }while( NON_EVENT != eExStep );

    return iRev;
}

/**
 * send 'DESELECT' command to PICC
 *
 * param:
 *       ptPcdInfo  :  PCD information structure pointer
 *       ucSPcb     :  S-block PCB
 *                       0xC2 - DESELECT( no parameter )
 *                       0xF2 - WTX( one byte wtx parameter )
 *       ucParam    :  S-block parameters
 * reval:
 *       0 - successfully
 *       others, failure
 */
int ISO14443_4_SRequest( struct ST_PCDINFO *ptPcdInfo, 
                         unsigned char      ucSPcb,
                         unsigned char      ucParam )
{
    unsigned char  aucTxBuf[5];
    unsigned char *pucTxBuf;
    unsigned char  aucRxBuf[5];
    
    int            ieReTransCount = 0;
    
    int            iRev = 0;
    
    pucTxBuf = aucTxBuf;
    *pucTxBuf++ = ucSPcb;
    if( 0xF2 == ucSPcb )*pucTxBuf++ = ucParam;
    
    do
    {
        ieReTransCount++;
        iRev = PcdTransCeive( ptPcdInfo, 
                              aucTxBuf, 
                              ( pucTxBuf - aucTxBuf ), 
                              aucRxBuf, 
                              5 );
        if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev  )
        {
            if( ieReTransCount > ISO14443_PROTOCOL_RETRANSMISSION_LIMITED )
            { 
                break;
            }
        }
        else
        {
            if( iRev )
            {
                if( ieReTransCount > ISO14443_PROTOCOL_RETRANSMISSION_LIMITED )
                {    
                    iRev  = ISO14443_4_ERR_PROTOCOL;
                    break;
                }
            }   
        }
    }while( iRev );
    
    return iRev;
}
