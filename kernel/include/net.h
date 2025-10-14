#ifndef NET_H
#define NET_H

#include "../types.h"
#include "../spinlock.h"

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

#define DEFAULT_MAC_ADDR	"24:0b:2a:21:09:20"

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
	int (*start)(void);
	int (*send)(void *packet, int length);
	int (*recv)(int flags, uchar **packetp);
	int (*free_pkt)(uchar *packet, int length);
	void (*stop)(void);
	int (*mcast)(const u8 *enetaddr, int join);
	int (*write_hwaddr)(uint8_t *enetaddr);
	int (*read_rom_hwaddr)(void);
	int (*set_promisc)(bool enable);
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
int eth_is_active(); /* Test device for active state */
int eth_init_state_only(void); /* Set active state */
void eth_halt_state_only(void); /* Set passive state */
#endif

// Packet queue for upper layers
struct rx_packet {
    char *buf;           // kalloc'd buffer with packet data
    int len;
    struct rx_packet *next;
};

#define RX_QUEUE_SIZE 32  // Size of the RX packet queue

struct rx_queue {
    struct spinlock lock;
    struct rx_packet *head;
    struct rx_packet *tail;
	int count;    //Total packet is RX queue
};

extern struct rx_queue rx_queue;

//
// endianness support
//

static inline uint16 bswaps(uint16 val)
{
  return (((val & 0x00ffU) << 8) |
          ((val & 0xff00U) >> 8));
}

static inline uint32 bswapl(uint32 val)
{
  return (((val & 0x000000ffUL) << 24) |
          ((val & 0x0000ff00UL) << 8) |
          ((val & 0x00ff0000UL) >> 8) |
          ((val & 0xff000000UL) >> 24));
}

// Use these macros to convert network bytes to the native byte order.
// Note that Risc-V uses little endian while network order is big endian.
static inline uint16 htons(uint16 val) {
    return bswaps(val);
}

static inline uint32 htonl(uint32 val) {
    return bswapl(val);
}

static inline uint16 ntohs(uint16 val) {
    return bswaps(val);
}

static inline uint32 ntohl(uint32 val) {
    return bswapl(val);
}


//
// useful networking headers
//

#define ETHADDR_LEN 6

// an Ethernet packet header (start of the packet).
struct eth {
  uint8  dhost[ETHADDR_LEN];
  uint8  shost[ETHADDR_LEN];
  uint16 type;
} __attribute__((packed));

#define ETHTYPE_IP  0x0800 // Internet protocol
#define ETHTYPE_ARP 0x0806 // Address resolution protocol

// an IP packet header (comes after an Ethernet header).
struct ip {
  uint8  ip_vhl; // version << 4 | header length >> 2
  uint8  ip_tos; // type of service
  uint16 ip_len; // total length, including this IP header
  uint16 ip_id;  // identification
  uint16 ip_off; // fragment offset field
  uint8  ip_ttl; // time to live
  uint8  ip_p;   // protocol
  uint16 ip_sum; // checksum, covers just IP header
  uint32 ip_src, ip_dst;
};

#define IPPROTO_ICMP 1  // Control message protocol
#define IPPROTO_TCP  6  // Transmission control protocol
#define IPPROTO_UDP  17 // User datagram protocol

#define MAKE_IP_ADDR(a, b, c, d)           \
  (((uint32)a << 24) | ((uint32)b << 16) | \
   ((uint32)c << 8) | (uint32)d)

// a UDP packet header (comes after an IP header).
struct udp {
  uint16 sport; // source port
  uint16 dport; // destination port
  uint16 ulen;  // length, including udp header, not including IP header
  uint16 sum;   // checksum
};

// an ARP packet (comes after an Ethernet header).
struct arp {
  uint16 hrd; // format of hardware address
  uint16 pro; // format of protocol address
  uint8  hln; // length of hardware address
  uint8  pln; // length of protocol address
  uint16 op;  // operation

  char   sha[ETHADDR_LEN]; // sender hardware address
  uint32 sip;              // sender IP address
  char   tha[ETHADDR_LEN]; // target hardware address
  uint32 tip;              // target IP address
} __attribute__((packed));

#define ARP_HRD_ETHER 1 // Ethernet

enum {
  ARP_OP_REQUEST = 1, // requests hw addr given protocol addr
  ARP_OP_REPLY = 2,   // replies with the hw addr of the protocol addr
};

// an DNS packet (comes after an UDP header).
struct dns {
  uint16 id;  // request ID

  uint8 rd: 1;  // recursion desired
  uint8 tc: 1;  // truncated
  uint8 aa: 1;  // authoritive
  uint8 opcode: 4; 
  uint8 qr: 1;  // query/response
  uint8 rcode: 4; // response code
  uint8 cd: 1;  // checking disabled
  uint8 ad: 1;  // authenticated data
  uint8 z:  1;  
  uint8 ra: 1;  // recursion available
  
  uint16 qdcount; // number of question entries
  uint16 ancount; // number of resource records in answer section
  uint16 nscount; // number of NS resource records in authority section
  uint16 arcount; // number of resource records in additional records
} __attribute__((packed));

struct dns_question {
  uint16 qtype;
  uint16 qclass;
} __attribute__((packed));
  
#define ARECORD (0x0001)
#define QCLASS  (0x0001)

struct dns_data {
  uint16 type;
  uint16 class;
  uint32 ttl;
  uint16 len;
} __attribute__((packed));

#define MAX_PORTNUM 16
#define MAX_RECV_QUEUE 16
struct bind_port {
  int total_ports;
  int ports[MAX_PORTNUM];
};

// xv6's ethernet and IP addresses
extern uint8 local_mac[ETHADDR_LEN];
extern uint32 local_ip;


#endif // NET_H
