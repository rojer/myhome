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

  uint8_t temp_ = 0;
  uint8_t tgt_temp_ = 0;
  uint8_t bogus_tgt_temp_ = 0;
  uint8_t batt_pct_ = 0;
  uint8_t state_ = 0;
  AdvData last_adv_data_;
};
