#include "common/cs_dbg.h"

#include "mgos.h"
#include "mgos_app.h"
#include "mgos_gpio.h"
#include "mgos_sys_config.h"
#include "mgos_time.h"
#include "mgos_timers.h"
#include "mgos_wifi.h"

#include "hub_control.hpp"
#include "hub_data.hpp"

static int s_sl_gpio = -1;

static void blink_off(void *arg) {
  mgos_gpio_write((intptr_t) arg, 0);
}

static void status_timer_cb(void *arg) {
  double now = mg_time();
  int sys_sid = mgos_sys_config_get_hub_sys_sid();
  if (s_sl_gpio >= 0) {
    mgos_gpio_write(s_sl_gpio, 1);
    mgos_set_timer(100, 0, blink_off, (void *) (intptr_t) s_sl_gpio);
  }
  bool heater_on;
  double last_heater_action_ts;
  if (HubControlGetHeaterStatus(&heater_on, &last_heater_action_ts)) {
    int ths = (last_heater_action_ts > 0 ? now - last_heater_action_ts : -1);
    LOG(LL_INFO,
        ("Heater %s (last action %d ago)", (heater_on ? "on" : "off"), ths));
    HubControlReportOutputs();
  }
  report_to_server(sys_sid, UPTIME_SUBID, now, mgos_uptime());
  report_to_server(sys_sid, HEAP_FREE_SUBID, now, mgos_get_free_heap_size());

  {
    static double s_last_connected = 0;
    double now_uptime = mgos_uptime();
    enum mgos_wifi_status wifi_st = mgos_wifi_get_status();
    if (wifi_st == MGOS_WIFI_IP_ACQUIRED) {
      s_last_connected = now_uptime;
    } else if (now - s_last_connected > 300) {
      LOG(LL_ERROR, ("Disconnected for 5 minutes, deglucking"));
      mgos_system_restart_after(500);
    }
  }
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  enum mgos_app_init_result res = MGOS_APP_INIT_ERROR;

  if (!hub_data_init()) {
    LOG(LL_ERROR, ("Data module init failed"));
    goto out;
  }

  if (!HubControlInit()) {
    LOG(LL_ERROR, ("Control module init failed"));
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
