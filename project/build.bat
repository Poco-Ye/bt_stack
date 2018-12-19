
set COMPILER_TOOLKIT=GCC
set GCC_BIN_PATH=C:\Progra~1\GNUARM.410\arm-elf\bin
set GCC_INC_PATH=C:\Progra~1\GNUARM.410\arm-elf\include -I C:\Progra~1\GNUARM.410\lib\gcc\arm-elf\4.1.0\include
set GCC_LIB_PATH=C:\Progra~1\GNUARM.410\arm-elf\lib -L C:\Progra~1\GNUARM.410\lib\gcc\arm-elf\4.1.0
set PATH=c:\make381;C:\Program Files\GNUARM.410\bin;C:\Program Files\GNUARM.410\arm-elf\bin;C:\Program Files\GNUARM.410\libexec\gcc\arm-elf\4.1.0

set MONITOR_VERSION=3.35
del ..\code\inc\version.h
echo>>..\code\inc\version.h #ifndef _VERSION_H_
echo>>..\code\inc\version.h #define _VERSION_H_
echo>>..\code\inc\version.h #define MONI_VERSION "%MONITOR_VERSION%"
echo>>..\code\inc\version.h #endif

set BT_COMM=true
set BLUEDROID=true 
set RFCOMM=true 
set SDP_CLIENT=true 
set SDP_SERVER=true 
set SDP_BLE=true
set GATT_DEVICE_INFO=true 
set THIRD_PARTY=true 
set EMBEDDED_OS=true
set CHIPSET_RTK=true
set KEY_DB=true
set TEST_CASE=true

del obj\rtm.o obj\dtd.o
set TARGET=SxxxMonitor_Debug
set aFile=%TARGET%_%date:~0,4%%DATE:~5,2%%DATE:~8,2%(v%MONITOR_VERSION%).bin
make -s %1 all
BcmIamgeBuild bin\%TARGET%.bin bin\%TARGET%_raw.bin 40300000
copy bin\%TARGET%.bin bin\%aFile%

objdump -d bin\%TARGET%_raw.elf >bin\asm1.txt
objdump -D bin\%TARGET%_raw.elf >bin\asm2.txt
objdump -t bin\%TARGET%_raw.elf >bin\sys.txt

pause
