#include <stdbool.h>

#pragma once

bool hub_light_get_status(bool *sensor_ok, bool *lights_on);

bool hub_light_init(void);
