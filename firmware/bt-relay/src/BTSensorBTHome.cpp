#include "BTSensorBTHome.hpp"

#include "shos.hpp"
#include "shos_bt.hpp"
#include "shos_bt_gap.h"

// static
bool BTSensorBTHome::Taste(const shos::bt::gap::AdvData &ad) {
  bthome::BTHomeData bthd;
  return bthd.Parse(shos::bt::Addr(), ad).ok();
}

BTSensorBTHome::BTSensorBTHome(const shos::bt::Addr &addr)
    : BTSensor(addr, Type::kBTHome) {}

BTSensorBTHome::~BTSensorBTHome() {}

const char *BTSensorBTHome::type_str() const {
  return "BTHome";
}

void BTSensorBTHome::Update(shos::Str adv_data,
                            const shos::bt::gap::AdvData &ad, int8_t rssi) {
  bthome::BTHomeData bthd;
  if (!bthd.Parse(addr_, ad).ok()) return;
  size_t i = 0;
  uint32_t changed = 0;
  for (const auto &new_v : bthd.values) {
    if (new_v.type != bthome::DataType::kSensor) continue;
    const auto old_v = bthd_.GetValue(new_v.obj_id, new_v.index);
    if (new_v.obj_id == bthome::kObjectIDPacketID && old_v.ok() &&
        new_v.float_val == old_v.ValueOrDie().float_val) {
      return;  // Duplicate packet.
    }
    if (!old_v.ok() || old_v.ValueOrDie().float_val != new_v.float_val) {
      changed |= (1 << i);
    }
    i++;
  }
  LOG(LL_DEBUG, ("%s, changed: %x", bthd.ToString().c_str(), int(changed)));
  bthd_ = bthd;
  UpdateCommon(rssi, changed);
}

void BTSensorBTHome::Report(uint32_t whatv) {
  for (size_t i = 0; i < bthd_.values.size(); i++) {
    const auto &v = bthd_.values[i];
    if (v.type != bthome::DataType::kSensor) continue;
    if (!(whatv & (1 << i))) continue;
    const uint16_t subid = ((v.obj_id << 8) | v.index);
    ReportData(subid, v.float_val);
  }
  if (whatv == kReportAll) {
    last_reported_uts_ = shos_uptime();
  }
}
