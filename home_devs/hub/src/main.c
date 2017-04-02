#include <stdbool.h>
#include <stdio.h>

#include "common/platform.h"
#include "fw/src/mgos_app.h"
#include "fw/src/mgos_gpio.h"

#include "hub_light.h"

enum mgos_app_init_result mgos_app_init(void) {
  return hub_light_init();
}
