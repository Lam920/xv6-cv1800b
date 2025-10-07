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
#include "bitops.h"

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

void dw_eth_init(void) {
    phys_addr_t iobase = ETH0_BASE;

    /* Init designware ethernet private info */
    priv.mac_regs_p = (struct eth_mac_regs *)iobase;
    priv.dma_regs_p = (struct eth_dma_regs *)(iobase + DW_DMA_BASE_OFFSET);
    priv.max_speed = 0;
	priv.interface = PHY_INTERFACE_MODE_RMII;


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

	int ret = phy_config(&phydev);
	if (ret < 0) {
		printf("[dw] phy_config failed\n");
		return;
	}
	else {
		printf("[dw] phy_config success\n");
	}

}

int phy_config(struct phy_device *phydev)
{
	/* Invoke an optional board-specific helper */
	printf("[dw] do phy_config\n");
	return genphy_config(phydev);
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

	adv &= ~(ADVERTISE_ALL | ADVERTISE_100BASE4 | ADVERTISE_PAUSE_CAP |
		 ADVERTISE_PAUSE_ASYM);
	if (advertise & ADVERTISED_10baseT_Half)
		adv |= ADVERTISE_10HALF;
	if (advertise & ADVERTISED_10baseT_Full)
		adv |= ADVERTISE_10FULL;
	if (advertise & ADVERTISED_100baseT_Half)
		adv |= ADVERTISE_100HALF;
	if (advertise & ADVERTISED_100baseT_Full)
		adv |= ADVERTISE_100FULL;
	if (advertise & ADVERTISED_Pause)
		adv |= ADVERTISE_PAUSE_CAP;
	if (advertise & ADVERTISED_Asym_Pause)
		adv |= ADVERTISE_PAUSE_ASYM;
	if (advertise & ADVERTISED_1000baseX_Half)
		adv |= ADVERTISE_1000XHALF;
	if (advertise & ADVERTISED_1000baseX_Full)
		adv |= ADVERTISE_1000XFULL;

	if (adv != oldadv) {
		err = phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE, adv);

		if (err < 0)
			return err;
		changed = 1;
	}

	bmsr = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);
	if (bmsr < 0)
		return bmsr;

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
int genphy_restart_aneg(struct phy_device *phydev)
{
	int ctl;

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


const struct eth_ops designware_eth_ops = {
	.start			= designware_eth_start,
	.send			= designware_eth_send,
	.recv			= designware_eth_recv,
	.free_pkt		= designware_eth_free_pkt,
	.stop			= designware_eth_stop,
	.write_hwaddr		= designware_eth_write_hwaddr,
};


static int _dw_write_hwaddr(u8 *mac_id)
{
	struct eth_mac_regs *mac_p = priv.mac_regs_p;
	u32 macid_lo, macid_hi;

	macid_lo = mac_id[0] + (mac_id[1] << 8) + (mac_id[2] << 16) +
		   (mac_id[3] << 24);
	macid_hi = mac_id[4] + (mac_id[5] << 8);

	writel(macid_hi, &mac_p->macaddr0hi);
	writel(macid_lo, &mac_p->macaddr0lo);

	return 0;
}

int dw_write_hwaddr(u8 *mac_id)
{
	return _dw_write_hwaddr(mac_id);
}