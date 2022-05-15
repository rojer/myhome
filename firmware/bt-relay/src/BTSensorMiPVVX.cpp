#include "BTSensorMiPVVX.hpp"

#include "mgos.hpp"
#include "mgos_bt.hpp"
#include "mgos_bt_gap.h"

// https://github.com/pvvx/ATC_MiThermometer#custom-format-all-data-little-endian
struct AdvDataMiPVVX {
  uint8_t size;      // = 19
  uint8_t uid;       // = 0x16, 16-bit UUID
  uint16_t uuid;     // = 0x181A, GATT Service 0x181A Environmental Sensing
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
bool BTSensorMiPVVX::Taste(const mgos::BTAddr &addr,
                           const struct mg_str &adv_data) {
  const struct AdvDataMiPVVX *ad = (const struct AdvDataMiPVVX *) adv_data.p;
  if (adv_data.len != sizeof(*ad)) return false;
  if (ad->uuid != 0x181a) return false;
  if (mgos::BTAddr(ad->addr, true /* reverse */) != addr) return false;
  return true;
}

BTSensorMiPVVX::BTSensorMiPVVX(const mgos::BTAddr &addr)
    : BTSensor(addr, Type::kMi) {
}

BTSensorMiPVVX::~BTSensorMiPVVX() {
}

const char *BTSensorMiPVVX::type_str() const {
  return "MiPVVX";
}

void BTSensorMiPVVX::Update(const struct mg_str &adv_data, int8_t rssi) {
  if (!Taste(addr_, adv_data)) return;
  union ReportData changed;
  const struct AdvDataMiPVVX *ad = (const struct AdvDataMiPVVX *) adv_data.p;
  if (ad->ctr != ctr_) {
    LOG(LL_DEBUG, ("%s T %d RH %d%% BATT %d%% / %dmV CNT %d %d",
                   addr_.ToString().c_str(), ad->temp, ad->rh_pct, ad->batt_pct,
                   ad->batt_mv, ad->ctr, changed.value));
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
    ctr_ = ad->ctr;
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
    last_reported_uts_ = mgos_uptime();
  }
}
