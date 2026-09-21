#include "control.h"

static volatile bool race_started = false;
static volatile bool race_auto_run = false;
static volatile uint32_t race_finish_ms = 0;
static volatile uint32_t sensor_front_left_start_ms = 0;
static volatile uint32_t sensor_front_right_start_ms = 0;

static volatile bool control_debug = false;

static volatile int32_t target_linear_speed = 0;
static volatile int32_t ideal_linear_speed = 0;
static volatile float ideal_angular_speed = 0.0;

static volatile int32_t target_fan_speed = 0;
static volatile float ideal_fan_speed = 0;
static volatile float fan_speed_accel = 0;

static volatile int16_t ideal_front_distance = 0;

static volatile bool linear_error_correction_enabled = true;

static volatile float linear_error;
static volatile float sum_linear_error;
static volatile float last_linear_error;

static volatile bool angular_error_correction_enabled = true;

static volatile float angular_error;
static volatile float sum_angular_error;
static volatile float last_angular_error;

static volatile bool side_sensors_correction_enabled = false;

/*
 * ============================================================
 * SEGUIDOR DE UNA SOLA PARED
 * ============================================================
 *
 * Estos valores son deliberadamente faciles de ajustar.
 *
 * Los targets salen de las mediciones reales tomadas con el
 * robot aproximadamente centrado:
 *
 *   sensor izquierdo ~141 mm
 *   sensor derecho   ~137 mm
 *
 * El sensor lateral NO aplica voltaje directamente a motores.
 * Genera una velocidad angular deseada pequeña y limitada.
 * El PID angular + MPU se encarga de ejecutarla.
 */
#define WALL_FOLLOW_LEFT_TARGET_MM   130.0f
#define WALL_FOLLOW_RIGHT_TARGET_MM  130.0f

#define WALL_FOLLOW_KP_ANGULAR 0.04f
#define WALL_FOLLOW_MAX_RADPS  0.20f


#define WALL_FOLLOW_VALID_MAX_MM     190.0f

static volatile enum wall_follow_side wall_follow_side = WALL_FOLLOW_NONE;

/*
 * ============================================================
 * HEADING LOCK CON MPU
 * ============================================================
 *
 * measured_heading_rad:
 *   angulo integrado realmente recorrido por el robot.
 *
 * target_heading_rad:
 *   orientacion cardinal que deberia tener el robot.
 *
 * Cada MOVE_LEFT/MOVE_RIGHT modifica target_heading_rad en 90°.
 * Durante una recta, si el giro termino algunos grados corto o
 * pasado, el MPU genera una correccion suave para recuperar el
 * heading objetivo.
 */
#define HEADING_KP              2.0f
#define HEADING_MAX_RADPS       0.25f

static volatile float measured_heading_rad = 0.0f;
static volatile float target_heading_rad = 0.0f;
static volatile bool front_sensors_angle_correction_enabled = false;
static volatile bool front_sensors_distance_correction_enabled = false;
static volatile bool front_sensors_diagonal_correction_enabled = false;

static volatile float side_sensors_error;
static volatile float last_side_sensors_error;
static volatile float sum_side_sensors_error;

static volatile float front_sensors_angle_error;
static volatile float last_front_sensors_angle_error;
static volatile float sum_front_sensors_angle_error;

static volatile float front_sensors_distance_error;
static volatile float last_front_sensors_distance_error;
static volatile float sum_front_sensors_distance_error;

static volatile float front_sensors_diagonal_error;
static volatile float last_front_sensors_diagonal_error;
static volatile float sum_front_sensors_diagonal_error;

static volatile float voltage_left;
static volatile float voltage_right;
static volatile int32_t pwm_left;
static volatile int32_t pwm_right;

/**
 * @brief Convierte un valor de voltaje dado a su correspondiente PWM
 *
 * @param voltage
 * @return int32_t PWM a aplicar al motor
 */
#ifndef MMSIM_ENABLED
static int32_t voltage_to_motor_pwm(float voltage) {
  return voltage / /* 8.0  */ get_battery_voltage() * MOTORES_MAX_PWM;
}
#endif

#ifndef MMSIM_ENABLED
static int32_t normalize_fan_percentage(float percentage) {
  return percentage > 0 ? (int32_t)constrain(get_battery_high_limit_voltage() * percentage / get_battery_voltage(), percentage, 100.0f) : 0;
}
#endif

/**
 * @brief Actualiza la velocidad lineal ideal en función de la velocidad lineal objetivo y la aceleración
 *
 */
#ifndef MMSIM_ENABLED
static void update_ideal_linear_speed(void) {
  if (ideal_linear_speed < target_linear_speed) {
    int16_t accel = get_kinematics().linear_accel.accel_soft;
    if (get_kinematics().linear_accel.speed_hard == 0 || ideal_linear_speed < get_kinematics().linear_accel.speed_hard) {
      accel = get_kinematics().linear_accel.accel_hard;
    }
    ideal_linear_speed += accel / CONTROL_FREQUENCY_HZ;
    if (ideal_linear_speed > target_linear_speed) {
      ideal_linear_speed = target_linear_speed;
    }
  } else if (ideal_linear_speed > target_linear_speed) {
    ideal_linear_speed -= get_kinematics().linear_accel.break_accel / CONTROL_FREQUENCY_HZ;
    if (ideal_linear_speed < target_linear_speed) {
      ideal_linear_speed = target_linear_speed;
    }
  }
}

static void update_fan_speed(void) {
  if (ideal_fan_speed < target_fan_speed) {
    ideal_fan_speed += fan_speed_accel / CONTROL_FREQUENCY_HZ;
    if (ideal_fan_speed > target_fan_speed) {
      ideal_fan_speed = target_fan_speed;
    }
  } else if (ideal_fan_speed > target_fan_speed) {
    ideal_fan_speed += fan_speed_accel / CONTROL_FREQUENCY_HZ;
    if (ideal_fan_speed < target_fan_speed) {
      ideal_fan_speed = target_fan_speed;
    }
  }
}



static float get_measured_linear_speed(void)
{
    return (
        get_encoder_left_speed() +
        get_encoder_right_speed()
    ) / 2.0f;
}
static float get_measured_angular_speed(void)
{
    return -mpu6500_get_gyro_z_radps();
}

/*
 * Lleva un angulo al intervalo [-PI, PI].
 */
static float wrap_heading_angle(float angle)
{
    while (angle > PI) {
        angle -= 2.0f * PI;
    }

    while (angle < -PI) {
        angle += 2.0f * PI;
    }

    return angle;
}

/*
 * Integra la velocidad angular medida por el MPU.
 * control_loop() corre a CONTROL_FREQUENCY_HZ.
 */
static void update_measured_heading(void)
{
    measured_heading_rad +=
        get_measured_angular_speed() /
        (float)CONTROL_FREQUENCY_HZ;

    measured_heading_rad =
        wrap_heading_angle(measured_heading_rad);
}

/*
 * Correccion de heading durante rectas.
 *
 * Ejemplo:
 *   target = 90°
 *   measured = 87°
 *   error = +3°
 *
 * Entonces solicita una pequeña velocidad angular positiva
 * hasta volver a apuntar aproximadamente a 90°.
 */
static float get_heading_angular_correction(void)
{
    float error =
        wrap_heading_angle(
            target_heading_rad -
            measured_heading_rad
        );

    float correction = HEADING_KP * error;

    return constrain(
        correction,
        -HEADING_MAX_RADPS,
        HEADING_MAX_RADPS
    );
}

/*
 * Devuelve una velocidad angular pequeña para volver a la
 * distancia objetivo de la pared seleccionada.
 *
 * Convencion del firmware:
 *   angular_speed > 0  -> giro a derecha
 *   angular_speed < 0  -> giro a izquierda
 */
static float get_wall_follow_angular_speed(void)
{
    float distance;
    float error;
    float angular_speed;

    if (wall_follow_side == WALL_FOLLOW_RIGHT) {

        distance = get_sensor_distance(
            SENSOR_SIDE_RIGHT_WALL_ID
        );

        /*
         * Si la pared elegida esta demasiado lejos/no es
         * confiable, no corregimos lateralmente. En ese caso
         * el MPU mantiene ideal_angular_speed = 0.
         */
        if (distance >= WALL_FOLLOW_VALID_MAX_MM) {
            return 0.0f;
        }

        /*
         * Positivo = demasiado lejos de la pared derecha.
         * Hay que girar suavemente a derecha.
         */
        error =
            distance -
            WALL_FOLLOW_RIGHT_TARGET_MM;

        angular_speed =
            WALL_FOLLOW_KP_ANGULAR * error;
    }

    else if (wall_follow_side == WALL_FOLLOW_LEFT) {

        distance = get_sensor_distance(
            SENSOR_SIDE_LEFT_WALL_ID
        );

        if (distance >= WALL_FOLLOW_VALID_MAX_MM) {
            return 0.0f;
        }

        /*
         * Positivo = demasiado lejos de la pared izquierda.
         * Hay que girar suavemente a izquierda.
         */
        error =
            distance -
            WALL_FOLLOW_LEFT_TARGET_MM;

        angular_speed =
            -WALL_FOLLOW_KP_ANGULAR * error;
    }

    else {
        return 0.0f;
    }

    /*
     * Limite duro para impedir volantazos aunque una lectura
     * sea mala o el robot quede muy desplazado.
     */
    return constrain(
        angular_speed,
        -WALL_FOLLOW_MAX_RADPS,
        WALL_FOLLOW_MAX_RADPS
    );
}
#endif

/**
 * @brief Comprueba si el robot está en funcionamiento
 *
 * @return bool
 */

bool is_race_started(void) {
  return race_started;
}

/**
 * @brief Establece el estado actual del robot
 *
 * @param state Estado actual del robot
 */

void set_race_started(bool state) {
  race_started = state;

#ifndef MMSIM_ENABLED
  reset_control_all();
  if (!state) {
    menu_reset();
    race_finish_ms = get_clock_ticks();
  }
#endif
}

bool is_race_auto_run(void) {
  return race_auto_run;
}

void set_race_auto_run(bool state) {
  race_auto_run = state;
}

void set_control_debug(bool state) {
  control_debug = state;
}

#ifndef MMSIM_ENABLED
int8_t check_start_run(void) {
  if (get_sensor_distance(SENSOR_FRONT_LEFT_WALL_ID) <= SENSOR_FRONT_DETECTION_START) {
    if (sensor_front_left_start_ms == 0 && sensor_front_right_start_ms == 0) {
      sensor_front_left_start_ms = get_clock_ticks();
    }
  } else {
    sensor_front_left_start_ms = 0;
  }
  if (get_sensor_distance(SENSOR_FRONT_RIGHT_WALL_ID) <= SENSOR_FRONT_DETECTION_START) {
    if (sensor_front_left_start_ms == 0 && sensor_front_right_start_ms == 0) {
      sensor_front_right_start_ms = get_clock_ticks();
    }
  } else {
    sensor_front_right_start_ms = 0;
  }

  if (sensor_front_left_start_ms >= SENSOR_START_MIN_MS || sensor_front_right_start_ms >= SENSOR_START_MIN_MS) {
    uint8_t sensor = sensor_front_left_start_ms >= SENSOR_START_MIN_MS ? SENSOR_FRONT_LEFT_WALL_ID : SENSOR_FRONT_RIGHT_WALL_ID;
    sensor_front_left_start_ms = 0;
    sensor_front_right_start_ms = 0;
    set_RGB_color(0, 50, 0);

    set_race_auto_run(false);
    uint32_t starting_ms = get_clock_ticks();
    while (get_clock_ticks() - starting_ms < 2000) {
      warning_status_led(50);

      if (sensor == SENSOR_FRONT_RIGHT_WALL_ID) {
        if (get_sensor_distance(SENSOR_FRONT_LEFT_WALL_ID) <= SENSOR_FRONT_DETECTION_START) {
          if (sensor_front_left_start_ms == 0 && sensor_front_right_start_ms == 0) {
            sensor_front_left_start_ms = get_clock_ticks();
          }
        } else {
          sensor_front_left_start_ms = 0;
        }

        if (sensor_front_left_start_ms >= SENSOR_START_MIN_MS) {
          set_RGB_color(50, 0, 50);
          set_race_auto_run(true);
        }
      }
    }
    set_RGB_color(0, 0, 0);
    set_status_led(false);

    set_race_started(true);
    menu_run_reset();
    return sensor;
  }
  return -1;
}
#endif

void set_linear_error_correction(bool enabled) {
  linear_error_correction_enabled = enabled;
}

void set_angular_error_correction(bool enabled) {
  angular_error_correction_enabled = enabled;
}

void set_side_sensors_correction(bool enabled) {
  side_sensors_correction_enabled = enabled;
}

void set_wall_follow_side(enum wall_follow_side side) {
  wall_follow_side = side;
}

/*
 * Pone la orientacion actual como 0°.
 * Se llama al comenzar Handwall.
 */
void reset_heading_reference(void) {
  measured_heading_rad = 0.0f;
  target_heading_rad = 0.0f;
}

/*
 * Modifica el heading objetivo cuando la navegacion ordena un
 * giro.
 *
 * Derecha: +PI/2
 * Izquierda: -PI/2
 * 180°: +/-PI
 */
void add_target_heading(float radians) {
  target_heading_rad =
      wrap_heading_angle(
          target_heading_rad + radians
      );
}

float get_measured_heading(void) {
  return measured_heading_rad;
}

float get_target_heading(void) {
  return target_heading_rad;
}

void set_front_sensors_angle_correction(bool enabled) {
  front_sensors_angle_correction_enabled = enabled;
}

bool is_front_sensors_angle_correction_enabled(void) {
  return front_sensors_angle_correction_enabled;
}

void set_front_sensors_distance_correction(bool enabled) {
  front_sensors_distance_correction_enabled = enabled;
  if (!enabled) {
    ideal_front_distance = 0;
  }
}

void set_front_sensors_diagonal_correction(bool enabled) {
  front_sensors_diagonal_correction_enabled = enabled;
}

void disable_sensors_correction(void) {
  set_front_sensors_angle_correction(false);
  set_front_sensors_diagonal_correction(false);
  set_side_sensors_correction(false);
}

void reset_control_errors(void) {
  sum_side_sensors_error = 0;
  last_side_sensors_error = 0;
  last_front_sensors_angle_error = 0;
  sum_front_sensors_angle_error = 0;
  sum_front_sensors_diagonal_error = 0;
  linear_error = 0;
  angular_error = 0;
  sum_angular_error = 0;
  sum_linear_error = 0;
  last_linear_error = 0;
  last_angular_error = 0;
}

void reset_control_speed(void) {
  target_linear_speed = 0;
  ideal_linear_speed = 0;
  ideal_angular_speed = 0.0;
  voltage_left = 0;
  voltage_right = 0;
  pwm_left = 0;
  pwm_right = 0;
}

void reset_control_fan_speed(void) {
  target_fan_speed = 0;
  ideal_fan_speed = 0;
  fan_speed_accel = 0;
}

void reset_control_all(void) {
  reset_control_errors();
  reset_control_speed();
  reset_control_fan_speed();
#ifndef MMSIM_ENABLED
  reset_motors_saturated();
  reset_encoder_avg();
#endif
}

void set_target_linear_speed(int32_t linear_speed) {
  target_linear_speed = linear_speed;
}

void force_linear_speed(int32_t linear_speed) {
  target_linear_speed = linear_speed;
  ideal_linear_speed = linear_speed;
}

int32_t get_ideal_linear_speed(void) {
  return ideal_linear_speed;
}

void set_ideal_angular_speed(float angular_speed) {
  ideal_angular_speed = angular_speed;
}

float get_ideal_angular_speed(void) {
  return ideal_angular_speed;
}

#ifndef MMSIM_ENABLED
void set_target_fan_speed(int32_t fan_speed, int32_t ms) {
  if (ms > 0) {
    target_fan_speed = normalize_fan_percentage(fan_speed);
    fan_speed_accel = (target_fan_speed - ideal_fan_speed) * CONTROL_FREQUENCY_HZ / ms;
  } else {
    target_fan_speed = normalize_fan_percentage(fan_speed);
    ideal_fan_speed = target_fan_speed;
    fan_speed_accel = 0;
  }
}
#endif

void set_ideal_front_distance(int16_t distance) {
  ideal_front_distance = distance;
}

/**
 * @brief Función de control general del robot
 * · Gestiona velocidades, aceleraciones, correcciones, ...
 *
 */

#ifndef MMSIM_ENABLED
void control_loop(void) {
  // gpio_set(GPIOB, GPIO13);
  // delay_us(100);
  // gpio_clear(GPIOB, GPIO13);
  // return;
  if (is_debug_enabled() && !is_debug_use_control()) {
    return;
  }
  /*
  if ((is_motor_pwm_saturated() || is_motor_angle_saturated()) && is_race_started()) {
    set_motors_speed(0, 0);
    set_fan_speed(0);
    if (get_clock_ticks() - get_motors_saturated_ms() < 3000) {
      blink_RGB_color(is_motor_pwm_saturated() ? 255 : 0, 0, is_motor_angle_saturated() ? 255 : 0, 50);
    } else {
      set_RGB_color(0, 0, 0);
      set_race_started(false);
    }
    return;
  }*/
  if (!is_race_started()) {
    if (race_finish_ms > 0 && get_clock_ticks() - race_finish_ms <= 3000) {
      set_motors_brake();
    } else {
      set_motors_speed(0, 0);
      set_motors_enable(false);
    }
    set_fan_speed(0);
    return;
  } else {
    set_motors_enable(true);
  }

  update_ideal_linear_speed();
  update_fan_speed();
  set_fan_speed(ideal_fan_speed);

  /*
   * Actualizamos el angulo real continuamente, tambien mientras
   * se ejecutan MOVE_LEFT/MOVE_RIGHT.
   */
  update_measured_heading();

  float linear_voltage = 0;
  float angular_voltage = 0;

  if (linear_error_correction_enabled) {
    last_linear_error = linear_error;
    linear_error = ideal_linear_speed - get_measured_linear_speed();
    sum_linear_error += linear_error;
  } else {
    linear_error = 0;
    sum_linear_error = 0;
    last_linear_error = 0;
  }

  if (angular_error_correction_enabled) {
    last_angular_error = angular_error;

    /*
     * Durante un giro MOVE_LEFT/MOVE_RIGHT, ideal_angular_speed
     * contiene el perfil del giro y no agregamos correcciones.
     *
     * Durante una recta:
     *
     *   1) HEADING LOCK:
     *      corrige los grados que hayan quedado mal del giro.
     *
     *   2) WALL FOLLOW:
     *      sigue usando solamente la pared elegida para
     *      recuperar la posicion lateral.
     *
     * Ambas correcciones estan limitadas para evitar volantazos.
     */
    float commanded_angular_speed = ideal_angular_speed;

    if (fabsf(ideal_angular_speed) < 0.01f) {

      commanded_angular_speed +=
          get_heading_angular_correction();

      if (side_sensors_correction_enabled) {
        commanded_angular_speed +=
            get_wall_follow_angular_speed();
      }
    }

    angular_error =
        commanded_angular_speed -
        get_measured_angular_speed();

    sum_angular_error += angular_error;
  } else {
    angular_error = 0;
    sum_angular_error = 0;
    last_angular_error = 0;
  }

  /*
   * La correccion lateral vieja (get_side_sensors_error)
   * mezclaba pared izquierda/derecha y aplicaba un PID lateral
   * directo a los motores.
   *
   * En este modo ya no se usa. El seguimiento de pared se
   * convierte arriba en una velocidad angular deseada para
   * el PID del MPU.
   */
  side_sensors_error = 0;
  sum_side_sensors_error = 0;
  last_side_sensors_error = 0;

  if (front_sensors_angle_correction_enabled) {
    front_sensors_angle_error = get_front_sensors_angle_error();
    sum_front_sensors_angle_error += front_sensors_angle_error;
  } else {
    front_sensors_angle_error = 0;
    sum_front_sensors_angle_error = 0;
    last_front_sensors_angle_error = 0;
  }

  if (front_sensors_distance_correction_enabled && ideal_front_distance > 0 && get_front_wall_distance() < CELL_DIMENSION) {
    front_sensors_distance_error = get_front_wall_distance() - ideal_front_distance;
    sum_front_sensors_distance_error += front_sensors_distance_error;
  } else {
    front_sensors_distance_error = 0;
    sum_front_sensors_distance_error = 0;
    last_front_sensors_distance_error = 0;
  }

  if (front_sensors_diagonal_correction_enabled) {
    front_sensors_diagonal_error = get_front_sensors_diagonal_error();
    sum_front_sensors_diagonal_error += front_sensors_diagonal_error;
  } else {
    front_sensors_diagonal_error = 0;
    sum_front_sensors_diagonal_error = 0;
    last_front_sensors_diagonal_error = 0;
  }

  linear_voltage =
      get_kinematics().kpi[KPI_LINEAR].kp * linear_error +
      get_kinematics().kpi[KPI_LINEAR].ki * sum_linear_error +
      get_kinematics().kpi[KPI_LINEAR].kd * (linear_error - last_linear_error) +

      get_kinematics().kpi[KPI_FRONT_DISTANCE_SENSORS].kp * front_sensors_distance_error +
      get_kinematics().kpi[KPI_FRONT_DISTANCE_SENSORS].ki * sum_front_sensors_distance_error +
      get_kinematics().kpi[KPI_FRONT_DISTANCE_SENSORS].kd * (front_sensors_distance_error - last_front_sensors_distance_error);

  angular_voltage =
      get_kinematics().kpi[KPI_ANGULAR].kp * angular_error +
      get_kinematics().kpi[KPI_ANGULAR].ki * sum_angular_error +
      get_kinematics().kpi[KPI_ANGULAR].kd * (angular_error - last_angular_error) +

      get_kinematics().kpi[KPI_FRONT_ANGLE_SENSORS].kp * front_sensors_angle_error +
      get_kinematics().kpi[KPI_FRONT_ANGLE_SENSORS].ki * sum_front_sensors_angle_error +
      get_kinematics().kpi[KPI_FRONT_ANGLE_SENSORS].kd * (front_sensors_angle_error - last_front_sensors_angle_error) +

      get_kinematics().kpi[KPI_FRONT_DIAGONAL_SENSORS].kp * front_sensors_diagonal_error +
      get_kinematics().kpi[KPI_FRONT_DIAGONAL_SENSORS].ki * sum_front_sensors_diagonal_error +
      get_kinematics().kpi[KPI_FRONT_DIAGONAL_SENSORS].kd * (front_sensors_diagonal_error - last_front_sensors_diagonal_error);

  last_side_sensors_error = side_sensors_error;
  last_front_sensors_angle_error = front_sensors_angle_error;
  last_front_sensors_distance_error = front_sensors_distance_error;
  last_front_sensors_diagonal_error = front_sensors_diagonal_error;

  voltage_left = linear_voltage + angular_voltage;
  voltage_right = linear_voltage - angular_voltage;
  pwm_left = voltage_to_motor_pwm(voltage_left);
  pwm_right = voltage_to_motor_pwm(voltage_right);
  set_motors_pwm(pwm_left, pwm_right);

  if (is_race_mode()) {
    static char *labels[] = {
        "target_linear_speed",
        "ideal_linear_speed",
        "measured_linear_speed",
        // "measured_left_speed",
        // "measured_right_speed",
        "ideal_angular_speed",
        "measured_angular_speed",
        "raw_angular_speed",
        "pwm_left",
        "pwm_right",
        // "encoder_avg_millimeters",
        // "side_sensors_error",
        // "angular_voltage",
        "battery_voltage"};
    macroarray_store(
        1,
        0b000111001,
        labels,
        9,
        (int16_t)target_linear_speed,
        (int16_t)ideal_linear_speed,
        (int16_t)(get_measured_linear_speed()),
        // (int16_t)(get_encoder_left_speed()),
        // (int16_t)(get_encoder_right_speed()),
        (int16_t)(ideal_angular_speed * 100),
        (int16_t)(get_measured_angular_speed() * 100),
        (int16_t)(mpu6500_get_gyro_z_raw() * 100),
        (int16_t)pwm_left,
        (int16_t)pwm_right,
        // (int16_t)get_encoder_avg_millimeters(),
        // (int16_t)(side_sensors_error * 100),
        // (int16_t)(angular_voltage * 100),
        (int16_t)(get_battery_voltage() * 100));
  }

  if (ideal_linear_speed != 0 || ideal_angular_speed != 0) {
    // static char *labels[] = {
    //     "target_linear_speed",
    //     "ideal_linear_speed",
    //     "measured_linear_speed",
    //     // "measured_left_speed",
    //     // "measured_right_speed",
    //     "ideal_angular_speed",
    //     "measured_angular_speed",
    //     "raw_angular_speed",
    //     "pwm_left",
    //     "pwm_right",
    //     // "encoder_avg_millimeters",
    //     // "side_sensors_error",
    //     // "angular_voltage",
    //     "battery_voltage"};
    // macroarray_store(
    //     1,
    //     0b000111001,
    //     labels,
    //     9,
    //     (int16_t)target_linear_speed,
    //     (int16_t)ideal_linear_speed,
    //     (int16_t)(get_measured_linear_speed()),
    //     // (int16_t)(get_encoder_left_speed()),
    //     // (int16_t)(get_encoder_right_speed()),
    //     (int16_t)(ideal_angular_speed * 100),
    //     (int16_t)(get_measured_angular_speed() * 100),
    //     (int16_t)(mpu6500_get_gyro_z_raw() * 100),
    //     (int16_t)pwm_left,
    //     (int16_t)pwm_right,
    //     // (int16_t)get_encoder_avg_millimeters(),
    //     // (int16_t)(side_sensors_error * 100),
    //     // (int16_t)(angular_voltage * 100),
    //     (int16_t)(get_battery_voltage() * 100));

    // static char *labels[] = {
    //     "target_linear_speed",
    //     "ideal_linear_speed",
    //     "measured_linear_speed",
    //     "ideal_angular_speed",
    //     "measured_angular_speed",
    //     "front_left_distance",
    //     "front_right_distance",
    //     "diagonal_error",
    //     "encoder_avg_millimeters",
    //     "wall_lost_toggle_state",
    //     "cell_change_toggle_state"};
    // macroarray_store(
    //     2,
    //     0b00011000000,
    //     labels,
    //     11,
    //     (int16_t)target_linear_speed,
    //     (int16_t)ideal_linear_speed,
    //     (int16_t)(get_measured_linear_speed()),
    //     (int16_t)(ideal_angular_speed * 100.0),
    //     (int16_t)(get_measured_angular_speed() * 100),
    //     (int16_t)get_sensor_distance(SENSOR_FRONT_LEFT_WALL_ID),
    //     (int16_t)get_sensor_distance(SENSOR_FRONT_RIGHT_WALL_ID),
    //     (int16_t)front_sensors_diagonal_error,
    //     (int16_t)get_encoder_avg_millimeters(),
    //     (int16_t)get_wall_lost_toggle_state() ? 1 : 0,
    //     (int16_t)get_cell_change_toggle_state() ? 1 : 0
    //     );

    // static char *labels[] = {
    //     "sl",
    //     "fl",
    //     "fr",
    //     "sr",
    // };
    // macroarray_store(
    //     0,
    //     0b0,
    //     labels,
    //     4,
    //     (int16_t)get_sensor_distance(SENSOR_SIDE_LEFT_WALL_ID),
    //     (int16_t)get_sensor_distance(SENSOR_FRONT_LEFT_WALL_ID),
    //     (int16_t)get_sensor_distance(SENSOR_FRONT_RIGHT_WALL_ID),
    //     (int16_t)get_sensor_distance(SENSOR_SIDE_RIGHT_WALL_ID));
  }
}
#endif

#ifndef MMSIM_ENABLED
void keep_z_angle(void) {
  float linear_voltage = 0;
  float angular_voltage = 0;
  last_linear_error = linear_error;
  linear_error = ideal_linear_speed - get_measured_linear_speed();
  sum_linear_error += linear_error;

  last_angular_error = angular_error;
  angular_error = ideal_angular_speed - get_measured_angular_speed();
  sum_angular_error += angular_error;

  angular_voltage =
      get_kinematics().kpi[KPI_ANGULAR].kp * angular_error +
      get_kinematics().kpi[KPI_ANGULAR].ki * sum_angular_error +
      get_kinematics().kpi[KPI_ANGULAR].kd * (angular_error - last_angular_error);

  linear_voltage =
      get_kinematics().kpi[KPI_LINEAR].kp * linear_error +
      get_kinematics().kpi[KPI_LINEAR].ki * sum_linear_error +
      get_kinematics().kpi[KPI_LINEAR].kd * (linear_error - last_linear_error);

  voltage_left = linear_voltage + angular_voltage;
  voltage_right = linear_voltage - angular_voltage;
  pwm_left = voltage_to_motor_pwm(voltage_left);
  pwm_right = voltage_to_motor_pwm(voltage_right);
  gpio_set(GPIOB, GPIO15);
  set_motors_pwm(pwm_left, pwm_right);
  // printf("R: %4ld L: %4ld\n", pwm_right, pwm_left);
}
#endif