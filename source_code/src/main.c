#include <battery.h>
#include <buttons.h>
#include <control.h>
#include <delay.h>
#include <encoders.h>
#include <floodfill.h>
#include <handwall.h>
#include <leds.h>
#include <macroarray.h>
#include <menu.h>
#include <motors.h>
#include <move.h>
#include <timetrial.h>

#include <mpu6500.h>
#include <rc5.h>
#include <rc5_mappings.h>
#include <sensors.h>
#include <setup.h>
#include <usart.h>


/*
 * ============================================================
 * CONFIGURACION DEBUG
 * ============================================================
 *
 * DEBUG_ZONE 0 -> funcionamiento normal del robot
 * DEBUG_ZONE 1 -> zona de pruebas
 */

#define DEBUG_ZONE 0


/*
 * ------------------------------------------------------------
 * Elementos a mostrar/probar dentro de DEBUG_ZONE
 * ------------------------------------------------------------
 */

#define DEBUG_MPU          1
#define DEBUG_SENSORES     1
#define DEBUG_ENCODERS     1
#define DEBUG_AUX          1

#define DEBUG_LEDS         0
#define DEBUG_RGB          0

/* CUIDADO: estos pueden mover el robot */
#define DEBUG_MOTORES      0
#define DEBUG_VENTILADOR   0

#define DEBUG_PERIOD_MS    200


/*
 * ============================================================
 * SYSTICK
 * ============================================================
 *
 * Se ejecuta periodicamente.
 *
 * Mantiene:
 *  - reloj interno
 *  - encoders
 *  - sensores IR
 *  - bateria
 *  - LEDs
 *  - botones
 *  - MPU6500
 */

void sys_tick_handler(void)
{
    clock_tick();

    update_encoder_readings();

    update_sensors_magics();

    update_battery_voltage();

    check_leds_while();

    check_buttons();

    mpu6500_update();
}


/*
 * ============================================================
 * ZONA DEBUG
 * ============================================================
 */

#if DEBUG_ZONE

static void debug_zone(void)
{
    printf("\n");
    printf("========================================\n");
    printf(" ZONA DEBUG\n");
    printf("========================================\n");


#if DEBUG_MOTORES || DEBUG_VENTILADOR

    printf("ATENCION: hay pruebas que pueden mover el robot\n");
    delay(3000);

#endif


#if DEBUG_MOTORES

    /*
     * set_motors_speed():
     *
     * 0    -> detenido
     * 1000 -> maximo
     */

    set_motors_enable(true);
    set_motors_speed(150, 150);

#endif


#if DEBUG_VENTILADOR

    /*
     * set_fan_speed():
     *
     * 0   -> apagado
     * 100 -> maximo
     */

    set_fan_speed(50);

#endif


    while (1)
    {

#if DEBUG_LEDS

        set_leds_wave(125);

#endif


#if DEBUG_RGB

        set_RGB_rainbow();

#endif


#if DEBUG_MPU

        printf(
            "MPU: 0x%02X\t",
            mpu6500_who_am_i()
        );

        printf(
            "Z(raw): %6.1f "
            "Z(radps): %6.4f "
            "Z(dps): %6.4f "
            "Z(deg): %6.4f\t",

            mpu6500_get_gyro_z_raw(),
            mpu6500_get_gyro_z_radps(),
            mpu6500_get_gyro_z_dps(),
            mpu6500_get_gyro_z_degrees()
        );

#endif


#if DEBUG_SENSORES

        printf(
            "S1: %4d "
            "S2: %4d "
            "S3: %4d "
            "S4: %4d\t",

            get_sensor_raw(
                SENSOR_FRONT_LEFT_WALL_ID,
                true
            ),

            get_sensor_raw(
                SENSOR_FRONT_RIGHT_WALL_ID,
                true
            ),

            get_sensor_raw(
                SENSOR_SIDE_LEFT_WALL_ID,
                true
            ),

            get_sensor_raw(
                SENSOR_SIDE_RIGHT_WALL_ID,
                true
            )
        );

#endif


#if DEBUG_ENCODERS

        printf(
            "L: %ld R: %ld\t",

            get_encoder_left_millimeters(),
            get_encoder_right_millimeters()
        );

#endif


#if DEBUG_AUX

        printf(
            "BA: %4d "
            "CI: %4d "
            "CD: %4d "
            "BO: %4d\t",

            get_aux_raw(AUX_BATTERY_ID),
            get_aux_raw(AUX_CURRENT_LEFT_ID),
            get_aux_raw(AUX_CURRENT_RIGHT_ID),
            get_aux_raw(AUX_MENU_BTN_ID)
        );

        printf(
            "BAT: %.2f V\t",
            get_battery_voltage()
        );

#endif


        printf("\n");

        delay(DEBUG_PERIOD_MS);
    }
}

#endif


/*
 * ============================================================
 * MAIN
 * ============================================================
 */


int main(void)
{
    /*
     * Inicializacion general del hardware:
     *
     * - reloj
     * - GPIO
     * - USART
     * - ADC
     * - DMA
     * - PWM
     * - timers
     * - encoders
     * - SysTick
     * - MPU6500
     *
     * IMPORTANTE:
     * setup.c debe tener setup_mpu() habilitado.
     */

    setup();


    /*
     * Deshabilita el buffering de stdout.
     *
     * Esto permite que los printf aparezcan inmediatamente
     * por UART.
     */

    setvbuf(
        stdout,
        NULL,
        _IONBF,
        0
    );


    /*
     * Carga calibraciones y configuraciones guardadas.
     *
     * IMPORTANTE:
     *
     * Ya NO ponemos sensors_distance_offset[] en cero.
     *
     * Los offsets calibrados quedan cargados desde EEPROM.
     */

    eeprom_load();


    /*
     * Detecta version del robot y carga las curvas
     * de calibracion correspondientes.
     */

    handle_robot_version();


    /*
     * Muestra nivel de bateria mediante LEDs.
     */

    show_battery_level();


    /*
     * Emparejamiento del control remoto IR.
     *
     * Si al arrancar se presiona el botón MODE dos veces
     * seguidas (doble toque, como doble clic) se entra en
     * modo aprendizaje: se piden en orden los botones
     * CH_MODE, CH_DOWN, CH_UP y PLAY_PAUSE, y los códigos
     * capturados quedan guardados en EEPROM.
     */

    if (get_menu_mode_btn()) {
        uint32_t first_tap_ms = get_clock_ticks();

        // El primer toque debe soltarse rápido (<=500ms);
        // si se mantiene más que eso no es un doble toque.
        while (
            get_menu_mode_btn() &&
            get_clock_ticks() - first_tap_ms <= 500
        ) {
            warning_status_led(100);
        }

        if (get_clock_ticks() - first_tap_ms <= 500) {
            uint32_t released_ms = get_clock_ticks();
            bool second_tap = false;

            while (get_clock_ticks() - released_ms < 500) {
                if (get_menu_mode_btn()) {
                    second_tap = true;
                    break;
                }
            }

            if (second_tap) {
                rc5_mappings_learn();
            }
        }
    }


    /*
     * Información inicial por UART.
     */

    printf(
        "BA: %4d "
        "CI: %4d "
        "CD: %4d "
        "BO: %4d\n",

        get_aux_raw(AUX_BATTERY_ID),
        get_aux_raw(AUX_CURRENT_LEFT_ID),
        get_aux_raw(AUX_CURRENT_RIGHT_ID),
        get_aux_raw(AUX_MENU_BTN_ID)
    );


#if DEBUG_ZONE

    /*
     * En debug no existe el menu normal que habilita
     * los sensores, por eso hay que habilitarlos manualmente.
     */

    set_sensors_enabled(true);

    debug_zone();


#else

    /*
     * ========================================================
     * FUNCIONAMIENTO NORMAL DEL ZOROBOT
     * ========================================================
     */

    while (1)
    {

        /*
         * ----------------------------------------------------
         * ROBOT DETENIDO / MENU
         * ----------------------------------------------------
         */

        if (!is_race_started())
        {

            /*
             * Gestion del menu mediante botones.
             */

            menu_handler();


            /*
             * Los sensores IR se habilitan cuando estamos
             * preparados para iniciar una carrera.
             */

            if (
                !get_sensors_enabled() &&
                menu_run_can_start()
            )
            {
                set_sensors_enabled(
                    menu_run_can_start()
                );

                delay(200);
            }
            else
            {
                set_sensors_enabled(
                    menu_run_can_start() ||
                    is_debug_enabled()
                );
            }


            /*
             * Si el menu esta situado en RUN,
             * comprobamos los sensores frontales para
             * iniciar el robot.
             */

            if (menu_run_can_start())
            {
                int8_t sensor_started =
                    check_start_run();


                /*
                 * check_start_run() cambia race_started
                 * cuando detecta la condicion de inicio.
                 */

                if (is_race_started())
                {

                    switch (
                        menu_run_get_explore_algorithm()
                    )
                    {

                        /*
                         * ------------------------------------
                         * HAND WALL
                         * ------------------------------------
                         */

                        case EXPLORE_HANDWALL:

                            switch (sensor_started)
                            {

                                case SENSOR_FRONT_LEFT_WALL_ID:

                                    handwall_use_left_hand();

                                    handwall_start();

                                    break;


                                case SENSOR_FRONT_RIGHT_WALL_ID:

                                    handwall_use_right_hand();

                                    handwall_start();

                                    break;


                                default:

                                    set_race_started(false);

                                    break;
                            }

                            break;


                        /*
                         * ------------------------------------
                         * FLOODFILL
                         * ------------------------------------
                         */

                        case EXPLORE_FLOODFILL:

                            switch (sensor_started)
                            {

                                /*
                                 * Sensor frontal izquierdo:
                                 * carrera sobre laberinto
                                 * ya explorado.
                                 */

                                case SENSOR_FRONT_LEFT_WALL_ID:

                                    floodfill_start_run();

                                    break;


                                /*
                                 * Sensor frontal derecho:
                                 * exploracion del laberinto.
                                 */

                                case SENSOR_FRONT_RIGHT_WALL_ID:

                                    floodfill_start_explore();

                                    break;


                                default:

                                    set_race_started(false);

                                    break;
                            }

                            break;


                        /*
                         * ------------------------------------
                         * TIME TRIAL
                         * ------------------------------------
                         */

                        case EXPLORE_TIME_TRIAL:

                            timetrial_start();

                            break;


                        /*
                         * ------------------------------------
                         * ERROR
                         * ------------------------------------
                         */

                        default:

                            set_race_started(false);

                            break;
                    }
                }
            }
        }


        /*
         * ----------------------------------------------------
         * ROBOT EN CARRERA
         * ----------------------------------------------------
         */

        else
        {

            switch (
                menu_run_get_explore_algorithm()
            )
            {

                /*
                 * HAND WALL
                 */

                case EXPLORE_HANDWALL:

                    handwall_loop();

                    break;


                /*
                 * FLOODFILL
                 */

                case EXPLORE_FLOODFILL:

                    floodfill_loop();

                    break;


                /*
                 * TIME TRIAL
                 */

                case EXPLORE_TIME_TRIAL:

                    timetrial_loop();

                    break;


                /*
                 * Estado invalido.
                 */

                default:

                    set_race_started(false);

                    break;
            }
        }
    }

#endif


    return 0;
}
