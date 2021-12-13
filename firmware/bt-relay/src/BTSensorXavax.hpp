#include "BTSensor.hpp"

class BTSensorXavax : public BTSensor {
 public:
  BTSensorXavax(const mgos::BTAddr &addr);
  virtual ~BTSensorXavax();

  static bool Taste(const struct mg_str &adv_data);

  const char *type_str() const override;

  void Update(const struct mg_str &adv_data, int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  uint8_t temp_ = 0;
  uint8_t tgt_temp_ = 0;
  uint8_t bogus_tgt_temp_ = 0;
  uint8_t batt_pct_ = 0;
  uint8_t state_ = 0;
};
