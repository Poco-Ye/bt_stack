#ifndef PCD_API_H
#define PCD_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define  CHIP_TYPE_RC531     ( 1 )
#define  CHIP_TYPE_PN512     ( 2 )
#define  CHIP_TYPE_RC663     ( 3 )
#define  CHIP_TYPE_AS3911    ( 4 )



#ifndef NULL
#define NULL   ( 0 )
#endif

#define PCD_VERSION    "4.11R" 
#define PCD_DATETIME   "2015.09.21"
#define PCD_VER        "411-150921-R"

typedef struct{
   	unsigned char Command[4];
   	unsigned short Lc;
   	unsigned char  DataIn[512];
   	unsigned short Le;
}APDU_SEND;

typedef struct{
	unsigned short LenOut;
   	unsigned char  DataOut[512];
   	unsigned char  SWA;
   	unsigned char  SWB;
}APDU_RESP;

/**
 * convert the error code from HAL to APIs
 *
 * param:
 *     iRev : HAL error code
 * 
 * retval:
 *     the API's return code
 *
 */
unsigned char ConvertHalToApi( int iRev );

/**
 * Read the version of PCD driver
 * 
 * params:
 *          ver : the buffer of version information
 *                it must have 30 bytes space.
 * retval:
 *          no return value.
 */
void PcdGetVer( char* ver );

/**
 * Initialising the PCD device
 * 
 * params:
 *          pucRfType: the type of Rf chip
            pucRfPara: antenna parameter
 * retval:
 *          return value.
 */
unsigned char PcdInit(unsigned char *pucRfType, unsigned char *pucRfPara);

/**
 * loading the configuration into driver chip and open carrier.
 *  
 * params:
 *         no params
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccOpen( void );

/**
 * loading the configuration into PCD structure.
 *  
 * params:
 *         ucMode     :  
 *         ptPiccPara : 
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccSetup( unsigned char ucMode, PICC_PARA *ptPiccPara );

/**
 * loading the configuration into driver chip and open carrier.
 *  
 * params:
 *         ucMode        :  the card polling mode, 0, 1, 'A', 'B' or 'M'. 
 *         pucCardType   :  return the card type, 'M', 'A', 'B'etc.
 *         pucSerialInfo :  the serial number of PICC
 *         pucCID        :  
 *         pucOther      :  
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccDetect( unsigned char  ucMode, 
                          unsigned char* pucCardType,
                          unsigned char* pucSerialInfo,
                          unsigned char* pucCID,
                          unsigned char* pucOther );

/**
 * using current configuration to send and receive.
 *  
 * params:
 *         pucSrc  : the buffer will be sent
 *         iTxN    : the length of datas sent
 *         pucDes  : the received buffer
 *         iRxN    : the length of receiver buffer
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccCmdExchange( unsigned int   uiTxN,
                               unsigned char *ucpSrc, 
                               unsigned int   *uipRxN, 
                               unsigned char *ucpDes ); 
                          
/**
 * send format information according to iso7816-4, and receiving the 
 * response from PICC.
 *  
 * params:
 *         ucCID      :   the cid of picc
 *         ptApduSend : the sent data structure
 *         ptApduRecv : the received data structure
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccIsoCommand( unsigned char ucCID, 
                              APDU_SEND*    ptApduSend, 
                              APDU_RESP*    ptApduRecv );

/**
 * remove card operation.
 *  
 * params:
 *         ucMode : 
 *         ucCID  :
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccRemove( unsigned char ucMode, unsigned char ucCID );


/**
 * loading the configuration of Felica into driver chip.
 *  
 * params:
 *         ucSpeed       : Felica speed parameter( 0 - 212bps, 1 - 424Kbps )
 *         ucModInvert   : inverted modulation
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccInitFelica( unsigned char ucSpeed, unsigned char ucModInvert );

/**
 * close carrier.
 *  
 * params:
 *         no params
 * retval:
 *         0 - successfully
 *         others, failure
 */
void PiccClose( void );

/**
 * loading the configuration into driver chip and open carrier.
 *  
 * params:
 *         ucMode      :  0 - Read / 1 - Write
 *         ucRegAddr   :  register address
 *         pucOutData  :  i/o data buffer
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccManageReg( unsigned char  ucMode, 
                             unsigned char  ucRegAddr, 
                             unsigned char* pucOutData );

/**
 * Mifare read crypto1 authenticate operations.
 * 
 * parameters:
 *          ucType         : the Key group, 'A' or 'B'
 *          ucBlkNo        : the block number
 *          pucPwd         : the password( 6 bytes )
 *          pucSerialNo    : the UID( 4 bytes )
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1Authority( unsigned char  ucType, 
                           unsigned char  ucBlkNo,
                           unsigned char *pucPwd, 
                           unsigned char *pucSerialNo );
 
/**
 * Mifare read clock operations.
 * 
 * parameters:
 *          ucBlkNo     : the block number
 *          pucBlkValue : the value of operation(16 bytes)
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1ReadBlock( unsigned char ucBlkNo, unsigned char *pucBlkValue );

/**
 * Mifare write block operations.
 * 
 * parameters:
 *          ucBlkNo     : the block number
 *          pucBlkValue : the value of operation(16 bytes)
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1WriteBlock( unsigned char ucBlkNo, unsigned char *pucBlkValue );

/**
 * Mifare operations, inc, dec or backup.
 * 
 * parameters:
 *          ucType        : operation type,
 *                            '+' - increase operation
 *                            '-' - decrease operation 
 *                            '>' - backup operation
 *          ucBlkNo       : the block number
 *          pucValue      : the value of operation
 *          ucUpdateBlkNo : backup operation destinct block number
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1Operate( unsigned char  ucType, 
                         unsigned char  ucBlkNo, 
                         unsigned char *pucValue, 
                         unsigned char  ucUpdateBlkNo );
        
/**
 * the error code definitions
 */
#define RET_RF_OK                      0x00 /*成功*/
                                       
#define RET_RF_ERR_PARAM               0x01 /*参数错误*/
#define RET_RF_ERR_NO_OPEN             0x02 /*射频模块未开启*/
                                       
#define RET_RF_ERR_NOT_ACT             0x03 /*卡片未激活*/
#define RET_RF_ERR_MULTI_CARD          0x14 /*多卡冲突*/
#define RET_RF_ERR_TIMEOUT             0x15 /*超时无响应*/
#define RET_RF_ERR_PROTOCOL            0x16 /*协议错误*/
                                       
#define RET_RF_ERR_TRANSMIT            0x17 /*通信传输错误*/
#define RET_RF_ERR_AUTH                0x18 /*M1卡认证失败*/
#define RET_RF_ERR_NO_AUTH             0x04 /*扇区未认证*/ /* change by wanls 2012.03.15 改变定义值与RC531统一*/
#define RET_RF_ERR_VAL                 0x1A /*数值块数据格式有误,或DesFire卡片操作中文件大小错误*/

#define RET_RF_ERR_CARD_EXIST          0x06 /*卡片仍在感应区内*//* change by wanls 2012.03.15 改变定义值与RC531统一*/
#define RET_RF_ERR_STATUS              0x1C /*卡片状态错误(如A/B卡调用M1卡接口, 或M1卡调用PiccIsoCommand接口)*/
                            
#define RET_RF_ERR_OVERFLOW            0x1E
#define RET_RF_ERR_FAILED              0x1F /*DesFire卡片的应答数据错误*/
                                       
#define RET_RF_ERR_COLLERR             0x20
#define RET_RF_ERR_FIFO                0x21 /*DesFire卡片操作中应用缓冲区空间不足*/
#define RET_RF_ERR_CRC                 0x22
#define RET_RF_ERR_FRAMING             0x23
#define RET_RF_ERR_PARITY              0x24
                    
#define RET_RF_ERR_DES_VAL             0x25 /*DesFire卡片应答数据DES运算结果不一致*/
#define RET_RF_ERR_NOT_ALLOWED         0x26 /*操作不允许, 例如当前所选文件不是记录文件时，不能执行读记录操作*/
#define RET_RF_ERR_USER_CANCEL         0x27 /* add by nt for paypass 3.0 test 2013/03/11 */
#define RET_RF_ERR_CHIP_ABNORMAL       0xFF /*接口芯片不存在或异常*/

/*compliance with P80, no used*/
#define RET_RF_DET_ERR_INVALID_PARAM   0x01
#define RET_RF_DET_ERR_NO_POWER        0x02
#define RET_RF_DET_ERR_NO_CARD         0x03
#define RET_RF_DET_ERR_COLL            0x04
#define RET_RF_DET_ERR_ACT             0x05
#define RET_RF_DET_ERR_PROTOCOL        0x06

/*compliance with p80, no used */
#define RET_RF_CMD_ERR_INVALID_PARAM   0x01
#define RET_RF_CMD_ERR_NO_POWER        0x02
#define RET_RF_CMD_ERR_NO_CARD         0x03
#define RET_RF_CMD_ERR_TX              0x04
#define RET_RF_CMD_ERR_PROTOCOL        0x05   

#define TERM_R50                       0x01
#define TERM_D100                      0x02  
#define TERM_BCM5892                   0x04

#ifdef __cplusplus
}
#endif

#endif
