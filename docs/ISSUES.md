# Muad-Dib — Problemas detectados (ISSUES)

Revisión de `source_code/src` y `source_code/include` sobre `master` @ `b0b255d`, del 2026-10-19. Las referencias `FR-x` / `NFR-x` remiten a `docs/FSD.md`.

**Alcance de este paso (Paso 2):** errores de lógica, cuellos de botella de rendimiento e inconsistencias de estilo.
**Paso 3:** contraste con los problemas conocidos del `.docx` y de los commits. Los hallazgos nuevos son L-22 a L-29; el mapa está en la sección D.

**Leyenda**
- **Severidad**:
  - 🔴 Crítico: puede hacer perder una pasada.
  - 🟠 Alto: degrada la navegación de forma notable.
  - 🟡 Medio.
  - ⚪ Bajo / cosmético.
- **Certeza**:
  - **Confirmado**: se deduce del código o de un cálculo.
  - **Probable**: hay que verificarlo en banco o en el laberinto.
- No se compiló el firmware: PlatformIO no está instalado en esta máquina. Ningún hallazgo depende de la compilación.

---

## Resumen priorizado

| ID | Sev. | Certeza | Título |
|---|---|---|---|
| L-01 | 🔴 | Confirmado | El giro de exploración suma ≈85°, no 90°, y sale ≈14 mm corto |
| L-02 | 🟠 | Confirmado | El ángulo del giro en arco depende de la velocidad lineal real |
| L-03 | 🔴 | Confirmado | El callejón sin salida no actualiza la posición en la celda (error de media celda) |
| L-04 | 🔴 | Probable | Los umbrales de pared frontal (180/280) superan las lecturas medidas de una pared a una celda |
| L-05 | 🟠 | Confirmado | Handwall usa la velocidad guardada en el menú: con un perfil ≠ EXPLORE no gira y prende la turbina |
| L-06 | 🟠 | Confirmado | Desborde en el layout de EEPROM: escritura fuera del arreglo y se pierde el botón IR NUM_5 |
| L-07 | 🟠 | Confirmado | El seguimiento de pared usa mm poco confiables y un objetivo (130) distinto al de la calibración (124) |
| L-08 | 🟡 | Confirmado | El heading lock nunca se re-referencia con las paredes y su corrección desplaza lateralmente |
| L-09 | 🟡 | Confirmado | El heading se integra con dt = 1 ms, pero el control corre cada 997 µs |
| L-10 | 🟡 | Probable | El integral del PID angular no tiene anti-windup ni se resetea después del arco |
| L-11 | 🟡 | Probable | El giro en el lugar es a lazo abierto en ángulo y puede disparar el corte por saturación |
| L-12 | 🟡 | Confirmado | `check_start_run()` compara un timestamp contra una duración y el lado del sensor pisa al del menú |
| L-13 | 🟡 | Confirmado | Acceso concurrente a SPI3 entre el hilo principal y la ISR del giróscopo |
| L-14 | 🟡 | Confirmado | La decisión de paredes sale de una sola muestra cruda, sin promedio ni histéresis |
| L-15 | ⚪ | Confirmado | Índice fuera de rango en `ln_lookup`. Se usan los coeficientes del ZoroBot3 "A" |
| L-16 | ⚪ | Confirmado | `reset_encoder_avg()` no tiene efecto |
| L-17 | 🟡 | Confirmado | La autoprueba de giro (botones 4/5) no reproduce el giro real |
| L-18 | ⚪ | Confirmado | `MOVE_BACK_WALL` modificado quedó roto para floodfill (código no usado por handwall) |
| L-19 | ⚪ | Confirmado | Esperas que no miran `is_race_started()` |
| L-20 | ⚪ | Confirmado | El contador de microsegundos da la vuelta cada 25,6 s |
| L-21 | 🟡 | Confirmado | Floodfill, mapa y time-trial no sirven para el reglamento LNR |
| L-22 | 🔴 | Probable | El offset del giróscopo se guarda ×1000 en `int16` y desborda: bias erróneo después de reiniciar |
| L-23 | 🟠 | Confirmado | La distancia en mm "da la vuelta" (negativo → ~65 500), queda vieja o satura en ~2,5 m |
| L-24 | 🟠 | Probable | `WALL_LOSS_TO_SENSING_POINT_DISTANCE` (162) nunca se calibró: los giros arrancan en un punto incorrecto |
| L-25 | 🟠 | Probable | Los umbrales laterales no están en la misma posición relativa; un corrimiento de ~20 mm produce aperturas falsas |
| L-26 | 🟡 | Probable | El DLPF del MPU6500 a 41 Hz agrega ~6 ms de retardo con PIDs ajustados para el LSM6DSR |
| L-27 | 🟡 | Confirmado | La corrección del sobregiro (PR #12) solo aplica a las autopruebas, no al laberinto |
| L-28 | 🟡 | Confirmado | Frenado en corto inmediato y aceleraciones altas: golpes sobre los engranajes |
| L-29 | ⚪ | Confirmado | Reinicios: no hay ninguna fuente de reset por software. Hay "pseudo-reinicios" por fin de carrera |
| P-01 | 🟡 | Confirmado | Aritmética `double` (emulada por software) dentro de las ISR |
| P-02 | ⚪ | Confirmado | `get_kinematics()` copia una struct entera unas 20 veces por ms |
| P-03 | 🟡 | Confirmado | Se borra un sector de flash cada vez que se arma un lado (congela las ISR 1–2 s) |
| P-04 | ⚪ | Confirmado | `mpu6500_read_register()` reinicia el SPI dos veces con `delay(100)` |
| P-05 | ⚪ | Confirmado | TIM2 corre a ~32 kHz, no a 16 kHz |
| P-06 | ⚪ | Confirmado | Escrituras al RGB (no montado) en lazos calientes |
| E-01 … E-12 | ⚪ | — | Estilo y mantenibilidad (ver sección C) |

---

## A. Errores de lógica

### L-01 🔴 El giro de exploración suma ≈85°, no 90°, y sale ≈14 mm corto
- **Ubicación:** `src/move.c:137-157` (`turns_explore`) y `move_arc_turn()` en `src/move.c:1932`.
- **Problema:** el giro en arco aplica `ω = max_angular_speed · perfil(s)` con rampas senoidales. El ángulo total es
  `θ = ω_max / v · (4·transition/π + arc)`.
  Con los valores actuales (8,5 rad/s, 600 mm/s, 75,04 mm, 8,89 mm) da **84,8°**. Una simulación de la trayectoria completa (start + arco + end) desde el punto de decisión da un desplazamiento de **113 mm lateral × 124 mm de avance**. Para una celda de 260 mm el objetivo es 130 × 130. Como control, la tabla original del ZoroBot3 da 90,0° y 93 × 93 mm en celdas de 180 mm.
- **Impacto:** al salir de cada giro faltan ~5° de rumbo. El heading lock los recupera durante la recta siguiente, y mientras tanto el robot deriva ~20–25 mm hacia la pared exterior del giro (ver L-08). Además, la siguiente decisión se toma ~17 mm antes del borde de celda, hasta que una pérdida de pared resincroniza la posición. Con varios giros seguidos el error se acumula y el robot termina rozando o chocando una pared.
- **Sugerencia:** recalcular el giro para la celda medida. Con transición y arco fijos, `ω_max ≈ 9,02 rad/s` da 90°. Para llegar a 130 × 130 hay que agregar ≈14 mm a `start` y a `end`, o bien ampliar el radio. Mejor todavía, derivar los parámetros de `CELL_DIMENSION` (FR-M7). Documentar la fórmula junto a la tabla.

### L-02 🟠 El ángulo del giro en arco depende de la velocidad lineal real
- **Ubicación:** `move_arc_turn()` en `src/move.c:1932-1959`.
- **Problema:** el avance dentro del perfil se mide en **distancia** (encoders), pero la velocidad angular pedida es un valor **en rad/s** que no depende de la velocidad. El ángulo real es `∫ω(s)/v(s) ds`. Si el robot va a 550 mm/s en lugar de 600, el giro da 84,8 × 600/550 ≈ **92,5°**.
- **Impacto:** el sobregiro y el subgiro varían según la velocidad de entrada y la batería. Esto coincide con el "a veces gira de más" que describe el `.docx`. Pasa sobre todo en el primer giro después de arrancar, de un callejón o de frenar, y si el PID lineal se queda atrás durante el arco.
- **Sugerencia:** calcular `ω = (ω_max / turn.linear_speed) · v_medida · perfil(s)`, es decir, curvatura × velocidad. Así el ángulo depende solo de la geometría.

### L-03 🔴 El callejón sin salida no actualiza la posición en la celda
- **Ubicación:** `handwall_dead_end_u_turn()` en `src/handwall.c:55-75`, y `keep_front_distance()` en `src/move.c:1617`.
- **Problema:**
  1. Las decisiones se toman con el eje de ruedas sobre el borde de celda. `move_front()` avanza `CELL_DIMENSION - current_cell_start_mm`. Después del U-turn el robot queda (idealmente) en el **centro** de la celda mirando hacia afuera, pero `current_cell_start_mm` sigue en 0. El `MOVE_FRONT` siguiente recorre 260 mm y termina en el **centro** de la celda anterior, no en su borde. Las decisiones quedan corridas media celda, y el error solo se corrige si aparece una pérdida de pared lateral (`WALL_LOSS_TO_SENSING_POINT_DISTANCE`). En un pasillo con paredes a ambos lados no se corrige nunca.
  2. La alineación inicial `keep_front_distance(124)` solo corre mientras `front_wall_detection()` sea verdadero, es decir, distancia en mm < `SENSOR_FRONT_DETECTION` = 0,8·260 = 208 mm. Al llegar al callejón, la pared del fondo está a ≈254 − 22 (frenada) ≈ 232 mm del eje, así que el lazo sale enseguida, **sin centrar**. El robot hace el primer giro de 90° cerca de la entrada y no en el centro.
- **Impacto:** después de cada callejón, los giros siguientes se hacen media celda tarde, o algo antes si falló el centrado. Es una causa muy probable de choques "al azar".
- **Sugerencia:** después del U-turn, retroceder hasta la pared del fondo y llamar a `set_starting_position()`, como hacía el ZoroBot3 original. La pared es una referencia exacta. Si no, fijar `current_cell_start_mm` según dónde terminó realmente el robot. Para centrar, usar `move_straight_until_front_distance()` o un umbral de detección frontal acorde a 260 mm.

### L-04 🔴 Los umbrales de pared frontal (180/280) superan las lecturas medidas a una celda
- **Ubicación:** `src/sensors.c:372-373`. El comentario en `src/sensors.c:357-370` dice "FL >= 70, FR >= 120".
- **Problema:** el `.docx` (§5) y el propio comentario indican que, con una pared a una celda, FL ≈ **114** y FR ≈ **193** (raw). Los umbrales definidos son **180** y **280**, por encima de esos valores. Si las mediciones son válidas en el punto de decisión, la pared del frente **no se detecta** y el robot avanza contra ella. Entonces salta la saturación y la carrera termina.
- **Certeza:** probable. Los umbrales pueden haberse subido después de reparar la soldadura del sensor frontal, y la tabla puede ser vieja. Hay que medir de nuevo.
- **Sugerencia:** medir FL y FR crudos con el eje en el borde de celda y la pared al fondo de la celda siguiente (≈206 mm del frente del robot), y ubicar los umbrales entre "despejado" y "pared". Actualizar el comentario. Agregar una prueba de banco (T4 del FSD).

### L-05 🟠 Handwall usa la velocidad guardada en el menú
- **Ubicación:** `handwall_start()` en `src/handwall.c:28`, `selftest_explore_turn()` en `src/main.c:320`, `configure_kinematics()` en `src/move.c:1488`, y `menu_run_load_values()` en `src/menu_run.c`.
- **Problema:** `configure_kinematics(menu_run_get_speed())` toma el valor de la pantalla VELOCIDAD, que persiste en EEPROM al armar un lado. Los perfiles NORMAL…HAKI **no tienen** entradas `MOVE_LEFT`/`MOVE_RIGHT`: esas posiciones de la tabla quedan en cero, y `move_arc_turn()` termina al instante. Esos perfiles además prenden la turbina (60–90 %) y van a 3000–6500 mm/s. Por otro lado, si la EEPROM está borrada, el valor leído es −1 y `kinematics_settings[-1]` lee fuera del arreglo.
- **Impacto:** un toque accidental de UP en la primera pantalla deja al robot sin giros, rápido y con la turbina encendida.
- **Sugerencia:** en handwall y en las autopruebas forzar `SPEED_EXPLORE`. Validar los rangos de `valueRun[]` al cargarlos de la EEPROM y usar 0 como valor por defecto.

### L-06 🟠 Desborde en el layout de EEPROM
- **Ubicación:** `include/eeprom.h:13`.
- **Problema:** `DATA_LENGTH = 2 + NUM_SENSORES + …`, pero los índices se calculan con `MPU_DATA_LENGTH = 3`. El último dato (parte alta del código IR de NUM_5) cae en `eeprom_data[DATA_LENGTH]`, una posición **fuera del arreglo**. `rc5_mappings_save_eeprom()` escribe ahí y pisa la variable estática vecina. `eeprom_save()` no lo graba, y `rc5_mappings_load_eeprom()` lo lee fuera de rango. El error ya existía en el original, pero antes el último bloque (RC5) no se usaba.
- **Impacto:** NUM_5 deja de funcionar al reiniciar, y hay corrupción de memoria al emparejar el control.
- **Sugerencia:** `DATA_LENGTH = MPU_DATA_LENGTH + NUM_SENSORES + …`, o mejor `DATA_INDEX_RC5_MAPPINGS + RC5_MAPPINGS_DATA_LENGTH`. Agregar un `_Static_assert` contra el tamaño del sector y volver a emparejar después del cambio.

### L-07 🟠 El seguimiento de pared usa mm poco confiables y un objetivo inconsistente
- **Ubicación:** `src/control.c:52-59` y `get_wall_follow_angular_speed()` en `src/control.c:244`. La calibración está en `side_sensors_calibration()`, `src/sensors.c:311`.
- **Problema:**
  - El centrado en rectas usa `get_sensor_distance()` (mm), que según el equipo no es confiable.
  - La calibración lateral ajusta el offset para que el robot centrado lea `MIDDLE_MAZE_DISTANCE` = (260−12)/2 = **124 mm**, pero el objetivo del seguidor es **130 mm**. Aun con sensores perfectos, el robot queda corrido ~6 mm hacia afuera de la pared seguida.
  - El comentario dice "~141 / ~137 mm", que ya no corresponde.
- **Impacto:** el centrado es incorrecto en todas las rectas, y se suma a L-01 y L-08.
- **Sugerencia:** usar `MIDDLE_MAZE_DISTANCE` como objetivo. A corto plazo, conviene una tabla raw→mm por sensor medida en el robot (ver Paso 3), o un seguidor que trabaje directamente con el raw y un objetivo raw calibrado en el centro.

### L-08 🟡 El heading lock nunca se re-referencia y su corrección desplaza lateralmente
- **Ubicación:** `src/control.c:79-80, 219-234, 643-654`, y `keep_front_distance()`.
- **Problema:**
  - El rumbo objetivo es absoluto (giróscopo). Cuando `keep_front_distance()` alinea el robot con una pared real, el error medido no se pone en cero. Al reactivar el control angular, el lock vuelve a tirar hacia el rumbo del giróscopo y deshace la alineación.
  - Un error residual `e₀` después de un giro se corrige en la recta con KP = 2. El robot avanza con rumbo errado y se desplaza lateralmente ≈ `v·e₀/KP`: con 5° a 600 mm/s son ≈26 mm.
  - `add_target_heading()` se llama antes de `move(MOVE_LEFT/RIGHT)`. Durante el tramo recto previo al arco, el lock ya ve 90° de error y gira al máximo (0,25 rad/s). Es poco (~0,5°), pero no es intencional.
- **Sugerencia:** después de alinearse contra una pared, igualar `measured_heading_rad` al objetivo. Después de giros en el lugar, esperar a que el error sea pequeño antes de avanzar. Aplicar el lock solo en rectas, por ejemplo con un flag que pongan `move_straight`/`move_front`, en lugar de `|ideal_angular| < 0.01`.

### L-09 🟡 El heading se integra con dt = 1 ms, pero el control corre cada 997 µs
- **Ubicación:** `setup_main_loop_timer()` en `src/setup.c:353` (`timer_set_period(TIM5, 996)` → 997 cuentas a 1 MHz) y `update_measured_heading()` en `src/control.c:198`.
- **Problema:** el lazo corre a ≈1003 Hz y divide por 1000, así que el ángulo medido sale **0,3 % más grande**. Cada 90° reales se leen como 90,27°.
- **Impacto:** el error se acumula con giros del mismo sentido. Seguir una pared genera muchos giros hacia el mismo lado, con un desvío de ~2,7° cada 10 giros, que el lock "corrige" girando mal.
- **Sugerencia:** usar período 999, o integrar con el dt real medido por DWT.

### L-10 🟡 El integral del PID angular no tiene anti-windup ni se resetea después del arco
- **Ubicación:** `move_side()` en `src/move.c:1157` (resetea solo **antes** del arco) y `control_loop()` en `src/control.c:656-660`.
- **Problema:** durante el arco, el retraso de seguimiento acumula `sum_angular_error` con `ki` = 0,015 por ms. Al volver `ideal_angular_speed` a 0, ese integral sigue empujando en el sentido del giro y produce **sobregiro**. Tampoco hay límite para el integral lineal ni para el angular.
- **Sugerencia:** limitar los integrales (clamp) y resetear o descargar el integral angular al terminar el arco. Si hace falta, agregar feed-forward de velocidad angular para no depender del integral.

### L-11 🟡 El giro en el lugar es a lazo abierto en ángulo y puede disparar la saturación
- **Ubicación:** `turns_inplace` en `src/move.c:104-135`, `move_inplace_turn()` en `src/move.c:1961`, y `check_motors_saturated()` en `src/motors.c`.
- **Problema:**
  - `move_inplace_turn()` genera un perfil **por tiempo** (10 rad/s, 350 rad/s²) y no verifica el ángulo final. El retraso del PID deja un error que se corrige recién en la recta siguiente (L-08).
  - Una aceleración de 350 rad/s² puede exigir PWM ≥ 1000 durante más de 30 ms. Desde el PR #15 eso **corta la carrera**, justo en un callejón.
- **Sugerencia:** usar `move_inplace_angle()` (lazo cerrado con giróscopo, ya corregido con la rampa de frenado) o un perfil con verificación final. Bajar la aceleración angular. Registrar si hubo saturación (LED) para diagnosticar.

### L-12 🟡 `check_start_run()` compara un timestamp contra una duración
- **Ubicación:** `src/control.c:356-409`. El error ya estaba en el original.
- **Problema:**
  - `sensor_front_left_start_ms` guarda `get_clock_ticks()` (el tiempo absoluto) y después se compara `>= SENSOR_START_MIN_MS` (150). Esa condición es siempre verdadera, así que **no hay tiempo mínimo de tapado**: cualquier lectura momentánea < 100 mm arma un lado.
  - En `main.c:576-583`, el lado armado por sensor **tiene prioridad** sobre el elegido en el menú, sin aviso visible.
  - La función bloquea el lazo principal 2 s.
- **Impacto:** al apoyar el robot o al pasar la mano cerca, puede cambiar el lado a seguir sin que el operador lo note.
- **Sugerencia:** comparar `get_clock_ticks() - start_ms >= SENSOR_START_MIN_MS`. Mostrar en los LEDs el lado efectivo. Pensar si el armado por sensor sigue haciendo falta cuando exista el botón físico.

### L-13 🟡 Acceso concurrente a SPI3
- **Ubicación:** `mpu6500_reload_config()` en `src/mpu6500.c:235`, `mpu6500_read_register()` en `src/mpu6500.c:84`, y `mpu6500_update()` (SysTick).
- **Problema:** desde el hilo principal se reinicia y reconfigura el SPI3 (`setup_spi_*`, que hace un reset del periférico) y se transmiten registros mientras la ISR de SysTick lee el giróscopo por el mismo bus. `mpu_updating` no se desactiva, a diferencia de la calibración.
- **Impacto:** lecturas o escrituras corruptas. Se activa al cambiar de perfil de velocidad (L-05) y en `DEBUG_ZONE` (`mpu6500_who_am_i()` cada 200 ms).
- **Sugerencia:** poner `mpu_updating = false` alrededor de cualquier acceso desde el hilo principal, o deshabilitar la IRQ de SysTick durante la transacción.

### L-14 🟡 La decisión de paredes sale de una sola muestra cruda
- **Ubicación:** `get_walls()` en `src/sensors.c:578-600`.
- **Problema:** cada decisión de navegación usa una sola diferencia on−off instantánea, sin filtro ni histéresis. `SENSOR_LOW_PASS_FILTER_ALPHA` existe, pero no se usa.
- **Impacto:** el ruido o una lectura justo en el umbral cambia la decisión (girar o seguir recto), y con eso cambia el camino entero del handwall.
- **Sugerencia:** filtrar el raw (EMA o promedio de N muestras) y usar dos umbrales (histéresis) por sensor.

### L-15 ⚪ Índice fuera de rango en `ln_lookup` y coeficientes ajenos
- **Ubicación:** `update_sensors_magics()` en `src/sensors.c:424-428` y `handle_robot_version()` en `src/config.c:41`.
- **Problema:**
  - `ln_index = (raw + c)/4` llega a 1024 (la tabla tiene 1024 entradas) cuando el raw satura y `c > 0`, y entonces lee fuera del arreglo.
  - Un UID desconocido (nuestro robot) carga los coeficientes del **ZoroBot3 "A"**. Esa curva no es la de nuestros sensores, y probablemente es la causa de raíz de que los mm no sean confiables (se amplía en el Paso 3).
- **Sugerencia:** limitar el índice. Agregar una versión `MUADDIB` con coeficientes propios o con una tabla medida.

### L-16 ⚪ `reset_encoder_avg()` no tiene efecto
- **Ubicación:** `src/encoders.c:96-103`.
- **Problema:** pone en cero los µm y los mm, pero no `left_total_ticks`/`right_total_ticks`. En el siguiente ms `update_encoder_readings()` los recalcula desde los ticks totales. No rompe nada, porque todo es relativo, pero engaña a quien lea el código.
- **Sugerencia:** resetear también los ticks totales, con la IRQ deshabilitada, o eliminar la función.

### L-17 🟡 La autoprueba de giro (botones 4/5) no reproduce el giro real
- **Ubicación:** `selftest_explore_turn()` en `src/main.c:317-323`.
- **Problema:**
  - No llama a `add_target_heading()`, así que en el tramo `end` el heading lock tira de vuelta hacia 0° y el resultado sale sesgado ~0,5°.
  - Arranca **desde parado**: el arco empieza acelerando y, por L-02, gira de más.
  - Usa la velocidad del menú (L-05).
- **Impacto:** es justamente la herramienta con la que el equipo ajusta los giros (`.docx` §6), y mide algo distinto de lo que pasa en el laberinto.
- **Sugerencia:** antes del giro, avanzar una celda a velocidad de exploración. Llamar a `add_target_heading()`. Forzar `SPEED_EXPLORE`. Reportar el ángulo final por LEDs o UART.

### L-18 ⚪ `MOVE_BACK_WALL` modificado quedó roto para floodfill
- **Ubicación:** `src/move.c:1226-1294`.
- **Problema:** se cambió a un 180° en la entrada de la celda, sin centrar y sin `set_starting_position()`. Floodfill lo usa (`src/floodfill.c:1215`) y espera el comportamiento original. El comentario dice que lo usa handwall, y ya no es así.
- **Sugerencia:** restaurar el original o borrar la rama. Corregir el comentario.

### L-19 ⚪ Esperas que no miran `is_race_started()`
- **Ubicación:** `keep_front_distance()` en `src/move.c:1617-1653`, `check_start_run()`, y `delay()` en `handwall_start()`.
- **Problema:** si se detiene con Play/Pause o por saturación, el hilo principal sigue bloqueado hasta ~1,75 s por cada llamada (el callejón hace dos) y continúa la secuencia. Los motores ya están deshabilitados, así que no es peligroso, pero el menú no responde.
- **Sugerencia:** agregar `is_race_started()` a esas condiciones.

### L-20 ⚪ El contador de microsegundos da la vuelta cada 25,6 s
- **Ubicación:** `get_us_counter()` en `src/delay.c`.
- **Problema:** `DWT_CYCCNT / 168` da la vuelta en 25,56 s y no en 2³² µs. Cada vuelta produce un intervalo IR erróneo, que puede arruinar una trama.
- **Sugerencia:** calcular la diferencia en ciclos (`now - last` en `uint32_t`) y recién después dividir.

### L-21 🟡 Floodfill, mapa y time-trial no sirven para el reglamento LNR
- **Ubicación:** `src/maze.c` (metas fijas 2×2 en el centro de 6×6 o 16×16), `src/floodfill.c` y `src/timetrial.c`.
- **Problema:**
  - En LNR la meta es **una** celda desconocida, la partida la elige el jurado y el laberinto es de 5×4 o 5×5.
  - Handwall no registra el mapa, así que no aprovecha la segunda pasada.
  - Elegir FLOODFILL o TIME_TRIAL por error en el menú lleva a un comportamiento inútil o peligroso.
- **Sugerencia:** para competir, deshabilitar esas opciones en el menú (o con un `#define`). A futuro, ver FR-N5 y FR-N6 del FSD.

---

## B. Rendimiento

### P-01 🟡 Aritmética `double` dentro de las ISR
- **Ubicación:**
  - `PI 3.1415` en `include/constants.h:11` (literal `double`).
  - `struct kpi_params` y `mpu_params` en `include/move.h:31-39` (`double`).
  - `MICROMETERS_PER_TICK` en `include/config.h:58`.
  - `ADC_LSB` y los alpha de los filtros.
- **Problema:** el Cortex-M4F solo tiene FPU de precisión simple, así que toda operación `double` se emula por software (decenas o cientos de ciclos). `control_loop()`, `update_encoder_readings()`, `mpu6500_update()`, `wrap_heading_angle()` (compara un `float` con `PI` `double`) y `update_battery_voltage()` corren a 1 kHz con varias de estas operaciones.
- **Sugerencia:** pasar las constantes a `float` (`3.14159265f`, sufijo `f`), cambiar los campos de las structs a `float` y compilar con `-Wdouble-promotion` para encontrar el resto.

### P-02 ⚪ `get_kinematics()` copia una struct entera unas 20 veces por ms
- **Ubicación:** `src/control.c:707-727` y `update_ideal_linear_speed()` en `src/control.c`; `mpu6500_update()` en `src/mpu6500.c`.
- **Problema:** la función devuelve `struct kinematics` por valor (~40 bytes), una copia por cada término del PID.
- **Sugerencia:** devolver `const struct kinematics *`, o tomar una copia local una sola vez al principio del lazo.

### P-03 🟡 Se borra un sector de flash cada vez que se arma un lado
- **Ubicación:** `menu_run_up()`, `menu_run_down()`, `menu_run_select_left_hand()` y `menu_run_select_right_hand()` en `src/menu_run.c`, que llaman a `menu_run_save_values()` → `eeprom_save()`.
- **Problema:** borrar el sector 11 (128 KB) tarda ~1–2 s, y en el F405 detiene la lectura de instrucciones desde flash. **Todas las ISR quedan congeladas**: botones, IR, LEDs y control. Además desgasta la flash (~10 k ciclos) en cada armado.
- **Impacto:** el robot no responde durante 1–2 s justo antes de largar.
- **Sugerencia:** no guardar al armar. Guardar el menú solo desde "Calibración → guardar EEPROM", o con una escritura diferida cuando cambió algo distinto del lado.

### P-04 ⚪ `mpu6500_read_register()` reinicia el SPI dos veces con `delay(100)`
- **Ubicación:** `src/mpu6500.c:84-101` y `setup_spi_*()` en `src/setup.c:462-480`.
- **Problema:** cada lectura de registro tarda ≥200 ms y resetea el periférico (ver L-13). En `DEBUG_ZONE` se llama en cada vuelta.
- **Sugerencia:** cambiar solo el prescaler (`SPI_CR1`) sin reset y quitar los `delay(100)`.

### P-05 ⚪ TIM2 corre a ~32 kHz, no a 16 kHz
- **Ubicación:** `setup_wall_sensor_manager()` en `src/setup.c:377`.
- **Problema:** el prescaler entero queda en 4, así que el timer cuenta a 84 MHz/5 = 16,8 MHz, y con 521 cuentas la interrupción sale a ≈32,2 kHz. Cada sensor se actualiza a ~2 kHz, pero se consume a 1 kHz. No es grave, pero son el doble de interrupciones de las necesarias y el comentario es incorrecto.
- **Sugerencia:** ajustar el período (~1049) si alcanza con 16 kHz, o corregir el comentario.

### P-06 ⚪ Escrituras al RGB (no montado) en lazos calientes
- **Ubicación:** `set_RGB_color()` dentro de `move_arc_turn()` (en cada iteración), `move_side()`, `handwall_loop()` y `handle_menu_run_values()`.
- **Sugerencia:** eliminarlas o envolverlas en un `#define HAS_RGB 0`.

---

## C. Estilo y mantenibilidad

| ID | Ubicación | Problema | Sugerencia |
|---|---|---|---|
| E-01 | `include/config.h:25-49`, `src/sensors.c:357-370`, `src/control.c:42-46`, `src/setup.c:373`, `src/move.c:1205-1224`, `MENU.md` | **Comentarios que contradicen el código**: 272/130 mm frente a 260/124; "FL ≥ 70 / FR ≥ 120" frente a 180/280; "~141/137 mm" frente a 130; "16 kHz" frente a 32 kHz; "handwall usa MOVE_BACK_WALL" (ya no). `MENU.md` todavía dice que tapar un sensor *inicia* la carrera (cambió en el PR #13). | Corregirlos. Un comentario erróneo es peor que ninguno. |
| E-02 | `src/menu_run.c:31-100` | Quedaron instrucciones de pegado ("Pegar esta función en menu_run.c reemplazando la original…") y hay un `#ifndef MMSIM_ENABLED` anidado y duplicado. | Reemplazarlas por una descripción breve y quitar el `#ifndef` duplicado. |
| E-03 | general | Indentación mezclada (2 y 4 espacios), estilos de llaves distintos y formato muy vertical (un argumento por línea en `move_back()`, `main.c`). Existe `.clang-format`, pero no se aplica. | Pasar `clang-format` a los archivos modificados por el equipo en un PR aparte, sin cambios de lógica. |
| E-04 | `src/control.c:777-858`, `src/move.c:938-958, 1826-1843`, `keep_z_angle()` | Mucho código comentado o muerto. `keep_z_angle()` habilita los motores (`gpio_set(GPIOB, GPIO15)`) de forma oculta. | Borrarlo (git conserva la historia). |
| E-05 | `include/buttons.h:16`, `include/sensors.h:94-95` | Funciones declaradas y nunca definidas: `get_start_btn`, `get_sensor_filtered`, `get_sensor_linearized`. `side_sensors_error` y `KPI_SIDE_SENSORS` en control quedan sin uso y se ponen en cero en cada tick. | Eliminarlas. |
| E-06 | `include/handwall.h:1`, `include/move.h:109`, `include/config.h:25` | Guard `HANDWALL_C` en un `.h`. Declaración adelantada de un enum (`enum speed_strategy;`), que no es C estándar. `/** Laberinto *//** Laberinto */` duplicado. | Corregirlos. |
| E-07 | `setup_adc2()` en `src/setup.c` | `sizeof(get_sensors())/sizeof(get_sensors()[0])` es el tamaño de un puntero (4). Funciona por casualidad porque hay 4 sensores. | Usar `get_sensors_num()`. |
| E-08 | `move.c`, `sensors.c`, `control.c` | Números mágicos dispersos: 90, 320, ±2 mm, 1500 ms, 150, 300 mm/s, 10 mm. | Nombrarlos como `#define` cerca de `config.h` y derivarlos de `CELL_DIMENSION` cuando sean geométricos (FR-M7). |
| E-09 | `include/constants.h:11` | `PI 3.1415` tiene poca precisión: 2·PI difiere en 2·10⁻⁴ rad por vuelta. | `#define PI 3.14159265f`. |
| E-10 | `src/rc5.c`, `include/rc5.h` | El módulo se llama "RC5", pero ya no decodifica RC5. `rc5_load_eeprom()` está vacía y `RC5_DATA_LENGTH` reserva 5 palabras sin uso. | Renombrarlo (`ir_capture`) cuando se toque. Documentar la reserva. |
| E-11 | `menu_run.c` (`modeRun`, `valueRun`, `lastBlinkMs`, `blinkState`), `calibrations.c`, `sensors.c` (`sensors_distance`, `side_sensors_error`), `config.c` (`_run_mode`) | Variables globales sin `static`. Hay un `side_sensors_error` global en `sensors.c` y otro `static` en `control.c`, lo que confunde. | Hacerlas `static` y exponer accesores. |
| E-12 | `src/move.c:59-63` | `kpi_explore[KPI_FRONT_DIAGONAL_SENSORS]` tiene `kp` y `ki` invertidos respecto de `kpi_run` (heredado; no se usa en exploración). | Igualarlos o eliminar la entrada. |

---

## D. Contraste con problemas conocidos (Paso 3)

Fuentes: `docs/Cambios_ZoroBot3_vs_MuadDib.docx` (§2–§8) y los commits del equipo desde el fork (`0235b39..b0b255d`, PRs #1–#15).

### D.1 Hallazgos nuevos

#### L-22 🔴 El offset del giróscopo desborda al guardarse en EEPROM
- **Ubicación:** `mpu6500_gyro_z_calibration()` en `src/mpu6500.c:299` (`eeprom_data[i] = offset_z[i] * 1000`) y `mpu6500_load_eeprom()` en `src/mpu6500.c:311`.
- **Problema:** el offset se guarda en LSB × 1000 dentro de un `int16_t`, y el máximo representable es ±32,7 LSB. En el MPU6500 eso equivale a **±1,0 dps** a 1000 dps y ±2,0 dps a 2000 dps. La hoja de datos indica una tolerancia inicial de ZRO de ±5 dps, así que es muy probable que el offset real (≈ 30–160 LSB) **desborde**. Convertir un `float` fuera de rango a `int16` además es comportamiento indefinido en C, y en la práctica el valor "da la vuelta". La rutina es una "copia fiel" de la del LSM6DSR, cuya sensibilidad (35 mdps/LSB) dejaba el valor justo dentro del rango.
- **Impacto:** justo después de calibrar, el offset está bien porque queda en RAM como `float`. **Después de reiniciar** se carga un bias equivocado (por ejemplo, 40 LSB se guardan y vuelven como −25,5 LSB, un error de ~2 dps). Entonces:
  - el heading medido deriva todo el tiempo y el heading lock gira el robot lentamente en las rectas;
  - los giros en el lugar y las autopruebas dan ángulos distintos según el tiempo transcurrido;
  - los giros en arco no lo notan tanto, porque el PID sigue la velocidad y el sesgo cambia poco en 0,3 s, pero el lock sí.
- **Cómo verificar:** comparar por UART el `Offset Z for full-scale …` que imprime la calibración con el que imprime el arranque siguiente.
- **Sugerencia:** guardar en LSB × 10 o × 100 (con ×100 entran ±327 LSB), o guardar el `float` en dos palabras. Validar el rango al cargar.

#### L-23 🟠 La distancia en mm "da la vuelta", queda vieja o satura
- **Ubicación:** `update_sensors_magics()` en `src/sensors.c:416-449`.
- **Problema:** esto explica los "valores saturados o inválidos" del `.docx` §5.
  1. `new_sensor_distance` se limita a ≥ 0 **antes** de sumar `ROBOT_*_LENGTH` y el offset calibrado. Con un offset negativo (que sale de la calibración cuando la curva sobreestima, como pasa con los coeficientes del ZoroBot3 "A"), una pared muy cerca da un valor negativo. Ese valor se guarda en `uint16_t` y se convierte en ≈ **65 500**: "lejísimos".
  2. Si `sensors_on <= sensors_off` (sin reflejo, o ruido), la distancia **no se actualiza** y queda el valor anterior.
  3. Si `raw + c < 4`, el índice es 0 y `ln = 1`, lo que da ≈ 2,5 m. Con `c = −50` (sensor lateral izquierdo, versión A), cualquier raw < 54 satura.
- **Impacto:** el caso 1 hace que, cuando el robot casi toca la pared, el seguidor la vea "inválida" (≥190 mm) y deje de corregir justo cuando más falta, y que `front_wall_detection()` dé falso pegado a la pared.
- **Sugerencia:** calcular en `int32`, aplicar el limitador al final (`0 … 1000`) y poner la distancia en "máximo" cuando no haya reflejo.

#### L-24 🟠 `WALL_LOSS_TO_SENSING_POINT_DISTANCE` nunca se calibró
- **Ubicación:** `include/config.h:52-56` ("Valor inicial estimado"), `move_straight()` en `src/move.c:1556-1557` y `run_straight()`.
- **Problema:** al perder una pared lateral, la posición se fuerza a "faltan 162 mm para el borde de celda". Ese valor depende de dónde apunta el haz del sensor lateral sobre la pared, que se mide en el robot y no se escala desde los 116 mm del ZoroBot3. Además, la pérdida ahora se detecta con umbral **raw**, que cambia con la distancia lateral (L-25): con el robot corrido, la pérdida se detecta antes o después.
- **Impacto:** cada error de X mm en este valor desplaza el **inicio del giro** X mm, y el giro termina X mm descentrado en el pasillo nuevo. Es una causa directa del síntoma principal del `.docx` ("se descentra y puede chocar").
- **Sugerencia:** medirlo. Avanzar a 600 mm/s junto a una pared que termina en un poste, registrar por UART el encoder en el momento de la pérdida y el encoder con el eje sobre el poste (marca en el piso). La diferencia es el valor buscado. Repetirlo con el robot ±10 mm corrido para ver la sensibilidad.

#### L-25 🟠 Los umbrales laterales no están en la misma posición relativa
- **Ubicación:** `src/sensors.c:565-566`. El PR #14 dice que ambos quedan en "~40 %".
- **Problema:** con las mediciones del `.docx`:
  - **izquierdo**: (220 − 135)/(300 − 135) ≈ **52 %**;
  - **derecho**: (340 − 230)/(510 − 230) ≈ **39 %**.
  El margen hasta "pared presente" es menor en el izquierdo. El raw cae aproximadamente con 1/d². Si el robot se corre ~20 mm lejos de la pared (89 → 109 mm desde el sensor), la lectura baja a ~0,67×: izquierdo 300 → ~200 (< 220, **apertura falsa**), derecho 510 → ~342 (justo en el límite).
- **Impacto:** con la deriva de L-01, L-07 y L-08, el robot "ve" una apertura que no existe y gira contra una pared. Siguiendo la izquierda es más sensible, y eso es una asimetría real entre izquierda y derecha, a diferencia de lo que concluye el `.docx` §7.
- **Sugerencia:** volver a medir con los sensores reparados, ubicar los umbrales cerca del 30–35 % (más cerca de "despejado"), sumar histéresis (L-14) y, mejor todavía, mantener el robot centrado (L-01, L-07).

#### L-26 🟡 El DLPF del MPU6500 a 41 Hz agrega ~6 ms de retardo
- **Ubicación:** `mpu6500_init()` en `src/mpu6500.c:218` (`REG_CONFIG = 0x03`), más el EMA por software (alpha 0,27).
- **Problema:** con DLPF_CFG = 3 el retardo del giróscopo es ≈ 5,9 ms según la hoja de datos. Los PID (`kpi_explore[KPI_ANGULAR]` kp 0,5 / ki 0,015) y los perfiles de giro vienen del ZoroBot3 con LSM6DSR, cuyo filtro "LP1 light" tiene mucho menos retardo. Con 6 ms de retardo en un lazo de 1 kHz baja el margen de fase: hay más seguimiento atrasado durante el arco (más windup, L-10) y más sobrepico al terminar.
- **Sugerencia:** probar DLPF_CFG = 1 (184 Hz, ~2,9 ms) o 0 (250 Hz, ~1 ms) y volver a ajustar kp/ki angular con la autoprueba corregida (L-17).

#### L-27 🟡 La corrección del sobregiro (PR #12) solo aplica a las autopruebas
- **Ubicación:** `move_inplace_angle()` en `src/move.c:2006-2039` (commit `b765967`).
- **Problema:**
  - La rampa de frenado se agregó a `move_inplace_angle()`, que solo usan los botones 1 y 2. En el laberinto (callejón) se usa `move_inplace_turn()`, que no cambió (L-11). El `.docx` §6 da el problema por resuelto, pero no lo está para el caso real.
  - El corte anticipado vale `ω²/(2α)` = 4²/(2·20) rad ≈ **22,9°**, y supone una desaceleración de 20 rad/s². Pero al terminar la prueba se llama a `set_race_started(false)`, que pone los motores en freno en corto, y el robot se detiene mucho más rápido. Es probable que ahora gire **de menos**.
  - El ángulo final no se informa, así que no se puede medir.
  - El "wrap" de `target_angle` tiene el signo invertido (`360 - target` en lugar de `target - 360`). Hoy no tiene efecto porque el ángulo es ≤ 180°.
- **Sugerencia:** frenar con una rampa de `ideal_angular_speed` (como la lineal), en lugar de cortar antes. Informar el ángulo final por UART o LEDs. Usar el mismo giro con lazo cerrado en el callejón.

#### L-28 🟡 Frenado en corto inmediato y aceleraciones altas
- **Ubicación:** `control_loop()` en `src/control.c:589-596` (freno en corto 3 s al terminar), `kinematics_settings[SPEED_EXPLORE]` (8000 mm/s²) y `turns_inplace` (350 rad/s²).
- **Problema:** con Play/Pause o al saturar, el robot pasa de 600 mm/s a freno en corto en 1 ms. Ese es un pico de par sobre piñón y corona, que el `.docx` §3 reporta como roto más de una vez. Las aceleraciones lineal y angular también son altas para el robot actual.
- **Sugerencia:** al detener, hacer una rampa a 0 (≈100–200 ms) antes del freno, o dejar el motor libre. Bajar la aceleración angular en el lugar (por ejemplo, a 100–150 rad/s²). Esto también reduce el riesgo de saturación falsa (L-11) y los picos de corriente (L-29). Los engranajes de nylon siguen siendo recomendables.

#### L-29 ⚪ Reinicios: no hay fuente de reset por software
- **Problema:** el firmware no tiene watchdog (IWDG/WWDG), no llama a `scb_reset_system()` y no define `hard_fault_handler`. El de libopencm3 es un lazo infinito: **congela**, no reinicia. Por lo tanto:
  - un **reinicio real** (se ve la barra de batería del arranque) tiene una causa **eléctrica**: brown-out por picos de corriente con la batería de 260 mAh (choques, frenado en corto, giros a 350 rad/s²), un conector flojo o el pin de reset;
  - un **robot congelado** (LEDs quietos, no responde) es una HardFault. Candidatos: `kinematics_settings[-1]` con la EEPROM borrada (L-05) y la corrupción de memoria al emparejar (L-06);
  - un **"pseudo-reinicio"**: desde el PR #15, una saturación termina la carrera y `menu_reset()` vuelve el menú a la pantalla de VELOCIDAD, desarmado. Visto desde afuera parece un reinicio. Lo mismo pasa con un Play/Pause espurio.
- **Sugerencia:** lo que propone el `.docx` §8. Leer `RCC_CSR` al arrancar (PORRST, BORRST, PINRST, SFTRST, IWDGRST, LPWRRST), mostrarlo en los LEDs durante 1 s y limpiar las banderas (`RCC_CSR_RMVF`). Agregar un `hard_fault_handler` que parpadee un patrón propio. Indicar con un LED distinto "fin por saturación".

### D.2 Problemas conocidos → causas en el código

| # | Problema conocido (fuente) | Estado según el equipo | Causas encontradas en el código | Comentario |
|---|---|---|---|---|
| K-1 | **Giros en arco imprecisos**: el robot se descentra y choca (`.docx` §6, §7) | Abierto, problema principal | **L-01** (84,8°, sale corto), **L-02** (el ángulo depende de v), **L-24** (inicio del giro mal referenciado), L-10 (windup), L-26 (retardo del giro), L-08 / L-09 / **L-22** (el heading lock corrige con error y desplaza), L-25 (aperturas falsas por el descentrado), L-17 (la herramienta de medición está sesgada) | El problema no es solo de parámetros: hay errores de modelo (L-01, L-02) y de referencia (L-24, L-22). Ajustar a mano sin corregir eso no va a converger. |
| K-2 | **Linealización de sensores**: los mm no son confiables (`.docx` §5, §7) | Abierto, se navega con raw | **L-15** (coeficientes del ZoroBot3 "A" + índice fuera de rango), **L-23** (wrap, valor viejo, saturación), calibración de un solo punto (`side/front_sensors_calibration` solo corrige el offset, no la pendiente), L-07 (el seguidor depende de los mm) | Sugerencia: medir raw a distancias conocidas (20–250 mm, cada 10 mm) por sensor y usar una tabla de interpolación raw→mm propia, o reajustar a, b, c. |
| K-3 | **Diferencia entre seguir izquierda y derecha**: "el código es simétrico" (`.docx` §7) | Se atribuye a la calibración | El código de navegación sí es simétrico, pero hay asimetrías efectivas: **L-25** (umbrales al 52 % y 39 %), **L-23** (un offset negativo hace "dar la vuelta" a un solo lado), L-15 (pendiente distinta por sensor), L-09 (el error se acumula con el sentido dominante de los giros) | Conclusión del `.docx` parcialmente correcta: la causa es de calibración, pero hay dos defectos de código que la amplifican. |
| K-4 | **Sobregiro en el giro en el lugar** (`.docx` §6, PR #12) | "Resuelto" | **L-27** (la corrección solo aplica a `move_inplace_angle`, posible subgiro ahora), L-11 (el callejón usa otro giro, a lazo abierto) | No está resuelto para el laberinto. |
| K-5 | **Callejón sin salida** (PR #14, `.docx` §2.1) | Implementado | **L-03** (no actualiza la posición: error de media celda; el centrado inicial no corre), L-11 (giro a lazo abierto, posible corte por saturación), L-19 | El ZoroBot3 original retrocedía contra la pared y fijaba la posición. Se perdió esa referencia. |
| K-6 | **Protección por choque** (PR #15, `.docx` §2.3) | Reactivada | L-11 (riesgo de disparo falso en giros en el lugar), L-28 (freno en corto), L-29 (pseudo-reinicio). FSD FR-C6: no hay recuperación | El equipo la había **comentado** en el WIP (`bfe237d`); conviene saber por qué antes de confiar en ella. Con R11 (15 s quieto) una recuperación salvaría la pasada. |
| K-7 | **Reinicios** (`.docx` §8) | Causa sin confirmar | **L-29** (sin reset por software: un reinicio real es eléctrico; un congelamiento es HardFault: L-05, L-06), P-03 (congelamiento de 1–2 s al armar, que puede confundirse) | Implementar el diagnóstico de `RCC_CSR`. |
| K-8 | **Engranajes rotos** (`.docx` §3) | Hardware (nylon) | **L-28** (freno en corto, 8000 mm/s², 350 rad/s²), choques sin recuperación | El firmware puede reducir los picos de par. |
| K-9 | **Sensor frontal con soldadura defectuosa** (`.docx` §3) | Reparado | **L-04** (los umbrales frontales no coinciden con las mediciones publicadas), L-25 | Los umbrales se fijaron en `bfe237d`, antes de la reparación. Hay que medir todo de nuevo. |
| K-10 | **Migración LSM6DSR → MPU6500** (`.docx` §3, PRs #2–#3) | Hecha | **L-22** (desborde del offset), **L-26** (DLPF), L-13 (SPI concurrente), P-04. El código del LSM6DSR se eliminó bien (sin duplicados) | `include/driver_mpu6500.h` (3429 líneas) no se incluye en ningún lado: código muerto. |
| K-11 | **Cristal de 24 MHz** (`.docx` §3) | Hecho | Verificado: PLLM = 24, PLLN = 336, PLLP = 2 → 168 MHz y PLLQ = 7 → 48 MHz. `SYSCLK_FREQUENCY_HZ` coincide. Sin problemas. | Solo están mal los comentarios del SPI ("PCLK1 = 36 MHz", "84 MHz/8"): SPI3 cuelga de APB1 a 42 MHz, así que da 5,25 MHz y 328 kHz. |
| K-12 | **Control IR, emparejamiento, Play/Pause** (PRs #1, #4–#9, #13) | Hecho (solo para pruebas) | **L-06** (desborde EEPROM, NUM_5), L-12 (armado por sensor sin tiempo mínimo y con prioridad sobre el menú), L-20 (wrap µs), P-03 (flash al armar). FSD FR-S1: falta el botón físico | — |
| K-13 | **Heading lock** "no alcanza cuando el error se acumula" (`.docx` §2.1, §6) | Abierto | **L-08** (sin re-referencia, desplazamiento lateral v·e/KP), **L-09** (+0,3 %), **L-22** (bias) | El lock es útil, pero hoy amplifica los errores de L-01 en lugar de cubrirlos. |

### D.3 Revisión por commit (PRs del equipo)

| Commit / PR | Cambio | Problemas introducidos o heredados |
|---|---|---|
| `8fc967b` (#1) | Mapeo IR en EEPROM | L-06 |
| `bfe237d` (#2) | Sincronización WIP: MPU6500, heading lock, seguidor de una pared, raw para navegación, celda 260, giro de exploración nuevo | L-01, L-07, L-08, L-04, L-25 (umbrales iniciales), L-22, L-26, L-13, L-24 (162 "estimado"), saturación **comentada**, E-01 (comentarios 272/130) |
| `54b2aee` (#2) | `DEBUG_ZONE` | P-04 / L-13 (`who_am_i` en lazo) |
| `8ca1246` (#3) | Quita el LSM6DSR | OK. Queda `driver_mpu6500.h` sin uso |
| `433ec09`, `960a797`, `145192b` (#4, #5) | Emparejamiento IR en el menú de calibración | L-06 (lo vuelve visible) |
| `8cdfc78` (#6) | Play/Pause inicia y detiene la carrera | OK (`set_race_started` desde una ISR es aceptable) |
| `afdb0f9` (#7) | PREV/NEXT eligen el lado | P-03 (guarda en flash en cada selección) |
| `046ae06` (#8) | Autopruebas con botones numéricos | L-17, L-05 (usa la velocidad del menú) |
| `b765967` (#12) | Rampa de frenado en `move_inplace_angle` | L-27 |
| `2d4eb84` (#13) | Inicio solo con Play/Pause | L-12 (el lado del sensor pisa al del menú), MENU.md desactualizado (E-01) |
| `c85660a` (#14) | U-turn con dos giros de 90° alineados; umbral derecho 300 → 340 | L-03, L-11, L-25 (la cuenta del "~40 %" no da), L-18 (`MOVE_BACK_WALL` queda incoherente) |
| `047f3e8` (#15) | Corte inmediato por saturación | L-28, L-29 (pseudo-reinicio), L-11 (sin recuperación) |

### D.4 Orden de corrección sugerido

Primero lo que corrige la referencia, después el modelo y por último el ajuste fino:

1. **L-22** (offset del giróscopo) y **L-06** (EEPROM). Son cambios chicos y de alto impacto. Recalibrar y guardar.
2. **L-05** (forzar `SPEED_EXPLORE`) y **L-21** (ocultar los algoritmos no aptos). Eliminan trampas de configuración.
3. **L-23** + **L-04** + **L-25**. Volver a medir los raw (la prueba T4 del FSD), corregir el cálculo de mm y fijar umbrales con histéresis (L-14).
4. **L-24**. Medir la distancia de pérdida de pared.
5. **L-01** + **L-02** + **L-10**. Recalcular el giro para 260 mm, pasar ω a curvatura × v y limitar el integral. Arreglar la autoprueba (**L-17**) y medir con ella (T3).
6. **L-03** + **L-11** + **L-27**. Callejón con retroceso contra la pared y giro a lazo cerrado.
7. **L-08** + **L-09** + **L-07**. Re-referenciar el heading lock, corregir el dt y unificar el objetivo del seguidor.
8. **L-28**, **L-29**, **P-03**. Frenado suave, diagnóstico de reinicios, no grabar al armar.
9. Rendimiento (P-01, P-02) y estilo (sección C), en PRs separados sin cambios de lógica.
