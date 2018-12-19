# Microsoft Developer Studio Project File - Name="SxxMonitor" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=SxxMonitor - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SxxMonitor.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SxxMonitor.mak" CFG="SxxMonitor - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SxxMonitor - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "SxxMonitor - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "SxxMonitor - Win32 Release"

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f makefile"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "makefile.exe"
# PROP BASE Bsc_Name "makefile.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f makefile"
# PROP Rebuild_Opt "/a"
# PROP Target_File "SxxMonitor.exe"
# PROP Bsc_Name "SxxMonitor.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "SxxMonitor - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f makefile"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "makefile.exe"
# PROP BASE Bsc_Name "makefile.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "build"
# PROP Rebuild_Opt "-B"
# PROP Target_File "SxxMonitor.exe"
# PROP Bsc_Name "SxxMonitor.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "SxxMonitor - Win32 Release"
# Name "SxxMonitor - Win32 Debug"

!IF  "$(CFG)" == "SxxMonitor - Win32 Release"

!ELSEIF  "$(CFG)" == "SxxMonitor - Win32 Debug"

!ENDIF 

# Begin Group "AppManage"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\AppManage\AppManage.c
# End Source File
# End Group
# Begin Group "comm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\comm\comm.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\comm\comm.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\comm\netapi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\comm\socket.h
# End Source File
# End Group
# Begin Group "Decompress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\Decompress\decompress.c
# End Source File
# End Group
# Begin Group "conf"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\conf\dtd.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\conf\kmm.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\conf\rtm.c
# End Source File
# End Group
# Begin Group "download"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\download\download.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\duplicate.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\LocalDL.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\localdownload.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\RemoteDownloadLib.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\uartdownload.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\UsbHostDL.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\download\UsbHostDL.h
# End Source File
# End Group
# Begin Group "DLL"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\DLL\dll.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\DLL\dll.h
# End Source File
# End Group
# Begin Group "encrypt"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\encrypt\CRC.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\CRC.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\des.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\digit.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\digit.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\global.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\HASH.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\HASH.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\md2.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\md2c.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\md5.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\md5c.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\nn.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\nn.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\Private_Key.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\Public_Key.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\r_random.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\r_random.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\r_stdlib.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\rsa.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\rsa.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\rsaref.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\encrypt\TestEncrypt.c
# End Source File
# End Group
# Begin Group "file"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\file\filecore.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\file\filedef.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\file\filedef.h
# End Source File
# End Group
# Begin Group "Flash"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\Flash\nand.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Flash\nand.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Flash\umc_cpuapi.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Flash\umc_cpuapi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Flash\umc_reg.h
# End Source File
# End Group
# Begin Group "Font"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\Font\Asc.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Font\font.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Font\font.h
# End Source File
# End Group
# Begin Group "intr"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\intr\base.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\intr\init.S
# End Source File
# Begin Source File

SOURCE=..\..\code\src\intr\swi.c
# End Source File
# End Group
# Begin Group "kb"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\kb\kb.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\kb\kb.h
# End Source File
# End Group
# Begin Group "lcd"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\lcd\LcdApi.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\lcd\Lcdapi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\lcd\LcdDrv.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\lcd\LcdDrv.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\lcd\LcdKd.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\lcd\Lcdkd.h
# End Source File
# End Group
# Begin Group "MagCard"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\MagCard\enmagrd.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\MagCard\enmagrd.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\MagCard\magcard.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\MagCard\magcard.h
# End Source File
# End Group
# Begin Group "Mifre"

# PROP Default_Filter ""
# Begin Group "Driver"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\bcm5892_rfbsp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\bcm5892_rfbsp.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\phhalHw_Rc663_Reg.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\rc663_driver.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\rc663_driver.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\rc663_regs_conf.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\driver\rc663_regs_conf.h
# End Source File
# End Group
# Begin Group "protocol"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\emvcl.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\emvcl.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443_3a.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443_3a.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443_3b.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443_3b.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443_4.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443_4.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443hw_hal.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\iso14443hw_hal.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\mifare.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\mifare.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\paxcl.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\paxcl.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\paypass.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\paypass.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\pcd_apis.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\Mifre\protocol\pcd_apis.h
# End Source File
# End Group
# End Group
# Begin Group "modem"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\modem\modem.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\modem\modem.h
# End Source File
# End Group
# Begin Group "ModuleCheck"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ModuleCheck\modulecheck.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ModuleCheck\ModuleCheck.h
# End Source File
# End Group
# Begin Group "ped"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ped\Ped.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ped\PedApi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ped\PedDownload.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ped\PedDrv.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ped\PedDrv.h
# End Source File
# End Group
# Begin Group "printer"

# PROP Default_Filter ""
# Begin Group "stylus"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\printer\stylus\printer_stylus.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\printer\stylus\printer_stylus.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\printer\stylus\printer_stylus_test.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\printer\stylus\printerApi_stylus.c
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\code\src\printer\printer.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\printer\printer.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\printer\prns800.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\printer\prns800.h
# End Source File
# End Group
# Begin Group "PUK"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\PUK\puk.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\PUK\puk.h
# End Source File
# End Group
# Begin Group "SecRam"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\code\src\SecRam\bcm5892-bbl.c"
# End Source File
# Begin Source File

SOURCE="..\..\code\src\SecRam\bcm5892-bbl.h"
# End Source File
# Begin Source File

SOURCE=..\..\code\src\SecRam\rtc.c
# End Source File
# End Group
# Begin Group "system"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\system\bcmlib.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\io_manager.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\main.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\menu.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\sofTimer.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\sofTimer.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\system.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\system\timer.c
# End Source File
# End Group
# Begin Group "usb"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\usb\fsapi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\ftdi_sio.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\sl811.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\uficmd.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usb.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usb_ftdi.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usb_sys.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbbulk.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbbulk.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbdev.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbdev.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbfat.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbfat.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbfcach.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbfcach.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\usb\usbhost.c
# End Source File
# End Group
# Begin Group "wnet"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\wnet\fluid.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\fluid.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\fluidbin.inc
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\fluiddevices.inc
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\UPG_Q24.C
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\WlApi.C
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\WlApi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\xmodem.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\wnet\xmodem.h
# End Source File
# End Group
# Begin Group "Iccard"

# PROP Default_Filter ""
# Begin Group "hardware"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_config.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_hard_async.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_hard_async.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_hard_bcm5892.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_hard_sync.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_queue.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\icc_queue.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\ncn8025.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\hardware\ncn8025.h
# End Source File
# End Group
# Begin Group "protocol No. 1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\iccard\protocol\icc_apis.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\protocol\icc_apis.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\protocol\icc_core.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\protocol\icc_core.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\iccard\protocol\icc_errno.h
# End Source File
# End Group
# End Group
# Begin Group "ipstack"

# PROP Default_Filter ""
# Begin Group "ppp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\cbc_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\des_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\des_locl.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\des_ver.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\ecb_des.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\ecb_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\gprs_ppp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\md32_common.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\md4.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\md4.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\md5.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\modem_ppp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\mschap.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\mschap.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\ncbc_enc.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\ppp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\ppp.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\ppp_md5.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\pppoe.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\pppoe.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\rpc_des.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\set_key.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\sha.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\sha1dgst.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\sha_locl.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\spr.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\win_ppp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ppp\wnet_ppp.h
# End Source File
# End Group
# Begin Group "ipv4"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\arp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\dhcpc.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\dns.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\ethernet.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\icmp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\ip.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\tcp.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\tcp_in.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\tcp_out.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\ipv4\udp.c
# End Source File
# End Group
# Begin Group "include"

# PROP Default_Filter ""
# Begin Group "inet"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\arp.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\atomic.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\dev.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\dhcp.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\ethernet.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\icmp.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\if.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet_config.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet_irq.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet_list.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet_softirq.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet_timer.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\inet_type.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\ip.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\ip_addr.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\mem_pool.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\skbuff.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\tcp.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\inet\udp.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\arch.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\netapi.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\include\socket.h
# End Source File
# End Group
# Begin Group "eth_dev"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ipstack\eth_dev\bcm5892_phy.c
# End Source File
# End Group
# Begin Group "core"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\dev.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\inet.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\inet_timer.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\ip_addr.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\ip_ver.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\mem_pool.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\netapi.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\skbuff.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\ipstack\core\socket.c
# End Source File
# End Group
# End Group
# Begin Group "S60H"

# PROP Default_Filter ""
# Begin Group "IrDA"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDA\irda_hand_phy.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDA\irda_phy.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDA\IrDATimer.c
# End Source File
# End Group
# Begin Group "IrDAProtocol"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDAProtocol\irda_proto.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDAProtocol\irda_proto.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDAProtocol\irda_upper.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\IrDAProtocol\IrDaProtocol.H
# End Source File
# End Group
# Begin Group "Proxy"

# PROP Default_Filter ""
# Begin Group "comm_S60H"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\comm\comm_proxy.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\comm\comm_proxy.h
# End Source File
# End Group
# Begin Group "ip"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\ip\app_api.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\ip\app_api.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\ip\app_proxy.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\ip\app_proxy.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\ip\cmd_index.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\ip\cmd_index.h
# End Source File
# End Group
# Begin Group "modem_S60H"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\modem\modem_proxy.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\modem\modem_proxy.h
# End Source File
# End Group
# Begin Group "printer_S60H"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\printer\printer_proxy.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\printer\printer_proxy.h
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\Proxy\printer\printer_test.c
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=..\..\code\src\S60H\common_s60h.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\softirq.c
# End Source File
# Begin Source File

SOURCE=..\..\code\src\S60H\softirq.h
# End Source File
# End Group
# Begin Group "build"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\build.bat
# End Source File
# Begin Source File

SOURCE=..\makefile
# End Source File
# Begin Source File

SOURCE=..\SxxMonitor.ld
# End Source File
# End Group
# End Target
# End Project
