typedef struct{
	unsigned int addr;
	unsigned char funcname[28];
}kernel_symbol;

#define  KERNEL_SYMBOL __attribute__ ((section ("__ksymtab"))) 
#define EXPORT_SYMBOL(sym) 	KERNEL_SYMBOL kernel_symbol __ksymtab_##sym = { (unsigned int)sym, #sym };


//SysFun
void Beep(void);
void Beepf(void);
void SetTime(void);
void GetTime(void);
void ReadSN(void);
void ReadVerInfo(void);
void TimerSet(void);
void EXReadSN(void);
void GetLastError(void);
void TimerCheck(void);
void SystemInit(void);
void PowerOff(void);
void OnBase(void);
void BatteryCheck(void);
void GetTermInfo(void);
void GetTimerCount(void);
void GetVolt(void);
void ReadFontLib(void);
void EnumFont(void);
void SelScrFont(void);
void SelPrnFont(void);
void Soft_Reset(void);
void SysSleep(void);
void SysIdle(void);
void GetTermInfoExt(void);
void GetRandom(void);
void SysConfig(void);
void GetHardwareConfig(void);
void UpperCase(void);
void GetAppId(void);
void GetUsClock(void);
void DelayMs(void);
void DelayUs(void);
void GetLcdFontDot(void);

     
//IcFun
void IccInit(void);
void IccAutoResp(void);
void IccClose(void);
void IccIsoCommand(void);
void IccDetect(void);
void Mc_Clk_Enable(void);
void Mc_Io_Dir(void);
void Mc_Io_Read(void);
void Mc_Io_Write(void);
void Mc_Vcc(void);
void Mc_Reset(void);
void Mc_Clk(void);
void Mc_C4_Write(void);
void Mc_C8_Write(void);
void Mc_C4_Read(void);
void Mc_C8_Read(void);
void Read_CardSlotInfo(void);
void Write_CardSlotInfo(void);
     
//Pi ccFun
void PiccOpen(void);
void PiccSetup(void);
void PiccClose(void);
void PiccDetect(void);
void PiccIsoCommand(void);
void PiccRemove(void);
void M1Authority(void);
void M1ReadBlock(void);
void M1WriteBlock(void);
void M1Operate(void);
void PiccLight(void);
void PiccCmdExchange(void);
void PiccInitFelica(void);
void PiccManageReg(void);
     
//MagFun
void MagOpen(void);
void MagClose(void);
void MagReset(void);
void MagSwiped(void);
void MagRead(void);
     
//LcdFun
void ScrCls(void);
void ScrClrLine(void);
void ScrGray(void);
void ScrBackLight(void);
void ScrGotoxy(void);
void ScrPlot(void);
void ScrDrLogo(void);
void ScrFontSet(void);
void ScrAttrSet(void);
void ScrSetIcon(void);
void ScrPrint(void);
void Lcdprintf(void);
void ScrDrLogoxy(void);
void ScrRestore(void);
void ScrDrawBox(void);
void SetLightTime(void);
void ScrGotoxyEx(void);
void ScrGetxyEx(void);
void ScrDrawRect(void);
void ScrClrRect(void);
void ScrSpaceSet(void);
void ScrGetLcdSize(void);
void ScrTextOut(void);
void ScrSetOutput(void);
void ScrSetEcho(void);
void ScrGetBackLight(void);

//KeyboardFun
void kbhit(void);
unsigned char getkey(void);
unsigned char _getkey(void){return getkey();}
void kbflush(void);
void GetString(void);
void kbmute(void);
void kblight(void);
int KbReleased(void);

//CommFun
void ModemDial(void);
void ModemCheck(void);
void ModemTxd(void);
void ModemRxd(void);
void ModemReset(void);
void ModemAsyncGet(void);
void OnHook(void);
void s_ModemInfo(void);
void ModemExCommand(void);
void PortOpen(void);
void PortClose(void);
void PortSend(void);
void PortRecv(void);
void PortRecvs(void);
void PortReset(void);
void PortTxPoolCheck(void);
void PortSends(void);
void PortPeep(void);
void SetHeartBeat(void);
     
//PrnFun
void PrnInit(void);
void PrnFontSet(void);
void PrnSpaceSet(void);
void PrnLeftIndent(void);
void PrnStep(void);
void PrnLogo(void);
void PrnStr(void);
void PrnStart(void);
void PrnStatus(void);
void PrnGetDotLine(void);
void PrnSetGray(void);
void PrnGetFontDot(void);
void PrnTemperature(void);
void PrnDoubleWidth(void);
void PrnDoubleHeight(void);
void PrnAttrSet(void);
void PrnPreFeedSet(void);

//FileFun
void open(void);
void read(void);
void write(void);
void close(void);
void seek(void);
void remove(void);

long filesize(void);
long freesize(void);
void truncate(void);
void fexist(void);
void GetFileInfo(void);
void s_SearchFile();
void s_RenameFile();
void s_filesize();
void LoadFontLib();
void s_removeId();
void s_remove();

void ex_open(void);
void s_open(void);
void FileToApp(void);
void FileToParam(void);
void FileToFont(void);
void FileToMonitor(void);
void FileToDriver(void);
void DelAppFile(void);
void DelFontFile(void);

void FileToPuk(void);
void FsRecycle(void);
void tell(void);

//OtherFunc
void GetEnv(void);
void PutEnv(void);
void des(void);
void Hash(void);
void ReadAppInfo(void);
void ReadAppState(void);
void RunApp(void);
void DoEvent(void);
void CheckIfManageExist(void);
void s_GetMatrixDot(void);
void s_iVerifySignature(void);
     
//LineNetFunc
void NetSocket(void);
void NetBind(void);
void NetConnect(void);
void NetListen(void);
void NetAccept(void);
void NetSend(void);
void NetSendto(void);
void NetRecv(void);
void NetRecvfrom(void);
void NetCloseSocket(void);
void Netioctl(void);
void SockAddrSet(void);
void SockAddrGet(void);
void DhcpStart(void);
void DhcpStop(void);
void DhcpCheck(void);
void PPPLogin(void);
void PPPLogout(void);
void PPPCheck(void);
void eth_mac_get(void);
void EthSet(void);
void EthSetRateDuplexMode(void);
void DnsResolve(void);
void DnsResolveExt(void);
void NetPing(void);
void RouteSetDefault(void);
void RouteGetDefault(void);
void NetDevGet(void);
void NetAddStaticArp(void);
void NetSetIcmp(void);
void PppoeLogin(void);
void PppoeLogout(void);
void PppoeCheck(void);
void EthGetFirstRouteMac(void);
     
//FATFun
void FsOpen(void);
void FsClose(void);
void FsDelete(void);
void FsGetInfo(void);
void FsRename(void);
void FsDirSeek(void);
void FsDirRead(void);
void FsFileRead(void);
void FsFileWrite(void);
void FsFileSeek(void);
void FsFileTell(void);
void FsFileTruncate(void);
void FsSetWorkDir(void);
void FsGetWorkDir(void);
void FsUdiskIn(void);
void FsGetDiskInfo(void);
     
     
//ExWNetFun
void WlInit(void);
void WlGetSignal(void);
void WlPppLogin(void);
void WlPppLogout(void);
void WlPppCheck(void);
void WlOpenPort(void);
void WlClosePort(void);
void WlSendCmd(void);
void WlSwitchPower(void);
void WlSelSim(void);
void WlAutoStart(void);
void WlAutoCheck(void);
void WlPppLoginEx(void);

/*internal APIs*/
void GetCallAppInfo(void);
void SoMemSwitch(void);
void CfgSoMem(void);
void GetAppInfoAddr(void);

/*multitask*/
void OsCreate(void);
void OsSuspend(void);
void OsResume(void);
void OsSleep(void);
void OsStop();
void OsStart();


void irq_restore_asm(void);
void irq_save_asm(void);
void ClearIDCache(void);
void MpConvertToSo(void);
void HexToAscii(void);
void CheckCPUfrequency(void);
void GetUseAreaCode(void);
void AsciiToHex(void);
void get_term_name(void);
void kbsound(void);
void get_kblightmode(void);
void get_scrbacklightmode(void);
void kbcheck(void);
void putkey(void);
void ReadMacAddr(void);
void SysParaRead(void);
void SysParaWrite(void);
void EthGet(void);
void eth_mac_set(void);
void NetGetVersion(void);
void s_PrnStop(void);
void ip_forcesyn(void);
void CheckUpSensor(void);
void GetSensorSW(void);
void ShowSensorInfo(void);
void CreateFileSys(void);
void s_read(void);
void k_write(void);
void s_seek(void);
void GetFileLen(void);
void WriteMonitor(void);
void s_iDeleteAppParaFile(void);
void iGetInput(void);
void LocalDload(void);
void PEDManage(void);
void ModuleUpdate(void);
void XmlGetElement(void);
void XmlDelElement(void);
void CopySW(void);
void WaitKeyBExit(void);
void XmlAddElement(void);
void GetCharSetName(void);
void s_GetFontVer(void);
void s_GetCurScrFont(void);
void ReadTUSN(void);
void is_chn_font(void);
void s_GetSPrnFontDot(void);
void s_LcdSetDot(void);
void s_ScrRect(void);
void s_ScrWriteBitmap(void);
void s_LcdDrawLine(void);
void s_ScrLightCtrl(void);
void s_LcdGetDot(void);
void s_LcdClear(void);
void s_LcdDrawIcon(void);
void s_ScrGetLcdBufAddr(void);
void s_ScrFillRect(void);
void GetLcdInfo(void);
void GetScrCurAttr(void);
void ScrSpaceGet(void);
void s_Lcdprintf(void);
void s_ScrPrint(void);
void s_CLcdPrint(void);
void CLcdDrawPixel(void);
void CLcdBgDrawBox(void);
void CLcdGetInfo(void);
void CLcdPrint(void);
void CLcdDrawRect(void);
void CLcdTextOut(void);
void CLcdSetFgColor(void);
void CLcdClrLine(void);
void CLcdClrRect(void);
void CLcdSetBgColor(void);
void CLcdGetPixel(void);
void CLcdWriteBitmap(void);
void CLcdReadBitmap(void);
void s_ScrGetLcdArea(void);
void s_CLcdTextOut(void);
void s_GetLcdFontCharAttr(void);
//void RSAPrivateDecrypt(void);
//void RSAPublicEncrypt(void);
void RSAPrivateEncrypt(void);
void RSARecover(void);
void RSAPublicDecrypt(void);
void sha256(void);
void MsrVerifyKey(void);
void MsrReadSn(void);
void MsrReadKeyStatus(void);
void MsrUpdateKey(void);
void MsrInitKey(void);
void MsrWriteSn(void);
void DeleteApp(void);
void CalcAppSize(void);
void CheckFirst(void);
void app_even_start(void);
void Decompress(void);
void WlCheckSim(void);
void WlTcpRetryNum(void);
void WlModelComUpdate(void);
void WlSetCtrl(void);
void WlPowerOff(void);
void WlSetTcpDetect(void);
void GetWlFirmwareVer(void);
void WlGetModuleInfo(void);
void WlComExchange(void);

void FsDirDelete(void);
void FsDirOpen(void);
void FsSetAttr(void);
void IcmpReply(void);
void GetLeanAngle(void);
void SoundPlay(void);
void TouchScreenAttrSet(void);
void TouchScreenFlush(void);
void TouchScreenOpen(void);
void TouchScreenRead(void);
void TouchScreenClose(void);
void DrawIcon(void);
void ScanOpen(void);
void ScanClose(void);
void ScannerReset(void);
void ScanRead(void);

/* export for PED */
void SCR_PRINT(void);
void s_Get10MsTimerCount(void);
void s_iDeleteApp(void);
void WriteSecRam(void);
void ReadSecRam(void);
void GetKeyMs(void);
void CheckIfDevelopPos(void);

//void BtOpen(void);
//void BtClose(void);

#if 0 //deleted by shics
void BtOpen(void);
void BtClose(void);
void BtGetConfig(void);
void BtSetConfig(void);
void BtScan(void);
void BtConnect(void);
void BtDisconnect(void);
void BtGetStatus(void);
void BtCtrl(void);
#endif

//export for wifi
void WifiOpen(void);
void WifiClose(void);
void WifiScan(void);
void WifiConnect(void);
void WifiDisconnect(void);
void WifiCheck(void);
void WifiCtrl(void);


void WifiUpdateOpen(void);
void s_TimerSetMs(void);
void s_TimerCheckMs(void);

EXPORT_SYMBOL(WifiUpdateOpen)   
EXPORT_SYMBOL(s_TimerSetMs)   
EXPORT_SYMBOL(s_TimerCheckMs)   	

// SysFun
EXPORT_SYMBOL(Beep)                 
EXPORT_SYMBOL(Beepf)  
EXPORT_SYMBOL(SetTime)              
EXPORT_SYMBOL(GetTime)              
EXPORT_SYMBOL(ReadSN)               
EXPORT_SYMBOL(ReadVerInfo)          
EXPORT_SYMBOL(TimerSet)             
EXPORT_SYMBOL(EXReadSN)             
EXPORT_SYMBOL(GetLastError)         
EXPORT_SYMBOL(TimerCheck)           
EXPORT_SYMBOL(SystemInit)           
EXPORT_SYMBOL(PowerOff)             
EXPORT_SYMBOL(OnBase)               
EXPORT_SYMBOL(BatteryCheck)         
EXPORT_SYMBOL(GetTermInfo)          
EXPORT_SYMBOL(GetTimerCount)        
EXPORT_SYMBOL(GetVolt)              
EXPORT_SYMBOL(ReadFontLib)          
EXPORT_SYMBOL(EnumFont)             
EXPORT_SYMBOL(SelScrFont)        
EXPORT_SYMBOL(SelPrnFont)        
EXPORT_SYMBOL(Soft_Reset)               
EXPORT_SYMBOL(SysSleep)             
EXPORT_SYMBOL(SysIdle)              
EXPORT_SYMBOL(GetTermInfoExt)       
EXPORT_SYMBOL(GetRandom)         
EXPORT_SYMBOL(SysConfig)            
EXPORT_SYMBOL(GetHardwareConfig)    
EXPORT_SYMBOL(UpperCase)
EXPORT_SYMBOL(GetAppId)
EXPORT_SYMBOL(GetUsClock)
EXPORT_SYMBOL(DelayMs)
EXPORT_SYMBOL(DelayUs)
EXPORT_SYMBOL(GetLcdFontDot)

// IcFun
EXPORT_SYMBOL(IccInit)              
EXPORT_SYMBOL(IccAutoResp)          
EXPORT_SYMBOL(IccClose)             
EXPORT_SYMBOL(IccIsoCommand)        
EXPORT_SYMBOL(IccDetect)            
EXPORT_SYMBOL(Mc_Clk_Enable)        
EXPORT_SYMBOL(Mc_Io_Dir)            
EXPORT_SYMBOL(Mc_Io_Read)           
EXPORT_SYMBOL(Mc_Io_Write)          
EXPORT_SYMBOL(Mc_Vcc)               
EXPORT_SYMBOL(Mc_Reset)             
EXPORT_SYMBOL(Mc_Clk)               
EXPORT_SYMBOL(Mc_C4_Write)          
EXPORT_SYMBOL(Mc_C8_Write)          
EXPORT_SYMBOL(Mc_C4_Read)           
EXPORT_SYMBOL(Mc_C8_Read)           
EXPORT_SYMBOL(Read_CardSlotInfo)    
EXPORT_SYMBOL(Write_CardSlotInfo)   

//PiccFun
EXPORT_SYMBOL(PiccOpen)             
EXPORT_SYMBOL(PiccSetup)            
EXPORT_SYMBOL(PiccClose)            
EXPORT_SYMBOL(PiccDetect)           
EXPORT_SYMBOL(PiccIsoCommand)       
EXPORT_SYMBOL(PiccRemove)           
EXPORT_SYMBOL(M1Authority)          
EXPORT_SYMBOL(M1ReadBlock)          
EXPORT_SYMBOL(M1WriteBlock)         
EXPORT_SYMBOL(M1Operate)            
EXPORT_SYMBOL(PiccLight)            
EXPORT_SYMBOL(PiccCmdExchange)      
EXPORT_SYMBOL(PiccInitFelica)       
EXPORT_SYMBOL(PiccManageReg)        

//MagFun
EXPORT_SYMBOL(MagOpen)              
EXPORT_SYMBOL(MagClose)             
EXPORT_SYMBOL(MagReset)             
EXPORT_SYMBOL(MagSwiped)            
EXPORT_SYMBOL(MagRead)              

//LcdFun
EXPORT_SYMBOL(ScrCls)               
EXPORT_SYMBOL(ScrClrLine)           
EXPORT_SYMBOL(ScrGray)              
EXPORT_SYMBOL(ScrBackLight)         
EXPORT_SYMBOL(ScrGotoxy )           
EXPORT_SYMBOL(ScrPlot)              
EXPORT_SYMBOL(ScrDrLogo)            
EXPORT_SYMBOL(ScrFontSet)           
EXPORT_SYMBOL(ScrAttrSet)           
EXPORT_SYMBOL(ScrSetIcon)           
EXPORT_SYMBOL(ScrPrint) 
EXPORT_SYMBOL(Lcdprintf)            
EXPORT_SYMBOL(ScrDrLogoxy)          
EXPORT_SYMBOL(ScrRestore)           
EXPORT_SYMBOL(ScrDrawBox)           
EXPORT_SYMBOL(SetLightTime)         
EXPORT_SYMBOL(ScrGotoxyEx)          
EXPORT_SYMBOL(ScrGetxyEx)           
EXPORT_SYMBOL(ScrDrawRect)          
EXPORT_SYMBOL(ScrClrRect)           
EXPORT_SYMBOL(ScrSpaceSet)          
EXPORT_SYMBOL(ScrGetLcdSize)        
EXPORT_SYMBOL(ScrTextOut)           
EXPORT_SYMBOL(ScrSetOutput)         
EXPORT_SYMBOL(ScrSetEcho)           
EXPORT_SYMBOL(ScrGetBackLight)

//KeyboardFun
EXPORT_SYMBOL(kbhit)                
EXPORT_SYMBOL(_getkey)              
EXPORT_SYMBOL(getkey)               
EXPORT_SYMBOL(kbflush)              
EXPORT_SYMBOL(GetString)            
EXPORT_SYMBOL(kbmute)               
EXPORT_SYMBOL(kblight)              
EXPORT_SYMBOL(KbReleased)
//CommFun
EXPORT_SYMBOL(ModemDial)            
EXPORT_SYMBOL(ModemCheck)           
EXPORT_SYMBOL(ModemTxd)             
EXPORT_SYMBOL(ModemRxd)             
EXPORT_SYMBOL(ModemReset)           
EXPORT_SYMBOL(ModemAsyncGet)        
EXPORT_SYMBOL(OnHook)               
EXPORT_SYMBOL(s_ModemInfo)          
EXPORT_SYMBOL(ModemExCommand)       
EXPORT_SYMBOL(PortOpen)             
EXPORT_SYMBOL(PortClose)            
EXPORT_SYMBOL(PortSend)             
EXPORT_SYMBOL(PortRecv)             
EXPORT_SYMBOL(PortRecvs)            
EXPORT_SYMBOL(PortReset)            
EXPORT_SYMBOL(PortTxPoolCheck)      
EXPORT_SYMBOL(PortSends)            
EXPORT_SYMBOL(PortPeep)             
EXPORT_SYMBOL(SetHeartBeat)         

//PrintFun
EXPORT_SYMBOL(PrnInit)              
EXPORT_SYMBOL(PrnFontSet)           
EXPORT_SYMBOL(PrnSpaceSet)          
EXPORT_SYMBOL(PrnLeftIndent)        
EXPORT_SYMBOL(PrnStep)              
EXPORT_SYMBOL(PrnLogo)              
EXPORT_SYMBOL(PrnStr)               
EXPORT_SYMBOL(PrnStart)             
EXPORT_SYMBOL(PrnStatus)            
EXPORT_SYMBOL(PrnGetDotLine)        
EXPORT_SYMBOL(PrnSetGray)           
EXPORT_SYMBOL(PrnGetFontDot)        
EXPORT_SYMBOL(PrnTemperature)       
EXPORT_SYMBOL(PrnDoubleWidth)       
EXPORT_SYMBOL(PrnDoubleHeight)      
EXPORT_SYMBOL(PrnAttrSet)           
EXPORT_SYMBOL(PrnPreFeedSet)           

//FileFun
EXPORT_SYMBOL(open)                 
EXPORT_SYMBOL(read)                 
EXPORT_SYMBOL(write)                
EXPORT_SYMBOL(close)                
EXPORT_SYMBOL(seek)                 
EXPORT_SYMBOL(remove)               
EXPORT_SYMBOL(filesize)             
EXPORT_SYMBOL(freesize)             
EXPORT_SYMBOL(truncate)             
EXPORT_SYMBOL(fexist)               
EXPORT_SYMBOL(GetFileInfo)
EXPORT_SYMBOL(s_SearchFile)
EXPORT_SYMBOL(s_RenameFile)
EXPORT_SYMBOL(s_filesize)
EXPORT_SYMBOL(LoadFontLib)
EXPORT_SYMBOL(s_removeId)
EXPORT_SYMBOL(s_remove)

EXPORT_SYMBOL(ex_open)   
EXPORT_SYMBOL(s_open)  
EXPORT_SYMBOL(FileToApp)            
EXPORT_SYMBOL(FileToParam)          
EXPORT_SYMBOL(FileToFont)           
EXPORT_SYMBOL(FileToMonitor)        
EXPORT_SYMBOL(FileToDriver)
EXPORT_SYMBOL(DelAppFile)           
EXPORT_SYMBOL(DelFontFile)          
EXPORT_SYMBOL(FileToPuk)            
EXPORT_SYMBOL(FsRecycle)            
EXPORT_SYMBOL(tell)   

//OtherFunc
EXPORT_SYMBOL(GetEnv)               
EXPORT_SYMBOL(PutEnv)               
EXPORT_SYMBOL(des)                  
EXPORT_SYMBOL(Hash)                 
EXPORT_SYMBOL(ReadAppInfo)          
EXPORT_SYMBOL(ReadAppState)         
EXPORT_SYMBOL(RunApp)               
EXPORT_SYMBOL(DoEvent)              
EXPORT_SYMBOL(CheckIfManageExist)   
EXPORT_SYMBOL(s_GetMatrixDot)       
EXPORT_SYMBOL(s_iVerifySignature)
             

//LineNetFunc
EXPORT_SYMBOL(NetSocket)            
EXPORT_SYMBOL(NetBind)              
EXPORT_SYMBOL(NetConnect)           
EXPORT_SYMBOL(NetListen)            
EXPORT_SYMBOL(NetAccept)            
EXPORT_SYMBOL(NetSend)              
EXPORT_SYMBOL(NetSendto)            
EXPORT_SYMBOL(NetRecv)              
EXPORT_SYMBOL(NetRecvfrom)          
EXPORT_SYMBOL(NetCloseSocket)       
EXPORT_SYMBOL(Netioctl)             
EXPORT_SYMBOL(SockAddrSet)          
EXPORT_SYMBOL(SockAddrGet)          
EXPORT_SYMBOL(DhcpStart)            
EXPORT_SYMBOL(DhcpStop)             
EXPORT_SYMBOL(DhcpCheck)            
EXPORT_SYMBOL(PPPLogin)             
EXPORT_SYMBOL(PPPLogout)            
EXPORT_SYMBOL(PPPCheck)             
EXPORT_SYMBOL(eth_mac_get)            
EXPORT_SYMBOL(EthSet)               
EXPORT_SYMBOL(EthSetRateDuplexMode) 
EXPORT_SYMBOL(DnsResolve)           
EXPORT_SYMBOL(DnsResolveExt)
EXPORT_SYMBOL(NetPing)              
EXPORT_SYMBOL(RouteSetDefault)      
EXPORT_SYMBOL(RouteGetDefault)      
EXPORT_SYMBOL(NetDevGet)            
EXPORT_SYMBOL(NetAddStaticArp)      
EXPORT_SYMBOL(NetSetIcmp)           
EXPORT_SYMBOL(PppoeLogin)           
EXPORT_SYMBOL(PppoeLogout)          
EXPORT_SYMBOL(PppoeCheck)           
EXPORT_SYMBOL(EthGetFirstRouteMac)  

//FATFun
EXPORT_SYMBOL(FsOpen)               
EXPORT_SYMBOL(FsClose)              
EXPORT_SYMBOL(FsDelete)             
EXPORT_SYMBOL(FsGetInfo)            
EXPORT_SYMBOL(FsRename)             
EXPORT_SYMBOL(FsDirSeek)            
EXPORT_SYMBOL(FsDirRead)            
EXPORT_SYMBOL(FsFileRead)           
EXPORT_SYMBOL(FsFileWrite)          
EXPORT_SYMBOL(FsFileSeek)           
EXPORT_SYMBOL(FsFileTell)           
EXPORT_SYMBOL(FsFileTruncate)       
EXPORT_SYMBOL(FsSetWorkDir)         
EXPORT_SYMBOL(FsGetWorkDir)         
EXPORT_SYMBOL(FsUdiskIn)            
EXPORT_SYMBOL(FsGetDiskInfo)        

//ExWNetFun
EXPORT_SYMBOL(WlInit)               
EXPORT_SYMBOL(WlGetSignal)          
EXPORT_SYMBOL(WlPppLogin)           
EXPORT_SYMBOL(WlPppLogout )         
EXPORT_SYMBOL(WlPppCheck)           
EXPORT_SYMBOL(WlOpenPort )          
EXPORT_SYMBOL(WlClosePort)          
EXPORT_SYMBOL(WlSendCmd)            
EXPORT_SYMBOL(WlSwitchPower)        
EXPORT_SYMBOL(WlSelSim)             
EXPORT_SYMBOL(WlAutoStart)          
EXPORT_SYMBOL(WlAutoCheck)          
EXPORT_SYMBOL(WlPppLoginEx) 

/*internal APIs*/
EXPORT_SYMBOL(GetCallAppInfo)
EXPORT_SYMBOL(SoMemSwitch)
EXPORT_SYMBOL(CfgSoMem)
EXPORT_SYMBOL(GetAppInfoAddr)

/*multitask*/
EXPORT_SYMBOL(OsCreate)
EXPORT_SYMBOL(OsSuspend)
EXPORT_SYMBOL(OsResume)
EXPORT_SYMBOL(OsSleep)
EXPORT_SYMBOL(OsStop)
EXPORT_SYMBOL(OsStart)

EXPORT_SYMBOL(irq_restore_asm)
EXPORT_SYMBOL(irq_save_asm)
EXPORT_SYMBOL(ClearIDCache)
EXPORT_SYMBOL(MpConvertToSo)
EXPORT_SYMBOL(HexToAscii)
EXPORT_SYMBOL(CheckCPUfrequency)
EXPORT_SYMBOL(GetUseAreaCode)
EXPORT_SYMBOL(AsciiToHex)
EXPORT_SYMBOL(get_term_name)
EXPORT_SYMBOL(kbsound)
EXPORT_SYMBOL(get_kblightmode)
EXPORT_SYMBOL(get_scrbacklightmode)
EXPORT_SYMBOL(kbcheck)
EXPORT_SYMBOL(putkey)
EXPORT_SYMBOL(ReadMacAddr)
EXPORT_SYMBOL(SysParaRead)
EXPORT_SYMBOL(SysParaWrite)
EXPORT_SYMBOL(EthGet)
EXPORT_SYMBOL(eth_mac_set)
EXPORT_SYMBOL(NetGetVersion)
EXPORT_SYMBOL(s_PrnStop)
EXPORT_SYMBOL(ip_forcesyn)
EXPORT_SYMBOL(CheckUpSensor)
EXPORT_SYMBOL(GetSensorSW)
EXPORT_SYMBOL(ShowSensorInfo)
EXPORT_SYMBOL(CreateFileSys)
EXPORT_SYMBOL(s_read)
EXPORT_SYMBOL(k_write)
EXPORT_SYMBOL(s_seek)
EXPORT_SYMBOL(GetFileLen)
EXPORT_SYMBOL(WriteMonitor)
EXPORT_SYMBOL(s_iDeleteAppParaFile)
EXPORT_SYMBOL(iGetInput)
EXPORT_SYMBOL(LocalDload)
EXPORT_SYMBOL(PEDManage)
EXPORT_SYMBOL(ModuleUpdate)
EXPORT_SYMBOL(XmlGetElement)
EXPORT_SYMBOL(XmlDelElement)
EXPORT_SYMBOL(CopySW)
EXPORT_SYMBOL(WaitKeyBExit)
EXPORT_SYMBOL(XmlAddElement)
EXPORT_SYMBOL(GetCharSetName)
EXPORT_SYMBOL(s_GetFontVer)
EXPORT_SYMBOL(s_GetCurScrFont)
EXPORT_SYMBOL(ReadTUSN)
EXPORT_SYMBOL(is_chn_font)
EXPORT_SYMBOL(s_GetSPrnFontDot)
EXPORT_SYMBOL(s_LcdSetDot)
EXPORT_SYMBOL(s_ScrRect)
EXPORT_SYMBOL(s_ScrWriteBitmap)
EXPORT_SYMBOL(s_LcdDrawLine)
EXPORT_SYMBOL(s_ScrLightCtrl)
EXPORT_SYMBOL(s_LcdGetDot)
EXPORT_SYMBOL(s_LcdClear)
EXPORT_SYMBOL(s_ScrGetLcdBufAddr)
EXPORT_SYMBOL(s_LcdDrawIcon)
EXPORT_SYMBOL(s_ScrFillRect)
EXPORT_SYMBOL(GetLcdInfo)
EXPORT_SYMBOL(GetScrCurAttr)
EXPORT_SYMBOL(ScrSpaceGet)
EXPORT_SYMBOL(s_Lcdprintf)
EXPORT_SYMBOL(s_ScrPrint)
EXPORT_SYMBOL(CLcdDrawPixel)
EXPORT_SYMBOL(s_CLcdPrint)
EXPORT_SYMBOL(CLcdBgDrawBox)
EXPORT_SYMBOL(CLcdGetInfo)
EXPORT_SYMBOL(CLcdPrint)
EXPORT_SYMBOL(CLcdDrawRect)
EXPORT_SYMBOL(CLcdTextOut)
EXPORT_SYMBOL(CLcdSetFgColor)
EXPORT_SYMBOL(CLcdClrLine)
EXPORT_SYMBOL(CLcdClrRect)
EXPORT_SYMBOL(CLcdSetBgColor)
EXPORT_SYMBOL(CLcdGetPixel)
EXPORT_SYMBOL(s_ScrGetLcdArea)
EXPORT_SYMBOL(s_CLcdTextOut)
EXPORT_SYMBOL(CLcdWriteBitmap)
EXPORT_SYMBOL(CLcdReadBitmap)
EXPORT_SYMBOL(s_GetLcdFontCharAttr)
//EXPORT_SYMBOL(RSAPrivateDecrypt)
//EXPORT_SYMBOL(RSAPublicEncrypt)
EXPORT_SYMBOL(RSAPrivateEncrypt)
EXPORT_SYMBOL(RSARecover)
EXPORT_SYMBOL(RSAPublicDecrypt)
EXPORT_SYMBOL(sha256)
EXPORT_SYMBOL(MsrVerifyKey)
EXPORT_SYMBOL(MsrReadSn)
EXPORT_SYMBOL(MsrReadKeyStatus)
EXPORT_SYMBOL(MsrUpdateKey)
EXPORT_SYMBOL(MsrInitKey)
EXPORT_SYMBOL(MsrWriteSn)
EXPORT_SYMBOL(DeleteApp)
EXPORT_SYMBOL(CalcAppSize)
EXPORT_SYMBOL(CheckFirst)
EXPORT_SYMBOL(app_even_start)
EXPORT_SYMBOL(Decompress)
EXPORT_SYMBOL(WlCheckSim)
EXPORT_SYMBOL(WlTcpRetryNum)
EXPORT_SYMBOL(WlModelComUpdate)
EXPORT_SYMBOL(WlSetCtrl)
EXPORT_SYMBOL(WlPowerOff)
EXPORT_SYMBOL(WlSetTcpDetect)
EXPORT_SYMBOL(GetWlFirmwareVer)
EXPORT_SYMBOL(WlGetModuleInfo)
EXPORT_SYMBOL(WlComExchange)

EXPORT_SYMBOL(FsDirDelete)
EXPORT_SYMBOL(FsDirOpen)
EXPORT_SYMBOL(FsSetAttr)
EXPORT_SYMBOL(IcmpReply)
EXPORT_SYMBOL(GetLeanAngle)
EXPORT_SYMBOL(SoundPlay)
EXPORT_SYMBOL(TouchScreenAttrSet)
EXPORT_SYMBOL(TouchScreenFlush)
EXPORT_SYMBOL(TouchScreenOpen)
EXPORT_SYMBOL(TouchScreenRead)
EXPORT_SYMBOL(TouchScreenClose)
EXPORT_SYMBOL(DrawIcon)
EXPORT_SYMBOL(ScanOpen)
EXPORT_SYMBOL(ScanClose)
EXPORT_SYMBOL(ScannerReset)
EXPORT_SYMBOL(ScanRead)

EXPORT_SYMBOL(SCR_PRINT)
EXPORT_SYMBOL(s_Get10MsTimerCount)
EXPORT_SYMBOL(s_iDeleteApp)
EXPORT_SYMBOL(WriteSecRam)
EXPORT_SYMBOL(ReadSecRam)
EXPORT_SYMBOL(GetKeyMs)
EXPORT_SYMBOL(CheckIfDevelopPos)

//EXPORT_SYMBOL(BtOpen)
//EXPORT_SYMBOL(BtClose)

#if 0 //deleted by shics
EXPORT_SYMBOL(BtOpen)
EXPORT_SYMBOL(BtClose)
EXPORT_SYMBOL(BtGetConfig)
EXPORT_SYMBOL(BtSetConfig)
EXPORT_SYMBOL(BtScan)
EXPORT_SYMBOL(BtConnect)
EXPORT_SYMBOL(BtDisconnect)
EXPORT_SYMBOL(BtGetStatus)
EXPORT_SYMBOL(BtCtrl)
#endif
//
EXPORT_SYMBOL(WifiOpen)
EXPORT_SYMBOL(WifiClose)
EXPORT_SYMBOL(WifiScan)
EXPORT_SYMBOL(WifiConnect)
EXPORT_SYMBOL(WifiDisconnect)
EXPORT_SYMBOL(WifiCheck)
EXPORT_SYMBOL(WifiCtrl)

/* export for SM */
int SM2GenKeyPair(void);
int SM2Sign(void);
int SM2Verify(void);
int SM2Recover(void);
int SM3(void);
int SM4(void);

EXPORT_SYMBOL(SM2GenKeyPair)
EXPORT_SYMBOL(SM2Sign)
EXPORT_SYMBOL(SM2Verify)
EXPORT_SYMBOL(SM2Recover)
EXPORT_SYMBOL(SM3)
EXPORT_SYMBOL(SM4)


