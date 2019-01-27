#include "mgos.h"

#include "mgos_rpc.h"

#include "ds18b20.h"
#include "sht3x.h"
#include "si7005.h"

#define INVALID_VALUE -1000.0

static const char *s_st = NULL;
static void (*s_read_func)(float *temp, float *rh) = NULL;

static void si7005_read(float *temp, float *rh) {
  *temp = si7005_read_temp();
  *rh = si7005_read_rh();
}

#define BTN_GPIO 0
#define LED_GPIO 2

static void read_sensor(void) {
  int sid = mgos_sys_config_get_sensor_id();
  float temp = INVALID_VALUE, rh = INVALID_VALUE;
  mgos_gpio_toggle(LED_GPIO);
  s_read_func(&temp, &rh);
  bool have_temp = (temp != INVALID_VALUE);
  bool have_rh = (rh != INVALID_VALUE);
  LOG(LL_INFO, ("SID %d ST %s T %.2f RH %.2f", sid, s_st, temp, rh));
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (sid < 0 || hub_addr == NULL) return;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  double now = mg_time();
  if (have_temp && have_rh) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.ReportTemp"), NULL,
                 NULL, &opts, "{sid: %d, st: %Q, ts: %lf, temp: %lf, rh: %lf}",
                 sid, s_st, now, temp, rh);
  } else if (have_temp) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.ReportTemp"), NULL,
                 NULL, &opts, "{sid: %d, st: %Q, ts: %lf, temp: %lf}", sid,
                 s_st, now, temp);
  } else if (have_rh) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.ReportTemp"), NULL,
                 NULL, &opts, "{sid: %d, st: %Q, ts: %lf, rh: %lf}", sid, s_st,
                 now, rh);
  }
  mgos_gpio_toggle(LED_GPIO);
}

static void sensor_timer_cb(void *arg) {
  read_sensor();
  (void) arg;
}

static void btn_cb(int pin, void *arg) {
  read_sensor();
  (void) pin;
  (void) arg;
}

// Workaround for https://github.com/cesanta/mongoose-os/issues/468
static void set_timer(void *arg) {
  mgos_set_timer(1000, 0, sensor_timer_cb, arg);
}

static void time_change_cb(int ev, void *evd, void *arg) {
  mgos_invoke_cb(set_timer, arg, false);
  (void) ev;
  (void) evd;
}

enum mgos_app_init_result mgos_app_init(void) {
  const char *st = mgos_sys_config_get_sensor_type();
  if (st == NULL) {
    LOG(LL_ERROR, ("Detecting sensors"));
    if (si7005_probe()) {
      st = "Si7005";
    } else if (sht3x_probe()) {
      st = "SHT3x";
    } else {
      LOG(LL_ERROR, ("No known sensors detected"));
      st = "";
    }
  }
  if (strcmp(st, "Si7005") == 0) {
    if (si7005_probe()) {
      LOG(LL_INFO, ("Si7005 sensor found"));
      si7005_set_heater(false);
      s_read_func = si7005_read;
    } else {
      LOG(LL_INFO, ("Si7005 sensor not found"));
    }
  } else if (strcmp(st, "SHT3x") == 0) {
    if (sht3x_probe()) {
      LOG(LL_INFO, ("SHT3x sensor found"));
      s_read_func = sht3x_read;
    } else {
      LOG(LL_INFO, ("SHT31 sensor not found"));
    }
  } else if (strcmp(st, "DS18B20") == 0) {
    if (ds18b20_probe()) {
      LOG(LL_INFO, ("DS18B20 sensor found"));
      s_read_func = ds18b20_read;
    } else {
      LOG(LL_INFO, ("DS18B20 sensor not found"));
    }
  } else if (*st == '\0') {
    // Nothing
  } else {
    LOG(LL_ERROR, ("Unknown sensor type '%s'", st));
  }
  if (s_read_func != NULL) {
    s_st = st;
    mgos_set_timer(mgos_sys_config_get_interval() * 1000, MGOS_TIMER_REPEAT,
                   sensor_timer_cb, NULL);
    mgos_event_add_handler(MGOS_EVENT_TIME_CHANGED, time_change_cb, NULL);
    mgos_gpio_set_button_handler(BTN_GPIO, MGOS_GPIO_PULL_UP,
                                 MGOS_GPIO_INT_EDGE_NEG, 20, btn_cb, NULL);
  }
  return MGOS_APP_INIT_SUCCESS;
}
