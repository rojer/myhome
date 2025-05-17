#include "BTHomeData.hpp"
#include "BTSensor.hpp"

class BTSensorBTHome : public BTSensor {
 public:
  BTSensorBTHome(const shos::bt::Addr &addr);
  virtual ~BTSensorBTHome();

  static bool Taste(const shos::bt::gap::AdvData &ad);

  const char *type_str() const override;

  void Update(shos::Str adv_data, const shos::bt::gap::AdvData &ad,
              int8_t rssi) override;

  void Report(uint32_t what) override;

 private:
  bthome::BTHomeData bthd_;
};
