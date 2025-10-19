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
uint32 local_ip = MAKE_IP_ADDR(192, 168, 1, 72);
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
#ifdef DYNAMIC_RX_BUFFERS
    kfree(inbuf);
#endif
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  // seen_arp = 1;

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
#ifdef DYNAMIC_RX_BUFFERS
  kfree(inbuf);
#endif
  kfree(buf);
}

// Simple IP checksum function
uint16_t
ip_checksum(struct ip *ip, int len)
{
  uint32_t sum = 0;
  uint16_t *p = (uint16_t *)ip;
  
  for (; len > 1; len -= 2) {
    sum += *p++;
  }
  if (len > 0) {
    sum += *(uint8_t *)p;
  }
  
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  
  return ~sum;
}

// ICMP checksum function
uint16_t
icmp_checksum(struct icmp *icmp, int len)
{
  uint32_t sum = 0;
  uint16_t *p = (uint16_t *)icmp;
  
  for (; len > 1; len -= 2) {
    sum += *p++;
  }
  if (len > 0) {
    sum += *(uint8_t *)p;
  }
  
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  
  return ~sum;
}


void
icmp_rx(char *inbuf, uint32_t src_ip, uint32_t dst_ip)
{
  struct eth *ineth = (struct eth *) inbuf;
  struct ip *inip = (struct ip *) (ineth + 1);
  struct icmp *inicmp = (struct icmp *) (inip + 1);

  // Calculate total IP length and ICMP data size
  int ip_total_len = ntohs(inip->ip_len);
  int ip_header_len = (inip->ip_vhl & 0x0F) * 4;
  int icmp_total_len = ip_total_len - ip_header_len;
  int icmp_data_len = icmp_total_len - sizeof(struct icmp);
  printf("ICMP: data_len=%d bytes\n", icmp_data_len);
  
  // Only respond to echo requests
  if (inicmp->type != ICMP_ECHO_REQUEST) {
#ifdef DYNAMIC_RX_BUFFERS
    kfree(inbuf);
#endif
    return;
  }
  
  printf("icmp_rx: received ICMP echo request\n");
  
  // Allocate buffer for ICMP reply
  char *buf = kalloc();
  if (buf == 0)
    panic("send_icmp_reply");
  
  // Ethernet header
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);  // 0x0800 for IP
  
  // IP header
  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45;  // IPv4, 5*4=20 byte header
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct icmp) + icmp_data_len);
  ip->ip_id = htons(1);
  ip->ip_off = 0;
  ip->ip_ttl = 64;
  ip->ip_p = IPPROTO_ICMP;
  ip->ip_sum = 0;  // Will calculate checksum later
  ip->ip_src = dst_ip;  // Our IP (was destination in request)
  ip->ip_dst = src_ip;  // Sender's IP (was source in request)
  
  // Calculate IP checksum (simple version)
  ip->ip_sum = ip_checksum(ip, sizeof(struct ip));
  
  // ICMP header
  struct icmp *icmp = (struct icmp *)(ip + 1);
  icmp->type = ICMP_ECHO_REPLY;
  icmp->code = 0;
  icmp->checksum = 0;
  icmp->id = inicmp->id;
  icmp->sequence = inicmp->sequence;
  
  // Calculate ICMP checksum
  icmp->checksum = icmp_checksum(icmp, sizeof(struct icmp) + icmp_data_len);
  
  // Send the reply
  designware_eth_ops.send(buf, sizeof(*eth) + sizeof(*ip) + sizeof(*icmp) + icmp_data_len);
#ifdef DYNAMIC_RX_BUFFERS
  kfree(inbuf);
#endif
  kfree(buf);
}


void
ip_rx(char *inbuf, int len) {
  struct eth *ineth = (struct eth *) inbuf;
  struct ip *inip = (struct ip *) (ineth + 1);
  
  printf("Receive IP packet: protocol %d, src: 0x%x, dst: 0x%x\n", 
         inip->ip_p, ntohl(inip->ip_src), ntohl(inip->ip_dst));
  
  // Check if packet is for us
  if (inip->ip_dst != htonl(local_ip)) {
    printf("IP packet not for us, dropping\n");
#ifdef DYNAMIC_RX_BUFFERS
    kfree(inbuf);
#endif
    return;
  }
  
  // Handle ICMP packets
  if (inip->ip_p == IPPROTO_ICMP) {
    icmp_rx(inbuf, inip->ip_src, inip->ip_dst);
  } else {
    // Handle other protocols (TCP/UDP) later
#ifdef DYNAMIC_RX_BUFFERS
    kfree(inbuf);
#endif
  }
}


int
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;
  printf("net_rx: received an ethernet packet of type 0x%x\n", ntohs(eth->type));
  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
#ifdef DYNAMIC_RX_BUFFERS
    kfree(buf);
#endif
  }
  return 0;
}