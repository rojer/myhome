#include "hub.h"

#include "mgos_rpc.h"
#include "mgos_sys_config.h"

void report_to_server(int sid, int subid, double ts, double value) {
  if (sid < 0)
    return;
  if (mgos_sys_config_get_hub_data_server_addr() == NULL)
    return;
  struct mg_rpc_call_opts opts = {
      .dst = mg_mk_str(mgos_sys_config_get_hub_data_server_addr()),
  };
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
               &opts, "{sid: %d, subid: %d, ts: %lf, v: %lf}", sid, subid, ts,
               value);
}
