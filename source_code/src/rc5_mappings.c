#include "rc5_mappings.h"

#include "eeprom.h"

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
