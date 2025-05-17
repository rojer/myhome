#include "BTSensorASensor.hpp"

#include <cmath>
#include <cstring>

#include "shos.hpp"
#include "shos_bt.hpp"
#include "shos_bt_gap.h"

struct AdvDataASensor {
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

// static
bool BTSensorASensor::Taste(shos::Str adv_data) {
  const struct AdvDataASensor *ad = (const struct AdvDataASensor *) adv_data.p;
  return (adv_data.len == sizeof(*ad) && ad->vendor == 0x00d2);
}

BTSensorASensor::BTSensorASensor(const shos::bt::Addr &addr)
    : BTSensor(addr, Type::kASensor) {}

BTSensorASensor::~BTSensorASensor() {}

const char *BTSensorASensor::type_str() const {
  return "ASensor";
}

void BTSensorASensor::Update(shos::Str adv_data,
                             const shos::bt::gap::AdvData &ad2, int8_t rssi) {
  if (!Taste(adv_data)) return;
  uint32_t changed = 0;
  const struct AdvDataASensor *ad = (const struct AdvDataASensor *) adv_data.p;
  // Don't trigger on temperature and battery percentage changes
  // because they tend to flap a lot.
  temp_ = ad->temp;
  batt_pct_ = ad->batt_pct;
  if (moving_ != ad->moving) {
    moving_ = ad->moving;
    changed |= 1;
  }
  UpdateCommon(rssi, changed);
}

void BTSensorASensor::Report(uint32_t what) {
  if (what & 1) {
    ReportData(1, moving_);
  }
  if (what == kReportAll) {
    ReportData(0, temp_);
    ReportData(2, batt_pct_);
    last_reported_uts_ = shos_uptime();
  }
}
