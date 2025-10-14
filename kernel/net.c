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
#include "bitops.h"

uint8 local_mac[ETHADDR_LEN] = { 0x24, 0x0b, 0x2a, 0x21, 0x09, 0x20 };
uint32 local_ip = MAKE_IP_ADDR(192, 168, 1, 59);
//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  /* [net] for sending ARP reply */
  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  designware_eth_ops.send(buf, sizeof(*eth) + sizeof(*arp));
  kfree(inbuf);
}

void
ip_rx(char *inbuf, int len) {
    printf("Receive IP packet of length %d bytes\n", len);
    kfree(inbuf);
}

int
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
  return 0;
}