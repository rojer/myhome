#include "hub.h"

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

void hub_add_data(const struct sensor_data *sd) {
  struct sensor_data_entry *sde = NULL;
  if (sd->ts <= 0 || sd->sid < 0) return;
  SLIST_FOREACH(sde, &s_data, next) {
    if (sde->sd.sid == sd->sid && sde->sd.subid == sd->subid) break;
  }
  if (sde == NULL) {
    sde = (struct sensor_data_entry *) calloc(1, sizeof(*sde));
    if (sde == NULL) return;
    LOG(LL_INFO, ("New sensor %d/%d", sd->sid, sd->subid));
    SLIST_INSERT_HEAD(&s_data, sde, next);
  } else if (sd->ts <= sde->sd.ts) {
    return;
  }
  LOG(LL_INFO, ("New data: %d/%d %.3lf", sd->sid, sd->subid, sd->value));
  report_to_server_sd(sd);
  sde->sd = *sd;
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
  struct sensor_data_entry *sde;
  SLIST_FOREACH(sde, &s_data, next) {
    if (!first) json_printf(&out, ", ");
    json_printf(&out, "{sid: %d, subid: %d}", sde->sd.sid, sde->sd.subid);
    first = false;
  }
  json_printf(&out, "]");
  mg_rpc_send_responsef(ri, "%.*s", (int) mb.len, mb.buf);
  mbuf_free(&mb);
  (void) cb_arg;
  (void) args;
  (void) fi;
}

bool hub_data_init(void) {
  struct mg_rpc *c = mgos_rpc_get_global();
  mg_rpc_add_handler(c, "Hub.Data.List", "", hub_data_list_handler, NULL);
  mg_rpc_add_handler(c, "Hub.Data.Get", "{sid: %d, subid: %d}",
                     hub_data_get_handler, NULL);
  return true;
}
