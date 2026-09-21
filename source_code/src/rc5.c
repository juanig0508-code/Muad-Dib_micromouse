/**
 * Captura de trama IR (Samsung/NEC/cualquiera) como "huella" (hash),
 * en vez de decodificar el protocolo exacto.
 *
 * Mucho mas tolerante que decodificar bit a bit: junta los tiempos
 * entre flancos de toda la trama en un hash, y cuando pasan >15ms
 * sin nuevos flancos, se considera que la trama termino.
 *
 * Botones iguales -> mismo hash (aprox). Botones distintos -> hash distinto.
 */
#include <rc5.h>
#include <stdbool.h>

#define FRAME_GAP_MS   15
#define MIN_EDGES      20
#define BIT_THRESHOLD_US 1000

static uint32_t last_us = 0;
static volatile uint32_t last_activity_ms = 0;
static volatile uint32_t signal_count = 0;

static volatile uint32_t frame_hash = 0;
static volatile uint8_t frame_edges = 0;
static volatile bool frame_open = false;

static volatile uint32_t last_code = 0;
static volatile uint32_t code_count = 0;

void rc5_load_eeprom(void) {
  // No usado en este test de deteccion IR.
}

uint32_t rc5_get_last_activity_ms(void) {
  return last_activity_ms;
}

uint32_t rc5_get_signal_count(void) {
  return signal_count;
}

uint32_t rc5_get_last_code(void) {
  return last_code;
}

uint32_t rc5_get_code_count(void) {
  return code_count;
}

void rc5_register(enum RC5_TRIGGER trigger) {
  uint32_t us = get_us_counter();
  uint32_t elapsed = us - last_us;
  last_us = us;

  if (trigger != RC5_TRIGGER_FALLING) {
    return;
  }

  last_activity_ms = get_clock_ticks();
  signal_count++;

  if (!frame_open) {
    frame_open = true;
    frame_hash = 0;
    frame_edges = 0;
  } else {
    uint8_t bit = (elapsed > BIT_THRESHOLD_US) ? 1 : 0;
    frame_hash = (frame_hash << 1) | bit;
    frame_edges++;
  }
}

void rc5_poll(void) {
  if (frame_open && (get_clock_ticks() - last_activity_ms) > FRAME_GAP_MS) {
    if (frame_edges >= MIN_EDGES) {
      last_code = frame_hash;
      code_count++;
    }
    frame_open = false;
  }
}
