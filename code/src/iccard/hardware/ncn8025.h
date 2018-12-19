#ifndef _NCN8025_H
#define _NCN8025_H

#ifdef __cplusplus
extern "C" {
#endif

#include "icc_device_configure.h"

/* the macro is corresponding with the index of GPIOA */
#define 	NCN8025_IO         sci_device_resource.ncn8025_interface.io_port,sci_device_resource.ncn8025_interface.io_pin
#define 	NCN8025_AUX1       sci_device_resource.ncn8025_interface.aux1_port,sci_device_resource.ncn8025_interface.aux1_pin
#define 	NCN8025_AUX2       sci_device_resource.ncn8025_interface.aux2_port,sci_device_resource.ncn8025_interface.aux2_pin
#define 	NCN8025_RSTIN      sci_device_resource.ncn8025_interface.rstin_port,sci_device_resource.ncn8025_interface.rstin_pin
#define 	NCN8025_NCMDVCC    sci_device_resource.ncn8025_interface.ncmdvcc_port,sci_device_resource.ncn8025_interface.ncmdvcc_pin
#define 	NCN8025_VSEL0      sci_device_resource.ncn8025_interface.vsel0_port,sci_device_resource.ncn8025_interface.vsel0_pin
#define 	NCN8025_VSEL1      sci_device_resource.ncn8025_interface.vsel1_port,sci_device_resource.ncn8025_interface.vsel1_pin



#define SAM_RST      sci_device_resource.sam_interface.sam_rst_port,sci_device_resource.sam_interface.sam_rst_pin
#define SAM1_PWR     sci_device_resource.sam_interface.sam1_pwr_port,sci_device_resource.sam_interface.sam1_pwr_pin
#define SAM2_PWR     sci_device_resource.sam_interface.sam2_pwr_port,sci_device_resource.sam_interface.sam2_pwr_pin
#define SAM3_PWR     sci_device_resource.sam_interface.sam3_pwr_port,sci_device_resource.sam_interface.sam3_pwr_pin
#define SAM4_PWR     sci_device_resource.sam_interface.sam4_pwr_port,sci_device_resource.sam_interface.sam4_pwr_pin
#define SAM_SEL0     sci_device_resource.sam_interface.sam_sel0_port,sci_device_resource.sam_interface.sam_sel0_pin
#define SAM_SEL1     sci_device_resource.sam_interface.sam_sel1_port,sci_device_resource.sam_interface.sam_sel1_pin
#define SAM_SEL_EN   sci_device_resource.sam_interface.sam_sel_en_port,sci_device_resource.sam_interface.sam_sel_en_pin


	

#define		VCC_MODE_ON             ( 0x01 )
#define		VCC_MODE_OFF            ( 0x02 )
#define 	VCC_VAL_1V8             ( 0x03 )
#define 	VCC_VAL_3V              ( 0x04 )
#define 	VCC_VAL_5V              ( 0x05 )

/* asyncronous card interface by ncn8025 */
int sci_card_xmit_setting_ncn8025(int devch);
void sci_card_pin_control_ncn8025(unsigned int pin, unsigned int val);
int  sci_card_cold_reset_ncn8025(int devch, int voltage, int reset_timing);
int  sci_card_warm_reset_ncn8025(int devch, int reset_timing);
int sci_card_powerdown_ncn8025(int devch);

/* syncronous card interface by ncn8025 */
int Mc_Vcc_NCN8025(unsigned char devch, unsigned int mode);
void Mc_Reset_NCN8025(unsigned char devch, unsigned char level);
void Mc_C4_Write_NCN8025(unsigned char devch, unsigned char level);
void Mc_C8_Write_NCN8025(unsigned char devch, unsigned char level);

/* global interface on ncn8025 */
void sci_usercard_detect_ncn8025(void);
void controller_intr_handler_ncn8025(unsigned long data);
int controller_enable_ncn8025(int enable);
int controller_init_ncn8025(void);



#ifdef __cplusplus
}
#endif

#endif
