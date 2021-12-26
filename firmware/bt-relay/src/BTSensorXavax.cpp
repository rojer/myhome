#include "BTSensorXavax.hpp"

#include <cmath>
#include <cstring>

#include "mgos.hpp"
#include "mgos_bt.hpp"
#include "mgos_bt_gap.h"

static const mgos::BTUUID kXavaxSvcUUID("47e9ee00-47e9-11e4-8939-164230d1df67");

struct AdvDataXavax {
  uint8_t temp;
  uint8_t tgt_temp;
  uint8_t batt_pct;
  uint8_t mode;
  uint8_t state;
  uint8_t unknown_ff;
  uint16_t unknown;
} __attribute__((packed));

union ReportData {
  struct {
    uint32_t temp : 1;
    uint32_t tgt_temp : 1;
    uint32_t batt_pct : 1;
    uint32_t state : 1;
  };
  uint32_t value = 0;
};

// static
bool BTSensorXavax::Taste(const struct mg_str &adv_data) {
  return mgos_bt_gap_adv_data_has_service(adv_data, &kXavaxSvcUUID);
}

BTSensorXavax::BTSensorXavax(const mgos::BTAddr &addr)
    : BTSensor(addr, Type::kXavax) {
}

BTSensorXavax::~BTSensorXavax() {
}

const char *BTSensorXavax::type_str() const {
  return "Xavax";
}

static float ConvTemp(uint8_t t) {
  if (t == 0xff) return NAN;
  return (t * 0.5);
}

void BTSensorXavax::Update(const struct mg_str &adv_data, int8_t rssi) {
  if (!Taste(adv_data)) return;
  struct mg_str xds = mgos_bt_gap_parse_adv_data(
      adv_data, MGOS_BT_GAP_EIR_MANUFACTURER_SPECIFIC_DATA);
  if (xds.len != sizeof(AdvDataXavax)) {
    if (xds.len == 0) return;
    LOG(LL_ERROR, ("Incompatible Xavax data length, %d", (int) xds.len));
    return;
  }
  const AdvDataXavax *xd = (const AdvDataXavax *) xds.p;
  LOG(LL_DEBUG, ("Xavax %s RSSI %d t 0x%02x tt 0x%02x batt %d state 0x%02x",
                 addr_.ToString().c_str(), rssi, xd->temp, xd->tgt_temp,
                 xd->batt_pct, xd->state));
  union ReportData changed;
  // Sometimes bogus values are reported for temperatures.
  //  1. Current gets value of target, target gets some random value:
  //   T 22.0 TT  4.0 batt 68% state 0x00 chg 0xff data 2c08448100ffd20d
  //   T  4.0 TT 20.5 batt 68% state 0x00 chg 0x3 data 0829448100ff06fc
  //  2. Target gets some random value. The value is the same as
  //     in (1) but doesn't seem to be related to anything.
  // So if temperature suddenly becomes equal to target, we ignore it.
  if (temp_ != xd->temp) {
    if (xd->temp == tgt_temp_ && abs(((int) xd->temp) - ((int) temp_)) >= 4) {
      float temp = ConvTemp(xd->temp);
      float tgt_temp = ConvTemp(xd->tgt_temp);
      LOG(LL_INFO, ("%s: Ignored bogus temperature report (%.2f %.2f)",
                    addr_.ToString().c_str(), temp, tgt_temp));
      // We remember the bogus value so we can detect (2).
      bogus_tgt_temp_ = xd->tgt_temp;
    } else {
      temp_ = xd->temp;
      changed.temp = true;
    }
  }
  if (tgt_temp_ != xd->tgt_temp) {
    if (xd->tgt_temp != bogus_tgt_temp_) {
      tgt_temp_ = xd->tgt_temp;
      changed.tgt_temp = true;
    } else {
      float temp = ConvTemp(xd->temp);
      float tgt_temp = ConvTemp(xd->tgt_temp);
      LOG(LL_INFO, ("%s: Ignored bogus tgt temp report (%.2f %.2f)",
                    addr_.ToString().c_str(), temp, tgt_temp));
    }
  }
  if (state_ != xd->state) {
    state_ = xd->state;
    changed.state = true;
  }
  // Battery percentage can be bogus sometimes.
  if (batt_pct_ != xd->batt_pct && xd->batt_pct <= 100) {
    batt_pct_ = xd->batt_pct;
    changed.batt_pct = true;
  }
  UpdateCommon(rssi, changed.value);
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
    last_reported_uts_ = mgos_uptime();
  }
}
