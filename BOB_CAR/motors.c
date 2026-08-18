#include "motors.h"

// stare interna 
static uint g_pwm_slice;

// utilitare interne 
static inline void set_dir(uint in1, uint in2, uint in3, uint in4) {
    gpio_put(MOTOR_IN1, in1);
    gpio_put(MOTOR_IN2, in2);
    gpio_put(MOTOR_IN3, in3);
    gpio_put(MOTOR_IN4, in4);
}

static inline void pwm_set_both(uint16_t level_a, uint16_t level_b) {
    pwm_set_chan_level(g_pwm_slice, PWM_CHAN_A, level_a);
    pwm_set_chan_level(g_pwm_slice, PWM_CHAN_B, level_b);
}

static inline uint16_t pct_to_level(uint8_t pct) {
    if (pct > 100) pct = 100;
    return (uint16_t)((pct * MOTOR_PWM_MAX) / 100);
}

// implementare API

void motors_init(void) {
    // pini directie — digital output
    uint dir_pins[] = {MOTOR_IN1, MOTOR_IN2, MOTOR_IN3, MOTOR_IN4};
    for (int i = 0; i < 4; i++) {
        gpio_init(dir_pins[i]);
        gpio_set_dir(dir_pins[i], GPIO_OUT);
        gpio_put(dir_pins[i], 0);
    }

    // GPIO14 si GPIO15 → PWM7A si PWM7B
    gpio_set_function(MOTOR_ENA, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR_ENB, GPIO_FUNC_PWM);

    g_pwm_slice = pwm_gpio_to_slice_num(MOTOR_ENA);  // slice 7

    // configurare frecventa PWM la 1 kHz 
    // Clk Pico 2 = 150 MHz; wrap = 1000 -> divider = 150e6 / (1000 * 1000) = 150
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 150.0f);
    pwm_config_set_wrap(&cfg, MOTOR_PWM_MAX - 1);
    pwm_init(g_pwm_slice, &cfg, true);

    pwm_set_both(0, 0);
}

void motors_enable(void) {
    pwm_set_both(MOTOR_PWM_MAX, MOTOR_PWM_MAX);  // 100%
}

void motors_disable(void) {
    pwm_set_both(0, 0);  // 0%
}

void motors_forward(void) {
    set_dir(0, 1, 1, 0);
    motors_enable();
}

void motors_backward(void) {
    set_dir(1, 0, 0, 1);
    motors_enable();
}

void motors_turn_left(void) {
    set_dir(1, 0, 1, 0);
    motors_enable();
}

void motors_turn_right(void) {
    set_dir(0, 1, 0, 1);
    motors_enable();
}

void motors_stop(void) {
    set_dir(0, 0, 0, 0);
    motors_disable();
}

// mod giroscop: control progresiv cu PWM diferential
// left_pct / right_pct: 0=stop 100=viteza maxima
void motors_set_pwm(uint8_t left_pct, uint8_t right_pct) {
    pwm_set_chan_level(g_pwm_slice, PWM_CHAN_A, pct_to_level(left_pct));
    pwm_set_chan_level(g_pwm_slice, PWM_CHAN_B, pct_to_level(right_pct));
}
