#include "BTSensor.hpp"

#include "mgos.hpp"

#include "BTSensorASensor.hpp"
#include "BTSensorMiATS.hpp"
#include "BTSensorMiPVVX.hpp"
#include "BTSensorXavax.hpp"

BTSensor::BTSensor(const mgos::BTAddr &addr, Type type)
    : addr_(addr),
      type_(type),
      sid_((((uint32_t) type) << 24) | (addr.addr[3] << 16) |
           (addr.addr[4] << 8) | addr.addr[5]) {
}

BTSensor::~BTSensor() {
}

const mgos::BTAddr &BTSensor::addr() const {
  return addr_;
}

BTSensor::Type BTSensor::type() const {
  return type_;
}

uint32_t BTSensor::sid() const {
  return sid_;
}

double BTSensor::last_seen_uts() const {
  return last_seen_uts_;
}

double BTSensor::last_reported_uts() const {
  return last_reported_uts_;
}

std::vector<BTSensor::Data> &BTSensor::data() {
  return data_;
}

BTSensor::Data::Data(uint32_t sid, uint32_t subid, double ts, double value)
    : sid(sid), subid(subid), ts(ts), value(value) {
}

std::string BTSensor::Data::ToJSON() const {
  return mgos::JSONPrintStringf("{sid: %u, subid: %u, ts: %.3f, v: %.1f}",
                                (unsigned) sid, (unsigned) subid, ts, value);
}

void BTSensor::UpdateCommon(int8_t rssi, uint32_t changed) {
  rssi_ = rssi;
  last_seen_ts_ = mg_time();
  last_seen_uts_ = mgos_uptime();
  if (changed != 0 && mgos_sys_config_get_report_on_change()) {
    Report(changed);
  }
}

void BTSensor::ReportData(uint32_t subid, double value) {
  data_.push_back(Data(sid_, subid, last_seen_ts_, value));
}

std::unique_ptr<BTSensor> CreateBTSensor(const mgos::BTAddr &addr,
                                         const struct mg_str &adv_data) {
  std::unique_ptr<BTSensor> res;
  if (BTSensorASensor::Taste(adv_data)) {
    res.reset(new BTSensorASensor(addr));
  } else if (BTSensorXavax::Taste(adv_data)) {
    res.reset(new BTSensorXavax(addr));
  } else if (BTSensorMiATS::Taste(addr, adv_data)) {
    res.reset(new BTSensorMiATS(addr));
  } else if (BTSensorMiPVVX::Taste(addr, adv_data)) {
    res.reset(new BTSensorMiPVVX(addr));
  }
  return std::move(res);
}
