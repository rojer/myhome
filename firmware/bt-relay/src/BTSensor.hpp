#include <stdint.h>

#include <memory>
#include <vector>

#include "mgos_bt.hpp"

#pragma once

class BTSensor {
 public:
  enum class Type {
    kNone = 0,
    kXavax = 1,
    kASensor = 2,
    kMi = 3,
  };

  struct Data {
    uint32_t sid = 0;
    uint32_t subid = 0;
    double ts = 0;
    double value = 0;

    Data(uint32_t sid, uint32_t subid, double ts, double value);
    std::string ToJSON() const;
  };

  BTSensor(const mgos::BTAddr &addr, Type type);
  virtual ~BTSensor();
  BTSensor(const BTSensor &other) = delete;

  const mgos::BTAddr &addr() const;
  Type type() const;
  uint32_t sid() const;
  double last_seen_uts() const;
  double last_reported_uts() const;
  std::vector<Data> &data();

  virtual const char *type_str() const = 0;

  virtual void Update(const struct mg_str &adv_data, int8_t rssi) = 0;

  static constexpr uint32_t kReportAll = 0xffffffff;
  virtual void Report(uint32_t what) = 0;

 protected:
  void UpdateCommon(int8_t rssi, uint32_t changed);
  void ReportData(uint32_t subid, double value);

  const mgos::BTAddr addr_;
  const Type type_;
  const uint32_t sid_;
  int8_t rssi_ = 0;
  double last_seen_ts_ = 0;
  double last_seen_uts_ = 0;
  double last_reported_uts_ = 0;

  std::vector<Data> data_;
};

std::unique_ptr<BTSensor> CreateBTSensor(const mgos::BTAddr &addr,
                                         const struct mg_str &adv_data);
