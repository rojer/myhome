#include "mgos.h"

#include "mgos_rpc.h"

#define HWS_GPIO 13

static void timer_cb(void *arg) {
  int sid = mgos_sys_config_get_sensor_id();
  bool hws = mgos_gpio_read(HWS_GPIO);
  LOG(LL_INFO, ("SID %d HWS %d", sid, hws));
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (sid >= 0 && hub_addr != NULL) {
    struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr) };
    mg_rpc_callf(mgos_rpc_get_global(),
        mg_mk_str("Sensor.ReportHWS"), NULL, NULL, &opts,
        "{sid: %d, ts: %lf, hws: %B}",
        sid, mg_time(), hws);
  }
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  mgos_gpio_set_mode(HWS_GPIO, MGOS_GPIO_MODE_INPUT);
  mgos_gpio_set_pull(HWS_GPIO, MGOS_GPIO_PULL_UP);
  mgos_set_timer(mgos_sys_config_get_interval() * 1000, MGOS_TIMER_REPEAT | MGOS_TIMER_RUN_NOW, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
