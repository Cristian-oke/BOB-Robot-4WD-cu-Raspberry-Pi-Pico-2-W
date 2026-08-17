#include "buzzer.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include <stdio.h>

// Jingle Bells frecvente in Hz
#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_F5  698
#define NOTE_REST 0

typedef struct {
    uint16_t freq;
    uint16_t duration_ms;
} Note;

static const Note jingle_bells[] = {
    {NOTE_E5, 250}, {NOTE_E5, 250}, {NOTE_E5, 500},
    {NOTE_E5, 250}, {NOTE_E5, 250}, {NOTE_E5, 500},
    {NOTE_E5, 250}, {NOTE_G5, 250}, {NOTE_C5, 375}, {NOTE_D5, 125},
    {NOTE_E5, 1000},
    
    {NOTE_F5, 250}, {NOTE_F5, 250}, {NOTE_F5, 375}, {NOTE_F5, 125},
    {NOTE_F5, 250}, {NOTE_E5, 250}, {NOTE_E5, 250}, {NOTE_E5, 125}, {NOTE_E5, 125},
    {NOTE_E5, 250}, {NOTE_D5, 250}, {NOTE_D5, 250}, {NOTE_E5, 250},
    {NOTE_D5, 500}, {NOTE_G5, 500},
    
    {NOTE_REST, 500} 
};

static const size_t SONG_LENGTH = sizeof(jingle_bells) / sizeof(Note);

static volatile size_t current_note_idx = 0;
static volatile bool is_playing = false;
static uint buzzer_slice_num;
static alarm_id_t current_alarm = 0;

static void set_buzzer_freq(uint16_t freq) {
    if (freq == 0) {
        pwm_set_chan_level(buzzer_slice_num, PWM_CHAN_B, 0); // Oprit
        return;
    }
    
    // wrap = (150,000,000 / 125) / freq = 1,200,000 / freq
    uint32_t sys_clk = 150000000; 
    float div = 125.0f;
    uint32_t wrap = (uint32_t)((sys_clk / div) / freq);
    
    pwm_set_wrap(buzzer_slice_num, wrap);
    pwm_set_chan_level(buzzer_slice_num, PWM_CHAN_B, wrap / 2); // 50% duty cycle pentru volum maxim curat
}

// Callback chemat automat de timerul hardware pe fundal
static int64_t buzzer_alarm_callback(alarm_id_t id, void *user_data) {
    if (!is_playing) return 0; 
    Note note = jingle_bells[current_note_idx];
    set_buzzer_freq(note.freq);
    
    // trecere la urmatoarea nota
    uint16_t duration = note.duration_ms;
    current_note_idx++;
    if (current_note_idx >= SONG_LENGTH) {
        current_note_idx = 0; // loop infinit
    }
    
    return duration * 1000;
}

void buzzer_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f); 
    pwm_init(buzzer_slice_num, &config, true);
    
    pwm_set_chan_level(buzzer_slice_num, PWM_CHAN_B, 0); // oprit initial
}

void buzzer_start_song(void) {
    if (is_playing) return;
    is_playing = true;
    current_note_idx = 0;
    
    current_alarm = add_alarm_in_ms(10, buzzer_alarm_callback, NULL, true);
}

void buzzer_stop(void) {
    is_playing = false;
    if (current_alarm) {
        cancel_alarm(current_alarm);
        current_alarm = 0;
    }
    pwm_set_chan_level(buzzer_slice_num, PWM_CHAN_B, 0);
}
