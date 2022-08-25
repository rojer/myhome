#include "ds18b20.h"

#include "mgos.h"
#include "mgos_ds18b20.h"

#define INVALID_VALUE -1000.0

extern "C" {

bool ds18b20_probe(void) {
  return mgos_sys_config_get_ds18b20_enable();
}

void ds18b20_read(int addr_, float *temp, float *rh) {
  *rh = INVALID_VALUE;
  *temp = mgos_ds18b20_get();
  if (*temp == 0.0 && mgos_uptime() < mgos_sys_config_get_ds18b20_poll_period() * 2 / 1000) {
    *temp = INVALID_VALUE;
  }
  (void) addr_;
}

}  // extern "C"
