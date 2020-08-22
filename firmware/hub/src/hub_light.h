#include <stdbool.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool hub_light_get_status(bool *sensor_ok, bool *lights_on);

bool hub_light_init(void);

#ifdef __cplusplus
}
#endif
