#ifndef NET_H
#define NET_H

#include "../types.h"

#define DEBUG_LL_STATE 0	/* Link local state machine changes */
#define DEBUG_DEV_PKT 0		/* Packets or info directed to the device */
#define DEBUG_NET_PKT 0		/* Packets on info on the network at large */
#define DEBUG_INT_STATE 0	/* Internal network state changes */

/*
 *	The number of receive packet buffers, and the required packet buffer
 *	alignment in memory.
 *
 */
#define PKTBUFSRX	4

/* Number of packets processed together */
#define ETH_PACKETS_BATCH_RECV	32

/* ARP hardware address length */
#define ARP_HLEN 6

/*
 * The size of a MAC address in string form, each digit requires two chars
 * and five separator characters to form '00:00:00:00:00:00'.
 */
#define ARP_HLEN_ASCII (ARP_HLEN * 2) + (ARP_HLEN - 1)

/* IPv4 addresses are always 32 bits in size */
struct in_addr {
	__be32 s_addr;
};

enum eth_state_t {
	ETH_STATE_INIT,
	ETH_STATE_PASSIVE,
	ETH_STATE_ACTIVE
};

#ifdef CONFIG_DM_ETH
/**
 * struct eth_pdata - Platform data for Ethernet MAC controllers
 *
 * @iobase: The base address of the hardware registers
 * @enetaddr: The Ethernet MAC address that is loaded from EEPROM or env
 * @phy_interface: PHY interface to use - see PHY_INTERFACE_MODE_...
 * @max_speed: Maximum speed of Ethernet connection supported by MAC
 * @priv_pdata: device specific plat
 */
struct eth_pdata {
	phys_addr_t iobase;
	unsigned char enetaddr[ARP_HLEN];
	int phy_interface;
	int max_speed;
	void *priv_pdata;
};

enum eth_recv_flags {
	/*
	 * Check hardware device for new packets (otherwise only return those
	 * which are already in the memory buffer ready to process)
	 */
	ETH_RECV_CHECK_DEVICE		= 1 << 0,
};

/**
 * struct eth_ops - functions of Ethernet MAC controllers
 *
 * start: Prepare the hardware to send and receive packets
 * send: Send the bytes passed in "packet" as a packet on the wire
 * recv: Check if the hardware received a packet. If so, set the pointer to the
 *	 packet buffer in the packetp parameter. If not, return an error or 0 to
 *	 indicate that the hardware receive FIFO is empty. If 0 is returned, the
 *	 network stack will not process the empty packet, but free_pkt() will be
 *	 called if supplied
 * free_pkt: Give the driver an opportunity to manage its packet buffer memory
 *	     when the network stack is finished processing it. This will only be
 *	     called when no error was returned from recv - optional
 * stop: Stop the hardware from looking for packets - may be called even if
 *	 state == PASSIVE
 * mcast: Join or leave a multicast group (for TFTP) - optional
 * write_hwaddr: Write a MAC address to the hardware (used to pass it to Linux
 *		 on some platforms like ARM). This function expects the
 *		 eth_pdata::enetaddr field to be populated. The method can
 *		 return -ENOSYS to indicate that this is not implemented for
		 this hardware - optional.
 * read_rom_hwaddr: Some devices have a backup of the MAC address stored in a
 *		    ROM on the board. This is how the driver should expose it
 *		    to the network stack. This function should fill in the
 *		    eth_pdata::enetaddr field - optional
 * set_promisc: Enable or Disable promiscuous mode
 */

struct udevice {
	char name[32];
	int status;
};

struct eth_ops {
	int (*start)(struct udevice *dev);
	int (*send)(struct udevice *dev, void *packet, int length);
	int (*recv)(struct udevice *dev, int flags, uchar **packetp);
	int (*free_pkt)(struct udevice *dev, uchar *packet, int length);
	void (*stop)(struct udevice *dev);
	int (*mcast)(struct udevice *dev, const u8 *enetaddr, int join);
	int (*write_hwaddr)(struct udevice *dev);
	int (*read_rom_hwaddr)(struct udevice *dev);
	int (*set_promisc)(struct udevice *dev, bool enable);
};

#define eth_get_ops(dev) ((struct eth_ops *)(dev)->driver->ops)

struct udevice *eth_get_dev(void); /* get the current device */
/*
 * The devname can be either an exact name given by the driver or device tree
 * or it can be an alias of the form "eth%d"
 */
struct udevice *eth_get_dev_by_name(const char *devname);
unsigned char *eth_get_ethaddr(void); /* get the current device MAC */

/* Used only when NetConsole is enabled */
int eth_is_active(struct udevice *dev); /* Test device for active state */
int eth_init_state_only(void); /* Set active state */
void eth_halt_state_only(void); /* Set passive state */
#endif


#endif // NET_H
