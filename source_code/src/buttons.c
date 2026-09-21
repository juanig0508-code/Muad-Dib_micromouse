#include "buttons.h"

#include "rc5.h"
#include "rc5_mappings.h"

#define RC5_BUTTON_DEBOUNCE_MS 50
#define RC5_BUTTON_TIMEOUT_MS 200

static uint32_t btn_analog = 0;

static uint32_t btn_menu_up_ms = 0;
static uint32_t btn_menu_down_ms = 0;
static uint32_t btn_menu_mode_ms = 0;

static uint32_t btn_ir_menu_up_ms = 0;
static uint32_t btn_ir_menu_down_ms = 0;
static uint32_t btn_ir_menu_mode_ms = 0;
static uint32_t btn_play_pause_ms = 0;

static uint32_t rc5_last_code_count = 0;
static uint32_t btn_ir_last_seen_ms[RC5_MAPPINGS_NUM_BUTTONS] = {0};

static bool debug_btn = false;

static uint32_t *get_ir_btn_ms(enum RC5_BUTTON button) {
  switch (button) {
    case RC5_CH_MODE:
      return &btn_ir_menu_mode_ms;
    case RC5_CH_DOWN:
      return &btn_ir_menu_down_ms;
    case RC5_CH_UP:
      return &btn_ir_menu_up_ms;
    case RC5_PLAY_PAUSE:
      return &btn_play_pause_ms;
  }
  return NULL;
}

static void check_analog_buttons(void) {
  btn_analog = get_aux_raw(AUX_MENU_BTN_ID);

  if (btn_analog >= 1300 && btn_analog <= 1700) {
    if (btn_menu_up_ms == 0) {
      btn_menu_up_ms = get_clock_ticks();
    }
  } else {
    btn_menu_up_ms = 0;
  }
  if (btn_analog >= 2300 && btn_analog <= 2700) {
    if (btn_menu_mode_ms == 0) {
      btn_menu_mode_ms = get_clock_ticks();
    }
  } else {
    btn_menu_mode_ms = 0;
  }
  if (btn_analog >= 3700) {
    if (btn_menu_down_ms == 0) {
      btn_menu_down_ms = get_clock_ticks();
    }
  } else {
    btn_menu_down_ms = 0;
  }
}

static void check_ir_buttons(void) {
  rc5_poll();

  uint32_t now = get_clock_ticks();
  uint32_t count = rc5_get_code_count();

  if (count != rc5_last_code_count) {
    rc5_last_code_count = count;
    enum RC5_BUTTON button;
    if (rc5_mappings_match(rc5_get_last_code(), &button)) {
      btn_ir_last_seen_ms[button] = now;
      uint32_t *ms = get_ir_btn_ms(button);
      if (ms != NULL && *ms == 0) {
        *ms = now;
      }
    }
  }

  for (uint16_t i = 0; i < RC5_MAPPINGS_NUM_BUTTONS; i++) {
    uint32_t *ms = get_ir_btn_ms((enum RC5_BUTTON)i);
    if (ms != NULL && *ms > 0 && now - btn_ir_last_seen_ms[i] > RC5_BUTTON_TIMEOUT_MS) {
      *ms = 0;
    }
  }
}

void check_buttons(void) {
  check_analog_buttons();
  check_ir_buttons();
}

/**
 * @brief Obtiene el estado del botón de Menú Arriba
 *
 * @return bool
 */
bool get_menu_up_btn(void) {
  bool analog = btn_menu_up_ms > 0 && get_clock_ticks() - btn_menu_up_ms > RC5_BUTTON_DEBOUNCE_MS;
  bool ir = btn_ir_menu_up_ms > 0 && get_clock_ticks() - btn_ir_menu_up_ms > RC5_BUTTON_DEBOUNCE_MS;
  return analog || ir;
}

/**
 * @brief Obtiene el estado del botón de Menú Abajo
 *
 * @return bool
 */
bool get_menu_down_btn(void) {
  bool analog = btn_menu_down_ms > 0 && get_clock_ticks() - btn_menu_down_ms > RC5_BUTTON_DEBOUNCE_MS;
  bool ir = btn_ir_menu_down_ms > 0 && get_clock_ticks() - btn_ir_menu_down_ms > RC5_BUTTON_DEBOUNCE_MS;
  return analog || ir;
}

/**
 * @brief Obtiene el estado del botón de Menú Modo
 *
 * @return bool
 */
bool get_menu_mode_btn(void) {
  bool analog = btn_menu_mode_ms > 0 && get_clock_ticks() - btn_menu_mode_ms > RC5_BUTTON_DEBOUNCE_MS;
  bool ir = btn_ir_menu_mode_ms > 0 && get_clock_ticks() - btn_ir_menu_mode_ms > RC5_BUTTON_DEBOUNCE_MS;
  return analog || ir;
}

/**
 * @brief Obtiene el estado del botón Play/Pause (solo IR, sin equivalente analógico)
 *
 * @return bool
 */
bool get_play_pause_btn(void) {
  return btn_play_pause_ms > 0 && get_clock_ticks() - btn_play_pause_ms > RC5_BUTTON_DEBOUNCE_MS;
}

void set_debug_btn(bool state){
  debug_btn = state;
}

bool get_debug_btn(void){
  return debug_btn;
}
