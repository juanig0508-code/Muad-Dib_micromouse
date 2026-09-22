#include "rc5_mappings.h"

#include "delay.h"
#include "eeprom.h"
#include "leds.h"
#include "rc5.h"

#define RC5_LEARN_TIMEOUT_MS 10000

static const enum RC5_BUTTON rc5_learn_order[RC5_MAPPINGS_NUM_BUTTONS] = {
    RC5_CH_MODE, RC5_CH_DOWN, RC5_CH_UP, RC5_PLAY_PAUSE, RC5_PREV, RC5_NEXT};

static uint32_t rc5_button_codes[RC5_MAPPINGS_NUM_BUTTONS];

void rc5_mappings_load_eeprom(void) {
  int16_t *eeprom_data = eeprom_get_data();
  for (uint16_t button = 0; button < RC5_MAPPINGS_NUM_BUTTONS; button++) {
    uint16_t index = DATA_INDEX_RC5_MAPPINGS + button * RC5_MAPPINGS_INTS_PER_BUTTON;
    uint16_t low = (uint16_t)eeprom_data[index];
    uint16_t high = (uint16_t)eeprom_data[index + 1];
    rc5_button_codes[button] = ((uint32_t)high << 16) | low;
  }
}

void rc5_mappings_save_eeprom(void) {
  int16_t data[RC5_MAPPINGS_DATA_LENGTH];
  for (uint16_t button = 0; button < RC5_MAPPINGS_NUM_BUTTONS; button++) {
    data[button * RC5_MAPPINGS_INTS_PER_BUTTON] = (int16_t)(rc5_button_codes[button] & 0xFFFF);
    data[button * RC5_MAPPINGS_INTS_PER_BUTTON + 1] = (int16_t)((rc5_button_codes[button] >> 16) & 0xFFFF);
  }
  eeprom_set_data(DATA_INDEX_RC5_MAPPINGS, data, RC5_MAPPINGS_DATA_LENGTH);
  eeprom_save();
}

void rc5_mappings_set(enum RC5_BUTTON button, uint32_t code) {
  if (button >= RC5_MAPPINGS_NUM_BUTTONS) {
    return;
  }
  rc5_button_codes[button] = code;
}

uint32_t rc5_mappings_get(enum RC5_BUTTON button) {
  if (button >= RC5_MAPPINGS_NUM_BUTTONS) {
    return 0;
  }
  return rc5_button_codes[button];
}

bool rc5_mappings_match(uint32_t code, enum RC5_BUTTON *button) {
  if (code == 0) {
    return false;
  }
  for (uint16_t i = 0; i < RC5_MAPPINGS_NUM_BUTTONS; i++) {
    if (rc5_button_codes[i] != 0 && rc5_button_codes[i] == code) {
      *button = (enum RC5_BUTTON)i;
      return true;
    }
  }
  return false;
}

void rc5_mappings_clear(void) {
  for (uint16_t i = 0; i < RC5_MAPPINGS_NUM_BUTTONS; i++) {
    rc5_button_codes[i] = 0;
  }
}

static void blink_status_led(uint8_t times, uint32_t on_ms, uint32_t off_ms) {
  for (uint8_t i = 0; i < times; i++) {
    set_status_led(true);
    delay(on_ms);
    set_status_led(false);
    delay(off_ms);
  }
}

/**
 * @brief Modo de aprendizaje: pide, uno por uno, que se presione en el
 * control remoto el botón correspondiente a CH_MODE, CH_DOWN, CH_UP,
 * PLAY_PAUSE, PREV y NEXT, capturando su código y guardándolo en
 * EEPROM al final.
 *
 * Los LEDs de información (0-5) indican qué botón se está esperando.
 * El LED de estado da la confirmación: un parpadeo corto por cada
 * botón capturado, tres parpadeos largos al terminar con éxito, y
 * parpadeos rápidos si se cancela por no recibir ninguna señal dentro
 * de RC5_LEARN_TIMEOUT_MS (en ese caso no se guarda nada).
 */
void rc5_mappings_learn(void) {
  for (uint8_t i = 0; i < RC5_MAPPINGS_NUM_BUTTONS; i++) {
    clear_info_leds();
    set_info_led(i, true);

    uint32_t start_count = rc5_get_code_count();
    uint32_t start_ms = get_clock_ticks();

    while (rc5_get_code_count() == start_count) {
      if (get_clock_ticks() - start_ms > RC5_LEARN_TIMEOUT_MS) {
        clear_info_leds();
        blink_status_led(6, 80, 80);
        return;
      }
    }

    rc5_mappings_set(rc5_learn_order[i], rc5_get_last_code());

    blink_status_led(1, 150, 150);
    delay(300);
  }

  rc5_mappings_save_eeprom();

  clear_info_leds();
  blink_status_led(3, 200, 200);
}
