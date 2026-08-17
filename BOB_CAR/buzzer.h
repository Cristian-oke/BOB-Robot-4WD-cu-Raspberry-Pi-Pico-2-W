#ifndef BUZZER_H
#define BUZZER_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

#define BUZZER_PIN 13

void buzzer_init(void);
void buzzer_start_song(void);
void buzzer_stop(void);

#endif 
