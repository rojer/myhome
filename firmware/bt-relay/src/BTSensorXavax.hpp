#include "BTSensor.hpp"

class BTSensorXavax : public BTSensor {
 public:
  BTSensorXavax(const shos::bt::Addr &addr);
  virtual ~BTSensorXavax();

  static bool Taste(const shos::bt::gap::AdvData &ad);

  const char *type_str() const override;

  void Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
              int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  struct AdvData {
    uint8_t temp = 0;
    uint8_t tgt_temp = 0;
    uint8_t batt_pct = 0;
    uint8_t mode = 0;
    uint8_t state = 0;
    uint8_t unknown_ff = 0;
    uint16_t unknown = 0;

    std::string ToString() const;
  } __attribute__((packed));

  bool ShouldSuppress(const AdvData &xd);

  uint8_t temp_ = 0;
  uint8_t tgt_temp_ = 0;
  uint8_t batt_pct_ = 0;
  uint8_t state_ = 0;
  AdvData last_adv_data_;
  int64_t t16_since_ = 0;
  int64_t tt16_since_ = 0;
};
