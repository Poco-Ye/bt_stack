#ifndef BCM5892_ADC_H
#define BCM5892_ADC_H


/* ---- Constants and Types ---------------------------------------------- */
#define ADC_MAJOR (241)
#define ADC_NUM_STATIC_CHANNELS (8)

#define ADC_MAJOR_STAT (ADC_MAJOR)
#define ADC_MAJOR_SCAN (ADC_MAJOR)
#define ADC_MINOR_STAT (0)
#define ADC_MINOR_SCAN (ADC_NUM_STATIC_CHANNELS) /* 8, so it goes after the static ones */

/* Static ADC registers */
#define ADC_REG_ST_CONFIG      (ADC0_REG_BASE_ADDR+0x00)
#define ADC_REG_ST_RAW_INT_STA (ADC0_REG_BASE_ADDR+0x04)
#define ADC_REG_ST_MSK_INT_STA (ADC0_REG_BASE_ADDR+0x08)
#define ADC_REG_ST_INT_EN      (ADC0_REG_BASE_ADDR+0x0c)
#define ADC_REG_ST_INT_CLR     (ADC0_REG_BASE_ADDR+0x10)
#define ADC_REG_ST_DATA        (ADC0_REG_BASE_ADDR+0x14)

#define ADC_REG_ST_CONFIG_OFF   (0x000)
#define ADC_REG_ST_CONFIG_PWR   (0x002)
#define ADC_REG_ST_CONFIG_EN    (0x001)
#define ADC_REG_ST_CONFIG_CHANNEL(i) ((i)<<3)
#define ADC_REG_ST_CONFIG_CLKDIV16 (0x080)
#define ADC_REG_ST_CONFIG_DIV_EN   (0x100)
#define ADC_REG_ST_CONFIG_DIV_SRST (0x200)
#define ADC_REG_ST_CONFIG_RESET    (0x400)

#define ADC_REG_ST_CONFIG_READY(i) (ADC_REG_ST_CONFIG_CHANNEL(i) | ADC_REG_ST_CONFIG_CLKDIV16 | ADC_REG_ST_CONFIG_DIV_EN | ADC_REG_ST_CONFIG_PWR)
#define ADC_REG_ST_CONFIG_GO(i) (ADC_REG_ST_CONFIG_CHANNEL(i) | ADC_REG_ST_CONFIG_CLKDIV16 | ADC_REG_ST_CONFIG_DIV_EN | ADC_REG_ST_CONFIG_PWR | ADC_REG_ST_CONFIG_EN)

#define ADC_REG_ST_INT_TRIGGER   (0x01)
#define ADC_REG_ST_INT_CONV_DONE (0x02)

/* Scanning ADC registers */

#define ADC_REG_SC_CONFIG      (ADC1_REG_BASE_ADDR   + 0x00)
#define ADC_REG_SC_RAW_INT_STA (ADC1_REG_BASE_ADDR   + 0x04)
#define ADC_REG_SC_MSK_INT_STA (ADC1_REG_BASE_ADDR   + 0x08)
#define ADC_REG_SC_INT_EN      (ADC1_REG_BASE_ADDR   + 0x0c)
#define ADC_REG_SC_INT_CLR     (ADC1_REG_BASE_ADDR   + 0x10)
#define ADC_REG_SC_DATA0       (ADC1_REG_BASE_ADDR   + 0x14)
#define ADC_REG_SC_DATA1       (ADC1_REG_BASE_ADDR   + 0x18)
#define ADC_REG_SC_DATA2       (ADC1_REG_BASE_ADDR   + 0x1c)
#define ADC_REG_SC_SCAN_RATE   (ADC1_REG_BASE_ADDR   + 0x20)
#define ADC_REG_SC_NUM_SCANS   (ADC1_REG_BASE_ADDR   + 0x24)
#define ADC_REG_SC_THRESHOLD0  (ADC1_REG_BASE_ADDR   + 0x28)
#define ADC_REG_SC_THRESHOLD1  (ADC1_REG_BASE_ADDR   + 0x2c)


#define ADC_REG_SC_CONFIG_OFF 0

#define ADC_REG_SC_CONFIG_EN(i) (1 << i)
#define ADC_REG_SC_CONFIG_CHAN_ALL_EN (0x00000007)
#define ADC_REG_SC_CONFIG_CHAN3_EN (0x00000020)
#define ADC_REG_SC_CONFIG_DED_CHAN_EN (0x00000010)
#define ADC_REG_SC_CONFIG_MSR_TDM_MUX_EN (0x00004000)
#define ADC_REG_SC_CONFIG_CONTINUOUS (0x00010000)
#define ADC_REG_SC_CONFIG_RESET (0x000e0000)
#define ADC_REG_SC_CONFIG_DIVVAL(idx) (((unsigned []) {8, 12, 16, 24})[(idx)])
#define ADC_REG_SC_CONFIG_DIVIDX(idx)   ((idx) << 22)
#define ADC_REG_SC_CONFIG_DIV_EN   (0x01000000)
#define ADC_REG_SC_CONFIG_DIV_SRST (0x02000000)
#define ADC_REG_SC_CONFIG_CHAN_CLR_EN (0x00001000)

#define ADC_REG_SC_CONFIG_PWR(i) (1 << (27 + (i)))
#define ADC_REG_SC_CONFIG_PWR_ALL (0x39000000)

#define ADC_REG_SC_INT_ALL (0x00770077)
#define ADC_REG_SC_INT_CONV_DONE (0x00700000)

#define ADC_REG_SC_INT_THERSHOLD_LOW	(0x00000070)
#define ADC_REG_SC_INT_THERSHOLD_HIGH	(0x00000007)

#define ADC_REG_SC_SCAN_RATE_VAL(a,b,c) ((a) | ((b) << 8) | ((c) << 16))
#define ADC_REG_SC_NUM_SCANS_VAL(a,b,c) ((a) | ((b) << 8) | ((c) << 16))


/*---------------- Accessor utility macros ------------------------*/
#define ADDRESS_WRITE(addr,data)  *((volatile unsigned int *)addr) = (unsigned int)(data)
#define ADDRESS_READ(addr)     *((volatile unsigned int *)addr)

#define ADC_REG(reg)  *(volatile unsigned int *)(0x000C6000+ reg)

#define ADC_WRITE_REG(val, reg) *((volatile unsigned int *)(0x000C6000+ reg))=(unsigned int)(val)

/* #define ADC_WRITE_REG(val, reg) PRINTK(KERN_INFO "Writing %08x to %p\n", (val), (IO_ADDRESS(bcm5892_adc_base) + reg)); iowrite32(val, IO_ADDRESS(bcm5892_adc_base) + reg) */
/* #define ADC_OR_REG(val, reg) PRINTK(KERN_INFO "Writing %08x to %p\n", (ioread32(IO_ADDRESS(bcm5892_adc_base) + reg) | val), (IO_ADDRESS(bcm5892_adc_base) + reg)); iowrite32(ioread32(IO_ADDRESS(bcm5892_adc_base) + reg) | val, IO_ADDRESS(bcm5892_adc_base) + reg) */

#endif /* #ifndef BCM5892_ADC_H */

