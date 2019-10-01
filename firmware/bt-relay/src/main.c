#include <math.h>

#include "mgos.h"

#include "common/queue.h"

#include "mgos_bt.h"
#include "mgos_bt_gap.h"

#include "mgos_rpc.h"

static bool s_scanning = false;

enum bt_sensor_type {
  BT_SENSOR_NONE = 0,
  BT_SENSOR_XAVAX = 1,
  BT_SENSOR_ASENSOR = 2,
};

static void start_scan(void) {
  if (s_scanning) return;
  LOG(LL_DEBUG, ("Starting scan"));
  struct mgos_bt_gap_scan_opts opts = {
      .duration_ms = 5000,
      .active = false,
  };
  s_scanning = mgos_bt_gap_scan(&opts);
}

struct asensor_data {
  uint8_t hdr[9];   // 02 01 06 03 03 f5 fe 13 ff
  uint16_t vendor;  // d2 00
  uint8_t mac[6];
  uint8_t fw_version;       // 02
  int8_t temp;              // temperature.
  uint8_t moving;           // 0 - stationary, 1 - moving.
  int8_t ax, ay, az;        // x, y, z acceleration values
  uint8_t cur_motion_dur;   // current motion duration, seconds.
  uint8_t prev_motion_dur;  // previous motion duration, seconds.
  uint8_t batt_pct;         // battery percentage.
  uint8_t pwr;              // measured power (?)
} __attribute__((packed));

struct asensor_state {
  int8_t temp;
  int8_t batt_pct;
  bool moving;
};

static bool parse_asensor_data(struct mg_str adv_data,
                               struct asensor_data *ad) {
  if (adv_data.len != 27) return false;
  memcpy(ad, adv_data.p, 27);
  if (ad->vendor != 0x00d2) return false;
  return true;
}

struct xavax_data {
  uint8_t temp;
  uint8_t tgt_temp;
  uint8_t batt_pct;
  uint8_t mode;
  uint8_t state;
  uint8_t unknown_ff;
  uint16_t unknown;
} __attribute__((packed));

struct xavax_state {
  uint8_t temp;
  uint8_t tgt_temp;
  uint8_t batt_pct;
  uint8_t state;
  union {
    struct {
      uint8_t temp : 1;
      uint8_t tgt_temp : 1;
      uint8_t batt_pct : 1;
      uint8_t state : 1;
    };
    uint8_t value;
  } changed;
};

static struct mgos_bt_uuid xavax_svc_uuid;
static const char *xavax_svc_uuid_str = "47e9ee00-47e9-11e4-8939-164230d1df67";

static bool parse_xavax_data(struct mg_str adv_data, struct xavax_data *xd) {
  if (!mgos_bt_gap_adv_data_has_service(adv_data, &xavax_svc_uuid))
    return false;
  struct mg_str xavax_data = mgos_bt_gap_parse_adv_data(
      adv_data, MGOS_BT_GAP_EIR_MANUFACTURER_SPECIFIC_DATA);
  if (xavax_data.len != sizeof(*xd)) {
    if (xavax_data.len == 0) return false;
    LOG(LL_ERROR, ("Incompatible Xavax data length, %d", (int) xavax_data.len));
    return false;
  }
  memcpy(xd, xavax_data.p, sizeof(*xd));
  return true;
}

union bt_sensor_data {
  struct asensor_data asensor;
  struct xavax_data xavax;
};

struct sensor_state {
  struct mgos_bt_addr addr;
  int8_t rssi;
  int sid;
  enum bt_sensor_type type;
  double last_seen_ts;
  double last_seen_uts;
  double last_reported_uts;
  union {
    struct asensor_state asensor;
    struct xavax_state xavax;
  };
  SLIST_ENTRY(sensor_state) next;
};

SLIST_HEAD(s_sensors, sensor_state)
s_sensors = SLIST_HEAD_INITIALIZER(s_sensors);

static struct sensor_state *find_sensor_state(const struct mgos_bt_addr *addr) {
  struct sensor_state *ss;
  SLIST_FOREACH(ss, &s_sensors, next) {
    if (mgos_bt_addr_cmp(&ss->addr, addr) == 0) return ss;
  }
  return NULL;
}

static float xavax_conv_temp(uint8_t t) {
  if (t == 0xff) return NAN;
  return (t * 0.5);
}

static void report_sensor(struct sensor_state *ss) {
  char addr[MGOS_BT_ADDR_STR_LEN];
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (hub_addr == NULL) return;
  int age = (int) (mgos_uptime() - ss->last_seen_uts);
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  switch (ss->type) {
    case BT_SENSOR_NONE:
      break;
    case BT_SENSOR_ASENSOR: {
      struct asensor_state *as = &ss->asensor;
      LOG(LL_INFO, ("Reporting %s RSSI %d SID %d t %d age %3d "
                    "T %d m %d batt %d",
                    mgos_bt_addr_to_str(&ss->addr, 0, addr), ss->rssi, ss->sid,
                    age, ss->type, as->temp, as->moving, as->batt_pct));
      mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                   &opts, "{sid: %d, subid: %d, ts: %lf, v: %d}", ss->sid, 0,
                   ss->last_seen_ts, as->temp);
      mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                   &opts, "{sid: %d, subid: %d, ts: %lf, v: %d}", ss->sid, 1,
                   ss->last_seen_ts, as->moving);
      mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
                   &opts, "{sid: %d, subid: %d, ts: %lf, v: %d}", ss->sid, 2,
                   ss->last_seen_ts, as->batt_pct);
      break;
    }
    case BT_SENSOR_XAVAX: {
      struct xavax_state *xs = &ss->xavax;
      float temp = xavax_conv_temp(xs->temp);
      float tgt_temp = xavax_conv_temp(xs->tgt_temp);
      LOG(LL_INFO,
          ("Reporting %s RSSI %d SID %d t %d age %3d "
           "T %.1lf TT %.1lf batt %d%% state 0x%02x chg %#x",
           mgos_bt_addr_to_str(&ss->addr, 0, addr), ss->rssi, ss->sid, ss->type,
           age, temp, tgt_temp, xs->batt_pct, xs->state, xs->changed.value));
      if (xs->temp != 0 && xs->temp != 0xff && xs->changed.temp) {
        mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL,
                     NULL, &opts, "{sid: %d, subid: %d, ts: %lf, v: %.1f}",
                     ss->sid, 0, ss->last_seen_ts, temp);
      }
      if (xs->tgt_temp != 0 && xs->tgt_temp != 0xff && xs->changed.temp) {
        mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL,
                     NULL, &opts, "{sid: %d, subid: %d, ts: %lf, v: %.1f}",
                     ss->sid, 1, ss->last_seen_ts, tgt_temp);
      }
      // Not only battery percentage can be 0 or 0xff for "unknown" but it can
      // sometimes can get non-sensical values like 224 (0xe0).
      if (xs->batt_pct != 0 && xs->batt_pct <= 100) {
        mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL,
                     NULL, &opts, "{sid: %d, subid: %d, ts: %lf, v: %d}",
                     ss->sid, 2, ss->last_seen_ts, xs->batt_pct);
      }
      if (xs->changed.state) {
        mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL,
                     NULL, &opts, "{sid: %d, subid: %d, ts: %lf, v: %d}",
                     ss->sid, 4, ss->last_seen_ts, xs->state);
      }
      xs->changed.value = 0;
      break;
    }
  }
}

static void gap_handler(int ev, void *ev_data, void *userdata) {
  switch (ev) {
    case MGOS_BT_GAP_EVENT_SCAN_RESULT: {
      char addr[MGOS_BT_ADDR_STR_LEN], buf[512];
      struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
      struct mgos_bt_gap_scan_result *r = ev_data;
      struct mg_str name = mgos_bt_gap_parse_name(r->adv_data);

      json_printf(
          &out, "{mac: %Q, name: %.*Q, rssi: %d, adv: %H, rsp: %H}",
          mgos_bt_addr_to_str(&r->addr, MGOS_BT_ADDR_STRINGIFY_TYPE, addr),
          (int) name.len, name.p, r->rssi, (int) r->adv_data.len, r->adv_data.p,
          (int) r->scan_rsp.len, r->scan_rsp.p);

      LOG(LL_DEBUG, ("%s", buf));
      enum bt_sensor_type st = BT_SENSOR_NONE;
      union bt_sensor_data sd;
      if (parse_asensor_data(r->adv_data, &sd.asensor)) {
        st = BT_SENSOR_ASENSOR;
        LOG(LL_DEBUG,
            ("ASensor %s RSSI %d t %d m %d batt %d pwr %d",
             mgos_bt_addr_to_str(&r->addr, 0, addr), r->rssi, sd.asensor.temp,
             sd.asensor.moving, sd.asensor.batt_pct, sd.asensor.pwr));
        st = BT_SENSOR_ASENSOR;
      } else if (parse_xavax_data(r->adv_data, &sd.xavax)) {
        st = BT_SENSOR_XAVAX;
        LOG(LL_DEBUG,
            ("Xavax %s RSSI %d t 0x%02x tt 0x%02x batt %d state 0x%02x",
             mgos_bt_addr_to_str(&r->addr, 0, addr), r->rssi, sd.xavax.temp,
             sd.xavax.tgt_temp, sd.xavax.batt_pct, sd.xavax.state));
      } else {
        break;
      }

      bool changed = false;
      struct sensor_state *ss = find_sensor_state(&r->addr);
      if (ss == NULL) {
        ss = (struct sensor_state *) calloc(1, sizeof(*ss));
        if (ss == NULL) break;
        ss->addr = r->addr;
        ss->type = st;
        ss->sid = ((st << 24) | (ss->addr.addr[3] << 16) |
                   (ss->addr.addr[4] << 8) | ss->addr.addr[5]);
        LOG(LL_INFO,
            ("New sensor %s type %d sid %d",
             mgos_bt_addr_to_str(&ss->addr, 0, addr), ss->type, ss->sid));
        SLIST_INSERT_HEAD(&s_sensors, ss, next);
        changed = true;
      }
      ss->rssi = r->rssi;
      switch (st) {
        case BT_SENSOR_NONE:
          break;
        case BT_SENSOR_ASENSOR: {
          struct asensor_data *ad = &sd.asensor;
          struct asensor_state *as = &ss->asensor;
          // Don't trigger on temperature and battery percentage changes
          // because they tend to flap a lot.
          as->temp = ad->temp;
          as->batt_pct = ad->batt_pct;
          if (as->moving != ad->moving) {
            as->moving = ad->moving;
            changed = true;
          }
          break;
        }
        case BT_SENSOR_XAVAX: {
          struct xavax_data *xd = &sd.xavax;
          struct xavax_state *xs = &ss->xavax;
          if (xs->temp != xd->temp) {
            xs->temp = xd->temp;
            xs->changed.temp = true;
          }
          if (xs->tgt_temp != xd->tgt_temp) {
            xs->tgt_temp = xd->tgt_temp;
            xs->changed.tgt_temp = true;
          }
          if (xs->state != xd->state) {
            xs->state = xd->state;
            xs->changed.state = true;
          }
          if (xs->batt_pct != xd->batt_pct) {
            xs->batt_pct = xd->batt_pct;
            xs->changed.batt_pct = true;
          }
          changed = (xs->changed.value != 0);
          break;
        }
      }
      ss->last_seen_ts = mg_time();
      ss->last_seen_uts = mgos_uptime();
      if (changed && mgos_sys_config_get_report_on_change()) {
        report_sensor(ss);
      }
      break;
    }
    case MGOS_BT_GAP_EVENT_SCAN_STOP: {
      LOG(LL_DEBUG, ("Scan finished"));
      s_scanning = false;
      break;
    }
  }
  (void) userdata;
}

static void timer_cb(void *arg) {
  double now = mgos_uptime();
  struct sensor_state *ss, *sst;
  SLIST_FOREACH_SAFE(ss, &s_sensors, next, sst) {
    if (now - ss->last_seen_uts > mgos_sys_config_get_ttl()) {
      char addr[MGOS_BT_ADDR_STR_LEN];
      LOG(LL_INFO,
          ("Removed sensor %s type %d sid %d",
           mgos_bt_addr_to_str(&ss->addr, 0, addr), ss->type, ss->sid));
      SLIST_REMOVE(&s_sensors, ss, sensor_state, next);
    } else if (now - ss->last_reported_uts >
               mgos_sys_config_get_report_interval()) {
      // Force reporting of everything.
      if (ss->type == BT_SENSOR_XAVAX) ss->xavax.changed.value = 0xff;
      report_sensor(ss);
      ss->last_reported_uts = mgos_uptime();
    }
  }
  start_scan();
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  mgos_bt_uuid_from_str(mg_mk_str(xavax_svc_uuid_str), &xavax_svc_uuid);

  mgos_event_add_group_handler(MGOS_BT_GAP_EVENT_BASE, gap_handler, NULL);

  mgos_set_timer(1000, MGOS_TIMER_REPEAT, timer_cb, NULL);

  start_scan();

  return MGOS_APP_INIT_SUCCESS;
}
