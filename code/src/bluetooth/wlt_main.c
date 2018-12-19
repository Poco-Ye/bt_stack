/*****< main.c >***************************************************************/
/*      Copyright 2012 Stonestreet One.                                       */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  MAIN - Main application implementation.                                   */
/*                                                                            */
/*  Author:  Tim Cook                                                         */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   01/28/12  T. Cook        Initial creation.                               */
/******************************************************************************/
//#include <posapi.h>
//#include <posapi_all.h>
#include <string.h>

#include "LIB_Private.h"
#include "LIB_Main.h"                /* Main application header.                  */

/////////////////////////////////////////////////////////

#include "LIB_GAP.h"
#include "LIB_GAPBLE.h"
#include "LIB_GATTBLE.h"

#include "LIB_SPP.h"
#include "LIB_SPPLE.h"
#include "LIB_ISPP.h"

//#define USE_ATCMD

#include "wlt2564.h"

#define SYSTEM_STACK_SIZE                        0x1800  /* Denotes the size  */
                                                         /* (in bytes) of the */
                                                         /* SafeRTOS system   */
                                                         /* stack.            */

//#define MAIN_THREAD_STACK_SIZE                   (configMINIMAL_STACK_SIZE + 1024)
                                                         /* Denotes the size  */
                                                         /* (in bytes) of the */
                                                         /* main application  */
                                                         /* thread.           */

//#define IDLE_THREAD_STACK_SIZE                    (configMINIMAL_STACK_SIZE + 512)
                                                         /* Denotes the size  */
                                                         /* (in bytes) of the */
                                                         /* idle thread.      */

#define DEFAULT_THREAD_PRIORITY                       3  /* Denotes the       */
                                                         /* priority of the   */
                                                         /* Main Application  */
                                                         /* Thread.           */

#define MAX_COMMAND_LENGTH                         (64)  /* Denotes the max   */
                                                         /* buffer size used  */
                                                         /* for user commands */
                                                         /* input via the     */
                                                         /* User Interface.   */

#define LED_TOGGLE_RATE_SUCCESS                    (500) /* The LED Toggle    */
                                                         /* rate when the demo*/
                                                         /* successfully      */
                                                         /* starts up.        */

#define INDENT_LENGTH                               (3)  /* Denotes the number*/
                                                         /* of character      */
                                                         /* spaces to be used */
                                                         /* for indenting when*/
                                                         /* displaying SDP    */
                                                         /* Data Elements.    */

   /* The following parameters are used when configuring HCILL Mode.    */
#define HCILL_MODE_INACTIVITY_TIMEOUT              (500)
#define HCILL_MODE_RETRANSMIT_TIMEOUT              (100)


#define UI_MODE_IS_CLIENT      (2)
#define UI_MODE_IS_SERVER      (1)
#define UI_MODE_SELECT         (0)
#define UI_MODE_IS_INVALID     (-1)

int				   UI_Mode; 				/* Holds the UI Mode.			   */


#define MAX_SUPPORTED_COMMANDS                     (100)  /* Denotes the       */
                                                         /* maximum number of */
                                                         /* User Commands that*/
                                                         /* are supported by  */
                                                         /* this application. */



#define NO_COMMAND_ERROR                           (-1)  /* Denotes that no   */
                                                         /* command was       */
                                                         /* specified to the  */
                                                         /* parser.           */

#define INVALID_COMMAND_ERROR                      (-2)  /* Denotes that the  */
                                                         /* Command does not  */
                                                         /* exist for         */
                                                         /* processing.       */

#define EXIT_CODE                                  (-3)  /* Denotes that the  */
                                                         /* Command specified */
                                                  /* was the Exit      */
                                                         /* Command.          */

#define FUNCTION_ERROR                             (-4)  /* Denotes that an   */
                                                         /* error occurred in */
                                                         /* execution of the  */
                                                         /* Command Function. */

#define TO_MANY_PARAMS                             (-5)  /* Denotes that there*/
                                                         /* are more          */
                                                         /* parameters then   */
                                                         /* will fit in the   */
                                                         /* UserCommand.      */

#define INVALID_PARAMETERS_ERROR                   (-6)  /* Denotes that an   */
                                                         /* error occurred due*/
                                                         /* to the fact that  */
                                                         /* one or more of the*/
                                                         /* required          */
                                                         /* parameters were   */
                                                         /* invalid.          */

#define UNABLE_TO_INITIALIZE_STACK                 (-7)  /* Denotes that an   */
                                                         /* error occurred    */
                                                         /* while Initializing*/
                                                         /* the Bluetooth     */
                                                         /* Protocol Stack.   */

#define INVALID_STACK_ID_ERROR                     (-8)  /* Denotes that an   */
                                                         /* occurred due to   */
                                                         /* attempted         */
                                                         /* execution of a    */
                                                         /* Command when a    */
                                                         /* Bluetooth Protocol*/
                                                         /* Stack has not been*/
                                                         /* opened.           */

#define UNABLE_TO_REGISTER_SERVER                  (-9)  /* Denotes that an   */
                                                         /* error occurred    */
                                                         /* when trying to    */
                                                         /* create a Serial   */
                                                         /* Port Server.      */

#define EXIT_TEST_MODE                             (-10) /* Flags exit from   */
                                                         /* Test Mode.        */

#define EXIT_MODE                                  (-11) /* Flags exit from   */
                                                         /* any Mode.         */



															/* any Mode.		 */
unsigned int 	   NumberCommands;			/* Variable which is used to hold  */
                                                                                                       /* the number of Commands that are */
                                                                                                       /* supported by this application.  */
                                                                                                       /* Commands are added individually.*/
    

//static BoardStr_t		  Function_BoardStr;	   /* Holds a BD_ADDR string in the   */
                                                                                                       /* various functions.			  */


extern int RFCOMMPortList[30];

/* The following type definition represents the structure which holds*/
/* the command and parameters to be executed. 					   */
typedef struct _tagUserCommand_t
{
      char			  *Command;
      ParameterList_t  Parameters;
} UserCommand_t;

      /* The following type definition represents the generic function	   */
      /* pointer to be used by all commands that can be executed by the    */
      /* test program.													   */
typedef int (*CommandFunction_t)(ParameterList_t *TempParam);

      /* The following type definition represents the structure which holds*/
      /* information used in the interpretation and execution of Commands. */
typedef struct _tagCommandTable_t
{
      char				*CommandName;
      CommandFunction_t  CommandFunction;
} CommandTable_t;
														  

													   
CommandTable_t	   CommandTable[MAX_SUPPORTED_COMMANDS]; /* Variable which is  */

//#define DEBUG_MAIN
#ifdef DEBUG_MAIN
#define Display(_x)            do { printf_uart _x; } while(0)
#else
#define Display(_x)
#endif

#ifdef DEBUG_ATCMD
#define DisplayATCMD(_x)            do { printf_uart _x; } while(0)
#else
#define DisplayATCMD(_x)
#endif

#ifdef DEBUG_ATCMD
#define DisplayATCMDNUM(_num,_x)           Stm32_ConsoleWrite(_num,_x) //do { Stm32_ConsoleWrite _num,_x; } while(0)
#else
#define DisplayATCMDNUM(_num,_x)
#endif


   /* The following buffers are used to hold the Thread Stacks of the   */
   /* SafeRTOS Threads that are created by this module.                 */
//static unsigned long MainAppThreadStack[(MAIN_THREAD_STACK_SIZE/sizeof(unsigned long)) + 1];
//static unsigned long IdleTaskStack[(IDLE_THREAD_STACK_SIZE/sizeof(unsigned long)) + 1];

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */


   //ParameterList_t *TempParam;
static unsigned int InputIndex;
static char         Input[MAX_COMMAND_LENGTH];

   /* Internal function prototypes.                                     */
static void *ToggleLED(void *UserParameter);
static int GetInput(void);
//static void GetInput(unsigned int InputBufferLength, char *InputBuffer);
//static int GetInput(unsigned int InputBufferLength, char *InputBuffer);


#define MAX_INQUIRY_RESULTS 35

BD_ADDR_t           InquiryResultList_cmd[MAX_INQUIRY_RESULTS]; /* Variable which   */
                                                    /* contains the inquiry result     */
                                                    /* received from the most recently */
                                                    /* preformed inquiry.              */
													
BD_ADDR_t			CurrentCBRemoteBD_ADDR;


extern  Mailbox_t		ret_valmail;  //mailbox hander
   
Callback_Event_Data_t *Callback_Event_Data;  //callback event&data

//typedef char BoardStr_t[15];
static BoardStr_t		   Callback_BoardStr;		/* Holds a BD_ADDR string in the   */
														/* Callbacks.					   */
static void BD_ADDRToStr(BD_ADDR_t Board_Address, BoardStr_t BoardStr)
{
	BTPS_SprintF((char *)BoardStr, "0x%02X%02X%02X%02X%02X%02X", Board_Address.BD_ADDR5, Board_Address.BD_ADDR4, Board_Address.BD_ADDR3, Board_Address.BD_ADDR2, Board_Address.BD_ADDR1, Board_Address.BD_ADDR0);
}


static BTPSCONST char *AlertCategories[] =
{
      "Simple Alert",
      "E-Mail",
      "News",
      "Call",
      "Missed Call",
      "SMS MMS",
      "Voice Mail",
      "Schedule",
      "High Priority Alert",
      "Instant Message",
      "All Categories"
} ;


   /* Sample Accessory Identification Information.                      */
   /* The following are Test Strings for Process Status values.         */
static BTPSCONST char *ProcessStatus[] =
{
   "Success",
   "Timeout Retrying",
   "Timeout Halting",
   "General Failure",
   "Process Failure",
   "Process Failure Retrying"
} ;


   /* Application Tasks.                                                */
static void DisplayCallback(char Character);
static void ProcessCharacters(void *UserParameter);
static void ApplicationIdleHook(void);
int MainThread(void *UserParameter);
static void UserInterface_Server(void);
static void UserInterface_Selection(void);
static unsigned long StringToUnsignedInteger(char *StringInteger);

#ifdef USE_ATCMD
  static char *StringParser(char *String,char chr);
#else
  static char *StringParser(char *String);
#endif
static int CommandParser(UserCommand_t *TempCommand, char *Input);
static int CommandInterpreter(UserCommand_t *TempCommand);
static int AddCommand(char *CommandName, CommandFunction_t CommandFunction);
static CommandFunction_t FindCommand(char *Command);
static void ClearCommands(void);
static int DisplayHelp(ParameterList_t *TempParam);	  

Boolean_t ProcessCommandLine(char *String);
static void DisplayPrompt(void);

static int InitializeVariable(void);

  #ifdef USE_ATCMD
  static int DisplayHelp(ParameterList_t *TempParam)
  {
	  DisplayATCMD(("\r\n"));
	  DisplayATCMD(("*********************************************************************\r\n"));
	  DisplayATCMD(("* GAP Command :   AT+Inquiry, AT+Pair,AT+SetLocalName,              *\r\n"));
	  DisplayATCMD(("*                 AT+DisplayInquiry,AT+Pincode,AT+GetLocalName,     *\r\n"));	  
	  DisplayATCMD(("*                 AT+SetClassOfDevice,AT+GetClassOfDevice,          *\r\n"));
	  DisplayATCMD(("*                 AT+SetDiscoveryMode,AT+SetConnectMode,            *\r\n"));
	  DisplayATCMD(("*                 AT+SetPairMode,AT+GetLocalAddr,AT+GetRemoteName,  *\r\n"));

  	  DisplayATCMD(("* GAPLE Command : AT+AdvertiseLE, AT+StartScanning, AT+StopScanning,*\r\n"));
          DisplayATCMD(("*                 AT+ConnectLE, AT+DisconnectLE,                    *\r\n")); 
	  #ifdef BULID_DEMO_SPP
  	  DisplayATCMD(("* SPP Command :   AT+OpenSPP, AT+CloseSPP,AT+SPPDisconnect,         *\r\n"));
	  DisplayATCMD(("*                 AT+SPPConnect, AT+SPPWrite,                       *\r\n"));
	  #endif
	  #ifdef BULID_DEMO_SPPLE
          DisplayATCMD(("* SPPLE Command : AT+RegisterSPPLE, AT+DiscoverSPPLE,               *\r\n")); 
          DisplayATCMD(("*                 AT+SPPLEWrite,AT+ConfigureSPPLE                   *\r\n"));
       #endif
	   #ifdef BULID_DEMO_AVRCP
	  DisplayATCMD(("* AVRCP Command : AT+RegisterAVRCP,AT+UNRegisteravrcp,AT+AVRConnect,*\r\n"));
          DisplayATCMD(("*                 AT+AVRDisconnect,AT+AVRPause,AT+AVRPlay,AT+AVRStop*\r\n"));
          DisplayATCMD(("*                 AT+AVRNext,AT+AVRLast,AT+GetSongName,             *\r\n"));
          DisplayATCMD(("*                 AT+GetArtistName,AT+GetPlayingTime,               *\r\n"));
          #endif
		  #ifdef BULID_DEMO_AUD
          DisplayATCMD(("* AUD Command :   AT+AUDInit,AT+OpenAUD,AT+CloseAUD,AT+PlayAUD,     *\r\n"));
          DisplayATCMD(("*                 AT+StopAUD,                                       *\r\n"));
          #endif
          
	  DisplayATCMD(("*                 AT+Help,  AT+ECHO,AT+MINFO                        *\r\n"));
	  DisplayATCMD(("*********************************************************************\r\n"));

  }
  #else
		 /* The following function is responsible for displaying the current  */
		 /* Command Options for either Serial Port Client or Serial Port	  */
		 /* Server.  The input parameter to this function is completely 	  */
		 /* ignored, and only needs to be passed in because all Commands that */
		 /* can be entered at the Prompt pass in the parsed information.  This*/
		 /* function displays the current Command Options that are available  */
		 /* and always returns zero.										  */
	  static int DisplayHelp(ParameterList_t *TempParam)
	  {
	  
		   Display(("\r\n"));
		   Display(("*******************************************************************\r\n"));
		   Display(("* GAP Command :   Inquiry, Pair,SetLocalName,GetLocalName,        *\r\n"));
		   Display(("*                 SetClassOfDevice,GetClassOfDevice,              *\r\n"));
		   Display(("*                 SetDiscoveryMode,SetConnectMode,                *\r\n"));
		   Display(("*                 SetPairMode,ServiceDiscovery                                     *\r\n"));
		   

		   Display(("* GAPLE Command : AdvertiseLE, StartScanning, StopScanning,      *\r\n"));
		   Display(("*                 ConnectLE, DisconnectLE,                       *\r\n"));	
#ifdef BULID_DEMO_SPP
		   
		   Display(("* SPP Command :   SPPProfileOpen, SPPProfileClose,SPPDisconnect, *\r\n"));
		   Display(("*                 SPPConnect, SPPWrite,SPPRead,                  *\r\n"));
#endif

#ifdef BULID_DEMO_SPPLE

		   Display(("* SPPLE Command : RegisterSPPLE, DiscoverSPPLE,  ConfigureSPPLE, *\r\n"));	
		   Display(("*                 LERead,LESend,                                 *\r\n"));
#endif

#ifdef BULID_DEMO_MAP

		   Display(("* MAP Command :   OpenMAP, CloseMAP, OpenNotification,           *\r\n"));
		   Display(("*                 CloseNotification, RequestNotification,        *\r\n"));
		   Display(("*                 GetMessage,                                    *\r\n"));
#endif
#ifdef BULID_DEMO_HFR

		   Display(("* HFRE Command :  HandsFree, QueryRemoteStatus, DialPhone,       *\r\n"));
		   Display(("*                 RedialPhone, AnswerCall, HangUpCall,           *\r\n"));
		   Display(("*                 ManageAudio, QueryOperator,                    *\r\n"));
#endif
#ifdef BULID_DEMO_ANP

		   Display(("* ANP Command :   RegisterANS, UnregisterANS, DiscoverANS,       *\r\n"));
		   Display(("*                 ConfigureRemoteANS, NotifyNewAlerts,           *\r\n"));
		   Display(("*                 NotifyUnreadAlerts, EnableNewAlertNotifications*\r\n"));
		   Display(("*                 DisableNewAlertNotifications,                  *\r\n"));
		   Display(("*                 EnableUnreadAlertNotifications,                *\r\n"));
		   Display(("*                 DisableUnreadAlertNotifications,               *\r\n"));
#endif
#ifdef BULID_DEMO_PXP

		   Display(("* PXP Command :   RegisterLLS, UnregisterLLS, DiscoverLLS,       *\r\n"));
		   Display(("*                 GetAlertLevel, SetAlertLevel, RegisterIAS,     *\r\n"));
		   Display(("*                 UnregisterIAS, DiscoverIAS, RegisterTPS,       *\r\n"));
		   Display(("*                 UnregisterTPS, DiscoverTPS, GetTxPowerLevel,   *\r\n"));
		   Display(("*                 SetTxPowerLevel,                               *\r\n"));
#endif
#ifdef BULID_DEMO_HID

		   Display(("* HID Command :   InitializeDevice, HIDConnect, HIDCloseConnect, *\r\n"));
		   Display(("*                 HIDDataWrite                                   *\r\n"));


#endif

#ifdef BULID_DEMO_PBAP

		
           Display(("* PBAP Command :  PBAPServiceDiscovery, OpenPBAP, ClosePBAP,     *\r\n"));
		   Display(("*                 PBAPPullPhoneBook,                             *\r\n"));
			

#endif

#ifdef BULID_DEMO_AVRCP

		   Display(("* AVRCP Command : RegisterAVRCP, UnRegisterAVRCP, ConnectAVRCP,  *\r\n"));
		   Display(("*                 DisconnectAVRCP, SendPassThroughMessage,       *\r\n"));
		   Display(("*                 GetSongName, GetArtistName, GetPlayingTime,    *\r\n"));
		   Display(("*                 SetInquiryMode, SetMaxPower,                   *\r\n"));

		  

#endif

#ifdef BULID_DEMO_AUD

		   Display(("* AUD Command : InitializeAUD, OpenAUD, CloseAUD, PlayAUD,       *\r\n"));
		   Display(("*               StopAUD,                                         *\r\n"));


#endif

		   Display(("*                 Help,                                          *\r\n"));
		   Display(("******************************************************************\r\n"));
		   
		   
	  
		 return(0);
	  }

 #endif
 
  #ifdef USE_ATCMD
  
  static void UserInterface_Server(void)
  {
	 /* Next display the available commands.							  */
	 //DisplayHelp(NULL);
  
	 /* Clear the installed command.									  */
	 ClearCommands();
  

        /* Install the commands revelant for this UI.						  */
        AddCommand("AT+INQUIRY", Inquiry_Cmd);
        AddCommand("AT+DISPLAYINQUIRY", DisplayInquiryList_Cmd);

	 AddCommand("AT+PAIR", Pair_Cmd);
	 AddCommand("AT+PINCODE", PINCodeResponse_Cmd);
	 AddCommand("AT+SETLOCALNAME", SetLocalName_Cmd);
	 AddCommand("AT+GETLOCALNAME", GetLocalName_Cmd);
	 AddCommand("AT+GETREMOTENAME", GetRemoteDeviceName_Cmd);
	 AddCommand("AT+SETCLASSOFDEVICE", SetClassOfDevice_Cmd);
	 AddCommand("AT+GETCLASSOFDEVICE", GetClassOfDevice_Cmd);
	 AddCommand("AT+SETDISCOVERYMODE",SetDiscoverabilityMode_Cmd);
	 AddCommand("AT+SETCONNECTMODE",SetConnectabilityMode_Cmd);
	 AddCommand("AT+SETPAIRMODE",SetPairabilityMode_Cmd);
	 AddCommand("AT+GETLOCALADDR",GetLocalAddress_Cmd);
         
#ifdef BULID_DEMO_SPP
	 AddCommand("AT+OPENSPP", SPPProfileOpen_Cmd);
	 AddCommand("AT+CLOSESPP", SPPProfileClose_Cmd);
	 AddCommand("AT+SPPDISCONNECT", SPPDisconnect_Cmd);
	 AddCommand("AT+SPPCONNECT", SPPConnect_Cmd);
	 AddCommand("AT+SPPWRITE", SPPDataWrite_Cmd);
		//AddCommand("SPPREAD", SPPDataRead_Cmd);
#endif

	 AddCommand("AT+ADVERTISELE", AdvertiseLE_Cmd);
	 AddCommand("AT+STARTSCANNING", StartScanning_Cmd);
	 AddCommand("AT+STOPSCANNING", StopScanning_Cmd);
	 AddCommand("AT+CONNECTLE", ConnectLE_Cmd);
	 AddCommand("AT+DISCONNECTLE", DisconnectLE_Cmd);
	 AddCommand("DISCOVERGAPS", DiscoverGAPS_Cmd);

#ifdef BULID_DEMO_SPPLE
	 AddCommand("AT+DISCOVERSPPLE", DiscoverSPPLE_Cmd);
	 AddCommand("AT+REGISTERSPPLE", RegisterSPPLE_Cmd);
	 AddCommand("UNREGISTERSPPLE", UnRegisterSPPLE_Cmd);
	 AddCommand("AT+CONFIGURESPPLE", ConfigureSPPLE_Cmd);
	 AddCommand("AT+SPPLEWRITE", SendDataCommand_Cmd);
	 AddCommand("LEREAD", ReadDataCommand_Cmd);
#endif

#ifdef BULID_DEMO_AVRCP	 
	AddCommand("AT+REGISTERAVRCP",RegisterProfile_Cmd);
        AddCommand("AT+UNREGISTERAVRCP",UnRegisterProfile_Cmd);
	AddCommand("AT+AVRCONNECT",AVRConnect_Cmd);
	AddCommand("AT+AVRDISCONNECT",AVRDisconnect_Cmd);
	AddCommand("AT+AVRPAUSE",SendPassThroughMessage_Cmd);
	AddCommand("AT+AVRPLAY",SendPassThroughMessage_Cmd);
	AddCommand("AT+AVRSTOP",SendPassThroughMessage_Cmd);
	AddCommand("AT+AVRNEXT",SendPassThroughMessage_Cmd);
	AddCommand("AT+AVRLAST",SendPassThroughMessage_Cmd);
	AddCommand("AT+GETSONGNAME",GetElementAttributeSongName_Cmd);
	AddCommand("AT+GETARTISTNAME",GetElementAttributeArtistName_Cmd);
	AddCommand("AT+GETPLAYINGTIME",GetElementAttributePlayingTime_Cmd);
#endif

#ifdef BULID_DEMO_AUD	
	AddCommand("AT+AUDINIT",InitializeAUD_Cmd);
	AddCommand("AT+OPENAUD",OpenRemoteStream_Cmd);
	AddCommand("AT+CLOSEAUD",CloseStream_Cmd);
	AddCommand("AT+PLAYAUD",PlayTone_Cmd);
	AddCommand("AT+STOPAUD",StopTone_Cmd);
#endif
	AddCommand("AT+HELP", DisplayHelp);
	 
		
  }
  #else
	  /* This function is responsible for taking the input from the user   */
	  /* and dispatching the appropriate Command Function.	First, this    */
	  /* function retrieves a String of user input, parses the user input  */
	  /* into Command and Parameters, and finally executes the Command or  */
	  /* Displays an Error Message if the input is not a valid Command.    */
   static void UserInterface_Server(void)
   {
	  /* Next display the available commands.							   */
	  DisplayHelp(NULL);
   
	  /* Clear the installed command.									   */
	  ClearCommands();



	  /* Install the commands revelant for this UI. 					   */
#ifdef BUILD_DEMO_GAP
	  AddCommand("INQUIRY", Inquiry_Cmd);
	  AddCommand("PAIR", Pair_Cmd);
	  AddCommand("SETLOCALNAME", SetLocalName_Cmd);
	  AddCommand("GETLOCALNAME", GetLocalName_Cmd);
	  AddCommand("SETCLASSOFDEVICE", SetClassOfDevice_Cmd);
	  AddCommand("GETCLASSOFDEVICE", GetClassOfDevice_Cmd);
	  AddCommand("SETDISCOVERYMODE",SetDiscoverabilityMode_Cmd);
	  AddCommand("SETCONNECTMODE",SetConnectabilityMode_Cmd);
	  AddCommand("SETPAIRMODE",SetPairabilityMode_Cmd);
      AddCommand("SERVICEDISCOVERY",ServiceDiscovery_Cmd);
	  AddCommand("PASSKEYRESPONSE",PassKeyResponse_Cmd);
#endif

#ifdef BULID_DEMO_SPP
          AddCommand("SPPPROFILEOPEN", SPPProfileOpen_Cmd);
          AddCommand("SPPPROFILECLOSE", SPPProfileClose_Cmd);
          AddCommand("SPPDISCONNECT", SPPDisconnect_Cmd);
          AddCommand("SPPCONNECT", SPPConnect_Cmd);
          AddCommand("SPPWRITE", SPPDataWrite_Cmd);
          AddCommand("SPPREAD", SPPDataRead_Cmd);
#endif

#ifdef BUILD_DEMO_GAPLE
	  AddCommand("ADVERTISELE", AdvertiseLE_Cmd);
	  AddCommand("STARTSCANNING", StartScanning_Cmd);
	  AddCommand("STOPSCANNING", StopScanning_Cmd);
	  AddCommand("CONNECTLE", ConnectLE_Cmd);
	  AddCommand("DISCONNECTLE", DisconnectLE_Cmd);
	  AddCommand("DISCOVERGAPS", DiscoverGAPS_Cmd);
#endif

#ifdef BULID_DEMO_SPPLE
	  AddCommand("DISCOVERSPPLE", DiscoverSPPLE_Cmd);
	  AddCommand("REGISTERSPPLE", RegisterSPPLE_Cmd);
	  AddCommand("UNREGISTERSPPLE", UnRegisterSPPLE_Cmd);
	  AddCommand("CONFIGURESPPLE", ConfigureSPPLE_Cmd);
	  AddCommand("LESEND", SendDataCommand_Cmd);
	  AddCommand("LEREAD", ReadDataCommand_Cmd);
#endif
	  
	  AddCommand("HELP", DisplayHelp);
	 

#ifdef BULID_DEMO_ANP
	  AddCommand("REGISTERANS", RegisterANS_Cmd);
	  AddCommand("UNREGISTERANS", UnregisterANS_Cmd);
	  AddCommand("DISCOVERANS", DiscoverANS_Cmd);
	  AddCommand("CONFIGUREREMOTEANS", ConfigureRemoteANS_Cmd);
	  AddCommand("NOTIFYNEWALERTS", NotifyNewAlerts_Cmd);
	  AddCommand("NOTIFYUNREADALERTS", NotifyUnreadAlerts_Cmd);
	  AddCommand("ENABLENEWALERTNOTIFICATIONS", EnableNewAlertNotifications_Cmd);
	  AddCommand("ENABLEUNREADALERTNOTIFICATIONS", EnableUnreadAlertNotifications_Cmd);
#endif

#ifdef BULID_DEMO_PXP
   
          AddCommand("REGISTERLLS", RegisterLLS_Cmd);
	  AddCommand("UNREGISTERLLS", UnregisterLLS_Cmd);
	  AddCommand("DISCOVERLLS", DiscoverLLS_Cmd);
	  AddCommand("GETALERTLEVEL", GetAlertLevel_Cmd);
	  AddCommand("SETALERTLEVEL", SetAlertLevel_Cmd);
   
	  AddCommand("REGISTERIAS",   RegisterIAS_Cmd);
	  AddCommand("UNREGISTERIAS", UnregisterIAS_Cmd);
	  AddCommand("DISCOVERIAS", DiscoverIAS_Cmd);
	  AddCommand("CONFIGUREALERTLEVEL",ConfigureAlertLevel_Cmd);
   
	  AddCommand("REGISTERTPS",   RegisterTPS_Cmd);
	  AddCommand("UNREGISTERTPS", UnregisterTPS_Cmd);
	  AddCommand("DISCOVERTPS", DiscoverTPS_Cmd);
	  AddCommand("GETTXPOWERLEVEL",GetTxPowerLevel_Cmd);
	  AddCommand("SETTXPOWERLEVEL",SetTxPowerLevel_Cmd);
#endif  

#ifdef BULID_DEMO_MAP
	  /*		   MAP				 */
	  AddCommand("OPENMAP", MAP_OpenRemoteServer_Cmd);
	  AddCommand("CLOSEMAP", CloseConnection_Cmd);
	  AddCommand("OPENNOTIFICATION",OpenNotificationServer_Cmd);
	  AddCommand("CLOSENOTIFICATION",CloseNotificationServer_Cmd);
	  AddCommand("REQUESTNOTIFICATION",RequestNotification_Cmd);
	  AddCommand("GETMESSAGE",GetMessage_Cmd);

#endif

#ifdef BULID_DEMO_HFR
	  /*		   HFP				   */
	  AddCommand("HANDSFREE",HFREOpenHandsFreeServer_Cmd);
	  AddCommand("QUERYREMOTESTATUS",HFREQueryRemoteIndicatorsStatus_Cmd);
	  AddCommand("DIALPHONE",HFREDialPhoneNumber_Cmd);
	  AddCommand("REDIALPHONE",HFRERedialLastPhoneNumber_Cmd);
	  AddCommand("ANSWERCALL",HFREAnswerIncomingCall_Cmd);
	  AddCommand("HANGUPCALL",HFREHangUpCall_Cmd);
	  AddCommand("MANAGEAUDIO",ManageAudioConnection_Cmd);
	  AddCommand("QUERYOPERATOR",HFREQueryOperator_Cmd);
#endif 

#ifdef BULID_DEMO_HID
      /*                 HID                            */
          AddCommand("INITIALIZEDEVICE",InitializeHIDDevice_Cmd);
          AddCommand("HIDCONNECT",HIDConnectHost_Cmd);
          AddCommand("HIDCLOSECONNECT",HIDCloseConnection_Cmd);
          AddCommand("HIDDATAWRITE",HIDDataWrite_Cmd);
		
		

#endif

#ifdef BULID_DEMO_PBAP
	/*           PBAP                  */
	AddCommand("PBAPSERVICEDISCOVERY",ServiceDiscovery_Cmd);
        AddCommand("OPENPBAP",PBAPOpenRemoteServer_Cmd);
	AddCommand("CLOSEPBAP",PBAPCloseConnection_Cmd);
	AddCommand("PBAPPULLPHONEBOOK",PullPhonebook_Cmd);
	
   
#endif

#ifdef BULID_DEMO_AVRCP
	/*            AVRCP             */
	
 	AddCommand("REGISTERAVRCP",RegisterProfile_Cmd);
        AddCommand("UNREGISTERAVRCP",UnRegisterProfile_Cmd);
	AddCommand("CONNECTAVRCP",AVRConnect_Cmd);
	AddCommand("DISCONNECTAVRCP",AVRDisconnect_Cmd);
	AddCommand("SENDPASSTHROUGHMESSAGE",SendPassThroughMessage_Cmd);
	AddCommand("GETSONGNAME",GetElementAttributeSongName_Cmd);
	AddCommand("GETARTISTNAME",GetElementAttributeArtistName_Cmd);
	AddCommand("GETPLAYINGTIME",GetElementAttributePlayingTime_Cmd);
	AddCommand("SETINQUIRYMODE",SetInquiryMode_Cmd);
	AddCommand("SETMAXPOWER",SetMaxOutputPower_Cmd);
	

#endif

#ifdef BULID_DEMO_AUD
	AddCommand("INITIALIZEAUD",InitializeAUD_Cmd);
	AddCommand("OPENAUD",OpenRemoteStream_Cmd);
	AddCommand("CLOSEAUD",CloseStream_Cmd);
	AddCommand("PLAYAUD",PlayTone_Cmd);
	AddCommand("STOPAUD",StopTone_Cmd);
	

#endif




	  
   }
#endif 
	  /* The following function is responsible for choosing the user	   */
	  /* interface to present to the user.								   */
   static void UserInterface_Selection(void)
   {
	UserInterface_Server();
   }

	  /* The following function is responsible for converting number	   */
	  /* strings to there unsigned integer equivalent.	This function can  */
	  /* handle leading and tailing white space, however it does not handle*/
	  /* signed or comma delimited values.	This function takes as its	   */
	  /* input the string which is to be converted.  The function returns  */
	  /* zero if an error occurs otherwise it returns the value parsed from*/
	  /* the string passed as the input parameter.						   */
   static unsigned long StringToUnsignedInteger(char *StringInteger)
   {
	  int			IsHex;
	  unsigned int	Index;
	  unsigned long ret_val = 0;
   
	  /* Before proceeding make sure that the parameter that was passed as */
	  /* an input appears to be at least semi-valid.					   */
	  if((StringInteger) && (BTPS_StringLength(StringInteger)))
	  {
		 /* Initialize the variable.									   */
		 Index = 0;
   
		 /* Next check to see if this is a hexadecimal number.			   */
		 if(BTPS_StringLength(StringInteger) > 2)
		 {
			if((StringInteger[0] == '0') && ((StringInteger[1] == 'x') || (StringInteger[1] == 'X')))
			{
			   IsHex = 1;
   
			   /* Increment the String passed the Hexadecimal prefix.	   */
			   StringInteger += 2;
			}
			else
			   IsHex = 0;
		 }
		 else
			IsHex = 0;
   
		 /* Process the value differently depending on whether or not a    */
		 /* Hexadecimal Number has been specified.						   */
		 if(!IsHex)
		 {
			/* Decimal Number has been specified.						   */
			while(1)
			{
			   /* First check to make sure that this is a valid decimal    */
			   /* digit.												   */
			   if((StringInteger[Index] >= '0') && (StringInteger[Index] <= '9'))
			   {
				  /* This is a valid digit, add it to the value being	   */
				  /* built. 											   */
				  ret_val += (StringInteger[Index] & 0xF);
   
				  /* Determine if the next digit is valid.				   */
				  if(((Index + 1) < BTPS_StringLength(StringInteger)) && (StringInteger[Index+1] >= '0') && (StringInteger[Index+1] <= '9'))
				  {
					 /* The next digit is valid so multiply the current    */
					 /* return value by 10. 							   */
					 ret_val *= 10;
				  }
				  else
				  {
					 /* The next value is invalid so break out of the loop.*/
					 break;
				  }
			   }
   
			   Index++;
			}
		 }
		 else
		 {
			/* Hexadecimal Number has been specified.					   */
			while(1)
			{
			   /* First check to make sure that this is a valid Hexadecimal*/
			   /* digit.												   */
			   if(((StringInteger[Index] >= '0') && (StringInteger[Index] <= '9')) || ((StringInteger[Index] >= 'a') && (StringInteger[Index] <= 'f')) || ((StringInteger[Index] >= 'A') && (StringInteger[Index] <= 'F')))
			   {
				  /* This is a valid digit, add it to the value being	   */
				  /* built. 											   */
				  if((StringInteger[Index] >= '0') && (StringInteger[Index] <= '9'))
					 ret_val += (StringInteger[Index] & 0xF);
				  else
				  {
					 if((StringInteger[Index] >= 'a') && (StringInteger[Index] <= 'f'))
						ret_val += (StringInteger[Index] - 'a' + 10);
					 else
						ret_val += (StringInteger[Index] - 'A' + 10);
				  }
   
				  /* Determine if the next digit is valid.				   */
				  if(((Index + 1) < BTPS_StringLength(StringInteger)) && (((StringInteger[Index+1] >= '0') && (StringInteger[Index+1] <= '9')) || ((StringInteger[Index+1] >= 'a') && (StringInteger[Index+1] <= 'f')) || ((StringInteger[Index+1] >= 'A') && (StringInteger[Index+1] <= 'F'))))
				  {
					 /* The next digit is valid so multiply the current    */
					 /* return value by 16. 							   */
					 ret_val *= 16;
				  }
				  else
				  {
					 /* The next value is invalid so break out of the loop.*/
					 break;
				  }
			   }
   
			   Index++;
			}
		 }
	  }
   
	  return(ret_val);
   }
   
#ifdef USE_ATCMD

   /* The following function is responsible for parsing strings into	*/
   /* components.  The first parameter of this function is a pointer to */
   /* the String to be parsed.	This function will return the start of	*/
   /* the string upon success and a NULL pointer on all errors. 		*/
static char *StringParser(char *String,char chr)
{
   int	 Index;
   char *ret_val = NULL;

   /* Before proceeding make sure that the string passed in appears to	*/
   /* be at least semi-valid.											*/
   if((String) && (BTPS_StringLength(String)))
   {
	  /* The string appears to be at least semi-valid.	Search for the	*/
	  /* first space character and replace it with a NULL terminating	*/
	  /* character. 													*/
	  for(Index=0, ret_val=String;Index < BTPS_StringLength(String);Index++)
	  {
		 /* Is this the space character.								*/
		 if((String[Index] == chr) || (String[Index] == '\r') || (String[Index] == '\n'))
		 {
			/* This is the space character, replace it with a NULL		*/
			/* terminating character and set the return value to the	*/
			/* begining character of the string.						*/
			String[Index] = '\0';
			break;
		 }
	  }
   }

   return(ret_val);
}


#else

	  /* The following function is responsible for parsing strings into    */
	  /* components.  The first parameter of this function is a pointer to */
	  /* the String to be parsed.  This function will return the start of  */
	  /* the string upon success and a NULL pointer on all errors.		   */
   static char *StringParser(char *String)
   {
	  int	Index;
	  char *ret_val = NULL;
   
	  /* Before proceeding make sure that the string passed in appears to  */
	  /* be at least semi-valid.										   */
	  if((String) && (BTPS_StringLength(String)))
	  {
		 /* The string appears to be at least semi-valid.  Search for the  */
		 /* first space character and replace it with a NULL terminating   */
		 /* character.													   */
		 for(Index=0, ret_val=String;Index < BTPS_StringLength(String);Index++)
		 {
			/* Is this the space character. 							   */
			if((String[Index] == ' ') || (String[Index] == '\r') || (String[Index] == '\n'))
			{
			   /* This is the space character, replace it with a NULL	   */
			   /* terminating character and set the return value to the    */
			   /* begining character of the string. 					   */
			   String[Index] = '\0';
			   break;
			}
		 }
	  }
   
	  return(ret_val);
   }

#endif

   
	  /* This function is responsable for taking command strings and	   */
	  /* parsing them into a command, param1, and param2.  After parsing   */
	  /* this string the data is stored into a UserCommand_t structure to  */
	  /* be used by the interpreter.  The first parameter of this function */
	  /* is the structure used to pass the parsed command string out of the*/
	  /* function.	The second parameter of this function is the string    */
	  /* that is parsed into the UserCommand structure.  Successful 	   */
	  /* execution of this function is denoted by a retrun value of zero.  */
	  /* Negative return values denote an error in the parsing of the	   */
	  /* string parameter.												   */
   static int CommandParser(UserCommand_t *TempCommand, char *Input)
   {
	  int			 ret_val;
	  int			 StringLength;
	  char			*LastParameter;
	  unsigned int	 Count		   = 0;
   
	  /* Before proceeding make sure that the passed parameters appear to  */
	  /* be at least semi-valid.										   */
	  if((TempCommand) && (Input) && (BTPS_StringLength(Input)))
	  {
		 /* First get the initial string length.						   */
		 StringLength = BTPS_StringLength(Input);
   
		 /* Retrieve the first token in the string, this should be the	   */
		 /* commmand.													   */
		 #ifdef USE_ATCMD
		 TempCommand->Command = StringParser(Input,'=');
		 #else
		 TempCommand->Command = StringParser(Input);
   		 #endif
		 /* Flag that there are NO Parameters for this Command Parse.	   */
		 TempCommand->Parameters.NumberofParameters = 0;
   
		  /* Check to see if there is a Command 						   */
		 if(TempCommand->Command)
		 {
			/* Initialize the return value to zero to indicate success on  */
			/* commands with no parameters. 							   */
			ret_val    = 0;
   
			/* Adjust the UserInput pointer and StringLength to remove the */
			/* Command from the data passed in before parsing the		   */
			/* parameters.												   */
			Input		 += BTPS_StringLength(TempCommand->Command)+1;
			StringLength  = BTPS_StringLength(Input);
   
			/* There was an available command, now parse out the parameters*/
			#ifdef USE_ATCMD
			while((StringLength > 0) && ((LastParameter = StringParser(Input,',')) != NULL))
			#else
			while((StringLength > 0) && ((LastParameter = StringParser(Input)) != NULL))
			#endif
			{
			   /* There is an available parameter, now check to see if	   */
			   /* there is room in the UserCommand to store the parameter  */
			   if(Count < (sizeof(TempCommand->Parameters.Params)/sizeof(Parameter_t)))
			   {
				  /* Save the parameter as a string.					   */
				  TempCommand->Parameters.Params[Count].strParam = LastParameter;
				  //TempParam->Params[Count].strParam=LastParameter;//joe add
   
				  /* Save the parameter as an unsigned int intParam will   */
				  /* have a value of zero if an error has occurred. 	   */
				  TempCommand->Parameters.Params[Count].intParam = StringToUnsignedInteger(LastParameter);
				  //TempParam->Params[Count].intParam=StringToUnsignedInteger(LastParameter);//joe add
   
				  Count++;
				  Input 	   += BTPS_StringLength(LastParameter)+1;
				  StringLength -= BTPS_StringLength(LastParameter)+1;
   
				  ret_val = 0;
			   }
			   else
			   {
				  /* Be sure we exit out of the Loop.					   */
				  StringLength = 0;
   
				  ret_val	   = TO_MANY_PARAMS;
			   }
			}
   
			/* Set the number of parameters in the User Command to the	   */
			/* number of found parameters								   */
			TempCommand->Parameters.NumberofParameters = Count;
			//TempParam->NumberofParameters=Count;//joe add
		 }
		 else
		 {
			/* No command was specified 								   */
			ret_val = NO_COMMAND_ERROR;
		 }
	  }
	  else
	  {
		 /* One or more of the passed parameters appear to be invalid.	   */
		 ret_val = INVALID_PARAMETERS_ERROR;
	  }
   
	  return(ret_val);
   }
   
	  /* This function is responsible for determining the command in which */
	  /* the user entered and running the appropriate function associated  */
	  /* with that command.  The first parameter of this function is a	   */
	  /* structure containing information about the commmand to be issued. */
	  /* This information includes the command name and multiple parameters*/
	  /* which maybe be passed to the function to be executed.	Successful */
	  /* execution of this function is denoted by a return value of zero.  */
	  /* A negative return value implies that that command was not found   */
	  /* and is invalid.												   */
   static int CommandInterpreter(UserCommand_t *TempCommand)
   {
	  int				i;
	  int				ret_val;
	  CommandFunction_t CommandFunction;
   
	  /* If the command is not found in the table return with an invaild   */
	  /* command error													   */
	  ret_val = INVALID_COMMAND_ERROR;
   
	  /* Let's make sure that the data passed to us appears semi-valid.    */
	  if((TempCommand) && (TempCommand->Command))
	  {
		 /* Now, let's make the Command string all upper case so that we   */
		 /* compare against it. 										   */
		 for(i=0;i<BTPS_StringLength(TempCommand->Command);i++)
		 {
			if((TempCommand->Command[i] >= 'a') && (TempCommand->Command[i] <= 'z'))
			   TempCommand->Command[i] -= ('a' - 'A');
		 }
   
		 /* Check to see if the command which was entered was exit. 	   */
		 if(BTPS_MemCompare(TempCommand->Command, "QUIT", BTPS_StringLength("QUIT")) != 0)
		 {
			/* The command entered is not exit so search for command in    */
			/* table.													   */
			if((CommandFunction = FindCommand(TempCommand->Command)) != NULL)
			{
			   /* The command was found in the table so call the command.  */
			   ret_val = (*CommandFunction)(&TempCommand->Parameters);
			   if(!ret_val)
			   {
				  /* Return success to the caller.						   */
				  ret_val = 0;
			   }
			   else
			   {
				  if((ret_val != EXIT_CODE) && (ret_val != EXIT_MODE))
					 ret_val = FUNCTION_ERROR;
			   }
			}
		 }
		 else
		 {
			/* The command entered is exit, set return value to EXIT_CODE  */
			/* and return.												   */
			ret_val = EXIT_CODE;
		 }
	  }
	  else
		 ret_val = INVALID_PARAMETERS_ERROR;
   
	  return(ret_val);
   }
   
	  /* The following function is provided to allow a means to 		   */
	  /* programatically add Commands the Global (to this module) Command  */
	  /* Table.  The Command Table is simply a mapping of Command Name	   */
	  /* (NULL terminated ASCII string) to a command function.	This	   */
	  /* function returns zero if successful, or a non-zero value if the   */
	  /* command could not be added to the list.						   */
   static int AddCommand(char *CommandName, CommandFunction_t CommandFunction)
   {
	  int ret_val = 0;
   
	  /* First, make sure that the parameters passed to us appear to be    */
	  /* semi-valid.													   */
	  if((CommandName) && (CommandFunction))
	  {
		 /* Next, make sure that we still have room in the Command Table   */
		 /* to add commands.											   */
		 if(NumberCommands < MAX_SUPPORTED_COMMANDS)
		 {
			/* Simply add the command data to the command table and 	   */
			/* increment the number of supported commands.				   */
			CommandTable[NumberCommands].CommandName	   = CommandName;
			CommandTable[NumberCommands++].CommandFunction = CommandFunction;
   
			/* Return success to the caller.							   */
			ret_val 									   = 0;
		 }
		 else
			ret_val = 1;
	  }
	  else
		 ret_val = 1;
   
	  return(ret_val);
   }
   
	  /* The following function searches the Command Table for the		   */
	  /* specified Command.  If the Command is found, this function returns*/
	  /* a NON-NULL Command Function Pointer.  If the command is not found */
	  /* this function returns NULL.									   */
   static CommandFunction_t FindCommand(char *Command)
   {
	  unsigned int		Index;
	  CommandFunction_t ret_val;
   
	  /* First, make sure that the command specified is semi-valid. 	   */
	  if(Command)
	  {
		 /* Now loop through each element in the table to see if there is  */
		 /* a match.													   */
		 for(Index=0,ret_val=NULL;((Index<NumberCommands) && (!ret_val));Index++)
		 {
			if((BTPS_StringLength(CommandTable[Index].CommandName) == BTPS_StringLength(Command)) && (BTPS_MemCompare(Command, CommandTable[Index].CommandName, BTPS_StringLength(CommandTable[Index].CommandName)) == 0))
			   ret_val = CommandTable[Index].CommandFunction;
		 }
	  }
	  else
		 ret_val = NULL;
   
	  return(ret_val);
   }
   
	  /* The following function is provided to allow a means to clear out  */
	  /* all available commands from the command table. 				   */
   static void ClearCommands(void)
   {
	  /* Simply flag that there are no commands present in the table.	   */
	  NumberCommands = 0;
   }


			/* Displays the correct prompt depening on the Server/Client Mode.	 */
		 static void DisplayPrompt(void)
		 {
	
				  Display(("\r\n--> \b"));
				  //Display(("\0"));
			
		 }

	  /* The following function is responsible for parsing user input	   */
	  /* and call appropriate command function. 						   */
   static Boolean_t CommandLineInterpreter(char *Command)
   {
	  int			Result = !EXIT_CODE;
	  Boolean_t 	ret_val = FALSE;
	  UserCommand_t TempCommand;
   
	  /* The string input by the user contains a value, now run the string */
	  /* through the Command Parser.									   */
	  if(CommandParser(&TempCommand, Command) >= 0)
	  {
		 Display(("\r\n"));
   
		 /* The Command was successfully parsed run the Command.		   */
		 Result = CommandInterpreter(&TempCommand);
		 switch(Result)
		 {
			case INVALID_COMMAND_ERROR:
			   Display(("Invalid Command: %s.\r\n",TempCommand.Command));
			   break;
			case FUNCTION_ERROR:
			   Display(("Function Error.\r\n"));
			   break;
			case EXIT_CODE:
			  
			   //if(ServerPortID)
				  //CloseServer(NULL);
			   //else
			   //{
				  //if(SerialPortID)
					 //CloseRemoteServer(NULL);
			   //}
   
			   /* Restart the User Interface Selection. 				   */
			 //  UI_Mode = UI_MODE_SELECT;
   
			   /* Set up the Selection Interface.						   */
			//   UserInterface_Selection();
			   break;
		 }
   
		 /* Display a prompt.											   */
		 DisplayPrompt();
   
		 ret_val = TRUE;
	  }
	  else
	  {
		 /* Display a prompt.											   */
		 DisplayPrompt();
   
		 Display(("\r\nInvalid Command.\r\n"));
	  }
   
	  return(ret_val);
   }


		 /* The following function is used to process a command line string.  */
		 /* This function takes as it's only parameter the command line string*/
		 /* to be parsed and returns TRUE if a command was parsed and executed*/
		 /* or FALSE otherwise. 											  */
	  Boolean_t ProcessCommandLine(char *String)
	  {
		 return(CommandLineInterpreter(String));
	  }


   /* The following Toggles an LED at a passed in blink rate.           */
static void *ToggleLED(void *UserParameter)
{
   int BlinkRate = (int)UserParameter;

   while(1)
   {
     // HAL_LedToggle(0);

      BTPS_Delay(BlinkRate);
   }
}

   /* The following function is registered with the application so that */
   /* it can display strings to the debug UART.                         */
static void DisplayCallback(char Character)
{
 //  while(!HAL_ConsoleWrite(1, &Character))
 //    ;
}


   /* The following function is called by the OS when a task has been   */
   /* deleted.                                                          */
//void ApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName)
//{
 //  Display(("Task %s Overflow\r\n", pcTaskName));
//}

   /* The following function is called by the OS when an error has been */
   /* detected.                                                         */
//static void ApplicationErrorHook(xTaskHandle xCurrentTask, signed portCHAR *pcErrorString, portBASE_TYPE ErrorCode)
//{
//   while(1)
//      ;
//}

   /* The following is the function that is registered with SafeRTOS to */
   /* be called back when a Task is deleted.                            */
//static void ApplicationTaskDeleteHook(xTaskHandle DeletedTask)
//{
   //
   /* Do Nothing.                                                       */
//}

   /* The following function exists to put the MCU to sleep when        */
   /* in the idle task.                                                 */
//static void ApplicationIdleHook(void)
//{
   /* Do Nothing.                                                       */
//}

static int InitializeVariable(void)
{
	Callback_Event_Data = (Callback_Event_Data_t*)BTPS_AllocateMemory(sizeof(Callback_Event_Data_t));
	//TempParam=(ParameterList_t*)BTPS_AllocateMemory(sizeof(ParameterList_t));
        return 0;
}

int FreeInitializeVariable(void)
{
	if(Callback_Event_Data != NULL)
		BTPS_FreeMemory(Callback_Event_Data);
	return 0;
}

/* kevin add for get spp port */

  /* The following function is responsible for actually displaying an  */
   /* individual SDP Data Element to the Display.  The Level Parameter  */
   /* is used in conjunction with the defined INDENT_LENGTH constant to */
   /* make readability easier when displaying Data Element Sequences    */
   /* and Data Element Alternatives.  This function will recursively    */
   /* call itself to display the contents of Data Element Sequences and */
   /* Data Element Alternatives when it finds these Data Types (and     */
   /* increments the Indent Level accordingly).                         */
static void DisplayDataElement(SDP_Data_Element_t *SDPDataElement, unsigned int Level)
{
   unsigned int Index;
   char         Buffer[256];

   switch(SDPDataElement->SDP_Data_Element_Type)
   {
      case deNIL:
         /* Display the NIL Type.                                       */
         Display(("%*s Type: NIL\r\n", (Level*INDENT_LENGTH), ""));
         break;
      case deNULL:
         /* Display the NULL Type.                                      */
         Display(("%*s Type: NULL\r\n", (Level*INDENT_LENGTH), ""));
         break;
      case deUnsignedInteger1Byte:
         /* Display the Unsigned Integer (1 Byte) Type.                 */
         Display(("%*s Type: Unsigned Int = 0x%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UnsignedInteger1Byte));
         break;
      case deUnsignedInteger2Bytes:
         /* Display the Unsigned Integer (2 Bytes) Type.                */
         Display(("%*s Type: Unsigned Int = 0x%04X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UnsignedInteger2Bytes));
         break;
      case deUnsignedInteger4Bytes:
         /* Display the Unsigned Integer (4 Bytes) Type.                */
         Display(("%*s Type: Unsigned Int = 0x%08X\r\n", (Level*INDENT_LENGTH), "", (unsigned int)SDPDataElement->SDP_Data_Element.UnsignedInteger4Bytes));
         break;
      case deUnsignedInteger8Bytes:
         /* Display the Unsigned Integer (8 Bytes) Type.                */
         Display(("%*s Type: Unsigned Int = 0x%02X%02X%02X%02X%02X%02X%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[7],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[6],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[5],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[4],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[3],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[2],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[1],
                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger8Bytes[0]));
         break;
      case deUnsignedInteger16Bytes:
         /* Display the Unsigned Integer (16 Bytes) Type.               */
         Display(("%*s Type: Unsigned Int = 0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[15],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[14],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[13],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[12],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[11],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[10],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[9],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[8],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[7],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[6],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[5],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[4],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[3],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[2],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[1],
                                                                                                                                                SDPDataElement->SDP_Data_Element.UnsignedInteger16Bytes[0]));
         break;
      case deSignedInteger1Byte:
         /* Display the Signed Integer (1 Byte) Type.                   */
         Display(("%*s Type: Signed Int = 0x%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.SignedInteger1Byte));
         break;
      case deSignedInteger2Bytes:
         /* Display the Signed Integer (2 Bytes) Type.                  */
         Display(("%*s Type: Signed Int = 0x%04X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.SignedInteger2Bytes));
         break;
      case deSignedInteger4Bytes:
         /* Display the Signed Integer (4 Bytes) Type.                  */
         Display(("%*s Type: Signed Int = 0x%08X\r\n", (Level*INDENT_LENGTH), "", (unsigned int)SDPDataElement->SDP_Data_Element.SignedInteger4Bytes));
         break;
      case deSignedInteger8Bytes:
         /* Display the Signed Integer (8 Bytes) Type.                  */
         Display(("%*s Type: Signed Int = 0x%02X%02X%02X%02X%02X%02X%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[7],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[6],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[5],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[4],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[3],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[2],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[1],
                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger8Bytes[0]));
         break;
      case deSignedInteger16Bytes:
         /* Display the Signed Integer (16 Bytes) Type.                 */
         Display(("%*s Type: Signed Int = 0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[15],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[14],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[13],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[12],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[11],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[10],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[9],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[8],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[7],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[6],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[5],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[4],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[3],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[2],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[1],
                                                                                                                                              SDPDataElement->SDP_Data_Element.SignedInteger16Bytes[0]));
         break;
      case deTextString:
         /* First retrieve the Length of the Text String so that we can */
         /* copy the Data into our Buffer.                              */
         Index = (SDPDataElement->SDP_Data_Element_Length < sizeof(Buffer))?SDPDataElement->SDP_Data_Element_Length:(sizeof(Buffer)-1);

         /* Copy the Text String into the Buffer and then NULL terminate*/
         /* it.                                                         */
         BTPS_MemCopy(Buffer, SDPDataElement->SDP_Data_Element.TextString, Index);
         Buffer[Index] = '\0';

         Display(("%*s Type: Text String = %s\r\n", (Level*INDENT_LENGTH), "", Buffer));
         break;
      case deBoolean:
         Display(("%*s Type: Boolean = %s\r\n", (Level*INDENT_LENGTH), "", (SDPDataElement->SDP_Data_Element.Boolean)?"TRUE":"FALSE"));
         break;
      case deURL:
         /* First retrieve the Length of the URL String so that we can  */
         /* copy the Data into our Buffer.                              */
         Index = (SDPDataElement->SDP_Data_Element_Length < sizeof(Buffer))?SDPDataElement->SDP_Data_Element_Length:(sizeof(Buffer)-1);

         /* Copy the URL String into the Buffer and then NULL terminate */
         /* it.                                                         */
         BTPS_MemCopy(Buffer, SDPDataElement->SDP_Data_Element.URL, Index);
         Buffer[Index] = '\0';

         Display(("%*s Type: URL = %s\r\n", (Level*INDENT_LENGTH), "", Buffer));
         break;
      case deUUID_16:
         Display(("%*s Type: UUID_16 = 0x%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UUID_16.UUID_Byte0,
                                                                                   SDPDataElement->SDP_Data_Element.UUID_16.UUID_Byte1));
         break;
      case deUUID_32:
         Display(("%*s Type: UUID_32 = 0x%02X%02X%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UUID_32.UUID_Byte0,
                                                                                           SDPDataElement->SDP_Data_Element.UUID_32.UUID_Byte1,
                                                                                           SDPDataElement->SDP_Data_Element.UUID_32.UUID_Byte2,
                                                                                           SDPDataElement->SDP_Data_Element.UUID_32.UUID_Byte3));
         break;
      case deUUID_128:
         Display(("%*s Type: UUID_128 = 0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n", (Level*INDENT_LENGTH), "", SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte0,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte1,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte2,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte3,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte4,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte5,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte6,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte7,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte8,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte9,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte10,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte11,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte12,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte13,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte14,
                                                                                                                                            SDPDataElement->SDP_Data_Element.UUID_128.UUID_Byte15));
         break;
      case deSequence:
         /* Display that this is a SDP Data Element Sequence.           */
         Display(("%*s Type: Data Element Sequence\r\n", (Level*INDENT_LENGTH), ""));

         /* Loop through each of the SDP Data Elements in the SDP Data  */
         /* Element Sequence.                                           */
         for(Index = 0; Index < SDPDataElement->SDP_Data_Element_Length; Index++)
         {
            /* Call this function again for each of the SDP Data        */
            /* Elements in this SDP Data Element Sequence.              */
            DisplayDataElement(&(SDPDataElement->SDP_Data_Element.SDP_Data_Element_Sequence[Index]), (Level + 1));
         }
         break;
      case deAlternative:
         /* Display that this is a SDP Data Element Alternative.        */
         Display(("%*s Type: Data Element Alternative\r\n", (Level*INDENT_LENGTH), ""));

         /* Loop through each of the SDP Data Elements in the SDP Data  */
         /* Element Alternative.                                        */
         for(Index = 0; Index < SDPDataElement->SDP_Data_Element_Length; Index++)
         {
            /* Call this function again for each of the SDP Data        */
            /* Elements in this SDP Data Element Alternative.           */
            DisplayDataElement(&(SDPDataElement->SDP_Data_Element.SDP_Data_Element_Alternative[Index]), (Level + 1));
         }
         break;
      default:
         Display(("%*s Unknown SDP Data Element Type\r\n", (Level*INDENT_LENGTH), ""));
         break;
   }
}


   /* The following function is responsible for Displaying the contents */
   /* of an SDP Service Attribute Response to the display.              */
static void DisplaySDPAttributeResponse(SDP_Service_Attribute_Response_Data_t *SDPServiceAttributeResponse, unsigned int InitLevel)
{
   int Index;

   /* First, check to make sure that there were Attributes returned.    */
   if(SDPServiceAttributeResponse->Number_Attribute_Values)
   {
      /* Loop through all returned SDP Attribute Values.                */
      for(Index = 0; Index < SDPServiceAttributeResponse->Number_Attribute_Values; Index++)
      {
         /* First Print the Attribute ID that was returned.             */
         Display(("%*s Attribute ID 0x%04X\r\n", (InitLevel*INDENT_LENGTH), "", SDPServiceAttributeResponse->SDP_Service_Attribute_Value_Data[Index].Attribute_ID));

         /* Now Print out all of the SDP Data Elements that were        */
         /* returned that are associated with the SDP Attribute.        */
         DisplayDataElement(SDPServiceAttributeResponse->SDP_Service_Attribute_Value_Data[Index].SDP_Data_Element, (InitLevel + 1));
      }
   }
   else
      Display(("No SDP Attributes Found.\r\n"));
}


   /* The following function is responsible for displaying the contents */
   /* of an SDP Service Search Attribute Response to the display.       */
static void DisplaySDPSearchAttributeResponse(SDP_Service_Search_Attribute_Response_Data_t *SDPServiceSearchAttributeResponse)
{
   int Index;

   /* First, check to see if Service Records were returned.             */
   if(SDPServiceSearchAttributeResponse->Number_Service_Records)
   {
      /* Loop through all returned SDP Service Records.                 */
      for(Index = 0; Index < SDPServiceSearchAttributeResponse->Number_Service_Records; Index++)
      {
         /* First display the number of SDP Service Records we are      */
         /* currently processing.                                       */
         Display(("Service Record: %u:\r\n", (Index + 1)));

         /* Call Display SDPAttributeResponse for all SDP Service       */
         /* Records received.                                           */
         DisplaySDPAttributeResponse(&(SDPServiceSearchAttributeResponse->SDP_Service_Attribute_Response_Data[Index]), 1);
      }
   }
   else
   {
      Display(("No SDP Service Records Found.\r\n"));
   }
}

extern tBtCtrlOpt *getBtOptPtr(void);

void SspPairResponseTask(void *None)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(ptBtOpt->SSP_Capability_Type == 1)
	{
		if(ptBtOpt->SSP_TimerCount++ >= 3000)
		{
			ptBtOpt->SSP_Capability_Type = 0;
		}
	}
	else if(ptBtOpt->SSP_Capability_Type == 2)
	{
		if(ptBtOpt->SSP_TimerCount++ >= 12000)
		{
			ptBtOpt->SSP_Capability_Type = 0;
		}
	}
	else
	{
		ptBtOpt->SSP_TimerCount = 0;
	}
}


void SendTask(void *None)
{
	int iLen, iRet;
	unsigned char ispp_data[116];
	unsigned char spp_data[384];
	unsigned char spple_data[100];
	int iSendLen;
	static int iISPPSend = 0;
	static int iSPPSend = 0;
	static int iSPPLESend = 0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	// 主连接只有SPP一种
	if(ptBtOpt->masterConnected != 0
		&& !QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue)))
	{
		switch(ptBtOpt->eConnectType)
		{
			case eConnected_SPP:
				iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, sizeof(spp_data));
				Display(("QueGetsWithoutDelete = %d\r\n", iLen));
				iRet = SPPDataWrite(spp_data, iLen, ptBtOpt->masterSerialPortID);
				if(iRet > 0)
				{
					Display(("SPPDataWrite(masterSerialPortID:%d) Seccuss <cnt:%d><len:%d>!!\r\n", 
						ptBtOpt->masterSerialPortID, iSPPSend++, iRet));
					QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, iRet);
				}
				else
				{
					Display(("SPPDataWrite(masterSerialPortID:%d) <Err = %d>\r\n", ptBtOpt->masterSerialPortID, iRet));
				}
				break;

			case eConnected_ISPP:
				Display(("eConnected_ISPP send data\r\n"));
				if(ptBtOpt->ISPP_Session_state == eISPP_Session_Open)
				{
					iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), ispp_data, sizeof(ispp_data));
					iRet = ISPPDataWrite(ispp_data, iLen); /* 一次116字节 */
					if(iRet > 0)
					{
						Display(("ISPP Send Data Seccuss <cnt:%d><len:%d>!!\r\n", iISPPSend++, iLen));
						QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), ispp_data, iLen);
					}
					else
					{
						Display(("ISPPDataWrite <Err = %d>\r\n", iRet));
					}
				}
				break;
		}		
	}
	// 从连接可能是SPP、ISPP、SPPLE三种的一种
	if(ptBtOpt->slaveConnected != 0 
		&& !QueIsEmpty((T_Queue *)(&ptBtOpt->tSlaveSendQue)))
	{
		switch(ptBtOpt->eConnectType)
		{
			case eConnected_SPP:
				iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tSlaveSendQue), spp_data, sizeof(spp_data));
				Display(("QueGetsWithoutDelete = %d\r\n", iLen));
				iRet = SPPDataWrite(spp_data, iLen, ptBtOpt->slaveSerialPortID);
				if(iRet > 0)
				{
					Display(("SPPDataWrite((slaveSerialPortID:%d)) Seccuss <cnt:%d><len:%d>!!\r\n", 
						ptBtOpt->slaveSerialPortID, iSPPSend++, iRet));
					QueGets((T_Queue *)(&ptBtOpt->tSlaveSendQue), spp_data, iRet);
				}
				else
				{
					Display(("SPPDataWrite(slaveSerialPortID:%d) <Err = %d>\r\n", ptBtOpt->slaveSerialPortID, iRet));
				}			
				break;
			case eConnected_SPPLE:
				iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tSlaveSendQue), spple_data, sizeof(spple_data));
				//Display(("QueGetsWithoutDelete = %d\r\n", iLen));
				iRet = SPPLEDataWrite(iLen, spple_data, ptBtOpt->SPPLE_Addr);
				//Display(("SPPLEDataWrite <iRet = %d>\r\n", iRet));
				if(iRet == 0)
				{
					Display(("SPPLE Send Data Seccuss <cnt:%d><len:%d>!!\r\n", iSPPLESend++, iLen));
					QueGets((T_Queue *)(&ptBtOpt->tSlaveSendQue), spple_data, iLen);
				}
				else
				{
					Display(("SPPLEDataWrite <Err = %d>\r\n", iRet));
				}
				break;
			case eConnected_ISPP:
				if(ptBtOpt->ISPP_Session_state == eISPP_Session_Open)
				{
					iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tSlaveSendQue), ispp_data, sizeof(ispp_data));
					//Display(("QueGetsWithoutDelete = %d\r\n", iLen));
					iRet = ISPPDataWrite(ispp_data, iLen); /* 一次116字节 */
					//Display(("ISPPDataWrite <iRet = %d>\r\n", iRet));
					if(iRet > 0)
					{
						Display(("ISPP Send Data Seccuss <cnt:%d><len:%d>!!\r\n", iISPPSend++, iLen));
						QueGets((T_Queue *)(&ptBtOpt->tSlaveSendQue), ispp_data, iLen);
					}
					else
					{
						Display(("ISPPDataWrite <Err = %d>\r\n", iRet));
					}
				}
				break;
			default:
				Display(("<ERROR:%d> SendTask eConnectType Err(%d)\r\n", 	__LINE__, ptBtOpt->eConnectType));
				break;			
		}
	}
}
extern unsigned int gDebugScanTimeT1, gDebugScanTimeT2;
void MainTask(void *UserParameter)
{
	unsigned char spp_data[512]; /* max = 328 */
	int i, iTmp, iRet;
	int Num_Dev;
	int Index=0;  
	char BoardStr[16];
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	BoardStr_t 									Remote_BoardStr;
	CL_Inquiry_Event_Data_t         			*CL_Inquiry_Event_Data;
	CL_Remote_Name_Event_Data_t 				*CL_Remote_Name_Event_Data;
	CL_Remote_Device_Event_Data_t 				*CL_Remote_Device_Event_Data;
	CL_Inquiry_Name_Event_Data_t				*CL_Inquiry_Name_Event_Data;
	CL_Pin_Code_Request_Event_Data_t			*CL_Pin_Code_Request_Event_Data;
	GAP_Authentication_Event_Data_t				*CL_GAP_Authentication_Event_Data;
	GAP_Authentication_Event_Data_t             *CL_Authentication_Event_Data;

	GAP_BLE_Connection_Complete_Event_Data_t	*GAP_BLE_Connection_Complete_Event_Data;
	GAP_BLE_Disconnection_Complete_Event_Data_t	*GAP_BLE_Disconnection_Complete_Event_Data;
	GAP_BLE_Advertising_Report_Event_Data_t		*GAP_BLE_Advertising_Report_Event_Data;
	GAP_BLE_Advertising_Report_Data_t			*DeviceEntryPtr;
	GAPS_Support_Service_Information_t			*GAPS_Support_Service_Information;

	GATT_BLE_Device_Connection_Data_t			*GATT_BLE_Device_Connection_Data;
	GATT_BLE_Device_Disconnection_Data_t		*GATT_BLE_Device_Disconnection_Data;
	GATT_BLE_Service_Discovery_Complete_Data_t	*GATT_BLE_Service_Discovery_Complete_Data;

	SDP_Response_Data_t 						*SDP_Response_Data;

	SPP_Port_Status_Indication_Data_t			*SPP_Port_Status_Indication_Data;
	SPP_Connect_Indication_Event_Data_t			*SPP_Connect_Indication_Event_Data;
	SPP_Data_Indication_Event_Data_t			*SPP_Data_Indication_Event_Data;
	SPP_Disconnect_Indication_Event_Data_t		*SPP_Disconnect_Indication_Event_Data;
	//kevin add for ISPP
	SPP_Open_Port_Indication_Data_t                 *ISPP_Open_Port_Indication_Data;
    SPP_Port_Status_Indication_Data_t               *ISPP_Port_Status_Indication_Data;
    SPP_Data_Indication_Data_t                      *ISPP_Data_Indication_Data;
	
	ISPP_Session_Open_Indication_Data_t  		*ISPP_Session_Open_Indication_Data;
	ISPP_Session_Data_Indication_Data_t 		*ISPP_Session_Data_Indication_Data;
	ISPP_Session_Close_Indication_Data_t  		*ISPP_Session_Close_Indication_Data;
	ISPP_Process_Status_Data_t					*ISPP_Process_Status;
	SPP_Close_Port_Indication_Data_t			*ISPP_Close_Port_Indication_Data;

	SPPLE_Data_Indication_Event_Data_t			*SPPLE_BLE_Data_Indication_Event_Data;
	SPPLE_Support_Service_Information_t			*SPPLE_Support_Service_Information;
        
	if(BTPS_QueryMailbox(ret_valmail))
	{
		if(BTPS_WaitMailbox(ret_valmail, (Callback_Event_Data_t *)Callback_Event_Data))
		{
			switch(Callback_Event_Data->CallbackEvent)
			{
				case CL_INQUIRY_RESULT:
				/* 在当前的查询过程中，此事件表明蓝牙设备或多个蓝牙设备的信息 */
					gDebugScanTimeT2 = GetTimerCount();
					Display(("<%d>****** ", __LINE__));
					Display(("CL_INQUIRY_RESULT <TimeTick:%d><TimerDone:%dmd>\r\n", gDebugScanTimeT2, gDebugScanTimeT2-gDebugScanTimeT1));
					//GAP_Inquiry_Event_Data1 = (GAP_Inquiry_Event_Data_t*)BTPS_AllocateMemory(sizeof(GAP_Inquiry_Event_Data_t));
					CL_Inquiry_Event_Data = ((CL_Inquiry_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Num_Dev = CL_Inquiry_Event_Data->Number_Devices;
					Display(("CL_Inquiry_Event_Data <Addr:%p>\r\n", CL_Inquiry_Event_Data));
					Display(("CL_Inquiry_Event_Data->Number_Devices: %d\r\n", CL_Inquiry_Event_Data->Number_Devices));
					DisplayATCMD(("+OK\r\nNum_Dev:%d\r\n",Num_Dev));
					for(i = 0; i < Num_Dev; i++)
					{
						InquiryResultList_cmd[i] = CL_Inquiry_Event_Data->CL_Inquiry_Data[i].BD_ADDR;
						BD_ADDRToStr(CL_Inquiry_Event_Data->CL_Inquiry_Data[i].BD_ADDR, Callback_BoardStr);
						Display(("Result: %d,%s.\r\n", (i+1), Callback_BoardStr));
						DisplayATCMD(("%d,%s\r\n", (i+1), Callback_BoardStr));
					}
					// TODO:
					if(Num_Dev == ptBtOpt->tInqResult.iCnt)
					{
						ptBtOpt->InquiryComplete = 1;
					}
					else
					{
						// FixMe
						ptBtOpt->InquiryComplete = -1;
						Display(("Warning<%d> InquiryResult(%d) != ptBtOpt->tInqResult.iCnt(%d)\r\n", __LINE__, Num_Dev, ptBtOpt->tInqResult.iCnt));
					}
					break;
				case CL_INQUIRY_NONAME_RESULT:
					/*  Inquiry过程中，如果搜索不到远端设备名称，会产生该事件，仅提供远端设备Mac地址 */
					Display(("CL_INQUIRY_NONAME_RESULT<TimeTick:%d>\r\n", GetTimerCount()));				
					CL_Remote_Device_Event_Data = ((CL_Remote_Device_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					{
						BD_ADDRToStr(CL_Remote_Device_Event_Data->Remote_Device, Callback_BoardStr);
						Display(("BD_ADDR: %s.\r\n", Callback_BoardStr));
						// TODO:
						if(ptBtOpt->InquiryComplete == 0) /* 只记录CL_INQUIRY_RESULT事件产生之前扫描到的设备 */
						{
							iTmp = ptBtOpt->tInqResult.iCnt;
							ptBtOpt->tInqResult.Device[iTmp].BD_ADDR = CL_Remote_Device_Event_Data->Remote_Device;
							Display(("ptBtOpt->tInqResult.iCnt = %d\r\n", ptBtOpt->tInqResult.iCnt));
							Display(("<%d> ", iTmp));
							s_DisplayWltAddr("ptBtOpt->tInqResult.Device", &ptBtOpt->tInqResult.Device[iTmp].BD_ADDR);
							ptBtOpt->tInqResult.iCnt++;
						}
						else
						{
							// FixMe
							Display(("Warning<%d> CL_INQUIRY_NONAME_RESULT after CL_INQUIRY_RESULT\r\n", __LINE__));
						}
					}
					break;
				case CL_INQUIRY_NAME_RESULT:
					/* Inquiry过程中，如果搜索到远端设备名称，会产生该事件，提供远端设备Mac地址和设备名称 */
					  Display(("CL_INQUIRY_NAME_RESULT<TimeTick:%d>\r\n", GetTimerCount()));				  
								  CL_Inquiry_Name_Event_Data = ((CL_Inquiry_Name_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					  {
						BD_ADDRToStr(CL_Inquiry_Name_Event_Data->Remote_Device, Callback_BoardStr);
						Display(("BD_ADDR: %s.\r\n", Callback_BoardStr));
						Display(("Name:%s.\r\n\r\n", CL_Inquiry_Name_Event_Data->Remote_Name));
						// TODO:
						if(ptBtOpt->InquiryComplete == 0) /* 只记录CL_INQUIRY_RESULT事件产生之前扫描到的设备 */
						{
							iTmp = ptBtOpt->tInqResult.iCnt;
							ptBtOpt->tInqResult.Device[iTmp].BD_ADDR = CL_Inquiry_Name_Event_Data->Remote_Device;
							if(strlen(CL_Inquiry_Name_Event_Data->Remote_Name) < BT_NAME_MAX_LEN)
							{
								strcpy(ptBtOpt->tInqResult.Device[iTmp].DeviceName, CL_Inquiry_Name_Event_Data->Remote_Name);
							}
							else
							{
								for(i = 0; i < BT_NAME_MAX_LEN-1; i++)
								{
									ptBtOpt->tInqResult.Device[iTmp].DeviceName[i] = CL_Inquiry_Name_Event_Data->Remote_Name[i];
								}
							}
							Display(("ptBtOpt->tInqResult.iCnt = %d\r\n", ptBtOpt->tInqResult.iCnt));
							Display(("<%d> ", iTmp));
							s_DisplayWltAddr("ptBtOpt->tInqResult.Device", &ptBtOpt->tInqResult.Device[iTmp].BD_ADDR);	
							ptBtOpt->tInqResult.iCnt++;
						}
						else
						{
							// FixMe
							Display(("Warning<%d> CL_INQUIRY_NAME_RESULT after CL_INQUIRY_RESULT\r\n", __LINE__));
						}
					 }
					BTPS_FreeMemory(CL_Inquiry_Name_Event_Data->Remote_Name);
					break;
				case CL_REMOTE_NAME_COMPLETE:
				/* 此事件表示远程设备名称请求操作已经完成 */
					Display(("<%d>****** ", __LINE__));
					Display(("CL_REMOTE_NAME_COMPLETE\r\n"));
					CL_Remote_Name_Event_Data = ((CL_Remote_Name_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("CL_Remote_Name_Event_Data <Addr:%p>\r\n", CL_Remote_Name_Event_Data));
					if(CL_Remote_Name_Event_Data->Remote_Name_Status == 0)
					{
						BD_ADDRToStr(CL_Remote_Name_Event_Data->Remote_Device, Callback_BoardStr);
						Display(("BD_ADDR: %s.\r\n", Callback_BoardStr));
		                Display(("Name: %s.\r\n\r\n", CL_Remote_Name_Event_Data->Remote_Name));
						DisplayATCMD(("+OK=%s\r\n",CL_Remote_Name_Event_Data->Remote_Name));
						
						for(i = 0; i < ptBtOpt->tInqResult.iCnt; i++)
						{
							if(!memcmp((unsigned char *)&ptBtOpt->tInqResult.Device[i].BD_ADDR, (unsigned char *)&CL_Remote_Name_Event_Data->Remote_Device, sizeof(BD_ADDR_t)))
							{
								memcpy(ptBtOpt->tInqResult.Device[i].DeviceName, CL_Remote_Name_Event_Data->Remote_Name, 64);
								ptBtOpt->GetRemoteNameComplete = 1;
								ptBtOpt->InquiryGetRemoteNameComplete = 1;
							}
						}
					}
					else
					{
						ptBtOpt->GetRemoteNameComplete = -1;
						ptBtOpt->InquiryGetRemoteNameComplete = -1;
					}
					break;
				case SPP_REMOTE_PORT:
					Display(("SPP_REMOTE_PORT\r\n"));
					Index = 0;
					if(RFCOMMPortList[Index])
					{
						while(RFCOMMPortList[Index])
						{
							Display(("RFCOMM Server Port Found at: %d \r\n", RFCOMMPortList[Index] ));
							Index++;
						}
						// TODO:
						ptBtOpt->ServiceDiscoveryComplete = 1;
						ptBtOpt->RemoteSeverPort = RFCOMMPortList[0];
					}
					else
					{
						Display(("No RFCOMM Server found"));
						ptBtOpt->ServiceDiscoveryComplete = -1;
					}
					break;
				case REMOTE_DEVICE_TYPE:
					Display(("REMOTE_DEVICE_TYPE \r\n"));
					if(Callback_Event_Data->CallbackParameter != NULL)
					{
						if((*((unsigned char*)Callback_Event_Data->CallbackParameter)) == 0x00)
						{
						    Display(("This is Non-IOS device.\r\n")); 
						    ptBtOpt->RemoteServerType = 0;				// 非ios设备
						}
						else
						{
						    Display(("This is IOS device. \r\n"));
						    ptBtOpt->RemoteServerType = 1;				// ios设备
						}
					}
					else
					{
					  	Display(("Get Error, Please Check remote device \r\n"));
					}
					break;
				case CL_PIN_CODE_RREQUEST:
				/* */
					Display(("<%d>****** ", __LINE__));
					Display(("CL_PIN_CODE_RREQUEST\r\n"));
					CL_Pin_Code_Request_Event_Data = ((CL_Pin_Code_Request_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("CL_Pin_Code_Request_Event_Data <Addr:%p>\r\n", CL_Pin_Code_Request_Event_Data));
					BD_ADDRToStr((CL_Pin_Code_Request_Event_Data->Remote_Device), Callback_BoardStr);
					Display(("Remote BD_ADDR: %s.\r\n", Callback_BoardStr));
					ConnectionPINCodeResponse(CL_Pin_Code_Request_Event_Data->Remote_Device, ptBtOpt->LocalPin);
					Display(("Auto Response PIN Code \"%s\"\r\n", ptBtOpt->LocalPin));
					CurrentCBRemoteBD_ADDR = CL_Pin_Code_Request_Event_Data->Remote_Device;
					s_WltSetAddrToAddr(&ptBtOpt->LastPair_BD_ADDR, &CurrentCBRemoteBD_ADDR);
					break;
				case USER_CONFIRMATION_REQUEST:
					Display(("<%d>****** ", __LINE__));
					Display(("USER_CONFIRMATION_REQUEST\r\n"));
					CL_GAP_Authentication_Event_Data = ((GAP_Authentication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					BD_ADDRToStr((CL_GAP_Authentication_Event_Data->Remote_Device), Callback_BoardStr);
					Display(("Remote BD_ADDR: %s.\r\n", Callback_BoardStr));
				    Display(("Remote Auth Number: %u\r\n", CL_GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value));
					Display(("---Respond with: UserConfirmationResponse---\r\n"));
					/* Response Yes or No? 1:Yes,0:No */
					// TODO:
					ptBtOpt->SSP_Capability_Type = 1;
					ptBtOpt->SSP_Key_Response = CL_GAP_Authentication_Event_Data->Authentication_Event_Data.Numeric_Value;
					ptBtOpt->SSP_PairRemote_BD_ADDR = CL_GAP_Authentication_Event_Data->Remote_Device;
					ptBtOpt->SSP_TimerCount = 0;
					#if 1
					iRet = UserConfirmationResponse((CL_GAP_Authentication_Event_Data->Remote_Device),1);
					if(iRet != 0)
					{
						Display(("<Err:%d> UserConfirmationResponse <StackErr:%d>\r\n",
							__LINE__, iRet));
					}
					#endif
					break;
				case PASSKEY_REQUEST:
					Display(("<%d>****** ", __LINE__));
					Display(("PASSKEY_REQUEST\r\n"));
					CL_GAP_Authentication_Event_Data = ((GAP_Authentication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					BD_ADDRToStr((CL_GAP_Authentication_Event_Data->Remote_Device), Callback_BoardStr);
					CurrentCBRemoteBD_ADDR=CL_GAP_Authentication_Event_Data->Remote_Device;
					Display(("Remote BD_ADDR: %s.\r\n", Callback_BoardStr));
				    Display(("---Respond with: PassKeyResponse---\r\n"));

					ptBtOpt->SSP_Capability_Type = 2;
					ptBtOpt->SSP_PairRemote_BD_ADDR = CL_GAP_Authentication_Event_Data->Remote_Device;
					ptBtOpt->SSP_TimerCount = 0;
					#if 0
					iRet = PassKeyResponse(CL_GAP_Authentication_Event_Data->Remote_Device, 123456);
					if(iRet != 0)
					{
						Display(("<Err:%d> PassKeyResponse <StackErr:%d>\r\n",
							__LINE__, iRet));
					}
					#endif
					break;			
				case SECURE_SIMPLE_PAIRING_COMPLETE:
					Display(("SECURE_SIMPLE_PAIRING_COMPLETE\r\n"));
					CL_GAP_Authentication_Event_Data = ((GAP_Authentication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					BD_ADDRToStr((CL_GAP_Authentication_Event_Data->Remote_Device), Callback_BoardStr);
					Display(("Remote BD_ADDR: %s.\r\n", Callback_BoardStr));
					if((CL_GAP_Authentication_Event_Data->Authentication_Event_Data.Secure_Simple_Pairing_Status) == 0)
					{
						  Display(("SECURE_SIMPLE_PAIRING_COMPLETE Authentication Success\r\n"));
					}
					else
					{
						  Display(("SECURE_SIMPLE_PAIRING_COMPLETE Authentication Failure\r\n"));			
					}
					break;
				case CL_AUTHENTICATION_STATUS:
					/* 配对结果事件，如果成功这里会提示成功，如果配对超时或者对方pincode错误，都会提示失败的 */
					Display(("<%d>****** ", __LINE__));
				 	Display(("CL_AUTHENTICATION_STATUS\r\n"));
					CL_Authentication_Event_Data = ((GAP_Authentication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("CL_Authentication_Event_Data <Addr:%p>\r\n", CL_Authentication_Event_Data));
					BD_ADDRToStr((CL_Authentication_Event_Data->Remote_Device), Callback_BoardStr);
					Display(("Remote BD_ADDR: %s.\r\n", Callback_BoardStr));
					   
					   /* Check to see if we were successful.  If not, then  */
					   /* any saved Link Key is now invalid and we need to	 */
					   /* delete any Link Key that is associated with the	 */
					   /* remote device.									 */
					   if(CL_Authentication_Event_Data->Authentication_Event_Data.Authentication_Status)
					   {
					   		ptBtOpt->PairComplete = -1;
						  Display(("Authentication Failure\r\n"));
					   }
					   else
					   {
					   		ptBtOpt->PairComplete = 1;
						   Display(("GAP_Authentication_Event_Type = %d\r\n", CL_Authentication_Event_Data->GAP_Authentication_Event_Type));
						  Display(("Authentication Success\r\n"));
					   }	
					break;
				case GAP_BLE_ADVERTISING_REPORT:
					/* 在扫描过程中，一个广播报告或是扫描响应报告被接收时会调度此事件 */
					Display(("<%d>****** ", __LINE__));
					Display(("GAP_BLE_ADVERTISING_REPORT\r\n"));
					GAP_BLE_Advertising_Report_Event_Data = ((GAP_BLE_Advertising_Report_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("GAP_BLE_Advertising_Report_Event_Data <Addr:%p>\r\n", GAP_BLE_Advertising_Report_Event_Data));
					for(i = 0; i < GAP_BLE_Advertising_Report_Event_Data->Number_Device_Entries; i++)
		            {
		               DeviceEntryPtr = &(GAP_BLE_Advertising_Report_Event_Data->Advertising_Data[i]);

		               /* Display the packet type for the device                */
		               switch(DeviceEntryPtr->Advertising_Report_Type)
		               {
		                  case wrtConnectableUndirected:
		                     Display(("  Advertising Type: %s.\r\n", "wrtConnectableUndirected"));
		                     break;
		                  case wrtConnectableDirected:
		                     Display(("  Advertising Type: %s.\r\n", "wrtConnectableDirected"));
		                     break;
		                  case wrtScannableUndirected:
		                     Display(("  Advertising Type: %s.\r\n", "wrtScannableUndirected"));
		                     break;
		                  case wrtNonConnectableUndirected:
		                     Display(("  Advertising Type: %s.\r\n", "wrtNonConnectableUndirected"));
		                     break;
		                  case wrtScanResponse:
		                     Display(("  Advertising Type: %s.\r\n", "wrtScanResponse"));
		                     break;
		               }

		               /* Display the Address Type.                             */
		               if(DeviceEntryPtr->Address_Type == wlatPublic)
		               {
		                  Display(("  Address Type: %s.\r\n","watPublic"));
		               }
		               else
		               {
		                  Display(("  Address Type: %s.\r\n","watRandom"));
		               }

		               /* Display the Device Address.                           */
		               Display(("  Address: 0x%02X%02X%02X%02X%02X%02X.\r\n", DeviceEntryPtr->BD_ADDR.BD_ADDR5, DeviceEntryPtr->BD_ADDR.BD_ADDR4, DeviceEntryPtr->BD_ADDR.BD_ADDR3, DeviceEntryPtr->BD_ADDR.BD_ADDR2, DeviceEntryPtr->BD_ADDR.BD_ADDR1, DeviceEntryPtr->BD_ADDR.BD_ADDR0));
		               Display(("  RSSI: 0x%02X.\r\n", DeviceEntryPtr->RSSI));
		               Display(("  Data Length: %d.\r\n", DeviceEntryPtr->Raw_Report_Length));

					   DisplayATCMD(("0x%02X%02X%02X%02X%02X%02X,0x%02X\r\n",DeviceEntryPtr->BD_ADDR.BD_ADDR5, DeviceEntryPtr->BD_ADDR.BD_ADDR4, DeviceEntryPtr->BD_ADDR.BD_ADDR3, DeviceEntryPtr->BD_ADDR.BD_ADDR2, DeviceEntryPtr->BD_ADDR.BD_ADDR1, DeviceEntryPtr->BD_ADDR.BD_ADDR0, DeviceEntryPtr->RSSI));
		               //DisplayAdvertisingData(&(DeviceEntryPtr->Advertising_Data));
		            }
					break;
				case GAP_BLE_CONNECTION_COMPLETE:
					/* 此事件被调度时，远程设备连接到本地设备 */
					Display(("<%d>****** ", __LINE__));
					Display(("GAP_BLE_CONNECTION_COMPLETE\r\n"));
					GAP_BLE_Connection_Complete_Event_Data = ((GAP_BLE_Connection_Complete_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("GAP_BLE_Connection_Complete_Event_Data <Addr:%p>\r\n", 
						GAP_BLE_Connection_Complete_Event_Data));

					BD_ADDRToStr(GAP_BLE_Connection_Complete_Event_Data->Peer_Address, Callback_BoardStr);

					Display(("   Status:       0x%02X.\r\n", GAP_BLE_Connection_Complete_Event_Data->Status));
					Display(("   Role:         %s.\r\n", (GAP_BLE_Connection_Complete_Event_Data->Master)?"Master":"Slave"));
					Display(("   Address Type: %s.\r\n", (GAP_BLE_Connection_Complete_Event_Data->Peer_Address_Type == wlatPublic)?"Public":"Random"));
					Display(("   BD_ADDR:      %s.\r\n", Callback_BoardStr));
					// TODO:
					//ptBtOpt->eConnectType = eConnected_SPPLE;
					break;
				case GATT_BLE_CONNECTION_DEVICE_CONNECTION:
					Display(("<%d>****** ", __LINE__));
					Display(("GATT_BLE_CONNECTION_DEVICE_CONNECTION\r\n"));
					GATT_BLE_Device_Connection_Data = ((GATT_BLE_Device_Connection_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("GATT_BLE_Device_Connection_Data <Addr:%p>\r\n", GATT_BLE_Device_Connection_Data));

					BD_ADDRToStr(GATT_BLE_Device_Connection_Data->RemoteDevice, Callback_BoardStr);
					Display(("   Connection ID:   %u.\r\n", GATT_BLE_Device_Connection_Data->ConnectionID));
					Display(("   Connection Type: %s.\r\n", ((GATT_BLE_Device_Connection_Data->ConnectionType == wgctLE)?"LE":"BR/EDR")));
					Display(("   Remote Device:   %s.\r\n", Callback_BoardStr));
					Display(("   Connection MTU:  %u.\r\n", GATT_BLE_Device_Connection_Data->MTU));
					DisplayATCMD(("+IND=BLECONNECTED,%s\r\n",Callback_BoardStr));
					// TODO:
					ptBtOpt->slaveConnected = 1;
					ptBtOpt->eConnectType = eConnected_SPPLE;
					ptBtOpt->isServerEnd = 1;
					s_WltSetAddrToAddr(&ptBtOpt->CurrentSPPLE_BD_ADDR, &GATT_BLE_Device_Connection_Data->RemoteDevice);
					strcpy(ptBtOpt->SPPLE_Addr, Callback_BoardStr);
					s_WltProfileClose(WLT_PROFILE_ISPP|WLT_PROFILE_SPP|WLT_PROFILE_BLE);
					break;
				case GAP_BLE_DISCONNECTION_COMPLETE:
					/* 当远程设备从本地设备断开时该事件会被调度 */
					Display(("<%d>****** ", __LINE__));
					Display(("GAP_BLE_DISCONNECTION_COMPLETE\r\n"));
					GAP_BLE_Disconnection_Complete_Event_Data = ((GAP_BLE_Disconnection_Complete_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("GAP_BLE_Disconnection_Complete_Event_Data <Addr:%p>\r\n", GAP_BLE_Disconnection_Complete_Event_Data));
					Display(("   Status: 0x%02X.\r\n", GAP_BLE_Disconnection_Complete_Event_Data->Status));
					Display(("   Reason: 0x%02X.\r\n", GAP_BLE_Disconnection_Complete_Event_Data->Reason));

					BD_ADDRToStr(GAP_BLE_Disconnection_Complete_Event_Data->Peer_Address, Callback_BoardStr);
					Display(("   BD_ADDR: %s.\r\n", Callback_BoardStr));
					// TODO:
					//ptBtOpt->eConnectType = eConnected_None;
					ptBtOpt->slaveConnected = 0;
					ptBtOpt->eConnectType = eConnected_None;
					QueReset((T_Queue *)(&ptBtOpt->tSlaveSendQue));
					memset(ptBtOpt->SPPLE_Addr, 0x00, sizeof(ptBtOpt->SPPLE_Addr));
					s_WltProfileOpen(WLT_PROFILE_ALL);
					break;
				case GATT_BLE_CONNECTION_DEVICE_DISCONNECTION:
					/* 当一个远程设备（LE或BR/EDR）不连接时会调度此事件 */
					Display(("<%d>****** ", __LINE__));
					Display(("GATT_BLE_CONNECTION_DEVICE_DISCONNECTION\r\n"));

					GATT_BLE_Device_Disconnection_Data = ((GATT_BLE_Device_Disconnection_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("GATT_BLE_Device_Disconnection_Data <Addr:%p>\r\n", GATT_BLE_Device_Disconnection_Data));
					BD_ADDRToStr(GATT_BLE_Device_Disconnection_Data->RemoteDevice, Callback_BoardStr);
					Display(("   Connection ID:   %u.\r\n", GATT_BLE_Device_Disconnection_Data->ConnectionID));
					Display(("   Connection Type: %s.\r\n", ((GATT_BLE_Device_Disconnection_Data->ConnectionType == wgctLE)?"LE":"BR/EDR")));
					Display(("   Remote Device:   %s.\r\n", Callback_BoardStr));
					DisplayATCMD(("+IND=BLEDISCONNECTED,%s\r\n",Callback_BoardStr));
					// TODO:
					//ptBtOpt->eConnectType = eConnected_None;
					//QueReset((T_Queue *)(&ptBtOpt->tSendQue));
					//memset(ptBtOpt->SPPLE_Addr, 0x00, sizeof(ptBtOpt->SPPLE_Addr));
					break;
				case GATT_BLE_SERVICE_DISCOVERY_COMPLETE:
					/* 调度此事件时，先前启动的服务Discovery这操作是在远程设备上完成 */
					Display(("<%d>****** ", __LINE__));
					//GATT_BLE_Service_Discovery_Complete_Data_t		*GATT_BLE_Service_Discovery_Complete_Data;
					Display(("GATT_BLE_SERVICE_DISCOVERY_COMPLETE\r\n"));
					GATT_BLE_Service_Discovery_Complete_Data = ((GATT_BLE_Service_Discovery_Complete_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("GATT_BLE_Service_Discovery_Complete_Data <Addr:%p>\r\n", GATT_BLE_Service_Discovery_Complete_Data));
					if(GATT_BLE_Service_Discovery_Complete_Data->Status)
					{
						Display(("DISCOVERY COMPLETE OK\r\n"));
						switch (GATT_BLE_Service_Discovery_Complete_Data->DiscoveryType)
						{
							case sdGAPS:
								GAPS_Support_Service_Information = (GAPS_Support_Service_Information_t*)(GATT_BLE_Service_Discovery_Complete_Data->SupportServer);
								Display(("\r\nGAPS Service Discovery Summary\r\n"));
								Display(("	Supported GAPS Service:	 %s\r\n", (GAPS_Support_Service_Information->Supported_GAPS ? "Supported" : "Not Supported")));
								break;
							case sdSPPLE:
								SPPLE_Support_Service_Information = (SPPLE_Support_Service_Information_t*)(GATT_BLE_Service_Discovery_Complete_Data->SupportServer);
								Display(("\r\nSPPLE Service Discovery Summary\r\n"));
								Display(("	Supported SPPLE Service:	 %s\r\n", (SPPLE_Support_Service_Information->Supported_SPPLE? "Supported" : "Not Supported")));
								DisplayATCMD(("+OK=%s\r\n", (SPPLE_Support_Service_Information->Supported_SPPLE? "Supported" : "Not Supported")));
						        break;
							case sdANS:break;
							case sdLLS:break;
							case sdIAS:break;
							case sdTPS:break;
							default:break;
						}
						BTPS_FreeMemory((GATT_BLE_Service_Discovery_Complete_Data->SupportServer));
					}
					break;
				case SPP_PORT_STATUS_INDICATION:
					// SPPConnect如果连接成功，会出现这个SPP_PORT_STATUS_INDICATION事件
					// 如果是被动建立SPP连接,产生SPP_CONNECT_INDICATION事件后,也会产生这个事件
					Display(("<%d>****** ", __LINE__));				
					Display(("SPP_PORT_STATUS_INDICATION\r\n"));
					SPP_Port_Status_Indication_Data = ((SPP_Port_Status_Indication_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("SPP_Port_Status_Indication_Data <Addr:%p>\r\n", SPP_Port_Status_Indication_Data));
					Display(("SPP Port Status Indication: 0x%04X, Status: 0x%04X, Break Status: 0x%04X, Length: 0x%04X.\r\n", 
						SPP_Port_Status_Indication_Data->SerialPortID,
						SPP_Port_Status_Indication_Data->PortStatus,
						SPP_Port_Status_Indication_Data->BreakStatus,
						SPP_Port_Status_Indication_Data->BreakTimeout));
					Display(("ptBtOpt->LocalSerialPortID: %d\r\n", ptBtOpt->LocalSerialPortID));
					Display(("SPP_Port_Status_Indication_Data->SerialPortID:%d\r\n", SPP_Port_Status_Indication_Data->SerialPortID));

					#if 0
					// 作为clien端,主动发起SPPConnect成功,关闭所有profile,只能有一个设备连接上
					// 如果是被动进行SPP连接,产生SPP_CONNECT_INDICATION事件后,也会产生这个事件,则可以忽略该事件的处理
					if(!ptBtOpt->isServerEnd)
					{
						//s_WltProfileClose(WLT_PROFILE_ALL);
						ptBtOpt->SPPConnectComplete = 1;
						ptBtOpt->eConnectType = eConnected_SPP;
						ptBtOpt->LocalSerialPortID = SPP_Port_Status_Indication_Data->SerialPortID;
					}
					#endif
					// 此事件表明发起主动连接成功，这里把SPPConnectComplete标识置位和获取SerialPortID是必须的
					// 额外操作都在连接的最后一步进行
					// 因为被动连接也会产生此事件，因此需要判断为非Server端(主连接)才进行操作
					if(!ptBtOpt->isServerEnd)
					{
						ptBtOpt->SPPConnectComplete = 1;
						ptBtOpt->eConnectType = eConnected_SPP;
						ptBtOpt->masterSerialPortID = SPP_Port_Status_Indication_Data->SerialPortID;
						s_WltProfileClose(WLT_PROFILE_ALL);		// 主动连接成功之后关闭所有profile
					}
					// 从连接已经被连上，然后又来了个主连接  
					if(ptBtOpt->slaveConnected == 1 && SPP_Port_Status_Indication_Data->SerialPortID != ptBtOpt->slaveSerialPortID)
					{
						SPPDisconnectByNumbers(SPP_Port_Status_Indication_Data->SerialPortID);
						if(ptBtOpt->BtConnectOptStep != 0)
						{
							ptBtOpt->BtConnectOptStep = 0;
						}
					}
					break;
				case SPP_CONNECT_INDICATION:
					/* 此事件表明已得到了远程端口打开连接，只有从连接会产生该事件 */
					Display(("<%d>****** ", __LINE__));
					Display(("SPP_CONNECT_INDICATION\r\n"));
					SPP_Connect_Indication_Event_Data = ((SPP_Connect_Indication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("SPP_Connect_Indication_Event_Data <Addr:%p>\r\n", SPP_Connect_Indication_Event_Data));
					BD_ADDRToStr((SPP_Connect_Indication_Event_Data->Remote_Device), Callback_BoardStr);
					Display(("BD_ADDR: %s.SerialPortID:0x%04X\r\n", Callback_BoardStr,SPP_Connect_Indication_Event_Data->SerialPortID));
					DisplayATCMD(("+IND=SPPCONNECTED,%s,0x%04X\r\n",Callback_BoardStr,SPP_Connect_Indication_Event_Data->SerialPortID));
					// TODO:
					#if 0
					// 作为Server端,被其他设备连接上了,关掉除了SPP的profile
					s_WltProfileClose(WLT_PROFILE_BLE|WLT_PROFILE_ISPP|WLT_PROFILE_SPPLE);
					ptBtOpt->eConnectType = eConnected_SPP;
					ptBtOpt->isServerEnd = 1;
					ptBtOpt->LocalSerialPortID = SPP_Connect_Indication_Event_Data->SerialPortID;
					s_WltSetAddrToAddr(&ptBtOpt->CurrentSPP_BD_ADDR, &SPP_Connect_Indication_Event_Data->Remote_Device);
					s_DisplayWltAddr("ptBtOpt->CurrentSPP_BD_ADDR", &ptBtOpt->CurrentSPP_BD_ADDR);
					#endif
					
					// 有1个SPP被动连接成功，则关闭其他Profile，保证不会再被其他连接连上
					s_WltProfileClose(WLT_PROFILE_BLE|WLT_PROFILE_ISPP|WLT_PROFILE_SPPLE);
					
					ptBtOpt->slaveConnected = 1;
					ptBtOpt->eConnectType = eConnected_SPP;
					ptBtOpt->isServerEnd = 1;
					ptBtOpt->slaveSerialPortID = SPP_Connect_Indication_Event_Data->SerialPortID;
					s_WltSetAddrToAddr(&ptBtOpt->slaveCOnnectedRemoteAddr, &SPP_Connect_Indication_Event_Data->Remote_Device);
					s_WltSetAddrToAddr(&ptBtOpt->CurrentSPP_BD_ADDR, &SPP_Connect_Indication_Event_Data->Remote_Device);
					if(ptBtOpt->BtConnectOptStep > 0)
					{
						ptBtOpt->BtConnectOptStep = 0;
					}
					s_DisplayWltAddr("ptBtOpt->slaveConnectedRemoteAddr", &ptBtOpt->slaveCOnnectedRemoteAddr);
                    break;
				case SPP_DATA_INDICATION:
					/* 此事件表明数据已经到达一个端口上，需调用SPP_Data_Read()这函数来获得 */
					Display(("<%d>****** ", __LINE__));
					Display(("SPP_DATA_INDICATION\r\n"));
					SPP_Data_Indication_Event_Data = ((SPP_Data_Indication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("SPP_Data_Indication_Event_Data <Addr:%p>\r\n", SPP_Data_Indication_Event_Data));
					Display(("SPP Data Indication, ID: 0x%04X, Length: 0x%04X.\r\n",
						SPP_Data_Indication_Event_Data->SerialPortID,
						SPP_Data_Indication_Event_Data->DataLength));
					Display(("SPP_Data_Indication_Event_Data->SerialPortID = %d\r\n", SPP_Data_Indication_Event_Data->SerialPortID));
					// TODO:
					#if 0
					//if(!QueIsFull((T_Queue *)(ptBtOpt->tRevcQue))
					{
						memset(spp_data, 0x00, sizeof(spp_data));
						Display(("SPP_Data_Len = %d\r\n", SPP_Data_Indication_Event_Data->DataLength));
						Display(("SerialPortID = %d\r\n", SPP_Data_Indication_Event_Data->SerialPortID));
						iRet = SPPDataRead(spp_data, SPP_Data_Indication_Event_Data->DataLength+1, SPP_Data_Indication_Event_Data->SerialPortID);
						//Display(("SPPDataRead<iRet=%d>:%s", iRet, spp_data));
						Display(("SPPDataRead <iRet = %d>\r\n", iRet));
						//Display(("\r\n"));
						iRet = QuePuts((T_Queue *)(&ptBtOpt->tRecvQue), spp_data, SPP_Data_Indication_Event_Data->DataLength);
						Display(("QuePuts <iRet = %d>\r\n", iRet));
						if(iRet != SPP_Data_Indication_Event_Data->DataLength)
						{
							Display(("xxxx SPP Read <QuePuts=%d> xxxx\r\n", iRet));
						}
					}
					#endif
					
					if(ptBtOpt->masterSerialPortID == SPP_Data_Indication_Event_Data->SerialPortID)
					{
						memset(spp_data, 0x00, sizeof(spp_data));
						
						Display(("SPP_Data_Len = %d\r\n", SPP_Data_Indication_Event_Data->DataLength));
						Display(("SerialPortID = %d\r\n", SPP_Data_Indication_Event_Data->SerialPortID));
						iRet = SPPDataRead(spp_data, SPP_Data_Indication_Event_Data->DataLength+1, SPP_Data_Indication_Event_Data->SerialPortID);
						//Display(("SPPDataRead<iRet=%d>:%s", iRet, spp_data));
						Display(("SPPDataRead <iRet = %d>\r\n", iRet));
						//Display(("\r\n"));
						iRet = QuePuts((T_Queue *)(&ptBtOpt->tMasterRecvQue), spp_data, SPP_Data_Indication_Event_Data->DataLength);
						Display(("QuePuts to tMasterRecvQue <iRet = %d>\r\n", iRet));
						if(iRet != SPP_Data_Indication_Event_Data->DataLength)
						{
							Display(("xxxx SPP Read <QuePuts=%d> xxxx\r\n", iRet));
						}
					}
					
					if(ptBtOpt->slaveSerialPortID == SPP_Data_Indication_Event_Data->SerialPortID)
					{
						memset(spp_data, 0x00, sizeof(spp_data));
						Display(("SPP_Data_Len = %d\r\n", SPP_Data_Indication_Event_Data->DataLength));
						Display(("SerialPortID = %d\r\n", SPP_Data_Indication_Event_Data->SerialPortID));
						iRet = SPPDataRead(spp_data, SPP_Data_Indication_Event_Data->DataLength+1, SPP_Data_Indication_Event_Data->SerialPortID);
						//Display(("SPPDataRead<iRet=%d>:%s", iRet, spp_data));
						Display(("SPPDataRead <iRet = %d>\r\n", iRet));
						//Display(("\r\n"));
						iRet = QuePuts((T_Queue *)(&ptBtOpt->tSlaveRecvQue), spp_data, SPP_Data_Indication_Event_Data->DataLength);
						Display(("QuePuts to tSlaveRecvQue <iRet = %d>\r\n", iRet));
						if(iRet != SPP_Data_Indication_Event_Data->DataLength)
						{
							Display(("xxxx SPP Read <QuePuts=%d> xxxx\r\n", iRet));
						}
					}
					
					break;
				case SPP_DISCONNECT_INDICATION:
					/* 此事件表明端口已经关闭 */
					Display(("<%d>****** ", __LINE__));
					Display(("SPP_DISCONNECT_INDICATION\r\n"));
					SPP_Disconnect_Indication_Event_Data = ((SPP_Disconnect_Indication_Event_Data_t*)(Callback_Event_Data->CallbackParameter)); 
					Display(("SPP_DISCONNECT_INDICATION: Port = %d \r\n", SPP_Disconnect_Indication_Event_Data->SerialPortID)); 
					#if 0
					ptBtOpt->eConnectType = eConnected_None;
					QueReset((T_Queue *)(&ptBtOpt->tSendQue));
					// 一个SPP连接断开之后,打开所有的profile，其他也可以连接
					s_WltProfileOpen(WLT_PROFILE_ALL);
					#endif
					// 从连接断开
					if(ptBtOpt->slaveSerialPortID == SPP_Disconnect_Indication_Event_Data->SerialPortID)
					{
						ptBtOpt->eConnectType = eConnected_None;
						ptBtOpt->slaveConnected = 0;
						if(ptBtOpt->BtConnectOptStep != 0)
						{
							ptBtOpt->BtConnectOptStep = 0;
						}
						QueReset((T_Queue *)(&ptBtOpt->tSlaveSendQue));
					}
					// 主连接断开
					if(ptBtOpt->masterSerialPortID == SPP_Disconnect_Indication_Event_Data->SerialPortID)
					{
						ptBtOpt->eConnectType = eConnected_None;
						ptBtOpt->masterConnected = 0;
						ptBtOpt->BtConnectErrCode = -9;	
						ptBtOpt->isServerEnd = 1;
						if(ptBtOpt->BtConnectOptStep != 0)
						{
							ptBtOpt->BtConnectOptStep = 0;
						}
						// 避免一连段对端立马断开时出现的bug
						QueReset((T_Queue *)(&ptBtOpt->tMasterSendQue));
					}
					// 只有主连接和从连接均断开时才需要打开所有profile
					if(ptBtOpt->slaveConnected == 0 && ptBtOpt->masterConnected == 0)
						s_WltProfileOpen(WLT_PROFILE_ALL);
					break;
				case ISPP_OPEN_SESSION_INDICATION:
					/* 配对完成后,打开应用会产生该事件,应用打开一个session??? */
					Display(("<%d>****** ", __LINE__));
					Display(("ISPP_OPEN_SESSION_INDICATION \r\n"));
					ISPP_Session_Open_Indication_Data=((ISPP_Session_Open_Indication_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("ISPP_Session_Open_Indication_Data <Addr:%p>\r\n", ISPP_Session_Open_Indication_Data));
					Display(("ISPP_OPEN_SESSION_INDICATION SessionID %d\r\n", ISPP_Session_Open_Indication_Data->SessionID));
					// TODO:
					//ptBtOpt->isServerEnd = 1;
					ptBtOpt->eConnectType = eConnected_ISPP;
					ptBtOpt->ISPP_Session_state = eISPP_Session_Open;
					// ISPP session 打开后才相当于SPP连接建立完成,关闭其他profile
					//s_WltProfileClose(WLT_PROFILE_BLE|WLT_PROFILE_SPP|WLT_PROFILE_SPPLE);
					break;
				case ISPP_PORT_PROCESS_STATUS:
					/* iPad点击配对完成后 */
					Display(("<%d>****** ", __LINE__));
					Display(("ISPP_PORT_PROCESS_STATUS \r\n"));
					ISPP_Process_Status=((ISPP_Process_Status_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("ISPP_Process_Status <Addr:%p>\r\n", ISPP_Process_Status));
					switch(ISPP_Process_Status->ProcessState)
					{
						case psStartIdentificationRequest:
							Display(("Step: psStartIdentificationRequest\r\n"));
							break;
						case psStartIdentificationProcess:
							Display(("Step: psStartIdentificationProcess\r\n"));
							break;
						case psIdentificationProcess:
							Display(("Step: psIdentificationProcess\r\n"));
							break;		
						case psIdentificationProcessComplete:
							Display(("ISPP Identification Process Complete %s\r\n", ProcessStatus[ISPP_Process_Status->Status]));

							if(ISPP_Process_Status->Status == 0){
								Display(("Now Apple Device Connect Success\r\n"));
									// TODO:
									s_WltSetAddrToAddr(&ptBtOpt->CurrentISPP_BD_ADDR, &ptBtOpt->LastPair_BD_ADDR);
									ptBtOpt->eConnectType = eConnected_ISPP;
									ptBtOpt->ISPP_Session_state = eISPP_Session_Close;
									ptBtOpt->ISPPConnectComplete = 1;
									if(ptBtOpt->isServerEnd == 0)			// 主动连接
									{
										ptBtOpt->masterConnected = 1;
										ptBtOpt->masterSerialPortID = ISPP_Process_Status->SerialPortID;
										// 关闭所有profile
										s_WltProfileClose(WLT_PROFILE_ALL);
										Display(("%d ISPP MASTER\r\n",__LINE__));
									}
									else
									{
										ptBtOpt->slaveConnected = 1;
										ptBtOpt->slaveSerialPortID = ISPP_Process_Status->SerialPortID;
										ptBtOpt->isServerEnd = 1;
										// 作为Server端被iOS设备连接上,关闭除ISPP之外的所有profile
										s_WltProfileClose(WLT_PROFILE_BLE|WLT_PROFILE_SPP|WLT_PROFILE_SPPLE);
										Display(("%d ISPP SLAVE\r\n",__LINE__));
									}
									// 作为Server端被iOS设备连接上,关闭除ISPP之外的所有profile
							//		s_WltProfileClose(WLT_PROFILE_BLE|WLT_PROFILE_SPP|WLT_PROFILE_SPPLE);
							}	
							break;
						case psStartAuthenticationProcess:
							Display(("Step: psStartAuthenticationProcess\r\n"));
							break;
						case psAuthenticationProcess:
							Display(("Step: psAuthenticationProcess\r\n"));
							break;
						case psAuthenticationProcessComplete:
							Display(("Step: psAuthenticationProcessComplete\r\n"));
							break;
						default:
							Display(("Step: <Err:%d>\r\n",ISPP_Process_Status->ProcessState));
							break;
					}
					break;			
				case ISPP_DATA_INDICATION:
					Display(("<%d>****** ", __LINE__));
					Display(("ISPP_DATA_INDICATION \r\n"));
	 				ISPP_Session_Data_Indication_Data = ((ISPP_Session_Data_Indication_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("ISPP_Session_Data_Indication_Data <Addr:%p>\r\n", ISPP_Session_Data_Indication_Data));
						Display(("SerialPortID : %d\r\n", ISPP_Session_Data_Indication_Data->SerialPortID));
						Display(("SerialPortID: %d\r\n", ISPP_Session_Data_Indication_Data->SessionID));
						Display(("iSPP Read Data<Len:%d>\r\n", ISPP_Session_Data_Indication_Data->DataLength));
					// TODO:
					if(ptBtOpt->masterSerialPortID == ISPP_Session_Data_Indication_Data->SerialPortID)
					{
						iRet = QuePuts((T_Queue *)&ptBtOpt->tMasterRecvQue, ISPP_Session_Data_Indication_Data->DataPtr, ISPP_Session_Data_Indication_Data->DataLength);
						Display(("QuePuts <iRet = %d>\r\n", iRet));
						if(iRet != ISPP_Session_Data_Indication_Data->DataLength)
						{
							Display(("xxxx ISPP Read <QuePuts=%d> xxxx\r\n", iRet));
						}
					}

					if(ptBtOpt->slaveSerialPortID == ISPP_Session_Data_Indication_Data->SerialPortID)
					{
						iRet = QuePuts((T_Queue *)&ptBtOpt->tSlaveRecvQue, ISPP_Session_Data_Indication_Data->DataPtr, ISPP_Session_Data_Indication_Data->DataLength);
						Display(("QuePuts <iRet = %d>\r\n", iRet));
						if(iRet != ISPP_Session_Data_Indication_Data->DataLength)
						{
							Display(("xxxx ISPP Read <QuePuts=%d> xxxx\r\n", iRet));
						}
					}
					break;		
				case ISPP_CLOSE_SESSION_INDICATION:
					/* 应用被home键退出会产生此事件,应用关闭一个session??? */
					Display(("<%d>****** ", __LINE__));
					Display(("ISPP_CLOSE_SESSION_INDICATION \r\n"));
					ISPP_Session_Close_Indication_Data=((ISPP_Session_Close_Indication_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("ISPP_Session_Close_Indication_Data <Addr:%p>\r\n", ISPP_Session_Close_Indication_Data));
					Display(("ISPP_CLOSE_SESSION_INDICATION SessionID %d\r\n", ISPP_Session_Close_Indication_Data->SessionID));
						// TODO:
						ptBtOpt->ISPP_Session_state = eISPP_Session_Close;
						ptBtOpt->eConnectType = eConnected_ISPP;
						// ISPP session 关闭后才相当于SPP连接断开,打开其他profile
						//s_WltProfileOpen(WLT_PROFILE_ALL);
					break;
				case ISPP_CLOSE_PORT_INDICATION:
					/* iPad忽略次设备会产生此事件 */
					Display(("<%d>****** ", __LINE__));
					Display(("ISPP_CLOSE_PORT_INDICATION \r\n"));
					ISPP_Close_Port_Indication_Data=((SPP_Close_Port_Indication_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("ISPP_Close_Port_Indication_Data <Addr:%p>\r\n", ISPP_Close_Port_Indication_Data));
					Display(("ISPP Close Port, ID: 0x%04X\r\n", ISPP_Close_Port_Indication_Data->SerialPortID));
					Display(("Now Apple Device Disconnect\r\n"));
					// TODO:
					ptBtOpt->isServerEnd = 1;
					ptBtOpt->masterConnected = 0;
					ptBtOpt->slaveConnected = 0;
					ptBtOpt->eConnectType = eConnected_None;
					QueReset((T_Queue *)(&ptBtOpt->tSlaveSendQue));
					ptBtOpt->ISPP_Session_state = eISPP_Session_Close;
					// iPad在"设置","蓝牙"中忽略此设备,才相当于SPP连接被断开,打开其他profile
					s_WltProfileClose(WLT_PROFILE_ALL);
					s_WltProfileOpen(WLT_PROFILE_ALL);
					break;
				case SPPLE_DATA_INDICATION:
					/* 此事件表明数据已经达到一个端口上 */
					Display(("<%d>****** ", __LINE__));
					Display(("SPPLE_DATA_INDICATION\r\n"));
					//SPPLE_Data_Indication_Event_Data_t				*SPPLE_BLE_Data_Indication_Event_Data;
	                SPPLE_BLE_Data_Indication_Event_Data = ((SPPLE_Data_Indication_Event_Data_t*)(Callback_Event_Data->CallbackParameter));
					Display(("SPPLE_BLE_Data_Indication_Event_Data <Addr:%p>\r\n", SPPLE_BLE_Data_Indication_Event_Data));
					BD_ADDRToStr(SPPLE_BLE_Data_Indication_Event_Data->ConnectionBD_ADDR, Remote_BoardStr);
	                Display(("spple data length:%d\r\n", SPPLE_BLE_Data_Indication_Event_Data->DataLength));
					// TODO:
					//if(!QueIsFull((T_Queue *)(ptBtOpt->tRevcQue))
					{
						unsigned char spple_data[512];
						iRet = SPPLEDataRead(sizeof(spple_data), spple_data);
						Display(("SPPLEDataRead <iRet = %d>\r\n", iRet));
						iRet = QuePuts((T_Queue *)&ptBtOpt->tSlaveRecvQue, spple_data, iRet);
						Display(("QuePuts <iRet = %d>\r\n", iRet));
						if(iRet != SPPLE_BLE_Data_Indication_Event_Data->DataLength)
						{
							Display(("xxxx SPPLE Read <QuePuts=%d> xxxx\r\n", iRet));
						}
					}
					break;
				default:
					Display(("<%d>****** ", __LINE__));
					Display(("Event Err: Callback_Event_Data->CallbackEvent = %d\r\n", Callback_Event_Data->CallbackEvent));
					break;
			}
			if((Callback_Event_Data->CallbackParameter)!=NULL)
				BTPS_FreeMemory((Callback_Event_Data->CallbackParameter));
			DisplayPrompt();
		}
	}
}

   /* The following function is responsible for retrieving the Commands */
   /* from the Serial Input routines and copying this Command into the  */
   /* specified Buffer.  This function blocks until a Command (defined  */
   /* to be a NULL terminated ASCII string).  The Serial Data Callback  */
   /* is responsible for building the Command and dispatching the Signal*/
   /* that this function waits for.                                     */
static int GetInput(void)
{
   char Char;
   int  Done;

   /* Initialize the Flag indicating a complete line has been parsed.   */
   Done = 0;

   /* Attempt to read data from the Console.                            */
   if(!PortRecv(0, &Char,0))
   {
   //	printf_uart("cmd\r\n");
   //  	PortSend(0, Char);
      switch(Char)
      {
         case '\r':
         case '\n':
            /* This character is a new-line or a line-feed character    */
            /* NULL terminate the Input Buffer.                         */
            Input[InputIndex] = '\0';

            /* Set Done to the number of bytes that are to be returned. */
            /* ** NOTE ** In the current implementation any data after a*/
            /*            new-line or line-feed character will be lost. */
            /*            This is fine for how this function is used is */
            /*            no more data will be entered after a new-line */
            /*            or line-feed.                                 */
            Done       = (InputIndex-1);
            InputIndex = 0;
            break;
         case 0x08:
            /* Backspace has been pressed, so now decrement the number  */
            /* of bytes in the buffer (if there are bytes in the        */
            /* buffer).                                                 */
            if(InputIndex)
            {
               InputIndex--;

                PortSend(0, '\b');
                PortSend(0, ' ');
				PortSend(0, '\b');

            }
            break;
         default:
            /* Accept any other printable characters.                   */
            if((Char >= ' ') && (Char <= '~'))
            {
               /* Add the Data Byte to the Input Buffer, and make sure  */
               /* that we do not overwrite the Input Buffer.            */
               Input[InputIndex++] = Char;
               PortSend(0, Char);

               /* Check to see if we have reached the end of the buffer.*/
               if(InputIndex == (MAX_COMMAND_LENGTH-1))
               {
                  Input[InputIndex] = 0;
                  Done              = (InputIndex-1);
                  InputIndex        = 0;
               }
            }
            break;
      }
   }

   return(Done);
}

/* The following function processes terminal input.                  */
static void ProcessCharacters(void *UserParameter)
{
   /* Check to see if we have a command to process.                     */
   if(GetInput() > 0)
   {
	  // printf_uart("ProcessCharacters\r\n");
      /* Attempt to process a character.                                */
      ProcessCommandLine(Input);
   }
}

extern  int SetISPPIdentificationInfo(unsigned int iFIDInfoLength, unsigned char *pFIDInfo,unsigned int iIAP2InfoLength,unsigned char *pIAP2Info) ;

/* The following defines the static information that identifies the	*/
/* accessory and the features that it supports.						*/
/* * NOTE * The data in this structure is custom for each accessory. */
/*		   The data that is contained in it will need to be updates */
/*		   for each project.										*/
static unsigned char FIDInfo[256];
static unsigned char IAP2Info[256];
#if 0
static unsigned char FIDInfo[] =
{
  0x0B, 					/* Number of Tokens included.			   */

  0x0C, 					/* Identity Token Length				   */
  0x00,0x00,				/* Identity Token IDs					   */
  0x01, 					/* Number of Supported Lingo's			   */
  0x00, 					/* Supported Lingos.					   */
  0x00,0x00,0x00,0x02,		/* Device Options						   */
  0x00,0x00,0x02,0x00,		/* Device ID from Authentication Processor */

  0x0A, 					/* AccCap Token Length. 				   */
  0x00,0x01,				/* AccCap Token IDs 					   */
  0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,		 /* AccCap Value	   */

  0x0D,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x01, 					/* Acc Name 							   */
  'P','A','X',' ','D','2','0','0','A',0, // 10

  0x06,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x04,0x01,0x00,0x00,		/* Acc Firmware Version 				   */

  0x06,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x05,0x01,0x00,0x00,		/* Acc Hardware Version 				   */

  0x07,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x06, 					/* Manufacturer 						   */
  'P','A','X',0, // 4

  0x09,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x07, 					/* Model Name							   */
  'D','2','0','0','A',0, // 6

  0x05,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x09, 					/* Max Incomming MTU size				   */
  0x00,0x80,				/* Set MTU to 128						   */

  0x07,
  0x00,0x02,				/* Acc Info Token IDs					   */
  0x0C,0x00,0x00,0x00,0x07, /* Supported Products.					   */

  0x12,
  0x00,0x04,				/* SDK Protocol Token					   */
  0x01, 					/* Supported Products.					   */
  'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0, // 15

  0x0D,
  0x00,0x05,				/* Bundle Seed ID Pref Token			   */
  'L','P','J','7','C','N','3','3','R','E',0
} ;


static unsigned char IAP2Info[] =
{
  0x00,0x0E,
  0x00,0x00,			   /* Acc Name								  */
  'P','A','X',' ','D','2','0','0','A',0, // 10

  0x00,0x0A,
  0x00,0x01,			   /* Model Name							  */
  'D','2','0','0','A',0,// 6

   0x00,0x08,
   0x00,0x02,				/* Manufacturer 						   */
  'P','A','X',0,// 4

  0x00,0x0D,
   0x00,0x03,				/* Serial Number						   */
   '0','0','0','0','0','0','0','0',0, // 9

  0x00,0x0A,
   0x00,0x04,				/* Acc Firmware Version 				   */
   '1','.','0','.','0',0,

  0x00,0x0A,
   0x00,0x05,				/* Acc Hardware Version 				   */
   '1','.','0','.','0',0,

  0x00,0x06,
   0x00,0x06,				/* Messages Sent by Acc 				   */
   0xEA,0x02,				/* App Launch							   */

  0x00,0x08,
   0x00,0x07,				/* Messages Received by Acc 			   */
   0xEA,0x00,				/* External Accessory Protocol Start	   */
   0xEA,0x01,				/* External Accessory Protocol Stop 	   */

  0x00,0x05,
   0x00,0x08,				/* Power Source Type					   */
   0x00,

  0x00,0x06,
   0x00,0x09,				/* Current Draw by Device				   */
   0x00,0x00,

  0x00,0x21,
  0x00,0x0A,			   /* External Accessory Protocol			  */
  0x00,0x05,
  0x00,0x00,			  /* Protocol ID							 */
  0x01,
  0x00,0x13,
  0x00,0x01,			  /* Protocol Name							 */
  'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0, // 15
  0x00,0x05,
  0x00,0x02,			  /* Match Action							 */
  0x00,

  0x00,0x07,
  0x00,0x0C,			   /* Current Language						  */
  'e','n',0,
  0x00,0x07,
  0x00,0x0D,			   /* Supported Languages					  */
  'e','n',0,
};
#endif

static void setSerailNumber2IAP2Info(unsigned char *iAp2Info)
{
	unsigned char sn[33];
	int i, iRet;
	int index = 0;
	
	iRet = ReadSN(sn);
	if(sn[0] != '\0')
	{
		index = (iAp2Info[0] << 8) | iAp2Info[1]; // [0]=0,[1]=14 -> index = 14
		index += ((iAp2Info[index] << 8) | iAp2Info[index+1]); // [14]=0,[15]=10 -> index = 24
		index += ((iAp2Info[index] << 8) | iAp2Info[index+1]); // [24]=0,[25]=8 -> index = 32
		index += 4;
		for(i = 0; i < 8; i++)				
			iAp2Info[index+i] = sn[i];
	}
}

extern int WLT_HAL_NV_DataWrite(int Length, unsigned char *Buffer);
extern int WLT_HAL_NV_DataRead(int Length, unsigned char *Buffer);

void DisplayBluetoothStackVersion()
{
    int ret_val;
    Bluetooth_Stack_Version_t version;
	ret_val = GetBluetoothStackVersion(&version);
    if(ret_val == 0)
    {
        Display(("**********************************************************\r\n"));
        Display(("Bluetooth Core Version:%d.%d.%d \r\n", 
			version.StackVersion[0], version.StackVersion[1], version.StackVersion[2]));
        Display(("       Release Version:%d.%d.%d \r\n",
			version.CustomerVersion[0], version.CustomerVersion[1], version.CustomerVersion[2]));
        Display(("**********************************************************\r\n"));
    }
    else
    {
        Display(("GetBluetoothStackVersion Error \r\n"));
    }
}

   /* The following function is registered with the application so that */
   /* it can display strings to the debug UART.                         */
static int PAXDisplayCallback(int Length, char *Buffer)
{
//   PortSends(0, (unsigned char *)Buffer, Length);
   return 0;
}

/* The following function is the main user interface thread.  It     */
/* opens the Bluetooth Stack and then drives the main user interface.*/
/* The following function is the main user interface thread.  It     */
/* opens the Bluetooth Stack and then drives the main user interface.*/
int MainThread(void *UserParameter)
{
	int i, iRet;
    int FIDInfoLen, IAP2InfoLen; 
	BD_ADDR_t  BD_ADDR;
	BTPS_Initialization_t   BTPS_Initialization;
	HCI_DriverInformation_t HCI_DriverInformation;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	DisplayBluetoothStackVersion();
	EnableHciLog(0);

	Register_HAL_NV_DataRead_Callback(WLT_HAL_NV_DataRead);
	Register_HAL_NV_DataWrite_Callback(WLT_HAL_NV_DataWrite);
	//Register_HAL_NV_DataRead_Callback(NULL);
	//Register_HAL_NV_DataWrite_Callback(NULL);

	Display(("Into MainThread\r\n"));
	/* Configure the UART Parameters.                                    */
	//HCI_DRIVER_SET_COMM_INFORMATION(&HCI_DriverInformation, 1, 115200, cpHCILL);
	HCI_DRIVER_SET_COMM_INFORMATION(&HCI_DriverInformation, 1, 115200, cpUART);

	Display(("HCI_DRIVER_SET_COMM_INFORMATION Finish\r\n"));

	HCI_DriverInformation.DriverInformation.COMMDriverInformation.InitializationDelay = 100;

	BTPS_Initialization.MessageOutputCallback = PAXDisplayCallback;

	/* Initialize the application.                                       */
	// PortSends(0,"start\r\n",strlen("start\r\n"));
	for(i = 0; i < 3; i++)
	{
		Display(("InitializeApplication[%d]\r\n", i));
		iRet = InitializeApplication(&HCI_DriverInformation, &BTPS_Initialization);
		Display(("InitializeApplication[%d]=%d\r\n", i, iRet));
		if(iRet >= 0)
			break;
		else
			DeInitializeApplication();
	}
	if(i >= 3)
	{
		Display(("InitializeApplication <Err:%d>\r\n", iRet));
		return iRet;
	}
	iRet = GetLocalAddress(&BD_ADDR); 	
	/* Check the return value of the submitted command for success.   */
	if(iRet == 0)
	{
		s_WltSetAddrToAddr(&ptBtOpt->Local_BD_ADDR, &BD_ADDR);

		s_DisplayWltAddr("ptBtOpt->Local_BD_ADDR", &ptBtOpt->Local_BD_ADDR);
		Display(("ptBtOpt->LocalPin: %s\r\n", ptBtOpt->LocalPin));
	}
	
	UserInterface_Selection();

    FIDInfoLen  = getFIDInfo(FIDInfo);
    IAP2InfoLen = getIAP2Info(IAP2Info);
	setSerailNumber2IAP2Info(IAP2Info);
	SetISPPIdentificationInfo(FIDInfoLen, (unsigned char *)&FIDInfo, IAP2InfoLen, (unsigned char *)&IAP2Info);
	
	InitializeVariable();
	BTPS_AddFunctionToScheduler(MainTask, NULL, 10);
	BTPS_AddFunctionToScheduler(SendTask, NULL, 10);
	
	return 0;
}

/* The following is the Main application entry point.  This function */
/* will configure the hardware and initialize the OS Abstraction     */
/* layer, create the Main application thread and start the scheduler.*/
int testMain(void)
{
	return 0;
}

