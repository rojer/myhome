#include "BTSensor.hpp"

#include <cmath>
#include <map>
#include <memory>
#include <string>

#include "mgos.hpp"

#include "mgos_bt.h"
#include "mgos_bt_gap.h"
#include "mgos_rpc.h"

#include "mgos_rpc.h"

static bool s_scanning = false;

std::map<mgos::BTAddr, std::unique_ptr<BTSensor>> s_sensors;

static void StartScan() {
  if (s_scanning) return;
  static bool s_active = false;
  LOG(LL_DEBUG, ("Starting scan act %d ns %d hf %d", s_active,
                 (int) s_sensors.size(), (int) mgos_get_free_heap_size()));
  struct mgos_bt_gap_scan_opts opts = {
      .duration_ms = 5000,
      .active = s_active,
      .interval_ms = 0,  // Default
      .window_ms = 0,  // Default
  };
  s_scanning = mgos_bt_gap_scan(&opts);
}

static void GAPHandler(int ev, void *ev_data, void *userdata) {
  switch (ev) {
    case MGOS_BT_GAP_EVENT_SCAN_RESULT: {
      char addr[MGOS_BT_ADDR_STR_LEN], buf[512];
      struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
      auto *r = (struct mgos_bt_gap_scan_result *) ev_data;
      struct mg_str name = mgos_bt_gap_parse_name(r->adv_data);

      json_printf(
          &out, "{mac: %Q, name: %.*Q, rssi: %d, adv: %H, rsp: %H}",
          mgos_bt_addr_to_str(&r->addr, MGOS_BT_ADDR_STRINGIFY_TYPE, addr),
          (int) name.len, name.p, r->rssi, (int) r->adv_data.len, r->adv_data.p,
          (int) r->scan_rsp.len, r->scan_rsp.p);

      LOG(LL_DEBUG, ("%s", buf));

      BTSensor *ss = nullptr;
      auto it = s_sensors.find(r->addr);
      if (it != s_sensors.end()) {
        ss = it->second.get();
      } else {
        auto ns = CreateBTSensor(r->addr, r->adv_data);
        if (ns != nullptr) {
          ss = ns.get();
          LOG(LL_INFO, ("New sensor %s type %d (%s) sid %u RSSI %d",
                        ss->addr().ToString().c_str(), (int) ss->type(),
                        ss->type_str(), (unsigned) ss->sid(), r->rssi));
          s_sensors.emplace(ss->addr(), std::move(ns));
        }
      }
      if (ss != nullptr) {
        ss->Update(r->adv_data, r->rssi);
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

static constexpr size_t kMaxPackets = 5;
static constexpr size_t kMaxPacketSize = 1000;

static void StatusTimerCB() {
  double now = mgos_uptime();
  size_t num_packets = 0, total_size = 0;
  std::string packet;

  auto send_packet = [&packet, &num_packets, &total_size]() {
    if (packet.empty()) return;
    const char *hub_addr = mgos_sys_config_get_hub_address();
    if (hub_addr != nullptr) {
      struct mg_rpc_call_opts opts = {
          .src = MG_NULL_STR,
          .dst = mg_mk_str(hub_addr),
          .tag = MG_NULL_STR,
          .key = MG_NULL_STR,
          .no_queue = false,
          .broadcast = false,
      };
      mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.DataMulti"),
                   nullptr, nullptr, &opts, "{data: [%s]}", packet.c_str());
    } else {
      LOG(LL_INFO, ("Would send %d %d: %s", num_packets, (int) packet.size(),
                    packet.c_str()));
    }
    total_size += packet.size();
    packet.clear();
    num_packets++;
  };

  for (auto it = s_sensors.begin(); it != s_sensors.end();) {
    BTSensor &ss = *it->second;
    auto oit = it;
    it++;
    auto &data = ss.data();
    double age = now - ss.last_seen_uts();
    if (data.empty() && age > mgos_sys_config_get_ttl()) {
      LOG(LL_INFO, ("Removed sensor %s type %d sid %u (age %.2f)",
                    ss.addr().ToString().c_str(), (int) ss.type(),
                    (unsigned) ss.sid(), age));
      s_sensors.erase(oit);
      continue;
    }
    double reported_age = now - ss.last_reported_uts();
    if (data.empty() && reported_age > mgos_sys_config_get_report_interval()) {
      ss.Report(BTSensor::kReportAll);
    }
    while (num_packets < kMaxPacketSize && !data.empty()) {
      const std::string &data_json = data.front().ToJSON();
      if (packet.size() + data_json.size() > kMaxPacketSize) {
        send_packet();
      }
      if (num_packets >= kMaxPackets) break;
      LOG(LL_INFO, ("Reporting %s: %s", ss.addr().ToString().c_str(),
                    data_json.c_str()));
      if (!packet.empty()) packet.append(", ");
      packet.append(data_json);
      data.erase(data.begin());
    }
  }
  send_packet();
  if (num_packets > 0) {
    LOG(LL_INFO, ("Sent %d packets (%d bytes), hf %d", (int) num_packets,
                  (int) total_size, (int) mgos_get_free_heap_size()));
  }
  StartScan();
}

static mgos::Timer s_statusTimer(StatusTimerCB);

enum mgos_app_init_result mgos_app_init(void) {
  mgos_event_add_group_handler(MGOS_BT_GAP_EVENT_BASE, GAPHandler, nullptr);

  s_statusTimer.Reset(1000, MGOS_TIMER_REPEAT);

  StartScan();

  return MGOS_APP_INIT_SUCCESS;
}
