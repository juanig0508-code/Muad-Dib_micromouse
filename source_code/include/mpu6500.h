#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>

#include "buttons.h"
#include "delay.h"
#include "eeprom.h"
#include "motors.h"
#include "setup.h"

#include "constants.h"
#include "math.h"


/* ============================================================
 * DRIVER MPU6500 - REEMPLAZO DIRECTO DE lsm6dsr
 * ============================================================
 *
 * Este header expone EXACTAMENTE la misma interfaz que
 * lsm6dsr.h, cambiando el prefijo lsm6dsr_ por mpu6500_.
 *
 * La idea es que adaptar main_real.c sea un buscar-y-reemplazar,
 * sin tocar la logica de move.c, control.c ni eeprom.c.
 *
 *
 * EQUIVALENCIAS DE FONDO DE ESCALA
 *
 * El LSM6DSR soporta 1000 / 2000 / 4000 dps.
 * El MPU6500  soporta  250 /  500 / 1000 / 2000 dps.
 *
 * O sea: el MPU6500 NO llega a 4000 dps.
 *
 * Por suerte eso no es un problema real: revisando la tabla de
 * kinematics en move.c, los perfiles solo usan 1000 dps
 * (SPEED_EXPLORE) y 2000 dps (todos los demas). El valor de
 * 4000 dps no se usa en ningun perfil.
 *
 * Igual dejamos definido MPU_FULL_SCALE_4000DPS para no romper
 * la compilacion si algo lo referencia. Si se pide, el driver
 * lo satura a 2000 dps y avisa por serial.
 *
 *
 * IMPORTANTE - NO CAMBIAR MPU_FULL_SCALE_COUNT
 *
 * eeprom.h calcula la posicion de TODOS los demas datos a
 * partir de MPU_DATA_LENGTH:
 *
 *     #define DATA_INDEX_SENSORS_OFFSETS \
 *             (DATA_INDEX_GYRO_Z + MPU_DATA_LENGTH)
 *
 * Si bajaramos el conteo de 3 a 2 (porque el MPU6500 tiene un
 * fondo de escala menos), se correrian los offsets de sensores,
 * el laberinto guardado y la config del menu. Por eso se
 * mantiene en 3 y simplemente el tercer slot queda sin uso.
 */

#define MPU_FULL_SCALE_COUNT 3


/*
 * Valores del campo FS_SEL del registro GYRO_CONFIG (0x1B),
 * ya desplazados a su posicion final (bits 4:3).
 *
 *      FS_SEL = 0 ->  250 dps
 *      FS_SEL = 1 ->  500 dps
 *      FS_SEL = 2 -> 1000 dps
 *      FS_SEL = 3 -> 2000 dps
 *
 * MPU_FULL_SCALE_4000DPS es un valor centinela: no existe en el
 * hardware y el driver lo traduce a 2000 dps.
 */
enum MPU_FULL_SCALE_DPS {
  MPU_FULL_SCALE_1000DPS = (2 << 3),
  MPU_FULL_SCALE_2000DPS = (3 << 3),
  MPU_FULL_SCALE_4000DPS = 0xFF,
};


/*
 * Sensibilidad en MILI-grados por segundo por LSB.
 *
 * Se usan las mismas unidades que el driver original para que
 * la formula de conversion quede identica:
 *
 *     dps = raw * sensibilidad / 1000
 *
 * MPU6500 (16 bits):
 *
 *     1000 dps -> 32.8 LSB/dps -> 1000000/32800 = 30.4878 mdps/LSB
 *     2000 dps -> 16.4 LSB/dps -> 1000000/16400 = 60.9756 mdps/LSB
 *
 * A diferencia del LSM6DSR (que tenia valores enteros: 35, 70,
 * 140), aca son decimales, por eso van como float y no como
 * enum.
 */
#define MPU_SENSITIVITY_1000DPS 30.4878f
#define MPU_SENSITIVITY_2000DPS 60.9756f
#define MPU_SENSITIVITY_4000DPS 60.9756f


#define MPU_DATA_LENGTH MPU_FULL_SCALE_COUNT


/*
 * Signo del eje Z.
 *
 * Si al girar el robot el angulo integrado crece para el lado
 * contrario al esperado, cambiar 1 por -1.
 *
 * Esto puede pasar porque el MPU6500 puede estar montado con
 * otra orientacion que el LSM6DSR original.
 */
#define MPU6500_GYRO_Z_SIGN 1


void mpu6500_init(void);
void mpu6500_reload_config(void);

uint8_t mpu6500_who_am_i(void);

void mpu6500_gyro_z_calibration(void);
void mpu6500_load_eeprom(void);

void mpu6500_update(void);
float mpu6500_get_gyro_z_raw(void);
float mpu6500_get_gyro_z_radps(void);
float mpu6500_get_gyro_z_dps(void);
float mpu6500_get_gyro_z_degrees(void);
void mpu6500_set_gyro_z_degrees(float deg);

float get_offset_z(void);
uint8_t get_current_full_scale_dps(void);

#endif // MPU6500_H