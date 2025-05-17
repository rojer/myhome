#include "BTSensor.hpp"

class BTSensorMiPVVX : public BTSensor {
 public:
  BTSensorMiPVVX(const shos::bt::Addr &addr);
  virtual ~BTSensorMiPVVX();

  static bool Taste(const shos::bt::Addr &addr,
                    const shos::bt::gap::AdvData &ad);

  const char *type_str() const override;

  void Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
              int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  int16_t temp_ = 0;
  uint16_t rh_pct_ = 0;
  uint16_t batt_mv_ = 0;
  uint8_t batt_pct_ = 0;
  uint8_t ctr_ = 0;
};
