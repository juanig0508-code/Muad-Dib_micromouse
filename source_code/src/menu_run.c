#include <menu_run.h>

#define MODE_SPEED 0
#define MODE_RACE 1
#define MODE_EXPLORE_TYPE 2
#define MODE_FLOODFILL_TYPE 3
#define MODE_MAZE_TYPE 4
#define MODE_SOLVE_STRATEGY 5
#define MODE_EXPLORE_ALGORITHM 6
uint8_t modeRun = MODE_SPEED;

#define MODE_SPEED_VALUES 6
#define MODE_EXPLORE_TYPE_VALUES 3
#define MODE_FLOODFILL_TYPE_VALUES 3
#define MODE_RACE_VALUES 2
#define MODE_MAZE_TYPE_VALUES 2
#define MODE_EXPLORE_ALGORITHM_VALUES 3
#define MODE_SOLVE_STRATEGY_VALUES 2

int16_t valueRun[MENU_RUN_NUM_MODES] = {0, 0, 0, 0, 0, 0, 1};

uint32_t lastBlinkMs = 0;
bool blinkState = false;

#ifndef MMSIM_ENABLED
/* ============================================================
 * REEMPLAZO DE handle_menu_run_values() - SIN RGB
 * ============================================================
 *
 * Pegar esta funcion en menu_run.c reemplazando la original
 * (la que va desde "static void handle_menu_run_values(void) {"
 * hasta su llave de cierre, justo antes de
 * handle_menu_run_btn).
 *
 *
 * POR QUE HACE FALTA
 *
 * El menu original usa el LED RGB para dos cosas:
 *
 *   1. Identificar la pantalla de CARRERA (rojo).
 *   2. Mostrar QUE VALOR esta elegido en las pantallas que
 *      tienen tres opciones (tipo de exploracion, tipo de
 *      floodfill, algoritmo).
 *
 * Sin RGB, esas dos cosas se pierden: no hay forma de saber si
 * estas en la pantalla de carrera, ni cual de las tres opciones
 * elegiste.
 *
 * El resto de la informacion ya la dan los LEDs A-E, que no
 * cambia.
 *
 *
 * LA REGLA NUEVA
 *
 *   FILA DE ARRIBA (1-5) -> QUE VALOR esta elegido
 *   FILA DE ABAJO  (A-E) -> EN QUE PANTALLA estas
 *
 * En cualquier pantalla que no sea la de velocidad, la fila de
 * arriba muestra el valor actual como una cuenta: 1 LED = valor
 * 0, 2 LEDs = valor 1, 3 LEDs = valor 2. Parpadeando.
 *
 * Compromiso: en esas pantallas se deja de ver la velocidad
 * elegida. Es a proposito, porque una sola regla clara se lee
 * mucho mejor que dos superpuestas. La velocidad se consulta
 * volviendo a la primera pantalla.
 *
 *
 * PANTALLA DE CARRERA
 *
 * Se marca con LOS DIEZ LEDS a la vez, que es un patron que no
 * usa ninguna otra pantalla:
 *
 *   parpadeando -> desarmado
 *   fijos       -> ARMADO, listo para tapar un sensor
 *
 * El LED de estado se deja como estaba: indica si se borra el
 * laberinto al empezar a explorar.
 *
 *
 * RESUMEN PARA USAR EL MENU
 *
 *   1. velocidad          fila arriba parpadea, fila abajo apagada
 *   2. CARRERA            los diez leds
 *   3. tipo exploracion   LED A parpadea
 *   4. tipo floodfill     LED B parpadea
 *   5. tipo laberinto     LED C parpadea
 *   6. estrategia         LED D parpadea
 *   7. algoritmo          LED E parpadea
 */

#ifndef MMSIM_ENABLED

/*
 * @brief Enciende los primeros n LEDs de la fila de arriba.
 *
 * Se usa para mostrar el valor seleccionado como una cuenta.
 */
static void show_count_row1(uint8_t count, bool state)
{
    static const uint8_t row1[5] = {
        INFO_LED_1, INFO_LED_2, INFO_LED_3, INFO_LED_4, INFO_LED_5
    };

    for (uint8_t i = 0; i < 5; i++) {
        set_info_led(row1[i], (i < count) ? state : false);
    }
}


static void clear_row2(void)
{
    set_info_led(INFO_LED_A, false);
    set_info_led(INFO_LED_B, false);
    set_info_led(INFO_LED_C, false);
    set_info_led(INFO_LED_D, false);
    set_info_led(INFO_LED_E, false);
}


static void set_row2_all(bool state)
{
    set_info_led(INFO_LED_A, state);
    set_info_led(INFO_LED_B, state);
    set_info_led(INFO_LED_C, state);
    set_info_led(INFO_LED_D, state);
    set_info_led(INFO_LED_E, state);
}


static void handle_menu_run_values(void)
{
    if (get_clock_ticks() - lastBlinkMs >= 125) {
        lastBlinkMs = get_clock_ticks();
        blinkState = !blinkState;
    }

    /*
     * El RGB se apaga siempre: en esta placa no esta montado.
     * Si en algun momento se conecta, se puede volver a la
     * version original.
     */
    set_RGB_color(0, 0, 0);


    /* ========================================================
     * 1 - VELOCIDAD
     * ========================================================
     *
     * Igual que el original: parpadea el LED de la velocidad
     * elegida, o los cinco juntos si es HAKI.
     */
    if (modeRun == MODE_SPEED) {

        clear_row2();
        set_status_led(false);

        if (valueRun[MODE_SPEED] == MODE_SPEED_VALUES - 1) {
            show_count_row1(5, blinkState);
        } else {
            show_count_row1(0, false);
            set_info_led((uint8_t)valueRun[MODE_SPEED], blinkState);
        }

        return;
    }


    /* ========================================================
     * 2 - CARRERA
     * ========================================================
     *
     * Los diez LEDs a la vez. Ninguna otra pantalla hace esto,
     * asi que no se puede confundir.
     *
     *   parpadeando -> desarmado
     *   fijos       -> armado
     */
    if (modeRun == MODE_RACE) {

        bool armed = (valueRun[MODE_RACE] == 1);

        show_count_row1(5, armed ? true : blinkState);
        set_row2_all(armed ? true : blinkState);

        /*
         * Sin cambios: indica si se borra el laberinto al
         * empezar la exploracion.
         */
        set_status_led(floodfill_is_reset_maze_on_start_explore());

        return;
    }


    /* ========================================================
     * 3 a 7 - EL RESTO
     * ========================================================
     *
     * Fila de arriba: el valor, como cuenta de LEDs.
     * Fila de abajo: cual pantalla, con su LED parpadeando.
     */
    set_status_led(false);
    clear_row2();

    uint8_t value = (uint8_t)valueRun[modeRun];

    show_count_row1(value + 1, blinkState);

    switch (modeRun) {

        case MODE_EXPLORE_TYPE:
            set_info_led(INFO_LED_A, blinkState);
            break;

        case MODE_FLOODFILL_TYPE:
            set_info_led(INFO_LED_B, blinkState);
            break;

        case MODE_MAZE_TYPE:
            set_info_led(INFO_LED_C, blinkState);
            break;

        case MODE_SOLVE_STRATEGY:
            set_info_led(INFO_LED_D, blinkState);
            break;

        case MODE_EXPLORE_ALGORITHM:
            set_info_led(INFO_LED_E, blinkState);
            break;

        default:
            modeRun = MODE_SPEED;
            break;
    }
}

#endif
static void handle_menu_run_btn(void) {
  if (get_menu_up_btn()) {
    while (get_menu_up_btn()) {
      handle_menu_run_values();
    }
    menu_run_up();
  }

  if (get_menu_down_btn()) {
    while (get_menu_down_btn()) {
      handle_menu_run_values();
    }
    menu_run_down();
  }
}
#endif

bool menu_run_handler(void) {
#ifndef MMSIM_ENABLED
  set_status_led(false);
  if (get_menu_mode_btn()) {
    uint32_t ms = get_clock_ticks();
    while (get_menu_mode_btn()) {
      if (get_clock_ticks() - ms >= 200) {
        warning_status_led(50);
      }
    }
    if (get_clock_ticks() - ms >= 200) {
      return true;
    } else if (modeRun != MODE_RACE || !valueRun[MODE_RACE]) {
      menu_run_mode_change();
    } else if (modeRun == MODE_RACE) {
      floodfill_set_reset_maze_on_start_explore(!floodfill_is_reset_maze_on_start_explore());
      set_status_led(floodfill_is_reset_maze_on_start_explore());
    } else {
      set_status_led(false);
    }
  }
  handle_menu_run_btn();
  handle_menu_run_values();
#endif
  return false;
}

void menu_run_reset(void) {
  modeRun = MODE_SPEED;
  valueRun[MODE_RACE] = 0;
}

void menu_run_load_values(void) {
#ifndef MMSIM_ENABLED
  int16_t *data = eeprom_get_data();
  for (uint16_t i = DATA_INDEX_MENU_RUN; i < (DATA_INDEX_MENU_RUN + MENU_RUN_NUM_MODES); i++) {
    valueRun[i - DATA_INDEX_MENU_RUN] = data[i];
  }
  valueRun[MODE_RACE] = 0;
#endif
}

void menu_run_mode_change() {
  modeRun = (modeRun + 1) % MENU_RUN_NUM_MODES;
}

void menu_run_up() {
#ifndef MMSIM_ENABLED
  uint8_t mode_values = 0;
  switch (modeRun) {
    case MODE_SPEED:
      mode_values = MODE_SPEED_VALUES;
      break;
    case MODE_RACE:
      mode_values = MODE_RACE_VALUES;
      break;
    case MODE_EXPLORE_TYPE:
      mode_values = MODE_EXPLORE_TYPE_VALUES;
      break;
    case MODE_FLOODFILL_TYPE:
      mode_values = MODE_FLOODFILL_TYPE_VALUES;
      break;
    case MODE_MAZE_TYPE:
      mode_values = MODE_MAZE_TYPE_VALUES;
      break;
    case MODE_EXPLORE_ALGORITHM:
      mode_values = MODE_EXPLORE_ALGORITHM_VALUES;
      break;
    case MODE_SOLVE_STRATEGY:
      mode_values = MODE_SOLVE_STRATEGY_VALUES;
      break;
  }
  valueRun[modeRun] = (valueRun[modeRun] + 1) % mode_values;
  if (modeRun == MODE_RACE && valueRun[modeRun] == 1) {
    set_RGB_color(50, 0, 0);
    eeprom_set_data(DATA_INDEX_MENU_RUN, valueRun, MENU_RUN_NUM_MODES);
    eeprom_save();
  }
#endif
}

void menu_run_down() {
  if (valueRun[modeRun] > 0) {
    valueRun[modeRun]--;
  }
}

bool menu_run_can_start(void) {
  return modeRun == MODE_RACE && valueRun[MODE_RACE] > 0;
}

int16_t *get_menu_run_values(void) {
  return valueRun;
}

enum speed_strategy menu_run_get_speed(void) {
#ifndef MMSIM_ENABLED
  return valueRun[MODE_SPEED];
#else
  return SPEED_HAKI;
#endif
}

enum explore_type menu_run_get_explore_type(void) {
#ifndef MMSIM_ENABLED
  return valueRun[MODE_EXPLORE_TYPE];
#else
  extern int MMSIM_EXPLORE_TYPE;
  return MMSIM_EXPLORE_TYPE;
#endif
}

enum floodfill_type menu_run_get_floodfill_type(void) {
#ifndef MMSIM_ENABLED
  return valueRun[MODE_FLOODFILL_TYPE];
#else
  extern int MMSIM_FLOODFILL_TYPE;
  return MMSIM_FLOODFILL_TYPE;
#endif
}

enum maze_type menu_run_get_maze_type(void) {
#ifndef MMSIM_ENABLED
  return valueRun[MODE_MAZE_TYPE];
#else
  return MAZE_COMPETITION;
#endif
}

enum solve_strategy menu_run_get_solve_strategy(void) {
#ifndef MMSIM_ENABLED
  return valueRun[MODE_SOLVE_STRATEGY];
#else
  return SOLVE_DIAGONALS;
#endif
}

enum explore_algorithm menu_run_get_explore_algorithm(void) {
#ifndef MMSIM_ENABLED
  return valueRun[MODE_EXPLORE_ALGORITHM];
#else
  return EXPLORE_FLOODFILL;
#endif
}