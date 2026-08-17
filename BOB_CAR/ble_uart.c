#include "ble_uart.h"
#include "bob_ble.h"   // generat din bob_ble.gatt de pico_btstack_make_gatt_header

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "ble/gatt-service/nordic_spp_service_server.h"
#include <stdio.h>
#include <string.h>

// stare interna 
static hci_con_handle_t  g_conn_handle  = HCI_CON_HANDLE_INVALID;
static volatile char     g_last_cmd     = '\0';
static volatile int8_t   g_rssi         = 0;
static btstack_timer_source_t g_rssi_timer;
static btstack_packet_callback_registration_t g_hci_cb_reg;

//helpers
static bool is_connected(void) {
    return g_conn_handle != HCI_CON_HANDLE_INVALID;
}

//citeste RSSI la interval de 500ms
static void rssi_timer_cb(btstack_timer_source_t *ts) {
    if (is_connected()) {
        hci_send_cmd(&hci_read_rssi, g_conn_handle);
    }
    btstack_run_loop_set_timer(ts, 500);
    btstack_run_loop_add_timer(ts);
}

// pachet advertising 
// identic cu exemplul nordic_spp_le_counter din BTstack
static const uint8_t adv_data[] = {
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    5, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'B', 'O', 'B', '2',
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
        0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
        0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e,};

// Nordic SPP packet handler 
// primeste date de la telefon ca RFCOMM_DATA_PACKET
// si evenimente de conexiune/deconexiune ca HCI_EVENT_GATTSERVICE_META
static void nordic_spp_packet_handler(uint8_t packet_type, uint16_t channel,uint8_t *packet, uint16_t size) {
    UNUSED(channel);

    switch (packet_type) {
        case RFCOMM_DATA_PACKET:
            // date primite de la telefon (echivalent cu att_write_cb pe RX)
            if (size >= 1) {
                char cmd = (char)packet[0];
                if (cmd == 'w' || cmd == 'a' || cmd == 's' || cmd == 'd' ||
                    cmd == 'f' || cmd == 'g' || cmd == 'x') {
                    g_last_cmd = cmd;
                    printf("[BLE] Comanda primita: '%c'\n", cmd);
                }
            }
            break;

        case HCI_EVENT_PACKET:
            if (hci_event_packet_get_type(packet) == HCI_EVENT_GATTSERVICE_META) {
                switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
                    case GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED:
                        g_conn_handle = gattservice_subevent_spp_service_connected_get_con_handle(packet);
                        printf("[BLE] NUS Conectat! Handle: 0x%04x\n", g_conn_handle);
                        break;
                    case GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED:
                        printf("[BLE] NUS Deconectat.\n");
                        g_conn_handle = HCI_CON_HANDLE_INVALID;
                        g_rssi        = 0;
                        g_last_cmd    = '\0';
                        break;
                    default:
                        break;
                }
            }
            break;

        default:
            break;
    }
}

//HCI event handler (advertising, RSSI, disconnection)
static void hci_packet_handler(uint8_t ptype, uint16_t channel,
                                uint8_t *packet, uint16_t size) {
    UNUSED(channel); UNUSED(size);

    if (ptype != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                printf("[BLE] BTstack activ. Pornesc advertising...\n");

                bd_addr_t null_addr;
                memset(null_addr, 0, 6);
                gap_advertisements_set_params(0x0030, 0x0030, 0, 0, null_addr, 0x07, 0x00);
                gap_advertisements_set_data(sizeof(adv_data), (uint8_t*)adv_data);
                gap_advertisements_enable(1);

                // porneste timer RSSI
                btstack_run_loop_set_timer_handler(&g_rssi_timer, rssi_timer_cb);
                btstack_run_loop_set_timer(&g_rssi_timer, 500);
                btstack_run_loop_add_timer(&g_rssi_timer);
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("[BLE] Deconectat. Advertising din nou...\n");
            g_conn_handle = HCI_CON_HANDLE_INVALID;
            g_rssi        = 0;
            g_last_cmd    = '\0';
            gap_advertisements_enable(1);
            break;

        case HCI_EVENT_COMMAND_COMPLETE:
            // procesare raspuns de la HCI_READ_RSSI
            if (HCI_OPCODE_HCI_READ_RSSI ==
                hci_event_command_complete_get_command_opcode(packet)) {
                if (packet[5] == 0) {  // status == success
                    g_rssi = (int8_t)packet[8];
                }
            }
            break;

        default:
            break;
    }
}

// initializare
void ble_uart_init(void) {
    l2cap_init();

    sm_init();
    att_server_init(profile_data, NULL, NULL);
    nordic_spp_service_server_init(&nordic_spp_packet_handler);
    g_hci_cb_reg.callback = &hci_packet_handler;
    hci_add_event_handler(&g_hci_cb_reg);
    hci_power_control(HCI_POWER_ON);
    printf("[BLE] Init complet (Nordic SPP Service). Asteptam HCI_STATE_WORKING...\n");
}

void ble_uart_task(void) {
    // in modul threadsafe_background, BTstack ruleaza in interrupt context
}

bool ble_uart_is_connected(void) { return is_connected(); }

void ble_uart_send(const char *msg) {
    if (!is_connected()) return;
    nordic_spp_service_server_send(g_conn_handle,(const uint8_t *)msg, (uint16_t)strlen(msg));
}

char ble_uart_get_last_cmd(void)  { return g_last_cmd; }
void ble_uart_clear_cmd(void)     { g_last_cmd = '\0'; }
int8_t ble_uart_get_rssi(void)    { return g_rssi; }
