#include "common/cs_dbg.h"

#include "mgos_app.h"
#include "mgos_gpio.h"
#include "mgos_sys_config.h"
#include "mgos_timers.h"

#include "hub_heater.h"
#include "hub_light.h"

static int s_sl_gpio = -1;

static void blink_off(void *arg) {
  mgos_gpio_write((int) arg, 0);
}

static void status_timer_cb(void *arg) {
  if (s_sl_gpio >= 0) {
    mgos_gpio_write(s_sl_gpio, 1);
    mgos_set_timer(100, 0, blink_off, (void *) s_sl_gpio);
  }
  bool sensor_ok, lights_on, heater_on;
  hub_light_get_status(&sensor_ok, &lights_on);
  hub_heater_get_status(&heater_on);
  LOG(LL_INFO,
      ("Light sensor %s, lights %s; heater %s", (sensor_ok ? "ok" : "error"),
       (lights_on ? "on" : "off"), (heater_on ? "on" : "off")));
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  enum mgos_app_init_result res = MGOS_APP_INIT_ERROR;

  if (!hub_light_init()) {
    LOG(LL_ERROR, ("Light module init failed"));
    goto out;
  }

  if (!hub_heater_init()) {
    LOG(LL_ERROR, ("Heater module init failed"));
    goto out;
  }

  if (mgos_sys_config_get_hub_status_interval() > 0) {
    s_sl_gpio = mgos_sys_config_get_hub_status_led_gpio();
    if (s_sl_gpio >= 0) {
      mgos_gpio_write(s_sl_gpio, 0);
      mgos_gpio_set_mode(s_sl_gpio, MGOS_GPIO_MODE_OUTPUT);
    }
    mgos_set_timer(mgos_sys_config_get_hub_status_interval() * 1000,
                   MGOS_TIMER_REPEAT, status_timer_cb, NULL);
  }

  res = MGOS_APP_INIT_SUCCESS;

out:
  return res;
}
