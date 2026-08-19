#include "wifi_server.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "pico/mutex.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// SHA1 compact (pentru WebSocket handshake RFC 6455)
typedef struct { uint32_t h[5]; uint8_t buf[64]; uint32_t bits[2]; uint32_t cnt; } SHA1;
static void sha1_init(SHA1 *c) {
    c->h[0]=0x67452301; c->h[1]=0xEFCDAB89; c->h[2]=0x98BADCFE; c->h[3]=0x10325476; c->h[4]=0xC3D2E1F0;
    c->bits[0]=c->bits[1]=c->cnt=0;
}
#define ROL32(v,n) (((v)<<(n))|((v)>>(32-(n))))
static void sha1_block(SHA1 *c, const uint8_t *p) {
    uint32_t w[80], a,b,d,e,f,k,t; int i;
    for(i=0;i<16;i++) w[i]=(uint32_t)p[i*4]<<24|(uint32_t)p[i*4+1]<<16|(uint32_t)p[i*4+2]<<8|p[i*4+3];
    for(;i<80;i++) w[i]=ROL32(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=c->h[0];b=c->h[1];uint32_t cc=c->h[2];d=c->h[3];e=c->h[4];
    for(i=0;i<80;i++){
        if(i<20){f=(b&cc)|(~b&d);k=0x5A827999;}
        else if(i<40){f=b^cc^d;k=0x6ED9EBA1;}
        else if(i<60){f=(b&cc)|(b&d)|(cc&d);k=0x8F1BBCDC;}
        else{f=b^cc^d;k=0xCA62C1D6;}
        t=ROL32(a,5)+f+e+k+w[i]; e=d; d=cc; cc=ROL32(b,30); b=a; a=t;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;
}
static void sha1_update(SHA1 *c, const uint8_t *data, size_t len) {
    for(size_t i=0;i<len;i++){
        c->buf[c->cnt++]=data[i];
        if(c->cnt==64){sha1_block(c,c->buf);c->cnt=0;c->bits[1]+=512;if(!c->bits[1])c->bits[0]++;}
    }
}
static void sha1_final(SHA1 *c, uint8_t *out) {
    uint64_t bits=(uint64_t)(c->bits[0]<<9|c->bits[1]>>23)*8+(c->cnt*8);
    uint8_t pad=0x80; sha1_update(c,&pad,1);
    while(c->cnt!=56){pad=0;sha1_update(c,&pad,1);}
    for(int i=7;i>=0;i--){pad=(bits>>(i*8))&0xFF;sha1_update(c,&pad,1);}
    for(int i=0;i<5;i++){out[i*4]=c->h[i]>>24;out[i*4+1]=(c->h[i]>>16)&0xFF;out[i*4+2]=(c->h[i]>>8)&0xFF;out[i*4+3]=c->h[i]&0xFF;}
}

// ════════════════════════════════════════════════════════════════════════════
//  Base64 encode
// ════════════════════════════════════════════════════════════════════════════
static const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(const uint8_t *in, size_t len, char *out) {
    size_t i=0,j=0;
    for(;i+2<len;i+=3){
        out[j++]=B64[in[i]>>2]; out[j++]=B64[((in[i]&3)<<4)|(in[i+1]>>4)];
        out[j++]=B64[((in[i+1]&0xF)<<2)|(in[i+2]>>6)]; out[j++]=B64[in[i+2]&0x3F];
    }
    if(i<len){
        out[j++]=B64[in[i]>>2];
        if(i+1<len){out[j++]=B64[((in[i]&3)<<4)|(in[i+1]>>4)];out[j++]=B64[(in[i+1]&0xF)<<2];}
        else{out[j++]=B64[(in[i]&3)<<4];out[j++]='=';}
        out[j++]='=';
    }
    out[j]='\0';
}

//  HTML page embedded (generat din lhf_web/index.html la build time)
extern const char   _binary_lhf_web_index_html_start[];
extern const char   _binary_lhf_web_index_html_end[];
extern const size_t _binary_lhf_web_index_html_size;

//  State shared (Core 1 scrie Core 0 citeste)
static volatile float  g_beta  = 0.0f;
static volatile float  g_gamma = 0.0f;
static volatile bool   g_has_data = false;
static mutex_t g_gyro_mutex;

//  WebSocket connection state
#define MAX_CLIENTS 6
typedef struct {
    struct tcp_pcb *pcb;
    bool ws_upgraded;
} Client;
static Client g_clients[MAX_CLIENTS];

//  WebSocket helpers
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static void ws_compute_accept(const char *key, char *out_b64) {
    char concat[128];
    snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
    SHA1 sha; sha1_init(&sha);
    sha1_update(&sha, (const uint8_t *)concat, strlen(concat));
    uint8_t digest[20]; sha1_final(&sha, digest);
    base64_encode(digest, 20, out_b64);
}

// parseaza frame WebSocket de la client (mascat)
// returneaza numarul de bytes de payload sau -1 la eroare
static int ws_parse_frame(const uint8_t *buf, size_t len, uint8_t *payload, size_t max_payload) {
    if (len < 2) return -1;
    // bool fin  = (buf[0] & 0x80) != 0;
    uint8_t opcode = buf[0] & 0x0F;
    if (opcode == 0x8) return -2;  // close frame
    bool masked = (buf[1] & 0x80) != 0;
    uint64_t plen = buf[1] & 0x7F;
    size_t hdr = 2;
    if (plen == 126) { if(len<4) return -1; plen=(buf[2]<<8)|buf[3]; hdr=4; }
    else if (plen == 127) { return -1; }  // prea lung
    if (masked) hdr += 4;
    if ((size_t)(hdr + plen) > len) return -1;
    if (plen > max_payload) return -1;
    const uint8_t *mask = masked ? buf + hdr - 4 : NULL;
    const uint8_t *data = buf + hdr;
    for (uint64_t i = 0; i < plen; i++)
        payload[i] = masked ? (data[i] ^ mask[i % 4]) : data[i];
    payload[plen] = '\0';
    return (int)plen;
}

// parseaza JSON simplu {"b":X,"g":Y}
static void parse_gyro_json(const char *json) {
    const char *pb = strstr(json, "\"b\":");
    const char *pg = strstr(json, "\"g\":");
    const char *ps = strstr(json, "\"stop\":");
    if (ps) {
        mutex_enter_blocking(&g_gyro_mutex);
        g_beta = 0; g_gamma = 0; g_has_data = true;
        mutex_exit(&g_gyro_mutex);
        return;
    }
    if (!pb || !pg) return;
    float beta  = strtof(pb + 4, NULL);
    float gamma = strtof(pg + 4, NULL);
    mutex_enter_blocking(&g_gyro_mutex);
    g_beta    = beta;
    g_gamma   = gamma;
    g_has_data = true;
    mutex_exit(&g_gyro_mutex);
}

//  Trimitere raspuns HTTP sau WebSocket upgrade
static void client_send_http_html(struct tcp_pcb *pcb) {
    static char hdr[256];
    size_t html_len = _binary_lhf_web_index_html_size;
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n\r\n", (unsigned)html_len);
    tcp_write(pcb, hdr, (u16_t)strlen(hdr), TCP_WRITE_FLAG_COPY);
    tcp_write(pcb, _binary_lhf_web_index_html_start, (u16_t)html_len, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
}

static void client_send_ws_upgrade(struct tcp_pcb *pcb, const char *key) {
    char accept[32];
    ws_compute_accept(key, accept);
    static char resp[256];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
    tcp_write(pcb, resp, (u16_t)n, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
}

//  lwIP TCP callbacks
static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    Client *c = (Client *)arg;
    if (!p || err != ERR_OK) { tcp_close(pcb); c->pcb = NULL; return ERR_OK; }

    uint8_t *data = (uint8_t *)p->payload;
    u16_t len = p->len;

    if (!c->ws_upgraded) {
        // parsare request HTTP pentru upgrade WebSocket
        char *req = (char *)data;
        if (strstr(req, "Upgrade: websocket") || strstr(req, "Upgrade: WebSocket")) {
            // gasire Sec-WebSocket-Key
            char *kstart = strstr(req, "Sec-WebSocket-Key:");
            if (kstart) {
                kstart += 19;  // skip "Sec-WebSocket-Key: "
                char key[32] = {0};
                int ki = 0;
                while (*kstart && *kstart != '\r' && ki < 31) key[ki++] = *kstart++;
                client_send_ws_upgrade(pcb, key);
                c->ws_upgraded = true;
                printf("[WS] Client conectat WebSocket.\n");
            }
        } else if (strstr(req, "GET / ") || strstr(req, "GET /favicon")) {
            client_send_http_html(pcb);
            // eliberare slot dupa ce sev trimite pagina HTML (Connection: close)
            tcp_recved(pcb, p->tot_len);
            pbuf_free(p);
            tcp_close(pcb);
            c->pcb = NULL;
            return ERR_OK;
        }
    } else {
        // frame WebSocket de la client
        uint8_t payload[256];
        int plen = ws_parse_frame(data, len, payload, sizeof(payload) - 1);
        if (plen > 0) {
            parse_gyro_json((char *)payload);
        } else if (plen == -2) {
            // Close frame
            tcp_close(pcb);
            c->pcb = NULL;
        }
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void on_conn_err(void *arg, err_t err) {
    Client *c = (Client *)arg;
    if (c) { c->pcb = NULL; }
}

static err_t on_accept(void *arg, struct tcp_pcb *new_pcb, err_t err) {
    if (err != ERR_OK || !new_pcb) return ERR_VAL;
    // Gasim slot liber
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i].pcb) {
            g_clients[i].pcb = new_pcb;
            g_clients[i].ws_upgraded = false;
            tcp_arg(new_pcb, &g_clients[i]);
            tcp_recv(new_pcb, on_recv);
            tcp_err(new_pcb, on_conn_err);
            return ERR_OK;
        }
    }
    // nu mai sunt sloturi
    tcp_abort(new_pcb);
    return ERR_ABRT;
}

// initializare + Run (pe Core 1)
// server DHCP oficial din pico-examples (dhcpserver.h/c)
#include "dhcpserver.h"

static dhcp_server_t g_dhcp_server;

void wifi_server_init(void) {
    mutex_init(&g_gyro_mutex);
    memset(g_clients, 0, sizeof(g_clients));

    cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t gw, mask;
    IP4_ADDR(&gw, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);

    struct netif *ap_netif = &cyw43_state.netif[CYW43_ITF_AP];
    netif_set_addr(ap_netif, &gw, &mask, &gw);
    netif_set_up(ap_netif);
    netif_set_link_up(ap_netif);

    printf("[WiFi] AP pornit: SSID=%s IP=192.168.4.1\n", WIFI_SSID);

    //pornire server DHCP oficial
    ip_addr_t dhcp_ip, dhcp_nm;
    ip_addr_copy(dhcp_ip, *netif_ip4_addr(ap_netif));
    ip_addr_copy(dhcp_nm, *netif_ip4_netmask(ap_netif));
    dhcp_server_init(&g_dhcp_server, &dhcp_ip, &dhcp_nm);
    printf("[WiFi] DHCP Server pornit.\n");

    // porneste TCP listener pe port 80 (HTTP + WebSocket)
    struct tcp_pcb *listen_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    tcp_bind(listen_pcb, IP_ANY_TYPE, HTTP_PORT);
    listen_pcb = tcp_listen(listen_pcb);
    tcp_accept(listen_pcb, on_accept);
    printf("[WiFi] HTTP+WS server pe portul %d\n", HTTP_PORT);
}

void wifi_server_run(void) {
    // loop infinit — Core 1 proceseaza lwIP
    while (true) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
}

//  getteri pentru datele giroscop (apelati din Core 0)
bool wifi_gyro_has_data(void) {
    mutex_enter_blocking(&g_gyro_mutex);
    bool v = g_has_data;
    mutex_exit(&g_gyro_mutex);
    return v;
}

void wifi_gyro_get(float *beta_deg, float *gamma_deg) {
    mutex_enter_blocking(&g_gyro_mutex);
    *beta_deg  = g_beta;
    *gamma_deg = g_gamma;
    mutex_exit(&g_gyro_mutex);
}

void wifi_gyro_clear(void) {
    mutex_enter_blocking(&g_gyro_mutex);
    g_has_data = false;
    mutex_exit(&g_gyro_mutex);
}
