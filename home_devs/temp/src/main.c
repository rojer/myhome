#include "mgos.h"

#include "mgos_rpc.h"

#include "si7005.h"

static void temp_timer_cb(void *arg) {
  int sid = mgos_sys_config_get_sensor_id();
  float temp = si7005_read_temp();
  float rh = si7005_read_rh();
  LOG(LL_INFO, ("SID %d T %.2f RH %.2f", sid, temp, rh));
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (sid >= 0 && hub_addr != NULL && temp != SI7005_INVALID_VALUE) {
    struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr) };
    mg_rpc_callf(mgos_rpc_get_global(),
        mg_mk_str("Sensor.ReportTemp"), NULL, NULL, &opts,
        "{sid: %d, ts: %lf, temp: %lf, rh: %lf}",
        sid, mg_time(), temp, rh);
  }
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  if (si7005_probe()) {
    LOG(LL_INFO, ("Si7005 sensor found"));
    si7005_set_heater(false);
  } else {
    LOG(LL_WARN, ("Failed to init temp sensor"));
  }
  mgos_set_timer(mgos_sys_config_get_interval() * 1000, MGOS_TIMER_REPEAT | MGOS_TIMER_RUN_NOW, temp_timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
