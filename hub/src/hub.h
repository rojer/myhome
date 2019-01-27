#pragma once

#define LIGHT_SID 50

#define CTL_SID 100 /* Pseudo-sensor used to report outputs state */
#define LIGHTS_SUBID 0
#define HEATER_SUBID 1

#define SYS_SID 200 /* Pseudo-sensor for system stats. */
#define UPTIME_SUBID 0
#define HEAP_FREE_SUBID 1

void report_to_server(int sid, int subid, double ts, double value);
