#include "typedefs.h"
#include "base.h"
#include "bcm5892-sdio.h"
#include <stdlib.h>
#include "string.h"
#include "sdio.h"
#include "posapi.h"
#include "platform.h"

#define SDIO_SPEC_100	            0
#define SDIO_SPEC_200	            1
#define SDIO_SPEC_300	            2
#define SDIO_MAX_DIV_SPEC_200	    256
#define SDIO_MAX_DIV_SPEC_300	    2046
#define SDIO_DIVIDER_SHIFT	        8
#define SDIO_DIVIDER_HI_SHIFT	    6
#define SDIO_DIV_MASK	            0xFF
#define SDIO_DIV_MASK_LEN	        8
#define SDIO_DIV_HI_MASK	        0x300
#define SDIO_CLOCK_CARD_EN	        0x0004
#define SDIO_CLOCK_INT_STABLE	    0x0002
#define SDIO_CLOCK_INT_EN	        0x0001
#define SDIO_TIMEOUT_CLK_MASK	    0x0000003F
#define SDIO_TIMEOUT_CLK_SHIFT      0
#define SDIO_TIMEOUT_CLK_UNIT       0x00000080
#define SDIO_CLOCK_BASE_MASK        0x00003F00
#define SDIO_CLOCK_BASE_SHIFT	    8
#define SDIO_TIMEOUT_CLOCK_MASK     0x0000002F
#define SDIO_TIMEOUT_CLK_UNIT_MASK  0x00000080   
#define SDIO_MAX_BLOCK_MASK	        0x00030000
#define SDIO_MAX_BLOCK_SHIFT        16
#define SDIO_MAX_BLK_SIZE_SHIFT     16
#define SDIO_MAX_BLK_SIZE_MASK      (3 << SDIO_MAX_BLK_SIZE_SHIFT)
#define SDIO_CAP_INT_MODE_MASK      (1 << 27)
#define SDIO_CAP_33V_SUPT           (1 << 24)
#define SDIO_CAP_SDMA_SUPT          (1 << 22)

#define SDIO_POWER_ON		0x01
#define SDIO_POWER_180	    0x0A
#define SDIO_POWER_300	    0x0C
#define SDIO_POWER_330	    0x0E

#define SDIO_RESET_ALL	    0x01
#define SDIO_RESET_CMD	    0x02
#define SDIO_RESET_DATA	    0x04

/* unit ms */
#define SW_RST_TIMEOUT_VALUE            100 
#define SET_CLK_EFFECT_TIMEOUT_VALUE    50

typedef struct 
{
    int ver;
    int max_clk;
    int timeout_clk;
    int speed_mode;
    int max_blk_size;
    int voltages;
} sdio_info_t;

sdio_info_t g_sdio_info;

int sdio_reset(unsigned char val)
{
    volatile unsigned int t0;

    SDIO_WRITE_BYTE(SDIO_SW_RST_REG, val);
    t0 = s_Get10MsTimerCount();

    while (SDIO_READ_BYTE(SDIO_SW_RST_REG) & val)
    {
        if ((s_Get10MsTimerCount() - t0) > (SW_RST_TIMEOUT_VALUE/10))
            return -1;
    }

    return 0;
}

static int sdio_set_clock(unsigned int clock)
{
    sdio_info_t *sdio_info=&g_sdio_info;
    unsigned int div, clk, t0;
    int MAX_SDCLK = sdio_info->max_clk;

    SDIO_WRITE_WORD(SDIO_CLOCK_C_REG, 0);

    if (clock == 0)
        return 0;

    if (sdio_info->ver >= SDIO_SPEC_300)
    {
        /* Version 3.00 divisors must be a multiple of 2. */
        if (MAX_SDCLK <= clock)
            div = 1;
        else
        {
            for (div = 2; div < SDIO_MAX_DIV_SPEC_300; div += 2)
                if ((MAX_SDCLK / div) <= clock)
                    break;
        }
    }
    else
    {
        /* Version 2.00 divisors must be a power of 2. */
        for (div = 1; div < SDIO_MAX_DIV_SPEC_200; div *= 2)
        {
            if ((MAX_SDCLK / div) <= clock)
                break;
        }
    }

    div >>= 1;

    clk = (div & SDIO_DIV_MASK) << SDIO_DIVIDER_SHIFT;
    clk |= ((div & SDIO_DIV_HI_MASK) >> SDIO_DIV_MASK_LEN) << SDIO_DIVIDER_HI_SHIFT;
    clk |= SDIO_CLOCK_INT_EN;
    SDIO_WRITE_WORD(SDIO_CLOCK_C_REG, clk);

    t0 = s_Get10MsTimerCount();
    while (!((clk = SDIO_READ_WORD(SDIO_CLOCK_C_REG)) & SDIO_CLOCK_INT_STABLE))
    {
        if ((s_Get10MsTimerCount() - t0) > SET_CLK_EFFECT_TIMEOUT_VALUE / 10) //50ms
            return -1;
    }

    clk |= SDIO_CLOCK_CARD_EN;
    SDIO_WRITE_WORD(SDIO_CLOCK_C_REG, clk);
    return 0;
}

static int sdio_get_cap(sdio_info_t *sdio_info)
{
    int caps;

    caps = SDIO_READ_DWORD(SDIO_CAP_REG);
    if(!(caps & SDIO_CAP_INT_MODE_MASK))
        return -1;

    if(!(caps & SDIO_CAP_33V_SUPT))
        return -2;

    if(!(caps & SDIO_CAP_SDMA_SUPT))
        return -3;

	if (!(caps & SDIO_CLOCK_BASE_MASK)) 
		return -4;
    
    sdio_info->max_clk = (caps & SDIO_CLOCK_BASE_MASK) >> SDIO_CLOCK_BASE_SHIFT;
    sdio_info->max_clk *= 1000000;

    sdio_info->timeout_clk = (caps & SDIO_TIMEOUT_CLOCK_MASK);
    if(caps & SDIO_TIMEOUT_CLK_UNIT_MASK)
        sdio_info->timeout_clk *= 1000000;
    else
        sdio_info->timeout_clk *= 1000;
    
    switch((caps & SDIO_MAX_BLK_SIZE_MASK) >> SDIO_MAX_BLK_SIZE_SHIFT)
    {
        case 0: sdio_info->max_blk_size = 512;  break;
        case 1: sdio_info->max_blk_size = 1024; break;
        case 2: sdio_info->max_blk_size = 2048; break;
        case 3: sdio_info->max_blk_size = 4096; break;
    };

    return 0;
}

void sdio_enable_bus_irq( void )
{
    SDIO_WRITE_WORD_AND(SDIO_INT_SIG_EN_REG,  ~(SDIO_BUF_R_EN_MASK | SDIO_BUF_W_EN_MASK));
    SDIO_WRITE_WORD_AND(SDIO_NORM_INT_EN_REG, ~(SDIO_BUF_W_EN_MASK | SDIO_BUF_R_EN_MASK));
}

void sdio_disable_bus_irq( void )
{
    SDIO_WRITE_WORD(SDIO_NORM_INT_EN_REG, 0);
}

static void sdio_power_switch(int power)
{
	if (power == 0) 
		SDIO_WRITE_BYTE(SDIO_POWER_C_REG, 0);
    else
        SDIO_WRITE_BYTE(SDIO_POWER_C_REG, SDIO_POWER_330 | SDIO_POWER_ON);

    DelayMs(10);
    return;
}

int sdio_init(void)
{
    sdio_info_t *sdio_info = &g_sdio_info;
    int ret;

    /* software reset */
    ret = sdio_reset(SDIO_RESET_ALL);
    if(ret) return -1;
 
    /* read version */
	sdio_info->ver = SDIO_READ_WORD(SDIO_HOST_VERSION) & 0xff;

    /* read capabilities */
    ret = sdio_get_cap(sdio_info);
    if(ret) return -2;

    /* disable interrupt */
    SDIO_WRITE_DWORD(SDIO_NORM_INT_EN_REG, SDIO_INT_ALL_MASK);
	SDIO_WRITE_DWORD(SDIO_INT_SIG_EN_REG, SDIO_INT_ALL_MASK);

    /* enable sdio power */
    sdio_power_switch(1);
 
    /* enable sdio clock */
    ret = sdio_set_clock(SDIO_CLK_HIGN_SPEED);
    if(ret) return -3;

    /* SDIO_HC_REG: sdma is default selected , and with high speed, 4-bit burst */
    SDIO_WRITE_BYTE(SDIO_HC_REG, (1 << SDIO_1_4BITS_MODE_SHIFT) | (1 << SDIO_H_SPEED_SHIFT));

    /* SDIO_TRANX_MODE_REG: sd is default selected, always multiblock select, even though one block data being sent */
    SDIO_WRITE_WORD_OR(SDIO_TRANX_MODE_REG, (unsigned short)((1 << SDIO_MS_BLK_SEL_SHIFT) | (1 << SDIO_BLK_CNT_EN_SHIFT)));

    return 0;
}

int sdio_deinit(void)
{
    sdio_power_switch(0);
    return 0;
}

int sdio_set_sdma_info(int transfer_size, int block_size)
{
    int dma_buff_size;
    int smode;

    if(transfer_size > 512 * 1024) 
        return -1;

    if(block_size >= 0x2000)
        return -2;

    if(transfer_size > 0x40000)
    {
        dma_buff_size = 0x80000;
        smode = 7;
    }
    else if(transfer_size > 0x20000)
    {
        dma_buff_size = 0x40000;
        smode = 6;
    }
    else if(transfer_size > 0x10000)
    {
        dma_buff_size = 0x20000;
        smode = 5;
    }
    else if(transfer_size > 0x8000)
    {
        dma_buff_size = 0x10000;
        smode = 4;
    }
    else if(transfer_size > 0x4000)
    {
        dma_buff_size = 0x8000;
        smode = 3;
    }
    else if(transfer_size > 0x2000)
    {
        dma_buff_size = 0x4000;
        smode = 2;
    }
    else if(transfer_size > 0x1000)
    {
        dma_buff_size = 0x2000;
        smode = 1;
    }
    else
    {
        dma_buff_size = 0x1000;
        smode = 0;
    }

    smode = 7;

    if(dma_buff_size / block_size > 65535)
        return -3;

    /* iset sdma buffer size */
    SDIO_WRITE_WORD(SDIO_BLK_SIZE_REG, 0);
    SDIO_WRITE_WORD_OR(SDIO_BLK_SIZE_REG, (smode << SDIO_DMA_SIZE_SHIFT));

    /* set block size */
    if(block_size >= 0x1000) SDIO_WRITE_WORD_OR(SDIO_BLK_SIZE_REG, (1 << 15));
    SDIO_WRITE_WORD_OR(SDIO_BLK_SIZE_REG, (block_size & 0xFFF));

    /* set writting block count */
    SDIO_WRITE_WORD(SDIO_BLK_CNT_REG, transfer_size / block_size);
    return 0;
}

static uchar *  user_data;
static uint  user_data_size;
static uchar * dma_data_source;
static uint dma_transfer_size;

int host_platform_sdio_transfer(int direction, int command, int mode, int block_size, uint argument, 
                                uint * data, ushort data_size, int response_expected, uint * response);

typedef struct
{
    uchar       write_data;           /* 0 - 7 */
    unsigned int  _stuff2          : 1; /* 8     */
    unsigned int  register_address :17; /* 9-25  */
    unsigned int  _stuff           : 1; /* 26    */
    unsigned int  raw_flag         : 1; /* 27    */
    unsigned int  function_number  : 3; /* 28-30 */
    unsigned int  rw_flag          : 1; /* 31    */
} wwd_bus_sdio_cmd52_argument_t;

typedef struct
{
    unsigned int  count            : 9; /* 0-8   */
    unsigned int  register_address :17; /* 9-25  */
    unsigned int  op_code          : 1; /* 26    */
    unsigned int  block_mode       : 1; /* 27    */
    unsigned int  function_number  : 3; /* 28-30 */
    unsigned int  rw_flag          : 1; /* 31    */
} wwd_bus_sdio_cmd53_argument_t;

typedef union
{
    uint              value;
    wwd_bus_sdio_cmd52_argument_t cmd52;
    wwd_bus_sdio_cmd53_argument_t cmd53;
} sdio_cmd_argument_t;

typedef enum
{
    BUS_FUNCTION       = 0,
    BACKPLANE_FUNCTION = 1,
    WLAN_FUNCTION      = 2
} wwd_bus_function_t;

#define BUS_PROTOCOL_LEVEL_MAX_RETRIES          5
#define BUS_FUNCTION_MASK                       0x3
#define F0_WORKING_TIMEOUT_MS                   500
#define SDIO_ENUMERATION_TIMEOUT_MS             500
#define SDIO_CHECK_BUSY_TIMEOUT_INTERVAL        1000
#define COMMAND_FINISHED_CMD52_TIMEOUT_LOOPS    10

int host_rtos_delay_milliseconds( uint num_ms )
{
    int start = GetTimerCount();

    while ((GetTimerCount() - start) < num_ms );
    return 0;
}

int wwd_bus_sdio_cmd52(int direction, int function, uint address, uchar value, int response_expected, uchar* response )
{
    uint sdio_response;
    int result;
    sdio_cmd_argument_t arg;

    arg.value = 0;
    arg.cmd52.function_number  = (unsigned int) ( function & BUS_FUNCTION_MASK );
    arg.cmd52.register_address = (unsigned int) ( address & 0x00001ffff );
    arg.cmd52.rw_flag = (unsigned int) ( ( direction == BUS_WRITE ) ? 1 : 0 );
    arg.cmd52.write_data = value;
    result = host_platform_sdio_transfer( direction, SDIO_CMD_52, SDIO_BYTE_MODE, SDIO_1B_BLOCK, arg.value, 0, 0, response_expected, &sdio_response );
    if ( response != NULL )
        *response = (uchar) ( sdio_response & 0x00000000ff );

    return result;
}

int wwd_bus_sdio_cmd53(int direction, int function, int mode, int ops, uint address, ushort data_size, uchar* data, int response_expected, uint* response )
{
    sdio_cmd_argument_t arg;
    int result;

    arg.value = 0;
    arg.cmd53.function_number  = (unsigned int) ( function & BUS_FUNCTION_MASK );
    arg.cmd53.register_address = (unsigned int) ( address & 0x00001ffff );
    arg.cmd53.op_code = (unsigned int) ops;
    arg.cmd53.rw_flag = (unsigned int) ( ( direction == BUS_WRITE ) ? 1 : 0 );
    if ( mode == SDIO_BYTE_MODE )
    {
        arg.cmd53.count = (unsigned int) ( data_size & 0x1FF );
    }
    else
    {
        arg.cmd53.count = (unsigned int) ( ( data_size / (ushort)SDIO_64B_BLOCK ) & 0x0000001ff );
        if ( (uint) ( arg.cmd53.count * (ushort)SDIO_64B_BLOCK ) < data_size )
        {
            ++arg.cmd53.count;
        }
        arg.cmd53.block_mode = (unsigned int) 1;
    }

    result = host_platform_sdio_transfer( direction, SDIO_CMD_53, mode, SDIO_64B_BLOCK, arg.value, (uint*) data, data_size, response_expected, response );
    return result;
}

int wwd_bus_sdio_transfer(int direction, int function, uint address, ushort data_size, uchar* data, int response_expected )
{
    int result;
    uchar retry_count = 0;

    do
    {
        if ( data_size == (ushort) 1 )
        {
            result = wwd_bus_sdio_cmd52( direction, function, address, *data, response_expected, data );
        }
        else
        {
            result = wwd_bus_sdio_cmd53( direction, function, ( data_size >= (ushort) 64 ) ? SDIO_BLOCK_MODE : SDIO_BYTE_MODE, 1,
                                        address, data_size, data, response_expected, NULL );
        }

        if ( result != 0 )
            return result;
        
    } while ( ( result != 0) && ( retry_count < (uchar) BUS_PROTOCOL_LEVEL_MAX_RETRIES ) );

    return result;
}

int wwd_bus_write_register_value(int function, uint address, uchar value_length, uint value )
{
    return wwd_bus_sdio_transfer( BUS_WRITE, function, address, value_length, (uchar*) &value, RESPONSE_NEEDED );
}

int wwd_bus_sdio_read_register_value(int function, uint address, uchar value_length, uchar* value )
{
    memset( value, 0, (size_t) value_length );
    return wwd_bus_sdio_transfer( BUS_READ, function, address, value_length, value, RESPONSE_NEEDED );
}

int sdio_reset_dev(void)
{
    int ret;

    ret = host_platform_sdio_enumerate();
    switch_comm_rate();
    return ret ? FALSE : TRUE; 
}

int switch_comm_rate(void)
{
    int i, loop_count=0,  result;
    uchar        byte_data;

    do {
        /* Enable function 1 (backplane) */
        wwd_bus_write_register_value(BUS_FUNCTION, SDIOD_CCCR_IOEN,
                                     (uchar) 1, SDIO_FUNC_ENABLE_1);
        if (loop_count != 0) {
            host_rtos_delay_milliseconds((uint) 1);
        }

        wwd_bus_sdio_read_register_value(BUS_FUNCTION, SDIOD_CCCR_IOEN,
                                         (uchar) 1, &byte_data);
        loop_count++;
        if (loop_count >= (uint) F0_WORKING_TIMEOUT_MS)
            return -1;

    }while (byte_data != (uchar) SDIO_FUNC_ENABLE_1);

    /* Read the bus width and set to 4 bits */
    wwd_bus_sdio_read_register_value(BUS_FUNCTION, SDIOD_CCCR_BICTRL,
                                     (uchar) 1, &byte_data);
    wwd_bus_write_register_value(BUS_FUNCTION, SDIOD_CCCR_BICTRL, (uchar) 1,
                                 (byte_data & (~BUS_SD_DATA_WIDTH_MASK)) |
                                 BUS_SD_DATA_WIDTH_4BIT);

    /* This code is required if we want more than 25 MHz clock */
    wwd_bus_sdio_read_register_value(BUS_FUNCTION, SDIOD_CCCR_SPEED_CONTROL, 1, &byte_data);
    if ( ( byte_data & 0x1 ) != 0 )
    {
        wwd_bus_write_register_value(BUS_FUNCTION, SDIOD_CCCR_SPEED_CONTROL, 1, byte_data | SDIO_SPEED_EHS);
    }
    else
    {
        return -2;
    }

    return 0;
}

int host_platform_sdio_enumerate(void)
{
    int result, loop_count, data = 0;

    loop_count = 0;

    do {
        /* Send CMD0 to set it to idle state */
        host_platform_sdio_transfer( BUS_WRITE, SDIO_CMD_0, SDIO_BYTE_MODE, SDIO_1B_BLOCK, 0, 0, 0, NO_RESPONSE, NULL );

        /* CMD5. */
        host_platform_sdio_transfer( BUS_READ, SDIO_CMD_5, SDIO_BYTE_MODE, SDIO_1B_BLOCK, 0, 0, 0, NO_RESPONSE, NULL );
        /* Send CMD3 to get RCA. */
        result = host_platform_sdio_transfer( BUS_READ, SDIO_CMD_3, SDIO_BYTE_MODE, SDIO_1B_BLOCK, 0, 0, 0, RESPONSE_NEEDED, &data );
        loop_count++;
        if ( loop_count >= (uint) SDIO_ENUMERATION_TIMEOUT_MS )
        {
            return -1;
        }

    } while ( ( result != 0) && ( host_rtos_delay_milliseconds( (uint) 1 ), ( 1 == 1 ) ) );
    /* If you're stuck here, check the platform matches your hardware */

    /* Send CMD7 with the returned RCA to select the card */
    /* relative card address */
    host_platform_sdio_transfer( BUS_WRITE, SDIO_CMD_7, SDIO_BYTE_MODE, SDIO_1B_BLOCK, data, 0, 0, RESPONSE_NEEDED, NULL );
    return 0;
}

static int find_optimal_block_size( uint data_size )
{
    if ( data_size > (uint) 256 )
        return SDIO_512B_BLOCK;
    if ( data_size > (uint) 128 )
        return SDIO_256B_BLOCK;
    if ( data_size > (uint) 64 )
        return SDIO_128B_BLOCK;
    if ( data_size > (uint) 32 )
        return SDIO_64B_BLOCK;
    if ( data_size > (uint) 16 )
        return SDIO_32B_BLOCK;
    if ( data_size > (uint) 8 )
        return SDIO_16B_BLOCK;
    if ( data_size > (uint) 4 )
        return SDIO_8B_BLOCK;
    if ( data_size > (uint) 2 )
        return SDIO_4B_BLOCK;

    return SDIO_4B_BLOCK;
}

 /*@modifies dma_data_source, user_data, user_data_size, dma_transfer_size@*/
static void sdio_prepare_data_transfer(int direction, int block_size, uchar* data, ushort data_size )
{
    /* Setup a single transfer using the temp buffer */
    user_data         = data;
    user_data_size    = data_size;
    dma_transfer_size = (uint) ( ( ( data_size + (ushort) block_size - 1 ) / (ushort) block_size ) * (ushort) block_size );

    if ( direction == BUS_WRITE )
    {
        memcpy((unsigned char *)temp_w_dma_buffer, data, data_size);
        dma_data_source = (unsigned char *)temp_w_dma_buffer;
    }
    else
    {
        dma_data_source = (unsigned char *)temp_r_dma_buffer;
    }
   
    SDIO_WRITE_DWORD(SDIO_SDMA_ADDR_REG, (int)dma_data_source);
    SDIO_WRITE_BYTE(SDIO_TIMER_CTRL_REG, 0x0e);
    sdio_set_sdma_info(dma_transfer_size, block_size);
    return ;
}

#include "stdarg.h"

void prints2(const char *str, ...)
{
    va_list varg;
    int retv;
    char sbuffer[2050];
    static int first_flag = 0;

    if (!first_flag)
    {
        PortOpen(11, "115200,8,n,1");    //use COM1 port
        first_flag = 1;
    }

    memset(sbuffer, 0, sizeof(sbuffer));
    va_start(varg, str);
    retv = vsprintf(sbuffer, (char *)str, varg);
    va_end(varg);
    PortSends(11, (unsigned char *)sbuffer, strlen((char *)sbuffer));
    DelayMs(3);
    return ;
}

void prints1(const char *str, ...)
{
    return ;
}

int host_platform_sdio_transfer(int direction, int command, int mode,
                                int block_size, uint argument, uint * data,
                                ushort data_size, int response_expected,
                                uint * response)
{
    int result, i, total_blk_cnt, remain_blk_cnt;
    int t0, temp_sta, tmpval, loop_count=0, attempts = 0;

    if ( response != NULL )
        *response = 0;

restart:
    attempts++;
    if (attempts >= BUS_LEVEL_MAX_RETRIES) /* Check if we've tried too many times */
    {
        result = -3;
        prints1("!!!!!!!!!!!!!!!!>>>>>>>>sdio error!\r\n");
        goto exit;
    }

    temp_sta = SDIO_READ_WORD(SDIO_NORM_STA_REG);
    SDIO_WRITE_WORD(SDIO_NORM_STA_REG, temp_sta); 

    temp_sta = SDIO_READ_WORD(SDIO_ERR_STA_REG);
    SDIO_WRITE_WORD(SDIO_ERR_STA_REG, temp_sta);

	t0 = s_Get10MsTimerCount();
	while ((temp_sta = SDIO_READ_DWORD(SDIO_PRES_STA_REG)) & (SDIO_DAT_INH_MASK | SDIO_CMD_INH_MASK))  //1000ms
    {
        if((s_Get10MsTimerCount()-t0) > SDIO_CHECK_BUSY_TIMEOUT_INTERVAL/10) 
            return -2;
	}
	
    /* Prepare the data transfer register */
    if ( command == SDIO_CMD_53 )
    {
        sdio_enable_bus_irq();

        /* Dodgy STM32 hack to set the CMD53 byte mode size to be the same as the block size */
        if ( mode == SDIO_BYTE_MODE )
        {
            block_size = find_optimal_block_size( data_size );
            if ( block_size < SDIO_512B_BLOCK )
                argument = ( argument & (uint) ( ~0x1FF ) ) | block_size;
            else
                argument = ( argument & (uint) ( ~0x1FF ) );
        }

        /* Prepare the SDIO for a data transfer */
        sdio_prepare_data_transfer(direction, block_size, (uchar*) data, data_size);
        total_blk_cnt = SDIO_READ_WORD(SDIO_BLK_CNT_REG);

        /* Send the command */
        SDIO_WRITE_DWORD(SDIO_ARG_REG, argument);
        SDIO_WRITE_WORD(SDIO_TRANX_MODE_REG, (SDIO_BLK_CNT_EN_MASK | SDIO_DMA_EN_MASK));

        if(direction == BUS_READ)
            SDIO_WRITE_WORD_OR(SDIO_TRANX_MODE_REG, (1<<SDIO_TRANX_DIR_SEL_SHIFT));
        else
            SDIO_WRITE_WORD_AND(SDIO_TRANX_MODE_REG, ~(1<<SDIO_TRANX_DIR_SEL_SHIFT));

        if(data_size <= block_size)
            SDIO_WRITE_WORD_AND(SDIO_TRANX_MODE_REG, ~(SDIO_MS_BLK_SEL_MASK));
        else
            SDIO_WRITE_WORD_OR(SDIO_TRANX_MODE_REG, SDIO_MS_BLK_SEL_MASK);

        if(response_expected == RESPONSE_NEEDED) 
        {
            if(data_size)
            {
                temp_sta = (command << SDIO_CMD_IDX_SHIFT) | SDIO_CMD_RESP_SHORT | SDIO_CMD_IDX_CHK_EN_MASK | SDIO_CRC_CHK_EN_MASK | SDIO_DAT_PRES_SEL_MASK;
                SDIO_WRITE_WORD(SDIO_CMD_REG, temp_sta);
            }
            else
            {
                SDIO_WRITE_WORD(SDIO_CMD_REG, ((command << SDIO_CMD_IDX_SHIFT) | SDIO_CMD_RESP_SHORT | SDIO_CMD_IDX_CHK_EN_MASK | SDIO_CRC_CHK_EN_MASK));
            }
        }
        else
        {
            if(data_size)
            {
                temp_sta = (command << SDIO_CMD_IDX_SHIFT) | SDIO_CMD_IDX_CHK_EN_MASK | SDIO_DAT_PRES_SEL_MASK | SDIO_CRC_CHK_EN_MASK;
                SDIO_WRITE_WORD(SDIO_CMD_REG, temp_sta);
            }
            else
                SDIO_WRITE_WORD(SDIO_CMD_REG, ((command << SDIO_CMD_IDX_SHIFT) | SDIO_CMD_IDX_CHK_EN_MASK | SDIO_CRC_CHK_EN_MASK));
        }

        loop_count = (uint) COMMAND_FINISHED_CMD52_TIMEOUT_LOOPS;

        do 
        {
            temp_sta = SDIO_READ_WORD(SDIO_NORM_STA_REG);
            if (temp_sta & SDIO_ERR_INT_MASK)
            {
                temp_sta = SDIO_READ_WORD(SDIO_ERR_STA_REG);
                loop_count--;
                if ( loop_count == 0 || ((response_expected == RESPONSE_NEEDED ) && ( temp_sta != 0)) )
                {
                    sdio_reset(0x02 | 0x04);
                    goto restart;
                }
            }
        } while ((temp_sta & SDIO_CMD_CMPL_MASK) != SDIO_CMD_CMPL_MASK);

        SDIO_WRITE_WORD(SDIO_NORM_STA_REG, SDIO_CMD_CMPL_MASK);

        int rsp_temp;
        if ( response != NULL )
            rsp_temp = *response = SDIO_READ_DWORD(SDIO_RESP1_REG);
        else
            rsp_temp = SDIO_READ_DWORD(SDIO_RESP1_REG);

        do {
            temp_sta = SDIO_READ_WORD(SDIO_NORM_STA_REG);
            if (temp_sta & SDIO_ERR_INT_MASK)
            {
                SDIO_WRITE_WORD(SDIO_NORM_STA_REG, temp_sta);
                temp_sta = SDIO_READ_WORD(SDIO_ERR_STA_REG);
                SDIO_WRITE_WORD(SDIO_ERR_STA_REG, temp_sta);

                loop_count--;
                if ( loop_count == 0 || ((response_expected == RESPONSE_NEEDED ) && ( temp_sta != 0)) )
                {
                    temp_sta = SDIO_READ_WORD(SDIO_ERR_STA_REG);
                    sdio_reset(0x02 | 0x04);
                    goto restart;
                }
            }

            if(temp_sta & SDIO_DMA_INT_MASK) /* update dma address */
            {
                SDIO_WRITE_WORD_OR(SDIO_NORM_STA_REG, SDIO_DMA_INT_MASK);
                tmpval = SDIO_READ_DWORD(SDIO_SDMA_ADDR_REG);
                remain_blk_cnt = SDIO_READ_WORD(SDIO_BLK_CNT_REG);
                tmpval += (total_blk_cnt - remain_blk_cnt) * block_size;
                SDIO_WRITE_DWORD(SDIO_SDMA_ADDR_REG, tmpval);
                total_blk_cnt = remain_blk_cnt;
            }

        } while ((temp_sta & SDIO_TRANX_CMPL_MASK) != SDIO_TRANX_CMPL_MASK);

        SDIO_WRITE_WORD(SDIO_NORM_STA_REG, temp_sta);

        if ( direction == BUS_READ )
            memcpy( user_data, dma_data_source, (size_t) user_data_size );
    }
    else
    {
        /* Send the command */
        SDIO_WRITE_DWORD(SDIO_ARG_REG, argument);
        
        if(response_expected == RESPONSE_NEEDED) 
            SDIO_WRITE_WORD(SDIO_CMD_REG, ((command << SDIO_CMD_IDX_SHIFT) | SDIO_CMD_RESP_SHORT | SDIO_CMD_IDX_CHK_EN_MASK | SDIO_CRC_CHK_EN_MASK));
        else
            SDIO_WRITE_WORD(SDIO_CMD_REG, ((command << SDIO_CMD_IDX_SHIFT) | SDIO_CMD_IDX_CHK_EN_MASK | SDIO_CRC_CHK_EN_MASK));

        loop_count = (uint) COMMAND_FINISHED_CMD52_TIMEOUT_LOOPS;
        do {
            temp_sta = SDIO_READ_WORD(SDIO_NORM_STA_REG);
            if (temp_sta & SDIO_ERR_INT_MASK)
            {
                temp_sta = SDIO_READ_WORD(SDIO_ERR_STA_REG);
                loop_count--;
                if ( loop_count == 0 || ((response_expected == RESPONSE_NEEDED ) && ( temp_sta != 0)) )
                {
                    sdio_reset(0x02 | 0x04);
                    goto restart;
                }
            }
        } while ((temp_sta & SDIO_CMD_CMPL_MASK) != SDIO_CMD_CMPL_MASK);

        SDIO_WRITE_WORD(SDIO_NORM_STA_REG, (0x40 | 0x80 | SDIO_CMD_CMPL_MASK));
    }

    if ( response != NULL )
        *response = SDIO_READ_DWORD(SDIO_RESP1_REG);

    result = 0;

exit:
    return result;
}

