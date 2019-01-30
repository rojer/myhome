#pragma once

#define LIGHT_SID 50

#define LIGHTS_SUBID 0
#define HEATER_SUBID 1

#define UPTIME_SUBID 0
#define HEAP_FREE_SUBID 1

void report_to_server(int sid, int subid, double ts, double value);
