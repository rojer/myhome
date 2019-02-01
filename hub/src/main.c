#include "common/cs_dbg.h"

#include "mgos.h"
#include "mgos_app.h"
#include "mgos_gpio.h"
#include "mgos_sys_config.h"
#include "mgos_timers.h"

#include "hub.h"
#include "hub_heater.h"
#include "hub_light.h"

static int s_sl_gpio = -1;

static void blink_off(void *arg) {
  mgos_gpio_write((int) arg, 0);
}

static void status_timer_cb(void *arg) {
  double now = mg_time();
  int ctl_sid = mgos_sys_config_get_hub_ctl_sid();
  int sys_sid = mgos_sys_config_get_hub_sys_sid();
  if (s_sl_gpio >= 0) {
    mgos_gpio_write(s_sl_gpio, 1);
    mgos_set_timer(100, 0, blink_off, (void *) s_sl_gpio);
  }
  bool sensor_ok, lights_on;
  if (hub_light_get_status(&sensor_ok, &lights_on)) {
    LOG(LL_INFO, ("Light sensor %s, lights %s", (sensor_ok ? "ok" : "error"),
                  (lights_on ? "on" : "off")));
    report_to_server(ctl_sid, LIGHTS_SUBID, now, lights_on);
  }
  bool heater_on;
  double last_heater_action_ts;
  if (hub_heater_get_status(&heater_on, &last_heater_action_ts)) {
    int ths = (last_heater_action_ts > 0 ? now - last_heater_action_ts : -1);
    LOG(LL_INFO,
        ("Heater %s (last action %d ago)", (heater_on ? "on" : "off"), ths));
    report_to_server(ctl_sid, HEATER_SUBID, now, heater_on);
  }
  report_to_server(sys_sid, UPTIME_SUBID, now, mgos_uptime());
  report_to_server(sys_sid, HEAP_FREE_SUBID, now, mgos_get_free_heap_size());
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
