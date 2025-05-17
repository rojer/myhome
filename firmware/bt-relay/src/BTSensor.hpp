#include <stdint.h>

#include <memory>
#include <vector>

#include "shos_bt.hpp"
#include "shos_bt_gap_adv.hpp"

#pragma once

class BTSensor {
 public:
  enum class Type {
    kNone = 0,
    kXavax = 1,
    kASensor = 2,
    kMi = 3,
    kBTHome = 4,
  };

  struct Data {
    uint32_t sid = 0;
    uint32_t subid = 0;
    double ts = 0;
    double value = 0;

    Data(uint32_t sid, uint32_t subid, double ts, double value);
    std::string ToJSON() const;
  };

  BTSensor(const shos::bt::Addr &addr, Type type);
  virtual ~BTSensor();
  BTSensor(const BTSensor &other) = delete;

  const shos::bt::Addr &addr() const;
  Type type() const;
  uint32_t sid() const;
  double last_seen_uts() const;
  double last_reported_uts() const;
  std::vector<Data> &data();

  virtual const char *type_str() const = 0;

  virtual void Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
                      int8_t rssi) = 0;

  static constexpr uint32_t kReportAll = 0xffffffff;
  virtual void Report(uint32_t what) = 0;

 protected:
  void UpdateCommon(int8_t rssi, uint32_t changed);
  void ReportData(uint32_t subid, double value);

  const shos::bt::Addr addr_;
  const Type type_;
  const int sid_;
  int8_t rssi_ = 0;
  double last_seen_ts_ = 0;
  double last_seen_uts_ = 0;
  double last_reported_uts_ = 0;

  std::vector<Data> data_;
};

std::unique_ptr<BTSensor> CreateBTSensor(const shos::bt::Addr &addr,
                                         shos::Str adv_data,
                                         const shos::bt::gap::AdvData &ad);
