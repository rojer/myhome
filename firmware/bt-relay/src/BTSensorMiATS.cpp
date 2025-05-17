#include "BTSensorMiATS.hpp"

#include "shos.hpp"
#include "shos_bt.hpp"
#include "shos_bt_gap.h"

// https://github.com/atc1441/ATC_MiThermometer/#advertising-format-of-the-custom-firmware
struct AdvDataMiATS {
  uint8_t dummy[4];
  uint8_t addr[6];
  uint16_t temp;  // BE
  uint8_t rh_pct;
  uint8_t batt_pct;
  uint16_t batt_mv;  // BE
  uint8_t pkt_cnt;
} __attribute__((packed));

union ReportData {
  struct {
    uint32_t temp : 1;
    uint32_t rh_pct : 1;
    uint32_t batt_pct : 1;
  };
  uint32_t value = 0;
};

// static
bool BTSensorMiATS::Taste(const shos::bt::Addr &addr, shos::Str adv_data) {
  const struct AdvDataMiATS *ad = (const struct AdvDataMiATS *) adv_data.p;
  if (adv_data.len != sizeof(*ad)) return false;
  if (shos::bt::Addr(ad->addr, false /* reverse */) != addr) return false;
  return true;
}

BTSensorMiATS::BTSensorMiATS(const shos::bt::Addr &addr)
    : BTSensor(addr, Type::kMi) {}

BTSensorMiATS::~BTSensorMiATS() {}

const char *BTSensorMiATS::type_str() const {
  return "MiATS";
}

void BTSensorMiATS::Update(shos::Str adv_data,
                           const shos::bt::gap::AdvData &ad2, int8_t rssi) {
  if (!Taste(addr_, adv_data)) return;
  union ReportData changed;
  const struct AdvDataMiATS *ad = (const struct AdvDataMiATS *) adv_data.p;
  if (ad->pkt_cnt != pkt_cnt_) {
    LOG(LL_DEBUG,
        ("%s T %d RH %d%% BATT %d%% / %dmV CNT %d %d", addr_.ToString().c_str(),
         (int16_t) ntohs(ad->temp), ad->rh_pct, ad->batt_pct,
         ntohs(ad->batt_mv), ad->pkt_cnt, int(changed.value)));
    if (ad->temp != temp_) {
      temp_ = ad->temp;
      changed.temp = true;
    }
    if (ad->rh_pct != rh_pct_) {
      rh_pct_ = ad->rh_pct;
      changed.rh_pct = true;
    }
    if (ad->batt_pct != batt_pct_) {
      batt_pct_ = ad->batt_pct;
      changed.batt_pct = true;
    }
    batt_mv_ = ad->batt_mv;
    pkt_cnt_ = ad->pkt_cnt;
  }
  UpdateCommon(rssi, changed.value);
}

void BTSensorMiATS::Report(uint32_t whatv) {
  union ReportData what;
  what.value = whatv;
  if (what.temp) {
    ReportData(0, ((int16_t) ntohs(temp_)) / 10.0f);
  }
  if (what.rh_pct) {
    ReportData(1, rh_pct_);
  }
  if (what.batt_pct) {
    ReportData(2, batt_pct_);
  }
  if (whatv == kReportAll) {
    last_reported_uts_ = shos_uptime();
  }
}
