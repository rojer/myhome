#include "BTSensor.hpp"

#include <cmath>
#include <map>
#include <memory>
#include <string>

#include "mgos.hpp"

#include "mgos_bt.h"
#include "mgos_bt_gap.h"
#include "mgos_ota.h"
#include "mgos_rpc.h"

#include "mgos_wifi.h"

static std::map<mgos::BTAddr, std::unique_ptr<BTSensor>> s_sensors;
static double s_scanning_since = 0;
static bool s_reboot_imminent = false;
static double s_last_scan_result = 0;

static void CheckScan() {
  bool should_scan = true;
  if (mgos_sys_config_get_scan_duration_ms() <= 0) {
    should_scan = false;
  }
  if (mgos_ota_is_in_progress()) {
    should_scan = false;
  }
  if (s_reboot_imminent) {
    should_scan = false;
  }
  if (!should_scan) {
    if (mgos_bt_gap_scan_in_progress()) {
      mgos_bt_gap_scan_stop();
    }
    if (s_scanning_since > 0) {
      LOG(LL_INFO, ("Stop scanning"));
      s_scanning_since = 0;
    }
    return;
  }
  if (mgos_bt_gap_scan_in_progress()) {
    return;
  }
  LOG(LL_DEBUG, ("Starting scan ns %d hf %d", (int) s_sensors.size(),
                 (int) mgos_get_free_heap_size()));
  struct mgos_bt_gap_scan_opts opts = {
      .duration_ms = 5000,
      .active = false,
      .interval_ms = 0,  // Default
      .window_ms = 0,    // Default
  };
  if (mgos_bt_gap_scan(&opts)) {
    if (s_scanning_since == 0) {
      s_scanning_since = mgos_uptime();
    }
  }
}

static void GAPHandler(int ev, void *ev_data, void *userdata) {
  switch (ev) {
    case MGOS_BT_GAP_EVENT_SCAN_RESULT: {
      char addr[MGOS_BT_ADDR_STR_LEN], buf[512];
      struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
      auto *r = (struct mgos_bt_gap_scan_result *) ev_data;
      struct mg_str name = mgos_bt_gap_parse_name(r->adv_data);

      s_last_scan_result = mgos_uptime();

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
      break;
    }
  }
  (void) userdata;
}

static void CheckSensors() {
  const double now = mgos_uptime();
  size_t num_packets = 0, total_size = 0;
  std::string packet;
  const size_t kMaxPackets = mgos_sys_config_get_max_packets();
  const size_t kMaxPacketSize = mgos_sys_config_get_max_packet_size();

  if (s_last_scan_result > 0 && (now - s_last_scan_result) > 600) {
    LOG(LL_ERROR, ("Seem to be stuck, rebooting"));
    mgos_system_restart_after(1000);
  }

  // Make sure we've had the time since starting (or resuming) scanning
  // to gather a few samples before reporting or removing stale sensors.
  if (s_scanning_since == 0 || now - s_scanning_since < 30) {
    return;
  }

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
    while (num_packets < kMaxPackets && !data.empty()) {
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
}

#define LED_R 26
#define LED_G 25
#define LED_B 27

static void CheckLEDs() {
  if (s_reboot_imminent) {
    mgos_gpio_write(LED_R, 0);
    mgos_gpio_write(LED_B, 0);
    mgos_gpio_write(LED_G, 0);
    mgos_gpio_blink(LED_G, 0, 0);
    return;
  }
  if (mgos_ota_is_in_progress()) {
    mgos_gpio_write(LED_R, 1);
  } else {
    mgos_gpio_write(LED_R, 0);
  }
  if (mgos_wifi_get_status() != MGOS_WIFI_IP_ACQUIRED) {
    mgos_gpio_write(LED_B, 1);
  } else {
    mgos_gpio_write(LED_B, 0);
  }
  if (s_scanning_since > 0) {
    mgos_gpio_blink(LED_G, 20, 980);
  } else {
    mgos_gpio_blink(LED_G, 0, 0);
  }
}

static void StatusTimerCB() {
  CheckScan();
  CheckSensors();
  CheckLEDs();
}

static mgos::Timer s_statusTimer(StatusTimerCB);

static void CommonEventCB(int ev, void *ev_data UNUSED_ARG,
                          void *userdata UNUSED_ARG) {
  switch (ev) {
    case MGOS_EVENT_REBOOT:
    case MGOS_EVENT_REBOOT_AFTER:
      s_reboot_imminent = true;
  }
  CheckScan();
  CheckLEDs();
}

enum mgos_app_init_result mgos_app_init(void) {
  mgos_event_add_group_handler(MGOS_BT_GAP_EVENT_BASE, GAPHandler, nullptr);
  mgos_event_add_handler(MGOS_EVENT_OTA_BEGIN, CommonEventCB, nullptr);
  mgos_event_add_handler(MGOS_EVENT_REBOOT, CommonEventCB, nullptr);
  mgos_event_add_handler(MGOS_EVENT_REBOOT_AFTER, CommonEventCB, nullptr);
  mgos_gpio_setup_output(LED_R, 1);
  mgos_gpio_setup_output(LED_G, 1);
  mgos_gpio_setup_output(LED_B, 1);
  s_statusTimer.Reset(1000, MGOS_TIMER_REPEAT);
  return MGOS_APP_INIT_SUCCESS;
}
