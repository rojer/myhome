/*
 * Copyright (c) Deomid "rojer" Ryabkov
 * All rights reserved
 */

#include "srf05.h"

#include "mgos.h"

#define INVALID_VALUE -1000.0

#define NUM_READINGS 8
static int s_idx = 0;
static float s_readings[NUM_READINGS] = {0};
static float s_last = 0.0;
static int64_t s_start = 0, s_end = 0;

extern float get_last_temp(void);

static IRAM void srf05_echo_int_handler(int pin, void *arg UNUSED_ARG) {
  if (mgos_gpio_read(pin)) {
    s_start = mgos_uptime_micros();
  } else {
    s_end = mgos_uptime_micros();
  }
}

float srf05_get_last(void) {
  return s_last;
}

float srf05_get_avg(void) {
  int i = 0;
  float sum = 0;
  while (i < NUM_READINGS) {
    float v = s_readings[i];
    if (v <= 0) break;
    sum += v;
    i++;
  }
  return (i > 0 ? sum / i : 0);
}

static void srf05_timer_cb(void *arg) {
  float temp = get_last_temp();
  if (temp == INVALID_VALUE) {
    temp = mgos_sys_config_get_srf05_temp();
  }
  float etime = 0, speed = 0, dist = 0;
  if (s_end > 0) {
    etime = (s_end - s_start) / 1000000.0f;
    speed = 331.3f + 0.606f * temp;
    dist = etime * speed / 2.0f;
    s_last = dist;
  } else if (s_start > 0) {  // Sensor missing.
    s_last = 0.0;
  }
  s_readings[s_idx++] = s_last;
  s_idx %= NUM_READINGS;
  LOG(LL_DEBUG, ("Etime %f s, t %.2f, sp %.2f, dist %.2f m, avg %.2f m", etime,
                 temp, speed, srf05_get_last(), srf05_get_avg()));
  s_start = s_end = 0;
  int trig_pin = (intptr_t) arg;
  mgos_gpio_write(trig_pin, 1);
  mgos_usleep(10);
  mgos_gpio_write(trig_pin, 0);
}

bool srf05_init(int sid, int trig_pin, int echo_pin) {
  if (trig_pin < 0 || echo_pin < 0) return false;
  LOG(LL_INFO, ("SRF05, TRIG:%d ECHO:%d", trig_pin, echo_pin));
  mgos_gpio_setup_output(trig_pin, 0);
  mgos_gpio_setup_input(echo_pin, MGOS_GPIO_PULL_DOWN);
  mgos_gpio_set_int_handler_isr(echo_pin, MGOS_GPIO_INT_EDGE_ANY,
                                srf05_echo_int_handler, NULL);
  mgos_gpio_enable_int(echo_pin);
  mgos_set_timer(1000, MGOS_TIMER_REPEAT, srf05_timer_cb,
                 (void *) (intptr_t) trig_pin);
  return true;
}
