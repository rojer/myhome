#include "mgos.h"

#include "mgos_bh1750.h"
#include "mgos_bme680.h"
#include "mgos_lolin_button.h"
#include "mgos_rpc.h"
#include "mgos_veml7700.h"
#include "mgos_wifi.h"

#include "bme680.h"
#include "ds18b20.h"
#include "sht3x.h"
#include "si7005.h"

#include "srf05.h"
#include "ssd1306.h"

#define INVALID_VALUE -1000.0

int s_addr = 0;
static const char *s_st = NULL;
static float s_last_temp = INVALID_VALUE;
static void (*s_temp_read_func)(int addr, float *temp, float *rh) = NULL;

static const char *s_light_st = NULL;
static struct mgos_bh1750 *s_bh = NULL;

static void si7005_read(int addr, float *temp, float *rh) {
  *temp = si7005_read_temp();
  *rh = si7005_read_rh();
  (void) addr;
}

#define BTN_GPIO 0
#define LED_GPIO 2

float get_last_temp(void) {
  return s_last_temp;
}

static void read_temp_sensor(void) {
  int sid = mgos_sys_config_get_sensor_id();
  const char *hub_addr = mgos_sys_config_get_hub_address();
  float temp = INVALID_VALUE, rh = INVALID_VALUE;
  if (s_temp_read_func == NULL) return;
  s_temp_read_func(s_addr, &temp, &rh);
  s_last_temp = temp;
  bool have_temp = (temp != INVALID_VALUE);
  bool have_rh = (rh != INVALID_VALUE);
  LOG(LL_INFO, ("SID %d ST %s T %.2f RH %.2f", sid, s_st, temp, rh));
  if (sid < 0 || hub_addr == NULL) return;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  double now = mg_time();
  const char *name = mgos_sys_config_get_sensor_name();
  char buf1[50], buf2[50];
  char *name_temp = buf1, *name_rh = buf2;
  mg_asprintf(&name_temp, sizeof(buf1), "%s%sTemp", (name ? name : ""),
              (name ? " " : ""));
  mg_asprintf(&name_rh, sizeof(buf2), "%s%sRH", (name ? name : ""),
              (name ? " " : ""));
  if (have_temp && have_rh) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.DataMulti"), NULL,
                 NULL, &opts,
                 "{ts: %.3f, data: ["
                 "{sid: %d, subid: %d, st: %Q, name: %Q, v: %.3f}, "
                 "{sid: %d, subid: %d, st: %Q, name: %Q, v: %.3f}]}",
                 now, sid, 0, s_st, name_temp, temp, sid, 1, s_st, name_rh, rh);
  } else if (have_temp) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                 &opts,
                 "{sid: %d, subid: %d, st: %Q, name: %Q, ts: %f, v: %.3f}", sid,
                 0, s_st, name_temp, now, temp);
  } else if (have_rh) {
    mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                 &opts,
                 "{sid: %d, subid: %d, st: %Q, name: %Q, ts: %f, v: %.3f}", sid,
                 1, s_st, name_rh, now, rh);
  }
  if (name_temp != buf1) free(name_temp);
  if (name_rh != buf2) free(name_rh);
}

static void read_light_sensor(void) {
  int sid = mgos_sys_config_get_sensor_id();
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (s_bh == NULL) return;
  int raw;
  float lux = mgos_bh1750_read_lux(s_bh, &raw);
  double now = mg_time();
  LOG(LL_INFO, ("SID %d: %.2f lx (%d raw)", sid, lux, raw));
  if (lux < 0) return;
  if (sid < 0 || hub_addr == NULL) return;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  const char *name = mgos_sys_config_get_sensor_name();
  char buf1[50], buf2[50];
  char *name_light = buf1, *name_raw = buf2;
  mg_asprintf(&name_light, sizeof(buf1), "%s%sLight", (name ? name : ""),
              (name ? " " : ""));
  mg_asprintf(&name_raw, sizeof(buf2), "%s%sLight (raw)", (name ? name : ""),
              (name ? " " : ""));
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.DataMulti"), NULL, NULL,
               &opts,
               "{ts: %.3f, data: ["
               "{sid: %d, subid: %d, st: %Q, name: %Q, v: %.3f}, "
               "{sid: %d, subid: %d, st: %Q, name: %Q, v: %d}]}",
               now, sid, 50, s_light_st, name_light, lux, sid, 51, s_light_st,
               name_raw, raw);
  if (name_light != buf1) free(name_light);
  if (name_raw != buf2) free(name_raw);
}

static void read_sensors(void) {
  mgos_gpio_toggle(LED_GPIO);
  read_temp_sensor();
  read_light_sensor();
  mgos_gpio_toggle(LED_GPIO);
}

static void sensor_timer_cb(void *arg UNUSED_ARG) {
  read_sensors();
}

static void btn_cb(int pin UNUSED_ARG, void *arg UNUSED_ARG) {
  read_sensors();
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

  struct mgos_ssd1306 *oled = mgos_ssd1306_get_global();
  if (oled != NULL) {
    mgos_ssd1306_printf_color(oled, 0, 0, SSD1306_COLOR_WHITE,
                              SSD1306_COLOR_BLACK, "SID:%d",
                              mgos_sys_config_get_sensor_id());
    mgos_ssd1306_printf_color(oled, 0, 9, SSD1306_COLOR_WHITE,
                              SSD1306_COLOR_BLACK, "T:  %.2f",
                              out->temp.signal);
    mgos_ssd1306_printf_color(oled, 0, 18, SSD1306_COLOR_WHITE,
                              SSD1306_COLOR_BLACK, "RH: %.2f", out->rh.signal);
    if (out->iaq.accuracy == 3) {
      mgos_ssd1306_printf_color(oled, 0, 27, SSD1306_COLOR_WHITE,
                                SSD1306_COLOR_BLACK, "IAQ:%.2f",
                                out->iaq.signal);
    } else {
      mgos_ssd1306_printf_color(oled, 0, 27, SSD1306_COLOR_WHITE,
                                SSD1306_COLOR_BLACK, "IAQ:?(%d)",
                                out->iaq.accuracy);
    }
    mgos_ssd1306_refresh(oled, false /* force */);
  }

  if (mgos_uptime() - s_bme680_last_reported < mgos_sys_config_get_interval()) {
    return;
  }
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

static void lolin_button_handler(int ev, void *ev_data, void *ud UNUSED_ARG) {
  const struct mgos_lolin_button_status *bs =
      (const struct mgos_lolin_button_status *) ev_data;
  const char *bn = NULL;
  switch (ev) {
    case MGOS_EV_LOLIN_BUTTON_A:
      bn = "A";
      break;
    case MGOS_EV_LOLIN_BUTTON_B:
      bn = "B";
      break;
    default:
      return;
  }
  const char *sn = NULL;
  switch (bs->state) {
    case MGOS_LOLIN_BUTTON_RELEASE:
      sn = "released";
      break;
    case MGOS_LOLIN_BUTTON_PRESS:
      sn = "pressed";
      break;
    case MGOS_LOLIN_BUTTON_DOUBLE_PRESS:
      sn = "double-pressed";
      break;
    case MGOS_LOLIN_BUTTON_LONG_PRESS:
      sn = "long-pressed";
      break;
    case MGOS_LOLIN_BUTTON_HOLD:
      sn = "held";
      break;
  }
  LOG(LL_INFO, ("Button %s %s", bn, sn));
}

struct mgos_veml7700 *s_veml7700 = NULL;

void veml7700_timer(void *arg UNUSED_ARG) {
  float lux_als = mgos_veml7700_read_lux_als(s_veml7700, true /* adjust */);
  float lux_white = mgos_veml7700_read_lux_white(s_veml7700, true /* adjust */);
  if (lux_als >= 0 && lux_white >= 0) {
    LOG(LL_INFO, ("%.3f lux ALS %.3f lux white", lux_als, lux_white));
  }
}

static void get_lux_handler(struct mg_rpc_request_info *ri,
                            void *cb_arg UNUSED_ARG,
                            struct mg_rpc_frame_info *fi UNUSED_ARG,
                            struct mg_str args UNUSED_ARG) {
  float lux_als = mgos_veml7700_read_lux_als(s_veml7700, true /* adjust */);
  float lux_white = mgos_veml7700_read_lux_white(s_veml7700, true /* adjust */);
  mg_rpc_send_responsef(ri, "{lux_als: %.3f, lux_white: %.3f}", lux_als,
                        lux_white);
}

static void srf05_timer_cb(void *arg UNUSED_ARG) {
  int sid = mgos_sys_config_get_sensor_id();
  const char *hub_addr = mgos_sys_config_get_hub_address();
  float dist = srf05_get_avg();
  double now = mg_time();
  LOG(LL_INFO, ("SID %d: %.3f m (last %.3f m) RSSI %d", sid, dist,
                srf05_get_last(), mgos_wifi_sta_get_rssi()));
  if (dist <= 0) return;
  if (sid < 0 || hub_addr == NULL) return;
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  const char *name = mgos_sys_config_get_sensor_name();
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
               &opts, "{sid: %d, subid: %d, st: %Q, name: %Q, ts: %f, v: %.3f}",
               sid, 30, "SRF05", (name ?: ""), now, dist);
}

enum mgos_app_init_result mgos_app_init(void) {
  int bme680_addr = mgos_sys_config_get_bme680_i2c_addr();
  const char *st = mgos_sys_config_get_sensor_type();
  if (st == NULL) {
    LOG(LL_ERROR, ("Detecting sensors"));
    if (si7005_probe()) {
      st = "Si7005";
    } else if (sht3x_probe(&s_addr)) {
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
      s_temp_read_func = si7005_read;
    } else {
      LOG(LL_ERROR, ("Si7005 sensor not found"));
    }
  } else if (strcmp(st, "SHT3x") == 0) {
    if (sht3x_probe(&s_addr)) {
      LOG(LL_INFO, ("SHT3x sensor found @ %#02x", s_addr));
      s_temp_read_func = sht3x_read;
    } else {
      LOG(LL_ERROR, ("SHT31 sensor not found"));
    }
  } else if (strcmp(st, "DS18B20") == 0) {
    if (ds18b20_probe()) {
      LOG(LL_INFO, ("DS18B20 sensor found"));
      s_temp_read_func = ds18b20_read;
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

  uint8_t bh1750_addr = mgos_bh1750_detect();
  if (bh1750_addr != 0) {
    s_light_st = "BH1750";
    LOG(LL_INFO, ("Found BH1750 sensor at %#x", bh1750_addr));
    s_bh = mgos_bh1750_create(bh1750_addr);
    mgos_bh1750_set_config(s_bh, MGOS_BH1750_MODE_CONT_HIGH_RES_2,
                           mgos_sys_config_get_bh1750_mtime());
  }

  if (s_temp_read_func != NULL || s_bh != NULL) {
    s_st = st;
    mgos_set_timer(mgos_sys_config_get_interval() * 1000, MGOS_TIMER_REPEAT,
                   sensor_timer_cb, NULL);
    mgos_gpio_set_button_handler(BTN_GPIO, MGOS_GPIO_PULL_UP,
                                 MGOS_GPIO_INT_EDGE_NEG, 20, btn_cb, NULL);
  }
  struct mgos_ssd1306 *oled = mgos_ssd1306_get_global();
  mgos_ssd1306_start(oled);
  mgos_ssd1306_clear(oled);
  mgos_ssd1306_refresh(oled, true /* force */);
  mgos_event_add_group_handler(MGOS_EV_LOLIN_BUTTON_BASE, lolin_button_handler,
                               NULL);

  if (srf05_init(mgos_sys_config_get_sensor_id(),
                 mgos_sys_config_get_srf05_trig_pin(),
                 mgos_sys_config_get_srf05_echo_pin())) {
    mgos_set_timer(mgos_sys_config_get_interval() * 1000, MGOS_TIMER_REPEAT,
                   srf05_timer_cb, NULL);
  }

  if (mgos_veml7700_detect(mgos_i2c_get_bus(0))) {
    s_veml7700 = mgos_veml7700_create(mgos_i2c_get_bus(0));
    bool res = mgos_veml7700_set_cfg(
        s_veml7700, MGOS_VEML7700_CFG_ALS_IT_100 | MGOS_VEML7700_CFG_ALS_GAIN_1,
        MGOS_VEML7700_PSM_0);
    LOG(LL_INFO, ("Detected VEML7700, config %d", res));
    mgos_set_timer(1000, MGOS_TIMER_REPEAT, veml7700_timer, NULL);
    mg_rpc_add_handler(mgos_rpc_get_global(), "Sensor.GetLux", "",
                       get_lux_handler, NULL);
  }

  return MGOS_APP_INIT_SUCCESS;
}
