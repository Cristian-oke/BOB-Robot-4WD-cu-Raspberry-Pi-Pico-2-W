#pragma once
#include "pico/stdlib.h"
#include <stdint.h>

//comenzi BLE acceptate
//compatibil cu serial Bluetooth Terminal (ca in Python)
// 'w' = inainte, 'a' = stanga, 's' = inapoi, 'd' = dreapta
// 'f' = Follow-Me mode, 'g' = Gyro WiFi mode, 'x' = stop/manual

// API public 
void ble_uart_init(void);
void ble_uart_task(void); // apelat periodic in main loop

bool    ble_uart_is_connected(void);
void    ble_uart_send(const char *msg);
char    ble_uart_get_last_cmd(void); // returneaza ultima comanda sau '\0'
void    ble_uart_clear_cmd(void);

// RSSI: valoare in dBm (ex: -45=bun/ -80=slab)
// returneaza 0 daca nu e conectat
int8_t  ble_uart_get_rssi(void);
