#include "mpu6500.h"


/* ============================================================
 * REGISTROS DEL MPU6500
 * ============================================================
 */

#define MPU_READ_FLAG 0x80

#define MPU_WHOAMI            0x75
#define MPU_WHOAMI_EXPECTED   0x70

#define REG_SMPLRT_DIV        0x19
#define REG_CONFIG            0x1A
#define REG_GYRO_CONFIG       0x1B
#define REG_ACCEL_CONFIG      0x1C
#define REG_ACCEL_CONFIG_2    0x1D

#define REG_GYRO_ZOUT_H       0x47
#define REG_GYRO_ZOUT_L       0x48

#define REG_SIGNAL_PATH_RESET 0x68
#define REG_USER_CTRL         0x6A
#define REG_PWR_MGMT_1        0x6B

#define MPU_DPS_TO_RADPS (PI / 180)


/* ============================================================
 * ESTADO INTERNO
 * ============================================================
 *
 * Misma estructura que lsm6dsr.c, para que el comportamiento
 * sea el mismo.
 */

static uint8_t full_scale_dps[] = {
    MPU_FULL_SCALE_1000DPS,
    MPU_FULL_SCALE_2000DPS,
    MPU_FULL_SCALE_4000DPS,
};

static float sensitivity_dps[] = {
    MPU_SENSITIVITY_1000DPS,
    MPU_SENSITIVITY_2000DPS,
    MPU_SENSITIVITY_4000DPS,
};

static uint8_t current_full_scale_dps;

static volatile float deg_integ;
static volatile float gyro_z_raw;
static bool mpu_updating = false;
static float offset_z[MPU_FULL_SCALE_COUNT];


/* ============================================================
 * ACCESO POR SPI
 * ============================================================
 *
 * Identico al esquema de lsm6dsr.c: CS por software en PA15,
 * SPI3, MSB first, modo 0.
 *
 * Diferencia con el LSM6DSR: en el MPU6500 el bit 7 del primer
 * byte indica lectura (1) o escritura (0). En escritura hay que
 * asegurarse de que ese bit vaya en 0.
 */

static uint8_t mpu6500_read_register(uint8_t address) {
  uint8_t reading;

  /*
   * IMPORTANTE: el MPU6500 solo admite hasta 1 MHz para leer
   * REGISTROS DE CONFIGURACION (WHO_AM_I, GYRO_CONFIG, etc).
   * Los 20 MHz del datasheet valen unicamente para los
   * registros de DATOS del sensor.
   *
   * Por eso bajamos la velocidad, leemos, y la restauramos.
   * Esta funcion se usa solo en configuracion y diagnostico,
   * asi que el costo no importa.
   *
   * La lectura del giro (mpu6500_read_gyro_z_raw) NO pasa por
   * aca justamente para poder ir a alta velocidad.
   */
  setup_spi_low_speed();

  gpio_clear(GPIOA, GPIO15);
  spi_send(SPI3, (MPU_READ_FLAG | address));
  spi_read(SPI3);
  spi_send(SPI3, 0x00);
  reading = spi_read(SPI3);
  gpio_set(GPIOA, GPIO15);

  setup_spi_high_speed();

  return reading;
}

static void mpu6500_write_register(uint8_t address, uint8_t value) {
  gpio_clear(GPIOA, GPIO15);
  spi_send(SPI3, (address & 0x7F));
  spi_read(SPI3);
  spi_send(SPI3, value);
  spi_read(SPI3);
  gpio_set(GPIOA, GPIO15);
}


/**
 * @brief Lee el giro en Z crudo (16 bits con signo).
 *
 * Se leen los dos bytes en una sola transaccion aprovechando el
 * auto-incremento de direccion del MPU6500, para que ambos
 * correspondan a la misma muestra.
 */
static int16_t mpu6500_read_gyro_z_raw(void) {
  uint8_t zh;
  uint8_t zl;

  gpio_clear(GPIOA, GPIO15);
  spi_send(SPI3, (MPU_READ_FLAG | REG_GYRO_ZOUT_H));
  spi_read(SPI3);
  spi_send(SPI3, 0x00);
  zh = spi_read(SPI3);
  spi_send(SPI3, 0x00);
  zl = spi_read(SPI3);
  gpio_set(GPIOA, GPIO15);

  return (int16_t)(((uint16_t)zh << 8) | zl);
}


/* ============================================================
 * FONDO DE ESCALA
 * ============================================================
 */

/**
 * @brief Traduce el fondo de escala pedido al valor real que
 * soporta el MPU6500.
 *
 * El MPU6500 no llega a 4000 dps, asi que ese caso se satura a
 * 2000 dps. No deberia ocurrir nunca: ningun perfil de
 * kinematics en move.c lo usa.
 */
static uint8_t resolve_full_scale(uint8_t requested) {
  if (requested == MPU_FULL_SCALE_4000DPS) {
    printf("[MPU6500] 4000dps no soportado, usando 2000dps\n");
    return MPU_FULL_SCALE_2000DPS;
  }
  return requested;
}

static void mpu6500_set_full_scale(uint8_t scale) {
  mpu6500_write_register(REG_GYRO_CONFIG, resolve_full_scale(scale));
}

static float get_sensitivity_dps(void) {
  switch (current_full_scale_dps) {
    case MPU_FULL_SCALE_1000DPS:
      return sensitivity_dps[0];
    case MPU_FULL_SCALE_2000DPS:
      return sensitivity_dps[1];
    case MPU_FULL_SCALE_4000DPS:
      return sensitivity_dps[2];
  }
  return 0;
}


/* ============================================================
 * INICIALIZACION
 * ============================================================
 */

void mpu6500_init(void) {

  /*
   * La configuracion de registros necesita SPI por debajo de
   * 1 MHz. La lectura de datos puede ir hasta 20 MHz.
   */
  setup_spi_low_speed();

  /* Reset completo del dispositivo. */
  mpu6500_write_register(REG_PWR_MGMT_1, 0x80);
  delay(100);

  /*
   * Salir de sleep y usar el PLL con referencia del giroscopo
   * como fuente de reloj (mas estable que el oscilador interno).
   */
  mpu6500_write_register(REG_PWR_MGMT_1, 0x01);
  delay(10);

  /* Reset del camino de senal de giro, acel y temperatura. */
  mpu6500_write_register(REG_SIGNAL_PATH_RESET, 0x07);
  delay(10);

  /*
   * Deshabilitar la interfaz I2C: el MPU6500 arranca en modo
   * I2C y hay que forzarlo a SPI.
   */
  mpu6500_write_register(REG_USER_CTRL, 0x10);
  delay(10);

  /* Sin division: tasa de muestreo = 1 kHz. */
  mpu6500_write_register(REG_SMPLRT_DIV, 0x00);

  /*
   * DLPF_CFG = 3 -> filtro de 41 Hz sobre el giroscopo.
   *
   * Es el equivalente mas cercano al filtro LP1 en modo LIGHT
   * que usaba el driver del LSM6DSR. El grueso del filtrado
   * igual lo hace el pasa-bajos por software de mpu6500_update(),
   * con el alpha que viene de kinematics.
   */
  mpu6500_write_register(REG_CONFIG, 0x03);

  /* Acelerometro: +/-2g y filtro de 41 Hz. */
  mpu6500_write_register(REG_ACCEL_CONFIG, 0x00);
  mpu6500_write_register(REG_ACCEL_CONFIG_2, 0x03);

  /* Fondo de escala del giro segun el perfil de kinematics. */
  current_full_scale_dps = resolve_full_scale(get_kinematics().mpu.full_scale_dps);
  mpu6500_set_full_scale(current_full_scale_dps);

  setup_spi_high_speed();

  delay(1000);
}


/**
 * @brief Recarga el fondo de escala si cambio el perfil de
 * kinematics.
 *
 * Misma logica que lsm6dsr_reload_config().
 */
void mpu6500_reload_config(void) {
  uint8_t requested = resolve_full_scale(get_kinematics().mpu.full_scale_dps);

  if (current_full_scale_dps != requested) {
    setup_spi_low_speed();
    mpu6500_set_full_scale(requested);
    setup_spi_high_speed();

    current_full_scale_dps = requested;
    delay(50);
    reset_control_all();
  }
}


uint8_t mpu6500_who_am_i(void) {
  return mpu6500_read_register(MPU_WHOAMI);
}


/* ============================================================
 * CALIBRACION
 * ============================================================
 *
 * Copia fiel de lsm6dsr_gyro_z_calibration():
 *
 *  - Recorre los fondos de escala y calcula el offset en cada
 *    uno (el offset crudo cambia con la escala).
 *  - Promedia 200 muestras con el robot quieto.
 *  - Guarda en EEPROM multiplicado por 1000 para conservar
 *    decimales en un int16.
 *
 * Se mantienen los 3 slots aunque el tercero (4000 dps) sea el
 * mismo que el segundo, para no correr el resto de los datos de
 * la EEPROM.
 */
void mpu6500_gyro_z_calibration(void) {
  mpu_updating = false;
  int16_t eeprom_data[MPU_FULL_SCALE_COUNT];

  for (uint8_t i = 0; i < MPU_FULL_SCALE_COUNT; i++) {

    setup_spi_low_speed();
    mpu6500_set_full_scale(full_scale_dps[i]);
    setup_spi_high_speed();
    delay(50);

    int32_t sum_z = 0;
    offset_z[i] = 0;

    for (int s = 0; s < 200; s++) {
      sum_z += mpu6500_read_gyro_z_raw();
      delay(5);
      set_info_leds();
    }
    clear_info_leds();

    offset_z[i] = sum_z / 200.0f;
    printf("Offset Z for full-scale %d dps: %.4f\n", full_scale_dps[i], offset_z[i]);
    eeprom_data[i] = offset_z[i] * 1000;
  }

  eeprom_set_data(DATA_INDEX_GYRO_Z, eeprom_data, MPU_DATA_LENGTH);

  setup_spi_low_speed();
  mpu6500_set_full_scale(current_full_scale_dps);
  setup_spi_high_speed();

  delay(100);
  mpu_updating = true;
}


void mpu6500_load_eeprom(void) {
  int16_t *eeprom_data = eeprom_get_data();

  for (uint8_t i = 0; i < MPU_FULL_SCALE_COUNT; i++) {
    offset_z[i] = eeprom_data[DATA_INDEX_GYRO_Z + i] / 1000.0f;
    printf("Offset Z for full-scale %d dps: %.4f\n", full_scale_dps[i], offset_z[i]);
  }

  mpu_updating = true;
}


/* ============================================================
 * ACTUALIZACION PERIODICA
 * ============================================================
 *
 * Se llama desde sys_tick_handler(), o sea a
 * SYSTICK_FREQUENCY_HZ (1 kHz).
 *
 * Identico a lsm6dsr_update():
 *
 *  1. Lee el crudo y le resta el offset calibrado.
 *  2. Aplica el pasa-bajos con el alpha del perfil actual.
 *  3. Integra el angulo dividiendo por SYSTICK_FREQUENCY_HZ.
 *
 * El signo negativo en la integracion es el del codigo
 * original. Si el angulo crece para el lado equivocado, no
 * tocar esta linea: cambiar MPU6500_GYRO_Z_SIGN en el header.
 */
void mpu6500_update(void) {
  if (mpu_updating) {
    float new_gyro_z_raw = MPU6500_GYRO_Z_SIGN * mpu6500_read_gyro_z_raw();
    new_gyro_z_raw -= get_offset_z();

    gyro_z_raw =
        get_kinematics().mpu.low_pass_filter_alpha * gyro_z_raw +
        (1 - get_kinematics().mpu.low_pass_filter_alpha) * new_gyro_z_raw;

    deg_integ = deg_integ - mpu6500_get_gyro_z_dps() / SYSTICK_FREQUENCY_HZ;
  }
}


/* ============================================================
 * LECTURAS
 * ============================================================
 */

float mpu6500_get_gyro_z_raw(void) {
  return gyro_z_raw;
}

float mpu6500_get_gyro_z_radps(void) {
  return (gyro_z_raw * get_sensitivity_dps() / 1000 * MPU_DPS_TO_RADPS);
}

float mpu6500_get_gyro_z_dps(void) {
  return (gyro_z_raw * get_sensitivity_dps()) / 1000;
}

float mpu6500_get_gyro_z_degrees(void) {
  return deg_integ;
}

void mpu6500_set_gyro_z_degrees(float deg) {
  deg_integ = deg;
}


float get_offset_z(void) {
  switch (current_full_scale_dps) {
    case MPU_FULL_SCALE_1000DPS:
      return offset_z[0];
    case MPU_FULL_SCALE_2000DPS:
      return offset_z[1];
    case MPU_FULL_SCALE_4000DPS:
      return offset_z[2];
  }
  return 0;
}

uint8_t get_current_full_scale_dps(void) {
  return current_full_scale_dps;
}