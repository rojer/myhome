#include "BTSensor.hpp"

class BTSensorMiPVVX : public BTSensor {
 public:
  BTSensorMiPVVX(const mgos::BTAddr &addr);
  virtual ~BTSensorMiPVVX();

  static bool Taste(const mgos::BTAddr &addr, const struct mg_str &adv_data);

  const char *type_str() const override;

  void Update(const struct mg_str &adv_data, int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  int16_t temp_ = 0;
  uint16_t rh_pct_ = 0;
  uint16_t batt_mv_ = 0;
  uint8_t batt_pct_ = 0;
  uint8_t ctr_ = 0;
};
