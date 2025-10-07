#ifndef PHY_H
#define PHY_H

#include "../types.h"


/* Indicates what features are supported by the interface. */
#define SUPPORTED_10baseT_Half		(1 << 0)
#define SUPPORTED_10baseT_Full		(1 << 1)
#define SUPPORTED_100baseT_Half		(1 << 2)
#define SUPPORTED_100baseT_Full		(1 << 3)
#define SUPPORTED_1000baseT_Half	(1 << 4)
#define SUPPORTED_1000baseT_Full	(1 << 5)
#define SUPPORTED_Autoneg		(1 << 6)
#define SUPPORTED_TP			(1 << 7)
#define SUPPORTED_AUI			(1 << 8)
#define SUPPORTED_MII			(1 << 9)
#define SUPPORTED_FIBRE			(1 << 10)
#define SUPPORTED_BNC			(1 << 11)
#define SUPPORTED_10000baseT_Full	(1 << 12)
#define SUPPORTED_Pause			(1 << 13)
#define SUPPORTED_Asym_Pause		(1 << 14)
#define SUPPORTED_2500baseX_Full	(1 << 15)
#define SUPPORTED_Backplane		(1 << 16)
#define SUPPORTED_1000baseKX_Full	(1 << 17)
#define SUPPORTED_10000baseKX4_Full	(1 << 18)
#define SUPPORTED_10000baseKR_Full	(1 << 19)
#define SUPPORTED_10000baseR_FEC	(1 << 20)
#define SUPPORTED_1000baseX_Half	(1 << 21)
#define SUPPORTED_1000baseX_Full	(1 << 22)

/* Indicates what features are advertised by the interface. */
#define ADVERTISED_10baseT_Half		(1 << 0)
#define ADVERTISED_10baseT_Full		(1 << 1)
#define ADVERTISED_100baseT_Half	(1 << 2)
#define ADVERTISED_100baseT_Full	(1 << 3)
#define ADVERTISED_1000baseT_Half	(1 << 4)
#define ADVERTISED_1000baseT_Full	(1 << 5)
#define ADVERTISED_Autoneg		(1 << 6)
#define ADVERTISED_TP			(1 << 7)
#define ADVERTISED_AUI			(1 << 8)
#define ADVERTISED_MII			(1 << 9)
#define ADVERTISED_FIBRE		(1 << 10)
#define ADVERTISED_BNC			(1 << 11)
#define ADVERTISED_10000baseT_Full	(1 << 12)
#define ADVERTISED_Pause		(1 << 13)
#define ADVERTISED_Asym_Pause		(1 << 14)
#define ADVERTISED_2500baseX_Full	(1 << 15)
#define ADVERTISED_Backplane		(1 << 16)
#define ADVERTISED_1000baseKX_Full	(1 << 17)
#define ADVERTISED_10000baseKX4_Full	(1 << 18)
#define ADVERTISED_10000baseKR_Full	(1 << 19)
#define ADVERTISED_10000baseR_FEC	(1 << 20)
#define ADVERTISED_1000baseX_Half	(1 << 21)
#define ADVERTISED_1000baseX_Full	(1 << 22)

/* The following are all involved in forcing a particular link
 * mode for the device for setting things.  When getting the
 * devices settings, these indicate the current mode and whether
 * it was foced up into this mode or autonegotiated.
 */

/* The forced speed, 10Mb, 100Mb, gigabit, 2.5Gb, 10GbE. */
#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000
#define SPEED_2500		2500
#define SPEED_10000		10000
#define SPEED_14000		14000
#define SPEED_20000		20000
#define SPEED_25000		25000
#define SPEED_40000		40000
#define SPEED_50000		50000
#define SPEED_56000		56000
#define SPEED_100000		100000
#define SPEED_200000		200000

/* Duplex, half or full. */
#define DUPLEX_HALF		0x00
#define DUPLEX_FULL		0x01

#ifndef PHY_ANEG_TIMEOUT
#define PHY_ANEG_TIMEOUT	4000
#endif

#define PHY_DEFAULT_FEATURES	(SUPPORTED_Autoneg | \
				 SUPPORTED_TP | \
				 SUPPORTED_MII)

#define PHY_10BT_FEATURES	(SUPPORTED_10baseT_Half | \
				 SUPPORTED_10baseT_Full)

#define PHY_100BT_FEATURES	(SUPPORTED_100baseT_Half | \
				 SUPPORTED_100baseT_Full)

#define PHY_1000BT_FEATURES	(SUPPORTED_1000baseT_Half | \
				 SUPPORTED_1000baseT_Full)

#define PHY_BASIC_FEATURES	(PHY_10BT_FEATURES | \
				 PHY_100BT_FEATURES | \
				 PHY_DEFAULT_FEATURES)

#define PHY_100BT1_FEATURES	(SUPPORTED_TP | \
				 SUPPORTED_MII | \
				 SUPPORTED_100baseT_Full)

#define PHY_GBIT_FEATURES	(PHY_BASIC_FEATURES | \
				 PHY_1000BT_FEATURES)

#define PHY_10G_FEATURES	(PHY_GBIT_FEATURES | \
				SUPPORTED_10000baseT_Full)

#define PHY_MAX_ADDR 32
#define MDIO_NAME_LEN 32
struct mii_dev {
	char name[MDIO_NAME_LEN];
	void *priv;
	int (*read)(struct mii_dev *bus, int addr, int devad, int reg);
	int (*write)(struct mii_dev *bus, int addr, int devad, int reg,
			u16 val);
	int (*reset)(struct mii_dev *bus);
#ifdef __riscv
	struct phy_device *phymap[PHY_MAX_ADDR];
#endif
	u32 phy_mask;
};

/* struct phy_driver: a structure which defines PHY behavior
 *
 * uid will contain a number which represents the PHY.  During
 * startup, the driver will poll the PHY to find out what its
 * UID--as defined by registers 2 and 3--is.  The 32-bit result
 * gotten from the PHY will be masked to
 * discard any bits which may change based on revision numbers
 * unimportant to functionality
 *
 */
struct phy_driver {
	char *name;
	unsigned int uid;
	unsigned int mask;
	unsigned int mmds;

	u32 features;

	/* Called to do any driver startup necessities */
	/* Will be called during phy_connect */
	int (*probe)(struct phy_device *phydev);

	/* Called to configure the PHY, and modify the controller
	 * based on the results.  Should be called after phy_connect */
	int (*config)(struct phy_device *phydev);

	/* Called when starting up the controller */
	int (*startup)(struct phy_device *phydev);

	/* Called when bringing down the controller */
	int (*shutdown)(struct phy_device *phydev);

	int (*readext)(struct phy_device *phydev, int addr, int devad, int reg);
	int (*writeext)(struct phy_device *phydev, int addr, int devad, int reg,
			u16 val);

	/* Phy specific driver override for reading a MMD register */
	int (*read_mmd)(struct phy_device *phydev, int devad, int reg);

	/* Phy specific driver override for writing a MMD register */
	int (*write_mmd)(struct phy_device *phydev, int devad, int reg,
			 u16 val);

	struct list_head list;

	/* driver private data */
	ulong data;
};

struct phy_device {
	/* Information about the PHY type */
	/* And management functions */
	struct mii_dev *bus;
	struct phy_driver *drv;
	void *priv;

	struct udevice *dev;
	/* forced speed & duplex (no autoneg)
	 * partner speed & duplex & pause (autoneg)
	 */
	int speed;
	int duplex;

	/* The most recently read link state */
	int link;
	int port;
	phy_interface_t interface;

	u32 advertising;
	u32 supported;
	u32 mmds;

	int autoneg;
	int addr;
	int pause;
	int asym_pause;
	u32 phy_id;
	bool is_c45;
	u32 flags;
};

struct fixed_link {
	int phy_id;
	int duplex;
	int link_speed;
	int pause;
	int asym_pause;
};

/**
 * phy_read - Convenience function for reading a given PHY register
 * @phydev: the phy_device struct
 * @devad: The MMD to read from
 * @regnum: register number to read
 * @return: value for success or negative errno for failure
 */
static inline int phy_read(struct phy_device *phydev, int devad, int regnum)
{
	struct mii_dev *bus = phydev->bus;

	if (!bus || !bus->read) {
		debug("%s: No bus configured\n", __func__);
		return -1;
	}

	return bus->read(bus, phydev->addr, devad, regnum);
}

/**
 * phy_write - Convenience function for writing a given PHY register
 * @phydev: the phy_device struct
 * @devad: The MMD to read from
 * @regnum: register number to write
 * @val: value to write to @regnum
 * @return: 0 for success or negative errno for failure
 */
static inline int phy_write(struct phy_device *phydev, int devad, int regnum,
			u16 val)
{
	struct mii_dev *bus = phydev->bus;

	if (!bus || !bus->write) {
		debug("%s: No bus configured\n", __func__);
		return -1;
	}

	return bus->write(bus, phydev->addr, devad, regnum, val);
}

/* Enable or disable autonegotiation.  If this is set to enable,
 * the forced link modes above are completely ignored.
 */
#define AUTONEG_DISABLE		0x00
#define AUTONEG_ENABLE		0x01


#endif /* PHY_H */
