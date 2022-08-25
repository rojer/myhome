/*
 * Copyright (c) Deomid "rojer" Ryabkov
 * All rights reserved
 */

#pragma once

#include <stdbool.h>

bool srf05_init(int sid, int trig_pin, int echo_pin, int poll_interval_ms);

float srf05_get_avg(void);
float srf05_get_max(void);
float srf05_get_last(void);
