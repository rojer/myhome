#include "BTSensorMiPVVX.hpp"

#include "shos.hpp"
#include "shos_bt.hpp"
#include "shos_bt_gap.h"

static constexpr shos::bt::UUID kSvcUUID(0x181A);  // Environmental Sensing

// https://github.com/pvvx/ATC_MiThermometer#custom-format-all-data-little-endian
struct SvcDataMiPVVX {
  uint8_t addr[6];   // [0] - lo, .. [6] - hi digits
  int16_t temp;      // x 0.01 degree
  uint16_t rh_pct;   // x 0.01 %
  uint16_t batt_mv;  // mV
  uint8_t batt_pct;  // 0..100 %
  uint8_t ctr;       // measurement count
  uint8_t flags;     // GPIO_TRG pin (marking "reset" on circuit board) flags:
                     // bit0: Reed Switch, input
                     // bit1: GPIO_TRG pin output value (pull Up/Down)
                     // bit2: Output GPIO_TRG pin is controlled according to the
                     // set parameters bit3: Temperature trigger event bit4:
                     // Humidity trigger event
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
bool BTSensorMiPVVX::Taste(const shos::bt::Addr &addr,
                           const shos::bt::gap::AdvData &ad) {
  const auto svc_data = ad.GetServiceData(kSvcUUID);
  if (svc_data.empty()) return false;
  if (svc_data.len < sizeof(SvcDataMiPVVX)) return false;
  const auto &sd = *reinterpret_cast<const SvcDataMiPVVX *>(svc_data.p);
  return (shos::bt::Addr(sd.addr, true /* reverse */) == addr);
}

BTSensorMiPVVX::BTSensorMiPVVX(const shos::bt::Addr &addr)
    : BTSensor(addr, Type::kMi) {}

BTSensorMiPVVX::~BTSensorMiPVVX() {}

const char *BTSensorMiPVVX::type_str() const {
  return "MiPVVX";
}

void BTSensorMiPVVX::Update(shos::Str adv_data,
                            const shos::bt::gap::AdvData &ad, int8_t rssi) {
  const auto svc_data = ad.GetServiceData(kSvcUUID);
  if (svc_data.len < sizeof(SvcDataMiPVVX)) return;
  const auto &sd = *reinterpret_cast<const SvcDataMiPVVX *>(svc_data.p);
  union ReportData changed;
  if (sd.ctr != ctr_) {
    LOG(LL_DEBUG, ("%s T %d RH %d%% BATT %d%% / %dmV CNT %d %d",
                   addr_.ToString().c_str(), sd.temp, sd.rh_pct, sd.batt_pct,
                   sd.batt_mv, sd.ctr, int(changed.value)));
    if (sd.temp != temp_) {
      temp_ = sd.temp;
      // changed.temp = true;
    }
    if (sd.rh_pct != rh_pct_) {
      rh_pct_ = sd.rh_pct;
      // changed.rh_pct = true;
    }
    if (sd.batt_pct != batt_pct_) {
      batt_pct_ = sd.batt_pct;
      // changed.batt_pct = true;
    }
    batt_mv_ = sd.batt_mv;
    ctr_ = sd.ctr;
  }
  UpdateCommon(rssi, changed.value);
}

void BTSensorMiPVVX::Report(uint32_t whatv) {
  union ReportData what;
  what.value = whatv;
  if (what.temp) {
    ReportData(0, temp_ / 100.0f);
  }
  if (what.rh_pct) {
    ReportData(1, rh_pct_ / 100.0f);
  }
  if (what.batt_pct) {
    ReportData(2, batt_pct_);
  }
  if (whatv == kReportAll) {
    last_reported_uts_ = shos_uptime();
  }
}
