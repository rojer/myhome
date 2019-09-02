#include "mgos.h"

#include "mgos_bme680.h"
#include "mgos_rpc.h"

#include "bme680.h"
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
  if (sid < 0) return;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  double now = mg_time();
  const char *name = mgos_sys_config_get_sensor_name();
  if (name == NULL) name = "";
  if (have_temp && have_rh) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.ReportTemp"), NULL,
                 NULL, &opts,
                 "{sid: %d, st: %Q, name: %Q, ts: %lf, temp: %lf, rh: %lf}",
                 sid, s_st, name, now, temp, rh);
  } else if (have_temp) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.ReportTemp"), NULL,
                 NULL, &opts, "{sid: %d, st: %Q, name: %Q, ts: %lf, temp: %lf}",
                 sid, s_st, name, now, temp);
  } else if (have_rh) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.ReportTemp"), NULL,
                 NULL, &opts, "{sid: %d, st: %Q, name: %Q, ts: %lf, rh: %lf}",
                 sid, s_st, name, now, rh);
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

bool bme680_probe(int addr) {
  struct bme680_dev dev;
  return mgos_bme68_init_dev_i2c(&dev, mgos_sys_config_get_bme680_i2c_bus(),
                                 addr) == 0;
}

static double s_bme680_last_reported = 0;

static const char *bme68_sensor_names[] = {
    "",   "IAQ",      "Static IAQ",        "CO2",        "VOC",
    "",   "Raw Temp", "Pressure",          "Raw RH",     "Raw Gas Resistance",
    "",   "",         "Gas stabilization", "Gas run-in", "Temp",
    "RH",
};

static void bme680_output_cb(int ev, void *ev_data, void *arg) {
  const struct mgos_bsec_output *out = (struct mgos_bsec_output *) ev_data;
  float ps_kpa = out->ps.signal / 1000.0f;
  float ps_mmhg = out->ps.signal / 133.322f;
  if (out->iaq.time_stamp > 0) {
    LOG(LL_INFO, ("IAQ %.2f (acc %d) T %.2f RH %.2f P %.2f kPa (%.2f mmHg)",
                  out->iaq.signal, out->iaq.accuracy, out->temp.signal,
                  out->rh.signal, ps_kpa, ps_mmhg));
  } else {
    LOG(LL_INFO, ("T %.2f RH %.2f P %.2f kPa (%.2f mmHg)", out->temp.signal,
                  out->rh.signal, ps_kpa, ps_mmhg));
  }
  if (mgos_uptime() - s_bme680_last_reported < mgos_sys_config_get_interval())
    return;
  int sid = mgos_sys_config_get_sensor_id();
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (sid < 0) return;
  LOG(LL_INFO, ("Reporting"));
  double now = mg_time();
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  const char *name = mgos_sys_config_get_sensor_name();
  for (uint8_t i = 0; i < out->num_outputs; i++) {
    char buf[50];
    char *sn = buf;
    const bsec_output_t *o = &out->outputs[i];
    const char *subn = (o->sensor_id < ARRAY_SIZE(bme68_sensor_names)
                            ? bme68_sensor_names[o->sensor_id]
                            : "");
    mg_asprintf(&sn, sizeof(buf), "%s%s%s", (name ? name : ""),
                (name ? " " : ""), subn);
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                 &opts, "{sid: %d, subid: %d, name: %Q, ts: %f, v: %.2f}", sid,
                 o->sensor_id, sn, now, o->signal);
    if (o->sensor_id == BSEC_OUTPUT_IAQ) {
      if (sn != buf) free(sn);
      sn = buf;
      subn = "IAQ accuracy";
      mg_asprintf(&sn, sizeof(buf), "%s%s%s", (name ? name : ""),
                  (name ? " " : ""), subn);
      mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                   &opts, "{sid: %d, subid: %d, name: %Q, ts: %f, v: %d}", sid,
                   o->sensor_id + 100, sn, now, o->accuracy);
    }
    if (sn != buf) free(sn);
  }
  s_bme680_last_reported = mgos_uptime();
  (void) ev;
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  int bme680_addr = mgos_sys_config_get_bme680_i2c_addr();
  const char *st = mgos_sys_config_get_sensor_type();
  if (st == NULL) {
    LOG(LL_ERROR, ("Detecting sensors"));
    if (si7005_probe()) {
      st = "Si7005";
    } else if (sht3x_probe()) {
      st = "SHT3x";
    } else if (bme680_probe(BME680_I2C_ADDR_PRIMARY)) {
      st = "BME680";
      bme680_addr = BME680_I2C_ADDR_PRIMARY;
    } else if (bme680_probe(BME680_I2C_ADDR_SECONDARY)) {
      st = "BME680";
      bme680_addr = BME680_I2C_ADDR_SECONDARY;
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
      LOG(LL_ERROR, ("Si7005 sensor not found"));
    }
  } else if (strcmp(st, "SHT3x") == 0) {
    if (sht3x_probe()) {
      LOG(LL_INFO, ("SHT3x sensor found"));
      s_read_func = sht3x_read;
    } else {
      LOG(LL_ERROR, ("SHT31 sensor not found"));
    }
  } else if (strcmp(st, "DS18B20") == 0) {
    if (ds18b20_probe()) {
      LOG(LL_INFO, ("DS18B20 sensor found"));
      s_read_func = ds18b20_read;
    } else {
      LOG(LL_ERROR, ("DS18B20 sensor not found"));
    }
  } else if (strcmp(st, "BME680") == 0) {
    if (bme680_probe(bme680_addr)) {
      LOG(LL_INFO, ("BME680 sensor found (0x%x)", bme680_addr));
      struct mgos_config_bme680 cfg = *mgos_sys_config_get_bme680();
      cfg.enable = true;
      cfg.i2c_addr = bme680_addr;
      if (!mgos_bme680_init_cfg(&cfg)) {
        LOG(LL_ERROR, ("BME680 init failed"));
      }
      mgos_event_add_handler(MGOS_EV_BME680_BSEC_OUTPUT, bme680_output_cb,
                             NULL);
    } else {
      LOG(LL_ERROR, ("BME680 sensor not found"));
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
