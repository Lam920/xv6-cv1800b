#include "include/designware.h"
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include "list.h"
#include "sd.h"
#include "printf.h"
#include "file.h"
#include "cv180x_reg.h"
#include "io.h"
#include "include/phy_interface.h"
#include "include/phy.h"
#include "include/designware.h"
#include "include/mdio.h"
#include "include/mii.h"
#include "include/net.h"
#include "include/cache.h"
#include "include/ethtool.h"
#include "kernel/net/net.h"
#include "bitops.h"
#include "kernel/net/ip.h"

static struct spinlock rx_lock;

/* Queue for rx packets stored */
struct rx_queue rx_queue;

static int dw_mdio_read(struct mii_dev *bus, int addr, int devad, int reg);
static int dw_mdio_write(struct mii_dev *bus, int addr, int devad, int reg, u16 val);

/* Platform data of ethernet node on Devicetree */
struct eth_pdata pdata;

/* Private data of ethernet driver ~ designware */
struct dw_eth_dev priv;

/* Bus that connected MAC and PHY using MIDO */
struct mii_dev bus;

/* Structure that holds PHY information ~ reprensent a PHY device */
struct phy_device phydev;

static int phy_find_by_mask(struct mii_dev *bus, uint phy_mask,
				    phy_interface_t interface, struct phy_device *phydev);

static int get_phy_device_by_mask(struct mii_dev *bus,
						 uint phy_mask,
						 phy_interface_t interface, struct phy_device *phydev);

static int phy_device_create(struct mii_dev *bus, int addr,
					    u32 phy_id, bool is_c45,
					    phy_interface_t interface, struct phy_device *dev);

static int create_phy_by_mask(struct mii_dev *bus,
					     uint phy_mask, int devad,
					     phy_interface_t interface, struct phy_device *dev);

static int get_phy_id(struct mii_dev *bus, int addr, int devad, u32 *phy_id);

int phy_config(struct phy_device *phydev);
int genphy_config(struct phy_device *phydev);
int genphy_config_aneg(struct phy_device *phydev);
void phy_connect_dev(struct phy_device *phydev);

void phy_init(void) {
    phys_addr_t iobase = ETH0_BASE;

    /* Init designware ethernet private info */
    priv.mac_regs_p = (struct eth_mac_regs *)iobase;
    priv.dma_regs_p = (struct eth_dma_regs *)(iobase + DW_DMA_BASE_OFFSET);
    priv.max_speed = 0;
	priv.interface = PHY_INTERFACE_MODE_RMII;

	memcpy(priv.author, "OpenAI_ChatGPT", strlen("OpenAI_ChatGPT"));


    /* Init designware ethernet bus ~ dw_mdio_init*/
    bus.read = &dw_mdio_read;
    bus.write = &dw_mdio_write;
    bus.reset = NULL;
    memcmp(bus.name, "ethernet@0x40700000", strlen("ethernet@0x40700000"));
    bus.priv = &priv;
    priv.bus = &bus;

    int phy_addr = -1;
    uint mask = (phy_addr >= 0) ? (1 << phy_addr) : 0xffffffff;

	printf("[dw] mask: %x\n", mask);

    phy_find_by_mask(priv.bus, mask, priv.interface, &phydev);

	phy_connect_dev(&phydev);

	phydev.supported &= PHY_GBIT_FEATURES;
	phydev.advertising = phydev.supported;

	int ret = phy_config(&phydev);
	if (ret < 0) {
		printf("[dw] phy_config failed\n");
		return;
	}
	else {
		printf("[dw] phy_config success\n");
	}

	priv.phydev = &phydev;

}

int phy_config(struct phy_device *phydev)
{
	/* Invoke an optional board-specific helper */
	printf("[dw] do phy_config\n");
	return genphy_config(phydev);
}


int phy_reset(struct phy_device *phydev)
{
	int reg;
	int timeout = 500;
	int devad = MDIO_DEVAD_NONE;
	printf("[dw] phy_reset with devad: %d\n", devad);
	if (phydev->flags & PHY_FLAG_BROKEN_RESET)
	{
		printf("[dw] phy soft reset is not supported\n");
		return 0;
	}

	if (phy_write(phydev, devad, MII_BMCR, BMCR_RESET) < 0) {
		printf("PHY reset failed\n");
		return -1;
	}

	/*
	 * Poll the control register for the reset bit to go to 0 (it is
	 * auto-clearing).  This should happen within 0.5 seconds per the
	 * IEEE spec.
	 */
	reg = phy_read(phydev, devad, MII_BMCR);
	while ((reg & BMCR_RESET) && timeout--) {
		reg = phy_read(phydev, devad, MII_BMCR);

		if (reg < 0) {
			printf("PHY status read failed\n");
			return -1;
		}
		delayus(1000);
	}

	if (reg & BMCR_RESET) {
		printf("PHY reset timed out\n");
		return -1;
	}

	return 0;
}

void phy_connect_dev(struct phy_device *phydev)
{
	/* Soft Reset the PHY */
	printf("[dw] do soft reset PHY\n");
	phy_reset(phydev);
	printf("[dw] Done PHY_reset\n");
}

int genphy_config(struct phy_device *phydev)
{
	int val;
	u32 features;

	printf("[dw] do genphy_config\n");

	features = (SUPPORTED_TP | SUPPORTED_MII
			| SUPPORTED_AUI | SUPPORTED_FIBRE |
			SUPPORTED_BNC);

	/* Do we support autonegotiation? */
	val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);

	printf("[dw] %s val: %d\n", __func__, val);

	if (val < 0)
		return val;

	if (val & BMSR_ANEGCAPABLE)
		features |= SUPPORTED_Autoneg;

	if (val & BMSR_100FULL)
		features |= SUPPORTED_100baseT_Full;
	if (val & BMSR_100HALF)
		features |= SUPPORTED_100baseT_Half;
	if (val & BMSR_10FULL)
		features |= SUPPORTED_10baseT_Full;
	if (val & BMSR_10HALF)
		features |= SUPPORTED_10baseT_Half;

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MDIO_DEVAD_NONE, MII_ESTATUS);

		if (val < 0)
			return val;

		if (val & ESTATUS_1000_TFULL)
			features |= SUPPORTED_1000baseT_Full;
		if (val & ESTATUS_1000_THALF)
			features |= SUPPORTED_1000baseT_Half;
		if (val & ESTATUS_1000_XFULL)
			features |= SUPPORTED_1000baseX_Full;
		if (val & ESTATUS_1000_XHALF)
			features |= SUPPORTED_1000baseX_Half;
	}

	phydev->supported &= features;
	phydev->advertising &= features;

	genphy_config_aneg(phydev);

	return 0;
}


/**
 * genphy_config_advert - sanitize and advertise auto-negotiation parameters
 * @phydev: target phy_device struct
 *
 * Description: Writes MII_ADVERTISE with the appropriate values,
 *   after sanitizing the values to make sure we only advertise
 *   what is supported.  Returns < 0 on error, 0 if the PHY's advertisement
 *   hasn't changed, and > 0 if it has changed.
 */
static int genphy_config_advert(struct phy_device *phydev)
{
	printf("[dw] do genphy_config_advert\n");
	u32 advertise;
	int oldadv, adv, bmsr;
	int err, changed = 0;

	/* Only allow advertising what this PHY supports */
	phydev->advertising &= phydev->supported;
	advertise = phydev->advertising;

	/* Setup standard advertisement */
	adv = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
	printf("[dw] %s adv: %d\n", __func__, adv);
	oldadv = adv;

	if (adv < 0)
		return adv;

	/* Start with selector field (IEEE 802.3) */
    adv = ADVERTISE_CSMA;  // This is 0x0001
    
    /* Add supported capabilities */
    adv |= ADVERTISE_10HALF;
    adv |= ADVERTISE_10FULL;
    adv |= ADVERTISE_100HALF;
    adv |= ADVERTISE_100FULL;
    
    /* Add pause frame support if you want flow control */
    adv |= ADVERTISE_PAUSE_CAP;
    adv |= ADVERTISE_PAUSE_ASYM;
	

	printf("[dw] %s new adv: %d\n", __func__, adv);
	if (adv != oldadv) {
		err = phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE, adv);

		if (err < 0)
		{
			printf("[dw] PHY ADVERTISE write failed\n");
			return err;
		}
		changed = 1;
	}

	bmsr = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);
	if (bmsr < 0)
	{
		printf("[dw] PHY BMSR read failed\n");
		return bmsr;
	}

	printf("[dw] %s bmsr: %d\n", __func__, bmsr);

	/* Per 802.3-2008, Section 22.2.4.2.16 Extended status all
	 * 1000Mbits/sec capable PHYs shall have the BMSR_ESTATEN bit set to a
	 * logical 1.
	 */
	if (!(bmsr & BMSR_ESTATEN))
		return changed;

	/* Configure gigabit if it's supported */
	adv = phy_read(phydev, MDIO_DEVAD_NONE, MII_CTRL1000);
	oldadv = adv;

	if (adv < 0)
		return adv;

	adv &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);

	if (phydev->supported & (SUPPORTED_1000baseT_Half |
				SUPPORTED_1000baseT_Full)) {
		if (advertise & SUPPORTED_1000baseT_Half)
			adv |= ADVERTISE_1000HALF;
		if (advertise & SUPPORTED_1000baseT_Full)
			adv |= ADVERTISE_1000FULL;
	}

	if (adv != oldadv)
		changed = 1;

	err = phy_write(phydev, MDIO_DEVAD_NONE, MII_CTRL1000, adv);
	if (err < 0)
		return err;

	return changed;
}

/**
 * genphy_setup_forced - configures/forces speed/duplex from @phydev
 * @phydev: target phy_device struct
 *
 * Description: Configures MII_BMCR to force speed/duplex
 *   to the values in phydev. Assumes that the values are valid.
 */
static int genphy_setup_forced(struct phy_device *phydev)
{
	int err;
	int ctl = BMCR_ANRESTART;

	phydev->pause = 0;
	phydev->asym_pause = 0;

	if (phydev->speed == SPEED_1000)
		ctl |= BMCR_SPEED1000;
	else if (phydev->speed == SPEED_100)
		ctl |= BMCR_SPEED100;

	if (phydev->duplex == DUPLEX_FULL)
		ctl |= BMCR_FULLDPLX;

	err = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, ctl);

	return err;
}

/**
 * genphy_restart_aneg - Enable and Restart Autonegotiation
 * @phydev: target phy_device struct
 */

void debug_phy_status(struct phy_device *phydev) {
    int bmcr = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
    int bmsr = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);
    int anar = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
    int anlpar = phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);
    
    printf("=== PHY Status ===\n");
    printf("BMCR (0x00): 0x%04x\n", bmcr);
    printf("  - Reset: %s\n", (bmcr & BMCR_RESET) ? "YES" : "NO");
    printf("  - Loopback: %s\n", (bmcr & BMCR_LOOPBACK) ? "YES" : "NO");
    printf("  - AutoNeg Enable: %s\n", (bmcr & BMCR_ANENABLE) ? "YES" : "NO");
    printf("  - Isolate: %s\n", (bmcr & BMCR_ISOLATE) ? "YES" : "NO");
    
    printf("BMSR (0x01): 0x%04x\n", bmsr);
    printf("  - Link: %s\n", (bmsr & BMSR_LSTATUS) ? "UP" : "DOWN");
    printf("  - AutoNeg Complete: %s\n", (bmsr & BMSR_ANEGCOMPLETE) ? "YES" : "NO");
    printf("  - AutoNeg Capable: %s\n", (bmsr & BMSR_ANEGCAPABLE) ? "YES" : "NO");
    
    printf("ANAR (0x04): 0x%04x\n", anar);
    printf("ANLPAR (0x05): 0x%04x\n", anlpar);
}

int genphy_restart_aneg(struct phy_device *phydev)
{
	int ctl;

	ctl = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
    if (ctl & BMCR_RESET) {
        printf("PHY still in reset, waiting...\n");
        int timeout = 500; // 500ms
        while ((ctl & BMCR_RESET) && timeout > 0) {
            delayus(1000); // 1ms
            ctl = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
            timeout--;
        }
        if (ctl & BMCR_RESET) {
            printf("PHY reset timeout!\n");
            return -1;
        }
    }

	ctl = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);

	if (ctl < 0)
		return ctl;

	ctl |= (BMCR_ANENABLE | BMCR_ANRESTART);

	/* Don't isolate the PHY if we're negotiating */
	ctl &= ~(BMCR_ISOLATE);

	ctl = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, ctl);

	return ctl;
}


/**
 * genphy_config_aneg - restart auto-negotiation or write BMCR
 * @phydev: target phy_device struct
 *
 * Description: If auto-negotiation is enabled, we configure the
 *   advertising, and then restart auto-negotiation.  If it is not
 *   enabled, then we write the BMCR.
 */
int genphy_config_aneg(struct phy_device *phydev)
{
	int result;

	printf("[dw] do genphy_config_aneg with autoneg: %d\n", phydev->autoneg);

	if (phydev->autoneg != AUTONEG_ENABLE)
		return genphy_setup_forced(phydev);

	result = genphy_config_advert(phydev);

	if (result < 0) /* error */
	{
		printf("[dw] genphy_config_advert failed\n");
		return result;
	}

	if (result == 0) {
		/*
		 * Advertisment hasn't changed, but maybe aneg was never on to
		 * begin with?  Or maybe phy was isolated?
		 */
		int ctl = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);

		printf("[dw] ctl: %d\n", ctl);

		if (ctl < 0)
			return ctl;

		if (!(ctl & BMCR_ANENABLE) || (ctl & BMCR_ISOLATE))
			result = 1; /* do restart aneg */
	}

	/*
	 * Only restart aneg if we are advertising something different
	 * than we were before.
	 */
	if (result > 0)
		result = genphy_restart_aneg(phydev);

	return result;
}


static int phy_find_by_mask(struct mii_dev *bus, uint phy_mask,
				    phy_interface_t interface, struct phy_device *phydev)
{
	/* Reset the bus */
	if (bus->reset) {
		printf("[dw]Resetting %s\n", bus->name);
		bus->reset(bus);

		/* Wait 15ms to make sure the PHY has come out of hard reset */
		delayms(15);
	}

	return get_phy_device_by_mask(bus, phy_mask, interface, phydev);
}

static int get_phy_device_by_mask(struct mii_dev *bus,
						 uint phy_mask,
						 phy_interface_t interface, struct phy_device *phydev)
{
	printf("[dw] get_phy_device_by_mask\n");
	/* try different access clauses  */
	
    printf("[dw] Try to create phy_by_mask \n");
    int ret = create_phy_by_mask(bus, phy_mask,
                    MDIO_DEVAD_NONE, interface, phydev);
    if (ret < 0)
	{
		printf("[dw] phydev cannot create :)) \n");
		return 0;
	}
	
	printf("not found\n");

	return -1;
}

static int create_phy_by_mask(struct mii_dev *bus,
					     uint phy_mask, int devad,
					     phy_interface_t interface, struct phy_device *dev)
{
	u32 phy_id = 0xffffffff;
	bool is_c45;

	while (phy_mask) {
		int addr = generic_ffs(phy_mask) - 1;
		printf("[dw] addr: %d\n", addr);
		int r = get_phy_id(bus, addr, devad, &phy_id);
		printf("[dw] Get phy_id: %d and return success with r: %d\n", phy_id, r);
		/*
		 * If the PHY ID is flat 0 we ignore it.  There are C45 PHYs
		 * that return all 0s for C22 reads (like Aquantia AQR112) and
		 * there are C22 PHYs that return all 0s for C45 reads (like
		 * Atheros AR8035).
		 */
		if (r == 0 && phy_id == 0)
			goto next;

		/* If the PHY ID is mostly f's, we didn't find anything */
		if (r == 0 && (phy_id & 0x1fffffff) != 0x1fffffff) {
			is_c45 = (devad == MDIO_DEVAD_NONE) ? false : true;
			printf("[dw]phy_device_create is_c45: %d\n", (int)is_c45);
			return phy_device_create(bus, addr, phy_id, is_c45,
						 interface, dev);
		}
next:
		phy_mask &= ~(1 << addr);
	}
	return -1;
}


static int phy_device_create(struct mii_dev *bus, int addr,
					    u32 phy_id, bool is_c45,
					    phy_interface_t interface, struct phy_device *dev)
{

	if (!dev) {
		printf("Failed to allocate PHY device for %s:%d\n",
		       bus ? bus->name : "(null bus)", addr);
		return -1;
	}

	memset(dev, 0, sizeof(*dev));

	dev->duplex = -1;
	dev->link = 0;
	dev->interface = interface;


	dev->autoneg = AUTONEG_ENABLE;

	dev->addr = addr;
	dev->phy_id = phy_id;
	dev->is_c45 = is_c45;
	dev->bus = bus;

	// dev->drv = get_phy_driver(dev, interface);

	return 0;
}


/**
 * get_phy_id - reads the specified addr for its ID.
 * @bus: the target MII bus
 * @addr: PHY address on the MII bus
 * @phy_id: where to store the ID retrieved.
 *
 * Description: Reads the ID registers of the PHY at @addr on the
 *   @bus, stores it in @phy_id and returns zero on success.
 */
static int get_phy_id(struct mii_dev *bus, int addr, int devad, u32 *phy_id)
{
	int phy_reg;
	printf("[dw] Enter get_phy_id\n");
	if (!bus) {
		printf("[dw] bus is null\n");
		return -1;
	}
	/*
	 * Grab the bits from PHYIR1, and put them
	 * in the upper half
	 */
	if (!bus->read) {
		printf("[dw] bus->read is null\n");
		return -1;
	}
	phy_reg = bus->read(bus, addr, devad, MII_PHYSID1);

	printf("[dw] phy_reg1: %d\n", phy_reg);

	if (phy_reg < 0)
		return -EIO;

	*phy_id = (phy_reg & 0xffff) << 16;

	/* Grab the bits from PHYIR2, and put them in the lower half */
	phy_reg = bus->read(bus, addr, devad, MII_PHYSID2);

	if (phy_reg < 0)
		return -EIO;

	printf("[dw] phy_reg2: %d\n", phy_reg);

	*phy_id |= (phy_reg & 0xffff);

	return 0;
}



static int dw_mdio_read(struct mii_dev *bus, int addr, int devad, int reg)
{
	// printf("[dw] Enter dw_mdio_read\n");
	struct eth_mac_regs *mac_p = priv.mac_regs_p;
	ulong start;
	u16 miiaddr;
	int timeout = CONFIG_MDIO_TIMEOUT;

	miiaddr = ((addr << MIIADDRSHIFT) & MII_ADDRMSK) |
		  ((reg << MIIREGSHIFT) & MII_REGMSK);

	// printf("[dw] miiaddr: %d at : %p\n", miiaddr, &mac_p->miiaddr);

	writel(miiaddr | MII_CLKRANGE_150_250M | MII_BUSY, &mac_p->miiaddr);

	// printf("[dw] Done write in mdio_read\n");

	start = get_timer(0);
	while (get_timer(start) < timeout) {
		if (!(readl(&mac_p->miiaddr) & MII_BUSY))
			return readl(&mac_p->miidata);
		delayus(10);
	};

	return -ETIMEDOUT;
}

static int dw_mdio_write(struct mii_dev *bus, int addr, int devad, int reg, u16 val)
{
	struct eth_mac_regs *mac_p = priv.mac_regs_p;
	ulong start;
	u16 miiaddr;
	int ret = -ETIMEDOUT, timeout = CONFIG_MDIO_TIMEOUT;

	writel(val, &mac_p->miidata);
	miiaddr = ((addr << MIIADDRSHIFT) & MII_ADDRMSK) |
		  ((reg << MIIREGSHIFT) & MII_REGMSK) | MII_WRITE;

	writel(miiaddr | MII_CLKRANGE_150_250M | MII_BUSY, &mac_p->miiaddr);

	start = get_timer(0);
	while (get_timer(start) < timeout) {
		if (!(readl(&mac_p->miiaddr) & MII_BUSY)) {
			ret = 0;
			break;
		}
		delayus(10);
	};

	return ret;
}

/****************** MAC APIs ********************/

static void tx_descs_init();
static void rx_descs_init();
static int _dw_write_hwaddr(uint8_t *mac_id);
int phy_startup(struct phy_device *phydev);

void print_tx_state(int state) ;
void print_rx_state(int state) ;


struct eth_ops designware_eth_ops = {
	.start			= designware_eth_start,
	.send			= designware_eth_send,
	.recv			= designware_eth_recv,
	.free_pkt		= designware_eth_free_pkt,
	.stop			= designware_eth_stop,
	.write_hwaddr		= designware_eth_write_hwaddr,
};

// Convert hex character to integer
static int hex_to_int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_mac_address(const char *str, uint8_t *mac)
{
    int i, hi, lo;
    
    for (i = 0; i < 6; i++) {
        // Parse two hex digits
        hi = hex_to_int(str[i * 3]);
        if (hi < 0) {
            printf("Invalid MAC address at position %d\n", i * 3);
            return -1;
        }
        
        lo = hex_to_int(str[i * 3 + 1]);
        if (lo < 0) {
            printf("Invalid MAC address at position %d\n", i * 3 + 1);
            return -1;
        }
        
        mac[i] = (hi << 4) | lo;
        
        // Check for colon separator (except after last byte)
        if (i < 5 && str[i * 3 + 2] != ':') {
            printf("Missing colon at position %d\n", i * 3 + 2);
            return -1;
        }
    }
    
    return 0;
}

static int _dw_write_hwaddr(u8 *mac_id)
{
	struct eth_mac_regs *mac_p = priv.mac_regs_p;
	u32 macid_lo, macid_hi;

	u8 enetaddr[6];

	printf("Parsing MAC address: %s\n", mac_id);

	if (parse_mac_address((char *)mac_id, enetaddr) != 0) {
		printf("Invalid MAC address: %s\n", mac_id);
		return -1;
	}

	printf("Parsed MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           enetaddr[0], enetaddr[1], enetaddr[2],
           enetaddr[3], enetaddr[4], enetaddr[5]);

	macid_lo = enetaddr[0] + (enetaddr[1] << 8) + (enetaddr[2] << 16) +
		   (enetaddr[3] << 24);
	macid_hi = enetaddr[4] + (enetaddr[5] << 8);

	writel(macid_hi, &mac_p->macaddr0hi);
	writel(macid_lo, &mac_p->macaddr0lo);

	return 0;
}

int dw_write_hwaddr(u8 *mac_id)
{
	return _dw_write_hwaddr(mac_id);
}

int designware_eth_write_hwaddr(uint8_t *mac_id)
{
	return _dw_write_hwaddr(mac_id);
}

/*
 * Start the PHY.  Returns 0 on success, or a negative error code.
 */
/**
 * genphy_update_link - update link status in @phydev
 * @phydev: target phy_device struct
 *
 * Description: Update the value in phydev->link to reflect the
 *   current link value.  In order to do this, we need to read
 *   the status register twice, keeping the second value.
 */
int genphy_update_link(struct phy_device *phydev)
{
	unsigned int mii_reg;

	printf("MAC Control: %x\n", readl(&priv.mac_regs_p->conf));
	printf("DMA BusMode: %x\n", readl(&priv.dma_regs_p->busmode));

	/*
	 * Wait if the link is up, and autonegotiation is in progress
	 * (ie - we're capable and it's not done)
	 */
	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);

	printf("[dw] mii_reg: %d\n", mii_reg);

	/*
	 * If we already saw the link up, and it hasn't gone down, then
	 * we don't need to wait for autoneg again
	 */
	if (phydev->link && mii_reg & BMSR_LSTATUS)
		return 0;

	if ((phydev->autoneg == AUTONEG_ENABLE) &&
	    !(mii_reg & BMSR_ANEGCOMPLETE)) {
		int i = 0;

		printf("Waiting for PHY auto negotiation to complete");
		while (!(mii_reg & BMSR_ANEGCOMPLETE)) {
			/*
			 * Timeout reached ?
			 */
			if (i > (PHY_ANEG_TIMEOUT / 50)) {
				printf(" TIMEOUT !\n");
				phydev->link = 0;
				debug_phy_status(phydev);
				return -ETIMEDOUT;
			}

			if ((i++ % 10) == 0)
				printf(".");

			mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);
			delayms(100);	/* 50 ms */
		}
		printf(" done\n");
		phydev->link = 1;
	} else {
		/* Read the link a second time to clear the latched state */
		mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);

		if (mii_reg & BMSR_LSTATUS)
		{
			printf("[DW] link up\n");
			phydev->link = 1;
		}
		else
		{
			printf("[DW] link down\n");
			phydev->link = 0;
		}
	}
	return 0;
}

/*
 * Generic function which updates the speed and duplex.  If
 * autonegotiation is enabled, it uses the AND of the link
 * partner's advertised capabilities and our advertised
 * capabilities.  If autonegotiation is disabled, we use the
 * appropriate bits in the control register.
 *
 * Stolen from Linux's mii.c and phy_device.c
 */
int genphy_parse_link(struct phy_device *phydev)
{
	int mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);

	/* We're using autonegotiation */
	if (phydev->autoneg == AUTONEG_ENABLE) {
		u32 lpa = 0;
		int gblpa = 0;
		u32 estatus = 0;

		/* Check for gigabit capability */
		if (phydev->supported & (SUPPORTED_1000baseT_Full |
					SUPPORTED_1000baseT_Half)) {
			/* We want a list of states supported by
			 * both PHYs in the link
			 */
			gblpa = phy_read(phydev, MDIO_DEVAD_NONE, MII_STAT1000);
			if (gblpa < 0) {
				printf("[dw] Could not read MII_STAT1000. ");
				printf("[dw] Ignoring gigabit capability\n");
				gblpa = 0;
			}
			gblpa &= phy_read(phydev,
					MDIO_DEVAD_NONE, MII_CTRL1000) << 2;
		}

		/* Set the baseline so we only have to set them
		 * if they're different
		 */
		phydev->speed = SPEED_10;
		phydev->duplex = DUPLEX_HALF;

		/* Check the gigabit fields */
		if (gblpa & (PHY_1000BTSR_1000FD | PHY_1000BTSR_1000HD)) {
			phydev->speed = SPEED_1000;

			if (gblpa & PHY_1000BTSR_1000FD)
				phydev->duplex = DUPLEX_FULL;

			/* We're done! */
			return 0;
		}

		lpa = phy_read(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE);
		lpa &= phy_read(phydev, MDIO_DEVAD_NONE, MII_LPA);

		printf("[DW] lpa: %x\n", lpa);

		if (lpa & (LPA_100FULL | LPA_100HALF)) {
			phydev->speed = SPEED_100;

			if (lpa & LPA_100FULL)
				phydev->duplex = DUPLEX_FULL;

		} else if (lpa & LPA_10FULL) {
			phydev->duplex = DUPLEX_FULL;
		}

		printf("[DW] speed: %d, duplex: %d\n", phydev->speed, phydev->duplex);

		/*
		 * Extended status may indicate that the PHY supports
		 * 1000BASE-T/X even though the 1000BASE-T registers
		 * are missing. In this case we can't tell whether the
		 * peer also supports it, so we only check extended
		 * status if the 1000BASE-T registers are actually
		 * missing.
		 */
		if ((mii_reg & BMSR_ESTATEN) && !(mii_reg & BMSR_ERCAP))
			estatus = phy_read(phydev, MDIO_DEVAD_NONE,
					   MII_ESTATUS);

		if (estatus & (ESTATUS_1000_XFULL | ESTATUS_1000_XHALF |
				ESTATUS_1000_TFULL | ESTATUS_1000_THALF)) {
			phydev->speed = SPEED_1000;
			if (estatus & (ESTATUS_1000_XFULL | ESTATUS_1000_TFULL))
				phydev->duplex = DUPLEX_FULL;
		}

	} else {
		u32 bmcr = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);

		phydev->speed = SPEED_10;
		phydev->duplex = DUPLEX_HALF;

		if (bmcr & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;

		if (bmcr & BMCR_SPEED1000)
			phydev->speed = SPEED_1000;
		else if (bmcr & BMCR_SPEED100)
			phydev->speed = SPEED_100;
	}

	return 0;
}


int genphy_startup(struct phy_device *phydev)
{
	int ret;
	printf("[dw] do genphy_startup, now genphy_update_link\n");
	delayms(5000);
	ret = genphy_update_link(phydev);
	if (ret)
		return ret;
	printf("[dw] do genphy_parse_link\n");
	return genphy_parse_link(phydev);
}


int phy_startup(struct phy_device *phydev)
{
	genphy_startup(phydev);
	return 0;
}


static int dw_adjust_link(struct dw_eth_dev *priv, struct eth_mac_regs *mac_p,
			  struct phy_device *phydev)
{
	printf("[dw] Enter dw_adjust_link\n");
	u32 conf = readl(&mac_p->conf) | FRAMEBURSTENABLE | DISABLERXOWN;

	if (!phydev->link) {
		printf("[dw]No link.\n");
		return 0;
	}

	if (phydev->speed != 1000)
		conf |= MII_PORTSELECT;
	else
		conf &= ~MII_PORTSELECT;

	if (phydev->speed == 100)
		conf |= FES_100;

	if (phydev->duplex)
		conf |= FULLDPLXMODE;

	writel(conf, &mac_p->conf);

	printf("Speed: %d, %s duplex%s\n", phydev->speed,
	       (phydev->duplex) ? "full" : "half",
	       (phydev->port == PORT_FIBRE) ? ", fiber mode" : "");

	return 0;
}

void dump_dma_status(struct eth_dma_regs *dma_p) {
    printf("\n=== DMA Setup Verification ===\n");
    
    // 1. Bus Mode
    uint32_t busmode = readl(&dma_p->busmode);
    printf("DMA_BUSMODE (0x%08x):\n", busmode);
    printf("  SWR (reset): %d (should be 0)\n", busmode & DMAMAC_SRST ? 1 : 0);
    printf("  DSL: %d\n", (busmode >> 2) & 0x1F);
    printf("  PBL: %d\n", (busmode >> 8) & 0x3F);
    printf("  FIXEDBURST: %d\n", (busmode & FIXEDBURST) ? 1 : 0);
    printf("  PRIORXTX: %d\n", (busmode >> 14) & 0x3);
    
    // 2. Operation Mode
    uint32_t opmode = readl(&dma_p->opmode);
    printf("\nDMA_OPMODE (0x%08x):\n", opmode);
    printf("  SR (RX Start): %d\n", (opmode & RXSTART) ? 1 : 0);
    printf("  ST (TX Start): %d\n", (opmode & TXSTART) ? 1 : 0);
    printf("  FTF (Flush TX): %d (should be 0 after flush completes)\n", 
           (opmode & FLUSHTXFIFO) ? 1 : 0);
    
    // 3. DMA Status Register - THIS IS KEY!
    uint32_t status = readl(&dma_p->status);
    printf("\nDMA_STATUS (0x%08x):\n", status);
    printf("  TI (TX interrupt): %d\n", (status & 0x1) ? 1 : 0);
    printf("  TPS (TX stopped): %d\n", (status & 0x2) ? 1 : 0);
    printf("  TU (TX unavail): %d\n", (status & 0x4) ? 1 : 0);
    printf("  RI (RX interrupt): %d\n", (status & 0x40) ? 1 : 0);
    printf("  RU (RX unavail): %d\n", (status & 0x80) ? 1 : 0);
    printf("  RPS (RX stopped): %d\n", (status & 0x100) ? 1 : 0);
    printf("  TX Process State: %d ", (status >> 20) & 0x7);
    print_tx_state((status >> 20) & 0x7);
    printf("  RX Process State: %d ", (status >> 17) & 0x7);
    print_rx_state((status >> 17) & 0x7);
    
    // 4. Descriptor List Addresses
    printf("\nDescriptor Addresses:\n");
    printf("  TX Desc List: 0x%08x\n", readl(&dma_p->txdesclistaddr));
    printf("  RX Desc List: 0x%08x\n", readl(&dma_p->rxdesclistaddr));
    
    // 5. Current Descriptor/Buffer Addresses
    printf("\nCurrent Pointers:\n");
    printf("  Current TX Desc: 0x%08x\n", readl(&dma_p->currhosttxdesc));
    printf("  Current RX Desc: 0x%08x\n", readl(&dma_p->currhostrxdesc));
    printf("  Current TX Buf:  0x%08x\n", readl(&dma_p->currhosttxbuffaddr));
    printf("  Current RX Buf:  0x%08x\n", readl(&dma_p->currhostrxbuffaddr));
    
    // 6. Interrupt Enable
    printf("\nDMA_INTENABLE: 0x%08x\n", readl(&dma_p->intenable));
}

void print_tx_state(int state) {
    switch(state) {
        case 0: printf("(Stopped)\n"); break;
        case 1: printf("(Fetching descriptor)\n"); break;
        case 2: printf("(Waiting for status)\n"); break;
        case 3: printf("(Reading from memory)\n"); break;
        case 6: printf("(Suspended)\n"); break;
        case 7: printf("(Closing descriptor)\n"); break;
        default: printf("(Unknown)\n");
    }
}

void print_rx_state(int state) {
    switch(state) {
        case 0: printf("(Stopped)\n"); break;
        case 1: printf("(Fetching descriptor)\n"); break;
        case 3: printf("(Waiting for packet)\n"); break;
        case 4: printf("(Suspended)\n"); break;
        case 5: printf("(Closing descriptor)\n"); break;
        case 7: printf("(Writing to memory)\n"); break;
        default: printf("(Unknown)\n");
    }
}


int designware_eth_init(u8 *enetaddr)
{
	printf("[dw] Enter %s with enetaddr: %s\n", __func__, (char *)enetaddr);
	struct eth_mac_regs *mac_p = priv.mac_regs_p;
	struct eth_dma_regs *dma_p = priv.dma_regs_p;
	unsigned int start;
	int ret;

	printf("[dw] mac_p: %p, dma_p: %p\n", mac_p, dma_p);

	writel(readl(&dma_p->busmode) | DMAMAC_SRST, &dma_p->busmode);
	printf("[dw] wrote SRST in busmode\n");

	/*
	 * When a MII PHY is used, we must set the PS bit for the DMA
	 * reset to succeed.
	 */
	if (!priv.phydev) {
		printf("[dw] priv.phydev is NULL\n");
		return -1;
	}

	printf("[dw] priv.phydev->interface: %d\n", priv.phydev->interface);
	if (priv.phydev->interface == PHY_INTERFACE_MODE_MII)
		writel(readl(&mac_p->conf) | MII_PORTSELECT, &mac_p->conf);
	else
		writel(readl(&mac_p->conf) & ~MII_PORTSELECT, &mac_p->conf);

	printf("[dw] %s priv.phydev->interface: %d\n", __func__, priv.phydev->interface);

	start = get_timer(0);
	while (readl(&dma_p->busmode) & DMAMAC_SRST) {
		if (get_timer(start) >= CONFIG_MACRESET_TIMEOUT) {
			printf("DMA reset timeout\n");
			return -ETIMEDOUT;
		}

		delayms(100);
	};

	/*
	 * Soft reset above clears HW address registers.
	 * So we have to set it here once again.
	 */
	_dw_write_hwaddr(enetaddr);

	rx_descs_init();
	tx_descs_init();

	writel(FIXEDBURST | PRIORXTX_41 | DMA_PBL, &dma_p->busmode);

	writel(readl(&dma_p->opmode) | FLUSHTXFIFO,
	       &dma_p->opmode);

	writel(readl(&dma_p->opmode) | RXSTART | TXSTART, &dma_p->opmode);

	/* Start up the PHY */
	ret = phy_startup(priv.phydev);
	if (ret) {
		printf("Could not initialize PHY %s\n",
		       priv.phydev->dev->name);
		return ret;
	}

	ret = dw_adjust_link(&priv, mac_p, priv.phydev);
	if (ret)
		return ret;

	dump_dma_status(dma_p);

	/* Enable DMA interrupts */
	uint32_t dma_ie = DMA_INTR_ENA_NIE |  // Enable normal interrupt summary
                      DMA_INTR_ENA_AIE |  // Enable abnormal interrupt summary
                      DMA_INTR_ENA_RIE |  // Enable RX interrupt
                      DMA_INTR_ENA_TIE;   // Enable TX interrupt
    
    // Write to DMA interrupt enable register (offset 0x1c)
    writel(dma_ie, &dma_p->intenable);
    
    // Mask all MAC interrupts (we don't need them)
    writel(0xFFFFFFFF, &mac_p->intmask);

	printf("ETH: DMA interrupts enabled: 0x%08x\n", dma_ie);
    printf("ETH: DMA intenable readback: 0x%08x\n", readl(&dma_p->intenable));

	return 0;
}


static void tx_descs_init()
{
	struct eth_dma_regs *dma_p = priv.dma_regs_p;
	struct dmamacdescr *desc_table_p = &priv.tx_mac_descrtable[0];
	char *txbuffs = &priv.txbuffs[0];
	struct dmamacdescr *desc_p;
	u32 idx;

	for (idx = 0; idx < CONFIG_TX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->dmamac_addr = (ulong)&txbuffs[idx * CONFIG_ETH_BUFSIZE];
		desc_p->dmamac_next = (ulong)&desc_table_p[idx + 1];

		desc_p->dmamac_cntl = DESC_TXCTRL_TXCHAIN;
		desc_p->txrx_status = 0;
	}

	/* Correcting the last pointer of the chain */
	desc_p->dmamac_next = (ulong)&desc_table_p[0];

	/* Flush all Tx buffer descriptors at once */
	flush_dcache_range((ulong)priv.tx_mac_descrtable,
			   (ulong)priv.tx_mac_descrtable +
			   sizeof(priv.tx_mac_descrtable));

	writel((ulong)&desc_table_p[0], &dma_p->txdesclistaddr);
	priv.tx_currdescnum = 0;
}

static void rx_descs_init()
{
	initlock(&rx_lock, "eth_rx");
	initlock(&rx_queue.lock, "rx_queue");
	struct eth_dma_regs *dma_p = priv.dma_regs_p;
	struct dmamacdescr *desc_table_p = &priv.rx_mac_descrtable[0];
	char *rxbuffs = &priv.rxbuffs[0];
	struct dmamacdescr *desc_p;
	u32 idx;

	/* Before passing buffers to GMAC we need to make sure zeros
	 * written there right after "priv" structure allocation were
	 * flushed into RAM.
	 * Otherwise there's a chance to get some of them flushed in RAM when
	 * GMAC is already pushing data to RAM via DMA. This way incoming from
	 * GMAC data will be corrupted. */
#ifndef DYNAMIC_RX_BUFFERS
 	flush_dcache_range((ulong)rxbuffs, (ulong)rxbuffs + RX_TOTAL_BUFSIZE);
#endif

	for (idx = 0; idx < CONFIG_RX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
#ifdef DYNAMIC_RX_BUFFERS
		rxbuffs[idx] = kalloc();
		flush_dcache_range((ulong)rxbuffs[idx], (ulong)rxbuffs[idx] + PGSIZE);
		desc_p->dmamac_addr = (ulong)rxbuffs[idx];
#else
		desc_p->dmamac_addr = (ulong)&rxbuffs[idx * CONFIG_ETH_BUFSIZE];
#endif
		desc_p->dmamac_next = (ulong)&desc_table_p[idx + 1];

		desc_p->dmamac_cntl =
			(MAC_MAX_FRAME_SZ & DESC_RXCTRL_SIZE1MASK) |
				      DESC_RXCTRL_RXCHAIN;

		desc_p->txrx_status = DESC_RXSTS_OWNBYDMA;
	}

	/* Correcting the last pointer of the chain */
	desc_p->dmamac_next = (ulong)&desc_table_p[0];

	/* Flush all Rx buffer descriptors at once */
	flush_dcache_range((ulong)priv.rx_mac_descrtable,
			   (ulong)priv.rx_mac_descrtable +
			   sizeof(priv.rx_mac_descrtable));

	writel((ulong)&desc_table_p[0], &dma_p->rxdesclistaddr);
	priv.rx_currdescnum = 0;
}



/* Init for ethernet */

int designware_eth_enable(void)
{
	struct eth_mac_regs *mac_p = priv.mac_regs_p;

	if (!priv.phydev->link)
	{
		printf("[dw] No link\n");
		return -EIO;
	}

	writel(readl(&mac_p->conf) | RXENABLE | TXENABLE, &mac_p->conf);

	return 0;
}
int designware_eth_start(void)
{
	int ret;

	printf("[dw] designware_eth_init\n");

	ret = designware_eth_init((uint8_t *)DEFAULT_MAC_ADDR);
	if (ret)
		return ret;
	ret = designware_eth_enable();
	if (ret)
		return ret;

	return 0;
}

static int _dw_eth_send(struct dw_eth_dev *priv, void *packet, int length)
{
	struct eth_dma_regs *dma_p = priv->dma_regs_p;

	acquire(&priv->lock);

	u32 desc_num = priv->tx_currdescnum;
	struct dmamacdescr *desc_p = &priv->tx_mac_descrtable[desc_num];
	ulong desc_start = (ulong)desc_p;
	ulong desc_end = desc_start +
		roundup(sizeof(*desc_p), ARCH_DMA_MINALIGN);
	ulong data_start = desc_p->dmamac_addr;
	ulong data_end = data_start + roundup(length, ARCH_DMA_MINALIGN);
	/*
	 * Strictly we only need to invalidate the "txrx_status" field
	 * for the following check, but on some platforms we cannot
	 * invalidate only 4 bytes, so we flush the entire descriptor,
	 * which is 16 bytes in total. This is safe because the
	 * individual descriptors in the array are each aligned to
	 * ARCH_DMA_MINALIGN and padded appropriately.
	 */
	invalidate_dcache_range(desc_start, desc_end);

	/* Check if the descriptor is owned by CPU */
	if (desc_p->txrx_status & DESC_TXSTS_OWNBYDMA) {
		printf("CPU not owner of tx frame\n");
		return -EPERM;
	}

	memcpy((void *)data_start, packet, length);
	if (length < ETH_ZLEN) {
		memset(&((char *)data_start)[length], 0, ETH_ZLEN - length);
		length = ETH_ZLEN;
	}

	/* Flush data to be sent */
	flush_dcache_range(data_start, data_end);

	desc_p->dmamac_cntl = (desc_p->dmamac_cntl & ~DESC_TXCTRL_SIZE1MASK) |
			      ((length << DESC_TXCTRL_SIZE1SHFT) &
			      DESC_TXCTRL_SIZE1MASK) | DESC_TXCTRL_TXLAST |
			      DESC_TXCTRL_TXFIRST | DESC_TXCTRL_TXINT;

	desc_p->txrx_status = DESC_TXSTS_OWNBYDMA;

	/* Flush modified buffer descriptor */
	flush_dcache_range(desc_start, desc_end);

	/* Test the wrap-around condition. */
	if (++desc_num >= CONFIG_TX_DESCR_NUM)
		desc_num = 0;

	priv->tx_currdescnum = desc_num;

	release(&priv->lock);

	/* Start the transmission */
	writel(POLL_DATA, &dma_p->txpolldemand);

	return 0;
}

static int _dw_eth_recv(struct dw_eth_dev *priv, uchar **packetp)
{
	acquire(&rx_lock);
	u32 status, desc_num = priv->rx_currdescnum;
	release(&rx_lock);
	struct dmamacdescr *desc_p = &priv->rx_mac_descrtable[desc_num];
	int length = -EAGAIN;
	ulong desc_start = (ulong)desc_p;
	ulong desc_end = desc_start +
		roundup(sizeof(*desc_p), ARCH_DMA_MINALIGN);
	ulong data_start = desc_p->dmamac_addr;
	ulong data_end;

	/* Invalidate entire buffer descriptor */
	invalidate_dcache_range(desc_start, desc_end);

	status = desc_p->txrx_status;

	/* Check  if the owner is the CPU */
	if (!(status & DESC_RXSTS_OWNBYDMA)) {

		length = (status & DESC_RXSTS_FRMLENMSK) >>
			 DESC_RXSTS_FRMLENSHFT;

		/* Invalidate received data */
		data_end = data_start + roundup(length, ARCH_DMA_MINALIGN);
		invalidate_dcache_range(data_start, data_end);
		*packetp = (uchar *)(ulong)desc_p->dmamac_addr;
	}

	return length;
}

static int _dw_free_pkt(struct dw_eth_dev *priv)
{
	u32 desc_num = priv->rx_currdescnum;
	struct dmamacdescr *desc_p = &priv->rx_mac_descrtable[desc_num];
	ulong desc_start = (ulong)desc_p;
	ulong desc_end = desc_start +
		roundup(sizeof(*desc_p), ARCH_DMA_MINALIGN);

	/*
	 * Make the current descriptor valid again and go to
	 * the next one
	 */
	desc_p->txrx_status |= DESC_RXSTS_OWNBYDMA;

	/* Flush only status field - others weren't changed */
	flush_dcache_range(desc_start, desc_end);

	/* Test the wrap-around condition. */
	if (++desc_num >= CONFIG_RX_DESCR_NUM)
		desc_num = 0;
	priv->rx_currdescnum = desc_num;

	return 0;
}

int designware_eth_send(void *packet, int length) {
	return _dw_eth_send(&priv, packet, length);
}
int designware_eth_recv(int flags, uchar **packetp) {
	return _dw_eth_recv(&priv, packetp);
}

int designware_eth_free_pkt(uchar *packet, int length) {
	return _dw_free_pkt(&priv);
}

void designware_eth_stop() {
	return ;
}


void check_mac_address(void)
{
    uint32_t mac_hi = readl(&priv.mac_regs_p->macaddr0hi);
    uint32_t mac_lo = readl(&priv.mac_regs_p->macaddr0lo);
    
    printf("\n=== MAC Address Check ===\n");
    printf("MAC_ADDR0_HI: 0x%08x\n", mac_hi);
    printf("MAC_ADDR0_LO: 0x%08x\n", mac_lo);
    
    // Extract MAC address (note: byte order!)
    uint8_t mac[6];
    mac[0] = (mac_lo >> 0) & 0xff;
    mac[1] = (mac_lo >> 8) & 0xff;
    mac[2] = (mac_lo >> 16) & 0xff;
    mac[3] = (mac_lo >> 24) & 0xff;
    mac[4] = (mac_hi >> 0) & 0xff;
    mac[5] = (mac_hi >> 8) & 0xff;
    
    printf("MAC in registers: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("MAC in ARP sent:  24:0b:2a:21:09:20\n");
    
    if (mac[0] == 0x24 && mac[1] == 0x0b && mac[2] == 0x2a &&
        mac[3] == 0x21 && mac[4] == 0x09 && mac[5] == 0x20) {
        printf("✓ MAC addresses MATCH\n");
    } else {
        printf("✗ MAC addresses MISMATCH - This is the problem!\n");
    }
}

void eth_intr(void)
{
	struct eth_dma_regs *dma_p = priv.dma_regs_p;
    uint32_t dma_status = readl(&dma_p->status);
    
    printf("ETH_INTR: DMA status=0x%08x\n", dma_status);
    
    // Check what triggered the interrupt
    if (dma_status & (1 << 0)) {  // TI - Transmit Interrupt
        printf("  TX complete\n");
        // Handle TX completion
        // TODO: Free TX buffers, update TX descriptor ring
    }
    
    if (dma_status & (1 << 6)) {  // RI - Receive Interrupt
        printf("  RX packet received\n");
        // Handle RX packet
        eth_intr_rx_packets();
    }
    
    if (dma_status & (1 << 2)) {  // TU - Transmit Buffer Unavailable
        printf("  TX buffer unavailable\n");
    }
    
    if (dma_status & (1 << 7)) {  // RU - Receive Buffer Unavailable
        printf("  RX buffer unavailable\n");
        // Resume RX if needed
        writel(1, &dma_p->rxpolldemand);
    }
    
    if (dma_status & (1 << 15)) {  // AIS - Abnormal Interrupt Summary
        printf("  Abnormal interrupt! Status=0x%08x\n", dma_status);
        
        if (dma_status & (1 << 13))  // FBI - Fatal Bus Error
            printf("    Fatal bus error!\n");
        if (dma_status & (1 << 8))   // RPS - RX Process Stopped
            printf("    RX process stopped\n");
        if (dma_status & (1 << 1))   // TPS - TX Process Stopped
            printf("    TX process stopped\n");
    }
    
    // Clear interrupts by writing back the status bits
    writel(dma_status & 0x1FFFF, &dma_p->status);
}

#if 0
/*
Store received packets into a queue for processing in the future, 
wakeup if any process is waiting for packets.
*/
void eth_intr_rx_packets() {
	uchar *packet;
	int length;
	int received = 0;
	uint32_t status;

	struct eth_dma_regs *dma_p = (struct eth_dma_regs *)priv.dma_regs_p;

	while ((length = designware_eth_recv(0, &packet)) > 0) {
		// Got a packet
		received++;
		printf("  RX packet of length %d bytes\n", length);

		ulong curr_desc_addr = readl(&dma_p->currhostrxdesc);
        u32 desc_num_check = (curr_desc_addr - (ulong)priv.rx_mac_descrtable) / sizeof(struct dmamacdescr);

		acquire(&rx_lock);
		u32 desc_num = priv.rx_currdescnum;
		release(&rx_lock);

		printf("[dw] desc_num_check: %d, desc_num: %d\n", desc_num_check, desc_num);

#ifdef DYNAMIC_RX_BUFFERS
		struct dmamacdescr *desc_p = &priv.rx_mac_descrtable[desc_num];
#endif

		if (rx_queue.count < RX_QUEUE_SIZE) {
			/* 
			Add packet to queue if is UDP/TCP packet, if is ARP request,
			Send ARP reply immediately.
			*/
			if (net_rx((char *)packet, length)){
				acquire(&rx_queue.lock);
				rx_queue.count++;
				release(&rx_queue.lock);
			}
		}

		/*
		Allocate new page for new rx packets. Assign init value
		for this rx packets later. Old packet point by DMA RX desc 
		is processing by net_rx. If complete ==> Free old packet
		*/
#ifdef DYNAMIC_RX_BUFFERS
		void *new_buffer = kalloc();
		if (!new_buffer) {
			panic("eth_intr_rx_packets: out of memory\n");
		}
		acquire(&rx_lock);
		priv.rxbuffs[desc_num] = new_buffer;

		flush_dcache_range((ulong)priv.rxbuffs[desc_num], (ulong)priv.rxbuffs[desc_num] + PGSIZE);
		desc_p->dmamac_addr = (ulong)priv.rxbuffs[desc_num];
		// Ensure all writes complete before setting ownership
        __sync_synchronize();  // Memory barrier

		desc_p->txrx_status = DESC_RXSTS_OWNBYDMA;

		// Flush descriptor to memory for DMA
		flush_dcache_range((ulong)desc_p, (ulong)desc_p + sizeof(*desc_p));

		if (++desc_num >= CONFIG_RX_DESCR_NUM)
			desc_num = 0;
		priv.rx_currdescnum = desc_num;
		release(&rx_lock);

		// Check if RX DMA suspended and resume if needed
		status = readl(&dma_p->status);
		if (status & (1 << 7)) {  // RU bit
			printf("RX: DMA suspended, resuming\n");
			writel(1, &dma_p->rxpolldemand);
		}
#else
		// // Free the packet buffer back to DMA
		designware_eth_free_pkt(packet, length);
		// Check if RX DMA suspended and resume if needed
		status = readl(&dma_p->status);
		if (status & (1 << 7)) {  // RU bit
			printf("RX: DMA suspended, resuming\n");
			writel(1, &dma_p->rxpolldemand);
		}
#endif
	}
	
	if (received == 0) {
		printf("  No more packets to receive\n");
	} else {
		printf("  Total %d packets received and queued\n", received);
	}
}

#endif


void enable_promiscuous_mode(void)
{
    uint32_t framefilt = readl(&priv.mac_regs_p->framefilt);
    
    printf("Frame filter before: 0x%08x\n", framefilt);
    
    // Enable promiscuous mode - receive ALL packets
    framefilt |= (1 << 0);  // PM bit
    
    writel(framefilt, &priv.mac_regs_p->framefilt);
    
    printf("Frame filter after:  0x%08x\n", readl(&priv.mac_regs_p->framefilt));
    printf("Promiscuous mode enabled\n");
}


static int eth_net_open(struct net_device *dev);
static int eth_net_close(struct net_device *dev);
static int eth_net_transmit(struct net_device *dev, uint16_t type, const uint8_t *packet, size_t len, const void *dst);

#define PRIV(x) ((struct dw_eth_dev *)(x)->priv)
static int eth_net_open(struct net_device *dev) {
	struct dw_eth_dev *priv = PRIV(dev);
	printf("Open ethernet net device of author: %s\n", priv->author);
	return 0;
}

static int eth_net_close(struct net_device *dev) {
	struct dw_eth_dev *priv = PRIV(dev);
	printf("Close ethernet net device of author: %s\n", priv->author);
	return 0;
}

/**
 * net-dev API: Write a packet to the network via the designware MAC
 *
 * @param dev The network device
 * @param packet The packet to write
 * @param len The length of the packet
 **/ 
static ssize_t eth_net_write(struct net_device *dev, const uint8_t *data, size_t len) {
	struct dw_eth_dev *priv = PRIV(dev);
	return _dw_eth_send(priv, (void *)data, len);
}

static int
eth_net_transmit(struct net_device *dev, uint16_t type, const uint8_t *packet, size_t len, const void *dst)
{
  return ether_transmit_helper(dev, type, packet, len, dst, eth_net_write);;
}

static ssize_t eth_net_read(struct net_device *dev, uint8_t *buf, size_t size) {
	struct dw_eth_dev *priv = PRIV(dev);
	uint32_t status;
	struct eth_dma_regs *dma_p = (struct eth_dma_regs *)priv->dma_regs_p;
	uchar *packet;
	int length = _dw_eth_recv(priv, &packet);
	if (length > 0 && (size_t)length <= size) {
		memcpy(buf, packet, length);
		_dw_free_pkt(priv);
		status = readl(&dma_p->status);
		if (status & (1 << 7)) {  // RU bit
			printf("RX: DMA suspended, resuming\n");
			writel(1, &dma_p->rxpolldemand);
		}
		return length;
	}
	return -EAGAIN;
}


void eth_intr_rx_packets() {
	acquire(&priv.lock);
	/* Process packet */
	ether_input_helper(priv.dev, eth_net_read);
	release(&priv.lock);
	/* Raise softirq to handle the packet later */
	intr_raise_irq(INTR_IRQ_SOFTIRQ);
}

struct net_device_ops eth_net_ops = {
    .open = eth_net_open,
    .close = eth_net_close,
    .transmit = eth_net_transmit,
};

void eth_init(void)
{
	designware_eth_ops.start();
	uint8_t mac_addr[ETHADDR_LEN] = { 0x24, 0x0b, 0x2a, 0x21, 0x09, 0x20 };
	// enable_promiscuous_mode();
	char mac[ETHER_ADDR_STR_LEN];
	initlock(&priv.lock, "lock");
	check_mac_address();
	struct net_device *dev;
	// setup device driver structure
    dev = net_device_alloc();
	if (!dev) {
        errorf("net_device_alloc() failure");
        return;
    }
	ether_setup_helper(dev);
	memcpy(dev->addr, mac_addr, sizeof(mac_addr));
	dev->priv = &priv;
	dev->ops = &eth_net_ops;
	if (net_device_register(dev) == -1) {
        errorf("net_device_register() failure");
        memory_free(dev);
        return;
    }
	/* Assign abstract network device of driver point to struct net_device */
	priv.dev = dev;
	printf("Ethernet network device registered\n");
	debugf("initialized, addr=%s", ether_addr_ntop(dev->addr, mac, sizeof(mac)));
	
	// TODO: Temporary Code
    struct ip_iface *iface;
    iface = ip_iface_alloc("192.168.1.72", "255.255.255.0");
    if (!iface) {
        errorf("ip_iface_alloc() failure");
        return;
    }
    if (ip_iface_register(dev, iface) == -1) {
        errorf("ip_iface_register() failure");
        return;
    }
}