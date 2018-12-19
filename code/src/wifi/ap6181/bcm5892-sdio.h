#ifndef __BCM5892_SDIO_H__
#define __BCM5892_SDIO_H__

#define temp_w_dma_buffer ((volatile unsigned char *)DMA_SDIO_BASE)
#define temp_r_dma_buffer ((volatile unsigned char *)DMA_SDIO_BASE)

#define SDIO_BASE_ADDR      0x00088000
#define SDIO_SDMA_ADDR_REG  SDIO_BASE_ADDR 
#define SDIO_BLK_SIZE_REG   (SDIO_BASE_ADDR + 0x04)
#define SDIO_DMA_SIZE_SHIFT     12
#define SDIO_DMA_SIZE_MASK      (7 << SDIO_DMA_SIZE_SHIFT)

#define SDIO_BLK_CNT_REG    (SDIO_BASE_ADDR + 0x06)
#define SDIO_ARG_REG        (SDIO_BASE_ADDR + 0x08)
#define SDIO_TRANX_MODE_REG (SDIO_BASE_ADDR + 0x0C)
#define SDIO_MS_BLK_SEL_SHIFT           5
#define SDIO_MS_BLK_SEL_MASK           (1 << SDIO_MS_BLK_SEL_SHIFT)
#define SDIO_TRANX_DIR_SEL_SHIFT        4 
#define SDIO_AUTO_CMD12_EN_SHIFT        2
#define SDIO_AUTO_CMD12_EN_MASK         (1 << SDIO_AUTO_CMD12_EN_SHIFT)
#define SDIO_BLK_CNT_EN_SHIFT           1
#define SDIO_BLK_CNT_EN_MASK            (1 << SDIO_BLK_CNT_EN_SHIFT)
#define SDIO_DMA_EN_SHIFT               0
#define SDIO_DMA_EN_MASK                (1 << 0)

#define SDIO_CMD_REG        (SDIO_BASE_ADDR + 0x0E)
#define SDIO_CMD_IDX_SHIFT          8
#define SDIO_DAT_PRES_SEL_SHIFT     5
#define SDIO_DAT_PRES_SEL_MASK      (1 << SDIO_DAT_PRES_SEL_SHIFT)
#define SDIO_CMD_IDX_CHK_EN_SHIFT   4
#define SDIO_CMD_IDX_CHK_EN_MASK    (1 << SDIO_CMD_IDX_CHK_EN_SHIFT)
#define SDIO_CRC_CHK_EN_SHIFT       3
#define SDIO_CRC_CHK_EN_MASK        (1 << SDIO_CRC_CHK_EN_SHIFT)
#define SDIO_RESP_TYPE_SHIFT        0

#define SDIO_RESP1_REG      (SDIO_BASE_ADDR + 0x10)
#define SDIO_RESP2_REG      (SDIO_BASE_ADDR + 0x14)
#define SDIO_PRES_STA_REG   (SDIO_BASE_ADDR + 0x24)
#define SDIO_RXACT_SHIFT            9
#define SDIO_RXACT_MASK             (1 << SDIO_RXACT_SHIFT)
#define SDIO_TXACT_SHIFT            8
#define SDIO_TXACT_MASK             (1 << SDIO_TXACT_SHIFT)
#define SDIO_DAT_INH_SHIFT          1
#define SDIO_DAT_INH_MASK           (1 << SDIO_DAT_INH_SHIFT)
#define SDIO_CMD_INH_SHIFT          0
#define SDIO_CMD_INH_MASK           (1 << SDIO_CMD_INH_SHIFT)

#define SDIO_HC_REG         (SDIO_BASE_ADDR + 0x28)
#define SDIO_DAT_TRANX_WID_SHIFT    1
#define SDIO_H_SPEED_EN_SHIFT       2
#define SDIO_DMA_SEL_SHIFT          3
#define SDIO_DMA_SEL_MASK           (3 << SDIO_DMA_SEL_SHIFT)

#define SDIO_POWER_C_REG    (SDIO_BASE_ADDR + 0x29)
#define SDIO_CLOCK_C_REG    (SDIO_BASE_ADDR + 0x2C)
#define SDIO_TIMER_CTRL_REG (SDIO_BASE_ADDR + 0x2E)
#define SDIO_SW_RST_REG     (SDIO_BASE_ADDR + 0x2F)

#define SDIO_NORM_STA_REG   (SDIO_BASE_ADDR + 0x30)
#define SDIO_ERR_INT_SHIFT      15
#define SDIO_ERR_INT_MASK       (1 << SDIO_ERR_INT_SHIFT)
#define SDIO_DMA_INT_SHIFT      3
#define SDIO_DMA_INT_MASK       (1 << SDIO_DMA_INT_SHIFT)
#define SDIO_TRANX_CMPL_SHIFT   1
#define SDIO_TRANX_CMPL_MASK    (1 << SDIO_TRANX_CMPL_SHIFT)
#define SDIO_CMD_CMPL_SHIFT     0
#define SDIO_CMD_CMPL_MASK      (1 << SDIO_CMD_CMPL_SHIFT)

#define SDIO_ERR_STA_REG    (SDIO_BASE_ADDR + 0x32)
#define SDIO_CMD_TMROUT_ERR_SHIFT       0
#define SDIO_CMD_TMROUT_ERR_MASK        (1 << SDIO_CMD_TMROUT_ERR_SHIFT)
#define SDIO_CMD_CRC_ERR_SHIFT          1
#define SDIO_CMD_CRC_ERR_MASK           (1 << SDIO_CMD_CRC_ERR_SHIFT)
#define SDIO_CMD_END_BIT_ERR_SHIFT      2
#define SDIO_CMD_END_BIT_ERR_MASK       (1 << SDIO_CMD_END_BIT_ERR_SHIFT)
#define SDIO_CMD_IDX_ERR_SHIFT          3
#define SDIO_CMD_IDX_ERR_MASK           (1 << SDIO_CMD_IDX_ERR_SHIFT)
#define SDIO_DAT_TMROUT_ERR_SHIFT       4
#define SDIO_DAT_TMROUT_ERR_MASK        (1 << SDIO_DAT_TMROUT_ERR_SHIFT)
#define SDIO_DAT_CRC_ERR_SHIFT          5
#define SDIO_DAT_CRC_ERR_MASK           (1 << SDIO_DAT_CRC_ERR_SHIFT)

#define SDIO_NORM_INT_EN_REG (SDIO_BASE_ADDR + 0x34)

#define SDIO_CMD_CMPL_EN_SHIFT      0
#define SDIO_CMD_CMPL_EN_MASK       (1 << SDIO_CMD_CMPL_EN_SHIFT)
#define SDIO_TRANX_CMPL_EN_SHIFT    1
#define SDIO_TRANX_CMPL_EN_MASK     (1 << SDIO_TRANX_CMPL_EN_SHIFT)
#define SDIO_DMA_INT_EN_SHIFT       3
#define SDIO_DMA_INT_EN_MASK        (1 << SDIO_DMA_INT_EN_SHIFT)
#define SDIO_BUF_W_EN_SHIFT         4
#define SDIO_BUF_W_EN_MASK          (1 << SDIO_BUF_W_EN_SHIFT)
#define SDIO_BUF_R_EN_SHIFT         5
#define SDIO_BUF_R_EN_MASK          (1 << SDIO_BUF_R_EN_SHIFT)

#define SDIO_INT_SIG_EN_REG         (SDIO_BASE_ADDR + 0x38)
#define SDIO_ERR_INT_SIGN_EN_REG    (SDIO_BASE_ADDR + 0x3A)

#define SDIO_TRANX_CMPL_EN_SIG_SHIFT    1
#define SDIO_TRANX_CMPL_EN_SIGN_MASK     (1 << SDIO_TRANX_CMPL_EN_SIG_SHIFT)
#define SDIO_DAT_CRC_ERR_SIG_SHIFT      5
#define SDIO_DAT_CRC_ERR_SIG_MASK       (1 << SDIO_DAT_CRC_ERR_SIG_SHIFT)

#define SDIO_AUTO_CMD12_ERR_REG     (SDIO_BASE_ADDR + 0x3C)
#define SDIO_CAP_REG                (SDIO_BASE_ADDR + 0x40)
#define SDIO_HOST_VERSION	        (SDIO_BASE_ADDR + 0xFE)

#define SDHCI_RESET_ALL	    0x01
#define SDHCI_RESET_CMD	    0x02
#define SDHCI_RESET_DATA	0x04

#define BUS_LEVEL_MAX_RETRIES                (5)
#define SDIO_CLK_HIGN_SPEED                  (12 * 1000 * 1000)
#define SDIO_CLK_LOW_SPEED                   (300 * 1000)
#define SDIO_INT_ALL_MASK	                 ((unsigned int)-1)

#define  SDIO_CMD_RESP_NONE	    0x00
#define  SDIO_CMD_RESP_LONG	    0x01
#define  SDIO_CMD_RESP_SHORT	0x02

#define WWD_SDIO_RETRIES_EXCEEDED       -1

#define SDIO_WRITE_BYTE(reg, val)   (*(volatile char *)(reg) = val)
#define SDIO_WRITE_BYTE_OR(reg, val) (*(volatile char *)(reg) |= val)
#define SDIO_WRITE_BYTE_AND(reg, val) (*(volatile char *)(reg) &= val)
#define SDIO_READ_BYTE(reg)         (*(volatile char *)(reg))

#define SDIO_WRITE_WORD(reg, val)   (*(volatile short *)(reg) = (val))
#define SDIO_WRITE_WORD_OR(reg, val) (*(volatile short *)(reg) |= (val))
#define SDIO_WRITE_WORD_AND(reg, val) (*(volatile short *)(reg) &= (val))
#define SDIO_READ_WORD(reg)         (*(volatile short *)(reg))

#define SDIO_WRITE_DWORD(reg, val)  (*(volatile int *)(reg) = val)
#define SDIO_WRITE_DWORD_OR(reg, val) (*(volatile int *)(reg) |= val)
#define SDIO_WRITE_DWORD_AND(reg, val) (*(volatile int *)(reg) &= val)
#define SDIO_READ_DWORD(reg)        (*(volatile int *)(reg))

#define SDIO_1_4BITS_MODE_SHIFT     1
#define SDIO_H_SPEED_SHIFT          2


typedef enum
{
    SDIO_CMD_0  =  0,
    SDIO_CMD_3  =  3,
    SDIO_CMD_5  =  5,
    SDIO_CMD_7  =  7,
    SDIO_CMD_52 = 52,
    SDIO_CMD_53 = 53,
    __MAX_VAL   = 64
} sdio_command_t;

typedef enum
{
    SDIO_BLOCK_MODE = ( 0 << 2 ), /* These are STM32 implementation specific */
    SDIO_BYTE_MODE  = ( 1 << 2 )  /* These are STM32 implementation specific */
} sdio_transfer_mode_t;

typedef enum
{
    SDIO_1B_BLOCK    =  1,
    SDIO_2B_BLOCK    =  2,
    SDIO_4B_BLOCK    =  4,
    SDIO_8B_BLOCK    =  8,
    SDIO_16B_BLOCK   =  16,
    SDIO_32B_BLOCK   =  32,
    SDIO_64B_BLOCK   =  64,
    SDIO_128B_BLOCK  =  128,
    SDIO_256B_BLOCK  =  256,
    SDIO_512B_BLOCK  =  512,
    SDIO_1024B_BLOCK = 1024,
    SDIO_2048B_BLOCK = 2048
} sdio_block_size_t;


typedef enum
{
    RESPONSE_NEEDED,
    NO_RESPONSE
} sdio_response_needed_t;

typedef enum
{
    /* If updating this enum, the bus_direction_mapping variable will also need to be updated */
    BUS_READ,
    BUS_WRITE
} wwd_bus_transfer_direction_t;


#endif
