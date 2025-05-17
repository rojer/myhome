#include "BTSensorXavax.hpp"

#include <cmath>
#include <cstring>

#include "shos.hpp"
#include "shos_bt.hpp"
#include "shos_bt_gap.h"

// 47e9ee00-47e9-11e4-8939-164230d1df67
static const shos::bt::UUID kSvcUUID({0x47, 0xe9, 0xee, 0x00, 0x47, 0xe9, 0x11,
                                      0xe4, 0x89, 0x39, 0x16, 0x42, 0x30, 0xd1,
                                      0xdf, 0x67});

union ReportData {
  struct {
    uint32_t temp : 1;
    uint32_t tgt_temp : 1;
    uint32_t batt_pct : 1;
    uint32_t state : 1;
  };
  uint32_t value = 0;
};

std::string BTSensorXavax::AdvData::ToString() const {
  return shos::SPrintf(
      "{T=%u TT=%u B=%u M=0x%02x S=0x%02x UFF=0x%02x U=0x%04x}", temp, tgt_temp,
      batt_pct, mode, state, unknown_ff, unknown);
}

// static
bool BTSensorXavax::Taste(const shos::bt::gap::AdvData &ad) {
  return ad.HasService(kSvcUUID);
}

BTSensorXavax::BTSensorXavax(const shos::bt::Addr &addr)
    : BTSensor(addr, Type::kXavax) {}

BTSensorXavax::~BTSensorXavax() {}

const char *BTSensorXavax::type_str() const {
  return "Xavax";
}

static float ConvTemp(uint8_t t) {
  if (t == 0xff) return NAN;
  return (t * 0.5);
}

void BTSensorXavax::Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
                           int8_t rssi) {
  shos::Str xds;
  for (const auto &e : ad) {
    if (e.type() == shos::bt::gap::AdvDataType::kVendorSpecific) {
      xds = e.data();
      break;
    }
  }
  if (xds.len != sizeof(AdvData)) {
    if (xds.len == 0) return;
    LOG(LL_ERROR, ("Incompatible Xavax data length, %d", (int) xds.len));
    return;
  }
  const AdvData &xd = *reinterpret_cast<const AdvData *>(xds.p);
  LOG(LL_DEBUG, ("Xavax %s RSSI %d t 0x%02x tt 0x%02x batt %3d%% state 0x%02x "
                 "mode 0x%02x unk 0x%04x",
                 addr_.ToString().c_str(), rssi, xd.temp, xd.tgt_temp,
                 xd.batt_pct, xd.state, xd.mode, xd.unknown));

  if (ShouldSuppress(xd)) return;

  union ReportData changed;
  if (temp_ != xd.temp) {
    temp_ = xd.temp;
    changed.temp = true;
  }
  if (tgt_temp_ != xd.tgt_temp) {
    tgt_temp_ = xd.tgt_temp;
    changed.tgt_temp = true;
  }
  if (state_ != xd.state) {
    state_ = xd.state;
    changed.state = true;
  }
  // Battery percentage can be bogus sometimes.
  if (batt_pct_ != xd.batt_pct && xd.batt_pct <= 100) {
    batt_pct_ = xd.batt_pct;
    changed.batt_pct = true;
  }
  last_adv_data_ = xd;
  UpdateCommon(rssi, changed.value);
}

bool BTSensorXavax::ShouldSuppress(const AdvData &xd) {
  // Check for bogus readings.
  // Sometimes device advertises bogus values: temp takes value of the target
  // temp and target temp becomes 0x20 (16C). Usually is persists for a minute
  // (until next adv data update). So if, target temp becomes 16 we wait for
  // 2+ minutes (two full refresh cycles) before we really believe it.
  bool suppress = false;
  if (xd.tgt_temp == 0x20) {
    const int64_t now = shos_uptime_micros();
    if (tt16_since_ == 0) tt16_since_ = now;
    if (now - tt16_since_ < 125 * 1000000) {
      LOG(LL_DEBUG,
          ("%s SID %d: Bogus data quarantine (%s): data age %lld",
           addr_.ToString().c_str(), sid_, "tgt_temp", now - tt16_since_));
      suppress = true;
    }
    // Ok, tgt_temp has been at 16 long enough it's probably legit.
  } else {
    if (tt16_since_ > 0 &&
        (shos_uptime_micros() - tt16_since_ < 125 * 1000000)) {
      LOG(LL_INFO,
          ("%s SID %d: Suppressed bogus data", addr_.ToString().c_str(), sid_));
    }
    tt16_since_ = 0;
  }
  // If temp is also 16, it could be a glitch, we similarly need to wait
  if (xd.temp == 0x20) {
    const int64_t now = shos_uptime_micros();
    if (t16_since_ == 0) t16_since_ = now;
    if (xd.tgt_temp == 0x20 && now - t16_since_ < 125 * 1000000) {
      LOG(LL_DEBUG, ("%s SID %d: Bogus data quarantine (%s): data age %lld",
                     addr_.ToString().c_str(), sid_, "temp", now - t16_since_));
      suppress = true;
    }
  } else {
    t16_since_ = 0;
  }
  return suppress;
}

void BTSensorXavax::Report(uint32_t whatv) {
  union ReportData what;
  what.value = whatv;
  if (what.temp && temp_ != 0xff && temp_ != 0) {
    ReportData(0, ConvTemp(temp_));
  }
  if (what.tgt_temp && tgt_temp_ != 0xff && tgt_temp_ != 0) {
    ReportData(1, ConvTemp(tgt_temp_));
  }
  // Not only battery percentage can be 0 or 0xff for "unknown" but it can
  // sometimes can get non-sensical values like 224 (0xe0).
  // We still report value 0 though, to report nearly dead battery.
  if (what.batt_pct && batt_pct_ <= 100 && tgt_temp_ != 0) {
    ReportData(2, batt_pct_);
  }
  if (what.state) {
    ReportData(4, state_);
  }
  if (whatv == kReportAll) {
    last_reported_uts_ = shos_uptime();
  }
}
