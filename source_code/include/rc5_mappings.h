#ifndef RC5_MAPPINGS_H
#define RC5_MAPPINGS_H

#include <stdbool.h>
#include <stdint.h>

#define RC5_MAPPINGS_NUM_BUTTONS 11
#define RC5_MAPPINGS_INTS_PER_BUTTON 2
#define RC5_MAPPINGS_DATA_LENGTH (RC5_MAPPINGS_NUM_BUTTONS * RC5_MAPPINGS_INTS_PER_BUTTON)

enum RC5_BUTTON {
  RC5_CH_MODE = 0,
  RC5_CH_DOWN = 1,
  RC5_CH_UP = 2,
  RC5_PLAY_PAUSE = 3,
  RC5_PREV = 4,
  RC5_NEXT = 5,
  RC5_NUM_1 = 6,
  RC5_NUM_2 = 7,
  RC5_NUM_3 = 8,
  RC5_NUM_4 = 9,
  RC5_NUM_5 = 10,
};

void rc5_mappings_load_eeprom(void);
void rc5_mappings_save_eeprom(void);
void rc5_mappings_set(enum RC5_BUTTON button, uint32_t code);
uint32_t rc5_mappings_get(enum RC5_BUTTON button);
bool rc5_mappings_match(uint32_t code, enum RC5_BUTTON *button);
void rc5_mappings_clear(void);
void rc5_mappings_learn(void);

#endif
