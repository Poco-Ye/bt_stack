#ifndef AS3911DRIVER_H
#define AS3911DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define EANBLE_CHANNEL_MASR             (0x80)
#define ENABLE_AM_CHANNEL               (0x00)
#define ENABLE_PM_CHANNEL               (0x80)

#define TYPE_A_COMMAND_REQA             (0x26)
#define TYPE_A_COMMAND_WUPA             (0x52)

#define AS3911_TX_TAG                   (0x00)
#define AS3911_RX_TAG                   (0x01)


#define AS3911_MASK_MAIN_INT            (0x00)
#define AS3911_MASK_TIME_NFC_INT        (0x01)
#define AS3911_MASK_ERROR_WAKE_INT      (0x02)

/* ingroup as3911IrqHandling */

#define AS3911_IRQ_MASK_ALL             (0xFF)     /* All AS3911 interrupt sources. */
#define AS3911_IRQ_MASK_NONE            (0x00)     /* No AS3911 interrupt source. */

/* Main interrupt register. */
#define AS3911_IRQ_MASK_OSC             (0x80)     /* AS3911 oscillator stable interrupt. */
#define AS3911_IRQ_MASK_WL              (0x40)     /* AS3911 FIFO water level inerrupt. */
#define AS3911_IRQ_MASK_RXS             (0x20)     /* AS3911 start of receive interrupt. */
#define AS3911_IRQ_MASK_RXE             (0x10)     /* AS3911 end of receive interrupt. */
#define AS3911_IRQ_MASK_TXE             (0x08)     /* AS3911 end of transmission interrupt. */
#define AS3911_IRQ_MASK_COL             (0x04)     /* AS3911 bit collision interrupt. */
	
#define AS3911_MASK_FIFO_OVERFLOW       (0x02)     /* AS3911 FiFo overflow */

/* Timer and NFC interrupt register. */
#define AS3911_IRQ_MASK_DCT             (0x80)     /* AS3911 termination of direct command interrupt. */
#define AS3911_IRQ_MASK_NRE             (0x40)     /* AS3911 no-response timer expired interrupt. */
#define AS3911_IRQ_MASK_GPE             (0x20)     /* AS3911 general purpose timer expired interrupt. */
#define AS3911_IRQ_MASK_EON             (0x10)     /* AS3911 external field on interrupt. */
#define AS3911_IRQ_MASK_EOF             (0x08)     /* AS3911 external field off interrupt. */
#define AS3911_IRQ_MASK_CAC             (0x04)     /* AS3911 collision during RF collision avoidance interrupt. */
#define AS3911_IRQ_MASK_CAT             (0x02)     /* AS3911 minimum guard time expired interrupt. */
#define AS3911_IRQ_MASK_NFCT            (0x01)     /* AS3911 initiator bit rate recognized interrupt. */

/* Error and wake-up interrupt register. */
#define AS3911_IRQ_MASK_CRC             (0x80)     /* AS3911 CRC error interrupt. */
#define AS3911_IRQ_MASK_PAR             (0x40)     /* AS3911 parity error interrupt. */
#define AS3911_IRQ_MASK_ERR2            (0x20)     /* AS3911 soft framing error interrupt. */
#define AS3911_IRQ_MASK_ERR1            (0x10)     /* AS3911 hard framing error interrupt. */
#define AS3911_IRQ_MASK_WT              (0x08)     /* AS3911 wake-up interrupt. */
#define AS3911_IRQ_MASK_WAM             (0x04)     /* AS3911 wake-up due to amplitude interrupt. */
#define AS3911_IRQ_MASK_WPH             (0x02)     /* AS3911 wake-up due to phase interrupt. */
#define AS3911_IRQ_MASK_WCAP            (0x01)     /* AS3911 wake-up due to capacitance measurement. */


/* Bitmask for the rxon bit of the auxiliary display register of the AS3911 */
#define AS3911_AUX_DISPLAY_RXON_BIT     (0x08)


int  As3911Init( struct ST_PCDINFO* );
int  As3911CarrierOn( struct ST_PCDINFO* );
int  As3911CarrierOff( struct ST_PCDINFO* );
int  As3911Config( struct ST_PCDINFO* );
int  As3911Waiter( struct ST_PCDINFO*, unsigned int );
int  As3911MifareAuthenticate( struct ST_PCDINFO* );
int  As3911Trans( struct ST_PCDINFO* );
int  As3911TransCeive( struct ST_PCDINFO* );
void As3911CommIsr( void );
int  As3911GetParamTagValue(PICC_PARA *ptParam, unsigned char *pucRfPara);
int  As3911SetParamValue(PICC_PARA *ptPiccPara );

struct ST_PCDOPS * GetAs3911OpsHandle();

#ifdef __cplusplus
}
#endif
#endif
