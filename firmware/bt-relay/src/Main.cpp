#include "BTSensor.hpp"

#include <cmath>
#include <map>
#include <memory>
#include <string>

#include "shos_app.h"
#include "shos_bt.hpp"
#include "shos_bt_gap.h"
#include "shos_bt_gap_adv.hpp"
#include "shos_bt_gap_scan.hpp"
#include "shos_dns_sd.hpp"
#include "shos_http_server.hpp"
#include "shos_json.hpp"
#include "shos_log.h"
#include "shos_ota.h"
#include "shos_rpc.hpp"
#include "shos_system.hpp"
#include "shos_time.h"
#include "shos_timers.hpp"

static std::map<shos::bt::Addr, std::unique_ptr<BTSensor>> s_sensors;
static std::unique_ptr<shos::bt::gap::ScanRequest> s_scan_req;
static double s_scanning_since = 0;
static bool s_reboot_imminent = false;
static double s_last_scan_result = 0;

static void ScanCB(
    const shos::StatusOr<const shos::bt::gap::ScanResult *> &resv) {
  if (!resv.ok()) {
    const shos::Status &st = resv.status();
    switch (st.error_code()) {
      case shos::bt::gap::kScanStartedErrorCode:
        LOG(LL_INFO, ("Scan started"));
        break;
      case shos::bt::gap::kScanFinishedErrorCode:
        LOG(LL_INFO, ("Scan finished"));
        s_scan_req.reset();
        break;
      default:
        LOG(LL_ERROR, ("Scan failed: %s", resv.status().ToString().c_str()));
        s_scan_req.reset();
        break;
    }
    return;
  }
  const shos::bt::gap::ScanResult &sr = *resv.ValueOrDie();

  s_last_scan_result = shos_uptime();

  std::string buf = shos::json::SPrintf("{mac: %Q, rssi: %d, adv: %H, rsp: %H}",
                                        sr.addr.ToString().c_str(), sr.rssi,
                                        int(sr.adv_data.len), sr.adv_data.p,
                                        int(sr.scan_rsp.len), sr.scan_rsp.p);

  LOG(LL_DEBUG, ("%s", buf.c_str()));

  shos::bt::gap::AdvData ad;
  if (!ad.Parse(sr.adv_data).ok()) return;

  BTSensor *ss = nullptr;
  auto it = s_sensors.find(sr.addr);
  if (it != s_sensors.end()) {
    ss = it->second.get();
  } else {
    auto ns = CreateBTSensor(sr.addr, sr.adv_data, ad);
    if (ns != nullptr) {
      ss = ns.get();
      LOG(LL_INFO, ("New sensor %s type %d (%s) sid %u RSSI %d",
                    ss->addr().ToString().c_str(), (int) ss->type(),
                    ss->type_str(), (unsigned) ss->sid(), sr.rssi));
      s_sensors.emplace(ss->addr(), std::move(ns));
    } else {
      LOG(LL_VERBOSE_DEBUG,
          ("Unreconized data: %s %s", sr.addr.ToString().c_str(),
           sr.adv_data.ToHexString().c_str()));
    }
  }
  if (ss != nullptr) {
    ss->Update(sr.adv_data, ad, sr.rssi);
  }
}

static void CheckScan() {
  bool should_scan = true;
  if (shos_ota_is_in_progress()) {
    should_scan = false;
  }
  if (s_reboot_imminent) {
    should_scan = false;
  }
  if (!should_scan) {
    if (s_scan_req != nullptr) {
      LOG(LL_INFO, ("Stop scanning"));
      s_scan_req.reset();
      s_scanning_since = 0;
    }
    return;
  }
  if (s_scan_req != nullptr) {
    return;
  }
  LOG(LL_DEBUG,
      ("Starting scan ns %zu hf %zu", s_sensors.size(), shos_heap_get_free()));
  shos::bt::gap::ScanOpts opts;
  opts.duration_ms = -1;  // forever
  opts.dedup_horizon_ms = 3000;
  opts.active = false;

  s_scan_req = shos::bt::gap::Scan(opts, ScanCB);
  if (s_scan_req != nullptr) {
    if (s_scanning_since == 0) {
      s_scanning_since = shos_uptime();
    }
  }
}

static void CheckSensors() {
  const double now = shos_uptime();
  size_t num_packets = 0, total_size = 0;
  std::string packet;
  const size_t kMaxPackets = shos_sys_config_get_max_packets();
  const size_t kMaxPacketSize = shos_sys_config_get_max_packet_size();

  if (s_last_scan_result > 0 && (now - s_last_scan_result) > 600) {
    LOG(LL_ERROR, ("Seem to be stuck, rebooting"));
    shos_system_restart_after(1000);
  }

  // Make sure we've had the time since starting (or resuming) scanning
  // to gather a few samples before reporting or removing stale sensors.
  if (s_scanning_since == 0 || now - s_scanning_since < 30) {
    return;
  }

  auto send_packet = [&packet, &num_packets, &total_size]() {
    if (packet.empty()) return;
    const char *hub_addr = shos_sys_config_get_hub_address();
    if (hub_addr != nullptr) {
      struct shos_rpc_call_opts opts = {
          .src = SHOS_NULL_STR,
          .dst = shos_mk_str(hub_addr),
          .tag = SHOS_NULL_STR,
          .key = SHOS_NULL_STR,
          .timeout_ms = 5000,
          .no_queue = false,
          .broadcast = false,
      };
      shos_rpc_inst_callf(shos_rpc_get_global_inst(),
                          shos::Str("Sensor.DataMulti"), nullptr, nullptr,
                          &opts, "{data: [%s]}", packet.c_str());
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
    if (data.empty() && age > shos_sys_config_get_ttl()) {
      LOG(LL_INFO, ("Removed sensor %s type %d sid %u (age %.2f)",
                    ss.addr().ToString().c_str(), (int) ss.type(),
                    (unsigned) ss.sid(), age));
      s_sensors.erase(oit);
      continue;
    }
    double reported_age = now - ss.last_reported_uts();
    if (data.empty() && reported_age > shos_sys_config_get_report_interval()) {
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
    LOG(LL_INFO, ("Sent %zu packets (%zu bytes), hf %zu", num_packets,
                  total_size, shos_heap_get_free()));
  }
}

static void StatusTimerCB() {
  CheckScan();
  CheckSensors();
}

static shos::Timer s_statusTimer(StatusTimerCB);

static void CommonEventCB(int ev, void *ev_data UNUSED_ARG,
                          void *userdata UNUSED_ARG) {
  switch (ev) {
    case SHOS_EVENT_REBOOT:
    case SHOS_EVENT_REBOOT_AFTER: s_reboot_imminent = true;
  }
  CheckScan();
}

extern "C" bool shos_app_init(void) {
  shos_event_add_handler(SHOS_EVENT_OTA_BEGIN, CommonEventCB, nullptr);
  shos_event_add_handler(SHOS_EVENT_REBOOT, CommonEventCB, nullptr);
  shos_event_add_handler(SHOS_EVENT_REBOOT_AFTER, CommonEventCB, nullptr);
  s_statusTimer.Reset(1000, SHOS_TIMER_REPEAT);

  const auto &lpr = shos::http::GetServerListenPort();
  if (lpr.ok()) {
    const char *inst = shos_sys_config_get_device_id();
    const uint16_t http_port = lpr.ValueOrDie();

    shos::dnssd::HostOpts host(inst);
    shos::dnssd::AddService(inst, "_shelly", shos::dnssd::ServiceProto::kTCP,
                            http_port, shos::dnssd::TxtEntries(), {}, host);
  }
  return true;
}
