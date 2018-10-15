#include "hub_heater.h"

#include "common/cs_dbg.h"

#include "mgos_gpio.h"
#include "mgos_rpc.h"
#include "mgos_sys_config.h"
#include "mgos_timers.h"

bool s_heater_on = false;
double s_deadline = 0;

struct temp_sensor_data {
  double ts;
  double temp;
};

struct temp_sensor_data s_tsd[2];
double s_last_eval, s_last_action_ts;

static const char *onoff(bool on) {
  return (on ? "on" : "off");
}

static bool check_thresh(int si, bool heater_is_on, struct temp_sensor_data sd, double s_min, double s_max) {
  bool want_on;
  if (s_min < 0 || s_max < 0) {
    want_on = false;
  } else if (sd.ts == 0) {
    LOG(LL_INFO, ("S%d: no data yet", si));
    want_on = false;
  } else if (mg_time() - sd.ts > 300) {
    LOG(LL_INFO, ("S%d: data is stale", si));
    want_on = false;
  } else if (!heater_is_on && sd.temp < s_min) {
    LOG(LL_INFO, ("S%d: %.3lf < %.3lf", si, sd.temp, s_min));
    want_on = true;
  } else if (heater_is_on && sd.temp < s_max) {
    want_on = true;
  } else {
    LOG(LL_INFO, ("S%d: %.3lf < %.3lf < %.3lf", si, s_min, sd.temp, s_max));
    want_on = false;
  }
  return want_on;
}

static void hub_heater_eval(void) {
  bool want_on = false;
  double now = mg_time();
  if (s_deadline != 0) return;  // Heater is under manual control.
  if (now - s_last_eval < 60) return;
  want_on |= check_thresh(0, s_heater_on, s_tsd[0],
                          mgos_sys_config_get_hub_heater_s0_min(), mgos_sys_config_get_hub_heater_s0_max());
  want_on |= check_thresh(1, s_heater_on, s_tsd[1],
                          mgos_sys_config_get_hub_heater_s1_min(), mgos_sys_config_get_hub_heater_s1_max());
  if (s_heater_on != want_on) {
    LOG(LL_INFO, ("Heater %s -> %s", onoff(s_heater_on), onoff(want_on)));
    s_heater_on = want_on;
    s_last_action_ts = now;
  }
  s_last_eval = now;
}

static void heater_timer_cb(void *arg) {
  double now = mg_time();
  const struct mgos_config_hub_heater *hcfg =
      &mgos_sys_config_get_hub()->heater;
  if (s_deadline > 0 && s_deadline < now) {
    s_heater_on = !s_heater_on;
    LOG(LL_INFO,
        ("Deadline expired, turning heater %s", (s_heater_on ? "on" : "off")));
    s_deadline = 0;
  }
  hub_heater_eval();
  mgos_gpio_write(hcfg->relay_gpio, s_heater_on);
  (void) arg;
}

bool hub_heater_get_status(bool *heater_on, double *last_action_ts) {
  if (heater_on != NULL) *heater_on = s_heater_on;
  if (last_action_ts != NULL) *last_action_ts = s_last_action_ts;
  return true;
}

static void hub_heater_get_status_handler(struct mg_rpc_request_info *ri,
                                          void *cb_arg,
                                          struct mg_rpc_frame_info *fi,
                                          struct mg_str args) {
  int duration = -1;
  double now = mg_time();
  if (s_deadline >= now) {
    duration = (int) (s_deadline - now);
  }
  mg_rpc_send_responsef(ri, "{heater_on: %B, duration: %d}", s_heater_on,
                        duration);
  (void) args;
  (void) cb_arg;
  (void) fi;
}

static void hub_heater_set_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  int heater_on = -1, duration = -1;

  json_scanf(args.p, args.len, ri->args_fmt, &heater_on, &duration);

  if (heater_on < 0) {
    mg_rpc_send_errorf(ri, -1, "heater_on is required %d", duration);
    goto out;
  }

  if (duration > 0) {
    if (duration < 30) duration = 30;
  } else {
    duration = 12 * 3600;
  }

  if (heater_on != s_heater_on) {
    LOG(LL_INFO,
        ("Heater %s by request, duration %d", onoff(heater_on), duration));
    s_heater_on = heater_on;
  }

  double now = mg_time();
  s_deadline = now + duration;
  s_last_action_ts = now;

  mg_rpc_send_responsef(ri, "{heater_on: %B, deadline: %lf}", s_heater_on,
                        s_deadline);

out:
  (void) cb_arg;
  (void) fi;
}

static void sensor_report_temp_handler(struct mg_rpc_request_info *ri,
                                       void *cb_arg,
                                       struct mg_rpc_frame_info *fi,
                                       struct mg_str args) {
  int sid = -1;
  double ts = -1, temp = -1000, rh = -1000;

  json_scanf(args.p, args.len, ri->args_fmt, &sid, &ts, &temp, &rh);

  if (rh > 0) {
    LOG(LL_INFO, ("sid: %d, ts: %lf, temp: %lf, rh: %lf", sid, ts, temp, rh));
  } else {
    LOG(LL_INFO, ("sid: %d, ts: %lf, temp: %lf", sid, ts, temp));
  }

  if (sid < 0 || temp == -1000) goto out;

  if (sid == 0 && ts > s_tsd[0].ts) {
    s_tsd[0].ts = ts;
    s_tsd[0].temp = temp;
  } else if (sid == 1 && ts > s_tsd[1].ts) {
    s_tsd[1].ts = ts;
    s_tsd[1].temp = temp;
  }

out:
  mg_rpc_send_responsef(ri, NULL);
  (void) cb_arg;
  (void) fi;
}

bool hub_heater_init(void) {
  bool res = false;
  const struct mgos_config_hub_heater *hcfg =
      &mgos_sys_config_get_hub()->heater;

  if (!hcfg->enable) {
    res = true;
    goto out;
  }

  mgos_gpio_set_mode(hcfg->relay_gpio, MGOS_GPIO_MODE_OUTPUT);
  mgos_set_timer(1000, MGOS_TIMER_REPEAT, heater_timer_cb, NULL);

  struct mg_rpc *c = mgos_rpc_get_global();
  mg_rpc_add_handler(c, "Hub.Heater.GetStatus", "",
                     hub_heater_get_status_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Heater.Set", "{heater_on: %d, duration: %d}",
                     hub_heater_set_handler, NULL);
  mg_rpc_add_handler(c, "Sensor.ReportTemp",
                     "{sid: %d, ts: %lf, temp: %lf, rh: %lf}",
                     sensor_report_temp_handler, NULL);

  res = true;

out:
  return res;
}
