#ifndef DESIGNWARE_H
#define DESIGNWARE_H

#include "../types.h"
#include "net.h"
#include "../spinlock.h"

#define CONFIG_SYS_HZ 1000
#define CACHE_LINE_SIZE 64  // C906 typically has 64-byte cache lines
#define ARCH_DMA_MINALIGN CACHE_LINE_SIZE

#define CONFIG_TX_DESCR_NUM	16
#define CONFIG_RX_DESCR_NUM	16
#define CONFIG_ETH_BUFSIZE	2048
#define TX_TOTAL_BUFSIZE	(CONFIG_ETH_BUFSIZE * CONFIG_TX_DESCR_NUM)
#define RX_TOTAL_BUFSIZE	(CONFIG_ETH_BUFSIZE * CONFIG_RX_DESCR_NUM)

#define CONFIG_MACRESET_TIMEOUT	(3 * CONFIG_SYS_HZ)
#define CONFIG_MDIO_TIMEOUT	(3 * CONFIG_SYS_HZ)

#define DW_DMA_BASE_OFFSET	(0x1000)

/* Default DMA Burst length */
#ifndef CONFIG_DW_GMAC_DEFAULT_DMA_PBL
#define CONFIG_DW_GMAC_DEFAULT_DMA_PBL 8
#endif

/* Bus mode register definitions */
#define FIXEDBURST		(1 << 16)
#define PRIORXTX_41		(3 << 14)
#define PRIORXTX_31		(2 << 14)
#define PRIORXTX_21		(1 << 14)
#define PRIORXTX_11		(0 << 14)
#define DMA_PBL			(CONFIG_DW_GMAC_DEFAULT_DMA_PBL<<8)
#define RXHIGHPRIO		(1 << 1)
#define DMAMAC_SRST		(1 << 0)

/* Poll demand definitions */
#define POLL_DATA		(0xFFFFFFFF)

/* Operation mode definitions */
#define STOREFORWARD		(1 << 21)
#define FLUSHTXFIFO		(1 << 20)
#define TXSTART			(1 << 13)
#define TXSECONDFRAME		(1 << 2)
#define RXSTART			(1 << 1)

/* Descriptior related definitions */
#define MAC_MAX_FRAME_SZ	(1600)

/* MAC configuration register definitions */
#define FRAMEBURSTENABLE	(1 << 21)
#define MII_PORTSELECT		(1 << 15)
#define FES_100			(1 << 14)
#define DISABLERXOWN		(1 << 13)
#define FULLDPLXMODE		(1 << 11)
#define RXENABLE		(1 << 2)
#define TXENABLE		(1 << 3)

/* MII address register definitions */
#define MII_BUSY		(1 << 0)
#define MII_WRITE		(1 << 1)
#define MII_CLKRANGE_60_100M	(0)
#define MII_CLKRANGE_100_150M	(0x4)
#define MII_CLKRANGE_20_35M	(0x8)
#define MII_CLKRANGE_35_60M	(0xC)
#define MII_CLKRANGE_150_250M	(0x10)
#define MII_CLKRANGE_250_300M	(0x14)

#define MIIADDRSHIFT		(11)
#define MIIREGSHIFT		(6)
#define MII_REGMSK		(0x1F << 6)
#define MII_ADDRMSK		(0x1F << 11)


/* tx control bits definitions */

#define DESC_TXCTRL_TXINT		(1 << 31)
#define DESC_TXCTRL_TXLAST		(1 << 30)
#define DESC_TXCTRL_TXFIRST		(1 << 29)
#define DESC_TXCTRL_TXCHECKINSCTRL	(3 << 27)
#define DESC_TXCTRL_TXCRCDIS		(1 << 26)
#define DESC_TXCTRL_TXRINGEND		(1 << 25)
#define DESC_TXCTRL_TXCHAIN		(1 << 24)

#define DESC_TXCTRL_SIZE1MASK		(0x7FF << 0)
#define DESC_TXCTRL_SIZE1SHFT		(0)
#define DESC_TXCTRL_SIZE2MASK		(0x7FF << 11)
#define DESC_TXCTRL_SIZE2SHFT		(11)


/* rx control bits definitions */
#define DESC_RXCTRL_RXINTDIS		(1 << 31)
#define DESC_RXCTRL_RXRINGEND		(1 << 25)
#define DESC_RXCTRL_RXCHAIN		(1 << 24)

#define DESC_RXCTRL_SIZE1MASK		(0x7FF << 0)
#define DESC_RXCTRL_SIZE1SHFT		(0)
#define DESC_RXCTRL_SIZE2MASK		(0x7FF << 11)
#define DESC_RXCTRL_SIZE2SHFT		(11)

/* tx status bits definitions */
#define DESC_TXSTS_OWNBYDMA		(1 << 31)
#define DESC_TXSTS_MSK			(0x1FFFF << 0)

/* rx status bits definitions */
#define DESC_RXSTS_OWNBYDMA		(1 << 31)
#define DESC_RXSTS_DAFILTERFAIL		(1 << 30)
#define DESC_RXSTS_FRMLENMSK		(0x3FFF << 16)
#define DESC_RXSTS_FRMLENSHFT		(16)

#define DESC_RXSTS_ERROR		(1 << 15)
#define DESC_RXSTS_RXTRUNCATED		(1 << 14)
#define DESC_RXSTS_SAFILTERFAIL		(1 << 13)
#define DESC_RXSTS_RXIPC_GIANTFRAME	(1 << 12)
#define DESC_RXSTS_RXDAMAGED		(1 << 11)
#define DESC_RXSTS_RXVLANTAG		(1 << 10)
#define DESC_RXSTS_RXFIRST		(1 << 9)
#define DESC_RXSTS_RXLAST		(1 << 8)
#define DESC_RXSTS_RXIPC_GIANT		(1 << 7)
#define DESC_RXSTS_RXCOLLISION		(1 << 6)
#define DESC_RXSTS_RXFRAMEETHER		(1 << 5)
#define DESC_RXSTS_RXWATCHDOG		(1 << 4)
#define DESC_RXSTS_RXMIIERROR		(1 << 3)
#define DESC_RXSTS_RXDRIBBLING		(1 << 2)
#define DESC_RXSTS_RXCRC		(1 << 1)


/* DMA interrupt bit */
// DMA Interrupt Enable Register bits
#define DMA_INTR_ENA_NIE    (1 << 16)  // Normal Interrupt Summary Enable
#define DMA_INTR_ENA_AIE    (1 << 15)  // Abnormal Interrupt Summary Enable
#define DMA_INTR_ENA_RIE    (1 << 6)   // Receive Interrupt Enable
#define DMA_INTR_ENA_TIE    (1 << 0)   // Transmit Interrupt Enable


#define ETH_ZLEN 60

struct eth_dma_regs {
	uint32_t busmode;		/* 0x00 */
	uint32_t txpolldemand;	/* 0x04 */
	uint32_t rxpolldemand;	/* 0x08 */
	uint32_t rxdesclistaddr;	/* 0x0c */
	uint32_t txdesclistaddr;	/* 0x10 */
	uint32_t status;		/* 0x14 */
	uint32_t opmode;		/* 0x18 */
	uint32_t intenable;		/* 0x1c */
	uint32_t reserved1[2];
	uint32_t axibus;		/* 0x28 */
	uint32_t reserved2[7];
	uint32_t currhosttxdesc;	/* 0x48 */
	uint32_t currhostrxdesc;	/* 0x4c */
	uint32_t currhosttxbuffaddr;	/* 0x50 */
	uint32_t currhostrxbuffaddr;	/* 0x54 */
};


struct eth_mac_regs {
	uint32_t conf;		/* 0x00 */
	uint32_t framefilt;		/* 0x04 */
	uint32_t hashtablehigh;	/* 0x08 */
	uint32_t hashtablelow;	/* 0x0c */
	uint32_t miiaddr;		/* 0x10 */
	uint32_t miidata;		/* 0x14 */
	uint32_t flowcontrol;	/* 0x18 */
	uint32_t vlantag;		/* 0x1c */
	uint32_t version;		/* 0x20 */
	u8 reserved_1[20];
	uint32_t intreg;		/* 0x38 */
	uint32_t intmask;		/* 0x3c */
	uint32_t macaddr0hi;		/* 0x40 */
	uint32_t macaddr0lo;		/* 0x44 */
};


struct dmamacdescr {
	uint32_t txrx_status;
	uint32_t dmamac_cntl;
	uint32_t dmamac_addr;
	uint32_t dmamac_next;
} __attribute__((aligned(ARCH_DMA_MINALIGN)));

struct dw_eth_dev {
	struct dmamacdescr tx_mac_descrtable[CONFIG_TX_DESCR_NUM];
	struct dmamacdescr rx_mac_descrtable[CONFIG_RX_DESCR_NUM];
	char txbuffs[TX_TOTAL_BUFSIZE] __attribute__((aligned(ARCH_DMA_MINALIGN)));
	char *rxbuffs[CONFIG_RX_DESCR_NUM] __attribute__((aligned(ARCH_DMA_MINALIGN)));

	uint32_t interface;
	uint32_t max_speed;
	uint32_t tx_currdescnum;
	uint32_t rx_currdescnum;

	struct eth_mac_regs *mac_regs_p;
	struct eth_dma_regs *dma_regs_p;
	struct phy_device *phydev;
	struct mii_dev *bus;

	/* Locking mechanism */
	struct spinlock eth_lock;
};


struct dw_eth_pdata {
	struct eth_pdata eth_pdata;
	uint32_t reset_delays[3];
};

/* Platform data of ethernet node on Devicetree */
extern struct eth_pdata pdata;

/* Private data of ethernet driver ~ designware */
extern struct dw_eth_dev priv;

/* Bus that connected MAC and PHY using MIDO */
extern struct mii_dev bus;

int designware_eth_start();
int designware_eth_send(void *packet, int length);
int designware_eth_recv(int flags, uchar **packetp);

int designware_eth_free_pkt(uchar *packet, int length);

void designware_eth_stop();

int designware_eth_write_hwaddr(uint8_t *enetaddr);

extern struct eth_ops designware_eth_ops;



// ARP packet structure
struct arp_packet {
    // Ethernet header (14 bytes)
    uint8_t  eth_dst[6];      // Destination MAC (broadcast for ARP request)
    uint8_t  eth_src[6];      // Source MAC (your MAC)
    uint16_t eth_type;        // 0x0806 for ARP
    
    // ARP header (28 bytes)
    uint16_t hw_type;         // Hardware type (1 = Ethernet)
    uint16_t proto_type;      // Protocol type (0x0800 = IPv4)
    uint8_t  hw_size;         // Hardware address size (6 for MAC)
    uint8_t  proto_size;      // Protocol address size (4 for IPv4)
    uint16_t opcode;          // 1 = request, 2 = reply
    uint8_t  sender_mac[6];   // Sender MAC address
    uint8_t  sender_ip[4];    // Sender IP address
    uint8_t  target_mac[6];   // Target MAC (00:00:00:00:00:00 for request)
    uint8_t  target_ip[4];    // Target IP address
} __attribute__((packed));

inline uint16_t htons(uint16_t n) {
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

void test_send_arp(void);
void check_mac_address(void);
void enable_promiscuous_mode(void);

#endif // DESIGNWARE_H
