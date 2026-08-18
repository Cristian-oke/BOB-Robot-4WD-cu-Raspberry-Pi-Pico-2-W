#pragma once
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// pini directie 
#define MOTOR_IN1   2   // motor stanga fata
#define MOTOR_IN2   3
#define MOTOR_IN3   4   // motor dreapta fata
#define MOTOR_IN4   5
#define MOTOR_ENA   14  // enable stanga  - PWM7A
#define MOTOR_ENB   15  // enable dreapta  - PWM7B

// constante PWM
#define MOTOR_PWM_FREQ_HZ   10000u   // 10 kHz silentios pentru DC
#define MOTOR_PWM_MAX       1000u    // rezolutie: 1000 trepte (0.1%)

// API public
void motors_init(void);

// mod digital (BLE manual + evitare obstacole) — vitezа 100%
void motors_forward(void);
void motors_backward(void);
void motors_turn_left(void);
void motors_turn_right(void);
void motors_stop(void);

// mod PWM progresiv (giroscop) — left/right: 0-100%
// directia e setata separat prin motors_set_direction()
void motors_set_pwm(uint8_t left_pct, uint8_t right_pct);

// utilitare
void motors_enable(void);
void motors_disable(void);
