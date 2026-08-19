#include "follow_me.h"
#include "sensors.h"
#include "motors.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>

//Tuning
#define STOP_DIST_CM        55.0f   // prea aproape pe centru -> stop
#define MAX_FOLLOW_DIST_CM  150.0f  // dincolo de asta pe centru -> pierdut

// cand centrul pierde userul lateralele il cauta
// acceptare un lateral ca user gasit daca citeste sub MAX_LATERAL_CM
#define MAX_LATERAL_CM      160.0f

// protecție zgomot senzori
#define LATERAL_DEADBAND_CM 15.0f

// durata impulsului de pivot cand userul e pierdut pe centru
#define IMPULSE_MS          160

// viteza mersului inainte (cand centrul are userul)
#define FWD_SPEED           80

// viteza pivotului de cautare
#define TURN_SPEED          75

// timeout total pierdere yinta -> intoarcere la SEARCH
#define LOST_TIMEOUT_US     2500000  // 2.5 secunde

// stare interna
static FollowMeState g_state        = FM_SEARCH;
static absolute_time_t g_last_seen_time;
static absolute_time_t g_impulse_end_time;
static bool g_in_impulse            = false;

void follow_me_init(void) {
    g_state      = FM_SEARCH;
    g_in_impulse = false;
    printf("[FM] Pornit. Asteapta tinta pe centru...\n");
}

// Tick principal
void follow_me_tick(void) {
    float dc = sensors_get_center_cm();
    float ds = sensors_get_left_cm();   // Fizic DREAPTA
    float dd = sensors_get_right_cm();  // Fizic STANGA

    bool vc = (dc > 0.5f && dc < 400.0f);
    bool vr = (ds > 0.5f && ds < 400.0f);
    bool vl = (dd > 0.5f && dd < 400.0f);

    //SEARCH
    if (g_state == FM_SEARCH) {
        motors_stop();
        leds_set(true, false, true);

        if (vc && dc > STOP_DIST_CM && dc < MAX_FOLLOW_DIST_CM) {
            g_state      = FM_APPROACH;
            g_in_impulse = false;
            g_last_seen_time = get_absolute_time();
            printf("[FM] Target gasit la %.1f cm!\n", dc);
        }
        return;
    }

    // asteapta sfarsitul unui impuls
    if (g_in_impulse) {
        if (absolute_time_diff_us(g_impulse_end_time, get_absolute_time()) >= 0) {
            motors_stop();
            g_in_impulse = false;
            sleep_ms(60); 
        }
        return;
    }

    //verif daca centrul vede userul
    bool center_has_user = vc && (dc > STOP_DIST_CM) && (dc < MAX_FOLLOW_DIST_CM);

    if (center_has_user) {
        // centrul vede userul -> merge DREPT lateralele complet ignorate
        g_last_seen_time = get_absolute_time();
        g_state = FM_APPROACH;
        leds_set(true, true, true);
        motors_forward();
        motors_set_pwm(FWD_SPEED, FWD_SPEED);
        return;
    }

    //prea apoarte pe centru -> stop
    if (vc && dc <= STOP_DIST_CM) {
        g_state = FM_TOO_CLOSE;
        motors_stop();
        leds_set(false, true, false);
        return;
    }

    //centrul nu mai vede userul -> trece in cautare laterala
    //timeout total -> intoarcere la cautare
    if (absolute_time_diff_us(g_last_seen_time, get_absolute_time()) > LOST_TIMEOUT_US) {
        printf("[FM] Target pierdut complet. Revin la Search.\n");
        motors_stop();
        g_state = FM_SEARCH;
        return;
    }

    //cautare user pe senzorii laterali
    float dist_left  = (vl && dd < MAX_LATERAL_CM) ? dd : 400.0f;
    float dist_right = (vr && ds < MAX_LATERAL_CM) ? ds : 400.0f;

    bool left_sees  = (dist_left  < MAX_LATERAL_CM);
    bool right_sees = (dist_right < MAX_LATERAL_CM);

    leds_set(true, false, true); 

    if (!left_sees && !right_sees) {
        motors_stop();
        return;
    }

    if (left_sees && right_sees) {
        float diff = dist_left - dist_right;

        if (fabsf(diff) < LATERAL_DEADBAND_CM) {
            if (dist_left < 90.0f && dist_right < 90.0f) {
                printf("[FM] Culoar detectat (L=%.1f R=%.1f) -> trec inainte\n",
                       dist_left, dist_right);
                motors_forward();
                motors_set_pwm(80, 80);
            } else {
                // departe si simetric -> nu e clar ce e -> sta pe loc
                motors_stop();
            }
            return;
        }

        if (diff < 0) {
            // stanga mai aproape -> impuls stanga
            motors_turn_left();
            motors_set_pwm(TURN_SPEED, TURN_SPEED);
            printf("[FM] Cauta STANGA (diff=%.1f)\n", diff);
        } else {
            // dreapta mai aproape -> impuls dreapta
            motors_turn_right();
            motors_set_pwm(TURN_SPEED, TURN_SPEED);
            printf("[FM] Cauta DREAPTA (diff=%.1f)\n", diff);
        }
    } else if (left_sees) {
        // numai stanga vede ceva -> impuls stanga
        motors_turn_left();
        motors_set_pwm(TURN_SPEED, TURN_SPEED);
        printf("[FM] Cauta STANGA (doar lateral stg)\n");
    } else {
        // numai dreapta vede ceva -> impuls dreapta
        motors_turn_right();
        motors_set_pwm(TURN_SPEED, TURN_SPEED);
        printf("[FM] Cauta DREAPTA (doar lateral dr)\n");
    }

    g_in_impulse       = true;
    g_impulse_end_time = make_timeout_time_ms(IMPULSE_MS);
}

FollowMeState follow_me_get_state(void) { return g_state; }
