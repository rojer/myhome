/*
 * Copyright (c) Deomid "rojer" Ryabkov
 * All rights reserved
 */

#include "mgos.h"

static int64_t s_start = 0, s_end = 0;

static IRAM void srf05_echo_int_handler(int pin, void *arg) {
  if (mgos_gpio_read(pin)) {
    s_start = mgos_uptime_micros();
  } else {
    s_end = mgos_uptime_micros();
  }
  (void) arg;
}

static void srf05_timer_cb(void *arg) {
  if (s_end > 0) {
    float temp = 20.0;
    float etime = (s_end - s_start) / 1000000.0f;
    float speed = 331.3 + 0.606 * temp;
    float dist = etime * speed / 2;

    LOG(LL_INFO, ("Etime %f s, sp %f, dist %f m", etime, speed, dist));
  }
  s_start = s_end = 0;
  int trig_pin = (intptr_t) arg;
  // LOG(LL_INFO, ("Start %d", echo_pin));
  mgos_gpio_write(trig_pin, 1);
  mgos_usleep(10);
  mgos_gpio_write(trig_pin, 0);
}

bool srf05_init(int sid, int trig_pin, int echo_pin) {
  if (trig_pin < 0 || echo_pin < 0) return false;
  mgos_gpio_setup_output(trig_pin, 0);
  mgos_gpio_setup_input(echo_pin, MGOS_GPIO_PULL_DOWN);
  mgos_gpio_set_int_handler_isr(echo_pin, MGOS_GPIO_INT_EDGE_ANY,
                                srf05_echo_int_handler, NULL);
  mgos_gpio_enable_int(echo_pin);
  mgos_set_timer(1000, MGOS_TIMER_REPEAT, srf05_timer_cb,
                 (void *) (intptr_t) trig_pin);
  return true;
}
