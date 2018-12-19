#ifndef PN512_REGS_H
#define PN512_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/********************************** PN512 Register ******************************************/

#define PN512_PAGE_REG           0x00   //Selects the register page
#define PN512_COMMAND_REG        0x01   //Starts and stops command execution
#define PN512_COMMIEN_REG        0x02   //Controls bits to enable and disable the passing of interrupt requests
#define PN512_DIVIEN_REG         0x03   //Controls bits to enable and disable the passing of interrupt requests
#define PN512_COMMIRQ_REG        0x04   //Contains Interrupt Request bits
#define PN512_DIVIRQ_REG         0x05   //Contains Interrupt Request bits
#define PN512_ERROR_REG          0x06   //Error bits showing the error status of the last command executed
#define PN512_STATUS1_REG        0x07   //Contains status bits for communication
#define PN512_STATUS2_REG        0x08   //Contains status bits of the receiver and transmitter
#define PN512_FIFODATA_REG       0x09   //n- and output of 64 byte FIFO buffer
#define PN512_FIFOLEVEL_REG      0x0A   //Indicates the number of bytes stored in the FIFO
#define PN512_WATERLEVEL_REG     0x0B   //Defines the level for FIFO under- and overflow warning
#define PN512_CONTROL_REG        0x0C   //Contains miscellaneous Control Register
#define PN512_BITFRAMING_REG     0x0D   //Adjustments for bit oriented frames
#define PN512_COLL_REG           0x0E   //Bit position fo the first bit collision detected on the RF-interface

#define PN512_MODE_REG           0x11   // Defines general modes for the transmitting and receiving
#define PN512_TXMODE_REG         0x12   // Defines the data rate and framing during transmission
#define PN512_RXMODE_REG         0x13   // Defines the data rate and framing during receiving
#define PN512_TXCONTROL_REG      0x14   // Controls the logical behavior of the antenna driver pins TX1 and TX2
#define PN512_TXAUTO_REG         0x15   // Controls the setting of the antenna drivers
#define PN512_TXSEL_REG          0x16   // Selects the internal sources for the antenna driver
#define PN512_RXSEL_REG          0x17   // Selects internal receiver settings
#define PN512_RXTHRESHOLD_REG    0x18   // Selects thresholds for the bit decoder
#define PN512_DEMOD_REG          0x19   // Defines demodulator settings
#define PN512_FELNFC1_REG        0x1A   // Defines the length of the valid range for the receive package
#define PN512_FELNFC2_REG        0x1B   // Defines the length of the valid range for the receive package
#define PN512_MIFNFC_REG         0x1C   // Controls the communication in ISO 14443/Mifare and NFC target mode at 106 kbit
#define PN512_MANUALRCV_REG      0x1D   // Allows manual fine tuning of the internal receiver
#define PN512_TYPEB_REG          0x1E   // Configure the ISO 14443 type B
#define PN512_SERIALSPEED_REG    0x1F   // Selects the speed of the serial UART interface

#define PN512_CRCRESULTMSB_REG   0x21   //  Shows the actual MSB and LSB values of the CRC calculation
#define PN512_CRCRESULTLSB_REG   0x22   // 
#define PN512_GSNOFF_REG         0x23   // Selects the conductance of the antenna driver pins TX1 and TX2 for modulation, when the driver is switched off
#define PN512_MODWIDTH_REG       0x24   // Controls the setting of the ModWidth
#define PN512_TXBITPHASE_REG     0x25   // Adjust the TX bit phase at 106 kbit
#define PN512_RFCFG_REG          0x26   // Configure the receiver gain and RF level
#define PN512_GSNON_REG          0x27   // Selects the conductance of the antenna driver pins TX1 and TX2 for modulation when the drivers are switched on
#define PN512_CWGSP_REG          0x28   // Selects the conductance of the antenna driver pins TX1 and TX2 for modulation during times of no modulation
#define PN512_MODGSP_REG         0x29   // Selects the conductance of the antenna driver pins TX1 and TX2 for modulation during modulation
#define PN512_TMODE_REG          0x2A   //Defines settings for the internal timers
#define PN512_TPRESCALER_REG     0x2B   //
#define PN512_TRELOAD_H_REG      0x2C   // Describle the 16-bit timer reload value
#define PN512_TRELOAD_L_REG      0x2D
#define PN512_TCOUNTERVAL_H_REG  0x2E   // Shows the 16-bit actual timer value
#define PN512_TCOUNTERVAL_L_REG  0x2F

#define PN512_TESTSEL1_REG       0x31   // General test signal configuration
#define PN512_TESTSEL2_REG       0x32   // General test signal configuration and PRBS control
#define PN512_TESTPINEN_REG      0x33   // Enable pin output driver on 8-bit parallel bus
#define PN512_TESTPINVALUE_REG   0x34   // Defines the values for the 8-bit parallel bus when it is used as I/O bus
#define PN512_TESTBUS_REG        0x35   // Shows the status of the internal testbus
#define PN512_AUTOTEST_REG       0x36   // Controls the digital selftest
#define PN512_VERSION_REG        0x37   // Shows the version
#define PN512_ANALOGTEST_REG     0x38   // Controls the pins AUX1 and AUX2
#define PN512_TESTDAC1_REG       0x39   // Defines the test value for the TestDAC1
#define PN512_TESTDAC2_REG       0x3A   // Defines the test value for the TestDAC2
#define PN512_TESTADC_REG        0x3B   // Shows the actual value of ADC I and Q


/********************************** PN512 Command *******************************************/

#define PN512_IDLE_CMD           0x00   // No action; cancels current command execution
#define PN512_CONFIG_CMD         0x01   // Configures the PN512 for FeliCa, Mifare and NFCIP-1 communication
#define PN512_GRAMID_CMD         0x02   // Generates a 10 byte random ID number
#define PN512_CALCRC_CMD         0x03   // Activates the CRC co-processor or performs a selftest
#define PN512_TRANSMIT_CMD       0x04   // Transmits data from the FIFO buffer
#define PN512_NOCMDCHANGE_CMD    0x07   // No command change.this command can be used to modify different bits
                                       // in the command register without touching the command .E.G. Power down bit
#define PN512_RECEIVE_CMD        0x08   // Activates the receiver circuitry
#define PN512_TRANSCEIVE_CMD     0x0C   // Transmits data from FIFO buffer and automatically activates the receiver after transmission is finished
#define PN512_AUTOCOLL_CMD       0x0D   // Handles FeliCa polling(Card Operation mode only) and Mifare anticollision(Card Operation mode only)
#define PN512_MFAUTHENT_CMD      0x0E   // Performs the Mifare standard authentication in Mifare Reader/Writer mode only
#define PN512_SOFTRESET_CMD      0x0F   // Resets the PN512





#define PN512_FIFO_SIZE          (64)
#define PN512_FIFO_WATERLEVEL    (16)

/********************************* PN512 Register Bit define ********************************/

#define RN512_BIT_TIMERUNNING    0x08
#define PN512_BIT_CRYPTO1ON      0x08
#define PN512_BIT_FLUSHBUFFER    0x80
#define PN512_BIT_WATERLEVELVAL  0x10
#define PN512_BIT_IRQ            0x10
#define PN512_BIT_START_SEND     0x80
#define PN512_BIT_MFCRYPTO1ON    0x08

/******* define Error state ************/
#define PN512_ERROR_TIMEOUT        0x20
#define PN512_ERROR_BUFFEROVFL     0x10
#define PN512_ERROR_COLLERR        0x08
#define PN512_ERROR_CRCERR         0x04
#define PN512_ERROR_PARITYERR      0x02
#define PN512_ERROR_PROTOCOLERR    0x01


/******* define CommIEnReg Bit *********/
#define PN512_BIT_IRQINV         0x80
#define PN512_BIT_TXIEN          0x40
#define PN512_BIT_RXIEN          0x20
#define PN512_BIT_IDLEIEN        0x10
#define PN512_BIT_HIALERTIEN      0x08
#define PN512_BIT_LOALERTIEN     0x04
#define PN512_BIT_ERRIEN         0x02
#define PN512_BIT_TIMERIEN       0x01

/******* define CommIRqReg Bit ********/
#define PN512_BIT_TXIRQ          0x40
#define PN512_BIT_RXIRQ          0x20
#define PN512_BIT_IDLEIRQ        0x10
#define PN512_BIT_HIALERTIRQ     0x08
#define PN512_BIT_LOALERTIRQ     0x04
#define PN512_BIT_ERRIRQ         0x02
#define PN512_BIT_TIMERIRQ       0x01

/******* define DivIEnReg Bit **********/
#define PN512_BIT_IRQPUSHPULL    0x80
#define PN512_BIT_SIGINACTIEN    0x10
#define PN512_BIT_MODEIEN        0x08
#define PN512_BIT_CRCIEN         0x04
#define PN512_BIT_RFONIEN        0x02
#define PN512_BIT_RFOFFIEN       0x01

/******* define DivIRqReg Bit **********/
#define PN512_BIT_SIGINACTIRQ    0x10
#define PN512_BIT_MODEIRQ        0x08
#define PN512_BIT_CRCIRQ         0x04
#define PN512_BIT_RFONIRQ        0x02
#define PN512_BIT_RFOFFIRQ       0x01


	


#ifdef __cplusplus
}
#endif

#endif
