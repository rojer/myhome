#include "BTSensor.hpp"

class BTSensorASensor : public BTSensor {
 public:
  BTSensorASensor(const shos::bt::Addr &addr);
  virtual ~BTSensorASensor();

  const char *type_str() const override;

  static bool Taste(shos::Str adv_data);

  void Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
              int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  int8_t temp_ = 0;
  int8_t batt_pct_ = 0;
  bool moving_ = false;
};
