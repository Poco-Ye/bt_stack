/* $Id: tda8026.h 5688 2012-04-06 05:35:27Z danfe $ */

#pragma once
#ifndef _TDA8026_H
#define _TDA8026_H

#ifdef __cplusplus
extern "C" {
#endif

#include "icc_hard_bcm5892.h"
#include "icc_device_configure.h"


#define BCM5892_I2C0_BASE_ADDR	0xC9000

#define BCM5892_I2C_CRSR	0x20
#define CRSR_SDA			0x80
#define CRSR_SCL			0x40
#define BCM5892_I2C_CLKEN	0x4C


#define BCM5892_READ_I2C_REG(reg) \
    readl((unsigned int)BCM5892_I2C0_BASE_ADDR + (reg))
#define BCM5892_WRITE_I2C_REG(reg, val) \
    writel((val), (unsigned int)BCM5892_I2C0_BASE_ADDR + (reg))

#define TDA8026_SDWN_PD		sci_device_resource.tda8026_interface.sdwn_pd_port,sci_device_resource.tda8026_interface.sdwn_pd_pin
#define TDA8026_I2C_SCL		sci_device_resource.tda8026_interface.i2c_scl_port,sci_device_resource.tda8026_interface.i2c_scl_pin
#define TDA8026_I2C_SDA		sci_device_resource.tda8026_interface.i2c_sda_port,sci_device_resource.tda8026_interface.i2c_sda_pin

#define SECOND_TDA8026_SDWN_PD		sci_device_resource.second_tda8026_interface.sdwn_pd_port,sci_device_resource.second_tda8026_interface.sdwn_pd_pin
#define SECOND_TDA8026_I2C_SCL		sci_device_resource.second_tda8026_interface.i2c_scl_port,sci_device_resource.second_tda8026_interface.i2c_scl_pin
#define SECOND_TDA8026_I2C_SDA		sci_device_resource.second_tda8026_interface.i2c_sda_port,sci_device_resource.second_tda8026_interface.i2c_sda_pin



/* asyncronous card interface by tda8026 */
int sci_card_xmit_setting_tda8026(int devch);
void sci_card_pin_control_tda8026(unsigned int pin, unsigned int val);
int  sci_card_cold_reset_tda8026(int devch, int voltage, int reset_timing);
int  sci_card_warm_reset_tda8026(int devch, int reset_timing);
int sci_card_powerdown_tda8026(int devch);


/* syncronous card interface by tda8026 */
int Mc_Vcc_TDA8026(unsigned char devch, unsigned int mode);
void Mc_Reset_TDA8026(unsigned char devch, unsigned char level);
void Mc_C4_Write_TDA8026(unsigned char devch, unsigned char level);
void Mc_C8_Write_TDA8026(unsigned char devch, unsigned char level);


/* global  interface on tda8026 */
void sci_usercard_detect_tda8026(void);
void controller_intr_handler_tda8026(unsigned long data);
int controller_enable_tda8026(int enable);
int controller_init_tda8026(void);


#ifdef __cplusplus
}
#endif

#endif /* _TDA8026_H */
