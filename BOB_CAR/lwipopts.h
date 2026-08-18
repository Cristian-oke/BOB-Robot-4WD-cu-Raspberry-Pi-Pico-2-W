
 // configurare lwIP pentru BOB_CAR (Pico W)
 
#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// optiuni de baza 
#define NO_SYS                    1    // fara OS (bare metal)
#define LWIP_SOCKET               0    // fara socket API (raw API)
#define LWIP_NETCONN              0    // fara netconn API
#define MEM_LIBC_MALLOC           0
#define MEMP_MEM_MALLOC           0
#define MEM_ALIGNMENT             4
#define MEM_SIZE                  16000
#define MEMP_NUM_TCP_SEG          32
#define MEMP_NUM_ARP_QUEUE        10
#define PBUF_POOL_SIZE            24
#define LWIP_ARP                  1
#define LWIP_ETHERNET             1
#define LWIP_ICMP                 1
#define LWIP_RAW                  1

// TCP
#define LWIP_TCP                  1
#define TCP_TTL                   255
#define TCP_MSS                   1460
#define TCP_SND_BUF               (8 * TCP_MSS)
#define TCP_SND_QUEUELEN          ((4 * TCP_SND_BUF) / TCP_MSS)
#define TCP_WND                   (8 * TCP_MSS)
#define LWIP_TCP_KEEPALIVE        1
#define MEMP_NUM_TCP_PCB          5
#define MEMP_NUM_TCP_PCB_LISTEN   3

// UDP + DHCP 
#define LWIP_UDP                  1
#define LWIP_DHCP                 1  // necesar pentru AP mode (DHCP server)
#define DHCP_DOES_ARP_CHECK       0

// DNS 
#define LWIP_DNS                  0

// statistici si debug 
#define LWIP_STATS                0
#define LWIP_DEBUG                0

// Checksumuri 
#define CHECKSUM_GEN_IP           1
#define CHECKSUM_GEN_UDP          1
#define CHECKSUM_GEN_TCP          1
#define CHECKSUM_CHECK_IP         1
#define CHECKSUM_CHECK_UDP        1
#define CHECKSUM_CHECK_TCP        1
#define LWIP_CHECKSUM_CTRL_PER_NETIF 0

// Timing 
#define LWIP_TIMERS               1
#define LWIP_TIMEVAL_PRIVATE      0
#define LWIP_PROVIDE_ERRNO        1

// SNTP (dezactivat)
#define SNTP_SERVER_DNS           0

// altele
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK   1
#define LWIP_NETIF_HOSTNAME        1
#define LWIP_NETIF_API             0

// Buffer output mare pentru a trimite HTML-ul intreg
#define TCP_OUTPUT_BUFFER_SIZE    (4 * TCP_MSS)

#endif 
