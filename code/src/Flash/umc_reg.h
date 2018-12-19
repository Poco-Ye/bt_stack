

//#include "bcm5892_sw.h"
#include "bcm5892_reg.h"

// ---------------------------------------------------------------------------
// // Register Address Mapping
// //
// ---------------------------------------------------------------------------
 #define UMC_R_umc_status_reg_MEMADDR (UMC_REG_BASE_ADDR + 0x00)
 #define UMC_R_umc_memif_cfg_MEMADDR  (UMC_REG_BASE_ADDR + 0x04)
 #define UMC_R_umc_memc_cfg_MEMADDR   (UMC_REG_BASE_ADDR + 0x08)
 #define UMC_R_umc_memc_clr_MEMADDR   (UMC_REG_BASE_ADDR + 0x0c)
 #define UMC_R_umc_direct_cmd_MEMADDR (UMC_REG_BASE_ADDR + 0x10)
 #define UMC_R_umc_set_cycles_MEMADDR (UMC_REG_BASE_ADDR + 0x14)
 #define UMC_R_umc_set_opmode_MEMADDR (UMC_REG_BASE_ADDR + 0x18)
 #define UMC_R_umc_set_opmode0_cs1_MEMADDR (UMC_REG_BASE_ADDR + 0x124)
 #define UMC_R_umc_addr_mask_MEMADDR  (UMC_REG_BASE_ADDR + 0xa00)
 #define UMC_R_umc_addr_match_MEMADDR (UMC_REG_BASE_ADDR + 0xa04)
 #define UMC_R_umc_conf_MEMADDR       (UMC_REG_BASE_ADDR + 0xa08)

 #define UMC_R_umc_ecc_status_MEMADDR  (UMC_REG_BASE_ADDR + 0x400)
 #define UMC_R_umc_ecc_memcfg_MEMADDR  (UMC_REG_BASE_ADDR + 0x404)
 #define UMC_R_umc_ecc_memcmd1_MEMADDR (UMC_REG_BASE_ADDR + 0x408)
 #define UMC_R_umc_ecc_memcmd2_MEMADDR (UMC_REG_BASE_ADDR + 0x40c)
 #define UMC_R_umc_ecc_addr0_MEMADDR   (UMC_REG_BASE_ADDR + 0x410)
 #define UMC_R_umc_ecc_addr1_MEMADDR   (UMC_REG_BASE_ADDR + 0x414)
 #define UMC_R_umc_ecc_value0_MEMADDR  (UMC_REG_BASE_ADDR + 0x418)
 #define UMC_R_umc_ecc_value1_MEMADDR  (UMC_REG_BASE_ADDR + 0x41c)
 #define UMC_R_umc_ecc_value2_MEMADDR  (UMC_REG_BASE_ADDR + 0x420)
 #define UMC_R_umc_ecc_value3_MEMADDR  (UMC_REG_BASE_ADDR + 0x424)

 #define UMC_R_umc_msu_scr_control_reg_MEMADDR            (UMC_REG_BASE_ADDR + 0x0b00)
 #define UMC_R_umc_msu_addr_swz_ctrl_lo_reg_MEMADDR       (UMC_REG_BASE_ADDR + 0x0b04)
 #define UMC_R_umc_msu_addr_swz_ctrl_hi_reg_MEMADDR       (UMC_REG_BASE_ADDR + 0x0b08)
 #define UMC_R_umc_msu_addr_key_reg_MEMADDR               (UMC_REG_BASE_ADDR + 0x0b0c)
 #define UMC_R_umc_msu_addr_scr_msk_reg_MEMADDR           (UMC_REG_BASE_ADDR + 0x0b10)
 #define UMC_R_umc_msu_data_swz_ctrl_lo_reg_MEMADDR       (UMC_REG_BASE_ADDR + 0x0b14)
 #define UMC_R_umc_msu_data_swz_ctrl_hi_reg_MEMADDR       (UMC_REG_BASE_ADDR + 0x0b18)
 #define UMC_R_umc_msu_data_key_reg_MEMADDR               (UMC_REG_BASE_ADDR + 0x0b1c)
 #define UMC_R_umc_msu_addr_scr_base_reg_MEMADDR          (UMC_REG_BASE_ADDR + 0x0b20)
 #define UMC_R_umc_msu_addr_scr_base_msk_reg_MEMADDR      (UMC_REG_BASE_ADDR + 0x0b24)

// ---------------------------------------------------
// NAND Interrupt Status fields
// ---------------------------------------------------
#define UMC_ECC_INT_INTERFACE1 0x400
#define UMC_RDY_INT_INTERFACE0 0x008
#define UMC_RDY_INT_INTERFACE1 0x010


