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
 * Recuperacion cuando el robot cree estar encerrado.
 *
 * IMPORTANTE:
 * YA NO EXISTE NINGUN GIRO DE 180° EN HANDWALL.
 *
 * Si seguimos pared derecha:
 *   gira 90° a izquierda en el lugar.
 *
 * Si seguimos pared izquierda:
 *   gira 90° a derecha en el lugar.
 *
 * Despues vuelve a handwall_loop() y vuelve a leer los sensores.
 */
static void handwall_recover_90(void)
{
  force_linear_speed(0);
  set_ideal_angular_speed(0);
  reset_control_errors();

  if (use_left_hand) {
    add_target_heading(PI / 2.0f);
    move_inplace_turn(MOVE_RIGHT_INPLACE);
  } else {
    add_target_heading(-PI / 2.0f);
    move_inplace_turn(MOVE_LEFT_INPLACE);
  }

  set_ideal_angular_speed(0);
  reset_control_errors();
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
   * Antes:
   *   MOVE_BACK_WALL -> 180°
   *
   * Ahora:
   *   solo 90° inplace y volver a medir.
   */
  if (walls.front && walls.left && walls.right) {
    handwall_recover_90();
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

    /*
     * Seguridad: si por alguna razon llegamos aca,
     * solamente recuperar 90°. Nunca 180°.
     */
    handwall_recover_90();
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

  /*
   * Seguridad: nunca ejecutar MOVE_BACK_WALL.
   */
  handwall_recover_90();
}
