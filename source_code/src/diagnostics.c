#include "diagnostics.h"

#include <stdio.h>

#define RESET_CAUSE_SHOW_MS 1500

/*
 * LED de informacion encendido segun la causa del ultimo reset, si
 * no fue un encendido normal:
 *
 *   LED 1 -> pin de reset (NRST)
 *   LED 2 -> reset por software
 *   LED 3 -> watchdog (IWDG / WWDG)
 *   LED 4 -> low-power
 *   LED 5 -> brown-out sin power-on (caida de tension)
 *
 * Con el BOR deshabilitado (opcion de fabrica), una caida fuerte de
 * la bateria reinicia por PDR y se ve como un encendido normal
 * (PORRST). Si el robot se reinicia en medio de una carrera sin que
 * nadie toque el interruptor y no aparece ninguno de estos LEDs, la
 * causa es electrica (bateria / conectores / picos de corriente).
 */
void show_reset_cause(void) {
  uint32_t csr = RCC_CSR;
  int8_t led = -1;
  const char *cause = "encendido (power-on / caida de tension)";

  if (csr & RCC_CSR_PORRSTF) {
    led = -1;
  } else if (csr & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) {
    led = 2;
    cause = "watchdog";
  } else if (csr & RCC_CSR_SFTRSTF) {
    led = 1;
    cause = "software";
  } else if (csr & RCC_CSR_LPWRRSTF) {
    led = 3;
    cause = "low-power";
  } else if (csr & RCC_CSR_BORRSTF) {
    led = 4;
    cause = "brown-out";
  } else if (csr & RCC_CSR_PINRSTF) {
    led = 0;
    cause = "pin de reset (NRST)";
  }

  printf("Causa del reset: %s (RCC_CSR = 0x%08lX)\n", cause, (unsigned long)csr);
  RCC_CSR |= RCC_CSR_RMVF;

  if (led >= 0) {
    uint32_t start_ms = get_clock_ticks();
    clear_info_leds();
    while (get_clock_ticks() - start_ms < RESET_CAUSE_SHOW_MS) {
      set_info_led(led, true);
      warning_status_led(100);
    }
    clear_info_leds();
    set_status_led(false);
  }
}

/*
 * Reemplaza al handler de libopencm3, que se queda en un lazo
 * infinito con los motores al ultimo PWM aplicado (el robot sigue
 * andando, congelado). Aca se cortan motores y turbina y el LED de
 * estado parpadea rapido para siempre: "robot congelado + LED de
 * estado rapido" = HardFault (error de software).
 *
 * No se puede usar delay(): el SysTick no corre dentro de una
 * HardFault.
 */
void hard_fault_handler(void) {
  gpio_clear(GPIOB, GPIO15);
  timer_set_oc_value(TIM1, TIM_OC1, 0);

  while (1) {
    gpio_toggle(GPIOB, GPIO14);
    for (volatile uint32_t i = 0; i < 2000000; i++) {
    }
  }
}
