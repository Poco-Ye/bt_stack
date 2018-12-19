#include "base.h"

/* -- declaration -- */
#define BCM5892_INTR_CLR_RESET		(0x0001FFFB)
#define BCM5892_RESET				(0x00000000)

#define BCM5892_ETH_CTRL			(START_ETH_CFG+0x0000)
#define BCM5892_INTR_MASK			(START_ETH_CFG+0x0004)
#define BCM5892_INTR	 			(START_ETH_CFG+0x0008)
#define BCM5892_INTR_RAW			(START_ETH_CFG+0x000C)
#define BCM5892_INTR_CLR			(START_ETH_CFG+0x0010)
#define BCM5892_MC_ADDRF_0			(START_ETH_CFG+0x0020)
#define BCM5892_MC_ADDRF_1			(START_ETH_CFG+0x0024)
#define BCM5892_PHY_CTRL			(START_ETH_CFG+0x0040)
#define BCM5892_RBUFFSZ	 			(START_ETH_CFG+0x0104)
#define BCM5892_RBASE	 			(START_ETH_CFG+0x0110)
#define BCM5892_RBCFG	 			(START_ETH_CFG+0x0114)
#define BCM5892_RBDPTR	 			(START_ETH_CFG+0x0118)
#define BCM5892_RSWPTR	 			(START_ETH_CFG+0x011c)
#define BCM5892_TBASE				(START_ETH_CFG+0x0210)
#define BCM5892_TBCFG				(START_ETH_CFG+0x0214)
#define BCM5892_TBDPTR				(START_ETH_CFG+0x0218)
#define BCM5892_TSWPTR				(START_ETH_CFG+0x021c)
#define BCM5892_MAC_BP				(START_ETH_CFG+0x0404)
#define BCM5892_MAC_CFG				(START_ETH_CFG+0x0408)
#define BCM5892_MAC_ADDR0			(START_ETH_CFG+0x040c)
#define BCM5892_MAC_ADDR1			(START_ETH_CFG+0x0410)
#define BCM5892_MAX_FRM				(START_ETH_CFG+0x0414)
#define BCM5892_MAC_PQ				(START_ETH_CFG+0x0418)
#define BCM5892_MAC_MODE_SR			(START_ETH_CFG+0x0440)
#define BCM5892_MIN_TX_IPG			(START_ETH_CFG+0x045c)

#define BCM5892_MAC_CFG_TXEN        (0x00000001)
#define BCM5892_MAC_CFG_RXEN        (0x00000002)
#define BCM5892_MAC_CFG_SPD_100     (0x00000004)
#define BCM5892_MAC_CFG_SPD_10      (0x00000000)
#define BCM5892_MAC_CFG_PROM        (0x00000010)
#define BCM5892_MAC_CFG_RXPAD       (0x00000020)
#define BCM5892_MAC_CFG_CFWD        (0x00000040) /* Terminate/Forward received CRC. "uiModeFlags"*/
#define BCM5892_MAC_CFG_PFWD        (0x00000080)
#define BCM5892_MAC_CFG_PDIS        (0x00000100)
#define BCM5892_MAC_CFG_TXAD        (0x00000200)
#define BCM5892_MAC_CFG_HDEN        (0x00000400)
#define BCM5892_MAC_CFG_OFEN        (0x00001000)
#define BCM5892_MAC_CFG_SRST        (0x00002000)
#define BCM5892_MAC_CFG_LLB         (0x00008000) /* Local loopback Mode. "uiModeFlags"*/
#define BCM5892_MAC_CFG_ACFG        (0x00400000)
#define BCM5892_MAC_CFG_CFA         (0x00800000) /* Control frame enable. "uiModeFlags"*/
#define BCM5892_MAC_CFG_NOLC        (0x01000000)
#define BCM5892_MAC_CFG_RLB         (0x02000000) /* Remote loopback Mode. "uiModeFlags"*/
#define BCM5892_MAC_CFG_TPD         (0x10000000)
#define BCM5892_MAC_CFG_TXRX        (0x20000000)
#define BCM5892_MAC_CFG_RFLT        (0x40000000)

#define BCM5892_INTR_RAW_PHY        (0x00010000)
#define BCM5892_INTR_RAW_RMRK       (0x00008000)
#define BCM5892_INTR_RAW_TMRK       (0x00004000)
#define BCM5892_INTR_RAW_RHLT       (0x00002000)
#define BCM5892_INTR_RAW_THLT       (0x00001000)
#define BCM5892_INTR_RAW_ROV        (0x00000800)
#define BCM5892_INTR_RAW_TUN        (0x00000400)
#define BCM5892_INTR_RAW_TEC        (0x00000200)
#define BCM5892_INTR_RAW_TLC        (0x00000100)
#define BCM5892_INTR_RAW_RXB        (0x00000080)
#define BCM5892_INTR_RAW_TXB        (0x00000040)
#define BCM5892_INTR_RAW_RXF        (0x00000020)
#define BCM5892_INTR_RAW_TXF        (0x00000010)
#define BCM5892_INTR_RAW_BERR       (0x00000008)
#define BCM5892_INTR_RAW_MIIM       (0x00000004)
#define BCM5892_INTR_RAW_GRSC       (0x00000002)
#define BCM5892_INTR_RAW_GTSC       (0x00000001)

#define BCM5892_INTR_CLR_PHY        (0x00010000)
#define BCM5892_INTR_CLR_RMRK       (0x00008000)
#define BCM5892_INTR_CLR_TMRK       (0x00004000)
#define BCM5892_INTR_CLR_RHLT       (0x00002000)
#define BCM5892_INTR_CLR_THLT       (0x00001000)
#define BCM5892_INTR_CLR_ROV        (0x00000800)
#define BCM5892_INTR_CLR_TUN        (0x00000400)
#define BCM5892_INTR_CLR_TEC        (0x00000200)
#define BCM5892_INTR_CLR_TLC        (0x00000100)
#define BCM5892_INTR_CLR_RXB        (0x00000080)
#define BCM5892_INTR_CLR_TXB        (0x00000040)
#define BCM5892_INTR_CLR_RXF        (0x00000020)
#define BCM5892_INTR_CLR_TXF        (0x00000010)
#define BCM5892_INTR_CLR_BERR       (0x00000008)
#define BCM5892_INTR_CLR_MIIM       (0x00000004)
#define BCM5892_INTR_CLR_GRSC       (0x00000002)
#define BCM5892_INTR_CLR_GTSC       (0x00000001)

#define BCM5892_INTR_PHY            (0x00010000)
#define BCM5892_INTR_RMRK           (0x00008000)
#define BCM5892_INTR_TMRK           (0x00004000)
#define BCM5892_INTR_RHLT           (0x00002000)
#define BCM5892_INTR_THLT           (0x00001000)
#define BCM5892_INTR_ROV            (0x00000800)
#define BCM5892_INTR_TUN            (0x00000400)
#define BCM5892_INTR_TEC            (0x00000200)
#define BCM5892_INTR_TLC            (0x00000100)
#define BCM5892_INTR_RXB            (0x00000080)
#define BCM5892_INTR_TXB            (0x00000040)
#define BCM5892_INTR_RXF            (0x00000020)
#define BCM5892_INTR_TXF            (0x00000010)
#define BCM5892_INTR_BERR           (0x00000008)
#define BCM5892_INTR_MIIM           (0x00000004)
#define BCM5892_INTR_GRSC           (0x00000002)
#define BCM5892_INTR_GTSC           (0x00000001)

#define BCM5892_ETH_CTRL_MEN		(0x0008)
#define BCM5892_ETH_CTRL_COR		(0x0004)
#define BCM5892_ETH_CTRL_GRS		(0x0002)
#define BCM5892_ETH_CTRL_GTS		(0x0001)

#define BCM5892_ETH_CTRL_BCR		(0x1000)
#define BCM5892_ETH_CTRL_FBP		(0x2000)
#define BCM5892_ETH_CTRL_BE			(0x4000)

#define BCM5892_TX_PAUSE_CTRL		(0x0730)
#define BCM5892_TX_FIFO_FLUSH		(0x0734)
#define BCM5892_RX_FIFO_STAT		(0x0738)
#define BCM5892_TX_FIFO_STAT		(0x073c)
#define BCM5892_TX_FIFO_MACTXFE		(0x0424)
#define BCM5892_TX_FIFO_MACTXFF		(0x0428)
#define BCM5892_RX_FIFO_MACRXFE		(0x041C)
#define BCM5892_RX_FIFO_MACRXFF		(0x0420)

#define BCM5892_TX_BD_SOP	        (0x80000000)
#define BCM5892_TX_BD_EOP	        (0x40000000)
#define BCM5892_TX_BD_PAD	        (0x00800000)
#define BCM5892_TX_BD_CAP	        (0x00400000)
#define BCM5892_TX_BD_CRP	        (0x00200000)
#define BCM5892_TX_BD_XDEF	        (0x00080000)
#define BCM5892_TX_BD_XCOL	        (0x00040000)
#define BCM5892_TX_BD_LCOL	        (0x00020000)
#define BCM5892_TX_BD_UN	        (0x00010000)

#define BCM5892_RX_BD_SOP	        (0x80000000)
#define BCM5892_RX_BD_EOP	        (0x40000000)
#define BCM5892_RX_BD_BC	        (0x00800000)
#define BCM5892_RX_BD_MC	        (0x00400000)
#define BCM5892_RX_BD_NO	        (0x00200000)
#define BCM5892_RX_BD_TC	        (0x00080000)
#define BCM5892_RX_BD_ER	        (0x00080000)
#define BCM5892_RX_BD_OF	        (0x00040000)


/* -- declaration -- END!! -- */

#define DEFAULT_RX_BUF_SIZE		1536 /*4 bytes for CRC*/
#define DEFAULT_RX_WATERMARK	5

#define DEFAULT_TX_BUF_SIZE		1536 /* 4 bytes for CRC*/
#define DEFAULT_TX_WATERMARK	5

#define MDIO_INTERNAL_PHY		2 	/* Internal Phy. */
#define MDIO_EXTERNAL_PHY		3 	/* External Phy. */

#define ETH_PACKET_SIZE       1518


#define DMA_TXBUF_SZ (1536) /*  size can NOT less than frame size cause it sent out as a whole frame */
#define DMA_RXBUF_SZ (1536) /*  can twist the size, but must 8 bytes aligned!!! */
#define DESCRIP_SIZE 8
#define DMA_TXBD_MAX 64 
#define DMA_RXBD_MAX 128

enum ETH_MODE {ETH_AUTO=0,ETH_10M_HALF,ETH_100M_HALF,ETH_10M_FULL,ETH_100M_FULL};

/* -- DMA BUFFER DESCRIPTOR DATA -- */
typedef struct bd {
	uint flag;
	uchar  *pdata;
}bd_t;

struct txbd {
	bd_t bds[DMA_TXBD_MAX];
	uchar dma_buf[DMA_TXBD_MAX][DMA_TXBUF_SZ];
}__attribute__ ((aligned (8)));

struct rxbd{
	bd_t bds[DMA_RXBD_MAX];
	uchar dma_buf[DMA_RXBD_MAX][DMA_RXBUF_SZ];
}__attribute__ ((aligned (8)));


/* flag of DMA buffer descriptor */
#define is_desc_sop_rx(flag)	((flag)&BCM5892_RX_BD_SOP)
#define is_desc_eop_rx(flag)	((flag)&BCM5892_RX_BD_EOP)
#define is_desc_bc_rx(flag)		((flag)&BCM5892_RX_BD_BC)
#define is_desc_mc_rx(flag)		((flag)&BCM5892_RX_BD_MC)
#define is_desc_er_rx(flag)		((flag)&BCM5892_RX_BD_ER)

#define ptbds ((struct txbd*)(DMA_ETH_BASE)) /*point to transfer buffer descriptions base*/
#define prbds ((struct rxbd*)(DMA_ETH_BASE+0x20000)) /*point to receive buffer descriptions base*/

#define addr_tbdesc(idx) (bd_t*)(((uint)(ptbds))+ (idx)*DESCRIP_SIZE)
#define addr_rbdesc(idx) (bd_t*)(((uint)(prbds))+ (idx)*DESCRIP_SIZE)
#define tbd_idx(addr_tbdsec) ((((uint)(addr_tbdsec))- ((uint)(ptbds)))/DESCRIP_SIZE)
#define rbd_idx(addr_rbdsec) ((((uint)(addr_rbdsec))-((uint)(prbds)))/DESCRIP_SIZE)

