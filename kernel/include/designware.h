#ifndef DESIGNWARE_H
#define DESIGNWARE_H

#include "../types.h"
#include "net.h"

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
	char rxbuffs[RX_TOTAL_BUFSIZE] __attribute__((aligned(ARCH_DMA_MINALIGN)));

	uint32_t interface;
	uint32_t max_speed;
	uint32_t tx_currdescnum;
	uint32_t rx_currdescnum;

	struct eth_mac_regs *mac_regs_p;
	struct eth_dma_regs *dma_regs_p;
	struct phy_device *phydev;
	struct mii_dev *bus;
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

int designware_eth_write_hwaddr();

#endif // DESIGNWARE_H
