#include <handwall.h>

static bool use_left_hand = true;
static uint32_t start_ms = 0;
static uint32_t time_limit = 0;

void handwall_use_left_hand(void) {
  use_left_hand = true;
  set_wall_follow_side(WALL_FOLLOW_LEFT);
}

void handwall_use_right_hand(void) {
  use_left_hand = false;
  set_wall_follow_side(WALL_FOLLOW_RIGHT);
}

void handwall_set_time_limit(uint32_t ms) {
  time_limit = ms;
}
void handwall_start(void) {
  start_ms = get_clock_ticks();

  /*
   * La orientacion al arrancar pasa a ser nuestro 0°.
   */
  reset_heading_reference();

  configure_kinematics(menu_run_get_speed());

  clear_info_leds();
  set_RGB_color(0, 0, 0);

  set_target_fan_speed(
      get_kinematics().fan_speed,
      400
  );

  delay(500);

  move(MOVE_START);
}
/*
 * Callejon sin salida: giro de 180° en el lugar, hecho como dos
 * giros de 90° con alineacion contra la pared en el medio.
 *
 *   1) Frena con rampa (no de golpe) en el lugar.
 *   2) Se alinea contra la pared del frente (keep_front_distance).
 *   3) Gira 90° hacia la pared del lado contrario al que sigue
 *      (siguiendo la izquierda gira a derecha y viceversa).
 *   4) Se alinea contra esa pared lateral y gira otros 90°.
 *
 * El heading objetivo se actualiza antes de cada giro para que el
 * heading lock no pelee contra el giro.
 */
static void handwall_dead_end_u_turn(void)
{
  float sign = use_left_hand ? 1.0f : -1.0f;
  enum movement turn = use_left_hand ? MOVE_RIGHT_INPLACE : MOVE_LEFT_INPLACE;

  move_straight(0, 0, false, true);

  disable_sensors_correction();
  reset_control_errors();
  keep_front_distance(MIDDLE_MAZE_DISTANCE, 150);

  for (uint8_t i = 0; i < 2; i++) {
    add_target_heading(sign * PI / 2.0f);
    move_inplace_turn(turn);
    set_ideal_angular_speed(0);
    reset_control_errors();
    if (i == 0) {
      keep_front_distance(MIDDLE_MAZE_DISTANCE, 150);
    }
  }
}

void handwall_loop(void) {
  if (time_limit > 0 && get_clock_ticks() - start_ms >= time_limit) {
    set_race_started(false);
    return;
  }

  struct walls walls = get_walls();

  set_RGB_color_while(255, 255, 0, 20);

  /*
   * ==========================================================
   * ENCERRADO / TRES PAREDES
   * ==========================================================
   *
   * Ahora:
   *   dos giros de 90° en el lugar con alineacion intermedia.
   */
  if (walls.front && walls.left && walls.right) {
    handwall_dead_end_u_turn();
    return;
  }

  /*
   * ==========================================================
   * SEGUIR PARED IZQUIERDA
   * ==========================================================
   */
  if (use_left_hand) {

    /* Apertura a la izquierda -> girar izquierda. */
    if (!walls.left) {
      add_target_heading(-PI / 2.0f);
      move(MOVE_LEFT);
      return;
    }

    /* Pared izquierda y frente libre -> seguir recto. */
    if (!walls.front) {
      move(MOVE_FRONT);
      return;
    }

    /* Frente cerrado y derecha libre -> girar derecha. */
    if (!walls.right) {
      add_target_heading(PI / 2.0f);
      move(MOVE_RIGHT);
      return;
    }

    handwall_dead_end_u_turn();
    return;
  }

  /*
   * ==========================================================
   * SEGUIR PARED DERECHA
   * ==========================================================
   */

  /* Apertura a la derecha -> girar derecha. */
  if (!walls.right) {
    add_target_heading(PI / 2.0f);
    move(MOVE_RIGHT);
    return;
  }

  /* Pared derecha y frente libre -> seguir recto. */
  if (!walls.front) {
    move(MOVE_FRONT);
    return;
  }

  /* Frente cerrado e izquierda libre -> girar izquierda. */
  if (!walls.left) {
    add_target_heading(-PI / 2.0f);
    move(MOVE_LEFT);
    return;
  }

  handwall_dead_end_u_turn();
}
