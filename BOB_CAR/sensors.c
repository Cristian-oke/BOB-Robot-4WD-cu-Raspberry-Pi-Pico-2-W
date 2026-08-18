#include "sensors.h"
#include "pico/mutex.h"

// stare interna (scrisa de Core 1 citita de Core 0) 
static volatile float g_dist_left   = DIST_MAX_CM;
static volatile float g_dist_center = DIST_MAX_CM;
static volatile float g_dist_right  = DIST_MAX_CM;

static mutex_t g_sensor_mutex;

void sensors_init(void) {
    mutex_init(&g_sensor_mutex);

    // TRIG pins — output
    uint trigs[] = {TRIG_LEFT, TRIG_CENTER, TRIG_RIGHT};
    for (int i = 0; i < 3; i++) {
        gpio_init(trigs[i]);
        gpio_set_dir(trigs[i], GPIO_OUT);
        gpio_put(trigs[i], 0);
    }

    // ECHO pins — input cu pull-down
    uint echos[] = {ECHO_LEFT, ECHO_CENTER, ECHO_RIGHT};
    for (int i = 0; i < 3; i++) {
        gpio_init(echos[i]);
        gpio_set_dir(echos[i], GPIO_IN);
        gpio_pull_down(echos[i]);
    }

    // LED pins — output
    uint leds[] = {LED_LEFT, LED_CENTER, LED_RIGHT};
    for (int i = 0; i < 3; i++) {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
        gpio_put(leds[i], 0);
    }
}

// masurare sincrona
// blocheaza pana la ECHO_TIMEOUT_US microsecunde
float sensors_measure(uint trig_pin, uint echo_pin) {
    // impuls TRIG 10us
    gpio_put(trig_pin, 0);
    sleep_us(2);
    gpio_put(trig_pin, 1);
    sleep_us(10);
    gpio_put(trig_pin, 0);

    // asteapta rising edge pe ECHO (timeout 15ms)
    uint32_t t0 = time_us_32();
    while (!gpio_get(echo_pin)) {
        if ((time_us_32() - t0) > 15000) return 401.0f; // cod eroare: nu a venit semnalul HIGH
    }

    // masara durata HIGH a ECHO
    t0 = time_us_32();
    while (gpio_get(echo_pin)) {
        if ((time_us_32() - t0) > ECHO_TIMEOUT_US) return 402.0f; // cod eroare: semnalul HIGH nu a coborat
    }
    uint32_t duration_us = time_us_32() - t0;
    return (duration_us * 0.0343f) / 2.0f;
}

// loop senzori (rulat pe Core 1) 
// apelat din main prin multicore_launch_core1()
void sensors_core1_loop(void) {
    while (true) {
        float dl = sensors_measure(TRIG_LEFT,   ECHO_LEFT);
        sleep_ms(20);  // pauza inter-senzor pentru evitare cross-talk
        float dc = sensors_measure(TRIG_CENTER, ECHO_CENTER);
        sleep_ms(20);
        float dr = sensors_measure(TRIG_RIGHT,  ECHO_RIGHT);
        sleep_ms(20);

        mutex_enter_blocking(&g_sensor_mutex);
        g_dist_left   = dl;
        g_dist_center = dc;
        g_dist_right  = dr;
        mutex_exit(&g_sensor_mutex);
    }
}

// getteri thread-safe 
float sensors_get_left_cm(void) {
    mutex_enter_blocking(&g_sensor_mutex);
    float v = g_dist_left;
    mutex_exit(&g_sensor_mutex);
    return v;
}

float sensors_get_center_cm(void) {
    mutex_enter_blocking(&g_sensor_mutex);
    float v = g_dist_center;
    mutex_exit(&g_sensor_mutex);
    return v;
}

float sensors_get_right_cm(void) {
    mutex_enter_blocking(&g_sensor_mutex);
    float v = g_dist_right;
    mutex_exit(&g_sensor_mutex);
    return v;
}

// LED helpers 
void leds_set(bool left, bool center, bool right) {
    gpio_put(LED_LEFT,   left);
    gpio_put(LED_CENTER, center);
    gpio_put(LED_RIGHT,  right);
}

void leds_off(void) {
    leds_set(false, false, false);
}
