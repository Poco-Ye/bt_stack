# Output file basename
OUTPUT = $(TARGET)_raw

BIN = bin
OBJ = obj
SRC =../code/src
LIB_PATH =../code/lib
CROSS = arm-elf-

# Compilation tools
CC = $(CROSS)gcc 
SIZE = $(CROSS)size
STRIP = $(CROSS)strip
OBJCOPY = $(CROSS)objcopy

AP6181WL_FLAG += -I../code/src/wifi/ap6181/include
AP6181WL_FLAG += -I../code/src/wifi/ap6181/arch/include

# FOR BT STACK #Added by SHICS
BTSTACK_PATH = ../code/src/btstack/src/
BTSTACK_FLAG = -I$(BTSTACK_PATH) -I$(BTSTACK_PATH)/classic/ -I$(BTSTACK_PATH)/ble/ -I$(BTSTACK_PATH)/ble/gatt-service/ 
BTSTACK_FLAG += -I$(BTSTACK_PATH)/../3rd-party/
BTSTACK_FLAG += -I$(BTSTACK_PATH)/../platform/embedded/
BTSTACK_FLAG += -I$(BTSTACK_PATH)/../chipset/bcm/
BTSTACK_FLAG += -I$(BTSTACK_PATH)/../key-db/

CFLAGS += -fsigned-char -mlittle-endian -g -O2 -c -I ../code/inc -I../code/src/ipstack/include 

CFLAGS += $(AP6181WL_FLAG)
CFLAGS += $(BTSTACK_FLAG)


LIBS   += C:/Progra~1/GNUARM.410/arm-elf/lib/libc.a C:/Progra~1/GNUARM.410/lib/gcc/arm-elf/4.1.0/libgcc.a 

#LIBS   += $(LIB_PATH)/libpedSSBCM.a
#LIBS   += $(LIB_PATH)/libsslPTS_Release.a
LIBS   += $(LIB_PATH)/libIconSxxx.a
LIBS   += $(LIB_PATH)/libsm_SF.a

# TODO: for WLT bluetooth
#deleted by shics
LFLAGS  = -Wl,-TSxxMonitor.ld,-Map,bin/$(TARGET).map,--cref -nostartfiles 

#project file directory
VPATH += $(wildcard $(SRC)/*) $(wildcard $(SRC)/*/*) $(wildcard $(SRC)/*/*/*) $(wildcard $(SRC)/*/*/*/*)
 
ASM_OBJECTS = init.o 

C_OBJECTS  =  paxos.o osmem.o scheduletimer.o mpatch.o
C_OBJECTS  +=  base.o system.o swi.o comm.o timer.o io_manager.o  so_manage.o export_symbol.o
C_OBJECTS  += main.o  kb.o kb_touch.o kbi2c.o beep.o adc.o dll.o SysPara.o sys_combin_api.o
C_OBJECTS  +=  bcm5892-bbl.o bcm5892-spi.o  rtc.o #testcase.o

C_OBJECTS  +=  filedef.o filecore.o filepublic.o
C_OBJECTS  +=  menu.o  modulecheck.o ModuleMenu.o #ssloper.o
C_OBJECTS  +=  nand.o  umc_cpuapi.o sha1.o badblock.o
C_OBJECTS  +=  Asc.o font.o LcdDrv.o LcdApi.o LcdKd.o clcdapi.o clcdkd.o clcddrv.o
C_OBJECTS  +=  des.o digit.o r_stdlib.o r_random.o nn.o md5c.o  CRC.o rsa.o HASH.o sha256.o bignum.o xy_rsa.o aes.o
C_OBJECTS  +=  puk.o
C_OBJECTS  +=  magcard.o enmagrd.o
C_OBJECTS  +=  AppManage.o download.o  decompress.o duplicate.o uartdownload.o UsbHostDL.o
C_OBJECTS  +=  modem.o 
C_OBJECTS  +=  printer.o printer_hal_sii.o printer_hal_prt.o printer_stylus.o printerApi_stylus.o printer_stylus_bcm5892.o 
C_OBJECTS  +=  WlApi.o bcm5892_eth.o
C_OBJECTS  +=  fat.o fcach.o
C_OBJECTS  +=  usb_ftdi.o usbbulk.o usbdev.o usbhost.o usb_ohci.o usbbulk_ohci.o usbhcd.o
C_OBJECTS  +=  sdhci.o mmc.o
C_OBJECTS  +=  CfgLowlevel.o CfgManage.o

C_OBJECTS  +=  icc_hard_async.o icc_hard_sync.o icc_queue.o tda8026.o ncn8025.o icc_device_configure.o
C_OBJECTS  +=  icc_apis.o icc_core.o

C_OBJECTS  +=  bcm5892_rfbsp.o rc663_driver.o rc663_regs_conf.o pn512_driver.o pn512_regs_conf.o as3911_driver.o as3911_regs_conf.o mifare_encrypt.o
C_OBJECTS  +=  iso14443hw_hal.o iso14443_3a.o iso14443_3b.o iso14443_4.o mifare.o emvcl.o paxcl.o paypass.o pcd_apis.o
C_OBJECTS  +=  dmu_cpuapi.o PowerManage.o battery.o

#tcpip object
C_OBJECTS  +=  bcm5892_phy.o
C_OBJECTS  +=  gprs_ppp.o  modem_ppp.o ppp.o pppoe.o  mschap.o ppp_md5.o
C_OBJECTS  +=  cbc_enc.o des_enc.o ecb_enc.o md4.o sha1dgst.o set_key.o 
C_OBJECTS  +=  dev.o inet.o ip_addr.o ip_ver.o mem_pool.o netapi.o skbuff.o  socket.o
C_OBJECTS  +=  inet_timer.o
C_OBJECTS  +=  ethernet.o ip.o arp.o icmp.o tcp.o tcp_in.o tcp_out.o udp.o dns.o dhcpc.o

#for B210 ##//deleted by shics
C_OBJECTS  +=  base_common.o  handset.o 
C_OBJECTS  +=  base_comm_proxy.o base_lan_api.o  base_modem_proxy.o ohci_host_proxy.o

C_OBJECTS += i2c.o tiltsensor.o
C_OBJECTS += bcm5892_i2s.o audio.o queue.o
C_OBJECTS += tsc2046.o ts_hal.o ts_sample.o button.o
C_OBJECTS += icon.o
C_OBJECTS += barcode.o

# TODO: for D200 mfi ##//deleted by shics
#C_OBJECTS += mfi_cp.o
#C_OBJECTS += simI2C.o

#for wifi
C_OBJECTS += wlanapp.o 
C_OBJECTS += rpwifi.o rpwlanapp.o
C_OBJECTS += ap6181.o bcm5892-sdio.o 

C_OBJECTS += bcmsdh.o dhd_common.o dhd_osl_ext.o dhd_sdio.o pkt_lbuf.o bcmutils.o siutils.o wlu.o arch.o

#for pmu
C_OBJECTS += pmu.o pmui2c.o

# TODO: for gps
C_OBJECTS += minmea.o
C_OBJECTS += gps.o

#chip sm
C_OBJECTS += sm_chip_thk88.o

#for btstack  

ifdef BT_COMM
C_OBJECTS += ad_parser.o btstack_hid_parser.o btstack_linked_list.o btstack_memory.o btstack_memory_pool.o btstack_ring_buffer.o 
C_OBJECTS += btstack_run_loop.o btstack_slip.o btstack_tlv.o btstack_util.o hci.o hci_cmd.o hci_dump.o hci_transport_em9304_spi.o
C_OBJECTS += hci_transport_h4.o hci_transport_h5.o l2cap.o l2cap_signaling.o
endif

ifdef BLUEDROID   # CLASSIC
C_OBJECTS += btstack_cvsd_plc.o 
C_OBJECTS += btstack_link_key_db_memory.o btstack_link_key_db_static.o btstack_link_key_db_tlv.o 
C_OBJECTS += btstack_sbc_decoder_bluedroid.o btstack_sbc_encoder_bluedroid.o btstack_sbc_plc.o device_id_server.o 
endif

ifdef RFCOMM  # CLASSIC
C_OBJECTS += rfcomm.o 
endif

ifdef SDP_CLIENT    # CLASSIC
C_OBJECTS += sdp_client.o sdp_client_rfcomm.o
endif

ifdef SDP_SERVER  # CLASSIC
C_OBJECTS += sdp_server.o sdp_util.o spp_server.o
endif

ifdef SDP_BLE   # BLE
C_OBJECTS += ancs_client.o att_db.o att_db_util.o att_dispatch.o att_server.o gatt_client.o le_device_db_memory.o le_device_db_tlv.o sm.o
endif

ifdef GATT_DEVICE_INFO   # GATT
C_OBJECTS += device_information_service_server.o 
endif

ifdef THIRD_PARTY   #THIRD LIB
C_OBJECTS += sbc_analysis.o sbc_enc_bit_alloc_mono.o sbc_encoder.o sbc_dct.o sbc_enc_bit_alloc_ste.o sbc_packing.o sbc_dct_coeffs.o sbc_enc_coeffs.o
C_OBJECTS += alloc.o bitalloc.o bitalloc-sbc.o bitstream-decode.o decoder-oina.o decoder-private.o decoder-sbc.o dequant.o framing.o framing-sbc.o oi_codec_version.o synthesis-8-generated.o synthesis-dct8.o synthesis-sbc.o
endif

ifdef EMBEDDED_OS  #EMBEDDED OS
C_OBJECTS += btstack_run_loop_embedded.o hal_tick.o hal_cpu.o btstack_uart_block_embedded.o bcm5892_hal_uart.o
endif

ifdef CHIPSET_BCM  #CHIPSET_BCM
C_OBJECTS += btstack_chipset_bcm.o btstack_chipset_bcm_download_firmware.o
endif

ifdef KEY_DB   #KEY_DB
C_OBJECTS += btstack_link_key_db_fs.o
endif

ifdef TEST_CASE   #TEST_CASE
C_OBJECTS += test_main.o classic_test.o
endif



ifeq (Release,$(findstring Release,$(TARGET)))
C_OBJECTS += rtm.o
else
C_OBJECTS += dtd.o
endif

# Append OBJ and BIN directories to output filename
OUTPUT := $(BIN)/$(OUTPUT)

C_OBJECTS_DDR = $(addprefix $(OBJ)/, $(C_OBJECTS))
ASM_OBJECTS_DDR = $(addprefix $(OBJ)/, $(ASM_OBJECTS))

all: $(ASM_OBJECTS_DDR) $(C_OBJECTS_DDR)
#	echo $(CC) $(LFLAGS)  -o $(OUTPUT).elf $^ $(LIBS)
	echo start link....
	$(CC) $(LFLAGS)  -o $(OUTPUT).elf $^ $(LIBS)
	$(STRIP) --strip-debug  --remove-section=.comment $^
	$(STRIP) --strip-debug  --remove-section=.comment $(OUTPUT).elf
	$(OBJCOPY) -O binary $(OUTPUT).elf $(OUTPUT).bin


$(C_OBJECTS_DDR): $(OBJ)/%.o: %.c Makefile 
	echo CC $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(ASM_OBJECTS_DDR): $(OBJ)/%.o: %.S Makefile 
	echo CC $<
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f $(OBJ)/*.* $(BIN)/*.*
 
