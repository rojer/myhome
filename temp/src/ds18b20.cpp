#include "ds18b20.h"

#include "mgos.h"
#include "mgos_sys_config.h"

#include "DallasTemperature.h"
#include "OneWire.h"

static OneWire *s_ow = nullptr;
static DallasTemperature *s_dt = nullptr;

#define INVALID_VALUE -1000.0

extern "C" {

bool ds18b20_probe(void) {
  LOG(LL_INFO, ("OneWire bus on %d", mgos_sys_config_get_ow_gpio()));
  s_ow = new OneWire(mgos_sys_config_get_ow_gpio());
  s_dt = new DallasTemperature(s_ow);
  s_dt->begin();
  return true;
}

void ds18b20_read(float *temp, float *rh) {
  *rh = INVALID_VALUE;
  *temp = INVALID_VALUE;
  int i;
  float t = -127;
  static uint8_t addr[8];
  static bool have_addr = false;
  for (i = 0; i < 3; i++) {
    if (!have_addr) {
      if (s_dt->getAddress(addr, 0)) {
        have_addr = true;
      } else {
        continue;
      }
    }
    s_dt->requestTemperatures();
    t = s_dt->getTempC(addr);
    if (t > -100) break;
  }
  LOG(LL_INFO, ("%02x%02x%02x%02x%02x%02x%02x%02x %d: %f", addr[0], addr[1],
                addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], i, t));
  if (t > -100) {
    *temp = t;
  }
}

}  // extern "C"
