
#ifndef _UMC_CPUAPI_H_
#define _UMC_CPUAPI_H_

#include "umc_reg.h"

// ---------------------------------------------------
// Timing register fields
// ---------------------------------------------------
#define T_TR_MASK      0x7
#define T_PC_MASK      0x7
#define T_WP_MASK      0x7
#define T_CEOE_MASK    0x7
#define T_WC_MASK      0xf
#define T_RC_MASK      0xf

#define UMC_F_t_tr_3_R         17
#define UMC_F_t_pc_3_R         14
#define UMC_F_t_wp_3_R         11
#define UMC_F_t_ceoe_3_R       8
#define UMC_F_t_wc_0_R         4
#define UMC_F_t_rc_1_R         0

// ---------------------------------------------------
// UMC Configuration values
// ---------------------------------------------------
#define TYPE_NODEVICE      0
#define TYPE_SRAM_NONMUXED 1
#define TYPE_NAND          2
#define TYPE_SRAM_MUXED    3

#define CS_NAND 0
#define CS_SRAM 1

#define WIDTH_8  0
#define WIDTH_16 1
#define WIDTH_32 2

#define CHIPS_1 0
#define CHIPS_2 1
#define CHIPS_3 2
#define CHIPS_4 3

#define ECC_ABORT_INT_DIS 0x0
#define ECC_ABORT_INT_EN 0x1

#define ECC_PAGE_512  0x1
#define ECC_PAGE_1024 0x2
#define ECC_PAGE_2048 0x3

#define ECC_BYPASS    0x0
#define ECC_APBRD     0x1
#define ECC_ENABLE    0x2

#define ECC_RD_512    0x0
#define ECC_RD_END    0x1

#define ECC_NOJUMP     0x0
#define ECC_JUMP_COL   0x1
#define ECC_JUMP_FULL  0x2

#define ECC_COL_ADDR_MT 0x410

// ---------------------------------------------------
// NAND Commands
// ---------------------------------------------------
#define NAND_PAGE_PRG_S_CMD 0x80
#define NAND_PAGE_PRG_E_CMD 0x10
#define NAND_PAGE_RD_S_CMD  0x00
#define NAND_PAGE_RD_E_CMD  0x30
#define NAND_BLK_ERS_S_CMD  0x60
#define NAND_BLK_ERS_E_CMD  0xD0
#define NAND_RAND_RD_S_CMD  0x05
#define NAND_RAND_RD_E_CMD  0xE0
#define NAND_RAND_WR_S_CMD  0x85




#endif
// ---------------------------------------------------------------------------
// End of File
// ---------------------------------------------------------------------------
