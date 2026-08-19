#include "obstacle.h"
#include "sensors.h"
#include "motors.h"
#include <stdio.h>

// 0 = drum liber / reset
// 1 = evita spre stanga
// 2 = evita spre dreapta

static int g_avoid_state = 0;

void obstacle_init(void) {
    g_avoid_state = 0;
}


// apelat din main loop doar cand comanda activa e 'w' (inainte)
void obstacle_tick(void) {
    float ds = sensors_get_left_cm();
    float dc = sensors_get_center_cm();
    float dd = sensors_get_right_cm();

    // drum liber
    if (dc > DIST_STOP_CM && ds > 15.0f && dd > 15.0f) {
        g_avoid_state = 0;
        motors_forward();
        leds_set(false, true, false);   // bec centru aprins
    } else {
        // obstacol detectat — alegere directie de evitare
        if (dd > ds || g_avoid_state == 1) {
            // dreapta mai libera -> evitare spre stanga
            if (g_avoid_state == 0) {
                motors_backward();      // scurt inapoi inainte de pivotare
                sleep_ms(150);
            }
            g_avoid_state = 1;
            motors_turn_left();
            leds_set(true, false, false);  // bec stanga aprins
            printf("[OBS] Evit stanga (ds=%.1f dc=%.1f dd=%.1f)\n", ds, dc, dd);

        } else if (dd < ds || g_avoid_state == 2) {
            // stanga mai libera -> evitare spre dreapta
            if (g_avoid_state == 0) {
                motors_backward();
                sleep_ms(150);
            }
            g_avoid_state = 2;
            motors_turn_right();
            leds_set(false, false, true);  // bec dreapta aprins
            printf("[OBS] Evit dreapta (ds=%.1f dc=%.1f dd=%.1f)\n", ds, dc, dd);
        }
    }
}
