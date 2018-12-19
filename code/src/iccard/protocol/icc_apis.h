#ifndef _ICC_APIS_H_
#define _ICC_APIS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL   0
#endif

/* Command-APDU */
typedef struct 
{
	unsigned char  Command[4];   /* Command Header */
	unsigned short Lc;           /* The number of bytes  of command data field */
	unsigned char  DataIn[512];  /* Command Data */
	unsigned short Le;           /* the number of bytes expected in the response data field  */
}APDU_SEND;

/* Response-APDU */
typedef struct
{
	unsigned short LenOut;      /* the Maximum number of bytes can be received in the data buffer */
	unsigned char  DataOut[512];/* Receive Data Buffer*/
	unsigned char  SWA;         /* First State Word */
	unsigned char  SWB;         /* Second State Word*/
}APDU_RESP;

/**
 * ===================================================================
 * the hardware and software of smart card initialize
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 *      none
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      none
 * ===================================================================
 */
void          s_Icc_Init( void );

/**
 * ===================================================================
 * To get the usage status of channel
 *
 * parameter:
 *      none
 *
 * return:
 *      > 0 - occupied
 *      0   - idle  
 */
int IccPowerStatus( void );

/**
 * ===================================================================
 * card initialize, get ATR, parse ATR, application parameter
 * -------------------------------------------------------------------
 * parameter:
 * -------------------------------------------------------------------
 *     [input]slot:
 * 		        bit[2:0] : slot number(0~7). 
 *              bit[4:3] : For Voltage Operation 
 *                            00 - 5V; 
 *                            01 - 1.8V; 
 *                            10 - 3V; 
 *                            11 - 5V
 *              bit[5]  :  For PPS condition
 *                            0  - Not Supported
 *                            1  - Supported
 *              bit[6]  :  For Data Rate   
 *                            0 - 9600bps 
 *                            1 - 38400bps
 *              bit[7]   : For Specification 
 *                            0 - EMV; 
 *                            1 - ISO7816-3 
 * -------------------------------------------------------------------
 *      [output]ATR:  
 *              ATR[ 0 ]     : the valid number of bytes of ATR in buffer
 *              ATR[ 1 : n ] : Data buffer of ATR.
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 * 		please refer to the "Error Code List" below
 * ===================================================================
 */
unsigned char IccInit(unsigned char slot, unsigned char *ATR);

/**
 * ===================================================================
 * Check whether the card is in socket or not
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input] slot :  please refer to the function named "IccInit" 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 * 		please refer to the "Error Code List" below
 * ===================================================================
 */
unsigned char IccDetect(unsigned char slot);

/**
 * ===================================================================
 * Data Exchange between IC card and Terminal
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input] slot      :  refer to the description of the function 
 * 			named "IccInit"  
 * -------------------------------------------------------------------
 * [input] ApduSend  : a pointer to the command struct
 * 			that the card will send
 * -------------------------------------------------------------------
 * [output] ApduRecv  : a pointer to the response struct 
 * 			 that the card have recevied  
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 * 	please refer to "error code list"
 * ===================================================================
 */
unsigned char IccIsoCommand(unsigned char slot, APDU_SEND *ApduSend, APDU_RESP *ApduRecv);

/**
 * ===================================================================
 * halt the IC card, implement the operation of Power Dump for Card 
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to the description of the function 
 * 			named "IccInit" 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      none.
 * ===================================================================
 */
void          IccClose(unsigned char slot);

/**
 * ===================================================================
 * To set the flag that indicate whether the terminal will automatically
 * get the remaining data from the card by sending "GET RESPONSE" command
 * or not 
 * -------------------------------------------------------------------
 * parameter： 
 * -------------------------------------------------------------------
 * [input]  slot     :  please refer to the description of the function
 * 			named "IccInit" 
 * -------------------------------------------------------------------
 * [input]  autoresp :  flag
 *                     0 - the terminal get the remaining data from the card
 *                     	   by sending "GET RESPONSE" command
 *                     1 - no auto
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      none
 * -------------------------------------------------------------------
 * notice：
 * -------------------------------------------------------------------
 *     It just be effective for the card with only T=0 protocol
 * ===================================================================
 */
void          IccAutoResp(unsigned char slot, unsigned char autoresp);

 /* ===================================================================
 * parameters relating to card initialization setting
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  please refer to the relating description of
 * 			the function named "IccInit"    
 * -------------------------------------------------------------------
 * [input]ParaConfig  : analyse as bitmap, usage of bitmap showing as below
 *              #VCC_5000MV      5000mV
 *              #VCC_3000MV      3000mV
 *              #VCC_1800MV      1800mV
 *              #BAUD_NORMAL     Fi = 372, Di = 1
 *              #BAUD_M2         Fi = 372, Di = 2
 *              #BAUD_M4         Fi = 372, Di = 4
 *              #SUP_PPS         PPS
 *              #SPEC_ISO        ISO mode                         
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *       0 - success 
 *       others, failure
 * -------------------------------------------------------------------
 * warning：
 *       1) The config should be made ahead of the funtion named
 *       "IccInit" called, or won't take effect
 *       2) The config may be made as the example below
 *         eg.
 *         IccChannelConfig( 0,  VCC_3000MV | BAUD_M4 | SUP_PPS | SPEC_ISO );
 *       3) The config of Speed Mode will be ignored if the mode of specification
 *       assigns EMV
 *	
 * ===================================================================
 */
#define BAUD_MASK      ( 3 << 5 )  /* mask for baud rate config */
#define BAUD_M4        ( 3 << 5 )  /* 38400bps */
#define BAUD_M2        ( 2 << 5 )  /* 19200bps */
#define BAUD_NORMAL    ( 1 << 5 )  /* 9600bps */

#define VCC_MASK       ( 3 << 2 )  /* mask for voltage config */
#define VCC_5000MV     ( 3 << 2 )  /* 5V */
#define VCC_3000MV     ( 2 << 2 )  /* 3V */
#define VCC_1800MV     ( 1 << 2 )  /* 1.8V */

#define SUP_PPS        ( 1 << 1 )  /* PPS config */
#define SPEC_ISO       ( 1 << 0 )  /* ISO config */
int           IccChannelConfig( unsigned char slot, unsigned int ParaConfig );

/**
 * ===================================================================
 * To get the number of times that the card plug in or out after powerup
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 *      none
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      number of times the card plug in or out
 * ===================================================================
 */
int           IccGetInsertCnt( void );

/**
 * ===================================================================
 * switch the operating mode between synchronised card and 
 * asynchronised card 
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - disable / 1 - enable
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *         none
 * ===================================================================
 */
void Mc_Clk_Enable( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * set the direction of IO pin, namely OUTPUT or INPUT
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - output / 1 - input
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *       none
 * ===================================================================
 */
void Mc_Io_Dir( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * get the logic level on IO pin
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *         logic level: 
 *             0   - LOW
 *             not 0 - HIGH
 * ===================================================================
 */
unsigned char Mc_Io_Read( unsigned char slot );

/**
 * ===================================================================
 * change the logic level on IO Pin
 * -------------------------------------------------------------------
 * parameter:
 * -------------------------------------------------------------------
 * [input]  slot     :  please refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_Io_Write( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * Power supply on or off to Card
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - power off / 1 - power on 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *     none
 * ===================================================================
 */
void Mc_Vcc( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * change the logic level on Reset Pin
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_Reset( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * supply clock signal to Card or not
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_Clk( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * change the logic level on C4 Pin
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_C4_Write( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * change the logic level on C8 Pin
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_C8_Write( unsigned char slot, unsigned char mode );

/**
 * ===================================================================
 * get the logic level from C4 Pin
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
unsigned char Mc_C4_Read( unsigned char slot );

/**
 * ===================================================================
 * get the logic level from C8 Pin
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
unsigned char Mc_C8_Read( unsigned char slot );

/**
 * ===================================================================
 * to get the flag indicating whether the card slot is occupied or not
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     : refer to the description of the function  
 *			named "IccInit"
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *     1 - occupied / 0 - idle
 * ===================================================================
 */
unsigned char Read_CardSlotInfo( unsigned char slot );

/**
 * ===================================================================
 * to set the flag indicating whether the card slot is occupied or not
 *
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to the description of the function 
 * 			named "IccInit"  
 * -------------------------------------------------------------------
 * [input]  slotopen :  1 - occupied / 0 - idle
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *     none
 * ===================================================================
 */
void Write_CardSlotInfo( unsigned char slot, unsigned char slotopen );

/* Error Code List For API */
#define   DEV_OTHERERR       0x03  /* other fault on device，probably invoked by hardware*/
#define   SLOTERR            0x04  /* invalid channel */
#define   PROTOCALERR        0x06  /* protocol error */
#define   CARD_OUT           0x02  /* card was pulled out */
#define   NO_INITERR         0x07  /* without initialization */
#define   DATA_LENTHERR      0x05  /* the length of data is invalid */
#define   PARERR             0x01  /* parity error */
#define   FUN_PARAERR        0x09  /* argument input is with fault */
#define   UNKOWN_LOGICAL     0x0A  /* unkown logical communication */
#define   DEV_CONFLICT       0x0B  /* confliction for share timers */

#define   ATR_TSERR          0x31  
#define   ATR_TCKERR         0x32 
#define   ATR_TIMEOUT        0x33 
#define   ATR_TA1ERR         0x34 
#define   ATR_TA2ERR         0X35 
#define   ATR_TA3ERR         0x36 
#define   ATR_TB1ERR         0x37 
#define   ATR_TB2ERR         0x38 
#define   ATR_TB3ERR         0x39 
#define   ATR_TC1ERR         0x3a 
#define   ATR_TC2ERR         0x3b
#define   ATR_TC3ERR         0x3c
#define   ATR_TD1ERR         0x3d
#define   ATR_TD2ERR         0x3e
#define   ATR_LENGTHERR      0x3f  /*  ATR the length of data is invalid  */
#define   ATR_TDORDERERR     0x30  /*  ISO标准中td包含的协议序列错误*/

#define   T0_TIMEOUT         0x41 
#define   T0_MORESENDERR     0X42  /* the number of times retransfer(send) is out of limit  */
#define   T0_MORERECEERR     0X43  /* the number of times retransfer(receive) is out of limit  */
#define   T0_PARERR          0x44 
#define   T0_INVALIDSW       0X45  /* invalid status word  */

#define   T1_BWTERR          0x11
#define   T1_CWTERR          0x12 
#define   T1_ABORTERR        0x13  /* give up or abort communication  */
#define   T1_EDCERR          0x14  
#define   T1_SYNCHERR        0x15  /* synchron error */
#define   T1_EGTERR          0x16  /* each character guard time error */
#define   T1_BGTERR          0x17  

/* Prologue Field */
#define   T1_NADERR          0x18  
#define   T1_PCBERR          0x19  
#define   T1_LENGTHERR       0x1a 

#define   T1_IFSCERR         0x1b
#define   T1_IFSDERR         0x1c
#define   T1_MOREERR         0x1d  /* the number of times retransfer is out of limit */
#define   T1_PARITYERR       0x1e 
#define   T1_INVALIDBLOCK    0x1f  
#define   T1_SENDERR         0x10  

#define   PPSS_ERR           0x20  /* not 0xFF  */
#define   PPS1_ERR           0x21  /* PPS1 is inconsistent with that request */
#define   PCK_ERR            0x22 

#ifdef __cplusplus
}
#endif

#endif
