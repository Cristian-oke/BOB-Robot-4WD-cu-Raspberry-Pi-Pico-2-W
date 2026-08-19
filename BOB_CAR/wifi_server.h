#pragma once
#include "pico/stdlib.h"
#include <stdbool.h>

// AP Wi-Fi credentials
#define WIFI_SSID     "BOB-WiFi"
#define WIFI_PASS     "bob12345"

// server port
#define HTTP_PORT     80

void   wifi_server_init(void);          // initializeaza AP + porneste server TCP
void   wifi_server_run(void);           // loop blocat — lansat pe Core 1

// date giroscop (scrise de Core 1 din WebSocket citite de Core 0 in FSM)
bool   wifi_gyro_has_data(void);
void   wifi_gyro_get(float *beta_deg, float *gamma_deg);
void   wifi_gyro_clear(void);
