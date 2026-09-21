#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>
#include <stdio.h>

#ifndef MMSIM_ENABLED
#include "buttons.h"
#include "leds.h"
#include "sensors.h"
#endif

#define STM32_UID_BASE (0x1FFF7A10U)
#define UID_WORD0 MMIO32(STM32_UID_BASE + 0x0)
#define UID_WORD1 MMIO32(STM32_UID_BASE + 0x4)
#define UID_WORD2 MMIO32(STM32_UID_BASE + 0x8)

enum ROBOT_VERSION {
  ZOROBOT3_UNKNOWN = 0,
  ZOROBOT3_A = 1,
  ZOROBOT3_B = 2,
  ZOROBOT3_C = 3
};

/** Laberinto *//** Laberinto */

/*
 * 260 mm libres entre caras interiores
 * + 12 mm de espesor de pared
 * = 272 mm entre centros de paredes consecutivas.
 */
#define CELL_DIMENSION 260

/*
 * 272 / sqrt(2)
 */
#define CELL_DIAGONAL 183.84f

#define WALL_WIDTH 12

/*
 * Distancia desde el centro del pasillo
 * hasta la cara interior de una pared:
 *
 * (272 - 12) / 2 = 130 mm
 */
#define MIDDLE_MAZE_DISTANCE \
    ((CELL_DIMENSION - WALL_WIDTH) / 2.0f)

#define SENSING_POINT_DISTANCE 0

/*
 * Valor inicial estimado.
 * Después debe ajustarse mediante pruebas.
 */
#define WALL_LOSS_TO_SENSING_POINT_DISTANCE 162
/** Características Físicas */
#define MICROMETERS_PER_TICK 10.494055
#define ROBOT_FRONT_LENGTH 48.121
#define ROBOT_BACK_LENGTH 40.706
#define ROBOT_WIDTH 70.2

#define WHEELS_SEPARATION 62
#define ROBOT_MIDDLE_WIDTH ((ROBOT_WIDTH / 2.0))

/** Movimiento */
#define MAX_MOTOR_SATURATION_COUNT 30
#define MAX_MOTOR_ANGULAR_SATURATION_COUNT 60

/** Sensores */
#define SENSOR_LOW_PASS_FILTER_ALPHA 0.1

#define SENSOR_FRONT_CALIBRATION_READINGS 20
#define SENSOR_SIDE_CALIBRATION_READINGS 100
#define SENSOR_FRONT_DETECTION ((CELL_DIMENSION * 0.8))
#define SENSOR_SIDE_DETECTION ((CELL_DIMENSION * 0.7))
#define SENSOR_SIDE_CORRECTION_MAX 75.0

/** Control de inicio de competición */
#define SENSOR_FRONT_DETECTION_START 100
#define SENSOR_START_MIN_MS 150

/** Divisor de Voltage */
#define VOLT_DIV_FACTOR_3S 5.30
#define VOLT_DIV_FACTOR_2S 3.15
#define BATTERY_2S_HIGH_LIMIT_VOLTAGE 8.4
#define BATTERY_2S_LOW_LIMIT_VOLTAGE 7.4
#define BATTERY_3S_HIGH_LIMIT_VOLTAGE 12.6
#define BATTERY_3S_LOW_LIMIT_VOLTAGE 11.1

/** Modo RUN */
#define CONFIG_RUN_RACE 1
#define CONFIG_RUN_DEBUG 0

void handle_robot_version(void);
void set_all_configs(void);
uint16_t get_config_run(void);

#endif