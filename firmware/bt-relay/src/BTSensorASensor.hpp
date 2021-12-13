#include "BTSensor.hpp"

class BTSensorASensor : public BTSensor {
 public:
  BTSensorASensor(const mgos::BTAddr &addr);
  virtual ~BTSensorASensor();

  const char *type_str() const override;

  static bool Taste(const struct mg_str &adv_data);

  void Update(const struct mg_str &adv_data, int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  int8_t temp_ = 0;
  int8_t batt_pct_ = 0;
  bool moving_ = false;
};
