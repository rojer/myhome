#include "hub_heater.h"

#include "common/cs_dbg.h"
#include "common/cs_time.h"

#include "mgos.h"
#include "mgos_crontab.h"
#include "mgos_gpio.h"
#include "mgos_rpc.h"
#include "mgos_sys_config.h"
#include "mgos_timers.h"

#include "hub.h"

#define NUM_LIMITS 10

static bool s_enabled = false;
bool s_heater_on = false;
double s_deadline = 0;

double s_last_eval, s_last_action_ts;

static const char *onoff(bool on) {
  return (on ? "on" : "off");
}

static const struct mgos_config_hub_heater_limits *get_limits(int idx) {
  switch (idx) {
    case 0:
      return mgos_sys_config_get_hub_heater_limits();
    case 1:
      return mgos_sys_config_get_hub_heater_limits1();
    case 2:
      return mgos_sys_config_get_hub_heater_limits2();
    case 3:
      return mgos_sys_config_get_hub_heater_limits3();
    case 4:
      return mgos_sys_config_get_hub_heater_limits4();
    case 5:
      return mgos_sys_config_get_hub_heater_limits5();
    case 6:
      return mgos_sys_config_get_hub_heater_limits6();
    case 7:
      return mgos_sys_config_get_hub_heater_limits7();
    case 8:
      return mgos_sys_config_get_hub_heater_limits8();
    case 9:
      return mgos_sys_config_get_hub_heater_limits9();
  }
  return NULL;
}

static bool check_thresh(bool heater_is_on,
                         const struct mgos_config_hub_heater_limits *ls) {
  bool want_on = false;
  struct sensor_data sd;
  if (ls == NULL || !ls->enable) {
    want_on = false;
  } else if (!hub_get_data(ls->sid, ls->subid, &sd)) {
    LOG(LL_INFO, ("S%d/%d: no data yet", ls->sid, ls->subid));
    want_on = false;
  } else if (cs_time() - sd.ts > 300) {
    LOG(LL_INFO, ("S%d/%d: data is stale", sd.sid, sd.subid));
    want_on = false;
  } else if (!heater_is_on && sd.value < ls->min) {
    LOG(LL_INFO,
        ("S%d/%d: %.3lf < %.3lf", sd.sid, sd.subid, sd.value, ls->min));
    want_on = true;
  } else if (heater_is_on && sd.value < ls->max) {
    want_on = true;
  } else {
    LOG(LL_INFO, ("S%d/%d: Ok (%.3lf; min %.3lf max %.3lf)", sd.sid, sd.subid,
                  sd.value, ls->min, ls->max));
    want_on = false;
  }
  return want_on;
}

static void hub_heater_eval(void) {
  bool want_on = false;
  double now = cs_time();
  if (s_deadline != 0) return;  // Heater is under manual control.
  if (now - s_last_eval < 60) return;
  for (int i = 0; i < NUM_LIMITS; i++) {
    want_on |= check_thresh(s_heater_on, get_limits(i));
  }
  if (s_heater_on != want_on) {
    LOG(LL_INFO, ("Heater %s -> %s", onoff(s_heater_on), onoff(want_on)));
    s_heater_on = want_on;
    s_last_action_ts = now;
    report_to_server(mgos_sys_config_get_hub_ctl_sid(), HEATER_SUBID, now,
                     s_heater_on);
  }
  s_last_eval = now;
}

static void heater_timer_cb(void *arg) {
  double now = cs_time();
  const struct mgos_config_hub_heater *hcfg =
      &mgos_sys_config_get_hub()->heater;
  if (s_deadline > 0 && s_deadline < now) {
    LOG(LL_INFO, ("Deadline expired"));
    if (s_heater_on) s_heater_on = false;
    s_deadline = 0;
  }
  hub_heater_eval();
  mgos_gpio_write(hcfg->relay_gpio, s_heater_on);
  (void) arg;
}

bool hub_heater_get_status(bool *heater_on, double *last_action_ts) {
  if (heater_on != NULL) *heater_on = s_heater_on;
  if (last_action_ts != NULL) *last_action_ts = s_last_action_ts;
  return s_enabled;
}

static void hub_heater_get_status_handler(struct mg_rpc_request_info *ri,
                                          void *cb_arg,
                                          struct mg_rpc_frame_info *fi,
                                          struct mg_str args) {
  int duration = -1;
  double now = cs_time();
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
  bool heater_on = -1;
  int duration = -1;

  json_scanf(args.p, args.len, ri->args_fmt, &heater_on, &duration);

  if (heater_on < 0) {
    mg_rpc_send_errorf(ri, -1, "heater_on is required");
    goto out;
  }

  if (duration <= 0) {
    duration = 12 * 3600;
  }

  if (heater_on != s_heater_on) {
    LOG(LL_INFO,
        ("Heater %s by request, duration %d", onoff(heater_on), duration));
    s_heater_on = heater_on;
    report_to_server(mgos_sys_config_get_hub_ctl_sid(), HEATER_SUBID, cs_time(),
                     s_heater_on);
  }

  double now = cs_time();
  s_deadline = now + duration;
  s_last_action_ts = now;

  hub_heater_eval();

  mg_rpc_send_responsef(ri, "{heater_on: %B, deadline: %lf}", s_heater_on,
                        s_deadline);

out:
  (void) cb_arg;
  (void) fi;
}

static void hub_heater_get_limits_handler(struct mg_rpc_request_info *ri,
                                          void *cb_arg,
                                          struct mg_rpc_frame_info *fi,
                                          struct mg_str args) {
  int sid = -1, subid = -1;

  json_scanf(args.p, args.len, ri->args_fmt, &sid, &subid);

  struct mbuf mb;
  mbuf_init(&mb, 50);
  struct json_out out = JSON_OUT_MBUF(&mb);
  json_printf(&out, "[");

  bool first = true;
  for (int i = 0; i < NUM_LIMITS; i++) {
    const struct mgos_config_hub_heater_limits *ls = get_limits(i);
    if (ls == NULL || ls->sid < 0 ||
        (sid >= 0 && !(ls->sid == sid && ls->subid == subid))) {
      continue;
    }
    if (!first) json_printf(&out, ", ");
    json_printf(&out, "{sid: %d, subid: %d, enable: %B, min: %lf, max: %lf}",
                ls->sid, ls->subid, ls->enable, ls->min, ls->max);
    first = false;
  }

  json_printf(&out, "]");
  mg_rpc_send_responsef(ri, "%.*s", (int) mb.len, mb.buf);
  mbuf_free(&mb);

  (void) cb_arg;
  (void) fi;
}

static void hub_heater_set_limits_handler(struct mg_rpc_request_info *ri,
                                          void *cb_arg,
                                          struct mg_rpc_frame_info *fi,
                                          struct mg_str args) {
  struct mgos_config_hub_heater_limits nls = {
      .sid = -1,
      .subid = -1,
      .enable = false,
      .min = 0.0,
      .max = 0.0,
  };

  json_scanf(args.p, args.len, ri->args_fmt, &nls.sid, &nls.subid, &nls.enable,
             &nls.min, &nls.max);

  if (nls.sid < 0 || nls.subid < 0) {
    mg_rpc_send_errorf(ri, -1, "sid and subid are required");
    goto out;
  }

  // Try to find an existing entry for this sensor first.
  const struct mgos_config_hub_heater_limits *ls;
  for (int i = 0; i < NUM_LIMITS + 1; i++) {
    ls = get_limits(i);
    if (ls != NULL && ls->sid == nls.sid && ls->subid == nls.subid) break;
  }
  // If not found, find an unused entry.
  if (ls == NULL) {
    for (int i = 0; i < NUM_LIMITS + 1; i++) {
      ls = get_limits(i);
      if (ls != NULL && ls->sid < 0) break;
    }
  }
  // If no unused entries, find a disabled one.
  if (ls == NULL) {
    for (int i = 0; i < NUM_LIMITS + 1; i++) {
      ls = get_limits(i);
      if (ls != NULL && !ls->enable) break;
    }
  }
  if (ls == NULL) {
    mg_rpc_send_errorf(ri, -2, "no available entries");
    goto out;
  }

  *((struct mgos_config_hub_heater_limits *) ls) = nls;

  char *msg = NULL;
  if (!mgos_sys_config_save(&mgos_sys_config, false /* try_once */, &msg)) {
    mg_rpc_send_errorf(ri, -1, "error saving config: %s", (msg ? msg : ""));
    free(msg);
    goto out;
  }

  hub_heater_eval();

  mg_rpc_send_responsef(ri, NULL);

out:
  (void) cb_arg;
  (void) fi;
}

static void sensor_report_temp_handler(struct mg_rpc_request_info *ri,
                                       void *cb_arg,
                                       struct mg_rpc_frame_info *fi,
                                       struct mg_str args) {
  int sid = -1;
  char *name = NULL;
  double ts = -1, temp = -1000, rh = -1000;

  json_scanf(args.p, args.len, ri->args_fmt, &sid, &name, &ts, &temp, &rh);

  const char *sn = (name != NULL ? name : "");
  if (rh > 0) {
    LOG(LL_INFO, ("sid: %d, name: %s, ts: %lf, temp: %lf, rh: %lf", sid, sn, ts,
                  temp, rh));
  } else {
    LOG(LL_INFO, ("sid: %d, name: %s, ts: %lf, temp: %lf", sid, sn, ts, temp));
  }

  if (sid < 0) goto out;

  if (temp > -300) {
    char buf[100] = {0};
    if (name != NULL) {
      snprintf(buf, sizeof(buf), "%s Temp", name);
    }
    struct sensor_data sd0 = {
        .sid = sid,
        .subid = 0,
        .ts = ts,
        .value = temp,
        .name = (name != NULL ? buf : NULL),
    };
    hub_add_data(&sd0);
  }
  if (rh >= 0) {
    char buf[100] = {0};
    if (name != NULL) {
      snprintf(buf, sizeof(buf), "%s RH", name);
    }
    struct sensor_data sd0 = {
        .sid = sid,
        .subid = 1,
        .ts = ts,
        .value = rh,
        .name = (name != NULL ? buf : NULL),
    };
    hub_add_data(&sd0);
  }

out:
  mg_rpc_send_responsef(ri, NULL);
  free(name);
  (void) cb_arg;
  (void) fi;
}

static void heater_crontab_cb(struct mg_str action, struct mg_str payload,
                              void *userdata) {
  bool heater_on = (bool) (intptr_t) userdata;

  LOG(LL_INFO, ("Heater %s by cron", onoff(heater_on)));

  double now = cs_time();
  s_deadline = now + 12 * 3600;  // XXX: For now
  s_last_action_ts = now;
  s_heater_on = heater_on;

  (void) payload;
  (void) action;
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
  mg_rpc_add_handler(c, "Hub.Heater.Set", "{heater_on: %B, duration: %d}",
                     hub_heater_set_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Heater.GetLimits", "{sid: %d, subid: %d}",
                     hub_heater_get_limits_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Heater.SetLimits",
                     "{sid: %d, subid: %d, enable: %B, min: %lf, max: %lf}",
                     hub_heater_set_limits_handler, NULL);
  mg_rpc_add_handler(c, "Sensor.ReportTemp",
                     "{sid: %d, name: %Q, ts: %lf, temp: %lf, rh: %lf}",
                     sensor_report_temp_handler, NULL);
  mgos_crontab_register_handler(mg_mk_str("heater_on"), heater_crontab_cb,
                                (void *) 1);
  mgos_crontab_register_handler(mg_mk_str("heater_off"), heater_crontab_cb,
                                (void *) 0);
  s_enabled = true;
  res = true;

out:
  return res;
}
