#pragma once
#include <stdbool.h>

// algoritm de evitare obstacole

void obstacle_init(void);
void obstacle_tick(void);  // apelat in main loop cand ultima_directie == 'w'
