#ifndef __EEPROM_H
#define __EEPROM_H

#include <libopencm3/stm32/flash.h>

#include "delay.h"
#include "leds.h"
#include "sensors.h"
#include "floodfill.h"
#include "menu_run.h"
#include "rc5.h"
#include "rc5_mappings.h"

#define EEPROM_SECTOR 11
#define EEPROM_BASE_ADDRESS (0x080E0000)

#define EEPROM_SECTOR_SIZE (128 * 1024)

#define DATA_INDEX_GYRO_Z 0
#define DATA_INDEX_SENSORS_OFFSETS (DATA_INDEX_GYRO_Z + MPU_DATA_LENGTH)
#define DATA_INDEX_MAZE (DATA_INDEX_SENSORS_OFFSETS + NUM_SENSORES)
#define DATA_INDEX_MENU_RUN (DATA_INDEX_MAZE + MAZE_CELLS)
#define DATA_INDEX_RC5 (DATA_INDEX_MENU_RUN + MENU_RUN_NUM_MODES)
#define DATA_INDEX_RC5_MAPPINGS (DATA_INDEX_RC5 + RC5_DATA_LENGTH)

/*
 * Largo total = fin del ultimo bloque. Antes se calculaba aparte
 * con un "2" fijo para el giroscopo (MPU_DATA_LENGTH es 3) y el
 * ultimo dato del mapeo IR quedaba fuera del arreglo.
 */
#define DATA_LENGTH (DATA_INDEX_RC5_MAPPINGS + RC5_MAPPINGS_DATA_LENGTH)

void eeprom_save(void);
void eeprom_load(void);
void eeprom_clear(void);
void eeprom_backup(void);
void eeprom_restore(void);
void eeprom_set_data(uint16_t index, int16_t *data, uint16_t length);
int16_t *eeprom_get_data(void);

#endif
