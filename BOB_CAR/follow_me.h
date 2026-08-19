#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Follow-Me: Steering diferential cu dead-band si impuls scurt
 Logica:
  1. SEARCH  — asteapta tinta pe senzorul central
  2. APPROACH — merge spre tintă cu steering proportional
               • |diff| < DEADBAND  -> drept
               • DEADBAND < |diff| < HARD_TURN -> steering soft (diferential)
               • |diff| >= HARD_TURN -> impuls scurt de pivotare (IMPULSE_MS ms)
  3. TOO_CLOSE — s-a apropiat sub STOP_DIST_CM -> stop
 RSSI BLE nu mai este folosit in logica de urmarire
*/

typedef enum {
    FM_SEARCH,       // sta pe loc asteapta detectia centrala
    FM_APPROACH,     // urmareste tinta (forward + steering)
    FM_REORIENT,     // rezervat / nefolosit momentan
    FM_TOO_CLOSE     // prea aproape -> stop
} FollowMeState;

void follow_me_init(void);
void follow_me_tick(void);          // apelat din main loop in modul Follow-Me
FollowMeState follow_me_get_state(void);
