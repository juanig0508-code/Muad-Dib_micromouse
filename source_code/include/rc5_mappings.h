#ifndef RC5_MAPPINGS_H
#define RC5_MAPPINGS_H

#include <stdbool.h>
#include <stdint.h>

#define RC5_MAPPINGS_NUM_BUTTONS 4
#define RC5_MAPPINGS_INTS_PER_BUTTON 2
#define RC5_MAPPINGS_DATA_LENGTH (RC5_MAPPINGS_NUM_BUTTONS * RC5_MAPPINGS_INTS_PER_BUTTON)

enum RC5_BUTTON {
  RC5_CH_MODE = 0,
  RC5_CH_DOWN = 1,
  RC5_CH_UP = 2,
  RC5_PLAY_PAUSE = 3,
};

void rc5_mappings_load_eeprom(void);
void rc5_mappings_save_eeprom(void);
void rc5_mappings_set(enum RC5_BUTTON button, uint32_t code);
uint32_t rc5_mappings_get(enum RC5_BUTTON button);
bool rc5_mappings_match(uint32_t code, enum RC5_BUTTON *button);
void rc5_mappings_clear(void);

#endif
