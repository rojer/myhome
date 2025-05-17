#include "BTSensor.hpp"

class BTSensorMiATS : public BTSensor {
 public:
  BTSensorMiATS(const shos::bt::Addr &addr);
  virtual ~BTSensorMiATS();

  static bool Taste(const shos::bt::Addr &addr, shos::Str adv_data);

  const char *type_str() const override;

  void Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
              int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  uint16_t temp_ = 0;
  uint8_t rh_pct_ = 0;
  uint8_t batt_pct_ = 0;
  uint16_t batt_mv_ = 0;
  uint8_t pkt_cnt_ = 0;
};
