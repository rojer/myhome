#include <stdbool.h>

#pragma once

bool hub_heater_init(void);

bool hub_heater_get_status(bool *heater_on, double *last_action_ts);
