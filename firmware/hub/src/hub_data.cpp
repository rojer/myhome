#include "hub_data.hpp"

#include <cmath>
#include <map>

#include "mgos.hpp"
#include "mgos_rpc.h"

static std::map<uint64_t, SensorData> s_data;

uint64_t SensorData::GetKey() const {
  return MakeKey(sid, subid);
}

uint64_t SensorData::MakeKey(int sid, int subid) {
  return ((static_cast<uint64_t>(sid) << 32) | subid);
}

std::string SensorData::ToString() const {
  return mgos::SPrintf("%d/%d (%s) %.3lf @ %.3lf", sid, subid, name.c_str(),
                       value, ts);
}

SensorData::SensorData(int _sid, int _subid, double _ts, double _value)
    : sid(_sid), subid(_subid), ts(_ts), value(_value) {
}

void report_to_server_sd(const struct SensorData *sd) {
  if (sd->sid < 0) return;
  if (mgos_sys_config_get_hub_data_server_addr() == NULL) return;
  struct mg_rpc_call_opts opts = {};
  opts.dst = mg_mk_str(mgos_sys_config_get_hub_data_server_addr()),
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
               &opts, "{sid: %d, subid: %d, ts: %lf, v: %lf}", sd->sid,
               sd->subid, sd->ts, sd->value);
}

static void hub_add_data_internal(const struct SensorData *sd, bool report) {
  if (sd->ts <= 0 || sd->sid < 0) return;
  SensorData &sde = s_data[sd->GetKey()];
  if (sde.sid < 0) {
    LOG(LL_INFO, ("New sensor %d/%d", sd->sid, sd->subid));
  }
  if (sd->ts <= sde.ts) {
    LOG(LL_INFO, ("Old data: %s", sd->ToString().c_str()));
    return;
  }
  sde = *sd;
  LOG(LL_INFO, ("New data: %s", sd->ToString().c_str()));
  if (report) {
    report_to_server_sd(sd);
  }
}

void report_to_server(int sid, int subid, double ts, double value) {
  struct SensorData sd(sid, subid, ts, value);
  hub_add_data_internal(&sd, true);
}

void hub_add_data(const struct SensorData *sd) {
  hub_add_data_internal(sd, true);
}

bool hub_get_data(int sid, int subid, struct SensorData *sd) {
  const auto it = s_data.find(SensorData::MakeKey(sid, subid));
  if (it == s_data.end()) return false;
  *sd = it->second;
  return true;
}

static void hub_data_save_timer_cb(void *arg UNUSED_ARG) {
  const char *fn = mgos_sys_config_get_hub_data_file();
  FILE *fp = fopen(fn, "w");
  if (fp == NULL) return;
  int n = 0, n_bytes = 0;
  struct json_out out = JSON_OUT_FILE(fp);
  for (const auto &e : s_data) {
    const struct SensorData &sd = e.second;
    n_bytes += json_printf(
        &out, "{sid: %d, subid: %d, name: %Q, ts: %.3lf, value: %lf}\n", sd.sid,
        sd.subid, sd.name.c_str(), sd.ts, sd.value);
    n++;
  }
  fclose(fp);
  LOG(LL_INFO, ("Saved %d entries (%d bytes) to %s", n, n_bytes, fn));
}

static void hub_data_load(const char *fn) {
  if (fn == NULL) return;
  FILE *fp = fopen(fn, "r");
  if (fp == NULL) return;
  char buf[256];
  int n = 0;
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    struct SensorData sd;
    char *name = nullptr;
    json_scanf(buf, strlen(buf),
               "{sid: %d, subid: %d, name: %Q, ts: %lf, value: %lf}", &sd.sid,
               &sd.subid, &name, &sd.ts, &sd.value);
    if (name != NULL) {
      sd.name = name;
      free(name);
    }
    hub_add_data(&sd);
    n++;
  }
  fclose(fp);
  LOG(LL_INFO, ("Loaded %d entries from %s", n, fn));
}

static void hub_data_get_handler(struct mg_rpc_request_info *ri,
                                 void *cb_arg UNUSED_ARG,
                                 struct mg_rpc_frame_info *fi UNUSED_ARG,
                                 struct mg_str args) {
  int sid = -1, subid = -1;
  json_scanf(args.p, args.len, ri->args_fmt, &sid, &subid);

  struct SensorData sd;
  if (!hub_get_data(sid, subid, &sd)) {
    mg_rpc_send_errorf(ri, -1, "invalid sid %d/%d", sid, subid);
    return;
  }

  mg_rpc_send_responsef(ri, "{sid: %d, subid: %d, ts: %.3lf, value: %.3lf}",
                        sid, subid, sd.ts, sd.value);
}

static void hub_data_reset_handler(struct mg_rpc_request_info *ri,
                                   void *cb_arg UNUSED_ARG,
                                   struct mg_rpc_frame_info *fi UNUSED_ARG,
                                   struct mg_str args) {
  int sid = -1, subid = -1;
  json_scanf(args.p, args.len, ri->args_fmt, &sid, &subid);
  if (sid < 0 && subid < 0) {
    LOG(LL_INFO, ("Reset all data"));
    s_data.clear();
    mg_rpc_send_responsef(ri, nullptr);
    return;
  }
  const auto it = s_data.find(SensorData::MakeKey(sid, subid));
  if (it == s_data.end()) {
    mg_rpc_send_errorf(ri, 404, "Not Found");
    return;
  }
  s_data.erase(it);
  mg_rpc_send_responsef(ri, nullptr);
}

static void hub_data_list_handler(struct mg_rpc_request_info *ri,
                                  void *cb_arg UNUSED_ARG,
                                  struct mg_rpc_frame_info *fi UNUSED_ARG,
                                  struct mg_str args UNUSED_ARG) {
  struct mbuf mb;
  bool first = true;
  mbuf_init(&mb, 50);
  struct json_out out = JSON_OUT_MBUF(&mb);
  json_printf(&out, "[");
  for (const auto &e : s_data) {
    const struct SensorData &sd = e.second;
    if (!first) json_printf(&out, ", ");
    if (!sd.name.empty()) {
      json_printf(&out,
                  "{sid: %d, subid: %d, name: %Q, ts: %.3lf, value: %.3lf}",
                  sd.sid, sd.subid, sd.name.c_str(), sd.ts, sd.value);
    } else {
      json_printf(&out, "{sid: %d, subid: %d, ts: %.3lf, value: %.3lf}", sd.sid,
                  sd.subid, sd.ts, sd.value);
    }
    first = false;
  }
  json_printf(&out, "]");
  mg_rpc_send_responsef(ri, "%.*s", (int) mb.len, mb.buf);
  mbuf_free(&mb);
}

static bool parse_data_point(struct mg_rpc_request_info *ri, struct mg_str s,
                             double default_ts) {
  bool res = false;
  int sid = -1, subid = 0;
  char *name = nullptr;
  double ts = NAN, value = NAN;
  json_scanf(s.p, s.len, "{sid: %d, subid: %d, name: %Q, ts: %lf, v: %lf}",
             &sid, &subid, &name, &ts, &value);
  mgos::ScopedCPtr name_owner(name);

  if (sid < 0) {
    mg_rpc_send_errorf(ri, -1, "invalid sid %d/%d", sid, subid);
    return false;
  }
  if (std::isnan(value)) {
    mg_rpc_send_errorf(ri, -2, "value is required");
    return false;
  }
  if (std::isnan(ts)) {
    if (default_ts > 0) {
      ts = default_ts;
    } else {
      ts = mg_time();
    }
  }

  struct SensorData sd = {
      .sid = sid,
      .subid = subid,
      .ts = ts,
      .value = value,
  };
  if (name != nullptr) {
    sd.name = name;
  }

  hub_add_data(&sd);

  res = true;

  return res;
}

static void hub_sensor_data_handler(struct mg_rpc_request_info *ri,
                                    void *cb_arg UNUSED_ARG,
                                    struct mg_rpc_frame_info *fi UNUSED_ARG,
                                    struct mg_str args) {
  if (!parse_data_point(ri, args, 0)) {
    return;  // Error already sent.
  }
  mg_rpc_send_responsef(ri, NULL);
}

static void hub_sensor_data_multi_handler(
    struct mg_rpc_request_info *ri, void *cb_arg UNUSED_ARG,
    struct mg_rpc_frame_info *fi UNUSED_ARG, struct mg_str args) {
  double default_ts = 0;
  struct json_token data = JSON_INVALID_TOKEN;
  json_scanf(args.p, args.len, ri->args_fmt, &default_ts, &data);
  if (data.type != JSON_TYPE_ARRAY_END) {
    mg_rpc_send_errorf(ri, -3, "data is required and must be an array");
    return;
  }

  struct json_token t;
  for (int i = 0; json_scanf_array_elem(data.ptr, data.len, "", i, &t) > 0;
       i++) {
    if (!parse_data_point(ri, mg_mk_str_n(t.ptr, t.len), default_ts)) {
      return;  // Error already sent.
    }
  }

  mg_rpc_send_responsef(ri, NULL);
}

bool hub_data_init(void) {
  struct mg_rpc *c = mgos_rpc_get_global();
  mg_rpc_add_handler(c, "Hub.Data.List", "", hub_data_list_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Data.Get", "{sid: %d, subid: %d}",
                     hub_data_get_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Data.Reset", "{sid: %d, subid: %d}",
                     hub_data_reset_handler, NULL);
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
  return true;
}
