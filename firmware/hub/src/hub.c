#include "hub.h"

#include <math.h>

#include "mgos.h"
#include "mgos_rpc.h"
#include "mgos_sys_config.h"

struct sensor_data_entry {
  struct sensor_data sd;
  SLIST_ENTRY(sensor_data_entry) next;
};

static SLIST_HEAD(s_data,
                  sensor_data_entry) s_data = SLIST_HEAD_INITIALIZER(s_data);

void report_to_server_sd(const struct sensor_data *sd) {
  if (sd->sid < 0) return;
  hub_add_data(sd);
  if (mgos_sys_config_get_hub_data_server_addr() == NULL) return;
  struct mg_rpc_call_opts opts = {
      .dst = mg_mk_str(mgos_sys_config_get_hub_data_server_addr()),
  };
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
               &opts, "{sid: %d, subid: %d, ts: %lf, v: %lf}", sd->sid,
               sd->subid, sd->ts, sd->value);
}

void report_to_server(int sid, int subid, double ts, double value) {
  struct sensor_data sd = {
      .sid = sid,
      .subid = subid,
      .ts = ts,
      .value = value,
  };
  report_to_server_sd(&sd);
}

void hub_add_data_internal(const struct sensor_data *sd, bool report) {
  struct sensor_data_entry *sde = NULL;
  if (sd->ts <= 0 || sd->sid < 0) return;
  SLIST_FOREACH(sde, &s_data, next) {
    if (sde->sd.sid == sd->sid && sde->sd.subid == sd->subid) break;
  }
  if (sde == NULL) {
    sde = (struct sensor_data_entry *) calloc(1, sizeof(*sde));
    if (sde == NULL) return;
    if (report) {
      LOG(LL_INFO, ("New sensor %d/%d", sd->sid, sd->subid));
    }
    SLIST_INSERT_HEAD(&s_data, sde, next);
  } else if (sd->ts <= sde->sd.ts) {
    return;
  }
  free(sde->sd.name);
  sde->sd = *sd;
  if (sd->name != NULL) {
    sde->sd.name = strdup(sd->name);
  }
  if (report) {
    LOG(LL_INFO, ("New data: %d/%d (%s) %.3lf", sd->sid, sd->subid,
                  (sd->name ? sd->name : ""), sd->value));
    report_to_server_sd(sd);
  }
}

void hub_add_data(const struct sensor_data *sd) {
  hub_add_data_internal(sd, true);
}

bool hub_get_data(int sid, int subid, struct sensor_data *sd) {
  struct sensor_data_entry *sde = NULL;
  SLIST_FOREACH(sde, &s_data, next) {
    if (sde->sd.sid == sid && sde->sd.subid == subid) break;
  }
  if (sde == NULL) return false;
  *sd = sde->sd;
  return true;
}

static void hub_data_save_timer_cb(void *arg) {
  const char *fn = mgos_sys_config_get_hub_data_file();
  FILE *fp = fopen(fn, "w");
  if (fp == NULL) return;
  int n = 0, n_bytes = 0;
  struct json_out out = JSON_OUT_FILE(fp);
  const struct sensor_data_entry *sde;
  SLIST_FOREACH(sde, &s_data, next) {
    const struct sensor_data *sd = &sde->sd;
    n_bytes += json_printf(
        &out, "{sid: %d, subid: %d, name: %Q, ts: %.3lf, value: %lf}\n",
        sd->sid, sd->subid, (sd->name ? sd->name : ""), sd->ts, sd->value);
    n++;
  }
  fclose(fp);
  LOG(LL_INFO, ("Saved %d entries (%d bytes) to %s", n, n_bytes, fn));
  (void) arg;
}

static void hub_data_load(const char *fn) {
  if (fn == NULL) return;
  FILE *fp = fopen(fn, "r");
  if (fp == NULL) return;
  char buf[256];
  int n = 0;
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    struct sensor_data sd = {
        .sid = -1,
        .subid = -1,
        .name = NULL,
        .ts = 0,
        .value = 0,
    };
    json_scanf(buf, strlen(buf),
               "{sid: %d, subid: %d, name: %Q, ts: %lf, value: %lf}", &sd.sid,
               &sd.subid, &sd.name, &sd.ts, &sd.value);
    if (sd.name != NULL && strlen(sd.name) == 0) {
      free(sd.name);
      sd.name = NULL;
    }
    hub_add_data(&sd);
    free(sd.name);
    n++;
  }
  fclose(fp);
  LOG(LL_INFO, ("Loaded %d entries from %s", n, fn));
}

static void hub_data_get_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args) {
  int sid = -1, subid = -1;
  json_scanf(args.p, args.len, ri->args_fmt, &sid, &subid);

  struct sensor_data sd;
  if (!hub_get_data(sid, subid, &sd)) {
    mg_rpc_send_errorf(ri, -1, "invalid sid %d/%d", sid, subid);
    return;
  }

  mg_rpc_send_responsef(ri, "{sid: %d, subid: %d, ts: %.3lf, value: %.3lf}",
                        sid, subid, sd.ts, sd.value);

  (void) cb_arg;
  (void) fi;
}

static void hub_data_list_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi,
                                  struct mg_str args) {
  struct mbuf mb;
  bool first = true;
  mbuf_init(&mb, 50);
  struct json_out out = JSON_OUT_MBUF(&mb);
  json_printf(&out, "[");
  const struct sensor_data_entry *sde;
  SLIST_FOREACH(sde, &s_data, next) {
    const struct sensor_data *sd = &sde->sd;
    if (!first) json_printf(&out, ", ");
    if (sd->name != NULL) {
      json_printf(&out, "{sid: %d, subid: %d, name: %Q}", sd->sid, sd->subid,
                  sd->name);
    } else {
      json_printf(&out, "{sid: %d, subid: %d}", sd->sid, sd->subid);
    }
    first = false;
  }
  json_printf(&out, "]");
  mg_rpc_send_responsef(ri, "%.*s", (int) mb.len, mb.buf);
  mbuf_free(&mb);
  (void) cb_arg;
  (void) args;
  (void) fi;
}

static bool parse_data_point(struct mg_rpc_request_info *ri, struct mg_str s,
                             double default_ts) {
  bool res = false;
  int sid = -1, subid = 0;
  char *name = NULL;
  double ts = NAN, value = NAN;
  json_scanf(s.p, s.len, "{sid: %d, subid: %d, name: %Q, ts: %lf, v: %lf}",
             &sid, &subid, &name, &ts, &value);

  if (sid < 0) {
    mg_rpc_send_errorf(ri, -1, "invalid sid %d/%d", sid, subid);
    goto out;
  }
  if (isnan(value)) {
    mg_rpc_send_errorf(ri, -2, "value is required");
    goto out;
  }
  if (isnan(ts)) {
    if (default_ts > 0) {
      ts = default_ts;
    } else {
      ts = mg_time();
    }
  }

  struct sensor_data sd = {
      .sid = sid,
      .subid = subid,
      .ts = ts,
      .value = value,
      .name = name,
  };

  hub_add_data(&sd);

  res = true;

out:
  free(name);
  return res;
}

static void hub_sensor_data_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg, struct mg_rpc_frame_info *fi,
                                    struct mg_str args) {
  if (!parse_data_point(ri, args, 0)) {
    goto out;  // Error already sent.
  }

  mg_rpc_send_responsef(ri, NULL);

out:
  (void) cb_arg;
  (void) fi;
}

static void hub_sensor_data_multi_handler(struct mg_rpc_request_info *ri,
                                          void *cb_arg,
                                          struct mg_rpc_frame_info *fi,
                                          struct mg_str args) {
  double default_ts = 0;
  struct json_token data = JSON_INVALID_TOKEN;
  json_scanf(args.p, args.len, ri->args_fmt, &default_ts, &data);
  if (data.type != JSON_TYPE_ARRAY_END) {
    mg_rpc_send_errorf(ri, -3, "data is required and must be an array");
    goto out;
  }

  struct json_token t;
  for (int i = 0; json_scanf_array_elem(data.ptr, data.len, "", i, &t) > 0;
       i++) {
    if (!parse_data_point(ri, mg_mk_str_n(t.ptr, t.len), default_ts)) {
      goto out;  // Error already sent.
    }
  }

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
    struct sensor_data sd0 = {};
    sd0.sid = sid;
    sd0.subid = 0;
    sd0.ts = ts;
    sd0.value = temp;
    sd0.name = (name != NULL ? buf : NULL);
    hub_add_data(&sd0);
  }
  if (rh >= 0) {
    char buf[100] = {0};
    if (name != NULL) {
      snprintf(buf, sizeof(buf), "%s RH", name);
    }
    struct sensor_data sd0 = {};
    sd0.sid = sid;
    sd0.subid = 1;
    sd0.ts = ts;
    sd0.value = rh;
    sd0.name = (name != NULL ? buf : NULL);
    hub_add_data(&sd0);
  }

out:
  mg_rpc_send_responsef(ri, NULL);
  free(name);
  (void) cb_arg;
  (void) fi;
}

bool hub_data_init(void) {
  struct mg_rpc *c = mgos_rpc_get_global();
  mg_rpc_add_handler(c, "Hub.Data.List", "", hub_data_list_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Data.Get", "{sid: %d, subid: %d}",
                     hub_data_get_handler, NULL);
  mg_rpc_add_handler(c, "Sensor.Data",
                     "{sid: %d, subid: %d, name: %Q, ts: %lf, v: %lf}",
                     hub_sensor_data_handler, NULL);
  mg_rpc_add_handler(c, "Sensor.DataMulti", "{ts: %lf, data: %T}",
                     hub_sensor_data_multi_handler, NULL);
  hub_data_load(mgos_sys_config_get_hub_data_file());
  if (mgos_sys_config_get_hub_data_save_interval() > 0) {
    mgos_set_timer(mgos_sys_config_get_hub_data_save_interval() * 1000,
                   MGOS_TIMER_REPEAT, hub_data_save_timer_cb, NULL);
  }
  mg_rpc_add_handler(c, "Sensor.ReportTemp",
                     "{sid: %d, name: %Q, ts: %lf, temp: %lf, rh: %lf}",
                     sensor_report_temp_handler, NULL);
  return true;
}
