/**
 Main entry point
 Robot 4WD Raspberry Pi Pico 2 W
 
 Core 0: FSM robot (Manual BLE, Evitare Obstacole, Follow-Me, Giroscop WiFi)
 Core 1: Senzori ultrasonici (masurare sincrona) + WiFi server (mod Giroscop)
 
  Moduri de operare (selectate prin BLE):
    - Manual  : comenzi w/a/s/d cu evitare activa la 'w'
    - FollowMe: algoritm hibrid Ultrasonic + BLE RSSI (comanda 'f')
    - Gyro    : control prin giroscop telefon via WiFi+WebSocket (comanda 'g')
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "motors.h"
#include "sensors.h"
#include "ble_uart.h"
#include "wifi_server.h"
#include "obstacle.h"
#include "follow_me.h"
#include "buzzer.h"

// definitii globale
typedef enum {
    MODE_MANUAL,     // Control BLE (w/a/s/d) + evitare obstacole la 'w'
    MODE_FOLLOW_ME,  // Urmarire hibrida (Ultrasonic + RSSI)
    MODE_GYRO        // Giroscop WiFi (WebSocket)
} RobotMode;

static volatile RobotMode g_mode = MODE_MANUAL;

#define CMD_TIMEOUT_MS 200

// pini giroscop → mapare motoare
#define GYRO_DEAD_ZONE 12.0f   // grade dead zone
#define GYRO_MAX_TILT  45.0f   // grade la viteza maxima (100%)

//  Core 1 — Senzori + WiFi server (in functie de mod)
static volatile bool g_gyro_mode_active = false;

void core1_entry(void) {
    printf("[Core1] Pornit exclusiv pentru senzori\n");
    sensors_core1_loop();
}

//  Mapare giroscop → comenzi motor (PWM diferential)
//  beta  = inclinare inainte/inapoi (Y)
//  gamma = inclinare stanga/dreapta (X)
static void gyro_apply(float beta, float gamma) {
    float abs_b = beta < 0 ? -beta : beta;
    float abs_g = gamma < 0 ? -gamma : gamma;

    bool in_dead_beta = (abs_b <= GYRO_DEAD_ZONE);
    bool in_dead_gamma = (abs_g <= GYRO_DEAD_ZONE);

    // Daca joystick-ul e pe centru -> STOP
    if (in_dead_beta && in_dead_gamma) {
        motors_stop();
        return;
    }

    //ROTIRE PE LOC (doar stanga sau dreapta)
    if (in_dead_beta) {
        float raw_turn = (abs_g - GYRO_DEAD_ZONE) / (GYRO_MAX_TILT - GYRO_DEAD_ZONE) * 100.0f;
        if (raw_turn > 100.0f) raw_turn = 100.0f;
        
        // mapare la [50, 100] (in loc are nevoie de mai mult cuplu sa rupa aderenta)
        uint8_t final_pwm = (uint8_t)(50.0f + (raw_turn * 0.5f));
        
        if (gamma > 0) {
            motors_turn_right(); // dreapta pe loc (stanga inainte, dreapta inapoi)
        } else {
            motors_turn_left();  // stanga pe loc (stanga inapoi, dreapta inainte)
        }
        motors_set_pwm(final_pwm, final_pwm);
        return;
    }

    //MERS INAINTE/INAPOI + DIAGONALA
    bool going_fwd = (beta > 0);
    float raw_speed = (abs_b - GYRO_DEAD_ZONE) / (GYRO_MAX_TILT - GYRO_DEAD_ZONE) * 100.0f;
    if (raw_speed > 100.0f) raw_speed = 100.0f;
    
    float raw_left = raw_speed;
    float raw_right = raw_speed;

    // calcul diferential pentru diagonala
    if (!in_dead_gamma) {
        float turn_factor = (abs_g - GYRO_DEAD_ZONE) / (GYRO_MAX_TILT - GYRO_DEAD_ZONE);
        if (turn_factor > 1.0f) turn_factor = 1.0f;
        
        if (gamma > 0) { // joystick in fata-dreapta -> reduce puterea rotilor din dreaota
            raw_right = raw_right * (1.0f - turn_factor);
        } else {         // joystick in fata-stanga -> reduce puterea rotilor din stanga
            raw_left = raw_left * (1.0f - turn_factor);
        }
    }

    // mapare PWM minim garantat pentru fiecare roata in miscare
    uint8_t left_pwm = 0;
    if (raw_left > 1.0f) {
        left_pwm = (uint8_t)(50.0f + (raw_left * 0.5f)); // minim 50%
    }
    uint8_t right_pwm = 0;
    if (raw_right > 1.0f) {
        right_pwm = (uint8_t)(50.0f + (raw_right * 0.5f)); // minim 50%
    }

    // directie generala si Siguranta senzori
    if (going_fwd) {
        float dist = sensors_get_center_cm();
        if (dist > 1.0f && dist < DIST_FOLLOW_STOP_CM) {
            motors_stop();
            return;
        }
        motors_forward();
    } else {
        motors_backward();
    }

    motors_set_pwm(left_pwm, right_pwm);
}

//  Main — Core 0
int main(void) {
    stdio_init_all();
    sleep_ms(1000); 
    printf("\n╔══════════════════════════╗\n");
    printf("║   BOB v2.0 — Pico C SDK  ║\n");
    printf("╚══════════════════════════╝\n");

    // initializare periferice
    motors_init();
    sensors_init();
    obstacle_init();
    buzzer_init();
    
    // porneste cantecul pe fundal non-stop
    buzzer_start_song();

    printf("[INIT] Motoare + Senzori OK\n");

    // initializare arhitectura hardware CYW43 (PENTRU WIFI + BLUETOOTH)
    if (cyw43_arch_init()) {
        printf("[FATAL] Eroare initializare CYW43!\n");
        return -1;
    }
    printf("[INIT] CYW43 Hardware OK\n");

    ble_uart_init();
    printf("[INIT] BLE OK — Advertising 'BOB'...\n");

    wifi_server_init();
    printf("[INIT] WiFi AP 'BOB-WiFi' OK\n");

    // lanseaza Core 1
    multicore_launch_core1(core1_entry);
    printf("[INIT] Core 1 pornit (Senzori)\n");

    // LED feedback boot
    leds_set(true, true, true);
    sleep_ms(500);
    leds_off();

    printf("[BOB] Gata! Asteapta comenzi BLE...\n");

    //  MAIN LOOP — Core 0
    absolute_time_t last_cmd_time = get_absolute_time();
    char last_dir = '\0';

    while (true) {

        // procesare comanda BLE 
        char cmd = ble_uart_get_last_cmd();
        if (cmd != '\0') {
            ble_uart_clear_cmd();

            if (cmd == 'f') {
                // activare Follow-Me
                g_mode = MODE_FOLLOW_ME;
                g_gyro_mode_active = false;
                follow_me_init();
                ble_uart_send("MOD: Follow-Me\n");
                printf("[MOD] Follow-Me activat\n");

            } else if (cmd == 'g') {
                // activare Gyro WiFi
                g_mode = MODE_GYRO;
                g_gyro_mode_active = true;
                motors_stop();
                ble_uart_send("MOD: Gyro WiFi. Conecteaza-te la BOB-WiFi!\n");
                printf("[MOD] Gyro activat\n");

            } else if (cmd == 'x') {
                // stop / manual
                g_mode = MODE_MANUAL;
                g_gyro_mode_active = false;
                motors_stop();
                leds_off();
                last_dir = '\0';
                ble_uart_send("MOD: Manual\n");
                printf("[MOD] Manual\n");

            } else {
                // comanda de miscare (w/a/s/d) — valida doar in manual
                if (g_mode == MODE_MANUAL) {
                    last_cmd_time = get_absolute_time();
                    last_dir = cmd;
                }
            }
        }

        //  FSM moduri de operare
        switch (g_mode) {

            case MODE_MANUAL: {
                int64_t elapsed_ms = absolute_time_diff_us(last_cmd_time, get_absolute_time()) / 1000;

                if (elapsed_ms > CMD_TIMEOUT_MS) {
                    // timeout 200ms fara comanda -< STOP
                    motors_stop();
                    leds_off();
                    last_dir = '\0';
                    obstacle_init();  // reset stare evitare
                } else {
                    switch (last_dir) {
                        case 'w':
                            obstacle_tick();  // inainte cu evitare obstacole
                            break;
                        case 's':
                            motors_backward();
                            leds_set(false, false, false);
                            break;
                        case 'a':
                            motors_turn_left();
                            leds_set(true, false, false);
                            break;
                        case 'd':
                            motors_turn_right();
                            leds_set(false, false, true);
                            break;
                        default:
                            motors_stop();
                            leds_off();
                            break;
                    }
                }
                break;
            }

            case MODE_FOLLOW_ME:
                follow_me_tick();
                break;

            case MODE_GYRO:
                if (wifi_gyro_has_data()) {
                    float beta, gamma;
                    wifi_gyro_get(&beta, &gamma);
                    wifi_gyro_clear();
                    gyro_apply(beta, gamma);
                }
                break;
        }
        // debug Senzori periodic prin BLE
        static uint32_t last_debug_ms = 0;
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (g_mode == MODE_MANUAL && (now_ms - last_debug_ms > 1000)) {
            char dbg[64];
            snprintf(dbg, sizeof(dbg), "L:%.0f C:%.0f R:%.0f\n", 
                     sensors_get_left_cm(), sensors_get_center_cm(), sensors_get_right_cm());
            ble_uart_send(dbg);
            last_debug_ms = now_ms;
        }

        sleep_ms(10);  // frecventa loop ~100Hz
    }
}
