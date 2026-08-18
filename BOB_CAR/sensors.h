#pragma once
#include "pico/stdlib.h"

// pini senzori 
#define TRIG_LEFT    16
#define ECHO_LEFT     6
#define TRIG_CENTER  17
#define ECHO_CENTER   8
#define TRIG_RIGHT   18
#define ECHO_RIGHT   10

// pini LED-uri 
#define LED_LEFT      7
#define LED_CENTER    9
#define LED_RIGHT    11

// constante 
#define DIST_STOP_CM        40.0f   
#define DIST_FOLLOW_STOP_CM 30.0f   
#define DIST_MAX_CM        400.0f   
#define ECHO_TIMEOUT_US    15000u  

// API public 
void sensors_init(void);

// masurare sincrona (blocanta 15ms max) — apelata din Core 1
float sensors_measure(uint trig_pin, uint echo_pin);

// loop senzori rulat pe Core 1 (blocat cu sleep_ms intern)
void sensors_core1_loop(void);

// getteri pentru valorile cached (actualizate de Core 1)
float sensors_get_left_cm(void);
float sensors_get_center_cm(void);
float sensors_get_right_cm(void);

// LED helpers
void leds_set(bool left, bool center, bool right);
void leds_off(void);
